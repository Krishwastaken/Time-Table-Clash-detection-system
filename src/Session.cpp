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
    int studentCount
)
    : subject(subject),
      faculty(faculty),
      room(room),
      section(section),
      day(day),
      startTime(startTime),
      endTime(endTime),
      studentCount(studentCount)
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
      studentCount(0)
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

int Session::getRoomCapacity() const
{
    std::string normalizedRoom = room;

    std::transform(
        normalizedRoom.begin(),
        normalizedRoom.end(),
        normalizedRoom.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

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

bool Session::operator<(const Session& other) const {
    if (day != other.day)
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

    return os;
}