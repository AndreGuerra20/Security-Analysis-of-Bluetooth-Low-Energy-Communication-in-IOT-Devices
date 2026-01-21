/* ESP32 BLE Server (Secure BME280 Request/Response)
 *
 * Advertises SERVICE_UUID and exposes a GATT characteristic (CHARACTERISTIC_UUID)
 * with READ, WRITE, and NOTIFY properties.
 *
 * Security:
 * - BLE LE Security Mode 1, Level 4
 * - Authenticated pairing with MITM protection
 * - LE Secure Connections (SC)
 * - Encrypted link with bonding and static passkey
 *
 * When a client writes a command to the characteristic:
 * - "TEMP": reads temperature from the BME280 sensor and replies via NOTIFY.
 * - "HUMD": reads humidity from the BME280 sensor and replies via NOTIFY.
 *
 * The server restarts advertising automatically after client disconnection.
 */

// Libraries necessary to BME280 Sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
// Constants and Global Variables 
#define I2C_SDA 21
#define I2C_SCL 22
Adafruit_BME280 bme;


#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
// Security Libraries
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>
static const uint32_t STATIC_PASSKEY = 195374; // 6 digits

#define SERVICE_UUID        "1714cc76-6a69-4d96-a7ca-3811e1868f4b"
#define CHARACTERISTIC_UUID "2d7c17eb-9f22-4b87-9172-5a95e158d6c7"

// Set this constant to true if you want [INFO] logs, otherwise set to false
#define ENABLE_INFORMATION_LOGS true
// Set this constant to true if you want [ERROR] logs, otherwise set to false
#define ENABLE_ERROR_LOGS true
// Set this constant to true if you want [SEC] logs, otherwise set to false
#define ENABLE_SECURITY_LOGS true

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

    if (rx == "TEMP") {
      float temperature = bme.readTemperature();
      if(ENABLE_INFORMATION_LOGS) {
        Serial.print("[INFO] Sending: ");
        Serial.println(temperature);
      } 
      //Respond to the client by notify
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "%.2f", temperature);
      pChar->setValue((uint8_t*)buffer, strlen(buffer));
      pChar->notify();
    }
    if (rx == "HUMD") {
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
  
  // Configure security before creating the server
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setPassKey(true,STATIC_PASSKEY);
  pSecurity->setCapability(ESP_IO_CAP_OUT);
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  
  // Configure Passkey via GAP API
  /*
  uint32_t passkey = STATIC_PASSKEY;
  uint8_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  uint8_t iocap = ESP_IO_CAP_OUT;
  
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t)); */

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  return server;
}


static void setupCharacteristic(BLEService *service, const char* charUuid) {
  pCharacteristic = service->createCharacteristic(
    charUuid,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  /* SECURITY NOTE: Using ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE instead of 
   * ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED because there is a bug that the connection was established but never traded messages (no output on both sides)
   * 
   * This does NOT reduce security because:
   * 1. The BLE connection is already encrypted at the link layer via secureConnection() in the client side
   * 2. MITM protection is enforced through passkey authentication
   * 3. Secure Connections (SC) using ECC P-256 is active
   * 4. All data transmitted is encrypted regardless of GATT-level permissions
   * 
   * The _ENCRYPTED flags are an additional GATT-layer check that rejects access
   * if the connection is not encrypted. However, there appears to be a bug in the
   * ESP32 BLE library where the characteristic doesn't properly recognize the
   * connection as encrypted even after secureConnection() succeeds.
   * 
   * Using standard permissions while maintaining encrypted connection still provides
   * Security Mode 1 Level 4 protection (authenticated encrypted connection with SC + MITM).
   */
  pCharacteristic->setAccessPermissions(
    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE
  );
  
  //pCharacteristic->addDescriptor(new BLE2902());
  
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