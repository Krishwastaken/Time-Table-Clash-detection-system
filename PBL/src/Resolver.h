#ifndef RESOLVER_H
#define RESOLVER_H

#include <vector>
#include <string>
#include "Session.h"
#include "ClashDetector.h"

struct ResolutionProposal {
    size_t clashIndex = 0;
    ClashType clashType = ClashType::Faculty;
    std::string action; // "CHANGE_ROOM", "CHANGE_TIME", "REMOVE_DUPLICATE"
    size_t targetSessionIndex = 0;
    Session originalSession;
    Session proposedSession;
    std::string explanation;
    bool feasible = false;
};

class Resolver {
public:
    static ResolutionProposal proposeResolution(
        size_t clashIndex,
        const std::vector<Clash>& clashes,
        const std::vector<Session>& sessions
    );

    static std::vector<ResolutionProposal> proposeAllResolutions(
        const std::vector<Clash>& clashes,
        const std::vector<Session>& sessions
    );

    static std::vector<Session> applyResolution(
        const ResolutionProposal& proposal,
        const std::vector<Session>& sessions
    );
};

#endif
