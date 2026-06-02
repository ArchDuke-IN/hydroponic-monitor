const { handleOptions, sendJSON, sendError } = require('./_helpers');
const { sql } = require('../lib/db');

/**
 * POST /api/update-ph
 * Receives data from ESP32 #1 (pH Monitor)
 *
 * Expected JSON body:
 * {
 *   "temp1": 25.5,
 *   "hum1": 65.0,
 *   "temp2": 24.8,
 *   "hum2": 70.0,
 *   "ph_val": 6.5
 * }
 */
module.exports = async (req, res) => {
  // Ensure CORS is set for all requests
  if (handleOptions(req, res)) return;

  if (req.method !== 'POST') {
    return sendError(res, 'Only POST method allowed', 405);
  }

  // Debug: Log the received body
  console.log('Received pH update:', req.body);

  const { temp1, hum1, temp2, hum2, ph_val } = req.body || {};

  // Parse and sanitize — accept partial data, convert null/NaN to 0
  const t1 = parseFloat(temp1) || 0;
  const h1 = parseFloat(hum1) || 0;
  const t2 = parseFloat(temp2) || 0;
  const h2 = parseFloat(hum2) || 0;
  const ph = parseFloat(ph_val);

  if (isNaN(ph)) {
    return sendError(res, 'ph_val must be a valid number', 400);
  }

  try {
    console.log('Starting pH data save process...');
    
    const result = await sql`
      INSERT INTO ph_readings (temp1, hum1, temp2, hum2, ph_val, created_at)
      VALUES (${t1}, ${h1}, ${t2}, ${h2}, ${ph}, NOW())
      RETURNING *
    `;

    console.log('pH data saved successfully:', result[0]);

    return sendJSON(res, { success: true, message: 'Data saved to Neon DB' });
  } catch (e) {
    console.error('Save error in update-ph:', e.message);
    console.error('Full error:', e);
    return sendError(res, `Database error: ${e.message}`, 500);
  }
};

