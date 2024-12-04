//json has not yet been created for color fade, it also it not being called in loop because its details aren't finalized, also functions have to be added
//from examples for different patterns, theatre rainbow etc.. at least 5 patterns exist in examples, mac address also needs to be added to .json packet

//This code uses BLE; ESP as server, nRF connect as client, sends ESP's mac address to client when client connects, if clients enters 'wifisetup', they are
//prompted to ask wifi ssid and password, when they enter, ESP connects to wifi, and sends back confirmation/disconnection. Then there are three keyword, 'on',
//'schedule' and 'colorfade', for turning LEDs on/off, for scheduling the on and off for LEDs, and for inducing color fade pattern, respectively. Being sent
//as .json packets to the server by client.

//only one characteristic UUID is being used

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_sntp.h"
#include "time.h"
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

#define PIN 13        // ESP Pin number
#define NUMPIXELS 5   // Number of LEDs

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Global Variables
BLECharacteristic *pCharacteristic;
int brightness = 100,  color_fade = 0;
int on_hour = 0, on_minute = 0, off_hour = 0, off_minute = 0;
int on_day = 0, on_month = 0, on_year = 0, off_day = 0, off_month = 0, off_year = 0;
int current_hour = 0, current_min = 0, current_sec = 0;
int current_day = 0, current_month = 0, current_year = 0;
int rval = 0, gval = 0, bval = 0;
int cf_on_hour = 0, cf_off_hour = 0, cf_on_minute = 0, cf_off_minute = 0; //color fade variables
bool is_on = false;
bool WiFiSetupRequested = false;
int step = 0; // Tracks the WiFi setup progress
String ssid = "", password = "";

// NTP server details
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 3600;

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Function Prototypes
void connectToWiFi();
void syncTime();
void handleJSONInput(String jsonString);
void saveScheduleToEEPROM();
void loadScheduleFromEEPROM();
bool TimeMatch(int current_hour, int current_minute, int target_hour, int target_minute);
bool DateMatch(int current_day, int current_month, int current_year, int target_day, int target_month, int target_year);
void LEDTurner(int LED_Number, uint8_t Rval, uint8_t Gval, uint8_t Bval, uint8_t brightness);
void updateLocalTime();
void colorfadeIn(int cf_on_hr, int cf_on_min, int cf_off_hr, int cf_off_min, int brightness, int Rval, int Gval, int Bval);
void colorfadeOut(int cf_on_hr, int cf_on_min, int cf_off_hr, int cf_off_min, int brightness, int Rval, int Gval, int Bval);

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
    String value = String(pCharacteristic->getValue().c_str());
    value.toLowerCase(); // Make input case-insensitive
    if (value.length() > 0) {
      Serial.println("Received value: " + value);

      // WiFi Setup Handling
      if (value == "wifisetup") {
        WiFiSetupRequested = true;
        pCharacteristic->setValue("Enter WiFi SSID:");
        pCharacteristic->notify();
        Serial.println("WiFi setup initiated. Awaiting SSID...");
        step = 0; // Start WiFi setup
        return;
      }

      if (WiFiSetupRequested) {
        if (step == 0) {
          ssid = value; // Store SSID
          pCharacteristic->setValue("Enter WiFi password:");
          pCharacteristic->notify();
          Serial.println("Received SSID: " + ssid);
          step = 1;
          return;
        } else if (step == 1) {
          password = value; // Store Password
          pCharacteristic->setValue("Attempting WiFi connection...");
          pCharacteristic->notify();
          Serial.println("Received Password: " + password);

          // Save WiFi credentials to EEPROM
          EEPROM.writeString(48, ssid);
          EEPROM.writeString(66, password);
          EEPROM.commit();

          // Attempt WiFi connection
          connectToWiFi();

          if (WiFi.status() == WL_CONNECTED) {
            pCharacteristic->setValue("WiFi connected!");
            pCharacteristic->notify();
            Serial.println("WiFi connected successfully.");
          } else {
            pCharacteristic->setValue("Failed to connect to WiFi. Retry wifisetup.");
            pCharacteristic->notify();
            Serial.println("WiFi connection failed.");
          }

          WiFiSetupRequested = false; // Reset the flag
          step = 0;
          return;
        }
      }

      // JSON Command Handling
      handleJSONInput(value);
    }
  }
};

