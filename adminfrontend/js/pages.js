/*
 * Complete Page Renderers & Interactive Module Handlers for Admin Portal
 */
(function () {
  "use strict";

  var C = window.AdminConfig;
  var D = window.AdminData;
  var I = window.Icons;
  var API = window.AdminAPI;

  function esc(v) {
    if (v == null) return "";
    return String(v).replace(/[&<>"']/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
    });
  }

  function value(v) {
    return v == null ? C.placeholder : esc(v);
  }

  function notConnected(v) {
    return v == null ? '<span class="chip chip-muted">Not connected</span>' : "";
  }

  function pageHeader(crumbs, title, sub, extraActions) {
    return (
      '<header class="page-header">' +
      '<div style="display:flex; justify-content:space-between; align-items:flex-start; flex-wrap:wrap; gap:12px;">' +
      '<div>' +
      '<nav class="breadcrumb" aria-label="Breadcrumb"><ol>' +
      crumbs
        .map(function (c, i) {
          var last = i === crumbs.length - 1;
          return (
            '<li' +
            (last ? ' aria-current="page"' : "") +
            ">" +
            (c.href && !last ? '<a href="' + c.href + '">' + esc(c.label) + "</a>" : esc(c.label)) +
            "</li>"
          );
        })
        .join("") +
      "</ol></nav>" +
      '<h1 tabindex="-1">' +
      esc(title) +
      "</h1>" +
      (sub ? '<p class="page-sub">' + esc(sub) + "</p>" : "") +
      "</div>" +
      (extraActions ? '<div class="action-group">' + extraActions + "</div>" : "") +
      "</div>" +
      "</header>"
    );
  }

  /* =====================================================================
   * 1. DASHBOARD
   * ===================================================================== */
  var KPIS = [
    { id: "students", label: "Total Students", icon: "users", desc: "Enrolled student records" },
    { id: "faculty", label: "Total Faculty", icon: "faculty", desc: "Faculty master directory" },
    { id: "sections", label: "Active Sections", icon: "grid", desc: "Sections with scheduled sessions" },
    { id: "sessions", label: "Scheduled Sessions", icon: "calendar", desc: "Timetable sessions in draft" },
    { id: "pending", label: "Pending Changes", icon: "pencil", desc: "Draft edits not yet published" },
    { id: "clashes", label: "Detected Clashes", icon: "alert", desc: "From C++ clash detection engine" }
  ];

  var CLASH_CATEGORIES = ["Faculty", "Room", "Section", "Room capacity", "Duplicate session"];

  var QUICK = [
    { route: "students", icon: "users", title: "Manage Students", desc: "Student records and cohorts" },
    { route: "faculty", icon: "faculty", title: "Manage Faculty", desc: "Faculty directory & offices" },
    { route: "timetable", icon: "calendar", title: "Manage Timetable", desc: "Sessions and weekly grid" },
    { route: "clash-detection", icon: "play", title: "Run Clash Detection", desc: "Execute C++ engine" }
  ];

  function dashboard() {
    var status = D.getTimetableStatus() || {};
    var clash = D.getClashSummary() || {};
    var activity = D.getActivityFeed() || [];

    var kpis = KPIS.map(function (k) {
      var v = D.getMetric(k.id);
      return (
        '<article class="kpi"><span class="kpi-icon">' +
        I.svg(k.icon, 22) +
        '</span><div class="kpi-body">' +
        '<div class="kpi-label">' +
        esc(k.label) +
        "</div>" +
        '<div class="kpi-value">' +
        (v == null ? '<span aria-hidden="true">' + C.placeholder + '</span>' : esc(v)) +
        "</div>" +
        '<div class="kpi-desc">' +
        esc(k.desc) +
        "</div></div>" +
        (v == null ? notConnected(v) : '<span class="chip chip-info">Live</span>') +
        "</article>"
      );
    }).join("");

    var rows = [
      ["Current status", status.state || "Draft (Unpublished)"],
      ["Last published version", status.lastPublished || "None published yet"],
      ["Last updated", status.lastUpdated ? new Date(status.lastUpdated).toLocaleString() : "Just now"],
      ["Pending changes", status.pending || "Draft sessions active"]
    ]
      .map(function (r) {
        return '<div class="kv"><dt>' + esc(r[0]) + "</dt><dd><strong>" + esc(r[1]) + "</strong></dd></div>";
      })
      .join("");

    var cats = CLASH_CATEGORIES.map(function (c) {
      var n = clash.byCategory && clash.byCategory[c];
      return (
        "<li><span>" +
        esc(c) +
        '</span><span class="badge ' +
        (n ? "badge-danger" : "badge-muted") +
        '">' +
        (n == null ? C.placeholder : esc(n)) +
        "</span></li>"
      );
    }).join("");

    var feedHtml = "";
    if (activity && activity.length > 0) {
      feedHtml = activity
        .map(function (a) {
          return (
            '<li class="feed-item"><span class="feed-icon">' +
            I.svg(a.icon || "list", 16) +
            "</span><div><div>" +
            esc(a.text) +
            '</div><div class="feed-meta"><span class="chip chip-info">' +
            esc(a.meta || "Audit") +
            "</span> " +
            (a.time ? new Date(a.time).toLocaleTimeString() : "") +
            "</div></div></li>"
          );
        })
        .join("");
    } else {
      feedHtml = '<li class="feed-item"><span class="feed-icon">' + I.svg("list", 16) + '</span><div><div>System initialized with academic datasets</div><div class="feed-meta"><span class="chip chip-info">System</span> Just now</div></div></li>';
    }

    var quick = QUICK.map(function (q) {
      return (
        '<a class="quick" href="#/' +
        q.route +
        '"><span class="quick-icon">' +
        I.svg(q.icon, 20) +
        '</span><span class="quick-text"><strong>' +
        esc(q.title) +
        "</strong><span>" +
        esc(q.desc) +
        '</span></span><span class="quick-arrow">' +
        I.svg("arrow", 16) +
        "</span></a>"
      );
    }).join("");

    return (
      pageHeader([{ label: "Admin Portal" }, { label: "Dashboard" }], "Dashboard", "Central administrative command center.") +
      '<div class="banner" role="note" style="border-left-color: #2563eb; background: #eff6ff;">' +
      I.svg("info", 18) +
      "<p><strong>Live Integrated System.</strong> Connected to backend REST API, SQLite database, and the native C++ clash detection and resolution engine.</p></div>" +
      '<section aria-label="Overview" class="kpi-grid">' +
      kpis +
      "</section>" +
      '<div class="dash-grid">' +
      '<div class="dash-col">' +
      '<section class="card"><div class="card-head"><h2>Timetable status</h2><span class="badge badge-info">' + esc(status.state || "Draft") + '</span></div><dl class="kv-list">' +
      rows +
      "</dl></section>" +
      '<section class="card"><div class="card-head"><h2>Recent activity audit</h2><span class="chip chip-info">Live logs</span></div>' +
      '<ul class="feed">' +
      feedHtml +
      "</ul></section>" +
      "</div>" +
      '<div class="dash-col">' +
      '<section class="card"><div class="card-head"><h2>Clash detection summary</h2>' +
      (clash.total ? '<span class="badge badge-danger">' + clash.total + ' Clashes</span>' : '<span class="badge badge-success">Evaluated</span>') +
      '</div><div class="mini-stats">' +
      "<div><span class=\"mini-value\" style=\"color:#dc2626;\">" + value(clash.total) + '</span><span class="mini-label">Detected</span></div>' +
      "<div><span class=\"mini-value\" style=\"color:#16a34a;\">" + value(clash.resolved) + '</span><span class="mini-label">Resolved</span></div>' +
      "<div><span class=\"mini-value\" style=\"color:#ea580c;\">" + value(clash.unresolved) + '</span><span class="mini-label">Unresolved</span></div>' +
      "</div>" +
      '<ul class="cat-list" aria-label="Clash categories">' +
      cats +
      "</ul>" +
      '<div style="margin-top:16px; display:flex; gap:8px;">' +
      '<a href="#/clash-detection" class="btn btn-primary btn-block" style="text-align:center;">' + I.svg("play", 16) + ' Run Clash Engine</a>' +
      "</div></section>" +
      '<section class="card"><div class="card-head"><h2>Quick actions</h2></div><div class="quick-list">' +
      quick +
      "</div></section>" +
      "</div>" +
      "</div>"
    );
  }

  /* =====================================================================
   * 2. STUDENT MANAGEMENT
   * ===================================================================== */
  function studentsPage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Student Management" }],
        "Student Management",
        "Maintain student records, section memberships, and future Student Portal identities.",
        '<button type="button" class="btn btn-primary" id="btn-add-student">' + I.svg("plus", 16) + ' Add Student</button>' +
        '<button type="button" class="btn btn-secondary" id="btn-import-students-modal">' + I.svg("upload", 16) + ' Import CSV</button>' +
        '<a href="' + API.getStudentsExportUrl() + '" class="btn btn-secondary" target="_blank">' + I.svg("download", 16) + ' Export CSV</a>'
      ) +
      '<div class="module-toolbar">' +
      '<div class="search-filters">' +
      '<input type="text" id="student-search" class="input search-input" placeholder="Search by ID or name...">' +
      '<select id="student-section-filter" class="select-filter"><option value="">All Sections</option></select>' +
      '<select id="student-status-filter" class="select-filter"><option value="">All Statuses</option><option value="ACTIVE">Active</option><option value="INACTIVE">Inactive</option></select>' +
      "</div>" +
      '<div id="student-count-badge" class="badge badge-info">Loading records...</div>' +
      "</div>" +
      '<div class="table-container">' +
      '<table class="data-table" id="students-table">' +
      "<thead><tr><th>Student ID</th><th>Name</th><th>Email</th><th>Section</th><th>Group</th><th>Status</th><th>Actions</th></tr></thead>" +
      '<tbody id="students-table-body"><tr><td colspan="7" style="text-align:center; padding:24px;">Loading student records...</td></tr></tbody>' +
      "</table>" +
      "</div>" +
      '<div class="pagination" id="student-pagination" hidden>' +
      '<span id="student-pagination-info">Showing records</span>' +
      '<div class="pagination-controls">' +
      '<button type="button" class="btn btn-secondary btn-sm" id="btn-student-prev">Previous</button>' +
      '<span id="student-page-number" style="font-weight:600; padding:0 8px;">1</span>' +
      '<button type="button" class="btn btn-secondary btn-sm" id="btn-student-next">Next</button>' +
      "</div>" +
      "</div>" +
      '<div id="modal-container"></div>'
    );
  }

  /* =====================================================================
   * 3. FACULTY MANAGEMENT
   * ===================================================================== */
  function facultyPage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Faculty Management" }],
        "Faculty Management",
        "Maintain faculty master records, departmental designations, and office locations across all sections.",
        '<button type="button" class="btn btn-primary" id="btn-add-faculty">' + I.svg("plus", 16) + ' Add Faculty</button>' +
        '<button type="button" class="btn btn-secondary" id="btn-import-faculty-modal">' + I.svg("upload", 16) + ' Import CSV</button>' +
        '<a href="' + API.getFacultyExportUrl() + '" class="btn btn-secondary" target="_blank">' + I.svg("download", 16) + ' Export CSV</a>'
      ) +
      '<div class="module-toolbar">' +
      '<div class="search-filters">' +
      '<input type="text" id="faculty-search" class="input search-input" placeholder="Search by name or Faculty ID...">' +
      '<select id="faculty-dept-filter" class="select-filter"><option value="">All Departments</option></select>' +
      '<select id="faculty-status-filter" class="select-filter"><option value="">All Statuses</option><option value="ACTIVE">Active</option><option value="INACTIVE">Inactive</option></select>' +
      "</div>" +
      '<div id="faculty-count-badge" class="badge badge-info">75 Master Records</div>' +
      "</div>" +
      '<div class="table-container">' +
      '<table class="data-table" id="faculty-table">' +
      "<thead><tr><th>Faculty ID</th><th>Name</th><th>Department</th><th>Designation</th><th>Email</th><th>Office Location</th><th>Status</th><th>Actions</th></tr></thead>" +
      '<tbody id="faculty-table-body"><tr><td colspan="8" style="text-align:center; padding:24px;">Loading faculty directory...</td></tr></tbody>' +
      "</table>" +
      "</div>" +
      '<div class="pagination" id="faculty-pagination" hidden>' +
      '<span id="faculty-pagination-info">Showing records</span>' +
      '<div class="pagination-controls">' +
      '<button type="button" class="btn btn-secondary btn-sm" id="btn-faculty-prev">Previous</button>' +
      '<span id="faculty-page-number" style="font-weight:600; padding:0 8px;">1</span>' +
      '<button type="button" class="btn btn-secondary btn-sm" id="btn-faculty-next">Next</button>' +
      "</div>" +
      "</div>" +
      '<div id="modal-container"></div>'
    );
  }

  /* =====================================================================
   * 4. ACADEMIC STRUCTURE MANAGEMENT
   * ===================================================================== */
  function academicStructurePage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Academic Structure" }],
        "Academic Structure",
        "Configure university semesters, departments, classrooms, and room capacities."
      ) +
      '<div class="tabs">' +
      '<button type="button" class="tab-btn active" data-tab="tab-rooms">Classrooms & Capacities</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-sections">Sections & Cohorts</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-subjects">Subjects & Courses</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-term">Term & Department</button>' +
      "</div>" +
      '<div id="tab-rooms" class="tab-content">' +
      '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:12px;">' +
      '<div><strong>Classroom Inventory & Capacity Limits</strong><p class="muted" style="margin:2px 0 0 0; font-size:13px;">CR rooms default to 100, LT rooms to 200, Auditorium to 500. Used by C++ clash detector.</p></div>' +
      '<button type="button" class="btn btn-primary btn-sm" id="btn-add-room">' + I.svg("plus", 14) + ' Add Room</button>' +
      "</div>" +
      '<div class="table-container"><table class="data-table"><thead><tr><th>Room Name</th><th>Type</th><th>Capacity</th><th>Building / Location</th><th>Actions</th></tr></thead>' +
      '<tbody id="rooms-table-body"><tr><td colspan="5" style="text-align:center; padding:16px;">Loading rooms...</td></tr></tbody></table></div>' +
      "</div>" +
      '<div id="tab-sections" class="tab-content" hidden>' +
      '<div style="margin-bottom:12px;"><strong>Academic Sections & Lab Cohorts</strong><p class="muted" style="margin:2px 0 0 0; font-size:13px;">All sections active in the 3rd Semester.</p></div>' +
      '<div class="table-container"><table class="data-table"><thead><tr><th>Section Name</th><th>Cohort Strength</th><th>Semester</th><th>Groups</th></tr></thead>' +
      '<tbody id="sections-table-body"></tbody></table></div>' +
      "</div>" +
      '<div id="tab-subjects" class="tab-content" hidden>' +
      '<div style="margin-bottom:12px;"><strong>Curriculum Subjects & Code Inventory</strong></div>' +
      '<div class="table-container"><table class="data-table"><thead><tr><th>Code</th><th>Subject Name</th><th>Type</th><th>Credits</th></tr></thead>' +
      '<tbody id="subjects-table-body"></tbody></table></div>' +
      "</div>" +
      '<div id="tab-term" class="tab-content" hidden>' +
      '<div class="card" style="max-width:600px;"><dl class="kv-list">' +
      '<div class="kv"><dt>Academic Year</dt><dd><strong>2026–2027</strong></dd></div>' +
      '<div class="kv"><dt>Current Semester</dt><dd><strong>Semester 3 (Autumn 2026)</strong></dd></div>' +
      '<div class="kv"><dt>Department</dt><dd><strong>Computer Science & Engineering (CSE)</strong></dd></div>' +
      '<div class="kv"><dt>Programme</dt><dd><strong>B.Tech in Computer Science & Engineering</strong></dd></div>' +
      "</dl></div>" +
      "</div>" +
      '<div id="modal-container"></div>'
    );
  }

  /* =====================================================================
   * 5. TIMETABLE MANAGEMENT
   * ===================================================================== */
  function timetablePage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Timetable Management" }],
        "Timetable Management",
        "Interactive timetable editor with section-wise weekly calendar grid and live session scheduling.",
        '<a href="#/clash-detection" class="btn btn-primary" id="btn-run-clash-tt">' + I.svg("play", 16) + ' RUN CLASH DETECTION</a>' +
        '<button type="button" class="btn btn-secondary" id="btn-add-session">' + I.svg("plus", 16) + ' Add Session</button>' +
        '<button type="button" class="btn btn-secondary" id="btn-publish-draft">' + I.svg("published", 16) + ' Publish Timetable</button>'
      ) +
      '<div class="module-toolbar">' +
      '<div class="search-filters">' +
      '<div class="action-group">' +
      '<button type="button" class="btn btn-primary btn-sm" id="view-toggle-grid">Weekly Grid</button>' +
      '<button type="button" class="btn btn-secondary btn-sm" id="view-toggle-table">Table View</button>' +
      "</div>" +
      '<select id="tt-section-select" class="select-filter" style="font-weight:600;"><option value="A">Section A</option></select>' +
      '<select id="tt-day-filter" class="select-filter"><option value="">All Days</option><option value="Monday">Monday</option><option value="Tuesday">Tuesday</option><option value="Wednesday">Wednesday</option><option value="Thursday">Thursday</option><option value="Friday">Friday</option><option value="Saturday">Saturday</option></select>' +
      '<input type="text" id="tt-search" class="input search-input" placeholder="Search subject or faculty...">' +
      "</div>" +
      '<div class="action-group">' +
      '<span class="badge badge-warning" id="draft-pill">Draft Modifications Active</span>' +
      '<span class="badge badge-info" id="session-count-pill">714 Sessions</span>' +
      "</div>" +
      "</div>" +
      '<div id="timetable-grid-container"></div>' +
      '<div id="timetable-table-container" hidden>' +
      '<div class="table-container"><table class="data-table" id="tt-table"><thead><tr><th>Section</th><th>Day</th><th>Time</th><th>Subject</th><th>Faculty</th><th>Room</th><th>Students</th><th>Actions</th></tr></thead>' +
      '<tbody id="tt-table-body"></tbody></table></div>' +
      "</div>" +
      '<div id="modal-container"></div>'
    );
  }

  /* =====================================================================
   * 6. CLASH DETECTION & RESOLUTION (REAL C++ ENGINE)
   * ===================================================================== */
  function clashDetectionPage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Clash Detection" }],
        "Clash Detection & Resolution Engine",
        "Direct integration with the university C++ detection engine and conflict resolution resolver.",
        '<div style="display:flex; gap:10px;">' +
        '<button type="button" class="btn btn-secondary" id="btn-batch-resolve" style="background:#2563eb; color:#fff; border-color:#1d4ed8;">' +
        I.svg("check", 16) + ' AUTO-RESOLVE ALL</button>' +
        '<button type="button" class="btn btn-primary" id="btn-exec-clash-detection" style="background:#dc2626; border-color:#b91c1c;">' +
        I.svg("play", 16) + ' RUN CLASH DETECTION</button>' +
        '</div>'
      ) +
      '<div class="banner" style="background:#fef2f2; border-left-color:#ef4444;">' +
      I.svg("alert", 20) +
      "<div><strong>Native C++ Engine Integration.</strong> Clicking 'Run Clash Detection' invokes the compiled C++ executable (<code>PBL/bin/engine_cli</code>), executes interval sweep-line analysis, detects room capacity shortfalls, and prepares automated graph-coloring resolutions.</div>" +
      "</div>" +
      '<div class="kpi-grid" style="grid-template-columns: repeat(7, 1fr); margin-bottom:20px;">' +
      '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Exact Incidents</div><div class="kpi-value" id="kpi-incidents-clashes" style="color:#0284c7;">--</div><div class="kpi-desc">Consolidated conflicts</div></div></article>' +
      '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Total Clashes</div><div class="kpi-value" id="kpi-total-clashes" style="color:#dc2626;">--</div><div class="kpi-desc">Across all sessions</div></div></article>' +
      '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Faculty Clashes</div><div class="kpi-value" id="kpi-fac-clashes">--</div><div class="kpi-desc">Overlapping schedules</div></div></article>' +
      '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Room Clashes</div><div class="kpi-value" id="kpi-room-clashes">--</div><div class="kpi-desc">Double booked venues</div></div></article>' +
      '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Section Clashes</div><div class="kpi-value" id="kpi-sec-clashes">--</div><div class="kpi-desc">Overlapping classes</div></div></article>' +
      '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Capacity Clashes</div><div class="kpi-value" id="kpi-cap-clashes">--</div><div class="kpi-desc">Cohort exceeds room limit</div></div></article>' +
      '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Conflict Rate</div><div class="kpi-value" id="kpi-density-clashes" style="color:#7c3aed;">--</div><div class="kpi-desc">Session conflict density</div></div></article>' +
      "</div>" +
      '<div class="module-toolbar">' +
      '<div class="search-filters">' +
      '<button type="button" class="btn btn-primary btn-sm clash-filter-btn" data-type="ALL">All Categories</button>' +
      '<button type="button" class="btn btn-secondary btn-sm clash-filter-btn" data-type="Faculty">Faculty</button>' +
      '<button type="button" class="btn btn-secondary btn-sm clash-filter-btn" data-type="Room">Room</button>' +
      '<button type="button" class="btn btn-secondary btn-sm clash-filter-btn" data-type="Section">Section</button>' +
      '<button type="button" class="btn btn-secondary btn-sm clash-filter-btn" data-type="Room capacity">Room Capacity</button>' +
      '<button type="button" class="btn btn-secondary btn-sm clash-filter-btn" data-type="Duplicate session">Duplicates</button>' +
      "</div>" +
      '<div style="display:flex; gap:8px; align-items:center;">' +
      '<a href="' + API.getClashesExportUrl("csv") + '" class="btn btn-secondary btn-sm" target="_blank" download="timetable_conflicts.csv">' + I.svg("download", 14) + ' Export CSV</a>' +
      '<a href="' + API.getClashesExportUrl("md") + '" class="btn btn-secondary btn-sm" target="_blank" download="timetable_conflicts.md">' + I.svg("download", 14) + ' Markdown</a>' +
      '<div id="engine-status-badge" class="badge badge-success">Engine Ready</div>' +
      "</div>" +
      "</div>" +
      '<div id="clashes-list-container"><div style="text-align:center; padding:40px; background:#fff; border-radius:8px; border:1px solid #e2e8f0;">' +
      I.svg("alert", 32) +
      '<h3 style="margin:12px 0 6px 0;">Run Clash Detection</h3><p class="muted">Click "RUN CLASH DETECTION" above to execute the C++ engine on the current draft timetable.</p></div></div>' +
      '<div id="modal-container"></div>'
    );
  }

  /* =====================================================================
   * 7. IMPORT / EXPORT
   * ===================================================================== */
  function importExportPage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Import / Export" }],
        "Import / Export",
        "Central data provisioning hub with file validation, faculty mapping checks, and CSV downloads."
      ) +
      '<div class="tabs">' +
      '<button type="button" class="tab-btn active" data-tab="tab-imp-timetable">Timetable (sessions.csv)</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-imp-students">Students (students.csv)</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-imp-faculty">Faculty (faculty_master.csv)</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-exports">Direct Exports</button>' +
      "</div>" +
      '<div id="tab-imp-timetable" class="tab-content">' +
      '<div class="card" style="margin-bottom:20px;">' +
      "<h3>Upload Timetable (CSV or HTML Timetable Grid)</h3>" +
      '<p class="muted">Supports official university HTML timetables (e.g. <code>3sem.html</code>, <code>5sem.html</code>) or CSV format (<code>Section, Day, Start_Time, End_Time, Subject, Faculty, Room, Student_Count</code>). The native C++ engine automatically extracts all section sessions.</p>' +
      '<div class="dropzone" id="tt-dropzone">' +
      I.svg("upload", 32) +
      '<p style="margin:8px 0 4px 0; font-weight:600;">Select or drop sessions.csv, 3sem.html or 5sem.html</p>' +
      '<input type="file" id="tt-file-input" accept=".csv,.html,.htm" style="display:none;">' +
      '<button type="button" class="btn btn-secondary btn-sm" onclick="document.getElementById(\'tt-file-input\').click()">Browse file</button>' +
      "</div>" +
      '<div id="tt-import-preview-box" hidden style="margin-top:16px;"></div>' +
      "</div>" +
      "</div>" +
      '<div id="tab-imp-students" class="tab-content" hidden>' +
      '<div class="card">' +
      "<h3>Upload Student Directory CSV</h3>" +
      '<p class="muted">Expected headers: <code>Student_ID, Name, Email, Section, Group</code>. Existing records are safely updated without silent deletion.</p>' +
      '<div class="dropzone" id="stu-dropzone">' +
      I.svg("upload", 32) +
      '<p style="margin:8px 0 4px 0; font-weight:600;">Select students.csv</p>' +
      '<input type="file" id="stu-file-input" accept=".csv" style="display:none;">' +
      '<button type="button" class="btn btn-secondary btn-sm" onclick="document.getElementById(\'stu-file-input\').click()">Browse file</button>' +
      "</div>" +
      '<div id="stu-import-preview-box" hidden style="margin-top:16px;"></div>' +
      "</div>" +
      "</div>" +
      '<div id="tab-imp-faculty" class="tab-content" hidden>' +
      '<div class="card">' +
      "<h3>Upload Faculty Master CSV</h3>" +
      '<p class="muted">Expected headers: <code>faculty_id, faculty_name, department, designation, email, phone, office_location</code>.</p>' +
      '<div class="dropzone" id="fac-dropzone">' +
      I.svg("upload", 32) +
      '<p style="margin:8px 0 4px 0; font-weight:600;">Select faculty_master.csv</p>' +
      '<input type="file" id="fac-file-input" accept=".csv" style="display:none;">' +
      '<button type="button" class="btn btn-secondary btn-sm" onclick="document.getElementById(\'fac-file-input\').click()">Browse file</button>' +
      "</div>" +
      '<div id="fac-import-preview-box" hidden style="margin-top:16px;"></div>' +
      "</div>" +
      "</div>" +
      '<div id="tab-exports" class="tab-content" hidden>' +
      '<div class="kpi-grid" style="grid-template-columns: repeat(4, 1fr);">' +
      '<div class="card"><h4>Timetable CSV</h4><p class="muted">Export the current working draft timetable.</p><a href="' + API.getTimetableExportUrl() + '" class="btn btn-primary btn-block">' + I.svg("download", 16) + ' Download Timetable CSV</a></div>' +
      '<div class="card"><h4>Students CSV</h4><p class="muted">Export all registered student records.</p><a href="' + API.getStudentsExportUrl() + '" class="btn btn-primary btn-block">' + I.svg("download", 16) + ' Download Students CSV</a></div>' +
      '<div class="card"><h4>Faculty Master CSV</h4><p class="muted">Export full faculty directory with office locations.</p><a href="' + API.getFacultyExportUrl() + '" class="btn btn-primary btn-block">' + I.svg("download", 16) + ' Download Faculty CSV</a></div>' +
      '<div class="card"><h4>Conflict Audit Report</h4><p class="muted">Download latest C++ clash analysis report.</p><div style="display:flex; gap:6px;"><a href="' + API.getClashesExportUrl("csv") + '" class="btn btn-secondary" style="flex:1;">CSV</a><a href="' + API.getClashesExportUrl("md") + '" class="btn btn-secondary" style="flex:1;">Markdown</a></div></div>' +
      "</div>" +
      "</div>"
    );
  }

  /* =====================================================================
   * 8. PUBLISHED TIMETABLES & VERSION HISTORY
   * ===================================================================== */
  function publishedPage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Published Timetables" }],
        "Published Timetables & Version History",
        "Authoritative published versions consumed by the Student Portal and Faculty Portal.",
        '<button type="button" class="btn btn-primary" id="btn-publish-new">' + I.svg("published", 16) + ' Publish Current Draft</button>'
      ) +
      '<div class="card" style="margin-bottom:20px; border-left:4px solid #16a34a;" id="active-published-box">' +
      '<div class="card-head"><h2>Currently Active Published Version</h2><span class="badge badge-success">ACTIVE</span></div>' +
      '<div id="active-published-details"><p class="muted">Loading published timetable status...</p></div>' +
      "</div>" +
      '<div style="margin-bottom:12px;"><h3>Version History & Audit Log</h3></div>' +
      '<div class="table-container"><table class="data-table"><thead><tr><th>Version</th><th>Status</th><th>Published By</th><th>Date Published</th><th>Sessions</th><th>Notes</th><th>Actions</th></tr></thead>' +
      '<tbody id="versions-table-body"><tr><td colspan="7" style="text-align:center; padding:16px;">Loading versions...</td></tr></tbody></table></div>' +
      '<div class="card" style="background:#f8fafc; margin-top:24px;">' +
      '<h4>Shared Portal Integration Endpoints</h4>' +
      '<p class="muted" style="font-size:13px;">The published schedule is exposed through dedicated read-only endpoints for external portals:</p>' +
      '<ul style="font-size:13px; line-height:1.6; margin:8px 0 0 16px;">' +
      '<li><strong>Student Portal Timetable:</strong> <code>GET /api/portal/student/timetable?section={Section}</code></li>' +
      '<li><strong>Faculty Teaching Schedule:</strong> <code>GET /api/portal/faculty/schedule?faculty_id={FacultyID}</code></li>' +
      '<li><strong>Campus Connect Directory:</strong> <code>GET /api/portal/faculty/directory</code></li>' +
      "</ul></div>" +
      '<div id="modal-container"></div>'
    );
  }

  /* =====================================================================
   * 9. ACTIVITY LOGS
   * ===================================================================== */
  function activityLogsPage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Activity Logs" }],
        "Administrative Audit Trail",
        "Immutable audit log tracking logins, CSV imports, timetable edits, clash runs, and publications.",
        '<button type="button" class="btn btn-secondary" id="btn-refresh-logs">' + I.svg("refresh", 16) + ' Refresh Logs</button>'
      ) +
      '<div class="module-toolbar">' +
      '<div class="search-filters">' +
      '<input type="text" id="log-search" class="input search-input" placeholder="Search by user or action...">' +
      '<select id="log-action-filter" class="select-filter"><option value="">All Actions</option><option value="LOGIN">Logins</option><option value="CLASH">Clash Detection</option><option value="RESOLUTION">Resolutions</option><option value="PUBLISHED">Publications</option><option value="IMPORTED">Imports</option></select>' +
      "</div>" +
      '<div id="logs-count-badge" class="badge badge-info">Audit Trail Active</div>' +
      "</div>" +
      '<div class="table-container"><table class="data-table"><thead><tr><th>Timestamp</th><th>Administrator</th><th>Action</th><th>Target Entity</th><th>Details</th><th>IP Address</th></tr></thead>' +
      '<tbody id="logs-table-body"><tr><td colspan="6" style="text-align:center; padding:20px;">Loading audit logs...</td></tr></tbody></table></div>'
    );
  }

  /* =====================================================================
   * 10. SETTINGS
   * ===================================================================== */
  function settingsPage() {
    return (
      pageHeader(
        [{ label: "Admin Portal", href: "#/dashboard" }, { label: "Settings" }],
        "System Settings & Accounts",
        "Configure university parameters, scheduling policies, and administrator credentials."
      ) +
      '<div class="tabs">' +
      '<button type="button" class="tab-btn active" data-tab="tab-set-univ">University & Academic</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-set-rules">Timetable Rules</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-set-admins">Administrator Accounts</button>' +
      '<button type="button" class="tab-btn" data-tab="tab-set-diag">Engine Diagnostics</button>' +
      "</div>" +
      '<div id="tab-set-univ" class="tab-content">' +
      '<form id="form-settings-univ" class="card" style="max-width:650px;">' +
      '<div class="field"><label>University Name</label><input type="text" id="set-univ-name" class="input"></div>' +
      '<div class="field"><label>Campus Location</label><input type="text" id="set-campus" class="input"></div>' +
      '<div class="field"><label>Academic Year</label><input type="text" id="set-acad-year" class="input"></div>' +
      '<div class="field"><label>Current Semester</label><input type="text" id="set-semester" class="input"></div>' +
      '<button type="submit" class="btn btn-primary">Save Academic Settings</button>' +
      "</form>" +
      "</div>" +
      '<div id="tab-set-rules" class="tab-content" hidden>' +
      '<form id="form-settings-rules" class="card" style="max-width:650px;">' +
      '<div class="field"><label>Working Days (comma separated)</label><input type="text" id="set-working-days" class="input"></div>' +
      '<div class="field"><label>Standard Slot Duration</label><input type="text" id="set-slot-duration" class="input"></div>' +
      '<div class="field"><label>Clash Detection Publishing Policy</label>' +
      '<select id="set-clash-policy" class="input"><option value="STRICT">Strict (Block publish if high severity clashes remain)</option><option value="WARN">Warning only (Allow administrator publish with override)</option></select></div>' +
      '<button type="submit" class="btn btn-primary">Save Scheduling Rules</button>' +
      "</form>" +
      "</div>" +
      '<div id="tab-set-admins" class="tab-content" hidden>' +
      '<div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:14px;">' +
      "<div><strong>Configured Administrators (from admin_master.csv)</strong></div>" +
      '<button type="button" class="btn btn-primary btn-sm" id="btn-add-admin-modal">' + I.svg("plus", 14) + ' Provision Admin</button>' +
      "</div>" +
      '<div class="table-container"><table class="data-table"><thead><tr><th>Admin ID</th><th>Name</th><th>Email</th><th>Role</th><th>Status</th><th>Last Login</th></tr></thead>' +
      '<tbody id="admins-table-body"></tbody></table></div>' +
      "</div>" +
      '<div id="tab-set-diag" class="tab-content" hidden>' +
      '<div class="card" style="max-width:700px;">' +
      '<h3>System & Integration Health</h3><dl class="kv-list">' +
      '<div class="kv"><dt>C++ Engine Binary</dt><dd><code>PBL/bin/engine_cli</code> (Compiled Apple Clang C++17)</dd></div>' +
      '<div class="kv"><dt>Database Mode</dt><dd>Node.js SQLite WAL (<code>database/timetable.sqlite</code>)</dd></div>' +
      '<div class="kv"><dt>Backend Server</dt><dd>Node.js Built-in HTTP REST API (Port 3000)</dd></div>' +
      '<div class="kv"><dt>Resolver Mode</dt><dd>Greedy Graph-Coloring & Room/Time Slot Reassignment</dd></div>' +
      "</dl></div>" +
      "</div>" +
      '<div id="modal-container"></div>'
    );
  }

  // Generic placeholder for unmapped pages
  function placeholder(m) {
    return (
      pageHeader([{ label: "Admin Portal", href: "#/dashboard" }, { label: m.label }], m.label, m.purpose) +
      '<div class="card"><p class="muted">This module is under construction.</p></div>'
    );
  }

  window.AdminPages = {
    esc: esc,
    value: value,
    pageHeader: pageHeader,
    dashboard: dashboard,
    students: studentsPage,
    faculty: facultyPage,
    "academic-structure": academicStructurePage,
    timetable: timetablePage,
    "clash-detection": clashDetectionPage,
    "import-export": importExportPage,
    published: publishedPage,
    "activity-logs": activityLogsPage,
    settings: settingsPage,
    placeholder: placeholder
  };
})();
