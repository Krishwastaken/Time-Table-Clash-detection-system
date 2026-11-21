const fs = require('node:fs');
const path = require('node:path');
const config = require('./config');
const { parseJSONBody, corsHeaders, sendJSON, sendError } = require('./middleware');

const handleAuthRoutes = require('./routes/auth.routes');
const handleStatsRoutes = require('./routes/stats.routes');
const handleStudentRoutes = require('./routes/students.routes');
const handleFacultyRoutes = require('./routes/faculty.routes');
const handleAcademicRoutes = require('./routes/academic.routes');
const handleTimetableRoutes = require('./routes/timetable.routes');
const handleClashRoutes = require('./routes/clash.routes');
const handlePublishedRoutes = require('./routes/published.routes');
const handleActivityRoutes = require('./routes/activity.routes');
const handleSettingsRoutes = require('./routes/settings.routes');
const handlePortalApiRoutes = require('./routes/portal_api.routes');

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon'
};

function serveStatic(req, res, pathname) {
  if (pathname === '/' || pathname === '/adminfrontend') {
    res.writeHead(302, { 'Location': '/adminfrontend/' });
    res.end();
    return true;
  }

  let relativePath = pathname;
  if (relativePath === '/adminfrontend/') {
    relativePath = '/index.html';
  } else if (relativePath.startsWith('/adminfrontend/')) {
    relativePath = relativePath.replace('/adminfrontend', '');
  }

  const filePath = path.join(config.FRONTEND_DIR, relativePath);

  // Security: prevent path traversal outside FRONTEND_DIR
  if (!filePath.startsWith(config.FRONTEND_DIR)) {
    res.writeHead(403);
    res.end('Access denied');
    return true;
  }

  if (fs.existsSync(filePath) && fs.statSync(filePath).isFile()) {
    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';
    res.writeHead(200, { 'Content-Type': contentType });
    fs.createReadStream(filePath).pipe(res);
    return true;
  }

  return false;
}

async function requestHandler(req, res) {
  corsHeaders(req, res);

  // Handle CORS preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  const parsedUrl = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = parsedUrl.pathname;

  // Static file serving for adminfrontend
  if (!pathname.startsWith('/api/')) {
    const served = serveStatic(req, res, pathname);
    if (served) return;

    // Fallback for root or SPA paths: serve index.html
    const fallbackServed = serveStatic(req, res, '/index.html');
    if (fallbackServed) return;

    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
    return;
  }

  // Parse JSON body for mutation requests
  let body = {};
  if (req.method === 'POST' || req.method === 'PUT' || req.method === 'PATCH') {
    body = await parseJSONBody(req);
  }

  try {
    // Route to handlers
    if (await handleAuthRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handleStatsRoutes(req, res, parsedUrl) || res.writableEnded) return;
    if (await handleStudentRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handleFacultyRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handleAcademicRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handleTimetableRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handleClashRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handlePublishedRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handleActivityRoutes(req, res, parsedUrl) || res.writableEnded) return;
    if (await handleSettingsRoutes(req, res, parsedUrl, body) || res.writableEnded) return;
    if (await handlePortalApiRoutes(req, res, parsedUrl) || res.writableEnded) return;

    if (res.writableEnded) return;

    // Unhandled API route
    return sendError(res, `API route '${pathname}' not found.`, 404);
  } catch (err) {
    console.error(`Internal server error at ${pathname}:`, err);
    return sendError(res, `Internal server error: ${err.message}`, 500);
  }
}

module.exports = {
  requestHandler
};
