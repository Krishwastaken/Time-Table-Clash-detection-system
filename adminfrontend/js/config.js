/*
 * Portal configuration and live data seam.
 * Connected to the central database and C++ clash detection engine.
 */
(function (root) {
  "use strict";

  root.AdminConfig = {
    portalName: "Admin Portal",
    systemName: "University Timetable Management System",
    phase: "Phase 2: Complete Integrated System",
    placeholder: "--"
  };

  var cache = {
    metrics: {},
    timetableStatus: null,
    clashSummary: null,
    activityFeed: []
  };

  root.AdminData = {
    /** @returns {number|string|null} null means "not connected". */
    getMetric: function (metricId) {
      if (cache.metrics && cache.metrics[metricId] !== undefined) {
        return cache.metrics[metricId];
      }
      return null;
    },

    /** Timetable status fields shown on the dashboard. */
    getTimetableStatus: function () {
      return cache.timetableStatus;
    },

    /** Clash summary from the C++ engine. */
    getClashSummary: function () {
      return cache.clashSummary;
    },

    getActivityFeed: function () {
      return cache.activityFeed;
    },

    setDashboardData: function (data) {
      if (!data) return;
      if (data.metrics) cache.metrics = data.metrics;
      if (data.timetableStatus) cache.timetableStatus = data.timetableStatus;
      if (data.clashSummary) cache.clashSummary = data.clashSummary;
      if (data.activityFeed) cache.activityFeed = data.activityFeed;
    },

    loadDashboardData: function () {
      if (!root.AdminAPI || !root.AdminAPI.getDashboard) {
        return Promise.resolve(null);
      }
      return root.AdminAPI.getDashboard().then(function (res) {
        if (res && res.ok) {
          root.AdminData.setDashboardData(res);
          return res;
        }
        return null;
      }).catch(function () {
        return null;
      });
    }
  };
})(typeof window !== "undefined" ? window : globalThis);
