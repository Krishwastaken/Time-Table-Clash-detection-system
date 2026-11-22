#ifndef SESSION_H
#define SESSION_H

#include <iostream>
#include <string>

// Weekly applicability of a session.
//   Always              - runs every week.
//   SaturdayOffOnly     - the "*" classes: they run on their weekday slot
//                         only in weeks when Saturday is an off day.
//   SaturdayWorkingOnly - runs only in weeks when Saturday is a working day
//                         (Saturday classes themselves).
enum class WeekRule {
    Always,
    SaturdayOffOnly,
    SaturdayWorkingOnly
};

class Session {
private:
    std::string subject;
    std::string faculty;
    std::string room;
    std::string section;
    std::string day;
    std::string startTime;
    std::string endTime;
    int studentCount;
    WeekRule weekRule;

public:
    Session(
        const std::string& subject,
        const std::string& faculty,
        const std::string& room,
        const std::string& section,
        const std::string& day,
        const std::string& startTime,
        const std::string& endTime,
        int studentCount,
        WeekRule weekRule = WeekRule::Always
    );

    Session();

    std::string getSubject() const;
    std::string getFaculty() const;
    std::string getRoom() const;
    std::string getSection() const;
    std::string getDay() const;
    std::string getStartTime() const;
    std::string getEndTime() const;
    int getStudentCount() const;
    int getRoomCapacity() const;
    WeekRule getWeekRule() const;
    void setWeekRule(WeekRule rule);
    void setRoom(const std::string& r);
    void setDay(const std::string& d);
    void setStartTime(const std::string& s);
    void setEndTime(const std::string& e);

    // 0 = Monday ... 6 = Sunday, 7 = unrecognised day name.
    // Accepts full names and 3-letter abbreviations, any capitalisation.
    int getDayIndex() const;

    // Orders by weekday (Monday first), then by start time.
    bool operator<(const Session& other) const;

    friend std::ostream& operator<<(std::ostream& os, const Session& session);
};

#endif
