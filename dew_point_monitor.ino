/*
 * Dual-Point Temperature/Humidity Monitor with Dew-Point Warning
 * 雙點溫濕度監測與結露預警系統
 *
 * ESP32-S3 + 2 x DHT11 + MQTT -> ThingSpeak
 *
 *   Sensor A: monitoring point (e.g. inside an electrical cabinet)
 *   Sensor B: reference point (ambient)
 *
 * ThingSpeak fields:
 *   1 A_Temp (°C)        2 A_Humidity (%)
 *   3 B_Temp (°C)        4 B_Humidity (%)
 *   5 A_DewPoint (°C)    6 A_AbsHumidity (g/m³)
 *   7 DeltaT = A - B     8 Status
 *     Status: 0=OK 1=Dew warning 2=Sensor A fail 3=Sensor B fail 4=Both fail
 *
 * Libraries: PubSubClient (Nick O'Leary), DHT sensor library (Adafruit)
 * Credentials live in secrets.h (not committed). Copy secrets.example.h to secrets.h.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <math.h>
#include "secrets.h"

// ---------- ThingSpeak MQTT ----------
const char* mqttServer = "mqtt3.thingspeak.com";
const int   mqttPort   = 1883;

// ---------- Hardware ----------
#define DHT_A_PIN 4
#define DHT_B_PIN 17
DHT dhtA(DHT_A_PIN, DHT11);
DHT dhtB(DHT_B_PIN, DHT11);

// ---------- Calibration offsets (run both sensors side by side, then fill in) ----------
const float OFFSET_A_T = 0.0, OFFSET_A_H = 0.0;
const float OFFSET_B_T = 0.0, OFFSET_B_H = 0.0;

// ---------- Parameters ----------
const float DEW_MARGIN = 3.0;                   // warn when (T - dew point) < margin; 4.5 used for breath demo
const unsigned long PUBLISH_INTERVAL = 20000;   // 20 s (ThingSpeak free tier: >= 15 s)

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastPublish = 0;

// Magnus formula: dew point (°C)
float dewPoint(float t, float rh) {
  const float a = 17.62, b = 243.12;
  float g = (a * t) / (b + t) + log(rh / 100.0);
  return (b * g) / (a - g);
}

// Absolute humidity (g/m³)
float absoluteHumidity(float t, float rh) {
  return 6.112 * exp((17.67 * t) / (t + 243.5)) * rh * 2.1674 / (273.15 + t);
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Connecting Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected, IP: " + WiFi.localIP().toString());
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting ThingSpeak MQTT...");
    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("OK");
    } else {
      Serial.printf("failed rc=%d, retry in 5 s\n", mqtt.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);   // ESP32-S3 native USB: wait for the serial port to enumerate
  Serial.println("Boot OK");
  dhtA.begin();
  dhtB.begin();
  connectWiFi();
  mqtt.setServer(mqttServer, mqttPort);
}

void loop() {
  connectWiFi();
  connectMQTT();
  mqtt.loop();

  if (millis() - lastPublish < PUBLISH_INTERVAL) return;
  lastPublish = millis();

  // 1. Read and calibrate
  float tA = dhtA.readTemperature() + OFFSET_A_T;
  float hA = dhtA.readHumidity()    + OFFSET_A_H;
  float tB = dhtB.readTemperature() + OFFSET_B_T;
  float hB = dhtB.readHumidity()    + OFFSET_B_H;

  bool okA = !isnan(tA) && !isnan(hA);
  bool okB = !isnan(tB) && !isnan(hB);

  // 2. Build payload with valid values only
  char payload[256] = "";
  char buf[64];
  int status = 0;
  const char* statusText = "OK";

  if (okA) {
    float dpA = dewPoint(tA, hA);
    float ahA = absoluteHumidity(tA, hA);
    snprintf(buf, sizeof(buf), "field1=%.1f&field2=%.0f&field5=%.1f&field6=%.2f&", tA, hA, dpA, ahA);
    strcat(payload, buf);
    if (tA - dpA < DEW_MARGIN) { status = 1; statusText = "DEW_WARNING"; }
    Serial.printf("A: %.1f C %.0f%% dew %.1f C abs %.2f g/m3\n", tA, hA, dpA, ahA);
  }
  if (okB) {
    snprintf(buf, sizeof(buf), "field3=%.1f&field4=%.0f&", tB, hB);
    strcat(payload, buf);
    Serial.printf("B: %.1f C %.0f%%\n", tB, hB);
  }
  if (okA && okB) {
    snprintf(buf, sizeof(buf), "field7=%.1f&", tA - tB);
    strcat(payload, buf);
  }

  // Sensor faults take priority over dew warning
  if (!okA && !okB)  { status = 4; statusText = "BOTH_FAIL"; }
  else if (!okA)     { status = 2; statusText = "SENSOR_A_FAIL"; }
  else if (!okB)     { status = 3; statusText = "SENSOR_B_FAIL"; }

  snprintf(buf, sizeof(buf), "field8=%d&status=%s", status, statusText);
  strcat(payload, buf);

  // 3. Publish
  char topic[48];
  snprintf(topic, sizeof(topic), "channels/%lu/publish", (unsigned long)THINGSPEAK_CHANNEL_ID);

  if (mqtt.publish(topic, payload)) {
    Serial.printf("Published [%s] -> %s\n\n", statusText, payload);
  } else {
    Serial.println("Publish failed\n");
  }
}
