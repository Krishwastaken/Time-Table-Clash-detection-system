
#include <iostream>
#include <cassert>
#include <stdexcept>
#include "sorting.h"

using namespace std;

Session makeSession(
    string subject,
    string faculty,
    string room,
    string startTime
) {
    return Session(
        subject, faculty, room, "CSE-A",
        "Monday", startTime, "12:00", 40
    );
}

int main() {
    vector<Session> sessions = {
        makeSession("DBMS", "Zara", "CR2", "11:00"),
        makeSession("DSA", "Aman", "CR1", "09:00"),
        makeSession("OOP", "Mira", "CR3", "10:00")
    };

    // Test 1: Sort by day and start time
    sortSessions(sessions);

    assert(sessions[0].getSubject() == "DSA");
    assert(sessions[1].getSubject() == "OOP");
    assert(sessions[2].getSubject() == "DBMS");

    cout << "PASS: Sort by day and start time\n";

    // Test 2: Sort by faculty
    sortSessions(sessions, "faculty");

    assert(sessions[0].getFaculty() == "Aman");
    assert(sessions[1].getFaculty() == "Mira");
    assert(sessions[2].getFaculty() == "Zara");

    cout << "PASS: Sort by faculty\n";

    // Test 3: Sort by room
    sortSessions(sessions, "room");

    assert(sessions[0].getRoom() == "CR1");
    assert(sessions[1].getRoom() == "CR2");
    assert(sessions[2].getRoom() == "CR3");

    cout << "PASS: Sort by room\n";

    // Test 4: Empty input must throw an exception
    vector<Session> emptySessions;
    bool exceptionCaught = false;

    try {
        sortSessions(emptySessions);
    } catch (const invalid_argument&) {
        exceptionCaught = true;
    }

    assert(exceptionCaught);

    cout << "PASS: Empty input validation\n";
    cout << "All sorting tests passed.\n";

    return 0;
}
