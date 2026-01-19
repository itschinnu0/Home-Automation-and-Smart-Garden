/************************************************************
 * SMART HOME AUTOMATION USING ESP32 & BLYNK
 * Author  : Chinnu0
 * Board   : ESP32
 * Features:
 *  - RFID-based Home Lock/Unlock
 *  - Automatic & Manual Fan Control
 *  - Soil Moisture based Water Pump
 *  - DHT11 Temperature & Humidity Monitoring
 *  - Structured Logging System
 ************************************************************/

// ================= LOGGING SYSTEM =================
short isDebugEnabled = 0;

#define CURRENT_LOG_LEVEL LOG_LEVEL_DEBUG // Change to INFO for final use

#define LOG_INFO(msg) Serial.println(String("[INFO]: ") + msg)
#define LOG_WARN(msg) Serial.println(String("[WARN]: ") + msg)
#define LOG_ERROR(msg) Serial.println(String("[ERROR]: ") + msg)
#define LOG_DEBUG(msg) \
  if (isDebugEnabled)  \
  Serial.println(String("[DEBUG]: ") + msg)

// ================= BLYNK CONFIG =================
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define DHT11_PIN 4
#define M_SENSOR_PIN 34
#define LED_PIN 2
#define RFID_GREEN_LED_PIN 15
#define RFID_RED_LED_PIN 5
#define DHTTYPE DHT11

// RFID Pins
#define MISO_PIN 18 // MISO
#define MOSI_PIN 19 // MOSI
#define SS_PIN 21   // SDA
#define RST_PIN 22  // RST
#define SCK_PIN 23  // SCK

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

// ================= TIMERS (milliseconds) =================
unsigned long HOME_TIMER_INTERVAL = 60000;             // 1 minute
unsigned long MOISTURE_TIMER_INTERVAL = 60000;         // 1 minute
unsigned long CRITICAL_MOISTURE_TIMER_INTERVAL = 1000; // 1 second

// ================= TEMPERATURE THRESHOLDS =================
short MIN_TEMP_THRESHOLD = 26;
short MID_TEMP_THRESHOLD = 28;
short MAX_TEMP_THRESHOLD = 30;

// ================= SOIL MOISTURE VALUES =================
int DRY_SOIL_VALUE = 4095;
int WET_SOIL_VALUE = 1500;

short MIN_MOISTURE_THRESHOLD = 30;
short MAX_MOISTURE_THRESHOLD = 90;

// ================= OBJECTS =================
BlynkTimer timer;
DHT dht(DHT11_PIN, DHTTYPE);
MFRC522 rfid(SS_PIN, RST_PIN);

// Global Variables
bool isHome = false;
bool isFanOn = false;
bool isPumpOn = false;
bool isManualMode = false;

int homeTimerID = -1;
int moistureTimerID = -1;

String masterKeyUID = "A3 32 01 2D";

// ================= HELPER FUNCTIONS =================
void redLedOnAndOff(int delayTime)
{
  digitalWrite(RFID_RED_LED_PIN, HIGH);
  delay(delayTime);
  digitalWrite(RFID_RED_LED_PIN, LOW);
}

void greenLedOnAndOff(int delayTime)
{
  digitalWrite(RFID_GREEN_LED_PIN, HIGH);
  delay(delayTime);
  digitalWrite(RFID_GREEN_LED_PIN, LOW);
}

void controlFan(int temp)
{
  LOG_DEBUG("controlFan() invoked");
  LOG_INFO("Controlling Fan with Temperature = " + String(temp) + "C");
  if (!isHome)
  {
    LOG_WARN("Home is Locked. Turning Fan OFF...");
    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, LOW);
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    LOG_DEBUG("Updating Blynk V5 (Fan) to 0...");
    Blynk.virtualWrite(V5, 0);
    LOG_INFO("Fan OFF!");
    return;
  }

  if (isManualMode)
  {
    LOG_WARN("Fan is in Manual Mode. Skipping automatic control...");
    return;
  }

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  if (temp < MIN_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    Blynk.virtualWrite(V5, 0);
    LOG_INFO("Fan Speed: 0%");
  }
  else if (temp >= MIN_TEMP_THRESHOLD && temp <= MID_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 128);
    isFanOn = true;
    LOG_DEBUG("Updating Blynk V5 (Fan) to 1...");
    Blynk.virtualWrite(V5, 1);
    LOG_INFO("Fan Speed: 50%");
  }
  else if (temp > MID_TEMP_THRESHOLD && temp <= MAX_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 192);
    isFanOn = true;
    LOG_DEBUG("Updating Blynk V5 (Fan) to 1...");
    Blynk.virtualWrite(V5, 1);
    LOG_INFO("Fan Speed: 75%");
  }
  else if (temp > MAX_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 255);
    isFanOn = true;
    LOG_DEBUG("Updating Blynk V5 (Fan) to 1...");
    Blynk.virtualWrite(V5, 1);
    LOG_INFO("High temperature → Fan Speed: 100%");
  }

  LOG_DEBUG("controlFan() ended.");
}

