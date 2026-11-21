# University Timetable Management System — Backend API

Production-ready REST API server powering the central Administrative Portal and bridging the existing C++ Clash Detection & Resolution Engine with the database and frontend.

Built using native Node.js APIs (`node:http`, `node:sqlite`, `node:crypto`, `node:child_process`) for maximum portability, security, and zero external dependency footprint.

---

## Architecture Overview

```
backend/
├── src/
│   ├── server.js              # Server entry point (starts HTTP daemon & static file host)
│   ├── app.js                 # API route dispatcher & static file server
│   ├── config.js              # Environment settings & path configuration
│   ├── db.js                  # SQLite initialization with WAL mode & foreign keys
│   ├── auth.js                # Secure scrypt hashing, verification, session tokens
│   ├── middleware.js          # CORS, Bearer token auth guard, response helpers
│   ├── utils/
│   │   └── csv_parser.js      # RFC-4180 compliant CSV parser and generator
│   ├── services/
│   │   ├── audit.service.js        # Immutable audit logging
│   │   ├── provisioning.service.js # Admin account provisioning from admin_master.csv
│   │   ├── seed.service.js         # Initial faculty, academic structure, and session seeding
│   │   └── cpp_engine.service.js   # C++ engine subprocess execution & output parsing
│   └── routes/
│       ├── auth.routes.js          # /api/auth (login, me, logout)
│       ├── stats.routes.js         # /api/dashboard
│       ├── students.routes.js      # /api/students CRUD, CSV import/export
│       ├── faculty.routes.js       # /api/faculty CRUD, CSV import/export
│       ├── academic.routes.js      # /api/academic structure, rooms, sections
│       ├── timetable.routes.js     # /api/timetable sessions CRUD, CSV import/export
│       ├── clash.routes.js         # /api/clash run, latest, resolve, apply
│       ├── published.routes.js     # /api/published versions, publish, rollback
│       ├── activity.routes.js      # /api/activity logs
│       ├── settings.routes.js      # /api/settings, admin provisioning
│       └── portal_api.routes.js    # /api/portal (read-only Student/Faculty endpoints)
├── scripts/
│   └── provision_admins.js    # CLI tool to provision admins from CSV
├── tests/
│   └── api.test.js            # Comprehensive in-memory API test suite (17/17 passed)
└── package.json
```

---

## Configuration & Environment Variables

| Variable | Default Value | Description |
|---|---|---|
| `PORT` | `3000` | HTTP port for the API server |
| `DATABASE_PATH` | `database/timetable.sqlite` | Path to SQLite database file |
| `JWT_SECRET` | Auto-generated 256-bit salt | Secret key for session signature tokens |
| `CPP_ENGINE_BIN` | `PBL/bin/engine_cli` | Absolute path to compiled C++ engine executable |

---

## Authentication & Security

- **Algorithm:** PBKDF2 / `scryptSync` with 16-byte cryptographically secure random salts and constant-time `timingSafeEqual` comparison to mitigate timing attacks.
- **Tokens:** Cryptographically signed session tokens stored in in-memory session cache and verified via `Authorization: Bearer <token>` or `cookie`.
- **Role Isolation:** Enforces Administrator privileges on all mutating operations.
- **Zero Plaintext:** Neither passwords nor plaintext CSVs are ever exposed in frontend assets or ordinary database columns.

---

## REST API Specification

### 1. Authentication
- `POST /api/auth/login` — Sign in with Email/Admin ID and Password. Returns `{ ok: true, token, user }`.
- `GET /api/auth/me` — Verify current session. Returns `{ ok: true, user }`.
- `POST /api/auth/logout` — Terminate active session.

### 2. Dashboard
- `GET /api/dashboard` — Returns metrics (students, faculty, active sections, scheduled sessions, pending changes, detected clashes), timetable status, clash summaries, and recent activity feed.

