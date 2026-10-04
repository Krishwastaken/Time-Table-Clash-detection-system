#ifndef SORTING_H
#define SORTING_H

#include <vector>
#include <string>
#include "session.h"

// DSA Unit 4: Two-Way Merge Sort — sorts by day, then start time (default key)
// Throws std::invalid_argument if the list is empty.
void sortSessions(std::vector<Session>& sessions);

// C++: Function Overloading + DSA: "Sorting on Different Keys"
// Sorts by a chosen key instead: "faculty" or "room"
void sortSessions(std::vector<Session>& sessions, const std::string& key);

#endif // SORTING_H