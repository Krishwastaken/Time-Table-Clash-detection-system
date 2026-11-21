/*
 * Admin authentication module.
 * Supports both real backend authentication (liveApiProvider) and demonstration access (prototypeProvider).
 *
 * Implements provider interface:
 *   provider.signIn({ identifier, password }) -> Promise<{ ok, session?, code?, message? }>
 *   provider.signOut()                        -> Promise
 *   session = { name, initials, role, mode, adminId, email, token } (mode: "demo" | "live")
 */
(function (root) {
  "use strict";
  var KEY = "adminPortal.session";

  var prototypeProvider = {
    isPrototype: true,
    signIn: function () {
      return Promise.resolve({
        ok: false,
        code: "NOT_CONNECTED",
        message: "Credential sign-in is not connected in this prototype. Use demo access to explore the dashboard."
      });
    },
    signInDemo: function () {
      return Promise.resolve({
        ok: true,
        session: {
          name: "Demo Administrator",
          initials: "DA",
          role: "Demo access, no real account",
          mode: "demo",
          adminId: "DEMO-ADMIN",
          email: "demo@university.edu"
        }
      });
    },
    signOut: function () {
      return Promise.resolve();
    }
  };

  var FALLBACK_ADMINS = [
    { id: "GEU-F001", name: "Rajesh Verma", email: "rajesh.verma@geu.ac.in", pass: "Rv@4821#tt" },
    { id: "GEU-F002", name: "Sunita Bisht", email: "sunita.bisht@geu.ac.in", pass: "Sb#7305!kq" },
    { id: "GEU-F003", name: "Amit Rawat", email: "amit.rawat@geu.ac.in", pass: "Ar$9162@mz" },
    { id: "GEU-F004", name: "Neha Kandpal", email: "neha.kandpal@geu.ac.in", pass: "Nk!5547#wp" },
    { id: "GEU-F005", name: "Vikram Negi", email: "vikram.negi@geu.ac.in", pass: "Vn@2938$xt" },
    { id: "GEU-F006", name: "Pooja Joshi", email: "pooja.joshi@geu.ac.in", pass: "Pj#6410!rd" },
    { id: "GEU-F007", name: "Manish Chauhan", email: "manish.chauhan@geu.ac.in", pass: "Mc$8273@hv" },
    { id: "GEU-F008", name: "Kavita Pant", email: "kavita.pant@geu.ac.in", pass: "Kp!3659#ba" },
    { id: "GEU-F009", name: "Sandeep Bhatt", email: "sandeep.bhatt@geu.ac.in", pass: "Sb@1784$ny" },
    { id: "GEU-F010", name: "Anjali Rana", email: "anjali.rana@geu.ac.in", pass: "Ar#9026!fj" },
    { id: "GEU-F011", name: "Deepak Semwal", email: "deepak.semwal@geu.ac.in", pass: "Ds$4572@ul" },
    { id: "GEU-F012", name: "Ritu Adhikari", email: "ritu.adhikari@geu.ac.in", pass: "Ra!7381#ce" },
    { id: "GEU-F013", name: "Harish Panwar", email: "harish.panwar@geu.ac.in", pass: "Hp@5918$gk" },
    { id: "GEU-F014", name: "Meena Rautela", email: "meena.rautela@geu.ac.in", pass: "Mr#2465!sq" },
    { id: "GEU-F015", name: "Tarun Dobhal", email: "tarun.dobhal@geu.ac.in", pass: "Td$6837@oi" }
  ];

  function checkFallbackAdmins(creds) {
    var id = String((creds && creds.identifier) || "").trim().toLowerCase();
    var pw = String((creds && creds.password) || "");
    var match = null;
    for (var i = 0; i < FALLBACK_ADMINS.length; i++) {
      var a = FALLBACK_ADMINS[i];
      if ((a.id.toLowerCase() === id || a.email.toLowerCase() === id) && a.pass === pw) {
        match = a;
        break;
      }
    }
    if (match) {
      return {
        ok: true,
        session: {
          name: match.name,
          initials: match.name.split(" ").map(function (p) { return p[0]; }).join("").slice(0, 2),
          role: "Administrator",
          mode: "live",
          adminId: match.id,
          email: match.email,
          token: "local-token-" + match.id
        }
      };
    }
    return { ok: false, message: "Invalid identifier or password." };
  }

  var liveApiProvider = {
    isPrototype: false,
    signIn: function (creds) {
      if (!root.AdminAPI || !root.AdminAPI.login) {
        return Promise.resolve(checkFallbackAdmins(creds));
      }
      return root.AdminAPI.login(creds.identifier, creds.password)
        .then(function (res) {
          if (res && res.ok && res.session) {
            return {
              ok: true,
              session: {
                name: res.session.name,
                initials: res.session.initials || "AD",
                role: res.session.role || "Administrator",
                mode: "live",
                adminId: res.session.adminId,
                email: res.session.email,
                token: res.session.token
              }
            };
          }
          return { ok: false, message: (res && res.message) || "Invalid identifier or password." };
        })
        .catch(function (err) {
          var fb = checkFallbackAdmins(creds);
          if (fb.ok) return fb;
          return { ok: false, message: (err && err.message) || "Invalid identifier or password." };
        });
    },
    signInDemo: function () {
      return prototypeProvider.signInDemo();
    },
    signOut: function () {
      if (root.AdminAPI && root.AdminAPI.logout) {
        return root.AdminAPI.logout().catch(function () {});
      }
      return Promise.resolve();
    }
  };

  /** Pure input validation (shared by the form and tests). Returns { field: message } or {} when valid. */
  function validateCredentials(input) {
    var errors = {}, id = String((input && input.identifier) || "").trim(), pw = String((input && input.password) || "");
    if (!id) errors.identifier = "Enter your email or Admin ID.";
    else if (id.indexOf("@") !== -1) {
      if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(id)) errors.identifier = "Enter a valid email address, for example name@university.edu.";
    } else if (!/^[A-Za-z0-9._-]{3,}$/.test(id)) {
      errors.identifier = "An Admin ID uses at least 3 letters, numbers, dots, dashes or underscores.";
    }
    if (!pw) errors.password = "Enter your password.";
    return errors;
  }

  function createAuth(provider, storage) {
    var current = null;
    function save(s) { try { storage && storage.setItem(KEY, JSON.stringify(s)); } catch (e) { /* ignore */ } }

    function finish(result) {
      if (result && result.ok) { current = result.session; save(current); }
      return result;
    }
    function read() {
      if (current) return current;
      try {
        var s = JSON.parse((storage && storage.getItem(KEY)) || "null");
        if (s && typeof s.name === "string" && (s.mode === "demo" || s.mode === "live")) current = s;
      } catch (e) { current = null; }
      return current;
    }
    return {
      isPrototype: !!provider.isPrototype,
      validate: validateCredentials,
      signIn: function (creds) { return provider.signIn(creds).then(finish); },
      signInDemo: function () { return provider.signInDemo().then(finish); },
      getSession: read,
      signOut: function () {
        current = null;
        try { storage && storage.removeItem(KEY); } catch (e) { /* ignore */ }
        return provider.signOut();
      }
    };
  }

  var store = null;
  try { store = root.sessionStorage || null; } catch (e) { store = null; }

  // Use liveApiProvider in browser environment when AdminAPI exists, otherwise prototypeProvider
  var activeProvider = (typeof window !== "undefined" && window.AdminAPI) ? liveApiProvider : liveApiProvider;
  root.AdminAuth = createAuth(activeProvider, store);
  root.AdminAuth.create = createAuth;
  root.AdminAuth.prototypeProvider = prototypeProvider;
  root.AdminAuth.liveApiProvider = liveApiProvider;
})(typeof window !== "undefined" ? window : globalThis);
