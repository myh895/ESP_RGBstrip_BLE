#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"

#define PIN 13       // ESP Pin number
#define NUMPIXELS 5   // Number of LEDs

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

const char *ssid = "UTF-LABS-Fast";
const char *password = "7WP683jx0HC%";

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 18000;
const int daylightOffset_sec = 3600;

int current_hour = 0, current_min = 0;

void timeavailable(struct timeval *t) {
  Serial.println("Got time adjustment from NTP!");
  updateLocalTime();
}

void colorfadeIn(int cf_on_hr, int cf_on_min, int cf_off_hr, int cf_off_min, int bright, int Rval, int Gval, int Bval) {
  int maxBrightness = map(bright, 0, 100, 0, 255);
  float currentBrightness = 0.0;

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
      currentBrightness += increment; // Accumulate floating-point brightness
      int roundedBrightness = (int)currentBrightness; // Convert to integer for NeoPixel
      if (roundedBrightness > maxBrightness) roundedBrightness = maxBrightness;
      pixels.setBrightness(roundedBrightness);
      for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(Rval, Gval, Bval)); // Set all LEDs to the requested color
      }
      pixels.setBrightness(roundedBrightness);
      pixels.show();
      elapsedSeconds++;
      Serial.printf("Elapsed Seconds: %d, Rounded Brightness: %d\n", elapsedSeconds, roundedBrightness);
    }
  }
  Serial.println("Fade-in completed.");
}

void updateLocalTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    current_hour = timeinfo.tm_hour;
    current_min = timeinfo.tm_min;
    Serial.printf("Current Time: %02d:%02d\n", current_hour, current_min);
  } else {
    Serial.println("Failed to fetch current time from NTP.");
  }
}

bool TimeMatch(int current_hour, int current_minute, int target_hour, int target_minute) {
  return (current_hour == target_hour && current_minute == target_minute);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  esp_sntp_servermode_dhcp(1); // (optional)
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  pixels.begin(); // Initialize NeoPixel
  pixels.clear();
  pixels.show();

  sntp_set_time_sync_notification_cb(timeavailable);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  delay(3000);
  updateLocalTime();
}

void loop() {
  int cf_on_hour = 12, cf_off_hour = 12, cf_on_minute = 49, cf_off_minute = 51;
  updateLocalTime();
  if (TimeMatch(current_hour, current_min, cf_on_hour, cf_on_minute)) {
    Serial.println("ON time condition met!");
    Serial.println("Turning LED ON (Scheduled)");
    colorfadeIn(cf_on_hour, cf_on_minute, cf_off_hour, cf_off_minute, 90, 240, 120, 180);
    Serial.println("The end.");
  }
  delay(5000);
}
