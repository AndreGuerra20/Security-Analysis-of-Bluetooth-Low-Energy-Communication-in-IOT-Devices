# ESP32 BLE Client-Server Code Overview

This document describes the purpose, architecture, and behavior of the Arduino sketches used in the BLE experimentation setup. The system is based on ESP32 devices and focuses on BLE communication, connection handling, performance measurement, and security configuration.

The implementation uses the ESP32 BLE Arduino stack (BLEDevice, BLEServer, BLEClient, BLEUtils, BLEScan).

---

## BLE Security Configuration

The current implementation operates under Bluetooth LE Security Mode 1, Level 4.

**LE Security Mode 1 - Level 4 (Authenticated LE Secure Connections with MITM + Encryption)**
- Link-layer encryption enabled
- Authenticated pairing (MITM protection)
- LE Secure Connections (ECDH-based pairing, SC)
- Bonding enabled (keys can be stored for future reconnects)
- Static passkey is used to enforce MITM protection

This means that:
- BLE communications are encrypted at the link layer after pairing completes successfully.
- The system enforces authenticated pairing using a 6-digit passkey (static passkey configured on both sides).
- Secure Connections (SC) is required, strengthening key agreement compared to legacy pairing.
- Devices that do not complete pairing/authentication should not be able to access data reliably.

**Implementation notes**
- The server configures security using `BLESecurity` with SC + MITM + bonding requirements.
- The client explicitly calls `secureConnection()` after connecting to force pairing/encryption before accessing attributes.
- The characteristic uses standard GATT access permissions (`ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE`) due to practical interoperability issues observed with the `_ENCRYPTED` permission flags in the ESP32 BLE stack, while still relying on the encrypted/authenticated link for Security Mode 1 Level 4.

---

## 1. `server_sketch.ino`  
### ESP32 BLE Server (BME280 Request/Response)

### Purpose
This sketch implements a Bluetooth Low Energy (BLE) server running on an ESP32. The server advertises a custom BLE service and exposes a GATT characteristic that supports client writes and server notifications. Upon receiving commands, the server reads sensor data from a BME280 and returns the result to the client.

### Main Features
- Initializes the ESP32 as a BLE server.
- Advertises a custom Service UUID:
  - `SERVICE_UUID = "dc1480ed-8d8f-4435-b083-c3c08f586a5e"`
- Exposes a Characteristic UUID:
  - `CHARACTERISTIC_UUID = "19e6651d-68e6-4090-85bb-6c5a3bdcd858"`
- Characteristic properties include:
  - READ, WRITE, NOTIFY
- Uses BME280 sensor over I2C (custom SDA/SCL pins configured).
- Enforces authenticated encrypted connection (Mode 1 Level 4) via passkey + SC + MITM + bonding.
- Automatically restarts advertising after client disconnection.

### Functional Behavior
1. The ESP32 initializes the BLE stack and configures security (passkey, SC, MITM, bonding).
2. A BLE server, service, and characteristic are created using predefined UUIDs.
3. Advertising starts and the server waits for client connections.
4. When a client writes data to the characteristic:
   - The server reads the received value and matches it against supported commands:
     - `"TEMP"`: reads temperature from the BME280, formats as a UTF-8 string with two decimals, and replies via `notify()`.
     - `"HUMD"`: reads humidity from the BME280, formats as a UTF-8 string with two decimals, and replies via `notify()`.
5. When the client disconnects:
   - Advertising is restarted to remain discoverable for subsequent rounds.

### Use Case
This server sketch acts as the sensor-backed target for BLE security and performance experiments, returning real data (temperature/humidity) to support functional validation and latency measurements under authenticated encrypted BLE connections.

---

## 2. `client_sketch.ino`  
### ESP32 BLE Client (Performance Test with Secure Connection)

### Purpose
This sketch implements a BLE client that scans for a BLE server advertising a specific Service UUID. For each iteration (round), the client connects to the server, establishes an authenticated encrypted link, sends a command, reads the server's response, measures the elapsed time for the request/response cycle, and disconnects.

### Main Features
- Performs active BLE scanning.
- Filters discovered devices by Service UUID:
  - `SERVICE_UUID = "dc1480ed-8d8f-4435-b083-c3c08f586a5e"`
- Connects to the target BLE server and forces pairing/encryption using `secureConnection()`.
- Locates the target characteristic:
  - `CHARACTERISTIC_UUID = "19e6651d-68e6-4090-85bb-6c5a3bdcd858"`
- Writes a command (`"TEMP"` in the current client logic).
- Reads the server response (expected to be a UTF-8 numeric string, e.g., `"19.64"`).
- Measures per-round time between:
  - start of the write (`tStart`) and completion of read (`tEnd`)
- Repeats the process for a predefined number of rounds and prints metrics (avg/min/max), ignoring the first round.

### Functional Behavior
1. A constant defines the total number of rounds (`NUM_ROUNDS`).
2. For each round:
   - The client scans for a fixed duration (`SCAN_TIME`).
   - When a device advertising the target `SERVICE_UUID` is found, the client attempts to connect.
3. After connecting:
   - The client calls `secureConnection()` to force pairing, encryption, MITM protection, and Secure Connections.
4. The client retrieves the remote service and characteristic.
5. The client performs the message exchange:
   - Writes `"TEMP"` to the characteristic (with response enabled).
   - Immediately reads the characteristic value to obtain the server's reply.
   - Stores the elapsed time `tEnd - tStart` for that round.
6. The client disconnects and starts the next round.
7. After all rounds:
   - Prints average, minimum, and maximum latency, ignoring the first round.

### Use Case
This client sketch is used to quantify the latency impact of authenticated encrypted BLE communication (Mode 1 Level 4) during a realistic request/response exchange with sensor-backed data.

---

## Overall Architecture Summary

- Server  
  Advertises a BLE service, enforces authenticated encrypted connections (Mode 1 Level 4), accepts command writes (`TEMP`, `HUMD`), reads BME280 sensor data, and replies via notifications.

- Client  
  Repeatedly scans for the server, connects, forces an authenticated encrypted link with `secureConnection()`, performs a command/response exchange, and measures the write→read latency per round.

- Security Level  
  BLE LE Security Mode 1, Level 4 is used across all interactions (authenticated pairing, encryption, SC, MITM).

Together, these sketches form a BLE client-server testbed suitable for functional validation and comparative security/performance evaluation using real ESP32 hardware.