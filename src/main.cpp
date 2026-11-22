#include "Input.h"
#include "sorting.h"
#include "ClashDetector.h"
#include "ReportFormatter.h"
#include "ConflictAnalysis.h"
#include "TimetableFetcher.h"
#include "Resolver.h"
#include "JsonExport.h"
#include "Session.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace std;

namespace {

bool endsWith(const string& text, const string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool fileExists(const string& path) {
    ifstream f(path);
    return static_cast<bool>(f);
}

// Loads a timetable from a saved college page (.html) or from a CSV file.
// If the page cannot be read, a CSV with the same name is used instead.
bool loadDataset(const string& path, vector<Session>& sessions) {
    vector<Session> loaded;

    if (endsWith(path, ".csv")) {
        loaded = loadSessionsFromCSV(path);
    } else if (!TimetableFetcher::generateAllSections(path, {}, loaded)) {
        const string fallback = path.substr(0, path.rfind('.')) + ".csv";

        cout << "Could not read " << path << ".\n";

        if (fileExists(fallback)) {
            cout << "Using the CSV fallback " << fallback << " instead.\n";
            loaded = loadSessionsFromCSV(fallback);
        }
    }

    if (loaded.empty())
        return false;

    sortSessions(loaded);          // Merge Sort: weekday, then start time
    sessions = loaded;
    return true;
}

void showMenu(const string& path, size_t sessionCount) {
    cout << "\n=====================================================\n";
    cout << "         TIMETABLE CLASH DETECTION SYSTEM\n";
    cout << "=====================================================\n";
    cout << " Dataset: " << (path.empty() ? "None selected" : path)
         << " | Sessions: " << sessionCount << "\n";
    cout << "-----------------------------------------------------\n";
    cout << " 1.  Load a dataset\n"
         << " 2.  Run clash detection\n"
         << " 3.  View summary\n"
         << " 4.  View detailed clashes\n"
         << " 5.  Filter clashes by type\n"
         << " 6.  Export TXT and CSV clash reports\n"
         << " 7.  Display sorted timetable\n"
         << " 8.  Consolidated conflicts (shared classes grouped)\n"
         << " 9.  Conflict graph (DFS)\n"
         << " 10. Auto-resolve clashes (preview, then apply)\n"
         << " 11. Generate clash-free timetable (saves 4 files)\n"
         << " 12. Export everything to JSON for the frontend\n"
         << " 13. Download a timetable page from a URL\n"
         << " 0.  Exit\n"
         << " Choice: ";
}

// Detect, resolve a copy, and write original + resolved data to JSON.
bool exportJson(const string& jsonPath, const string& title,
                const vector<Session>& sessions, const vector<Clash>& clashes) {
    vector<Session> resolved = sessions;
    ResolveResult result = Resolver::resolve(resolved, clashes);

    return JsonExport::write(jsonPath, title, sessions, clashes, resolved, result);
}

// Non-interactive mode for the frontend:  timetable.exe --export <input> <output.json>
int runExport(const string& input, const string& output) {
    vector<Session> sessions;

    if (!loadDataset(input, sessions)) {
        cerr << "Could not load " << input << "\n";
        return 1;
    }

    vector<Clash> clashes = ClashDetector::detectClashes(sessions, true);

    if (!exportJson(output, input, sessions, clashes)) {
        cerr << "Could not write " << output << "\n";
        return 1;
    }

    cout << "Wrote " << output << " (" << sessions.size() << " sessions, "
         << clashes.size() << " clash records)\n";
    return 0;
}

bool selectDataset(string& path, vector<Session>& sessions,
                   vector<Clash>& clashes, bool& analyzed) {
    cout << "\n========== SELECT DATASET ==========\n"
         << "1. B.Tech CSE - 3rd Semester (data/3sem.html)\n"
         << "2. B.Tech CSE - 5th Semester (data/5sem.html)\n"
         << "3. Another file (.html or .csv)\n"
         << "0. Cancel\n"
         << "Choice: ";

    int choice;

    if (!(cin >> choice)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input.\n";
        return false;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string newPath;

    if (choice == 1)      newPath = "data/3sem.html";
    else if (choice == 2) newPath = "data/5sem.html";
    else if (choice == 3) {
        cout << "File path: ";
        getline(cin, newPath);
    } else {
        return false;
    }

    vector<Session> loaded;

    if (!loadDataset(newPath, loaded)) {
        cout << "Could not load a timetable from " << newPath << ".\n";
        return false;
    }

    path = newPath;
    sessions = loaded;
    clashes.clear();
    analyzed = false;

    cout << "\nLoaded " << sessions.size() << " sessions from " << path << ".\n";
    return true;
}

void filterClashes(const vector<Session>& sessions, const vector<Clash>& clashes) {
    cout << "Filter: 0 Faculty | 1 Room | 2 Section | 3 Capacity | 4 Duplicate\nType: ";

    int type;
    if (cin >> type && type >= 0 && type <= 4) {
        ReportFormatter::details(sessions, clashes, type);
    } else {
        cin.clear();
        cout << "Invalid type.\n";
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 4 && string(argv[1]) == "--export")
        return runExport(argv[2], argv[3]);

    string path;
    vector<Session> sessions;
    vector<Clash> clashes;
    bool analyzed = false;

    selectDataset(path, sessions, clashes, analyzed);

    while (true) {
        showMenu(path, sessions.size());

        int choice;

        if (!(cin >> choice)) {
            if (cin.eof())
                break;

            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input. Enter a number.\n";
            continue;
        }

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (choice == 0)
            break;

        if (choice == 1) {
            selectDataset(path, sessions, clashes, analyzed);
            continue;
        }

        if (choice == 13) {
            string url, out;
            cout << "URL: ";
            getline(cin, url);
            cout << "Save as (e.g. data/3sem.html): ";
            getline(cin, out);
            TimetableFetcher::downloadTimetable(url, out);
            continue;
        }

        if (sessions.empty()) {
            cout << "Please load a dataset first.\n";
            continue;
        }

        if (choice == 2) {
            clashes = ClashDetector::detectClashes(sessions, true);
            analyzed = true;
            ReportFormatter::summary(sessions, clashes);
            continue;
        }

        if (choice == 7) {
            for (size_t i = 0; i < sessions.size(); ++i)
                cout << i + 1 << ". " << sessions[i] << '\n';
            continue;
        }

        if (choice < 3 || choice > 12) {
            cout << "Unknown option.\n";
            continue;
        }

        if (!analyzed) {
            cout << "Run detection (option 2) first.\n";
            continue;
        }

        if (choice == 3) {
            ReportFormatter::summary(sessions, clashes);
        } else if (choice == 4) {
            ReportFormatter::details(sessions, clashes);
        } else if (choice == 5) {
            filterClashes(sessions, clashes);
        } else if (choice == 6) {
            const bool txt = ReportFormatter::exportTXT("clash_report.txt", sessions, clashes);
            const bool csv = ReportFormatter::exportCSV("clash_report.csv", sessions, clashes);
            cout << (txt && csv ? "Reports saved: clash_report.txt, clash_report.csv\n"
                                : "Could not save one or more reports.\n");
        } else if (choice == 8) {
            ConflictAnalysis::printIncidents(cout, sessions, clashes);
        } else if (choice == 9) {
            ConflictAnalysis::printGroups(cout, sessions, clashes);
        } else if (choice == 10) {
            if (clashes.empty()) {
                cout << "No clashes to resolve.\n";
                continue;
            }

            vector<Session> proposal = sessions;               // work on a copy
            ResolveResult r = Resolver::resolve(proposal, clashes);
            Resolver::print(cout, r);

            if (r.applied.empty())
                continue;

            cout << "Apply these changes? (y/n): ";
            string answer;
            getline(cin, answer);

            if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
                sessions = proposal;
                clashes = ClashDetector::detectClashes(sessions, true);
                cout << "Applied. " << clashes.size() << " conflict record(s) remain.\n";
                cout << (Resolver::exportTimetable("resolved_timetable.csv", sessions)
                             ? "Saved resolved_timetable.csv\n"
                             : "Could not save resolved_timetable.csv\n");
            } else {
                cout << "Discarded.\n";
            }
        } else if (choice == 11) {
            vector<Session> resolved = sessions;               // the original stays untouched
            ResolveResult r = Resolver::resolve(resolved, clashes);
            Resolver::print(cout, r);

            const bool ok =
                Resolver::exportTimetable("original_timetable.csv", sessions) &&
                Resolver::exportTimetable("clash_free_timetable.csv", resolved) &&
                Resolver::exportChangeLog("change_log.csv", r) &&
                Resolver::exportUnresolved("unresolved_clashes.csv", r, resolved);

            cout << (ok ? "Saved: original_timetable.csv, clash_free_timetable.csv, "
                          "change_log.csv, unresolved_clashes.csv\n"
                        : "Could not save one or more files.\n");

            if (r.clashesAfter > 0)
                cout << "Note: " << r.clashesAfter << " clash record(s) cannot be fixed by "
                     << "changing rooms (class times are fixed); see unresolved_clashes.csv.\n";
        } else if (choice == 12) {
            cout << (exportJson("timetable_report.json", path, sessions, clashes)
                         ? "Saved timetable_report.json\n"
                         : "Could not save timetable_report.json\n");
        }
    }

    cout << "Goodbye!\n";
    return 0;
}
