# Time Table Clash Detection System

A C++ project that loads timetable sessions from CSV, sorts them, and detects scheduling conflicts.

## Features
- CSV-based timetable input
- `Session` class for timetable entries
- Merge Sort by day and start time
- Clash detection for faculty, room, section, room capacity, and duplicate sessions
- Modular input, sorting, and clash-detection code

## DSA and OOP Concepts
- **OOP:** Classes, encapsulation, constructors, getters, and operator overloading (where implemented)
- **DSA:** Vectors, Merge Sort, sweep-line scanning, and hash maps (`unordered_map`, where implemented)

## Clash Rules
- Sessions are compared only on the same day.
- Two sessions overlap when `start1 < end2 && start2 < end1`; back-to-back sessions do not clash.
- Capacity rules: CR rooms hold up to 100 students; LT rooms hold up to 200. Labs are excluded.
- `LUNCH`, `LIB`, `SWAYAM`, `Training`, and `Extra-Curricular Activities` should be excluded from clash detection.

## CSV Format
The core loader uses seven columns:

```csv
subject,faculty,room,section,day,startTime,endTime
Data Structures,Dr. Sharma,CR 101,A,Monday,09:00,10:00
```

If testing room-capacity clashes, the CSV parser and `Session` class must also support a student-count field.

## Build and Run
Open the terminal in the repository's root directory and compile the source files:

g++ -std=c++17 src/*.cpp -o timetable.exe

Run the program:

Windows PowerShell:

.\timetable.exe

Git Bash:

./timetable.exe

Note: Run the program from the repository root so relative paths to files in the data/ directory work correctly.

## Testing
Test clean schedules, each clash type, duplicate rows, back-to-back sessions, excluded activities, and room names with different capitalization.

## Next Steps
Connect the system to the college timetable API, normalize its data, and explore clash-resolution suggestions.

## Repository
[Time-Table-Clash-detection-system](https://github.com/Krishwastaken/Time-Table-Clash-detection-system)
