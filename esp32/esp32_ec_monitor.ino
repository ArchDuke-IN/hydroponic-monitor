#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>

// ======================= USER CONFIGURATION =======================
const char* ssid = "kaustav kar";
const char* password = "Kaustav@896";

// Website Configuration
const char* serverUrl = "https://hydroponic-monitor.vercel.app/api/update-ec"; 

// --- PINS ---
const int PIN_POWER = 4;     // Powers the voltage divider (GPIO HIGH = 3.3V)
const int PIN_READ = 34;     // ADC reading of voltage across probe

// --- SETTINGS ---
const int SAMPLE_COUNT = 20;
const int READ_INTERVAL = 3000;

// --- LOGGING SETTINGS ---
const long WARMUP_TIME = 180000;
const long LOG_INTERVAL = 180000;
const int MAX_LOGS = 500;

// ======================= GLOBALS =======================
float R_known = 1000.0;      // Trimpot resistance in ohms — CHANGE THIS to your measured value
float K_cell = 1.0;          // Cell constant of your probe
float temperature = 25.0;    // Water temperature (°C) for compensation
float highBoost = 1.25;      // Correction factor for high EC ranges

float voltage = 0;
float probeResistance = 0;
float ecValue = 0;

// --- DATA LOGGING STORAGE ---
String timeLog[MAX_LOGS];
float ecLog[MAX_LOGS];
int logIndex = 0; 
unsigned long bootTime = 0;
unsigned long lastLogTime = 0;
unsigned long lastReadTime = 0; 

WebServer server(80);
Preferences preferences;

// ======================= HELPERS =======================

void sendECToCloud(float ec, float volt, float temp) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"device_id\":\"ESP32_EC\",";
    json += "\"ec_value\":" + String(ec) + ",";
    json += "\"voltage\":" + String(volt) + ",";
    json += "\"temperature\":" + String(temp);
    json += "}";

    int httpResponseCode = http.POST(json);

    if (httpResponseCode > 0) {
      Serial.println("Cloud Update Success.");
    } else {
      Serial.print("Error sending to cloud: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Cannot upload.");
  }
}

// ======================= MEDIAN FILTER =======================
float getMedianVoltage() {
  int rawValues[SAMPLE_COUNT];
  digitalWrite(PIN_POWER, HIGH); delay(50);  // Longer settle time for DIY probe
  for (int i = 0; i < SAMPLE_COUNT; i++) { rawValues[i] = analogRead(PIN_READ); delay(10); }
  digitalWrite(PIN_POWER, LOW);

  for (int i = 0; i < SAMPLE_COUNT - 1; i++) {
    for (int j = 0; j < SAMPLE_COUNT - i - 1; j++) {
      if (rawValues[j] > rawValues[j + 1]) {
        int temp = rawValues[j]; rawValues[j] = rawValues[j + 1]; rawValues[j + 1] = temp;
      }
    }
  }
  return rawValues[SAMPLE_COUNT / 2] * (3.3 / 4095.0);
}

// ======================= SENSOR LOGIC =======================
void updateSensor() {
  float v = getMedianVoltage();
  voltage = v;

  // Circuit: GPIO 4(3.3V) ── R_known(trimpot) ──┬── ADC(GPIO 34)
  //                                              └── Probe ── GND
  // ADC reads voltage across the probe (bottom of voltage divider)
  // V_adc = 3.3 * R_probe / (R_known + R_probe)
  // Therefore: R_probe = V_adc * R_known / (3.3 - V_adc)

  if (v > 3.2 || v < 0.01) {
    if (v > 3.2) {
      ecValue = 0;       // Probe disconnected / open circuit
      probeResistance = 999999;
    } else {
      ecValue = 99999;    // Short circuit
      probeResistance = 0;
    }
  } else {
    probeResistance = (v * R_known) / (3.3 - v);
    float conductance = 1.0 / probeResistance;
    float ec = conductance * K_cell * 1000000.0;

    // High-EC boost correction
    if (ec > 1800) ec = ec * highBoost;

    // Temperature compensation: EC ↑ 2% per °C above 25°C
    ec = ec / (1.0 + 0.019 * (temperature - 25.0));

    // Smart filter (smoothing)
    float diff = abs(ec - ecValue);
    if (ecValue == 0) ecValue = ec;
    else if (diff > 50) ecValue = (ecValue * 0.2) + (ec * 0.8);
    else ecValue = (ecValue * 0.95) + (ec * 0.05);

    Serial.print("Raw ADC: "); Serial.print((v / 3.3) * 4095);
    Serial.print(" | V: "); Serial.print(v, 3);
    Serial.print(" | R_probe: "); Serial.print(probeResistance);
    Serial.print(" ohm | EC: "); Serial.println(ecValue);
  }
}

