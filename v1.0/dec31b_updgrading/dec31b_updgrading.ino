#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
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

#define GPS_RX 6
#define GPS_TX 7

#define FILTER_ALPHA 0.15f
#define NEUTRAL_DEADZONE 1.5
#define PARTICLE_COUNT 15
#define PARTICLE_SPEED 5.0

/* ---------------- OBJECTS ---------------- */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial GPSSerial(1);

/* ---------------- MODES ---------------- */
enum Mode {
  MODE_SHIFTER,
  MODE_FLUID,
  MODE_LOGO,
  MODE_SPEED,
  MODE_0_100
};

Mode currentMode = MODE_SHIFTER;
Mode lastMode    = MODE_SHIFTER;

/* ---------------- STATE ---------------- */
int currentGear = 0;
float fax = 0, fay = 0;
float neutralX = 0, neutralY = 0;

float speedKmph = 0.0;

bool lastBtn = HIGH;
bool inverted = false;

/* ---------------- 0–100 TIMER ---------------- */
bool runActive = false;
unsigned long runStart = 0;
float runTime = 0;

/* ---------------- PARTICLES ---------------- */
struct Particle { float x, y; };
Particle particles[PARTICLE_COUNT];
float lastFax = 0, lastFay = 0;

/* ---------------- PROTOTYPES ---------------- */
void handleButton();
void autoCalibrateNeutral();
void updateGear();
void updateGPS();
void drawGearText();
void drawFluid();
void drawLogo();
void drawSpeed();
void drawZeroToHundred();
void displayError(const char* msg);

/* ---------------- SETUP ---------------- */
void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  if (!mpu.begin()) {
    displayError("MPU FAIL");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  autoCalibrateNeutral();

  for (int i = 0; i < PARTICLE_COUNT; i++) {
    particles[i].x = SCREEN_WIDTH / 2;
    particles[i].y = SCREEN_HEIGHT / 2;
  }
}

/* ---------------- LOOP ---------------- */
void loop() {
  handleButton();
  updateGear();
  updateGPS();

  if (currentMode != lastMode) {
    display.clearDisplay();
    lastMode = currentMode;
  }

  switch (currentMode) {
    case MODE_SHIFTER:  drawGearText(); break;
    case MODE_FLUID:    drawFluid(); break;
    case MODE_LOGO:     drawLogo(); break;
    case MODE_SPEED:    drawSpeed(); break;
    case MODE_0_100:    drawZeroToHundred(); break;
  }

  delay(30);
}

/* ---------------- BUTTON ---------------- */
void handleButton() {
  static unsigned long pressStart = 0;
  bool now = digitalRead(BTN_PIN);

  if (now == LOW && lastBtn == HIGH) {
    pressStart = millis();
  }

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

/* ---------------- GPS ---------------- */
void updateGPS() {
  while (GPSSerial.available()) {
    gps.encode(GPSSerial.read());
  }

  if (gps.speed.isValid()) {
    speedKmph = gps.speed.kmph();
  }
}

/* ---------------- AUTO-CALIBRATION ---------------- */
void autoCalibrateNeutral() {
  display.clearDisplay();
  display.setCursor(10, 25);
  display.setTextSize(1);
  display.print("Calibrating Neutral...");
  display.display();

  sensors_event_t a, g, t;
  float sx = 0, sy = 0;

  for (int i = 0; i < 50; i++) {
    mpu.getEvent(&a, &g, &t);
    sx += a.acceleration.x;
    sy += a.acceleration.y;
    delay(10);
  }

  neutralX = sx / 50;
  neutralY = sy / 50;

  display.clearDisplay();
  display.setCursor(30, 25);
  display.print("Neutral Set");
  display.display();
  delay(700);
}

/* ---------------- IMU & GEAR ---------------- */
void updateGear() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  fax = fax * (1 - FILTER_ALPHA) + a.acceleration.x * FILTER_ALPHA;
  fay = fay * (1 - FILTER_ALPHA) + a.acceleration.y * FILTER_ALPHA;

  float x = fax - neutralX;
  float y = fay - neutralY;

  if (abs(x) < NEUTRAL_DEADZONE && abs(y) < NEUTRAL_DEADZONE) {
    currentGear = 0;
    return;
  }

  int column = 0;
  if (x < -1.0) column = -1;
  else if (x > 1.0) column = 1;

  bool up = (y < 0);
  bool down = (y > 0);

  if (column == -1 && up) currentGear = 1;
  else if (column == -1 && down) currentGear = 2;
  else if (column == 0 && up) currentGear = 3;
  else if (column == 0 && down) currentGear = 4;
  else if (column == 1 && up) currentGear = 5;
  else if (column == 1 && down) currentGear = -1;
}

/* ---------------- GEAR + SPEED ---------------- */
void drawGearText() {
  static int lastGear = 99;
  if (lastMode != MODE_SHIFTER) lastGear = 99;
  if (currentGear == lastGear) return;

  const char* g = "N";
  if (currentGear == -1) g = "R";
  else if (currentGear > 0) {
    static char buf[2];
    buf[0] = '0' + currentGear;
    buf[1] = 0;
    g = buf;
  }

  display.clearDisplay();
  display.setTextSize(4);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(g, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 8);
  display.print(g);

  display.setTextSize(1);
  display.setCursor(36, 52);
  display.print(speedKmph, 1);
  display.print(" km/h");

  display.display();
  lastGear = currentGear;
}

/* ---------------- SPEED ONLY ---------------- */
void drawSpeed() {
  display.clearDisplay();

  display.setTextSize(3);
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

  display.display();
}

/* ---------------- 0–100 MODE ---------------- */
void drawZeroToHundred() {

  if (!runActive && speedKmph < 2.0) {
    runTime = 0;
  }

  if (!runActive && speedKmph > 2.0) {
    runActive = true;
    runStart = millis();
  }

  if (runActive && speedKmph < 2.0) {
    runActive = false;
  }

  if (runActive && speedKmph < 100.0) {
    runTime = (millis() - runStart) / 1000.0;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 5);
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
  display.setCursor(90, 50);
  display.print("sec");

  display.display();
}

/* ---------------- FLUID ---------------- */
void drawFluid() {
  display.clearDisplay();

  float dx = fax - lastFax;
  float dy = fay - lastFay;

  for (int i = 0; i < PARTICLE_COUNT; i++) {
    particles[i].x += -dx * PARTICLE_SPEED + random(-1, 2);
    particles[i].y +=  dy * PARTICLE_SPEED + random(-1, 2);

    particles[i].x = constrain(particles[i].x, 0, SCREEN_WIDTH);
    particles[i].y = constrain(particles[i].y, 0, SCREEN_HEIGHT);

    display.fillCircle(particles[i].x, particles[i].y, 2, SSD1306_WHITE);
  }

  lastFax = fax;
  lastFay = fay;
  display.display();
}

/* ---------------- LOGO ---------------- */
void drawLogo() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 25);
  display.print("AE101 GT");
  display.display();
}

/* ---------------- ERROR ---------------- */
void displayError(const char* msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 25);
  display.print(msg);
  display.display();
}
