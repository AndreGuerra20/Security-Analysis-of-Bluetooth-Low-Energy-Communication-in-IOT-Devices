/* ESP32 BLE Server
   Implements a BLE server that advertises the SERVICE_UUID and exposes
   a characteristic with write and notify properties.
   Accepts BLE client connections, receives messages written to the
   CHARACTERISTIC_UUID, and when "Hello World!" is received,
   responds with a confirmation notification ("OK: received").
   After a client disconnects, advertising is automatically restarted to allow multiple client connections.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "583a4873-0bef-4f1f-807d-9a33868e213f"
#define CHARACTERISTIC_UUID "7807a0e0-d39f-4f1e-8e2e-83d62f4ac064"

// Set this constant to true if you want [INFO] logs, otherwise set to false
#define ENABLE_INFORMATION_LOGS true

BLECharacteristic *pCharacteristic;
volatile bool deviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  // Override the default methods
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] BLE Client connected.");
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] BLE Client disconnected. Resetting Advertising...");
    // Begin advertising again for multiple connections
    BLEDevice::startAdvertising();
  }
};

class CharCallbacks : public BLECharacteristicCallbacks {
  // Override the default method
  void onWrite(BLECharacteristic *pChar) override {
    String rx = pChar->getValue();
    if (rx.length() == 0 || !ENABLE_INFORMATION_LOGS) return;

    Serial.print("[INFO] Message Received: ");
    Serial.println(rx);

    if (rx == "Hello World!") {
      if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Correct Message: Hello World!");
      // Optional: respond to the client by notify
      pChar->setValue("OK: received");
      pChar->notify();
    }
  }
};

static BLEServer* initBLEDeviceAndServer(const char* deviceName) {
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initialising BLE Server...");
  BLEDevice::init(deviceName);

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  return server;
}

static void setupCharacteristic(BLEService *service, const char* charUuid) {
  pCharacteristic = service->createCharacteristic(
    charUuid,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new CharCallbacks());
}

static void setupAdvertising(const char* serviceUuid) {
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(serviceUuid);
  adv->setScanResponse(true);

  //adv->setMinPreferred(0x06);
  adv->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] BLE Server ready. Waiting for connections...");
}

void setup() {
  // baud rate
  Serial.begin(115200);

  BLEServer *pServer = initBLEDeviceAndServer("ESP32_BLE_Server");

  // Create Service 
  BLEService *pService = pServer->createService(SERVICE_UUID);

  setupCharacteristic(pService, CHARACTERISTIC_UUID);

  // Start Service
  pService->start();

  // Start Advertising
  setupAdvertising(SERVICE_UUID);
}

void loop() {
  // Hearbeat code -> Notifies every 5 seconds
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    if(ENABLE_INFORMATION_LOGS) Serial.printf("[INFO] Server is Alive. ClientIsConnected=%s\n", deviceConnected ? "Yes" : "No");
  }
  delay(10);
}