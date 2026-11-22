#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include "Session.h"
#include "Input.h"
#include "ClashDetector.h"
#include "ConflictAnalysis.h"
#include "ReportFormatter.h"
#include "Resolver.h"
#include "JsonExport.h"
#include "TimetableFetcher.h"

namespace {

std::string jsonEscape(const std::string& str) {
    std::ostringstream o;
    for (char c : str) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

std::string sessionToJSON(const Session& s, size_t idx) {
    std::ostringstream ss;
    ss << "{"
       << "\"index\":" << idx << ","
       << "\"subject\":\"" << jsonEscape(s.getSubject()) << "\","
       << "\"faculty\":\"" << jsonEscape(s.getFaculty()) << "\","
       << "\"room\":\"" << jsonEscape(s.getRoom()) << "\","
       << "\"section\":\"" << jsonEscape(s.getSection()) << "\","
       << "\"day\":\"" << jsonEscape(s.getDay()) << "\","
       << "\"startTime\":\"" << jsonEscape(s.getStartTime()) << "\","
       << "\"endTime\":\"" << jsonEscape(s.getEndTime()) << "\","
       << "\"studentCount\":" << s.getStudentCount() << ","
       << "\"roomCapacity\":" << s.getRoomCapacity()
       << "}";
    return ss.str();
}

std::string clashTypeName(ClashType t) {
    switch (t) {
        case ClashType::Faculty: return "Faculty";
        case ClashType::Room: return "Room";
        case ClashType::Section: return "Section";
        case ClashType::RoomCapacity: return "Room capacity";
        case ClashType::DuplicateSession: return "Duplicate session";
    }
    return "Unknown";
}

std::string clashSeverity(ClashType t) {
    switch (t) {
        case ClashType::Faculty: return "HIGH";
        case ClashType::Room: return "HIGH";
        case ClashType::Section: return "HIGH";
        case ClashType::RoomCapacity: return "MEDIUM";
        case ClashType::DuplicateSession: return "LOW";
    }
    return "LOW";
}

bool writeSessionsToCSV(const std::string& filePath, const std::vector<Session>& sessions) {
    std::ofstream out(filePath);
    if (!out.is_open()) return false;
    out << "Section,Day,Start_Time,End_Time,Subject,Faculty,Room,Student_Count\n";
    for (const auto& s : sessions) {
        out << "\"" << jsonEscape(s.getSection()) << "\","
            << "\"" << jsonEscape(s.getDay()) << "\","
            << "\"" << jsonEscape(s.getStartTime()) << "\","
            << "\"" << jsonEscape(s.getEndTime()) << "\","
            << "\"" << jsonEscape(s.getSubject()) << "\","
            << "\"" << jsonEscape(s.getFaculty()) << "\","
            << "\"" << jsonEscape(s.getRoom()) << "\","
            << s.getStudentCount() << "\n";
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <mode> [args...]\n"
                  << "Modes:\n"
                  << "  detect <file.csv|html> [--json] [--out <file>]\n"
                  << "  resolve <file.csv|html> [clash_index] [--json]\n"
                  << "  export <in_file> <out.json>\n"
                  << "  apply <in.csv> <target_idx> <room> <day> <start> <end> <out.csv>\n"
                  << "  summary <file.csv|html>\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "detect") {
        if (argc < 3) {
            std::cerr << "Error: detect requires CSV or HTML file path\n";
            return 1;
        }
        std::string csvPath = argv[2];
        bool jsonOutput = false;
        std::string outPath;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") jsonOutput = true;
            else if (arg == "--out" && i + 1 < argc) {
                outPath = argv[++i];
            }
        }

        std::vector<Session> sessions = loadSessionsFromCSV(csvPath);
        std::vector<Clash> clashes = ClashDetector::detectClashes(sessions, true);

        int facCount = 0, roomCount = 0, secCount = 0, capCount = 0, dupCount = 0;
        for (const auto& c : clashes) {
            switch (c.type) {
                case ClashType::Faculty: facCount++; break;
                case ClashType::Room: roomCount++; break;
                case ClashType::Section: secCount++; break;
                case ClashType::RoomCapacity: capCount++; break;
                case ClashType::DuplicateSession: dupCount++; break;
            }
        }

        auto incidents = ConflictAnalysis::consolidate(clashes, sessions);
        auto groups = ConflictAnalysis::connectedGroups(clashes, sessions.size());

        if (jsonOutput) {
            std::ostringstream js;
            js << "{\n"
               << "  \"totalSessions\": " << sessions.size() << ",\n"
               << "  \"totalClashes\": " << clashes.size() << ",\n"
               << "  \"summaryByCategory\": {\n"
               << "    \"Faculty\": " << facCount << ",\n"
               << "    \"Room\": " << roomCount << ",\n"
               << "    \"Section\": " << secCount << ",\n"
               << "    \"Room capacity\": " << capCount << ",\n"
               << "    \"Duplicate session\": " << dupCount << "\n"
               << "  },\n"
               << "  \"distinctIncidentsCount\": " << incidents.size() << ",\n"
               << "  \"connectedGroupsCount\": " << groups.size() << ",\n"
               << "  \"clashes\": [\n";

            for (size_t i = 0; i < clashes.size(); ++i) {
                const auto& c = clashes[i];
                const auto& s1 = sessions[c.firstSession];
                bool hasS2 = (c.secondSession != static_cast<size_t>(-1) && c.secondSession < sessions.size());

                js << "    {\n"
                   << "      \"id\": \"CLASH-" << (i + 1) << "\",\n"
                   << "      \"index\": " << i << ",\n"
                   << "      \"type\": \"" << clashTypeName(c.type) << "\",\n"
                   << "      \"severity\": \"" << clashSeverity(c.type) << "\",\n"
                   << "      \"description\": \"" << jsonEscape(c.description) << "\",\n"
                   << "      \"firstSession\": " << sessionToJSON(s1, c.firstSession) << ",\n"
                   << "      \"secondSession\": " << (hasS2 ? sessionToJSON(sessions[c.secondSession], c.secondSession) : "null") << ",\n"
                   << "      \"affectedSections\": [\"" << jsonEscape(s1.getSection()) << "\""
                   << (hasS2 && s1.getSection() != sessions[c.secondSession].getSection() ? (", \"" + jsonEscape(sessions[c.secondSession].getSection()) + "\"") : "") << "],\n"
                   << "      \"affectedFaculty\": [\"" << jsonEscape(s1.getFaculty()) << "\""
                   << (hasS2 && s1.getFaculty() != sessions[c.secondSession].getFaculty() ? (", \"" + jsonEscape(sessions[c.secondSession].getFaculty()) + "\"") : "") << "],\n"
                   << "      \"affectedRooms\": [\"" << jsonEscape(s1.getRoom()) << "\""
                   << (hasS2 && s1.getRoom() != sessions[c.secondSession].getRoom() ? (", \"" + jsonEscape(sessions[c.secondSession].getRoom()) + "\"") : "") << "]\n"
                   << "    }" << (i + 1 < clashes.size() ? "," : "") << "\n";
            }
            js << "  ]\n}\n";

            if (!outPath.empty()) {
                std::ofstream f(outPath);
                f << js.str();
            } else {
                std::cout << js.str();
            }
        } else {
            ClashDetector::printClashes(clashes, sessions);
            ConflictAnalysis::printIncidents(std::cout, sessions, clashes);
            ConflictAnalysis::printGroups(std::cout, sessions, clashes);
        }
        return 0;
    }

    if (mode == "resolve") {
        if (argc < 3) {
            std::cerr << "Error: resolve requires file path\n";
            return 1;
        }
        std::string csvPath = argv[2];
        int targetClashIdx = -1;
        bool jsonOutput = false;
        std::string outPath;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") jsonOutput = true;
            else if (arg == "--out" && i + 1 < argc) {
                outPath = argv[++i];
            }
            else if (targetClashIdx == -1 && std::isdigit(static_cast<unsigned char>(arg[0]))) {
                targetClashIdx = std::stoi(arg);
            }
        }

        std::vector<Session> sessions = loadSessionsFromCSV(csvPath);
        std::vector<Clash> clashes = ClashDetector::detectClashes(sessions, true);

        if (targetClashIdx >= 0) {
            auto p = Resolver::proposeSingleClash(targetClashIdx, clashes, sessions);
            if (jsonOutput) {
                int propCap = p.proposedCapacity;
                if (propCap <= 0 && !p.proposedRoom.empty() && p.proposedRoom != "[REMOVED]") {
                    propCap = Session("", "", p.proposedRoom, "", "", "", "", 0).getRoomCapacity();
                }
                if (propCap <= 0) propCap = p.originalCapacity;

                std::cout << "{\n"
                          << "  \"clashIndex\": " << p.clashIndex << ",\n"
                          << "  \"clashId\": \"" << p.clashId << "\",\n"
                          << "  \"feasible\": " << (p.feasible ? "true" : "false") << ",\n"
                          << "  \"action\": \"" << p.action << "\",\n"
                          << "  \"explanation\": \"" << jsonEscape(p.explanation) << "\",\n"
                          << "  \"targetSessionIndex\": " << p.targetSessionIndex << ",\n"
                          << "  \"original\": {\n"
                          << "    \"subject\": \"" << jsonEscape(p.originalSubject) << "\",\n"
                          << "    \"section\": \"" << jsonEscape(p.originalSection) << "\",\n"
                          << "    \"faculty\": \"" << jsonEscape(p.originalFaculty) << "\",\n"
                          << "    \"room\": \"" << jsonEscape(p.originalRoom) << "\",\n"
                          << "    \"day\": \"" << jsonEscape(p.originalDay) << "\",\n"
                          << "    \"startTime\": \"" << jsonEscape(p.originalStartTime) << "\",\n"
                          << "    \"endTime\": \"" << jsonEscape(p.originalEndTime) << "\",\n"
                          << "    \"roomCapacity\": " << p.originalCapacity << ",\n"
                          << "    \"studentCount\": " << p.originalStudents << "\n"
                          << "  },\n"
                          << "  \"proposed\": {\n"
                          << "    \"subject\": \"" << jsonEscape(p.originalSubject) << "\",\n"
                          << "    \"section\": \"" << jsonEscape(p.originalSection) << "\",\n"
                          << "    \"faculty\": \"" << jsonEscape(p.originalFaculty) << "\",\n"
                          << "    \"room\": \"" << jsonEscape(p.proposedRoom) << "\",\n"
                          << "    \"day\": \"" << jsonEscape(p.proposedDay) << "\",\n"
                          << "    \"startTime\": \"" << jsonEscape(p.proposedStartTime) << "\",\n"
                          << "    \"endTime\": \"" << jsonEscape(p.proposedEndTime) << "\",\n"
                          << "    \"roomCapacity\": " << propCap << ",\n"
                          << "    \"studentCount\": " << p.originalStudents << "\n"
                          << "  },\n"
                          << "  \"clashesBefore\": " << p.clashesBefore << ",\n"
                          << "  \"clashesAfter\": " << p.clashesAfter << "\n"
                          << "}\n";
            } else {
                std::cout << "Resolution Proposal for Clash #" << (p.clashIndex + 1) << ":\n";
                std::cout << "Feasible: " << (p.feasible ? "Yes" : "No") << "\n";
                std::cout << "Action: " << p.action << "\n";
                std::cout << "Explanation: " << p.explanation << "\n";
                std::cout << "Original: " << p.originalDay << " " << p.originalStartTime << "-" << p.originalEndTime << " in " << p.originalRoom << "\n";
                if (p.feasible) {
                    std::cout << "Proposed: " << p.proposedDay << " " << p.proposedStartTime << "-" << p.proposedEndTime << " in " << p.proposedRoom << "\n";
                }
            }
        } else {
            std::vector<Session> resolved = sessions;
            ResolveResult r = Resolver::resolve(resolved, clashes);
            if (!outPath.empty()) {
                writeSessionsToCSV(outPath, resolved);
            }
            if (jsonOutput) {
                std::cout << "{\n"
                          << "  \"clashesBefore\": " << r.clashesBefore << ",\n"
                          << "  \"clashesAfter\": " << r.clashesAfter << ",\n"
                          << "  \"appliedCount\": " << r.applied.size() << ",\n"
                          << "  \"unresolvedCount\": " << r.unresolved.size() << ",\n"
                          << "  \"applied\": [\n";
                for (size_t i = 0; i < r.applied.size(); ++i) {
                    const auto& a = r.applied[i];
                    std::cout << "    {\n"
                              << "      \"kind\": \"" << (a.kind == FixKind::ChangeRoom ? "CHANGE_ROOM" : "REMOVE_DUPLICATE") << "\",\n"
                              << "      \"sessionIndex\": " << a.sessionIndex << ",\n"
                              << "      \"subject\": \"" << jsonEscape(a.subject) << "\",\n"
                              << "      \"section\": \"" << jsonEscape(a.section) << "\",\n"
                              << "      \"faculty\": \"" << jsonEscape(a.faculty) << "\",\n"
                              << "      \"from\": \"" << jsonEscape(a.from) << "\",\n"
                              << "      \"to\": \"" << jsonEscape(a.to) << "\",\n"
                              << "      \"reason\": \"" << jsonEscape(a.reason) << "\"\n"
                              << "    }" << (i + 1 < r.applied.size() ? "," : "") << "\n";
                }
                std::cout << "  ],\n  \"unresolved\": [\n";
                for (size_t i = 0; i < r.unresolved.size(); ++i) {
                    const auto& u = r.unresolved[i];
                    std::cout << "    {\n"
                              << "      \"type\": \"" << clashTypeName(u.type) << "\",\n"
                              << "      \"description\": \"" << jsonEscape(u.description) << "\",\n"
                              << "      \"reason\": \"" << jsonEscape(u.reason) << "\"\n"
                              << "    }" << (i + 1 < r.unresolved.size() ? "," : "") << "\n";
                }
                std::cout << "  ]\n}\n";
            } else {
                Resolver::print(std::cout, r);
            }
        }
        return 0;
    }

