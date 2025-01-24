#include <SoftwareSerial.h>

// Define ESP-01 Pins
#define ESP01_TX 3 // Arduino TX to ESP-01 RX
#define ESP01_RX 2 // Arduino RX to ESP-01 TX

// Create SoftwareSerial object for ESP-01
SoftwareSerial espSerial(ESP01_RX, ESP01_TX);

void setup() {
  // Serial for debugging
  Serial.begin(9600);
  
  // Serial for ESP-01 communication
  espSerial.begin(9600); // Some modules use 115200; adjust if needed
  
  Serial.println("Initializing ESP-01...");
  delay(2000);

  // Reset ESP-01
  sendATCommand("AT+RST", 2000);
  
  // Set ESP-01 to Station mode
  sendATCommand("AT+CWMODE=1", 1000);
  
  // Connect to Wi-Fi
  String ssid = "eee-iot";       // Replace with your Wi-Fi SSID
  String password = "I0t@mar2025!"; // Replace with your Wi-Fi password
  sendATCommand("AT+CWJAP=\"" + ssid + "\",\"" + password + "\"", 5000);
}

void loop() {
  // Print ESP-01 responses if available
  if (espSerial.available()) {
    String response = espSerial.readString();
    Serial.println("ESP Response: " + response);
  }
}

// Function to send AT commands to ESP-01
void sendATCommand(String command, int delayMs) {
  espSerial.println(command);
  delay(delayMs);
  while (espSerial.available()) {
    String response = espSerial.readString();
    Serial.println(response); // Debug response
  }
}
