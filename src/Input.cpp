#include "Input.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Helper function to remove trailing '\r' if line has Windows CRLF line endings
static std::string trimCR(const std::string& str) {
    std::string result = str;
    if (!result.empty() && result.back() == '\r') {
        result.pop_back();
    }
    return result;
}

std::vector<Session> loadSessionsFromCSV(const std::string& filepath) {
    std::vector<Session> sessions;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open timetable CSV file at path: " << filepath << std::endl;
        return sessions;
    }

    std::string line;
    // Skip the CSV header row
    if (!std::getline(file, line)) {
        std::cerr << "Warning: CSV file is empty: " << filepath << std::endl;
        file.close();
        return sessions;
    }

    int lineNumber = 1;
    while (std::getline(file, line)) {
        lineNumber++;
        line = trimCR(line);

        if (line.empty()) {
            continue; // Skip empty lines
        }

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;

        while (std::getline(ss, token, ',')) {
            row.push_back(token);
        }

        // A valid row must contain exactly 7 fields
        if (row.size() != 7) {
            std::cerr << "Warning: Skipping malformed row at line " << lineNumber 
                      << " (expected 7 fields, found " << row.size() << "): " << line << std::endl;
            continue;
        }

        Session session;
        session.subject   = row[0];
        session.faculty   = row[1];
        session.room      = row[2];
        session.section   = row[3];
        session.day       = row[4];
        session.startTime = row[5];
        session.endTime   = row[6];

        sessions.push_back(session);
    }

    file.close();
    return sessions;
}
