#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include <string>
#include "Session.h"

// Loads timetable sessions from a CSV file and returns them as a vector of Session objects.
std::vector<Session> loadSessionsFromCSV(const std::string& filepath);

#endif // INPUT_H
