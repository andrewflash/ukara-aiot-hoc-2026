#include "mpu6050_helper.h"

void scanI2CBus(Stream &out) {
  out.println("I2C scanner start...");
  uint8_t count = 0;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      out.print("I2C device found at 0x");
      if (address < 16) out.print("0");
      out.println(address, HEX);
      count++;
    }
  }
  if (count == 0) out.println("No I2C devices found.");
}

bool initMpu6050(Adafruit_MPU6050 &mpu, bool debug) {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  if (debug) {
    Serial.print("Board: ");
#ifdef BOARD_NAME
    Serial.println(BOARD_NAME);
#else
    Serial.println("UNKNOWN");
#endif
    Serial.print("I2C SDA: ");
    Serial.print(I2C_SDA_PIN);
    Serial.print(" | SCL: ");
    Serial.println(I2C_SCL_PIN);
    scanI2CBus(Serial);
  }

  if (!mpu.begin(0x68, &Wire)) {
    if (debug) Serial.println("MPU6050 not found at 0x68. Trying 0x69...");
    if (!mpu.begin(0x69, &Wire)) {
      if (debug) Serial.println("Failed to find MPU6050. Check wiring and I2C pins.");
      return false;
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  if (debug) Serial.println("MPU6050 initialized.");
  return true;
}

ImuSample readMpu6050(Adafruit_MPU6050 &mpu) {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  ImuSample s;
  s.accX = a.acceleration.x;
  s.accY = a.acceleration.y;
  s.accZ = a.acceleration.z;
  s.gyroX = g.gyro.x;
  s.gyroY = g.gyro.y;
  s.gyroZ = g.gyro.z;
  s.tempC = temp.temperature;
  return s;
}
