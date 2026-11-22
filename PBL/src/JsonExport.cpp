#include "JsonExport.h"
#include "ConflictAnalysis.h"
#include "TimeUtils.h"

#include <cstdio>
#include <fstream>

using namespace std;

namespace {

const size_t NONE = static_cast<size_t>(-1);

string esc(const string& s) {
    string out = "\"";

    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }

    return out + "\"";
}

const char* weeks(WeekRule r) {
    return r == WeekRule::SaturdayOffOnly ? "saturday_off"
         : r == WeekRule::SaturdayWorkingOnly ? "saturday_working" : "always";
}

const char* typeName(ClashType t) {
    switch (t) {
        case ClashType::Faculty:          return "faculty";
        case ClashType::Room:             return "room";
        case ClashType::Section:          return "section";
        case ClashType::RoomCapacity:     return "capacity";
        case ClashType::DuplicateSession: return "duplicate";
    }
    return "unknown";
}

string idOrNull(size_t id) {
    return id == NONE ? "null" : to_string(id);
}

void writeSessions(ofstream& f, const vector<Session>& sessions, const vector<size_t>* originalIndex) {
    f << "[";

    for (size_t i = 0; i < sessions.size(); ++i) {
        const Session& s = sessions[i];

        f << (i ? ",\n      " : "\n      ")
          << "{\"id\":" << i;

        if (originalIndex)
            f << ",\"originalId\":" << (*originalIndex)[i];

        f << ",\"subject\":" << esc(s.getSubject())
          << ",\"faculty\":" << esc(s.getFaculty())
          << ",\"room\":" << esc(s.getRoom())
          << ",\"section\":" << esc(s.getSection())
          << ",\"day\":" << esc(s.getDay())
          << ",\"start\":" << esc(s.getStartTime())
          << ",\"end\":" << esc(s.getEndTime())
          << ",\"students\":" << s.getStudentCount()
          << ",\"weekRule\":\"" << weeks(s.getWeekRule()) << "\""
          << ",\"freePeriod\":" << (TimeUtils::isFreeActivity(TimeUtils::normalize(s.getSubject())) ? "true" : "false")
          << "}";
    }

    f << "\n    ]";
}

void writeClashes(ofstream& f, const vector<Clash>& clashes, const char* indent) {
    f << "[";

    for (size_t i = 0; i < clashes.size(); ++i) {
        const Clash& c = clashes[i];

        f << (i ? string(",\n") + indent : string("\n") + indent)
          << "{\"id\":" << i
          << ",\"type\":\"" << typeName(c.type) << "\""
          << ",\"weeks\":\"" << weeks(c.scenario) << "\""
          << ",\"sessionA\":" << c.firstSession
          << ",\"sessionB\":" << idOrNull(c.secondSession)
          << ",\"description\":" << esc(c.description) << "}";
    }

    f << "\n    ]";
}

}  // namespace

namespace JsonExport {

bool write(const string& path, const string& title, const vector<Session>& original,
           const vector<Clash>& clashes, const vector<Session>& resolved,
           const ResolveResult& result) {
    ofstream f(path);
    if (!f)
        return false;

    size_t roomChanges = 0, removed = 0;
    for (const auto& x : result.applied) {
        if (x.kind == FixKind::ChangeRoom) ++roomChanges;
        else ++removed;
    }

    f << "{\n  \"title\": " << esc(title) << ",\n"
      << "  \"summary\": {\"sessions\":" << original.size()
      << ",\"clashesBefore\":" << result.clashesBefore
      << ",\"clashesAfter\":" << result.clashesAfter
      << ",\"roomChanges\":" << roomChanges
      << ",\"duplicatesRemoved\":" << removed
      << ",\"unresolved\":" << result.unresolved.size() << "},\n";

    f << "  \"original\": {\n    \"sessions\": ";
    writeSessions(f, original, nullptr);
    f << ",\n    \"clashes\": ";
    writeClashes(f, clashes, "      ");

    // Incidents: the same clash between the same two classes, grouped over
    // the sections that share them.
    const vector<ConflictAnalysis::Incident> incidents = ConflictAnalysis::consolidate(clashes, original);

    f << ",\n    \"incidents\": [";
    for (size_t i = 0; i < incidents.size(); ++i) {
        const auto& inc = incidents[i];

        f << (i ? ",\n      " : "\n      ") << "{\"id\":" << i << ",\"types\":[";
        for (size_t k = 0; k < inc.reasons.size(); ++k)
            f << (k ? "," : "") << "\"" << typeName(inc.reasons[k].type) << "\"";

        f << "],\"weeks\":\"" << weeks(inc.reasons.empty() ? WeekRule::Always : inc.reasons[0].scenario)
          << "\",\"sessionsA\":[";
        for (size_t k = 0; k < inc.firstGroup.size(); ++k)
            f << (k ? "," : "") << inc.firstGroup[k];
        f << "],\"sessionsB\":[";
        for (size_t k = 0; k < inc.secondGroup.size(); ++k)
            f << (k ? "," : "") << inc.secondGroup[k];
        f << "],\"descriptions\":[";
        for (size_t k = 0; k < inc.reasons.size(); ++k)
            f << (k ? "," : "") << esc(inc.reasons[k].description);
        f << "]}";
    }
    f << "\n    ]\n  },\n";

    f << "  \"resolved\": {\n    \"sessions\": ";
    writeSessions(f, resolved, &result.originalIndex);

    f << ",\n    \"changes\": [";
    for (size_t i = 0; i < result.applied.size(); ++i) {
        const auto& x = result.applied[i];

        f << (i ? ",\n      " : "\n      ")
          << "{\"kind\":\"" << (x.kind == FixKind::ChangeRoom ? "room_change" : "duplicate_removed") << "\""
          << ",\"originalId\":" << x.sessionIndex
          << ",\"subject\":" << esc(x.subject)
          << ",\"section\":" << esc(x.section)
          << ",\"faculty\":" << esc(x.faculty)
          << ",\"from\":" << esc(x.from)
          << ",\"to\":" << esc(x.to)
          << ",\"reason\":" << esc(x.reason) << "}";
    }

    f << "\n    ],\n    \"unresolved\": [";
    for (size_t i = 0; i < result.unresolved.size(); ++i) {
        const auto& u = result.unresolved[i];

        f << (i ? ",\n      " : "\n      ")
          << "{\"type\":\"" << typeName(u.type) << "\""
          << ",\"weeks\":\"" << weeks(u.scenario) << "\""
          << ",\"sessionA\":" << u.first
          << ",\"sessionB\":" << idOrNull(u.second)
          << ",\"description\":" << esc(u.description)
          << ",\"reason\":" << esc(u.reason) << "}";
    }
    f << "\n    ]\n  }\n}\n";

    return static_cast<bool>(f);
}

}  // namespace JsonExport
