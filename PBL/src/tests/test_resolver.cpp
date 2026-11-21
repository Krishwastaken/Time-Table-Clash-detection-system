#include <iostream>
#include <cassert>
#include "Session.h"
#include "Input.h"
#include "ClashDetector.h"
#include "Resolver.h"

int main() {
    std::string path = "data/timetable1.csv";
    std::vector<Session> sessions = loadSessionsFromCSV(path);
    std::cout << "Loaded " << sessions.size() << " sessions from " << path << "\n";
    assert(!sessions.empty());

    std::vector<Clash> clashes = ClashDetector::detectClashes(sessions);
    std::cout << "Detected " << clashes.size() << " initial clashes\n";
    assert(clashes.size() == 8);

    // Test resolving clash 0 (Capacity clash)
    ResolutionProposal p0 = Resolver::proposeResolution(0, clashes, sessions);
    std::cout << "Clash 0 resolution proposal: " << p0.action << " -> " << p0.explanation << "\n";
    assert(p0.feasible);
    assert(p0.action == "CHANGE_ROOM");

    // Apply proposal 0
    std::vector<Session> updated = Resolver::applyResolution(p0, sessions);
    std::vector<Clash> clashesAfter0 = ClashDetector::detectClashes(updated);
    std::cout << "Clashes after applying proposal 0: " << clashesAfter0.size() << "\n";
    assert(clashesAfter0.size() < clashes.size());

    // Test resolving clash 2 (Faculty clash)
    ResolutionProposal p2 = Resolver::proposeResolution(2, clashes, sessions);
    std::cout << "Clash 2 resolution proposal: " << p2.action << " -> " << p2.explanation << "\n";
    assert(p2.feasible);

    std::cout << "ALL RESOLVER C++ UNIT TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
