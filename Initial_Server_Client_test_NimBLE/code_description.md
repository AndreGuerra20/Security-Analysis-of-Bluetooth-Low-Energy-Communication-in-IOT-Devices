# ESP32 BLE Client–Server Code Overview

This document describes the purpose, architecture, and behavior of the three Arduino sketches used in the BLE experimentation setup. The system is based on ESP32 devices and focuses on BLE communication, connection handling, and performance measurement.

---

## 1. `server_sketch.ino`  
### ESP32 BLE Server

### Purpose
This sketch implements a Bluetooth Low Energy (BLE) **server** running on an ESP32. The server advertises a custom BLE service and exposes a writable characteristic that allows BLE clients to send messages. Upon receiving a predefined message, the server responds with a notification.

### Main Features
- Initializes the ESP32 as a BLE server.
- Advertises a custom **Service UUID**.
- Exposes a **Characteristic UUID** with write and notify permissions.
- Accepts incoming BLE client connections.
- Processes data written by the client.
- Sends a confirmation notification back to the client.
- Automatically restarts advertising after client disconnection.

### Functional Behavior
1. The ESP32 initializes the BLE stack and creates a BLE server.
2. A BLE service and characteristic are registered using predefined UUIDs.
3. The server starts advertising and waits for a client connection.
4. When a client writes data to the characteristic:
   - The server reads the received value.
   - If the message matches the expected payload (e.g., `"Hello World!"`), the server sends a notification acknowledging receipt.
5. When the client disconnects, advertising restarts automatically.

### Use Case
This server sketch is used as the communication endpoint for BLE clients and acts as the controlled target in functional and security experiments.

---

## 2. `client_sketch.ino`  
### ESP32 BLE Client (Basic Communication)

### Purpose
This sketch implements a BLE **client** that scans for nearby BLE devices advertising a specific service UUID. Once found, the client connects to the server and writes a predefined message to the server’s characteristic.

### Main Features
- Performs active BLE scanning.
- Filters discovered devices by Service UUID.
- Connects to the target BLE server.
- Writes data to the server’s characteristic.
- Disconnects after the operation.
- Repeats the scan–connect–write cycle.

### Functional Behavior
1. The ESP32 initializes as a BLE client.
2. A BLE scan is started for a fixed period.
3. Advertised devices are inspected for the target Service UUID.
4. When a matching server is found:
   - The client establishes a BLE connection.
   - Locates the desired characteristic.
   - Writes a predefined message (e.g., `"Hello World!"`).
5. The client disconnects and waits before starting a new scan.

### Use Case
This sketch provides a simple and controlled BLE client implementation used to validate connectivity, service discovery, and basic message exchange.

---

## 3. `client_performance_metrics_sketch.ino`  
### ESP32 BLE Client with Performance Metrics

### Purpose
This sketch extends the basic BLE client functionality by adding **performance measurement capabilities**. It executes multiple communication rounds and measures the total time required to complete a full BLE message exchange.

### Main Features
- Executes a fixed number of BLE connection rounds.
- Measures round-trip communication time per iteration.
- Stores timing results for each round.
- Computes average communication time at the end.
- Outputs performance metrics via the serial interface.

### Functional Behavior
1. A constant defines the total number of communication rounds.
2. For each round:
   - The client scans for the BLE server.
   - Establishes a connection.
   - Writes a message to the server.
   - Optionally waits for a notification or confirmation.
   - Records the elapsed time for the full exchange.
3. Timing values are accumulated across rounds.
4. After completing all rounds:
   - The average communication time is calculated.
   - Results are printed to the serial monitor.

### Use Case
This sketch is designed for experimental evaluation of BLE performance, enabling comparison of latency and consistency across multiple connection cycles. It is particularly useful for security and protocol analysis where timing behavior is relevant.

---

## Overall Architecture Summary

- **Server**: Advertises a BLE service and responds to client messages.
- **Client (Basic)**: Discovers the server and performs a single message exchange.
- **Client (Metrics)**: Repeats the exchange multiple times and gathers performance statistics.

Together, these sketches form a complete BLE client–server testbed suitable for functional validation, performance analysis, and security experimentation on real ESP32 hardware.
