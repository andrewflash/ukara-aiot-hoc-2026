#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// Wokwi WiFi
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// MQTT broker demo
const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// Topic khusus Demo 1A
const char* MQTT_TOPIC = "aiot/demo/1a/rulebased";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Moving average config
const int WINDOW_SIZE = 5;
float tempWindow[WINDOW_SIZE];
int tempIndex = 0;
int tempCount = 0;

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL_MS = 3000;

float updateMovingAverage(float newValue) {
  tempWindow[tempIndex] = newValue;
  tempIndex = (tempIndex + 1) % WINDOW_SIZE;

  if (tempCount < WINDOW_SIZE) {
    tempCount++;
  }

  float sum = 0;
  for (int i = 0; i < tempCount; i++) {
    sum += tempWindow[i];
  }

  return sum / tempCount;
}

String getRuleStatus(float temperature, float movingAverage) {
  // Rule 1: threshold utama
  if (temperature > 35.0) {
    return "ANOMALY";
  }

  if (temperature >= 30.0) {
    return "WARNING";
  }

  // Rule 2: perubahan mendadak dari moving average
  if (tempCount >= WINDOW_SIZE) {
    float deviation = abs(temperature - movingAverage);

    if (deviation > 5.0) {
      return "ANOMALY";
    }

    if (deviation > 3.0) {
      return "WARNING";
    }
  }

  return "NORMAL";
}

float getRuleScore(float temperature, float movingAverage) {
  float score = 0.10;

  if (temperature >= 30.0 && temperature <= 35.0) {
    score = 0.55;
  }

  if (temperature > 35.0) {
    score = 0.90;
  }

  if (tempCount >= WINDOW_SIZE) {
    float deviation = abs(temperature - movingAverage);

    if (deviation > 5.0) {
      score = max(score, 0.85f);
    } else if (deviation > 3.0) {
      score = max(score, 0.60f);
    }
  }

  return score;
}

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

    String clientId = "demo1a-rulebased-";
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

void publishData(float temperature, float humidity, float movingAverage, String status, float score) {
  StaticJsonDocument<384> doc;

  doc["demo"] = "1A";
  doc["method"] = "rule_based";
  doc["device_id"] = "esp32-wokwi-dht22";
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["moving_average"] = movingAverage;
  doc["status"] = status;
  doc["score"] = score;

  char payload[384];
  serializeJson(doc, payload);

  mqttClient.publish(MQTT_TOPIC, payload);

  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  connectWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("Demo 1A started: Rule-Based Anomaly Detection");
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  unsigned long now = millis();

  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("DHT read failed");
      return;
    }

    float movingAverage = updateMovingAverage(temperature);
    String status = getRuleStatus(temperature, movingAverage);
    float score = getRuleScore(temperature, movingAverage);

    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.print(" | Humidity: ");
    Serial.print(humidity);
    Serial.print(" | MA: ");
    Serial.print(movingAverage);
    Serial.print(" | Status: ");
    Serial.print(status);
    Serial.print(" | Score: ");
    Serial.println(score);

    publishData(temperature, humidity, movingAverage, status, score);
  }
}