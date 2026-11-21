/*
 * Unit tests for the Admin sign-in seam (validation, demo session, logout).
 * Run from adminfrontend/:  node tests/auth.test.js
 */
const assert = require("assert");
global.window = global;
require("../js/auth.js");
const Auth = global.AdminAuth;
let passed = 0, failed = 0;
async function test(name, fn) {
  try { await fn(); passed++; console.log("  PASS  " + name); } catch (e) { failed++; console.log("  FAIL  " + name + "\n        " + e.message); }
}
function store() { const m = {}; return { m, getItem: k => (k in m ? m[k] : null), setItem: (k, v) => { m[k] = v; }, removeItem: k => { delete m[k]; } }; }

(async () => {
  console.log("Validation");
  await test("empty fields are rejected", () => { const e = Auth.validate({ identifier: "", password: "" }); assert.ok(e.identifier && e.password); });
  await test("whitespace-only ID is rejected", () => assert.ok(Auth.validate({ identifier: "   ", password: "x" }).identifier));
  await test("malformed email is rejected", () => assert.ok(Auth.validate({ identifier: "a@b", password: "x" }).identifier));
  await test("valid email accepted", () => assert.deepStrictEqual(Auth.validate({ identifier: "name@university.edu", password: "x" }), {}));
  await test("valid Admin ID accepted; too-short or spaced IDs rejected", () => {
    assert.deepStrictEqual(Auth.validate({ identifier: "adm.01", password: "x" }), {});
    assert.ok(Auth.validate({ identifier: "ab", password: "x" }).identifier);
    assert.ok(Auth.validate({ identifier: "ad min", password: "x" }).identifier);
  });
  await test("missing password is rejected", () => assert.ok(Auth.validate({ identifier: "adm.01", password: "" }).password));

  console.log("Prototype provider and session");
  await test("credential sign-in never succeeds in the prototype", async () => {
    const a = Auth.create(Auth.prototypeProvider, store()), r = await a.signIn({ identifier: "adm.01", password: "anything" });
    assert.strictEqual(r.ok, false); assert.strictEqual(r.code, "NOT_CONNECTED"); assert.strictEqual(a.getSession(), null);
  });
  await test("demo access creates a labelled demo session", async () => {
    const s = store(), a = Auth.create(Auth.prototypeProvider, s), r = await a.signInDemo();
    assert.ok(r.ok); assert.strictEqual(a.getSession().mode, "demo"); assert.ok(JSON.parse(s.m["adminPortal.session"]).mode === "demo");
  });
  await test("session is restored from storage", async () => {
    const s = store(); await Auth.create(Auth.prototypeProvider, s).signInDemo();
    assert.strictEqual(Auth.create(Auth.prototypeProvider, s).getSession().name, "Demo Administrator");
  });
  await test("logout clears the session", async () => {
    const s = store(), a = Auth.create(Auth.prototypeProvider, s); await a.signInDemo(); await a.signOut();
    assert.strictEqual(a.getSession(), null); assert.strictEqual(Object.keys(s.m).length, 0);
  });
  await test("tampered session in storage is ignored", () => {
    const s = store(); s.setItem("adminPortal.session", JSON.stringify({ name: "x", mode: "root" }));
    assert.strictEqual(Auth.create(Auth.prototypeProvider, s).getSession(), null);
  });
  await test("provider can be swapped without changing callers", async () => {
    const live = { signIn: async () => ({ ok: true, session: { name: "Real", initials: "R", role: "Admin", mode: "live" } }), signInDemo: async () => ({ ok: false }), signOut: async () => {} };
    const a = Auth.create(live, store()); assert.ok((await a.signIn({ identifier: "x", password: "y" })).ok); assert.strictEqual(a.getSession().mode, "live");
  });

  console.log("\n" + passed + " passed, " + failed + " failed");
  process.exit(failed ? 1 : 0);
})();
