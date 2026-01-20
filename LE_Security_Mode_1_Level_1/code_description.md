# ESP32 BLE Client–Server Code Overview

This document describes the purpose, architecture, and behavior of the Arduino sketches used in the BLE experimentation setup. The system is based on ESP32 devices and focuses on BLE communication, connection handling, performance measurement, and security configuration.

The implementation uses the ESP32 BLE Arduino stack (BLEDevice, BLEServer, BLEClient, BLEUtils, BLEScan).

---

## BLE Security Configuration

The current implementation operates under Bluetooth LE Security Mode 1, Level 1.

**LE Security Mode 1 – Level 1**
- No security
- No authentication
- No encryption

This means that:
- All BLE communications are transmitted in plaintext.
- No pairing or bonding procedure is enforced.
- No encryption is applied at the link layer.
- Any nearby BLE device that knows the Service and Characteristic UUIDs can potentially observe or interact with the communication.

This security level was intentionally selected to serve as a baseline scenario for subsequent security analysis and comparison with higher BLE security modes.

---

## 1. `server_sketch.ino`  
### ESP32 BLE Server

### Purpose
This sketch implements a Bluetooth Low Energy (BLE) server running on an ESP32. The server advertises a custom BLE service and exposes a writable characteristic that allows BLE clients to send messages.

### Main Features
- Initializes the ESP32 as a BLE server.
- Advertises a custom Service UUID.
- Exposes a Characteristic UUID with write and notify properties.
- Accepts incoming BLE client connections.
- Logs client connection and disconnection events.
- Processes data written by the client.
- Automatically restarts advertising after client disconnection.

### Functional Behavior
1. The ESP32 initializes the BLE stack and creates a BLE server.
2. A BLE service and characteristic are registered using predefined UUIDs.
3. The characteristic is configured without authentication or encryption requirements.
4. The server starts advertising and waits for incoming client connections.
5. When a client writes data to the characteristic:
   - The server reads and logs the received value via the serial interface.
6. When the client disconnects:
   - The server restarts advertising to remain discoverable.

### Use Case
This server sketch acts as the controlled target device for BLE functional, performance, and security experiments, especially for analyzing unencrypted BLE communication.

---

## 2. `client_sketch.ino`  
### ESP32 BLE Client with Iterative Communication

### Purpose
This sketch implements a BLE client that repeatedly scans for a BLE server advertising a specific Service UUID. For each iteration, the client connects to the server, performs a full message exchange, measures the elapsed time, and disconnects.

### Main Features
- Performs active BLE scanning.
- Filters discovered devices by Service UUID.
- Connects to the target BLE server.
- Writes data to the server’s characteristic.
- Measures the total time of each communication round.
- Repeats the process for a predefined number of rounds.
- Computes and outputs average communication latency.

### Functional Behavior
1. A constant defines the total number of communication rounds.
2. For each round:
   - The client starts a BLE scan for a fixed duration.
   - Advertised devices are inspected for the target Service UUID.
3. When a matching server is found:
   - A BLE connection is established.
   - The target characteristic is located.
   - A predefined message is written to the characteristic.
4. The client records the elapsed time for the full scan, connect, and write cycle.
5. The client disconnects and waits briefly before the next iteration.
6. After completing all rounds:
   - The average communication time is calculated.
   - Performance metrics are printed to the serial monitor.

### Use Case
This sketch is used to evaluate BLE connection latency and communication overhead under no-security conditions, providing baseline performance metrics for comparison with more secure BLE configurations.

---

## Overall Architecture Summary

- Server  
  Advertises a BLE service and accepts unencrypted write operations from clients.

- Client  
  Repeatedly discovers the server, connects, exchanges data, and measures timing metrics.

- Security Level  
  BLE LE Security Mode 1, Level 1 is used across all interactions, enabling controlled analysis of plaintext BLE communication.

Together, these sketches form a complete BLE client–server testbed suitable for functional validation, latency measurement, and foundational security analysis on real ESP32 hardware.