// ======================= LOGGING LOGIC =======================
void handleLogging() {
  unsigned long now = millis();
  if (now < WARMUP_TIME) return; 
  if (now - lastLogTime > LOG_INTERVAL) {
    lastLogTime = now;
    if (logIndex >= MAX_LOGS) {
      for (int i = 0; i < MAX_LOGS - 1; i++) {
        timeLog[i] = timeLog[i+1];
        ecLog[i] = ecLog[i+1];
      }
      logIndex = MAX_LOGS - 1;
    }
    String timeLabel = String((now / 60000)) + "m"; 
    timeLog[logIndex] = timeLabel;
    ecLog[logIndex] = ecValue;
    logIndex++;
    Serial.println("Logged: " + timeLabel + " -> " + String(ecValue) + " uS/cm");
    sendECToCloud(ecValue, voltage, temperature);
  }
}

// ======================= WEBPAGE =======================
String getHTML() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#f4f4f9;}";
  html += ".box{background:white;padding:20px;border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);max-width:600px;margin:auto;}";
  html += "h1{color:#333;} .val{font-size:40px;color:#00796b;font-weight:bold;}";
  html += ".debug{font-size:12px;color:#888;margin:5px 0;}";
  html += "button{padding:10px 20px;background:#00796b;color:white;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += "input{padding:8px;width:100px;border:1px solid #ccc;border-radius:5px;margin:3px;}";
  html += "label{font-size:13px;display:inline-block;width:140px;text-align:right;margin-right:8px;}";
  html += "</style></head><body>";

  html += "<div class='box'><h1>EC Monitor</h1>";
  html += "<div class='val'><span id='ec'>--</span> <span style='font-size:20px'>µS/cm</span></div>";
  html += "<p class='debug'>Voltage: <span id='v'>--</span> V | Probe R: <span id='rp'>--</span> Ω</p>";
  html += "<p class='debug'>R_known: " + String(R_known) + " Ω | K_cell: " + String(K_cell, 4) + " | Temp: " + String(temperature, 1) + "°C</p>";

  html += "<canvas id='myChart' height='250'></canvas>";
  html += "<br><a href='/download'><button>Download CSV</button></a>";

  // --- Calibration Section ---
  html += "<hr><h3>Calibration</h3>";

  // 1. Set R_known (trimpot value)
  html += "<form action='/setr' method='POST' style='margin:10px 0'>";
  html += "<label>R_known (ohms):</label><input name='rval' value='" + String(R_known, 0) + "'><br>";
  html += "<label>Temperature (°C):</label><input name='temp' value='" + String(temperature, 1) + "'><br>";
  html += "<button type='submit'>Save Settings</button></form>";

  // 2. K_cell calibration with known standard
  html += "<form action='/calibrate' method='POST' style='margin:10px 0'>";
  html += "<label>EC Standard (µS/cm):</label><input name='target' placeholder='1413' value='1413'><br>";
  html += "<button>Calibrate K_cell</button></form>";

  // 3. K_cell manual adjustment
  html += "<form action='/setk' method='POST' style='margin:10px 0'>";
  html += "<label>K_cell manual:</label><input name='kval' value='" + String(K_cell, 4) + "'><br>";
  html += "<button>Set K_cell</button></form>";

  html += "<p style='font-size:11px;color:#aaa'>Step 1: Measure trimpot with multimeter → set R_known<br>";
  html += "Step 2: Dip probe in EC standard (e.g. 1413 µS/cm) → click Calibrate<br>";
  html += "Step 3: Verify reading matches standard</p>";

  html += "</div>";

  html += "<script>";
  html += "var ctx = document.getElementById('myChart').getContext('2d');";
  html += "var chart = new Chart(ctx, {type: 'line', data: {labels: [], datasets: [{label: 'EC (µS/cm)', borderColor: '#00796b', data: []}]}, options: {scales: {y: {beginAtZero: false}}}});";
  html += "setInterval(() => {";
  html += " fetch('/json').then(r=>r.json()).then(d => {";
  html += "   document.getElementById('ec').innerText = d.currEC.toFixed(0);";
  html += "   document.getElementById('v').innerText = d.currV.toFixed(3);";
  html += "   document.getElementById('rp').innerText = d.probeR.toFixed(0);";
  html += "   chart.data.labels = d.labels;";
  html += "   chart.data.datasets[0].data = d.data;";
  html += "   chart.update();";
  html += " });";
  html += "}, 3000);</script></body></html>";
  return html;
}

