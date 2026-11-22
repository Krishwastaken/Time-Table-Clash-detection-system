/*
 * Interactive Handlers and Event Controllers for Admin Portal Modules
 */
(function (root) {
  "use strict";

  var API = root.AdminAPI;
  var I = root.Icons;
  var P = root.AdminPages;
  var D = root.AdminData;
  var esc = P.esc;

  function $(sel, parent) {
    return (parent || document).querySelector(sel);
  }
  function $$(sel, parent) {
    return Array.from((parent || document).querySelectorAll(sel));
  }

  function showModal(html) {
    var c = $("#modal-container");
    if (!c) {
      c = document.createElement("div");
      c.id = "modal-container";
      document.body.appendChild(c);
    }
    c.innerHTML = '<div class="modal-overlay" id="active-modal-overlay">' + html + '</div>';

    var closeBtn = $(".modal-close-btn", c);
    if (closeBtn) closeBtn.onclick = closeModal;
    var cancelBtn = $(".modal-cancel-btn", c);
    if (cancelBtn) cancelBtn.onclick = closeModal;

    c.onclick = function (e) {
      if (e.target.id === "active-modal-overlay") closeModal();
    };
  }

  function closeModal() {
    var c = $("#modal-container");
    if (c) c.innerHTML = "";
  }

  function toast(msg, isError) {
    var box = document.createElement("div");
    box.className = "toast " + (isError ? "toast-error" : "toast-success");
    box.style.cssText =
      "position:fixed; bottom:24px; right:24px; background:" +
      (isError ? "#dc2626" : "#15803d") +
      "; color:#fff; padding:12px 20px; border-radius:8px; font-weight:600; font-size:14px; box-shadow:0 4px 12px rgba(0,0,0,0.15); z-index:9999;";
    box.textContent = msg;
    document.body.appendChild(box);
    setTimeout(function () {
      box.remove();
    }, 3500);
  }

  /* ---------------- Tabs ---------------- */
  function initTabs(container) {
    $$(".tab-btn", container).forEach(function (btn) {
      btn.onclick = function () {
        var targetId = this.dataset.tab;
        $$(".tab-btn", container).forEach(function (b) { b.classList.remove("active"); });
        $$(".tab-content", container).forEach(function (tc) { tc.hidden = true; });
        this.classList.add("active");
        var target = $("#" + targetId, container);
        if (target) target.hidden = false;
      };
    });
  }

  /* =====================================================================
   * 1. DASHBOARD CONTROLLER
   * ===================================================================== */
  function initDashboard() {
    D.loadDashboardData().then(function (data) {
      if (!data) return;
      var curHash = (location.hash || "").replace(/^#\/?/, "");
      if (curHash === "dashboard" || !curHash) {
        $("#page").innerHTML = P.dashboard();
        initDashboardEvents();
      }
    });
  }

  function initDashboardEvents() {
    // any quick action bindings
  }

  /* =====================================================================
   * 2. STUDENTS CONTROLLER
   * ===================================================================== */
  function initStudents(container) {
    var page = 1;
    var searchTimer = null;

    function loadStudents() {
      var q = $("#student-search", container)?.value || "";
      var sec = $("#student-section-filter", container)?.value || "";
      var status = $("#student-status-filter", container)?.value || "";

      API.getStudents({ q: q, section: sec, status: status, page: page, limit: 25 })
        .then(function (res) {
          renderStudentsTable(res);
        })
        .catch(function (err) {
          $("#students-table-body", container).innerHTML =
            '<tr><td colspan="7" style="color:#dc2626; text-align:center; padding:20px;">' +
            esc(err.message) +
            "</td></tr>";
        });
    }

    function renderStudentsTable(data) {
      var tbody = $("#students-table-body", container);
      var countBadge = $("#student-count-badge", container);
      if (!tbody) return;

      var students = data.students || [];
      var p = data.pagination || {};

      if (countBadge) {
        countBadge.textContent = p.total + " Students Enrolled";
      }

      if (students.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; padding:24px;" class="muted">No student records matching your filters.</td></tr>';
        return;
      }

      tbody.innerHTML = students
        .map(function (s) {
          return (
            "<tr>" +
            "<td><code>" +
            esc(s.student_id) +
            "</code></td>" +
            "<td><strong>" +
            esc(s.name) +
            "</strong></td>" +
            "<td>" +
            esc(s.email || "--") +
            "</td>" +
            '<td><span class="badge badge-info">' +
            esc(s.section_id || "--") +
            "</span></td>" +
            "<td>" +
            esc(s.group_name || "--") +
            "</td>" +
            '<td><span class="badge ' +
            (s.status === "ACTIVE" ? "badge-success" : "badge-muted") +
            '">' +
            esc(s.status) +
            "</span></td>" +
            '<td><div class="action-group">' +
            '<button type="button" class="btn btn-secondary btn-sm btn-edit-student" data-id="' +
            esc(s.student_id) +
            '">Edit</button>' +
            '<button type="button" class="btn btn-ghost btn-sm btn-del-student" data-id="' +
            esc(s.student_id) +
            '" style="color:#dc2626;">Deactivate</button>' +
            "</div></td>" +
            "</tr>"
          );
        })
        .join("");

      // Pagination
      var pag = $("#student-pagination", container);
      if (pag) {
        pag.hidden = p.totalPages <= 1;
        $("#student-pagination-info", container).textContent =
          "Showing " + students.length + " of " + p.total + " records (Page " + p.page + " of " + p.totalPages + ")";
        $("#student-page-number", container).textContent = p.page;
        $("#btn-student-prev", container).disabled = p.page <= 1;
        $("#btn-student-next", container).disabled = p.page >= p.totalPages;
      }

      // Attach row edit/delete actions
      $$(".btn-edit-student", tbody).forEach(function (btn) {
        btn.onclick = function () {
          var sid = this.dataset.id;
          var stu = students.find(function (x) { return x.student_id === sid; });
          if (stu) openEditStudentModal(stu);
        };
      });

      $$(".btn-del-student", tbody).forEach(function (btn) {
        btn.onclick = function () {
          var sid = this.dataset.id;
          if (confirm("Are you sure you want to deactivate student " + sid + "?")) {
            API.deleteStudent(sid, false).then(function () {
              toast("Student " + sid + " deactivated.");
              loadStudents();
            }).catch(function (e) { toast(e.message, true); });
          }
        };
      });
    }

    // Populate section filter dropdown
    API.getAcademicStructure().then(function (res) {
      var sel = $("#student-section-filter", container);
      if (!sel || !res.structure) return;
      (res.structure.sections || []).forEach(function (s) {
        var opt = document.createElement("option");
        opt.value = s.name;
        opt.textContent = "Section " + s.name;
        sel.appendChild(opt);
      });
    }).catch(function () {});

    // Bind filters
    var searchInput = $("#student-search", container);
    if (searchInput) {
      searchInput.oninput = function () {
        clearTimeout(searchTimer);
        searchTimer = setTimeout(function () {
          page = 1;
          loadStudents();
        }, 300);
      };
    }

    var secFilter = $("#student-section-filter", container);
    if (secFilter) {
      secFilter.onchange = function () { page = 1; loadStudents(); };
    }

    var statusFilter = $("#student-status-filter", container);
    if (statusFilter) {
      statusFilter.onchange = function () { page = 1; loadStudents(); };
    }

    $("#btn-student-prev", container).onclick = function () {
      if (page > 1) { page--; loadStudents(); }
    };
    $("#btn-student-next", container).onclick = function () {
      page++; loadStudents();
    };

    // Add Student modal button
    $("#btn-add-student", container).onclick = function () {
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Add Student Record</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-add-student"><div class="modal-body">' +
        '<div class="field"><label>Student ID *</label><input type="text" id="add-stu-id" class="input" placeholder="e.g. GEU-2026-1050" required></div>' +
        '<div class="field"><label>Full Name *</label><input type="text" id="add-stu-name" class="input" required></div>' +
        '<div class="field"><label>Email</label><input type="email" id="add-stu-email" class="input"></div>' +
        '<div class="field"><label>Section</label><input type="text" id="add-stu-section" class="input" placeholder="e.g. A, B, AI-DS1"></div>' +
        '<div class="field"><label>Group</label><input type="text" id="add-stu-group" class="input" placeholder="e.g. G1 or G2"></div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">Save Student</button></div>' +
        '</form></div>'
      );

      $("#form-add-student").onsubmit = function (e) {
        e.preventDefault();
        var payload = {
          studentId: $("#add-stu-id").value,
          name: $("#add-stu-name").value,
          email: $("#add-stu-email").value,
          section: $("#add-stu-section").value,
          group: $("#add-stu-group").value
        };
        API.createStudent(payload).then(function () {
          closeModal();
          toast("Student created successfully.");
          loadStudents();
        }).catch(function (err) { toast(err.message, true); });
      };
    };

    function openEditStudentModal(stu) {
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Edit Student: ' + esc(stu.student_id) + '</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-edit-student"><div class="modal-body">' +
        '<div class="field"><label>Full Name *</label><input type="text" id="edit-stu-name" class="input" value="' + esc(stu.name) + '" required></div>' +
        '<div class="field"><label>Email</label><input type="email" id="edit-stu-email" class="input" value="' + esc(stu.email || "") + '"></div>' +
        '<div class="field"><label>Section</label><input type="text" id="edit-stu-section" class="input" value="' + esc(stu.section_id || "") + '"></div>' +
        '<div class="field"><label>Group</label><input type="text" id="edit-stu-group" class="input" value="' + esc(stu.group_name || "") + '"></div>' +
        '<div class="field"><label>Status</label><select id="edit-stu-status" class="input"><option value="ACTIVE" ' + (stu.status === "ACTIVE" ? "selected" : "") + '>Active</option><option value="INACTIVE" ' + (stu.status === "INACTIVE" ? "selected" : "") + '>Inactive</option></select></div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">Update Details</button></div>' +
        '</form></div>'
      );

      $("#form-edit-student").onsubmit = function (e) {
        e.preventDefault();
        var payload = {
          name: $("#edit-stu-name").value,
          email: $("#edit-stu-email").value,
          section: $("#edit-stu-section").value,
          group: $("#edit-stu-group").value,
          status: $("#edit-stu-status").value
        };
        API.updateStudent(stu.student_id, payload).then(function () {
          closeModal();
          toast("Student updated.");
          loadStudents();
        }).catch(function (err) { toast(err.message, true); });
      };
    }

    // Modal for CSV import
    $("#btn-import-students-modal", container).onclick = function () {
      location.hash = "#/import-export";
    };

    loadStudents();
  }

  /* =====================================================================
   * 3. FACULTY CONTROLLER
   * ===================================================================== */
  function initFaculty(container) {
    var page = 1;
    var searchTimer = null;

    function loadFaculty() {
      var q = $("#faculty-search", container)?.value || "";
      var dept = $("#faculty-dept-filter", container)?.value || "";
      var status = $("#faculty-status-filter", container)?.value || "";

      API.getFaculty({ q: q, department: dept, status: status, page: page, limit: 25 })
        .then(function (res) {
          renderFacultyTable(res);
        })
        .catch(function (err) {
          $("#faculty-table-body", container).innerHTML =
            '<tr><td colspan="8" style="color:#dc2626; text-align:center; padding:20px;">' +
            esc(err.message) +
            "</td></tr>";
        });
    }

    function renderFacultyTable(data) {
      var tbody = $("#faculty-table-body", container);
      var countBadge = $("#faculty-count-badge", container);
      if (!tbody) return;

      var facultyList = data.faculty || [];
      var p = data.pagination || {};

      if (countBadge) countBadge.textContent = p.total + " Master Records";

      if (facultyList.length === 0) {
        tbody.innerHTML = '<tr><td colspan="8" style="text-align:center; padding:24px;" class="muted">No faculty members found.</td></tr>';
        return;
      }

      tbody.innerHTML = facultyList
        .map(function (f) {
          return (
            "<tr>" +
            "<td><code>" +
            esc(f.faculty_id) +
            "</code></td>" +
            "<td><strong>" +
            esc(f.faculty_name) +
            "</strong></td>" +
            "<td>" +
            esc(f.department || "--") +
            "</td>" +
            "<td>" +
            esc(f.designation || "--") +
            "</td>" +
            "<td>" +
            esc(f.email || "--") +
            "</td>" +
            "<td>" +
            (f.office_location ? '<strong>' + esc(f.office_location) + '</strong>' : '<span class="muted" style="font-style:italic;">Unassigned</span>') +
            "</td>" +
            '<td><span class="badge ' +
            (f.status === "ACTIVE" ? "badge-success" : "badge-muted") +
            '">' +
            esc(f.status) +
            "</span></td>" +
            '<td><div class="action-group">' +
            '<button type="button" class="btn btn-secondary btn-sm btn-edit-faculty" data-id="' +
            esc(f.faculty_id) +
            '">Edit</button>' +
            '<button type="button" class="btn btn-ghost btn-sm btn-del-faculty" data-id="' +
            esc(f.faculty_id) +
            '" style="color:#dc2626;">Deactivate</button>' +
            "</div></td>" +
            "</tr>"
          );
        })
        .join("");

      var pag = $("#faculty-pagination", container);
      if (pag) {
        pag.hidden = p.totalPages <= 1;
        $("#faculty-pagination-info", container).textContent =
          "Showing " + facultyList.length + " of " + p.total + " records (Page " + p.page + " of " + p.totalPages + ")";
        $("#faculty-page-number", container).textContent = p.page;
        $("#btn-faculty-prev", container).disabled = p.page <= 1;
        $("#btn-faculty-next", container).disabled = p.page >= p.totalPages;
      }

      $$(".btn-edit-faculty", tbody).forEach(function (btn) {
        btn.onclick = function () {
          var fid = this.dataset.id;
          var f = facultyList.find(function (x) { return x.faculty_id === fid; });
          if (f) openEditFacultyModal(f);
        };
      });

      $$(".btn-del-faculty", tbody).forEach(function (btn) {
        btn.onclick = function () {
          var fid = this.dataset.id;
          if (confirm("Deactivate faculty member " + fid + "?")) {
            API.deleteFaculty(fid, false).then(function () {
              toast("Faculty deactivated.");
              loadFaculty();
            }).catch(function (e) { toast(e.message, true); });
          }
        };
      });
    }

    function openEditFacultyModal(f) {
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Edit Faculty: ' + esc(f.faculty_id) + '</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-edit-faculty"><div class="modal-body">' +
        '<div class="field"><label>Faculty Name *</label><input type="text" id="edit-fac-name" class="input" value="' + esc(f.faculty_name) + '" required></div>' +
        '<div class="field"><label>Department</label><input type="text" id="edit-fac-dept" class="input" value="' + esc(f.department || "") + '"></div>' +
        '<div class="field"><label>Designation</label><input type="text" id="edit-fac-desig" class="input" value="' + esc(f.designation || "") + '"></div>' +
        '<div class="field"><label>Email</label><input type="email" id="edit-fac-email" class="input" value="' + esc(f.email || "") + '"></div>' +
        '<div class="field"><label>Phone</label><input type="text" id="edit-fac-phone" class="input" value="' + esc(f.phone || "") + '"></div>' +
        '<div class="field"><label>Office Location (Blank if unassigned)</label><input type="text" id="edit-fac-office" class="input" value="' + esc(f.office_location || "") + '" placeholder="e.g. CS Block Room 304"></div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">Save Changes</button></div>' +
        '</form></div>'
      );

      $("#form-edit-faculty").onsubmit = function (e) {
        e.preventDefault();
        var payload = {
          facultyName: $("#edit-fac-name").value,
          department: $("#edit-fac-dept").value,
          designation: $("#edit-fac-desig").value,
          email: $("#edit-fac-email").value,
          phone: $("#edit-fac-phone").value,
          officeLocation: $("#edit-fac-office").value
        };
        API.updateFaculty(f.faculty_id, payload).then(function () {
          closeModal();
          toast("Faculty details updated.");
          loadFaculty();
        }).catch(function (err) { toast(err.message, true); });
      };
    }

    $("#btn-add-faculty", container).onclick = function () {
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Add Faculty Member</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-add-faculty"><div class="modal-body">' +
        '<div class="field"><label>Faculty ID *</label><input type="text" id="add-fac-id" class="input" placeholder="e.g. F076" required></div>' +
        '<div class="field"><label>Faculty Name *</label><input type="text" id="add-fac-name" class="input" required></div>' +
        '<div class="field"><label>Department</label><input type="text" id="add-fac-dept" class="input" value="Computer Science & Engineering"></div>' +
        '<div class="field"><label>Designation</label><input type="text" id="add-fac-desig" class="input" value="Assistant Professor"></div>' +
        '<div class="field"><label>Email</label><input type="email" id="add-fac-email" class="input"></div>' +
        '<div class="field"><label>Office Location</label><input type="text" id="add-fac-office" class="input" placeholder="Optional"></div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">Create Faculty</button></div>' +
        '</form></div>'
      );

      $("#form-add-faculty").onsubmit = function (e) {
        e.preventDefault();
        var payload = {
          facultyId: $("#add-fac-id").value,
          facultyName: $("#add-fac-name").value,
          department: $("#add-fac-dept").value,
          designation: $("#add-fac-desig").value,
          email: $("#add-fac-email").value,
          officeLocation: $("#add-fac-office").value
        };
        API.createFaculty(payload).then(function () {
          closeModal();
          toast("Faculty created.");
          loadFaculty();
        }).catch(function (err) { toast(err.message, true); });
      };
    };

    var searchInput = $("#faculty-search", container);
    if (searchInput) {
      searchInput.oninput = function () {
        clearTimeout(searchTimer);
        searchTimer = setTimeout(function () { page = 1; loadFaculty(); }, 300);
      };
    }

    $("#btn-faculty-prev", container).onclick = function () {
      if (page > 1) { page--; loadFaculty(); }
    };
    $("#btn-faculty-next", container).onclick = function () {
      page++; loadFaculty();
    };

    $("#btn-import-faculty-modal", container).onclick = function () {
      location.hash = "#/import-export";
    };

    loadFaculty();
  }

  /* =====================================================================
   * 4. ACADEMIC STRUCTURE CONTROLLER
   * ===================================================================== */
  function initAcademicStructure(container) {
    initTabs(container);

    function loadStructure() {
      API.getAcademicStructure().then(function (res) {
        var struct = res.structure || {};

        // Rooms
        var roomsTbody = $("#rooms-table-body", container);
        if (roomsTbody) {
          roomsTbody.innerHTML = (struct.rooms || []).map(function (r) {
            return (
              "<tr>" +
              "<td><strong>" + esc(r.name) + "</strong></td>" +
              "<td>" + esc(r.room_type) + "</td>" +
              '<td><span class="badge ' + (r.capacity >= 200 ? 'badge-success' : (r.capacity >= 100 ? 'badge-info' : 'badge-warning')) + '" style="font-size:13px;">' + r.capacity + ' seats</span></td>' +
              "<td>" + esc(r.building || "--") + " (" + esc(r.floor || "") + ")</td>" +
              '<td><button type="button" class="btn btn-secondary btn-sm btn-edit-room" data-id="' + esc(r.room_id) + '" data-name="' + esc(r.name) + '" data-cap="' + r.capacity + '">Edit Capacity</button></td>' +
              "</tr>"
            );
          }).join("");

          $$(".btn-edit-room", roomsTbody).forEach(function (btn) {
            btn.onclick = function () {
              var rid = this.dataset.id;
              var rname = this.dataset.name;
              var rcap = this.dataset.cap;
              var newCap = prompt("Enter new capacity for " + rname + ":", rcap);
              if (newCap && !isNaN(parseInt(newCap, 10))) {
                API.updateRoom(rid, { capacity: parseInt(newCap, 10) }).then(function () {
                  toast("Room " + rname + " capacity updated to " + newCap);
                  loadStructure();
                }).catch(function (e) { toast(e.message, true); });
              }
            };
          });
        }

        // Sections
        var secTbody = $("#sections-table-body", container);
        if (secTbody) {
          secTbody.innerHTML = (struct.sections || []).map(function (s) {
            return (
              "<tr>" +
              "<td><strong>Section " + esc(s.name) + "</strong></td>" +
              "<td>" + (s.student_count || 60) + " Students</td>" +
              "<td>Semester 3</td>" +
              "<td>G1 (30), G2 (30)</td>" +
              "</tr>"
            );
          }).join("");
        }

        // Subjects
        var subTbody = $("#subjects-table-body", container);
        if (subTbody) {
          subTbody.innerHTML = (struct.subjects || []).map(function (s) {
            return (
              "<tr>" +
              "<td><code>" + esc(s.subject_code) + "</code></td>" +
              "<td><strong>" + esc(s.name) + "</strong></td>" +
              "<td>" + (s.is_lab ? "Laboratory" : "Theory Lecture") + "</td>" +
              "<td>" + s.credits + " Credits</td>" +
              "</tr>"
            );
          }).join("");
        }
      });
    }

    $("#btn-add-room", container).onclick = function () {
      var rname = prompt("Enter Room Name (e.g. CR-18 or LT-16):");
      if (!rname) return;
      var rcap = prompt("Enter Room Capacity:", "100");
      if (!rcap) return;
      API.createRoom({ name: rname, capacity: parseInt(rcap, 10), roomType: rname.startsWith("LT") ? "Lecture Theatre" : "Classroom" })
        .then(function () {
          toast("Room created.");
          loadStructure();
        })
        .catch(function (e) { toast(e.message, true); });
    };

    loadStructure();
  }

  /* =====================================================================
   * 5. TIMETABLE CONTROLLER
   * ===================================================================== */
  function initTimetable(container) {
    var currentView = "grid"; // "grid" | "table"
    var allSessions = [];

    var TIME_SLOTS = [
      { start: "08:00", end: "08:55", label: "08:00 - 08:55" },
      { start: "09:00", end: "09:55", label: "09:00 - 09:55" },
      { start: "09:55", end: "10:50", label: "09:55 - 10:50" },
      { start: "11:10", end: "12:05", label: "11:10 - 12:05" },
      { start: "12:05", end: "13:00", label: "12:05 - 13:00" },
      { start: "13:55", end: "14:50", label: "13:55 - 14:50" },
      { start: "15:10", end: "16:05", label: "15:10 - 16:05" },
      { start: "16:05", end: "17:00", label: "16:05 - 17:00" },
      { start: "17:00", end: "17:55", label: "17:00 - 17:55" }
    ];

    var DAYS = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];

    function loadTimetableData() {
      var sec = $("#tt-section-select", container)?.value || "A";
      var day = $("#tt-day-filter", container)?.value || "";
      var q = $("#tt-search", container)?.value || "";

      API.getSessions({ section: sec, day: day, q: q }).then(function (res) {
        allSessions = res.sessions || [];
        $("#session-count-pill", container).textContent = allSessions.length + " Sessions";

        if (currentView === "grid") {
          renderWeeklyGrid();
        } else {
          renderTableView();
        }
      });
    }

    function renderWeeklyGrid() {
      var gridContainer = $("#timetable-grid-container", container);
      var tableContainer = $("#timetable-table-container", container);
      gridContainer.hidden = false;
      tableContainer.hidden = true;

      var html = '<div class="calendar-grid">';
      html += '<div class="calendar-header-cell">Day / Time</div>';
      TIME_SLOTS.forEach(function (slot) {
        html += '<div class="calendar-header-cell">' + esc(slot.label) + "</div>";
      });

      DAYS.forEach(function (d) {
        html += '<div class="calendar-day-cell">' + esc(d) + "</div>";
        TIME_SLOTS.forEach(function (slot) {
          // Find matching sessions in this slot
          var matching = allSessions.filter(function (s) {
            return s.day === d && s.start_time <= slot.start && s.end_time >= slot.end;
          });

          html += '<div class="calendar-slot">';
          matching.forEach(function (sess) {
            html +=
              '<div class="session-card" data-id="' +
              esc(sess.id) +
              '" title="Click to edit session">' +
              '<div class="session-subject">' +
              esc(sess.subject) +
              "</div>" +
              '<div class="session-meta">' +
              I.svg("room", 12) +
              " <strong>" +
              esc(sess.room) +
              "</strong> | " +
              esc(sess.faculty) +
              "</div>" +
              "</div>";
          });
          html += "</div>";
        });
      });

      html += "</div>";
      gridContainer.innerHTML = html;

      // Click to edit
      $$(".session-card", gridContainer).forEach(function (card) {
        card.onclick = function () {
          var sid = this.dataset.id;
          var sess = allSessions.find(function (x) { return x.id === sid; });
          if (sess) openEditSessionModal(sess);
        };
      });
    }

    function renderTableView() {
      var gridContainer = $("#timetable-grid-container", container);
      var tableContainer = $("#timetable-table-container", container);
      gridContainer.hidden = true;
      tableContainer.hidden = false;

      var tbody = $("#tt-table-body", container);
      if (allSessions.length === 0) {
        tbody.innerHTML = '<tr><td colspan="8" style="text-align:center; padding:20px;">No sessions found for selected filters.</td></tr>';
        return;
      }

      tbody.innerHTML = allSessions
        .map(function (s) {
          return (
            "<tr>" +
            "<td><strong>" + esc(s.section) + "</strong></td>" +
            "<td>" + esc(s.day) + "</td>" +
            "<td>" + esc(s.start_time) + " - " + esc(s.end_time) + "</td>" +
            "<td><strong>" + esc(s.subject) + "</strong></td>" +
            "<td>" + esc(s.faculty) + "</td>" +
            '<td><span class="badge badge-info">' + esc(s.room) + "</span></td>" +
            "<td>" + s.student_count + "</td>" +
            '<td><div class="action-group">' +
            '<button type="button" class="btn btn-secondary btn-sm btn-edit-tt" data-id="' + esc(s.id) + '">Edit</button>' +
            '<button type="button" class="btn btn-ghost btn-sm btn-del-tt" data-id="' + esc(s.id) + '" style="color:#dc2626;">Delete</button>' +
            "</div></td>" +
            "</tr>"
          );
        })
        .join("");

      $$(".btn-edit-tt", tbody).forEach(function (btn) {
        btn.onclick = function () {
          var sid = this.dataset.id;
          var sess = allSessions.find(function (x) { return x.id === sid; });
          if (sess) openEditSessionModal(sess);
        };
      });

      $$(".btn-del-tt", tbody).forEach(function (btn) {
        btn.onclick = function () {
          var sid = this.dataset.id;
          if (confirm("Delete session?")) {
            API.deleteSession(sid).then(function () {
              toast("Session deleted from draft.");
              loadTimetableData();
            }).catch(function (e) { toast(e.message, true); });
          }
        };
      });
    }

    function openEditSessionModal(sess) {
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Edit Draft Session</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-edit-session"><div class="modal-body">' +
        '<div class="field"><label>Subject *</label><input type="text" id="edit-sess-sub" class="input" value="' + esc(sess.subject) + '" required></div>' +
        '<div class="field"><label>Faculty *</label><input type="text" id="edit-sess-fac" class="input" value="' + esc(sess.faculty) + '" required></div>' +
        '<div class="field"><label>Room *</label><input type="text" id="edit-sess-room" class="input" value="' + esc(sess.room) + '" required></div>' +
        '<div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">' +
        '<div class="field"><label>Day *</label><select id="edit-sess-day" class="input">' +
        DAYS.map(function (d) { return '<option value="' + d + '" ' + (sess.day === d ? "selected" : "") + ">" + d + "</option>"; }).join("") +
        '</select></div>' +
        '<div class="field"><label>Section *</label><input type="text" id="edit-sess-sec" class="input" value="' + esc(sess.section) + '" required></div>' +
        '</div>' +
        '<div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">' +
        '<div class="field"><label>Start Time (HH:MM)</label><input type="text" id="edit-sess-start" class="input" value="' + esc(sess.start_time) + '" required></div>' +
        '<div class="field"><label>End Time (HH:MM)</label><input type="text" id="edit-sess-end" class="input" value="' + esc(sess.end_time) + '" required></div>' +
        '</div>' +
        '<div class="field"><label>Student Count</label><input type="number" id="edit-sess-count" class="input" value="' + sess.student_count + '"></div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">Save to Draft</button></div>' +
        '</form></div>'
      );

      $("#form-edit-session").onsubmit = function (e) {
        e.preventDefault();
        var payload = {
          subject: $("#edit-sess-sub").value,
          faculty: $("#edit-sess-fac").value,
          room: $("#edit-sess-room").value,
          day: $("#edit-sess-day").value,
          section: $("#edit-sess-sec").value,
          startTime: $("#edit-sess-start").value,
          endTime: $("#edit-sess-end").value,
          studentCount: parseInt($("#edit-sess-count").value, 10) || 60
        };

        API.updateSession(sess.id, payload).then(function () {
          closeModal();
          toast("Draft session updated.");
          loadTimetableData();
        }).catch(function (err) { toast(err.message, true); });
      };
    }

    $("#btn-add-session", container).onclick = function () {
      var curSec = $("#tt-section-select", container)?.value || "A";
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Add Scheduled Session</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-add-session"><div class="modal-body">' +
        '<div class="field"><label>Subject *</label><input type="text" id="add-sess-sub" class="input" placeholder="e.g. Data Structures with C" required></div>' +
        '<div class="field"><label>Faculty *</label><input type="text" id="add-sess-fac" class="input" placeholder="e.g. Dr. Amit Kumar" required></div>' +
        '<div class="field"><label>Room *</label><input type="text" id="add-sess-room" class="input" placeholder="e.g. LT-1 or CR-2" required></div>' +
        '<div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">' +
        '<div class="field"><label>Day *</label><select id="add-sess-day" class="input">' +
        DAYS.map(function (d) { return '<option value="' + d + '">' + d + "</option>"; }).join("") +
        '</select></div>' +
        '<div class="field"><label>Section *</label><input type="text" id="add-sess-sec" class="input" value="' + esc(curSec) + '" required></div>' +
        '</div>' +
        '<div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">' +
        '<div class="field"><label>Start Time *</label><input type="text" id="add-sess-start" class="input" value="09:00" required></div>' +
        '<div class="field"><label>End Time *</label><input type="text" id="add-sess-end" class="input" value="09:55" required></div>' +
        '</div>' +
        '<div class="field"><label>Student Count</label><input type="number" id="add-sess-count" class="input" value="60"></div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">Schedule Session</button></div>' +
        '</form></div>'
      );

      $("#form-add-session").onsubmit = function (e) {
        e.preventDefault();
        var payload = {
          subject: $("#add-sess-sub").value,
          faculty: $("#add-sess-fac").value,
          room: $("#add-sess-room").value,
          day: $("#add-sess-day").value,
          section: $("#add-sess-sec").value,
          startTime: $("#add-sess-start").value,
          endTime: $("#add-sess-end").value,
          studentCount: parseInt($("#add-sess-count").value, 10) || 60
        };

        API.createSession(payload).then(function () {
          closeModal();
          toast("Session scheduled in draft.");
          loadTimetableData();
        }).catch(function (err) { toast(err.message, true); });
      };
    };

    // Populate section filter
    API.getAcademicStructure().then(function (res) {
      var sel = $("#tt-section-select", container);
      if (!sel || !res.structure) return;
      sel.innerHTML = (res.structure.sections || []).map(function (s) {
        return '<option value="' + esc(s.name) + '">Section ' + esc(s.name) + '</option>';
      }).join("");
      loadTimetableData();
    });

    $("#tt-section-select", container).onchange = loadTimetableData;
    $("#tt-day-filter", container).onchange = loadTimetableData;
    $("#tt-search", container).oninput = function () {
      setTimeout(loadTimetableData, 250);
    };

    $("#view-toggle-grid", container).onclick = function () {
      currentView = "grid";
      this.classList.replace("btn-secondary", "btn-primary");
      $("#view-toggle-table", container).classList.replace("btn-primary", "btn-secondary");
      renderWeeklyGrid();
    };

    $("#view-toggle-table", container).onclick = function () {
      currentView = "table";
      this.classList.replace("btn-secondary", "btn-primary");
      $("#view-toggle-grid", container).classList.replace("btn-primary", "btn-secondary");
      renderTableView();
    };

    $("#btn-publish-draft", container).onclick = function () {
      location.hash = "#/published";
    };
  }

  /* =====================================================================
   * 6. CLASH DETECTION & RESOLVER CONTROLLER (C++ ENGINE)
   * ===================================================================== */
  function initClashDetection(container) {
    var latestClashReport = null;
    var currentFilter = "ALL";

    function refreshClashStats(data) {
      if (!data) return;
      latestClashReport = data;
      var sum = data.summaryByCategory || {};

      var incEl = $("#kpi-incidents-clashes", container);
      if (incEl) {
        incEl.textContent = data.distinctIncidentsCount || sum.distinctIncidents || data.totalClashes || 0;
      }

      $("#kpi-total-clashes", container).textContent = data.totalClashes || 0;
      $("#kpi-fac-clashes", container).textContent = sum["Faculty"] || 0;
      $("#kpi-room-clashes", container).textContent = sum["Room"] || 0;
      $("#kpi-sec-clashes", container).textContent = sum["Section"] || 0;
      $("#kpi-cap-clashes", container).textContent = sum["Room capacity"] || 0;

      var denEl = $("#kpi-density-clashes", container);
      if (denEl) {
        var tot = data.totalClashes || 0;
        var sCount = (D.getMetric && D.getMetric("sessions")) || 0;
        var den = data.conflictDensity != null ? data.conflictDensity + "%" : (sCount > 0 ? Math.round((tot / sCount) * 100) + "%" : "--");
        denEl.textContent = den;
      }

      renderClashesList();
    }

    function renderClashesList() {
      var listContainer = $("#clashes-list-container", container);
      if (!listContainer) return;

      if (!latestClashReport || !latestClashReport.clashes || latestClashReport.clashes.length === 0) {
        listContainer.innerHTML =
          '<div style="text-align:center; padding:32px; background:#fff; border-radius:8px; border:1px solid #e2e8f0;">' +
          I.svg("published", 32) +
          '<h3 style="margin:12px 0 6px 0; color:#16a34a;">No Clashes Detected!</h3><p class="muted">The current draft timetable is completely conflict-free according to the C++ clash engine.</p></div>';
        return;
      }

      var filtered = latestClashReport.clashes.filter(function (c) {
        if (currentFilter === "ALL") return true;
        return c.type === currentFilter;
      });

      var html = '<div style="margin-bottom:12px; font-weight:600; color:#475569;">Showing ' + filtered.length + ' conflicts:</div>';

      filtered.forEach(function (c) {
        var s1 = c.firstSession || {};
        var s2 = c.secondSession || null;

        html +=
          '<div class="clash-card severity-' + esc(c.severity) + '">' +
          '<div class="clash-card-head">' +
          '<div class="clash-title-group">' +
          '<span class="badge ' + (c.severity === "HIGH" ? "badge-danger" : "badge-warning") + '">' + esc(c.severity) + ' SEVERITY</span>' +
          '<strong>' + esc(c.id) + ' — ' + esc(c.type) + '</strong>' +
          "</div>" +
          '<button type="button" class="btn btn-primary btn-sm btn-resolve-clash" data-idx="' + c.index + '">' +
          I.svg("sliders", 14) + ' Resolve Clash (C++ Resolver)</button>' +
          "</div>" +
          '<div class="clash-desc">' + I.svg("alert", 16) + ' ' + esc(c.description) + '</div>' +
          '<div class="clash-sessions-split">' +
          '<div class="clash-session-box">' +
          "<h5>Session A (" + esc(s1.section) + ")</h5>" +
          "<div><strong>" + esc(s1.subject) + "</strong></div>" +
          '<div style="margin-top:4px; font-size:12px; color:#475569;">' +
          "Faculty: " + esc(s1.faculty) + "<br>" +
          "Room: <strong>" + esc(s1.room) + "</strong> (Cap: " + (s1.roomCapacity || "N/A") + ")<br>" +
          "Time: " + esc(s1.day) + " " + esc(s1.startTime) + " - " + esc(s1.endTime) + " (" + (s1.studentCount || "--") + " students)" +
          "</div></div>";

        if (s2) {
          html +=
            '<div class="clash-session-box">' +
            "<h5>Session B (" + esc(s2.section) + ")</h5>" +
            "<div><strong>" + esc(s2.subject) + "</strong></div>" +
            '<div style="margin-top:4px; font-size:12px; color:#475569;">' +
            "Faculty: " + esc(s2.faculty) + "<br>" +
            "Room: <strong>" + esc(s2.room) + "</strong> (Cap: " + (s2.roomCapacity || "N/A") + ")<br>" +
            "Time: " + esc(s2.day) + " " + esc(s2.startTime) + " - " + esc(s2.endTime) + " (" + (s2.studentCount || "--") + " students)" +
            "</div></div>";
        } else {
          html +=
            '<div class="clash-session-box" style="background:#fffbeb;">' +
            "<h5>Capacity Diagnostic</h5>" +
            "<div>Single session or combined cohort exceeding room seating limit.</div>" +
            '<div style="margin-top:4px; font-size:12px; color:#9a6700;">' +
            "Room limit: " + s1.roomCapacity + " | Enrolled students: " + s1.studentCount + " (Shortage: " + (s1.studentCount - s1.roomCapacity) + " seats)" +
            "</div></div>";
        }

        html += "</div></div>";
      });

      listContainer.innerHTML = html;

      // Attach Resolve Clash buttons
      $$(".btn-resolve-clash", listContainer).forEach(function (btn) {
        btn.onclick = function () {
          var cidx = parseInt(this.dataset.idx, 10);
          openResolverModal(cidx);
        };
      });
    }

    function openResolverModal(cidx) {
      showModal(
        '<div class="modal-dialog modal-dialog-large">' +
        '<div class="modal-header"><h3>C++ Clash Resolver: Proposal Analysis</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<div class="modal-body" id="resolver-modal-body">' +
        '<div style="text-align:center; padding:30px;">' + I.svg("refresh", 24) + '<p>Evaluating conflict graph and querying C++ resolver...</p></div>' +
        '</div>' +
        '</div>'
      );

      API.resolveClash(cidx).then(function (res) {
        var p = res.proposal || {};
        var mb = $("#resolver-modal-body");
        if (!mb) return;

        var orig = p.original || {};
        var prop = p.proposed || {};

        var isRoomChanged = orig.room !== prop.room;
        var isTimeChanged = orig.startTime !== prop.startTime || orig.day !== prop.day;

        mb.innerHTML =
          '<div style="margin-bottom:16px;">' +
          '<div style="display:flex; justify-content:space-between; align-items:center;">' +
          '<h4>Proposed Action: <span class="badge badge-info">' + esc(p.action) + '</span></h4>' +
          '<span class="badge ' + (p.feasible ? 'badge-success' : 'badge-danger') + '">' + (p.feasible ? 'FEASIBLE SOLUTION' : 'INFEASIBLE') + '</span>' +
          '</div>' +
          '<div class="clash-desc" style="background:#f0fdf4; border-color:#bbf7d0; color:#166534; margin-top:8px;">' +
          '<strong>Resolver Explanation:</strong> ' + esc(p.explanation) +
          '</div></div>' +
          '<div class="diff-view">' +
          '<div class="diff-col">' +
          '<h4>Current Assignment <span class="badge badge-muted">Original</span></h4>' +
          '<div class="diff-item"><span class="diff-label">Subject</span><span class="diff-value">' + esc(orig.subject) + '</span></div>' +
          '<div class="diff-item"><span class="diff-label">Section</span><span class="diff-value">' + esc(orig.section) + '</span></div>' +
          '<div class="diff-item"><span class="diff-label">Faculty</span><span class="diff-value">' + esc(orig.faculty) + '</span></div>' +
          '<div class="diff-item"><span class="diff-label">Room</span><span class="diff-value">' + esc(orig.room) + ' (Cap: ' + orig.roomCapacity + ')</span></div>' +
          '<div class="diff-item"><span class="diff-label">Day & Time</span><span class="diff-value">' + esc(orig.day) + ' ' + esc(orig.startTime) + ' - ' + esc(orig.endTime) + '</span></div>' +
          '</div>' +
          '<div class="diff-col proposed">' +
          '<h4>Proposed Assignment <span class="badge badge-success">Optimized</span></h4>' +
          '<div class="diff-item"><span class="diff-label">Subject</span><span class="diff-value">' + esc(prop.subject) + '</span></div>' +
          '<div class="diff-item"><span class="diff-label">Section</span><span class="diff-value">' + esc(prop.section) + '</span></div>' +
          '<div class="diff-item"><span class="diff-label">Faculty</span><span class="diff-value">' + esc(prop.faculty) + '</span></div>' +
          '<div class="diff-item"><span class="diff-label">Room</span><span class="diff-value ' + (isRoomChanged ? 'diff-highlight' : '') + '">' + esc(prop.room) + ' (Cap: ' + prop.roomCapacity + ')</span></div>' +
          '<div class="diff-item"><span class="diff-label">Day & Time</span><span class="diff-value ' + (isTimeChanged ? 'diff-highlight' : '') + '">' + esc(prop.day) + ' ' + esc(prop.startTime) + ' - ' + esc(prop.endTime) + '</span></div>' +
          '</div>' +
          '</div>' +
          '<div class="modal-footer" style="margin:20px -20px -20px -20px;">' +
          '<button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button>' +
          '<button type="button" class="btn btn-secondary" id="btn-reject-proposal" style="color:#dc2626;">Reject Proposal</button>' +
          '<button type="button" class="btn btn-primary" id="btn-apply-proposal" ' + (!p.feasible ? 'disabled' : '') + '>' +
          I.svg("check", 16) + ' Apply Resolution</button>' +
          '</div>';

        $("#btn-apply-proposal").onclick = function () {
          this.disabled = true;
          this.textContent = "Applying changes...";
          API.applyResolution(p).then(function (applyRes) {
            closeModal();
            toast("Resolution applied to draft timetable! Clash detection re-evaluated.");
            if (applyRes.newReport) {
              refreshClashStats(applyRes.newReport);
            } else {
              triggerDetection();
            }
          }).catch(function (e) { toast(e.message, true); });
        };

        $("#btn-reject-proposal").onclick = function () {
          API.rejectResolution(p.clashId, "Rejected by administrator").then(function () {
            closeModal();
            toast("Proposal marked as rejected.");
          }).catch(function (e) { toast(e.message, true); });
        };
      }).catch(function (err) {
        var mb = $("#resolver-modal-body");
        if (mb) mb.innerHTML = '<div style="color:#dc2626; padding:20px;">Failed to query C++ resolver: ' + esc(err.message) + '</div>';
      });
    }

    function openBatchResolverModal() {
      showModal(
        '<div class="modal-dialog modal-dialog-large">' +
        '<div class="modal-header"><h3>Automated Graph Coloring Batch Resolver</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<div class="modal-body" id="batch-resolver-body">' +
        '<div style="text-align:center; padding:30px;">' + I.svg("refresh", 24) + '<p>Running Welsh-Powell graph coloring resolver via C++ engine...</p></div>' +
        '</div>' +
        '</div>'
      );

      API.resolveAllClashes(false).then(function (res) {
        var d = res.data || {};
        var mb = $("#batch-resolver-body");
        if (!mb) return;

        var applied = d.applied || [];
        var unres = d.unresolved || [];
        var resolvedCount = Math.max(0, (d.clashesBefore || 0) - (d.clashesAfter || 0));

        var html =
          '<div class="banner" style="background:#f0fdf4; border-color:#86efac; color:#166534; margin-bottom:16px;">' +
          '<strong>Welsh-Powell Graph Coloring Completed.</strong> Automatically resolved ' + resolvedCount + ' clashes. ' +
          'Found ' + applied.length + ' optimal room reassignment(s). ' + unres.length + ' conflicts require administrative scheduling decisions.' +
          '</div>' +
          '<div class="kpi-grid" style="grid-template-columns: repeat(4, 1fr); margin-bottom:16px;">' +
          '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Initial Clashes</div><div class="kpi-value" style="color:#dc2626;">' + (d.clashesBefore || 0) + '</div></div></article>' +
          '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Remaining Clashes</div><div class="kpi-value" style="color:#ea580c;">' + (d.clashesAfter || 0) + '</div></div></article>' +
          '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Applied Fixes</div><div class="kpi-value" style="color:#16a34a;">' + applied.length + '</div></div></article>' +
          '<article class="kpi"><div class="kpi-body"><div class="kpi-label">Unresolved</div><div class="kpi-value" style="color:#64748b;">' + unres.length + '</div></div></article>' +
          '</div>';

        if (applied.length > 0) {
          html += '<h4 style="margin:16px 0 8px 0; color:#1e293b;">Applied Room Reassignments (' + applied.length + ')</h4>' +
            '<div class="table-container" style="max-height:200px; overflow-y:auto; margin-bottom:16px;">' +
            '<table class="data-table"><thead><tr><th>Section</th><th>Subject</th><th>Original Venue</th><th>New Room</th><th>Reason</th></tr></thead><tbody>';
          applied.forEach(function (a) {
            html += '<tr><td><strong>' + esc(a.section) + '</strong></td><td>' + esc(a.subject) + '</td><td>' + esc(a.from) + '</td><td><span class="badge badge-success">' + esc(a.to) + '</span></td><td>' + esc(a.reason) + '</td></tr>';
          });
          html += '</tbody></table></div>';
        }

        if (unres.length > 0) {
          html += '<h4 style="margin:16px 0 8px 0; color:#1e293b;">Unresolved Conflicts with Academic Constraints (' + unres.length + ')</h4>' +
            '<div class="table-container" style="max-height:180px; overflow-y:auto; margin-bottom:16px;">' +
            '<table class="data-table"><thead><tr><th>Conflict Type</th><th>Description</th><th>Reason</th></tr></thead><tbody>';
          unres.forEach(function (u) {
            html += '<tr><td><span class="badge badge-warning">' + esc(u.type) + '</span></td><td>' + esc(u.description) + '</td><td><small class="muted">' + esc(u.reason) + '</small></td></tr>';
          });
          html += '</tbody></table></div>';
        }

        html += '<div style="display:flex; justify-content:flex-end; gap:10px; margin-top:20px; border-top:1px solid #e2e8f0; padding-top:16px;">' +
          '<button type="button" class="btn btn-secondary modal-close-btn">Cancel</button>' +
          '<button type="button" class="btn btn-primary" id="btn-apply-batch-to-draft" style="background:#16a34a; border-color:#15803d;">' +
          I.svg("check", 16) + ' Apply All ' + applied.length + ' Fixes to Draft Timetable</button>' +
          '</div>';

        mb.innerHTML = html;

        var applyBtn = $("#btn-apply-batch-to-draft", mb);
        if (applyBtn) {
          applyBtn.onclick = function () {
            this.disabled = true;
            this.textContent = "Applying fixes to draft...";
            API.resolveAllClashes(true).then(function () {
              toast("Batch resolution applied! Draft timetable updated.");
              closeModal();
              triggerDetection();
            }).catch(function (err) {
              toast(err.message, true);
              applyBtn.disabled = false;
            });
          };
        }
      }).catch(function (err) {
        var mb = $("#batch-resolver-body");
        if (mb) mb.innerHTML = '<div style="color:#dc2626; padding:20px;">Resolver Error: ' + esc(err.message) + '</div>';
      });
    }

    function triggerDetection() {
      var btn = $("#btn-exec-clash-detection", container);
      if (btn) {
        btn.disabled = true;
        btn.innerHTML = I.svg("refresh", 16) + " Running C++ Detection...";
      }

      API.runClashDetection().then(function (res) {
        if (btn) {
          btn.disabled = false;
          btn.innerHTML = I.svg("play", 16) + " RUN CLASH DETECTION";
        }
        toast("Clash detection completed successfully via C++ engine.");
        refreshClashStats(res.report);
      }).catch(function (err) {
        if (btn) {
          btn.disabled = false;
          btn.innerHTML = I.svg("play", 16) + " RUN CLASH DETECTION";
        }
        toast(err.message, true);
      });
    }

    $("#btn-exec-clash-detection", container).onclick = triggerDetection;
    var batchBtn = $("#btn-batch-resolve", container);
    if (batchBtn) batchBtn.onclick = openBatchResolverModal;

    $$(".clash-filter-btn", container).forEach(function (btn) {
      btn.onclick = function () {
        $$(".clash-filter-btn", container).forEach(function (b) { b.classList.replace("btn-primary", "btn-secondary"); });
        this.classList.replace("btn-secondary", "btn-primary");
        currentFilter = this.dataset.type;
        renderClashesList();
      };
    });

    // Load initial clash state if available
    API.getLatestClashes().then(function (res) {
      if (res && res.data) {
        refreshClashStats(res.data);
      }
    });
  }

  /* =====================================================================
   * 7. IMPORT / EXPORT CONTROLLER
   * ===================================================================== */
  function initImportExport(container) {
    initTabs(container);

    // Timetable CSV file upload
    var ttInput = $("#tt-file-input", container);
    if (ttInput) {
      ttInput.onchange = function (e) {
        var file = e.target.files[0];
        if (!file) return;
        var reader = new FileReader();
        reader.onload = function (evt) {
          var csvText = evt.target.result;
          previewTimetableImport(csvText);
        };
        reader.readAsText(file);
      };
    }

    function previewTimetableImport(csvText) {
      var pbox = $("#tt-import-preview-box", container);
      pbox.hidden = false;
      pbox.innerHTML = '<div style="padding:16px; text-align:center;">Validating timetable rows and mapping faculty...</div>';

      API.importTimetable(csvText, false, false).then(function (res) {
        var s = res.summary || {};
        var prev = res.preview || [];

        pbox.innerHTML =
          '<div class="import-summary-grid">' +
          '<div class="summary-card"><div class="val">' + s.totalRows + '</div><div class="lbl">Total Rows</div></div>' +
          '<div class="summary-card"><div class="val" style="color:#16a34a;">' + s.validSessions + '</div><div class="lbl">Valid Sessions</div></div>' +
          '<div class="summary-card"><div class="val">' + s.mappedFacultyCount + '</div><div class="lbl">Matched Faculty</div></div>' +
          '<div class="summary-card"><div class="val" style="color:#dc2626;">' + (s.unmappedFacultyList ? s.unmappedFacultyList.length : 0) + '</div><div class="lbl">Unmapped Faculty</div></div>' +
          '</div>' +
          (s.unmappedFacultyList && s.unmappedFacultyList.length > 0
            ? '<div class="banner" style="background:#fffbeb; border-color:#f59e0b; margin-bottom:12px;"><strong>Notice:</strong> The following faculty references are not in the faculty master list: <code>' + esc(s.unmappedFacultyList.join(", ")) + '</code></div>'
            : '') +
          '<h4>Data Preview (First 15 sessions)</h4>' +
          '<div class="table-container"><table class="data-table"><thead><tr><th>#</th><th>Section</th><th>Day</th><th>Time</th><th>Subject</th><th>Faculty</th><th>Room</th></tr></thead>' +
          '<tbody>' +
          prev.map(function (r) {
            return '<tr><td>' + r.row + '</td><td>' + esc(r.section) + '</td><td>' + esc(r.day) + '</td><td>' + esc(r.startTime) + '-' + esc(r.endTime) + '</td><td>' + esc(r.subject) + '</td><td>' + esc(r.faculty) + '</td><td>' + esc(r.room) + '</td></tr>';
          }).join("") +
          '</tbody></table></div>' +
          '<div style="display:flex; justify-content:flex-end; gap:10px; margin-top:16px;">' +
          '<button type="button" class="btn btn-secondary" onclick="document.getElementById(\'tt-import-preview-box\').hidden=true">Discard</button>' +
          '<button type="button" class="btn btn-primary" id="btn-commit-tt-import">' + I.svg("check", 16) + ' Commit & Import ' + s.validSessions + ' Sessions</button>' +
          '</div>';

        $("#btn-commit-tt-import").onclick = function () {
          this.disabled = true;
          this.textContent = "Importing sessions...";
          API.importTimetable(csvText, true, true).then(function () {
            toast("Successfully imported timetable sessions into draft.");
            pbox.hidden = true;
          }).catch(function (e) { toast(e.message, true); });
        };
      }).catch(function (err) {
        pbox.innerHTML = '<div style="color:#dc2626; padding:16px;">CSV Validation Error: ' + esc(err.message) + '</div>';
      });
    }

    // Student CSV file upload
    var stuInput = $("#stu-file-input", container);
    if (stuInput) {
      stuInput.onchange = function (e) {
        var file = e.target.files[0];
        if (!file) return;
        var reader = new FileReader();
        reader.onload = function (evt) {
          var csvText = evt.target.result;
          API.importStudents(csvText, true).then(function (res) {
            toast("Imported " + res.summary.newRecords + " new, " + res.summary.updatedRecords + " updated students.");
          }).catch(function (e) { toast(e.message, true); });
        };
        reader.readAsText(file);
      };
    }

    // Faculty CSV file upload
    var facInput = $("#fac-file-input", container);
    if (facInput) {
      facInput.onchange = function (e) {
        var file = e.target.files[0];
        if (!file) return;
        var reader = new FileReader();
        reader.onload = function (evt) {
          var csvText = evt.target.result;
          API.importFaculty(csvText, true).then(function (res) {
            toast("Imported " + res.summary.newRecords + " new, " + res.summary.updatedRecords + " updated faculty.");
          }).catch(function (e) { toast(e.message, true); });
        };
        reader.readAsText(file);
      };
    }
  }

  /* =====================================================================
   * 8. PUBLISHED TIMETABLES CONTROLLER
   * ===================================================================== */
  function initPublished(container) {
    function loadVersions() {
      API.getPublishedVersions().then(function (res) {
        var versions = res.versions || [];
        var active = versions.find(function (v) { return v.status === "PUBLISHED"; });

        var actBox = $("#active-published-details", container);
        if (actBox) {
          if (active) {
            actBox.innerHTML =
              '<div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:12px;">' +
              '<div>' +
              '<h3 style="margin:0 0 4px 0; font-size:20px; color:#15803d;">' + esc(active.versionNumber) + '</h3>' +
              '<p class="muted" style="margin:0; font-size:13px;">Published on ' + new Date(active.publishedAt).toLocaleString() + ' by <strong>' + esc(active.publishedBy) + '</strong></p>' +
              '<p style="margin:6px 0 0 0;">' + esc(active.notes || "Official approved timetable.") + '</p>' +
              '</div>' +
              '<div style="text-align:right;">' +
              '<div style="font-size:24px; font-weight:700;">' + active.sessionCount + '</div>' +
              '<div class="muted" style="font-size:12px;">Published Sessions</div>' +
              '</div>' +
              '</div>';
          } else {
            actBox.innerHTML = '<p class="muted">No timetable version has been published yet. Review draft and publish when ready.</p>';
          }
        }

        var tbody = $("#versions-table-body", container);
        if (!tbody) return;

        if (versions.length === 0) {
          tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; padding:20px;" class="muted">No published versions on record.</td></tr>';
          return;
        }

        tbody.innerHTML = versions.map(function (v) {
          return (
            "<tr>" +
            "<td><strong>" + esc(v.versionNumber) + "</strong></td>" +
            '<td><span class="badge ' + (v.status === "PUBLISHED" ? "badge-success" : "badge-muted") + '">' + esc(v.status) + '</span></td>' +
            "<td>" + esc(v.publishedBy) + "</td>" +
            "<td>" + (v.publishedAt ? new Date(v.publishedAt).toLocaleString() : "--") + "</td>" +
            "<td>" + v.sessionCount + "</td>" +
            "<td>" + esc(v.notes || "--") + "</td>" +
            '<td><button type="button" class="btn btn-secondary btn-sm btn-restore-ver" data-id="' + esc(v.versionId) + '" data-num="' + esc(v.versionNumber) + '">Restore to Draft</button></td>' +
            "</tr>"
          );
        }).join("");

        $$(".btn-restore-ver", tbody).forEach(function (btn) {
          btn.onclick = function () {
            var vid = this.dataset.id;
            var vnum = this.dataset.num;
            if (confirm("Restore version " + vnum + " into the current draft timetable? This will overwrite existing unpublished changes.")) {
              API.restoreVersion(vid).then(function (r) {
                toast(r.message || "Version restored.");
                loadVersions();
              }).catch(function (e) { toast(e.message, true); });
            }
          };
        });
      });
    }

    $("#btn-publish-new", container).onclick = function () {
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Publish Timetable Release</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-publish"><div class="modal-body">' +
        '<div class="field"><label>Version Label</label><input type="text" id="pub-ver-num" class="input" placeholder="e.g. v1.0 or v2.0"></div>' +
        '<div class="field"><label>Release Notes</label><textarea id="pub-notes" class="input" style="height:80px;" placeholder="Notes regarding this published timetable version..."></textarea></div>' +
        '<div class="callout" style="background:#fffbeb; border-color:#f59e0b; margin-top:12px;">' +
        '<strong>Publishing Policy:</strong> Publishing will freeze the current draft schedule and make it accessible to the Student and Faculty Portals.' +
        '</div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">' + I.svg("published", 16) + ' Confirm & Publish</button></div>' +
        '</form></div>'
      );

      $("#form-publish").onsubmit = function (e) {
        e.preventDefault();
        var ver = $("#pub-ver-num").value;
        var notes = $("#pub-notes").value;
        API.publishTimetable(ver, notes, true).then(function (r) {
          closeModal();
          toast(r.message || "Timetable published!");
          loadVersions();
        }).catch(function (err) { toast(err.message, true); });
      };
    };

    loadVersions();
  }

  /* =====================================================================
   * 9. ACTIVITY LOGS CONTROLLER
   * ===================================================================== */
  function initActivityLogs(container) {
    function loadLogs() {
      var q = $("#log-search", container)?.value || "";
      var act = $("#log-action-filter", container)?.value || "";

      API.getActivityLogs({ q: q, action: act, limit: 50 }).then(function (res) {
        var tbody = $("#logs-table-body", container);
        if (!tbody) return;

        var logs = res.logs || [];
        if (logs.length === 0) {
          tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; padding:20px;" class="muted">No audit logs found.</td></tr>';
          return;
        }

        tbody.innerHTML = logs.map(function (l) {
          var detailsStr = typeof l.details === "object" ? JSON.stringify(l.details) : String(l.details || "");
          if (detailsStr.length > 80) detailsStr = detailsStr.slice(0, 80) + "...";

          return (
            "<tr>" +
            "<td>" + new Date(l.timestamp).toLocaleString() + "</td>" +
            "<td><strong>" + esc(l.adminName) + "</strong> (" + esc(l.adminId) + ")</td>" +
            '<td><span class="badge badge-info">' + esc(l.action) + '</span></td>' +
            "<td><code>" + esc(l.entityType) + "</code></td>" +
            '<td style="font-family:monospace; font-size:12px;">' + esc(detailsStr) + "</td>" +
            "<td>" + esc(l.ipAddress || "127.0.0.1") + "</td>" +
            "</tr>"
          );
        }).join("");
      });
    }

    $("#btn-refresh-logs", container).onclick = loadLogs;
    $("#log-search", container).oninput = function () {
      setTimeout(loadLogs, 300);
    };
    $("#log-action-filter", container).onchange = loadLogs;

    loadLogs();
  }

  /* =====================================================================
   * 10. SETTINGS CONTROLLER
   * ===================================================================== */
  function initSettings(container) {
    initTabs(container);

    function loadSettings() {
      API.getSettings().then(function (res) {
        var s = res.settings || {};
        var admins = res.admins || [];

        $("#set-univ-name", container).value = s.university_name || "Graphic Era (Deemed to be University)";
        $("#set-campus", container).value = s.campus || "Main Campus, Dehradun";
        $("#set-acad-year", container).value = s.academic_year || "2026–2027";
        $("#set-semester", container).value = s.current_semester || "Semester 3 (Autumn 2026)";

        $("#set-working-days", container).value = s.working_days || "Monday,Tuesday,Wednesday,Thursday,Friday,Saturday";
        $("#set-slot-duration", container).value = s.time_slot_duration || "55 minutes";
        $("#set-clash-policy", container).value = s.clash_detection_policy || "STRICT";

        var tbody = $("#admins-table-body", container);
        if (tbody) {
          tbody.innerHTML = admins.map(function (a) {
            return (
              "<tr>" +
              "<td><code>" + esc(a.admin_id) + "</code></td>" +
              "<td><strong>" + esc(a.name) + "</strong></td>" +
              "<td>" + esc(a.email) + "</td>" +
              "<td>" + esc(a.role) + "</td>" +
              '<td><span class="badge badge-success">' + esc(a.status) + '</span></td>' +
              "<td>" + (a.last_login ? new Date(a.last_login).toLocaleString() : "Never") + "</td>" +
              "</tr>"
            );
          }).join("");
        }
      });
    }

    $("#form-settings-univ", container).onsubmit = function (e) {
      e.preventDefault();
      var payload = {
        university_name: $("#set-univ-name", container).value,
        campus: $("#set-campus", container).value,
        academic_year: $("#set-acad-year", container).value,
        current_semester: $("#set-semester", container).value
      };
      API.updateSettings(payload).then(function () {
        toast("Academic settings updated.");
      }).catch(function (e) { toast(e.message, true); });
    };

    $("#form-settings-rules", container).onsubmit = function (e) {
      e.preventDefault();
      var payload = {
        working_days: $("#set-working-days", container).value,
        time_slot_duration: $("#set-slot-duration", container).value,
        clash_detection_policy: $("#set-clash-policy", container).value
      };
      API.updateSettings(payload).then(function () {
        toast("Scheduling policies updated.");
      }).catch(function (e) { toast(e.message, true); });
    };

    $("#btn-add-admin-modal", container).onclick = function () {
      showModal(
        '<div class="modal-dialog">' +
        '<div class="modal-header"><h3>Provision New Administrator</h3><button type="button" class="btn btn-ghost modal-close-btn">&times;</button></div>' +
        '<form id="form-create-admin"><div class="modal-body">' +
        '<div class="field"><label>Admin ID *</label><input type="text" id="add-adm-id" class="input" placeholder="e.g. GEU-F016" required></div>' +
        '<div class="field"><label>Full Name *</label><input type="text" id="add-adm-name" class="input" required></div>' +
        '<div class="field"><label>University Email *</label><input type="email" id="add-adm-email" class="input" required></div>' +
        '<div class="field"><label>Password *</label><input type="password" id="add-adm-pw" class="input" required></div>' +
        '<div class="field"><label>Administrative Role</label><select id="add-adm-role" class="input"><option value="Administrator">Administrator</option><option value="Timetable Coordinator">Timetable Coordinator</option><option value="Super Admin">Super Admin</option></select></div>' +
        '</div>' +
        '<div class="modal-footer"><button type="button" class="btn btn-secondary modal-cancel-btn">Cancel</button><button type="submit" class="btn btn-primary">Create Account</button></div>' +
        '</form></div>'
      );

      $("#form-create-admin").onsubmit = function (e) {
        e.preventDefault();
        var payload = {
          adminId: $("#add-adm-id").value,
          name: $("#add-adm-name").value,
          email: $("#add-adm-email").value,
          password: $("#add-adm-pw").value,
          role: $("#add-adm-role").value
        };
        API.createAdmin(payload).then(function () {
          closeModal();
          toast("Administrator account provisioned.");
          loadSettings();
        }).catch(function (err) { toast(err.message, true); });
      };
    };

    loadSettings();
  }

  // Central Dispatcher
  root.AdminPages.initPage = function (moduleId, container) {
    if (moduleId === "dashboard") initDashboard();
    else if (moduleId === "students") initStudents(container);
    else if (moduleId === "faculty") initFaculty(container);
    else if (moduleId === "academic-structure") initAcademicStructure(container);
    else if (moduleId === "timetable") initTimetable(container);
    else if (moduleId === "clash-detection") initClashDetection(container);
    else if (moduleId === "import-export") initImportExport(container);
    else if (moduleId === "published") initPublished(container);
    else if (moduleId === "activity-logs") initActivityLogs(container);
    else if (moduleId === "settings") initSettings(container);
  };
})(typeof window !== "undefined" ? window : globalThis);
