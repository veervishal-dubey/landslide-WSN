#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "dt_model.h"

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

SensorMessage msg;
MPU6050 mpu;
bool mpuReady = false;
int16_t ax, ay, az, gx, gy, gz;


uint8_t gatewayMAC[] = {0xB0, 0xA7, 0x32, 0x17, 0x27, 0xF0};  // <-- AP MAC


#define ESPNOW_CHANNEL 1

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 3000;

float idleBaselineNoise = 16384.0f;

float readSoilSmoothed() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(34);
    delay(5);
  }
  float avgRaw    = sum / 10.0f;
  float percentage = (1.0f - (avgRaw / 4095.0f)) * 100.0f;
  if (percentage < 0.0f)   percentage = 0.0f;
  if (percentage > 100.0f) percentage = 100.0f;
  return percentage;
}


void onSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.printf("[Node] Send to gateway: %s\n",
    status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED — check channel & MAC");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // I2C init
  Wire.begin(21, 22);
  Wire.setTimeOut(50);


  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);


  esp_err_t ch_err = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.printf("[Node] Set channel %d: %s\n", ESPNOW_CHANNEL,
    ch_err == ESP_OK ? "OK" : esp_err_to_name(ch_err));

  // Init ESP-NOW after channel is locked
  if (esp_now_init() != ESP_OK) {
    Serial.println("[Node] ESP-NOW init FAILED");
    return;
  }
  esp_now_register_send_cb(onSent);


  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, gatewayMAC, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.ifidx   = WIFI_IF_STA;
  peerInfo.encrypt = false;

  esp_err_t peer_err = esp_now_add_peer(&peerInfo);
  if (peer_err != ESP_OK) {
    Serial.printf("[Node] Failed to add peer: %s\n", esp_err_to_name(peer_err));
    return;
  }
  Serial.println("[Node] Gateway peer registered OK.");


  String macStr = WiFi.macAddress();
  strncpy(msg.NodeID, macStr.c_str(), sizeof(msg.NodeID) - 1);
  msg.NodeID[sizeof(msg.NodeID) - 1] = '\0';
  Serial.printf("[Node] My MAC (NodeID): %s\n", msg.NodeID);
  Serial.printf("[Node] Sending to gateway AP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    gatewayMAC[0], gatewayMAC[1], gatewayMAC[2],
    gatewayMAC[3], gatewayMAC[4], gatewayMAC[5]);


  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("[Node] MPU6050 connected.");
    mpuReady = true;

    Serial.println("[Node] Calibrating vibration baseline (keep still)...");
    long totalMag = 0;
    for (int i = 0; i < 20; i++) {
      mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
      double axL = ax, ayL = ay, azL = az;
      totalMag += (long)sqrt(axL*axL + ayL*ayL + azL*azL);
      delay(30);
    }
    idleBaselineNoise = totalMag / 20.0f;
    Serial.printf("[Node] Baseline noise: %.2f\n", idleBaselineNoise);
  } else {
    Serial.println("[Node] MPU6050 NOT FOUND — check wiring on SDA/SCL (21/22)");
  }

  Serial.println("[Node] Setup complete. Starting transmit loop.");
}

void loop() {
  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    msg.soilMoistureReading = readSoilSmoothed();
    float deviation = 0.0f;

    if (mpuReady) {
      mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
      double axL = ax, ayL = ay, azL = az;
      float magnitude = (float)sqrt(axL*axL + ayL*ayL + azL*azL);
      deviation = fabsf(magnitude - idleBaselineNoise);
    }
    msg.vibrationMagnitude = deviation;


    float features[DT_N_FEATURES];
    features[FEAT_VIB_NORM]         = deviation / 16384.0f;
    features[FEAT_VIB_SQ]           = features[FEAT_VIB_NORM] * features[FEAT_VIB_NORM];
    features[FEAT_VIB_DANGER_FLAG]  = (deviation > 1500.0f) ? 1.0f : 0.0f;
    features[FEAT_SOIL_NORM]        = msg.soilMoistureReading / 100.0f;
    features[FEAT_SOIL_SQ]          = features[FEAT_SOIL_NORM] * features[FEAT_SOIL_NORM];
    features[FEAT_SOIL_DANGER_FLAG] = (msg.soilMoistureReading > 51.0f) ? 1.0f : 0.0f;
    float activeSoilFactor          = (features[FEAT_SOIL_NORM] < 0.05f) ? 0.05f : features[FEAT_SOIL_NORM];
    features[FEAT_VIB_X_SOIL]       = features[FEAT_VIB_NORM] * activeSoilFactor;


    int16_t node = 0;
    while (DT_left[node] != -1) {
      uint8_t feat = DT_feature[node];
      float   thr  = DT_threshold[node];
      node = (features[feat] <= thr) ? DT_left[node] : DT_right[node];
    }
    msg.classLabel = DT_leaf_class[node];

  
    if (deviation > 3000.0f)
      msg.classLabel = 2;

    msg.soilAlert      = (msg.soilMoistureReading > 51.0f);
    msg.vibrationAlert = (deviation > 1500.0f);
    msg.alert          = (msg.classLabel > 0);
    msg.heartbeat      = true;

    esp_err_t result = esp_now_send(gatewayMAC, (uint8_t *)&msg, sizeof(msg));
    if (result != ESP_OK)
      Serial.printf("[Node] esp_now_send error: %s\n", esp_err_to_name(result));

    Serial.printf("[Node] Soil: %.1f%% | Vib: %.1f | Class: %s (%d)\n",
      msg.soilMoistureReading, deviation,
      msg.classLabel == 2 ? "DANGER" : (msg.classLabel == 1 ? "WARNING" : "NORMAL"),
      msg.classLabel);
  }
}