// ======================= HANDLERS =======================
void handleJSON() {
  String json = "{";
  json += "\"currEC\":" + String(ecValue) + ",";
  json += "\"currV\":" + String(voltage) + ",";
  json += "\"probeR\":" + String(probeResistance) + ",";
  json += "\"labels\":[";
  for(int i=0; i<logIndex; i++) { json += "\"" + timeLog[i] + "\""; if(i<logIndex-1) json += ","; }
  json += "],";
  json += "\"data\":[";
  for(int i=0; i<logIndex; i++) { json += String(ecLog[i]); if(i<logIndex-1) json += ","; }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleDownload() {
  String csv = "Time (Minutes),EC Value (uS/cm)\n";
  for(int i=0; i<logIndex; i++) {
    csv += timeLog[i] + "," + String(ecLog[i]) + "\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=hydro_data.csv");
  server.send(200, "text/csv", csv);
}

// Calibrate K_cell using a known EC standard solution
void handleCalibrate() {
  if (server.hasArg("target") && voltage > 0.1 && voltage < 3.2) {
    float probeR = (voltage * R_known) / (3.3 - voltage);
    float target = server.arg("target").toFloat();
    // Calculate K_cell needed to make the reading match the target
    float rawConductance = (1.0 / probeR) * 1000000.0;
    float tempComp = (1.0 + 0.019 * (temperature - 25.0));
    K_cell = (target * tempComp) / rawConductance;
    preferences.putFloat("k_cell", K_cell);
    ecValue = target;
    Serial.print("Calibrated! K_cell = ");
    Serial.println(K_cell, 4);
  }
  server.sendHeader("Location", "/"); server.send(303);
}

// Set R_known (trimpot resistance) and temperature
void handleSetR() {
  if (server.hasArg("rval")) {
    float newR = server.arg("rval").toFloat();
    if (newR > 10 && newR < 10000000) {
      R_known = newR;
      preferences.putFloat("r_known", R_known);
      Serial.print("R_known set to: ");
      Serial.println(R_known);
    }
  }
  if (server.hasArg("temp")) {
    float newT = server.arg("temp").toFloat();
    if (newT > 0 && newT < 60) {
      temperature = newT;
      preferences.putFloat("temperature", temperature);
    }
  }
  server.sendHeader("Location", "/"); server.send(303);
}

// Manually set K_cell
void handleSetK() {
  if (server.hasArg("kval")) {
    K_cell = server.arg("kval").toFloat();
    preferences.putFloat("k_cell", K_cell);
    Serial.print("K_cell manually set to: ");
    Serial.println(K_cell, 4);
  }
  server.sendHeader("Location", "/"); server.send(303);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_POWER, OUTPUT); pinMode(PIN_READ, INPUT); digitalWrite(PIN_POWER, LOW);

  preferences.begin("ec-meter", false);
  R_known = preferences.getFloat("r_known", 1000.0);
  K_cell = preferences.getFloat("k_cell", 1.0);
  highBoost = preferences.getFloat("boost", 1.25);
  temperature = preferences.getFloat("temperature", 25.0);

  bootTime = millis();

  Serial.println("=== EC Monitor Boot ===");
  Serial.print("R_known: "); Serial.print(R_known); Serial.println(" ohms");
  Serial.print("K_cell: "); Serial.println(K_cell, 4);
  Serial.print("Temperature: "); Serial.println(temperature);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  server.on("/json", handleJSON);
  server.on("/download", handleDownload); 
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/setr", HTTP_POST, handleSetR);
  server.on("/setk", HTTP_POST, handleSetK);
  server.begin();
}

void loop() {
  server.handleClient();
  if (millis() - lastReadTime > READ_INTERVAL) {
    updateSensor();
    lastReadTime = millis();
  }
  handleLogging();
}
