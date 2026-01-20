# ESP32 BLE Client–Server Code Overview

This document describes the final BLE client and server implementation used in the project **Secure Communications with BLE in IoT Devices**. The code targets the ESP32 platform using the Arduino framework and the classic ESP32 BLE stack, namely `BLEDevice`, `BLEUtils`, `BLEServer`, and `BLEClient`.

---

## client_sketch.ino

The BLE client acts as both a performance evaluation tool and a secure communication endpoint. Its main responsibilities are:

- Scanning for nearby BLE peripherals advertising the predefined `SERVICE_UUID`.
- Connecting to the first BLE server that matches the advertised service.
- Executing a fixed number of communication rounds, defined by a constant.
- In each round, sending an application-layer command, such as `TEMP`, encrypted with AES-GCM.
- Waiting for the encrypted response from the server and decrypting it.
- Measuring the elapsed time between sending the request and receiving the response.
- Storing timing values for each round and computing performance metrics, including average, minimum, and maximum latency, while ignoring the first round to reduce warm-up effects.

From a security perspective, the client never transmits commands in plaintext. All application data is protected using AES-GCM, ensuring confidentiality, integrity, and authenticity independently of BLE pairing or bonding mechanisms.

The client code is organized to clearly separate:
- BLE discovery, connection, and reconnection logic.
- Secure message construction and encryption.
- Performance measurement, metric aggregation, and reporting.

---

## server_sketch.ino

The BLE server runs on an ESP32 device equipped with a BME280 environmental sensor. Its responsibilities include:

- Advertising a custom BLE service identified by `SERVICE_UUID`.
- Exposing a characteristic identified by `CHARACTERISTIC_UUID` for bidirectional communication.
- Handling client connections and disconnections.
- Receiving encrypted data through characteristic write events.
- Decrypting and authenticating incoming messages using AES-GCM.
- Validating commands and processing only authenticated requests.
- Reading sensor data, such as temperature or humidity, from the BME280.
- Encrypting the response and sending it back to the client via the same characteristic.

If message authentication fails, the server discards the request and does not perform any action. This approach mitigates common BLE threats, including unauthorized command injection, replay attempts, and message tampering.

---

## crypto_aes_gcm.h / crypto_aes_gcm.cpp

These files implement the cryptographic support layer shared by both the client and the server. Their responsibilities include:

- AES-GCM encryption of application-layer payloads.
- AES-GCM decryption with authentication tag verification.
- Definition and handling of the message layout, including nonce (IV), ciphertext, and authentication tag.
- Encapsulation of cryptographic logic to avoid duplication and reduce implementation errors.

Keeping cryptographic functionality isolated in dedicated source files improves code readability, maintainability, and security auditing.

---

## Summary

Together, the client and server sketches form a complete prototype for secure BLE communication in IoT devices, featuring:

- Application-layer encryption and authentication using AES-GCM.
- Secure command and sensor data exchange over BLE.
- Quantitative performance evaluation of secure BLE message exchanges.
- A modular design that cleanly separates communication, cryptography, and application logic.

This implementation serves as the experimental foundation for analyzing the security, performance, and practical trade-offs of securing BLE communications in real-world IoT environments.
