#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>

const char* ssid = "name";
const char* password = "password";

// NodeMCU board, ESP-12E, 8622
#define BAUD_RATE 9600
#define OUTPUT_WATERING_PIN D5
#define INPUT_SENSOR_PIN A0
#define TCP_PORT 23  // TCP port for logging

// Capacitive Soil Moisture sensor v1.2
#define DRY_VALUE 650
#define WET_VALUE 276

#define second 1000
#define WATERING_TIME 120 * second  // 2 minutes
#define MAX_DAILY_WATERING_TIME 1800 * second  // 30 minutes
#define WIFI_TIMEOUT 30 * second

unsigned long dailyWateringTime = 0;
unsigned long lastResetTime = 0;
bool timeRetrieved = false;
int currentHour = 0;
bool isWatering = false;

ESP8266WiFiMulti WiFiMulti;
WiFiServer server(TCP_PORT);
WiFiClient client;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60 * second);  // NTP Client to get time, update every minute

void setup() {
  Serial.begin(BAUD_RATE);
  delay(second);
  logMessage("Setup started: Soil Moisture Sensor Calibration Test");

  pinMode(OUTPUT_WATERING_PIN, OUTPUT);
  digitalWrite(OUTPUT_WATERING_PIN, LOW);

  logMessage("Attempting to connect to Wi-Fi: " + String(ssid));
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    delay(0.5 * second);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    logMessage("Wi-Fi connected successfully.");
    logMessage("IP address: " + WiFi.localIP().toString());

    timeClient.begin();
    timeClient.setTimeOffset(3600 * 2);  // Adjusts for timezone

    logMessage("Attempting to retrieve time from NTP server...");
    if (timeClient.update()) {
      timeRetrieved = true;
      currentHour = timeClient.getHours();
      logMessage("Time successfully retrieved. Current hour: " + String(currentHour));
    } else {
      logMessage("Failed to retrieve time from NTP server.");
    }

    logMessage("Starting TCP server on port " + String(TCP_PORT) + "...");
    server.begin();
    logMessage("TCP server started successfully.");
  } else {
    logMessage("Wi-Fi connection failed. Continuing without Wi-Fi.");
  }
}

void loop() {
  if (millis() - lastResetTime > second * 3600 * 24) {
    dailyWateringTime = 0;
    lastResetTime = millis();
    
    logMessage("Resetting daily watering time.");
    
    // Try to update time from NTP server once a day
    if (WiFi.status() == WL_CONNECTED && timeClient.update()) {
      timeRetrieved = true;
      currentHour = timeClient.getHours();
      logMessage("Daily NTP update successful. Current hour: " + String(currentHour));
    } else {
      timeRetrieved = false;
      logMessage("Failed to retrieve time from NTP server during daily update.");
    }
  }

  int sensorValue = analogRead(INPUT_SENSOR_PIN);
  int moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
  moisturePercentage = constrain(moisturePercentage, 0, 100);

  logMessage("Soil Moisture Value: " + String(sensorValue) + " -> " + String(moisturePercentage) + "%");

  bool withinTimeRange = timeRetrieved ? (currentHour >= 10 && currentHour < 18) : true;

  String stopReasons = "";
  bool shouldStop = false;

  // Check conditions and log the limiting factor
  if (moisturePercentage >= 50) {
    stopReasons += "Moisture is above 50%; ";
    shouldStop = true;
  }
  if (dailyWateringTime >= MAX_DAILY_WATERING_TIME) {
    stopReasons += "Daily limit reached; ";
    shouldStop = true;
  }
  if (!withinTimeRange) {
    stopReasons += "Out of time range (10:00 - 18:00); ";
    shouldStop = true;
  }

  if (shouldStop && isWatering) {
    logMessage(stopReasons + "stopping watering...");
    digitalWrite(OUTPUT_WATERING_PIN, LOW);
    isWatering = false;
  } else if (shouldStop && !isWatering) {
    logMessage("Watering not started. Limiting factors: " + stopReasons);
  } else if (!shouldStop && !isWatering) {
    logMessage("Conditions met, starting watering...");
    digitalWrite(OUTPUT_WATERING_PIN, HIGH);
    isWatering = true;
    unsigned long startWateringTime = millis();
    
    while (millis() - startWateringTime < WATERING_TIME) {
      sensorValue = analogRead(INPUT_SENSOR_PIN);
      moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
      moisturePercentage = constrain(moisturePercentage, 0, 100);

      if (moisturePercentage >= 50 || dailyWateringTime >= MAX_DAILY_WATERING_TIME) {
        logMessage("Moisture exceeds 50% or daily limit reached, stopping watering...");
        break;
      }
      
      logMessage("During Watering - Soil Moisture Value: " + String(sensorValue) + " -> " + String(moisturePercentage) + "%");

      delay(second);
    }
    
    unsigned long wateringDuration = millis() - startWateringTime;
    dailyWateringTime += wateringDuration;
    digitalWrite(OUTPUT_WATERING_PIN, LOW);
    isWatering = false;
    logMessage("Watering complete.");
  }

  delay(10 * second);

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
  unsigned long timeSinceBoot = millis();
  unsigned long secondsSinceBoot = timeSinceBoot / 1000;
  unsigned long minutesSinceBoot = secondsSinceBoot / 60;
  unsigned long hoursSinceBoot = minutesSinceBoot / 60;
  
  secondsSinceBoot %= 60;
  minutesSinceBoot %= 60;

  String timestamp = "[" + String(hoursSinceBoot) + "h " + String(minutesSinceBoot) + "m " + String(secondsSinceBoot) + "s] ";

  String logEntry = timestamp + message;
  Serial.println(logEntry);
  
  if (client && client.connected()) {
    client.println(logEntry);
  }
}
