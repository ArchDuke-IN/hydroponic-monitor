#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

const char* ssid = "kaustav kar";
const char* password = "Kaustav@896";
const char* serverUrl = "https://hydroponic-monitor.vercel.app/api/update-ph";
const unsigned long uploadInterval = 60000;

#define DHT11_PIN 14
#define DS18B20_PIN 27
#define PH_PIN 34
#define DHTTYPE DHT11
#define PIN_LED 2

DHT dht(DHT11_PIN, DHTTYPE);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
WebServer server(80);
Preferences preferences;

float ph_calibration = 21.34;
float ph_slope = -5.70;
float min_ph_limit = 6.0;
float max_ph_limit = 8.5;
float calVoltage7 = 0;
float calVoltage4 = 0;
unsigned long lastUploadTime = 0;
bool wifiConnected = false;
String apIP = "";

float readPH() {
  int adcValue = analogRead(PH_PIN);
  float voltage = adcValue * (3.3 / 4095.0);
  return ph_slope * voltage + ph_calibration;
}

void sendDataToCloud(float t1, float h1, float t2, float ph) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    String json = "{\"device_id\":\"ESP32_PH\",\"temp1\":" + String(t1) + ",\"hum1\":" + String(h1) + ",\"temp2\":" + String(t2) + ",\"hum2\":0,\"ph_val\":" + String(ph) + "}";
    int code = http.POST(json);
    if (code > 0) Serial.println("Cloud OK");
    else { Serial.print("Cloud err: "); Serial.println(code); }
    http.end();
  }
}

void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    digitalWrite(PIN_LED, LOW);
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    int a = 0;
    while (WiFi.status() != WL_CONNECTED && a < 20) { delay(500); a++; Serial.print("."); }
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      digitalWrite(PIN_LED, HIGH);
      Serial.print("\nReconnected IP: "); Serial.println(WiFi.localIP());
    } else Serial.println("\nReconnect failed.");
  }
}

