const { saveECReading } = require('./_db');
const { handleOptions, sendJSON, sendError } = require('./_helpers');

/**
 * POST /api/update-ec
 * Receives data from ESP32 #2 (EC Monitor)
 *
 * Expected JSON body:
 * {
 *   "ec_value": 1200,
 *   "voltage": 1.85,
 *   "temperature": 25.0
 * }
 */
module.exports = async (req, res) => {
  // Ensure CORS is set for all requests
  if (handleOptions(req, res)) return;

  if (req.method !== 'POST') {
    return sendError(res, 'Only POST method allowed', 405);
  }

  // Debug: Log the received body
  console.log('Received EC update:', req.body);

  const { ec_value, voltage, temperature } = req.body || {};

  // Validate required fields
  if (ec_value == null || voltage == null || temperature == null) {
    return sendError(res, 'Missing required fields: ec_value, voltage, temperature');
  }

  // Parse and validate
  const ec = parseFloat(ec_value);
  const volt = parseFloat(voltage);
  const temp = parseFloat(temperature);

  if (isNaN(ec) || isNaN(volt) || isNaN(temp)) {
    return sendError(res, 'All values must be valid numbers', 400);
  }

  if (ec < 0 || ec > 100000) {
    return sendError(res, 'ec_value out of range (0-100000 µS/cm)', 400);
  }

  try {
    const newData = { ec_value: ec, voltage: volt, temperature: temp };
    saveECReading(newData); 
    return sendJSON(res, { success: true, message: 'Data saved to cloud' });
  } catch (e) {
    console.error('Save error:', e);
    return sendError(res, 'Internal server error', 500);
  }
};