void setup() {
  Serial.begin(115200);
  EEPROM.begin(128);  // Adjust size based on usage

  // Load WiFi credentials from EEPROM
  ssid = EEPROM.readString(48);
  password = EEPROM.readString(66);
  if (ssid.isEmpty() || password.isEmpty()) {
    Serial.println("No WiFi credentials found in EEPROM.");
  }

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
  updateLocalTime();
  loadScheduleFromEEPROM();

  if (DateMatch(current_day, current_month, current_year, on_day, on_month, on_year) && 
      TimeMatch(current_hour, current_min, on_hour, on_minute)) {
    Serial.println("ON time condition met!");
    while (!(TimeMatch(current_hour, current_min, off_hour, off_minute) && 
             DateMatch(current_day, current_month, current_year, off_day, off_month, off_year))) {
      is_on = true;
      Serial.println("Turning LED ON (Scheduled)");
      LEDTurner(NUMPIXELS, rval, gval, bval, brightness);
      pCharacteristic->setValue("LED is ON (Scheduled)");
      pCharacteristic->notify();
      updateLocalTime();  
      delay(1000);
    }
    Serial.println("Scheduled OFF condition met. Turning LED OFF.");
    is_on = false;
    LEDTurner(NUMPIXELS, 0, 0, 0, 0);
  }

  delay(5000);
}

void connectToWiFi() {
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.println("Attempting WiFi connection...");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
  } else {
    Serial.println("\nWiFi connection failed.");
  }
}

void syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  Serial.println("Time synchronized.");
}

void updateLocalTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    current_hour = timeinfo.tm_hour;
    current_min = timeinfo.tm_min;
    current_sec = timeinfo.tm_sec;
    current_day = timeinfo.tm_mday;
    current_month = timeinfo.tm_mon + 1;
    current_year = timeinfo.tm_year + 1900;

    Serial.printf("Current Time: %02d:%02d:%02d\n", current_hour, current_min, current_sec);
    Serial.printf("Current Date: %02d-%02d-%04d\n", current_day, current_month, current_year);
  } else {
    Serial.println("Failed to fetch current time from NTP.");
  }
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
    rval = doc["rval"];
    gval = doc["gval"];
    bval = doc["bval"];
    color_fade = doc["color_fade"];
    sscanf(doc["on_date"], "%d/%d/%d", &on_day, &on_month, &on_year);
    sscanf(doc["on_time"], "%d:%d", &on_hour, &on_minute);
    sscanf(doc["off_date"], "%d/%d/%d", &off_day, &off_month, &off_year);
    sscanf(doc["off_time"], "%d:%d", &off_hour, &off_minute);
    brightness = (255/100)*brightness;

    saveScheduleToEEPROM();
    Serial.println("Schedule stored.");
  } 
  else if (keyword == "on") {
    String action = doc["action"]; // Get "on" or "off" action from JSON
    brightness = doc["brightness"];
    rval = doc["rval"];
    gval = doc["gval"];
    bval = doc["bval"];
    brightness = (255/100)*brightness;

    if (action == "on") {
      is_on = true;
      Serial.println("Action: ON, Turning LED ON");
      LEDTurner(NUMPIXELS, rval, gval, bval, brightness); 
    } 
    else if (action == "off") {
      is_on = false;
      Serial.println("Action: OFF, Turning LED OFF");
      LEDTurner(NUMPIXELS, 0, 0, 0, 0); //turn off all LEDs
    } 
    Serial.printf("LED State: %s\n", is_on ? "ON" : "OFF");
  }
  else if (keyword == "colorfade") {
    String action = doc["action"]; // Get "on" or "off" action from JSON
    brightness = doc["brightness"];
    rval = doc["rval"];
    gval = doc["gval"];
    bval = doc["bval"];
    color_fade = doc["color_fade"];
    brightness = (255/100)*brightness;

    if (action == "on") {
      is_on = true;
      Serial.println("Action: ON, Turning LED ON");
      LEDTurner(NUMPIXELS, rval, gval, bval, brightness); 
    } 
    else if (action == "off") {
      is_on = false;
      Serial.println("Action: OFF, Turning LED OFF");
      LEDTurner(NUMPIXELS, 0, 0, 0, 0); //turn off all LEDs
    } 
    Serial.printf("LED State: %s\n", is_on ? "ON" : "OFF");
  }
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

