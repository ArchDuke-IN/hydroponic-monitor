const fs = require('fs');
const path = require('path');

/**
 * Simple data store for ESP32 sensor readings.
 * Uses /tmp/ for persistence between invocations on Vercel.
 * /tmp/ is shared within the same function instance (warm starts).
 */

const DATA_FILE = path.join('/tmp', 'sensor_data.json');

function getDefaultData() {
  return {
    ph_monitor: null,    // Latest reading from ESP32 #1
    ec_monitor: null,    // Latest reading from ESP32 #2
    ph_history: [],      // Last 100 pH monitor readings
    ec_history: [],      // Last 100 EC monitor readings
  };
}

function readData() {
  try {
    if (fs.existsSync(DATA_FILE)) {
      const raw = fs.readFileSync(DATA_FILE, 'utf8');
      return JSON.parse(raw);
    }
  } catch (e) {
    console.error('Error reading data file:', e.message);
  }
  return getDefaultData();
}

function writeData(data) {
  try {
    fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 2), 'utf8');
  } catch (e) {
    console.error('Error writing data file:', e.message);
  }
}

function savePHReading(reading) {
  const data = readData();
  const entry = {
    ...reading,
    timestamp: new Date().toISOString(),
  };
  data.ph_monitor = entry;
  data.ph_history.push(entry);
  if (data.ph_history.length > 100) {
    data.ph_history = data.ph_history.slice(-100);
  }
  writeData(data);
  return entry;
}

function saveECReading(reading) {
  const data = readData();
  const entry = {
    ...reading,
    timestamp: new Date().toISOString(),
  };
  data.ec_monitor = entry;
  data.ec_history.push(entry);
  if (data.ec_history.length > 100) {
    data.ec_history = data.ec_history.slice(-100);
  }
  writeData(data);
  return entry;
}

function getLatest() {
  return readData();
}

module.exports = { savePHReading, saveECReading, getLatest };
