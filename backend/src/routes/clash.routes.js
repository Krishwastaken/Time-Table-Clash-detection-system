const { requireAuth, sendJSON, sendError } = require('../middleware');
const { runClashDetection, getLatestClashRun, proposeResolutionForClash, applyProposalToDraft, resolveAllClashes } = require('../services/cpp_engine.service');
const auditService = require('../services/audit.service');
const db = require('../db');

function handleClashRoutes(req, res, url, body) {
  // Batch resolve all clashes
  if (url.pathname === '/api/clash/resolve-all' && req.method === 'POST') {
    const session = requireAuth(req, res);
    if (!session) return true;

    try {
      const appliedToDraft = !!(body && body.apply === true);
      const outcome = resolveAllClashes({ adminId: session.adminId, adminName: session.name, applyToDraft: appliedToDraft });
      return sendJSON(res, { ok: true, data: outcome.result, appliedToDraft });
    } catch (err) {
      console.error('Batch clash resolution failed:', err);
      return sendError(res, `C++ Batch Resolver failed: ${err.message}`, 500);
    }
  }
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

  // 6. GET /api/clash/export: Export conflict report as CSV or Markdown
  if (url.pathname === '/api/clash/export' && req.method === 'GET') {
    const format = (url.searchParams.get('format') || 'csv').toLowerCase();
    const latest = getLatestClashRun();

    if (!latest || !latest.clashes || latest.clashes.length === 0) {
      return sendError(res, 'No clash detection data available to export. Run clash detection first.', 404);
    }

    const { toCSVString } = require('../utils/csv_parser');

    if (format === 'markdown' || format === 'md') {
      let md = '# University Timetable Conflict Audit Report\n\n';
      md += `*Generated:* ${new Date().toISOString()}  \n`;
      md += `*Run ID:* \`${latest.runId}\`  \n`;
      md += `*Total Detected Clashes:* **${latest.totalClashes}**  \n\n`;

      md += '## Conflict Breakdown by Category\n\n';
      md += '| Category | Count |\n| :--- | :--- |\n';
      for (const [cat, cnt] of Object.entries(latest.summaryByCategory || {})) {
        md += `| ${cat} | ${cnt} |\n`;
      }

      md += '\n## Detected Conflict Records\n\n';
      md += '| ID | Category | Severity | Description |\n';
      md += '| :--- | :--- | :--- | :--- |\n';
      for (const c of latest.clashes) {
        md += `| ${c.clash_id} | ${c.clash_type} | ${c.severity} | ${c.description.replace(/\|/g, '\\|')} |\n`;
      }

      res.setHeader('Content-Type', 'text/markdown; charset=utf-8');
      res.setHeader('Content-Disposition', 'attachment; filename="timetable_conflicts_report.md"');
      res.writeHead(200);
      res.end(md);
      return true;
    }

    // Default: CSV export
    const rows = [
      ['Clash_ID', 'Category', 'Severity', 'Description', 'Status']
    ];

    for (const c of latest.clashes) {
      rows.push([
        c.clash_id,
        c.clash_type,
        c.severity,
        c.description,
        c.status
      ]);
    }

    res.setHeader('Content-Type', 'text/csv; charset=utf-8');
    res.setHeader('Content-Disposition', 'attachment; filename="timetable_conflicts_report.csv"');
    res.writeHead(200);
    res.end(toCSVString(rows));
    return true;
  }

  return false;
}

module.exports = handleClashRoutes;
