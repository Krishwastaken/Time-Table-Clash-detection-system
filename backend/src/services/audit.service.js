const crypto = require('node:crypto');
const db = require('../db');

function logAction({ adminId, adminName, action, entityType, entityId = null, details = null, ip = null }) {
  const logId = 'LOG-' + Date.now() + '-' + crypto.randomBytes(4).toString('hex');
  const detailsJson = details ? (typeof details === 'string' ? details : JSON.stringify(details)) : null;

  try {
    db.run(
      `INSERT INTO audit_logs (log_id, admin_id, admin_name, action, entity_type, entity_id, details_json, ip_address)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
      [logId, adminId || 'SYSTEM', adminName || 'System Service', action, entityType, entityId, detailsJson, ip || '127.0.0.1']
    );
  } catch (err) {
    console.error('Audit log error:', err.message);
  }
}

function getRecentLogs(limit = 50, filterAction = null) {
  if (filterAction) {
    return db.all('SELECT * FROM audit_logs WHERE action LIKE ? ORDER BY timestamp DESC LIMIT ?', [`%${filterAction}%`, limit]);
  }
  return db.all('SELECT * FROM audit_logs ORDER BY timestamp DESC LIMIT ?', [limit]);
}

module.exports = {
  logAction,
  getRecentLogs
};
