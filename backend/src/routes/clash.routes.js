const { requireAuth, sendJSON, sendError } = require('../middleware');
const { runClashDetection, getLatestClashRun, proposeResolutionForClash, applyProposalToDraft } = require('../services/cpp_engine.service');
const auditService = require('../services/audit.service');
const db = require('../db');

function handleClashRoutes(req, res, url, body) {
  // 1. POST /api/clash/detect: Execute real C++ clash detection engine
  if (url.pathname === '/api/clash/detect' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    try {
      const outcome = runClashDetection({ adminId: session.adminId, adminName: session.name });
      return sendJSON(res, { ok: true, report: outcome.result });
    } catch (err) {
      console.error('Clash detection failed:', err);
      return sendError(res, `C++ Clash Detection Engine failed: ${err.message}`, 500);
    }
  }

  // 2. GET /api/clash/last: Retrieve latest clash detection summary & clashes
  if (url.pathname === '/api/clash/last' && req.method === 'GET') {
    const latest = getLatestClashRun();
    return sendJSON(res, { ok: true, data: latest });
  }

  // 3. POST /api/clash/resolve: Call C++ resolver for an individual clash
  if (url.pathname === '/api/clash/resolve' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { clashIndex } = body;
    if (clashIndex === undefined || clashIndex === null) {
      return sendError(res, 'clashIndex is required to propose a resolution.');
    }

    try {
      const outcome = proposeResolutionForClash(clashIndex);
      return sendJSON(res, { ok: true, proposal: outcome.proposal });
    } catch (err) {
      console.error('Clash resolution failed:', err);
      return sendError(res, `C++ Resolver failed: ${err.message}`, 500);
    }
  }

  // 4. POST /api/clash/apply: Apply resolver proposal to draft
  if (url.pathname === '/api/clash/apply' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { proposal } = body;
    if (!proposal) {
      return sendError(res, 'Proposal payload required.');
    }

    const outcome = applyProposalToDraft(proposal, { adminId: session.adminId, adminName: session.name });
    if (!outcome.ok) {
      return sendError(res, outcome.message || 'Failed to apply resolution.');
    }

    // Re-run clash detection automatically after applying resolution to verify
    try {
      const postDetection = runClashDetection({ adminId: session.adminId, adminName: session.name });
      return sendJSON(res, {
        ok: true,
        message: 'Resolution applied to draft timetable. Clash detection re-evaluated.',
        proposalId: outcome.proposalId,
        newReport: postDetection.result
      });
    } catch (e) {
      return sendJSON(res, {
        ok: true,
        message: 'Resolution applied to draft timetable.',
        proposalId: outcome.proposalId
      });
    }
  }

  // 5. POST /api/clash/reject: Reject proposed resolution
  if (url.pathname === '/api/clash/reject' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    const { proposalId, reason } = body;
    if (proposalId) {
      db.run("UPDATE resolution_proposals SET status = 'REJECTED' WHERE proposal_id = ?", [proposalId]);
      db.run(
        "INSERT INTO resolution_actions (action_id, proposal_id, action, admin_id, notes) VALUES (?, ?, 'REJECTED', ?, ?)",
        ['ACT-' + Date.now(), proposalId, session.adminId, reason || 'Rejected by Administrator']
      );
    }

    auditService.logAction({
      adminId: session.adminId,
      adminName: session.name,
      action: 'RESOLUTION_REJECTED',
      entityType: 'resolution_proposals',
      entityId: proposalId,
      details: { reason }
    });

    return sendJSON(res, { ok: true, message: 'Resolution proposal rejected.' });
  }

  return false;
}

module.exports = handleClashRoutes;
