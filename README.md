# esp32-smart-irrigation-rootx
ESP32 IoT Smart Irrigation System with Wokwi simulation, rain lockout &amp; dry-run protection — Team RootX (Paradigm Shift Hackathon 2026).

# Smart Irrigation System Using ESP32 (Team RootX)

An IoT-based, single-zone smart irrigation prototype built for the **Paradigm Shift Hackathon 2026 (IEEE Student Branch, JGEC)**. The system automates watering using real-time soil moisture, rainfall detection, reservoir water-level monitoring, and ambient climate data—running all core irrigation and safety logic locally on the ESP32.

## Live Online Simulation (Wokwi)
**[Click here to launch the live Wokwi Simulation](https://wokwi.com/projects/481730947337352193)**

### Note on Simulation Constraints (Hardware Signal Emulation)
Because online circuit simulators (like Wokwi) do not natively include specialized agricultural probes (capacitive soil sensors, rain pads, or water-depth PCBs), we used standard electrical **signal emulators** that feed the exact same `0–3.3V` analog and `HIGH/LOW` digital signals into the ESP32. This validates 100% of our firmware, safety lockouts, and LCD/IoT logic before physical assembly:

| Physical Prototype Component | Wokwi Simulation Equivalent | Signal Type & Behavior |
| :--- | :--- | :--- |
| **Capacitive Soil Moisture Sensor** | **Potentiometer** (`GPIO 34`) | Analog (`0–4095`) mapped to `0–100%` soil moisture |
| **Rain Sensor Module** | **Slide Switch 1** (`GPIO 27`) | Digital (`INPUT_PULLUP`); `LOW` = Rain detected (pauses pump) |
| **Water Level / Depth Sensor** | **Slide Switch 2** (`GPIO 26`) | Digital (`INPUT_PULLUP`); `LOW` = Tank empty (safety cutoff) |
| **DHT22 (Temp & Humidity)** | **DHT22** (`GPIO 4`) | Native digital climate sensor |
| **12V DC Water Pump** | **Relay (`GPIO 25`) + Red LED** | LED lights up when relay switches the pump ON |
| **16×2 I2C LCD** | **LCD 1602 I2C** (`GPIO 21/22`) | Native live status display |

## How to Test the Simulation
1. Open the Wokwi link above and click the green **Start Simulation** button.
2. **Test Auto-Irrigation:** Turn the potentiometer down so soil moisture drops below the threshold—the relay triggers, the red pump LED turns ON, and the LCD displays `IRRIGATE PMP:ON`.
3. **Test Rain Lockout:** Flip **Slide Switch 1**—irrigation immediately suspends even if the soil is dry.
4. **Test Dry-Run Protection:** Flip **Slide Switch 2**—the pump locks OFF to protect against an empty reservoir.
5. **Blynk IoT Mode:** Set `#define ENABLE_BLYNK true` in `sketch.ino` and add your Blynk template credentials to stream live telemetry over Wokwi's built-in virtual Wi-Fi.

## Repository Contents
- `sketch.ino` — Complete ESP32 firmware (sensor calibration, safety lockouts, LCD & optional Blynk IoT)
- `diagram.json` — Wokwi circuit wiring layout
- `libraries.txt` — Required Arduino/ESP32 libraries (`LiquidCrystal I2C`, `DHT sensor library`, `Blynk`)
