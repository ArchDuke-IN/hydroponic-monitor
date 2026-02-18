const { savePHReading } = require('./_db');
const { handleOptions, sendJSON, sendError } = require('./_helpers');

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
  if (handleOptions(req, res)) return;

  if (req.method !== 'POST') {
    return sendError(res, 'Only POST method allowed', 405);
  }

  const { temp1, hum1, temp2, hum2, ph_val } = req.body || {};

  // Validate required fields
  if (temp1 == null || hum1 == null || temp2 == null || hum2 == null || ph_val == null) {
    return sendError(res, 'Missing required fields: temp1, hum1, temp2, hum2, ph_val');
  }

  try {
    const newData = { temp1, hum1, temp2, hum2, ph_val };
    await savePHReading(newData); // Await async save
    sendJSON(res, { success: true, message: 'Data saved to cloud' });
  } catch (e) {
    console.error('Save error:', e);
    sendError(res, 'Internal server error', 500);
  }
};

  // Validate ranges
  const t1 = parseFloat(temp1);
  const h1 = parseFloat(hum1);
  const t2 = parseFloat(temp2);
  const h2 = parseFloat(hum2);
  const ph = parseFloat(ph_val);

  if (isNaN(t1) || isNaN(h1) || isNaN(t2) || isNaN(h2) || isNaN(ph)) {
    return sendError(res, 'All values must be valid numbers');
  }

  if (ph < 0 || ph > 14) {
    return sendError(res, 'ph_val must be between 0 and 14');
  }

  // Save to store
  try {
    const entry = await savePHReading({
      device_id: device_id || 'ESP32_PH',
      temp1: t1,
      hum1: h1,
      temp2: t2,
      hum2: h2,
      ph_val: ph,
    });

    console.log(`pH data saved: pH=${ph}, T1=${t1}, H1=${h1}, T2=${t2}, H2=${h2}`);

    sendJSON(res, {
      success: true,
      message: 'Data saved successfully',
      timestamp: entry.timestamp,
    });
  } catch (err) {
    console.error('Failed to save pH reading:', err);
    sendError(res, 'Database error');
  }
};

