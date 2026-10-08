#ifndef SESSION_H
#define SESSION_H

#include <iostream>
#include <string>

class Session {
private:
    std::string subject;
    std::string faculty;
    std::string room;
    std::string section;
    std::string day;
    std::string startTime;
    std::string endTime;

public:
    Session(
        const std::string& subject,
        const std::string& faculty,
        const std::string& room,
        const std::string& section,
        const std::string& day,
        const std::string& startTime,
        const std::string& endTime
    );

    Session();

    std::string getSubject() const;
    std::string getFaculty() const;
    std::string getRoom() const;
    std::string getSection() const;
    std::string getDay() const;
    std::string getStartTime() const;
    std::string getEndTime() const;

    bool operator<(const Session& other) const;

    friend std::ostream& operator<<(std::ostream& os, const Session& session);
};

#endif