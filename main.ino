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

#define HOME_TIMER_INTERVAL 60000L             // 1 minute
#define MOISTURE_TIMER_INTERVAL 60000L         // 1 minute
#define CRITICAL_MOISTURE_TIMER_INTERVAL 1000L // 1 second

#define MIN_TEMP_THRESHOLD 26
#define MID_TEMP_THRESHOLD 28
#define MAX_TEMP_THRESHOLD 30

#define DRY_SOIL_VALUE 4095 // Below this percentage, soil is considered dry
#define WET_SOIL_VALUE 1500 // Above this percentage, soil is considered wet
#define MIN_MOISTURE_THRESHOLD 30
#define MAX_MOISTURE_THRESHOLD 90

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
  Serial.println("[INFO]: Controlling Fan...");
  if (!isHome)
  {
    Serial.println("[INFO]: Home is Locked. Turning Fan OFF...");
    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, LOW);
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    Serial.println("[INFO]: Updating Blynk V5 (Fan) to 0...");
    Blynk.virtualWrite(V5, 0);
    Serial.println("[INFO]: Fan OFF!");
    return;
  }

  if (isManualMode)
  {
    Serial.println("[WARN]: Fan is in Manual Mode. Skipping automatic control...");
    return;
  }

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  if (temp < MIN_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 0);
    isFanOn = false;
    Blynk.virtualWrite(V5, 0);
    Serial.println("[INFO]: Fan Speed: 0%");
  }
  else if (temp >= MIN_TEMP_THRESHOLD && temp <= MID_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 128);
    isFanOn = true;
    Serial.println("[INFO]: Updating Blynk V5 (Fan) to 1");
    Blynk.virtualWrite(V5, 1);
    Serial.println("[INFO]: Fan Speed: 50%");
  }
  else if (temp > MID_TEMP_THRESHOLD && temp <= MAX_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 192);
    isFanOn = true;
    Serial.println("[INFO]: Updating Blynk V5 (Fan) to 1");
    Blynk.virtualWrite(V5, 1);
    Serial.println("[INFO]: Fan Speed: 75%");
  }
  else if (temp > MAX_TEMP_THRESHOLD)
  {
    ledcWrite(FAN_ENA, 255);
    isFanOn = true;
    Serial.println("[INFO]: Updating Blynk V5 (Fan) to 1");
    Blynk.virtualWrite(V5, 1);
    Serial.println("[INFO]: Fan Speed: 100%");
  }
}

// --- TIMER 1: Home Condition (Temperature / Humidity Status) ---

void checkHome()
{
  Serial.println("[INFO]: Checking Home's Temperature & Humidity...");

  Serial.println("[INFO]: Reading DHT11...");

  int humidity = dht.readHumidity();
  int temperature = dht.readTemperature();

  Serial.print("[INFO]: Temp: ");
  Serial.print(temperature);
  Serial.print("°C | Hum: ");
  Serial.print(humidity);
  Serial.println("%");

  Serial.println("[INFO]: Writing to Blynk V0 (Temperature), V1 (Humidity)...");

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);

  Serial.println("[INFO]: Sending data to controlFan()...");
  controlFan(temperature);

  Serial.println("[INFO]: Check Home Condition Ended.");
}

