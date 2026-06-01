#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs for the Phone App
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// Hardware Pins for ESP32-S3 (ADC1 safe pins)
const int pin1 = 4;
const int pin2 = 5;
const int pin3 = 6;
const int pin4 = 7;

// --- SENSOR REACTION SPEED (EMA Filter) ---
// Lower number = Smoother lines but slower reaction time.
// Higher number = Faster reaction time but bouncier lines.
// Default is 0.05 (Heavy smoothing, optimized for 3-Bar sensors).
// If you are using 1-Bar sensors, change this to 0.15 for quick gauge reaction.
float s1Smoothed = 0, s2Smoothed = 0, s3Smoothed = 0, s4Smoothed = 0;
const float alpha = 0.05; // 0.05 is a good default for 3-Bar sensors, 0.15 is better for 1-Bar sensors. Adjust to your preference.

// Calibration Offsets (Zero out the gauges)
float s1Offset = 0, s2Offset = 0, s3Offset = 0, s4Offset = 0;

// Non-blocking timer variables for BLE transmission
unsigned long lastTxTime = 0;
const unsigned long txInterval = 50; // Transmit data every 50ms (20Hz) for smooth phone graphics

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { 
      deviceConnected = false; 
      // Restart advertising so you can reconnect if you close the app
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) {
      String rxValue = pChar->getValue().c_str();
      if (rxValue == "ZERO") {
        s1Offset = s1Smoothed;
        s2Offset = s2Smoothed;
        s3Offset = s3Smoothed;
        s4Offset = s4Smoothed;
        Serial.println("All 4 Sensors Zeroed Successfully!");
      }
    }
};

void setup() {
  Serial.begin(115200);
  
  // Set ADC resolution (12-bit is standard: 0 to 4095)
  analogReadResolution(12);

  // Initialize BLE
  BLEDevice::init("Carbie_Syncer");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_WRITE
                    );
                    
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  // Grab the initial baseline readings
  s1Smoothed = analogRead(pin1);
  s2Smoothed = analogRead(pin2);
  s3Smoothed = analogRead(pin3);
  s4Smoothed = analogRead(pin4);
}

void loop() {
  // --- BURST OVERSAMPLING ---
  // How many rapid micro-readings the ESP32 takes to crush electrical noise.
  // Default is 100 (High noise reduction, mandatory for 3-Bar sensors).
  // If you are using 1-Bar sensors, you can lower this to 20 to make the loop run even faster.
  long sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;
  const int numSamples = 100; // Adjust this number based on your sensor type. More samples = smoother but slower updates. Fewer samples = faster but bouncier updates. 100 is a good default for 3-Bar sensors, 20 is often sufficient for 1-Bar sensors with less noise. Experiment to find the sweet spot for your setup!

  for (int i = 0; i < numSamples; i++) {
    sum1 += analogRead(pin1);
    sum2 += analogRead(pin2);
    sum3 += analogRead(pin3);
    sum4 += analogRead(pin4);
    delayMicroseconds(30); // Tiny pause to allow ADC voltages to settle stabilizes readings
  }

  // Calculate the average of this rapid burst
  float raw1 = (float)sum1 / numSamples;
  float raw2 = (float)sum2 / numSamples;
  float raw3 = (float)sum3 / numSamples;
  float raw4 = (float)sum4 / numSamples;

  // --- 2. EXPONENTIAL MOVING AVERAGE (EMA) FILTER ---
  // Apply the smoothing formula to the clean, oversampled averages
  s1Smoothed = (raw1 * alpha) + (s1Smoothed * (1.0 - alpha));
  s2Smoothed = (raw2 * alpha) + (s2Smoothed * (1.0 - alpha));
  s3Smoothed = (raw3 * alpha) + (s3Smoothed * (1.0 - alpha));
  s4Smoothed = (raw4 * alpha) + (s4Smoothed * (1.0 - alpha));

  // --- 3. NON-BLOCKING BLUETOOTH TRANSMISSION ---
  // Check if it's time to transmit data without stopping the engine sampling math
  unsigned long currentMillis = millis();
  if (deviceConnected && (currentMillis - lastTxTime >= txInterval)) {
    lastTxTime = currentMillis;

    // Calculate final value relative to your calibrated offset
    int v1 = (int)(s1Smoothed - s1Offset);
    int v2 = (int)(s2Smoothed - s2Offset);
    int v3 = (int)(s3Smoothed - s3Offset);
    int v4 = (int)(s4Smoothed - s4Offset);

    // Package the 4 sensor values into a comma-separated string
    char txString[32];
    snprintf(txString, sizeof(txString), "%d,%d,%d,%d", v1, v2, v3, v4);
    
    // Send it to the web app
    pCharacteristic->setValue(txString);
    pCharacteristic->notify();
  }
}