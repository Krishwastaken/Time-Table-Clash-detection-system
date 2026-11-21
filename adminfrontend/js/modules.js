/*
 * Module registry: the single list that drives the sidebar, routes, breadcrumbs and placeholder pages.
 *
 * To build a module in a later phase: add a `render(container)` function to its entry (or register a page
 * in AdminPages). Pages without a `render` fall back to the "coming in the next phase" placeholder.
 * The sidebar and routing never change.
 */
window.AdminModules = [
  { id: "dashboard", label: "Dashboard", icon: "dashboard", group: "Overview", kind: "dashboard" },

  { id: "students", label: "Student Management", icon: "users", group: "Data",
    purpose: "Import and manage the student records and section assignments used across the system.",
    source: "students.csv",
    planned: ["Import students from students.csv with validation", "Search, view and edit student records", "Review section membership", "Report duplicate or invalid records"] },

  { id: "faculty", label: "Faculty Management", icon: "faculty", group: "Data",
    purpose: "Maintain the faculty master list used by every timetable session.",
    source: "faculty_master.csv",
    planned: ["Import faculty from faculty_master.csv", "Manage department, designation and contact details", "Support faculty who teach across every section", "Match faculty named in sessions to faculty records"] },

  { id: "academic-structure", label: "Academic Structure", icon: "layers", group: "Data",
    purpose: "Define the sections, subjects and rooms that timetables are built from.",
    source: "Derived from imported data",
    planned: ["Sections and how they are grouped", "Subjects and room inventory", "Working days and time slots"] },

  { id: "timetable", label: "Timetable Management", icon: "calendar", group: "Timetable",
    purpose: "Import, review and edit the scheduled sessions that make up each timetable.",
    source: "sessions.csv",
    planned: ["Import sessions from sessions.csv", "View timetables by section, faculty and room", "Edit sessions as draft changes before publishing"] },

  { id: "clash-detection", label: "Clash Detection", icon: "alert", group: "Timetable",
    purpose: "Run the project's C++ clash detection engine and review what it finds, then review suggested resolutions.",
    source: "Existing C++ clash detection engine and resolver",
    planned: ["Run detection on the current draft timetable", "Show clash categories, affected sessions and explanations", "Review and apply suggested resolutions from the C++ resolver", "Re-run detection after changes"] },

  { id: "import-export", label: "Import / Export", icon: "transfer", group: "Timetable",
    purpose: "Bring data in from CSV files and export timetables for sharing or printing.",
    source: "CSV files",
    planned: ["Validated CSV import for students, faculty and sessions", "Import error reports", "Export timetables"] },

  { id: "published", label: "Published Timetables", icon: "published", group: "Timetable",
    purpose: "Publish approved timetable versions and make them available to the Student and Faculty portals.",
    source: "Not connected. Nothing is shared between portals yet.",
    planned: ["Version history of published timetables", "Publish and roll back a version", "Serve published data to the Student and Faculty portals"] },

  { id: "activity-logs", label: "Activity Logs", icon: "list", group: "System",
    purpose: "Keep an audit trail of administrative changes.",
    source: "Not connected",
    planned: ["Record imports, edits, clash runs and publishes", "Filter by date, user and action"] },

  { id: "settings", label: "Settings", icon: "sliders", group: "System",
    purpose: "Configure the portal and the timetable system.",
    source: "Not connected",
    planned: ["Academic term and working hours", "Authentication settings (replaces the prototype sign-in)", "Backend connection settings"] }
];
