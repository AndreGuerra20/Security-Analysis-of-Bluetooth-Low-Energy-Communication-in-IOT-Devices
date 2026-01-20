#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_BME280 bme;

float readTemperatureBME280() {
  return bme.readTemperature();
}

void setup() {
  Serial.begin(115200);

  // Initialize the I2C bus
  Wire.begin(I2C_SDA, I2C_SCL);

  bool status = bme.begin(0x76); // Try 0x77 if not working
  if (!status) {
    Serial.println("[Error] BME280 not found");
    while (true);
  }
  Serial.println("[INFO] BME280 successfully initialized");
}

void loop() {
  float temperature = readTemperatureBME280();

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  // Wait 2 seconds
  delay(2000);
}

