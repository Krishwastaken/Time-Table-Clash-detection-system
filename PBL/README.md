# Timetable Clash Detection & Resolution Engine (C++)

Core C++ algorithmic engine for the University Timetable Management System. Performs clash detection, conflict analysis, and automated clash resolution proposals.

---

## Features & Algorithms

1. **Clash Detection (`ClashDetector.cpp`):**
   - **Faculty Overlap:** Detects when a faculty member is assigned to simultaneous classes across different rooms or sections.
   - **Room Double-Booking:** Detects when the same room is assigned to multiple overlapping sessions.
   - **Section Overlap:** Detects when students of a single section have conflicting session slots.
   - **Room Capacity Overrun:** Validates cohort size against venue capacity limits (Auditorium: 500, Lecture Theatre: 200, Classroom: 100, Labs: 45).
   - **Duplicate Sessions:** Identifies redundant or repeated identical session bookings.
   - **Combined Lectures:** Automatically distinguishes legitimate combined lectures (multiple sections sharing an auditorium for the same subject and faculty) from conflicts.

2. **Conflict Graph Analysis (`ConflictAnalysis.cpp`):**
   - Builds an adjacency graph of conflicting sessions.
   - Computes connected conflict components via Depth-First Search (DFS) to isolate clusters of interdependent scheduling problems.

3. **Greedy Graph-Coloring Resolver (`Resolver.cpp`):**
   - Generates minimal-disruption resolution proposals for any detected clash:
     - **Room Reassignment (`CHANGE_ROOM`):** Reassigns capacity-constrained or room-colliding sessions to suitable available venues without inducing secondary clashes.
     - **Time Slot Reassignment (`CHANGE_TIME`):** Shifts overlapping sessions to available open time slots on the same day.
     - **Duplicate Removal (`DELETE_SESSION`):** Safely flags redundant duplicates for removal.
   - Performs simulated post-resolution validation to compute and verify clash reduction before changes are applied.

4. **Multi-Format CSV Ingestion (`Input.cpp`):**
   - RFC-4180 quoted CSV parsing.
   - Alphanumeric header normalization (supports both `data/timetable1.csv` and `sessions.csv` with arbitrary casing and underscores).
   - Graceful fallback for empty or unstated student counts.

---

## Command Line Interface (`bin/engine_cli`)

The engine provides a unified CLI invoked by the Node.js backend:

```sh
# 1. Detect clashes and output human-readable report
./bin/engine_cli detect <path_to_sessions.csv>

# 2. Detect clashes and output JSON (used by REST API)
./bin/engine_cli detect <path_to_sessions.csv> --json

# 3. Propose resolution for a specific clash index
./bin/engine_cli resolve <path_to_sessions.csv> <clash_index> --json

# 4. Apply resolution and export modified CSV
./bin/engine_cli apply <path_to_sessions.csv> <clash_index> <output.csv>

# 5. Output summary metrics
./bin/engine_cli summary <path_to_sessions.csv>
```

---

## Building and Running Tests

```sh
# Compile all binaries
make

# Run the complete test suite (Clash Detection, Resolver, and Sorting)
make test

# Clean build artifacts
make clean
```
