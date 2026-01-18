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
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_ERROR  3
#define LOG_LEVEL_DEBUG  4

#define CURRENT_LOG_LEVEL LOG_LEVEL_DEBUG   // Change to INFO for final use

#define LOG_INFO(msg)   if (CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO)  Serial.println(String("[INFO]  ") + msg)
#define LOG_WARN(msg)   if (CURRENT_LOG_LEVEL >= LOG_LEVEL_WARN)  Serial.println(String("[WARN]  ") + msg)
#define LOG_ERROR(msg)  if (CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR) Serial.println(String("[ERROR] ") + msg)
#define LOG_DEBUG(msg)  if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG) Serial.println(String("[DEBUG] ") + msg)

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

// ================= WIFI =================
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// ================= PINS =================
#define DHT11_PIN 4
#define M_SENSOR_PIN 34
#define LED_PIN 2

#define RFID_GREEN_LED_PIN 15
#define RFID_RED_LED_PIN   5
#define DHTTYPE DHT11

// RFID
#define MISO_PIN 18
#define MOSI_PIN 19
#define SS_PIN   21
#define RST_PIN  22
#define SCK_PIN  23

// L298N - Fan
#define FAN_IN1 26
#define FAN_IN2 27
#define FAN_ENA 14

// L298N - Pump
#define PUMP_IN3 32
#define PUMP_IN4 33
#define PUMP_ENB 25

// PWM
#define PWM_FREQ 1000
#define PWM_RES  8

// ================= TIMERS =================
unsigned long HOME_TIMER_INTERVAL = 60000;
unsigned long MOISTURE_TIMER_INTERVAL = 60000;
unsigned long CRITICAL_MOISTURE_TIMER_INTERVAL = 1000;

// ================= THRESHOLDS =================
int MIN_TEMP_THRESHOLD = 26;
int MID_TEMP_THRESHOLD = 28;
int MAX_TEMP_THRESHOLD = 30;

int DRY_SOIL_VALUE = 4095;
int WET_SOIL_VALUE = 1500;
int MIN_MOISTURE_THRESHOLD = 30;
int MAX_MOISTURE_THRESHOLD = 90;

// ================= OBJECTS =================
BlynkTimer timer;
DHT dht(DHT11_PIN, DHTTYPE);
MFRC522 rfid(SS_PIN, RST_PIN);

// ================= STATE =================
bool isHome = false;
bool isFanOn = false;
bool isPumpOn = false;
bool isManualMode = false;

int homeTimerID = -1;
int moistureTimerID = -1;

String masterKeyUID = "A3 32 01 2D";

// ================= HELPER FUNCTIONS =================
void redLedBlink(int delayTime)
{
  digitalWrite(RFID_RED_LED_PIN, HIGH);
  delay(delayTime);
  digitalWrite(RFID_RED_LED_PIN, LOW);
}

void greenLedBlink(int delayTime)
{
  digitalWrite(RFID_GREEN_LED_PIN, HIGH);
  delay(delayTime);
  digitalWrite(RFID_GREEN_LED_PIN, LOW);
}

// ================= FAN CONTROL =================
void controlFan(int temp)
{
  LOG_DEBUG("controlFan() invoked");
  LOG_INFO("Temperature = " + String(temp) + "C");

  if (!isHome)
  {
    LOG_WARN("Home locked → Fan forced OFF");
    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, LOW);
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    Blynk.virtualWrite(V5, 0);
    return;
  }

  if (isManualMode)
  {
    LOG_WARN("Manual mode active → Auto fan skipped");
    return;
  }

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  if (temp < MIN_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    Blynk.virtualWrite(V5, 0);
    LOG_INFO("Fan OFF (Low temp)");
  }
  else if (temp <= MID_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 128);
    isFanOn = true;
    Blynk.virtualWrite(V5, 1);
    LOG_INFO("Fan 50%");
  }
  else if (temp <= MAX_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 192);
    isFanOn = true;
    Blynk.virtualWrite(V5, 1);
    LOG_INFO("Fan 75%");
  }
  else
  {
    ledcWrite(FAN_ENA, 255);
    isFanOn = true;
    Blynk.virtualWrite(V5, 1);
    LOG_WARN("High temp → Fan 100%");
  }
}

