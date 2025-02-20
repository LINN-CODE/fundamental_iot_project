#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>

// ***** Pin Definitions *****
#define ESP_RX 2   // Arduino RX (ESP-01 TX)
#define ESP_TX 3   // Arduino TX (ESP-01 RX)
#define SERVO_PIN 9  // Servo control pin
#define BUZZER_PIN 8 //Buzzer pin 

// Ultrasonic Sensor Pins
#define TRIG_PIN 6
#define ECHO_PIN 7

// Fingerprint Sensor Pins
#define RX_PIN 4 // Fingerprint TX to Arduino RX
#define TX_PIN 5 // Fingerprint RX to Arduino TX

// ***** WiFi Credentials *****
#define WIFI_SSID "linn"
#define WIFI_PASSWORD "1234linn"
#define SERVER_PORT "80"  // ESP-01 Web Server Port

// ***** Initialize Components *****
SoftwareSerial espSerial(ESP_RX, ESP_TX);  // Software Serial for ESP-01
LiquidCrystal_I2C lcd(0x27, 16, 2);    // LCD pin setup
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

// ***** Function: Connect ESP-01 to WiFi & Get IP Address *****
void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi...");
  
    sendATCommand("AT+RST", 5000);  // Reset ESP-01
    sendATCommand("AT+CWMODE=1", 2000);  // Set to Station Mode

    // Connect to WiFi
    String connectCmd = "AT+CWJAP=\"" + String(WIFI_SSID) + "\",\"" + String(WIFI_PASSWORD) + "\"";
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
        Serial.println(F("Failed to get IP Address!"));
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Error!");
    }
}

void startWebServer() {
    sendATCommand("AT+CIPMUX=1", 2000);  // Enable Multiple Connections
    String response = sendATCommand("AT+CIPSERVER=1," + String(SERVER_PORT), 2000);  // Start Server


    if (response.indexOf("OK") != -1) {
        Serial.println(F(" Web Server Started Successfully!"));
    } else {
        Serial.println(F(" Web Server Failed to Start!"));
    }
}


void processHttpRequest(String request) {
    Serial.println("HTTP Request: " + request);
    espSerial.listen(); // Ensure ESP-01 is active

    if (request.indexOf("/control_servo?position=90") != -1) {
        servo.write(90); // Open door
        delay(3000);
        servo.write(0);  // Close door
    }
    else if (request.indexOf("/finger_print?enroll=") != -1) {
        mySerial.listen();  // Switch to Fingerprint Sensor
        delay(500);

        // Extract ID from URL
        int idStart = request.indexOf("=") + 1;
        String idStr = request.substring(idStart);
        idStr.trim();  // Remove any spaces

        int enrollID = idStr.toInt();  // Convert to integer

        Serial.print("Enrolling Fingerprint with ID: ");
        Serial.println(enrollID);

        lcd.clear();
        lcd.print("Enrolling ID: ");
        lcd.setCursor(0, 1);
        lcd.print(enrollID);

        enrollFingerprint(enrollID);  // Call function with ID
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

    // Fix Ultrasonic Sensor Initialization
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    connectToWiFi();
    startWebServer();

    mySerial.begin(57600);
    finger.begin(57600);
    delay(1000);
     // Initialize Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
    // Process Web Requests
    espSerial.listen();  // Ensure ESP-01 is active
    if (espSerial.available()) {
        String request = "";
        unsigned long startTime = millis();

        while (millis() - startTime < 2000) {
            while (espSerial.available()) {
                char c = espSerial.read();
                request += c;
            }
            if (request.indexOf("HTTP/1.1") != -1) {
                break;
            }
        }

        if (request.length() > 0) {
            processHttpRequest(request);
        }
        
    }

    delay(1000); // Allow non-blocking execution

    // Fix Ultrasonic Sensor Execution
    float distance = getDistance();
    Serial.println("Distance: " + String(distance) + " cm");

    if (distance > 0 && distance <= 20) {
        lcd.backlight();
        lcd.clear();
        lcd.print("Object Detected!");
        delay(1000);

        lcd.setCursor(0, 1);
        lcd.print("d: Detect");



        mySerial.listen(); // Switch to fingerprint sensor only when needed
        String test = detectFingerprint();
        clearSerialBuffer();
    } else {
        lcd.clear();
        lcd.noBacklight();
    }
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

String detectFingerprint() {
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

            // Unlock Door
            servo.write(90);
            delay(3000);
            servo.write(0);
            
            
            delay(2000);
            return "granted";
        }
    }

    // Unauthorized Access Detected!
    Serial.println("No fingerprint detected.");
    lcd.clear();
    lcd.print("Access Denied!");
    lcd.setCursor(0, 1);
    lcd.print("No Match");
    
    // Trigger Buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);

    

    delay(2000);
    return "denied";
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

void enrollFingerprint(int id) {
    lcd.backlight();
    mySerial.listen();  // Ensure Fingerprint Sensor is Active
    delay(500);  

    lcd.clear();
    lcd.print("Place Finger...");

    int p = -1;
    while (p != FINGERPRINT_OK) {
        p = finger.getImage();
        if (p == FINGERPRINT_NOFINGER) continue;
        if (p == FINGERPRINT_IMAGEFAIL) {
            Serial.println("Imaging Error");
            lcd.clear();
            lcd.print("Try Again");
            return;
        }
    }

    lcd.clear();
    lcd.print("Image Captured!");
    delay(1000);

    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) {
        Serial.println("Conversion Error");
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
            Serial.println("Imaging Error");
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
        Serial.println("Conversion Error");
        lcd.clear();
        lcd.print("Conversion Error");
        return;
    }

    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
        Serial.println("Model Creation Error");
        lcd.clear();
        lcd.print("Model Error");
        return;
    }

    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK) {
        Serial.println("Fingerprint Stored!");
        lcd.clear();
        lcd.print("Stored ID: ");
        lcd.print(id);
    } else {
        Serial.println("Storage Error");
        lcd.clear();
        lcd.print("Store Error!");
    }
    delay(2000);
}