void handleRoot() {
  if (server.hasArg("minVal") && server.hasArg("maxVal")) {
    min_ph_limit = server.arg("minVal").toFloat();
    max_ph_limit = server.arg("maxVal").toFloat();
    preferences.putFloat("ph_min", min_ph_limit);
    preferences.putFloat("ph_max", max_ph_limit);
    preferences.end(); preferences.begin("ph-meter", false);
  }

  float ambientTemp = dht.readTemperature();
  float humidity = dht.readHumidity();
  ds18b20.requestTemperatures();
  float waterTemp = ds18b20.getTempCByIndex(0);
  float ph = readPH();
  int adcRaw = analogRead(PH_PIN);
  float voltage = adcRaw * (3.3 / 4095.0);

  String ledClass = "led-off";
  String cardAlert = "";
  String statusMsg = "Stable";
  if (ph < min_ph_limit) { ledClass = "led-on"; cardAlert = "alert-border"; statusMsg = "ACIDIC"; }
  else if (ph > max_ph_limit) { ledClass = "led-on"; cardAlert = "alert-border"; statusMsg = "ALKALINE"; }

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;background:#eef2f3;text-align:center;padding:10px;}";
  html += ".card{background:white;padding:20px;margin:15px auto;max-width:400px;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += ".led{height:25px;width:25px;border-radius:50%;display:inline-block;vertical-align:middle;margin-left:10px;border:2px solid #444;}";
  html += ".led-off{background-color:#555;} .led-on{background-color:#ff0000;box-shadow:0 0 15px #ff0000;border-color:#800000;}";
  html += ".alert-border{border:2px solid red;background-color:#fff0f0;}";
  html += "h2{color:#333;margin-top:0;} .data{font-size:1.5em;font-weight:bold;color:#0275d8;}";
  html += ".unit{font-size:0.8em;color:#666;} .offline{color:red;font-size:14px;}";
  html += "input[type=text]{width:60px;padding:5px;text-align:center;border:1px solid #ccc;border-radius:4px;}";
  html += "input[type=number]{width:80px;padding:5px;border:1px solid #ccc;border-radius:4px;}";
  html += ".btn{background-color:#0275d8;color:white;padding:10px 20px;border-radius:5px;border:none;cursor:pointer;margin:3px;text-decoration:none;display:inline-block;}";
  html += ".btn-green{background-color:#28a745;} .btn-orange{background-color:#fd7e14;}";
  html += "</style></head><body>";
  html += "<h1>pH Monitor</h1>";
  if (!wifiConnected) html += "<p class='offline'>OFFLINE — Connect to ESP32-PH-Monitor WiFi</p>";
  html += "<p>AP: ESP32-PH-Monitor | IP: " + apIP + "</p>";
  html += "<a href='/' class='btn'>Refresh</a>";

  html += "<div class='card'><h2>Ambient (DHT11)</h2>";
  if (isnan(ambientTemp)) html += "<p>Sensor Error</p>";
  else html += "<p>Temp: <span class='data'>" + String(ambientTemp, 1) + "</span> <span class='unit'>&deg;C</span></p><p>Hum: <span class='data'>" + String(humidity, 0) + "</span> <span class='unit'>%</span></p>";
  html += "</div>";

  html += "<div class='card'><h2>Water Temp (DS18B20)</h2>";
  if (isnan(waterTemp)) html += "<p>Sensor Error</p>";
  else html += "<p>Temp: <span class='data'>" + String(waterTemp, 1) + "</span> <span class='unit'>&deg;C</span></p>";
  html += "</div>";

  html += "<div class='card " + cardAlert + "'><h2>Water Quality</h2>";
  html += "<div style='display:flex;justify-content:center;align-items:center;margin-bottom:10px;'>";
  html += "<span>Alert: </span><div class='led " + ledClass + "'></div></div>";
  html += "<p>pH: <span class='data'>" + String(ph, 2) + "</span></p>";
  html += "<p>ADC: " + String(adcRaw) + " | V: " + String(voltage, 3) + "V</p>";
  html += "<p><strong>" + statusMsg + "</strong></p>";

  html += "<hr><form action='/' method='GET'>";
  html += "<p>Safe Range:</p>";
  html += "Min: <input type='text' name='minVal' value='" + String(min_ph_limit, 1) + "'> ";
  html += "Max: <input type='text' name='maxVal' value='" + String(max_ph_limit, 1) + "'> ";
  html += "<br><br><input type='submit' class='btn' value='Save'></form>";
  html += "</div>";

  html += "<div class='card'><h2>pH Calibration</h2>";
  html += "<p>Current: slope=" + String(ph_slope, 4) + " offset=" + String(ph_calibration, 4) + "</p>";
  html += "<p>Cal pH7 V: " + String(calVoltage7, 4) + " | Cal pH4 V: " + String(calVoltage4, 4) + "</p>";
  html += "<form action='/calph7' method='POST'><button class='btn btn-green'>Calibrate pH 7.0</button></form>";
  html += "<form action='/calph4' method='POST'><button class='btn btn-orange'>Calibrate pH 4.0</button></form>";
  html += "<p style='font-size:11px;color:#aaa'>1: Dip probe in pH 7 buffer → Calibrate pH 7<br>2: Rinse, dip in pH 4 buffer → Calibrate pH 4<br>3: Slope & offset auto-calculated</p>";
  html += "</div>";

  html += "<a href='/wifi' class='btn' style='background:#6c757d'>WiFi Settings</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCalPH7() {
  int adc = analogRead(PH_PIN);
  calVoltage7 = adc * (3.3 / 4095.0);
  preferences.putFloat("cal_v7", calVoltage7);
  if (calVoltage4 > 0) {
    ph_slope = (7.0 - 4.0) / (calVoltage7 - calVoltage4);
    ph_calibration = 7.0 - ph_slope * calVoltage7;
    preferences.putFloat("ph_slope", ph_slope);
    preferences.putFloat("ph_cal", ph_calibration);
  }
  preferences.end(); preferences.begin("ph-meter", false);
  Serial.print("pH7 cal: V="); Serial.println(calVoltage7, 4);
  if (calVoltage4 > 0) { Serial.print("Slope="); Serial.print(ph_slope, 4); Serial.print(" Offset="); Serial.println(ph_calibration, 4); }
  server.sendHeader("Location", "/"); server.send(303);
}

void handleCalPH4() {
  int adc = analogRead(PH_PIN);
  calVoltage4 = adc * (3.3 / 4095.0);
  preferences.putFloat("cal_v4", calVoltage4);
  if (calVoltage7 > 0) {
    ph_slope = (7.0 - 4.0) / (calVoltage7 - calVoltage4);
    ph_calibration = 7.0 - ph_slope * calVoltage7;
    preferences.putFloat("ph_slope", ph_slope);
    preferences.putFloat("ph_cal", ph_calibration);
  }
  preferences.end(); preferences.begin("ph-meter", false);
  Serial.print("pH4 cal: V="); Serial.println(calVoltage4, 4);
  if (calVoltage7 > 0) { Serial.print("Slope="); Serial.print(ph_slope, 4); Serial.print(" Offset="); Serial.println(ph_calibration, 4); }
  server.sendHeader("Location", "/"); server.send(303);
}

void handleWiFi() {
  String h = "<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;padding:20px'>";
  h += "<h2>WiFi Settings</h2><form action='/savewifi' method='POST'>";
  h += "<label>SSID:</label><input name='ssid' value='" + String(ssid) + "'><br>";
  h += "<label>Password:</label><input type='password' name='pass'><br>";
  h += "<button class='btn'>Save & Reboot</button></form>";
  h += "<p>Status: " + String(wifiConnected ? "Connected" : "Disconnected") + "</p>";
  h += "<p>AP IP: " + apIP + "</p><a href='/'><button class='btn'>Back</button></a></body></html>";
  server.send(200, "text/html", h);
}

void handleSaveWiFi() {
  if (server.hasArg("ssid")) {
    preferences.putString("wifi_ssid", server.arg("ssid"));
    preferences.putString("wifi_pass", server.hasArg("pass") && server.arg("pass").length() > 0 ? server.arg("pass") : "");
    preferences.end();
    server.send(200, "text/html", "<html><body><h2>Saved! Rebooting...</h2></body></html>");
    delay(1000);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW);

  dht.begin();
  ds18b20.begin();

  preferences.begin("ph-meter", false);
  ph_slope = preferences.getFloat("ph_slope", -5.70);
  ph_calibration = preferences.getFloat("ph_cal", 21.34);
  calVoltage7 = preferences.getFloat("cal_v7", 0);
  calVoltage4 = preferences.getFloat("cal_v4", 0);
  min_ph_limit = preferences.getFloat("ph_min", 6.0);
  max_ph_limit = preferences.getFloat("ph_max", 8.5);
  String savedSSID = preferences.getString("wifi_ssid", "");
  String savedPass = preferences.getString("wifi_pass", "");
  if (savedSSID.length() > 0) { ssid = savedSSID.c_str(); password = savedPass.c_str(); }
  preferences.end();

  Serial.println("\n=== pH Monitor Boot ===");
  Serial.print("Slope: "); Serial.println(ph_slope, 4);
  Serial.print("Offset: "); Serial.println(ph_calibration, 4);
  Serial.print("Cal V7: "); Serial.println(calVoltage7, 4);
  Serial.print("Cal V4: "); Serial.println(calVoltage4, 4);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32-PH-Monitor", NULL);
  apIP = WiFi.softAPIP().toString();
  Serial.print("AP IP: "); Serial.println(apIP);

  WiFi.begin(ssid, password);
  Serial.print("WiFi");
  int d = 0;
  while (WiFi.status() != WL_CONNECTED && d * 500 < 15000) { delay(500); d++; Serial.print("."); digitalWrite(PIN_LED, !digitalRead(PIN_LED)); }
  if (WiFi.status() == WL_CONNECTED) { wifiConnected = true; digitalWrite(PIN_LED, HIGH); Serial.print("\nIP: "); Serial.println(WiFi.localIP()); }
  else { digitalWrite(PIN_LED, LOW); Serial.println("\nOFFLINE — connect to ESP32-PH-Monitor"); }

  server.on("/", handleRoot);
  server.on("/calph7", HTTP_POST, handleCalPH7);
  server.on("/calph4", HTTP_POST, handleCalPH4);
  server.on("/wifi", handleWiFi);
  server.on("/savewifi", HTTP_POST, handleSaveWiFi);
  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
  if (millis() - lastUploadTime > uploadInterval) {
float ambientTemp = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(ambientTemp)) ambientTemp = 0;
    if (isnan(humidity)) humidity = 0;
    ds18b20.requestTemperatures();
    float waterTemp = ds18b20.getTempCByIndex(0);
    if (isnan(waterTemp)) waterTemp = 0;
    float ph = readPH();
    sendDataToCloud(ambientTemp, humidity, waterTemp, ph);
    lastUploadTime = millis();
  }
  ensureWiFi();
}
