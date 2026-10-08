#include <iostream>
#include <iomanip>
#include <stdexcept>
#include "input.h"
#include "sorting.h"

using namespace std;

void printSessions(const vector<Session>& sessions) {
    for (size_t i = 0; i < sessions.size(); ++i) {
        cout << "[" << (i + 1) << "] " << sessions[i] << endl; // uses overloaded <<
    }
}

int main() {
    string filePath = "data/timetable.csv";
    vector<Session> sessions = loadSessionsFromCSV(filePath);

    cout << "==========================================" << endl;
    cout << "       TIMETABLE INPUT MODULE TEST        " << endl;
    cout << "==========================================" << endl;
    cout << "Total Sessions Loaded: " << sessions.size() << endl;
    cout << "------------------------------------------" << endl;

    try {
        cout << "\n--- Sorted by Day + Time (Merge Sort, default key) ---" << endl;
        sortSessions(sessions);              // Overload 1: default merge sort
        printSessions(sessions);

        cout << "\n--- Sorted by Faculty (different key) ---" << endl;
        sortSessions(sessions, "faculty");   // Overload 2: sort by different key
        printSessions(sessions);

        cout << "\n--- Triggering exception on empty list ---" << endl;
        vector<Session> empty;
        sortSessions(empty);                 // this will throw

    } catch (const invalid_argument& e) {
        cout << "Caught exception: " << e.what() << endl; // C++ Unit 5: Exception Handling
    }

    return 0;
}