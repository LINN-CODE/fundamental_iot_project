#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>

// ***** Pin Definitions *****
#define ESP_RX 2         // Arduino RX (ESP-01 TX)
#define ESP_TX 3         // Arduino TX (ESP-01 RX)
#define SERVO_PIN 9      // Servo control pin

// Buzzer pin 
#define BUZZER_PIN 8     

// Ultrasonic Sensor Pins
#define TRIG_PIN 6
#define ECHO_PIN 7

// Fingerprint Sensor Pins
#define RX_PIN 4         // Fingerprint TX to Arduino RX
#define TX_PIN 5         // Fingerprint RX to Arduino TX

// ***** WiFi Credentials *****
#define WIFI_SSID "linn"
#define WIFI_PASSWORD "1234linn"

// ***** Initialize Components *****
SoftwareSerial espSerial(ESP_RX, ESP_TX);  // Software Serial for ESP-01
LiquidCrystal_I2C lcd(0x27, 16, 2);        // LCD I2C address 0x27, 16 cols x 2 rows
Servo servo;

// Create SoftwareSerial object for the fingerprint sensor
SoftwareSerial mySerial(RX_PIN, TX_PIN);
Adafruit_Fingerprint finger(&mySerial);

int servoPosition = 0;

// ***** Function: Send AT Command to ESP-01 & Read Response *****
String sendATCommand(String command, int delayMs) {
  espSerial.println(command);
  delay(delayMs);
  
  String response = "";
  while (espSerial.available()) {
    char c = espSerial.read();
    response += c;
  }
  Serial.println("AT Response: " + response);
  return response;
}

// ***** Function: Connect ESP-01 to WiFi *****
void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi...");
  
  sendATCommand("AT+RST", 5000);      // Reset ESP-01
  sendATCommand("AT+CWMODE=1", 2000); // Set to Station Mode

  // Connect to WiFi
  String connectCmd = "AT+CWJAP=\"" + String(WIFI_SSID) + "\",\"" + String(WIFI_PASSWORD) + "\"";
  sendATCommand(connectCmd, 10000);
  
  delay(2000); // Wait for connection

  // Get IP Address from ESP-01 (optional display)
  String ipResponse = sendATCommand("AT+CIFSR", 5000);
  int ipStart = ipResponse.indexOf("STAIP,\"") + 7;  // Look for "STAIP,\""
  int ipEnd = ipResponse.indexOf("\"", ipStart);
  String extractedIP = "";

  if (ipStart != -1 && ipEnd != -1) {
    extractedIP = ipResponse.substring(ipStart, ipEnd);
  }

  if (extractedIP.length() > 0) {
    Serial.println("ESP-01 IP Address: " + extractedIP);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP Address:");
    lcd.setCursor(0, 1);
    lcd.print(extractedIP);
  } else {
    Serial.println(F("Failed to get IP Address!"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Error!");
  }
}

// ***** Function: Update ThingSpeak *****

void updateThingSpeak(String field1, String field2) {
  
  espSerial.listen();  // Ensure ESP-01 is active
  String cmd = "AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80"; // Start TCP connection
  sendATCommand(cmd, 2000);

  // Build HTTP request
  String httpRequest = 
  "GET /update?api_key=F1U0QCD37NVRKIQ4&field1=Unauthorized&field2=Wrong%20Fingerprint%20Access HTTP/1.1\r\n"
  "Host: api.thingspeak.com\r\n"
  "Connection: close\r\n\r\n";

  // Send the request
  cmd = "AT+CIPSEND=" + String(httpRequest.length());
  sendATCommand(cmd, 1000);
  sendATCommand(httpRequest, 2000);

  // Close the connection
  sendATCommand("AT+CIPCLOSE", 1000);
}

// Function to Clear Serial Buffer
void clearSerialBuffer() {
  while (Serial.available()) {
    Serial.read();
  }
}

// Ultrasonic Sensor Function
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

// ***** Function: Detect Fingerprints *****
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

      // Turn the Servo Motor (open door)
      servo.write(90); // Open door
      delay(3000);
      servo.write(0);  // Close door

      clearSerialBuffer();
      return;
    }
  }

  // If no valid fingerprint was detected:
  Serial.println("No fingerprint detected.");
  lcd.clear();
  lcd.print("Access Denied!");
  lcd.setCursor(0, 1);
  lcd.print("No Match");


  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
  updateThingSpeak("Unauthorized", "Wrong Fingerprint Access");

  delay(2000);
  clearSerialBuffer();
}

// Function to Capture and Identify a Fingerprint
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

// (Optional) Enroll Fingerprint Function
void enrollFingerprint() {
  mySerial.listen();  // Ensure Fingerprint Sensor is Active
  delay(500);         // Small delay to stabilize serial switching

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
      String input = Serial.readStringUntil('\n'); 
      id = input.toInt(); 
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

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Error converting 2nd image.");
    lcd.clear();
    lcd.print("Conversion Error");
    return;
  }

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

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);  // Default ESP-01 Baud Rate
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  
  servo.attach(SERVO_PIN);
  servo.write(servoPosition);

  // Initialize Ultrasonic Sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Connect to WiFi (needed for ThingSpeak updates)
  connectToWiFi();

  mySerial.begin(57600);
  finger.begin(57600);
  delay(1000);
  
  // Initialize Buzzer (commented out usage in detectFingerprint)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  // Check ultrasonic sensor for object detection
  float distance = getDistance();
  Serial.println("Distance: " + String(distance) + " cm");

  if (distance > 0 && distance <= 20) {
    lcd.backlight();
    lcd.clear();
    lcd.print("Object Detected!");
    delay(1000);

    lcd.setCursor(0, 1);
    lcd.print("d: Detect");

    mySerial.listen(); // Switch to fingerprint sensor when needed
    detectFingerprint();

    clearSerialBuffer();
  } else {
    lcd.clear();
    lcd.noBacklight();
  }

  delay(1000); // Allow non-blocking execution
}