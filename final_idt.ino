/*
 * Project: Smart Home & Garden with RFID Security (Water Saving Mode)
 * Board: ESP32 Dev Module
 *
 * REQUIRED LIBRARIES:
 * 1. Blynk by Volodymyr Shymanskyy
 * 2. DHT11 by DFRobot
 * 3. MFRC522 by Miguel Balboa
 */

#define BLYNK_TEMPLATE_ID "TMPL3mb8grOS1"
#define BLYNK_TEMPLATE_NAME "Home Automation and Smart Garden"
#define BLYNK_AUTH_TOKEN "Our8GSqOFmjdBcpFKVLX3Swg4W5_71hb"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>

// --- WIFI CREDENTIALS ---
#define WIFI_SSID "Chinnu0"
#define WIFI_PASSWORD "Chinnu@7"

// --- PIN DEFINITIONS ---
// Sensors & LED
#define DHT11_PIN 4
#define M_SENSOR_PIN 34
#define LED_PIN 2
#define RFID_LED_PIN 15
#define DHTTYPE DHT11

// RFID Pins (SPI)
#define SS_PIN 21   // SDA
#define RST_PIN 22  // RST

// Motor A (Fan) - L298N
#define FAN_IN1 26
#define FAN_IN2 27
#define FAN_ENA 14

// Motor B (Water Pump) - L298N
#define PUMP_IN3 32
#define PUMP_IN4 33
#define PUMP_ENB 25

// PWM Settings
#define PWM_FREQ 1000
#define PWM_RES 8

// Objects
DHT dht(DHT11_PIN, DHTTYPE);
BlynkTimer timer;
MFRC522 rfid(SS_PIN, RST_PIN);

// Global Variables
bool isHome = false;
bool isPumpOn = false;                // Tracks if pump is currently running
int moistureTimerID;                  // ID to control moisture timer interval
String masterKeyUID = "A3 32 01 2D";  // REPLACE WITH YOUR UID

// --- FUNCTIONS ---

void controlFan(int temp) {
  Serial.println(">>> controlFan() called");
  if (!isHome) {
    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, LOW);
    ledcWrite(FAN_ENA, 0);
    Serial.println("    Fan OFF (Not Home)");
    return;
  }

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  if (temp < 20) {
    ledcWrite(FAN_ENA, 0);
    Serial.println("    Fan Speed: OFF");
  } else if (temp >= 20 && temp <= 25) {
    ledcWrite(FAN_ENA, 102);  // 40%
    Serial.println("    Fan Speed: 40%");
  } else if (temp > 25 && temp <= 30) {
    ledcWrite(FAN_ENA, 178);  // 70%
    Serial.println("    Fan Speed: 70%");
  } else if (temp > 30) {
    ledcWrite(FAN_ENA, 255);  // 100%
    Serial.println("    Fan Speed: 100%");
  }
}

// --- TIMER 1: ENVIRONMENT (Temp/Hum/Status) ---
// Runs every 1 MINUTE
void checkEnvironment() {
  Serial.println("\n=== checkEnvironment() START ===");

  Serial.println("Reading DHT11...");
  int humidity = dht.readHumidity();
  int temperature = dht.readTemperature();

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print("C | Hum: ");
  Serial.println(humidity);

  Serial.println("Writing to Blynk V0, V1...");

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);

  controlFan(temperature);

  Serial.println("Updating Status...");
  // Update Status String
  if (isHome) {
    Blynk.virtualWrite(V4, "Status: HOME (Unlocked)");
    digitalWrite(RFID_LED_PIN, HIGH);
    delay(500);
    digitalWrite(RFID_LED_PIN, LOW);
  } else {
    Blynk.virtualWrite(V4, "Status: AWAY (Locked)");
    digitalWrite(RFID_LED_PIN, HIGH);
    delay(500);
    digitalWrite(RFID_LED_PIN, LOW);
    delay(250);
    digitalWrite(RFID_LED_PIN, HIGH);
    delay(500);
    digitalWrite(RFID_LED_PIN, LOW);
  }
  Serial.println("=== checkEnvironment() END ===\n");
}

// --- TIMER 2: MOISTURE & PUMP LOGIC ---
// Standard: Every 1 MINUTE
// Critical (Pump ON): Every 1 SECOND
void checkMoisture() {
  Serial.println("\n--- checkMoisture() START ---");

  Serial.println("Reading Analog Pin...");
  int rawValue = analogRead(M_SENSOR_PIN);
  Serial.println("Reading Analog Pin...");

  int moisturePercent = map(rawValue, 4095, 1500, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  Serial.print("[Soil] Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");
  Blynk.virtualWrite(V3, moisturePercent);

  // PUMP CONTROL LOGIC
  // 1. Turn ON if dry (< 30%) AND Pump is currently OFF
  if (moisturePercent < 30 && !isPumpOn) {
    Serial.println(">>> DRY SOIL: Pump ON (1s update)");

    //   // Activate Pump
    digitalWrite(PUMP_IN3, HIGH);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 255);

    isPumpOn = true;

    //   // CHANGE TIMER TO 1 SECOND for precise shutoff
    timer.changeInterval(moistureTimerID, 1000L);
  }

  // // 2. Turn OFF if wet (> 90%) AND Pump is currently ON
  else if (moisturePercent > 90 && isPumpOn) {
    Serial.println(">>> WET SOIL: Pump OFF (60s update)");

    //   // Stop Pump
    digitalWrite(PUMP_IN3, LOW);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 0);

    isPumpOn = false;

    //   // REVERT TIMER TO 1 MINUTE to save resources
    timer.changeInterval(moistureTimerID, 10000L);
    // }
    Serial.println("--- checkMoisture() END ---\n");
  }
}

