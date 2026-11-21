#include "ClashDetector.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

using namespace std;

namespace {

const size_t NO_SECOND_SESSION = static_cast<size_t>(-1);

// Trim and convert to upper case.
string normalize(const string& value) {
    auto first = find_if_not(
        value.begin(), value.end(),
        [](unsigned char c) { return isspace(c); }
    );

    auto last = find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char c) { return isspace(c); }
    ).base();

    if (first >= last)
        return "";

    string result(first, last);

    transform(
        result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(toupper(c)); }
    );

    return result;
}

// Room names: upper case, single spaces, no spaces around '-'
// ("CR - 7", "cr-7" and "CR-7" all become "CR-7").
string normalizeRoom(const string& value) {
    const string upper = normalize(value);
    string result;

    for (size_t i = 0; i < upper.size(); ++i) {
        const char c = upper[i];

        if (isspace(static_cast<unsigned char>(c))) {
            if (!result.empty() && result.back() != ' ' && result.back() != '-')
                result += ' ';
            continue;
        }

        if (c == '-' && !result.empty() && result.back() == ' ')
            result.pop_back();

        result += c;
    }

    while (!result.empty() && result.back() == ' ')
        result.pop_back();

    return result;
}

// Convert HH:MM to minutes after midnight; -1 for invalid values.
int toMinutes(const string& time) {
    string value = normalize(time);

    if (value.size() != 5 || value[2] != ':')
        return -1;

    if (!isdigit(static_cast<unsigned char>(value[0])) ||
        !isdigit(static_cast<unsigned char>(value[1])) ||
        !isdigit(static_cast<unsigned char>(value[3])) ||
        !isdigit(static_cast<unsigned char>(value[4]))) {
        return -1;
    }

    int hours = stoi(value.substr(0, 2));
    int minutes = stoi(value.substr(3, 2));

    if (hours > 23 || minutes > 59)
        return -1;

    return hours * 60 + minutes;
}

// Which weeks a session is active in. Saturday classes only exist when
// Saturday is a working day; "*" classes only when it is off.
WeekRule effectiveRule(const Session& session) {
    if (session.getWeekRule() != WeekRule::Always)
        return session.getWeekRule();

    if (session.getDayIndex() == 5)
        return WeekRule::SaturdayWorkingOnly;

    return WeekRule::Always;
}

// Weeks in which BOTH sessions run. Returns false when there is no such
// week, i.e. the pair can never be active together.
bool combineRules(WeekRule a, WeekRule b, WeekRule& result) {
    if (a == WeekRule::Always) { result = b; return true; }
    if (b == WeekRule::Always) { result = a; return true; }
    if (a == b)                { result = a; return true; }
    return false;
}

string withScenario(const string& text, WeekRule rule) {
    if (rule == WeekRule::SaturdayOffOnly)
        return text + " [only in weeks when Saturday is off]";

    if (rule == WeekRule::SaturdayWorkingOnly)
        return text + " [only in weeks when Saturday is a working day]";

    return text;
}

// Free periods and non-class activities never occupy a room or a teacher.
bool isFreeActivity(const string& subjectUpper) {
    return subjectUpper == "LUNCH" ||
           subjectUpper == "LIB" ||
           subjectUpper == "SWAYAM" ||
           subjectUpper.find("AUDIT COURSE") != string::npos ||
           subjectUpper.find("TRAINING") != string::npos ||
           subjectUpper.find("EXTRA-CURRICULAR") != string::npos;
}

// "G1", "G2" ... from a subject such as "Data Structures Lab (G1)".
string extractGroup(const string& subjectUpper) {
    size_t pos = subjectUpper.find("(G");

    if (pos == string::npos)
        return "";

    size_t end = subjectUpper.find(')', pos);

    if (end == string::npos)
        return "";

    string group = subjectUpper.substr(pos + 1, end - pos - 1);

    if (group.size() < 2 || group[0] != 'G')
        return "";

    for (size_t i = 1; i < group.size(); i++) {
        if (!isdigit(static_cast<unsigned char>(group[i])))
            return "";
    }

    return group;
}

