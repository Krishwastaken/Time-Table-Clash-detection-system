
#include <iostream>
#include "TimetableFetcher.h"

int main()
{
    const std::string url =
        "https://csitgeu.in/wp/wp-content/uploads/2026/11/Time-Table-of-B.-Tech.-CSE-Vth-Semester-12-07-2026.html";

    const std::string outputPath = "data/timetable.html";

    if (TimetableFetcher::downloadTimetable(url, outputPath))
    {
        std::cout << "\nFetcher test successful.\n";
    }
    else
    {
        std::cout << "\nFetcher test failed.\n";
    }

    return 0;
}
