#ifndef TIME_UTILS_H
#define TIME_UTILS_H

// Small helpers shared by ClashDetector and Resolver, so both always agree on
// what a room name, a faculty list, a time or a "free period" means.

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include "Session.h"

namespace TimeUtils {

using std::string;
using std::vector;
using std::find;
using std::find_if_not;
using std::transform;
using std::isspace;
using std::isdigit;
using std::toupper;
using std::stoi;
using std::to_string;

// Trim and convert to upper case.
inline string normalize(const string& value) {
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
inline string normalizeRoom(const string& value) {
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
inline int toMinutes(const string& time) {
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
inline WeekRule effectiveRule(const Session& session) {
    if (session.getWeekRule() != WeekRule::Always)
        return session.getWeekRule();

    if (session.getDayIndex() == 5)
        return WeekRule::SaturdayWorkingOnly;

    return WeekRule::Always;
}

// Weeks in which BOTH sessions run. Returns false when there is no such
// week, i.e. the pair can never be active together.
inline bool combineRules(WeekRule a, WeekRule b, WeekRule& result) {
    if (a == WeekRule::Always) { result = b; return true; }
    if (b == WeekRule::Always) { result = a; return true; }
    if (a == b)                { result = a; return true; }
    return false;
}

// Free periods and non-class activities never occupy a room or a teacher.
inline bool isFreeActivity(const string& subjectUpper) {
    return subjectUpper == "LUNCH" ||
           subjectUpper == "LIB" ||
           subjectUpper == "SWAYAM" ||
           subjectUpper.find("AUDIT COURSE") != string::npos ||
           subjectUpper.find("TRAINING") != string::npos ||
           subjectUpper.find("EXTRA-CURRICULAR") != string::npos;
}

// "G1", "G2" ... from a subject such as "Data Structures Lab (G1)".
inline string extractGroup(const string& subjectUpper) {
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
inline vector<string> facultyNames(const string& raw, bool ignorePresident) {
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
inline vector<string> roomNames(const string& raw) {
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

// "mm:ss" helper used by the resolver when it needs to print a time.
inline string formatTime(int minutes) {
    string h = to_string(minutes / 60), m = to_string(minutes % 60);
    return string(h.size() < 2 ? "0" : "") + h + ":" + string(m.size() < 2 ? "0" : "") + m;
}

inline bool validInterval(const Session& session) {
    const int start = toMinutes(session.getStartTime());
    const int end = toMinutes(session.getEndTime());
    return session.getDayIndex() < 7 && start >= 0 && end >= 0 && start < end;
}

// Capacity of one named room (0 = unknown), using the same table as the
// Session class.
inline int capacityOfRoomName(const string& room) {
    return Session("", "", room, "", "", "", "", 0).getRoomCapacity();
}

// Laboratories are the only rooms a lab session may be re-assigned to.
inline bool isLabRoom(const string& roomUpper) {
    return roomUpper.find("LAB") != string::npos;
}

} // namespace TimeUtils

#endif
