
#ifndef CLASH_DETECTOR_H
#define CLASH_DETECTOR_H

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
};

class ClashDetector {
public:
    // Returns all detected clashes. Does not modify the input vector.
    static std::vector<Clash> detectClashes(
        const std::vector<Session>& sessions
    );

    // Prints a human-readable clash report.
    static void printClashes(const std::vector<Clash>& clashes,
                             const std::vector<Session>& sessions);
};

#endif
