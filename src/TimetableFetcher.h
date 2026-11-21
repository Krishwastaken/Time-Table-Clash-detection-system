
#ifndef TIMETABLE_FETCHER_H
#define TIMETABLE_FETCHER_H

#include <vector>
#include "Session.h"
#include <string>

namespace TimetableFetcher
{
    bool downloadTimetable(const std::string& url,const std::string& outputPath);
    bool inspectFirstSectionTables(const std::string& htmlPath);
    bool inspectFirstSectionSlots(const std::string& htmlPath);
    bool inspectFirstSectionSubjects(const std::string& htmlPath);
    bool inspectFirstSectionLookup(const std::string& htmlPath);
    bool inspectFirstSectionRooms(const std::string& htmlPath);
    bool inspectFirstSectionParallelLabs(const std::string& htmlPath);
    bool generateFirstSectionSessions(const std::string& htmlPath,std::vector<Session>& sessions);
    bool generateAllSections(const std::string& htmlPath,const std::vector<std::string>& sectionNames,std::vector<Session>& sessions);    
}

#endif
