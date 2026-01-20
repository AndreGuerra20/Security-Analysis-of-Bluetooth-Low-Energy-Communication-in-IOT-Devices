/* ESP32 BLE Server (BME280 Request/Response)
   Advertises SERVICE_UUID and exposes a GATT characteristic (CHARACTERISTIC_UUID)
   with READ, WRITE, WRITE_NR, and NOTIFY properties.

   When a client writes a command to the characteristic:
   - "TEMP": reads temperature from the BME280 sensor, formats it as a UTF-8 string
             (e.g., "19.64"), stores it in the characteristic value, and sends it via NOTIFY.
   - "HUMD":  reads humidity from the BME280 sensor and replies in the same way.

   The server automatically restarts advertising after a client disconnects, allowing
   multiple sequential client connections. A periodic heartbeat log is printed every 5 seconds.
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
// 
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>
static const uint32_t STATIC_PASSKEY = 195374; // 6 digits

#define SERVICE_UUID        "dc1480ed-8d8f-4435-b083-c3c08f586a5e"
#define CHARACTERISTIC_UUID "19e6651d-68e6-4090-85bb-6c5a3bdcd858"

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

/* GAP callback */
void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr,true,STATIC_PASSKEY);
      break;

    case ESP_GAP_BLE_NC_REQ_EVT:
      esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr,true);
      break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (param->ble_security.auth_cmpl.success) {
        Serial.println("[SEC] Authentication success");
      } else {
        Serial.print("[SEC] Authentication failed. Reason: ");
        Serial.println(param->ble_security.auth_cmpl.fail_reason);
      }
      break;

    default:
      break;
  }
}

static BLEServer* initBLEDeviceAndServer(const char* deviceName) {
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Initialising BLE Server...");
  BLEDevice::init(deviceName);
  
  /*
  uint8_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t resp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  // IO capability
  uint8_t iocap = ESP_IO_CAP_OUT;
  
  
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,      &iocap,    sizeof(iocap));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,    &key_size, sizeof(key_size));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,    &init_key, sizeof(init_key));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,     &resp_key, sizeof(resp_key));
  */

  // Force Security Mode 1 Level 4 target. SC + MITM + (optional) bonding
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  pSecurity->setCapability(ESP_IO_CAP_OUT);     // Server "displays" passkey (Serial)
  pSecurity->setKeySize(16);                    // 128-bit key size
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  //pSecurity->setPassKey(true,STATIC_PASSKEY);
  //pSecurity->startSecurity()

  //esp_ble_gap_register_callback(gapEventHandler);

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
  pCharacteristic->setAccessPermissions(
    ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM
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