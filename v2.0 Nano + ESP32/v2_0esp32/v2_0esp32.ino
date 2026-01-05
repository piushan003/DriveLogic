#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

/* ---------------- CONFIG ---------------- */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1

#define SDA_PIN 4
#define SCL_PIN 5
#define BTN_PIN 2

// UARTs (DO NOT CHANGE – matches your wiring)
#define NANO_RX 10          // Nano TX → ESP32-C3 GPIO10
#define GPS_RX  6           // GPS TX → ESP32-C3 GPIO6
#define GPS_TX  7           // GPS RX → ESP32-C3 GPIO7

#define PARTICLE_COUNT 15

// GPS safety
#define GPS_TIMEOUT_MS 5000     // 5 sec no data → warning
#define GPS_MIN_SATS   4
#define SPEED_NOISE_KMPH 1.5    // filter ghost movement

/* ---------------- OBJECTS ---------------- */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
TinyGPSPlus gps;

HardwareSerial NanoSerial(1);
HardwareSerial GPSSerial(0);

/* ---------------- MODES ---------------- */
enum Mode {
  MODE_SHIFTER,
  MODE_FLUID,
  MODE_SPEED,
  MODE_0_100,
  MODE_LOGO
};

Mode currentMode = MODE_SHIFTER;
Mode lastMode    = MODE_SHIFTER;

/* ---------------- STATE ---------------- */
int currentGear = 0;
int lastGear = 99;

float speedKmph = 0.0;

bool lastBtn = HIGH;
bool inverted = false;

/* ---------------- GPS STATE ---------------- */
bool gpsFix = false;
bool gpsTimeout = true;
unsigned long lastGPSMillis = 0;

/* ---------------- 0–100 ---------------- */
bool runActive = false;
unsigned long runStart = 0;
float runTime = 0;

/* ---------------- PARTICLES ---------------- */
struct Particle { float x, y; };
Particle particles[PARTICLE_COUNT];

/* ---------------- SETUP ---------------- */
void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  NanoSerial.begin(115200, SERIAL_8N1, NANO_RX, -1);
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);

  // ---- Ripple intro ----
  for (int r = 2; r < SCREEN_WIDTH / 2; r += 4) {
    display.clearDisplay();
    display.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, r, SSD1306_WHITE);
    display.display();
    delay(35);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(18, 25);
  display.print("AE101 GT");
  display.display();
  delay(800);

  for (int i = 0; i < PARTICLE_COUNT; i++) {
    particles[i].x = SCREEN_WIDTH / 2;
    particles[i].y = SCREEN_HEIGHT / 2;
  }
}

/* ---------------- LOOP ---------------- */
void loop() {
  handleButton();
  readNano();
  readGPS();

  if (currentMode != lastMode) {
    display.clearDisplay();
    lastMode = currentMode;
    if (currentMode == MODE_SHIFTER) lastGear = 99;
  }

  switch (currentMode) {
    case MODE_SHIFTER: drawGear(); break;
    case MODE_FLUID:   drawFluid(); break;
    case MODE_SPEED:   drawSpeed(); break;
    case MODE_0_100:   drawZeroToHundred(); break;
    case MODE_LOGO:    drawLogo(); break;
  }

  delay(30);
}

/* ---------------- BUTTON ---------------- */
void handleButton() {
  static unsigned long pressStart = 0;
  bool now = digitalRead(BTN_PIN);

  if (now == LOW && lastBtn == HIGH) pressStart = millis();

  if (now == HIGH && lastBtn == LOW) {
    unsigned long pressTime = millis() - pressStart;

    if (pressTime > 1200) {
      inverted = !inverted;
      display.invertDisplay(inverted);
    } else if (pressTime > 50) {
      currentMode = (Mode)((currentMode + 1) % 5);
    }
  }

  lastBtn = now;
}

/* ---------------- READ NANO ---------------- */
void readNano() {
  while (NanoSerial.available()) {
    String line = NanoSerial.readStringUntil('\n');
    if (line.startsWith("G:")) {
      currentGear = line.substring(2).toInt();
    }
  }
}

/* ---------------- READ GPS ---------------- */
void readGPS() {
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
    lastGPSMillis = millis();
  }

  gpsTimeout = (millis() - lastGPSMillis > GPS_TIMEOUT_MS);

  gpsFix = gps.location.isValid() &&
           gps.speed.isValid() &&
           gps.satellites.value() >= GPS_MIN_SATS &&
           !gpsTimeout;

  if (gpsFix) {
    float raw = gps.speed.kmph();
    speedKmph = (raw < SPEED_NOISE_KMPH) ? 0.0 : raw;
  }
}

/* ---------------- GEAR MODE ---------------- */
void drawGear() {
  if (currentGear == lastGear) return;

  display.clearDisplay();
  display.setTextSize(4);

  const char* g = "N";
  static char buf[2];

  if (currentGear == -1) g = "R";
  else if (currentGear > 0) {
    buf[0] = '0' + currentGear;
    buf[1] = 0;
    g = buf;
  }

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(g, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 6);
  display.print(g);

  display.setTextSize(1);
  display.setCursor(0, 52);

  if (!gpsFix) {
    display.print(gpsTimeout ? "GPS LOST" : "GPS WARMING...");
  } else {
    display.print(speedKmph, 1);
    display.print(" km/h");
  }

  display.display();
  lastGear = currentGear;
}

/* ---------------- FLUID MODE ---------------- */
void drawFluid() {
  display.clearDisplay();

  for (int i = 0; i < PARTICLE_COUNT; i++) {
    particles[i].x += random(-2, 3);
    particles[i].y += random(-2, 3);

    particles[i].x = constrain(particles[i].x, 0, SCREEN_WIDTH);
    particles[i].y = constrain(particles[i].y, 0, SCREEN_HEIGHT);

    display.fillCircle(particles[i].x, particles[i].y, 2, SSD1306_WHITE);
  }

  display.display();
}

/* ---------------- SPEED MODE ---------------- */
void drawSpeed() {
  display.clearDisplay();
  display.setTextSize(3);

  if (!gpsFix) {
    display.setTextSize(1);
    display.setCursor(20, 30);
    display.print(gpsTimeout ? "GPS SIGNAL LOST" : "GPS WARMING...");
  } else {
    char buf[8];
    sprintf(buf, "%.0f", speedKmph);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 10);
    display.print(buf);

    display.setTextSize(1);
    display.setCursor(46, 45);
    display.print("km/h");
  }

  display.display();
}

/* ---------------- 0–100 MODE ---------------- */
void drawZeroToHundred() {
  display.clearDisplay();

  if (!gpsFix) {
    display.setTextSize(1);
    display.setCursor(15, 30);
    display.print("WAIT FOR GPS FIX");
    display.display();
    return;
  }

  if (!runActive && speedKmph < 2.0) runTime = 0;
  if (!runActive && speedKmph > 2.0) {
    runActive = true;
    runStart = millis();
  }
  if (runActive && speedKmph >= 100.0) runActive = false;
  if (runActive) runTime = (millis() - runStart) / 1000.0;

  display.setTextSize(1);
  display.setCursor(25, 5);
  display.print("0 - 100 km/h");

  display.setTextSize(3);
  char buf[8];
  sprintf(buf, "%.2f", runTime);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 25);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(90, 52);
  display.print("sec");

  display.display();
}

/* ---------------- LOGO ---------------- */
void drawLogo() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(18, 25);
  display.print("AE101 GT");
  display.display();
}
