#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"

uint8_t gatewayMAC[] = {0xB0, 0xA7, 0x32, 0x17, 0x27, 0xF0};

#define SOIL_PIN 34
#define SEND_INTERVAL 3000
#define SOIL_ALERT_THRESHOLD 70
#define VIBRATION_THRESHOLD 2000

typedef struct {
  char NodeID[20];
  float soilMoistureReading;
  float vibrationMagnitude;
  bool soilAlert;
  bool vibrationAlert;
  bool alert;
  bool heartbeat;
} SensorMessage;

MPU6050 mpu;
int16_t ax, ay, az, gx, gy, gz;
SensorMessage msg;
unsigned long lastSend = 0;

void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Send: OK" : "Send: FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 failed - running soil only");
  } else {
    Serial.println("MPU6050 ready");
  }

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
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);

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
    msg.soilAlert = msg.soilMoistureReading > SOIL_ALERT_THRESHOLD;

    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    float magnitude = sqrt((float)ax*ax + (float)ay*ay + (float)az*az);
    float deviation = abs(magnitude - 16384.0);
    msg.vibrationMagnitude = deviation;
    msg.vibrationAlert = deviation > VIBRATION_THRESHOLD;

    msg.alert = msg.soilAlert || msg.vibrationAlert;
    msg.heartbeat = true;

    esp_now_send(gatewayMAC, (uint8_t *)&msg, sizeof(msg));

    Serial.printf("Soil: %.1f%% | Vib: %.1f | SoilAlert: %s | VibAlert: %s\n",
      msg.soilMoistureReading, deviation,
      msg.soilAlert ? "YES" : "NO",
      msg.vibrationAlert ? "YES" : "NO");

    lastSend = millis();
  }
}
