#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ---------------- CONFIG ---------------- */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1

#define SDA_PIN 4
#define SCL_PIN 5
#define BTN_PIN 2

#define FILTER_ALPHA 0.15f

// Direction thresholds
#define MOVE_THRESHOLD 1.2
#define DIAG_RATIO 0.6   // how diagonal it must be

/* ---------------- OBJECTS ---------------- */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

/* ---------------- STATE ---------------- */
int currentGear = 0;   // -1=R, 0=N, 1-5 gears
float fax = 0, fay = 0;
float neutralX = 0, neutralY = 0;

/* ---------------- SETUP ---------------- */
void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  if (!mpu.begin()) while (1);

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  autoCalibrateNeutral();
}

/* ---------------- LOOP ---------------- */
void loop() {
  updateGear();
  drawGearText();
  delay(30);
}

/* ---------------- NEUTRAL CALIBRATION ---------------- */
void autoCalibrateNeutral() {
  sensors_event_t a, g, t;
  float sx = 0, sy = 0;

  for (int i = 0; i < 50; i++) {
    mpu.getEvent(&a, &g, &t);
    sx += a.acceleration.x;
    sy += a.acceleration.y;
    delay(10);
  }

  neutralX = sx / 50.0;
  neutralY = sy / 50.0;
}

/* ---------------- GEAR LOGIC (DIRECTIONAL) ---------------- */
void updateGear() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  // Low-pass filter
  fax = fax * (1 - FILTER_ALPHA) + a.acceleration.x * FILTER_ALPHA;
  fay = fay * (1 - FILTER_ALPHA) + a.acceleration.y * FILTER_ALPHA;

  // Direction from neutral
  float dx = fax - neutralX;
  float dy = fay - neutralY;

  float mag = sqrt(dx * dx + dy * dy);

  // Stay neutral if movement is small
  if (mag < MOVE_THRESHOLD) {
    currentGear = 0;
    return;
  }

  float ax = abs(dx);
  float ay = abs(dy);

  // NORTH / SOUTH
  if (ay > ax * (1 / DIAG_RATIO)) {
    if (dy < 0) currentGear = 3;   // North
    else        currentGear = 4;   // South
  }
  // DIAGONALS
  else {
    if (dx < 0 && dy < 0) currentGear = 1;   // NW
    else if (dx < 0 && dy > 0) currentGear = 2; // SW
    else if (dx > 0 && dy < 0) currentGear = 5; // NE
    else if (dx > 0 && dy > 0) currentGear = -1; // SE (Reverse)
  }
}

/* ---------------- DISPLAY ---------------- */
void drawGearText() {
  display.clearDisplay();
  display.setTextSize(4);

  String gear;
  switch (currentGear) {
    case -1: gear = "2"; break;
    case 0:  gear = "N"; break;
    case 1:  gear = "5"; break;
    case 2:  gear = "R"; break;
    case 3:  gear = "3"; break;
    case 4:  gear = "4"; break;
    case 5:  gear = "1"; break;
  }

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(gear, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(gear);
  display.display();
}
