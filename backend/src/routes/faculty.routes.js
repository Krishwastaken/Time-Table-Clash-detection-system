const db = require('../db');
const { requireAuth, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');
const { parseCSV, normalizeHeader, toCSVString } = require('../utils/csv_parser');

function handleFacultyRoutes(req, res, url, body) {
  // 1. GET /api/faculty
  if (url.pathname === '/api/faculty' && req.method === 'GET') {
    const q = url.searchParams.get('q') || '';
    const dept = url.searchParams.get('department') || '';
    const status = url.searchParams.get('status') || '';
    const page = Math.max(1, parseInt(url.searchParams.get('page') || '1', 10));
    const limit = Math.max(5, Math.min(100, parseInt(url.searchParams.get('limit') || '25', 10)));
    const offset = (page - 1) * limit;

    let where = 'WHERE 1=1';
    const params = [];

    if (q) {
      where += ' AND (faculty_id LIKE ? OR faculty_name LIKE ? OR email LIKE ?)';
      params.push(`%${q}%`, `%${q}%`, `%${q}%`);
    }
    if (dept) {
      where += ' AND department = ?';
      params.push(dept);
    }
    if (status) {
      where += ' AND status = ?';
      params.push(status);
    }

    const totalRow = db.get(`SELECT COUNT(*) as c FROM faculty ${where}`, params);
    const total = totalRow ? totalRow.c : 0;

    const faculty = db.all(
      `SELECT * FROM faculty ${where} ORDER BY faculty_id ASC LIMIT ? OFFSET ?`,
      [...params, limit, offset]
    );

    // Get list of distinct departments for filter dropdown
    const departments = db.all("SELECT DISTINCT department FROM faculty WHERE department IS NOT NULL AND department != '' ORDER BY department");

    return sendJSON(res, {
      ok: true,
      faculty,
      departments: departments.map(d => d.department),
      pagination: {
        page,
        limit,
        total,
        totalPages: Math.ceil(total / limit)
      }
    });
  }

  // 2. POST /api/faculty: Add faculty manually
  if (url.pathname === '/api/faculty' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { facultyId, facultyName, department, designation, email, phone, officeLocation } = body;
    if (!facultyId || !facultyName) {
      return sendError(res, 'Faculty ID and Faculty Name are mandatory.');
    }

    const existing = db.get('SELECT faculty_id FROM faculty WHERE faculty_id = ?', [facultyId]);
    if (existing) {
      return sendError(res, `Faculty ID '${facultyId}' already exists.`);
    }

    db.run(
      `INSERT INTO faculty (faculty_id, faculty_name, department, designation, email, phone, office_location, status)
       VALUES (?, ?, ?, ?, ?, ?, ?, 'ACTIVE')`,
      [
        facultyId.trim(),
        facultyName.trim(),
        (department || 'Computer Science & Engineering').trim(),
        (designation || 'Assistant Professor').trim(),
        (email || '').trim(),
        (phone || '').trim(),
        (officeLocation || '').trim() // intentional blank preserved
      ]
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'FACULTY_CREATED',
      entityType: 'faculty',
      entityId: facultyId,
      details: { facultyName, department }
    });

    return sendJSON(res, { ok: true, message: 'Faculty member created successfully.' });
  }

  // 3. PUT /api/faculty/:id: Edit faculty details
  if (url.pathname.startsWith('/api/faculty/') && req.method === 'PUT') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const facultyId = decodeURIComponent(url.pathname.replace('/api/faculty/', ''));
    const { facultyName, department, designation, email, phone, officeLocation, status } = body;

    const existing = db.get('SELECT * FROM faculty WHERE faculty_id = ?', [facultyId]);
    if (!existing) {
      return sendError(res, `Faculty '${facultyId}' not found.`, 404);
    }

    db.run(
      `UPDATE faculty
       SET faculty_name = ?, department = ?, designation = ?, email = ?, phone = ?, office_location = ?, status = ?, updated_at = datetime('now')
       WHERE faculty_id = ?`,
      [
        facultyName !== undefined ? facultyName.trim() : existing.faculty_name,
        department !== undefined ? department.trim() : existing.department,
        designation !== undefined ? designation.trim() : existing.designation,
        email !== undefined ? email.trim() : existing.email,
        phone !== undefined ? phone.trim() : existing.phone,
        officeLocation !== undefined ? officeLocation.trim() : existing.office_location,
        status !== undefined ? status : existing.status,
        facultyId
      ]
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'FACULTY_UPDATED',
      entityType: 'faculty',
      entityId: facultyId,
      details: { facultyName, officeLocation }
    });

    return sendJSON(res, { ok: true, message: 'Faculty member updated successfully.' });
  }

  // 4. DELETE /api/faculty/:id: Deactivate or delete
  if (url.pathname.startsWith('/api/faculty/') && req.method === 'DELETE') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const facultyId = decodeURIComponent(url.pathname.replace('/api/faculty/', ''));
    const permanent = url.searchParams.get('permanent') === 'true';

    const existing = db.get('SELECT * FROM faculty WHERE faculty_id = ?', [facultyId]);
    if (!existing) {
      return sendError(res, `Faculty '${facultyId}' not found.`, 404);
    }

    if (permanent) {
      db.run('DELETE FROM faculty WHERE faculty_id = ?', [facultyId]);
    } else {
      db.run("UPDATE faculty SET status = 'INACTIVE', updated_at = datetime('now') WHERE faculty_id = ?", [facultyId]);
    }

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: permanent ? 'FACULTY_DELETED' : 'FACULTY_DEACTIVATED',
      entityType: 'faculty',
      entityId: facultyId
    });

    return sendJSON(res, { ok: true, message: permanent ? 'Faculty deleted permanently.' : 'Faculty deactivated.' });
  }

  // 5. POST /api/faculty/import: Import faculty CSV
  if (url.pathname === '/api/faculty/import' && req.method === 'POST') {
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
    const idIdx = headers.indexOf('facultyid') !== -1 ? headers.indexOf('facultyid') : headers.indexOf('id');
    const nameIdx = headers.indexOf('facultyname') !== -1 ? headers.indexOf('facultyname') : headers.indexOf('name');
    const deptIdx = headers.indexOf('department');
    const desigIdx = headers.indexOf('designation');
    const emailIdx = headers.indexOf('email');
    const phoneIdx = headers.indexOf('phone');
    const officeIdx = headers.indexOf('officelocation') !== -1 ? headers.indexOf('officelocation') : headers.indexOf('office');

    if (idIdx === -1 || nameIdx === -1) {
      return sendError(res, 'CSV missing required columns: faculty_id and faculty_name are required.');
    }

    let newCount = 0;
    let updatedCount = 0;
    let duplicateCount = 0;
    let rejectedCount = 0;
    const errors = [];
    const previewRecords = [];

    for (let i = 1; i < rows.length; i++) {
      const r = rows[i];
      const fid = String(r[idIdx] || '').trim();
      const fname = String(r[nameIdx] || '').trim();
      const dept = deptIdx !== -1 ? String(r[deptIdx] || '').trim() : '';
      const desig = desigIdx !== -1 ? String(r[desigIdx] || '').trim() : '';
      const email = emailIdx !== -1 ? String(r[emailIdx] || '').trim() : '';
      const phone = phoneIdx !== -1 ? String(r[phoneIdx] || '').trim() : '';
      const office = officeIdx !== -1 ? String(r[officeIdx] || '').trim() : '';

      if (!fid || !fname) {
        errors.push({ row: i + 1, error: 'Missing faculty_id or faculty_name.' });
        rejectedCount++;
        continue;
      }

      const existing = db.get('SELECT * FROM faculty WHERE faculty_id = ?', [fid]);
      if (existing) {
        duplicateCount++;
      } else {
        newCount++;
      }

      if (i <= 10) {
        previewRecords.push({ facultyId: fid, facultyName: fname, department: dept, designation: desig, email, phone, officeLocation: office, isExisting: !!existing });
      }

      if (commit) {
        if (existing) {
          db.run(
            `UPDATE faculty
             SET faculty_name = ?, department = ?, designation = ?, email = ?, phone = ?, office_location = ?, updated_at = datetime('now')
             WHERE faculty_id = ?`,
            [
              fname,
              dept || existing.department,
              desig || existing.designation,
              email || existing.email,
              phone || existing.phone,
              office || existing.office_location,
              fid
            ]
          );
          updatedCount++;
        } else {
          db.run(
            `INSERT INTO faculty (faculty_id, faculty_name, department, designation, email, phone, office_location, status)
             VALUES (?, ?, ?, ?, ?, ?, ?, 'ACTIVE')`,
            [fid, fname, dept || 'Computer Science & Engineering', desig || 'Assistant Professor', email, phone, office]
          );
        }
      }
    }

    if (commit) {
      auditService.logAction({
        adminId: session.adminId,
        adminName: session.name,
        action: 'FACULTY_CSV_IMPORTED',
        entityType: 'faculty',
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

  // 6. GET /api/faculty/export: Export CSV
  if (url.pathname === '/api/faculty/export' && req.method === 'GET') {
    const faculty = db.all('SELECT faculty_id, faculty_name, department, designation, email, phone, office_location FROM faculty ORDER BY faculty_id ASC');
    const header = ['faculty_id', 'faculty_name', 'department', 'designation', 'email', 'phone', 'office_location'];
    const rows = [header];

    for (const f of faculty) {
      rows.push([f.faculty_id, f.faculty_name, f.department, f.designation, f.email, f.phone, f.office_location]);
    }

    res.setHeader('Content-Type', 'text/csv');
    res.setHeader('Content-Disposition', 'attachment; filename="faculty_master_export.csv"');
    res.writeHead(200);
    res.end(toCSVString(rows));
    return true;
  }

  return false;
}

module.exports = handleFacultyRoutes;
