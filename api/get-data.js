const { getLatest } = require('./_db');
const { handleOptions, sendJSON } = require('./_helpers');

/**
 * GET /api/get-data
 * Returns combined latest data from both ESP32 devices
 * This is what the dashboard calls every 60 seconds
 */
module.exports = async (req, res) => {
  // Ensure CORS is set
  if (handleOptions(req, res)) return;

  const store = getLatest(); 
  // Debug: Log what we are fetching
  console.log('Fetching latest data:', store);
  
  const ph = store.ph_monitor;
  const ec = store.ec_monitor;

  // Calculate TDS from EC (TDS ≈ EC × 0.64) - EC is coming in µS/cm
  // but if we want TDS in ppm, we usually use EC in µS/cm * 0.5 or 0.7 depending on scale. 
  // Let's assume standard 0.5 conversion for hydroponics: 1 EC (mS/cm) = 500 ppm
  // Since we have EC in µS/cm, TDS ppm = EC_uS * 0.5
  // Or if using your formula 0.64
  
  let tdsValue = 0;
  let ecMsCm = 0;
  let ecRaw = 0;

  if (ec && ec.ec_value) {
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
              temperature: ph.temp1,
              humidity: ph.hum1,
              soil_moisture: ph.hum2, // Using hum2 as proxy for now
              light_intensity: 500, // Dummy
              timestamp: ph.timestamp,
            },
          ]
        : [],

      water_quality: [
            {
              ph_value: ph ? parseFloat(ph.ph_val) : 0,
              tds_value: tdsValue,
              ec_value: ecMsCm,
              water_temp: ph ? parseFloat(ph.temp2) : (ec ? parseFloat(ec.temperature) : 0),
              water_level: 75, // Dummy
              voltage: ec ? ec.voltage : 0,
              timestamp: ec ? ec.timestamp : (ph ? ph.timestamp : new Date().toISOString()),
            },
          ]
    },
    device_status: {
      ph_monitor: ph ? 'connected' : 'disconnected',
      ec_monitor: ec ? 'connected' : 'disconnected',
      ph_last_update: ph ? ph.timestamp : null,
      ec_last_update: ec ? ec.timestamp : null,
    },
  };

  sendJSON(res, response);
};
