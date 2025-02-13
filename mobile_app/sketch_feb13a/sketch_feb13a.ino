#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ***** Pin Definitions *****
#define ESP_RX 2   // Arduino RX (ESP-01 TX)
#define ESP_TX 3   // Arduino TX (ESP-01 RX)
#define SERVO_PIN 9  // Servo control pin

// ***** WiFi Credentials *****
#define WIFI_SSID "linn"
#define WIFI_PASSWORD "1234linn"
#define SERVER_PORT "80"  // ESP-01 Web Server Port

// ***** Initialize Components *****
SoftwareSerial espSerial(ESP_RX, ESP_TX);  // Software Serial for ESP-01
LiquidCrystal_I2C lcd(0x27, 16, 2);    // LCD pin setup
Servo servo;

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

// ***** Setup Function *****
void setup() {
    Serial.begin(9600);
    espSerial.begin(9600);  // Default ESP-01 Baud Rate
  
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Starting...");

    servo.attach(SERVO_PIN);
    servo.write(servoPosition);

    connectToWiFi();
    startWebServer();
}

// ***** Loop Function (Fixed to Handle Fragmented HTTP Requests) *****
void loop() {
    if (espSerial.available()) {
        String request = "";
        unsigned long startTime = millis();
        
        // Wait until the full request is received (max wait: 2 seconds)
        while (millis() - startTime < 2000) {
            while (espSerial.available()) {
                char c = espSerial.read();
                request += c;
            }
            if (request.indexOf("HTTP/1.1") != -1) { // End of HTTP request
                break;
            }
        }
        
        if (request.length() > 0) {
            processHttpRequest(request);
        }
    }
}