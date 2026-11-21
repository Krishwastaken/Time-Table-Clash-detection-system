const { getSession } = require('./auth');

function parseJSONBody(req) {
  return new Promise((resolve) => {
    let body = '';
    req.on('data', chunk => {
      body += chunk;
      if (body.length > 50 * 1024 * 1024) { // 50MB max for CSV uploads
        req.destroy();
      }
    });
    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch (e) {
        resolve({ _raw: body });
      }
    });
  });
}

function corsHeaders(req, res) {
  // If only res passed
  if (!res && req && req.setHeader) {
    res = req;
    req = null;
  }
  const origin = (req && req.headers && req.headers['origin']) || '';
  if (origin && origin !== 'null') {
    res.setHeader('Access-Control-Allow-Origin', origin);
    res.setHeader('Access-Control-Allow-Credentials', 'true');
  } else {
    res.setHeader('Access-Control-Allow-Origin', '*');
  }
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With');
}

function getAuthToken(req) {
  const authHeader = req.headers['authorization'];
  if (authHeader && authHeader.startsWith('Bearer ')) {
    return authHeader.slice(7).trim();
  }
  const cookieHeader = req.headers['cookie'];
  if (cookieHeader) {
    const cookies = Object.fromEntries(cookieHeader.split(';').map(c => {
      const [k, ...v] = c.trim().split('=');
      return [k, v.join('=')];
    }));
    if (cookies['admin_token']) return cookies['admin_token'];
  }
  return null;
}

function requireAuth(req, res) {
  const token = getAuthToken(req);
  const session = getSession(token);
  if (!session) {
    res.writeHead(401, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ ok: false, message: 'Authentication required. Please sign in.' }));
    return null;
  }
  return session;
}

function sendJSON(res, data, statusCode = 200) {
  corsHeaders(res);
  res.writeHead(statusCode, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify(data));
  return true;
}

function sendError(res, message, statusCode = 400) {
  corsHeaders(res);
  res.writeHead(statusCode, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ ok: false, message }));
  return true;
}

module.exports = {
  parseJSONBody,
  corsHeaders,
  getAuthToken,
  requireAuth,
  sendJSON,
  sendError
};
