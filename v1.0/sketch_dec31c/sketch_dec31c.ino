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
#define NEUTRAL_DEADZONE 1.5

/* ---- FLUID TUNING ---- */
#define PARTICLE_COUNT 12
#define PARTICLE_FORCE 0.65
#define FLUID_DRAG     0.91
#define MAX_VEL        5.5

/* ---- GEAR TRANSITION ---- */
#define GEAR_ANIM_TIME 150

/* ---------------- OBJECTS ---------------- */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

/* ---------------- MODES ---------------- */
enum Mode {
  MODE_SHIFTER,
  MODE_FLUID,
  MODE_LOGO
};
Mode currentMode = MODE_SHIFTER;

/* ---------------- STATE ---------------- */
float fax = 0, fay = 0;
float neutralX = 0, neutralY = 0;

bool lastBtn = HIGH;
unsigned long lastBtnTime = 0;

/* ---------------- GEAR ---------------- */
int currentGear = 0;
int lastGear = 0;
unsigned long gearAnimStart = 0;

/* ---------------- PARTICLES ---------------- */
struct Particle {
  float x, y;
  float vx, vy;
};
Particle particles[PARTICLE_COUNT];

float lastFax = 0, lastFay = 0;

/* ---------------- PROTOTYPES ---------------- */
void handleButton();
void autoCalibrateNeutral();
void updateGear();
void drawGearText();
void drawFluid();
void drawLogo();
void showIntro();

/* ---------------- SETUP ---------------- */
void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  showIntro();

  if (!mpu.begin()) while (1);

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  autoCalibrateNeutral();

  for (int i = 0; i < PARTICLE_COUNT; i++) {
    particles[i].x = random(15, SCREEN_WIDTH - 15);
    particles[i].y = random(15, SCREEN_HEIGHT - 15);
    particles[i].vx = 0;
    particles[i].vy = 0;
  }
}

/* ---------------- LOOP ---------------- */
void loop() {
  handleButton();

  sensors_event_t a,g,t;
  mpu.getEvent(&a,&g,&t);

  fax = fax * (1 - FILTER_ALPHA) + a.acceleration.x * FILTER_ALPHA;
  fay = fay * (1 - FILTER_ALPHA) + a.acceleration.y * FILTER_ALPHA;

  updateGear();

  switch (currentMode) {
    case MODE_SHIFTER: drawGearText(); break;
    case MODE_FLUID:   drawFluid();    break;
    case MODE_LOGO:    drawLogo();     break;
  }

  delay(25);
}

/* ---------------- INTRO ---------------- */
void showIntro() {
  for (int t = 0; t < 180; t++) {
    display.clearDisplay();
    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2;

    for (int i = 0; i < 12; i++) {
      float a = t * 0.05 + i;
      display.fillCircle(cx + cos(a) * 22, cy + sin(a) * 14, 1, SSD1306_WHITE);
    }

    display.setTextSize(2);
    display.setCursor(20, 18);
    display.print("AE101 GT");
    display.display();
    delay(30);
  }
}

/* ---------------- BUTTON ---------------- */
void handleButton() {
  bool now = digitalRead(BTN_PIN);
  if (now != lastBtn && millis() - lastBtnTime > 200) {
    lastBtnTime = millis();
    lastBtn = now;
    if (now == LOW) {
      currentMode = (Mode)((currentMode + 1) % 3);
    }
  }
}

/* ---------------- CALIBRATION ---------------- */
void autoCalibrateNeutral() {
  sensors_event_t a,g,t;
  float sx = 0, sy = 0;
  for (int i = 0; i < 50; i++) {
    mpu.getEvent(&a,&g,&t);
    sx += a.acceleration.x;
    sy += a.acceleration.y;
    delay(10);
  }
  neutralX = sx / 50;
  neutralY = sy / 50;
}

/* ---------------- GEAR DETECTION ---------------- */
void updateGear() {
  float x = fax - neutralX;
  float y = fay - neutralY;

  lastGear = currentGear;

  if (abs(x) < NEUTRAL_DEADZONE && abs(y) < NEUTRAL_DEADZONE) {
    currentGear = 0;
  } else {
    if (x < -1 && y < 0) currentGear = 1;
    else if (x < -1 && y > 0) currentGear = 2;
    else if (x > -1 && x < 1 && y < 0) currentGear = 3;
    else if (x > -1 && x < 1 && y > 0) currentGear = 4;
    else if (x > 1 && y < 0) currentGear = 5;
    else if (x > 1 && y > 0) currentGear = -1;
  }

  if (currentGear != lastGear) {
    gearAnimStart = millis();
  }
}

/* ---------------- GEAR DISPLAY ---------------- */
void drawGearText() {
  display.clearDisplay();

  String gearStr;
  switch (currentGear) {
    case -1: gearStr = "2"; break;
    case 0:  gearStr = "N"; break;
    case 1:  gearStr = "5"; break;
    case 2:  gearStr = "R"; break;
    case 3:  gearStr = "3"; break;
    case 4:  gearStr = "4"; break;
    case 5:  gearStr = "1"; break;
  }

  float anim = min(1.0f, (millis() - gearAnimStart) / (float)GEAR_ANIM_TIME);
  int textSize = 2 + anim * 2;

  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(textSize);
  display.getTextBounds(gearStr, 0, 0, &x1, &y1, &w, &h);

  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(gearStr);

  display.display();
}

/* ---------------- FLUID MODE ---------------- */
void drawFluid() {
  display.clearDisplay();

  float dx = -(fax - lastFax);
  float dy =  (fay - lastFay);

  for (int i = 0; i < PARTICLE_COUNT; i++) {
    particles[i].vx += dx * PARTICLE_FORCE;
    particles[i].vy += dy * PARTICLE_FORCE;

    particles[i].vx *= FLUID_DRAG;
    particles[i].vy *= FLUID_DRAG;

    particles[i].vx = constrain(particles[i].vx, -MAX_VEL, MAX_VEL);
    particles[i].vy = constrain(particles[i].vy, -MAX_VEL, MAX_VEL);

    particles[i].x += particles[i].vx;
    particles[i].y += particles[i].vy;

    if (particles[i].x < 6 || particles[i].x > SCREEN_WIDTH - 6)
      particles[i].vx *= -0.5;
    if (particles[i].y < 6 || particles[i].y > SCREEN_HEIGHT - 6)
      particles[i].vy *= -0.5;

    display.fillCircle(particles[i].x, particles[i].y, 3, SSD1306_WHITE);
    display.drawCircle(particles[i].x + 1, particles[i].y, 3, SSD1306_WHITE);
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
