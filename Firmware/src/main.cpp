#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// Hardware Pins (ADC1)
const int pin1 = 4; const int pin2 = 5;
const int pin3 = 6; const int pin4 = 7;

// --- LIVE SENSOR PROFILES ---
float alpha = 0.05;       
int numSamples = 100;     
int pulseThreshold = 10;  

float s1Smoothed = 0, s2Smoothed = 0, s3Smoothed = 0, s4Smoothed = 0;
float s1Offset = 0, s2Offset = 0, s3Offset = 0, s4Offset = 0;

// --- RPM & PEAK TRACKING VARIABLES ---
unsigned long lastPulseTime = 0;
float currentRPM = 0, smoothedRPM = 0, prevS1 = 0;
bool isRising = false;

// Peak Mode Variables
bool peakMode = false;
float minTracker1 = 4095, minTracker2 = 4095, minTracker3 = 4095, minTracker4 = 4095;
float peak1 = 0, peak2 = 0, peak3 = 0, peak4 = 0;

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
      } 
      else if (rxValue == "PEAK_ON") {
        peakMode = true;
      }
      else if (rxValue == "PEAK_OFF") {
        peakMode = false;
      }
      else if (rxValue == "FILTER_LIGHT") {
        alpha = 0.15; numSamples = 20; pulseThreshold = 5;
      } 
      else if (rxValue == "FILTER_MEDIUM") {
        alpha = 0.10; numSamples = 50; pulseThreshold = 8;
      } 
      else if (rxValue == "FILTER_HEAVY") {
        alpha = 0.05; numSamples = 100; pulseThreshold = 10;
      } 
      else if (rxValue == "FILTER_MAX") {
        alpha = 0.02; numSamples = 200; pulseThreshold = 15;
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

  // --- 3. PEAK & RPM DETECTION ---
  unsigned long currentMillis = millis();
  
  if (s1Smoothed < minTracker1) minTracker1 = s1Smoothed;
  if (s2Smoothed < minTracker2) minTracker2 = s2Smoothed;
  if (s3Smoothed < minTracker3) minTracker3 = s3Smoothed;
  if (s4Smoothed < minTracker4) minTracker4 = s4Smoothed;

  if (s1Smoothed > (prevS1 + pulseThreshold)) {
      if (!isRising) {
          unsigned long pulseInterval = currentMillis - lastPulseTime;
          if (pulseInterval > 12) { 
              lastPulseTime = currentMillis;
              currentRPM = (60000.0 / (float)pulseInterval) * 2.0;
              
              peak1 = minTracker1; peak2 = minTracker2; 
              peak3 = minTracker3; peak4 = minTracker4;
          }
          minTracker1 = s1Smoothed; minTracker2 = s2Smoothed; 
          minTracker3 = s3Smoothed; minTracker4 = s4Smoothed;
          isRising = true;
      }
  } else if (s1Smoothed < (prevS1 - pulseThreshold)) {
      isRising = false; 
  }
  prevS1 = s1Smoothed;

  if (currentMillis - lastPulseTime > 1500) { 
      currentRPM = 0; 
      peak1 = s1Smoothed; peak2 = s2Smoothed; 
      peak3 = s3Smoothed; peak4 = s4Smoothed;
  }

  smoothedRPM = (currentRPM * 0.1) + (smoothedRPM * 0.9);

  // --- 4. BLUETOOTH TRANSMISSION ---
  if (deviceConnected && (currentMillis - lastTxTime >= txInterval)) {
    lastTxTime = currentMillis;

    int v1, v2, v3, v4;

    if (peakMode) {
        v1 = (int)(peak1 - s1Offset); v2 = (int)(peak2 - s2Offset);
        v3 = (int)(peak3 - s3Offset); v4 = (int)(peak4 - s4Offset);
    } else {
        v1 = (int)(s1Smoothed - s1Offset); v2 = (int)(s2Smoothed - s2Offset);
        v3 = (int)(s3Smoothed - s3Offset); v4 = (int)(s4Smoothed - s4Offset);
    }

    int finalRPM = (int)smoothedRPM;
    char txString[40]; 
    snprintf(txString, sizeof(txString), "%d,%d,%d,%d,%d", v1, v2, v3, v4, finalRPM);
    
    pCharacteristic->setValue(txString);
    pCharacteristic->notify();
  }
}
