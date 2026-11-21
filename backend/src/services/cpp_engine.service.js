const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const db = require('../db');
const config = require('../config');
const auditService = require('./audit.service');
const { toCSVString } = require('../utils/csv_parser');

function ensureBinary() {
  if (!fs.existsSync(config.ENGINE_BIN)) {
    // Attempt compile if binary not built yet
    console.log('Compiling C++ clash detection and resolver engine...');
    const buildRes = spawnSync('make', ['-C', config.PBL_DIR, 'bin/engine_cli'], {
      encoding: 'utf8'
    });
    if (buildRes.status !== 0) {
      throw new Error(`Failed to compile C++ engine: ${buildRes.stderr || buildRes.stdout}`);
    }
  }
}

function dumpDraftSessionsToCSV() {
  const sessions = db.all(
    'SELECT section, day, start_time, end_time, subject, faculty, room, student_count FROM timetable_sessions WHERE is_draft = 1 ORDER BY day, start_time'
  );

  const header = ['Section', 'Day', 'Start_Time', 'End_Time', 'Subject', 'Faculty', 'Room', 'Student_Count'];
  const rows = [header];

  for (const s of sessions) {
    rows.push([
      s.section,
      s.day,
      s.start_time,
      s.end_time,
      s.subject,
      s.faculty,
      s.room,
      s.student_count || 60
    ]);
  }

  const tmpPath = path.join(os.tmpdir(), `timetable_draft_${Date.now()}_${crypto.randomBytes(4).toString('hex')}.csv`);
  fs.writeFileSync(tmpPath, toCSVString(rows), 'utf8');
  return tmpPath;
}

function runClashDetection({ adminId, adminName } = {}) {
  ensureBinary();
  const tmpCSV = dumpDraftSessionsToCSV();

  try {
    const res = spawnSync(config.ENGINE_BIN, ['detect', tmpCSV, '--json'], {
      encoding: 'utf8',
      maxBuffer: 32 * 1024 * 1024
    });

    if (res.status !== 0) {
      throw new Error(`C++ Engine execution failed: ${res.stderr || res.stdout}`);
    }

    const result = JSON.parse(res.stdout);
    const runId = 'RUN-' + Date.now();

    // Save to clash_runs
    db.run(
      `INSERT INTO clash_runs (run_id, total_clashes, resolved_count, unresolved_count, engine_output, summary_json)
       VALUES (?, ?, 0, ?, ?, ?)`,
      [runId, result.totalClashes, result.totalClashes, 'Execution successful via C++ engine_cli', JSON.stringify(result.summaryByCategory)]
    );

    // Clear old detected_clashes and insert new ones
    db.run('DELETE FROM detected_clashes');

    const insertClash = db.db.prepare(
      `INSERT INTO detected_clashes (clash_id, run_id, clash_type, severity, first_session_id, second_session_id, description, status)
       VALUES (?, ?, ?, ?, ?, ?, ?, 'UNRESOLVED')`
    );

    // Save detected clashes
    for (const c of result.clashes) {
      insertClash.run(
        c.id,
        runId,
        c.type,
        c.severity || 'HIGH',
        c.firstSession ? String(c.firstSession.index) : null,
        c.secondSession ? String(c.secondSession.index) : null,
        c.description
      );
    }

    if (adminId) {
      auditService.logAction({
        adminId,
        adminName,
        action: 'CLASH_DETECTION_RUN',
        entityType: 'clash_runs',
        entityId: runId,
        details: { totalClashes: result.totalClashes, summary: result.summaryByCategory }
      });
    }

    result.runId = runId;
    return { ok: true, result };
  } finally {
    try { fs.unlinkSync(tmpCSV); } catch (e) { /* ignore */ }
  }
}

