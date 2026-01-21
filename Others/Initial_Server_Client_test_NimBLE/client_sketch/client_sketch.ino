/* ESP32 BLE Client (NimBLE-Arduino)
   Scans for BLE devices that advertise the SERVICE_UUID.
   When one is found, it connects to the server and writes "Hello World!"
   to the CHARACTERISTIC_UUID. Then it waits 10 seconds and scans again.
*/

#include <NimBLEDevice.h>

#define SERVICE_UUID        "583a4873-0bef-4f1f-807d-9a33868e213f"
#define CHARACTERISTIC_UUID "7807a0e0-d39f-4f1e-8e2e-83d62f4ac064"

// Set this constant to true if you want [INFO] logs, otherwise set to false
#define ENABLE_INFORMATION_LOGS true
// Set this constant to true if you want [ERROR] logs, otherwise set to false
#define ENABLE_ERROR_LOGS true

static bool doConnect = false;
static NimBLEAdvertisedDevice* serverFound = nullptr;

class MyScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {

    if (ENABLE_INFORMATION_LOGS) {
      Serial.print("[INFO] Device found: ");
      Serial.println(advertisedDevice->toString().c_str());
    }

    if (advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
      if (ENABLE_INFORMATION_LOGS)
        Serial.println("[INFO]==> Found device with correct Service UUID!");

      if (serverFound) {
        delete serverFound;
        serverFound = nullptr;
      }

      serverFound = new NimBLEAdvertisedDevice(*advertisedDevice);
      doConnect = true;

      NimBLEDevice::getScan()->stop();
    }
  }
};

static bool connectToServer(NimBLEAdvertisedDevice* device) {
  if (!device) return false;

  if (ENABLE_INFORMATION_LOGS) {
    Serial.print("[INFO] Trying to connect to: ");
    Serial.println(device->getAddress().toString().c_str());
  }

  NimBLEClient* pClient = NimBLEDevice::createClient();
  if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Client Created");

  if (!pClient->connect(device)) {
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] Failed to connect");
    NimBLEDevice::deleteClient(pClient);
    return false;
  }

  if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Connected to the Server");

  NimBLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] Service not found");
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    return false;
  }

  NimBLERemoteCharacteristic* pRemoteCharacteristic =
      pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);

  if (pRemoteCharacteristic == nullptr) {
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] Characteristic not found");
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    return false;
  }

  // Write the message "Hello World!" to the Server (UTF-8 string)
  std::string message = "Hello World!";
  if (ENABLE_INFORMATION_LOGS) {
    Serial.print("[INFO] Sending: ");
    Serial.println(message.c_str());
  }

  bool ok = pRemoteCharacteristic->writeValue(
      (uint8_t*)message.data(), message.length(), false /* response */);

  if (ok) {
    if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Message Sent");
  } else {
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] Write failed");
  }

  // Optional: keep the connection for 2 seconds to guarantee delivery
  delay(2000);

  pClient->disconnect();
  if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Connection with Server ended");

  NimBLEDevice::deleteClient(pClient);
  return ok;
}

static void startScanOnce(uint32_t seconds) {
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(new MyScanCallbacks(), true /* wantDuplicates */);
  pScan->setActiveScan(true);
  pScan->start(seconds, false);
}

void setup() {
  Serial.begin(115200);
  if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initializing BLE Client (NimBLE)...");

  NimBLEDevice::init("");

  // Start an initial scan
  startScanOnce(5);
}

void loop() {
  if (doConnect && serverFound != nullptr) {
    connectToServer(serverFound);

    // Reset flags
    doConnect = false;
    delete serverFound;
    serverFound = nullptr;

    if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Sleeping 10 seconds before new scan");
    delay(10000);

    // Start a new scan
    startScanOnce(5);
  }

  delay(200);
}