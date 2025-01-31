#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <Servo.h>

// **Fingerprint Sensor Pins**
#define RX_PIN 4 // Fingerprint TX to Arduino RX
#define TX_PIN 5 // Fingerprint RX to Arduino TX

// **Buzzer and Servo Pins**
#define BUZZER_PIN 8
#define SERVO_PIN 9

// **Ultrasonic Sensor Pins**
#define TRIG_PIN 6
#define ECHO_PIN 7

// Define ESP-01 Pins
#define ESP01_TX 3 // Arduino TX to ESP-01 RX
#define ESP01_RX 2 // Arduino RX to ESP-01 TX

// **Create SoftwareSerial object for the fingerprint sensor**
SoftwareSerial mySerial(RX_PIN, TX_PIN);
Adafruit_Fingerprint finger(&mySerial);

// Create SoftwareSerial object for ESP-01
SoftwareSerial espSerial(ESP01_RX, ESP01_TX);

// **LCD Object**
LiquidCrystal_I2C lcd(0x27, 16, 2);

// **Servo Object**
Servo servo;

void setup() {
  Serial.begin(9600);
  // Serial for ESP-01 communication
  espSerial.begin(9600);
  while (!Serial);

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
  
  // **Initialize LCD**
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // **Initialize Ultrasonic Sensor**
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // **Initialize Fingerprint Sensor**
  finger.begin(57600);
  delay(1000);

  // **Initialize Servo**
  servo.attach(SERVO_PIN);
  servo.write(0); // Locked position

  // **Initialize Buzzer**
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // **Check Fingerprint Sensor**
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor detected!");
    lcd.clear();
    lcd.print("Sensor Ready");
  } else {
    Serial.println("Fingerprint sensor NOT detected!");
    lcd.clear();
    lcd.print("Sensor Error!");
    while (true) {
      delay(100);
    }
  }

  // **Display Template Count**
  finger.getTemplateCount();
  lcd.setCursor(0, 1);
  lcd.print("Templates: ");
  lcd.print(finger.templateCount);
  delay(2000);
  lcd.clear();
  lcd.print("Waiting...");
}

void loop() {
  float distance = getDistance();

  // Print ESP-01 responses if available
  if (espSerial.available()) {
    String response = espSerial.readString();
    Serial.println("ESP Response: " + response);
  }


  if (distance > 0 && distance <= 20) { // Only activate if distance is <= 20 cm
    lcd.backlight();
    lcd.clear();
    lcd.print("Object Detected!");
    delay(1000);
    
    Serial.println("\nPress 'e' to enroll a new fingerprint or 'd' to detect fingerprints.");
    lcd.clear();
    lcd.print("e: Enroll");
    lcd.setCursor(0, 1);
    lcd.print("d: Detect");

    while (!Serial.available());
    char option = Serial.read();

    if (option == 'e') {
      lcd.clear();
      lcd.print("Enroll Finger");
      enrollFingerprint();
    } else if (option == 'd') {
      lcd.clear();
      lcd.print("Detect Finger");
      detectFingerprint();
    } else {
      Serial.println("Invalid option. Try again.");
      lcd.clear();
      lcd.print("Invalid Option");
      delay(2000);
    }
    clearSerialBuffer(); // **Fix Invalid Option Issue**
  } else {
    lcd.clear();
    lcd.noBacklight();
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

// **Function to Clear Serial Buffer**
void clearSerialBuffer() {
  while (Serial.available()) {
    Serial.read();
  }
}

// **Ultrasonic Sensor Function**
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2; // Convert to cm
  return distance;
}

// **Function to Detect Fingerprints**
void detectFingerprint() {
  lcd.clear();
  lcd.print("Waiting for Finger");
  int timeout = 5000; // Wait for 5 seconds
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    int id = getFingerprintID();
    if (id != -1) {
      Serial.print("Found ID: ");
      Serial.println(id);
      lcd.clear();
      lcd.print("Access Granted!");
      lcd.setCursor(0, 1);
      lcd.print("ID: ");
      lcd.print(id);

      // **Turn the Servo Motor**
      servo.write(90); // Open door
      delay(3000);
      servo.write(0);  // Close door

      delay(3000);
      clearSerialBuffer();
      return;
    }
  }

  Serial.println("No fingerprint detected.");
  lcd.clear();
  lcd.print("Access Denied!");
  lcd.setCursor(0, 1);
  lcd.print("No Match");

  // **Trigger the Buzzer**
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);

  delay(2000);
  clearSerialBuffer();
}

// **Function to Capture and Identify a Fingerprint**
int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) return -1;
  if (p == FINGERPRINT_IMAGEFAIL) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) return finger.fingerID;

  return -1;
}

// Function to enroll a fingerprint
void enrollFingerprint() {
  int id = 0;
  lcd.clear();
  lcd.print("Enter ID: ");
  Serial.println("Enter ID (1-127) to associate with the fingerprint:");

  // Clear the input buffer
  while (Serial.available()) {
    Serial.read();
  }

  // Wait for valid ID input
  while (id <= 0 || id > 127) {
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n'); // Read user input until newline
      id = input.toInt(); // Convert the input to an integer

      if (id <= 0 || id > 127) {
        Serial.println("Invalid ID. Enter 1-127.");
        lcd.clear();
        lcd.print("Invalid ID");
        delay(2000);
        lcd.clear();
        lcd.print("Enter ID: ");
      }
    }
  }

  lcd.clear();
  lcd.print("ID: ");
  lcd.print(id);

  // First capture
  int p = -1;
  lcd.setCursor(0, 1);
  lcd.print("Place Finger...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Imaging error. Try again.");
      lcd.clear();
      lcd.print("Imaging Error");
      return;
    }
  }

  lcd.clear();
  lcd.print("Image Captured!");
  delay(1000);

  // Convert the image to a template
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image.");
    lcd.clear();
    lcd.print("Conversion Error");
    return;
  }

  lcd.clear();
  lcd.print("Remove Finger");
  delay(2000);

  // Second capture
  p = -1;
  lcd.clear();
  lcd.print("Place Again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Imaging error. Try again.");
      lcd.clear();
      lcd.print("Imaging Error");
      return;
    }
  }

  lcd.clear();
  lcd.print("2nd Captured");
  delay(1000);

  // Convert the second image to a template
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting 2nd image.");
    lcd.clear();
    lcd.print("Conversion Error");
    return;
  }

  // Create the model and store it
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Error creating model.");
    lcd.clear();
    lcd.print("Model Error");
    return;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint Stored!");
    lcd.clear();
    lcd.print("Stored!");
  } else {
    Serial.println("Error storing fingerprint.");
    lcd.clear();
    lcd.print("Store Error");
  }
  delay(2000);

  // Clear the serial buffer before returning
  while (Serial.available()) {
    Serial.read();
  }
}
