#ifndef RESOLVER_H
#define RESOLVER_H

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>
#include "ClashDetector.h"
#include "Session.h"

enum class FixKind {
    MoveTime,         // session moved to another day/time (room may change too)
    ChangeRoom,       // same time, different room
    RemoveDuplicate   // exact duplicate row deleted
};

struct Resolution {
    FixKind kind;
    std::size_t sessionIndex;   // 0-based index in the list that was passed in
    std::string subject, section, faculty;
    std::string from;           // e.g. "Monday 09:00-10:00 (CR1)"
    std::string to;             // empty for RemoveDuplicate
    std::string reason;
};

struct ResolveResult {
    std::vector<Resolution> applied;
    std::size_t clashesBefore = 0;
    std::size_t clashesAfter = 0;   // re-checked with ClashDetector after the fixes
};

namespace Resolver {

struct Config {
    int slotStep = 30;   // minutes between candidate start times
};

// Greedy resolver. Modifies `sessions` (moves, re-rooms, removes duplicates).
// The list is NOT re-sorted: call sortSessions() and ClashDetector::detectClashes() afterwards.
ResolveResult resolve(std::vector<Session>& sessions,
                      const std::vector<Clash>& clashes,
                      const Config& cfg = Config());

void print(std::ostream& out, const ResolveResult& result);

// Saves the timetable in the same CSV format that input.cpp reads.
bool exportTimetable(const std::string& path, const std::vector<Session>& sessions);

// Saves what the resolver changed (one row per fix) as a CSV change log.
bool exportChangeLog(const std::string& path, const ResolveResult& result);

}  // namespace Resolver

#endif
