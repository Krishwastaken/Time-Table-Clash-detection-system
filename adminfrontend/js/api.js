/*
 * API Client for Admin Portal Backend.
 * Communicates with the REST endpoints on /api/*.
 * Works both when served directly from the backend server or standalone via CORS.
 */
(function (root) {
  "use strict";

  var BASE_URL = "";
  // If running from an external server (e.g. port 8000, 5500 Live Server, 5173, or file://), point to backend at port 3000
  if (typeof location !== "undefined") {
    if (location.protocol === "file:" || (location.port && location.port !== "3000")) {
      BASE_URL = "http://localhost:3000";
    }
  }

  function getHeaders() {
    var headers = { "Content-Type": "application/json" };
    try {
      var s = JSON.parse(sessionStorage.getItem("adminPortal.session") || "null");
      if (s && s.token) {
        headers["Authorization"] = "Bearer " + s.token;
      }
    } catch (e) { /* ignore */ }
    return headers;
  }

  function req(method, endpoint, data) {
    var url = BASE_URL + endpoint;
    var options = {
      method: method,
      headers: getHeaders(),
      credentials: "omit"
    };

    if (data && (method === "POST" || method === "PUT" || method === "DELETE")) {
      options.body = JSON.stringify(data);
    }

    return fetch(url, options).then(function (res) {
      if (res.headers.get("content-type") && res.headers.get("content-type").indexOf("text/csv") !== -1) {
        return res.text();
      }
      return res.json().then(function (json) {
        if (!res.ok) {
          throw new Error(json.message || "Request failed with status " + res.status);
        }
        return json;
      });
    });
  }

  root.AdminAPI = {
    setBaseUrl: function (url) { BASE_URL = url; },
    getBaseUrl: function () { return BASE_URL; },

    // Auth
    login: function (identifier, password) {
      return req("POST", "/api/auth/login", { identifier: identifier, password: password });
    },
    logout: function () {
      return req("POST", "/api/auth/logout");
    },
    me: function () {
      return req("GET", "/api/auth/me");
    },

    // Stats
    getDashboard: function () {
      return req("GET", "/api/stats/dashboard");
    },

    // Students
    getStudents: function (params) {
      var qs = new URLSearchParams(params || {}).toString();
      return req("GET", "/api/students" + (qs ? "?" + qs : ""));
    },
    createStudent: function (data) {
      return req("POST", "/api/students", data);
    },
    updateStudent: function (id, data) {
      return req("PUT", "/api/students/" + encodeURIComponent(id), data);
    },
    deleteStudent: function (id, permanent) {
      return req("DELETE", "/api/students/" + encodeURIComponent(id) + (permanent ? "?permanent=true" : ""));
    },
    importStudents: function (csvContent, commit) {
      return req("POST", "/api/students/import", { csvContent: csvContent, commit: commit });
    },
    getStudentsExportUrl: function () {
      return (BASE_URL || "") + "/api/students/export";
    },

    // Faculty
    getFaculty: function (params) {
      var qs = new URLSearchParams(params || {}).toString();
      return req("GET", "/api/faculty" + (qs ? "?" + qs : ""));
    },
    createFaculty: function (data) {
      return req("POST", "/api/faculty", data);
    },
    updateFaculty: function (id, data) {
      return req("PUT", "/api/faculty/" + encodeURIComponent(id), data);
    },
    deleteFaculty: function (id, permanent) {
      return req("DELETE", "/api/faculty/" + encodeURIComponent(id) + (permanent ? "?permanent=true" : ""));
    },
    importFaculty: function (csvContent, commit) {
      return req("POST", "/api/faculty/import", { csvContent: csvContent, commit: commit });
    },
    getFacultyExportUrl: function () {
      return (BASE_URL || "") + "/api/faculty/export";
    },

    // Academic Structure
    getAcademicStructure: function () {
      return req("GET", "/api/academic/structure");
    },
    createRoom: function (data) {
      return req("POST", "/api/academic/rooms", data);
    },
    updateRoom: function (id, data) {
      return req("PUT", "/api/academic/rooms/" + encodeURIComponent(id), data);
    },
    deleteRoom: function (id) {
      return req("DELETE", "/api/academic/rooms/" + encodeURIComponent(id));
    },

    // Timetable
    getSessions: function (params) {
      var qs = new URLSearchParams(params || {}).toString();
      return req("GET", "/api/timetable/sessions" + (qs ? "?" + qs : ""));
    },
    createSession: function (data) {
      return req("POST", "/api/timetable/sessions", data);
    },
    updateSession: function (id, data) {
      return req("PUT", "/api/timetable/sessions/" + encodeURIComponent(id), data);
    },
    deleteSession: function (id) {
      return req("DELETE", "/api/timetable/sessions/" + encodeURIComponent(id));
    },
    importTimetable: function (csvContent, commit, replaceDraft) {
      return req("POST", "/api/timetable/import", { csvContent: csvContent, commit: commit, replaceDraft: replaceDraft });
    },
    getTimetableExportUrl: function () {
      return (BASE_URL || "") + "/api/timetable/export";
    },

    // Clash Detection & Resolver (Real C++ Engine)
    runClashDetection: function () {
      return req("POST", "/api/clash/detect");
    },
    getLatestClashes: function () {
      return req("GET", "/api/clash/last");
    },
    resolveClash: function (clashIndex) {
      return req("POST", "/api/clash/resolve", { clashIndex: clashIndex });
    },
    resolveAllClashes: function (applyToDraft) {
      return req("POST", "/api/clash/resolve-all", { apply: !!applyToDraft });
    },
    applyResolution: function (proposal) {
      return req("POST", "/api/clash/apply", { proposal: proposal });
    },
    rejectResolution: function (proposalId, reason) {
      return req("POST", "/api/clash/reject", { proposalId: proposalId, reason: reason });
    },

    // Publishing
    getPublishedVersions: function () {
      return req("GET", "/api/published/versions");
    },
    publishTimetable: function (versionNumber, notes, forcePublish) {
      return req("POST", "/api/published/publish", { versionNumber: versionNumber, notes: notes, forcePublish: forcePublish });
    },
    restoreVersion: function (versionId) {
      return req("POST", "/api/published/restore/" + encodeURIComponent(versionId));
    },

    // Activity Logs
    getActivityLogs: function (params) {
      var qs = new URLSearchParams(params || {}).toString();
      return req("GET", "/api/activity/logs" + (qs ? "?" + qs : ""));
    },

    // Settings
    getSettings: function () {
      return req("GET", "/api/settings");
    },
    updateSettings: function (settings) {
      return req("POST", "/api/settings", { settings: settings });
    },
    createAdmin: function (data) {
      return req("POST", "/api/settings/admin", data);
    },

    // Shared Portals (Read-only)
    getStudentTimetable: function (section) {
      return req("GET", "/api/portal/student/timetable?section=" + encodeURIComponent(section));
    },
    getFacultySchedule: function (facultyId) {
      return req("GET", "/api/portal/faculty/schedule?faculty_id=" + encodeURIComponent(facultyId));
    },
    getFacultyDirectory: function () {
      return req("GET", "/api/portal/faculty/directory");
    }
  };
})(typeof window !== "undefined" ? window : globalThis);
