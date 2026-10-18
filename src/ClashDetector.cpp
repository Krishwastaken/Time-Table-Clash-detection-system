
#include "ClashDetector.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

using namespace std;

namespace {

const size_t NO_SECOND_SESSION =
    static_cast<size_t>(-1);

// Uppercase and unnessecary whitespace 
string normalize(const string& value) {
    auto first = find_if_not(
        value.begin(), value.end(),
        [](unsigned char c) {
            return isspace(c);
        }
    );

    auto last = find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char c) {
            return isspace(c);
        }
    ).base();

    if (first >= last)
        return "";

    string result(first, last);

    transform(
        result.begin(), result.end(), result.begin(),
        [](unsigned char c) {
            return static_cast<char>(toupper(c));
        }
    );

    return result;
}

// Determine room capacity using normalized room names.
int roomCapacityFor(const Session& session) {
    string room = normalize(session.getRoom());

    if (room.rfind("CR", 0) == 0)
        return 100;

    if (room.rfind("LT", 0) == 0)
        return 200;

    return 0;
}

// Convert HH:MM to minutes after midnight.
// Return -1 for invalid time values.
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

bool validInterval(const Session& session) {
    int start = toMinutes(session.getStartTime());
    int end = toMinutes(session.getEndTime());

    return start >= 0 && end >= 0 && start < end;
}

// Strict overlap: sessions touching at a boundary do not clash.
bool overlaps(const Session& a, const Session& b) {
    if (normalize(a.getDay()) != normalize(b.getDay()))
        return false;

    if (!validInterval(a) || !validInterval(b))
        return false;

    int aStart = toMinutes(a.getStartTime());
    int aEnd = toMinutes(a.getEndTime());
    int bStart = toMinutes(b.getStartTime());
    int bEnd = toMinutes(b.getEndTime());

    return aStart < bEnd && bStart < aEnd;
}

// Exclude non-class activities by subject.
bool isExcluded(const Session& session) {
    string subject = normalize(session.getSubject());

    return subject == "LUNCH" ||
           subject == "LIB" ||
           subject == "SWAYAM" ||
           subject.find("TRAINING") != string::npos ||
           subject.find("EXTRA-CURRICULAR ACTIVITIES")
               != string::npos;
}

// Create a unique, delimiter-safe session signature.
string makeSessionKey(const Session& session) {
    string key;

    auto append = [&key](const string& value) {
        key += to_string(value.size());
        key += ':';
        key += value;
    };

    append(normalize(session.getSubject()));
    append(normalize(session.getFaculty()));
    append(normalize(session.getRoom()));
    append(normalize(session.getSection()));
    append(normalize(session.getDay()));

    // Use normalized time values so equivalent valid times match.
    append(to_string(toMinutes(session.getStartTime())));
    append(to_string(toMinutes(session.getEndTime())));
    append(to_string(session.getStudentCount()));

    return key;
}

bool sameSession(const Session& a, const Session& b) {
    return makeSessionKey(a) == makeSessionKey(b);
}

void addClash(
    vector<Clash>& clashes,
    ClashType type,
    size_t first,
    size_t second,
    const string& description
) {
    clashes.push_back({type, first, second, description});
}

string clashTypeName(ClashType type) {
    switch (type) {
        case ClashType::Faculty:
            return "FACULTY CLASH";

        case ClashType::Room:
            return "ROOM CLASH";

        case ClashType::Section:
            return "SECTION CLASH";

        case ClashType::RoomCapacity:
            return "ROOM CAPACITY CLASH";

        case ClashType::DuplicateSession:
            return "DUPLICATE SESSION";
    }

    return "UNKNOWN CLASH";
}

