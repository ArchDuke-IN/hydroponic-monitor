const { saveECReading } = require('./_db');
const { handleOptions, sendJSON, sendError } = require('./_helpers');

/**
 * POST /api/update-ec
 * Receives data from ESP32 #2 (EC Monitor)
 *
 * Expected JSON body:
 * {
 *   "device_id": "ESP32_EC",
 *   "ec_value": 1200,
 *   "voltage": 1.85,
 *   "temperature": 25.0
 * }
 */
module.exports = (req, res) => {
  if (handleOptions(req, res)) return;

  if (req.method !== 'POST') {
    return sendError(res, 'Only POST method allowed', 405);
  }

  const { device_id, ec_value, voltage, temperature } = req.body || {};

  // Validate required fields
  if (ec_value == null || voltage == null || temperature == null) {
    return sendError(res, 'Missing required fields: ec_value, voltage, temperature');
  }

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
  const entry = saveECReading({
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
};
