/* ESP32 BLE Client (Performance Test)
   Scans for BLE peripherals advertising SERVICE_UUID and connects to the first match.
   For each round, it writes the command "TEMP" to CHARACTERISTIC_UUID, then reads back
   the server response (expected to be a UTF-8 string, e.g., a temperature value).
   The elapsed time between write and read is measured in microseconds and stored.
   After NUM_ROUNDS rounds, it prints average, min, and max metrics, ignoring the first
   round to reduce warm-up effects.
*/

// Libraries necessary to AES encryption/decryption
#include "crypto_aes_gcm.h"

static const uint8_t AES128_KEY[16] = {
  0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81
};

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

#define SERVICE_UUID        "4a3596a3-9dee-4040-b7b2-35e40716b283"
#define CHARACTERISTIC_UUID "93240635-635e-4c4e-9d8c-eb921ad1bcb3"

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

static bool sendEncryptedTemp(BLERemoteCharacteristic *ch) {
  static uint32_t seq = 0;

  // plaintext: "TEMP" + seq
  uint8_t pt[8];
  pt[0] = 'T'; pt[1] = 'E'; pt[2] = 'M'; pt[3] = 'P';
  memcpy(&pt[4], &seq, 4);

  // nonce 12 bytes aleatorio
  uint8_t nonce[12];
  uint32_t r1 = (uint32_t)esp_random();
  uint32_t r2 = (uint32_t)esp_random();
  uint32_t r3 = (uint32_t)esp_random();
  memcpy(&nonce[0], &r1, 4);
  memcpy(&nonce[4], &r2, 4);
  memcpy(&nonce[8], &r3, 4);

  uint8_t ct[sizeof(pt)];
  uint8_t tag[16];

  if (!aes_gcm_encrypt(AES128_KEY, nonce, pt, sizeof(pt), ct, tag)) {
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] TEMP encrypt failed");
    return false;
  }

  // payload: nonce + ct + tag
  uint8_t out[12 + sizeof(ct) + 16]; // 36 bytes
  memcpy(out, nonce, 12);
  memcpy(out + 12, ct, sizeof(ct));
  memcpy(out + 12 + sizeof(ct), tag, 16);

  bool ok = ch->writeValue(out, sizeof(out), true);
  if (!ok && ENABLE_ERROR_LOGS) Serial.println("[ERROR] writeValue failed");

  seq++;
  return ok;
}

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
  Serial.print("Average: "); Serial.print(avg); Serial.println(" μs");
  Serial.print("Minimum: "); Serial.print(minT); Serial.println(" μs");
  Serial.print("Maximum: "); Serial.print(maxT); Serial.println(" μs");
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

  // Write the message TEMP to the Server (UTF-8 string)
  std::string mensagem = "TEMP";
  if (ENABLE_INFORMATION_LOGS){
    Serial.print("[INFO] Sending: ");
    Serial.println(mensagem.c_str());
  }
  
  //tStart = millis();
  tStart = micros();

  if (!sendEncryptedTemp(pRemoteCharacteristic)) {
    return false;
  }
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Message Sent");
  //delay(50); //If the server response isnt being displayed in the terminal
               // its recommended to uncomment this line 
               // !!WARNING!! By doing this it will affect performance metrics by adding 50000 μs

  if (pRemoteCharacteristic->canRead()) {
    String rawStr = pRemoteCharacteristic->readValue();

    size_t len = rawStr.length();
    if (len < (12 + 9 + 16)) {
      if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] Encrypted payload too small");
      return false;
    }

    const uint8_t *buf = (const uint8_t *)rawStr.c_str();

    const uint8_t *nonce = buf;          // 12 bytes
    const uint8_t *ct    = buf + 12;     // 9 bytes
    const uint8_t *tag   = buf + 12 + 9; // 16 bytes

    uint8_t pt[9];

    bool ok = aes_gcm_decrypt(AES128_KEY,nonce,ct,9,tag,pt);

    if (!ok) {
      if (ENABLE_ERROR_LOGS)
        Serial.println("[ERROR] AES-GCM decrypt failed (invalid tag)");
      return false;
    }

    uint8_t type = pt[0];
    float value;
    uint32_t seq;

    memcpy(&value, &pt[1], 4);
    memcpy(&seq,   &pt[5], 4);

    if (ENABLE_INFORMATION_LOGS) {
      Serial.print("[INFO] Decrypted seq=");
      Serial.print(seq);
      Serial.print(" type=");
      Serial.print(type == 0x01 ? "TEMP" : (type == 0x02 ? "HUM" : "UNK"));
      Serial.print(" value=");
      Serial.println(value, 2);
    }
  }

  //tEnd = millis();
  tEnd = micros();
  pClient->disconnect();
  roundTimes[currentRound] = tEnd - tStart;

  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Connection with Served ended");
  return true;
}

void setup() {
  Serial.begin(115200);
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initializing BLE Client...");

  BLEDevice::init("BLE_Client_Test");
  BLEDevice::setMTU(100);
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
        Serial.println(" μs");
      }
      currentRound++;
    }
  }
  //if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Sleeping 10 seconds before new scan");
  //delay(2000);
}