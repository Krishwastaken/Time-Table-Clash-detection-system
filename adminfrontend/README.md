# Admin Portal (Phase 2: Complete Integrated System)

Central Administrative Portal for the University Timetable Management and Clash Detection System.

Built using clean HTML5, CSS3, and modern Vanilla JavaScript without framework bloat or build steps. Designed to integrate directly with the central Node.js REST API and the existing C++ Clash Detection & Resolution Engine.

---

## What Has Been Accomplished

The portal has been upgraded from a prototype foundation into a fully operational production control centre:

1. **Live Authentication System:**
   - Swappable authentication provider (`liveApiProvider`) backing real administrative accounts provisioned from `admin_master.csv` (`GEU-F001` through `GEU-F015`).
   - Secure session management with Bearer token storage in `sessionStorage` and automatic session recovery across tab refreshes.
   - Demonstration access (`prototypeProvider`) retained for sandbox testing.

2. **Dashboard Overview:**
   - Live KPI overview cards dynamically showing real numbers from the central database (Total Students, Total Faculty, Active Sections, Scheduled Sessions, Pending Changes, Detected Clashes).
   - Real-time Timetable publishing status and C++ clash breakdown.
   - Live immutable Activity Logs audit feed and quick action dispatchers.

3. **10 Fully Functional Administrative Modules:**
   - **Dashboard:** Unified command centre.
   - **Student Management:** Full CRUD, search by student ID/name, section filters, manual entry modal, safe CSV import with preview and validation, CSV export.
   - **Faculty Management:** 75 master records (`F001` to `F075`), department filters, search, office location updates, teaching assignments overview, CSV export.
   - **Academic Structure:** Tabbed management of Departments, Programmes, Academic Years, Semesters, Sections, Student Groups, and Room capacities (Auditoriums: 500, LT: 200, CR: 100, Labs: 45).
   - **Timetable Management:** Weekly timetable grid (Monday–Saturday) and searchable tabular view; filtering by Section, Faculty, Room, and Day; draft session manual creation and editing.
   - **Clash Detection & C++ Resolver:** Direct execution of the C++ detection engine; category filtering (Faculty, Room, Section, Room Capacity, Duplicates); interactive resolution generator proposing room reassignments, time shifts, or duplicate removals with before/after comparison and instant draft application.
   - **Import / Export Hub:** Validated CSV file uploads for timetables (`sessions.csv`), student directories (`students.csv`), and faculty masters (`faculty_master.csv`), including faculty reference checks. One-click CSV exports.
   - **Published Timetables:** Version management (`v1.0.0`), changelog documentation, snapshot creation with clash check policies, rollback mechanism, and shared portal endpoint links.
   - **Activity Logs:** Searchable, filterable audit trail tracking administrative actions (imports, edits, clash runs, publishing, rollbacks).
   - **Settings:** Configurable working days, slot durations, clash publishing policies, administrator provisioning, and engine diagnostic checks.

---

## Directory Structure

```
adminfrontend/
├── index.html           # Main entry point: sign-in card and portal shell
├── css/
│   ├── theme.css        # University navy/slate design system & typography
│   ├── portal.css       # Portal layout, sidebar, topbar, cards, and drawers
│   └── modules.css      # Table layouts, modals, badges, weekly grid, and clash diffs
├── js/
│   ├── config.js        # AdminData seam connecting frontend components to API
│   ├── api.js           # REST API client for all backend endpoints
│   ├── icons.js         # Inline SVG icon library
│   ├── auth.js          # Authentication controller with live & prototype providers
│   ├── modules.js       # Module registry and navigation definitions
│   ├── pages.js         # HTML template renderers for all 10 modules
│   ├── components.js    # Interactive controllers, event listeners, and modals
│   └── app.js           # Application orchestration, hash router, and session lifecycle
├── assets/
│   └── logo.svg         # University vector crest
└── tests/
    ├── auth.test.js     # 12 unit tests for validation & session logic
    └── live_auth.test.js# Integration test for live authentication provider
```

---

## Running the Frontend

### Recommended (Full-Stack with Backend)
Start the backend server from the project root, which serves the Admin Portal and all API endpoints simultaneously:

```sh
cd backend
npm start
```
Open **http://localhost:3000** in your browser.

### Standalone Static Server
```sh
cd adminfrontend
python3 -m http.server 8000
```
Open **http://localhost:8000** in your browser. (Note: API calls will target `http://localhost:3000/api` by default).

---

## Running Frontend Tests

```sh
# Run original unit tests (12/12 passed)
node adminfrontend/tests/auth.test.js

# Run live API provider integration test
node adminfrontend/tests/live_auth.test.js
```
