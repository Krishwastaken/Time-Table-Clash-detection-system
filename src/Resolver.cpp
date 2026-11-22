#include "Resolver.h"
#include "TimeUtils.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <ostream>
#include <set>
#include <utility>

using namespace std;
using namespace TimeUtils;

namespace {

const size_t NONE = static_cast<size_t>(-1);

// Pre-computed facts about one session, so the search never re-parses strings.
struct Info {
    int day = 7, start = -1, end = -1, students = 0, priority = 0;
    WeekRule rule = WeekRule::Always;
    string subject;                 // upper case, with the "(G1)" tag
    string baseSubject;             // upper case, without the group tag
    vector<string> rooms;           // normalised physical rooms
    bool free = false, valid = false, lab = false;
};

// Who yields first when two sessions fight over a room (lower = moved first):
// electives, then labs, then core subjects; a "Dr." is kept in place a little
// more firmly than other staff (senior faculty are a preference, not a clash).
int priorityOf(const Session& s, const string& subject) {
    int p = 2;
    if (subject.find("ELECTIVE") != string::npos) p = 0;
    else if (subject.find("LAB") != string::npos) p = 1;
    if (normalize(s.getFaculty()).rfind("DR", 0) == 0) p += 3;
    return p;
}

Info makeInfo(const Session& s) {
    Info x;
    x.day = s.getDayIndex();
    x.start = toMinutes(s.getStartTime());
    x.end = toMinutes(s.getEndTime());
    x.students = s.getStudentCount();
    x.rule = effectiveRule(s);
    x.subject = normalize(s.getSubject());
    x.baseSubject = x.subject;
    const string group = extractGroup(x.subject);
    if (!group.empty()) {
        const size_t pos = x.baseSubject.find("(" + group + ")");
        if (pos != string::npos)
            x.baseSubject = normalize(x.baseSubject.erase(pos, group.size() + 2));
    }
    x.rooms = roomNames(s.getRoom());
    x.free = isFreeActivity(x.subject);
    x.valid = validInterval(s);
    x.lab = x.subject.find("LAB") != string::npos || normalizeRoom(s.getRoom()).find("LAB") != string::npos;
    x.priority = priorityOf(s, x.subject);
    return x;
}

bool timesOverlap(const Info& a, const Info& b) {
    return a.day == b.day && a.start < b.end && b.start < a.end;
}

string placeText(const Session& s) {
    return s.getDay() + " " + s.getStartTime() + "-" + s.getEndTime() + " (" + s.getRoom() + ")";
}

string csvField(const string& s) {
    if (s.find_first_of(",\"\r\n") == string::npos)
        return s;

    string result = "\"";
    for (char c : s) {
        if (c == '"') result += '"';
        result += c;
    }
    return result + "\"";
}

const char* ruleCode(WeekRule r) {
    return r == WeekRule::SaturdayOffOnly ? "SAT_OFF"
         : r == WeekRule::SaturdayWorkingOnly ? "SAT_WORK" : "ALWAYS";
}

string why(const Clash& c) {
    switch (c.type) {
        case ClashType::Faculty:
            return "The same teacher is in two places at once. Fixing it needs a different "
                   "time or teacher, and the timetable is fixed.";
        case ClashType::Section:
            return "The section has two classes at the same time. Fixing it needs a "
                   "different time, and the timetable is fixed.";
        case ClashType::Room:
            return "No other suitable room is free: a replacement must be the same kind of "
                   "room, big enough, and free in the same weeks.";
        case ClashType::RoomCapacity:
            return "No bigger suitable room is free at this time.";
        case ClashType::DuplicateSession:
            return "Duplicate row.";
    }
    return "";
}

// Holds the timetable state while rooms are being re-assigned.
class RoomPlanner {
public:
    RoomPlanner(const vector<Session>& sessions, const vector<char>& removed,
                const ClashDetector::RoomPools& pools)
        : sessions_(sessions), removed_(removed), pools_(pools) {
        info_.resize(sessions.size());

        for (size_t i = 0; i < sessions.size(); ++i) {
            info_[i] = makeInfo(sessions[i]);
            if (removed_[i] || info_[i].free || !info_[i].valid)
                continue;

            const vector<string> display = displayNames(sessions[i].getRoom());
            for (size_t k = 0; k < info_[i].rooms.size(); ++k) {
                const string& room = info_[i].rooms[k];
                inRoom_[room].push_back(i);
                display_.emplace(room, display.size() > k ? display[k] : room);
                if (info_[i].lab)
                    labHosts_[info_[i].baseSubject].insert(room);
            }
        }
    }

    const Info& info(size_t i) const { return info_[i]; }

