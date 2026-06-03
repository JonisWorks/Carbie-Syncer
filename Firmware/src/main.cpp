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
const int pin1 = 4; const int pin2 = 5;
const int pin3 = 6; const int pin4 = 7;

// --- LIVE SENSOR PROFILES ---
float alpha = 0.05;       
int numSamples = 100;     
int pulseThreshold = 10;  // Threshold for the RPM noise filter

float s1Smoothed = 0, s2Smoothed = 0, s3Smoothed = 0, s4Smoothed = 0;
float s1Offset = 0, s2Offset = 0, s3Offset = 0, s4Offset = 0;

// --- RPM TRACKING VARIABLES ---
unsigned long lastPulseTime = 0;
float currentRPM = 0, smoothedRPM = 0, prevS1 = 0;
bool isRising = false;

// Non-blocking timer
unsigned long lastTxTime = 0;
const unsigned long txInterval = 50; 

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { 
      deviceConnected = false; 
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) {
      String rxValue = pChar->getValue().c_str();
      
      if (rxValue == "ZERO") {
        s1Offset = s1Smoothed; s2Offset = s2Smoothed;
        s3Offset = s3Smoothed; s4Offset = s4Smoothed;
        Serial.println("Sensors Zeroed!");
      } 
      else if (rxValue == "SET_1BAR") {
        alpha = 0.15; numSamples = 20; pulseThreshold = 5;
        Serial.println("Profile: 1-Bar (Fast)");
      } 
      else if (rxValue == "SET_2BAR") {
        alpha = 0.10; numSamples = 50; pulseThreshold = 8;
        Serial.println("Profile: 2-Bar (Balanced)");
      } 
      else if (rxValue == "SET_3BAR") {
        alpha = 0.05; numSamples = 100; pulseThreshold = 10;
        Serial.println("Profile: 3-Bar (Default)");
      } 
      else if (rxValue == "SET_NOISY") {
        alpha = 0.02; numSamples = 200; pulseThreshold = 15;
        Serial.println("Profile: Ultra-Smooth (Noisy)");
      }
    }
};

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

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

  s1Smoothed = analogRead(pin1); s2Smoothed = analogRead(pin2);
  s3Smoothed = analogRead(pin3); s4Smoothed = analogRead(pin4);
  prevS1 = s1Smoothed;
}

void loop() {
  // --- 1. BURST OVERSAMPLING ---
  long sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;

  for (int i = 0; i < numSamples; i++) {
    sum1 += analogRead(pin1); sum2 += analogRead(pin2);
    sum3 += analogRead(pin3); sum4 += analogRead(pin4);
    delayMicroseconds(30); 
  }

  float raw1 = (float)sum1 / numSamples; float raw2 = (float)sum2 / numSamples;
  float raw3 = (float)sum3 / numSamples; float raw4 = (float)sum4 / numSamples;

  // --- 2. EMA FILTER ---
  s1Smoothed = (raw1 * alpha) + (s1Smoothed * (1.0 - alpha));
  s2Smoothed = (raw2 * alpha) + (s2Smoothed * (1.0 - alpha));
  s3Smoothed = (raw3 * alpha) + (s3Smoothed * (1.0 - alpha));
  s4Smoothed = (raw4 * alpha) + (s4Smoothed * (1.0 - alpha));

  // --- 3. SOFTWARE RPM DETECTION ---
  unsigned long currentMillis = millis();
  
  if (s1Smoothed > (prevS1 + pulseThreshold)) {
      if (!isRising) {
          unsigned long pulseInterval = currentMillis - lastPulseTime;
          if (pulseInterval > 12) { // Ignore >10,000 RPM noise
              lastPulseTime = currentMillis;
              currentRPM = (60000.0 / (float)pulseInterval) * 2.0;
          }
          isRising = true;
      }
  } else if (s1Smoothed < (prevS1 - pulseThreshold)) {
      isRising = false; 
  }
  prevS1 = s1Smoothed;

  // Timeout: Drop to 0 if engine shuts off
  if (currentMillis - lastPulseTime > 1500) { currentRPM = 0; }

  // Smooth the RPM for the screen
  smoothedRPM = (currentRPM * 0.1) + (smoothedRPM * 0.9);

  // --- 4. BLUETOOTH TRANSMISSION ---
  if (deviceConnected && (currentMillis - lastTxTime >= txInterval)) {
    lastTxTime = currentMillis;

    int v1 = (int)(s1Smoothed - s1Offset); int v2 = (int)(s2Smoothed - s2Offset);
    int v3 = (int)(s3Smoothed - s3Offset); int v4 = (int)(s4Smoothed - s4Offset);
    int finalRPM = (int)smoothedRPM;

    char txString[40]; // 40 bytes to fit the 5th variable
    snprintf(txString, sizeof(txString), "%d,%d,%d,%d,%d", v1, v2, v3, v4, finalRPM);
    
    pCharacteristic->setValue(txString);
    pCharacteristic->notify();
  }
}
