# Hydroponic Monitoring System — Complete System Overview

## 1. System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     WEB DASHBOARD (Browser)                      │
│  https://hydroponic-monitor.vercel.app                           │
│  Public/index.html → script.js → fetch() every 60s              │
└──────────────────────────┬──────────────────────────────────────┘
                           │ GET /api/get-data
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                    VERCELL SERVERLESS API                         │
│                                                                   │
│  /api/get-data.js    ← Dashboard polls this every 60s            │
│  /api/update-ph.js   ← ESP32 #1 posts pH/temp/humidity data      │
│  /api/update-ec.js   ← ESP32 #2 posts EC data                    │
│  /api/health.js      ← Health check                              │
│                                                                   │
│  Backend: Node.js + @neondatabase/serverless                     │
│  Database: Neon (PostgreSQL) — Tables: ph_readings, ec_readings │
└──────────┬──────────────────────────────┬───────────────────────┘
           │ POST /api/update-ph          │ POST /api/update-ec
           ▼                              ▼
┌─────────────────────────┐   ┌─────────────────────────┐
│   ESP32 #1 — pH Monitor │   │   ESP32 #2 — EC Monitor │
│                         │   │                         │
│  Pin 14: DHT11          │   │  Pin 34: EC Probe       │
│  Pin 27: DS18B20 (H2O)  │   │  Pin 4:  Power Ctrl     │
│  Pin 34: pH Sensor      │   │                         │
│                         │   │                         │
│  Uploads every 60s      │   │  Uploads every 180s     │
└─────────────────────────┘   └─────────────────────────┘
```

---

## 2. Hardware Components

### ESP32 #1 — pH/Temperature Monitor (esp32_ph_monitor.ino)

| Component | Pin | Type | Purpose |
|-----------|-----|------|---------|
| **DHT11** | GPIO 14 | Digital (DHT lib) | Ambient temperature + humidity |
| **DS18B20** | GPIO 27 | OneWire (DallasTemp lib) | Water temperature (stainless steel probe, water-resistant) |
| **pH Sensor** | GPIO 34 | Analog (ADC) | pH level of nutrient solution |

The DS18B20 requires a **4.7kΩ pull-up resistor** between the data line (GPIO 27) and 3.3V.

### ESP32 #2 — EC/TDS Monitor (esp32_ec_monitor.ino)

| Component | Pin | Type | Purpose |
|-----------|-----|------|---------|
| **Trimpot (R_known)** | Between GPIO 4 & GPIO 34 | Variable resistor | Forms voltage divider with probe |
| **Nichrome Wire Probe** | GPIO 34 → GND (through probe) | DIY EC probe | Measures conductivity of nutrient solution |
| **Power Control** | GPIO 4 | Digital output | Powers divider ON only during readings |

#### EC Circuit Diagram

```
GPIO 4 (3.3V when HIGH)
    │
    ├── Trimpot (R_known — variable, e.g. 10kΩ pot)
    │       │
    │       └──┬── GPIO 34 (ADC read)
    │          │
    │          └── Nichrome wire probe ──── GND
```

The voltage divider works as:
- `V_adc = 3.3V × R_probe / (R_known + R_probe)`
- Therefore: `R_probe = V_adc × R_known / (3.3 − V_adc)`
- Conductivity: `EC (µS/cm) = (1 / R_probe) × K_cell × 1,000,000`

**R_known** = your trimpot's actual resistance (measure with multimeter).\
**K_cell** = cell constant of your probe (determined by wire spacing — typically 0.5 to 5.0 for DIY probes).

---

## 3. Database Schema (Neon PostgreSQL)

### Table: `ph_readings`

| Column | Type | Source Sensor | Description |
|--------|------|-------------|-------------|
| `id` | SERIAL PK | Auto | Auto-increment ID |
| `temp1` | FLOAT | DHT11 (GPIO 14) | Ambient temperature (°C) |
| `hum1` | FLOAT | DHT11 (GPIO 14) | Humidity (%) |
| `temp2` | FLOAT | DS18B20 (GPIO 27) | Water temperature (°C) |
| `hum2` | FLOAT | Unused | Always 0 |
| `ph_val` | FLOAT | pH Sensor (GPIO 34) | pH reading |
| `created_at` | TIMESTAMP | Auto | When data was recorded |

### Table: `ec_readings`

| Column | Type | Source | Description |
|--------|------|--------|-------------|
| `id` | SERIAL PK | Auto | Auto-increment ID |
| `ec_value` | FLOAT | EC Probe | Conductivity (µS/cm) |
| `voltage` | FLOAT | EC Probe | Raw voltage (V) |
| `temperature` | FLOAT | Manual config | Used for temp compensation |
| `created_at` | TIMESTAMP | Auto | When data was recorded |

---

## 4. Data Flow (Step by Step)

### pH/Temp Data Path (ESP32 #1):

```
1. ESP32 #1 reads sensors every 60 seconds:
   - DHT11 → ambientTemp (°C), humidity (%)
   - DS18B20 → waterTemp (°C)
   - pH Sensor → phValue

