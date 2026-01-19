# 📲 Home Automation and Smart Garden

## ESP32 | Blynk | RFID | Sensors | IoT

### Overview

Smart Home Automation using ESP32 — combining access control, fan control, water pump automation, and environmental monitoring with real-time mobile dashboard control via the Blynk IoT platform.

This project transforms a regular home and garden setup into an intelligent, IoT-controlled system.

## ✨ Key Features

- ✔ RFID-based home lock/unlock
- ✔ Automatic & Manual Fan control
- ✔ Soil Moisture based Water Pump
- ✔ Temperature & Humidity monitoring (DHT11)
- ✔ Structured logging & WiFi/Blynk status feedback
- ✔ Non-blocking LED indicators
- ✔ Configurable thresholds via Blynk dashboard

Powered by ESP32, controlled via Blynk, and designed to be modular, robust, and flash-safe.

## 🧠 Features in Detail

### 🔐 Access Control
- Uses MFRC522 RFID
- Toggle home status (locked/unlocked) with authorized RFID cards
- Status synced to Blynk dashboard

### 🌡 Smart Environmental Monitoring
- DHT11 for temperature & humidity
- Real-time graphing on Blynk
- Auto fan speed control based on thresholds

### 🌀 Fan Control
- Manual or automatic modes
- PWM speed control
- Respect thresholds and user preference

### 💦 Water Pump Automation
- Reads soil moisture via analog sensor
- Auto pump on/off based on moisture
- Adjustable thresholds via Blynk

### 📱 Blynk Integration
- Live sensor data
- Toggle switches
- Sliders for thresholds and settings
- Terminal logs

## 🛠 Architecture Overview

ESP32 connects with:
- DHT11 → Temperature & Humidity
- Soil Moisture Sensor → Analog moisture
- MFRC522 RFID Module → SPI
- L298N Motor Driver → Drives fan + pump
- LEDs → Status indicator

Blynk handles:
- Device monitoring
- User interaction
- Configuration updates

## 🧰 Hardware Pin Connections

- ESP32 LED → Status indication
- GPIO 4 → DHT11
- GPIO 34 → Soil moisture (analog)
- GPIO 21/22/23/19/18 → MFRC522 RFID
- GPIO 26/27/14 → Fan (L298N)
- GPIO 32/33/25 → Pump (L298N)
- GPIO 12 → Light/relay
- GPIO 15/5 → RFID status LEDs

## 📦 Required Libraries

Install these before uploading:

- BlynkSimpleEsp32
- WiFiManager
- Preferences
- DHT
- MFRC522

## 🚀 Quick Setup

### 1. Clone the Repo
```bash
git clone https://github.com/itschinnu0/Home-Automation-and-Smart-Garden.git
cd Home-Automation-and-Smart-Garden
```

### 2. Open in Arduino / VSCode (PlatformIO)
Load `Home_Automation.ino`.

### 3. Fill Blynk Credentials
Replace in your code:
```cpp
#define BLYNK_TEMPLATE_ID "TMPL3mb8grOS1"
#define BLYNK_AUTH_TOKEN  "YourBlynkToken"
```

### 4. Install Libraries
Via Library Manager or PlatformIO.

### 5. Upload
Connect ESP32, choose board/port, and upload.

### 6. Connect to WiFi
On first boot, ESP32 opens an AP to configure WiFi.

## 📱 Blynk Dashboard

Use the Blynk app to:
- View temperature, humidity, soil moisture
- Toggle fan, light, manual mode
- Change thresholds
- See logs in Terminal widget

## ⚡ Behavior Details

### 🧩 Fan Control Modes

| Mode | Behavior |
|------|----------|
| Auto | Temperature based PWM |
| Manual | Dashboard fan slider controls speed |

### 🌿 Moisture Logic
- Moisture < MIN → Pump ON
- Moisture > MAX → Pump OFF
- Timer adjusts for critical readings

### 🧪 Testing Scenarios
- ✔ Power on & WiFi setup
- ✔ RFID card access grants/denies
- ✔ Temperature threshold responses
- ✔ Manual vs Auto fan behavior
- ✔ Pump activation at dry soil
- ✔ Blynk disconnect/reconnect

## 📁 Code Structure
```
├── Home_Automation.ino   # Main firmware
├── README.md             # This file
├── assets/               # Optional images/diagrams
├── docs/                 # Documentation (pinouts, flowcharts)
└── LICENSE
```

## 🗂 Appendix

A short excerpt of key functions is included in `docs/APPENDIX.md`.

## 📈 Block Diagram & Flowchart

Block diagrams and flowcharts for this project are available in the `docs/` folder.

## 🧠 Versioning

This project uses:
- ESP32 Core for Arduino
- Blynk IoT v2
- Arduino JSON / Widgets

## 📄 License

This project is released under the MIT License — full license text in this repository.

## ✉️ Contact

**Created by:** Chinnu0

- **GitHub:** https://github.com/itschinnu0
- **Project Repo:** https://github.com/itschinnu0/Home-Automation-and-Smart-Garden

## ⭐ Support This Project

Please ⭐ star the repo!

Share feedback or improvements via Issues or PRs.

