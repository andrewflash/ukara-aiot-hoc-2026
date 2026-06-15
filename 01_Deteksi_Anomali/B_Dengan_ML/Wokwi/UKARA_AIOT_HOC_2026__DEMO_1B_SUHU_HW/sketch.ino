#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// Wokwi WiFi
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// MQTT broker demo
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// Topic khusus Demo 1B
const char* MQTT_TOPIC = "aiot/demo/1b/ml";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL_MS = 3000;

// Sampling config
const int SAMPLE_COUNT = 40;
const int SAMPLE_DELAY_MS = 25;

struct Features {
  float meanMagnitude;
  float peakMagnitude;
  float variability;
};

struct MLResult {
  String label;
  String status;
  float confidence;
  float anomalyScore;
};

void connectWiFi() {
  Serial.print("Connecting to WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT... ");

    String clientId = "demo1b-wokwi-ml-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retry in 2 seconds");
      delay(2000);
    }
  }
}

bool initMPU6050() {
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050");
    return false;
  }

  Serial.println("MPU6050 found");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  return true;
}

float getMagnitude(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

Features extractFeatures() {
  float sumMagnitude = 0;
  float peakMagnitude = 0;

  float values[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float magnitude = getMagnitude(
      a.acceleration.x,
      a.acceleration.y,
      a.acceleration.z
    );

    values[i] = magnitude;
    sumMagnitude += magnitude;

    if (magnitude > peakMagnitude) {
      peakMagnitude = magnitude;
    }

    delay(SAMPLE_DELAY_MS);
  }

  float meanMagnitude = sumMagnitude / SAMPLE_COUNT;

  float variance = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    float diff = values[i] - meanMagnitude;
    variance += diff * diff;
  }

  variance = variance / SAMPLE_COUNT;
  float variability = sqrt(variance);

  Features f;
  f.meanMagnitude = meanMagnitude;
  f.peakMagnitude = peakMagnitude;
  f.variability = variability;

  return f;
}

MLResult runTinyMLSimulation(Features f) {
  /*
    Ini simulasi model ML sederhana.

    Ide model:
    - meanMagnitude melihat rata-rata kekuatan gerakan.
    - peakMagnitude melihat puncak hentakan.
    - variability melihat perubahan pola gerakan.

    Dalam ML sungguhan, bobot/aturan ini berasal dari training.
    Di demo ini, bobot dibuat manual agar konsep inferensi mudah dipahami.
  */

  float normalScore = 0.0;
  float warningScore = 0.0;
  float anomalyScore = 0.0;

  // Baseline gravitasi sekitar 9.8 m/s^2 saat sensor diam.
  float deviationFromGravity = abs(f.meanMagnitude - 9.8);

  // Skor dibuat dari kombinasi fitur.
  normalScore = 1.0 - min(1.0f, (deviationFromGravity + f.variability) / 5.0f);

  warningScore = min(1.0f, (f.variability + deviationFromGravity) / 4.0f);

  anomalyScore = min(1.0f, ((f.peakMagnitude - 12.0f) / 8.0f) + (f.variability / 6.0f));

  if (anomalyScore < 0) anomalyScore = 0;
  if (normalScore < 0) normalScore = 0;

  MLResult result;

  if (anomalyScore >= 0.70) {
    result.label = "ANOMALY";
    result.status = "ANOMALY";
    result.confidence = anomalyScore;
    result.anomalyScore = anomalyScore;
  } else if (warningScore >= 0.45) {
    result.label = "WARNING";
    result.status = "WARNING";
    result.confidence = warningScore;
    result.anomalyScore = max(anomalyScore, warningScore * 0.6f);
  } else {
    result.label = "NORMAL";
    result.status = "NORMAL";
    result.confidence = normalScore;
    result.anomalyScore = anomalyScore;
  }

  return result;
}

void publishData(Features f, MLResult result) {
  StaticJsonDocument<512> doc;

  doc["demo"] = "1B";
  doc["method"] = "ml_simulation_wokwi";
  doc["device_id"] = "esp32-wokwi-mpu6050";
  doc["label"] = result.label;
  doc["status"] = result.status;
  doc["confidence"] = result.confidence;
  doc["anomaly_score"] = result.anomalyScore;

  JsonObject features = doc.createNestedObject("features");
  features["mean_magnitude"] = f.meanMagnitude;
  features["peak_magnitude"] = f.peakMagnitude;
  features["variability"] = f.variability;

  char payload[512];
  serializeJson(doc, payload);

  mqttClient.publish(MQTT_TOPIC, payload);

  Serial.println("Published:");
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);

  if (!initMPU6050()) {
    while (1) {
      delay(1000);
    }
  }

  connectWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("Demo 1B Wokwi started: ML Simulation with MPU6050");
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  unsigned long now = millis();

  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;

    Features f = extractFeatures();
    MLResult result = runTinyMLSimulation(f);

    Serial.println("=== ML Simulation Result ===");
    Serial.print("Mean magnitude: ");
    Serial.println(f.meanMagnitude);
    Serial.print("Peak magnitude: ");
    Serial.println(f.peakMagnitude);
    Serial.print("Variability: ");
    Serial.println(f.variability);
    Serial.print("Label: ");
    Serial.println(result.label);
    Serial.print("Confidence: ");
    Serial.println(result.confidence);
    Serial.print("Anomaly score: ");
    Serial.println(result.anomalyScore);
    Serial.println();

    publishData(f, result);
  }
}