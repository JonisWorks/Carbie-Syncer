# Carbie-Syncer

A DIY, somehow accurate, Bluetooth 4-cylinder digital sync tool for motorcycle carburetors and throttle bodies. Built on an ESP32 and automotive MAP sensors, it broadcasts real-time vacuum data directly to your phone.

## Features
* **100% Wireless:** Read live data on your phone via Web Bluetooth.
* **The "Braid" Graph:** Live chart visualizes vacuum spikes to perfectly sync all cylinders.
* **Digital Tachometer:** Calculates RPM purely from Cylinder 1's vacuum pulses—no ignition wiring needed.
* **Live Sensor Profiles:** Swap mathematical filtering (1-Bar to 3-Bar) instantly via the web app.
* **Auto-Averaging:** Color-coded bars guide your physical adjustments.

## Bill of Materials
* 1x **ESP32-S3 Development Board**
* 4x **Automotive MAP Sensors** 3 pin, 5v (1-Bar recommended; cheap 3-Bar turbo sensors should work fine using the web app's heavier filter profiles).
* 4x **10kΩ Resistors** & 4x **20kΩ Resistors** (For the voltage divider)
* 4x **4.7µF Ceramic Capacitors**
* 1x **5V Power Source** (Standard USB)
* 4x Vacuum hoses & DIY Enclosure (CAD files in `/3D_Files`)

## 🔧 Wiring Diagram 
The ESP32 is 3.3V, but MAP sensors output 5V. You **must** build this voltage divider for each sensor. *(Warning: Always verify your specific sensor's pinout before soldering!)*

Repeat exactly 4 times. Ensure your 5V power supply and ESP32 share a common Ground.

```text
[MAP Sensor +5V]  -------> Shared 5V Power Rail
[MAP Sensor GND]  -------> Shared Ground Rail

[MAP SIGNAL] 
       │
   [10k Ω Resistor]
       │
       ├───────> To ESP32 GPIO Pin (e.g., Pins 4, 5, 6, 7)
       │
   [20k Ω Resistor] ───┐
       │               │
   [4.7µF Capacitor] ──┴────> Shared Ground Rail
```
*(Note: Ensure your external 5V power supply shares a common Ground wire with the ESP32!)*

## Software Setup
This project uses **PlatformIO** for the microcontroller firmware and a pure HTML/JS file for the phone interface.

### 1. Flash the ESP32
1. Open the `Firmware` folder in VS Code using the PlatformIO extension.
2. The `platformio.ini` file uses a generic, safe environment configuration that should successfully flash to almost any ESP32-S3 board.
3. Build and Upload the `main.cpp` firmware to your ESP32.

### 2. The Phone App
There is nothing to install.
1. Open the `WebApp/index.html` file in a modern web browser that supports Web Bluetooth (Google Chrome is highly recommended).
2. Power up the ESP32, click **Connect BLE Tool**, and pair it.

## Operating Instructions

1. **Power Up**: Turn on the ESP32 and connect your phone.

2. **Hook up Hoses**: Attach hoses tightly to the engine's intake barbs. (Crucial: Cylinder 1 MUST be connected to get an RPM reading).

3. **Calibrate**: With hoses connected but the ENGINE OFF, press "Zero Sensors". This accounts for garage air pressure and hose compression.

4. **Select Profile**: Pick your sensor type from the dropdown (e.g., 3-Bar Default, or 1-Bar Fast).

5. **Tune**: Start the engine and adjust sync screws until all lines merge.

## DISCLAIMER
1. This project is provided "as is". While the 5V circuitry is generally very safe, I am not responsible for any damaged property, fires, or personal injury that may occur while building or using this tool or your bikes performance. While I want to help and provide this project as best as I can there could be bugs.   

## License
CC BY-NC 4.0: Free for personal, DIY garage use. No commercial use or selling fully assembled kits without explicit written permission from jonisworks.