2. ESP32 sends HTTP POST to:
   POST https://hydroponic-monitor.vercel.app/api/update-ph
   Body: {
     "temp1": 25.3,    // DHT11 ambient temp
     "hum1": 65.0,     // DHT11 humidity
     "temp2": 24.1,    // DS18B20 water temp
     "hum2": 0,        // Unused
     "ph_val": 6.82    // pH reading
   }

3. Vercel API stores in Neon DB:
   INSERT INTO ph_readings (temp1, hum1, temp2, hum2, ph_val, created_at)
   VALUES (25.3, 65.0, 24.1, 0, 6.82, NOW())

4. Dashboard polls every 60 seconds:
   GET https://hydroponic-monitor.vercel.app/api/get-data

5. API returns latest row from each table as JSON
```

### EC Data Path (ESP32 #2):

```
1. ESP32 #2 reads EC probe every 3 seconds (logs every 3 minutes):
   - Powers probe ON → reads median voltage → calculates EC
   - Powers probe OFF

2. ESP32 sends HTTP POST every 3 minutes:
   POST https://hydroponic-monitor.vercel.app/api/update-ec
   Body: {
     "ec_value": 1450,
     "voltage": 2.15,
     "temperature": 25.0
   }
```

---

## 5. Dashboard Field Mapping

| Dashboard Card | Label | Source Data | Sensor |
|---------------|-------|-------------|--------|
| Temperature | Zone A | `ph_readings.temp1` | DHT11 (ambient) |
| Temperature | Zone B | `ph_readings.temp2` | DS18B20 (water) |
| pH Level | Zone A | `ph_readings.ph_val` | pH Probe |
| pH Level | Zone B | Same as Zone A | Duplicate display |
| Humidity | (in summary) | `ph_readings.hum1` | DHT11 |
| EC | EC | `ec_readings.ec_value` | EC Probe |
| TDS | TDS | Calculated: EC × 0.5 | Derived from EC |
| Soil Moisture | — | `null` | No sensor connected |
| Light Intensity | — | `null` | No sensor connected |
| Water Level | — | `null` | No sensor connected |
| Flow Rate | — | `null` | No sensor connected |

---

## 6. Issues Found & Fixed (June 2026)

### Issue 1: Wrong sensor library for DS18B20
- **Before**: Both temperature sensors were initialized as DHT11. The DS18B20 on GPIO 27 returned NaN when read with the DHT library.
- **Effect**: All uploads were blocked because the code required `!isnan(temp1) && !isnan(temp2)`. Since the DS18B20 always returned NaN, **nothing was uploaded at all** — not even pH or DHT11 data.
- **Fix**: Added `OneWire` and `DallasTemperature` libraries. DS18B20 now reads independently on GPIO 27. Each sensor is read with its correct protocol.

### Issue 2: Upload guard blocked all data on single sensor failure
- **Before**: `if (!isnan(t1) && !isnan(t2)) { sendDataToCloud(...); }`
- **Effect**: If either temperature sensor failed, ALL data (including pH) was discarded.
- **Fix**: Sensors now upload independently. A failed sensor sends `NaN` but does not block other sensor data.

### Issue 3: Dummy/hardcoded values in API response
- **Before**: `light_intensity: 500`, `water_level: 75`, `soil_moisture: hum2`
- **Effect**: Dashboard showed fake data as if real sensors existed.
- **Fix**: Unavailable sensors now return `null`. Dashboard cards for missing sensors will show "—" clearly indicating no data.

### Issue 4: ESP32 local web page still showed "Location 1 / Location 2"
- **Before**: Local ESP32 web page labeled both sensors as generic "Location 1" and "Location 2".
- **Fix**: Updated labels to "Ambient (DHT11)" and "Water Temp (DS18B20)" for clarity.

### Issue 5: EC sensor wrong values with trimpot setup
- **Before**: Code assumed a fixed 1kΩ resistor (R_known = 1000) and default cell constant (K_cell = 1.0). The trimpot could be any value (10k, 100k, etc.) and the DIY nichrome probe has unknown cell constant.
- **Effect**: EC readings were orders of magnitude off. No way to calibrate R_known (trimpot value) or K_cell (probe constant) via web UI.
- **Fix**: Added dedicated calibration forms in the ESP32 web interface:
  - **Set R_known**: Enter your trimpot's measured resistance
  - **Calibrate K_cell**: Dip probe in known EC standard → clicks Calibrate
  - **Manual K_cell**: Direct entry for fine-tuning
  - Serial monitor now shows raw ADC, voltage, probe resistance, and EC for debugging

---

## 7. Testing & Verification Checklist

### On the ESP32 #1 (pH Monitor):
- [ ] ESP32 serial monitor shows `Cloud Upload Success. Code: 200` every 60 seconds
- [ ] ESP32 local web page at `http://[ESP32-IP]/` shows correct sensor readings
- [ ] DHT11 shows valid temperature (15-35°C) and humidity (30-90%)
- [ ] DS18B20 shows valid water temperature (15-35°C)
- [ ] pH sensor shows value in range 4.0-9.0

