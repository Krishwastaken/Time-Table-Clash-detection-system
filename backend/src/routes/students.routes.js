const db = require('../db');
const { requireAuth, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');
const { parseCSV, normalizeHeader, toCSVString } = require('../utils/csv_parser');

function handleStudentRoutes(req, res, url, body) {
  // 1. GET /api/students: search, filter, paginate, sort
  if (url.pathname === '/api/students' && req.method === 'GET') {
    const q = url.searchParams.get('q') || '';
    const section = url.searchParams.get('section') || '';
    const group = url.searchParams.get('group') || '';
    const status = url.searchParams.get('status') || '';
    const page = Math.max(1, parseInt(url.searchParams.get('page') || '1', 10));
    const limit = Math.max(5, Math.min(100, parseInt(url.searchParams.get('limit') || '25', 10)));
    const offset = (page - 1) * limit;

    let where = 'WHERE 1=1';
    const params = [];

    if (q) {
      where += ' AND (student_id LIKE ? OR name LIKE ? OR email LIKE ?)';
      params.push(`%${q}%`, `%${q}%`, `%${q}%`);
    }
    if (section) {
      where += ' AND section_id = ?';
      params.push(section);
    }
    if (group) {
      where += ' AND group_name = ?';
      params.push(group);
    }
    if (status) {
      where += ' AND status = ?';
      params.push(status);
    }

    const totalRow = db.get(`SELECT COUNT(*) as c FROM students ${where}`, params);
    const total = totalRow ? totalRow.c : 0;

    const students = db.all(
      `SELECT * FROM students ${where} ORDER BY student_id ASC LIMIT ? OFFSET ?`,
      [...params, limit, offset]
    );

    return sendJSON(res, {
      ok: true,
      students,
      pagination: {
        page,
        limit,
        total,
        totalPages: Math.ceil(total / limit)
      }
    });
  }

  // 2. POST /api/students: Add student
  if (url.pathname === '/api/students' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { studentId, name, email, section, group, status } = body;
    if (!studentId || !name) {
      return sendError(res, 'Student ID and name are required.');
    }

    const existing = db.get('SELECT student_id FROM students WHERE student_id = ?', [studentId]);
    if (existing) {
      return sendError(res, `Student ID '${studentId}' already exists.`);
    }

    db.run(
      `INSERT INTO students (student_id, name, email, section_id, group_name, status)
       VALUES (?, ?, ?, ?, ?, ?)`,
      [studentId.trim(), name.trim(), (email || '').trim(), (section || '').trim(), (group || '').trim(), status || 'ACTIVE']
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'STUDENT_CREATED',
      entityType: 'students',
      entityId: studentId,
      details: { name, section, group }
    });

    return sendJSON(res, { ok: true, message: 'Student created successfully.' });
  }

  // 3. PUT /api/students/:id: Edit student
  if (url.pathname.startsWith('/api/students/') && req.method === 'PUT') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const studentId = decodeURIComponent(url.pathname.replace('/api/students/', ''));
    const { name, email, section, group, status } = body;

    const existing = db.get('SELECT * FROM students WHERE student_id = ?', [studentId]);
    if (!existing) {
      return sendError(res, `Student record '${studentId}' not found.`, 404);
    }

    db.run(
      `UPDATE students
       SET name = ?, email = ?, section_id = ?, group_name = ?, status = ?, updated_at = datetime('now')
       WHERE student_id = ?`,
      [
        name ? name.trim() : existing.name,
        email !== undefined ? email.trim() : existing.email,
        section !== undefined ? section.trim() : existing.section_id,
        group !== undefined ? group.trim() : existing.group_name,
        status !== undefined ? status : existing.status,
        studentId
      ]
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'STUDENT_UPDATED',
      entityType: 'students',
      entityId: studentId,
      details: { name, section, group, status }
    });

    return sendJSON(res, { ok: true, message: 'Student updated successfully.' });
  }

  // 4. DELETE /api/students/:id: Deactivate or Delete student
  if (url.pathname.startsWith('/api/students/') && req.method === 'DELETE') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const studentId = decodeURIComponent(url.pathname.replace('/api/students/', ''));
    const permanent = url.searchParams.get('permanent') === 'true';

    const existing = db.get('SELECT * FROM students WHERE student_id = ?', [studentId]);
    if (!existing) {
      return sendError(res, `Student '${studentId}' not found.`, 404);
    }

    if (permanent) {
      db.run('DELETE FROM students WHERE student_id = ?', [studentId]);
    } else {
      db.run("UPDATE students SET status = 'INACTIVE', updated_at = datetime('now') WHERE student_id = ?", [studentId]);
    }

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: permanent ? 'STUDENT_DELETED' : 'STUDENT_DEACTIVATED',
      entityType: 'students',
      entityId: studentId
    });

    return sendJSON(res, { ok: true, message: permanent ? 'Student deleted permanently.' : 'Student deactivated.' });
  }

  // 5. POST /api/students/import: Preview / Import Student CSV
  if (url.pathname === '/api/students/import' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { csvContent, commit } = body;
    if (!csvContent) {
      return sendError(res, 'No CSV content provided.');
    }

    const rows = parseCSV(csvContent);
    if (rows.length < 2) {
      return sendError(res, 'CSV is empty or contains no records.');
    }

    const headers = rows[0].map(normalizeHeader);
    const idIdx = headers.indexOf('studentid') !== -1 ? headers.indexOf('studentid') : headers.indexOf('id');
    const nameIdx = headers.indexOf('name');
    const emailIdx = headers.indexOf('email');
    const secIdx = headers.indexOf('section') !== -1 ? headers.indexOf('section') : headers.indexOf('sectionid');
    const grpIdx = headers.indexOf('group') !== -1 ? headers.indexOf('group') : headers.indexOf('groupname');

    if (idIdx === -1 || nameIdx === -1) {
      return sendError(res, 'CSV missing required columns: Student_ID and Name are mandatory.');
    }

    let newCount = 0;
    let updatedCount = 0;
    let duplicateCount = 0;
    let rejectedCount = 0;
    const errors = [];
    const previewRecords = [];

    for (let i = 1; i < rows.length; i++) {
      const r = rows[i];
      const sid = String(r[idIdx] || '').trim();
      const sname = String(r[nameIdx] || '').trim();
      const semail = emailIdx !== -1 ? String(r[emailIdx] || '').trim() : '';
      const ssec = secIdx !== -1 ? String(r[secIdx] || '').trim() : '';
      const sgrp = grpIdx !== -1 ? String(r[grpIdx] || '').trim() : '';

      if (!sid || !sname) {
        errors.push({ row: i + 1, error: 'Missing Student ID or Name.' });
        rejectedCount++;
        continue;
      }

      const existing = db.get('SELECT * FROM students WHERE student_id = ?', [sid]);
      if (existing) {
        duplicateCount++;
      } else {
        newCount++;
      }

      if (i <= 10) {
        previewRecords.push({ studentId: sid, name: sname, email: semail, section: ssec, group: sgrp, isExisting: !!existing });
      }

      if (commit) {
        if (existing) {
          db.run(
            `UPDATE students SET name = ?, email = ?, section_id = ?, group_name = ?, updated_at = datetime('now') WHERE student_id = ?`,
            [sname, semail || existing.email, ssec || existing.section_id, sgrp || existing.group_name, sid]
          );
          updatedCount++;
        } else {
          db.run(
            `INSERT INTO students (student_id, name, email, section_id, group_name, status) VALUES (?, ?, ?, ?, ?, 'ACTIVE')`,
            [sid, sname, semail, ssec, sgrp]
          );
        }
      }
    }

    if (commit) {
      auditService.logAction({
        adminId: session.adminId,
        adminName: session.name,
        action: 'STUDENTS_CSV_IMPORTED',
        entityType: 'students',
        details: { newCount, updatedCount, duplicates: duplicateCount, rejectedCount }
      });
    }

    return sendJSON(res, {
      ok: true,
      committed: !!commit,
      summary: {
        totalRows: rows.length - 1,
        newRecords: newCount,
        updatedRecords: updatedCount,
        duplicates: duplicateCount,
        rejectedRecords: rejectedCount,
        errors
      },
      preview: previewRecords
    });
  }

  // 6. GET /api/students/export: Export CSV
  if (url.pathname === '/api/students/export' && req.method === 'GET') {
    const students = db.all('SELECT student_id, name, email, section_id, group_name, status FROM students ORDER BY student_id ASC');
    const header = ['Student_ID', 'Name', 'Email', 'Section', 'Group', 'Status'];
    const rows = [header];

    for (const s of students) {
      rows.push([s.student_id, s.name, s.email, s.section_id, s.group_name, s.status]);
    }

    res.setHeader('Content-Type', 'text/csv');
    res.setHeader('Content-Disposition', 'attachment; filename="students_export.csv"');
    res.writeHead(200);
    res.end(toCSVString(rows));
    return true;
  }

  return false;
}

module.exports = handleStudentRoutes;
