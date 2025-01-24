#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// Define Pins for SoftwareSerial
#define RX_PIN 4 // Arduino Pin connected to fingerprint sensor TX
#define TX_PIN 5 // Arduino Pin connected to fingerprint sensor RX

// Create SoftwareSerial object for communication with the fingerprint sensor
SoftwareSerial mySerial(RX_PIN, TX_PIN);

// Create the Adafruit_Fingerprint object
Adafruit_Fingerprint finger(&mySerial);

void setup() {
  // Start the Serial Monitor for debugging
  Serial.begin(9600);
  while (!Serial); // For boards like Leonardo that need this
  
  Serial.println("\nFingerprint Sensor Initialization");
  delay(100);

  // Start the fingerprint sensor communication
  finger.begin(57600); // Baud rate, adjust if needed (e.g., 9600, 115200)

  // Check if the fingerprint sensor is detected
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor detected!");
  } else {
    Serial.println("Error: Fingerprint sensor not detected. Check wiring and power.");
    while (true) {
      delay(100); // Halt here if the sensor is not found
    }
  }

  // Get the number of templates stored in the sensor
  finger.getTemplateCount();
  Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates.");
  Serial.println("Ready for fingerprint enrollment...");
}

void loop() {
  // Enrollment menu
  Serial.println("\nPress 'e' to enroll a new fingerprint or 'd' to detect fingerprints.");
  while (!Serial.available()); // Wait for user input
  char option = Serial.read();

  if (option == 'e') {
    enrollFingerprint(); // Call the enrollment function
  } else if (option == 'd') {
    detectFingerprint(); // Call the detection function
  } else {
    Serial.println("Invalid option. Try again.");
  }
}

// Function to enroll a fingerprint
void enrollFingerprint() {
  int id = 0;
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
        Serial.println("Invalid ID. Please enter a number between 1 and 127.");
      }
    }
  }

  Serial.print("Enrolling fingerprint with ID "); Serial.println(id);

  // First capture
  int p = -1;
  Serial.println("Place your finger on the sensor...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Imaging error. Try again.");
      return;
    }
  }
  Serial.println("Image taken successfully!");

  // Convert the image to a template
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image. Try again.");
    return;
  }

  Serial.println("Remove your finger and place it again...");
  delay(2000);

  // Second capture
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Imaging error. Try again.");
      return;
    }
  }
  Serial.println("Second image taken successfully!");

  // Convert the second image to a template
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting image. Try again.");
    return;
  }

  // Create the model and store it in the specified ID slot
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Error creating fingerprint model. Try again.");
    return;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint successfully stored!");
  } else {
    Serial.println("Error storing fingerprint. Try again.");
  }
}

// Function to detect fingerprints with a 5-second timeout
void detectFingerprint() {
  Serial.println("Place your finger on the sensor. Waiting for 5 seconds...");
  int timeout = 5000; // Timeout period in milliseconds
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    int id = getFingerprintID(); // Try to capture the fingerprint
    if (id != -1) { // If a valid fingerprint is detected
      Serial.print("Access Granted. Found Fingerprint ID #");
      Serial.println(id);
      return;
    }
  }

  Serial.println("No valid fingerprint detected within 5 seconds.");
}

// Function to capture and identify a fingerprint
int getFingerprintID() {
  uint8_t p = finger.getImage();

  // Handle the response from the sensor
  if (p == FINGERPRINT_NOFINGER) {
    return -1; // No finger detected
  } else if (p == FINGERPRINT_IMAGEFAIL) {
    Serial.println("Imaging error.");
    return -1;
  } else if (p == FINGERPRINT_OK) {
    Serial.println("Image taken successfully.");
  } else {
    Serial.println("Unknown error while capturing fingerprint.");
    return -1;
  }

  // Convert the image to a fingerprint template
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to convert image.");
    return -1;
  }

  // Search for a match in the database
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    // A match was found
    Serial.print("Fingerprint matched! ID: "); Serial.println(finger.fingerID);
    Serial.print("Confidence: "); Serial.println(finger.confidence);
    return finger.fingerID;
  } else if (p == FINGERPRINT_NOTFOUND) {
    return -1; // No matching fingerprint found
  } else {
    Serial.println("Error during fingerprint search.");
    return -1;
  }
}
