#include "Resolver.h"
#include "ClashDetector.h"
#include "TimeUtils.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <ostream>
#include <queue>
#include <set>
#include <tuple>
#include <utility>

using namespace std;
using namespace TimeUtils;

namespace {

constexpr size_t NONE = static_cast<size_t>(-1);

// Pre-normalised copy of a session, so the search loops never re-parse strings.
struct Info {
    string day, faculty, room, section;
    int start = -1, end = -1, students = 0;
    bool valid = false, excluded = false, lunch = false;
};

Info makeInfo(const Session& s) {
    Info x;
    x.day = normalize(s.getDay());
    x.faculty = normalize(s.getFaculty());
    x.room = normalize(s.getRoom());
    x.section = normalize(s.getSection());
    x.start = toMinutes(s.getStartTime());
    x.end = toMinutes(s.getEndTime());
    x.students = s.getStudentCount();
    x.valid = validInterval(s);
    x.excluded = isExcluded(s);
    x.lunch = normalize(s.getSubject()) == "LUNCH";
    return x;
}

bool timesOverlap(int aStart, int aEnd, int bStart, int bEnd) {
    return aStart < bEnd && bStart < aEnd;
}

struct Flags {
    bool capacity = false, room = false, faculty = false, section = false;
    bool any() const { return capacity || room || faculty || section; }
};

// Which kinds of conflict does session i currently have?
Flags flagsFor(const vector<Info>& info, const vector<char>& removed, size_t i) {
    Flags f;
    const Info& a = info[i];

    if (removed[i] || a.excluded || !a.valid)
        return f;

    int cap = roomCapacityFor(a.room);
    if (cap > 0 && a.students > cap)
        f.capacity = true;

    for (size_t j = 0; j < info.size(); ++j) {
        const Info& b = info[j];

        if (j == i || removed[j] || b.excluded || !b.valid || b.day != a.day)
            continue;
        if (!timesOverlap(a.start, a.end, b.start, b.end))
            continue;

        if (b.faculty == a.faculty) f.faculty = true;
        if (b.room == a.room)       f.room = true;
        if (b.section == a.section) f.section = true;
    }

    return f;
}

// Can session `self` be placed at (day, start..end, room) without any clash?
// `day` and `room` must already be normalised.
bool isFree(const vector<Info>& info, const vector<char>& removed, size_t self,
            const string& day, int start, int end, const string& room) {
    const Info& me = info[self];

    int cap = roomCapacityFor(room);
    if (cap > 0 && me.students > cap)
        return false;

    for (size_t j = 0; j < info.size(); ++j) {
        const Info& o = info[j];

        if (j == self || removed[j] || !o.valid || o.day != day)
            continue;
        if (!timesOverlap(start, end, o.start, o.end))
            continue;

        if (o.excluded) {
            // Ignored by the detector, but never schedule over a section's lunch break.
            if (o.lunch && o.section == me.section)
                return false;
            continue;
        }

        if (o.faculty == me.faculty || o.room == room || o.section == me.section)
            return false;
    }

    return true;
}

string describeFlags(const Flags& f) {
    vector<string> parts;
    if (f.faculty)  parts.push_back("faculty clash");
    if (f.room)     parts.push_back("room clash");
    if (f.section)  parts.push_back("section clash");
    if (f.capacity) parts.push_back("room too small");

    string text;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) text += ", ";
        text += parts[i];
    }
    return text;
}

string placeText(const string& day, const string& start, const string& end, const string& room) {
    return day + " " + start + "-" + end + " (" + room + ")";
}

string placeText(const Session& s) {
    return placeText(s.getDay(), s.getStartTime(), s.getEndTime(), s.getRoom());
}

struct Candidate {
    long cost;
    int day;      // weekday rank
    int start;    // minutes
    bool original;
};

string csvField(const string& s) {
    if (s.find_first_of(",\"\r\n") == string::npos)
        return s;

    string result = "\"";
    for (char c : s) {
        if (c == '"')
            result += '"';
        result += c;
    }
    return result + "\"";
}

}  // namespace

