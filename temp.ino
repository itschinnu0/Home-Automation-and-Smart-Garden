/*
 * Project: Smart Home & Garden with RFID Security (Water Saving Mode)
 * Board: ESP32 DevKit V1
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

#define WIFI_SSID "Sarab"
#define WIFI_PASSWORD "idontknow"

#define DHT11_PIN 4
#define M_SENSOR_PIN 34
#define LED_PIN 2
#define RFID_GREEN_LED_PIN 15
#define RFID_RED_LED_PIN 5 
#define DHTTYPE DHT11

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
BlynkTimer timer;
DHT dht(DHT11_PIN, DHTTYPE);
MFRC522 rfid(SS_PIN, RST_PIN);

// Global Variables
bool isHome = false;
bool isFanOn = false;
bool isPumpOn = false;
bool isManualMode = false;
int moistureTimerID;
String masterKeyUID = "A3 32 01 2D";

// --- FUNCTIONS ---

void redLedOnAndOff(int delayTime) {
  digitalWrite(RFID_RED_LED_PIN, HIGH);
  delay(delayTime);
  digitalWrite(RFID_RED_LED_PIN, LOW);
}

void greenLedOnAndOff(int delayTime) {
  digitalWrite(RFID_GREEN_LED_PIN, HIGH);
  delay(delayTime);
  digitalWrite(RFID_GREEN_LED_PIN, LOW);
}

void controlFan(int temp) {
  Serial.println("[INFO]: Control Fan started!");
  if (!isHome) {
    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, LOW);
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    Blynk.virtualWrite(V5, 0);
    Serial.println("Fan OFF");
    return;
  }

  if (isManualMode) {
    Serial.println("Manual Mode!");
    return;
  }

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  if (temp < 26) {
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    Blynk.virtualWrite(V5, 0);
    Serial.println("Fan Speed: 0%");
  } else if (temp >= 26 && temp <= 28) {
    ledcWrite(FAN_ENA, 128);
    Blynk.virtualWrite(V5, 1);
    Serial.println("Fan Speed: 50%");
  } else if (temp > 28 && temp <= 30) {
    ledcWrite(FAN_ENA, 192);
    Blynk.virtualWrite(V5, 1);
    Serial.println("Fan Speed: 75%");
  } else if (temp > 30) {
    ledcWrite(FAN_ENA, 255);
    Blynk.virtualWrite(V5, 1);
    Serial.println("Fan Speed: 100%");
  }
}

// --- TIMER 1: ENVIRONMENT (Temp/Hum/Status) ---
// Runs every 1 MINUTE
void checkEnvironment() {
  Serial.println("[INFO]: Check Environment started!");

  Serial.println("[INFO]: Reading DHT11...");

  int humidity = dht.readHumidity();
  int temperature = dht.readTemperature();

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print("°C | Hum: ");
  Serial.print(humidity);
  Serial.println("%");

  Serial.println("Writing to Blynk V0, V1...");

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);

  Serial.println("Sending data to controlFan..");
  controlFan(temperature);

  Serial.println("Check Environment end");
}

// --- TIMER 2: MOISTURE & PUMP LOGIC ---
// Standard: Every 1 MINUTE
// Critical (Pump ON): Every 1 SECOND
void checkMoisture() {
  Serial.println("");

  Serial.println("Reading Analog Pin...");
  int rawValue = analogRead(M_SENSOR_PIN);

  int moisturePercent = map(rawValue, 4095, 1500, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  Serial.print("[Soil] Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  Blynk.virtualWrite(V3, moisturePercent);

  // PUMP CONTROL LOGIC
  // 1. Turn ON if dry (< 30%) AND Pump is currently OFF
  if (moisturePercent < 30 && !isPumpOn) {
    Serial.println("");

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
    Serial.println("");

    //   // Stop Pump
    digitalWrite(PUMP_IN3, LOW);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 0);

    isPumpOn = false;

    //   // REVERT TIMER TO 1 MINUTE to save resources
    timer.changeInterval(moistureTimerID, 10000L);

    Serial.println("");
  }
}

// Check RFID Card
void checkRFID() {

  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.println("");

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

    if (!isHome) {
      Blynk.virtualWrite(V4, 0);
      greenLedOnAndOff(500);
      delay(250);
      greenLedOnAndOff(500);
    } else {
      Blynk.virtualWrite(V4, 1);
      greenLedOnAndOff(500);
    }
    delay(1000);
  } else {
    Serial.println("✗ Access Denied");
    redLedOnAndOff(2000);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void setup() {
  Serial.begin(115200);

  delay(5000);
  Serial.println("========== SYSTEM BOOT ==========");

  Serial.println("Step 0: Initializing DHT11..");
  dht.begin();

  Serial.println("Step 1: Initializing Pins...");

  pinMode(LED_PIN, OUTPUT);
  pinMode(RFID_GREEN_LED_PIN, OUTPUT);
  pinMode(RFID_RED_LED_PIN, OUTPUT);
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
    Serial.println("System will continue, but RFID won't work");
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
    Serial.println("LED OFF (Not Home)");
    Blynk.virtualWrite(V2, 0);
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

BLYNK_WRITE(V5) {
  Serial.print("Blynk V5 Write: ");
  Serial.println(param.asInt());

  if (!isHome) {
    Serial.println("FAN OFF (Not Home)");
    Blynk.virtualWrite(V5, 0);
    return;
  }

  if (!isManualMode) {
    Serial.println("Fan is not manual mode!");
    return;
  } 

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  int buttonState = param.asInt();
  if (buttonState == 1) {
    isFanOn = true;
    ledcWrite(FAN_ENA, 255);
    Serial.println("FAN ON!");
  } else {
    isFanOn = false;
    ledcWrite(FAN_ENA, 0);
    Serial.println("FAN OFF!");
  }
}

BLYNK_WRITE(V6) {
  Serial.print("Blynk V6 Write: ");
  Serial.println(param.asInt());

  int buttonState = param.asInt();
  if (buttonState == 1) {
    isManualMode = true;
  } else {
    isManualMode = false;
  }
}

BLYNK_WRITE(V7) {

  if (!isHome) {
    Serial.println("FAN OFF (Not Home)");
    Blynk.virtualWrite(V5, 0);
    return;
  }

  if (!isFanOn) {
    Serial.println("Fan is not on!");
    return;
  }

  if (isManualMode) {
    int fanSpeed = param.asInt();
    Serial.print("Blynk Speed: ");
    Serial.println(fanSpeed);
    int speed = (fanSpeed * 255) / 100;;
    Serial.print("Speed: ");
    Serial.println(speed);

    ledcWrite(FAN_ENA, speed);
    Serial.print("Fan running on ");
    Serial.println(speed);
  }
}

void loop() {
  Blynk.run();
  timer.run();
  checkRFID();
}