    // Does session i currently have a room problem?
    bool hasRoomProblem(size_t i) const {
        const Info& a = info_[i];
        if (removed_[i] || a.free || !a.valid || a.rooms.empty())
            return false;

        const int cap = capacityOfRoomName(sessions_[i].getRoom());
        if (cap > 0 && a.students > cap)
            return true;

        for (const string& room : a.rooms)
            if (roomIsOverbooked(i, room))
                return true;

        return false;
    }

    // Sections that share one lecture (same subject, room, time and weeks).
    // They must move together, otherwise the lecture splits and the teacher
    // ends up in several rooms at once.
    vector<size_t> sharedGroup(size_t i) const {
        vector<size_t> members = { i };
        const Info& a = info_[i];

        if (a.lab || a.rooms.size() != 1)
            return members;

        auto it = inRoom_.find(a.rooms[0]);
        if (it == inRoom_.end())
            return members;

        for (size_t j : it->second) {
            const Info& b = info_[j];
            if (j != i && !removed_[j] && b.day == a.day && b.start == a.start &&
                b.end == a.end && b.rule == a.rule && b.subject == a.subject)
                members.push_back(j);
        }

        return members;
    }

    // Can the sessions in `members` (together `students` people) move into
    // `room` without creating a room problem?
    bool canHost(const vector<size_t>& members, int students, const string& room) const {
        const Info& a = info_[members[0]];
        const int cap = capacityOfRoomName(room);

        if (cap <= 0 || students > cap)
            return false;

        auto it = inRoom_.find(room);
        if (it == inRoom_.end())
            return true;

        auto pool = pools_.find(room);
        size_t overlapping = 0;

        for (size_t j : it->second) {
            if (removed_[j] || find(members.begin(), members.end(), j) != members.end() ||
                !timesOverlap(a, info_[j]))
                continue;

            WeekRule both;
            if (!combineRules(a.rule, info_[j].rule, both))
                continue;               // never run in the same week

            ++overlapping;
        }

        return pool == pools_.end() ? overlapping == 0
                                    : overlapping < static_cast<size_t>(pool->second);
    }

    // Rooms the session could move to, best fit (smallest) first.
    vector<string> candidates(size_t i) const {
        const Info& a = info_[i];
        vector<pair<int, string>> found;

        const bool inAudi = !a.rooms.empty() && a.rooms[0].find("AUDI") != string::npos;

        for (const auto& entry : inRoom_) {
            const string& room = entry.first;

            if (find(a.rooms.begin(), a.rooms.end(), room) != a.rooms.end())
                continue;

            const bool roomIsLab = isLabRoom(room);

            if (a.lab != roomIsLab)
                continue;                                   // lab <-> lab, lecture <-> lecture

            if (a.lab) {
                auto hosts = labHosts_.find(a.baseSubject);
                if (hosts == labHosts_.end() || !hosts->second.count(room))
                    continue;                               // only labs that already run this subject
            } else if (!inAudi && room.find("AUDI") != string::npos) {
                continue;                                   // keep the auditorium for its own lectures
            }

            const int cap = capacityOfRoomName(room);
            if (cap > 0)
                found.push_back({cap, room});
        }

        sort(found.begin(), found.end());

        vector<string> rooms;
        for (const auto& f : found)
            rooms.push_back(f.second);
        return rooms;
    }

    void moveTo(size_t i, const string& room) {
        for (const string& old : info_[i].rooms) {
            auto& v = inRoom_[old];
            v.erase(remove(v.begin(), v.end(), i), v.end());
        }

        info_[i].rooms = { room };
        inRoom_[room].push_back(i);
    }

    string displayName(const string& room) const {
        auto it = display_.find(room);
        return it == display_.end() ? room : it->second;
    }

private:
    // "Lab-7 & Lab-6" -> {"Lab-7", "Lab-6"} in the spelling used in the timetable.
    static vector<string> displayNames(const string& raw) {
        vector<string> names;
        string token;
        for (size_t i = 0; i <= raw.size(); ++i) {
            if (i == raw.size() || raw[i] == '&') {
                size_t a = token.find_first_not_of(" \t"), b = token.find_last_not_of(" \t");
                if (a != string::npos) names.push_back(token.substr(a, b - a + 1));
                token.clear();
            } else {
                token += raw[i];
            }
        }
        return names;
    }

