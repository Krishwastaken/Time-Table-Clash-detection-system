#ifndef RESOLVER_H
#define RESOLVER_H
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>
#include "ClashDetector.h"
#include "Session.h"


// Timetable is fixed: only rooms and duplicate rows can change.
// Clashes that require a time shift are reported as unresolved.

enum class FixKind {
    ChangeRoom,       
    RemoveDuplicate   
};

class Resolution {
public:
    FixKind kind;
    std::size_t sessionIndex;   
    std::string subject, section, faculty;
    std::string from;           
    std::string to;             
    std::string reason;
};

class UnresolvedClash {
public:
    ClashType type;
    std::size_t first;          
    std::size_t second;         
    WeekRule scenario;          
    std::string description;
    std::string reason;        
};

class ResolveResult {
public:
    std::vector<Resolution> applied;
    std::vector<UnresolvedClash> unresolved;
    std::vector<Clash> remaining;             
    std::vector<std::size_t> originalIndex;   
    std::size_t clashesBefore = 0;
    std::size_t clashesAfter  = 0;
};

namespace Resolver {
class Config {
public:
    bool ignorePresidentClass = true;
    ClashDetector::RoomPools roomPools = ClashDetector::defaultRoomPools();
};


// Greedy room re-assignment. Modifies `sessions` (rooms changed, duplicates
// removed). result.originalIndex maps each remaining session to its original position.

ResolveResult resolve(std::vector<Session>& sessions,
                      const std::vector<Clash>& clashes,
                      const Config& cfg = Config());

void print(std::ostream& out, const ResolveResult& result);

// Saves the timetable in the CSV format Input.cpp reads.
bool exportTimetable(const std::string& path, const std::vector<Session>& sessions);

// One row per applied fix.
bool exportChangeLog(const std::string& path, const ResolveResult& result);

// One row per unresolvable clash with reason.
bool exportUnresolved(const std::string& path, const ResolveResult& result,
                      const std::vector<Session>& resolvedSessions);

}  
#endif