// Blynk essentials - MUST be first
#define BLYNK_TEMPLATE_ID "TMPL3f95sR3sA"
#define BLYNK_TEMPLATE_NAME "landslide wsn"
#define BLYNK_AUTH_TOKEN "xifl5xSp78azqZ1SsZEds8IMYw-g7692"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>

#define WIFI_SSID "Modi Modi Modi"
#define WIFI_PASS "qwertyuiop"
#define BUZZER_PIN 14
#define NODE_TIMEOUT 10000

typedef struct {
  char NodeID[20];
  float soilMoistureReading;
  bool alert;
  bool heartbeat;
} SensorMessage;

typedef struct {
  char NodeID[20];
  unsigned long lastSeen;
  bool online;
  float lastMoisture;
} nodeStatus;

nodeStatus nodes[10];
int nodeCount = 0;
bool globalAlert = false;

WebServer server(80);

int findOrAddNode(const char* id) {
  for (int i = 0; i < nodeCount; i++)
    if (strcmp(nodes[i].NodeID, id) == 0) return i;
  if (nodeCount < 10) {
    strcpy(nodes[nodeCount].NodeID, id);
    nodes[nodeCount].online = true;
    nodes[nodeCount].lastSeen = millis();
    nodes[nodeCount].lastMoisture = 0;
    nodeCount++;
    Serial.printf("New Node Registered: %s\n", nodes[nodeCount-1].NodeID);
    return nodeCount - 1;
  }
  return -1;
}

void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  SensorMessage msg;
  memcpy(&msg, data, sizeof(msg));

  int idx = findOrAddNode(msg.NodeID);
  if (idx == -1) return;

  bool wasOnline = nodes[idx].online;
  nodes[idx].lastSeen = millis();
  nodes[idx].lastMoisture = msg.soilMoistureReading;
  nodes[idx].online = true;

  if (!wasOnline)
    Serial.printf("Node rejoined: %s\n", msg.NodeID);

  Serial.printf("Node %s | Soil %.1f%% | Alert %s\n",
    msg.NodeID, msg.soilMoistureReading, msg.alert ? "YES" : "NO");

  if (idx == 0) {
    Blynk.virtualWrite(V0, msg.soilMoistureReading);
    Blynk.virtualWrite(V2, 1);
  } else if (idx == 1) {
    Blynk.virtualWrite(V1, msg.soilMoistureReading);
    Blynk.virtualWrite(V3, 1);
  }

  if (msg.alert) {
    globalAlert = true;
    Blynk.virtualWrite(V4, 1);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Fissure WSN</title>";
  html += "<meta http-equiv='refresh' content='3'/>";
  html += "<style>";
  html += "body{font-family:Arial;padding:20px;background:#1a1a2e;color:#eee;}";
  html += "h1{color:#00ff88;}";
  html += ".node{background:#16213e;border-radius:8px;padding:15px;margin:10px 0;}";
  html += ".online{color:#00ff88;} .offline{color:#ff4444;}";
  html += ".alert{color:#ff9900;font-weight:bold;}";
  html += ".danger{background:#ff4444;padding:15px;border-radius:8px;margin:10px 0;}";
  html += "</style></head><body>";
  html += "<h1>Fissure - Landslide WSN</h1>";
  html += "<p>Active Nodes: " + String(nodeCount) + "</p>";

  for (int i = 0; i < nodeCount; i++) {
    html += "<div class='node'>";
    html += "<b>Node " + String(i+1) + "</b> | " + String(nodes[i].NodeID) + "<br>";
    html += "Status: <span class='" + String(nodes[i].online ? "online'>ONLINE" : "offline'>OFFLINE") + "</span><br>";
    html += "Soil Moisture: " + String(nodes[i].lastMoisture, 1) + "%<br>";
    if (nodes[i].lastMoisture > 70)
      html += "<span class='alert'>⚠ HIGH MOISTURE</span>";
    html += "</div>";
  }

  if (globalAlert)
    html += "<div class='danger'><b>🚨 LANDSLIDE RISK DETECTED</b></div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void checkNodeTimeouts() {
  for (int i = 0; i < nodeCount; i++) {
    bool wasOnline = nodes[i].online;
    nodes[i].online = (millis() - nodes[i].lastSeen) < NODE_TIMEOUT;

    if (wasOnline && !nodes[i].online) {
      Serial.printf("Node Offline: %s\n", nodes[i].NodeID);
      if (i == 0) Blynk.virtualWrite(V2, 0);
      if (i == 1) Blynk.virtualWrite(V3, 0);
    }
    if (!wasOnline && nodes[i].online)
      Serial.printf("Node %s has rejoined.\n", nodes[i].NodeID);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();
  } else {
    Serial.println("\nWiFi failed. Offline mode.");
  }

  WiFi.softAP("Fissure WSN", "landslide123");
  Serial.println("AP Started: " + WiFi.softAPIP().toString());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onReceive);
  Serial.println("ESP Now ready.");

  server.on("/", handleRoot);
  server.begin();

  Serial.println("Fissure is live.");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
  server.handleClient();
  checkNodeTimeouts();
  delay(1000);
}