    bool roomIsOverbooked(size_t i, const string& room) const {
        const Info& a = info_[i];
        auto it = inRoom_.find(room);
        if (it == inRoom_.end())
            return false;

        auto pool = pools_.find(room);
        size_t overlapping = 0;

        for (size_t j : it->second) {
            if (j == i || removed_[j] || !timesOverlap(a, info_[j]))
                continue;

            const Info& b = info_[j];
            WeekRule both;
            if (!combineRules(a.rule, b.rule, both))
                continue;

            // A lecture shared by several sections is one class, not a clash.
            const bool shared = !a.lab && !b.lab && room != "UNKNOWN" &&
                                a.start == b.start && a.end == b.end &&
                                a.rule == b.rule && a.subject == b.subject;
            if (shared)
                continue;

            ++overlapping;
            if (pool == pools_.end())
                return true;
        }

        return pool != pools_.end() && overlapping >= static_cast<size_t>(pool->second);
    }

    const vector<Session>& sessions_;
    const vector<char>& removed_;
    const ClashDetector::RoomPools& pools_;
    vector<Info> info_;
    map<string, vector<size_t>> inRoom_;
    map<string, string> display_;
    map<string, set<string>> labHosts_;
};

}  

namespace Resolver {

ResolveResult resolve(vector<Session>& sessions, const vector<Clash>& clashes, const Config& cfg) {
    ResolveResult result;
    result.clashesBefore = clashes.size();

    const size_t n = sessions.size();
    vector<char> removed(n, false);

    // Step 1: exact duplicates are data-entry errors; the later copy is deleted.
    for (const Clash& c : clashes) {
        if (c.type != ClashType::DuplicateSession || c.firstSession >= n ||
            c.secondSession >= n || removed[c.secondSession])
            continue;

        removed[c.secondSession] = true;
        const Session& d = sessions[c.secondSession];

        result.applied.push_back({
            FixKind::RemoveDuplicate, c.secondSession,
            d.getSubject(), d.getSection(), d.getFaculty(),
            placeText(d), "",
            "exact duplicate of session #" + to_string(c.firstSession + 1)
        });
    }

    // Step 2: build the conflict graph. Vertices are sessions, an edge joins
    // two sessions that fight over the same room at the same time. Sessions
    // with a capacity problem are vertices without an edge.
    RoomPlanner planner(sessions, removed, cfg.roomPools);
    vector<size_t> degree(n, 0);
    vector<char> needsRoom(n, false);

    for (const Clash& c : clashes) {
        if (c.type != ClashType::Room && c.type != ClashType::RoomCapacity)
            continue;

        for (size_t s : {c.firstSession, c.secondSession}) {
            if (s < n && !removed[s]) {
                needsRoom[s] = true;
                if (c.type == ClashType::Room)
                    ++degree[s];
            }
        }
    }

    // Step 3: greedy colouring. A "colour" is a room. Sessions are taken in
    // order of who should yield first (priority), then by number of conflicts
    // (Welsh-Powell: most constrained first). Each gets the first suitable
    // room that is free in every week it runs. A session whose clash was
    // already cured by a partner moving away is left alone.
    vector<size_t> order;
    for (size_t i = 0; i < n; ++i)
        if (needsRoom[i])
            order.push_back(i);

    stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (planner.info(a).priority != planner.info(b).priority)
            return planner.info(a).priority < planner.info(b).priority;
        return degree[a] > degree[b];
    });

    for (size_t i : order) {
        if (!planner.hasRoomProblem(i))
            continue;

        const vector<size_t> members = planner.sharedGroup(i);
        int total = 0;

        for (size_t m : members) {
            total += sessions[m].getStudentCount();
        }

        // A shared lecture moves only to a room that fits every section;
        // otherwise it stays and the clash is reported as unresolved.
        string chosen;
        for (const string& room : planner.candidates(i)) {
            if (planner.canHost(members, total, room)) {
                chosen = room;
                break;
            }
        }

        if (chosen.empty())
            continue;

        const string newRoom = planner.displayName(chosen);

        for (size_t m : members) {
            const Session me = sessions[m];
            const int cap = capacityOfRoomName(me.getRoom());
            const bool tooSmall = cap > 0 && me.getStudentCount() > cap;

            result.applied.push_back({
                FixKind::ChangeRoom, m,
                me.getSubject(), me.getSection(), me.getFaculty(),
                placeText(me),
                me.getDay() + " " + me.getStartTime() + "-" + me.getEndTime() + " (" + newRoom + ")",
                members.size() > 1
                    ? "shared lecture of " + to_string(members.size()) + " sections moved together"
                    : (tooSmall ? "room too small for " + to_string(me.getStudentCount()) + " students"
                                : "room double-booked")
            });

            sessions[m] = Session(me.getSubject(), me.getFaculty(), newRoom, me.getSection(),
                                  me.getDay(), me.getStartTime(), me.getEndTime(),
                                  me.getStudentCount(), me.getWeekRule());
            planner.moveTo(m, chosen);
        }
    }

    // Step 4: drop removed duplicates, then re-verify with the real detector.
    vector<Session> kept;
    kept.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (!removed[i]) {
            kept.push_back(sessions[i]);
            result.originalIndex.push_back(i);
        }
    }
    sessions = std::move(kept);

    result.remaining = ClashDetector::detectClashes(sessions, cfg.ignorePresidentClass, cfg.roomPools);
    result.clashesAfter = result.remaining.size();

    for (const Clash& c : result.remaining) {
        result.unresolved.push_back({
            c.type, c.firstSession, c.secondSession, c.scenario, c.description, why(c)
        });
    }

    return result;
}

