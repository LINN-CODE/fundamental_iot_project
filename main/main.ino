#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// Define Pins for SoftwareSerial (Fingerprint Sensor)
#define RX_PIN 4 // Arduino Pin connected to fingerprint sensor TX
#define TX_PIN 5 // Arduino Pin connected to fingerprint sensor RX

// Create SoftwareSerial object for communication with the fingerprint sensor
SoftwareSerial mySerial(RX_PIN, TX_PIN);

// Create the Adafruit_Fingerprint object
Adafruit_Fingerprint finger(&mySerial);

// Create an LCD object with the I2C address (usually 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 16, 2 to 20, 4 if using a 20x4 LCD

void setup() {
  // Start the Serial Monitor for debugging
  Serial.begin(9600);
  while (!Serial); // For boards like Leonardo that need this
  
  // Initialize the LCD
  lcd.init();
  lcd.backlight(); // Turn on the backlight
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  
  // Initialize the fingerprint sensor
  finger.begin(57600); // Baud rate, adjust if needed (e.g., 9600, 115200)
  delay(1000);

  // Check if the fingerprint sensor is detected
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor detected!");
    lcd.clear();
    lcd.print("Sensor Ready");
  } else {
    Serial.println("Fingerprint sensor not detected!");
    lcd.clear();
    lcd.print("Sensor Error!");
    while (true) {
      delay(100); // Halt here if the sensor is not found
    }
  }

  // Display template count
  finger.getTemplateCount();
  lcd.setCursor(0, 1);
  lcd.print("Templates: ");
  lcd.print(finger.templateCount);
  delay(2000);
  lcd.clear();
  lcd.print("Ready for Use");
}

void loop() {
  // Display the menu on the Serial Monitor
  Serial.println("\nPress 'e' to enroll a new fingerprint or 'd' to detect fingerprints.");
  lcd.clear();
  lcd.print("e: Enroll");
  lcd.setCursor(0, 1);
  lcd.print("d: Detect");

  while (!Serial.available()); // Wait for user input
  char option = Serial.read();

  if (option == 'e') {
    lcd.clear();
    lcd.print("Enroll Finger");
    enrollFingerprint(); // Call the enrollment function
  } else if (option == 'd') {
    lcd.clear();
    lcd.print("Detect Finger");
    detectFingerprint(); // Call the detection function
  } else {
    Serial.println("Invalid option. Try again.");
    lcd.clear();
    lcd.print("Invalid Option");
    delay(2000);
  }
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
}

void detectFingerprint() {
  lcd.clear();
  lcd.print("Waiting...");
  int timeout = 5000; // Wait for 5 seconds
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    int id = getFingerprintID();
    if (id != -1) {
      Serial.print("Found ID: ");
      Serial.println(id);
      lcd.clear();
      lcd.print("ID Found: ");
      lcd.print(id);
      delay(3000);

      // Clear the serial buffer before returning
      while (Serial.available()) {
        Serial.read();
      }
      return;
    }
  }

  Serial.println("No fingerprint detected.");
  lcd.clear();
  lcd.print("No Match");
  delay(2000);

  // Clear the serial buffer before returning
  while (Serial.available()) {
    Serial.read();
  }
}

// Function to capture and identify a fingerprint
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
