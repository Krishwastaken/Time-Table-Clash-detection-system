const http = require('node:http');
const config = require('./config');
const { requestHandler } = require('./app');
const { runSeed } = require('./services/seed.service');
const { provisionAdminsFromCSV } = require('./services/provisioning.service');

function bootstrap() {
  console.log('------------------------------------------------------------');
  console.log('University Timetable Management System: Admin Backend');
  console.log('------------------------------------------------------------');

  try {
    // 1. Seed database tables (Faculty, Academic Structure, Sessions, Settings)
    console.log('[1/3] Initializing database and seeding academic records...');
    runSeed();

    // 2. Provision administrators from admin_master.csv
    console.log('[2/3] Provisioning administrator accounts from admin_master.csv...');
    const prov = provisionAdminsFromCSV();
    if (prov.ok) {
      console.log(`      Provisioned: ${prov.createdCount} new, ${prov.skippedCount} existing.`);
    } else {
      console.warn(`      Provisioning notice: ${prov.message}`);
    }

    // 3. Start HTTP server
    console.log('[3/3] Starting HTTP server...');
    const server = http.createServer(requestHandler);

    server.listen(config.PORT, () => {
      console.log('============================================================');
      console.log(`  Backend API & Admin Portal Live at:`);
      console.log(`  -> Admin Portal:  http://localhost:${config.PORT}/adminfrontend/`);
      console.log(`  -> Dashboard API: http://localhost:${config.PORT}/api/stats/dashboard`);
      console.log(`  -> API Docs:      http://localhost:${config.PORT}/api/docs`);
      console.log('============================================================');
    });

    return server;
  } catch (err) {
    console.error('Fatal initialization error:', err);
    process.exit(1);
  }
}

if (require.main === module) {
  bootstrap();
}

module.exports = { bootstrap };
