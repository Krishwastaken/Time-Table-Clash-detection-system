const db = require('../db');
const { sendJSON } = require('../middleware');

function handleActivityRoutes(req, res, url) {
  if (url.pathname === '/api/activity/logs' && req.method === 'GET') {
    const q = url.searchParams.get('q') || '';
    const action = url.searchParams.get('action') || '';
    const entityType = url.searchParams.get('entityType') || '';
    const page = Math.max(1, parseInt(url.searchParams.get('page') || '1', 10));
    const limit = Math.max(10, Math.min(200, parseInt(url.searchParams.get('limit') || '50', 10)));
    const offset = (page - 1) * limit;

    let where = 'WHERE 1=1';
    const params = [];

    if (q) {
      where += ' AND (action LIKE ? OR admin_name LIKE ? OR details_json LIKE ?)';
      params.push(`%${q}%`, `%${q}%`, `%${q}%`);
    }
    if (action) {
      where += ' AND action LIKE ?';
      params.push(`%${action}%`);
    }
    if (entityType) {
      where += ' AND entity_type = ?';
      params.push(entityType);
    }

    const totalRow = db.get(`SELECT COUNT(*) as c FROM audit_logs ${where}`, params);
    const total = totalRow ? totalRow.c : 0;

    const logs = db.all(
      `SELECT * FROM audit_logs ${where} ORDER BY timestamp DESC LIMIT ? OFFSET ?`,
      [...params, limit, offset]
    );

    const parsedLogs = logs.map(l => {
      let details = null;
      try { details = JSON.parse(l.details_json || '{}'); } catch (e) { details = l.details_json; }
      return {
        logId: l.log_id,
        timestamp: l.timestamp,
        adminId: l.admin_id,
        adminName: l.admin_name,
        action: l.action,
        entityType: l.entity_type,
        entityId: l.entity_id,
        details,
        ipAddress: l.ip_address
      };
    });

    return sendJSON(res, {
      ok: true,
      logs: parsedLogs,
      pagination: {
        page,
        limit,
        total,
        totalPages: Math.ceil(total / limit)
      }
    });
  }

  return false;
}

module.exports = handleActivityRoutes;
