#include "ReportFormatter.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <tuple>

namespace {

constexpr std::size_t NO_SECOND_SESSION = static_cast<std::size_t>(-1);

// Returns a weekday's sorting position.
int dayOrder(const std::string& day) {
    const std::vector<std::string> days = {
        "Monday", "Tuesday", "Wednesday", "Thursday",
        "Friday", "Saturday", "Sunday"
    };

    for (std::size_t index = 0; index < days.size(); ++index) {
        if (days[index] == day) {
            return static_cast<int>(index);
        }
    }

    return 7;
}

// Sorts clashes by day, time, and type.
std::vector<Clash> sortClashes(std::vector<Clash> clashes,
                           const std::vector<Session>& sessions) {
    std::sort(clashes.begin(), clashes.end(),
              [&](const Clash& left, const Clash& right) {
                  const Session& first = sessions[left.firstSession];
                  const Session& second = sessions[right.firstSession];

                  return std::make_tuple(
                             dayOrder(first.getDay()),
                             first.getStartTime(),
                             static_cast<int>(left.type),
                             left.firstSession,
                             left.secondSession
                         ) < std::make_tuple(
                             dayOrder(second.getDay()),
                             second.getStartTime(),
                             static_cast<int>(right.type),
                             right.firstSession,
                             right.secondSession
                         );
              });

    return clashes;
}

// Escapes commas and quotes in CSV fields.
std::string escapeCSVField(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) {
        return value;
    }

    std::string escaped = "\"";
    for (char character : value) {
        if (character == '"') {
            escaped += '"';
        }
        escaped += character;
    }

    return escaped + "\"";
}

// Writes clash details to a stream.
void writeClashDetails(std::ostream& out,
               const std::vector<Session>& sessions,
               const std::vector<Clash>& clashes,
               int typeFilter) {
    const std::vector<Clash> sortedClashes = sortClashes(clashes, sessions);
    std::size_t displayedCount = 0;

    for (const Clash& clash : sortedClashes) {
        if (typeFilter >= 0 && static_cast<int>(clash.type) != typeFilter) {
            continue;
        }

        ++displayedCount;
        const Session& first = sessions[clash.firstSession];

        out << "\n[" << std::setw(3) << std::setfill('0') << displayedCount
            << std::setfill(' ') << "] "
            << ReportFormatter::typeName(clash.type) << " | "
            << first.getDay() << " "
            << first.getStartTime() << "-" << first.getEndTime() << "\n";

        out << "  Reason    : " << clash.description << "\n";
        out << "  Session A : " << first.getSubject() << " | "
            << first.getSection() << " | " << first.getFaculty()
            << " | " << first.getRoom() << "\n";

        if (clash.secondSession != NO_SECOND_SESSION &&
            clash.secondSession < sessions.size()) {
            const Session& second = sessions[clash.secondSession];

            out << "  Session B : " << second.getSubject() << " | "
                << second.getSection() << " | " << second.getFaculty()
                << " | " << second.getRoom() << "\n";
        }
    }

    if (displayedCount == 0) {
        out << "\nNo matching conflicts.\n";
    }
}

} // namespace

namespace ReportFormatter {

// Returns the display name for a clash type.
const char* typeName(ClashType type) {
    switch (type) {
        case ClashType::Faculty:
            return "FACULTY";
        case ClashType::Room:
            return "ROOM";
        case ClashType::Section:
            return "SECTION";
        case ClashType::RoomCapacity:
            return "CAPACITY";
        case ClashType::DuplicateSession:
            return "DUPLICATE";
    }

    return "UNKNOWN";
}

// Prints session totals and clash counts.
void summary(const std::vector<Session>& sessions,
             const std::vector<Clash>& clashes) {
    std::array<int, 5> counts{};
    std::set<std::string> sections;
    std::set<std::string> faculty;
    std::set<std::string> rooms;

    for (const Session& session : sessions) {
        sections.insert(session.getSection());
        faculty.insert(session.getFaculty());
        rooms.insert(session.getRoom());
    }

    for (const Clash& clash : clashes) {
        counts.at(static_cast<int>(clash.type))++;
    }

    std::cout << "\n============================================================\n";
    std::cout << "                 TIMETABLE ANALYSIS SUMMARY\n";
    std::cout << "============================================================\n";
    std::cout << "  Sessions: " << sessions.size()
              << "   Sections: " << sections.size()
              << "   Faculty: " << faculty.size()
              << "   Rooms: " << rooms.size() << "\n";
    std::cout << "  Total conflict records: " << clashes.size() << "\n";

    for (int index = 0; index < 5; ++index) {
        std::cout << "  " << std::left << std::setw(14)
                  << typeName(static_cast<ClashType>(index))
                  << std::right << counts[index] << "\n";
    }

    std::cout << "============================================================\n";
}

// Prints the detailed clash report.
void details(const std::vector<Session>& sessions,
             const std::vector<Clash>& clashes,
             int typeFilter) {
    std::cout << "\n==================== DETAILED CLASHES ====================\n";
    writeClashDetails(std::cout, sessions, clashes, typeFilter);
    std::cout << "==========================================================\n";
}

// Exports a plain-text clash report.
bool exportTXT(const std::string& path,
               const std::vector<Session>& sessions,
               const std::vector<Clash>& clashes) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << "TIMETABLE CLASH REPORT\nSessions: " << sessions.size()
         << "\nConflicts: " << clashes.size() << "\n";
    writeClashDetails(file, sessions, clashes, -1);

    return static_cast<bool>(file);
}

// Exports clashes as CSV rows.
bool exportCSV(const std::string& path,
               const std::vector<Session>& sessions,
               const std::vector<Clash>& clashes) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << "type,day,startTime,endTime,reason,subjectA,facultyA,roomA,sectionA,"
         << "subjectB,facultyB,roomB,sectionB\n";

    for (const Clash& clash : sortClashes(clashes, sessions)) {
        const Session& first = sessions[clash.firstSession];
        const Session* second = nullptr;

        if (clash.secondSession != NO_SECOND_SESSION &&
            clash.secondSession < sessions.size()) {
            second = &sessions[clash.secondSession];
        }

        const std::vector<std::string> fields = {
            typeName(clash.type),
            first.getDay(),
            first.getStartTime(),
            first.getEndTime(),
            clash.description,
            first.getSubject(),
            first.getFaculty(),
            first.getRoom(),
            first.getSection(),
            second ? second->getSubject() : "",
            second ? second->getFaculty() : "",
            second ? second->getRoom() : "",
            second ? second->getSection() : ""
        };

        for (std::size_t index = 0; index < fields.size(); ++index) {
            if (index != 0) {
                file << ',';
            }
            file << escapeCSVField(fields[index]);
        }

        file << '\n';
    }

    return static_cast<bool>(file);
}

} // namespace ReportFormatter
