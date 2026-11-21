const db = require('../db');
const { requireAuth, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');
const { hashPassword } = require('../auth');

function handleSettingsRoutes(req, res, url, body) {
  // 1. GET /api/settings
  if (url.pathname === '/api/settings' && req.method === 'GET') {
    const settingsRows = db.all('SELECT * FROM system_settings');
    const settings = {};
    for (const s of settingsRows) {
      settings[s.setting_key] = s.setting_value;
    }

    // List admins safely without password hashes
    const admins = db.all('SELECT admin_id, name, email, role, status, created_at, last_login FROM admin_profiles ORDER BY admin_id');

    return sendJSON(res, {
      ok: true,
      settings,
      admins
    });
  }

  // 2. POST /api/settings: Update configuration
  if (url.pathname === '/api/settings' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { settings } = body;
    if (!settings || typeof settings !== 'object') {
      return sendError(res, 'Settings object required.');
    }

    for (const [k, v] of Object.entries(settings)) {
      db.run(
        `INSERT INTO system_settings (setting_key, setting_value, updated_at, updated_by)
         VALUES (?, ?, datetime('now'), ?)
         ON CONFLICT(setting_key) DO UPDATE SET setting_value = excluded.setting_value, updated_at = datetime('now'), updated_by = excluded.updated_by`,
        [k, String(v), session.adminId]
      );
    }

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'SETTINGS_UPDATED',
      entityType: 'system_settings',
      details: settings
    });

    return sendJSON(res, { ok: true, message: 'Settings saved successfully.' });
  }

  // 3. POST /api/settings/admin: Create admin
  if (url.pathname === '/api/settings/admin' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { adminId, name, email, password, role } = body;
    if (!adminId || !name || !email || !password) {
      return sendError(res, 'Admin ID, Name, Email, and Password are required.');
    }

    const existing = db.get('SELECT admin_id FROM admin_profiles WHERE admin_id = ? OR LOWER(email) = LOWER(?)', [adminId, email]);
    if (existing) {
      return sendError(res, 'An administrator with this ID or email already exists.');
    }

    const { hash, salt } = hashPassword(password);
    db.run(
      `INSERT INTO admin_profiles (admin_id, name, email, password_hash, salt, role, status)
       VALUES (?, ?, ?, ?, ?, ?, 'ACTIVE')`,
      [adminId.trim(), name.trim(), email.trim().toLowerCase(), hash, salt, role || 'Administrator']
    );

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'ADMIN_ACCOUNT_CREATED',
      entityType: 'admin_profiles',
      entityId: adminId,
      details: { name, email, role }
    });

    return sendJSON(res, { ok: true, message: 'Administrator account created successfully.' });
  }

  return false;
}

module.exports = handleSettingsRoutes;
