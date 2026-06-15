/*
  Edge Impulse Data Collection Firmware
  Streams accX,accY,accZ over Serial for Edge Impulse Data Forwarder.

  Build/upload:
    pio run -e esp32dev_collect -t upload
    pio run -e esp32c3_collect -t upload
    pio run -e xiao_esp32s3_collect -t upload

  Edge Impulse:
    edge-impulse-data-forwarder --frequency 50

  Axis names:
    accX, accY, accZ
*/

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include "common/mpu6050_helper.h"

#ifndef SAMPLE_RATE_HZ
#define SAMPLE_RATE_HZ 50
#endif

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

Adafruit_MPU6050 mpu;
static const uint32_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;
uint32_t lastSampleUs = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);

  // Important: debug=false so Edge Impulse receives only numeric CSV lines.
  bool ok = initMpu6050(mpu, false);
  if (!ok) {
    while (true) delay(1000);
  }

  lastSampleUs = micros();
}

void loop() {
  uint32_t now = micros();
  if ((uint32_t)(now - lastSampleUs) >= SAMPLE_PERIOD_US) {
    lastSampleUs += SAMPLE_PERIOD_US;
    ImuSample s = readMpu6050(mpu);

    Serial.print(s.accX, 6);
    Serial.print(',');
    Serial.print(s.accY, 6);
    Serial.print(',');
    Serial.println(s.accZ, 6);
  }
}
