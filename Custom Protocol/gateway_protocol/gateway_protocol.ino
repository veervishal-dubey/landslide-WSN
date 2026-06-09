#define BLYNK_TEMPLATE_ID   "TMPL3f95sR3sA"
#define BLYNK_TEMPLATE_NAME "landslide wsn"
#define BLYNK_AUTH_TOKEN    "xifl5xSp78azqZ1SsZEds8IMYw-g7692"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>

#define WIFI_SSID    "Modi Modi Modi"
#define WIFI_PASS    "qwertyuiop"
#define BUZZER_PIN   14
#define NODE_TIMEOUT 10000
#define MAX_NODES    10


#define NODE1_MAC "B0:A7:32:17:42:E0"
#define NODE2_MAC "B0:A7:32:15:5C:04"


#define ESPNOW_CHANNEL 1

typedef struct __attribute__((packed)) {
  char    NodeID[20];
  float   soilMoistureReading;
  float   vibrationMagnitude;
  uint8_t classLabel;
  bool    soilAlert;
  bool    vibrationAlert;
  bool    alert;
  bool    heartbeat;
} SensorMessage;

typedef struct {
  char    NodeID[20];
  unsigned long lastSeen;
  bool    online;
  float   lastMoisture;
  float   lastVibration;
  uint8_t lastClass;
  bool    lastSoilAlert;
  bool    lastVibAlert;
} NodeStatus;

NodeStatus    nodes[MAX_NODES];
int           nodeCount        = 0;
bool          globalAlert      = false;
unsigned long buzzerStart      = 0;
bool          buzzerOn         = false;
unsigned long lastTimeoutCheck = 0;

WebServer server(80);

const char* className(uint8_t c) {
  if (c == 2) return "DANGER";
  if (c == 1) return "WARNING";
  return "Normal";
}

int findOrAddNode(const char* id) {
  for (int i = 0; i < nodeCount; i++)
    if (strcmp(nodes[i].NodeID, id) == 0) return i;
  if (nodeCount < MAX_NODES) {
    strncpy(nodes[nodeCount].NodeID, id, sizeof(nodes[nodeCount].NodeID));
    nodes[nodeCount].online        = true;
    nodes[nodeCount].lastSeen      = millis();
    nodes[nodeCount].lastMoisture  = 0;
    nodes[nodeCount].lastVibration = 0;
    nodes[nodeCount].lastClass     = 0;
    nodes[nodeCount].lastSoilAlert = false;
    nodes[nodeCount].lastVibAlert  = false;
    Serial.printf("New node registered: %s\n", nodes[nodeCount].NodeID);
    return nodeCount++;
  }
  Serial.println("Node table full");
  return -1;
}

void updateGlobalAlert() {
  bool any = false;
  for (int i = 0; i < nodeCount; i++)
    if (nodes[i].online && nodes[i].lastClass > 0) { any = true; break; }

  globalAlert = any;
  Blynk.virtualWrite(V4, any ? 1 : 0);

  if (any && !buzzerOn) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOn    = true;
    buzzerStart = millis();
  }
}


void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(SensorMessage)) {
    Serial.printf("[Gateway] Size mismatch! Got %d, expected %d\n", len, (int)sizeof(SensorMessage));
    return;
  }

  SensorMessage msg;
  memcpy(&msg, data, sizeof(msg));

  // Safety: ensure NodeID is null-terminated
  msg.NodeID[sizeof(msg.NodeID) - 1] = '\0';

  int idx = findOrAddNode(msg.NodeID);
  if (idx == -1) return;

  bool wasOnline         = nodes[idx].online;
  nodes[idx].lastSeen    = millis();
  nodes[idx].online      = true;
  nodes[idx].lastMoisture  = msg.soilMoistureReading;
  nodes[idx].lastVibration = msg.vibrationMagnitude;
  nodes[idx].lastClass     = msg.classLabel;
  nodes[idx].lastSoilAlert = msg.soilAlert;
  nodes[idx].lastVibAlert  = msg.vibrationAlert;

  if (!wasOnline)
    Serial.printf("[Gateway] Node rejoined: %s\n", msg.NodeID);

  Serial.printf("[%s] Soil %.1f%% | Vib %.1f | Class: %s\n",
    msg.NodeID, msg.soilMoistureReading,
    msg.vibrationMagnitude, className(msg.classLabel));

  String id = String(msg.NodeID);
  if (id == NODE1_MAC) {
    Blynk.virtualWrite(V0, msg.soilMoistureReading);
    Blynk.virtualWrite(V2, msg.soilAlert ? 1 : 0);
    Blynk.virtualWrite(V5, msg.vibrationMagnitude);
    Blynk.virtualWrite(V6, msg.vibrationAlert ? 1 : 0);
  } else if (id == NODE2_MAC) {
    Blynk.virtualWrite(V1, msg.soilMoistureReading);
    Blynk.virtualWrite(V3, msg.soilAlert ? 1 : 0);
    Blynk.virtualWrite(V7, msg.vibrationMagnitude);
    Blynk.virtualWrite(V8, msg.vibrationAlert ? 1 : 0);
  } else {
 
    Serial.printf("[Gateway] WARNING: Unrecognized NodeID '%s' — check NODE1_MAC/NODE2_MAC defines!\n", msg.NodeID);
  }

  updateGlobalAlert();
}

