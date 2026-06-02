#include <esp_now.h>
#include <WiFi.h>

#define BUZZER_PIN 14  // Change to whatever GPIO your buzzer is on

void buzzAlert() {
  // Three short beeps
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  String msg = String((const char*)incomingData, len);

  Serial.print("Message received: ");
  Serial.println(msg);

  // Only trigger buzzer if message is "ALERT"
  if (msg == "ALERT") {
    Serial.println("ALERT received! Triggering buzzer...");
    buzzAlert();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);  // Ensure buzzer starts OFF

  WiFi.mode(WIFI_STA);
  delay(500);

  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Receiver ready, waiting for messages...");
}

void loop() {
  // nothing needed
}