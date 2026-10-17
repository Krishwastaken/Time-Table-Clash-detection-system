#ifndef CONFLICT_ANALYSIS_H
#define CONFLICT_ANALYSIS_H
#include "ClashDetector.h"
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace ConflictAnalysis {
struct Incident {
    std::size_t first;
    std::size_t second; // size_t(-1) for single-session issues
    std::vector<Clash> reasons;
};
struct Component {
    std::vector<std::size_t> sessions;
    std::size_t conflictEdges = 0;
};
std::vector<Incident> consolidate(const std::vector<Clash>& clashes,
                                  const std::vector<Session>& sessions);
std::vector<Component> connectedGroups(const std::vector<Clash>& clashes,
                                       std::size_t sessionCount);
void printIncidents(std::ostream& out, const std::vector<Session>& sessions,
                    const std::vector<Clash>& clashes);
void printGroups(std::ostream& out, const std::vector<Session>& sessions,
                 const std::vector<Clash>& clashes);
}
#endif
