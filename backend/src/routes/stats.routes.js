const db = require('../db');
const { sendJSON } = require('../middleware');
const { getLatestClashRun } = require('../services/cpp_engine.service');

function handleStatsRoutes(req, res, url) {
  if (url.pathname === '/api/stats/dashboard' && req.method === 'GET') {
    const studentCount = db.get('SELECT COUNT(*) as c FROM students')?.c || 0;
    const facultyCount = db.get('SELECT COUNT(*) as c FROM faculty')?.c || 0;
    const sectionCount = db.get('SELECT COUNT(DISTINCT section) as c FROM timetable_sessions WHERE is_draft = 1')?.c || 0;
    const sessionCount = db.get('SELECT COUNT(*) as c FROM timetable_sessions WHERE is_draft = 1')?.c || 0;

    const publishedVersion = db.get("SELECT * FROM timetable_versions WHERE status = 'PUBLISHED' ORDER BY published_at DESC LIMIT 1");
    const latestClash = getLatestClashRun();

    const pendingCount = db.get('SELECT COUNT(*) as c FROM timetable_sessions WHERE is_draft = 1')?.c || 0;
    const lastSessionUpdate = db.get('SELECT updated_at FROM timetable_sessions ORDER BY updated_at DESC LIMIT 1')?.updated_at;

    const recentLogs = db.all('SELECT * FROM audit_logs ORDER BY timestamp DESC LIMIT 6');

    const activityFeed = recentLogs.map(l => {
      let icon = 'pencil';
      if (l.action.includes('LOGIN')) icon = 'users';
      else if (l.action.includes('CLASH')) icon = 'alert';
      else if (l.action.includes('PUBLISH')) icon = 'published';
      else if (l.action.includes('IMPORT')) icon = 'upload';
      else if (l.action.includes('RESOLUTION')) icon = 'sliders';

      return {
        icon,
        text: `${l.action.replace(/_/g, ' ')} by ${l.admin_name}`,
        time: l.timestamp,
        meta: l.entity_type
      };
    });

    const categories = {
      'Faculty': latestClash?.summaryByCategory?.['Faculty'] ?? null,
      'Room': latestClash?.summaryByCategory?.['Room'] ?? null,
      'Section': latestClash?.summaryByCategory?.['Section'] ?? null,
      'Room capacity': latestClash?.summaryByCategory?.['Room capacity'] ?? null,
      'Duplicate session': latestClash?.summaryByCategory?.['Duplicate session'] ?? null
    };

    // Calculate timetable conflict density & peak time slot distributions
    let conflictDensity = 0;
    if (sessionCount > 0 && latestClash && latestClash.totalClashes > 0) {
      conflictDensity = Math.min(100, Math.round((latestClash.totalClashes / sessionCount) * 100 * 10) / 10);
    }

    const dayDistribution = {
      Monday: 0,
      Tuesday: 0,
      Wednesday: 0,
      Thursday: 0,
      Friday: 0,
      Saturday: 0
    };

    const draftSessions = db.all('SELECT day, start_time FROM timetable_sessions WHERE is_draft = 1');
    for (const s of draftSessions) {
      if (dayDistribution[s.day] !== undefined) {
        dayDistribution[s.day]++;
      }
    }

    return sendJSON(res, {
      ok: true,
      metrics: {
        students: studentCount,
        faculty: facultyCount,
        sections: sectionCount,
        sessions: sessionCount,
        pending: publishedVersion ? (sessionCount !== publishedVersion.session_count ? 'Yes' : 'None') : sessionCount,
        clashes: latestClash ? latestClash.totalClashes : null,
        conflictDensity
      },
      timetableStatus: {
        state: publishedVersion ? 'Published (with draft modifications)' : 'Draft (Unpublished)',
        lastPublished: publishedVersion ? publishedVersion.version_number : null,
        lastUpdated: lastSessionUpdate || new Date().toISOString(),
        pending: sessionCount + ' sessions in draft'
      },
      clashSummary: {
        total: latestClash ? latestClash.totalClashes : null,
        resolved: latestClash ? latestClash.resolvedCount : null,
        unresolved: latestClash ? latestClash.unresolvedCount : null,
        conflictDensity,
        byCategory: categories,
        dayDistribution
      },
      activityFeed
    });
  }

  return false;
}

module.exports = handleStatsRoutes;
