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

    WeekRule scenario = WeekRule::Always;
};

class ClashDetector {
public:
    using RoomPools = std::map<std::string, int>;

    // The pools used by default (the 4-lab Logic Design lab block).
    static RoomPools defaultRoomPools();


    static std::vector<Clash> detectClashes(
        const std::vector<Session>& sessions,
        bool ignorePresidentClass = false
    );

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
