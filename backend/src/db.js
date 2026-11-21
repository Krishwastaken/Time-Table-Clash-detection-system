const { DatabaseSync } = require('node:sqlite');
const fs = require('node:fs');
const path = require('node:path');
const config = require('./config');

fs.mkdirSync(config.DB_DIR, { recursive: true });

const db = new DatabaseSync(config.DB_PATH);

// Enable foreign keys and WAL mode
db.exec('PRAGMA foreign_keys = ON;');

function initSchema() {
  db.exec(`
    CREATE TABLE IF NOT EXISTS admin_profiles (
      admin_id TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      email TEXT UNIQUE NOT NULL,
      password_hash TEXT NOT NULL,
      salt TEXT NOT NULL,
      role TEXT DEFAULT 'Administrator',
      status TEXT DEFAULT 'ACTIVE',
      created_at TEXT DEFAULT (datetime('now')),
      last_login TEXT
    );

    CREATE TABLE IF NOT EXISTS departments (
      dept_id TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      code TEXT UNIQUE NOT NULL,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS programmes (
      prog_id TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      dept_id TEXT,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS academic_years (
      year_id TEXT PRIMARY KEY,
      year_label TEXT NOT NULL,
      is_current INTEGER DEFAULT 0,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS semesters (
      semester_id TEXT PRIMARY KEY,
      year_id TEXT,
      semester_number INTEGER NOT NULL,
      term TEXT NOT NULL,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS subjects (
      subject_id TEXT PRIMARY KEY,
      subject_code TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL,
      dept_id TEXT,
      credits INTEGER DEFAULT 3,
      is_lab INTEGER DEFAULT 0,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS sections (
      section_id TEXT PRIMARY KEY,
      name TEXT UNIQUE NOT NULL,
      semester_id TEXT,
      student_count INTEGER DEFAULT 60,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS student_groups (
      group_id TEXT PRIMARY KEY,
      section_id TEXT,
      name TEXT NOT NULL,
      student_count INTEGER DEFAULT 30
    );

    CREATE TABLE IF NOT EXISTS rooms (
      room_id TEXT PRIMARY KEY,
      name TEXT UNIQUE NOT NULL,
      room_type TEXT DEFAULT 'Classroom',
      capacity INTEGER NOT NULL DEFAULT 100,
      building TEXT,
      floor TEXT,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS students (
      student_id TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      email TEXT UNIQUE,
      section_id TEXT,
      group_name TEXT,
      status TEXT DEFAULT 'ACTIVE',
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS faculty (
      faculty_id TEXT PRIMARY KEY,
      faculty_name TEXT NOT NULL,
      department TEXT,
      designation TEXT,
      email TEXT UNIQUE,
      phone TEXT,
      office_location TEXT,
      status TEXT DEFAULT 'ACTIVE',
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS timetable_versions (
      version_id TEXT PRIMARY KEY,
      version_number TEXT NOT NULL,
      status TEXT DEFAULT 'DRAFT',
      published_by TEXT,
      published_at TEXT,
      session_count INTEGER DEFAULT 0,
      notes TEXT,
      clash_summary_json TEXT,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS timetable_sessions (
      id TEXT PRIMARY KEY,
      version_id TEXT,
      section TEXT NOT NULL,
      day TEXT NOT NULL,
      start_time TEXT NOT NULL,
      end_time TEXT NOT NULL,
      subject TEXT NOT NULL,
      faculty TEXT NOT NULL,
      room TEXT NOT NULL,
      student_count INTEGER DEFAULT 60,
      is_draft INTEGER DEFAULT 1,
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS timetable_imports (
      import_id TEXT PRIMARY KEY,
      file_name TEXT NOT NULL,
      file_type TEXT NOT NULL,
      total_rows INTEGER DEFAULT 0,
      imported_rows INTEGER DEFAULT 0,
      rejected_rows INTEGER DEFAULT 0,
      error_log TEXT,
      imported_by TEXT,
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS clash_runs (
      run_id TEXT PRIMARY KEY,
      run_at TEXT DEFAULT (datetime('now')),
      total_clashes INTEGER DEFAULT 0,
      resolved_count INTEGER DEFAULT 0,
      unresolved_count INTEGER DEFAULT 0,
      engine_output TEXT,
      summary_json TEXT
    );

    CREATE TABLE IF NOT EXISTS detected_clashes (
      clash_id TEXT PRIMARY KEY,
      run_id TEXT,
      clash_type TEXT NOT NULL,
      severity TEXT DEFAULT 'HIGH',
      first_session_id TEXT,
      second_session_id TEXT,
      description TEXT NOT NULL,
      status TEXT DEFAULT 'UNRESOLVED',
      created_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS resolution_proposals (
      proposal_id TEXT PRIMARY KEY,
      clash_id TEXT,
      action TEXT NOT NULL,
      target_session_id TEXT,
      original_data_json TEXT,
      proposed_data_json TEXT,
      explanation TEXT,
      status TEXT DEFAULT 'PENDING',
      applied_at TEXT,
      applied_by TEXT
    );

    CREATE TABLE IF NOT EXISTS resolution_actions (
      action_id TEXT PRIMARY KEY,
      proposal_id TEXT,
      action TEXT NOT NULL,
      admin_id TEXT,
      timestamp TEXT DEFAULT (datetime('now')),
      notes TEXT
    );

    CREATE TABLE IF NOT EXISTS audit_logs (
      log_id TEXT PRIMARY KEY,
      timestamp TEXT DEFAULT (datetime('now')),
      admin_id TEXT NOT NULL,
      admin_name TEXT NOT NULL,
      action TEXT NOT NULL,
      entity_type TEXT NOT NULL,
      entity_id TEXT,
      details_json TEXT,
      ip_address TEXT
    );

    CREATE TABLE IF NOT EXISTS system_settings (
      setting_key TEXT PRIMARY KEY,
      setting_value TEXT NOT NULL,
      updated_at TEXT DEFAULT (datetime('now')),
      updated_by TEXT
    );

    CREATE INDEX IF NOT EXISTS idx_sessions_day_time ON timetable_sessions (day, start_time, end_time);
    CREATE INDEX IF NOT EXISTS idx_sessions_room ON timetable_sessions (room);
    CREATE INDEX IF NOT EXISTS idx_sessions_faculty ON timetable_sessions (faculty);
    CREATE INDEX IF NOT EXISTS idx_sessions_section ON timetable_sessions (section);
    CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_logs (timestamp);
  `);
}

initSchema();

module.exports = {
  db,
  all(sql, params = []) {
    const stmt = db.prepare(sql);
    return stmt.all(...params);
  },
  get(sql, params = []) {
    const stmt = db.prepare(sql);
    return stmt.get(...params);
  },
  run(sql, params = []) {
    const stmt = db.prepare(sql);
    return stmt.run(...params);
  },
  exec(sql) {
    return db.exec(sql);
  }
};