### On the Web Dashboard:
- [ ] Dashboard loads at `https://hydroponic-monitor.vercel.app`
- [ ] Temperature Zone A matches DHT11 reading
- [ ] Temperature Zone B (Water) matches DS18B20 reading
- [ ] pH value updates every 60 seconds
- [ ] EC/TDS values display from ESP32 #2
- [ ] Missing sensors (soil moisture, light, water level) show "—"
- [ ] Connection status shows "Connected" when recent data exists

### API Testing:
```bash
# Check health
curl https://hydroponic-monitor.vercel.app/api/health

# Get latest data
curl https://hydroponic-monitor.vercel.app/api/get-data

# Simulate pH upload
curl -X POST https://hydroponic-monitor.vercel.app/api/update-ph \
  -H "Content-Type: application/json" \
  -d '{"temp1":25.3,"hum1":65,"temp2":24.1,"hum2":0,"ph_val":6.82}'

# Simulate EC upload  
curl -X POST https://hydroponic-monitor.vercel.app/api/update-ec \
  -H "Content-Type: application/json" \
  -d '{"ec_value":1450,"voltage":2.15,"temperature":25.0}'
```

---

## 8. How to Flash / Update ESP32

### ESP32 #1 — pH Monitor:
1. Install required libraries in Arduino IDE:
   - `DHT sensor library` by Adafruit
   - `OneWire` by Jim Studt
   - `DallasTemperature` by Miles Burton
2. Open `esp32/esp32_ph_monitor.ino`
3. Verify Wi-Fi credentials (lines 9-10) match your network
4. Verify server URL (line 13) points to your Vercel deployment
5. Select board: `ESP32 Dev Module`
6. Upload via USB

### ESP32 #2 — EC Monitor:
1. Required libraries: none extra (uses built-in WiFi, HTTPClient, Preferences)
2. Open `esp32/esp32_ec_monitor.ino`
3. Update Wi-Fi credentials (lines 7-8)
4. Update server URL (line 11)
5. Upload via USB

---

## 8b. EC Sensor Calibration Procedure

