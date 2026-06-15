/*
  Edge Impulse Inference + MQTT Firmware (real MPU6050)

  Before build:
  1. Edge Impulse -> Deployment -> Arduino library -> Build.
  2. Extract the downloaded library into lib/, e.g.
       lib/DemoAIoTUkara1_inferencing/
  3. Update the #include below if the generated header name differs.

  Build/upload:
    pio run -e esp32dev_infer -t upload
    pio run -e esp32c3_infer -t upload
    pio run -e xiao_esp32s3_infer -t upload
*/

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MPU6050.h>
#include "common/mpu6050_helper.h"

// GANTI sesuai nama library Edge Impulse hasil export
#include <DemoAIoTUkara1_inferencing.h>

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

// ==== WiFi & MQTT config (edit sesuai kebutuhan) ====
const char *WIFI_SSID = "TEST_SSID";
const char *WIFI_PASSWORD = "PASSWORD";

const char *MQTT_HOST = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char *MQTT_TOPIC = "aiot/demo/1b/ml";

Adafruit_MPU6050 mpu;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
uint32_t lastInferenceMs = 0;
const uint32_t INFERENCE_INTERVAL_MS = 250;

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

float magnitude(float x, float y, float z) {
  return sqrtf(x * x + y * y + z * z);
}

float featureMeanMagnitude() {
  float sum = 0.0f;
  size_t count = 0;

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
    float x = features[ix + 0];
    float y = (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 1) ? features[ix + 1] : 0.0f;
    float z = (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 2) ? features[ix + 2] : 0.0f;
    sum += magnitude(x, y, z);
    count++;
  }

  return count ? sum / count : 0.0f;
}

float featurePeakMagnitude() {
  float peak = 0.0f;

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
    float x = features[ix + 0];
    float y = (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 1) ? features[ix + 1] : 0.0f;
    float z = (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 2) ? features[ix + 2] : 0.0f;
    float m = magnitude(x, y, z);
    if (m > peak) peak = m;
  }

  return peak;
}

bool sampleMpuIntoFeatureBuffer() {
  if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME < 3) {
    Serial.println("Model must use at least 3 raw samples per frame: accX,accY,accZ");
    return false;
  }

  for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
    ImuSample s = readMpu6050(mpu);

    features[ix + 0] = s.accX;
    features[ix + 1] = s.accY;
    features[ix + 2] = s.accZ;

    // If your Edge Impulse model uses 6 axes, add gyro values too.
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME >= 6) {
      features[ix + 3] = s.gyroX;
      features[ix + 4] = s.gyroY;
      features[ix + 5] = s.gyroZ;
    }

    delay(EI_CLASSIFIER_INTERVAL_MS);
  }

  return true;
}

String bestLabelFromResult(ei_impulse_result_t &result, float &confidence) {
  String best = "unknown";
  confidence = 0.0f;

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    float v = result.classification[ix].value;
    if (v > confidence) {
      confidence = v;
      best = result.classification[ix].label;
    }
  }

  best.toUpperCase();
  return best;
}

float anomalyScoreFromResult(ei_impulse_result_t &result) {
#if EI_CLASSIFIER_HAS_ANOMALY == 1
  return result.anomaly;
#else
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    String label = result.classification[ix].label;
    label.toUpperCase();
    if (label.indexOf("ANOM") >= 0) {
      return result.classification[ix].value;
    }
  }
  return 0.0f;
#endif
}

String statusFromLabelAndScore(String label, float anomalyScore, float confidence) {
  label.toUpperCase();

  if (label.indexOf("ANOM") >= 0) return "ANOMALY";
  if (label.indexOf("WARN") >= 0) return "WARNING";
  if (label.indexOf("NORMAL") >= 0) return "NORMAL";

  if (anomalyScore >= 0.70f) return "ANOMALY";
  if (anomalyScore >= 0.40f) return "WARNING";
  if (confidence < 0.50f) return "WARNING";
  return "NORMAL";
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting WiFi to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println("\nWiFi timeout, retrying...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }

  Serial.println();
  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  while (!mqttClient.connected()) {
    Serial.print("Connecting MQTT to ");
    Serial.print(MQTT_HOST);
    Serial.print(":");
    Serial.println(MQTT_PORT);

    String clientId = "aiot-ei-";
#ifdef BOARD_NAME
    clientId += BOARD_NAME;
#else
    clientId += "esp32";
#endif
    clientId += "-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT connected.");
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

void publishResult(String label, String status, float confidence, float anomalyScore) {
  StaticJsonDocument<768> doc;

  doc["demo"] = "1B";
  doc["method"] = "edge_impulse_real_mpu6050";
#ifdef BOARD_NAME
  doc["board"] = BOARD_NAME;
#else
  doc["board"] = "unknown";
#endif
  doc["device_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
  doc["label"] = label;
  doc["status"] = status;
  doc["confidence"] = confidence;
  doc["anomaly_score"] = anomalyScore;

  JsonObject f = doc["features"].to<JsonObject>();
  f["mean_magnitude"] = featureMeanMagnitude();
  f["peak_magnitude"] = featurePeakMagnitude();

  size_t last = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
  JsonArray lastSample = doc["last_acc_sample"].to<JsonArray>();
  lastSample.add(features[last + 0]);
  lastSample.add(features[last + 1]);
  lastSample.add(features[last + 2]);

  char payload[768];
  serializeJson(doc, payload);

  mqttClient.publish(MQTT_TOPIC, payload);
  Serial.println(payload);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1500);

  Serial.println("AIoT Edge Impulse Inference Firmware");
#ifdef BOARD_NAME
  Serial.print("Board: ");
  Serial.println(BOARD_NAME);
#endif

  if (!initMpu6050(mpu, true)) {
    while (true) {
      Serial.println("MPU6050 init failed. Check wiring.");
      delay(2000);
    }
  }

  connectWiFi();
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  uint32_t now = millis();

  if (now - lastInferenceMs >= INFERENCE_INTERVAL_MS) {
    lastInferenceMs = now;

    if (!sampleMpuIntoFeatureBuffer()) return;

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &raw_feature_get_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res != EI_IMPULSE_OK) {
      Serial.print("run_classifier failed: ");
      Serial.println(res);
      return;
    }

    Serial.println("Classification:");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      Serial.print("  ");
      Serial.print(result.classification[ix].label);
      Serial.print(": ");
      Serial.println(result.classification[ix].value, 5);
    }

    float confidence = 0.0f;
    String label = bestLabelFromResult(result, confidence);
    float anomalyScore = anomalyScoreFromResult(result);
    String status = statusFromLabelAndScore(label, anomalyScore, confidence);

    Serial.print("Best label: ");
    Serial.print(label);
    Serial.print(" | confidence: ");
    Serial.print(confidence, 5);
    Serial.print(" | anomaly_score: ");
    Serial.print(anomalyScore, 5);
    Serial.print(" | status: ");
    Serial.println(status);

    publishResult(label, status, confidence, anomalyScore);
  }
}
