/*
 * App shell: sign-in screen, sidebar, hash routing and logout.
 * Routes are #/<module id>; unknown routes fall back to the dashboard.
 */
(function () {
  "use strict";
  var M = window.AdminModules, A = window.AdminAuth, P = window.AdminPages, I = window.Icons, esc = P.esc;
  var $ = function (s) { return document.querySelector(s); };

  /* ---------- Sidebar ---------- */
  function renderNav() {
    var out = "", group = "";
    M.forEach(function (m) {
      if (m.group !== group) {
        if (group) out += "</ul>";
        group = m.group;
        out += '<div class="nav-group">' + esc(group) + '</div><ul class="nav-list">';
      }
      out += '<li><a class="nav-link" href="#/' + m.id + '" data-route="' + m.id + '" title="' + esc(m.label) + '">' +
        I.svg(m.icon, 20) + '<span class="nav-label">' + esc(m.label) + '</span></a></li>';
    });
    $("#nav").innerHTML = out + "</ul>";
  }
  function setMenu(open) {
    document.body.classList.toggle("nav-open", open);
    $("#scrim").hidden = !open;
    $("#menu-btn").setAttribute("aria-expanded", String(open));
  }

  /* ---------- Views ---------- */
  function showLogin() {
    document.body.dataset.auth = "out";
    $("#login-view").hidden = false; $("#portal").hidden = true;
    document.title = "Admin Portal | University Timetable Management System";
  }
  function showPortal(session) {
    document.body.dataset.auth = "in";
    $("#login-view").hidden = true; $("#portal").hidden = false;
    $("#avatar").textContent = session.initials || "A";
    $("#profile-name").textContent = session.name;
    $("#profile-role").textContent = session.role || "";
    $("#mode-pill").hidden = session.mode !== "demo";
  }

  function route() {
    var session = A.getSession();
    if (!session) { showLogin(); return; }
    showPortal(session);
    var id = (location.hash || "").replace(/^#\/?/, "").split("/")[0] || "dashboard";
    var m = M.filter(function (x) { return x.id === id; })[0];
    if (!m) { history.replaceState(null, "", "#/dashboard"); m = M[0]; }

    var html = (typeof P[m.id] === "function") ? P[m.id]() : (m.kind === "dashboard" ? P.dashboard() : (m.render ? m.render() : P.placeholder(m)));
    $("#page").innerHTML = html;
    if (typeof P.initPage === "function") {
      try {
        P.initPage(m.id, $("#page"));
      } catch (err) {
        console.error("Failed to init page component:", err);
      }
    }
    document.querySelectorAll(".nav-link").forEach(function (a) {
      if (a.dataset.route === m.id) a.setAttribute("aria-current", "page"); else a.removeAttribute("aria-current");
    });
    document.title = m.label + " | Admin Portal";
    setMenu(false);
    window.scrollTo(0, 0);
    var h = $("#page h1"); if (h) h.focus({ preventScroll: true });
  }

  /* ---------- Sign-in form ---------- */
  var form = $("#login-form"), alertBox = $("#form-alert");
  function fieldError(name, msg) {
    var input = $("#" + name), box = $("#" + name + "-error");
    box.textContent = msg || ""; box.hidden = !msg;
    if (msg) input.setAttribute("aria-invalid", "true"); else input.removeAttribute("aria-invalid");
  }
  function clearErrors() { fieldError("identifier"); fieldError("password"); alertBox.hidden = true; alertBox.textContent = ""; }
  function enter(result) {
    if (!result.ok) { alertBox.textContent = result.message; alertBox.hidden = false; return; }
    form.reset(); clearErrors();
    if (!location.hash) history.replaceState(null, "", "#/dashboard");
    route();
  }

  form.addEventListener("submit", function (e) {
    e.preventDefault(); clearErrors();
    var creds = { identifier: $("#identifier").value, password: $("#password").value };
    var errors = A.validate(creds);
    if (errors.identifier || errors.password) {
      fieldError("identifier", errors.identifier); fieldError("password", errors.password);
      (errors.identifier ? $("#identifier") : $("#password")).focus();
      return;
    }
    A.signIn(creds).then(enter);
  });
  ["identifier", "password"].forEach(function (n) { $("#" + n).addEventListener("input", function () { fieldError(n); alertBox.hidden = true; }); });
  $("#demo-btn").addEventListener("click", function () { clearErrors(); A.signInDemo().then(enter); });
  $("#toggle-password").addEventListener("click", function () {
    var pw = $("#password"), show = pw.type === "password";
    pw.type = show ? "text" : "password";
    this.textContent = show ? "Hide" : "Show";
    this.setAttribute("aria-pressed", String(show));
    this.setAttribute("aria-label", show ? "Hide password" : "Show password");
  });

  /* ---------- Logout and shell events ---------- */
  $("#logout-btn").innerHTML = I.svg("logout", 16) + "<span>Log out</span>";
  $("#menu-btn").innerHTML = I.svg("menu", 22);
  $("#logout-btn").addEventListener("click", function () {
    A.signOut().then(function () {
      history.replaceState(null, "", location.pathname + location.search);
      setMenu(false);
      showLogin();
      $("#identifier").focus();
    });
  });
  $("#menu-btn").addEventListener("click", function () { setMenu(!document.body.classList.contains("nav-open")); });
  $("#scrim").addEventListener("click", function () { setMenu(false); });
  document.addEventListener("keydown", function (e) { if (e.key === "Escape") setMenu(false); });
  window.addEventListener("hashchange", route);

  renderNav();
  route();
})();
