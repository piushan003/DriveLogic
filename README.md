# DriveLogic

**Motion-Based Digital Gear Indicator & Telemetry Display**

ShiftSense HUD is a custom-built **digital gear knob and vehicle telemetry system** designed for manual transmission cars.  
It uses **motion sensing (IMU)** and **GPS data** to display real-time gear position, speed, and performance metrics on a compact OLED screen.

Built and tested on a **Toyota AE101 (4A-GE swapped)**, this system is designed to feel **OEM**, **motorsport-inspired**, and **reliable in real driving conditions**.


## ✨ Features

- 🎯 **Motion-based gear detection**
  - No switches, no linkage sensors
  - Uses IMU tilt vectors mounted directly in the gear knob

- 🧭 **Elliptical gear zones**
  - More realistic than rectangular zones
  - Matches real H-pattern shifter movement

- ⚙️ **Neutral line logic**
  - Gear changes only register through Neutral
  - Prevents accidental gear jumps (e.g., 2 → 4)

- 📟 **OLED HUD (128×64)**
  - Large gear display
  - Speed overlay
  - Multiple display modes

- 🛰️ **GPS Speed & Performance**
  - Accurate km/h display
  - 0–100 km/h timer
  - GPS fix & timeout fail-safe

- 🌊 **Fluid / Circle Mode**
  - Motion-reactive animation driven by IMU data

- 🔁 **Mode Button**
  - Short press: cycle modes
  - Long press: invert display

- 🛡️ **Fail-safe design**
  - GPS lock detection
  - Speed filtering to avoid false 1 km/h readings
  - 0–100 mode locked until GPS is ready

## 🧩 System Architecture

This project uses **two microcontrollers**, each with a dedicated role:

### 🔹 Arduino Nano (Gear Sensor Unit)
- MPU6050 IMU
- Handles:
  - Motion filtering
  - Neutral calibration
  - Elliptical gear zone detection
- Sends gear data to ESP32 via UART

### 🔹 ESP32-C3 (HUD Controller)
- OLED display
- GPS module (TinyGPS++)
- Handles:
  - UI rendering
  - Mode switching
  - Speed & performance calculations


## 🔌 Hardware Used

- Arduino Nano  
- ESP32-C3  
- MPU6050 IMU  
- GPS Module (NEO-6M or compatible)  
- 128×64 OLED (SSD1306, I²C)  
- Momentary push button  


## 🧠 Why Elliptical Gear Zones?

Traditional rectangular zones cause:
- Gear overlap
- False shifts
- Poor diagonal detection

Elliptical zones:
- Match the **natural arc of a shifter**
- Allow smoother transitions
- Reduce misclassification between adjacent gears (2↔4, 1↔3)

Adaptive ellipse sizing further compensates for:
- Driver force variation
- Chassis vibration
- Sensor mounting angle



## 🛰️ GPS Notes

- GPS requires **cold start time** (up to several minutes on first power-up)
- Small speed readings (0–1 km/h) while stationary are normal due to:
  - Satellite drift
  - Doppler noise

Mitigations implemented:
- Speed dead-zone
- GPS fix validation
- Timeout warning
- 0–100 mode locked until GPS is stable



## ⚠️ Important Notes

- This project is **vehicle-installed**
- Code changes should be made carefully
- Neutral calibration assumes the shifter is untouched at startup
- IMU is mounted **inverted** (logic adjusted in software)



## 📜 License

This project is released for **educational and personal use**.  
Use at your own risk when installing in a vehicle.
