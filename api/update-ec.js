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
  if (handleOptions(req, res)) return;

  if (req.method !== 'POST') {
    return sendError(res, 'Only POST method allowed', 405);
  }

  const { ec_value, voltage, temperature } = req.body || {};

  // Validate required fields
  if (ec_value == null || voltage == null || temperature == null) {
    return sendError(res, 'Missing required fields: ec_value, voltage, temperature');
  }

  try {
    const newData = { ec_value, voltage, temperature };
    await saveECReading(newData); // Await async save
    sendJSON(res, { success: true, message: 'Data saved to cloud' });
  } catch (e) {
    console.error('Save error:', e);
    sendError(res, 'Internal server error', 500);
  }
};

  // Parse and validate
  const ec = parseFloat(ec_value);
  const volt = parseFloat(voltage);
  const temp = parseFloat(temperature);

  if (isNaN(ec) || isNaN(volt) || isNaN(temp)) {
    return sendError(res, 'All values must be valid numbers');
  }

  if (ec < 0 || ec > 100000) {
    return sendError(res, 'ec_value out of range (0-100000 µS/cm)');
  }

  // Save to store
  try {
    const entry = await saveECReading({
      device_id: device_id || 'ESP32_EC',
      ec_value: ec,
      voltage: volt,
      temperature: temp,
    });

    console.log(`EC data saved: EC=${ec} µS/cm, V=${volt}V, T=${temp}°C`);

    sendJSON(res, {
      success: true,
      message: 'Data saved successfully',
      timestamp: entry.timestamp,
    });
  } catch (err) {
    console.error('Failed to save EC reading:', err);
    sendError(res, 'Database error');
  }
};