// --- TIMER 1: Home Condition (Temperature / Humidity Status) ---
void checkHome()
{
  LOG_DEBUG("checkHome() invoked");

  LOG_DEBUG("Reading DHT11...");

  int humidity = dht.readHumidity();
  int temperature = dht.readTemperature();

  LOG_INFO("Temp: " + String(temperature) + "°C | Hum: " + String(humidity) + "%");
  if (isnan(humidity) || isnan(temperature))
  {
    LOG_ERROR("DHT11 read failed!");
    return;
  }
  LOG_DEBUG("Writing to Blynk V0 (Temperature), V1 (Humidity)...");

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);

  LOG_DEBUG("Sending data to controlFan()...");
  controlFan(temperature);

  LOG_DEBUG("checkHome() ended.");
}

// --- TIMER 2: MOISTURE & PUMP LOGIC ---
// Critical (Pump ON): Every 1 SECOND
void checkMoisture()
{
  LOG_DEBUG("checkMoisture() invoked");

  LOG_DEBUG("Reading Moisture Sensor...");
  int rawValue = analogRead(M_SENSOR_PIN);

  int moisturePercent = map(rawValue, DRY_SOIL_VALUE, WET_SOIL_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  LOG_INFO("Soil Moisture: " + String(moisturePercent) + "%");

  LOG_DEBUG("Writing to Blynk V3 (Soil Moisture)...");
  Blynk.virtualWrite(V3, moisturePercent);

  if (moisturePercent < MIN_MOISTURE_THRESHOLD && !isPumpOn)
  {
    digitalWrite(PUMP_IN3, HIGH);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 255);

    isPumpOn = true;
    LOG_INFO("Turning Water Pump ON...");
    timer.changeInterval(moistureTimerID, CRITICAL_MOISTURE_TIMER_INTERVAL);
  }

  else if (moisturePercent > MAX_MOISTURE_THRESHOLD && isPumpOn)
  {
    digitalWrite(PUMP_IN3, LOW);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 0);

    isPumpOn = false;
    LOG_INFO("Turning Water Pump OFF...");
    timer.changeInterval(moistureTimerID, MOISTURE_TIMER_INTERVAL);
  }
  LOG_DEBUG("checkMoisture() ended.");
}