// Split "Hon'ble President Sir / Mr. A & Ms. B" into individual people:
// upper case, honorifics and "G1-" prefixes removed, unknowns dropped.
vector<string> facultyNames(const string& raw, bool ignorePresident) {
    vector<string> names;
    string token;
    vector<string> tokens;

    for (size_t i = 0; i <= raw.size(); ++i) {
        const char c = (i < raw.size()) ? raw[i] : '/';

        if (c == '/' || c == '&' || c == ',' || c == ';') {
            tokens.push_back(token);
            token.clear();
        } else {
            token += c;
        }
    }

    static const char* const honorifics[] =
        { "PROF", "MRS", "DR", "MR", "MS" };

    for (const string& rawToken : tokens) {
        string name = normalize(rawToken);

        if (name.empty())
            continue;

        // Group label such as "G1-" or "G2 -".
        if (name.size() > 2 && name[0] == 'G' &&
            isdigit(static_cast<unsigned char>(name[1]))) {
            size_t dash = name.find('-');
            if (dash != string::npos)
                name = normalize(name.substr(dash + 1));
        }

        for (const char* prefix : honorifics) {
            const string p = prefix;
            if (name.compare(0, p.size(), p) == 0 &&
                name.size() > p.size() &&
                (name[p.size()] == '.' || name[p.size()] == ' ')) {
                name = normalize(name.substr(p.size() + 1));
                break;
            }
        }

        // Collapse inner whitespace.
        string collapsed;
        for (char c : name) {
            if (isspace(static_cast<unsigned char>(c))) {
                if (!collapsed.empty() && collapsed.back() != ' ')
                    collapsed += ' ';
            } else {
                collapsed += c;
            }
        }
        name = collapsed;

        if (name.empty() || name == "NOT SPECIFIED" || name == "NA" ||
            name == "N/A" || name == "TBA")
            continue;

        if (ignorePresident && name.find("PRESIDENT") != string::npos)
            continue;

        if (find(names.begin(), names.end(), name) == names.end())
            names.push_back(name);
    }

    return names;
}

// A session may use several rooms ("Lab-7 & Lab-6"). Unknown and online
// sessions use no physical room.
vector<string> roomNames(const string& raw) {
    vector<string> names;
    string token;

    for (size_t i = 0; i <= raw.size(); ++i) {
        if (i == raw.size() || raw[i] == '&') {
            const string name = normalizeRoom(token);
            token.clear();

            if (!name.empty() && name != "UNKNOWN" && name != "ONLINE" &&
                find(names.begin(), names.end(), name) == names.end())
                names.push_back(name);
        } else {
            token += raw[i];
        }
    }

    return names;
}

// Everything the detector needs about each session, computed once so the
// sweep never re-parses strings.
struct Prepared {
    vector<int> day, start, end;
    vector<char> valid, lab;
    vector<WeekRule> rule;
    vector<string> subject, room, section, group, key;
    vector<vector<string>> rooms;
};

string makeSessionKey(const Session& session, int start, int end) {
    string key;

    auto append = [&key](const string& value) {
        key += to_string(value.size());
        key += ':';
        key += value;
    };

    append(normalize(session.getSubject()));
    append(normalize(session.getFaculty()));
    append(normalizeRoom(session.getRoom()));
    append(normalize(session.getSection()));
    append(to_string(session.getDayIndex()));
    append(to_string(start));
    append(to_string(end));
    append(to_string(session.getStudentCount()));
    append(to_string(static_cast<int>(effectiveRule(session))));

    return key;
}

