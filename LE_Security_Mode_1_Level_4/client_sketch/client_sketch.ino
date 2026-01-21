/* ESP32 BLE Client (Secure Performance Test)
 *
 * Scans for BLE devices advertising SERVICE_UUID and connects to the first match.
 *
 * Security:
 * - BLE LE Security Mode 1, Level 4
 * - Authenticated pairing with MITM protection
 * - LE Secure Connections (SC)
 * - Encrypted link enforced via secureConnection()
 *
 * For each communication round:
 * - Establishes a secure BLE connection with the server.
 * - Writes the command "TEMP" to CHARACTERISTIC_UUID.
 * - Reads the server response (UTF-8 sensor value).
 * - Measures the elapsed time between write and read.
 *
 * After NUM_ROUNDS, average, minimum, and maximum latency metrics are printed
 * (the first round is ignored to reduce warm-up effects).
 */
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
// 
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>
static const uint32_t STATIC_PASSKEY = 195374; // 6 digits

#define SERVICE_UUID        "1714cc76-6a69-4d96-a7ca-3811e1868f4b"
#define CHARACTERISTIC_UUID "2d7c17eb-9f22-4b87-9172-5a95e158d6c7"

// Set this constant to true if you want [INFO] logs, otherwise set to false
#define ENABLE_INFORMATION_LOGS true
// Set this constant to true if you want [ERROR] logs, otherwise set to false
#define ENABLE_ERROR_LOGS true

// Constants for Perfomance Metrics
#define NUM_ROUNDS 20+1 // The first round is excluded from the average calculation to 
                        // ensure a more representative performance measurement.
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

  // Start at 1 to ignore the first round
  for (int i = 1; i < NUM_ROUNDS; i++) {
    sum += roundTimes[i];
    if (roundTimes[i] < minT) minT = roundTimes[i];
    if (roundTimes[i] > maxT) maxT = roundTimes[i];
  }

  float avg = (float)sum / NUM_ROUNDS;

  Serial.println("\n--- BLE Performance Metrics ---");
  Serial.print("Rounds: "); Serial.println(NUM_ROUNDS - 1);
  Serial.print("Average: "); Serial.print(avg); Serial.println(" ms");
  Serial.print("Minimum: "); Serial.print(minT); Serial.println(" ms");
  Serial.print("Maximum: "); Serial.print(maxT); Serial.println(" ms");
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

  // Force pairing. encryption. MITM. SC before accessing protected attribute
  if (!pClient->secureConnection()) {
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] secureConnection() failed");
    pClient->disconnect();
    return false;
  }
  if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Secure connection established");

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

  // Write the message TEMP to the Server (UTF-8 string)
  std::string mensagem = "TEMP";
  if (ENABLE_INFORMATION_LOGS){
    Serial.print("[INFO] Sending: ");
    Serial.println(mensagem.c_str());
  }
  
  tStart = millis();
  //tStart = micros();

  pRemoteCharacteristic->writeValue((uint8_t*)mensagem.data(), mensagem.length(), true);
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Message Sent");
  //delay(500); //If the server response isnt being displayed in the terminal
               // its recommended to uncomment this line 
               // !!WARNING!! By doing this it will affect performance metrics by adding 50000 μs

  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    if(ENABLE_INFORMATION_LOGS) {
      Serial.print("[INFO] Received: ");
      Serial.println(value.c_str());
    }
  }

  tEnd = millis();
  //tEnd = micros();
  pClient->disconnect();
  roundTimes[currentRound] = tEnd - tStart;

  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Connection with Served ended");
  return true;
}

void setup() {
  Serial.begin(115200);
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initializing BLE Client...");

  BLEDevice::init("BLE_Client_Test");
  //BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setPassKey(true,STATIC_PASSKEY);
  pSecurity->setAuthenticationMode(false, true, true);
  pSecurity->setCapability(ESP_IO_CAP_IN);      // Client "inputs" passkey
  //pSecurity->setKeySize(16);
  //pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  //pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  
  //esp_ble_gap_register_callback(gapEventHandler);
  
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
    // Dont show first round
    if (connectAndExchange()) {
      if (currentRound > 0) {
        Serial.print("Round ");
        Serial.print(currentRound);
        Serial.print(" time: ");
        Serial.print(roundTimes[currentRound]);
        Serial.println(" ms");
      }
      currentRound++;
    }
  }
  //if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Sleeping 10 seconds before new scan");
  //delay(2000);
}