#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <EEPROM.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

String ssid = "";
String password = "";

BLECharacteristic *pCharacteristic; // Global characteristic pointer
int step = 0; //step from ssid to password
bool WiFiSetupRequested = false; //Wifi setup/change request flag

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println(F("Client connected!"));
    String espMac = BLEDevice::getAddress().toString(); // Get BLE MAC address
    String message = "Device found: " + espMac;

    pCharacteristic->setValue(message.c_str()); //sending ESP mac address to client 
    pCharacteristic->notify();
    Serial.println("Sent to phone: " + message);
}

  void onDisconnect(BLEServer *pServer) {
    Serial.println(F("Client disconnected. Restarting advertising..."));
    BLEDevice::startAdvertising();
  }
};

void connectToWiFi() {
  
  ssid = EEPROM.readString(0);
  password = EEPROM.readString(24);

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.println(F("Attempting WiFi connection..."));
  
  //wait 60 seconds for wifi connection
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 60000) {
    delay(500);
    Serial.print(F("."));
    pCharacteristic->setValue("Connecting to WiFi...");
    pCharacteristic->notify();
  }

  if (WiFi.status() == WL_CONNECTED) {
    String successMessage = "Connected to WiFi: " + WiFi.localIP().toString();
    pCharacteristic->setValue(successMessage.c_str());
    pCharacteristic->notify();
    Serial.println(F("WiFi connected."));
    WiFiSetupRequested = false;
  } 
  
  else {
    pCharacteristic->setValue("Could not connect to WiFi. Please re-enter credentials.");
    pCharacteristic->notify();
    step = 0;  // Restart from SSID input  // Reset the flag to ask for credentials again
  }
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = String(pCharacteristic->getValue().c_str());

    if (value.length() > 0){
    //String command = value; // Preserve original input
    //command.toLowerCase();  // Convert to lowercase for comparison

      if (value == "wifisetup") { // Check command in lowercase
        WiFiSetupRequested = true;
        pCharacteristic->setValue("Enter WiFi SSID:");
        pCharacteristic->notify();
        Serial.println("WiFi change requested. Prompting for SSID.");
        step = 0;
      } 
      else if (WiFiSetupRequested) {
        if (step == 0) {
          ssid = value; // Use the original case-sensitive input
          pCharacteristic->setValue("Enter WiFi password:");
          pCharacteristic->notify();
          Serial.println("Received SSID: " + ssid);
          step = 1;
        } 
        else if (step == 1) {
          password = value; // Use the original case-sensitive input
          step = 2;
          Serial.println("Received Password: " + password);

          // Store SSID and password in EEPROM
          EEPROM.writeString(0, ssid);
          EEPROM.writeString(24, password);
          EEPROM.commit();

          // Attempt WiFi connection
          connectToWiFi();
        }
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  EEPROM.begin(36);

  // Initialize BLE
  BLEDevice::init("ESP32_Yousuf");
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
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Helps with iPhone connections
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  //Serial.println(F("BLE Server is ready and advertising..."));
}

void loop() {
  delay(1000);
}