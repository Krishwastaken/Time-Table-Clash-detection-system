#ifndef CLASH_DETECTOR_H
#define CLASH_DETECTOR_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include "Session.h"

enum class ClashType {
    Faculty,
    Room,
    Section,
    RoomCapacity,
    DuplicateSession
};

struct Clash {
    ClashType type;

    // Index of the first session in the supplied vector.
    std::size_t firstSession;

    // Index of the second session, or -1 for a single-session clash.
    std::size_t secondSession;

    std::string description;

    // Weeks in which this clash really exists:
    //   Always              - every week
    //   SaturdayOffOnly     - only when Saturday is off (involves a "*" class)
    //   SaturdayWorkingOnly - only when Saturday is a working day
    WeekRule scenario = WeekRule::Always;
};

class ClashDetector {
public:
    // Rooms that are really several identical rooms: normalised (upper case)
    // room name -> number of sessions that may run there at once.
    using RoomPools = std::map<std::string, int>;

    // The pools used by default (the 4-lab Logic Design lab block).
    static RoomPools defaultRoomPools();

    // Returns all detected clashes. Does not modify the input vector.
    //
    // The input may be in any order (main() merge-sorts it first, which
    // keeps the report in day/time order).
    //
    // ignorePresidentClass: when true, "Hon'ble President Sir" is not treated
    // as a faculty member for faculty-clash purposes (he is a nominal name
    // on many parallel lectures). The session itself still takes part in
    // room and section checks.
    static std::vector<Clash> detectClashes(
        const std::vector<Session>& sessions,
        bool ignorePresidentClass = false
    );

    // Same, with an explicit set of room pools.
    static std::vector<Clash> detectClashes(
        const std::vector<Session>& sessions,
        bool ignorePresidentClass,
        const RoomPools& roomPools
    );

    // Prints a human-readable clash report.
    static void printClashes(const std::vector<Clash>& clashes,
                             const std::vector<Session>& sessions);
};

#endif
