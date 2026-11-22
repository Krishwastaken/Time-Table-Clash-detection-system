#ifndef JSON_EXPORT_H
#define JSON_EXPORT_H

#include <string>
#include <vector>
#include "ClashDetector.h"
#include "Resolver.h"
#include "Session.h"

// Writes everything into one JSON file.
// {
//   "title": "...",
//   "summary": { sessions, clashesBefore, clashesAfter, roomChanges, duplicatesRemoved, unresolved },
//   "original": {
//     "sessions":  [ { id, subject, faculty, room, section, day, start, end, students, weekRule, freePeriod } ],
//     "clashes":   [ { id, type, weeks, sessionA, sessionB|null, description } ],
//     "incidents": [ { id, types, weeks, sessionsA[], sessionsB[], descriptions[] } ]   // grouped by shared class
//   },
//   "resolved": {
//     "sessions":   [ same fields + originalId ],
//     "changes":    [ { kind, originalId, subject, section, faculty, from, to, reason } ],
//     "unresolved": [ { type, weeks, sessionA, sessionB|null, description, reason } ]   // ids refer to resolved.sessions
//   }
// }
//

namespace JsonExport {

bool write(const std::string& path,
           const std::string& title,
           const std::vector<Session>& original,
           const std::vector<Clash>& clashes,
           const std::vector<Session>& resolved,
           const ResolveResult& result);

}

#endif
