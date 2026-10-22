#include "ConflictAnalysis.h"
#include "ReportFormatter.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <set>
#include <stack>
#include <tuple>
#include <utility>

namespace {

constexpr std::size_t NO_SECOND_SESSION = static_cast<std::size_t>(-1);

// Sorts weekdays.
int dayOrder(std::string day) {
    for (char& character : day) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))
        );
    }

    const std::vector<std::string> weekdays = {
        "monday", "tuesday", "wednesday", "thursday",
        "friday", "saturday", "sunday"
    };

    const auto position = std::find(weekdays.begin(), weekdays.end(), day);
    if (position == weekdays.end()) {
        return 7;
    }

    return static_cast<int>(position - weekdays.begin());
}

}

namespace ConflictAnalysis {

std::vector<Incident> consolidate(const std::vector<Clash>& clashes,
                                  const std::vector<Session>& sessions) {

    std::map<std::pair<std::size_t, std::size_t>, std::vector<Clash>> clashesByPair;

    for (const Clash& clash : clashes) {
        const bool invalidFirst = clash.firstSession >= sessions.size();
        const bool invalidSecond =
            clash.secondSession != NO_SECOND_SESSION &&
            clash.secondSession >= sessions.size();

        if (invalidFirst || invalidSecond) {
            continue;
        }

        std::size_t firstIndex = clash.firstSession;
        std::size_t secondIndex = clash.secondSession;

        if (secondIndex != NO_SECOND_SESSION && secondIndex < firstIndex) {
            std::swap(firstIndex, secondIndex);
        }

        clashesByPair[{firstIndex, secondIndex}].push_back(clash);
    }

    std::vector<Incident> incidents;

    for (auto& entry : clashesByPair) {
        std::vector<Clash>& reasons = entry.second;

        std::sort(reasons.begin(), reasons.end(),
                  [](const Clash& left, const Clash& right) {
                      return static_cast<int>(left.type) <
                             static_cast<int>(right.type);
                  });

        incidents.push_back({
            entry.first.first,
            entry.first.second,
            std::move(reasons)
        });
    }

    std::sort(incidents.begin(), incidents.end(),
              [&](const Incident& left, const Incident& right) {
                  const Session& leftSession = sessions[left.first];
                  const Session& rightSession = sessions[right.first];

                  return std::make_tuple(
                             dayOrder(leftSession.getDay()),
                             leftSession.getStartTime(),
                             left.first, left.second
                         ) < std::make_tuple(
                             dayOrder(rightSession.getDay()),
                             rightSession.getStartTime(),
                             right.first, right.second
                         );
              });

    return incidents;
}

std::vector<Component> connectedGroups(const std::vector<Clash>& clashes,
                                       std::size_t sessionCount) {

    std::vector<std::set<std::size_t>> neighbors(sessionCount);

    for (const Clash& clash : clashes) {
        if (clash.firstSession >= sessionCount ||
            clash.secondSession >= sessionCount ||
            clash.firstSession == clash.secondSession) {
            continue;
        }

        neighbors[clash.firstSession].insert(clash.secondSession);
        neighbors[clash.secondSession].insert(clash.firstSession);
    }

    std::vector<char> visited(sessionCount, false);
    std::vector<Component> groups;

    for (std::size_t start = 0; start < sessionCount; ++start) {
        if (visited[start] || neighbors[start].empty()) {
            continue;
        }

        Component group;
        std::stack<std::size_t> pending;
        pending.push(start);
        visited[start] = true;

        while (!pending.empty()) {
            const std::size_t current = pending.top();
            pending.pop();

            group.sessions.push_back(current);
            group.conflictEdges += neighbors[current].size();

            for (std::size_t next : neighbors[current]) {
                if (!visited[next]) {
                    visited[next] = true;
                    pending.push(next);
                }
            }
        }

        std::sort(group.sessions.begin(), group.sessions.end());

        group.conflictEdges /= 2;
        groups.push_back(std::move(group));
    }

    std::sort(groups.begin(), groups.end(),
              [](const Component& left, const Component& right) {
                  return left.sessions.front() < right.sessions.front();
              });

    return groups;
}

void printIncidents(std::ostream& out, const std::vector<Session>& sessions,
                    const std::vector<Clash>& clashes) {
    const std::vector<Incident> incidents = consolidate(clashes, sessions);

    out << "\n================ CONSOLIDATED CONFLICTS ================\n";
    out << "Raw conflict reasons: " << clashes.size()
        << " | Distinct incidents: " << incidents.size() << "\n";

    for (std::size_t index = 0; index < incidents.size(); ++index) {
        const Incident& incident = incidents[index];
        const Session& firstSession = sessions[incident.first];

        out << "\n[" << std::setw(3) << std::setfill('0') << index + 1
            << std::setfill(' ') << "] "
            << firstSession.getDay() << " "
            << firstSession.getStartTime() << "-"
            << firstSession.getEndTime() << "\n";

        out << "  Session A (#" << incident.first + 1 << "): "
            << firstSession.getSubject() << " | "
            << firstSession.getSection() << " | "
            << firstSession.getFaculty() << " | "
            << firstSession.getRoom() << "\n";

        if (incident.second != NO_SECOND_SESSION) {
            const Session& secondSession = sessions[incident.second];

            out << "  Session B (#" << incident.second + 1 << "): "
                << secondSession.getSubject() << " | "
                << secondSession.getSection() << " | "
                << secondSession.getFaculty() << " | "
                << secondSession.getRoom() << "\n";
        }

        for (const Clash& reason : incident.reasons) {
            out << "    - " << ReportFormatter::typeName(reason.type)
                << ": " << reason.description << "\n";
        }
    }

    if (incidents.empty()) {
        out << "No conflicts detected.\n";
    }

    out << "========================================================\n";
}

void printGroups(std::ostream& out, const std::vector<Session>& sessions,
                 const std::vector<Clash>& clashes) {
    const std::vector<Component> groups = connectedGroups(clashes, sessions.size());

    out << "\n================= CONFLICT GRAPH (DFS) =================\n";
    out << "Connected groups: " << groups.size()
        << " (single-session capacity issues excluded)\n";

    for (std::size_t index = 0; index < groups.size(); ++index) {
        const Component& group = groups[index];

        out << "\nGroup " << index + 1
            << " | Sessions: " << group.sessions.size()
            << " | Unique pairwise edges: " << group.conflictEdges << "\n";

        for (std::size_t sessionIndex : group.sessions) {
            const Session& session = sessions[sessionIndex];

            out << "  #" << sessionIndex + 1 << " "
                << session.getDay() << " "
                << session.getStartTime() << "-" << session.getEndTime()
                << " | " << session.getSubject()
                << " | " << session.getSection()
                << " | " << session.getRoom() << "\n";
        }
    }

    if (groups.empty()) {
        out << "No connected conflict groups.\n";
    }

    out << "========================================================\n";
}

}
