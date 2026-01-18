#include <NimBLEDevice.h>

static bool scanning = false;

class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    Serial.print("[SCAN] ");
    Serial.print(d->getAddress().toString().c_str());
    Serial.print(" rssi=");
    Serial.print(d->getRSSI());
    Serial.print(" name=");
    Serial.println(d->getName().c_str());
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    scanning = false;
    Serial.print("[SCAN] End. Count=");
    Serial.println(results.getCount());
  }
};

static ScanCB cb;

void setup() {
  Serial.begin(115200);
  delay(200);

  NimBLEDevice::init("scanner");

  NimBLEScan* s = NimBLEDevice::getScan();
  s->setScanCallbacks(&cb, false);
  s->setActiveScan(true);
  s->setInterval(160);
  s->setWindow(80);
  s->setDuplicateFilter(false);

  Serial.println("[SCAN] Starting 8s scan...");
  scanning = true;
  s->start(8, false);
}

void loop() {
  if (!scanning) {
    delay(1500);
    Serial.println("[SCAN] Starting 8s scan...");
    scanning = true;
    NimBLEDevice::getScan()->start(8, false);
  }
  delay(50);
}
