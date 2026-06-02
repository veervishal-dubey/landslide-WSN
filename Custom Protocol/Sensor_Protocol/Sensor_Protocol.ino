#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

uint8_t gatewayMAC[] = {0xB0, 0xA7, 0x32, 0x17, 0x27, 0xF0};

#define SOIL_PIN 34
#define SEND_INTERVAL 3000
#define ALERT_THRESHOLD 70

typedef struct {
  char NodeID[20];
  float soilMoistureReading;
  bool alert;
  bool heartbeat;
} SensorMessage;

SensorMessage msg;
unsigned long lastSend = 0;

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Send: OK" : "Send: FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  snprintf(msg.NodeID, sizeof(msg.NodeID), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.print("Node ID: ");
  Serial.println(msg.NodeID);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gatewayMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add gateway peer");
    return;
  }

  Serial.println("Sensor node ready.");
}

void loop() {
  if (millis() - lastSend >= SEND_INTERVAL) {
    int raw = analogRead(SOIL_PIN);
    msg.soilMoistureReading = map(raw, 0, 4095, 100, 0);
    msg.alert = msg.soilMoistureReading > ALERT_THRESHOLD;
    msg.heartbeat = true;

    esp_now_send(gatewayMAC, (uint8_t *)&msg, sizeof(msg));

    Serial.printf("Sent | Soil: %.1f%% | Alert: %s\n",
      msg.soilMoistureReading, msg.alert ? "YES" : "NO");

    lastSend = millis();
  }
}