// Check RFID Card
void checkRFID()
{

  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;

  LOG_DEBUG("checkRFID() invoked");

  String content = "";
  for (byte i = 0; i < rfid.uid.size; i++)
  {
    content.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(rfid.uid.uidByte[i], HEX));
  }
  content.toUpperCase();

  if (content.substring(1) == masterKeyUID)
  {
    isHome = !isHome;
    LOG_INFO("✓ Access Granted!");
    LOG_INFO("==========================");
    LOG_INFO("Home Status: " + String(isHome ? "UNLOCKED" : "LOCKED"));
    LOG_INFO("==========================");
    if (!isHome)
    {
      LOG_DEBUG("Updating Blynk V4 (Home Status) to 0...");
      Blynk.virtualWrite(V4, 0);
      greenLedOnAndOff(500);
      delay(250);
      greenLedOnAndOff(500);
    }
    else
    {
      LOG_DEBUG("Updating Blynk V4 (Home Status) to 1...");
      Blynk.virtualWrite(V4, 1);
      greenLedOnAndOff(500);
    }
    delay(1000);
  }
  else
  {
    LOG_INFO("✗ Access Denied!");
    redLedOnAndOff(2000);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void setup()
{
  Serial.begin(115200);
  delay(5000);
  LOG_INFO("========== SYSTEM BOOT ==========");

  LOG_DEBUG("Initializing DHT11...");
  dht.begin();

  LOG_DEBUG("Setting up Pin Modes...");
  pinMode(LED_PIN, OUTPUT);
  pinMode(RFID_GREEN_LED_PIN, OUTPUT);
  pinMode(RFID_RED_LED_PIN, OUTPUT);
  pinMode(FAN_IN1, OUTPUT);
  pinMode(FAN_IN2, OUTPUT);
  pinMode(PUMP_IN3, OUTPUT);
  pinMode(PUMP_IN4, OUTPUT);

  LOG_DEBUG("Attaching PWM Channels...");
  ledcAttach(FAN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PUMP_ENB, PWM_FREQ, PWM_RES);

  LOG_DEBUG("Setting Motors to OFF...");
  digitalWrite(FAN_IN1, LOW);
  digitalWrite(FAN_IN2, LOW);
  ledcWrite(FAN_ENA, 0);
  digitalWrite(PUMP_IN3, LOW);
  digitalWrite(PUMP_IN4, LOW);
  ledcWrite(PUMP_ENB, 0);

  LOG_DEBUG("Initializing SPI for RFID...");
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN); // SCK, MISO, MOSI

  LOG_DEBUG("Initializing RFID Reader...");
  rfid.PCD_Init();
  delay(100);

  // Check if RFID is responding
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print("RFID Firmware Version: 0x");
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF)
  {
    LOG_WARN("✗ WARNING: RFID not detected!");
    LOG_WARN("System will continue, but RFID won't work");
  }
  else
  {
    LOG_INFO("✓ RFID OK");
  }


  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  homeTimerID = timer.setInterval(HOME_TIMER_INTERVAL, checkHome);
  moistureTimerID = timer.setInterval(MOISTURE_TIMER_INTERVAL, checkMoisture);

  Blynk.syncAll();

  LOG_DEBUG("Initial States to Blynk...");
  Blynk.virtualWrite(V2, 0);              // LED
  Blynk.virtualWrite(V4, isHome ? 1 : 0); // Home Status
  Blynk.virtualWrite(V5, isFanOn ? 1 : 0);              // Fan
  Blynk.virtualWrite(V6, isManualMode ? 1 : 0);              // Manual Mode

  LOG_INFO("========== BOOT COMPLETE ==========");
}

BLYNK_WRITE(V2)
{
  LOG_DEBUG("Blynk V2 (LED) Value: " + String(param.asInt()));

  if (!isHome)
  {
    LOG_WARN("LED OFF (Not in a Home)");
    LOG_DEBUG("Updating Blynk V2 (LED) to 0...");
    Blynk.virtualWrite(V2, 0);
    return;
  }

  int buttonState = param.asInt();
  if (buttonState == 1)
  {
    digitalWrite(LED_PIN, HIGH);
    LOG_INFO("LED ON!");
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
    LOG_INFO("LED OFF!");
  }
}

// BLYNK_WRITE(V4)
// {
//   LOG_DEBUG("Blynk V4 (Home Status) Value: " + String(param.asInt()));
//   // This widget is read-only. Ignore any changes.
//   LOG_DEBUG("Updating Blynk V4 (Home Status) to " + String(isHome ? 1 : 0) + "...");
//   Blynk.virtualWrite(V4, isHome ? 1 : 0);
// }

BLYNK_WRITE(V5)
{
  LOG_DEBUG("Blynk V5 (Fan) Value: " + String(param.asInt()));

  if (!isHome)
  {
    LOG_INFO("FAN OFF (Not in a Home)");
    LOG_DEBUG("Updating Blynk V5 (Fan) to 0...");
    Blynk.virtualWrite(V5, 0);
    return;
  }

  if (!isManualMode)
  {
    LOG_WARN("Fan is not in Manual Mode!");
    return;
  }

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  int buttonState = param.asInt();
  if (buttonState == 1)
  {
    isFanOn = true;
    ledcWrite(FAN_ENA, 255);
    LOG_INFO("FAN ON!");
  }
  else
  {
    isFanOn = false;
    ledcWrite(FAN_ENA, 0);
    LOG_INFO("FAN OFF!");
  }
}

