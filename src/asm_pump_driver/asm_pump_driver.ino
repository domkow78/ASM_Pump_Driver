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
const unsigned long rmsWindowMs = 200; // 10 full periods at 50 Hz for better averaging
const float currentDeadZoneAmps = 0.01; // Ignore residual noise below this value

// Hysteresis parameters in amperes RMS.
// Pump power range 12-38W on 230V AC -> Irms ~ 0.052 A (12W) .. 0.165 A (38W).
// Thresholds must be below the lowest running current (~0.052 A).
const float threshold_on = 0.035; // Current (A) to turn relay ON (pump running)
const float threshold_off = 0.02; // Current (A) to turn relay OFF (pump stopped)
bool relayState = false; // Current relay state

// --- Simple activity detection (mean absolute deviation) ---
// Instead of computing a precise RMS current, we sample the sensor fast,
// subtract the resting value (~Vcc/2) and average the absolute deviation.
// This is robust to inverter frequency changes and needs no accurate
// current calibration. If the averaged deviation exceeds a threshold,
// the pump is considered running.
const unsigned long activityWindowMs = 30;   // Averaging window (20-50 ms)
const unsigned long activitySampleIntervalUs = 250; // ~4 kHz sampling (2-5 kHz)
// Thresholds are expressed in ADC counts of mean absolute deviation.
const float activityThresholdOn  = 8.0; // Deviation to turn relay ON (pump running)
const float activityThresholdOff = 4.0; // Deviation to turn relay OFF (pump stopped)

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

// Simple pump-activity detection based on mean absolute deviation.
// 1. Sample the ADC fast (a few kHz).
// 2. Subtract the resting value (~Vcc/2, computed as the mean of samples).
// 3. Accumulate the absolute deviation of each sample.
// 4. Average over a short window (20-50 ms).
// Returns the mean absolute deviation in ADC counts.
float measurePumpActivity() {
    unsigned long startMs = millis();
    unsigned long sampleCount = 0;
    long sum = 0;               // Sum of raw ADC values (for resting value)
    int samples[300];           // Buffer for one window (max ~300 samples)
    const int maxSamples = 300;

    // First pass: collect raw samples at a fixed rate.
    while ((millis() - startMs < activityWindowMs) && (sampleCount < maxSamples)) {
        int raw = analogRead(sensorPin);
        samples[sampleCount] = raw;
        sum += raw;
        sampleCount++;
        delayMicroseconds(activitySampleIntervalUs);
    }

    if (sampleCount == 0) {
        return 0.0;
    }

    // Resting value = mean of the samples (dynamic, removes offset drift).
    float mean = (float)sum / sampleCount;

    // Second pass: accumulate absolute deviation from the resting value.
    float sumAbsDeviation = 0.0;
    for (unsigned long i = 0; i < sampleCount; i++) {
        sumAbsDeviation += fabs(samples[i] - mean);
    }

    return sumAbsDeviation / sampleCount;
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
    // 1. Measure supply voltage Vcc
    long vccMillivolts = readVccMillivolts();
    float rawVccVoltage = vccMillivolts / 1000.0;
    float adcReferenceVoltage = updateAverageVccVoltage(vccMillivolts);

    // 2. Direct raw ADC reading on the sensor pin (control measurement)
    int rawAdcValue = analogRead(sensorPin);
    float adcVoltage = (rawAdcValue / 1023.0) * adcReferenceVoltage;

    // 3. Pump activity detection (mean absolute deviation, no RMS needed)
    float activity = measurePumpActivity();

    // Apply hysteresis logic on the activity value
    if (!relayState && activity > activityThresholdOn) {
        relayState = true;
        digitalWrite(relayPin, HIGH);
        digitalWrite(ledPin, HIGH);
    } else if (relayState && activity < activityThresholdOff) {
        relayState = false;
        digitalWrite(relayPin, LOW);
        digitalWrite(ledPin, LOW);
    }

    // NOTE: current (RMS) measurement temporarily disabled
    // float currentAmps = measureRmsCurrent(adcReferenceVoltage);

    Serial.print("Vcc(avg): ");
    Serial.print(adcReferenceVoltage, 3);
    Serial.print(" V | Vcc(raw): ");
    Serial.print(rawVccVoltage, 3);
    Serial.print(" V | ADC: ");
    Serial.print(rawAdcValue);
    Serial.print(" (");
    Serial.print(adcVoltage, 3);
    Serial.print(" V) | Act: ");
    Serial.print(activity, 2);
    Serial.print(" | Relay: ");
    Serial.println(relayState ? "ON" : "OFF");

    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Line 1: Vcc supply voltage
    display.setCursor(0, 0);
    display.print("Vcc: ");
    display.print(adcReferenceVoltage, 3);
    display.print(" V");

    // Line 2: direct raw ADC reading (control measurement)
    display.setCursor(0, 8);
    display.print("ADC: ");
    display.print(rawAdcValue);
    display.print(" (");
    display.print(adcVoltage, 3);
    display.print("V)");

    // Line 3: pump activity (mean absolute deviation)
    display.setCursor(0, 16);
    display.print("Act: ");
    display.print(activity, 2);

    // Line 7: relay state
    display.setCursor(0, 48);
    display.print("Relay: ");
    display.print(relayState ? "ON" : "OFF");

    // Line 8: hysteresis thresholds
    display.setCursor(0, 56);
    display.print("ON:");
    display.print(activityThresholdOn, 1);
    display.print(" OFF:");
    display.print(activityThresholdOff, 1);

    display.display();

    delay(1000); // Delay for stability
}