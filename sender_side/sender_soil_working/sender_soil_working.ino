#include <esp_now.h>
#include <WiFi.h>

#define SOIL_PIN            34

#define SOIL_WET_THRESHOLD  2500
#define WET_DURATION        5000

uint8_t receiverAddress[] = {0xB0, 0xA7, 0x32, 0x17, 0x42, 0xE0};

unsigned long wetStartTime  = 0;
bool wetConditionActive     = false;
bool alertSentThisCycle     = false;

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Delivery: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS " : "FAILED ");
}

void sendAlert(const char* reason) {
  const char *msg = "ALERT";
  esp_now_send(receiverAddress, (uint8_t *)msg, strlen(msg));
  Serial.print("ALERT sent — reason: ");
  Serial.println(reason);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("Sender ready — monitoring soil moisture...");
}



void loop() {
  int soilRaw = analogRead(SOIL_PIN);
  bool soilWet = (soilRaw < SOIL_WET_THRESHOLD);

  Serial.print("Soil ADC: ");
  Serial.print(soilRaw);
  Serial.println(soilWet ? "  → WET " : "  → DRY ");

  if (soilWet) {
    if (!wetConditionActive) {
      wetStartTime        = millis();
      wetConditionActive  = true;
      alertSentThisCycle  = false;
      Serial.println("Wet soil detected — monitoring duration...");
    } else {
      unsigned long elapsed = millis() - wetStartTime;
      Serial.print("Wet for: ");
      Serial.print(elapsed);
      Serial.println("ms");

      if (elapsed >= WET_DURATION && !alertSentThisCycle) {
        sendAlert("sustained wet soil detected");
        alertSentThisCycle = true;
      }
    }
  } else {
    if (wetConditionActive) {
      Serial.println("Soil dried — resetting monitor.");
    }
    wetConditionActive = false;
    alertSentThisCycle = false;
  }

  int soilRaw2 = analogRead(SOIL_PIN);
  Serial.print("Soil ADC: ");
  Serial.println(soilRaw2);


  delay(2000);
}