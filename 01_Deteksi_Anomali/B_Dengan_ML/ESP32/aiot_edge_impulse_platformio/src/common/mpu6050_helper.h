#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 21
#endif

#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 22
#endif

struct ImuSample {
  float accX;
  float accY;
  float accZ;
  float gyroX;
  float gyroY;
  float gyroZ;
  float tempC;
};

bool initMpu6050(Adafruit_MPU6050 &mpu, bool debug = true);
ImuSample readMpu6050(Adafruit_MPU6050 &mpu);
void scanI2CBus(Stream &out);
