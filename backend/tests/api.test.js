const assert = require('node:assert');
const { Readable, Writable } = require('node:stream');
const { requestHandler } = require('../src/app');
const { runSeed } = require('../src/services/seed.service');
const { provisionAdminsFromCSV } = require('../src/services/provisioning.service');

// Initialize database & admins
runSeed();
provisionAdminsFromCSV();

let adminToken = '';

function invokeApi(method, path, body = null, headers = {}) {
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
    req.url = path;
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

    res.getHeader = function (k) {
      return resHeaders[k.toLowerCase()];
    };

    res.on('finish', () => {
      let json = null;
      try { json = JSON.parse(resBody); } catch (e) { json = resBody; }
      resolve({ status: resStatusCode, headers: resHeaders, body: json });
    });

    requestHandler(req, res);
  });
}

async function runTests() {
  console.log('========================================================');
  console.log('RUNNING FULL IN-MEMORY BACKEND & C++ INTEGRATION TESTS');
  console.log('========================================================\n');

  let passed = 0;
  let total = 0;

  async function test(name, fn) {
    total++;
    try {
      await fn();
      console.log(`  PASS: ${name}`);
      passed++;
    } catch (err) {
      console.error(`  FAIL: ${name} ->`, err.message);
    }
  }

  // 1. Authentication Tests
  await test('Authentication: Invalid credentials rejected', async () => {
    const res = await invokeApi('POST', '/api/auth/login', {
      identifier: 'rajesh.verma@geu.ac.in',
      password: 'WrongPassword123'
    });
    assert.strictEqual(res.status, 401);
    assert.strictEqual(res.body.ok, false);
  });

  await test('Authentication: Valid admin login using Email & Password from admin_master.csv', async () => {
    const res = await invokeApi('POST', '/api/auth/login', {
      identifier: 'rajesh.verma@geu.ac.in',
      password: 'Rv@4821#tt'
    });
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.strictEqual(res.body.session.name, 'Rajesh Verma');
    assert.strictEqual(res.body.session.adminId, 'GEU-F001');
    adminToken = res.body.session.token;
  });

  await test('Authentication: Valid admin login using Admin ID', async () => {
    const res = await invokeApi('POST', '/api/auth/login', {
      identifier: 'GEU-F002',
      password: 'Sb#7305!kq'
    });
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.session.name, 'Sunita Bisht');
  });

  await test('Authentication: Verify session via /api/auth/me', async () => {
    const res = await invokeApi('GET', '/api/auth/me');
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.authenticated, true);
    assert.strictEqual(res.body.session.adminId, 'GEU-F001');
  });

  // 2. Dashboard KPIs
  await test('Dashboard: Metrics return real counts (not null or placeholder)', async () => {
    const res = await invokeApi('GET', '/api/stats/dashboard');
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.ok(res.body.metrics.faculty >= 75);
    assert.ok(res.body.metrics.sessions >= 700);
    assert.ok(res.body.metrics.sections >= 10);
  });

  // 3. Student Management
  await test('Students: List, search and pagination', async () => {
    const res = await invokeApi('GET', '/api/students?limit=10');
    assert.strictEqual(res.status, 200);
    assert.ok(res.body.students.length > 0);
  });

  await test('Students: Create, edit, and deactivate student', async () => {
    const testSid = 'GEU-TEST-999';
    const createRes = await invokeApi('POST', '/api/students', {
      studentId: testSid,
      name: 'Test Student Quality',
      email: 'test.student@geu.ac.in',
      section: 'A',
      group: 'G1'
    });
    assert.strictEqual(createRes.status, 200);

    const editRes = await invokeApi('PUT', `/api/students/${testSid}`, {
      name: 'Test Student Quality Edited',
      group: 'G2'
    });
    assert.strictEqual(editRes.status, 200);

    const deleteRes = await invokeApi('DELETE', `/api/students/${testSid}?permanent=true`);
    assert.strictEqual(deleteRes.status, 200);
  });

  // 4. Faculty Management
  await test('Faculty: Master directory populated from faculty_master.csv', async () => {
    const res = await invokeApi('GET', '/api/faculty?limit=100');
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.faculty.length, 75);
    const f1 = res.body.faculty.find(f => f.faculty_id === 'F001');
    assert.ok(f1);
    assert.strictEqual(f1.faculty_name, 'Dr. Hemant Singh Pokhariya');
  });

  await test('Faculty: Update office location', async () => {
    const updateRes = await invokeApi('PUT', '/api/faculty/F001', {
      officeLocation: 'CS Block Room 304'
    });
    assert.strictEqual(updateRes.status, 200);

    const getRes = await invokeApi('GET', '/api/faculty?q=F001');
    const f = getRes.body.faculty.find(x => x.faculty_id === 'F001');
    assert.strictEqual(f.office_location, 'CS Block Room 304');
  });

  // 5. Academic Structure
  await test('Academic Structure: Rooms, capacities, and sections returned', async () => {
    const res = await invokeApi('GET', '/api/academic/structure');
    assert.strictEqual(res.status, 200);
    assert.ok(res.body.structure.rooms.length >= 20);
    assert.ok(res.body.structure.sections.length >= 15);
    const lt1 = res.body.structure.rooms.find(r => r.name === 'LT-1');
    assert.ok(lt1);
    assert.strictEqual(lt1.capacity, 200);
  });

  // 6. Timetable Management
  await test('Timetable: Sessions loaded and filterable by section', async () => {
    const res = await invokeApi('GET', '/api/timetable/sessions?section=A');
    assert.strictEqual(res.status, 200);
    assert.ok(res.body.sessions.length > 0);
    assert.ok(res.body.sessions.every(s => s.section === 'A'));
  });

  // 7. C++ Clash Detection Engine Integration
  await test('C++ Engine: Execute clash detection and parse output', async () => {
    const res = await invokeApi('POST', '/api/clash/detect');
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.ok(res.body.report.totalSessions >= 700);
    assert.ok(res.body.report.totalClashes > 0);
    assert.ok(res.body.report.clashes.length > 0);
    assert.ok(res.body.report.clashes[0].type);
  });

  // 8. C++ Resolver Integration
  await test('C++ Resolver: Propose resolution for a detected clash', async () => {
    const res = await invokeApi('POST', '/api/clash/resolve', { clashIndex: 0 });
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.ok(res.body.proposal.action);
    assert.ok(res.body.proposal.explanation);
  });

  // 9. Publishing & Version History
  await test('Publishing: Publish timetable version', async () => {
    const res = await invokeApi('POST', '/api/published/publish', {
      versionNumber: 'v1.0-test',
      notes: 'Initial automated publish test',
      forcePublish: true
    });
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);

    const verRes = await invokeApi('GET', '/api/published/versions');
    assert.strictEqual(verRes.status, 200);
    assert.ok(verRes.body.versions.some(v => v.versionNumber === 'v1.0-test'));
  });

  // 10. Shared Portal APIs
  await test('Shared Portal API: Student timetable retrieval', async () => {
    const res = await invokeApi('GET', '/api/portal/student/timetable?section=A');
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.strictEqual(res.body.section, 'A');
    assert.ok(res.body.sessions.length > 0);
  });

  await test('Shared Portal API: Faculty directory for Campus Connect', async () => {
    const res = await invokeApi('GET', '/api/portal/faculty/directory');
    assert.strictEqual(res.status, 200);
    assert.strictEqual(res.body.ok, true);
    assert.strictEqual(res.body.total, 75);
  });

  // 11. Activity Logs
  await test('Activity Logs: Audit trail tracks admin actions', async () => {
    const res = await invokeApi('GET', '/api/activity/logs');
    assert.strictEqual(res.status, 200);
    assert.ok(res.body.logs.length > 0);
  });

  console.log('\n========================================================');
  console.log(`TEST SUMMARY: ${passed}/${total} passed (${total - passed} failed)`);
  console.log('========================================================\n');
  process.exit(passed === total ? 0 : 1);
}

runTests().catch(err => {
  console.error('Test runner fatal error:', err);
  process.exit(1);
});
