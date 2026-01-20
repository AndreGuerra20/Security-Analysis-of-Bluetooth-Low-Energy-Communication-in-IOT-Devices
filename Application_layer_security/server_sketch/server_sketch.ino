/* ESP32 BLE Server (Secure BME280 Request/Response)
   Advertises SERVICE_UUID and exposes a GATT characteristic (CHARACTERISTIC_UUID)
   with READ, WRITE, WRITE_NR, and NOTIFY properties.

   The server implements an application-layer secure request/response model using
   AES-GCM. All incoming commands are expected to be encrypted and authenticated,
   ensuring confidentiality, integrity, and authenticity independently of BLE
   pairing or bonding mechanisms.

   When a client writes an encrypted command to the characteristic:
   - The server extracts the nonce, ciphertext, and authentication tag.
   - The command is decrypted and verified using AES-GCM.
   - Only authenticated commands are processed. Invalid messages are discarded.

   Supported commands include:
   - "TEMP": reads temperature from the BME280 sensor.
   - "HUMD": reads humidity from the BME280 sensor.

   For valid requests, the server builds a binary response containing:
   - A one-byte type identifier (TEMP or HUMD).
   - The sensor value encoded as a float.
   - A sequence number for basic replay tracking.

   The response is encrypted and authenticated with AES-GCM using a fresh nonce,
   written back to the characteristic, and transmitted to the client via NOTIFY.

   The server automatically restarts advertising after a client disconnects,
   allowing multiple sequential client connections without manual intervention.
   A periodic heartbeat message is printed every 5 seconds to indicate liveness
   and connection status.

   This server is intended to evaluate secure application-layer BLE communication
   with real sensor data on ESP32-based IoT devices.
*/

// Libraries necessary to AES encryption
#include "crypto_aes_gcm.h"

// AES Key: 4c7d9c0c22fa2cb06517f037b84996f2
static const uint8_t AES128_KEY[16] = {
    0x4c, 0x7d, 0x9c, 0x0c, 0x22, 0xfa, 0x2c, 0xb0,
    0x65, 0x17, 0xf0, 0x37, 0xb8, 0x49, 0x96, 0xf2
};

// Libraries necessary to BME280 Sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
// Constants and Global Variables for the sensor
#define I2C_SDA 21
#define I2C_SCL 22
Adafruit_BME280 bme;


#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4a3596a3-9dee-4040-b7b2-35e40716b283"
#define CHARACTERISTIC_UUID "93240635-635e-4c4e-9d8c-eb921ad1bcb3"

