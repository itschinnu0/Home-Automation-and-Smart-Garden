#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME "Home Automation and Smart Garden"
#define BLYNK_AUTH_TOKEN ""
#define BLYNK_PRINT Serial
#define WIFI_SSID "Chinnu0"
#define WIFI_PASSWORD "Chinnu@7"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT11.h>

#define LED_PIN 12
#define M_SENSOR_PIN 18
#define DHT11_PIN 25

DHT11 dht11(DHT11_PIN);
BlynkTimer timer;

void sendSensorData() {
  int temperature = 0;
  int humidity = 0;

  int result = dht11.readTemperatureHumidity(temperature, humidity);

  if (result == 0) {
    Serial.print("Temperature:  ");
    Serial.print(temperature);
    Serial.print(" °C\tHumidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  } else {
    Serial.println(DHT11::getErrorString(result));
    return;
  }

  int rawValue = analogRead(M_SENSOR_PIN);
  int moisturePercent = map(rawValue, 4095, 2200, 0, 100);
  Serial.print("Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V3, moisturePercent);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  timer.setInterval(2000L, sendSensorData);
}

// BLYNK_WRITE(V0) {
//   int temperature = param.asInt();

// }

BLYNK_WRITE(V2) {
  int buttonState = param.asInt();
  if (buttonState == 1) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("ON!");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("OFF!");
  }
}

void loop() {
  Blynk.run();
  timer.run();
}