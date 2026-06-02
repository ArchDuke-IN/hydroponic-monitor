const { handleOptions, sendJSON } = require('./_helpers');
const { sql } = require('../lib/db');

/**
 * GET /api/get-data
 * Returns combined latest data from both ESP32 devices
 * This is what the dashboard calls every 60 seconds
 */
module.exports = async (req, res) => {
  // Ensure CORS is set
  if (handleOptions(req, res)) return;

  try {
    const [phResult, ecResult] = await Promise.all([
      sql`SELECT * FROM ph_readings ORDER BY created_at DESC LIMIT 1`,
      sql`SELECT * FROM ec_readings ORDER BY created_at DESC LIMIT 1`
    ]);

    const ph = phResult[0] || null;
    const ec = ecResult[0] || null;

  let tdsValue = 0;
  let ecMsCm = 0;
  let ecRaw = 0;

  if (ec && ec.ec_value != null) {
    ecRaw = parseFloat(ec.ec_value);
    ecMsCm = parseFloat((ecRaw / 1000).toFixed(2));
    tdsValue = Math.round(ecRaw * 0.5);
  }

  const response = {
    success: true,
    timestamp: new Date().toISOString(),
    data: {
      plant_monitoring: ph
        ? [
            {
              temperature: ph.temp1,       // DHT11 — Ambient temperature
              humidity: ph.hum1,            // DHT11 — Humidity
              soil_moisture: null,          // No soil moisture sensor
              light_intensity: null,        // No light sensor
              timestamp: ph.created_at,
            },
          ]
        : [],

      water_quality: [
            {
              ph_value: ph ? parseFloat(ph.ph_val) : 0,
              tds_value: tdsValue,
              ec_value: ecMsCm,
              water_temp: ph ? parseFloat(ph.temp2) : (ec ? parseFloat(ec.temperature) : 0),  // DS18B20 — Water temperature
              water_level: null,            // No water level sensor
              voltage: ec ? ec.voltage : 0,
              timestamp: ec ? ec.created_at : (ph ? ph.created_at : new Date()),
            },
          ]
    },
    device_status: {
      ph_monitor: ph ? 'connected' : 'disconnected',
      ec_monitor: ec ? 'connected' : 'disconnected',
      ph_last_update: ph ? ph.created_at : null,
      ec_last_update: ec ? ec.created_at : null,
    },
  };

    return sendJSON(res, response);
  } catch (err) {
    console.error('Error in get-data:', err);
    return sendJSON(res, { success: false, error: 'Failed to fetch data' }, 500);
  }
};