BLYNK_WRITE(V6)
{
  LOG_DEBUG("Blynk V6 (Manual Mode) Value: " + String(param.asInt()));

  int buttonState = param.asInt();
  if (buttonState == 1)
  {
    isManualMode = true;
    LOG_INFO("Manual Mode ENABLED");
  }
  else
  {
    isManualMode = false;
    LOG_INFO("Manual Mode DISABLED");
  }
}

BLYNK_WRITE(V7)
{

  if (!isHome)
  {
    LOG_INFO("FAN OFF (Not in a Home)");
    LOG_DEBUG("Updating Blynk V5 (Fan) to 0...");
    Blynk.virtualWrite(V5, 0);
    return;
  }

  if (!isFanOn)
  {
    LOG_WARN("Fan is not ON!");
    return;
  }

  if (isManualMode)
  {
    int fanSpeed = param.asInt();
    LOG_DEBUG("Blynk Speed: " + String(fanSpeed));
    int speed = (fanSpeed * 255) / 100;

    ledcWrite(FAN_ENA, speed);
    LOG_DEBUG("PWM Value: " + String(speed));
    LOG_INFO("Fan Speed set to " + String(fanSpeed) + "%");
  }
}

BLYNK_WRITE(V10)
{
  MIN_TEMP_THRESHOLD = param.asInt();
  LOG_INFO("MIN_TEMP_THRESHOLD set to " + String(MIN_TEMP_THRESHOLD));
}

BLYNK_WRITE(V11)
{
  MID_TEMP_THRESHOLD = param.asInt();
  LOG_INFO("MID_TEMP_THRESHOLD set to " + String(MID_TEMP_THRESHOLD));
}

BLYNK_WRITE(V12)
{
  MAX_TEMP_THRESHOLD = param.asInt();
  LOG_INFO("MAX_TEMP_THRESHOLD set to " + String(MAX_TEMP_THRESHOLD));
}

BLYNK_WRITE(V13)
{
  DRY_SOIL_VALUE = param.asInt();
  LOG_INFO("DRY_SOIL_VALUE set to " + String(DRY_SOIL_VALUE));
}

BLYNK_WRITE(V14)
{
  WET_SOIL_VALUE = param.asInt();
  LOG_INFO("WET_SOIL_VALUE set to " + String(WET_SOIL_VALUE));
}

BLYNK_WRITE(V15)
{
  MIN_MOISTURE_THRESHOLD = param.asInt();
  LOG_INFO("MIN_MOISTURE_THRESHOLD set to " + String(MIN_MOISTURE_THRESHOLD));
}

BLYNK_WRITE(V16)
{
  MAX_MOISTURE_THRESHOLD = param.asInt();
  LOG_INFO("MAX_MOISTURE_THRESHOLD set to " + String(MAX_MOISTURE_THRESHOLD));
}

BLYNK_WRITE(V17)
{
  HOME_TIMER_INTERVAL = param.asLong() * 1000;

  if (homeTimerID != -1)
  {
    timer.deleteTimer(homeTimerID);
  }

  homeTimerID = timer.setInterval(HOME_TIMER_INTERVAL, checkHome);

  LOG_INFO("[INFO]: Home Timer updated to: " + String(HOME_TIMER_INTERVAL));
}

BLYNK_WRITE(V18)
{
  MOISTURE_TIMER_INTERVAL = param.asLong() * 1000;

  if (moistureTimerID != -1)
  {
    timer.deleteTimer(moistureTimerID);
  }

  moistureTimerID = timer.setInterval(MOISTURE_TIMER_INTERVAL, checkMoisture);

  LOG_INFO("[INFO]: Moisture Timer updated to: " + String(MOISTURE_TIMER_INTERVAL));
}

BLYNK_WRITE(V19)
{
  CRITICAL_MOISTURE_TIMER_INTERVAL = param.asLong() * 1000;

  LOG_INFO("[INFO]: Critical Moisture Timer updated to: " + String(CRITICAL_MOISTURE_TIMER_INTERVAL));
}

void loop()
{
  Blynk.run();
  timer.run();
  checkRFID();
}
