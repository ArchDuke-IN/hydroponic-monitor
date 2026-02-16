# Hydroponic Monitoring System# Hydroponic Monitoring System Dashboard



Real-time IoT dashboard for hydroponic farming with ESP32 sensor integration. Built for Adamas University.A professional, minimalistic IoT dashboard for monitoring hydroponic systems using ESP32 microcontroller.



## Architecture## Features



```### 📊 Real-time Monitoring

ESP32 #1 (pH Monitor)  ──POST──▶  /api/update-ph- **Soil Moisture** - Tracks moisture levels in Zone A and Zone B

ESP32 #2 (EC Monitor)  ──POST──▶  /api/update-ec- **pH Levels** - Monitors acidity/alkalinity for optimal plant growth

Dashboard (Browser)    ◀──GET───  /api/get-data- **Temperature** - Ambient temperature monitoring for both zones

```- **EC & TDS** - Electrical conductivity and total dissolved solids tracking

- **Flow Rate** - Nutrient solution flow monitoring

**Frontend:** HTML / CSS / JS (static, in `public/`)  

**Backend:** Node.js serverless functions (in `api/`)  ### 📈 Data Visualization

**Hosting:** Vercel (free tier)- Historical trend graphs for EC and pH levels

- Real-time progress bars with color-coded status indicators

## ESP32 Sensors- Summary cards showing average values across all sensors

- Multiple timeframe views (24H, 7D, 30D)

| Device | Sensors | Data Fields |

|--------|---------|-------------|### 🎯 Status Indicators

| ESP32 #1 | 2× DHT11 + pH | `temp1, hum1, temp2, hum2, ph_val` |- **Normal** (Green) - Values within optimal range

| ESP32 #2 | EC sensor | `ec_value, voltage, temperature` |- **Warning** (Orange) - Values approaching critical levels

- **Danger** (Red) - Values outside safe operating range

## Deploy to Vercel

### 💾 Data Export

```bash- Export sensor data to CSV format

npm i -g vercel- Print-friendly report generation

vercel- Historical data storage (last 100 readings)

```

## Design Principles

That's it. Vercel auto-detects the project structure.

### Minimalistic & Professional

After deploying, copy your Vercel URL (e.g. `https://hydroponic-xyz.vercel.app`) and update your ESP32 code:- Clean, modern interface using Inter font family

- Subtle shadows and smooth transitions

**ESP32 #1 — line 13:**- Consistent color palette and spacing

```cpp- Professional presentation suitable for research conferences

const char* serverUrl = "https://hydroponic-xyz.vercel.app/api/update-ph";

```### Research-Ready

- Clear data visualization

**ESP32 #2 — line 9:**- Scientific units displayed (μS/cm, pH, °C, ppm, L/min)

```cpp- Timestamp tracking for data correlation

const char* serverUrl = "https://hydroponic-xyz.vercel.app/api/update-ec";- Export functionality for further analysis

```

## Technical Specifications

Upload both ESP32 boards. Done.

### Sensor Optimal Ranges

## API Endpoints- **Moisture**: 40-70% (Min: 20%, Max: 80%)

- **pH**: 5.5-6.5 (Min: 4.0, Max: 9.0)

| Method | Path | Description |- **Temperature**: 20-28°C (Min: 15°C, Max: 35°C)

|--------|------|-------------|- **EC**: 1200-1800 μS/cm (Min: 500, Max: 2500)

| `POST` | `/api/update-ph` | Receive pH monitor data |- **TDS**: 700-1000 ppm (Min: 300, Max: 1500)

| `POST` | `/api/update-ec` | Receive EC monitor data |- **Flow Rate**: 1.5-3 L/min (Min: 0.5, Max: 5)

| `GET`  | `/api/get-data`  | Get latest data for dashboard |

| `GET`  | `/api/health`    | Health check |### Update Intervals

- Sensor Data: 5 seconds

## Local Development- Graph Refresh: 30 seconds

- ESP32 Connection Status: 1 second

```bash

npm install## Usage

npx vercel dev

```### Opening the Dashboard

Simply open `index.html` in any modern web browser.

Opens at `http://localhost:3000`

### Controls

## Project Structure- **Refresh Data** - Manually refresh all sensor readings

- **Export CSV** - Download historical data as CSV file

```- **Print Report** - Generate printer-friendly report

├── public/            Frontend (static files)

│   ├── index.html### ESP32 Integration

│   ├── styles.cssThe dashboard is designed to receive real-time data from ESP32 via:

│   ├── script.js- WebSocket connection

│   └── logo.png- HTTP REST API

├── api/               Backend (Vercel serverless functions)- MQTT protocol

│   ├── update-ph.js   ← ESP32 #1 sends data here

│   ├── update-ec.js   ← ESP32 #2 sends data here*Note: Current version uses simulated data for demonstration.*

│   ├── get-data.js    ← Dashboard fetches from here

│   ├── health.js      ← Health check## File Structure

│   ├── _db.js         ← Data storage layer```

│   └── _helpers.js    ← CORS / response helpershydrophonic system/

├── vercel.json        Vercel config├── index.html          # Main dashboard interface

├── package.json├── styles.css          # Professional styling

└── README.md├── script.js           # Data management and visualization

```├── logo.png           # Adamas University logo

└── README.md          # Documentation

## Credits```



Adamas University, Kolkata — Research Conference 2026  ## Browser Compatibility

© 2026 ARCHDUKE.IN- Chrome 90+

- Firefox 88+
- Safari 14+
- Edge 90+

## Responsive Design
Fully responsive layout that works on:
- Desktop (1920x1080 and above)
- Laptop (1366x768 and above)
- Tablet (768px and above)
- Mobile (320px and above)

## Conference Presentation Tips
1. Use full-screen mode (F11) for presentations
2. Demonstrate real-time updates (auto-refresh every 5 seconds)
3. Export CSV to show data collection capabilities
4. Show responsive design on different screen sizes
5. Highlight professional UI/UX design elements

## Future Enhancements
- [ ] Real ESP32 integration
- [ ] Database backend for long-term storage
- [ ] Email/SMS alerts for critical values
- [ ] Multi-zone support (expandable to more zones)
- [ ] Advanced analytics and predictions
- [ ] Mobile app version

## Credits
Developed for Adamas University, Kolkata  
Research Conference 2026  
© 2026 ARCHDUKE.IN - All Rights Reserved

## License
Proprietary - For academic and research purposes only
