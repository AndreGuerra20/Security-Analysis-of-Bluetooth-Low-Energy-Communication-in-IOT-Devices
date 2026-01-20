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

#define SERVICE_UUID        "dc1480ed-8d8f-4435-b083-c3c08f586a5e"
#define CHARACTERISTIC_UUID "19e6651d-68e6-4090-85bb-6c5a3bdcd858"

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
    if (rx == "HUM") {
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