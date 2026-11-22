#ifndef REPORT_FORMATTER_H
#define REPORT_FORMATTER_H

#include "ClashDetector.h"
#include <string>
#include <vector>

namespace ReportFormatter {

// Returns the clash type label.
const char* typeName(ClashType type);

// Prints the timetable summary.
void summary(const std::vector<Session>& sessions,
             const std::vector<Clash>& clashes);

// Prints detailed or filtered clashes.
void details(const std::vector<Session>& sessions,
             const std::vector<Clash>& clashes,
             int typeFilter = -1);

// Saves a text report.
bool exportTXT(const std::string& path,
               const std::vector<Session>& sessions,
               const std::vector<Clash>& clashes);

// Saves a CSV report.
bool exportCSV(const std::string& path,
               const std::vector<Session>& sessions,
               const std::vector<Clash>& clashes);

} // namespace ReportFormatter

#endif
