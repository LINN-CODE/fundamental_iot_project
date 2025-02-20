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

// **LCD Object**
LiquidCrystal_I2C lcd(0x27, 16, 2);

// **Servo Object**
Servo servo;

// Create SoftwareSerial object for ESP-01
SoftwareSerial espSerial(ESP01_RX, ESP01_TX);

// ***** WiFi Credentials *****
#define WIFI_SSID "linn"
#define WIFI_PASSWORD "1234linn"
#define SERVER_PORT "80"  // ESP-01 Web Server Port

String lcdMessage = "Welcome!";
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

// ***** Function: Connect ESP-01 to WiFi & Get IP Address *****
void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi...");
  
    sendATCommand("AT+RST", 5000);  // Reset ESP-01
    sendATCommand("AT+CWMODE=1", 2000);  // Set to Station Mode

    // Connect to WiFi
    String connectCmd = "AT+CWJAP=\"" + String("linn") + "\",\"" + String("1234linn") + "\"";
    sendATCommand(connectCmd, 10000);
    
    delay(2000); // Wait for connection

    // Get IP Address from ESP-01
    String ipResponse = sendATCommand("AT+CIFSR", 5000);
    
    // Extract IP from response
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
        Serial.println("❌ Failed to get IP Address!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Error!");
    }
}


// ***** Function: Start ESP-01 Web Server *****
void startWebServer() {
    sendATCommand("AT+CIPMUX=1", 2000);  // Enable Multiple Connections
    sendATCommand("AT+CIPSERVER=1," + String(SERVER_PORT), 2000);  // Start Server
}

// ***** Function: Process Incoming HTTP Requests from ESP-01 *****
void processHttpRequest(String request) {
    Serial.println("HTTP Request: " + request);

    // *Fix: Only move servo when the servo request is received*
    if (request.indexOf("/control_servo?position=") != -1) {
        int posStart = request.indexOf("=") + 1;
        int posEnd = request.indexOf(" ", posStart);
        servoPosition = request.substring(posStart, posEnd).toInt();
        servo.write(servoPosition);
        Serial.println("✅ Servo set to " + String(servoPosition));
    }

    // *Fix: Correct LCD Message Handling*
    else if (request.indexOf("/send_message?msg=") != -1) {
        int msgStart = request.indexOf("=") + 1;
        lcdMessage = request.substring(msgStart); // Extract entire message
        lcdMessage.trim(); // Remove any extra spaces

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(lcdMessage);
        Serial.println("✅ LCD updated: " + lcdMessage);
    }
}

void setup() {
  Serial.begin(9600);
  // Serial for ESP-01 communication
  espSerial.begin(9600);

  // **Initialize LCD**
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // **Initialize Ultrasonic Sensor**
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  connectToWiFi();
  startWebServer();
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
  
  if(espSerial.available()){
    String request = "";
    unsigned long startTime = millis();

    while(millis() - startTime < 2000){
      while(espSerial.available()){
        char c = espSerial.read();
        request += c;
      }
      if (request.indexOf("HTTP/1.1" != -1)){
        break;
      }
    }
    if(request.length() > 0){
      processHttpRequest(request);
    }
  }

  float distance = getDistance();

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
