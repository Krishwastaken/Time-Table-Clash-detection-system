const db = require('../db');
const { requireAuth, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');
const { parseCSV, normalizeHeader, toCSVString } = require('../utils/csv_parser');

function handleTimetableRoutes(req, res, url, body) {
  // 1. GET /api/timetable/sessions
  if (url.pathname === '/api/timetable/sessions' && req.method === 'GET') {
    const section = url.searchParams.get('section') || '';
    const faculty = url.searchParams.get('faculty') || '';
    const room = url.searchParams.get('room') || '';
    const day = url.searchParams.get('day') || '';
    const subject = url.searchParams.get('subject') || '';
    const q = url.searchParams.get('q') || '';

    let where = 'WHERE is_draft = 1';
    const params = [];

    if (section) {
      where += ' AND section = ?';
      params.push(section);
    }
    if (faculty) {
      where += ' AND faculty LIKE ?';
      params.push(`%${faculty}%`);
    }
    if (room) {
      where += ' AND room = ?';
      params.push(room);
    }
    if (day) {
      where += ' AND day = ?';
      params.push(day);
    }
    if (subject) {
      where += ' AND subject LIKE ?';
      params.push(`%${subject}%`);
    }
    if (q) {
      where += ' AND (subject LIKE ? OR faculty LIKE ? OR room LIKE ? OR section LIKE ?)';
      params.push(`%${q}%`, `%${q}%`, `%${q}%`, `%${q}%`);
    }

    const sessions = db.all(
      `SELECT * FROM timetable_sessions ${where} ORDER BY day, start_time, section ASC`,
      params
    );

    // Get filter dropdown options
    const sections = db.all('SELECT DISTINCT section FROM timetable_sessions WHERE is_draft = 1 ORDER BY section');
    const days = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
    const rooms = db.all('SELECT DISTINCT room FROM timetable_sessions WHERE is_draft = 1 ORDER BY room');

    return sendJSON(res, {
      ok: true,
      total: sessions.length,
      sessions,
      filters: {
        sections: sections.map(s => s.section),
        days,
        rooms: rooms.map(r => r.room)
      }
    });
  }

  // 2. POST /api/timetable/sessions: Add session
  if (url.pathname === '/api/timetable/sessions' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { section, day, startTime, endTime, subject, faculty, room, studentCount } = body;
    if (!section || !day || !startTime || !endTime || !subject) {
      return sendError(res, 'Section, Day, Start Time, End Time, and Subject are required.');
    }

    const sessId = `SESS-CUSTOM-${Date.now()}`;
    db.run(
      `INSERT INTO timetable_sessions (id, version_id, section, day, start_time, end_time, subject, faculty, room, student_count, is_draft)
       VALUES (?, 'DRAFT-CURRENT', ?, ?, ?, ?, ?, ?, ?, ?, 1)`,
      [sessId, section.trim(), day.trim(), startTime.trim(), endTime.trim(), subject.trim(), (faculty || 'UNKNOWN').trim(), (room || 'UNKNOWN').trim(), studentCount || 60]
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'TIMETABLE_SESSION_ADDED',
      entityType: 'timetable_sessions',
      entityId: sessId,
      details: { section, day, startTime, endTime, subject, room }
    });

    return sendJSON(res, { ok: true, message: 'Session added to draft timetable.', sessionId: sessId });
  }

  // 3. PUT /api/timetable/sessions/:id: Edit session
  if (url.pathname.startsWith('/api/timetable/sessions/') && req.method === 'PUT') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const sessId = decodeURIComponent(url.pathname.replace('/api/timetable/sessions/', ''));
    const { section, day, startTime, endTime, subject, faculty, room, studentCount } = body;

    const existing = db.get('SELECT * FROM timetable_sessions WHERE id = ?', [sessId]);
    if (!existing) {
      return sendError(res, `Session '${sessId}' not found.`, 404);
    }

    db.run(
      `UPDATE timetable_sessions
       SET section = ?, day = ?, start_time = ?, end_time = ?, subject = ?, faculty = ?, room = ?, student_count = ?, updated_at = datetime('now')
       WHERE id = ?`,
      [
        section ? section.trim() : existing.section,
        day ? day.trim() : existing.day,
        startTime ? startTime.trim() : existing.start_time,
        endTime ? endTime.trim() : existing.end_time,
        subject ? subject.trim() : existing.subject,
        faculty !== undefined ? faculty.trim() : existing.faculty,
        room !== undefined ? room.trim() : existing.room,
        studentCount !== undefined ? studentCount : existing.student_count,
        sessId
      ]
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'TIMETABLE_SESSION_EDITED',
      entityType: 'timetable_sessions',
      entityId: sessId,
      details: {
        old: { room: existing.room, time: `${existing.day} ${existing.start_time}-${existing.end_time}` },
        updated: { room, time: `${day} ${startTime}-${endTime}` }
      }
    });

    return sendJSON(res, { ok: true, message: 'Session updated in draft timetable.' });
  }

  // 4. DELETE /api/timetable/sessions/:id: Delete session
  if (url.pathname.startsWith('/api/timetable/sessions/') && req.method === 'DELETE') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const sessId = decodeURIComponent(url.pathname.replace('/api/timetable/sessions/', ''));
    const existing = db.get('SELECT * FROM timetable_sessions WHERE id = ?', [sessId]);
    if (!existing) {
      return sendError(res, `Session '${sessId}' not found.`, 404);
    }

    db.run('DELETE FROM timetable_sessions WHERE id = ?', [sessId]);

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'TIMETABLE_SESSION_DELETED',
      entityType: 'timetable_sessions',
      entityId: sessId,
      details: { subject: existing.subject, section: existing.section, time: `${existing.day} ${existing.start_time}` }
    });

    return sendJSON(res, { ok: true, message: 'Session deleted from draft.' });
  }

  // 5. POST /api/timetable/import: Import sessions CSV with preview and faculty mapping verification
  if (url.pathname === '/api/timetable/import' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { csvContent, commit, replaceDraft } = body;
    if (!csvContent) {
      return sendError(res, 'No CSV content provided.');
    }

    const rows = parseCSV(csvContent);
    if (rows.length < 2) {
      return sendError(res, 'CSV is empty or contains no records.');
    }

    const headers = rows[0].map(normalizeHeader);
    const secIdx = headers.indexOf('section');
    const dayIdx = headers.indexOf('day');
    const startIdx = headers.indexOf('starttime');
    const endIdx = headers.indexOf('endtime');
    const subIdx = headers.indexOf('subject');
    const facIdx = headers.indexOf('faculty');
    const roomIdx = headers.indexOf('room');
    const countIdx = headers.indexOf('studentcount') !== -1 ? headers.indexOf('studentcount') : headers.indexOf('students');

    if (secIdx === -1 || dayIdx === -1 || startIdx === -1 || endIdx === -1 || subIdx === -1) {
      return sendError(res, 'CSV missing required headers: Section, Day, Start_Time, End_Time, and Subject are required.');
    }

    // Load existing faculty master list for mapping validation
    const facultyMaster = db.all('SELECT faculty_id, faculty_name FROM faculty');
    const knownFacultyNames = new Set(facultyMaster.map(f => f.faculty_name.toLowerCase().trim()));

    let validCount = 0;
    let malformedCount = 0;
    const errors = [];
    const previewRecords = [];
    const unmappedFaculty = new Set();
    const mappedFaculty = new Set();

    if (commit && replaceDraft) {
      db.run('DELETE FROM timetable_sessions WHERE is_draft = 1');
    }

    for (let i = 1; i < rows.length; i++) {
      const r = rows[i];
      const sec = String(r[secIdx] || '').trim();
      const day = String(r[dayIdx] || '').trim();
      const start = String(r[startIdx] || '').trim();
      const end = String(r[endIdx] || '').trim();
      const sub = String(r[subIdx] || '').trim();
      const fac = facIdx !== -1 ? String(r[facIdx] || '').trim() : 'UNKNOWN';
      const room = roomIdx !== -1 ? String(r[roomIdx] || '').trim() : 'UNKNOWN';
      let count = 60;
      if (countIdx !== -1 && r[countIdx]) {
        const parsedCount = parseInt(r[countIdx], 10);
        if (!isNaN(parsedCount) && parsedCount > 0) count = parsedCount;
      }

      if (!sec || !day || !start || !end || !sub) {
        errors.push({ row: i + 1, error: 'Missing mandatory session fields.' });
        malformedCount++;
        continue;
      }

      // Check faculty mapping
      const professors = fac.split(/&|\/|,/).map(p => p.trim()).filter(p => p.length > 0 && p !== 'UNKNOWN');
      let isMapped = false;
      for (const p of professors) {
        const pLower = p.toLowerCase();
        let matched = false;
        for (const k of knownFacultyNames) {
          if (k.includes(pLower) || pLower.includes(k)) {
            matched = true;
            break;
          }
        }
        if (matched) {
          mappedFaculty.add(p);
          isMapped = true;
        } else {
          unmappedFaculty.add(p);
        }
      }

      validCount++;

      if (i <= 15) {
        previewRecords.push({
          row: i + 1,
          section: sec,
          day,
          startTime: start,
          endTime: end,
          subject: sub,
          faculty: fac,
          room,
          studentCount: count,
          facultyMapped: isMapped
        });
      }

      if (commit) {
        const sessId = `SESS-IMP-${Date.now()}-${i}`;
        db.run(
          `INSERT INTO timetable_sessions (id, version_id, section, day, start_time, end_time, subject, faculty, room, student_count, is_draft)
           VALUES (?, 'DRAFT-CURRENT', ?, ?, ?, ?, ?, ?, ?, ?, 1)`,
          [sessId, sec, day, start, end, sub, fac, room, count]
        );
      }
    }

    if (commit) {
      auditService.logAction({
        adminId: session.adminId,
        adminName: session.name,
        action: 'TIMETABLE_CSV_IMPORTED',
        entityType: 'timetable_sessions',
        details: { validCount, malformedCount, unmappedFacultyCount: unmappedFaculty.size }
      });
    }

    return sendJSON(res, {
      ok: true,
      committed: !!commit,
      summary: {
        totalRows: rows.length - 1,
        validSessions: validCount,
        malformedSessions: malformedCount,
        unmappedFacultyList: Array.from(unmappedFaculty).slice(0, 20),
        mappedFacultyCount: mappedFaculty.size,
        errors
      },
      preview: previewRecords
    });
  }

  // 6. GET /api/timetable/export: Export current draft
  if (url.pathname === '/api/timetable/export' && req.method === 'GET') {
    const sessions = db.all(
      'SELECT section, day, start_time, end_time, subject, faculty, room, student_count FROM timetable_sessions WHERE is_draft = 1 ORDER BY day, start_time, section'
    );

    const header = ['Section', 'Day', 'Start_Time', 'End_Time', 'Subject', 'Faculty', 'Room', 'Student_Count'];
    const rows = [header];

    for (const s of sessions) {
      rows.push([s.section, s.day, s.start_time, s.end_time, s.subject, s.faculty, s.room, s.student_count]);
    }

    res.setHeader('Content-Type', 'text/csv');
    res.setHeader('Content-Disposition', 'attachment; filename="timetable_draft_export.csv"');
    res.writeHead(200);
    res.end(toCSVString(rows));
    return true;
  }

  return false;
}

module.exports = handleTimetableRoutes;
