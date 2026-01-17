const int sensorPin = 14;  // ADC pin for AOUT
void setup() {
  Serial.begin(115200);
}
void loop() {
  int rawValue = analogRead(sensorPin);  // Reads 0-4095
  int moisturePercent = map(rawValue, 4095, 2200, 0, 100);  // Rough percentage
  Serial.print("Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");
  delay(1000);
}
