#include <iostream>
#include <iomanip>
#include <stdexcept>
#include "Input.h"
#include "sorting.h"
#include "ClashDetector.h"

using namespace std;

void printSessions(const vector<Session>& sessions) {
    for (size_t i = 0; i < sessions.size(); ++i) {
        cout << "[" << (i + 1) << "] " << sessions[i] << endl; // uses overloaded <<
    }
}


int main() {
    string filePath = "data/timetable1.csv";
    vector<Session> sessions = loadSessionsFromCSV(filePath);

    cout << "==========================================" << endl;
    cout << "       TIMETABLE INPUT MODULE TEST        " << endl;
    cout << "==========================================" << endl;
    cout << "Total Sessions Loaded: "
         << sessions.size() << endl;
    cout << "------------------------------------------" << endl;

    try {
        cout << "\n--- Sorted by Day + Time (Merge Sort) ---"
             << endl;

        sortSessions(sessions);
        printSessions(sessions);

        //clash detection module
        cout << "\n--- Clash Detection ---" << endl;

        vector<Clash> clashes =
            ClashDetector::detectClashes(sessions);

        ClashDetector::printClashes(clashes, sessions);

        //sorting
        cout << "\n--- Sorted by Faculty ---" << endl;
        sortSessions(sessions, "faculty");
        printSessions(sessions);

        cout << "\n--- Triggering exception on empty list ---"
             << endl;

        vector<Session> empty;
        sortSessions(empty);

    } catch (const invalid_argument& e) {
        cout << "Caught exception: " << e.what() << endl;
    }

    return 0;
}
