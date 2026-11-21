const db = require('../db');
const { requireAuth, sendJSON, sendError } = require('../middleware');
const auditService = require('../services/audit.service');
const { getLatestClashRun } = require('../services/cpp_engine.service');

function handlePublishedRoutes(req, res, url, body) {
  // 1. GET /api/published/versions
  if (url.pathname === '/api/published/versions' && req.method === 'GET') {
    const versions = db.all(
      `SELECT v.*, a.name as publisher_name
       FROM timetable_versions v
       LEFT JOIN admin_profiles a ON v.published_by = a.admin_id
       ORDER BY v.created_at DESC`
    );

    const parsed = versions.map(v => {
      let clashSummary = null;
      try { clashSummary = JSON.parse(v.clash_summary_json || '{}'); } catch (e) { clashSummary = {}; }
      return {
        versionId: v.version_id,
        versionNumber: v.version_number,
        status: v.status,
        publishedBy: v.publisher_name || v.published_by || 'System',
        publishedAt: v.published_at,
        sessionCount: v.session_count,
        notes: v.notes,
        clashSummary,
        createdAt: v.created_at
      };
    });

    return sendJSON(res, { ok: true, versions: parsed });
  }

  // 2. POST /api/published/publish: Publish current draft
  if (url.pathname === '/api/published/publish' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { versionNumber, notes, forcePublish } = body;
    const draftSessions = db.all('SELECT * FROM timetable_sessions WHERE is_draft = 1');

    if (draftSessions.length === 0) {
      return sendError(res, 'No draft sessions available to publish.');
    }

    // Check clash run
    const latestClashes = getLatestClashRun();
    const unresolvedCount = latestClashes ? latestClashes.unresolvedCount : 0;

    // Check policy setting
    const policyRow = db.get("SELECT setting_value FROM system_settings WHERE setting_key = 'clash_detection_policy'");
    const isStrict = policyRow && policyRow.setting_value === 'BLOCK';

    if (isStrict && unresolvedCount > 0 && !forcePublish) {
      return sendError(res, `Cannot publish timetable: ${unresolvedCount} unresolved clashes remain and strict policy is enabled. Use forcePublish override if approved.`);
    }

    // Generate version number if not provided
    const lastVer = db.get("SELECT version_number FROM timetable_versions WHERE status = 'PUBLISHED' ORDER BY published_at DESC LIMIT 1");
    let nextNum = 'v1.0';
    if (versionNumber) {
      nextNum = versionNumber.startsWith('v') ? versionNumber : 'v' + versionNumber;
    } else if (lastVer && lastVer.version_number) {
      const match = lastVer.version_number.match(/v(\d+)\.(\d+)/);
      if (match) {
        nextNum = `v${match[1]}.${parseInt(match[2], 10) + 1}`;
      }
    }

    const versionId = 'VER-' + Date.now();

    // Archive previous published versions
    db.run("UPDATE timetable_versions SET status = 'ARCHIVED' WHERE status = 'PUBLISHED'");

    // Insert new published version
    db.run(
      `INSERT INTO timetable_versions (version_id, version_number, status, published_by, published_at, session_count, notes, clash_summary_json)
       VALUES (?, ?, 'PUBLISHED', ?, datetime('now'), ?, ?, ?)`,
      [
        versionId,
        nextNum,
        session.adminId,
        draftSessions.length,
        notes || 'Approved timetable version published by ' + session.name,
        JSON.stringify(latestClashes ? latestClashes.summaryByCategory : {})
      ]
    );

    // Copy draft sessions into published version snapshots
    const insertSession = db.db.prepare(
      `INSERT INTO timetable_sessions (id, version_id, section, day, start_time, end_time, subject, faculty, room, student_count, is_draft)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)`
    );

    for (const s of draftSessions) {
      const pubSessId = `SESS-PUB-${versionId}-${s.id}`;
      insertSession.run(
        pubSessId,
        versionId,
        s.section,
        s.day,
        s.start_time,
        s.end_time,
        s.subject,
        s.faculty,
        s.room,
        s.student_count
      );
    }

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'TIMETABLE_PUBLISHED',
      entityType: 'timetable_versions',
      entityId: versionId,
      details: {
        versionNumber: nextNum,
        sessionCount: draftSessions.length,
        unresolvedClashes: unresolvedCount,
        notes
      }
    });

    return sendJSON(res, {
      ok: true,
      message: `Timetable version ${nextNum} successfully published!`,
      versionId,
      versionNumber: nextNum
    });
  }

  // 3. POST /api/published/restore/:versionId: Restore previous version to draft
  if (url.pathname.startsWith('/api/published/restore/') && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const versionId = decodeURIComponent(url.pathname.replace('/api/published/restore/', ''));
    const version = db.get('SELECT * FROM timetable_versions WHERE version_id = ?', [versionId]);
    if (!version) {
      return sendError(res, `Version '${versionId}' not found.`, 404);
    }

    const versionSessions = db.all('SELECT * FROM timetable_sessions WHERE version_id = ? AND is_draft = 0', [versionId]);
    if (versionSessions.length === 0) {
      return sendError(res, `No session records found for version ${version.version_number}.`);
    }

    // Replace current draft with restored version sessions
    db.run('DELETE FROM timetable_sessions WHERE is_draft = 1');

    const insertDraft = db.db.prepare(
      `INSERT INTO timetable_sessions (id, version_id, section, day, start_time, end_time, subject, faculty, room, student_count, is_draft)
       VALUES (?, 'DRAFT-CURRENT', ?, ?, ?, ?, ?, ?, ?, ?, 1)`
    );

    for (const s of versionSessions) {
      const newDraftId = `SESS-RESTORED-${Date.now()}-${s.id}`;
      insertDraft.run(
        newDraftId,
        s.section,
        s.day,
        s.start_time,
        s.end_time,
        s.subject,
        s.faculty,
        s.room,
        s.student_count
      );
    }

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'TIMETABLE_VERSION_RESTORED',
      entityType: 'timetable_versions',
      entityId: versionId,
      details: { versionNumber: version.version_number, restoredCount: versionSessions.length }
    });

    return sendJSON(res, {
      ok: true,
      message: `Restored version ${version.version_number} (${versionSessions.length} sessions) into current draft.`
    });
  }

  // 4. GET /api/published/compare: Compare versions
  if (url.pathname === '/api/published/compare' && req.method === 'GET') {
    const v1 = url.searchParams.get('v1') || 'draft';
    const v2 = url.searchParams.get('v2');

    const s1 = v1 === 'draft'
      ? db.all('SELECT * FROM timetable_sessions WHERE is_draft = 1')
      : db.all('SELECT * FROM timetable_sessions WHERE version_id = ?', [v1]);

    const s2 = v2
      ? db.all('SELECT * FROM timetable_sessions WHERE version_id = ?', [v2])
      : db.all("SELECT s.* FROM timetable_sessions s JOIN timetable_versions v ON s.version_id = v.version_id WHERE v.status = 'PUBLISHED' AND s.is_draft = 0");

    return sendJSON(res, {
      ok: true,
      comparison: {
        version1Count: s1.length,
        version2Count: s2.length,
        difference: s1.length - s2.length
      }
    });
  }

  return false;
}

module.exports = handlePublishedRoutes;
