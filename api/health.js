const { handleOptions, sendJSON } = require('./_helpers');
const { sql } = require('../lib/db');

/**
 * GET /api/health
 * Health check endpoint to verify API and DB are running
 */
module.exports = async (req, res) => {
  if (handleOptions(req, res)) return;

  try {
    const result = await sql`SELECT 1 as db_alive`;
    sendJSON(res, {
      success: true,
      message: 'Hydroponic API is running',
      dbStatus: result[0].db_alive === 1 ? 'connected' : 'error',
      timestamp: new Date().toISOString(),
    });
  } catch (error) {
    sendJSON(res, {
      success: false,
      message: 'API running but DB disconnected',
      error: error.message,
      timestamp: new Date().toISOString(),
    }, 500);
  }
};
