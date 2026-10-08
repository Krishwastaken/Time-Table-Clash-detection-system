#ifndef SESSION_H
#define SESSION_H

#include <string>
#include <iostream>

struct Session {
    std::string subject;
    std::string faculty;
    std::string room;
    std::string section;
    std::string day;
    std::string startTime;
    std::string endTime;
};

// For printing a Session
inline std::ostream& operator<<(std::ostream& os, const Session& s) {
    os << s.subject << " | "
       << s.faculty << " | "
       << s.room << " | "
       << s.section << " | "
       << s.day << " | "
       << s.startTime << " - "
       << s.endTime;

    return os;
}

// For comparing Sessions during Merge Sort
inline bool operator<(const Session& a, const Session& b) {

    // Convert day into proper timetable order
    auto dayOrder = [](const std::string& day) {
        if (day == "Monday") return 1;
        if (day == "Tuesday") return 2;
        if (day == "Wednesday") return 3;
        if (day == "Thursday") return 4;
        if (day == "Friday") return 5;
        if (day == "Saturday") return 6;
        if (day == "Sunday") return 7;
        return 8;
    };

    // First compare day
    if (dayOrder(a.day) != dayOrder(b.day))
        return dayOrder(a.day) < dayOrder(b.day);

    // If same day, compare start time
    return a.startTime < b.startTime;
}

#endif