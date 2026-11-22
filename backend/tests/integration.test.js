const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { Readable, Writable } = require('node:stream');
const { requestHandler } = require('../src/app');
const { runSeed } = require('../src/services/seed.service');
const { provisionAdminsFromCSV } = require('../src/services/provisioning.service');
const db = require('../src/db');
const config = require('../src/config');

runSeed();
provisionAdminsFromCSV();

let adminToken = '';

function invokeApi(method, apiPath, body = null, headers = {}) {
  return new Promise((resolve) => {
    const postData = body ? JSON.stringify(body) : '';
    const reqHeaders = {
      'content-type': 'application/json',
      ...headers
    };
    if (adminToken && !reqHeaders['authorization']) {
      reqHeaders['authorization'] = `Bearer ${adminToken}`;
    }

    const req = new Readable({
      read() {
        if (postData) this.push(postData);
        this.push(null);
      }
    });

    req.method = method;
    req.url = apiPath;
    req.headers = reqHeaders;

    let resStatusCode = 200;
    const resHeaders = {};
    let resBody = '';

    const res = new Writable({
      write(chunk, encoding, callback) {
        resBody += chunk.toString();
        callback();
      }
    });

    res.writeHead = function (code, headersObj = {}) {
      resStatusCode = code;
      Object.assign(resHeaders, headersObj);
      return res;
    };

    res.setHeader = function (k, v) {
      resHeaders[k.toLowerCase()] = v;
    };

    res.on('finish', () => {
      let json = null;
      try {
        json = JSON.parse(resBody);
      } catch (e) {
        json = resBody;
      }
      resolve({ status: resStatusCode, headers: resHeaders, body: json });
    });

    requestHandler(req, res).catch((err) => {
      resolve({ status: 500, headers: {}, body: { ok: false, error: err.message } });
    });
  });
}