Prepared prepare(const vector<Session>& sessions) {
    Prepared p;
    const size_t n = sessions.size();

    p.day.resize(n);
    p.start.resize(n);
    p.end.resize(n);
    p.valid.resize(n);
    p.lab.resize(n);
    p.rule.resize(n);
    p.subject.resize(n);
    p.room.resize(n);
    p.section.resize(n);
    p.group.resize(n);
    p.key.resize(n);
    p.rooms.resize(n);

    for (size_t i = 0; i < n; ++i) {
        const Session& s = sessions[i];

        p.day[i] = s.getDayIndex();
        p.start[i] = toMinutes(s.getStartTime());
        p.end[i] = toMinutes(s.getEndTime());
        p.valid[i] = p.day[i] < 7 && p.start[i] >= 0 &&
                     p.end[i] >= 0 && p.start[i] < p.end[i];
        p.rule[i] = effectiveRule(s);
        p.subject[i] = normalize(s.getSubject());
        p.room[i] = normalizeRoom(s.getRoom());
        p.rooms[i] = roomNames(s.getRoom());
        p.section[i] = normalize(s.getSection());
        p.group[i] = extractGroup(p.subject[i]);
        p.lab[i] = p.subject[i].find("LAB") != string::npos ||
                   p.room[i].find("LAB") != string::npos;
        p.key[i] = makeSessionKey(s, p.start[i], p.end[i]);
    }

    return p;
}

// Same subject, same room, same time: one lecture shared by several
// sections. Not a clash for room or faculty. Labs are never shared.
bool isCombinedClass(const Prepared& p, size_t i, size_t j) {
    if (p.lab[i] || p.lab[j])
        return false;

    if (p.room[i].empty() || p.room[i] == "UNKNOWN")
        return false;

    return p.day[i] == p.day[j] &&
           p.rule[i] == p.rule[j] &&
           p.start[i] == p.start[j] &&
           p.end[i] == p.end[j] &&
           p.room[i] == p.room[j] &&
           p.subject[i] == p.subject[j];
}

void addClash(
    vector<Clash>& clashes,
    ClashType type,
    size_t first,
    size_t second,
    const string& description,
    WeekRule scenario = WeekRule::Always
) {
    Clash clash;
    clash.type = type;
    clash.firstSession = first;
    clash.secondSession = second;
    clash.description = withScenario(description, scenario);
    clash.scenario = scenario;
    clashes.push_back(clash);
}

string clashTypeName(ClashType type) {
    switch (type) {
        case ClashType::Faculty:          return "FACULTY CLASH";
        case ClashType::Room:             return "ROOM CLASH";
        case ClashType::Section:          return "SECTION CLASH";
        case ClashType::RoomCapacity:     return "ROOM CAPACITY CLASH";
        case ClashType::DuplicateSession: return "DUPLICATE SESSION";
    }

    return "UNKNOWN CLASH";
}

// Sweep Line over one resource (one room, one teacher or one section).
//
// Sessions are visited in order of start time. "active" holds the sessions
// that have started and not yet finished. When a session starts, every
// active session overlaps it, because
//     start(active) <= start(new)  and  end(active) > start(new).
// A session whose end <= the new start is dropped first, so classes that
// merely touch (09:55 end / 09:55 start) never overlap.
// Time: O(k log k + number of reported pairs) instead of O(k^2).
template <typename OnOverlap>
void sweepResource(const vector<size_t>& group, const Prepared& p,
                   OnOverlap onOverlap) {
    vector<size_t> order = group;

    sort(order.begin(), order.end(), [&p](size_t a, size_t b) {
        if (p.day[a] != p.day[b])     return p.day[a] < p.day[b];
        if (p.start[a] != p.start[b]) return p.start[a] < p.start[b];
        if (p.end[a] != p.end[b])     return p.end[a] < p.end[b];
        return a < b;
    });

    vector<size_t> active;
    int activeDay = -1;

    for (size_t current : order) {
        if (p.day[current] != activeDay) {
            active.clear();
            activeDay = p.day[current];
        }

        active.erase(
            remove_if(active.begin(), active.end(),
                      [&](size_t other) {
                          return p.end[other] <= p.start[current];
                      }),
            active.end()
        );

        for (size_t other : active)
            onOverlap(min(other, current), max(other, current));

        active.push_back(current);
    }
}

