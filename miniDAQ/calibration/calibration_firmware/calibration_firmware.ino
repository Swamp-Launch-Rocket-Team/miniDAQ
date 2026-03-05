#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_NAU7802.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// --- CONFIGURATION ---

// Custom I2C Pins for Sensor B
#define SDA_CUSTOM 33
#define SCL_CUSTOM 32

// WIFI Connectivity
const char* ssid = "TP-Link_7858";
const char* password = "59700692";

WebSocketsServer webSocket = WebSocketsServer(81);

#define LED_PIN 2

unsigned long lastSend = 0;

// --- OBJECTS ---

TwoWire I2C_Custom = TwoWire(1);
Adafruit_NAU7802 nauA; // Sensor on Default I2C (A1, A2)
Adafruit_NAU7802 nauB; // Sensor on Custom I2C  (B1, B2)

// --- HELPER FUNCTIONS ---

int32_t readChannel(Adafruit_NAU7802 &sensor, uint8_t channel) {
  sensor.setChannel(channel);
  for (uint8_t i = 0; i < 5; i++) {
    while (!sensor.available()) delay(1);
    sensor.read();
  }
  while (!sensor.available()) delay(1);
  return sensor.read();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // No taring on ESP32 anymore
  // This can be left empty or used for future commands
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected");
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("\n--- 4-Channel NAU7802 Logger ---");

  Wire.begin(); // Default I2C (21 SDA, 22 SCL)
  I2C_Custom.begin(SDA_CUSTOM, SCL_CUSTOM); // Custom I2C (33 SDA, 32 SCL)

  if (!nauA.begin(&Wire)) {
    Serial.println("Failed to find NAU7802 'A' (Default Pins 21/22)");
    while (1) delay(10);
  }
  nauA.setLDO(NAU7802_3V0);
  nauA.setGain(NAU7802_GAIN_128);
  nauA.setRate(NAU7802_RATE_320SPS);
  Serial.println("Sensor A configured.");

  if (!nauB.begin(&I2C_Custom)) {
    Serial.println("Failed to find NAU7802 'B' (Custom Pins 33/32)");
    while (1) delay(10);
  }
  nauB.setLDO(NAU7802_3V0);
  nauB.setGain(NAU7802_GAIN_128);
  nauB.setRate(NAU7802_RATE_320SPS);
  Serial.println("Sensor B configured.");

  Serial.println("Logging started...");
}

void loop() {
  webSocket.loop();

  if (millis() - lastSend > 100) {
    int32_t valA1 = readChannel(nauA, 0);
    int32_t valA2 = readChannel(nauA, 1);
    int32_t valB1 = readChannel(nauB, 0);
    int32_t valB2 = readChannel(nauB, 1);
    unsigned long timestamp = millis();

    StaticJsonDocument<200> doc;
    doc["time"] = timestamp;
    doc["A1"] = valA1;
    doc["A2"] = valA2;
    doc["B1"] = valB1;
    doc["B2"] = valB2;

    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
    Serial.print(".");

    lastSend = millis();
  }
}