const assert = require('node:assert');
const path = require('node:path');

// Mock browser global
global.window = global;
global.sessionStorage = {
  data: {},
  getItem(k) { return this.data[k] || null; },
  setItem(k, v) { this.data[k] = String(v); },
  removeItem(k) { delete this.data[k]; }
};

// Mock AdminAPI
global.AdminAPI = {
  login(id, pw) {
    if (id === 'rajesh.verma@geu.ac.in' && pw === 'Rv@4821#tt') {
      return Promise.resolve({
        ok: true,
        session: {
          name: 'Rajesh Verma',
          initials: 'RV',
          role: 'Administrator',
          adminId: 'GEU-F001',
          email: 'rajesh.verma@geu.ac.in',
          token: 'mock-token-123'
        }
      });
    }
    return Promise.reject(new Error('Invalid credentials'));
  },
  logout() {
    return Promise.resolve();
  }
};

require('../js/auth.js');

async function testLiveAuth() {
  console.log('Testing liveApiProvider...');
  const auth = AdminAuth.create(AdminAuth.liveApiProvider, global.sessionStorage);

  // 1. Invalid login
  const badRes = await auth.signIn({ identifier: 'bad@geu.ac.in', password: 'wrong' });
  assert.strictEqual(badRes.ok, false);

  // 2. Valid login
  const goodRes = await auth.signIn({ identifier: 'rajesh.verma@geu.ac.in', password: 'Rv@4821#tt' });
  assert.strictEqual(goodRes.ok, true);
  assert.strictEqual(goodRes.session.name, 'Rajesh Verma');
  assert.strictEqual(goodRes.session.adminId, 'GEU-F001');
  assert.strictEqual(goodRes.session.mode, 'live');

  // 3. Session persistence
  const s = auth.getSession();
  assert.strictEqual(s.name, 'Rajesh Verma');

  // 4. Logout
  await auth.signOut();
  assert.strictEqual(auth.getSession(), null);

  console.log('ALL LIVE AUTH TESTS PASSED!\n');
}

testLiveAuth().catch(err => {
  console.error(err);
  process.exit(1);
});
