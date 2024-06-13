#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

const char* ssid = "Wall-e";
const char* password = "sausis45";

#define BAUD_RATE 9600
#define DRY_VALUE 650
#define WET_VALUE 276
#define WATER_PORT D1
#define WATERING_TIME 120000  // 2 minutes in milliseconds
#define MAX_DAILY_WATERING_TIME 1800000  // 30 minutes in milliseconds
#define WIFI_TIMEOUT 30000  // 30 seconds in milliseconds
#define HOUR_DELAY 3600000

unsigned long dailyWateringTime = 0;
unsigned long lastResetTime = 0;
bool timeRetrieved = false;
int currentHour = 0;

// NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, HOUR_DELAY);

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
      Serial.print("Current hour: ");
      Serial.println(currentHour);
    } else {
      timeRetrieved = false;
      Serial.println("Failed to retrieve time from NTP server.");
    }
  }

  int sensorValue = analogRead(A0);
  int moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
  moisturePercentage = constrain(moisturePercentage, 0, 100);

  Serial.print("Soil Moisture Value: ");
  Serial.print(sensorValue);
  Serial.print(" -> ");
  Serial.print(moisturePercentage);
  Serial.println("%");

  bool withinTimeRange = timeRetrieved ? (currentHour >= 10 && currentHour < 18) : true;

  if (moisturePercentage < 50 && dailyWateringTime < MAX_DAILY_WATERING_TIME && withinTimeRange) {
    Serial.println("Moisture is below 50%, starting watering...");
    digitalWrite(WATER_PORT, HIGH);
    unsigned long startWateringTime = millis();
    while (millis() - startWateringTime < WATERING_TIME) {
      sensorValue = analogRead(A0);
      moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
      moisturePercentage = constrain(moisturePercentage, 0, 100);

      if (moisturePercentage > 60 || dailyWateringTime >= MAX_DAILY_WATERING_TIME) {
        Serial.println("Moisture exceeds 60% or daily limit reached, stopping watering...");
        break;
      }
      
      Serial.print("During Watering - Soil Moisture Value: ");
      Serial.print(sensorValue);
      Serial.print(" -> ");
      Serial.print(moisturePercentage);
      Serial.println("%");

      delay(1000);
    }
    unsigned long wateringDuration = millis() - startWateringTime;
    dailyWateringTime += wateringDuration;
    digitalWrite(WATER_PORT, LOW);
    Serial.println("Watering complete.");
  }

  delay(1000);
}