// ================= HOME CHECK =================
void checkHome()
{
  LOG_INFO("Reading DHT11");

  int humidity = dht.readHumidity();
  int temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature))
  {
    LOG_ERROR("DHT11 read failed");
    return;
  }

  LOG_INFO("Temp: " + String(temperature) + "C | Hum: " + String(humidity) + "%");

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);

  controlFan(temperature);
}

// ================= MOISTURE CHECK =================
void checkMoisture()
{
  int raw = analogRead(M_SENSOR_PIN);
  int moisture = map(raw, DRY_SOIL_VALUE, WET_SOIL_VALUE, 0, 100);
  moisture = constrain(moisture, 0, 100);

  LOG_INFO("Soil Moisture = " + String(moisture) + "%");
  Blynk.virtualWrite(V3, moisture);

  if (moisture < MIN_MOISTURE_THRESHOLD && !isPumpOn)
  {
    LOG_WARN("Soil dry → Pump ON");
    digitalWrite(PUMP_IN3, HIGH);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 255);
    isPumpOn = true;
    timer.changeInterval(moistureTimerID, CRITICAL_MOISTURE_TIMER_INTERVAL);
  }
  else if (moisture > MAX_MOISTURE_THRESHOLD && isPumpOn)
  {
    LOG_INFO("Soil wet → Pump OFF");
    digitalWrite(PUMP_IN3, LOW);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 0);
    isPumpOn = false;
    timer.changeInterval(moistureTimerID, MOISTURE_TIMER_INTERVAL);
  }
}

// ================= RFID CHECK =================
void checkRFID()
{
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++)
  {
    uid += (rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  if (uid.substring(1) == masterKeyUID)
  {
    isHome = !isHome;
    LOG_INFO("RFID Access Granted → Home " + String(isHome ? "UNLOCKED" : "LOCKED"));
    Blynk.virtualWrite(V4, isHome ? 1 : 0);
    greenLedBlink(500);
  }
  else
  {
    LOG_ERROR("RFID Access Denied");
    redLedBlink(2000);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);
  LOG_INFO("System Booting...");

  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(RFID_GREEN_LED_PIN, OUTPUT);
  pinMode(RFID_RED_LED_PIN, OUTPUT);
  pinMode(FAN_IN1, OUTPUT);
  pinMode(FAN_IN2, OUTPUT);
  pinMode(PUMP_IN3, OUTPUT);
  pinMode(PUMP_IN4, OUTPUT);

  ledcAttach(FAN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PUMP_ENB, PWM_FREQ, PWM_RES);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  rfid.PCD_Init();

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  homeTimerID = timer.setInterval(HOME_TIMER_INTERVAL, checkHome);
  moistureTimerID = timer.setInterval(MOISTURE_TIMER_INTERVAL, checkMoisture);

  Blynk.syncAll();
  LOG_INFO("System Ready");
}

// ================= BLYNK HANDLERS =================
BLYNK_WRITE(V2)
{
  if (!isHome)
  {
    Blynk.virtualWrite(V2, 0);
    LOG_WARN("LED blocked (Home locked)");
    return;
  }
  digitalWrite(LED_PIN, param.asInt());
}

BLYNK_WRITE(V5)
{
  if (!isHome || !isManualMode)
  {
    Blynk.virtualWrite(V5, 0);
    return;
  }
  isFanOn = param.asInt();
  ledcWrite(FAN_ENA, isFanOn ? 255 : 0);
}

BLYNK_WRITE(V6)
{
  isManualMode = param.asInt();
  LOG_INFO("Manual Mode = " + String(isManualMode));
}

BLYNK_WRITE(V7)
{
  if (isManualMode && isFanOn)
  {
    int speed = (param.asInt() * 255) / 100;
    ledcWrite(FAN_ENA, speed);
    LOG_INFO("Fan speed = " + String(speed));
  }
}

// ================= LOOP =================
void loop()
{
  Blynk.run();
  timer.run();
  checkRFID();
}
