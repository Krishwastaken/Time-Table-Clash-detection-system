
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ClashDetector.h"

using namespace std;

vector<string> splitCSVLine(const string& line) {
    vector<string> fields;
    string field;
    bool insideQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];

        if (ch == '"') {
            if (insideQuotes &&
                i + 1 < line.size() &&
                line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (ch == ',' && !insideQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += ch;
        }
    }

    fields.push_back(field);
    return fields;
}

string trim(string value) {
    auto notSpace = [](unsigned char ch) {
        return !isspace(ch);
    };

    value.erase(
        value.begin(),
        find_if(value.begin(), value.end(), notSpace)
    );

    value.erase(
        find_if(value.rbegin(), value.rend(), notSpace).base(),
        value.end()
    );

    return value;
}

string normalizeHeader(string value) {
    value = trim(value);

    transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(tolower(ch));
        }
    );

    return value;
}

bool loadTimetable(
    const string& fileName,
    vector<Session>& sessions
) {
    ifstream file(fileName);

    if (!file.is_open()) {
        cerr << "Could not open CSV file: "
             << fileName << '\n';
        return false;
    }

    string line;

    if (!getline(file, line)) {
        cerr << "The CSV file is empty.\n";
        return false;
    }

    vector<string> headers = splitCSVLine(line);
    unordered_map<string, size_t> column;

    for (size_t i = 0; i < headers.size(); ++i) {
        column[normalizeHeader(headers[i])] = i;
    }

    const vector<string> required = {
        "subject", "faculty", "room", "section",
        "day", "starttime", "endtime", "studentcount"
    };

    for (const string& name : required) {
        if (column.find(name) == column.end()) {
            cerr << "Missing required CSV column: "
                 << name << '\n';
            return false;
        }
    }

    size_t lineNumber = 1;

    while (getline(file, line)) {
        ++lineNumber;

        if (trim(line).empty()) {
            continue;
        }

        vector<string> fields = splitCSVLine(line);

        size_t largestColumn = 0;

        for (const string& name : required) {
            largestColumn = max(
                largestColumn, column.at(name)
            );
        }

        if (fields.size() <= largestColumn) {
            cerr << "Invalid row at line "
                 << lineNumber << ".\n";
            return false;
        }

        try {
            string subject = trim(fields[column.at("subject")]);
            string faculty = trim(fields[column.at("faculty")]);
            string room = trim(fields[column.at("room")]);
            string section = trim(fields[column.at("section")]);
            string day = trim(fields[column.at("day")]);
            string start = trim(fields[column.at("starttime")]);
            string end = trim(fields[column.at("endtime")]);

            int studentCount = stoi(
                trim(fields[column.at("studentcount")])
            );

            sessions.emplace_back(
                subject, faculty, room, section,
                day, start, end, studentCount
            );
        } catch (const exception&) {
            cerr << "Invalid data at CSV line "
                 << lineNumber << ".\n";
            return false;
        }
    }

    return true;
}

bool runTest(const string& name, bool passed) {
    cout << (passed ? "PASS: " : "FAIL: ")
         << name << '\n';

    return passed;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0]
             << " <timetable.csv>\n";
        return 1;
    }

    vector<Session> sessions;

    if (!loadTimetable(argv[1], sessions)) {
        return 1;
    }

    cout << "CSV loaded: " << argv[1] << '\n';
    cout << "Sessions loaded: "
         << sessions.size() << "\n\n";

    vector<Clash> clashes =
        ClashDetector::detectClashes(sessions);

    int passedTests = 0;
    int totalTests = 0;

    auto check = [&](const string& name, bool passed) {
        ++totalTests;

        if (runTest(name, passed)) {
            ++passedTests;
        }
    };

    // The input file must contain timetable records.
    check(
        "CSV contains timetable sessions",
        !sessions.empty()
    );

    // Every clash must point to valid session records.
    bool validReferences = true;

    for (const Clash& clash : clashes) {
        if (clash.firstSession >= sessions.size()) {
            validReferences = false;
            break;
        }

        if (clash.type == ClashType::RoomCapacity) {
            if (clash.secondSession !=
                numeric_limits<size_t>::max()) {
                validReferences = false;
                break;
            }
        } else if (
            clash.secondSession >= sessions.size() ||
            clash.firstSession == clash.secondSession
        ) {
            validReferences = false;
            break;
        }
    }

    check(
        "All clash records reference valid CSV sessions",
        validReferences
    );

    // Capacity clashes concern one session, not a pair.
    bool validCapacityClashes = true;

    for (const Clash& clash : clashes) {
        if (clash.type == ClashType::RoomCapacity) {
            if (clash.secondSession !=
                numeric_limits<size_t>::max()) {
                validCapacityClashes = false;
                break;
            }
        }
    }

    check(
        "Room capacity clashes have the correct structure",
        validCapacityClashes
    );

    // Excluded activities must not appear in reported clashes.
    auto isExcludedSubject = [](string subject) {
        transform(
            subject.begin(), subject.end(), subject.begin(),
            [](unsigned char ch) {
                return static_cast<char>(toupper(ch));
            }
        );

        subject = trim(subject);

        return subject == "LUNCH" ||
               subject == "LIB" ||
               subject == "SWAYAM" ||
               subject.find("TRAINING") != string::npos ||
               subject.find("EXTRA-CURRICULAR ACTIVITIES")
                   != string::npos;
    };

    bool excludedActivitiesIgnored = true;

    for (const Clash& clash : clashes) {
        if (isExcludedSubject(
                sessions[clash.firstSession].getSubject())) {
            excludedActivitiesIgnored = false;
            break;
        }

        if (clash.secondSession !=
            numeric_limits<size_t>::max()) {
            if (isExcludedSubject(
                    sessions[clash.secondSession].getSubject())) {
                excludedActivitiesIgnored = false;
                break;
            }
        }
    }

    check(
        "Excluded activities do not appear in clashes",
        excludedActivitiesIgnored
    );

    // Display the actual results produced from this CSV.
    cout << "\n";
    ClashDetector::printClashes(clashes, sessions);

    cout << "\nTest summary: "
         << passedTests << "/" << totalTests
         << " passed.\n";

    cout << "Clashes found in CSV: "
         << clashes.size() << '\n';

    return passedTests == totalTests ? 0 : 1;
}