void handleRoot() {
  String html = F("<!DOCTYPE html><html><head>"
    "<title>Fissure WSN</title>"
    "<meta http-equiv='refresh' content='3'/>"
    "<style>"
    "body{font-family:Arial;padding:20px;background:#1a1a2e;color:#eee;}"
    "h1{color:#00ff88;}"
    ".node{background:#16213e;border-radius:8px;padding:15px;margin:10px 0;}"
    ".online{color:#00ff88;}.offline{color:#ff4444;}"
    ".warn{color:#ff9900;font-weight:bold;}"
    ".danger-box{background:#ff4444;padding:15px;border-radius:8px;margin:10px 0;}"
    "</style></head><body>");

  html += F("<h1>Fissure - Landslide WSN</h1>");
  html += "<p>Active Nodes: " + String(nodeCount) + "</p>";

  for (int i = 0; i < nodeCount; i++) {
    html += "<div class='node'>";
    html += "<b>Node " + String(i+1) + "</b> | " + String(nodes[i].NodeID) + "<br>";
    html += "Status: <span class='";
    html += nodes[i].online ? "online'>ONLINE" : "offline'>OFFLINE";
    html += "</span><br>";
    html += "Soil: "      + String(nodes[i].lastMoisture,  1) + "%<br>";
    html += "Vibration: " + String(nodes[i].lastVibration, 1) + "<br>";
    html += "Class: <b>"  + String(className(nodes[i].lastClass)) + "</b><br>";
    if (nodes[i].lastSoilAlert) html += "<span class='warn'>⚠ HIGH MOISTURE</span><br>";
    if (nodes[i].lastVibAlert)  html += "<span class='warn'>⚠ VIBRATION ALERT</span><br>";
    html += "</div>";
  }

  if (globalAlert)
    html += "<div class='danger-box'><b>🚨 LANDSLIDE RISK DETECTED</b></div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void checkNodeTimeouts() {
  bool anyOnlineAlert = false;
  for (int i = 0; i < nodeCount; i++) {
    bool wasOnline = nodes[i].online;
    nodes[i].online = (millis() - nodes[i].lastSeen) < NODE_TIMEOUT;

    if (wasOnline && !nodes[i].online) {
      Serial.printf("[Gateway] Node offline: %s\n", nodes[i].NodeID);
      if (strcmp(nodes[i].NodeID, NODE1_MAC) == 0) Blynk.virtualWrite(V2, 0);
      if (strcmp(nodes[i].NodeID, NODE2_MAC) == 0) Blynk.virtualWrite(V3, 0);
      nodes[i].lastClass = 0;
    }

    if (nodes[i].online && nodes[i].lastClass > 0)
      anyOnlineAlert = true;
  }

  if (globalAlert && !anyOnlineAlert) {
    globalAlert = false;
    Blynk.virtualWrite(V4, 0);
    Serial.println("[Gateway] Global alert cleared.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.mode(WIFI_AP_STA);
  delay(100);


  WiFi.softAP("Fissure-WSN", "landslide123", ESPNOW_CHANNEL);
  Serial.printf("[Gateway] AP started on channel %d\n", ESPNOW_CHANNEL);

  
  Serial.print("[Gateway] Connecting to WiFi router");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[Gateway] WiFi connected on channel %d (router channel)\n", WiFi.channel());

    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    Serial.printf("[Gateway] Forced radio to channel %d for ESP-NOW\n", ESPNOW_CHANNEL);

    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();
  } else {
    Serial.println("\n[Gateway] WiFi failed. Running in local-only mode.");
    // Even without router, force the channel for ESP-NOW
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  }

  Serial.printf("[Gateway] Active channel: %d\n", WiFi.channel());

  // Step 4 — Init ESP-NOW AFTER channel is set
  if (esp_now_init() != ESP_OK) {
    Serial.println("[Gateway] ESP-NOW init FAILED");
    return;
  }
  esp_now_register_recv_cb(onReceive);

  server.on("/", handleRoot);
  server.begin();
  Serial.println("[Gateway] Ready. Waiting for nodes...");


  Serial.printf("[Gateway] My MAC (STA): %s\n", WiFi.macAddress().c_str());
  Serial.printf("[Gateway] My MAC (AP):  %s\n", WiFi.softAPmacAddress().c_str());
  Serial.println("[Gateway] >>> Nodes must send to the AP MAC address shown above! <<<");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED)
    Blynk.run();

  server.handleClient();

  if (buzzerOn && millis() - buzzerStart >= 1000) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
  }

  if (millis() - lastTimeoutCheck >= 1000) {
    checkNodeTimeouts();
    lastTimeoutCheck = millis();
  }
}