### Step 1: Measure the Trimpot
Use a multimeter to measure the resistance between:
- The center wiper pin and the outer pin that connects to GPIO 4
- (Or measure across the entire trimpot if unsure — set your multimeter to Ω)

Enter this value in the ESP32 web page at `http://[ESP32-IP]/` under **Set R_known**.

**Typical values**: A 10kΩ trimpop set to ~4.7kΩ mid-range, or a 100kΩ set to ~47kΩ.

### Step 2: No Multimeter? Estimate R_known
1. Put the probe in **air** (open circuit). The ADC should read near 3.3V.
2. **Short the probe wires together**. The ADC should read near 0V.
3. If neither is true, adjust the trimpot until open-circuit reads ~3.0V+ and short-circuit reads ~0.1V-.
4. Then estimate R_known from the trimpot's labeled max value (e.g., if it's a 10kΩ pot at half-turn, try 5000).

### Step 3: Calibrate K_cell with Standard Solution
1. **Get an EC calibration standard** (recommended: 1413 µS/cm solution, available on Amazon/electronics shops for ~₹200)
2. OR make a DIY standard: dissolve 0.745g of KCl (or common salt) in 1L distilled water = ~1413 µS/cm at 25°C
3. Dip probe in the solution
4. Open ESP32 web page → **Calibrate** → enter the standard value → click Calibrate
5. Reading should now match the standard

### Step 4: No Standard Solution? Approximate
Without a standard, use these reference EC values:
| Solution | Approx EC (µS/cm) |
|----------|-------------------|
| Distilled water | 0-10 |
| Tap water | 100-800 |
| Hydroponic nutrient (weak) | 500-1000 |
| Hydroponic nutrient (normal) | 1200-1800 |
| Salt water (1 tsp salt / 1L water) | ~5000 |
| Seawater | ~50000 |

Dip probe in tap water of known EC (check your local water bill or search online), or just set K_cell to make the reading match a reasonable value for your solution.

### Serial Monitor Debug Output
When the ESP32 is connected via USB, open **Arduino IDE → Tools → Serial Monitor** at 115200 baud. You'll see:
```
Raw ADC: 2048 | V: 1.650 | R_probe: 1000 ohm | EC: 1000.00
```
This raw data helps diagnose connection issues.

---

## 9. Project File Structure

```
hydrophonic system/
├── api/                          # Vercel serverless backend
│   ├── _helpers.js               # CORS + response helpers
│   ├── get-data.js               # Dashboard data endpoint (GET)
│   ├── health.js                 # Health check (GET)
│   ├── update-ec.js              # EC data receiver (POST)
│   └── update-ph.js              # pH data receiver (POST)
├── esp32/                        # ESP32 firmware
│   ├── esp32_ph_monitor.ino      # pH + temp sensors
│   └── esp32_ec_monitor.ino      # EC sensor
├── lib/
│   └── db.js                     # Neon DB connection
├── public/                       # Static frontend
│   ├── index.html                # Dashboard UI
│   ├── script.js                 # Dashboard logic
│   ├── styles.css                # Styling
│   └── logo.png                  # University logo
├── .env                          # Environment variables
├── package.json
├── vercel.json                   # Vercel deployment config
└── SYSTEM-OVERVIEW.md            # This document
```

---

## 10. Environment Variables (Vercel)

| Variable | Value | Purpose |
|----------|-------|---------|
| `DATABASE_CONNECTION_STRING` | `postgresql://...` | Neon PostgreSQL connection string |

These are set in the Vercel project dashboard under **Settings → Environment Variables**, not in code.

---

## 11. Known Limitations

1. **No soil moisture sensor** — Dashboard shows "—" for soil moisture
2. **No light sensor** — Dashboard shows "—" for light intensity
3. **No flow meter** — Dashboard shows "—" for flow rate
4. **No water level sensor** — Dashboard shows "—" for water level
5. **Historical data** stored client-side only (last 100 points in browser memory) — resets on page refresh
6. **pH Zone B** is a duplicate of Zone A — only one pH sensor is connected
7. **EC probe warm-up**: First 3 minutes of data after ESP32 #2 boot are discarded

---

*Document generated June 2026 — For the visiting engineer from Adamas University*
