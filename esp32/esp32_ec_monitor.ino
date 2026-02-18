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
const int PIN_POWER = 4;
const int PIN_READ = 34;

// --- SETTINGS ---
const int SAMPLE_COUNT = 20; // Hardware Filter
const int READ_INTERVAL = 3000; // Update screen every 3 seconds

// --- LOGGING SETTINGS ---
const long WARMUP_TIME = 180000; // 3 Minutes (Ignore data during this time)
const long LOG_INTERVAL = 180000; // 3 Minutes (Log data every 3 mins)
const int MAX_LOGS = 500; // Store last 500 readings (~24 hours of data)

// ======================= GLOBALS =======================
float R_known = 1000.0;      
float K_cell = 1.0;          
float temperature = 25.0;    
float highBoost = 1.25;      

float voltage = 0;
float ecValue = 0; // Smooth Value

// --- DATA LOGGING STORAGE ---
String timeLog[MAX_LOGS]; // Stores timestamps
float ecLog[MAX_LOGS]; // Stores EC values
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

    // Construct JSON Payload
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
  digitalWrite(PIN_POWER, HIGH); delay(20); 
  for (int i = 0; i < SAMPLE_COUNT; i++) { rawValues[i] = analogRead(PIN_READ); delay(5); }
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
   
  if (v > 3.2 || v < 0.1) {
    if (v > 3.2) ecValue = 0; else ecValue = 99999;
  } else {
    float r = (v * R_known) / (3.3 - v);
    float ec = (1.0 / r) * K_cell * 1000000.0;
    if (ec > 1800) ec = ec * highBoost;
    ec = ec / (1.0 + 0.019 * (temperature - 25.0));

    // Smart Filter
    float diff = abs(ec - ecValue);
    if (ecValue == 0) ecValue = ec;
    else if (diff > 50) ecValue = (ecValue * 0.2) + (ec * 0.8);
    else ecValue = (ecValue * 0.95) + (ec * 0.05);
  }
}

// ======================= LOGGING LOGIC =======================
void handleLogging() {
  unsigned long now = millis();

  // 1. Check if we are still in WARMUP period (First 3 mins)
  if (now < WARMUP_TIME) return; 

  // 2. Check if it is time to Log (Every 3 mins)
  if (now - lastLogTime > LOG_INTERVAL) {
    lastLogTime = now;

    // Shift arrays if full (Circular Buffer logic)
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
    
    Serial.println("Data Logged: " + timeLabel + " -> " + String(ecValue));

    // --- SEND TO WEBSITE ---
    sendECToCloud(ecValue, voltage, temperature);
  }
}

// ======================= WEBPAGE GENERATOR =======================
String getHTML() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  // Load Chart.js from internet
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#f4f4f9;}";
  html += ".box{background:white;padding:20px;border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);max-width:600px;margin:auto;}";
  html += "h1{color:#333;} .val{font-size:40px;color:#00796b;font-weight:bold;}";
  html += "button{padding:10px 20px;background:#00796b;color:white;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += "</style></head><body>";

  html += "<div class='box'><h1>Hydro Monitor</h1>";
  html += "<div class='val'><span id='ec'>--</span> <span style='font-size:20px'>µS/cm</span></div>";
  html += "<p>Voltage: <span id='v'>--</span> V</p>";
   
  // THE CHART CANVAS
  html += "<canvas id='myChart'></canvas>";

  html += "<br><a href='/download'><button>Download Excel Data</button></a>";
   
  html += "<hr><h3>Calibration</h3><form action='/calibrate' method='POST'>";
  html += "<input name='target' placeholder='1413' style='padding:8px;width:100px;'> <button>Calibrate</button></form>";
   
  html += "</div>";

  // JAVASCRIPT FOR GRAPH
  html += "<script>";
  html += "var ctx = document.getElementById('myChart').getContext('2d');";
  html += "var chart = new Chart(ctx, {type: 'line', data: {labels: [], datasets: [{label: 'EC Value', borderColor: '#00796b', data: []}]}, options: {scales: {y: {beginAtZero: false}}}});";
   
  html += "setInterval(() => {";
  html += " fetch('/json').then(r=>r.json()).then(d => {";
  html += " document.getElementById('ec').innerText = d.currEC.toFixed(0);";
  html += " document.getElementById('v').innerText = d.currV.toFixed(2);";
  // Update Chart
  html += " chart.data.labels = d.labels;";
  html += " chart.data.datasets[0].data = d.data;";
  html += " chart.update();";
  html += " });";
  html += "}, 3000);</script></body></html>";

  return html;
}

// JSON API for updating the Graph
void handleJSON() {
  String json = "{";
  json += "\"currEC\":" + String(ecValue) + ",";
  json += "\"currV\":" + String(voltage) + ",";
   
  json += "\"labels\":[";
  for(int i=0; i<logIndex; i++) { json += "\"" + timeLog[i] + "\""; if(i<logIndex-1) json += ","; }
  json += "],";
   
  json += "\"data\":[";
  for(int i=0; i<logIndex; i++) { json += String(ecLog[i]); if(i<logIndex-1) json += ","; }
  json += "]}";
   
  server.send(200, "application/json", json);
}

// CSV Download Handler
void handleDownload() {
  String csv = "Time (Minutes),EC Value (uS/cm)\n";
  for(int i=0; i<logIndex; i++) {
    csv += timeLog[i] + "," + String(ecLog[i]) + "\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=hydro_data.csv");
  server.send(200, "text/csv", csv);
}

void handleCalibrate() {
  if (server.hasArg("target") && voltage > 0.1 && voltage < 3.2) {
    float r = (voltage * R_known) / (3.3 - voltage);
    float target = server.arg("target").toFloat();
    K_cell = (target / (1.0 + 0.019*(temperature-25.0))) / ((1.0/r)*1000000.0);
    preferences.putFloat("k_cell", K_cell);
    ecValue = target; // Force update
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
   
  bootTime = millis();
   
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println(WiFi.localIP());

  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  server.on("/json", handleJSON);
  server.on("/download", handleDownload); 
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.begin();
}

void loop() {
  server.handleClient();
  if (millis() - lastReadTime > READ_INTERVAL) { // Update Reading loop (Fast)
    updateSensor();
    lastReadTime = millis();
  }
  handleLogging(); // Check Logging Loop (Slow)
}
