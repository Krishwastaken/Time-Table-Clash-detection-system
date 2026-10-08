#include <iostream>
#include <iomanip>
#include "Input.h"

using namespace std;

int main() {
    string filePath = "data/timetable.csv";
    vector<Session> sessions = loadSessionsFromCSV(filePath);

    cout << "==========================================" << endl;
    cout << "       TIMETABLE INPUT MODULE TEST        " << endl;
    cout << "==========================================" << endl;
    cout << "Total Sessions Loaded: " << sessions.size() << endl;
    cout << "------------------------------------------" << endl;

    for (size_t i = 0; i < sessions.size(); ++i) {
        const Session& s = sessions[i];
        cout << "[" << (i + 1) << "] " << s.subject << endl;
        cout << "    Faculty   : " << s.faculty << endl;
        cout << "    Room      : " << s.room << endl;
        cout << "    Section   : " << s.section << endl;
        cout << "    Day       : " << s.day << endl;
        cout << "    Time      : " << s.startTime << " - " << s.endTime << endl;
        cout << "------------------------------------------" << endl;
    }

    return 0;
}