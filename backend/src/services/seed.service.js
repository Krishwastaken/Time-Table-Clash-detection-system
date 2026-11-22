const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const db = require('../db');
const { parseCSV, normalizeHeader } = require('../utils/csv_parser');
const config = require('../config');
const auditService = require('./audit.service');

function seedFaculty() {
  const count = db.get('SELECT COUNT(*) as c FROM faculty');
  if (count && count.c > 0) return;

  if (!fs.existsSync(config.FACULTY_CSV)) return;

  const content = fs.readFileSync(config.FACULTY_CSV, 'utf8');
  const rows = parseCSV(content);
  if (rows.length < 2) return;

  const headers = rows[0].map(normalizeHeader);
  const idIdx = headers.indexOf('facultyid');
  const nameIdx = headers.indexOf('facultyname');
  const deptIdx = headers.indexOf('department');
  const desigIdx = headers.indexOf('designation');
  const emailIdx = headers.indexOf('email');
  const phoneIdx = headers.indexOf('phone');
  const officeIdx = headers.indexOf('officelocation');

  let inserted = 0;
  for (let i = 1; i < rows.length; i++) {
    const row = rows[i];
    const fid = row[idIdx];
    const fname = row[nameIdx];
    if (!fid || !fname) continue;

    const dept = row[deptIdx] || 'Computer Science & Engineering';
    const desig = row[desigIdx] || (fname.startsWith('Dr.') ? 'Associate Professor' : 'Assistant Professor');
    const email = row[emailIdx] || `faculty${i}@geu.ac.in`;
    const phone = row[phoneIdx] || '';
    const office = row[officeIdx] || ''; // Keep intentional blanks

    db.run(
      `INSERT OR IGNORE INTO faculty (faculty_id, faculty_name, department, designation, email, phone, office_location)
       VALUES (?, ?, ?, ?, ?, ?, ?)`,
      [fid, fname, dept, desig, email, phone, office]
    );
    inserted++;
  }
}

