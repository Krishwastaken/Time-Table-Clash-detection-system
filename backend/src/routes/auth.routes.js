const { authenticateCredentials, getSession, destroySession } = require('../auth');
const { getAuthToken, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');

function handleAuthRoutes(req, res, url, body) {
  if (url.pathname === '/api/auth/login' && req.method === 'POST') {
    const { identifier, password } = body;
    const result = authenticateCredentials(identifier, password);
    if (!result.ok) {
      auditService.logAction({
        adminId: identifier || 'UNKNOWN',
        adminName: 'Anonymous',
        action: 'FAILED_LOGIN_ATTEMPT',
        entityType: 'auth',
        details: { identifier }
      });
      return sendError(res, result.message, 401);
    }

    auditService.logAction({
      adminId: result.session.adminId,
      adminName: result.session.name,
      action: 'ADMIN_LOGIN',
      entityType: 'auth',
      details: { email: result.session.email }
    });

    res.setHeader('Set-Cookie', `admin_token=${result.session.token}; Path=/; HttpOnly; SameSite=Lax; Max-Age=28800`);
    return sendJSON(res, { ok: true, session: result.session });
  }

  if (url.pathname === '/api/auth/logout' && req.method === 'POST') {
    const token = getAuthToken(req);
    const session = getSession(token);
    if (session) {
      auditService.logAction({
        adminId: session.adminId,
        adminName: session.name,
        action: 'ADMIN_LOGOUT',
        entityType: 'auth'
      });
      destroySession(token);
    }
    res.setHeader('Set-Cookie', 'admin_token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT');
    return sendJSON(res, { ok: true, message: 'Logged out successfully.' });
  }

  if (url.pathname === '/api/auth/me' && req.method === 'GET') {
    const token = getAuthToken(req);
    const session = getSession(token);
    if (!session) {
      return sendJSON(res, { ok: false, authenticated: false });
    }
    return sendJSON(res, { ok: true, authenticated: true, session });
  }

  return false;
}

module.exports = handleAuthRoutes;