namespace Resolver {

ResolveResult resolve(vector<Session>& sessions, const vector<Clash>& clashes, const Config& cfg) {
    ResolveResult result;
    result.clashesBefore = clashes.size();

    const size_t n = sessions.size();
    vector<char> removed(n, false);
    vector<size_t> degree(n, 0);

    // Step 1: exact duplicates are data-entry errors, so the later copy is deleted, not moved.
    // Every other clash raises the "conflict degree" of the sessions involved.
    for (const auto& c : clashes) {
        if (c.firstSession >= n)
            continue;

        if (c.type == ClashType::DuplicateSession) {
            if (c.secondSession < n && !removed[c.secondSession]) {
                removed[c.secondSession] = true;

                const Session& d = sessions[c.secondSession];
                result.applied.push_back({
                    FixKind::RemoveDuplicate, c.secondSession,
                    d.getSubject(), d.getSection(), d.getFaculty(),
                    placeText(d), "",
                    "exact duplicate of session #" + to_string(c.firstSession + 1)
                });
            }
            continue;
        }

        ++degree[c.firstSession];
        if (c.secondSession != NONE && c.secondSession < n)
            ++degree[c.secondSession];
    }

    vector<Info> info(n);
    for (size_t i = 0; i < n; ++i)
        info[i] = makeInfo(sessions[i]);

    // Step 2: what the timetable looks like (working days, time window, rooms).
    map<int, string> dayLabel;   // weekday rank -> spelling used in the CSV
    int lo = INT_MAX, hi = -1;

    for (size_t i = 0; i < n; ++i) {
        if (removed[i] || info[i].excluded || !info[i].valid)
            continue;

        int rank = dayRank(sessions[i].getDay());
        if (rank < 7 && !dayLabel.count(rank))
            dayLabel[rank] = sessions[i].getDay();

        lo = min(lo, info[i].start);
        hi = max(hi, info[i].end);
    }

    // Rooms with a known capacity, smallest first (best fit).
    vector<pair<int, string>> rooms;
    set<string> seenRooms;
    for (size_t i = 0; i < n; ++i) {
        if (removed[i])
            continue;

        int cap = roomCapacityFor(sessions[i].getRoom());
        if (cap > 0 && seenRooms.insert(info[i].room).second)
            rooms.push_back({cap, sessions[i].getRoom()});
    }
    sort(rooms.begin(), rooms.end());

    // Step 3: greedy repair. Priority queue = most conflicted session first.
    priority_queue<pair<size_t, size_t>> heap;   // (degree, n - index) so lower index wins ties
    for (size_t i = 0; i < n; ++i)
        if (!removed[i] && degree[i] > 0)
            heap.push({degree[i], n - i});

    while (!heap.empty()) {
        size_t i = n - heap.top().second;
        heap.pop();

        if (removed[i])
            continue;

        // A partner that was already moved may have solved this one for free.
        Flags f = flagsFor(info, removed, i);
        if (!f.any())
            continue;

        const Session me = sessions[i];
        const Info orig = info[i];

        if (lo > hi || orig.end <= orig.start)
            continue;

        const bool needTime = f.faculty || f.section;   // changing the room cannot fix these
        const int duration = orig.end - orig.start;
        const int rank = dayRank(me.getDay());

        // Candidate slots, closest to the original time first.
        vector<Candidate> cands;
        if (!needTime)
            cands.push_back({0, rank, orig.start, true});

        for (const auto& entry : dayLabel) {
            for (int s = lo; s + duration <= hi; s += max(1, cfg.slotStep)) {
                if (entry.first == rank && s == orig.start)
                    continue;
                cands.push_back({
                    static_cast<long>(abs(entry.first - rank)) * 10000 + abs(s - orig.start),
                    entry.first, s, false
                });
            }
        }

        stable_sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) {
            return make_tuple(a.cost, a.day, a.start) < make_tuple(b.cost, b.day, b.start);
        });

        // Alternative rooms that are big enough.
        vector<string> altRooms;
        for (const auto& r : rooms)
            if (r.first >= orig.students && normalize(r.second) != orig.room)
                altRooms.push_back(r.second);

        bool fixed = false;

