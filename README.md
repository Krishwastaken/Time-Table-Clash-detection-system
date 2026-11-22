# Time Table Clash Detection System

A C++17 command-line project that reads a college timetable (from the college website's HTML page or a CSV file), detects every scheduling conflict, explains them, and automatically generates a **clash-free timetable** by re-assigning rooms. Built with the standard library only.

## Features
- **Timetable fetching:** download the college timetable page and parse every section's grid into sessions (merged cells, lab groups, shared lectures, faculty and room extraction)
- **CSV input** with validation (bad rows are rejected with their line number, good rows still load)
- **Stable merge sort** by weekday, then start time
- **Clash detection:** faculty, room, section, room capacity and duplicate sessions
- **Week scenarios:** "*" classes (Saturday off) and Saturday classes (Saturday working) are only compared in weeks where both run
- **Room pools:** one listed room that is really several identical labs
- **Conflict analysis:** shared lectures grouped into single incidents; conflict graph with connected groups (DFS)
- **Resolver:** removes exact duplicates and moves sessions to free, suitable rooms; re-runs the detector to verify the result
- **Reports:** on-screen summary / details / filter by type, TXT and CSV exports
- **JSON export** of original data, clashes, incidents and resolved timetable for the frontend

## Project Structure
| File | Purpose |
|---|---|
| `main.cpp` | Menu UI and `--export` command-line mode |
| `TimetableFetcher.h/.cpp` | Download and parse the college timetable page |
| `Input.h/.cpp` | CSV loader and validator |
| `Session.h/.cpp` | Session class (encapsulated data, room capacity, weekday-aware ordering) |
| `TimeUtils.h` | Shared helpers: name/room normalisation, time parsing, week rules |
| `sorting.h/.cpp` | Merge sort (day + start time), insertion sort by faculty/room |
| `ClashDetector.h/.cpp` | Clash detection (grouping + sweep line) |
| `ConflictAnalysis.h/.cpp` | Incident grouping and conflict-graph components (DFS) |
| `Resolver.h/.cpp` | Room re-assignment and clash-free timetable export |
| `ReportFormatter.h/.cpp` | Summary, detailed and exported reports |
| `JsonExport.h/.cpp` | JSON output for the frontend |

## DSA Concepts
- **Stable merge sort** (divide and conquer) insertion sort for key-based sorting
- **Grouping with `map` / `unordered_map`** so only related sessions are compared
- **Graph model:** sessions are vertices, clashes are edges; adjacency sets and **iterative DFS** for connected groups
- **Greedy graph colouring** in the Resolver, with rooms as colours
- **Inverted index** (`room -> sessions`) and **best-fit** room selection
- Hand-written parsers: CSV state machine and HTML table scanner

## C++ Concepts
- Classes, encapsulation, constructors with initialiser lists, function overloading 
- `enum class`, function overloading, default arguments
- Templates and lambdas, STL containers and algorithms, `std::move`
- Namespaces and anonymous namespaces, const-correctness, references
- Exceptions, file I/O with RAII, `std::regex`, stream manipulators, command-line arguments

## Clash Rules
- Sessions are compared only on the same day, and only when they can run in the same week type.
- Two sessions overlap when `start1 < end2 && start2 < end1`; back-to-back sessions do not clash.
- **Faculty:** one person in two overlapping sessions (names are split and honorifics removed; "President" classes can be ignored).
- **Room:** a room in two overlapping sessions. Sections attending the same lecture (same subject, room, time) are not a clash; labs are never shared.
- **Section:** a section in two overlapping sessions (different lab groups such as G1/G2 may run in parallel).
- **Capacity:** student count above the room capacity (CR 80, LT 160, Digital Lab-1/2 80, Lab-1/7 40, Lab-2/5/6 80, GEHU auditorium 500; unknown rooms are not checked).
- **Duplicate:** the same session appears more than once.
- `LUNCH`, `LIB`, `SWAYAM`, audit courses, training and extra-curricular activities are excluded.

## Resolver
Class times are fixed, so the resolver only changes **rooms** and removes **exact duplicates**:
1. Remove duplicate rows.
2. Build the room-conflict graph.
3. Greedy colouring: electives move first, then labs, then core subjects; most-conflicted first; the smallest suitable free room is chosen.
4. Re-run the clash detector on the result.

Faculty and section clashes need a time change, so they are listed as **unresolved** with a reason.

## Build and Run

```
g++ -std=c++17 -O2 -Wall -Wextra -Isrc src/*.cpp -o timetable.exe
```

Windows PowerShell: `.\timetable.exe`  |  Git Bash: `./timetable.exe`

Run from the repository root so relative paths such as `data/3sem.html` work.

### Menu
```
1  Load a dataset                     8  Consolidated conflicts
2  Run clash detection                9  Conflict graph (DFS)
3  View summary                       10 Auto-resolve (preview, then apply)
4  View detailed clashes              11 Generate clash-free timetable (4 files)
5  Filter clashes by type             12 Export everything to JSON
6  Export TXT and CSV reports         13 Download a timetable page from a URL
7  Display sorted timetable           0  Exit
```

### Command-line export (for the frontend)
```
timetable.exe --export data/3sem.html timetable_report.json
```

## Input Formats
- **College page:** `data/3sem.html`, `data/5sem.html`. If a page cannot be read, a CSV with the same name is used as a fallback.
- **CSV:** header row, 8 columns plus an optional `weekRule` column (`ALWAYS`, `SAT_OFF`, `SAT_WORK`):

## Output Files
| File | Contents |
|---|---|
| `clash_report.txt`, `clash_report.csv` | Detected clashes (option 6) |
| `original_timetable.csv` | Timetable as loaded (option 11) |
| `clash_free_timetable.csv` | Timetable after room changes (options 10 and 11) |
| `change_log.csv` | One row per applied fix |
| `unresolved_clashes.csv` | Clashes that need a time change, with the reason |
| `timetable_report.json` | Everything above for the frontend (option 12) |

## Testing
Test clean schedules, each clash type, duplicate rows, back-to-back sessions, excluded activities, room names in different capitalisation, shared lectures, Saturday-week scenarios, and malformed CSV rows. After resolving, check that `clashesAfter` only contains clashes that need a time change.

## Limitations 
- Only rooms can change; a time-shift repair level would let teacher and section clashes be resolved.
- The fetcher depends on the college page layout.
- Room capacities come from a built-in table by room-name prefix.

## Repository
[Time-Table-Clash-detection-system](https://github.com/Krishwastaken/Time-Table-Clash-detection-system)