function seedAcademicStructure() {
  // 1. Department
  db.run("INSERT OR IGNORE INTO departments (dept_id, name, code) VALUES ('DEPT-CSE', 'Computer Science & Engineering', 'CSE')");

  // 2. Programme
  db.run("INSERT OR IGNORE INTO programmes (prog_id, name, dept_id) VALUES ('PROG-BTECH-CSE', 'B.Tech in Computer Science & Engineering', 'DEPT-CSE')");

  // 3. Academic Year
  db.run("INSERT OR IGNORE INTO academic_years (year_id, year_label, is_current) VALUES ('AY-2026-27', '2026–2027', 1)");

  // 4. Semester
  db.run("INSERT OR IGNORE INTO semesters (semester_id, year_id, semester_number, term) VALUES ('SEM-3-2026', 'AY-2026-27', 3, 'Autumn')");

  // 5. Sections
  const SECTIONS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'AI-DS1', 'AI-DS2', 'AI-ML1', 'AI-ML2', 'AI-ML3', 'AI-ML4', 'Cyber'];
  for (const s of SECTIONS) {
    db.run("INSERT OR IGNORE INTO sections (section_id, name, semester_id, student_count) VALUES (?, ?, 'SEM-3-2026', 60)", [`SEC-${s}`, s]);
    db.run("INSERT OR IGNORE INTO student_groups (group_id, section_id, name, student_count) VALUES (?, ?, 'G1', 30)", [`GRP-${s}-1`, `SEC-${s}`]);
    db.run("INSERT OR IGNORE INTO student_groups (group_id, section_id, name, student_count) VALUES (?, ?, 'G2', 30)", [`GRP-${s}-2`, `SEC-${s}`]);
  }

  // 6. Rooms with initial capacities
  const ROOMS = [
    { name: 'CR-2', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '1st Floor' },
    { name: 'CR-3', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '1st Floor' },
    { name: 'CR-4', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '1st Floor' },
    { name: 'CR-5', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '1st Floor' },
    { name: 'CR-6', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '2nd Floor' },
    { name: 'CR-7', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '2nd Floor' },
    { name: 'CR-8', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '2nd Floor' },
    { name: 'CR-12', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '3rd Floor' },
    { name: 'CR-17', type: 'Classroom', cap: 100, bldg: 'Main Block', floor: '3rd Floor' },
    { name: 'LT-1', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: 'Ground Floor' },
    { name: 'LT-2', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: 'Ground Floor' },
    { name: 'LT-5', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: '1st Floor' },
    { name: 'LT-6', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: '1st Floor' },
    { name: 'LT-7', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: '1st Floor' },
    { name: 'LT-9', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: '2nd Floor' },
    { name: 'LT-13', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: '2nd Floor' },
    { name: 'LT-14', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: '3rd Floor' },
    { name: 'LT-15', type: 'Lecture Theatre', cap: 200, bldg: 'B.Tech Block', floor: '3rd Floor' },
    { name: 'GEHU-Audi (5th Floor)', type: 'Auditorium', cap: 500, bldg: 'GEHU Tower', floor: '5th Floor' },
    { name: 'Digital Lab-1', type: 'Lab', cap: 45, bldg: 'Computing Center', floor: 'Ground Floor' },
    { name: 'Digital Lab-2', type: 'Lab', cap: 45, bldg: 'Computing Center', floor: 'Ground Floor' },
    { name: 'Lab-1', type: 'Lab', cap: 45, bldg: 'Aryabhatt Block', floor: 'Ground Floor' },
    { name: 'Lab-2', type: 'Lab', cap: 45, bldg: 'Aryabhatt Block', floor: 'Ground Floor' },
    { name: 'Lab-5', type: 'Lab', cap: 45, bldg: 'Aryabhatt Block', floor: '1st Floor' },
    { name: 'Lab-6', type: 'Lab', cap: 45, bldg: 'Aryabhatt Block', floor: '1st Floor' },
    { name: 'Lab-7', type: 'Lab', cap: 45, bldg: 'Aryabhatt Block', floor: '2nd Floor' },
    { name: 'New Lab-1', type: 'Lab', cap: 45, bldg: 'CS Block', floor: 'Ground Floor' },
    { name: 'New Lab-2', type: 'Lab', cap: 45, bldg: 'CS Block', floor: 'Ground Floor' }
  ];

  for (const r of ROOMS) {
    db.run(
      `INSERT OR IGNORE INTO rooms (room_id, name, room_type, capacity, building, floor)
       VALUES (?, ?, ?, ?, ?, ?)`,
      [`ROOM-${r.name.replace(/[^A-Za-z0-9]/g, '-')}`, r.name, r.type, r.cap, r.bldg, r.floor]
    );
  }

  // 7. Core Subjects
  const SUBJECTS = [
    { code: 'TCS-301', name: 'Data Structures with C', lab: 0, credits: 4 },
    { code: 'PCS-301', name: 'Data Structures Lab', lab: 1, credits: 2 },
    { code: 'TCS-302', name: 'Object Oriented Programming with C++', lab: 0, credits: 4 },
    { code: 'PCS-302', name: 'OOPS with C++ Lab', lab: 1, credits: 2 },
    { code: 'TCS-303', name: 'Logic Design and Computer Organization', lab: 0, credits: 4 },
    { code: 'PCS-303', name: 'Logic Design and Computer Organization Lab', lab: 1, credits: 2 },
    { code: 'TCS-304', name: 'Discrete Structures and Combinatorics', lab: 0, credits: 4 },
    { code: 'XCS-301', name: 'Career Skills-I / PDCC', lab: 0, credits: 2 },
    { code: 'ACS-301', name: 'Probability and Statistics (Audit Course)', lab: 0, credits: 2 },
    { code: 'SWY-301', name: 'Joy of Computing using Python (Through Swayam)', lab: 0, credits: 3 },
    { code: 'SWY-302', name: 'Statistical Learning for Reliability Analysis (Through Swayam)', lab: 0, credits: 3 }
  ];

  for (const s of SUBJECTS) {
    db.run(
      `INSERT OR IGNORE INTO subjects (subject_id, subject_code, name, dept_id, credits, is_lab)
       VALUES (?, ?, ?, 'DEPT-CSE', ?, ?)`,
      [`SUB-${s.code}`, s.code, s.name, s.credits, s.lab]
    );
  }
}

