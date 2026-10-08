#ifndef SESSION_H
#define SESSION_H

#include <string>

struct Session {
    std::string subject;
    std::string faculty;
    std::string room;
    std::string section;
    std::string day;
    std::string startTime;
    std::string endTime;
};

#endif // SESSION_H
