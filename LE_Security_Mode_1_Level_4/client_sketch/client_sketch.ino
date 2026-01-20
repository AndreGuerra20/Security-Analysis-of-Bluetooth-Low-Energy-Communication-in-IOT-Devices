/* ESP32 BLE Client (Performance Test)
   Scans for BLE peripherals advertising SERVICE_UUID and connects to the first match.
   For each round, it writes the command "TEMP" or "HUMD" to CHARACTERISTIC_UUID, then reads back
   the server response (expected to be a UTF-8 string, e.g., a temperature value).
   The elapsed time between write and read is measured in microseconds and stored.
   After NUM_ROUNDS rounds, it prints average, min, and max metrics, ignoring the first
   round to reduce warm-up effects.
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

#define SERVICE_UUID        "dc1480ed-8d8f-4435-b083-c3c08f586a5e"
#define CHARACTERISTIC_UUID "19e6651d-68e6-4090-85bb-6c5a3bdcd858"

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

class MySecurityCallbacks : public BLESecurityCallbacks {

  uint32_t onPassKeyRequest() override {
    Serial.println("[SEC] Passkey requested");
    return STATIC_PASSKEY;   // PIN estático
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    Serial.print("[SEC] Passkey notify: ");
    Serial.println(pass_key);
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    Serial.print("[SEC] Confirm PIN: ");
    Serial.println(pass_key);
    return true;   // aceita o PIN
  }

  bool onSecurityRequest() override {
    Serial.println("[SEC] Security request");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    if (cmpl.success) {
      Serial.println("[SEC] Authentication success");
    } else {
      Serial.print("[SEC] Authentication failed, reason: ");
      Serial.println(cmpl.fail_reason);
    }
  }
};

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
  std::string mensagem = "HUMD";
  if (ENABLE_INFORMATION_LOGS){
    Serial.print("[INFO] Sending: ");
    Serial.println(mensagem.c_str());
  }
  
  //tStart = millis();
  tStart = micros();

  pRemoteCharacteristic->writeValue((uint8_t*)mensagem.data(), mensagem.length(), false);
  if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Message Sent");
  //delay(500); //If the server response isnt being displayed in the terminal
               // its recommended to uncomment this line 
               // !!WARNING!! By doing this it will affect performance metrics by adding 50000 μs

  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    if(ENABLE_INFORMATION_LOGS) Serial.print("[INFO] Received: ");
    Serial.println(value.c_str());
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
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

  /*
  uint8_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t resp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  // IO capability
  uint8_t iocap = ESP_IO_CAP_IN; // client "enters" passkey

  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,      &iocap,    sizeof(iocap));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,    &key_size, sizeof(key_size));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,    &init_key, sizeof(init_key));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,     &resp_key, sizeof(resp_key));
  */

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  pSecurity->setCapability(ESP_IO_CAP_IN);      // Client "inputs" passkey
  pSecurity->setKeySize(16);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  pSecurity->setPassKey(true,STATIC_PASSKEY);

  
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
        Serial.println(" μs");
      }
      currentRound++;
    }
  }
  //if(ENABLE_INFORMATION_LOGS) Serial.println("[INFO] Sleeping 10 seconds before new scan");
  //delay(2000);
}