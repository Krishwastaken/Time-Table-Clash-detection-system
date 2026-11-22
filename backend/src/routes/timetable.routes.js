const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const { spawnSync } = require('node:child_process');
const db = require('../db');
const config = require('../config');
const { requireAuth, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');
const { parseCSV, normalizeHeader, toCSVString } = require('../utils/csv_parser');
const { exportResolvedTimetableCSV } = require('../services/cpp_engine.service');

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

  // 5. POST /api/timetable/import: Import sessions CSV or HTML timetable with preview and faculty mapping verification
  if (url.pathname === '/api/timetable/import' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const rawContent = (body.csvContent || body.content || body.htmlContent || '').trim();
    if (!rawContent) {
      return sendError(res, 'No CSV or HTML content provided.');
    }

    const isHTML = /<html|<table|<!doctype/i.test(rawContent.slice(0, 1000));
    const { commit, replaceDraft, includeFreePeriods } = body;

    // Load existing faculty master list for mapping validation
    const facultyMaster = db.all('SELECT faculty_id, faculty_name FROM faculty');
    const knownFacultyNames = new Set(facultyMaster.map(f => f.faculty_name.toLowerCase().trim()));

    let parsedSessions = [];
    let errors = [];

    if (isHTML) {
      // Process HTML timetable via engine_cli export
      const tmpHtml = path.join(os.tmpdir(), `timetable_upload_${Date.now()}_${crypto.randomBytes(4).toString('hex')}.html`);
      const tmpJson = path.join(os.tmpdir(), `timetable_export_${Date.now()}_${crypto.randomBytes(4).toString('hex')}.json`);
      try {
        fs.writeFileSync(tmpHtml, rawContent, 'utf8');
        const resExp = spawnSync(config.ENGINE_BIN, ['export', tmpHtml, tmpJson], {
          encoding: 'utf8',
          maxBuffer: 32 * 1024 * 1024
        });
        if (resExp.status !== 0 || !fs.existsSync(tmpJson)) {
          return sendError(res, `Failed to parse HTML timetable: ${resExp.stderr || resExp.stdout}`);
        }
        const exportedData = JSON.parse(fs.readFileSync(tmpJson, 'utf8'));
        const rawSessions = (exportedData && exportedData.original && exportedData.original.sessions) || [];
        for (const s of rawSessions) {
          if (!includeFreePeriods && s.freePeriod) {
            continue; // skip lunch/library/swayam by default
          }
          parsedSessions.push({
            section: s.section || 'UNKNOWN',
            day: s.day || '',
            startTime: s.start || '',
            endTime: s.end || '',
            subject: s.subject || '',
            faculty: s.faculty || 'UNKNOWN',
            room: s.room || 'UNKNOWN',
            studentCount: s.students || 60
          });
        }
      } catch (err) {
        return sendError(res, `HTML parsing failed: ${err.message}`, 500);
      } finally {
        try { if (fs.existsSync(tmpHtml)) fs.unlinkSync(tmpHtml); } catch (e) {}
        try { if (fs.existsSync(tmpJson)) fs.unlinkSync(tmpJson); } catch (e) {}
      }
    } else {
      // Process CSV timetable
      const rows = parseCSV(rawContent);
      if (rows.length < 2) {
        return sendError(res, 'CSV is empty or contains no records.');
      }
      const headers = rows[0].map(normalizeHeader);
      const secIdx = headers.indexOf('section');
      const dayIdx = headers.indexOf('day');
      const startIdx = headers.indexOf('start_time') !== -1 ? headers.indexOf('start_time') : headers.indexOf('starttime');
      const endIdx = headers.indexOf('end_time') !== -1 ? headers.indexOf('end_time') : headers.indexOf('endtime');
      const subIdx = headers.indexOf('subject');
      const facIdx = headers.indexOf('faculty');
      const roomIdx = headers.indexOf('room');
      const countIdx = headers.indexOf('student_count') !== -1 ? headers.indexOf('student_count') : (headers.indexOf('studentcount') !== -1 ? headers.indexOf('studentcount') : headers.indexOf('students'));

      if (secIdx === -1 || dayIdx === -1 || startIdx === -1 || endIdx === -1 || subIdx === -1) {
        return sendError(res, 'CSV missing required headers: Section, Day, Start_Time, End_Time, and Subject are required.');
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
          continue;
        }

        parsedSessions.push({
          section: sec,
          day,
          startTime: start,
          endTime: end,
          subject: sub,
          faculty: fac,
          room,
          studentCount: count
        });
      }
    }

    let validCount = 0;
    const previewRecords = [];
    const unmappedFaculty = new Set();
    const mappedFaculty = new Set();

    if (commit && replaceDraft) {
      db.run('DELETE FROM timetable_sessions WHERE is_draft = 1');
    }

    const insertStmt = commit ? db.db.prepare(
      `INSERT INTO timetable_sessions (id, version_id, section, day, start_time, end_time, subject, faculty, room, student_count, is_draft)
       VALUES (?, 'DRAFT-CURRENT', ?, ?, ?, ?, ?, ?, ?, ?, 1)`
    ) : null;

    for (let i = 0; i < parsedSessions.length; i++) {
      const s = parsedSessions[i];
      const professors = s.faculty.split(/&|\/|,/).map(p => p.trim()).filter(p => p.length > 0 && p !== 'UNKNOWN');
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

      if (i < 15) {
        previewRecords.push({
          row: i + 1,
          section: s.section,
          day: s.day,
          startTime: s.startTime,
          endTime: s.endTime,
          subject: s.subject,
          faculty: s.faculty,
          room: s.room,
          studentCount: s.studentCount,
          facultyMapped: isMapped
        });
      }

      if (commit && insertStmt) {
        const sessId = `SESS-IMP-${Date.now()}-${i + 1}`;
        insertStmt.run(sessId, s.section, s.day, s.startTime, s.endTime, s.subject, s.faculty, s.room, s.studentCount);
      }
    }

    if (commit) {
      auditService.logAction({
        adminId: session.adminId,
        adminName: session.name,
        action: isHTML ? 'TIMETABLE_HTML_IMPORTED' : 'TIMETABLE_CSV_IMPORTED',
        entityType: 'timetable_sessions',
        details: { validCount, format: isHTML ? 'HTML' : 'CSV', unmappedFacultyCount: unmappedFaculty.size }
      });
    }

    return sendJSON(res, {
      ok: true,
      committed: !!commit,
      format: isHTML ? 'HTML' : 'CSV',
      summary: {
        totalRows: parsedSessions.length + errors.length,
        validSessions: validCount,
        malformedSessions: errors.length,
        unmappedFacultyList: Array.from(unmappedFaculty).slice(0, 20),
        mappedFacultyCount: mappedFaculty.size,
        errors
      },
      preview: previewRecords
    });
  }

  // 6. GET /api/timetable/export: Export current draft or direct resolved timetable
  if (url.pathname === '/api/timetable/export' && req.method === 'GET') {
    const isResolved = url.searchParams.get('resolved') === '1' || url.searchParams.get('resolved') === 'true';

    if (isResolved) {
      const csvData = exportResolvedTimetableCSV();
      res.setHeader('Content-Type', 'text/csv');
      res.setHeader('Content-Disposition', 'attachment; filename="resolved_timetable.csv"');
      res.writeHead(200);
      res.end(csvData);
      return true;
    }

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
