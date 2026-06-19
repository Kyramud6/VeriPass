#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==================== Pins ====================
#define SS_PIN 5    // SDA -> GPIO5
#define RST_PIN 2   // RST -> GPIO2
// RGB LIGHT
#define LED_R 25
#define LED_G 26
#define LED_B 27
// BUZZER
#define BUZZER_PIN 14
// OLED SCREEN
#define OLED_SDA 22
#define OLED_SCL 21
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RFID MODULE
MFRC522 rfid(SS_PIN, RST_PIN);

// WiFi settings
const char* ssid = "INTI-Guest";
const char* password = "iu@aruba2020";

// Server settings
const char* serverURL = "http://13.214.116.232:5000/check_rfid";
int room_id = 1;
String room_name = "A1-CL13 Multimedia Lab"; 

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  setColor(0, 0, 0); // All OFF

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed!");
    for(;;); // halt
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("OLED Ready!");
  display.display();
  delay(1000);

  Serial.println("Connecting WiFi...");
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi Connected!");
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("WiFi Connected!");
  display.display();
  delay(1000);
}

// ==================== Loop ====================
void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Normalize UID (uppercase hex, no spaces)
  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(rfid.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();

  Serial.print("Card UID: ");
  Serial.println(uidStr);

  // Show scanning (blue LED)
  setColor(0, 0, 255);
  delay(200);

  // Send to Flask API
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

   String postData = "{\"card_uid\":\"" + uidStr + "\", \"room_id\":\"" + room_id + "\", \"scanner_name\":\"" + room_name + "\"}";
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Server response: " + response);

      DynamicJsonDocument doc (512);
      DeserializationError error = deserializeJson (doc, response);

      if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        setColor(0, 0, 255); // Blue fallback if JSON broken
        display.clearDisplay();
        display.setCursor(0,10);
        display.println("JSON Parse Error");
        display.display();
      } else {
        String status = doc["status"].as<String>();
        String reason = doc["reason"].as<String>();
        Serial.print("Parsed status: ");
        Serial.println(status);
        Serial.print("Reason: ");
        Serial.println(reason);

        // OLED update
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("Status: " + status);
        display.setCursor(0, 15);
        display.println("Reason:");
        display.setCursor(0, 30);
        display.println(reason);
        display.display();

        // RGB + Buzzer feedback
        if (status == "granted") {
          setColor(0, 255, 0);   // Green
          beep(2, 200);
        } else if (status == "denied") {
          setColor(255, 0, 0);   // Red
          beep(1, 500);
        } else {
          setColor(0, 0, 255);   // Blue fallback
        }
      }
    } else {
      Serial.print("Error sending POST: ");
      Serial.println(httpResponseCode);
      setColor(255, 0, 0); // Error → Red
      beep(3, 200);
    }
    http.end();
  }

  delay(1000);
  setColor(0, 0, 0); // OFF
  rfid.PICC_HaltA();
}

// ==================== Helper ====================
void setColor(int red, int green, int blue) {
  digitalWrite(LED_R, red > 0 ? HIGH : LOW);
  digitalWrite(LED_G, green > 0 ? HIGH : LOW);
  digitalWrite(LED_B, blue > 0 ? HIGH : LOW);
}

// Buzzer Function
void beep(int count, int duration) {
  for (int i = 0; i < count; i++) {
    tone(BUZZER_PIN, 1000);  // 1 kHz tone
    delay(duration);
    noTone(BUZZER_PIN);
    delay(100);
  }
}
