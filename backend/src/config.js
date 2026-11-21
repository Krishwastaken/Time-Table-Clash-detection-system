const path = require('node:path');

const ROOT_DIR = path.resolve(__dirname, '../../');
const DATA_DIR = path.join(ROOT_DIR, 'data');
const DB_DIR = path.join(ROOT_DIR, 'database');
const PBL_DIR = path.join(ROOT_DIR, 'PBL');
const FRONTEND_DIR = path.join(ROOT_DIR, 'adminfrontend');

module.exports = {
  PORT: process.env.PORT || 3000,
  ROOT_DIR,
  DATA_DIR,
  DB_DIR,
  DB_PATH: process.env.DB_PATH || path.join(DB_DIR, 'timetable.sqlite'),
  PBL_DIR,
  ENGINE_BIN: path.join(PBL_DIR, 'bin', 'engine_cli'),
  FRONTEND_DIR,
  ADMIN_CSV: path.join(DATA_DIR, 'admin_master.csv'),
  FACULTY_CSV: path.join(DATA_DIR, 'faculty_master.csv'),
  SESSIONS_CSV: path.join(DATA_DIR, 'sessions.csv'),
  SESSION_SECRET: process.env.SESSION_SECRET || 'university-admin-secret-key-2026',
  SESSION_DURATION_MS: 8 * 60 * 60 * 1000 // 8 hours
};
