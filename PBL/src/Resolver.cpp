#include "Resolver.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <set>

namespace {

std::string normalizeStr(const std::string& value) {
    auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
    auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    if (first >= last) return "";
    std::string result(first, last);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

int toMins(const std::string& time) {
    std::string v = normalizeStr(time);
    if (v.size() != 5 || v[2] != ':') return -1;
    if (!std::isdigit(v[0]) || !std::isdigit(v[1]) || !std::isdigit(v[3]) || !std::isdigit(v[4])) return -1;
    return std::stoi(v.substr(0, 2)) * 60 + std::stoi(v.substr(3, 2));
}

bool intervalsOverlap(int aStart, int aEnd, int bStart, int bEnd) {
    return aStart < bEnd && bStart < aEnd;
}

int roomCap(const std::string& room) {
    std::string r = normalizeStr(room);
    if (r.rfind("CR", 0) == 0) return 100;
    if (r.rfind("LT", 0) == 0) return 200;
    if (r.find("AUDI") != std::string::npos) return 500;
    if (r.find("LAB") != std::string::npos) return 45;
    return 100;
}

const std::vector<std::string> CANDIDATE_ROOMS = {
    "LT-1", "LT-2", "LT-5", "LT-6", "LT-7", "LT-9", "LT-13", "LT-14", "LT-15",
    "CR-2", "CR-3", "CR-4", "CR-5", "CR-6", "CR-7", "CR-8", "CR-12", "CR-17",
    "Digital Lab-1", "Digital Lab-2", "Lab-1", "Lab-2", "Lab-5", "Lab-6", "Lab-7",
    "New Lab-1", "New Lab-2", "GEHU-Audi (5th Floor)"
};

struct TimeSlot {
    std::string start;
    std::string end;
};

const std::vector<TimeSlot> CANDIDATE_SLOTS = {
    {"08:00", "08:55"},
    {"09:00", "09:55"},
    {"09:55", "10:50"},
    {"11:10", "12:05"},
    {"12:05", "13:00"},
    {"13:55", "14:50"},
    {"15:10", "16:05"},
    {"16:05", "17:00"},
    {"17:00", "17:55"}
};

const std::vector<std::string> WEEKDAYS = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

bool isRoomOccupied(const std::string& room, const std::string& day, int startMin, int endMin,
                    const std::vector<Session>& sessions, size_t ignoreIdx) {
    std::string normR = normalizeStr(room);
    std::string normD = normalizeStr(day);
    for (size_t i = 0; i < sessions.size(); ++i) {
        if (i == ignoreIdx) continue;
        const auto& s = sessions[i];
        if (normalizeStr(s.getDay()) != normD) continue;
        if (normalizeStr(s.getRoom()) != normR) continue;
        int sStart = toMins(s.getStartTime());
        int sEnd = toMins(s.getEndTime());
        if (sStart >= 0 && sEnd >= 0 && intervalsOverlap(startMin, endMin, sStart, sEnd)) {
            return true;
        }
    }
    return false;
}

bool isFacultyOccupied(const std::string& faculty, const std::string& day, int startMin, int endMin,
                       const std::vector<Session>& sessions, size_t ignoreIdx) {
    std::string normF = normalizeStr(faculty);
    if (normF.empty()) return false;
    std::string normD = normalizeStr(day);
    for (size_t i = 0; i < sessions.size(); ++i) {
        if (i == ignoreIdx) continue;
        const auto& s = sessions[i];
        if (normalizeStr(s.getDay()) != normD) continue;
        if (normalizeStr(s.getFaculty()) != normF) continue;
        int sStart = toMins(s.getStartTime());
        int sEnd = toMins(s.getEndTime());
        if (sStart >= 0 && sEnd >= 0 && intervalsOverlap(startMin, endMin, sStart, sEnd)) {
            return true;
        }
    }
    return false;
}

bool isSectionOccupied(const std::string& section, const std::string& day, int startMin, int endMin,
                       const std::vector<Session>& sessions, size_t ignoreIdx) {
    std::string normSec = normalizeStr(section);
    if (normSec.empty()) return false;
    std::string normD = normalizeStr(day);
    for (size_t i = 0; i < sessions.size(); ++i) {
        if (i == ignoreIdx) continue;
        const auto& s = sessions[i];
        if (normalizeStr(s.getDay()) != normD) continue;
        if (normalizeStr(s.getSection()) != normSec) continue;
        int sStart = toMins(s.getStartTime());
        int sEnd = toMins(s.getEndTime());
        if (sStart >= 0 && sEnd >= 0 && intervalsOverlap(startMin, endMin, sStart, sEnd)) {
            return true;
        }
    }
    return false;
}

} // namespace

ResolutionProposal Resolver::proposeResolution(
    size_t clashIndex,
    const std::vector<Clash>& clashes,
    const std::vector<Session>& sessions
) {
    ResolutionProposal proposal;
    if (clashIndex >= clashes.size()) {
        proposal.explanation = "Invalid clash index.";
        proposal.feasible = false;
        return proposal;
    }

    const Clash& clash = clashes[clashIndex];
    proposal.clashIndex = clashIndex;
    proposal.clashType = clash.type;

    if (clash.firstSession >= sessions.size()) {
        proposal.explanation = "Referenced session index out of bounds.";
        proposal.feasible = false;
        return proposal;
    }

    // 1. DUPLICATE SESSION
    if (clash.type == ClashType::DuplicateSession) {
        size_t targetIdx = (clash.secondSession != static_cast<size_t>(-1) && clash.secondSession < sessions.size())
                           ? clash.secondSession : clash.firstSession;
        proposal.targetSessionIndex = targetIdx;
        proposal.originalSession = sessions[targetIdx];
        proposal.proposedSession = sessions[targetIdx];
        proposal.action = "REMOVE_DUPLICATE";
        proposal.explanation = "Remove redundant duplicate session record from timetable.";
        proposal.feasible = true;
        return proposal;
    }

    // 2. ROOM CAPACITY CLASH
    if (clash.type == ClashType::RoomCapacity) {
        size_t targetIdx = clash.firstSession;
        const Session& target = sessions[targetIdx];
        proposal.targetSessionIndex = targetIdx;
        proposal.originalSession = target;
        proposal.proposedSession = target;

        int reqStudents = target.getStudentCount();
        int curCap = roomCap(target.getRoom());
        int startMin = toMins(target.getStartTime());
        int endMin = toMins(target.getEndTime());

        // Find candidate room that has enough capacity and is free at that time
        std::string foundRoom;
        int bestCap = 99999;
        for (const auto& r : CANDIDATE_ROOMS) {
            int c = roomCap(r);
            if (c >= reqStudents && c < bestCap) {
                if (!isRoomOccupied(r, target.getDay(), startMin, endMin, sessions, targetIdx)) {
                    foundRoom = r;
                    bestCap = c;
                }
            }
        }

        if (!foundRoom.empty()) {
            proposal.action = "CHANGE_ROOM";
            proposal.proposedSession.setRoom(foundRoom);
            std::ostringstream ss;
            ss << "Move session from " << target.getRoom() << " (capacity " << curCap
               << ") to " << foundRoom << " (capacity " << bestCap
               << ") to accommodate " << reqStudents << " students.";
            proposal.explanation = ss.str();
            proposal.feasible = true;
        } else {
            // If no room is free at that time, find alternate time slot with suitable room
            for (const auto& slot : CANDIDATE_SLOTS) {
                int sMin = toMins(slot.start);
                int eMin = toMins(slot.end);
                for (const auto& r : CANDIDATE_ROOMS) {
                    if (roomCap(r) >= reqStudents &&
                        !isRoomOccupied(r, target.getDay(), sMin, eMin, sessions, targetIdx) &&
                        !isFacultyOccupied(target.getFaculty(), target.getDay(), sMin, eMin, sessions, targetIdx) &&
                        !isSectionOccupied(target.getSection(), target.getDay(), sMin, eMin, sessions, targetIdx)) {
                        proposal.action = "CHANGE_TIME_AND_ROOM";
                        proposal.proposedSession.setRoom(r);
                        proposal.proposedSession.setStartTime(slot.start);
                        proposal.proposedSession.setEndTime(slot.end);
                        std::ostringstream ss;
                        ss << "Reschedule session to " << target.getDay() << " " << slot.start << "-" << slot.end
                           << " in room " << r << " (capacity " << roomCap(r) << ").";
                        proposal.explanation = ss.str();
                        proposal.feasible = true;
                        break;
                    }
                }
                if (proposal.feasible) break;
            }
            if (!proposal.feasible) {
                proposal.explanation = "No free room with capacity >= " + std::to_string(reqStudents) + " was found.";
            }
        }
        return proposal;
    }

    // Pairwise clashes: Faculty, Room, Section
    size_t s1Idx = clash.firstSession;
    size_t s2Idx = clash.secondSession;
    if (s2Idx >= sessions.size()) {
        s2Idx = s1Idx;
    }
    // Target session is typically s2 (the overlapping peer)
    size_t targetIdx = s2Idx;
    const Session& target = sessions[targetIdx];
    proposal.targetSessionIndex = targetIdx;
    proposal.originalSession = target;
    proposal.proposedSession = target;

    // 3. ROOM CLASH
    if (clash.type == ClashType::Room) {
        int reqStudents = target.getStudentCount();
        int startMin = toMins(target.getStartTime());
        int endMin = toMins(target.getEndTime());

        // First attempt: keep time, change room to another unoccupied room
        std::string foundRoom;
        for (const auto& r : CANDIDATE_ROOMS) {
            if (normalizeStr(r) == normalizeStr(target.getRoom())) continue;
            if (roomCap(r) >= reqStudents &&
                !isRoomOccupied(r, target.getDay(), startMin, endMin, sessions, targetIdx)) {
                foundRoom = r;
                break;
            }
        }

        if (!foundRoom.empty()) {
            proposal.action = "CHANGE_ROOM";
            proposal.proposedSession.setRoom(foundRoom);
            std::ostringstream ss;
            ss << "Move conflicting session for section " << target.getSection()
               << " (" << target.getSubject() << ") from " << target.getRoom()
               << " to available room " << foundRoom << " at " << target.getStartTime() << "-" << target.getEndTime() << ".";
            proposal.explanation = ss.str();
            proposal.feasible = true;
            return proposal;
        }

        // If no alternate room free at same time, find alternate time slot
        for (const auto& slot : CANDIDATE_SLOTS) {
            int sMin = toMins(slot.start);
            int eMin = toMins(slot.end);
            if (!isRoomOccupied(target.getRoom(), target.getDay(), sMin, eMin, sessions, targetIdx) &&
                !isFacultyOccupied(target.getFaculty(), target.getDay(), sMin, eMin, sessions, targetIdx) &&
                !isSectionOccupied(target.getSection(), target.getDay(), sMin, eMin, sessions, targetIdx)) {
                proposal.action = "CHANGE_TIME";
                proposal.proposedSession.setStartTime(slot.start);
                proposal.proposedSession.setEndTime(slot.end);
                std::ostringstream ss;
                ss << "Reschedule session for section " << target.getSection()
                   << " to " << target.getDay() << " " << slot.start << "-" << slot.end
                   << " in room " << target.getRoom() << ".";
                proposal.explanation = ss.str();
                proposal.feasible = true;
                return proposal;
            }
        }

        proposal.explanation = "No immediate conflict-free alternate room or time slot found for room clash.";
        return proposal;
    }

    // 4. FACULTY CLASH & 5. SECTION CLASH
    if (clash.type == ClashType::Faculty || clash.type == ClashType::Section) {
        int reqStudents = target.getStudentCount();

        // Search for alternate time slot on the same day first, then other weekdays
        std::vector<std::string> searchDays = { target.getDay() };
        for (const auto& d : WEEKDAYS) {
            if (normalizeStr(d) != normalizeStr(target.getDay())) {
                searchDays.push_back(d);
            }
        }

        for (const auto& d : searchDays) {
            for (const auto& slot : CANDIDATE_SLOTS) {
                int sMin = toMins(slot.start);
                int eMin = toMins(slot.end);
                // Check if faculty and section are free
                if (!isFacultyOccupied(target.getFaculty(), d, sMin, eMin, sessions, targetIdx) &&
                    !isSectionOccupied(target.getSection(), d, sMin, eMin, sessions, targetIdx)) {
                    
                    // Check if current room or another suitable room is free
                    std::string assignedRoom;
                    if (!isRoomOccupied(target.getRoom(), d, sMin, eMin, sessions, targetIdx) &&
                        roomCap(target.getRoom()) >= reqStudents) {
                        assignedRoom = target.getRoom();
                    } else {
                        for (const auto& r : CANDIDATE_ROOMS) {
                            if (roomCap(r) >= reqStudents && !isRoomOccupied(r, d, sMin, eMin, sessions, targetIdx)) {
                                assignedRoom = r;
                                break;
                            }
                        }
                    }

                    if (!assignedRoom.empty()) {
                        proposal.action = (assignedRoom == target.getRoom()) ? "CHANGE_TIME" : "CHANGE_TIME_AND_ROOM";
                        proposal.proposedSession.setDay(d);
                        proposal.proposedSession.setStartTime(slot.start);
                        proposal.proposedSession.setEndTime(slot.end);
                        proposal.proposedSession.setRoom(assignedRoom);

                        std::ostringstream ss;
                        if (clash.type == ClashType::Faculty) {
                            ss << "Reschedule session for section " << target.getSection()
                               << " to " << d << " " << slot.start << "-" << slot.end
                               << " in " << assignedRoom
                               << " where faculty " << target.getFaculty() << " is available.";
                        } else {
                            ss << "Reschedule session for section " << target.getSection()
                               << " to " << d << " " << slot.start << "-" << slot.end
                               << " in " << assignedRoom
                               << " to resolve section overlap.";
                        }
                        proposal.explanation = ss.str();
                        proposal.feasible = true;
                        return proposal;
                    }
                }
            }
        }

        proposal.explanation = "Unable to find an alternate conflict-free slot for faculty/section.";
        return proposal;
    }

    proposal.explanation = "Unknown clash type resolution.";
    return proposal;
}

std::vector<ResolutionProposal> Resolver::proposeAllResolutions(
    const std::vector<Clash>& clashes,
    const std::vector<Session>& sessions
) {
    std::vector<ResolutionProposal> proposals;
    proposals.reserve(clashes.size());
    for (size_t i = 0; i < clashes.size(); ++i) {
        proposals.push_back(proposeResolution(i, clashes, sessions));
    }
    return proposals;
}

std::vector<Session> Resolver::applyResolution(
    const ResolutionProposal& proposal,
    const std::vector<Session>& sessions
) {
    std::vector<Session> updated = sessions;
    if (!proposal.feasible) return updated;

    if (proposal.action == "REMOVE_DUPLICATE") {
        if (proposal.targetSessionIndex < updated.size()) {
            updated.erase(updated.begin() + proposal.targetSessionIndex);
        }
    } else if (proposal.targetSessionIndex < updated.size()) {
        updated[proposal.targetSessionIndex] = proposal.proposedSession;
    }
    return updated;
}
