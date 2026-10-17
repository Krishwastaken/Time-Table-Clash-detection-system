#ifndef REPORT_FORMATTER_H
#define REPORT_FORMATTER_H
#include "ClashDetector.h"
#include <string>
#include <vector>
namespace ReportFormatter {
void summary(const std::vector<Session>& sessions, const std::vector<Clash>& clashes);
void details(const std::vector<Session>& sessions, const std::vector<Clash>& clashes, int typeFilter = -1);
bool exportTXT(const std::string& path, const std::vector<Session>& sessions, const std::vector<Clash>& clashes);
bool exportCSV(const std::string& path, const std::vector<Session>& sessions, const std::vector<Clash>& clashes);
const char* typeName(ClashType type);
}
#endif
