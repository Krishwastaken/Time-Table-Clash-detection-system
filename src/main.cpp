#include "Input.h"
#include "sorting.h"
#include "ClashDetector.h"
#include "ReportFormatter.h"
#include "ConflictAnalysis.h"
#include "TimetableFetcher.h"
#include "Session.h"

#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace std;

// Loads and sorts the selected timetable.
void loadTimetable(const string& path, vector<Session>& sessions,
                   vector<Clash>& clashes, bool& analyzed) {
    sessions = loadSessionsFromCSV(path);
    clashes.clear();
    analyzed = false;

    if (!sessions.empty()) {
        sortSessions(sessions);
        cout << "Loaded " << sessions.size() << " sessions from " << path << "\n";
    } else {
        cout << "No sessions loaded. Check the file path and CSV format.\n";
    }
}

// Displays the menu.
void showMenu(const string& path, size_t sessionCount)
{
    cout << "\n=====================================================\n";
    cout << "         TIMETABLE CLASH DETECTION SYSTEM\n";
    cout << "=====================================================\n";
    cout << " Dataset: " << (path.empty() ? "None selected" : path)
         << " | Sessions: " << sessionCount << "\n";
    cout << "-----------------------------------------------------\n";

    cout << " 1. Load another dataset\n"
         << " 2. Run clash detection\n"
         << " 3. View summary\n"
         << " 4. View detailed clashes\n"
         << " 5. Filter clashes by type\n"
         << " 6. Export TXT and CSV reports\n"
         << " 7. Display sorted timetable\n"
         << " 8. Consolidated conflicts\n"
         << " 9. Conflict graph (DFS)\n"
         << " 0. Exit\n"
         << " Choice: ";
}

// Prints sessions in sorted order.
void showSortedTimetable(const vector<Session>& sessions) {
    for (size_t i = 0; i < sessions.size(); ++i) {
        cout << i + 1 << ". " << sessions[i] << '\n';
    }
}

// Displays clashes of one selected type.
void filterClashes(const vector<Session>& sessions,
                   const vector<Clash>& clashes) {
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

// Exports the TXT and CSV reports.
void saveReports(const vector<Session>& sessions,
                 const vector<Clash>& clashes) {
    const bool txtSaved = ReportFormatter::exportTXT(
        "clash_report.txt", sessions, clashes);
    const bool csvSaved = ReportFormatter::exportCSV(
        "clash_report.csv", sessions, clashes);

    cout << (txtSaved && csvSaved
             ? "Reports saved: clash_report.txt, clash_report.csv\n"
             : "Could not save one or more reports.\n");
}

vector<string> getSections3()
{
    return {
        "CSE A", "CSE B", "CSE C", "CSE D",
        "CSE E", "CSE F", "CSE G", "CSE H",
        "CSE I", "CSE J", "CSE K",
        "AI-DS1", "AI-DS2",
        "AI-ML1", "AI-ML2", "AI-ML3", "AI-ML4",
        "Cyber"
    };
}

vector<string> getSections5()
{
    return {
        "A", "B", "C", "D", "E", "F", "G", "H",
        "ARQ", "ML1", "ML2", "DS1", "DS2", "Cyber", "AI"
    };
}

bool selectDataset(
    string& path,
    vector<Session>& sessions,
    vector<Clash>& clashes,
    bool& analyzed,
    int& selectedSemester)
{
    cout << "\n========== SELECT DATASET ==========\n";
    cout << "1. B.Tech CSE - 3rd Semester\n";
    cout << "2. B.Tech CSE - 5th Semester\n";
    cout << "3. Load custom CSV dataset\n";
    cout << "0. Cancel\n";
    cout << "Choice: ";

    int choice;

    if (!(cin >> choice))
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input.\n";
        return false;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    vector<string> sections;

    if (choice == 1)
    {
        path = "data/3sem.html";
        sections = getSections3();
    }
    else if (choice == 2)
    {
        path = "data/5sem.html";
        sections = getSections5();
    }
    else if (choice == 3)
    {
        cout << "Enter CSV file path: ";
        getline(cin, path);

        vector<Session> newSessions = loadSessionsFromCSV(path);

        if (newSessions.empty())
        {
            cout << "Could not load CSV dataset.\n";
            return false;
        }

        sortSessions(newSessions);

        sessions = newSessions;
        clashes.clear();
        analyzed = false;

        selectedSemester = 0; // Custom dataset

        cout << "Successfully loaded "
            << sessions.size() << " sessions from CSV.\n";

        return true;
    }
    else if (choice == 0)
    {
        return false;
    }
    else
    {
        cout << "Invalid choice.\n";
        return false;
    }

    vector<Session> newSessions;

    bool success = TimetableFetcher::generateAllSections(
        path, sections, newSessions
    );

    if (!success)
    {
        cout << "Could not generate timetable. Check the HTML file.\n";
        return false;
    }

    sortSessions(newSessions);

    sessions = newSessions;
    clashes.clear();
    analyzed = false;

    // Remember which semester is currently loaded.
    selectedSemester = (choice == 1) ? 3 : 5;

    cout << "\nSuccessfully loaded " << sessions.size()
         << " sessions from " << path << ".\n";

    return true;
}

int main()
{
    string path = "";

    vector<Session> sessions;
    vector<Clash> clashes;

    bool analyzed = false;
    int selectedSemester = 0;

    // Ask the user to select the initial dataset.
    selectDataset(path, sessions, clashes, analyzed, selectedSemester);

    while (true)
    {
        showMenu(path, sessions.size());

        int choice;

        if (!(cin >> choice))
        {
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

        if (choice == 1)
        {
            selectDataset(path, sessions, clashes, analyzed, selectedSemester);
            continue;
        }

        if (sessions.empty())
        {
            cout << "Please load a dataset first.\n";
            continue;
        }

        if (choice == 2)
        {
            bool ignorePresidentClass = (selectedSemester == 3);

            clashes = ClashDetector::detectClashes(
                sessions,
                ignorePresidentClass
            );
            analyzed = true;

            ReportFormatter::summary(sessions, clashes);
            continue;
        }

        if (choice == 7)
        {
            showSortedTimetable(sessions);
            continue;
        }

        if (choice == 8 || choice == 9)
        {
            if (!analyzed)
            {
                cout << "Run detection (option 2) first.\n";
                continue;
            }

            if (choice == 8)
                ConflictAnalysis::printIncidents(cout, sessions, clashes);
            else
                ConflictAnalysis::printGroups(cout, sessions, clashes);

            continue;
        }

        if (choice < 3 || choice > 6)
        {
            cout << "Unknown option.\n";
            continue;
        }

        if (!analyzed)
        {
            cout << "Run detection (option 2) first.\n";
            continue;
        }

        if (choice == 3)
            ReportFormatter::summary(sessions, clashes);
        else if (choice == 4)
            ReportFormatter::details(sessions, clashes);
        else if (choice == 5)
            filterClashes(sessions, clashes);
        else if (choice == 6)
            saveReports(sessions, clashes);
    }

    cout << "Goodbye!\n";

    return 0;
}
