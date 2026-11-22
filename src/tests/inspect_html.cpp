#include <iostream>
#include <vector>
#include <string>

#include "TimetableFetcher.h"
#include "Session.h"

int main()
{
    std::vector<Session> sessions;

    std::vector<std::string> sections5 = {
        "A", "B", "C", "D", "E", "F", "G", "H",
        "ARQ", "ML1", "ML2", "DS1", "DS2", "Cyber", "AI"
    };

    bool success = TimetableFetcher::generateAllSections(
        "data/5sem.html",
        sections5,
        sessions
    );

    if (!success)
    {
        std::cout << "\nGeneration failed.\n";
        return 1;
    }

    std::cout << "\nTotal sessions generated: "
              << sessions.size() << '\n';

    std::cout << "\n========== GENERATED SESSIONS ==========\n";

    for (const Session& session : sessions)
    {
        std::cout << session << '\n';
    }

    return 0;
}