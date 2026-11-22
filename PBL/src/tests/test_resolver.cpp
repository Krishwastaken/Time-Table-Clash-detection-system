#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "Session.h"
#include "Input.h"
#include "ClashDetector.h"
#include "Resolver.h"

int main() {
    std::string path = "data/timetable1.csv";
    std::vector<Session> sessions = loadSessionsFromCSV(path);
    std::cout << "Loaded " << sessions.size() << " sessions from " << path << "\n";
    assert(!sessions.empty());

    std::vector<Clash> clashes = ClashDetector::detectClashes(sessions, true);
    std::cout << "Detected " << clashes.size() << " initial clashes\n";
    assert(clashes.size() > 0);

    // Test greedy resolver
    std::vector<Session> copy = sessions;
    ResolveResult result = Resolver::resolve(copy, clashes);
    std::cout << "Clashes before: " << result.clashesBefore
              << " | after greedy resolve: " << result.clashesAfter << "\n";
    std::cout << "Applied fixes: " << result.applied.size()
              << " | Unresolved: " << result.unresolved.size() << "\n";
    for (const auto& a : result.applied) {
        std::cout << "  - Fix on session " << a.sessionIndex << " (" << a.subject << "): from " << a.from << " -> " << a.to << " (" << a.reason << ")\n";
    }
    assert(result.clashesAfter < result.clashesBefore);
    assert(!result.applied.empty());

    // Test single clash proposal (Clash 1: Logic Design in LT-1 with 210 students)
    auto p1 = Resolver::proposeSingleClash(2, clashes, sessions);
    std::cout << "Clash 1 single proposal: action=" << p1.action
              << ", feasible=" << (p1.feasible ? "true" : "false")
              << ", expl=" << p1.explanation << "\n";
    assert(p1.feasible);

    // Test export methods
    bool expTT = Resolver::exportTimetable("data/test_out.csv", copy);
    bool expLog = Resolver::exportChangeLog("data/test_log.csv", result);
    bool expUnres = Resolver::exportUnresolved("data/test_unres.csv", result, copy);
    assert(expTT);
    assert(expLog);
    assert(expUnres);

    std::cout << "ALL RESOLVER C++ UNIT TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
