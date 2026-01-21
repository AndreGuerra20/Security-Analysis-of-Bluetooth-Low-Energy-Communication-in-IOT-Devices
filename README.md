# Security Analysis of Bluetooth Low Energy Communication in IoT Devices

This repository contains the source code, experimental scenarios, and supporting material developed for the project **“Security Analysis of Bluetooth Low Energy Communication in IoT Devices”**. The main goal of this work is to analyse, compare, and experimentally evaluate different Bluetooth Low Energy (BLE) security configurations in real IoT devices, focusing on security properties, performance overhead, and practical implementation aspects.

The project is implemented using **ESP32-based IoT devices**, BLE client–server communication models, and both native BLE security mechanisms and application-layer cryptographic protections.

---

## Project Objectives

The main objectives of this repository are:

- Study BLE communication security mechanisms and their practical implications
- Compare unsecured BLE communication with secured BLE modes
- Evaluate application-layer encryption as an alternative or complement to BLE-native security
- Measure performance metrics such as latency, overhead, and resource usage
- Analyse advantages, limitations, and attack surfaces of each approach

---

## Repository Structure

The repository is organised into multiple directories, each corresponding to a specific security scenario or experimental setup. In each directory you will find code_description.md files that better describe the scenario implemented.

### `LE_Security_Mode_1_Level_1/` - Scenario A
Baseline scenario with **no BLE security** enabled.

Key points:
- No encryption, authentication, or pairing
- Plaintext data exchange between client and server
- Used as reference for performance and security comparison

### `Application_layer_security/` - Scenario B
BLE communication where security is implemented at the **application layer** (end-to-end), instead of relying on BLE pairing/bonding.

Key points:
- Symmetric encryption using **AES-GCM**
- Provides confidentiality, integrity, and replay protection
- BLE link itself can remain unencrypted
- Used to evaluate flexibility vs. implementation complexity

### `LE_Security_Mode_1_Level_4/` - Scenario C
Implementation of **Bluetooth LE Security Mode 1, Level 4** (strongest native BLE security in this project scope).

Key points:
- Authenticated pairing
- LE Secure Connections
- Encryption with MITM protection
- Used to measure security overhead compared with the baseline and application-layer security

---

## Hardware and Tools

Experiments were conducted using:

- **3x FireBeetle 2 ESP32-E** boards
- **BME280** environmental sensor (temperature/humidity)
- **Nordic nRF52840 Dongle** as BLE sniffer
- **Arduino framework** for development
- **Wireshark** for packet inspection and analysis

---

## Evaluation Metrics

Across scenarios, the project evaluates:

- Communication latency (round-trip time per request)
- Security overhead (pairing/encryption costs)
- Implementation complexity
- Practical security properties and limitations
- Exposure to known BLE attacks and threat models

---

## Intended Audience

This repository is intended for:

- Students and researchers working on IoT and wireless security
- Developers studying BLE security trade-offs
- Academic projects involving experimental BLE protocol analysis

---

## Disclaimer

This project is for **educational and research purposes only**.  
Do not use this code in production without a full security review and adaptation.
