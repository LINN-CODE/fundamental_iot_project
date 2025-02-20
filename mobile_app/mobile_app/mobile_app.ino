#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>

// Pin Definitions
#define ESP_RX 2   
#define ESP_TX 3   
#define SERVO_PIN 9  
#define BUZZER_PIN 8 
#define TRIG_PIN 6
#define ECHO_PIN 7
#define RX_PIN 4
#define TX_PIN 5

// WiFi
#define WIFI_SSID "linn"
#define WIFI_PASSWORD "1234linn"
#define SERVER_PORT "80"

// Objects
SoftwareSerial espSerial(ESP_RX, ESP_TX);  
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo;
SoftwareSerial mySerial(RX_PIN, TX_PIN);
Adafruit_Fingerprint finger(&mySerial);

int servoPosition = 0;

// Send AT Command to ESP-01
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

// Connect ESP-01 to WiFi
void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi...");

  sendATCommand("AT+RST", 5000);
  sendATCommand("AT+CWMODE=1", 2000);

  String cmd = "AT+CWJAP=\"" + String(WIFI_SSID) + "\",\"" + String(WIFI_PASSWORD) + "\"";
  sendATCommand(cmd, 10000);

  delay(2000);

  String ipResponse = sendATCommand("AT+CIFSR", 5000);
  int ipStart = ipResponse.indexOf("STAIP,\"") + 7;
  int ipEnd = ipResponse.indexOf("\"", ipStart);
  String extractedIP = "";

  if (ipStart != -1 && ipEnd != -1) {
    extractedIP = ipResponse.substring(ipStart, ipEnd);
  }

  if (extractedIP.length() > 0) {
    Serial.println("ESP-01 IP: " + extractedIP);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP Address:");
    lcd.setCursor(0, 1);
    lcd.print(extractedIP);
  } else {
    Serial.println("Failed to get IP!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Error!");
  }
}

// Start Web Server
void startWebServer() {
  sendATCommand("AT+CIPMUX=1", 2000);
  String resp = sendATCommand("AT+CIPSERVER=1," + String(SERVER_PORT), 2000);
  if (resp.indexOf("OK") != -1) {
    Serial.println("Web Server Started!");
  } else {
    Serial.println("Web Server Failed!");
  }
}

// Process Incoming Requests
void processHttpRequest(String request) {
  Serial.println("HTTP Request: " + request);
  espSerial.listen();

  if (request.indexOf("/control_servo?position=90") != -1) {
    mySerial.listen();
    delay(500);
    servo.write(90);
    delay(3000);
    servo.write(0);
  } else if (request.indexOf("/finger_print?enroll=1") != -1) {
    mySerial.listen();
    delay(500);
    lcd.clear();
    lcd.backlight();
    lcd.print("Enroll Finger...");
    enrollFingerprint();
  }
}

// Clear Serial Buffer
void clearSerialBuffer() {
  while (Serial.available()) {
    Serial.read();
  }
}

// Ultrasonic
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

// Detect Fingerprints
void detectFingerprint() {
  lcd.clear();
  lcd.print("Waiting Finger");
  int timeout = 5000;
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

      servo.write(90);
      delay(3000);
      servo.write(0);

      delay(3000);
      clearSerialBuffer();
      return;
    }
  }

  Serial.println("No fingerprint!");
  lcd.clear();
  lcd.print("Access Denied!");
  lcd.setCursor(0, 1);
  lcd.print("No Match");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
  delay(2000);
  clearSerialBuffer();
}

// Get Fingerprint ID
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

// Enroll Finger
void enrollFingerprint() {
  mySerial.listen();
  delay(500);

  int id = 0;
  lcd.clear();
  lcd.print("Enter ID: ");
  Serial.println("Enter ID (1-127):");
  while (Serial.available()) Serial.read();

  while (id <= 0 || id > 127) {
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      id = input.toInt();
      if (id <= 0 || id > 127) {
        Serial.println("Invalid ID. 1-127");
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
      Serial.println("Imaging error.");
      lcd.clear();
      lcd.print("Imaging Error");
      return;
    }
  }

  lcd.clear();
  lcd.print("Image OK!");
  delay(1000);
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Conversion error.");
    lcd.clear();
    lcd.print("Error");
    return;
  }

  lcd.clear();
  lcd.print("Remove Finger");
  delay(2000);

  p = -1;
  lcd.clear();
  lcd.print("Place Again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Imaging error.");
      lcd.clear();
      lcd.print("Imaging Error");
      return;
    }
  }

  lcd.clear();
  lcd.print("2nd OK");
  delay(1000);
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Conversion error.");
    lcd.clear();
    lcd.print("Error");
    return;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Model error.");
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
    Serial.println("Store error.");
    lcd.clear();
    lcd.print("Store Error");
  }
  delay(2000);
  while (Serial.available()) Serial.read();
}

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");

  servo.attach(SERVO_PIN);
  servo.write(servoPosition);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  connectToWiFi();
  startWebServer();

  mySerial.begin(57600);
  finger.begin(57600);
  delay(1000);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  espSerial.listen();
  if (espSerial.available()) {
    String request = "";
    unsigned long startTime = millis();
    while (millis() - startTime < 2000) {
      while (espSerial.available()) {
        request += (char)espSerial.read();
      }
      if (request.indexOf("HTTP/1.1") != -1) break;
    }
    if (request.length() > 0) processHttpRequest(request);
  }

  delay(1000);

  float distance = getDistance();
  Serial.println("Distance: " + String(distance) + " cm");

  if (distance > 0 && distance <= 20) {
    lcd.backlight();
    lcd.clear();
    lcd.print("Object Detected!");
    delay(1000);
    lcd.setCursor(0, 1);
    lcd.print("d: Detect");
    mySerial.listen();
    detectFingerprint();
    clearSerialBuffer();
  } else {
    lcd.clear();
    lcd.noBacklight();
  }
}