    if (mode == "export") {
        if (argc < 4) {
            std::cerr << "Usage: export <in_file> <out.json>\n";
            return 1;
        }
        std::string inPath = argv[2];
        std::string outPath = argv[3];
        std::vector<Session> sessions = loadSessionsFromCSV(inPath);
        std::vector<Clash> clashes = ClashDetector::detectClashes(sessions, true);
        std::vector<Session> resolved = sessions;
        ResolveResult res = Resolver::resolve(resolved, clashes);
        if (JsonExport::write(outPath, inPath, sessions, clashes, resolved, res)) {
            std::cout << "Successfully exported timetable report to " << outPath << "\n";
            return 0;
        } else {
            std::cerr << "Failed to write export to " << outPath << "\n";
            return 1;
        }
    }

    if (mode == "apply") {
        if (argc < 8) {
            std::cerr << "Usage: apply <in.csv> <target_idx> <room> <day> <start> <end> <out.csv>\n";
            return 1;
        }
        std::string inPath = argv[2];
        size_t targetIdx = std::stoul(argv[3]);
        std::string newRoom = argv[4];
        std::string newDay = argv[5];
        std::string newStart = argv[6];
        std::string newEnd = argv[7];
        std::string outPath = argv[8];

        std::vector<Session> sessions = loadSessionsFromCSV(inPath);
        if (targetIdx >= sessions.size()) {
            std::cerr << "Error: target session index out of bounds\n";
            return 1;
        }

        sessions[targetIdx].setRoom(newRoom);
        sessions[targetIdx].setDay(newDay);
        sessions[targetIdx].setStartTime(newStart);
        sessions[targetIdx].setEndTime(newEnd);

        if (!writeSessionsToCSV(outPath, sessions)) {
            std::cerr << "Error: failed to write to " << outPath << "\n";
            return 1;
        }
        std::cout << "Successfully updated session " << targetIdx << " and saved to " << outPath << "\n";
        return 0;
    }

    if (mode == "summary") {
        if (argc < 3) {
            std::cerr << "Error: summary requires file path\n";
            return 1;
        }
        std::string csvPath = argv[2];
        std::vector<Session> sessions = loadSessionsFromCSV(csvPath);
        std::vector<Clash> clashes = ClashDetector::detectClashes(sessions, true);
        ReportFormatter::summary(sessions, clashes);
        return 0;
    }

    std::cerr << "Unknown mode: " << mode << "\n";
    return 1;
}