function seedSessions() {
  const existing = db.get('SELECT COUNT(*) as c FROM timetable_sessions WHERE is_draft = 1');
  const has699Clashes = db.get("SELECT COUNT(*) as c FROM clash_runs WHERE total_clashes = 699")?.c > 0;

  // Re-seed if no sessions or if currently holding the legacy 714 prototype draft
  if (existing && existing.c > 0 && !has699Clashes && existing.c !== 714) return;

  // Clear legacy prototype draft and old bogus 699 runs
  db.run('DELETE FROM timetable_sessions WHERE is_draft = 1');
  db.run('DELETE FROM detected_clashes');
  db.run("DELETE FROM clash_runs WHERE total_clashes = 699");

  const htmlPath = path.join(config.DATA_DIR, '3sem.html');
  const os = require('node:os');
  const { spawnSync } = require('node:child_process');

  if (fs.existsSync(htmlPath)) {
    const tmpJson = path.join(os.tmpdir(), `seed_3sem_${Date.now()}.json`);
    try {
      const res = spawnSync(config.ENGINE_BIN, ['export', htmlPath, tmpJson], { encoding: 'utf8' });
      if (res.status === 0 && fs.existsSync(tmpJson)) {
        const exported = JSON.parse(fs.readFileSync(tmpJson, 'utf8'));
        const sessions = (exported && exported.original && exported.original.sessions) || [];

        const stmt = db.db.prepare(
          `INSERT INTO timetable_sessions (id, version_id, section, day, start_time, end_time, subject, faculty, room, student_count, is_draft)
           VALUES (?, 'DRAFT-CURRENT', ?, ?, ?, ?, ?, ?, ?, ?, 1)`
        );

        let inserted = 0;
        for (const s of sessions) {
          stmt.run(
            `SESS-3SEM-${s.id}`,
            s.section,
            s.day,
            s.start,
            s.end,
            s.subject,
            s.faculty || '',
            s.room || '',
            s.students || 80
          );
          inserted++;
        }

        db.run(
          `INSERT OR REPLACE INTO timetable_versions (version_id, version_number, status, notes, session_count)
           VALUES ('DRAFT-CURRENT', 'v1.0-official-3sem', 'DRAFT', 'Official university timetable ingested from 3sem.html', ?)`,
          [inserted]
        );
        return;
      }
    } catch (err) {
      console.error('Failed to seed from 3sem.html, falling back to sessions.csv:', err);
    } finally {
      try { if (fs.existsSync(tmpJson)) fs.unlinkSync(tmpJson); } catch (e) {}
    }
  }

  if (!fs.existsSync(config.SESSIONS_CSV)) return;

  const content = fs.readFileSync(config.SESSIONS_CSV, 'utf8');
  const rows = parseCSV(content);
  if (rows.length < 2) return;

  const headers = rows[0].map(normalizeHeader);
  const secIdx = headers.indexOf('section');
  const dayIdx = headers.indexOf('day');
  const startIdx = headers.indexOf('starttime');
  const endIdx = headers.indexOf('endtime');
  const subIdx = headers.indexOf('subject');
  const facIdx = headers.indexOf('faculty');
  const roomIdx = headers.indexOf('room');
  const countIdx = headers.indexOf('studentcount');

  let inserted = 0;
  for (let i = 1; i < rows.length; i++) {
    const row = rows[i];
    const sec = row[secIdx];
    const day = row[dayIdx];
    const start = row[startIdx];
    const end = row[endIdx];
    const sub = row[subIdx];
    const fac = row[facIdx];
    const room = row[roomIdx];
    if (!sec || !day || !start || !end || !sub) continue;

    let sCount = 60;
    if (countIdx !== -1 && row[countIdx]) {
      const c = parseInt(row[countIdx], 10);
      if (!isNaN(c) && c > 0) sCount = c;
    }

    const sessId = `SESS-${i}-${sec}-${day.slice(0, 3)}-${start.replace(':', '')}`;

    db.run(
      `INSERT OR IGNORE INTO timetable_sessions (id, version_id, section, day, start_time, end_time, subject, faculty, room, student_count, is_draft)
       VALUES (?, 'DRAFT-CURRENT', ?, ?, ?, ?, ?, ?, ?, ?, 1)`,
      [sessId, sec, day, start, end, sub, fac || '', room || '', sCount]
    );
    inserted++;
  }

  // Create initial draft version record
  db.run(
    `INSERT OR IGNORE INTO timetable_versions (version_id, version_number, status, notes, session_count)
     VALUES ('DRAFT-CURRENT', 'v0.1-draft', 'DRAFT', 'Initial timetable imported from sessions.csv', ?)`,
    [inserted]
  );
}

function seedStudents() {
  const count = db.get('SELECT COUNT(*) as c FROM students');
  if (count && count.c > 0) return;

  const sections = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'AI-DS1', 'AI-DS2', 'AI-ML1', 'AI-ML2', 'AI-ML3', 'AI-ML4', 'Cyber'];
  let roll = 1000;

  for (const sec of sections) {
    for (let i = 1; i <= 5; i++) {
      roll++;
      const stuId = `GEU-2026-${roll}`;
      const name = `Student ${roll} (${sec})`;
      const email = `student${roll}@geu.ac.in`;
      const group = i <= 3 ? 'G1' : 'G2';

      db.run(
        `INSERT OR IGNORE INTO students (student_id, name, email, section_id, group_name, status)
         VALUES (?, ?, ?, ?, ?, 'ACTIVE')`,
        [stuId, name, email, sec, group]
      );
    }
  }
}

function seedSystemSettings() {
  const settings = [
    ['university_name', 'Graphic Era (Deemed to be University)'],
    ['campus', 'Main Campus, Dehradun'],
    ['academic_year', '2026–2027'],
    ['current_semester', 'Semester 3 (Autumn 2026)'],
    ['working_days', 'Monday,Tuesday,Wednesday,Thursday,Friday,Saturday'],
    ['time_slot_duration', '55 minutes'],
    ['clash_detection_policy', 'STRICT'], // STRICT: warn on clashes, BLOCK: prevent publish if HIGH clashes
    ['enable_resolver_auto_apply', 'false']
  ];

  for (const [k, v] of settings) {
    db.run(
      'INSERT OR IGNORE INTO system_settings (setting_key, setting_value, updated_by) VALUES (?, ?, ?)',
      [k, v, 'SYSTEM']
    );
  }
}

function runSeed() {
  seedFaculty();
  seedAcademicStructure();
  seedSessions();
  seedStudents();
  seedSystemSettings();
}

module.exports = {
  runSeed
};
