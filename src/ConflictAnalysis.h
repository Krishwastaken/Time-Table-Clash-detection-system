#ifndef CONFLICT_ANALYSIS_H
#define CONFLICT_ANALYSIS_H

#include "ClashDetector.h"
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace ConflictAnalysis {

// Groups clash reasons for one session pair.
struct Incident {
    std::size_t first;
    std::size_t second;
    std::vector<Clash> reasons;
};

// Stores a connected conflict group.
struct Component {
    std::vector<std::size_t> sessions;
    std::size_t conflictEdges = 0;
};

// Groups clashes by session pair.
std::vector<Incident> consolidate(const std::vector<Clash>& clashes,
                                  const std::vector<Session>& sessions);

// Finds connected groups using DFS.
std::vector<Component> connectedGroups(const std::vector<Clash>& clashes,
                                       std::size_t sessionCount);

// Prints consolidated clashes.
void printIncidents(std::ostream& out, const std::vector<Session>& sessions,
                    const std::vector<Clash>& clashes);

// Prints conflict graph groups.
void printGroups(std::ostream& out, const std::vector<Session>& sessions,
                 const std::vector<Clash>& clashes);

} // namespace ConflictAnalysis

#endif
