#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <map>
#include "time.h"

// ================= WiFi =================
const char* ssid = "Neo!";//YOUR WIFI NAME REMAINDER:IT SUPPORT ONLY 2.4GHZ BAND//
const char* password = "neo111111";//HERE YOU NEED TO WRITE YOUR WIFI PASSWORD//

String serverName = "https://script.google.com/macros/s/AKfycbyKrZ03geZt0oNEGCtifnFSBvu8FRBAoQh-EqW_vq8cEtM/exec";//PASTE YOUR GOOGLE SHEET LINK//

// ================= RFID =================
#define SS_PIN 5
#define RST_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= Buzzer =================
#define buzzer 2

// ================= Time =================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // IST
const int daylightOffset_sec = 0;

// ================= UID Mapping =================
std::map<String, String> uidToName = {
  {"40EDDF61", "tamizharasan"},
  {"0B263806", "sundar"}
};

// ================= Entry/Exit =================
std::map<String, bool> entryState;

// ================= Setup =================
void setup() {
  Serial.begin(115200);

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("WiFi Connected");
  delay(2000);

  // Time setup
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wait for NTP sync
  Serial.print("Waiting for NTP time");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Time synced: " + String(asctime(&timeinfo)));

  // RFID (SPI)
  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();

  pinMode(buzzer, OUTPUT);

  // Initial LCD message
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scan Card");
  lcd.setCursor(0,1);
  lcd.print("                "); // clear second line
}

// ================= Get Time =================
String getTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00:00";

  char buffer[9];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

// ================= Get Date =================
String getDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00/00/00";

  char buffer[9];
  strftime(buffer, sizeof(buffer), "%d/%m/%y", &timeinfo);
  return String(buffer);
}

// ================= Loop =================
void loop() {
  // Default LCD message
  lcd.setCursor(0,0);
  lcd.print("Scan Card      ");  // pad spaces to clear old text
  lcd.setCursor(0,1);
  lcd.print("                "); // blank line

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  // Read UID
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    char buf[3];
    sprintf(buf, "%02X", rfid.uid.uidByte[i]);
    uid += buf;
  }

  Serial.println("Card Detected!");
  Serial.println("UID: " + uid);

  // Get Name
  String name = "Unknown";
  if (uidToName.count(uid)) {
    name = uidToName[uid];
  }

  // ENTRY / EXIT toggle
  bool isEntry = !entryState[uid];
  entryState[uid] = isEntry;

  String status = isEntry ? "ENTRY" : "EXIT";

  // Get date & time
  String date = getDate();
  String time = getTime();

  // Show scanned info on LCD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(status + " " + name);
  lcd.setCursor(0,1);
  lcd.print(time);

  // Buzzer
  digitalWrite(buzzer, HIGH);
  delay(200);
  digitalWrite(buzzer, LOW);

  // Send to Google Sheet
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = serverName +
                 "?name=" + name +
                 "&entry=" + status +
                 "&rfid=" + uid +
                 "&date=" + date +
                 "&time=" + time;

    http.begin(url);
    int code = http.GET();
    Serial.print("HTTP Response Code: ");
    Serial.println(code);
    http.end();
  }

  delay(2000); // show scanned info for 2 seconds

  // Reset LCD to default message
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scan Card");
  lcd.setCursor(0,1);
  lcd.print("                "); // blank line to clear previous time
}
