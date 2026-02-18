#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <HTTPClient.h>

// ================= CONFIGURATION =================

// 1. Wi-Fi Credentials
const char* ssid = "kaustav kar";      
const char* password = "Kaustav@896";

// 2. Website Configuration
const char* serverUrl = "https://hydroponic-monitor.vercel.app/api/update-ph"; 
const unsigned long uploadInterval = 60000; // Upload every 60 seconds

// 3. Pin Definitions
#define SENSOR1_PIN 14 // First Temp/Hum Sensor
#define SENSOR2_PIN 27 // Second Temp/Hum Sensor
#define PH_PIN 34 // pH Sensor

// 4. Sensor Type
#define DHTTYPE DHT11       

// ================= GLOBALS =================
DHT dht1(SENSOR1_PIN, DHTTYPE);
DHT dht2(SENSOR2_PIN, DHTTYPE);
WebServer server(80);

// pH Calibration
float ph_calibration = 21.34;
float ph_slope = -5.70;       

// Default pH Limits
float min_ph_limit = 6.0;
float max_ph_limit = 8.5;

// Timer for Cloud Upload
unsigned long lastUploadTime = 0;

// --- Helper Functions ---
float readPH() {
  int adcValue = analogRead(PH_PIN);
  float voltage = adcValue * (3.3 / 4095.0);
  float phValue = ph_slope * voltage + ph_calibration;
  return phValue;
}

void sendDataToCloud(float t1, float h1, float t2, float h2, float ph) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Create JSON Payload
    String json = "{";
    json += "\"device_id\":\"ESP32_PH\",";
    json += "\"temp1\":" + String(t1) + ",";
    json += "\"hum1\":" + String(h1) + ",";
    json += "\"temp2\":" + String(t2) + ",";
    json += "\"hum2\":" + String(h2) + ",";
    json += "\"ph_val\":" + String(ph);
    json += "}";

    int httpResponseCode = http.POST(json);

    if (httpResponseCode > 0) {
      Serial.print("Cloud Upload Success. Code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error sending to cloud: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Cannot upload.");
  }
}

void handleRoot() {
  // --- 1. HANDLE USER INPUTS (SETTINGS) ---
  if (server.hasArg("minVal") && server.hasArg("maxVal")) {
    min_ph_limit = server.arg("minVal").toFloat();
    max_ph_limit = server.arg("maxVal").toFloat();
  }

  // --- 2. READ SENSORS ---
  float t1 = dht1.readTemperature();
  float h1 = dht1.readHumidity();
  float t2 = dht2.readTemperature();
  float h2 = dht2.readHumidity();
  float ph = readPH();

  // --- 3. LOGIC FOR LED & ALERTS ---
  String ledClass = "led-off"; 
  String cardAlert = "";
  String statusMsg = "Status: Stable";

  if (ph < min_ph_limit) {
    ledClass = "led-on"; 
    cardAlert = "alert-border";
    statusMsg = " ACIDIC WARNING";
  }
  else if (ph > max_ph_limit) {
    ledClass = "led-on"; 
    cardAlert = "alert-border";
    statusMsg = " ALKALINE WARNING";
  }

  // --- 4. GENERATE HTML ---
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: sans-serif; background: #eef2f3; text-align: center; padding: 10px; }";
  html += ".card { background: white; padding: 20px; margin: 15px auto; max-width: 400px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += ".led { height: 25px; width: 25px; border-radius: 50%; display: inline-block; vertical-align: middle; margin-left: 10px; border: 2px solid #444; }";
  html += ".led-off { background-color: #555; }"; 
  html += ".led-on { background-color: #ff0000; box-shadow: 0 0 15px #ff0000; border-color: #800000; }"; 
  html += ".alert-border { border: 2px solid red; background-color: #fff0f0; }";
  html += "h2 { color: #333; margin-top: 0; }";
  html += ".data { font-size: 1.5em; font-weight: bold; color: #0275d8; }";
  html += ".unit { font-size: 0.8em; color: #666; }";
  html += "input[type=text] { width: 60px; padding: 5px; text-align: center; border: 1px solid #ccc; border-radius: 4px; }";
  html += ".btn { background-color: #0275d8; color: white; padding: 10px 20px; border-radius: 5px; border: none; cursor: pointer; }";
  html += ".btn-refresh { background-color: #28a745; text-decoration: none; display:inline-block; margin-bottom: 20px; }";
  html += "</style></head><body>";

  html += "<h1>ESP32 Monitor</h1>";
  html += "<a href='/' class='btn btn-refresh'>Refresh Data</a>";

  // -- Location 1 --
  html += "<div class='card'><h2>Location 1</h2>";
  if (isnan(t1)) html += "<p>Sensor Error</p>";
  else html += "<p>Temp: <span class='data'>" + String(t1, 1) + "</span> <span class='unit'>&deg;C</span></p><p>Hum: <span class='data'>" + String(h1, 0) + "</span> <span class='unit'>%</span></p>";
  html += "</div>";

  // -- Location 2 --
  html += "<div class='card'><h2>Location 2</h2>";
  if (isnan(t2)) html += "<p>Sensor Error</p>";
  else html += "<p>Temp: <span class='data'>" + String(t2, 1) + "</span> <span class='unit'>&deg;C</span></p><p>Hum: <span class='data'>" + String(h2, 0) + "</span> <span class='unit'>%</span></p>";
  html += "</div>";

  // -- Water Quality --
  html += "<div class='card " + cardAlert + "'><h2>Water Quality</h2>";
  html += "<div style='display: flex; justify-content: center; align-items: center; margin-bottom: 10px;'>";
  html += "<span>Alert Light: </span>";
  html += "<div class='led " + ledClass + "'></div>"; 
  html += "</div>";
  html += "<p>pH Level: <span class='data'>" + String(ph, 2) + "</span></p>";
  html += "<p><strong>" + statusMsg + "</strong></p>";

  // -- Settings Form --
  html += "<hr><form action='/' method='GET'>";
  html += "<p>Set Safe Range:</p>";
  html += "Min: <input type='text' name='minVal' value='" + String(min_ph_limit, 1) + "'> ";
  html += "Max: <input type='text' name='maxVal' value='" + String(max_ph_limit, 1) + "'> ";
  html += "<br><br><input type='submit' class='btn' value='Save Settings'>";
  html += "</form>";
  html += "</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Start Sensors
  dht1.begin();
  dht2.begin();

  // Start Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("Web Server IP: http://");
  Serial.println(WiFi.localIP());

  // Start Server
  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient(); // Handle Local Dashboard

  // --- AUTOMATIC CLOUD UPLOAD (Every 60s) ---
  if (millis() - lastUploadTime > uploadInterval) {
    float t1 = dht1.readTemperature();
    float h1 = dht1.readHumidity();
    float t2 = dht2.readTemperature();
    float h2 = dht2.readHumidity();
    float ph = readPH();

    if (!isnan(t1) && !isnan(t2)) {
      sendDataToCloud(t1, h1, t2, h2, ph);
    }
    
    lastUploadTime = millis();
  }
}
