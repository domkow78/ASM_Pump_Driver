// Include necessary libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define pin connections
const int relayPin = 8;  // Relay connected to pin D8
const int ledPin    = 13; // Status LED connected to D13
const int sensorPin = A0; // ACS724 sensor connected to pin A0

// Hysteresis parameters (adjust values based on actual signal level)
const float threshold_on = 1.2; // Voltage threshold to turn relay ON
const float threshold_off = 0.8; // Voltage threshold to turn relay OFF
bool relayState = false; // Current relay state

// Rolling statistics (sliding window of 10 samples)
const int WINDOW_SIZE = 10;
float     readings[WINDOW_SIZE];
int       readIndex   = 0;
int       sampleCount = 0;
float     readingSum  = 0.0;

// Create an instance of the OLED display
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
    // Initialize serial output for debugging
    Serial.begin(115200);
    while (!Serial) {
        ;
    }

    // Initialize the relay pin and status LED
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW); // Ensure relay is off initially
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);   // Ensure LED is off initially

    Serial.println("ASM Pump Driver start");

    // Initialize the OLED display
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Monitoring current...");
    display.display();

    Serial.println("OLED initialized");
}

void loop() {
    // Read the analog value from the sensor
    int sensorValue = analogRead(sensorPin);
    float current = (sensorValue / 1023.0) * 5.0; // Convert to voltage

    Serial.print("Sensor value: ");
    Serial.print(sensorValue);
    Serial.print(" | Voltage: ");
    Serial.print(current, 3);
    Serial.print(" V");

    // Apply hysteresis logic
    if (!relayState && current > threshold_on) {
        // Turn relay ON if current exceeds upper threshold
        relayState = true;
        digitalWrite(relayPin, HIGH);
        digitalWrite(ledPin, HIGH);
        Serial.println(" | Relay: ON");
    } else if (relayState && current < threshold_off) {
        // Turn relay OFF if current drops below lower threshold
        relayState = false;
        digitalWrite(relayPin, LOW);
        digitalWrite(ledPin, LOW);
        Serial.println(" | Relay: OFF");
    } else {
        Serial.print(" | Relay: ");
        Serial.println(relayState ? "ON" : "OFF");
    }

    // Compute rolling statistics
    readingSum -= readings[readIndex];
    readings[readIndex] = current;
    readingSum += current;
    readIndex = (readIndex + 1) % WINDOW_SIZE;
    if (sampleCount < WINDOW_SIZE) sampleCount++;

    float rollingAvg = readingSum / sampleCount;
    float rollingMax = readings[0];
    float rollingMin = readings[0];
    for (int i = 1; i < sampleCount; i++) {
        if (readings[i] > rollingMax) rollingMax = readings[i];
        if (readings[i] < rollingMin) rollingMin = readings[i];
    }

    Serial.print("Stats | Avg: "); Serial.print(rollingAvg, 3);
    Serial.print(" Max: ");        Serial.print(rollingMax, 3);
    Serial.print(" Min: ");        Serial.println(rollingMin, 3);

    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("Volt: ");
    display.print(current, 3);
    display.print(" V");

    display.setCursor(0, 8);
    display.print("Relay: ");
    display.print(relayState ? "ON" : "OFF");

    display.setCursor(0, 16);
    display.print("ON:");
    display.print(threshold_on, 2);
    display.print(" OFF:");
    display.print(threshold_off, 2);

    display.setCursor(0, 24);
    display.print("Avg: ");
    display.print(rollingAvg, 3);
    display.print(" V");

    display.setCursor(0, 32);
    display.print("Max: ");
    display.print(rollingMax, 3);
    display.print(" V");

    display.setCursor(0, 40);
    display.print("Min: ");
    display.print(rollingMin, 3);
    display.print(" V");

    display.display();

    delay(1000); // Delay for stability
}