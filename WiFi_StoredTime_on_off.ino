#include <WiFi.h>
#include "time.h"
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>
#include "esp_sntp.h"
#include <EEPROM.h>
#include <Preferences.h>

const char *ssid = "PSC-2.4G";
const char *password = "karachi123";

#define EEPROM_SIZE 4 // EEPROM size
#define PIN 13        // ESP Pin number
#define NUMPIXELS 5   // Number of LEDs

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 18000; // GMT offset in seconds (+5)
const int daylightOffset_sec = 3600;

byte current_hour = 0, current_min = 0, current_sec = 0;
byte on_hour = 0, on_minute = 0, off_hour = 0, off_minute = 0;
int on_hour_address = 0, on_minute_address = 1, off_hour_address = 2, off_minute_address = 3;

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time from NTP server.");
    return;
  }
  current_hour = timeinfo.tm_hour;
  current_min = timeinfo.tm_min;
  current_sec = timeinfo.tm_sec;
  Serial.printf("Current time: %02d:%02d:%02d\n", current_hour, current_min, current_sec);
}

void timeavailable(struct timeval *t) {
  Serial.println("Time synchronized via NTP!");
  printLocalTime();
}

void LEDTurner(int LED_Number, uint8_t Rval, uint8_t Gval, uint8_t Bval, uint8_t brightness) {
  pixels.clear();
  pixels.show(); 
  delay(200);

  pixels.setBrightness(brightness);

  for (int i = 0; i < LED_Number; i++) {
    pixels.setPixelColor(i, pixels.Color(Rval, Gval, Bval));
    pixels.show();
    delay(200);
  }
}

void Time_change() {
  Serial.println("Setting new on/off times.");
  on_hour = 16; 
  on_minute = 4; 
  off_hour = 16; 
  off_minute = 5; 
  EEPROM.write(on_hour_address, on_hour);
  EEPROM.write(on_minute_address, on_minute);
  EEPROM.write(off_hour_address, off_hour);
  EEPROM.write(off_minute_address, off_minute);
  EEPROM.commit();
  Serial.printf("New on time: %02d:%02d\n", on_hour, on_minute);
}

void EEPROM_reset(){
  on_hour = 0; // Example value
  on_minute = 0; // Example value
  EEPROM.write(on_hour_address, on_hour);
  EEPROM.write(on_minute_address, on_minute);
  EEPROM.commit();
}

void setup() {
  pixels.begin(); // Initialize NeoPixel
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  // Read stored values from EEPROM
  on_hour = EEPROM.read(on_hour_address);
  on_minute = EEPROM.read(on_minute_address);
  off_hour = EEPROM.read(off_hour_address);
  off_minute = EEPROM.read(off_minute_address);

  Serial.printf("Read on_hour: %d, on_minute: %d, off_hour: %d, off_minute: %d\n", on_hour, on_minute, off_hour, off_minute);

  // Validate EEPROM data (initialize if invalid)
  if (on_hour > 23 || on_minute > 59 || off_hour > 23 || off_minute > 59) {
    Serial.println("Invalid EEPROM data. Initializing default values.");
    on_hour = 0;
    on_minute = 0;
    off_hour = 0;
    off_minute = 0;
    EEPROM.write(on_hour_address, on_hour);
    EEPROM.write(on_minute_address, on_minute);
    EEPROM.write(off_hour_address, off_hour);
    EEPROM.write(off_minute_address, off_minute);
    EEPROM.commit();
  }

  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  sntp_set_time_sync_notification_cb(timeavailable);

  Time_change();
}

void loop() {
  delay(5000);
  printLocalTime();
  if (current_hour == on_hour && current_min == on_minute) {
    Serial.println("Turning on LEDs!");
    while (!(current_hour == off_hour && current_min == off_minute)){
      LEDTurner(NUMPIXELS, 200, 0, 50, 155); // Turn LEDs on for 2 minutes
      printLocalTime();
    }
    LEDTurner(NUMPIXELS, 0, 0, 0, 0); //turn off all LEDs
    //EEPROM_reset();
  } 
  else {
    printLocalTime();
    //EEPROM_reset();
  }
}

