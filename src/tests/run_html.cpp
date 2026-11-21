// Loads a saved timetable page, sorts it, runs the detector and prints the report.
//   g++ -std=c++17 -O2 -Isrc tests/run_html.cpp src/Session.cpp src/sorting.cpp src/ClashDetector.cpp src/ReportFormatter.cpp src/ConflictAnalysis.cpp src/TimetableFetcher.cpp -o run_html.exe
//   run_html.exe data/3sem.html
#include "ClashDetector.h"
#include "ConflictAnalysis.h"
#include "ReportFormatter.h"
#include "TimetableFetcher.h"
#include "sorting.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "usage: run_html <timetable.html> [details]\n"; return 1; }

    std::vector<Session> sessions;
    if (!TimetableFetcher::generateAllSections(argv[1], {}, sessions)) {
        std::cout << "Could not parse " << argv[1] << "\n";
        return 1;
    }

    sortSessions(sessions);
    auto clashes = ClashDetector::detectClashes(sessions, true);
    ReportFormatter::summary(sessions, clashes);
    if (argc > 2) ReportFormatter::details(sessions, clashes);
    ConflictAnalysis::printIncidents(std::cout, sessions, clashes);
    return 0;
}
