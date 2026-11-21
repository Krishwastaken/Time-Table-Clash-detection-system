-- =====================================================================
-- University Timetable Management and Clash Detection System
-- Production Database Schema (PostgreSQL / Supabase / SQLite Compatible)
-- =====================================================================

-- 1. Administrative Accounts & Profiles
CREATE TABLE IF NOT EXISTS admin_profiles (
    admin_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    salt VARCHAR(64) NOT NULL,
    role VARCHAR(64) DEFAULT 'Administrator',
    status VARCHAR(32) DEFAULT 'ACTIVE',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP
);

-- 2. Departments
CREATE TABLE IF NOT EXISTS departments (
    dept_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    code VARCHAR(32) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 3. Academic Programmes
CREATE TABLE IF NOT EXISTS programmes (
    prog_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    dept_id VARCHAR(64) REFERENCES departments(dept_id) ON DELETE CASCADE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 4. Academic Years & Semesters
CREATE TABLE IF NOT EXISTS academic_years (
    year_id VARCHAR(64) PRIMARY KEY,
    year_label VARCHAR(64) NOT NULL,
    is_current BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS semesters (
    semester_id VARCHAR(64) PRIMARY KEY,
    year_id VARCHAR(64) REFERENCES academic_years(year_id) ON DELETE CASCADE,
    semester_number INTEGER NOT NULL,
    term VARCHAR(32) NOT NULL, -- e.g., 'Autumn', 'Spring'
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 5. Subjects / Courses
CREATE TABLE IF NOT EXISTS subjects (
    subject_id VARCHAR(64) PRIMARY KEY,
    subject_code VARCHAR(64) UNIQUE NOT NULL,
    name VARCHAR(255) NOT NULL,
    dept_id VARCHAR(64) REFERENCES departments(dept_id) ON DELETE SET NULL,
    credits INTEGER DEFAULT 3,
    is_lab BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 6. Sections and Groups
CREATE TABLE IF NOT EXISTS sections (
    section_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(64) UNIQUE NOT NULL,
    semester_id VARCHAR(64) REFERENCES semesters(semester_id) ON DELETE SET NULL,
    student_count INTEGER DEFAULT 60,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS student_groups (
    group_id VARCHAR(64) PRIMARY KEY,
    section_id VARCHAR(64) REFERENCES sections(section_id) ON DELETE CASCADE,
    name VARCHAR(64) NOT NULL,
    student_count INTEGER DEFAULT 30
);

-- 7. Rooms & Venues
CREATE TABLE IF NOT EXISTS rooms (
    room_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(128) UNIQUE NOT NULL,
    room_type VARCHAR(64) DEFAULT 'Classroom', -- 'LT', 'CR', 'Lab', 'Auditorium'
    capacity INTEGER NOT NULL DEFAULT 100,
    building VARCHAR(128),
    floor VARCHAR(32),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 8. Students
CREATE TABLE IF NOT EXISTS students (
    student_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    email VARCHAR(255) UNIQUE,
    section_id VARCHAR(64) REFERENCES sections(name) ON DELETE SET NULL,
    group_name VARCHAR(32),
    status VARCHAR(32) DEFAULT 'ACTIVE',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 9. Faculty Master Directory
CREATE TABLE IF NOT EXISTS faculty (
    faculty_id VARCHAR(64) PRIMARY KEY,
    faculty_name VARCHAR(255) NOT NULL,
    department VARCHAR(255),
    designation VARCHAR(128),
    email VARCHAR(255) UNIQUE,
    phone VARCHAR(64),
    office_location VARCHAR(255),
    status VARCHAR(32) DEFAULT 'ACTIVE',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 10. Timetable Published Versions
CREATE TABLE IF NOT EXISTS timetable_versions (
    version_id VARCHAR(64) PRIMARY KEY,
    version_number VARCHAR(32) NOT NULL,
    status VARCHAR(32) DEFAULT 'DRAFT', -- 'DRAFT', 'PUBLISHED', 'ARCHIVED'
    published_by VARCHAR(64) REFERENCES admin_profiles(admin_id),
    published_at TIMESTAMP,
    session_count INTEGER DEFAULT 0,
    notes TEXT,
    clash_summary_json TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 11. Timetable Scheduled Sessions
CREATE TABLE IF NOT EXISTS timetable_sessions (
    id VARCHAR(64) PRIMARY KEY,
    version_id VARCHAR(64) REFERENCES timetable_versions(version_id) ON DELETE CASCADE,
    section VARCHAR(64) NOT NULL,
    day VARCHAR(32) NOT NULL,
    start_time VARCHAR(16) NOT NULL,
    end_time VARCHAR(16) NOT NULL,
    subject VARCHAR(255) NOT NULL,
    faculty VARCHAR(255) NOT NULL,
    room VARCHAR(128) NOT NULL,
    student_count INTEGER DEFAULT 60,
    is_draft BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 12. Timetable Imports Audit
CREATE TABLE IF NOT EXISTS timetable_imports (
    import_id VARCHAR(64) PRIMARY KEY,
    file_name VARCHAR(255) NOT NULL,
    file_type VARCHAR(64) NOT NULL,
    total_rows INTEGER DEFAULT 0,
    imported_rows INTEGER DEFAULT 0,
    rejected_rows INTEGER DEFAULT 0,
    error_log TEXT,
    imported_by VARCHAR(64) REFERENCES admin_profiles(admin_id),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 13. Clash Detection Runs
CREATE TABLE IF NOT EXISTS clash_runs (
    run_id VARCHAR(64) PRIMARY KEY,
    run_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_clashes INTEGER DEFAULT 0,
    resolved_count INTEGER DEFAULT 0,
    unresolved_count INTEGER DEFAULT 0,
    engine_output TEXT,
    summary_json TEXT
);

-- 14. Detected Clashes
CREATE TABLE IF NOT EXISTS detected_clashes (
    clash_id VARCHAR(64) PRIMARY KEY,
    run_id VARCHAR(64) REFERENCES clash_runs(run_id) ON DELETE CASCADE,
    clash_type VARCHAR(64) NOT NULL,
    severity VARCHAR(32) DEFAULT 'HIGH',
    first_session_id VARCHAR(64),
    second_session_id VARCHAR(64),
    description TEXT NOT NULL,
    status VARCHAR(32) DEFAULT 'UNRESOLVED',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 15. Resolution Proposals
CREATE TABLE IF NOT EXISTS resolution_proposals (
    proposal_id VARCHAR(64) PRIMARY KEY,
    clash_id VARCHAR(64) REFERENCES detected_clashes(clash_id) ON DELETE CASCADE,
    action VARCHAR(64) NOT NULL,
    target_session_id VARCHAR(64),
    original_data_json TEXT,
    proposed_data_json TEXT,
    explanation TEXT,
    status VARCHAR(32) DEFAULT 'PENDING', -- 'PENDING', 'APPLIED', 'REJECTED'
    applied_at TIMESTAMP,
    applied_by VARCHAR(64) REFERENCES admin_profiles(admin_id)
);

-- 16. Resolution Audit Actions
CREATE TABLE IF NOT EXISTS resolution_actions (
    action_id VARCHAR(64) PRIMARY KEY,
    proposal_id VARCHAR(64) REFERENCES resolution_proposals(proposal_id),
    action VARCHAR(64) NOT NULL, -- 'APPLIED', 'REVERTED', 'REJECTED'
    admin_id VARCHAR(64) REFERENCES admin_profiles(admin_id),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    notes TEXT
);

-- 17. Administrative Audit Logs
CREATE TABLE IF NOT EXISTS audit_logs (
    log_id VARCHAR(64) PRIMARY KEY,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    admin_id VARCHAR(64) NOT NULL,
    admin_name VARCHAR(255) NOT NULL,
    action VARCHAR(128) NOT NULL,
    entity_type VARCHAR(64) NOT NULL,
    entity_id VARCHAR(64),
    details_json TEXT,
    ip_address VARCHAR(64)
);

-- 18. System Settings
CREATE TABLE IF NOT EXISTS system_settings (
    setting_key VARCHAR(128) PRIMARY KEY,
    setting_value TEXT NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_by VARCHAR(64)
);

-- Indexes for Fast Querying and Clash Analysis
CREATE INDEX IF NOT EXISTS idx_sessions_day_time ON timetable_sessions (day, start_time, end_time);
CREATE INDEX IF NOT EXISTS idx_sessions_room ON timetable_sessions (room);
CREATE INDEX IF NOT EXISTS idx_sessions_faculty ON timetable_sessions (faculty);
CREATE INDEX IF NOT EXISTS idx_sessions_section ON timetable_sessions (section);
CREATE INDEX IF NOT EXISTS idx_faculty_name ON faculty (faculty_name);
CREATE INDEX IF NOT EXISTS idx_students_section ON students (section_id);
CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_logs (timestamp);
