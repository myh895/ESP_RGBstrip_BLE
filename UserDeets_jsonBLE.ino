//connects to client (nRF used) through BLE, sends ESP mac address to client, and takes json packets from client, with 2 options:
//1. Scheduling: client sends brightness etc and on off times which get stored in eeprom and used for comparison to turn on led. Time retrieved from ntp server
//2. Simple on/off: on or off command along with brightness etc to manually turn light on/off
//LED changes not implemented yet

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "time.h"
#include <EEPROM.h>

// WiFi credentials
const char *ssid = "PSC-2.4G";
const char *password = "karachi123";

// NTP server details
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Global Variables
BLECharacteristic *pCharacteristic;
int brightness, intensity, color_fade;
int on_hour, on_minute, off_hour, off_minute;
int on_day, on_month, on_year, off_day, off_month, off_year;
bool is_on = false;

// Function Prototypes
void connectToWiFi();
void syncTime();
void handleJSONInput(String jsonString);
void controlLED(int brightness, int intensity, int color_fade);
void saveScheduleToEEPROM();
void loadScheduleFromEEPROM();
bool isTimeMatch(int current_hour, int current_minute, int target_hour, int target_minute);
bool isDateMatch(int current_day, int current_month, int current_year, int target_day, int target_month, int target_year);

// BLE Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("Client connected!");
    String macAddress = BLEDevice::getAddress().toString();
    pCharacteristic->setValue("ESP MAC: " + macAddress);
    pCharacteristic->notify();
  }

  void onDisconnect(BLEServer *pServer) {
    Serial.println("Client disconnected.");
    BLEDevice::startAdvertising();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String jsonString = String(pCharacteristic->getValue().c_str());
    if (jsonString.length() > 0) {
      Serial.println("Received JSON: " + jsonString);
      handleJSONInput(jsonString);
    }
  }
};

void setup() {
  Serial.begin(115200);
  EEPROM.begin(64);  // Adjust size based on usage

  connectToWiFi();
  syncTime();

  BLEDevice::init("ESP32_BLE");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->setValue("Waiting for client...");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  loadScheduleFromEEPROM();  // Load schedule data from EEPROM

  Serial.println("BLE ready and advertising...");
}

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int current_hour = timeinfo.tm_hour;
    int current_minute = timeinfo.tm_min;
    int current_day = timeinfo.tm_mday;
    int current_month = timeinfo.tm_mon + 1;
    int current_year = timeinfo.tm_year + 1900;

    // Load schedule from EEPROM for comparison
    loadScheduleFromEEPROM();

    // Compare current time with scheduled time
    if (isDateMatch(current_day, current_month, current_year, on_day, on_month, on_year) &&
        isTimeMatch(current_hour, current_minute, on_hour, on_minute)) {
      is_on = true;
      Serial.println("Turning LED ON (Scheduled)");
      controlLED(brightness, intensity, color_fade);
    }

    if (isDateMatch(current_day, current_month, current_year, off_day, off_month, off_year) &&
        isTimeMatch(current_hour, current_minute, off_hour, off_minute)) {
      is_on = false;
      Serial.println("Turning LED OFF (Scheduled)");
      controlLED(0, 0, 0);  // Turn off LED
    }
  }
  delay(1000);
}

// Function Definitions
void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}

void syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  Serial.println("Time synchronized.");
}

void handleJSONInput(String jsonString) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    Serial.println("Failed to parse JSON.");
    return;
  }

  String keyword = doc["keyword"];
  if (keyword == "schedule") {
    brightness = doc["brightness"];
    intensity = doc["intensity"];
    color_fade = doc["color_fade"];
    sscanf(doc["on_date"], "%d %d %d", &on_day, &on_month, &on_year);
    sscanf(doc["on_time"], "%d %d", &on_hour, &on_minute);
    sscanf(doc["off_date"], "%d %d %d", &off_day, &off_month, &off_year);
    sscanf(doc["off_time"], "%d %d", &off_hour, &off_minute);

    saveScheduleToEEPROM();
    Serial.println("Schedule stored.");
  } else if (keyword == "on") {
    String action = doc["action"]; // Get "on" or "off" action from JSON
    brightness = doc["brightness"];
    intensity = doc["intensity"];
    color_fade = doc["color_fade"];

    // Set LED state based on action
    is_on = (action == "on");
    Serial.printf("Action: %s, LED State: %s\n", action.c_str(), is_on ? "ON" : "OFF");

    // Apply the LED control
    controlLED(is_on ? brightness : 0, is_on ? intensity : 0, is_on ? color_fade : 0);
    Serial.println("Manual control applied.");
  }
}

void controlLED(int brightness, int intensity, int color_fade) {
  // Add LED control logic here (e.g., NeoPixel or PWM control)
  Serial.printf("LED Control -> Brightness: %d, Intensity: %d, Color Fade: %d\n",
                brightness, intensity, color_fade);
}

void saveScheduleToEEPROM() {
  EEPROM.writeInt(0, on_day);
  EEPROM.writeInt(4, on_month);
  EEPROM.writeInt(8, on_year);
  EEPROM.writeInt(12, on_hour);
  EEPROM.writeInt(16, on_minute);
  EEPROM.writeInt(20, off_day);
  EEPROM.writeInt(24, off_month);
  EEPROM.writeInt(28, off_year);
  EEPROM.writeInt(32, off_hour);
  EEPROM.writeInt(36, off_minute);
  EEPROM.commit();
}

void loadScheduleFromEEPROM() {
  on_day = EEPROM.readInt(0);
  on_month = EEPROM.readInt(4);
  on_year = EEPROM.readInt(8);
  on_hour = EEPROM.readInt(12);
  on_minute = EEPROM.readInt(16);
  off_day = EEPROM.readInt(20);
  off_month = EEPROM.readInt(24);
  off_year = EEPROM.readInt(28);
  off_hour = EEPROM.readInt(32);
  off_minute = EEPROM.readInt(36);

  // Debugging EEPROM values
  Serial.printf("Loaded from EEPROM -> On Date: %02d-%02d-%04d, On Time: %02d:%02d\n",
                on_day, on_month, on_year, on_hour, on_minute);
  Serial.printf("Off Date: %02d-%02d-%04d, Off Time: %02d:%02d\n",
                off_day, off_month, off_year, off_hour, off_minute);
}

bool isTimeMatch(int current_hour, int current_minute, int target_hour, int target_minute) {
  return (current_hour == target_hour && current_minute == target_minute);
}

bool isDateMatch(int current_day, int current_month, int current_year, int target_day, int target_month, int target_year) {
  return (current_day == target_day && current_month == target_month && current_year == target_year);
}
