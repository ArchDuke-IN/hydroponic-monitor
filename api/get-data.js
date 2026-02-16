const { getLatest } = require('./_db');
const { handleOptions, sendJSON } = require('./_helpers');

/**
 * GET /api/get-data
 * Returns combined latest data from both ESP32 devices
 * This is what the dashboard calls every 60 seconds
 */
module.exports = (req, res) => {
  if (handleOptions(req, res)) return;

  const store = getLatest();
  const ph = store.ph_monitor;
  const ec = store.ec_monitor;

  // Calculate TDS from EC (TDS ≈ EC × 0.64)
  const tdsValue = ec ? Math.round(ec.ec_value * 0.64) : null;

  // Convert EC from µS/cm to mS/cm
  const ecMsCm = ec ? parseFloat((ec.ec_value / 1000).toFixed(2)) : null;

  const response = {
    success: true,
    timestamp: new Date().toISOString(),
    data: {
      plant_monitoring: ph
        ? [
            {
              temperature: ph.temp1,
              humidity: ph.hum1,
              soil_moisture: ph.hum2,
              light_intensity: 500, // Not measured by your sensors
              timestamp: ph.timestamp,
            },
          ]
        : [],

      water_quality:
        ph || ec
          ? [
              {
                ph_value: ph ? ph.ph_val : null,
                tds_value: tdsValue,
                ec_value: ecMsCm,
                water_temp: ph ? ph.temp2 : ec ? ec.temperature : null,
                water_level: 75, // Not measured by your sensors
                voltage: ec ? ec.voltage : null,
                timestamp: ec ? ec.timestamp : ph ? ph.timestamp : null,
              },
            ]
          : [],
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
