/* ESP32 BLE Client (Secure Performance Test)
   Scans for BLE peripherals advertising SERVICE_UUID and connects to the first match found.
   For each communication round, the client builds an application-layer command requesting
   temperature or humidity data. The request type is defined by REQUEST_OPTION or randomly
   selected when configured.

   All commands are encrypted and authenticated using AES-GCM at the application layer.
   Each message includes a random nonce and a sequence number to ensure confidentiality,
   integrity, and replay protection, independently of BLE pairing or bonding mechanisms.

   After sending the encrypted request to CHARACTERISTIC_UUID, the client reads the encrypted
   response from the server, validates the authentication tag, and decrypts the payload.
   The response contains the sensor type, the measured value, and the corresponding sequence
   number.

   The elapsed time between sending the encrypted request and receiving the valid decrypted
   response is measured in microseconds and stored for each round.
   After NUM_ROUNDS executions, the client computes and prints average, minimum, and maximum
   latency metrics, ignoring the first round to reduce warm-up effects.

   This client is intended for evaluating both the security and performance impact of
   application-layer encryption in BLE-based IoT communications on real ESP32 devices.
*/


// Libraries necessary to AES encryption/decryption
#include "crypto_aes_gcm.h"

// AES Key: 4c7d9c0c22fa2cb06517f037b84996f2
static const uint8_t AES128_KEY[16] = {
    0x4c, 0x7d, 0x9c, 0x0c, 0x22, 0xfa, 0x2c, 0xb0,
    0x65, 0x17, 0xf0, 0x37, 0xb8, 0x49, 0x96, 0xf2
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
// Set this constant according wich option you want: 1 Request to the Server only temperature readings,
// 2 Request to the Server only humidity readings, 3 Request to the Server randomly deciding between temperature or humidity readings for each round
#define REQUEST_OPTION 3

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

static bool sendEncryptedCMD(BLERemoteCharacteristic *ch) {
  static uint32_t seq = 0;
  uint8_t pt[8];
  if (REQUEST_OPTION == 1){
    // plaintext: "TEMP" + seq
    pt[0] = 'T'; pt[1] = 'E'; pt[2] = 'M'; pt[3] = 'P';
    memcpy(&pt[4], &seq, 4);

    // Write the message TEMP to the Server (UTF-8 string)
    std::string mensagem = "TEMP";
    if (ENABLE_INFORMATION_LOGS){
      Serial.print("[INFO] Sending: ");
      Serial.println(mensagem.c_str());
    }
  } else if (REQUEST_OPTION == 2) {
    // plaintext: "HUMD" + seq
    pt[0] = 'H'; pt[1] = 'U'; pt[2] = 'M'; pt[3] = 'D';
    memcpy(&pt[4], &seq, 4);

    // Write the message HUMD to the Server (UTF-8 string)
    std::string mensagem = "HUMD";
    if (ENABLE_INFORMATION_LOGS){
      Serial.print("[INFO] Sending: ");
      Serial.println(mensagem.c_str());
    }
  } else if (REQUEST_OPTION == 3) {
    if (esp_random() % 2){
      // plaintext: "TEMP" + seq
      pt[0] = 'T'; pt[1] = 'E'; pt[2] = 'M'; pt[3] = 'P';
      memcpy(&pt[4], &seq, 4);

      // Write the message TEMP to the Server (UTF-8 string)
      std::string mensagem = "TEMP";
      if (ENABLE_INFORMATION_LOGS){
        Serial.print("[INFO] Sending: ");
        Serial.println(mensagem.c_str());
      }
    } else {
      // plaintext: "HUMD" + seq
      pt[0] = 'H'; pt[1] = 'U'; pt[2] = 'M'; pt[3] = 'D';
      memcpy(&pt[4], &seq, 4);

      // Write the message HUMD to the Server (UTF-8 string)
      std::string mensagem = "HUMD";
      if (ENABLE_INFORMATION_LOGS){
        Serial.print("[INFO] Sending: ");
        Serial.println(mensagem.c_str());
      }
    }
  }

  // nonce - 12 random bytes -> 3 random unsigned integers
  uint8_t nonce[12];
  uint32_t r1 = (uint32_t)esp_random();
  uint32_t r2 = (uint32_t)esp_random();
  uint32_t r3 = (uint32_t)esp_random();
  memcpy(&nonce[0], &r1, 4);
  memcpy(&nonce[4], &r2, 4);
  memcpy(&nonce[8], &r3, 4);

  uint8_t ct[sizeof(pt)]; // cypher text
  uint8_t tag[16];

  // Encrypt plain text (pt), storing the result in ct array
  if (!aes_gcm_encrypt(AES128_KEY, nonce, pt, sizeof(pt), ct, tag)) {
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] TEMP encrypt failed");
    return false;
  }

  // payload = nonce(12) + ct(8) + tag(16)
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
  
  //tStart = millis();
  tStart = micros();

  if (!sendEncryptedCMD(pRemoteCharacteristic)) {
    return false;
  }
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Message Sent");
  //delay(50); //If the server response isnt being displayed in the terminal
               // its recommended to uncomment this line 
               // !!WARNING!! By doing this it will affect performance metrics by adding 50000 μs

  if (pRemoteCharacteristic->canRead()) {
    String rawStr = pRemoteCharacteristic->readValue();
    size_t len = rawStr.length();
    const size_t PAYLOAD_CT_LEN = 9; // Payload in Cypher Text length = type(1) + float(4) + seq(4)
    const size_t MIN_LEN = AES_GCM_IV_SIZE + PAYLOAD_CT_LEN + AES_GCM_TAG_SIZE; // Minimum Length (37)
    if (len < MIN_LEN) {
      if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] Encrypted payload received too small");
      return false;
    }

    // Convert string into bytes (Get a pointer to the raw byte buffer)
    const uint8_t *buf = (const uint8_t *)rawStr.c_str();

    // The first 12 bytes of the payload correspond to the AES-GCM nonce (IV).
    const uint8_t *nonce = buf;          // 12 bytes

    // The ciphertext starts immediately after the nonce.
    const uint8_t *ct  = buf + 12;     // 9 bytes
    
    // The authentication tag is located after the nonce and the ciphertext.
    const uint8_t *tag = buf + 12 + 9; // 16 bytes

    uint8_t pt[PAYLOAD_CT_LEN]; // Plain text message received

    bool ok = aes_gcm_decrypt(AES128_KEY,nonce,ct,9,tag,pt); // Decrypt cypher text storing plain text in pt

    if (!ok) {
      if (ENABLE_ERROR_LOGS)
        Serial.println("[ERROR] AES-GCM decrypt failed (invalid tag)");
      return false;
    }

    // Convert from raw bytes to type and float 
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
  if (!(REQUEST_OPTION == 1 || REQUEST_OPTION == 2 || REQUEST_OPTION == 3)){
    if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] REQUEST_OPTION must be 1,2 or 3");
    while (true); // Block execution to this line
  }
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