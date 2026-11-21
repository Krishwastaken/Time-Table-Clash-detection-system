const db = require('../db');
const { requireAuth, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');

function handleAcademicRoutes(req, res, url, body) {
  // 1. GET /api/academic/structure
  if (url.pathname === '/api/academic/structure' && req.method === 'GET') {
    const years = db.all('SELECT * FROM academic_years ORDER BY year_label DESC');
    const semesters = db.all('SELECT * FROM semesters ORDER BY semester_number ASC');
    const departments = db.all('SELECT * FROM departments ORDER BY name ASC');
    const programmes = db.all('SELECT * FROM programmes ORDER BY name ASC');
    const subjects = db.all('SELECT * FROM subjects ORDER BY subject_code ASC');
    const sections = db.all('SELECT * FROM sections ORDER BY name ASC');
    const rooms = db.all('SELECT * FROM rooms ORDER BY name ASC');

    return sendJSON(res, {
      ok: true,
      structure: {
        academicYears: years,
        semesters,
        departments,
        programmes,
        subjects,
        sections,
        rooms
      }
    });
  }

  // 2. POST /api/academic/rooms: Add Room
  if (url.pathname === '/api/academic/rooms' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { name, roomType, capacity, building, floor } = body;
    if (!name || !capacity) {
      return sendError(res, 'Room name and capacity are required.');
    }

    const roomId = 'ROOM-' + name.replace(/[^A-Za-z0-9]/g, '-');
    const existing = db.get('SELECT room_id FROM rooms WHERE name = ?', [name]);
    if (existing) {
      return sendError(res, `Room '${name}' already exists.`);
    }

    db.run(
      `INSERT INTO rooms (room_id, name, room_type, capacity, building, floor)
       VALUES (?, ?, ?, ?, ?, ?)`,
      [roomId, name.trim(), roomType || 'Classroom', parseInt(capacity, 10), (building || '').trim(), (floor || '').trim()]
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'ROOM_CREATED',
      entityType: 'rooms',
      entityId: roomId,
      details: { name, capacity }
    });

    return sendJSON(res, { ok: true, message: 'Room created successfully.' });
  }

  // 3. PUT /api/academic/rooms/:id: Update Room Capacity
  if (url.pathname.startsWith('/api/academic/rooms/') && req.method === 'PUT') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const roomId = decodeURIComponent(url.pathname.replace('/api/academic/rooms/', ''));
    const { name, roomType, capacity, building, floor } = body;

    const existing = db.get('SELECT * FROM rooms WHERE room_id = ? OR name = ?', [roomId, roomId]);
    if (!existing) {
      return sendError(res, `Room '${roomId}' not found.`, 404);
    }

    db.run(
      `UPDATE rooms
       SET name = ?, room_type = ?, capacity = ?, building = ?, floor = ?
       WHERE room_id = ?`,
      [
        name ? name.trim() : existing.name,
        roomType || existing.room_type,
        capacity !== undefined ? parseInt(capacity, 10) : existing.capacity,
        building !== undefined ? building.trim() : existing.building,
        floor !== undefined ? floor.trim() : existing.floor,
        existing.room_id
      ]
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'ROOM_UPDATED',
      entityType: 'rooms',
      entityId: existing.room_id,
      details: { name: existing.name, oldCapacity: existing.capacity, newCapacity: capacity }
    });

    return sendJSON(res, { ok: true, message: 'Room updated successfully.' });
  }

  // 4. DELETE /api/academic/rooms/:id: Delete Room
  if (url.pathname.startsWith('/api/academic/rooms/') && req.method === 'DELETE') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const roomId = decodeURIComponent(url.pathname.replace('/api/academic/rooms/', ''));
    db.run('DELETE FROM rooms WHERE room_id = ? OR name = ?', [roomId, roomId]);

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'ROOM_DELETED',
      entityType: 'rooms',
      entityId: roomId
    });

    return sendJSON(res, { ok: true, message: 'Room removed.' });
  }

  // 5. POST /api/academic/subjects: Add Subject
  if (url.pathname === '/api/academic/subjects' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { code, name, deptId, credits, isLab } = body;
    if (!code || !name) {
      return sendError(res, 'Subject code and name are required.');
    }

    const subId = 'SUB-' + code.replace(/[^A-Za-z0-9]/g, '-');
    db.run(
      `INSERT INTO subjects (subject_id, subject_code, name, dept_id, credits, is_lab)
       VALUES (?, ?, ?, ?, ?, ?)`,
      [subId, code.trim(), name.trim(), deptId || 'DEPT-CSE', credits || 3, isLab ? 1 : 0]
    );

    return sendJSON(res, { ok: true, message: 'Subject added successfully.' });
  }

  // 6. POST /api/academic/sections: Add Section
  if (url.pathname === '/api/academic/sections' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { name, studentCount } = body;
    if (!name) return sendError(res, 'Section name is required.');

    const secId = 'SEC-' + name.trim();
    db.run(
      `INSERT INTO sections (section_id, name, semester_id, student_count)
       VALUES (?, ?, 'SEM-3-2026', ?)`,
      [secId, name.trim(), studentCount || 60]
    );

    return sendJSON(res, { ok: true, message: 'Section added successfully.' });
  }

  return false;
}

module.exports = handleAcademicRoutes;
