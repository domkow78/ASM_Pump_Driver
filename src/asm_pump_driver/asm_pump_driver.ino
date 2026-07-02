# Include necessary libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define pin connections
const int relayPin = 8; // Relay connected to pin D8
const int sensorPin = A0; // ACS724 sensor connected to pin A0

// Hysteresis parameters (adjust values based on actual signal level)
const float threshold_on = 1.2; // Voltage threshold to turn relay ON
const float threshold_off = 0.8; // Voltage threshold to turn relay OFF
bool relayState = false; // Current relay state

// Create an instance of the OLED display
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
    // Initialize the relay pin
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW); // Ensure relay is off initially

    // Initialize the OLED display
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Monitoring current...");
    display.display();
}

void loop() {
    // Read the analog value from the sensor
    int sensorValue = analogRead(sensorPin);
    float current = (sensorValue / 1023.0) * 5.0; // Convert to voltage

    // Apply hysteresis logic
    if (!relayState && current > threshold_on) {
        // Turn relay ON if current exceeds upper threshold
        relayState = true;
        digitalWrite(relayPin, HIGH);
    } else if (relayState && current < threshold_off) {
        // Turn relay OFF if current drops below lower threshold
        relayState = false;
        digitalWrite(relayPin, LOW);
    }

    // Display the current value and relay state on the OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.print("Current: ");
    display.print(current);
    display.print(" V");
    
    display.setCursor(0, 10);
    display.print("Relay: ");
    display.print(relayState ? "ON" : "OFF");
    
    display.display();

    delay(1000); // Delay for stability
}