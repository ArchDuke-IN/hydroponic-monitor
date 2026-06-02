#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>

const char* ssid = "kaustav kar";
const char* password = "Kaustav@896";
const char* serverUrl = "https://hydroponic-monitor.vercel.app/api/update-ec";

const int PIN_POWER = 4;
const int PIN_READ = 34;
const int PIN_LED = 2;

const int SAMPLE_COUNT = 20;
const int READ_INTERVAL = 3000;
const long WARMUP_TIME = 180000;
const long LOG_INTERVAL = 180000;
const int MAX_LOGS = 500;
const unsigned long WIFI_TIMEOUT = 15000;

float R_known = 1000.0;
float K_cell = 1.0;
float temperature = 25.0;
float highBoost = 1.25;
float voltage = 0;
float probeResistance = 0;
float ecValue = 0;

String timeLog[MAX_LOGS];
float ecLog[MAX_LOGS];
int logIndex = 0;
unsigned long bootTime = 0;
unsigned long lastLogTime = 0;
unsigned long lastReadTime = 0;

WebServer server(80);
Preferences preferences;
bool wifiConnected = false;
String apIP = "";

void sendECToCloud(float ec, float volt, float temp) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    String json = "{\"device_id\":\"ESP32_EC\",\"ec_value\":" + String(ec) + ",\"voltage\":" + String(volt) + ",\"temperature\":" + String(temp) + "}";
    int code = http.POST(json);
    if (code > 0) Serial.println("Cloud OK");
    else { Serial.print("Cloud err: "); Serial.println(code); }
    http.end();
  }
}

float getMedianVoltage() {
  int rawValues[SAMPLE_COUNT];
  digitalWrite(PIN_POWER, HIGH);
  delay(50);
  for (int i = 0; i < SAMPLE_COUNT; i++) { rawValues[i] = analogRead(PIN_READ); delay(10); }
  digitalWrite(PIN_POWER, LOW);
  for (int i = 0; i < SAMPLE_COUNT - 1; i++) {
    for (int j = 0; j < SAMPLE_COUNT - i - 1; j++) {
      if (rawValues[j] > rawValues[j + 1]) { int t = rawValues[j]; rawValues[j] = rawValues[j + 1]; rawValues[j + 1] = t; }
    }
  }
  return rawValues[SAMPLE_COUNT / 2] * (3.3 / 4095.0);
}

void updateSensor() {
  float v = getMedianVoltage();
  voltage = v;
  if (v > 3.2 || v < 0.01) {
    if (v > 3.2) { ecValue = 0; probeResistance = 999999; }
    else { ecValue = 99999; probeResistance = 0; }
  } else {
    probeResistance = (v * R_known) / (3.3 - v);
    float ec = (1.0 / probeResistance) * K_cell * 1000000.0;
    if (ec > 1800) ec *= highBoost;
    ec /= (1.0 + 0.019 * (temperature - 25.0));
    float diff = abs(ec - ecValue);
    if (ecValue == 0) ecValue = ec;
    else if (diff > 50) ecValue = (ecValue * 0.2) + (ec * 0.8);
    else ecValue = (ecValue * 0.95) + (ec * 0.05);
    Serial.print("ADC:"); Serial.print((v / 3.3) * 4095);
    Serial.print(" V:"); Serial.print(v, 3);
    Serial.print(" R:"); Serial.print(probeResistance);
    Serial.print(" EC:"); Serial.println(ecValue);
  }
}

void handleLogging() {
  unsigned long now = millis();
  if (now < WARMUP_TIME) return;
  if (now - lastLogTime > LOG_INTERVAL) {
    lastLogTime = now;
    if (logIndex >= MAX_LOGS) {
      for (int i = 0; i < MAX_LOGS - 1; i++) { timeLog[i] = timeLog[i+1]; ecLog[i] = ecLog[i+1]; }
      logIndex = MAX_LOGS - 1;
    }
    String t = String((now / 60000)) + "m";
    timeLog[logIndex] = t;
    ecLog[logIndex] = ecValue;
    logIndex++;
    Serial.println("Log: " + t + " -> " + String(ecValue) + " uS/cm");
    sendECToCloud(ecValue, voltage, temperature);
  }
}

void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    digitalWrite(PIN_LED, LOW);
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      digitalWrite(PIN_LED, HIGH);
      Serial.print("\nReconnected IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nReconnect failed, will retry later.");
    }
  }
}