// --- TIMER 2: MOISTURE & PUMP LOGIC ---
// Critical (Pump ON): Every 1 SECOND
void checkMoisture()
{
  Serial.println("[INFO]: Checking Soil Moisture...");

  Serial.println("[INFO]: Reading Moisture Sensor...");
  int rawValue = analogRead(M_SENSOR_PIN);

  int moisturePercent = map(rawValue, DRY_SOIL_VALUE, WET_SOIL_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  Serial.print("[INFO]: Soil Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  Serial.println("[INFO]: Writing to Blynk V3 (Soil Moisture)...");
  Blynk.virtualWrite(V3, moisturePercent);

  if (moisturePercent < MIN_MOISTURE_THRESHOLD && !isPumpOn)
  {
    digitalWrite(PUMP_IN3, HIGH);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 255);

    isPumpOn = true;
    Serial.println("[INFO]: Turning Water Pump ON...");
    timer.changeInterval(moistureTimerID, CRITICAL_MOISTURE_TIMER_INTERVAL);
  }

  else if (moisturePercent > MAX_MOISTURE_THRESHOLD && isPumpOn)
  {
    digitalWrite(PUMP_IN3, LOW);
    digitalWrite(PUMP_IN4, LOW);
    ledcWrite(PUMP_ENB, 0);
    
    isPumpOn = false;
    Serial.println("[INFO]: Turning Water Pump OFF...");
    timer.changeInterval(moistureTimerID, MOISTURE_TIMER_INTERVAL);
  }
  Serial.println("[INFO]: Check Moisture Ended.");
}

// Check RFID Card
void checkRFID()
{

  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.println("[INFO]: Checking RFID Card...");

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
    Serial.println("[INFO]: ✓ Access Granted!");
    Serial.print("==========================\n");
    Serial.print("[INFO]: Home Status: ");
    Serial.println(isHome ? "UNLOCKED" : "LOCKED");
    Serial.print("==========================\n");

    if (!isHome)
    {
      Blynk.virtualWrite(V4, 0);
      greenLedOnAndOff(500);
      delay(250);
      greenLedOnAndOff(500);
    }
    else
    {
      Blynk.virtualWrite(V4, 1);
      greenLedOnAndOff(500);
    }
    delay(1000);
  }
  else
  {
    Serial.println("[INFO]: ✗ Access Denied!");
    redLedOnAndOff(2000);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void setup()
{
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
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN); // SCK, MISO, MOSI
  Serial.println("✓ SPI OK");

  Serial.println("Step 5: Initializing RFID...");
  rfid.PCD_Init();
  delay(100);

  // Check if RFID is responding
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print("RFID Firmware Version: 0x");
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF)
  {
    Serial.println("✗ WARNING: RFID not detected! Check wiring (especially 3.3V and GND)");
    Serial.println("System will continue, but RFID won't work");
  }
  else
  {
    Serial.println("✓ RFID OK");
  }

  Serial.println("Step 6: Connecting to Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  Serial.println("✓ Blynk Connected!");

  Serial.println("Step 7: Setting up timers...");
  timer.setInterval(10000L, checkHome);
  moistureTimerID = timer.setInterval(10000L, checkMoisture);
  Serial.println("✓ Timers OK");

  Serial.println("\n========== BOOT COMPLETE ==========");
  Serial.println("Entering main loop...\n");
}

BLYNK_WRITE(V2)
{
  Serial.print("[INFO]: Blynk V2 (LED) Value: ");
  Serial.println(param.asInt());

  if (!isHome)
  {
    Serial.println("[INFO]: LED OFF (Not in a Home)");
    Blynk.virtualWrite(V2, 0);
    return;
  }

  int buttonState = param.asInt();
  if (buttonState == 1)
  {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[INFO]: LED ON!");
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
    Serial.println("[INFO]: LED OFF!");
  }
}

BLYNK_WRITE(V4)
{
  Serial.print("[INFO]: Blynk V4 (Home Status) Value: ");
  Serial.println(param.asInt());

  // This widget is read-only. Ignore any changes.
  Blynk.virtualWrite(V4, isHome ? 1 : 0);
}

BLYNK_WRITE(V5)
{
  Serial.print("[INFO]: Blynk V5 (Fan) Value: ");
  Serial.println(param.asInt());

  if (!isHome)
  {
    Serial.println("[INFO]: FAN OFF (Not in a Home)");
    Blynk.virtualWrite(V5, 0);
    return;
  }

  if (!isManualMode)
  {
    Serial.println("[WARN]: Fan is not in Manual Mode!");
    return;
  }

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);

  int buttonState = param.asInt();
  if (buttonState == 1)
  {
    isFanOn = true;
    ledcWrite(FAN_ENA, 255);
    Serial.println("[INFO]: FAN ON!");
  }
  else
  {
    isFanOn = false;
    ledcWrite(FAN_ENA, 0);
    Serial.println("[INFO]: FAN OFF!");
  }
}

BLYNK_WRITE(V6)
{
  Serial.print("[INFO]: Blynk V6 (Manual Mode) Value: ");
  Serial.println(param.asInt());

  int buttonState = param.asInt();
  if (buttonState == 1)
  {
    isManualMode = true;
  }
  else
  {
    isManualMode = false;
  }
}

BLYNK_WRITE(V7)
{

  if (!isHome)
  {
    Serial.println("[INFO]: FAN OFF (Not in a Home)");
    Blynk.virtualWrite(V5, 0);
    return;
  }

  if (!isFanOn)
  {
    Serial.println("[WARN]: Fan is not ON!");
    return;
  }

  if (isManualMode)
  {
    int fanSpeed = param.asInt();
    Serial.print("[INFO]: Blynk Speed: ");
    Serial.println(fanSpeed);
    int speed = (fanSpeed * 255) / 100;

    ledcWrite(FAN_ENA, speed);
    Serial.print("[INFO]: Fan running on speed: ");
    Serial.print(speed);
    Serial.println("%");
  }
}

void loop()
{
  Blynk.run();
  timer.run();
  checkRFID();
}
