const { handleOptions, sendJSON, sendError } = require('./_helpers');
const { getDb } = require('../lib/mongo');

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

  // Validate required fields
  if (temp1 == null || hum1 == null || temp2 == null || hum2 == null || ph_val == null) {
    return sendError(res, 'Missing required fields: temp1, hum1, temp2, hum2, ph_val');
  }

  // Validate ranges
  const t1 = parseFloat(temp1);
  const h1 = parseFloat(hum1);
  const t2 = parseFloat(temp2);
  const h2 = parseFloat(hum2);
  const ph = parseFloat(ph_val);

  if (isNaN(t1) || isNaN(h1) || isNaN(t2) || isNaN(h2) || isNaN(ph)) {
    return sendError(res, 'All values must be valid numbers', 400);
  }

  try {
    console.log('Starting pH data save process...');
    const db = await getDb();
    console.log('Database connection obtained');
    
    const collection = db.collection('ph_readings');
    const doc = {
      temp1: t1,
      hum1: h1,
      temp2: t2,
      hum2: h2,
      ph_val: ph,
      createdAt: new Date(),
    };

    console.log('Inserting document:', doc);
    await collection.insertOne(doc);
    console.log('pH data saved successfully:', doc);

    return sendJSON(res, { success: true, message: 'Data saved to cloud' });
  } catch (e) {
    console.error('Save error in update-ph:', e.message);
    console.error('Full error:', e);
    return sendError(res, `Database error: ${e.message}`, 500);
  }
};

