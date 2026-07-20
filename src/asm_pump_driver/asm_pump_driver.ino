// Include necessary libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define pin connections
const int relayPin = 8;  // Relay connected to pin D8
const int ledPin    = 13; // Status LED connected to D13
const int sensorPin = A0; // ACS712 sensor connected to pin A0
const long bandgapReferenceMillivolts = 1068; // Calibrated internal 1.1V reference for this Nano
const float acs712SensitivityVoltsPerAmp = 0.100; // ACS712 20A version = 100 mV/A
const int zeroOffsetSampleCount = 100;

// Hysteresis parameters (adjust values based on actual signal level)
const float threshold_on = 1.2; // Current threshold in amperes to turn relay ON
const float threshold_off = 0.8; // Current threshold in amperes to turn relay OFF
bool relayState = false; // Current relay state
float zeroOffsetVoltage = 0.0;

// Rolling statistics for current (sliding window of 10 samples)
const int WINDOW_SIZE = 10;
float     readings[WINDOW_SIZE];
int       readIndex   = 0;
int       sampleCount = 0;
float     readingSum  = 0.0;

// Rolling average for Vcc measurement
const int VCC_WINDOW_SIZE = 8;
long      vccReadings[VCC_WINDOW_SIZE];
int       vccReadIndex   = 0;
int       vccSampleCount = 0;
long      vccReadingSum  = 0;

// Create an instance of the OLED display
Adafruit_SSD1306 display(128, 64, &Wire, -1);

long readVccMillivolts() {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    delay(2);
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC)) {
        ;
    }

    uint8_t low = ADCL;
    uint8_t high = ADCH;
    long reading = (high << 8) | low;

    return (bandgapReferenceMillivolts * 1023L) / reading;
#else
    return 5000L;
#endif
}

float updateAverageVccVoltage(long vccMillivolts) {
    vccReadingSum -= vccReadings[vccReadIndex];
    vccReadings[vccReadIndex] = vccMillivolts;
    vccReadingSum += vccReadings[vccReadIndex];
    vccReadIndex = (vccReadIndex + 1) % VCC_WINDOW_SIZE;

    if (vccSampleCount < VCC_WINDOW_SIZE) {
        vccSampleCount++;
    }

    return (vccReadingSum / (float)vccSampleCount) / 1000.0;
}

float readSensorVoltage(float adcReferenceVoltage) {
    int sensorValue = analogRead(sensorPin);
    return (sensorValue / 1023.0) * adcReferenceVoltage;
}

float calibrateZeroOffsetVoltage() {
    float voltageSum = 0.0;

    for (int i = 0; i < zeroOffsetSampleCount; i++) {
        long vccMillivolts = readVccMillivolts();
        float adcReferenceVoltage = vccMillivolts / 1000.0;
        voltageSum += readSensorVoltage(adcReferenceVoltage);
        delay(5);
    }

    return voltageSum / zeroOffsetSampleCount;
}

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
    display.print("Calibrating zero...");
    display.display();

    Serial.println("OLED initialized");

    zeroOffsetVoltage = calibrateZeroOffsetVoltage();
    Serial.print("Zero offset: ");
    Serial.print(zeroOffsetVoltage, 3);
    Serial.println(" V");
}

void loop() {
    long vccMillivolts = readVccMillivolts();
    float rawVccVoltage = vccMillivolts / 1000.0;
    float adcReferenceVoltage = updateAverageVccVoltage(vccMillivolts);

    // Read the analog value from the sensor
    int sensorValue = analogRead(sensorPin);
    float sensorVoltage = (sensorValue / 1023.0) * adcReferenceVoltage;
    float deltaVoltage = sensorVoltage - zeroOffsetVoltage;
    float currentAmps = deltaVoltage / acs712SensitivityVoltsPerAmp;
    float currentAbsAmps = currentAmps >= 0.0 ? currentAmps : -currentAmps;

    Serial.print("Sensor value: ");
    Serial.print(sensorValue);
    Serial.print(" | Sensor: ");
    Serial.print(sensorVoltage, 3);
    Serial.print(" V | Zero: ");
    Serial.print(zeroOffsetVoltage, 3);
    Serial.print(" V | Delta: ");
    Serial.print(deltaVoltage, 3);
    Serial.print(" V | Current: ");
    Serial.print(currentAmps, 3);
    Serial.print(" A | Vref(avg): ");
    Serial.print(adcReferenceVoltage, 3);
    Serial.print(" V | Vcc(raw): ");
    Serial.print(rawVccVoltage, 3);
    Serial.print(" V");

    // Apply hysteresis logic
    if (!relayState && currentAbsAmps > threshold_on) {
        // Turn relay ON if current exceeds upper threshold
        relayState = true;
        digitalWrite(relayPin, HIGH);
        digitalWrite(ledPin, HIGH);
        Serial.println(" | Relay: ON");
    } else if (relayState && currentAbsAmps < threshold_off) {
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
    readings[readIndex] = currentAbsAmps;
    readingSum += currentAbsAmps;
    readIndex = (readIndex + 1) % WINDOW_SIZE;
    if (sampleCount < WINDOW_SIZE) sampleCount++;

    float rollingAvg = readingSum / sampleCount;
    Serial.print("Stats | Avg: ");
    Serial.print(rollingAvg, 3);
    Serial.println(" A");

    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("Curr: ");
    display.print(currentAbsAmps, 3);
    display.print(" A");

    display.setCursor(0, 8);
    display.print("Vcc: ");
    display.print(adcReferenceVoltage, 3);
    display.print(" V");

    display.setCursor(0, 16);
    display.print("Zero: ");
    display.print(zeroOffsetVoltage, 3);
    display.print(" V");

    display.setCursor(0, 24);
    display.print("Relay: ");
    display.print(relayState ? "ON" : "OFF");

    display.setCursor(0, 32);
    display.print("ON:");
    display.print(threshold_on, 2);
    display.print(" OFF:");
    display.print(threshold_off, 2);
    display.print("A");

    display.setCursor(0, 48);
    display.print("Avg: ");
    display.print(rollingAvg, 3);
    display.print(" A");

    display.display();

    delay(1000); // Delay for stability
}