// Compare only sessions belonging to the same hash-map group.
template <typename DescriptionFunction>
void checkHashedGroups(
    const vector<Session>& sessions,
    const unordered_map<
        string, vector<size_t>
    >& groups,
    ClashType type,
    DescriptionFunction getDescription,
    vector<Clash>& clashes
) {
    for (const auto& entry : groups) {
        const string& resource = entry.first;
        const vector<size_t>& indices = entry.second;

        for (size_t a = 0; a < indices.size(); ++a) {
            for (size_t b = a + 1; b < indices.size(); ++b) {
                size_t i = indices[a];
                size_t j = indices[b];

                // Exact duplicates get their own clash classification.
                if (sameSession(sessions[i], sessions[j]))
                    continue;

                if (!overlaps(sessions[i], sessions[j]))
                    continue;

                addClash(
                    clashes,
                    type,
                    i,
                    j,
                    getDescription(resource)
                );
            }
        }
    }
}

} // namespace

vector<Clash> ClashDetector::detectClashes(
    const vector<Session>& sessions
) {
    vector<Clash> clashes;

    using SessionGroups =
        unordered_map<string, vector<size_t>>;

    SessionGroups facultyGroups;
    SessionGroups roomGroups;
    SessionGroups sectionGroups;
    SessionGroups duplicateGroups;

    // One pass: validate sessions, check capacity, and build hash maps.
    for (size_t i = 0; i < sessions.size(); ++i) {
        const Session& session = sessions[i];

        if (isExcluded(session))
            continue;

        if (!validInterval(session))
            continue;

        // 1. Room capacity clash.
        int students = session.getStudentCount();
        int capacity = roomCapacityFor(session);

        if (students >= 0 && capacity > 0 && students > capacity) {
            ostringstream message;

            message << session.getRoom()
                    << " has capacity " << capacity
                    << ", but " << session.getSubject()
                    << " (" << session.getSection()
                    << ") has " << students << " students.";

            addClash(
                clashes,
                ClashType::RoomCapacity,
                i,
                NO_SECOND_SESSION,
                message.str()
            );
        }

        // 2. Group by faculty.
        string faculty = normalize(session.getFaculty());

        if (!faculty.empty())
            facultyGroups[faculty].push_back(i);

        // 3. Group by room.
        string room = normalize(session.getRoom());

        if (!room.empty())
            roomGroups[room].push_back(i);

        // 4. Group by section.
        string section = normalize(session.getSection());

        if (!section.empty())
            sectionGroups[section].push_back(i);

        // 5. Group exact duplicate-session signatures.
        duplicateGroups[makeSessionKey(session)].push_back(i);
    }

    // Faculty clashes.
    checkHashedGroups(
        sessions,
        facultyGroups,
        ClashType::Faculty,
        [](const string& faculty) {
            return faculty +
                " is assigned to two overlapping sessions.";
        },
        clashes
    );

    // Room clashes.
    checkHashedGroups(
        sessions,
        roomGroups,
        ClashType::Room,
        [](const string& room) {
            return "Room " + room +
                " is assigned to two overlapping sessions.";
        },
        clashes
    );

    // Section clashes.
    checkHashedGroups(
        sessions,
        sectionGroups,
        ClashType::Section,
        [](const string& section) {
            return "Section " + section +
                " has two overlapping sessions.";
        },
        clashes
    );

    // Duplicate sessions.
    for (const auto& entry : duplicateGroups) {
        const vector<size_t>& indices = entry.second;

        for (size_t a = 0; a < indices.size(); ++a) {
            for (size_t b = a + 1; b < indices.size(); ++b) {
                addClash(
                    clashes,
                    ClashType::DuplicateSession,
                    indices[a],
                    indices[b],
                    "The same timetable session appears more than once."
                );
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

        cout << "Reason: "
                  << clash.description << '\n';

        cout << "Session 1: "
                  << sessions[clash.firstSession] << '\n';

        if (clash.secondSession != NO_SECOND_SESSION) {
            cout << "Session 2: "
                      << sessions[clash.secondSession] << '\n';
        }
    }

    cout << "\n------------------------------------------\n";
    cout << "Total clash records: " << clashes.size() << '\n';
    cout << "==========================================\n";
}