### 3. Student Management
- `GET /api/students?q=&section=&status=&page=1&limit=25` — Paginated student directory with search.
- `POST /api/students` — Create a new student record manually.
- `GET /api/students/:id` — Get single student profile.
- `PUT /api/students/:id` — Update student details.
- `DELETE /api/students/:id` — Deactivate or delete student record.
- `POST /api/students/import` — Multipart CSV import with preview, validation, duplicate detection, and summary.
- `GET /api/students/export` — Download student directory as CSV.

### 4. Faculty Management
- `GET /api/faculty?q=&department=&page=1&limit=25` — Paginated faculty directory.
- `POST /api/faculty` — Create new faculty member.
- `GET /api/faculty/:id` — Get single faculty profile and assigned teaching sections.
- `PUT /api/faculty/:id` — Update faculty details (office location, email, phone, designation).
- `DELETE /api/faculty/:id` — Deactivate or remove faculty member.
- `POST /api/faculty/import` — CSV import with duplicate ID protection.
- `GET /api/faculty/export` — Download faculty directory as CSV.

### 5. Academic Structure
- `GET /api/academic/structure` — Complete hierarchy (departments, programmes, academic years, semesters, sections, student groups, rooms with capacities).
- `POST /api/academic/rooms` — Add or update room capacity.
- `POST /api/academic/sections` — Add or update academic section.

### 6. Timetable Management
- `GET /api/timetable/sessions?section=&faculty=&room=&day=` — Query draft timetable sessions.
- `POST /api/timetable/sessions` — Add manual timetable session.
- `PUT /api/timetable/sessions/:id` — Edit session details.
- `DELETE /api/timetable/sessions/:id` — Remove session from draft.
- `POST /api/timetable/import` — Upload and parse `sessions.csv`. Validates faculty references against Faculty Master records.
- `GET /api/timetable/export` — Export draft timetable as standard CSV.

### 7. Clash Detection & Resolver (C++ Engine)
- `POST /api/clash/run` — Executes C++ binary `./PBL/bin/engine_cli detect <csv> --json`, parses conflict graph, records severity, affected entities, and conflict components.
- `GET /api/clash/latest` — Fetches most recent clash run report with full summary.
- `POST /api/clash/resolve` — Invokes C++ greedy graph-coloring resolver for a specific clash index (`./PBL/bin/engine_cli resolve <csv> <idx> --json`), returning before/after comparison and proposed change.
- `POST /api/clash/apply` — Applies proposed change (room reassignment, time change, or duplicate removal) to the draft database and marks proposal applied.

### 8. Publishing & Version Control
- `GET /api/published/versions` — Version history of all published timetables.
- `POST /api/published/publish` — Snapshots current draft into an immutable published version (e.g. `v1.0.0`), checking clash policy.
- `POST /api/published/rollback` — Restores a previous published version into the active draft timetable.

### 9. Activity Audit Logs
- `GET /api/activity/logs?module=&adminId=&page=1&limit=50` — Paginated immutable audit trail with timestamp, admin identity, action, entity, and diff details.

### 10. Settings & Provisioning
- `GET /api/settings` — Returns current university configuration and scheduling policies.
- `PUT /api/settings` — Updates working days, slot durations, and publish clash policies.
- `GET /api/settings/admins` — Lists provisioned administrators.
- `POST /api/settings/admins` — Manually provision an administrator.

### 11. Shared Portal APIs (External Consuming Endpoints)
Read-only endpoints intended for the independently developed Student Portal and Faculty Portal:
- `GET /api/portal/timetables/student/:section` — Returns published weekly timetable sessions for a student's section.
- `GET /api/portal/timetables/faculty/:facultyId` — Returns published schedule for a faculty member.
- `GET /api/portal/faculty-directory` — Campus Connect directory with faculty names, departments, designations, and office locations.

---

## Running Backend Commands

```sh
# Run backend API test suite (17 integration tests)
npm test

# Provision administrator accounts from data/admin_master.csv
npm run provision

# Recompile C++ engine binaries
npm run build:cpp

# Start production server (serves API and adminfrontend)
npm start
```