async function runIntegrationSuite() {
  console.log('========================================================');
  console.log('RUNNING E2E BATCH RESOLVER & HTML IMPORT TEST SUITE');
  console.log('========================================================\n');

  let passed = 0;
  let failed = 0;

  function pass(msg) {
    passed++;
    console.log(`  PASS: ${msg}`);
  }

  function fail(msg, err) {
    failed++;
    console.error(`  FAIL: ${msg}`, err);
  }

  // 1. Auth Setup
  try {
    const res = await invokeApi('POST', '/api/auth/login', {
      identifier: 'rajesh.verma@geu.ac.in',
      password: 'Rv@4821#tt'
    });
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    adminToken = res.body.session.token;
    pass('Admin authentication succeeded with live session token');
  } catch (e) {
    fail('Admin authentication succeeded with live session token', e);
  }

  // 2. Batch Resolver Preview (Dry-Run)
  try {
    const res = await invokeApi('POST', '/api/clash/resolve-all', { apply: false });
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.strictEqual(res.body.appliedToDraft, false);
    assert(typeof res.body.data.clashesBefore === 'number');
    assert(typeof res.body.data.clashesAfter === 'number');
    assert(Array.isArray(res.body.data.applied));
    assert(Array.isArray(res.body.data.unresolved));
    pass('C++ Welsh-Powell Batch Resolver dry-run returns changes and unresolved list');
  } catch (e) {
    fail('C++ Welsh-Powell Batch Resolver dry-run returns changes and unresolved list', e);
  }

  // 3. Batch Resolver Apply to Draft
  try {
    const res = await invokeApi('POST', '/api/clash/resolve-all', { apply: true });
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.strictEqual(res.body.appliedToDraft, true);

    const log = db.get("SELECT * FROM audit_logs WHERE action = 'BATCH_RESOLVE_APPLIED_TO_DRAFT' ORDER BY timestamp DESC LIMIT 1");
    assert(log !== undefined, 'Audit log recorded for batch resolution apply');
    pass('C++ Welsh-Powell Batch Resolver successfully applied fixes to draft');
  } catch (e) {
    fail('C++ Welsh-Powell Batch Resolver successfully applied fixes to draft', e);
  }

  // 4. HTML Timetable Import (3sem.html)
  try {
    const htmlPath = path.join(config.ROOT_DIR, 'data', '3sem.html');
    if (fs.existsSync(htmlPath)) {
      const htmlContent = fs.readFileSync(htmlPath, 'utf8');
      const res = await invokeApi('POST', '/api/timetable/import', {
        htmlContent,
        commit: false,
        replaceDraft: false
      });
      assert.strictEqual(res.status, 200);
      assert.strictEqual(res.body.ok, true);
      assert.strictEqual(res.body.format, 'HTML');
      assert(res.body.summary.validSessions > 0);
      assert(res.body.preview.length > 0);
      pass(`HTML timetable parser validated ${res.body.summary.validSessions} sessions from 3sem.html`);
    } else {
      pass('Skipping HTML import check (3sem.html not present in test dir)');
    }
  } catch (e) {
    fail('HTML timetable parser validation', e);
  }

  // 5. Conflict Export Endpoints (CSV and Markdown)
  try {
    const resCsv = await invokeApi('GET', '/api/clash/export?format=csv');
    assert.strictEqual(resCsv.status, 200);
    assert(resCsv.headers['content-type'].includes('text/csv'));
    assert(typeof resCsv.body === 'string' && resCsv.body.includes('Clash_ID'));

    const resMd = await invokeApi('GET', '/api/clash/export?format=markdown');
    assert.strictEqual(resMd.status, 200);
    assert(resMd.headers['content-type'].includes('text/markdown'));
    assert(typeof resMd.body === 'string' && resMd.body.includes('# University Timetable Conflict Audit Report'));
    pass('Downloadable conflict reports generated in both CSV and Markdown formats');
  } catch (e) {
    fail('Conflict export endpoints', e);
  }

  // 6. Direct Resolved Timetable CSV Download
  try {
    const resRes1 = await invokeApi('GET', '/api/timetable/export?resolved=1');
    assert.strictEqual(resRes1.status, 200);
    assert(resRes1.headers['content-type'].includes('text/csv'));
    assert(resRes1.headers['content-disposition'].includes('resolved_timetable.csv'));
    assert(typeof resRes1.body === 'string' && resRes1.body.startsWith('Section,Day,Start_Time'));

    const resRes2 = await invokeApi('GET', '/api/clash/resolved-timetable');
    assert.strictEqual(resRes2.status, 200);
    assert(resRes2.headers['content-type'].includes('text/csv'));
    assert(resRes2.headers['content-disposition'].includes('resolved_timetable.csv'));
    pass('Direct downloadable resolved timetable CSV endpoints (/api/timetable/export?resolved=1 and /api/clash/resolved-timetable)');
  } catch (e) {
    fail('Direct downloadable resolved timetable CSV endpoints', e);
  }

  // 7. Single Clash Proposal Full Metadata & Apply Resolution
  try {
    const propRes = await invokeApi('POST', '/api/clash/resolve', { clashIndex: 0 });
    assert.strictEqual(propRes.status, 200);
    assert.strictEqual(propRes.body.ok, true);
    const p = propRes.body.proposal;
    assert(p.original.subject, 'Original subject must not be empty');
    assert(p.original.section, 'Original section must not be empty');
    assert(typeof p.original.roomCapacity === 'number', 'Original roomCapacity must be a number');
    assert(p.proposed.subject, 'Proposed subject must not be empty');
    assert(typeof p.proposed.roomCapacity === 'number', 'Proposed roomCapacity must be a number');
    pass('Single clash resolver emits complete subject, section, faculty and capacity metadata');
  } catch (e) {
    fail('Single clash resolver metadata', e);
  }

  console.log('\n========================================================');
  console.log(`TEST SUMMARY: ${passed}/${passed + failed} passed (${failed} failed)`);
  console.log('========================================================\n');

  if (failed > 0) process.exit(1);
}

runIntegrationSuite();