function getLatestClashRun() {
  const latestRun = db.get('SELECT * FROM clash_runs ORDER BY run_at DESC LIMIT 1');
  if (!latestRun) {
    return null;
  }

  const clashes = db.all('SELECT * FROM detected_clashes WHERE run_id = ? ORDER BY clash_id', [latestRun.run_id]);
  let summary = {};
  try {
    summary = JSON.parse(latestRun.summary_json || '{}');
  } catch (e) {
    summary = {};
  }

  return {
    runId: latestRun.run_id,
    runAt: latestRun.run_at,
    totalClashes: latestRun.total_clashes,
    resolvedCount: latestRun.resolved_count,
    unresolvedCount: latestRun.unresolved_count,
    summaryByCategory: summary,
    clashes
  };
}

function proposeResolutionForClash(clashIndex) {
  ensureBinary();
  const tmpCSV = dumpDraftSessionsToCSV();

  try {
    const res = spawnSync(config.ENGINE_BIN, ['resolve', tmpCSV, String(clashIndex), '--json'], {
      encoding: 'utf8',
      maxBuffer: 16 * 1024 * 1024
    });

    if (res.status !== 0) {
      throw new Error(`C++ Resolver execution failed: ${res.stderr || res.stdout}`);
    }

    const proposal = JSON.parse(res.stdout);
    return { ok: true, proposal };
  } finally {
    try { fs.unlinkSync(tmpCSV); } catch (e) { /* ignore */ }
  }
}

function applyProposalToDraft(proposal, { adminId, adminName } = {}) {
  if (!proposal || !proposal.feasible) {
    return { ok: false, message: 'Cannot apply an infeasible proposal.' };
  }

  // Get current draft sessions in sorted order matching the C++ session index
  const sessions = db.all(
    'SELECT * FROM timetable_sessions WHERE is_draft = 1 ORDER BY day, start_time'
  );

  const targetIdx = proposal.targetSessionIndex;
  if (targetIdx >= sessions.length) {
    return { ok: false, message: 'Target session index not found in current draft.' };
  }

  const targetSession = sessions[targetIdx];
  const proposalId = 'PROP-' + Date.now();

  if (proposal.action === 'REMOVE_DUPLICATE') {
    db.run('DELETE FROM timetable_sessions WHERE id = ?', [targetSession.id]);
  } else {
    const p = proposal.proposed;
    db.run(
      `UPDATE timetable_sessions
       SET room = ?, day = ?, start_time = ?, end_time = ?, updated_at = datetime('now')
       WHERE id = ?`,
      [p.room, p.day, p.startTime, p.endTime, targetSession.id]
    );
  }

  // Record resolution proposal and action
  db.run(
    `INSERT INTO resolution_proposals (proposal_id, clash_id, action, target_session_id, original_data_json, proposed_data_json, explanation, status, applied_at, applied_by)
     VALUES (?, ?, ?, ?, ?, ?, ?, 'APPLIED', datetime('now'), ?)`,
    [
      proposalId,
      proposal.clashId || ('CLASH-' + (proposal.clashIndex + 1)),
      proposal.action,
      targetSession.id,
      JSON.stringify(proposal.original),
      JSON.stringify(proposal.proposed),
      proposal.explanation,
      adminId || 'ADMIN'
    ]
  );

  db.run(
    `INSERT INTO resolution_actions (action_id, proposal_id, action, admin_id, notes)
     VALUES (?, ?, 'APPLIED', ?, ?)`,
    ['ACT-' + Date.now(), proposalId, adminId || 'ADMIN', proposal.explanation]
  );

  if (adminId) {
    auditService.logAction({
      adminId,
      adminName,
      action: 'RESOLUTION_APPLIED',
      entityType: 'timetable_sessions',
      entityId: targetSession.id,
      details: {
        proposalId,
        action: proposal.action,
        original: proposal.original,
        proposed: proposal.proposed,
        explanation: proposal.explanation
      }
    });
  }

  return { ok: true, proposalId, targetSessionId: targetSession.id };
}

module.exports = {
  runClashDetection,
  getLatestClashRun,
  proposeResolutionForClash,
  applyProposalToDraft
};
