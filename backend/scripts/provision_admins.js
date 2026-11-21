#!/usr/bin/env node
const { provisionAdminsFromCSV } = require('../src/services/provisioning.service');

console.log('Running administrator account provisioning...');
const res = provisionAdminsFromCSV();
console.log('Result:', res);
if (res.errors && res.errors.length > 0) {
  console.warn('Errors encountered:', res.errors);
}
