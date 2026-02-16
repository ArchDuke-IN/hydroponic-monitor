const { getLatest } = require('./_db');
const { handleOptions, sendJSON } = require('./_helpers');

/**
 * GET /api/health
 * Health check endpoint to verify API is running
 */
module.exports = (req, res) => {
  if (handleOptions(req, res)) return;

  const store = getLatest();

  sendJSON(res, {
    success: true,
    message: 'Hydroponic API is running',
    uptime: process.uptime(),
    devices: {
      ph_monitor: store.ph_monitor ? 'has data' : 'no data yet',
      ec_monitor: store.ec_monitor ? 'has data' : 'no data yet',
    },
    readings_stored: {
      ph_history: store.ph_history.length,
      ec_history: store.ec_history.length,
    },
    timestamp: new Date().toISOString(),
  });
};
