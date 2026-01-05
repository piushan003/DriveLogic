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
#define LOCK_FRAMES 3
#define NEUTRAL_DEADZONE 1.5
#define PARTICLE_COUNT 15
#define PARTICLE_SPEED 5.0

/* ---------------- OBJECTS ---------------- */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

/* ---------------- MODES ---------------- */
enum Mode { MODE_SHIFTER, MODE_FLUID, MODE_LOGO };
Mode currentMode = MODE_SHIFTER;

/* ---------------- STATE ---------------- */
int currentGear = 0;    // -1=R, 0=N, 1-5 gears
float fax = 0, fay = 0;
float neutralX = 0, neutralY = 0;

bool lastBtn = HIGH;
unsigned long lastBtnTime = 0;

/* ---------------- PARTICLES ---------------- */
struct Particle { float x, y; };
Particle particles[PARTICLE_COUNT];
float lastFax = 0, lastFay = 0;

/* ---------------- PROTOTYPES ---------------- */
void handleButton();
void autoCalibrateNeutral();
void updateGear();
void drawGearText();
void drawFluid();
void drawLogo();
void displayError(const char* msg);

/* ---------------- SETUP ---------------- */
void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  if (!mpu.begin()) { displayError("MPU FAIL"); while(1); }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  autoCalibrateNeutral();

  // Initialize particle positions
  for(int i=0;i<PARTICLE_COUNT;i++){
    particles[i].x = SCREEN_WIDTH/2;
    particles[i].y = SCREEN_HEIGHT/2;
  }
}

/* ---------------- LOOP ---------------- */
void loop() {
  handleButton();
  updateGear();

  switch(currentMode){
    case MODE_SHIFTER: drawGearText(); break;
    case MODE_FLUID: drawFluid(); break;
    case MODE_LOGO: drawLogo(); break;
  }

  delay(30);
}

/* ---------------- BUTTON ---------------- */
void handleButton(){
  bool now = digitalRead(BTN_PIN);
  if(now != lastBtn && millis() - lastBtnTime > 200){
    lastBtnTime = millis();
    lastBtn = now;
    if(now == LOW){
      currentMode = (Mode)((currentMode+1)%3);
      display.clearDisplay();
    }
  }
}

/* ---------------- AUTO-CALIBRATION ---------------- */
void autoCalibrateNeutral(){
  display.clearDisplay();
  display.setCursor(10,25);
  display.setTextSize(1);
  display.print("Calibrating Neutral...");
  display.display();

  sensors_event_t a,g,t;
  float sumX=0,sumY=0;
  const int samples=50;
  for(int i=0;i<samples;i++){
    mpu.getEvent(&a,&g,&t);
    sumX+=a.acceleration.x;
    sumY+=a.acceleration.y;
    delay(10);
  }
  neutralX = sumX/samples;
  neutralY = sumY/samples;

  display.clearDisplay();
  display.setCursor(10,25);
  display.setTextSize(1);
  display.print("Neutral Set!");
  display.display();
  delay(1000);
}

/* ---------------- IMU & GEAR ---------------- */
void updateGear() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a,&g,&t);

  // Low-pass filter
  fax = fax * (1 - FILTER_ALPHA) + a.acceleration.x * FILTER_ALPHA;
  fay = fay * (1 - FILTER_ALPHA) + a.acceleration.y * FILTER_ALPHA;

  float x = fax - neutralX;
  float y = fay - neutralY;

  // Neutral Deadzone
  if(abs(x) < NEUTRAL_DEADZONE && abs(y) < NEUTRAL_DEADZONE){
    currentGear = 0;
    return;
  }

  // Column detection
  int column = 0; // -1=left, 0=middle, 1=right
  if(x < -1.0) column = -1;
  else if(x > 1.0) column = 1;

  // Row detection
  bool up = (y < 0);
  bool down = (y > 0);

  // Gear map
  if(column == -1 && up) currentGear = 1;
  else if(column == -1 && down) currentGear = 2;
  else if(column == 0 && up) currentGear = 3;
  else if(column == 0 && down) currentGear = 4;
  else if(column == 1 && up) currentGear = 5;
  else if(column == 1 && down) currentGear = -1; // Reverse
}

/* ---------------- DISPLAY ---------------- */
void drawGearText() {

  static int lastGear = 99;   // remembers previous gear
  if (currentGear == lastGear) return;

  const char* gearText = "";

  switch (currentGear) {
    case -1: gearText = "2nd"; break;
    case 0:  gearText = "N";   break;
    case 1:  gearText = "5th"; break;
    case 2:  gearText = "R";   break;
    case 3:  gearText = "3rd"; break;
    case 4:  gearText = "4th"; break;
    case 5:  gearText = "1st"; break;
  }

  /* ---- subtle snap transition ---- */
  for (int size = 2; size <= 3; size++) {
    display.clearDisplay();
    display.setTextSize(size);
    display.setTextColor(SSD1306_WHITE);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(gearText, 0, 0, &x1, &y1, &w, &h);

    int x = (SCREEN_WIDTH  - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2 ;   // race HUD bias

    display.setCursor(x, y);
    display.print(gearText);
    display.display();

    delay(25);
  }

  lastGear = currentGear;
}


/* ---------------- FLUID/ANIMATION ---------------- */
void drawFluid(){
  display.clearDisplay();

  float deltaX = fax - lastFax;
  float deltaY = fay - lastFay;

  for(int i=0;i<PARTICLE_COUNT;i++){
    particles[i].x += -deltaX * PARTICLE_SPEED;
    particles[i].y += deltaY * PARTICLE_SPEED;

    // Optional jitter
    particles[i].x += random(-1,2);
    particles[i].y += random(-1,2);

    // Constrain particles to screen
    particles[i].x = constrain(particles[i].x,0,SCREEN_WIDTH);
    particles[i].y = constrain(particles[i].y,0,SCREEN_HEIGHT);

    display.fillCircle((int)particles[i].x,(int)particles[i].y,2,SSD1306_WHITE);
  }

  lastFax = fax;
  lastFay = fay;

  display.display();
}

/* ---------------- LOGO ---------------- */
void drawLogo(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20,25);
  display.print("AE101 GT");
  display.display();
}

/* ---------------- UTIL ---------------- */
void displayError(const char* msg){
  display.clearDisplay();
  display.setCursor(10,25);
  display.setTextSize(2);
  display.print(msg);
  display.display();
}
