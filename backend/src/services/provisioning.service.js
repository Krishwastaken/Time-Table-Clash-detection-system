const fs = require('node:fs');
const path = require('node:path');
const db = require('../db');
const { hashPassword } = require('../auth');
const { parseCSV, normalizeHeader } = require('../utils/csv_parser');
const auditService = require('./audit.service');
const config = require('../config');

function provisionAdminsFromCSV(csvPath = config.ADMIN_CSV) {
  if (!fs.existsSync(csvPath)) {
    return { ok: false, message: `Admin master CSV not found at ${csvPath}` };
  }

  const content = fs.readFileSync(csvPath, 'utf8');
  const rows = parseCSV(content);

  if (rows.length < 2) {
    return { ok: false, message: 'Admin master CSV is empty or missing data rows' };
  }

  const headers = rows[0].map(normalizeHeader);
  const idIdx = headers.indexOf('adminid');
  const nameIdx = headers.indexOf('name');
  const emailIdx = headers.indexOf('email');
  const passIdx = headers.indexOf('password');

  if (idIdx === -1 || nameIdx === -1 || emailIdx === -1 || passIdx === -1) {
    return { ok: false, message: 'Admin master CSV missing required columns (Admin_ID, Name, Email, Password)' };
  }

  let createdCount = 0;
  let skippedCount = 0;
  const errors = [];

  for (let i = 1; i < rows.length; i++) {
    const row = rows[i];
    const adminId = String(row[idIdx] || '').trim();
    const name = String(row[nameIdx] || '').trim();
    const email = String(row[emailIdx] || '').trim().toLowerCase();
    const rawPassword = String(row[passIdx] || '');

    if (!adminId || !email || !rawPassword) {
      errors.push(`Row ${i + 1}: Missing admin ID, email, or password.`);
      skippedCount++;
      continue;
    }

    // Check if admin already exists by ID or email
    const existing = db.get(
      'SELECT admin_id, email FROM admin_profiles WHERE admin_id = ? OR LOWER(email) = ?',
      [adminId, email]
    );

    if (existing) {
      skippedCount++;
      continue;
    }

    const { hash, salt } = hashPassword(rawPassword);

    try {
      db.run(
        `INSERT INTO admin_profiles (admin_id, name, email, password_hash, salt, role, status)
         VALUES (?, ?, ?, ?, ?, 'Administrator', 'ACTIVE')`,
        [adminId, name || adminId, email, hash, salt]
      );
      createdCount++;
    } catch (err) {
      errors.push(`Row ${i + 1} (${adminId}): ${err.message}`);
      skippedCount++;
    }
  }

  if (createdCount > 0) {
    auditService.logAction({
      adminId: 'SYSTEM',
      adminName: 'Provisioning Service',
      action: 'ADMIN_ACCOUNTS_PROVISIONED',
      entityType: 'admin_profiles',
      details: { created: createdCount, skipped: skippedCount, totalProcessed: rows.length - 1 }
    });
  }

  return {
    ok: true,
    createdCount,
    skippedCount,
    totalRecords: rows.length - 1,
    errors
  };
}

module.exports = {
  provisionAdminsFromCSV
};
