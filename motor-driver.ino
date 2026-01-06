// // L298N pins
#define IN1 26
#define IN2 25
#define ENA 27  // PWM pin

// #define IN3 32
// #define IN4 33
// #define ENB 34  // PWM pin

// PWM settings
#define PWM_CHANNEL_A 0
// #define PWM_CHANNEL_B 1
#define PWM_FREQ 1000
#define PWM_RES 8  // 0–255

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  // pinMode(IN3, OUTPUT);
  // pinMode(IN4, OUTPUT);

  // Setup PWM
  ledcAttach(PWM_CHANNEL_A, PWM_FREQ, PWM_RES);
  // ledcAttach(PWM_CHANNEL_B, PWM_FREQ, PWM_RES);

  // Stop motors
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  // digitalWrite(IN3, LOW);
  // digitalWrite(IN4, LOW);

  ledcWrite(ENA, 0);
  // ledcWrite(ENB, 0);
}

void loop() {
  
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  // digitalWrite(IN3, HIGH);
  // digitalWrite(IN4, LOW);

  ledcWrite(PWM_CHANNEL_A, 180);  // Motor A speed
  // ledcWrite(PWM_CHANNEL_B, 220);  // Motor B speed

  delay(3000);

  // Stop
  ledcWrite(PWM_CHANNEL_A, 0);
  // ledcWrite(PWM_CHANNEL_B, 0);
  delay(2000);
}
