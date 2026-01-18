/* ESP32 BLE Server (NimBLE-Arduino)
   Implements a BLE server that advertises the SERVICE_UUID and exposes
   a characteristic with write and notify properties.
   Accepts BLE client connections, receives messages written to the
   CHARACTERISTIC_UUID, and when "Hello World!" is received,
   responds with a confirmation notification ("OK: received").
   After a client disconnects, advertising is automatically restarted to allow multiple client connections.
*/

#include <NimBLEDevice.h>

#define SERVICE_UUID        "583a4873-0bef-4f1f-807d-9a33868e213f"
#define CHARACTERISTIC_UUID "7807a0e0-d39f-4f1e-8e2e-83d62f4ac064"

// Set this constant to true if you want [INFO] logs
#define ENABLE_INFORMATION_LOGS true

static NimBLECharacteristic* pCharacteristic = nullptr;
static volatile bool deviceConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    if (ENABLE_INFORMATION_LOGS)
      Serial.println("[INFO] BLE Client connected.");
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    if (ENABLE_INFORMATION_LOGS)
      Serial.println("[INFO] BLE Client disconnected. Restarting advertising.");

    NimBLEDevice::startAdvertising();
  }
};

class CharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    std::string rx = pChar->getValue();

    if (ENABLE_INFORMATION_LOGS) {
      Serial.print("[INFO] Message Received: ");
      Serial.println(rx.c_str());
    }

    // Optional notify response
    if (rx == "Hello World!") {
      pChar->setValue("OK: received");
      pChar->notify();
    }
  }
};

static void setupAdvertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06); // LE General Discoverable + BR/EDR not supported
  advData.setName("ESP32_BLE_Server");
  advData.addServiceUUID(NimBLEUUID(SERVICE_UUID));

  NimBLEAdvertisementData scanRespData; // optional, can stay empty

  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanRespData);

  NimBLEDevice::startAdvertising();

  if (ENABLE_INFORMATION_LOGS)
    Serial.println("[INFO] Advertising started.");
}

void setup() {
  Serial.begin(115200);

  if (ENABLE_INFORMATION_LOGS)
    Serial.println("[INFO] Initialising BLE Server (NimBLE)...");

  NimBLEDevice::init("ESP32_BLE_Server");

  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::WRITE_NR |
    NIMBLE_PROPERTY::NOTIFY
  );

  pCharacteristic->setCallbacks(new CharCallbacks());

  pService->start();

  setupAdvertising();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    if (ENABLE_INFORMATION_LOGS) {
      Serial.printf(
        "[INFO] Server alive. ClientIsConnected=%s\n",
        deviceConnected ? "Yes" : "No"
      );
    }
  }
  delay(10);
}