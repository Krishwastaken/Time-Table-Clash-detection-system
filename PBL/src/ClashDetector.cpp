#include "ClashDetector.h"
#include "TimeUtils.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

using namespace std;

using namespace TimeUtils;

namespace {

const size_t NO_SECOND_SESSION = static_cast<size_t>(-1);

string withScenario(const string& text, WeekRule rule) {
    if (rule == WeekRule::SaturdayOffOnly)
        return text + " [only in weeks when Saturday is off]";

    if (rule == WeekRule::SaturdayWorkingOnly)
        return text + " [only in weeks when Saturday is a working day]";

    return text;
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
