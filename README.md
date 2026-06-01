# Carbie-Syncer by jonisworks

A DIY, somehow accurate, Bluetooth-enabled 4-cylinder digital synchronization tool for motorcycle carburetors and throttle bodies. Built around an ESP32 microcontroller and automotive MAP sensors, it broadcasts real-time vacuum data directly to a web browser on your phone.

## Features
* **100% Wireless:** Mount the hardware at your convenient place and read the live data on your phone using Web Bluetooth (BLE).
* **The "Braid" Graph:** A live, rolling chart that visualizes vacuum spikes, allowing you to perfectly tune the synchronization until all four cylinders merge into a single flat line.
* **Auto-Averaging:** Automatically calculates the engine's average vacuum and uses smart color thresholds to guide your adjustments.
* **Adaptive Firmware:** Features burst oversampling and Exponential Moving Average (EMA) filtering to provide rock-solid readings, regardless of electrical noise. 
* **Open-Source Hardware:** Prototype Enclosure CAD files are provided in `.step` format, allowing for easy modification and remixing

## Bill of Materials (Hardware)
To build this tool, you will need:
* 1x **ESP32 Development Board** (Designed around the ESP32-S3)
* 4x **Automotive MAP Sensors** *(For this project Polcar E10-0066 MAP sensors were used, See Sensor Notes below)*
* 4x **10kΩ Resistors** 
* 4x **20kΩ Resistors** 
* 4x **4.7µF Ceramic Capacitors** 
* 1x **5V Power Source (Standard PC USB or a standard 5V phone wall charger is enough)** 
* 4x Vacuum hoses
* Some sort of enclosure for electronics, wires, solder, soldering iron. 

### ⚠️ A Note on MAP Sensors
For the absolute best hardware resolution and fastest gauge reaction time, **naturally aspirated 1-Bar MAP sensors** are strongly recommended. 

However, if you only find cheap **3-Bar (Turbo) MAP sensors**, the firmware has been heavily armored with software burst-sampling to mathematically smooth out 3-Bar sensor data, but introduces delay and lower tuning resolution.

## 🔧 The Wiring Diagram 
Because the ESP32 operates at 3.3V and the sensors output 5V, you **must** use a voltage divider on the signal lines. 

**In this example (Polcar E10-0066) 3bar MAP sensors were used, but almost any other standard 5V analog 1bar - 3bar 3-pin MAP sensor should work. Just for convenience, look for one with a barb for easy tube fitting.**

**⚠️ WARNING: Pinouts are probably not universal. While the Polcar E10-0066 and Bosch 0-261-230-119 uses Pins 1 - 5v, 2 - GND, and 3 - SIGNAL, other brands (like GM, Denso, or Delphi) might have a completely different pin order. Always look up the pinout diagram for your specific sensor before wiring it up, source cheapest ( my way at least) identical sensors.**

Repeat this wiring circuit exactly 4 times (one for each MAP sensor) :

[MAP Sensor +5V Pin 1]  -------> Connect to Shared 5V Power Rail
[MAP Sensor GND Pin 2]  -------> Connect to Shared Ground Rail

[MAP Sensor SIGNAL Pin 3] 
       │
   [10k Ω Resistor]
       │
       ├───────> To ESP32 GPIO Pin (e.g., Pins 4, 5, 6, 7)
       │
   [20k Ω Resistor] ───┐
       │               │
   [4.7µF Capacitor] ──┴────> Connect to Shared Ground Rail

*(Note: Ensure your external 5V power supply shares a common Ground wire with the ESP32!)*

## Software Setup
This project uses **PlatformIO** for the microcontroller firmware and a pure HTML/JS file for the phone interface.

### 1. Flash the ESP32
1. Open the `Firmware` folder in VS Code using the PlatformIO extension.
2. The `platformio.ini` file uses a generic, safe environment configuration that will successfully flash to almost any ESP32-S3 board without PSRAM memory crashes.
3. **Power User Tweak:** The code is pre-configured with heavy filtering for 3-Bar sensors. If you are using 1-Bar sensors, open `main.cpp` and look for the `alpha` and `numSamples` variables at the top of the loop. Instructions are included in the code comments to speed up the reaction time for your specific sensors.
4. Build and Upload the `main.cpp` firmware to your ESP32.

### 2. The Phone App
There is nothing to install! 
1. Open the `WebApp/index.html` file in a modern web browser that supports Web Bluetooth (Google Chrome is highly recommended).
2. Power up the ESP32, click **Connect BLE Tool**, and pair it.

## 3D Printed Enclosure
The CAD files for the custom enclosure (`carbie_syncer_box_lower.step` and `carbie_syncer_box_upper.step`) are located in the `3D_Files` folder. 
* Import the `.step` files directly into your slicer, or modify them in your preferred CAD software.
* **Material Recommendation:** PETG, ABS or some other exotics.

## Operating Instructions
1. **Always calibrate first:** Power on the device and click "Zero Sensors" in the app *before* starting the motorcycle engine to set the baseline atmospheric pressure.
2. Connect the vacuum hoses to your engine's intake manifold barbs. Ensure a snug fit; loose hoses cause vacuum leaks and completely false readings.
3. Start the engine and adjust your carburetor synchronization screws until the lines on the live graph merge.

## DISCLAIMER
1. This project is provided "as is". While the 5V circuitry is generally very safe, I am not responsible for any damaged property, fires, or personal injury that may occur while building or using this tool or your bikes performance. While I want to help and provide this project as best as I can there could be bugs.   

## License
**License: Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)**
This project is free for personal, DIY, and hobbyist use. You may download, modify, and build this tool for your own garage. However, you may **not** use this design, code, or 3D files for commercial purposes or sell fully assembled units/kits without explicit written permission from jonisworks.