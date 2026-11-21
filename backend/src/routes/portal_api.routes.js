const db = require('../db');
const { sendJSON, sendError } = require('../middleware');

function handlePortalApiRoutes(req, res, url) {
  // 1. GET /api/portal/student/timetable
  // Returns published timetable for a given section
  if (url.pathname === '/api/portal/student/timetable' && req.method === 'GET') {
    const section = url.searchParams.get('section');
    if (!section) {
      return sendError(res, "Query parameter 'section' is required (e.g. ?section=A).");
    }

    const latestPublished = db.get("SELECT version_id, version_number, published_at FROM timetable_versions WHERE status = 'PUBLISHED' ORDER BY published_at DESC LIMIT 1");
    if (!latestPublished) {
      return sendJSON(res, {
        ok: true,
        message: 'No published timetable is currently active.',
        sessions: []
      });
    }

    const sessions = db.all(
      `SELECT id, section, day, start_time, end_time, subject, faculty, room
       FROM timetable_sessions
       WHERE version_id = ? AND section = ? AND is_draft = 0
       ORDER BY day, start_time`,
      [latestPublished.version_id, section]
    );

    return sendJSON(res, {
      ok: true,
      section,
      version: latestPublished.version_number,
      publishedAt: latestPublished.published_at,
      totalSessions: sessions.length,
      sessions
    });
  }

  // 2. GET /api/portal/faculty/schedule
  // Returns published teaching schedule for a faculty member
  if (url.pathname === '/api/portal/faculty/schedule' && req.method === 'GET') {
    const facultyId = url.searchParams.get('faculty_id');
    const facultyName = url.searchParams.get('faculty_name');

    let facRecord = null;
    if (facultyId) {
      facRecord = db.get('SELECT * FROM faculty WHERE faculty_id = ?', [facultyId]);
    } else if (facultyName) {
      facRecord = db.get('SELECT * FROM faculty WHERE faculty_name LIKE ?', [`%${facultyName}%`]);
    }

    const latestPublished = db.get("SELECT version_id, version_number, published_at FROM timetable_versions WHERE status = 'PUBLISHED' ORDER BY published_at DESC LIMIT 1");
    if (!latestPublished) {
      return sendJSON(res, {
        ok: true,
        faculty: facRecord,
        message: 'No published timetable is currently active.',
        schedule: []
      });
    }

    const searchName = facRecord ? facRecord.faculty_name : (facultyName || facultyId || '');
    const schedule = db.all(
      `SELECT id, section, day, start_time, end_time, subject, faculty, room
       FROM timetable_sessions
       WHERE version_id = ? AND is_draft = 0 AND faculty LIKE ?
       ORDER BY day, start_time`,
      [latestPublished.version_id, `%${searchName}%`]
    );

    return sendJSON(res, {
      ok: true,
      faculty: facRecord,
      version: latestPublished.version_number,
      publishedAt: latestPublished.published_at,
      totalSessions: schedule.length,
      schedule
    });
  }

  // 3. GET /api/portal/faculty/directory (Campus Connect)
  if (url.pathname === '/api/portal/faculty/directory' && req.method === 'GET') {
    const faculty = db.all(
      `SELECT faculty_id, faculty_name, department, designation, email, phone, office_location
       FROM faculty
       WHERE status = 'ACTIVE'
       ORDER BY faculty_name ASC`
    );

    return sendJSON(res, {
      ok: true,
      total: faculty.length,
      directory: faculty
    });
  }

  // 4. GET /api/docs: OpenAPI / API Documentation summary
  if (url.pathname === '/api/docs' && req.method === 'GET') {
    const docs = {
      title: 'University Timetable Management System API',
      version: '1.0.0',
      description: 'Central Administrative, Clash Engine, and Shared Portal Integration API',
      auth: {
        type: 'Bearer Token or HttpOnly Cookie (admin_token)',
        loginEndpoint: 'POST /api/auth/login'
      },
      endpoints: [
        { method: 'POST', path: '/api/auth/login', desc: 'Authenticate admin using email/ID and password' },
        { method: 'POST', path: '/api/auth/logout', desc: 'Invalidate current session' },
        { method: 'GET', path: '/api/auth/me', desc: 'Get active session profile' },
        { method: 'GET', path: '/api/stats/dashboard', desc: 'Dashboard KPIs, status, clash summary, activity' },
        { method: 'GET', path: '/api/students', desc: 'List, filter, paginate student directory' },
        { method: 'POST', path: '/api/students', desc: 'Create new student' },
        { method: 'PUT', path: '/api/students/:id', desc: 'Update student' },
        { method: 'DELETE', path: '/api/students/:id', desc: 'Deactivate or delete student' },
        { method: 'POST', path: '/api/students/import', desc: 'Validated CSV import with preview and error summary' },
        { method: 'GET', path: '/api/students/export', desc: 'Export students to CSV' },
        { method: 'GET', path: '/api/faculty', desc: 'List, filter, paginate faculty master directory' },
        { method: 'POST', path: '/api/faculty', desc: 'Add faculty member' },
        { method: 'PUT', path: '/api/faculty/:id', desc: 'Update faculty details & office location' },
        { method: 'DELETE', path: '/api/faculty/:id', desc: 'Deactivate faculty member' },
        { method: 'POST', path: '/api/faculty/import', desc: 'Import faculty master CSV' },
        { method: 'GET', path: '/api/faculty/export', desc: 'Export faculty to CSV' },
        { method: 'GET', path: '/api/academic/structure', desc: 'Get academic years, semesters, departments, rooms, sections' },
        { method: 'POST', path: '/api/academic/rooms', desc: 'Add classroom or lecture theatre' },
        { method: 'PUT', path: '/api/academic/rooms/:id', desc: 'Update room capacity' },
        { method: 'GET', path: '/api/timetable/sessions', desc: 'Get draft timetable sessions with multi-filters' },
        { method: 'POST', path: '/api/timetable/sessions', desc: 'Add new draft session' },
        { method: 'PUT', path: '/api/timetable/sessions/:id', desc: 'Edit draft session' },
        { method: 'DELETE', path: '/api/timetable/sessions/:id', desc: 'Delete draft session' },
        { method: 'POST', path: '/api/timetable/import', desc: 'Import sessions CSV with faculty mapping check' },
        { method: 'GET', path: '/api/timetable/export', desc: 'Export timetable to CSV' },
        { method: 'POST', path: '/api/clash/detect', desc: 'Execute C++ Clash Detection engine on draft timetable' },
        { method: 'GET', path: '/api/clash/last', desc: 'Get latest clash detection results' },
        { method: 'POST', path: '/api/clash/resolve', desc: 'Request proposed resolution from C++ resolver' },
        { method: 'POST', path: '/api/clash/apply', desc: 'Apply proposed resolution to draft timetable' },
        { method: 'POST', path: '/api/clash/reject', desc: 'Reject proposed resolution' },
        { method: 'GET', path: '/api/published/versions', desc: 'View published timetable version history' },
        { method: 'POST', path: '/api/published/publish', desc: 'Publish current draft as a new version' },
        { method: 'POST', path: '/api/published/restore/:id', desc: 'Rollback/restore previous published version' },
        { method: 'GET', path: '/api/activity/logs', desc: 'Query administrative audit logs' },
        { method: 'GET', path: '/api/settings', desc: 'Get system settings & admin account list' },
        { method: 'POST', path: '/api/settings', desc: 'Save system configuration' },
        { method: 'GET', path: '/api/portal/student/timetable', desc: 'Shared endpoint for external Student Portal' },
        { method: 'GET', path: '/api/portal/faculty/schedule', desc: 'Shared endpoint for external Faculty Portal' },
        { method: 'GET', path: '/api/portal/faculty/directory', desc: 'Shared endpoint for Campus Connect directory' }
      ]
    };
    return sendJSON(res, docs);
  }

  return false;
}

module.exports = handlePortalApiRoutes;
