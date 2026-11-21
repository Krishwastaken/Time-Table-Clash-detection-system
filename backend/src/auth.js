const crypto = require('node:crypto');
const db = require('./db');
const config = require('./config');

const sessions = new Map(); // token -> { adminId, name, email, role, mode, expiresAt }

function hashPassword(password, salt = null) {
  if (!salt) {
    salt = crypto.randomBytes(16).toString('hex');
  }
  const hash = crypto.scryptSync(password, salt, 64).toString('hex');
  return { hash, salt };
}

function verifyPassword(password, storedHash, salt) {
  const hash = crypto.scryptSync(password, salt, 64).toString('hex');
  return crypto.timingSafeEqual(Buffer.from(hash, 'hex'), Buffer.from(storedHash, 'hex'));
}

function createSession(admin) {
  const token = crypto.randomBytes(32).toString('hex');
  const session = {
    token,
    adminId: admin.admin_id,
    name: admin.name,
    email: admin.email,
    role: admin.role || 'Administrator',
    mode: 'live',
    initials: admin.name.split(' ').map(n => n[0]).join('').slice(0, 2).toUpperCase() || 'AD',
    expiresAt: Date.now() + config.SESSION_DURATION_MS
  };
  sessions.set(token, session);

  // Update last login in db
  try {
    db.run("UPDATE admin_profiles SET last_login = datetime('now') WHERE admin_id = ?", [admin.admin_id]);
  } catch (err) {
    // Ignore error
  }

  return session;
}

function getSession(token) {
  if (!token) return null;
  const s = sessions.get(token);
  if (!s) return null;
  if (Date.now() > s.expiresAt) {
    sessions.delete(token);
    return null;
  }
  return s;
}

function destroySession(token) {
  if (token) {
    sessions.delete(token);
  }
}

function authenticateCredentials(identifier, password) {
  const id = String(identifier || '').trim();
  const pw = String(password || '');

  if (!id || !pw) {
    return { ok: false, message: 'Email/Admin ID and password are required.' };
  }

  // Look up admin by email or admin_id
  const admin = db.get(
    'SELECT * FROM admin_profiles WHERE LOWER(email) = LOWER(?) OR LOWER(admin_id) = LOWER(?)',
    [id, id]
  );

  if (!admin) {
    return { ok: false, message: 'Invalid credentials. Administrator record not found.' };
  }

  if (admin.status && admin.status !== 'ACTIVE') {
    return { ok: false, message: 'This administrative account is inactive.' };
  }

  const valid = verifyPassword(pw, admin.password_hash, admin.salt);
  if (!valid) {
    return { ok: false, message: 'Invalid password. Please verify and try again.' };
  }

  const session = createSession(admin);
  return { ok: true, session };
}

module.exports = {
  hashPassword,
  verifyPassword,
  createSession,
  getSession,
  destroySession,
  authenticateCredentials
};