// Check RFID Card
void checkRFID() {

  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.println("\n*** RFID DETECTED ***");

  String content = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    content.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(rfid.uid.uidByte[i], HEX));
  }
  content.toUpperCase();

  if (content.substring(1) == masterKeyUID) {
    isHome = !isHome;
    Serial.print("Access Granted. New State: ");
    Serial.println(isHome ? "HOME" : "AWAY");

    // Visual Feedback
    digitalWrite(RFID_LED_PIN, HIGH);
    delay(200);
    digitalWrite(RFID_LED_PIN, LOW);
    delay(100);
    if (!isHome) {
      digitalWrite(RFID_LED_PIN, HIGH);
      delay(500);
      digitalWrite(RFID_LED_PIN, LOW);
      delay(250);
      digitalWrite(RFID_LED_PIN, HIGH);
      delay(500);
      digitalWrite(RFID_LED_PIN, LOW);
    } else {
      digitalWrite(RFID_LED_PIN, HIGH);
      delay(500);
      digitalWrite(RFID_LED_PIN, LOW);
    }
    delay(1000);
  } else {
    Serial.println("✗ Access Denied");
    digitalWrite(RFID_LED_PIN, HIGH);
    delay(2000);
    digitalWrite(RFID_LED_PIN, LOW);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void setup() {
  Serial.begin(115200);

  delay(2000);
  Serial.println("\n\n========== SYSTEM BOOT ==========");

  Serial.println("Step 0: Initializing DHT11..");
  dht.begin();

  Serial.println("Step 1: Initializing Pins...");

  pinMode(LED_PIN, OUTPUT);
  pinMode(RFID_LED_PIN, OUTPUT);
  pinMode(FAN_IN1, OUTPUT);
  pinMode(FAN_IN2, OUTPUT);
  pinMode(PUMP_IN3, OUTPUT);
  pinMode(PUMP_IN4, OUTPUT);

  Serial.println("✓ Pins OK");

  Serial.println("Step 2: Attaching PWM...");

  ledcAttach(FAN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PUMP_ENB, PWM_FREQ, PWM_RES);

  Serial.println("✓ PWM OK");

  // Initial State: Motors OFF
  Serial.println("Step 3: Motors OFF...");

  digitalWrite(FAN_IN1, LOW);
  digitalWrite(FAN_IN2, LOW);
  ledcWrite(FAN_ENA, 0);
  digitalWrite(PUMP_IN3, LOW);
  digitalWrite(PUMP_IN4, LOW);
  ledcWrite(PUMP_ENB, 0);

  Serial.println("✓ Motors Reset");

  Serial.println("Step 4: Initializing SPI...");
  SPI.begin(23, 18, 19);  // SCK, MISO, MOSI
  Serial.println("✓ SPI OK");

  Serial.println("Step 5: Initializing RFID...");
  rfid.PCD_Init();
  delay(100);

  // Check if RFID is responding
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print("RFID Firmware Version: 0x");
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF) {
    Serial.println("✗ WARNING: RFID not detected! Check wiring (especially 3.3V and GND)");
    Serial.println("   System will continue, but RFID won't work");
  } else {
    Serial.println("✓ RFID OK");
  }

  Serial.println("Step 6: Connecting to Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  Serial.println("✓ Blynk Connected!");

  Serial.println("Step 7: Setting up timers...");
  timer.setInterval(10000L, checkEnvironment);
  moistureTimerID = timer.setInterval(10000L, checkMoisture);
  Serial.println("✓ Timers OK");

  Serial.println("\n========== BOOT COMPLETE ==========");
  Serial.println("Entering main loop...\n");
}

BLYNK_WRITE(V2) {
  Serial.print("Blynk V2 Write: ");
  Serial.println(param.asInt());

  if (!isHome) {
    Serial.println("    LED OFF (Not Home)");
    return;
  }

  int buttonState = param.asInt();
  if (buttonState == 1) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ON!");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED OFF!");
  }
}

BLYNK_WRITE(V4) {
  Serial.print("Blynk V4 Write: ");
  Serial.println(param.asInt());

  int buttonState = param.asInt();
  if (buttonState == 1) {
    isHome = true;
  } else {
    isHome = false;
  }
}

void loop() {
  Blynk.run();
  timer.run();
  checkRFID();
}
