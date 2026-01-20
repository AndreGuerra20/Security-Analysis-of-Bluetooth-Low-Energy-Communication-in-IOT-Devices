# ESP32 BLE Client–Server Code Overview (Application-Layer Security)

This document describes the purpose, architecture, and behavior of the Arduino sketches and cryptographic helper files used in the BLE experimentation setup. The system is based on ESP32 devices and focuses on BLE communication, secure request/response exchange, performance measurement, and application-layer encryption.

The implementation uses the ESP32 BLE Arduino stack (BLEDevice, BLEServer, BLEClient, BLEUtils, BLEScan) and mbedTLS for AES-GCM.

---

## BLE Security Configuration

At the BLE link layer, the system operates under Bluetooth LE Security Mode 1, Level 1.

**LE Security Mode 1 – Level 1**
- No BLE link-layer security
- No authentication
- No encryption

This means that:
- BLE packets are transmitted without link-layer encryption.
- No pairing or bonding procedure is enforced.
- Any nearby device that knows the Service and Characteristic UUIDs can attempt to interact with the GATT server.

To secure the exchanged data despite this baseline BLE configuration, the implementation applies **application-layer authenticated encryption** using **AES-128-GCM**:
- Confidentiality: payload contents are encrypted.
- Integrity and authenticity: payloads are authenticated using the GCM tag.
- Basic replay resistance: sequence numbers are included inside the plaintext.

This design enables controlled comparison between plaintext BLE traffic and secure application-layer traffic, without depending on BLE pairing modes.

---

## 1. `server_sketch.ino`  
### ESP32 BLE Server (Secure BME280 Request/Response)

### Purpose
This sketch implements a BLE server on an ESP32 that advertises a custom service and characteristic, receives encrypted commands from a client, decrypts and validates them, reads data from a BME280 sensor, then encrypts and returns the response to the client.

### Main Features
- Initializes the ESP32 as a BLE server and starts advertising a custom Service UUID.
- Exposes a GATT characteristic with READ, WRITE, WRITE_NR, and NOTIFY properties.
- Receives encrypted requests via characteristic writes.
- Decrypts and authenticates received payloads with AES-GCM.
- Reads real sensor values from a BME280 (temperature and humidity).
- Encrypts and notifies the client with the corresponding encrypted response.
- Restarts advertising after client disconnection and prints a periodic heartbeat.

### Functional Behavior
1. Initializes Serial, I2C, and the BME280 sensor.
2. Initializes BLE stack, server, service, and characteristic.
3. Starts advertising and waits for client connections.
4. On characteristic write:
   - Validates minimum payload length.
   - Parses the incoming buffer as:
     - nonce (12 bytes) + ciphertext (8 bytes) + tag (16 bytes)
   - Decrypts and verifies AES-GCM tag.
   - Extracts plaintext command:
     - 4-byte ASCII command ("TEMP" or "HUMD") + 4-byte sequence number.
   - If valid:
     - Reads temperature or humidity.
     - Builds a binary plaintext response:
       - type (1 byte: 0x01 TEMP, 0x02 HUMD) + value (float, 4 bytes) + seq (uint32, 4 bytes)
     - Encrypts with AES-GCM using a fresh nonce.
     - Writes encrypted payload to the characteristic and sends it via NOTIFY.
5. Prints a heartbeat message every 5 seconds indicating connection state.

### Use Case
This server sketch is the controlled secure endpoint used to evaluate confidentiality/integrity at the application layer and the impact of encryption on latency when exchanging real sensor data.

---

## 2. `client_sketch.ino`  
### ESP32 BLE Client (Secure Performance Test)

### Purpose
This sketch implements a BLE client that repeatedly scans for a server advertising a target Service UUID, connects, sends an encrypted command requesting temperature or humidity, reads the encrypted response, decrypts and validates it, and measures round-trip latency.

### Main Features
- Active BLE scanning with filtering by Service UUID.
- Connects to the first matching BLE server found.
- Encrypts requests using AES-128-GCM at the application layer.
- Reads encrypted responses, verifies the authentication tag, and decrypts payloads.
- Runs multiple rounds and reports performance metrics (average, min, max), ignoring the first round.

### Functional Behavior
1. Repeats for a fixed number of rounds (NUM_ROUNDS):
   - Starts BLE scan for a fixed duration (SCAN_TIME).
   - Stops scan as soon as a device advertising the target service is found.
2. Connects to the server, locates the service and characteristic.
3. Builds a plaintext command (8 bytes):
   - "TEMP" or "HUMD" (4 bytes) + sequence number (4 bytes).
4. Generates a fresh 12-byte nonce and encrypts the command using AES-GCM.
5. Sends the encrypted request as:
   - nonce (12 bytes) + ciphertext (8 bytes) + tag (16 bytes)
6. Reads the encrypted response from the characteristic and parses it as:
   - nonce (12 bytes) + ciphertext (9 bytes) + tag (16 bytes)
7. Decrypts and validates the response:
   - type (1 byte) + float value (4 bytes) + seq (4 bytes)
8. Measures elapsed time in microseconds between sending the request and reading/decrypting the response.
9. After all rounds, prints average, minimum, and maximum latency metrics (first round excluded from printing to reduce warm-up effects).

### Use Case
This sketch provides repeatable latency measurements for secure request/response exchanges, enabling comparison against plaintext BLE baselines and evaluation of overhead introduced by application-layer AES-GCM.

---

## 3. `crypto_aes_gcm.cpp`  
### AES-128-GCM Implementation (mbedTLS)

### Purpose
This file provides the implementation of AES-GCM authenticated encryption and decryption using the ESP32 mbedTLS library.

### Main Features
- Implements `aes_gcm_encrypt()`:
  - Initializes an mbedTLS GCM context.
  - Configures AES-128 key.
  - Encrypts plaintext and generates a 16-byte authentication tag.
- Implements `aes_gcm_decrypt()`:
  - Initializes an mbedTLS GCM context.
  - Configures AES-128 key.
  - Authenticates and decrypts ciphertext.
  - Returns failure if the authentication tag is invalid.

### Notes
- No Additional Authenticated Data (AAD) is used (AAD length is 0).
- The IV/nonce size is fixed to 12 bytes (recommended size for GCM).

---

## 4. `crypto_aes_gcm.h`  
### AES-128-GCM Interface and Constants

### Purpose
This header defines AES-GCM constants and exposes a minimal cryptographic interface shared by both client and server sketches.

### Main Features
- Defines typical AES-GCM sizes:
  - Key size: 16 bytes (128-bit)
  - IV/nonce size: 12 bytes
  - Tag size: 16 bytes
- Declares:
  - `aes_gcm_encrypt(...)`
  - `aes_gcm_decrypt(...)`

---

## Overall Architecture Summary

- Server  
  Advertises a BLE service, accepts encrypted write requests, decrypts and validates commands, reads BME280 values, encrypts responses, and notifies the client.

- Client  
  Discovers the server, connects, sends encrypted commands (TEMP/HUMD), reads encrypted responses, decrypts them, and measures round-trip latency over multiple rounds.

- Security Model  
  BLE link-layer security remains disabled (Mode 1 Level 1) to serve as a baseline, while confidentiality and integrity are enforced at the application layer using AES-128-GCM.