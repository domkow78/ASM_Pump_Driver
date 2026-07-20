// Include necessary libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define pin connections
const int relayPin = 8;  // Relay connected to pin D8
const int ledPin    = 13; // Status LED connected to D13
const int sensorPin = A0; // ACS724 -2.5A..+2.5A (Pololu 4040) connected to pin A0
const long bandgapReferenceMillivolts = 1068; // Calibrated internal 1.1V reference for this Nano
const float zeroOffsetVoltage = 2.345; // ACS724 output voltage at zero current (~Vcc/2)
const float acs724SensitivityVoltsPerAmp = 0.400; // ACS724 +/-2.5A (Pololu 4040) = 400 mV/A

// AC RMS measurement parameters (mains 50 Hz -> 20 ms per period)
const unsigned long rmsWindowMs = 100; // 5 full periods at 50 Hz
const float currentDeadZoneAmps = 0.05; // Ignore residual noise below this value

// Hysteresis parameters (adjust values based on actual signal level)
const float threshold_on = 1.2; // Voltage threshold to turn relay ON
const float threshold_off = 0.8; // Voltage threshold to turn relay OFF
bool relayState = false; // Current relay state

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

// Measure AC RMS current over a fixed window.
// Vzero is derived dynamically as the mean of the samples, which removes
// offset drift. Returns RMS current in amperes.
float measureRmsCurrent(float adcReferenceVoltage) {
    unsigned long startMs = millis();
    unsigned long sampleCount = 0;
    float sum = 0.0;         // Sum of sample voltages (for dynamic Vzero)
    float sumSquares = 0.0;  // Sum of squared sample voltages

    // First pass: collect sum and sum of squares of the sensor voltage.
    while (millis() - startMs < rmsWindowMs) {
        float v = (analogRead(sensorPin) / 1023.0) * adcReferenceVoltage;
        sum += v;
        sumSquares += v * v;
        sampleCount++;
    }

    if (sampleCount == 0) {
        return 0.0;
    }

    float mean = sum / sampleCount;                       // Dynamic Vzero
    float meanSquares = sumSquares / sampleCount;
    float variance = meanSquares - (mean * mean);         // RMS of AC component
    if (variance < 0.0) {
        variance = 0.0;
    }

    float vRms = sqrt(variance);
    float iRms = vRms / acs724SensitivityVoltsPerAmp;

    if (iRms < currentDeadZoneAmps) {
        iRms = 0.0;
    }

    return iRms;
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
    display.print("Monitoring current...");
    display.display();

    Serial.println("OLED initialized");
}

void loop() {
    long vccMillivolts = readVccMillivolts();
    float rawVccVoltage = vccMillivolts / 1000.0;
    float adcReferenceVoltage = updateAverageVccVoltage(vccMillivolts);

    // Measure AC RMS current over the sampling window (dynamic Vzero inside)
    float currentAmps = measureRmsCurrent(adcReferenceVoltage);

    Serial.print("Irms: ");
    Serial.print(currentAmps, 3);
    Serial.print(" A | Vref(avg): ");
    Serial.print(adcReferenceVoltage, 3);
    Serial.print(" V | Vcc(raw): ");
    Serial.print(rawVccVoltage, 3);
    Serial.print(" V");

    // Apply hysteresis logic
    if (!relayState && currentAmps > threshold_on) {
        // Turn relay ON if current exceeds upper threshold
        relayState = true;
        digitalWrite(relayPin, HIGH);
        digitalWrite(ledPin, HIGH);
        Serial.println(" | Relay: ON");
    } else if (relayState && currentAmps < threshold_off) {
        // Turn relay OFF if current drops below lower threshold
        relayState = false;
        digitalWrite(relayPin, LOW);
        digitalWrite(ledPin, LOW);
        Serial.println(" | Relay: OFF");
    } else {
        Serial.print(" | Relay: ");
        Serial.println(relayState ? "ON" : "OFF");
    }

    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("Irms: ");
    display.print(currentAmps, 3);
    display.print(" A");

    display.setCursor(0, 8);
    display.print("Vcc: ");
    display.print(adcReferenceVoltage, 3);
    display.print(" V");

    display.setCursor(0, 16);
    display.print("Relay: ");
    display.print(relayState ? "ON" : "OFF");

    display.setCursor(0, 24);
    display.print("ON:");
    display.print(threshold_on, 2);
    display.print(" OFF:");
    display.print(threshold_off, 2);
    display.print("A");

    display.display();

    delay(1000); // Delay for stability
}