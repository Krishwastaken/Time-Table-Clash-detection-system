#include "Session.h"

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

int Session::getRoomCapacity() const {
    if (room.rfind("CR", 0) == 0)
        return 100;

    if (room.rfind("LT", 0) == 0)
        return 200;

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