// Set this constant to true if you want [INFO] logs, otherwise set to false
#define ENABLE_INFORMATION_LOGS true
// Set this constant to true if you want [ERROR] logs, otherwise set to false
#define ENABLE_ERROR_LOGS true

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
    String cmd_rcv = pChar->getValue();
    size_t len = cmd_rcv.length();

    const size_t CMD_CT_LEN = 8; // Command Cypher Text length = 4 bytes "TEMP" + 4 bytes seq
    const size_t MIN_LEN = AES_GCM_IV_SIZE + CMD_CT_LEN + AES_GCM_TAG_SIZE; // Minimum Length

    if (len < MIN_LEN) {
      if (ENABLE_ERROR_LOGS)
        Serial.println("[ERROR] Encrypted payload received too small");
      return;
    }

    // Convert string into bytes (Get a pointer to the raw byte buffer)
    const uint8_t *buf = (const uint8_t *)cmd_rcv.c_str();

    // The first 12 bytes of the payload correspond to the AES-GCM nonce (IV).
    const uint8_t* nonce = buf;                              // 12 bytes

    // The ciphertext starts immediately after the nonce.
    const uint8_t* ct = buf + AES_GCM_IV_SIZE;            // 8 bytes

    // The authentication tag is located after the nonce and the ciphertext.
    const uint8_t* tag = buf + AES_GCM_IV_SIZE + CMD_CT_LEN; // 16 bytes

    uint8_t pt_cmd[CMD_CT_LEN]; // Plain text command received
    bool ok = aes_gcm_decrypt(AES128_KEY,nonce,ct,CMD_CT_LEN,tag,pt_cmd); // Decrypt cypher text storing plain text in pt_cmd

    if (!ok) {
      if (ENABLE_ERROR_LOGS)
        Serial.println("[ERROR] CMD decrypt failed (invalid tag)");
      return;
    }

    //Convert from bytes to usable String
    char cmd[5]; 
    cmd[0] = (char)pt_cmd[0]; 
    cmd[1] = (char)pt_cmd[1]; 
    cmd[2] = (char)pt_cmd[2]; 
    cmd[3] = (char)pt_cmd[3]; 
    cmd[4] = '\0';

    uint32_t seq;
    memcpy(&seq, &pt_cmd[4], 4);

    if (ENABLE_INFORMATION_LOGS) {
      Serial.print("[INFO] Decrypted CMD=");
      Serial.print(cmd);
      Serial.print(" seq=");
      Serial.println(seq);
    }

    if (strcmp(cmd, "TEMP") == 0) {
      float temperature = bme.readTemperature();
      static uint32_t seq = 0;

      uint8_t pt[1 + 4 + 4]; // plaintext (pt): [type(1)] [float(4)] [seq(4)]
      pt[0] = 0x01; // TEMP
      memcpy(&pt[1], &temperature, 4);
      memcpy(&pt[5], &seq, 4);

      // nonce 12 bytes (4 random + 8 bytes with seq and padding)
      uint8_t nonce[12];
      uint32_t r = esp_random();
      memcpy(&nonce[0], &r, 4); // 4 random bytes
      memcpy(&nonce[4], &seq, 4); // 4 sequence bytes
      uint32_t z = 0;
      memcpy(&nonce[8], &z, 4); // 4 padding bytes

      uint8_t ct[sizeof(pt)]; // Cypher text
      uint8_t tag[16];

      // Encrypt plain text (pt), storing the result in ct array
      if (!aes_gcm_encrypt(AES128_KEY, nonce, pt, sizeof(pt), ct, tag)) {
        if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] AES-GCM encrypt failed");
        return;
      }

      // payload = nonce(12) + ct(9) + tag(16)
      uint8_t out[12 + sizeof(ct) + 16];
      memcpy(out, nonce, 12);
      memcpy(out + 12, ct, sizeof(ct));
      memcpy(out + 12 + sizeof(ct), tag, 16);

      pChar->setValue(out, sizeof(out));
      pChar->notify();

      if (ENABLE_INFORMATION_LOGS) {
        Serial.print("[INFO] Sent encrypted TEMP. Bytes=");
        Serial.println(sizeof(out));
      }
      seq++;
    } else if (strcmp(cmd, "HUMD") == 0) {
      float humidity = bme.readHumidity();
      static uint32_t seq = 0;

      uint8_t pt[1 + 4 + 4]; // plaintext (pt): [type(1)] [float(4)] [seq(4)]
      pt[0] = 0x02; // HUMD
      memcpy(&pt[1], &humidity, 4);
      memcpy(&pt[5], &seq, 4);

      // nonce 12 bytes (4 random + 8 bytes with seq and padding)
      uint8_t nonce[12];
      uint32_t r = esp_random();
      memcpy(&nonce[0], &r, 4); // 4 random bytes
      memcpy(&nonce[4], &seq, 4); // 4 sequence bytes
      uint32_t z = 0;
      memcpy(&nonce[8], &z, 4); // 4 padding bytes

      uint8_t ct[sizeof(pt)]; // Cypher text
      uint8_t tag[16];

      // Encrypt plain text (pt), storing the result in ct vector
      if (!aes_gcm_encrypt(AES128_KEY, nonce, pt, sizeof(pt), ct, tag)) {
        if (ENABLE_ERROR_LOGS) Serial.println("[ERROR] AES-GCM encrypt failed");
        return;
      }

      // payload = nonce(12) + ct(9) + tag(16)
      uint8_t out[12 + sizeof(ct) + 16];
      memcpy(out, nonce, 12);
      memcpy(out + 12, ct, sizeof(ct));
      memcpy(out + 12 + sizeof(ct), tag, 16);

      pChar->setValue(out, sizeof(out));
      pChar->notify();

      if (ENABLE_INFORMATION_LOGS) {
        Serial.print("[INFO] Sent encrypted HUMD. Bytes=");
        Serial.println(sizeof(out));
      }
      seq++;
    }
  }
};

static BLEServer* initBLEDeviceAndServer(const char* deviceName) {
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initialising BLE Server...");
  BLEDevice::init(deviceName);
  BLEDevice::setMTU(37); // Increase Maximum Transmit Unit because of encryption

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  return server;
}


static void setupCharacteristic(BLEService *service, const char* charUuid) {
  // Define the Characteristics of GATT atributes
  pCharacteristic = service->createCharacteristic(
    charUuid,
    BLECharacteristic::PROPERTY_READ |
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

  // Initialize the I2C bus for temperature & humidity sensor
  Wire.begin(I2C_SDA, I2C_SCL); 
  bool status = bme.begin(0x76); // Try 0x77 if not working
  if (!status) {
    if (ENABLE_ERROR_LOGS) Serial.println("[Error] BME280 not found, restart the device or change bme.begin(address)");
    while (true);
  }
  if (ENABLE_INFORMATION_LOGS) Serial.println("[INFO] BME280 successfully initialized");

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