        for (const auto& c : cands) {
            string dayText = c.original ? me.getDay() : dayLabel[c.day];
            string dayNorm = normalize(dayText);

            vector<string> tryRooms;
            if (!c.original)
                tryRooms.push_back(me.getRoom());   // prefer keeping the room
            for (const auto& r : altRooms)
                tryRooms.push_back(r);

            for (const auto& room : tryRooms) {
                string roomNorm = normalize(room);

                if (!isFree(info, removed, i, dayNorm, c.start, c.start + duration, roomNorm))
                    continue;

                const string newStart = c.original ? me.getStartTime() : formatTime(c.start);
                const string newEnd = c.original ? me.getEndTime() : formatTime(c.start + duration);

                const bool timeChanged = !c.original;

                result.applied.push_back({
                    timeChanged ? FixKind::MoveTime : FixKind::ChangeRoom, i,
                    me.getSubject(), me.getSection(), me.getFaculty(),
                    placeText(me), placeText(dayText, newStart, newEnd, room),
                    describeFlags(f)
                });

                sessions[i] = Session(me.getSubject(), me.getFaculty(), room, me.getSection(),
                                      dayText, newStart, newEnd, me.getStudentCount());
                info[i] = makeInfo(sessions[i]);
                fixed = true;
                break;
            }

            if (fixed)
                break;
        }
        // If nothing fits, the session stays as it is and shows up in the "after" count.
    }

    // Step 4: drop removed duplicates, then re-verify with the real detector.
    vector<Session> kept;
    kept.reserve(n);
    for (size_t i = 0; i < n; ++i)
        if (!removed[i])
            kept.push_back(sessions[i]);
    sessions = std::move(kept);

    result.clashesAfter = ClashDetector::detectClashes(sessions).size();
    return result;
}

void print(ostream& out, const ResolveResult& r) {
    out << "\n==================== CLASH RESOLVER ====================\n";
    out << "Conflict records before: " << r.clashesBefore
        << " | after: " << r.clashesAfter << "\n";

    if (r.applied.empty())
        out << "\nNo changes proposed.\n";

    for (size_t i = 0; i < r.applied.size(); ++i) {
        const auto& x = r.applied[i];

        const char* label = x.kind == FixKind::MoveTime ? "MOVE"
                          : x.kind == FixKind::ChangeRoom ? "ROOM"
                          : "REMOVE";

        out << "\n[" << setw(3) << setfill('0') << i + 1 << setfill(' ') << "] "
            << label << " | #" << x.sessionIndex + 1 << " " << x.subject
            << " | " << x.section << " | " << x.faculty << "\n";
        out << "  From : " << x.from << "\n";
        if (!x.to.empty())
            out << "  To   : " << x.to << "\n";
        out << "  Why  : " << x.reason << "\n";
    }

    if (r.clashesAfter > 0)
        out << "\n" << r.clashesAfter
            << " conflict record(s) could not be resolved automatically.\n";

    out << "========================================================\n";
}

bool exportTimetable(const string& path, const vector<Session>& sessions) {
    ofstream f(path);
    if (!f)
        return false;

    f << "subject,faculty,room,section,day,startTime,endTime,studentCount\n";

    for (const auto& s : sessions) {
        f << csvField(s.getSubject()) << ',' << csvField(s.getFaculty()) << ','
          << csvField(s.getRoom()) << ',' << csvField(s.getSection()) << ','
          << csvField(s.getDay()) << ',' << csvField(s.getStartTime()) << ','
          << csvField(s.getEndTime()) << ',' << s.getStudentCount() << '\n';
    }

    return static_cast<bool>(f);
}

bool exportChangeLog(const string& path, const ResolveResult& r) {
    ofstream f(path);
    if (!f)
        return false;

    f << "change,session,subject,section,faculty,from,to,reason\n";

    for (const auto& x : r.applied) {
        const char* label = x.kind == FixKind::MoveTime ? "MOVED"
                          : x.kind == FixKind::ChangeRoom ? "ROOM CHANGED"
                          : "REMOVED";

        f << label << ',' << x.sessionIndex + 1 << ',' << csvField(x.subject) << ','
          << csvField(x.section) << ',' << csvField(x.faculty) << ','
          << csvField(x.from) << ',' << csvField(x.to) << ',' << csvField(x.reason) << '\n';
    }

    return static_cast<bool>(f);
}

} 
