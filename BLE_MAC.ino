//This code connects the ESP to our phone using BLE through the nRF app and sends the ESP's MAC address to the client

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic; // Global characteristic pointer

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("Client connected!");
    // Get ESP's BLE MAC address
    String espMac = BLEDevice::getAddress().toString(); // Get BLE MAC address
    String message = "Device found: " + espMac;

    // Send ESP's MAC address to the phone
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
    Serial.println("Sent to phone: " + message);
  }

  void onDisconnect(BLEServer *pServer) {
    Serial.println("Client disconnected. Restarting advertising...");
    BLEDevice::startAdvertising();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = String(pCharacteristic->getValue().c_str());
    if (value.length() > 0) {
      Serial.print("Acknowledgment received from phone: ");
      Serial.println(value);
      if (value == "ACK") {
        Serial.println("Phone acknowledged the MAC address.");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);

  // Initialize BLE
  BLEDevice::init("ESP32_MAC_Sender");
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

  Serial.println("BLE Server is ready and advertising...");
}

void loop() {
  delay(1000);
}
