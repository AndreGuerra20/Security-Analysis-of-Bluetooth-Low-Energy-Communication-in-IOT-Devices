/* ESP32 BLE Server (BME280 Request/Response)
   Advertises SERVICE_UUID and exposes a GATT characteristic (CHARACTERISTIC_UUID)
   with READ, WRITE, WRITE_NR, and NOTIFY properties.

   When a client writes a command to the characteristic:
   - "TEMP": reads temperature from the BME280 sensor, formats it as a UTF-8 string
             (e.g., "19.64"), stores it in the characteristic value, and sends it via NOTIFY.
   - "HUM":  reads humidity from the BME280 sensor and replies in the same way.

   The server automatically restarts advertising after a client disconnects, allowing
   multiple sequential client connections. A periodic heartbeat log is printed every 5 seconds.
*/

// Libraries necessary to AES encryption
#include "mbedtls/gcm.h"
#include "esp_system.h"   // esp_random()

static const uint8_t AES128_KEY[16] = {
  0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81
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

static bool aes_gcm_encrypt(
  const uint8_t *key16,
  const uint8_t *nonce12,
  const uint8_t *pt, size_t pt_len,
  uint8_t *ct,
  uint8_t *tag16
) {
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key16, 128);
  if (ret != 0) { mbedtls_gcm_free(&ctx); return false; }

  // AAD opcional. Podem meter "BME" ou versão do protocolo.
  const uint8_t *aad = nullptr;
  size_t aad_len = 0;

  ret = mbedtls_gcm_crypt_and_tag(
    &ctx,
    MBEDTLS_GCM_ENCRYPT,
    pt_len,
    nonce12, 12,
    aad, aad_len,
    pt,
    ct,
    16, tag16
  );

  mbedtls_gcm_free(&ctx);
  return (ret == 0);
}

static bool aes_gcm_decrypt(
  const uint8_t *key16,
  const uint8_t *nonce12,
  const uint8_t *ct, size_t ct_len,
  const uint8_t *tag16,
  uint8_t *pt_out
) {
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key16, 128);
  if (ret != 0) { mbedtls_gcm_free(&ctx); return false; }

  const uint8_t *aad = nullptr;
  size_t aad_len = 0;

  ret = mbedtls_gcm_auth_decrypt(
    &ctx,
    ct_len,
    nonce12, 12,
    aad, aad_len,
    tag16, 16,
    ct,
    pt_out
  );

  mbedtls_gcm_free(&ctx);
  return (ret == 0);
}

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
    String v = pChar->getValue();
    size_t len = v.length();
    if (len < (12 + 8 + 16)) {
      if (ENABLE_ERROR_LOGS)
        Serial.println("[ERROR] RX too small for encrypted TEMP");
      return;
    }

    const uint8_t *buf = (const uint8_t *)v.c_str();

    const uint8_t *nonce = buf;        // 12 bytes
    const uint8_t *ct    = buf + 12;   // 8 bytes
    const uint8_t *tag   = buf + 12 + 8;

    uint8_t pt[8];
    bool ok = aes_gcm_decrypt(
      AES128_KEY,
      nonce,
      ct,
      8,
      tag,
      pt
    );

    if (!ok) {
      if (ENABLE_ERROR_LOGS)
        Serial.println("[ERROR] CMD decrypt failed (invalid tag)");
      return;
    }

    char cmd[5];
    cmd[0] = (char)pt[0];
    cmd[1] = (char)pt[1];
    cmd[2] = (char)pt[2];
    cmd[3] = (char)pt[3];
    cmd[4] = '\0';

    uint32_t seq;
    memcpy(&seq, &pt[4], 4);

    if (ENABLE_INFORMATION_LOGS) {
      Serial.print("[INFO] Decrypted CMD=");
      Serial.print(cmd);
      Serial.print(" seq=");
      Serial.println(seq);
    }

    if (strcmp(cmd, "TEMP") == 0) {
      float temperature = bme.readTemperature();

      // plaintext: [type(1)] [float(4)] [seq(4)]
      static uint32_t seq = 0;
      uint8_t pt[1 + 4 + 4];
      pt[0] = 0x01; // TEMP
      memcpy(&pt[1], &temperature, 4);
      memcpy(&pt[5], &seq, 4);

      // nonce 12 bytes (4 random + 8 bytes com seq e padding)
      uint8_t nonce[12];
      uint32_t r = esp_random();
      memcpy(&nonce[0], &r, 4);
      memcpy(&nonce[4], &seq, 4);
      uint32_t z = 0;
      memcpy(&nonce[8], &z, 4);

      uint8_t ct[sizeof(pt)];
      uint8_t tag[16];

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
    }
    if (strcmp(cmd, "HUM") == 0) {
      float humidity = bme.readHumidity();
      if(ENABLE_INFORMATION_LOGS) {
        Serial.print("[INFO] Sending: ");
        Serial.println(humidity);
      } 
      //Respond to the client by notify
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "%.2f", humidity);
      pChar->setValue((uint8_t*)buffer, strlen(buffer));
      pChar->notify();
    }
  }
};

static BLEServer* initBLEDeviceAndServer(const char* deviceName) {
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initialising BLE Server...");
  BLEDevice::init(deviceName);
  BLEDevice::setMTU(100); // Increase Maximum Transmit Unit because of encryption

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
    if (ENABLE_ERROR_LOGS) Serial.println("[Error] BME280 not found");
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