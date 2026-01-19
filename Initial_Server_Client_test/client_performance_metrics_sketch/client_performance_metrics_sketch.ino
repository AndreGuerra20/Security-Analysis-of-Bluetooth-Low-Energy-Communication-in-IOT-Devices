/* ESP32 BLE Client
   Scans for BLE devices that advertise the SERVICE_UUID.
   When one is found, it connects to the server and writes "Hello World!"
   to the CHARACTERISTIC_UUID. Then it waits 5 seconds and scans again.
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

#define SERVICE_UUID        "583a4873-0bef-4f1f-807d-9a33868e213f"
#define CHARACTERISTIC_UUID "7807a0e0-d39f-4f1e-8e2e-83d62f4ac064"

// Set this constant to true if you want [INFO] logs, otherwise set to false
#define ENABLE_INFORMATION_LOGS true
// Set this constant to true if you want [ERROR] logs, otherwise set to false
#define ENABLE_ERROR_LOGS true

// Constants for Perfomance Metrics
#define NUM_ROUNDS 20
#define SCAN_TIME  5

// Global Variables
unsigned long tStart;
unsigned long tEnd;
unsigned long roundTimes[NUM_ROUNDS];
int currentRound = 0;

BLEAdvertisedDevice* serverFound;
bool doConnect = false;
bool responseReceived = false;

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {}
  void onDisconnect(BLEClient* pclient) {
    responseReceived = true;
  }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if(ENABLE_INFORMATION_LOGS) {
      Serial.print("[INFO] Device found: ");
      Serial.println(advertisedDevice.toString().c_str());
    } 
  
    // Verify if the advertised device found has the Service UUID that we want
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO]==> Found device with correct Service UUID!");
      // Save the pointer to connect later
      serverFound = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      BLEDevice::getScan()->stop();
    }
  }
};

void printMetrics() {
  unsigned long sum = 0;
  unsigned long minT = ULONG_MAX;
  unsigned long maxT = 0;

  for (int i = 0; i < NUM_ROUNDS; i++) {
    sum += roundTimes[i];
    if (roundTimes[i] < minT) minT = roundTimes[i];
    if (roundTimes[i] > maxT) maxT = roundTimes[i];
  }

  float avg = (float)sum / NUM_ROUNDS;

  Serial.println("\n--- Métricas de Performance BLE ---");
  Serial.print("Rondas: "); Serial.println(NUM_ROUNDS);
  Serial.print("Média: "); Serial.print(avg); Serial.println(" ms");
  Serial.print("Mínimo: "); Serial.print(minT); Serial.println(" ms");
  Serial.print("Máximo: "); Serial.print(maxT); Serial.println(" ms");
}


bool connectAndExchange() {
  if(ENABLE_INFORMATION_LOGS) {
    Serial.print("[INFO] Trying to connect to: ");
    Serial.println(serverFound->getAddress().toString().c_str());
  }
  
  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.println("[INFO] Client Created");

  if (!pClient->connect(serverFound)) {
    if(ENABLE_ERROR_LOGS) Serial.println("[ERROR] Failed to connect");
    return false;
  }
  Serial.println("[INFO] Connected to the Server");

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    if(ENABLE_ERROR_LOGS) Serial.println("[ERROR] Service not found");
    pClient->disconnect();
    return false;
  }

  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    if(ENABLE_ERROR_LOGS) Serial.println("[ERROR] Characteristic not found");
    pClient->disconnect();
    return false;
  }

  // Write the message Hello World! to the Server (UTF-8 string)
  std::string mensagem = "Hello World!";
  if (ENABLE_INFORMATION_LOGS){
    Serial.print("[INFO] Sending: ");
    Serial.println(mensagem.c_str());
  }
  
  tStart = millis();

  pRemoteCharacteristic->writeValue((uint8_t*)mensagem.data(), mensagem.length(), false);
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Message Sent");

  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    Serial.print("READ: ");
    Serial.println(value.c_str());
  }

  tEnd = millis();
  pClient->disconnect();
  roundTimes[currentRound] = tEnd - tStart;

  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Connection with Served ended");
  return true;
}

void setup() {
  Serial.begin(115200);
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initializing BLE Client...");

  BLEDevice::init("BLE_Client_Test");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
}

void loop() {
  if (currentRound >= NUM_ROUNDS) {
    printMetrics();
    while (true); // Block execution to this line
  }

  doConnect = false;
  BLEDevice::getScan()->start(SCAN_TIME, false);

  if (doConnect && serverFound != nullptr) {
    if (connectAndExchange()) {
      Serial.print("Ronda ");
      Serial.print(currentRound);
      Serial.print(" tempo: ");
      Serial.print(roundTimes[currentRound]);
      Serial.println(" ms");
      currentRound++;
    }
  }
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Sleeping 10 seconds before new scan");
  delay(2000);
}