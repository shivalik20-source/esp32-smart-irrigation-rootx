// ============================================================================
// OPTIONAL BLYNK IoT CONFIGURATION (Set ENABLE_BLYNK to true to use Blynk)
// ============================================================================
#define ENABLE_BLYNK false

#if ENABLE_BLYNK
  #define BLYNK_TEMPLATE_ID   "YourTemplateID"
  #define BLYNK_TEMPLATE_NAME "SmartIrrigation"
  #define BLYNK_AUTH_TOKEN    "YourAuthToken"
  #define BLYNK_PRINT Serial
#endif

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#if ENABLE_BLYNK
  #include <WiFi.h>
  #include <BlynkSimpleEsp32.h>
#endif

// ============================================================================
// PIN DEFINITIONS (ESP32 ADC1 / Safe GPIOs)
// ============================================================================
#define SOIL_PIN    34      // Potentiometer (0-4095) simulating soil moisture
#define RAIN_PIN    27      // Slide Switch 1: LOW = Rain detected (INPUT_PULLUP)
#define FLOAT_PIN   26      // Slide Switch 2: LOW = Tank empty (INPUT_PULLUP)
#define RELAY_PIN   25      // Relay module IN pin -> Pump indicator
#define DHT_PIN     4       // DHT22 data pin
#define DHT_TYPE    DHT22

// ============================================================================
// THRESHOLDS & TIMINGS
// ============================================================================
const int MOISTURE_LOW_THRESHOLD  = 40;          // Irrigate when moisture < 40%
const int MOISTURE_HIGH_THRESHOLD = 65;          // Stop early if moisture >= 65%
const unsigned long MAX_PUMP_RUNTIME_MS = 10000; // Max 10s continuous run (demo)
const unsigned long SOAK_COOLDOWN_MS    = 5000;  // 5s soak/cooldown pause
const unsigned long UI_UPDATE_MS        = 500;   // LCD & Serial update interval
const unsigned long BLYNK_SYNC_MS       = 2000;  // Blynk cloud push interval

#if ENABLE_BLYNK
  const char* WIFI_SSID = "Wokwi-GUEST";
  const char* WIFI_PASS = "";
#endif

// ============================================================================
// HARDWARE OBJECTS & STATE VARIABLES
// ============================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);

bool pumpOn = false;
bool inSoak = false;
bool manualMode = false;
bool manualPumpCmd = false;

unsigned long pumpStartMs = 0;
unsigned long soakStartMs = 0;
unsigned long lastUiMs = 0;
unsigned long lastBlynkMs = 0;

int currentMoisturePct = 0;
float currentTempC = 0.0;
float currentHumPct = 0.0;
bool isRaining = false;
bool isTankLow = false;
String currentStatus = "INIT    ";

#if ENABLE_BLYNK
// V0: Manual/Auto Mode Switch (0 = AUTO, 1 = MANUAL)
BLYNK_WRITE(V0) {
  manualMode = (param.asInt() == 1);
  if (!manualMode) {
    manualPumpCmd = false;
  }
}

// V1: Manual Pump Button (0 = OFF, 1 = ON, active in MANUAL mode)
BLYNK_WRITE(V1) {
  manualPumpCmd = (param.asInt() == 1);
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V0, V1);
}
#endif

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(RAIN_PIN, INPUT_PULLUP);
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  dht.begin();

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 Irrigation");
  lcd.setCursor(0, 1);
  lcd.print("Booting...      ");

#if ENABLE_BLYNK
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect(3000);
#endif

  delay(1200);
  lcd.clear();
  Serial.println("ESP32 Smart Irrigation Wokwi Simulation Ready.");
}

void loop() {
  unsigned long now = millis();

#if ENABLE_BLYNK
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
#endif

  if (now - lastUiMs >= UI_UPDATE_MS) {
    lastUiMs = now;

    // 1. Read sensors
    int rawSoil = analogRead(SOIL_PIN);
    currentMoisturePct = constrain(map(rawSoil, 0, 4095, 0, 100), 0, 100);

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) currentTempC = t;
    if (!isnan(h)) currentHumPct = h;

    isRaining = (digitalRead(RAIN_PIN) == LOW);
    isTankLow = (digitalRead(FLOAT_PIN) == LOW);

    // 2. Update soak timer
    if (inSoak && (now - soakStartMs >= SOAK_COOLDOWN_MS)) {
      inSoak = false;
    }

    // 3. Safety interlocks & irrigation control
    if (isTankLow) {
      pumpOn = false;
      inSoak = false;
      manualPumpCmd = false;
      currentStatus = "TANK LOW";
    } else if (isRaining && !manualMode) {
      pumpOn = false;
      currentStatus = "RAINLOCK";
    } else if (manualMode) {
      pumpOn = manualPumpCmd;
      currentStatus = pumpOn ? "MAN-RUN " : "MANUAL  ";
    } else if (pumpOn) {
      if (now - pumpStartMs >= MAX_PUMP_RUNTIME_MS) {
        pumpOn = false;
        inSoak = true;
        soakStartMs = now;
        currentStatus = "SOAK/MAX";
      } else if (currentMoisturePct >= MOISTURE_HIGH_THRESHOLD) {
        pumpOn = false;
        inSoak = true;
        soakStartMs = now;
        currentStatus = "MOIST OK";
      } else {
        currentStatus = "IRRIGATE";
      }
    } else if (inSoak) {
      currentStatus = "SOAKING ";
    } else if (currentMoisturePct < MOISTURE_LOW_THRESHOLD) {
      pumpOn = true;
      pumpStartMs = now;
      currentStatus = "IRRIGATE";
    } else {
      currentStatus = "MOIST OK";
    }

    // 4. Drive pump output
    digitalWrite(RELAY_PIN, pumpOn ? HIGH : LOW);

    // 5. Update 16x2 I2C LCD
    char line1[17];
    snprintf(line1, sizeof(line1), "M:%2d%% T:%2dC H:%2d%%",
             currentMoisturePct, (int)round(currentTempC), (int)round(currentHumPct));
    lcd.setCursor(0, 0);
    lcd.print(line1);

    char line2[17];
    snprintf(line2, sizeof(line2), "%-8s PMP:%s",
             currentStatus.c_str(), pumpOn ? "ON " : "OFF");
    lcd.setCursor(0, 1);
    lcd.print(line2);

    // 6. Log to Serial Monitor
    Serial.print("Soil: ");
    Serial.print(currentMoisturePct);
    Serial.print("% | Temp: ");
    Serial.print(currentTempC, 1);
    Serial.print("C | Hum: ");
    Serial.print(currentHumPct, 1);
    Serial.print("% | Rain: ");
    Serial.print(isRaining ? "YES" : "NO");
    Serial.print(" | TankLow: ");
    Serial.print(isTankLow ? "YES" : "NO");
    Serial.print(" | State: ");
    Serial.print(currentStatus);
    Serial.print(" | Pump: ");
    Serial.println(pumpOn ? "ON" : "OFF");
  }

#if ENABLE_BLYNK
  if ((now - lastBlynkMs >= BLYNK_SYNC_MS) && Blynk.connected()) {
    lastBlynkMs = now;
    Blynk.virtualWrite(V2, currentMoisturePct);
    Blynk.virtualWrite(V3, currentTempC);
    Blynk.virtualWrite(V4, currentHumPct);
    Blynk.virtualWrite(V5, pumpOn ? 1 : 0);
  }
#endif
}