// A room that is really several identical rooms ("B.Tech. Block IInd Floor
// Lab-1" is 4 labs). It only clashes when MORE than `capacity` sessions
// overlap. Done once per week type so "*" classes are never counted in
// weeks when they do not run:
//   Always            - sessions held every week
//   SaturdayOffOnly   - those plus the "*" classes
//   SaturdayWorkingOnly - those plus the Saturday classes
// A pass reports an overflow only if it involves a session of its own
// week type, so nothing is reported twice.
template <typename OnOverflow>
void sweepPool(const vector<size_t>& group, const Prepared& p, size_t capacity,
               WeekRule weeks, OnOverflow onOverflow) {
    vector<size_t> order;

    for (size_t i : group) {
        if (p.rule[i] == WeekRule::Always || p.rule[i] == weeks)
            order.push_back(i);
    }

    sort(order.begin(), order.end(), [&p](size_t a, size_t b) {
        if (p.day[a] != p.day[b])     return p.day[a] < p.day[b];
        if (p.start[a] != p.start[b]) return p.start[a] < p.start[b];
        if (p.end[a] != p.end[b])     return p.end[a] < p.end[b];
        return a < b;
    });

    vector<size_t> active;
    int activeDay = -1;

    for (size_t current : order) {
        if (p.day[current] != activeDay) {
            active.clear();
            activeDay = p.day[current];
        }

        active.erase(
            remove_if(active.begin(), active.end(),
                      [&](size_t other) {
                          return p.end[other] <= p.start[current];
                      }),
            active.end()
        );

        if (active.size() >= capacity) {
            bool involvesWeekType = (p.rule[current] == weeks);

            for (size_t other : active)
                if (p.rule[other] == weeks)
                    involvesWeekType = true;

            if (weeks == WeekRule::Always || involvesWeekType) {
                onOverflow(min(active.front(), current),
                           max(active.front(), current), weeks);
            }
        }

        active.push_back(current);
    }
}

using Groups = map<string, vector<size_t>>;

} // namespace

ClashDetector::RoomPools ClashDetector::defaultRoomPools() {
    // The Logic Design lab block: 4 labs run 4 sessions at once.
    return { {"B.TECH. BLOCK IIND FLOOR LAB-1", 4} };
}

vector<Clash> ClashDetector::detectClashes(
    const vector<Session>& sessions,
    bool ignorePresidentClass
) {
    return detectClashes(sessions, ignorePresidentClass, defaultRoomPools());
}

