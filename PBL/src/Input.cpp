#include "Input.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace {

std::string trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    return (start >= end) ? "" : std::string(start, end);
}

std::string normalizeCol(const std::string& str) {
    std::string res;
    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            res += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return res;
}

std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.push_back(trim(field));
            field.clear();
        } else {
            field += ch;
        }
    }
    fields.push_back(trim(field));
    return fields;
}

} // namespace

std::vector<Session> loadSessionsFromCSV(const std::string& filepath) {
    std::vector<Session> sessions;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open timetable CSV file at path: " << filepath << std::endl;
        return sessions;
    }

    std::string line;
    if (!std::getline(file, line)) {
        std::cerr << "Warning: CSV file is empty: " << filepath << std::endl;
        file.close();
        return sessions;
    }

    std::vector<std::string> rawHeaders = parseCSVLine(line);
    std::unordered_map<std::string, size_t> colMap;
    for (size_t i = 0; i < rawHeaders.size(); ++i) {
        colMap[normalizeCol(rawHeaders[i])] = i;
    }

    // Required columns
    std::string subCol = colMap.count("subject") ? "subject" : "";
    std::string facCol = colMap.count("faculty") ? "faculty" : "";
    std::string roomCol = colMap.count("room") ? "room" : "";
    std::string secCol = colMap.count("section") ? "section" : "";
    std::string dayCol = colMap.count("day") ? "day" : "";
    std::string startCol = colMap.count("starttime") ? "starttime" : "";
    std::string endCol = colMap.count("endtime") ? "endtime" : "";
    std::string countCol = colMap.count("studentcount") ? "studentcount" : (colMap.count("students") ? "students" : "");

    bool hasNamedCols = !subCol.empty() && !facCol.empty() && !secCol.empty() && !dayCol.empty();

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;

        std::vector<std::string> row = parseCSVLine(line);

        std::string subject, faculty, room, section, day, startTime, endTime;
        int studentCount = 60; // Default reasonable cohort if unspecified

        if (hasNamedCols) {
            auto getVal = [&](const std::string& key, const std::string& defVal = "") -> std::string {
                if (!key.empty() && colMap.count(key) && colMap[key] < row.size()) {
                    return row[colMap[key]];
                }
                return defVal;
            };

            subject = getVal(subCol);
            faculty = getVal(facCol);
            room = getVal(roomCol);
            section = getVal(secCol);
            day = getVal(dayCol);
            startTime = getVal(startCol);
            endTime = getVal(endCol);

            std::string countStr = getVal(countCol);
            if (!countStr.empty()) {
                try {
                    studentCount = std::stoi(countStr);
                } catch (...) {
                    studentCount = 60;
                }
            }
        } else if (row.size() >= 8) {
            // Fallback for legacy 8-column format
            // Check whether row[0] looks like subject or section
            if (row[0].size() <= 4 && (row[1] == "Monday" || row[1] == "Tuesday" || row[1] == "Wednesday" || row[1] == "Thursday" || row[1] == "Friday" || row[1] == "Saturday")) {
                // Section, Day, Start_Time, End_Time, Subject, Faculty, Room, Student_Count
                section = row[0];
                day = row[1];
                startTime = row[2];
                endTime = row[3];
                subject = row[4];
                faculty = row[5];
                room = row[6];
                try { studentCount = std::stoi(row[7]); } catch (...) { studentCount = 60; }
            } else {
                // subject, faculty, room, section, day, startTime, endTime, studentCount
                subject = row[0];
                faculty = row[1];
                room = row[2];
                section = row[3];
                day = row[4];
                startTime = row[5];
                endTime = row[6];
                try { studentCount = std::stoi(row[7]); } catch (...) { studentCount = 60; }
            }
        } else {
            continue; // Skip malformed row
        }

        sessions.emplace_back(subject, faculty, room, section, day, startTime, endTime, studentCount);
    }

    file.close();
    return sessions;
}