String getHTML() {
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  h += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  h += "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#f4f4f9;}";
  h += ".box{background:white;padding:20px;border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);max-width:600px;margin:auto;}";
  h += "h1{color:#333;} .val{font-size:40px;color:#00796b;font-weight:bold;} .offline{color:red;font-size:14px;}";
  h += ".debug{font-size:12px;color:#888;margin:5px 0;}";
  h += "button{padding:10px 20px;background:#00796b;color:white;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  h += "input{padding:8px;width:100px;border:1px solid #ccc;border-radius:5px;margin:3px;}";
  h += "label{font-size:13px;display:inline-block;width:140px;text-align:right;margin-right:8px;}";
  h += "</style></head><body><div class='box'>";
  if (!wifiConnected) h += "<p class='offline'>OFFLINE — Connect to ESP32 WiFi to configure</p>";
  h += "<h1>EC Monitor</h1>";
  h += "<div class='val'><span id='ec'>--</span> <span style='font-size:20px'>µS/cm</span></div>";
  h += "<p class='debug'>Voltage: <span id='v'>--</span> V | Probe R: <span id='rp'>--</span> Ω</p>";
  h += "<p class='debug'>R_known: " + String(R_known) + " Ω | K_cell: " + String(K_cell, 4) + " | Temp: " + String(temperature, 1) + "°C</p>";
  h += "<p class='debug'>AP: ESP32-EC-Monitor | IP: " + apIP + "</p>";
  h += "<canvas id='myChart' height='250'></canvas>";
  h += "<br><a href='/download'><button>Download CSV</button></a>";
  h += "<hr><h3>Calibration</h3>";
  h += "<form action='/setr' method='POST' style='margin:10px 0'>";
  h += "<label>R_known (ohms):</label><input name='rval' value='" + String(R_known, 0) + "'><br>";
  h += "<label>Water Temp (°C):</label><input name='temp' value='" + String(temperature, 1) + "'><br>";
  h += "<button type='submit'>Save</button></form>";
  h += "<form action='/calibrate' method='POST' style='margin:10px 0'>";
  h += "<label>EC Standard (µS/cm):</label><input name='target' placeholder='1413' value='1413'><br>";
  h += "<button>Calibrate K_cell</button></form>";
  h += "<form action='/setk' method='POST' style='margin:10px 0'>";
  h += "<label>K_cell manual:</label><input name='kval' value='" + String(K_cell, 4) + "'><br>";
  h += "<button>Set K_cell</button></form>";
  h += "<p style='font-size:11px;color:#aaa'>1: Measure trimpot → set R_known<br>2: Dip in standard → Calibrate<br>3: Verify reading</p>";
  h += "</div><script>";
  h += "var ctx=document.getElementById('myChart').getContext('2d');";
  h += "var ch=new Chart(ctx,{type:'line',data:{labels:[],datasets:[{label:'EC (uS/cm)',borderColor:'#00796b',data:[]}]},options:{scales:{y:{beginAtZero:false}}}});";
  h += "setInterval(()=>{fetch('/json').then(r=>r.json()).then(d=>{";
  h += "document.getElementById('ec').innerText=d.currEC.toFixed(0);";
  h += "document.getElementById('v').innerText=d.currV.toFixed(3);";
  h += "document.getElementById('rp').innerText=d.probeR.toFixed(0);";
  h += "ch.data.labels=d.labels;ch.data.datasets[0].data=d.data;ch.update();";
  h += "})},3000);</script></body></html>";
  return h;
}

void handleJSON() {
  String j = "{\"currEC\":" + String(ecValue) + ",\"currV\":" + String(voltage) + ",\"probeR\":" + String(probeResistance) + ",\"labels\":[";
  for (int i = 0; i < logIndex; i++) { j += "\"" + timeLog[i] + "\""; if (i < logIndex-1) j += ","; }
  j += "],\"data\":[";
  for (int i = 0; i < logIndex; i++) { j += String(ecLog[i]); if (i < logIndex-1) j += ","; }
  j += "]}";
  server.send(200, "application/json", j);
}

void handleDownload() {
  String csv = "Time (Minutes),EC Value (uS/cm)\n";
  for (int i = 0; i < logIndex; i++) csv += timeLog[i] + "," + String(ecLog[i]) + "\n";
  server.sendHeader("Content-Disposition", "attachment; filename=hydro_data.csv");
  server.send(200, "text/csv", csv);
}

void handleCalibrate() {
  if (server.hasArg("target")) {
    float target = server.arg("target").toFloat();
    float v = getMedianVoltage();
    if (v > 0.1 && v < 3.2) {
      float probeR = (v * R_known) / (3.3 - v);
      K_cell = (target * (1.0 + 0.019 * (temperature - 25.0))) / ((1.0 / probeR) * 1000000.0);
      preferences.putFloat("k_cell", K_cell);
      preferences.end(); preferences.begin("ec-meter", false);
      ecValue = target; voltage = v; probeResistance = probeR;
      Serial.print("K_cell="); Serial.println(K_cell, 4);
    } else Serial.println("Calib skip: voltage out of range");
  }
  server.sendHeader("Location", "/"); server.send(303);
}

void handleSetR() {
  if (server.hasArg("rval")) {
    float nr = server.arg("rval").toFloat();
    if (nr > 10 && nr < 10000000) { R_known = nr; preferences.putFloat("r_known", R_known); }
  }
  if (server.hasArg("temp")) {
    float nt = server.arg("temp").toFloat();
    if (nt > 0 && nt < 60) { temperature = nt; preferences.putFloat("temperature", temperature); }
  }
  preferences.end(); preferences.begin("ec-meter", false);
  server.sendHeader("Location", "/"); server.send(303);
}

void handleSetK() {
  if (server.hasArg("kval")) {
    K_cell = server.arg("kval").toFloat();
    preferences.putFloat("k_cell", K_cell);
    preferences.end(); preferences.begin("ec-meter", false);
    Serial.print("K_cell="); Serial.println(K_cell, 4);
  }
  server.sendHeader("Location", "/"); server.send(303);
}

void handleWiFi() {
  String h = "<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;padding:20px'>";
  h += "<h2>WiFi Settings</h2>";
  h += "<form action='/savewifi' method='POST'>";
  h += "<label>SSID:</label><input name='ssid' value='" + String(ssid) + "'><br>";
  h += "<label>Password:</label><input type='password' name='pass'><br>";
  h += "<button type='submit'>Save & Reboot</button></form>";
  h += "<p>Current status: " + String(wifiConnected ? "Connected" : "Disconnected") + "</p>";
  h += "<p>STA IP: " + (wifiConnected ? WiFi.localIP().toString() : "N/A") + "</p>";
  h += "<p>AP IP: " + apIP + "</p>";
  h += "<a href='/'><button>Back</button></a></body></html>";
  server.send(200, "text/html", h);
}

void handleSaveWiFi() {
  if (server.hasArg("ssid")) {
    preferences.putString("wifi_ssid", server.arg("ssid"));
    preferences.putString("wifi_pass", server.hasArg("pass") && server.arg("pass").length() > 0 ? server.arg("pass") : "");
    preferences.end();
    server.send(200, "text/html", "<html><body><h2>Saved! Rebooting...</h2><script>setTimeout(()=>{},3000);</script></body></html>");
    delay(1000);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_POWER, OUTPUT); pinMode(PIN_READ, INPUT);
  pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_POWER, LOW); digitalWrite(PIN_LED, LOW);

  preferences.begin("ec-meter", false);
  R_known = preferences.getFloat("r_known", 1000.0);
  K_cell = preferences.getFloat("k_cell", 1.0);
  highBoost = preferences.getFloat("boost", 1.25);
  temperature = preferences.getFloat("temperature", 25.0);

  String savedSSID = preferences.getString("wifi_ssid", "");
  String savedPass = preferences.getString("wifi_pass", "");
  if (savedSSID.length() > 0) {
    ssid = savedSSID.c_str();
    password = savedPass.c_str();
  }
  preferences.end();

  bootTime = millis();
  Serial.println("\n=== EC Monitor Boot ===");
  Serial.print("R_known: "); Serial.println(R_known);
  Serial.print("K_cell: "); Serial.println(K_cell, 4);
  Serial.print("Temp: "); Serial.println(temperature);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32-EC-Monitor", NULL);
  apIP = WiFi.softAPIP().toString();
  Serial.print("AP IP: "); Serial.println(apIP);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED && dots * 500 < WIFI_TIMEOUT) {
    delay(500);
    dots++;
    Serial.print(".");
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    digitalWrite(PIN_LED, HIGH);
    Serial.print("\nSTA IP: "); Serial.println(WiFi.localIP());
  } else {
    digitalWrite(PIN_LED, LOW);
    Serial.println("\nOFFLINE MODE — Connect to ESP32-EC-Monitor WiFi");
  }

  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  server.on("/json", handleJSON);
  server.on("/download", handleDownload);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/setr", HTTP_POST, handleSetR);
  server.on("/setk", HTTP_POST, handleSetK);
  server.on("/wifi", handleWiFi);
  server.on("/savewifi", HTTP_POST, handleSaveWiFi);
  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - lastReadTime > READ_INTERVAL) {
    updateSensor();
    lastReadTime = now;
  }
  handleLogging();
  ensureWiFi();
}
