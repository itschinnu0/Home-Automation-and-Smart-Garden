#include "BluetoothSerial.h"

#define ledPin 15

char data = 0;
const char *pin = "2070";
String device_name = "Chinnu0-ESP32";
BluetoothSerial SerialBT;

/**
 * @brief Initializes the Bluetooth LED control system and establishes serial communication.
 * 
 * This function performs the following initialization tasks:
 * - Configures the LED pin as an output and sets it to HIGH state
 * - Initializes serial communication at 115200 baud rate for debugging
 * - Starts Bluetooth Serial communication with the device name
 * - Optionally sets a PIN for Bluetooth pairing if USE_PIN macro is defined
 * 
 * @note This function should be called once during Arduino startup
 * @note The device becomes discoverable and ready for Bluetooth pairing after this runs
 * @note If USE_PIN is defined, the Bluetooth connection will require PIN authentication
 * 
 * @see pinMode()
 * @see digitalWrite()
 * @see Serial.begin()
 * @see SerialBT.begin()
 * @see SerialBT.setPin()
 */
void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  Serial.begin(115200);
  SerialBT.begin(device_name);
  Serial.printf("The device with name %s is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());

#ifdef USE_PIN 
  SerialBT.setPin(pin); 
  Serial.println("Using PIN");
#endif
}

void loop() {
  if (SerialBT.available()) {
    data = SerialBT.read();
    Serial.println(data);

    if (data == '0') {
      digitalWrite(ledPin, LOW);
      Serial.println("LED: OFF");
    } else if (data == '1') {
      digitalWrite(ledPin, HIGH);
      Serial.println("LED: ON");
    }
  }
}