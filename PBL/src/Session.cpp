#include "Session.h"
#include <algorithm>
#include <cctype>

Session::Session(
    const std::string& subject,
    const std::string& faculty,
    const std::string& room,
    const std::string& section,
    const std::string& day,
    const std::string& startTime,
    const std::string& endTime,
    int studentCount,
    WeekRule weekRule
)
    : subject(subject),
      faculty(faculty),
      room(room),
      section(section),
      day(day),
      startTime(startTime),
      endTime(endTime),
      studentCount(studentCount),
      weekRule(weekRule)
{
}

Session::Session()
    : subject(""),
      faculty(""),
      room(""),
      section(""),
      day(""),
      startTime(""),
      endTime(""),
      studentCount(0),
      weekRule(WeekRule::Always)
{
}


std::string Session::getSubject() const {
    return subject;
}

std::string Session::getFaculty() const {
    return faculty;
}

std::string Session::getRoom() const {
    return room;
}

std::string Session::getSection() const {
    return section;
}

std::string Session::getDay() const {
    return day;
}

std::string Session::getStartTime() const {
    return startTime;
}

std::string Session::getEndTime() const {
    return endTime;
}

int Session::getStudentCount() const {
    return studentCount;
}

namespace {

// Capacity of ONE room (lower-case name). 0 = unknown.
int capacityOfRoom(const std::string& normalizedRoom)
{
    // Classrooms
    if (normalizedRoom.find("cr") == 0)
        return 80;

    // Lecture theatres
    if (normalizedRoom.find("lt") == 0)
        return 160;

    // Digital laboratories
    if (normalizedRoom.find("digital lab-1") != std::string::npos ||
        normalizedRoom.find("digital lab-2") != std::string::npos)
        return 80;

    // Other laboratories
    if (normalizedRoom.find("lab-1") != std::string::npos ||
    normalizedRoom.find("lab-7") != std::string::npos)
    {
        return 40;
    }

    if (normalizedRoom.find("lab-2") != std::string::npos ||
    normalizedRoom.find("lab-5") != std::string::npos ||
    normalizedRoom.find("lab-6") != std::string::npos)
    {
        return 80;
    }
    
    // Auditorium
    if (normalizedRoom.find("gehu-audi") != std::string::npos)
        return 500;

    return 0;
}


} // namespace

// A session held in two rooms ("Lab-7 & Lab-6") may use both: capacities add up.
int Session::getRoomCapacity() const
{
    std::string lower = room;

    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    int total = 0;
    std::string part;

    for (std::size_t i = 0; i <= lower.size(); ++i) {
        if (i == lower.size() || lower[i] == '&') {
            const int capacity = capacityOfRoom(part);
            part.clear();

            if (capacity == 0)
                return 0;                   // one unknown room: do not guess

            total += capacity;
        } else {
            part += lower[i];
        }
    }

    return total;
}

WeekRule Session::getWeekRule() const {
    return weekRule;
}

void Session::setWeekRule(WeekRule rule) {
    weekRule = rule;
}

void Session::setRoom(const std::string& r) {
    room = r;
}

void Session::setDay(const std::string& d) {
    day = d;
}

void Session::setStartTime(const std::string& s) {
    startTime = s;
}

void Session::setEndTime(const std::string& e) {
    endTime = e;
}

int Session::getDayIndex() const {
    std::string d;
    for (char c : day) {
        if (!std::isspace(static_cast<unsigned char>(c)))
            d += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    static const char* const names[7] = {
        "monday", "tuesday", "wednesday", "thursday",
        "friday", "saturday", "sunday"
    };

    for (int i = 0; i < 7; ++i) {
        const std::string full = names[i];
        if (d == full || d == full.substr(0, 3))
            return i;
    }

    return 7;
}

// Weekday order first (the old version compared the day NAMES as strings,
// which sorted Friday before Monday), then start time (HH:MM sorts correctly
// as text because it is zero padded).
bool Session::operator<(const Session& other) const {
    const int a = getDayIndex();
    const int b = other.getDayIndex();

    if (a != b)
        return a < b;

    if (a == 7 && day != other.day)
        return day < other.day;

    return startTime < other.startTime;
}

std::ostream& operator<<(std::ostream& os, const Session& session) {
    os << session.subject << " | "
       << session.faculty << " | "
       << session.room << " | "
       << session.section << " | "
       << session.day << " | "
       << session.startTime << " - "
       << session.endTime << " | "
       << session.studentCount << " students";

    if (session.weekRule == WeekRule::SaturdayOffOnly)
        os << " [Saturday-off weeks only]";
    else if (session.weekRule == WeekRule::SaturdayWorkingOnly)
        os << " [Saturday-working weeks only]";

    return os;
}