bool TimeMatch(int current_hour, int current_minute, int target_hour, int target_minute) {
  return (current_hour == target_hour && current_minute == target_minute);
}

bool DateMatch(int current_day, int current_month, int current_year, int target_day, int target_month, int target_year) {
  return (current_day == target_day && current_month == target_month && current_year == target_year);
}

void LEDTurner(int LED_Number, uint8_t Rval, uint8_t Gval, uint8_t Bval, uint8_t bright) {
  pixels.clear();
  pixels.show(); 
  delay(200);

  pixels.setBrightness(bright);

  for (int i = 0; i < LED_Number; i++) {
    pixels.setPixelColor(i, pixels.Color(Rval, Gval, Bval));
    pixels.show();
    delay(200);
  }
}

void colorfade(bool InOut, int cf_on_hr, int cf_on_min, int cf_off_hr, int cf_off_min, int bright, int Rval, int Gval, int Bval) {
  //InOut = true means fadeIn, going from off to on (Sunrise)
  //InOut = false means fadeOut, going from on to off (Sunset)
  int maxBrightness = map(bright, 0, 100, 0, 255);
  if (InOut == true){
    float currentBrightness = 0.0;
  }
  else if (InOut == false){
    float currentBrightness = maxBrightness;
  }
  pixels.clear();
  pixels.show();
  pixels.setBrightness(0);

  int diff = 0;
  if (cf_on_hr < cf_off_hr || (cf_on_hr == cf_off_hr && cf_on_min < cf_off_min)) {
    if (cf_on_hr == cf_off_hr){
      diff = (cf_off_min - cf_on_min) * 60;
    }
    else if(cf_on_hr < cf_off_hr){
      diff = (((cf_off_hr - cf_on_hr -1) * 3600) + ((cf_off_min - cf_on_min) * 60)); // Total seconds
    } 
  }
  else {
    Serial.println("Error: On time must be earlier than off time.");
    return;
  }

  if (diff <= 0) {
    Serial.println("Error: Invalid time difference.");
    return;
  }

  float increment = (float)maxBrightness / diff; // Increment per second
  Serial.printf("Max Brightness: %d, Increment per second: %f, Total Time (sec): %d\n", maxBrightness, increment, diff);

  unsigned long previousMillis = millis();
  int elapsedSeconds = 0;

  while (elapsedSeconds < diff) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 1000) { // 1 second elapsed
      previousMillis = currentMillis;
      if (InOut == true){
        currentBrightness += increment;
      }
      else if (InOut == false){
        currentBrightness -= increment;
      }
      int roundedBrightness = (int)currentBrightness; // Convert to integer for NeoPixel
      if (InOut == true){
        if (roundedBrightness > maxBrightness) roundedBrightness = maxBrightness;;
      }
      else if (InOut == false){
        if (currentBrightness < 0) currentBrightness = 0;
      }
        pixels.setBrightness(roundedBrightness);
      for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(Rval, Gval, Bval)); // Set all LEDs to the requested color
      }
      pixels.show();
      elapsedSeconds++;
      Serial.printf("Elapsed Seconds: %d, Rounded Brightness: %d\n", elapsedSeconds, roundedBrightness);
    }
  }
  Serial.println("Fade-in completed.");
}
