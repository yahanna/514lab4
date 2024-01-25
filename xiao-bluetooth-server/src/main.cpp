#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000; // Interval at which to send data over BLE (if conditions are met).

// HC-SR04 Pins
const int trigPin = 1;
const int echoPin = 2;

// Sensor data and DSP variables
float distance;
const int numReadings = 10;
float readings[numReadings]; // the readings from the sensor
int readIndex = 0;           // the index of the current reading
float total = 0;             // the running total
float average = 0;           // the average

#define SERVICE_UUID        "d4d8b28b-8928-4044-b3b2-fbed8f587fd0"
#define CHARACTERISTIC_UUID "c8e44563-c8f3-4822-8a41-9f4df10fa9ac"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// Function to calculate moving average
float calculateMovingAverage(float newDistance) {
    total -= readings[readIndex];
    readings[readIndex] = newDistance;
    total += readings[readIndex];
    readIndex = (readIndex + 1) % numReadings;
    return total / numReadings;
}

void setup() {
    Serial.begin(115200);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    for (int thisReading = 0; thisReading < numReadings; thisReading++) {
        readings[thisReading] = 0;
    }

    BLEDevice::init("welcome");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Waiting for clients...");
}

void loop() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH);
    distance = duration * 0.034 / 2;

    average = calculateMovingAverage(distance);

    Serial.print("Raw Distance: ");
    Serial.print(distance);
    Serial.print(" cm, Denoised: ");
    Serial.print(average);
    Serial.println(" cm");

if (deviceConnected) {
    // Send new readings to database only if the denoised distance is less than 30 cm
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        if (average < 30) { // Check if the denoised distance is less than 30 cm
            // Prepare the message with the average distance
            String message = "Distance: " + String(average, 2) + " cm";
            // Set the characteristic's value to the message
            pCharacteristic->setValue(message.c_str());
            // Notify the connected device with the new value
            pCharacteristic->notify();
            // Print the message to the Serial monitor for debugging
            Serial.println("BLE Notify value: " + message);
        } else {
            // If the distance is 30 cm or more, you can handle it differently or do nothing
            Serial.println("Distance is 30 cm or more, not sending over BLE.");
        }
        // Update the time of the last transmission
        previousMillis = currentMillis;
    }
}


    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
    delay(1000); // Delay to slow down the loop for readability of the serial output.
}
