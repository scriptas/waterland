#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>

const char* ssid = "Wall-e";
const char* password = "sausis45";

#define BAUD_RATE 9600
#define DRY_VALUE 650
#define WET_VALUE 276
#define WATER_PORT D5
#define WATERING_TIME 120000  // 2 minutes in milliseconds
#define MAX_DAILY_WATERING_TIME 1800000  // 30 minutes in milliseconds
#define WIFI_TIMEOUT 30000  // 30 seconds in milliseconds
#define TCP_PORT 23  // TCP port for logging

unsigned long dailyWateringTime = 0;
unsigned long lastResetTime = 0;
bool timeRetrieved = false;
int currentHour = 0;

ESP8266WiFiMulti WiFiMulti;
WiFiServer server(TCP_PORT);
WiFiClient client;

// NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // Update every minute

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);
  Serial.println("Soil Moisture Sensor Calibration Test");

  pinMode(WATER_PORT, OUTPUT);
  digitalWrite(WATER_PORT, LOW);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    timeClient.begin();
    timeClient.setTimeOffset(3600 * 2);  // Adjust for your timezone if necessary

    // Try to get time from NTP server
    if (timeClient.update()) {
      timeRetrieved = true;
      currentHour = timeClient.getHours();
      Serial.print("Current hour: ");
      Serial.println(currentHour);
    }

    server.begin();
    Serial.println("TCP server started.");
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed. Continuing without WiFi.");
  }
}

void loop() {
  if (millis() - lastResetTime > 86400000) {
    dailyWateringTime = 0;
    lastResetTime = millis();
    
    // Try to update time from NTP server once a day
    if (WiFi.status() == WL_CONNECTED && timeClient.update()) {
      timeRetrieved = true;
      currentHour = timeClient.getHours();
      logMessage("Current hour: " + String(currentHour));
    } else {
      timeRetrieved = false;
      logMessage("Failed to retrieve time from NTP server.");
    }
  }

  int sensorValue = analogRead(A0);
  int moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
  moisturePercentage = constrain(moisturePercentage, 0, 100);

  logMessage("Soil Moisture Value: " + String(sensorValue) + " -> " + String(moisturePercentage) + "%");

  bool withinTimeRange = timeRetrieved ? (currentHour >= 10 && currentHour < 18) : true;

  String stopReasons = "Stop Reasons: ";
  bool shouldStop = false;

  if (moisturePercentage >= 50) {
    stopReasons += "Moisture is above 50%; ";
    shouldStop = true;
  }
  if (dailyWateringTime >= MAX_DAILY_WATERING_TIME) {
    stopReasons += "Daily limit reached; ";
    shouldStop = true;
  }
  if (!withinTimeRange) {
    stopReasons += "Out of time range; ";
    shouldStop = true;
  }

  if (shouldStop) {
    logMessage(stopReasons + "stopping watering...");
    digitalWrite(WATER_PORT, LOW);
  } else {
    logMessage("Conditions met, starting watering...");
    digitalWrite(WATER_PORT, HIGH);
    unsigned long startWateringTime = millis();
    while (millis() - startWateringTime < WATERING_TIME) {
      sensorValue = analogRead(A0);
      moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
      moisturePercentage = constrain(moisturePercentage, 0, 100);

      if (!(moisturePercentage < 50) || dailyWateringTime >= MAX_DAILY_WATERING_TIME) {
        logMessage("Moisture exceeds 50% or daily limit reached, stopping watering...");
        break;
      }
      
      logMessage("During Watering - Soil Moisture Value: " + String(sensorValue) + " -> " + String(moisturePercentage) + "%");

      delay(1000);
    }
    unsigned long wateringDuration = millis() - startWateringTime;
    dailyWateringTime += wateringDuration;
    digitalWrite(WATER_PORT, LOW);
    logMessage("Watering complete.");
  }

  delay(10000);  // Delay 10 seconds instead of 1 second

  // Handle client connection for logging
  if (!client || !client.connected()) {
    client = server.available();
  }

  if (client && client.connected()) {
    while (client.available()) {
      client.read();  // Clear any incoming data
    }
  }
}

void logMessage(String message) {
  Serial.println(message);
  if (client && client.connected()) {
    client.println(message);
  }
}
