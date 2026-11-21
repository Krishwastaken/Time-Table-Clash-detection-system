#ifndef TIMETABLE_FETCHER_H
#define TIMETABLE_FETCHER_H

#include <string>
#include <vector>
#include "Session.h"

namespace TimetableFetcher
{
    // Downloads the timetable page with the curl.exe that ships with Windows 10+.
    bool downloadTimetable(const std::string& url, const std::string& outputPath);

    // Parses only the first section (diagnostic helper).
    bool generateFirstSectionSessions(const std::string& htmlPath,
                                      std::vector<Session>& sessions);

    // Parses every section of a saved timetable page.
    // Section names are read from the page headings ("Section-A",
    // "Section: AI-DS1", "-ML1"...) and normalised (A, AIDS1, ML1, CYBER).
    // sectionNames is only a fallback for pages whose headings cannot be read.
    bool generateAllSections(const std::string& htmlPath,
                             const std::vector<std::string>& sectionNames,
                             std::vector<Session>& sessions);
}

#endif
