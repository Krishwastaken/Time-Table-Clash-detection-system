#ifndef SORTING_H
#define SORTING_H

#include <vector>
#include <string>
#include "session.h"

// Two-Way Merge Sort — sorts by day, then start time
// Throws std::invalid_argument if the list is empty.
void sortSessions(std::vector<Session>& sessions);

// Function Overloading 
// Sorts by a chosen key instead: "faculty" or "room"
void sortSessions(std::vector<Session>& sessions, const std::string& key);

#endif // SORTING_H