void print(ostream& out, const ResolveResult& r) {
    out << "\n==================== CLASH RESOLVER ====================\n";
    out << "Conflict records before: " << r.clashesBefore
        << " | after: " << r.clashesAfter << "\n";
    out << "Class times are fixed: only rooms are changed.\n";

    if (r.applied.empty())
        out << "\nNo changes proposed.\n";

    for (size_t i = 0; i < r.applied.size(); ++i) {
        const auto& x = r.applied[i];

        out << "\n[" << setw(3) << setfill('0') << i + 1 << setfill(' ') << "] "
            << (x.kind == FixKind::ChangeRoom ? "ROOM" : "REMOVE")
            << " | #" << x.sessionIndex + 1 << " " << x.subject
            << " | " << x.section << " | " << x.faculty << "\n";
        out << "  From : " << x.from << "\n";
        if (!x.to.empty())
            out << "  To   : " << x.to << "\n";
        out << "  Why  : " << x.reason << "\n";
    }

    if (!r.unresolved.empty()) {
        out << "\n---- Could not be resolved (" << r.unresolved.size() << ") ----\n";
        size_t shown = 0;
        for (const auto& u : r.unresolved) {
            if (++shown > 15) {
                out << "  ... and " << r.unresolved.size() - 15
                    << " more (see unresolved_clashes.csv)\n";
                break;
            }
            out << "  - " << u.description << "\n    " << u.reason << "\n";
        }
    }

    out << "========================================================\n";
}

bool exportTimetable(const string& path, const vector<Session>& sessions) {
    ofstream f(path);
    if (!f)
        return false;

    f << "subject,faculty,room,section,day,startTime,endTime,studentCount,weekRule\n";

    for (const auto& s : sessions) {
        f << csvField(s.getSubject()) << ',' << csvField(s.getFaculty()) << ','
          << csvField(s.getRoom()) << ',' << csvField(s.getSection()) << ','
          << csvField(s.getDay()) << ',' << csvField(s.getStartTime()) << ','
          << csvField(s.getEndTime()) << ',' << s.getStudentCount() << ','
          << ruleCode(s.getWeekRule()) << '\n';
    }

    return static_cast<bool>(f);
}

bool exportChangeLog(const string& path, const ResolveResult& r) {
    ofstream f(path);
    if (!f)
        return false;

    f << "change,session,subject,section,faculty,from,to,reason\n";

    for (const auto& x : r.applied) {
        f << (x.kind == FixKind::ChangeRoom ? "ROOM CHANGED" : "REMOVED") << ','
          << x.sessionIndex + 1 << ',' << csvField(x.subject) << ','
          << csvField(x.section) << ',' << csvField(x.faculty) << ','
          << csvField(x.from) << ',' << csvField(x.to) << ',' << csvField(x.reason) << '\n';
    }

    return static_cast<bool>(f);
}

bool exportUnresolved(const string& path, const ResolveResult& r, const vector<Session>& resolved) {
    ofstream f(path);
    if (!f)
        return false;

    f << "type,weeks,sessionA,sectionA,sessionB,sectionB,description,reason\n";

    for (const auto& u : r.unresolved) {
        const Session& a = resolved[u.first];
        const bool two = u.second != NONE && u.second < resolved.size();

        static const char* const typeNames[] = {"FACULTY", "ROOM", "SECTION", "CAPACITY", "DUPLICATE"};

        f << typeNames[static_cast<int>(u.type)] << ',' << ruleCode(u.scenario) << ','
          << csvField(a.getSubject() + " " + a.getDay() + " " + a.getStartTime()) << ','
          << csvField(a.getSection()) << ','
          << csvField(two ? resolved[u.second].getSubject() + " " + resolved[u.second].getDay() + " " + resolved[u.second].getStartTime() : "") << ','
          << csvField(two ? resolved[u.second].getSection() : "") << ','
          << csvField(u.description) << ',' << csvField(u.reason) << '\n';
    }

    return static_cast<bool>(f);
}

} 
