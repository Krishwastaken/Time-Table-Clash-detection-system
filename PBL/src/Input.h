#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include <string>
#include "Session.h"

std::vector<Session> loadSessionsFromCSV(const std::string& filepath);

#endif 