vector<Clash> ClashDetector::detectClashes(
    const vector<Session>& sessions,
    bool ignorePresidentClass,
    const RoomPools& roomPools
) {
    vector<Clash> clashes;
    const Prepared p = prepare(sessions);

    Groups facultyGroups, roomGroups, sectionGroups, duplicateGroups;

    // One pass: skip free periods and invalid rows, check capacity,
    // and hash every session into its faculty / room / section groups.
    for (size_t i = 0; i < sessions.size(); ++i) {
        const Session& session = sessions[i];

        if (isFreeActivity(p.subject[i]) || !p.valid[i])
            continue;

        // 1. Room capacity.
        const int students = session.getStudentCount();
        const int capacity = session.getRoomCapacity();

        if (students >= 0 && capacity > 0 && students > capacity) {
            ostringstream message;

            message << session.getRoom()
                    << " has capacity " << capacity
                    << ", but " << session.getSubject()
                    << " (" << session.getSection()
                    << ") has " << students << " students.";

            addClash(clashes, ClashType::RoomCapacity, i,
                     NO_SECOND_SESSION, message.str(), p.rule[i]);
        }

        // 2. Faculty: one entry per person.
        for (const string& name :
             facultyNames(session.getFaculty(), ignorePresidentClass)) {
            facultyGroups[name].push_back(i);
        }

        // 3. Physical room(s).
        for (const string& room : p.rooms[i])
            roomGroups[room].push_back(i);

        // 4. Section.
        if (!p.section[i].empty())
            sectionGroups[p.section[i]].push_back(i);

        // 5. Exact duplicates.
        duplicateGroups[p.key[i]].push_back(i);
    }

    // Faculty clashes. A pair sharing two teachers is reported once.
    set<pair<size_t, size_t>> reportedFacultyPairs;

    for (const auto& entry : facultyGroups) {
        const string& name = entry.first;

        sweepResource(entry.second, p, [&](size_t i, size_t j) {
            if (p.key[i] == p.key[j])
                return;                       // reported as duplicate

            WeekRule scenario;
            if (!combineRules(p.rule[i], p.rule[j], scenario))
                return;

            if (isCombinedClass(p, i, j))
                return;

            if (!reportedFacultyPairs.insert({i, j}).second)
                return;

            addClash(clashes, ClashType::Faculty, i, j,
                     name + " is assigned to two overlapping sessions.",
                     scenario);
        });
    }

    // Room clashes. A pair sharing two rooms is reported once.
    set<pair<size_t, size_t>> reportedRoomPairs;

    for (const auto& entry : roomGroups) {
        const string& room = entry.first;

        auto pool = roomPools.find(room);

        if (pool != roomPools.end()) {
            for (WeekRule weeks : {WeekRule::Always, WeekRule::SaturdayOffOnly,
                                   WeekRule::SaturdayWorkingOnly}) {
                sweepPool(entry.second, p, static_cast<size_t>(pool->second),
                          weeks, [&](size_t i, size_t j, WeekRule scenario) {
                    addClash(clashes, ClashType::Room, i, j,
                             "Room " + room + " has " +
                             to_string(pool->second) +
                             " parallel labs, but more than that many sessions overlap.",
                             scenario);
                });
            }
            continue;
        }

        sweepResource(entry.second, p, [&](size_t i, size_t j) {
            if (p.key[i] == p.key[j])
                return;

            WeekRule scenario;
            if (!combineRules(p.rule[i], p.rule[j], scenario))
                return;

            if (isCombinedClass(p, i, j))
                return;

            if (!reportedRoomPairs.insert({i, j}).second)
                return;

            addClash(clashes, ClashType::Room, i, j,
                     "Room " + room +
                     " is assigned to two overlapping sessions.",
                     scenario);
        });
    }

    // Section clashes. Different lab groups (G1 / G2) of one section
    // legitimately run in parallel.
    for (const auto& entry : sectionGroups) {
        const string& section = entry.first;

        sweepResource(entry.second, p, [&](size_t i, size_t j) {
            if (p.key[i] == p.key[j])
                return;

            if (!p.group[i].empty() && !p.group[j].empty() &&
                p.group[i] != p.group[j])
                return;

            WeekRule scenario;
            if (!combineRules(p.rule[i], p.rule[j], scenario))
                return;

            addClash(clashes, ClashType::Section, i, j,
                     "Section " + section +
                     " has two overlapping sessions.",
                     scenario);
        });
    }

    // Duplicate sessions.
    for (const auto& entry : duplicateGroups) {
        const vector<size_t>& indices = entry.second;

        for (size_t a = 0; a < indices.size(); ++a) {
            for (size_t b = a + 1; b < indices.size(); ++b) {
                addClash(clashes, ClashType::DuplicateSession,
                         indices[a], indices[b],
                         "The same timetable session appears more than once.",
                         p.rule[indices[a]]);
            }
        }
    }

    return clashes;
}

void ClashDetector::printClashes(
    const vector<Clash>& clashes,
    const vector<Session>& sessions
) {
    cout << "\n==========================================\n";
    cout << "          CLASH DETECTION REPORT\n";
    cout << "==========================================\n";

    if (clashes.empty()) {
        cout << "No clashes detected.\n";
        return;
    }

    for (size_t i = 0; i < clashes.size(); ++i) {
        const Clash& clash = clashes[i];

        cout << "\n[" << i + 1 << "] "
             << clashTypeName(clash.type) << '\n';

        cout << "Reason: " << clash.description << '\n';

        cout << "Session 1: " << sessions[clash.firstSession] << '\n';

        if (clash.secondSession != NO_SECOND_SESSION) {
            cout << "Session 2: " << sessions[clash.secondSession] << '\n';
        }
    }

    cout << "\n------------------------------------------\n";
    cout << "Total clash records: " << clashes.size() << '\n';
    cout << "==========================================\n";
}
