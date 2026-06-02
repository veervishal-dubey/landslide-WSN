#include <esp_now.h>
#include <WiFi.h>

#define BUTTON_PIN 35  // Change to whatever GPIO your button is on

uint8_t receiverAddress[] = {0xB0, 0xA7, 0x32, 0x17, 0x42, 0xE0};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Delivery: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS ✅" : "FAILED ❌");
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // LOW when pressed

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

  Serial.println("Sender ready");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {  // Button pressed
    const char *msg = "ALERT";
    esp_now_send(receiverAddress, (uint8_t *)msg, strlen(msg));
    Serial.println("Button pressed — ALERT sent");

    // Debounce: wait for release before allowing another send
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(10);
    }
    delay(50);  // Small debounce after release
  }
}