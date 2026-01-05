#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/* ---------------- CONFIG ---------------- */
#define MPU_SDA A4
#define MPU_SCL A5

#define GEAR_TX 1 // Nano TX to ESP32 RX

#define FILTER_ALPHA 0.15f
#define NEUTRAL_DEADZONE 1.5

/* ---------------- OBJECTS ---------------- */
Adafruit_MPU6050 mpu;

/* ---------------- STATE ---------------- */
float fax = 0, fay = 0;
float neutralX = 0, neutralY = 0;
int currentGear = 0;

/* ---------------- PROTOTYPES ---------------- */
void autoCalibrateNeutral();
void updateGear();
void sendData();

/* ---------------- SETUP ---------------- */
void setup() {
  Wire.begin(); // <-- just this for Nano, no arguments
  Serial.begin(115200); // TX to ESP32

  if (!mpu.begin()) {
    while (1); // halt if MPU fails
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  autoCalibrateNeutral();
}


/* ---------------- LOOP ---------------- */
void loop() {
  updateGear();
  sendData();
  delay(30);
}

/* ---------------- AUTO-CALIBRATION ---------------- */
void autoCalibrateNeutral() {
  sensors_event_t a, g, temp;
  float sumX = 0, sumY = 0;
  const int samples = 50;

  for (int i = 0; i < samples; i++) {
    mpu.getEvent(&a, &g, &temp);
    sumX += a.acceleration.x;
    sumY += a.acceleration.y;
    delay(10);
  }

  neutralX = sumX / samples;
  neutralY = sumY / samples;
}

/* ---------------- GEAR DETECTION ---------------- */
void updateGear() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Low-pass filter
  fax = fax * (1 - FILTER_ALPHA) + a.acceleration.x * FILTER_ALPHA;
  fay = fay * (1 - FILTER_ALPHA) + a.acceleration.y * FILTER_ALPHA;

  float x = fax - neutralX;
  float y = fay - neutralY;

  // Neutral deadzone
  if (abs(x) < NEUTRAL_DEADZONE && abs(y) < NEUTRAL_DEADZONE) {
    currentGear = 0;
    return;
  }

  // Column detection
  int column = 0; // -1 left, 0 center, 1 right
  if (x < -1.0) column = -1;
  else if (x > 1.0) column = 1;

  // Row detection
  bool up = (y < 0);
  bool down = (y > 0);

  // Gear map with inverted mounting correction
  if (column == -1 && up) currentGear = 5;
  else if (column == -1 && down) currentGear = -1; // R
  else if (column == 0 && up) currentGear = 1;
  else if (column == 0 && down) currentGear = 2;
  else if (column == 1 && up) currentGear = 3;
  else if (column == 1 && down) currentGear = 4;
}

/* ---------------- SEND DATA ---------------- */
void sendData() {
  // Format: G:gear\n
  Serial.print("G:");
  Serial.println(currentGear);
}
