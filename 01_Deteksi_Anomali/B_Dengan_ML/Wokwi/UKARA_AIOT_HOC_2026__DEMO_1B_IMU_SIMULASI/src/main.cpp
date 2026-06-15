#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// GANTI sesuai nama library Edge Impulse hasil export
#include <DemoAIoTUkara1_inferencing.h>

#define POT_PIN 34

// Wokwi WiFi
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";

// MQTT broker untuk dashboard
const char *MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char *MQTT_TOPIC = "aiot/demo/1b/ml";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

unsigned long lastInference = 0;
const unsigned long INFERENCE_INTERVAL_MS = 3000;

// Untuk signal Edge Impulse
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

float pseudoRandomNoise(float scale)
{
    return ((float)random(-1000, 1000) / 1000.0f) * scale;
}

float getSine(float frequency, float t, float amplitude)
{
    return amplitude * sinf(2.0f * PI * frequency * t);
}

String getSimulatedCondition(int potValue)
{
    if (potValue < 1365)
    {
        return "normal";
    }

    if (potValue < 2730)
    {
        return "warning";
    }

    return "anomaly";
}

void generateSyntheticAccelerometerData()
{
    /*
      Edge Impulse classifier butuh buffer features dengan panjang:
      EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE.

      Untuk data accelerometer 3 axis:
      features = accX, accY, accZ, accX, accY, accZ, ...
    */

    int potValue = analogRead(POT_PIN);
    String condition = getSimulatedCondition(potValue);

    int samplesPerFrame = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;

    if (samplesPerFrame != 3)
    {
        Serial.println("WARNING: Model input is not 3-axis accelerometer format.");
        Serial.print("EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME = ");
        Serial.println(samplesPerFrame);
    }

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
    {
        float sampleIndex = (float)(ix / EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME);
        float t = sampleIndex * ((float)EI_CLASSIFIER_INTERVAL_MS / 1000.0f);

        float accX = 0.0;
        float accY = 0.0;
        float accZ = 9.8;

        if (condition == "normal")
        {
            accX = pseudoRandomNoise(0.15);
            accY = pseudoRandomNoise(0.15);
            accZ = 9.8 + pseudoRandomNoise(0.15);
        }

        else if (condition == "warning")
        {
            accX = getSine(3.0, t, 1.2) + pseudoRandomNoise(0.25);
            accY = getSine(4.0, t, 0.8) + pseudoRandomNoise(0.25);
            accZ = 9.8 + getSine(5.0, t, 1.0) + pseudoRandomNoise(0.25);
        }

        else
        {
            accX = getSine(8.0, t, 4.0) + pseudoRandomNoise(0.8);
            accY = getSine(9.0, t, 3.2) + pseudoRandomNoise(0.8);
            accZ = 9.8 + getSine(10.0, t, 4.5) + pseudoRandomNoise(1.0);

            // Spike untuk membuat anomali lebih jelas
            if (random(0, 100) < 8)
            {
                accX += random(40, 80) / 10.0f;
                accY += random(30, 60) / 10.0f;
                accZ += random(40, 80) / 10.0f;
            }
        }

        features[ix + 0] = accX;

        if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 1)
        {
            features[ix + 1] = accY;
        }

        if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 2)
        {
            features[ix + 2] = accZ;
        }

        delay(EI_CLASSIFIER_INTERVAL_MS);
    }
}

String getBestLabel(ei_impulse_result_t result, float &confidence)
{
    String bestLabel = "unknown";
    confidence = 0.0f;

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        float value = result.classification[ix].value;

        if (value > confidence)
        {
            confidence = value;
            bestLabel = result.classification[ix].label;
        }
    }

    bestLabel.toUpperCase();
    return bestLabel;
}

String normalizeStatus(String label, float anomalyScore, float confidence)
{
    label.toUpperCase();

    if (label.indexOf("ANOM") >= 0)
    {
        return "ANOMALY";
    }

    if (label.indexOf("WARN") >= 0)
    {
        return "WARNING";
    }

    if (label.indexOf("NORMAL") >= 0)
    {
        return "NORMAL";
    }

    // Fallback jika label tidak sesuai
    if (anomalyScore >= 0.70)
    {
        return "ANOMALY";
    }

    if (anomalyScore >= 0.40)
    {
        return "WARNING";
    }

    if (confidence < 0.50)
    {
        return "WARNING";
    }

    return "NORMAL";
}

float calculateFeatureMagnitudeMean()
{
    float sum = 0;
    int count = 0;

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
    {
        float x = features[ix + 0];
        float y = 0;
        float z = 0;

        if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 1)
        {
            y = features[ix + 1];
        }

        if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 2)
        {
            z = features[ix + 2];
        }

        float mag = sqrtf(x * x + y * y + z * z);
        sum += mag;
        count++;
    }

    if (count == 0)
        return 0;
    return sum / count;
}

float calculateFeatureMagnitudePeak()
{
    float peak = 0;

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
    {
        float x = features[ix + 0];
        float y = 0;
        float z = 0;

        if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 1)
        {
            y = features[ix + 1];
        }

        if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 2)
        {
            z = features[ix + 2];
        }

        float mag = sqrtf(x * x + y * y + z * z);

        if (mag > peak)
        {
            peak = mag;
        }
    }

    return peak;
}

void publishResult(String simulatedCondition, String label, String status, float confidence, float anomalyScore)
{
    StaticJsonDocument<768> doc;

    doc["demo"] = "1B";
    doc["method"] = "edge_impulse_wokwi";
    doc["device_id"] = "esp32-wokwi-edge-impulse";
    doc["simulated_condition"] = simulatedCondition;
    doc["label"] = label;
    doc["status"] = status;
    doc["confidence"] = confidence;
    doc["anomaly_score"] = anomalyScore;

    JsonObject featureSummary = doc.createNestedObject("features");
    featureSummary["mean_magnitude"] = calculateFeatureMagnitudeMean();
    featureSummary["peak_magnitude"] = calculateFeatureMagnitudePeak();

    JsonArray lastSample = doc.createNestedArray("last_acc_sample");
    size_t last = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;

    lastSample.add(features[last + 0]);

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 1)
    {
        lastSample.add(features[last + 1]);
    }

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME > 2)
    {
        lastSample.add(features[last + 2]);
    }

    char payload[768];
    serializeJson(doc, payload);

    if (!mqttClient.publish(MQTT_TOPIC, payload)) {
        Serial.println("MQTT publish FAILED (cek buffer size / koneksi)");
    }

    Serial.println("MQTT payload:");
    Serial.println(payload);
}

void connectWiFi()
{
    Serial.print("Connecting to WiFi");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

void connectMQTT()
{
    while (!mqttClient.connected())
    {
        Serial.print("Connecting to MQTT... ");

        String clientId = "demo1b-ei-wokwi-";
        clientId += String(random(0xffff), HEX);

        if (mqttClient.connect(clientId.c_str()))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retry in 2 seconds");
            delay(2000);
        }
    }
}

void runEdgeImpulseInference()
{
    int potValue = analogRead(POT_PIN);
    String simulatedCondition = getSimulatedCondition(potValue);

    Serial.println();
    Serial.println("=== Generating simulated accelerometer data ===");
    Serial.print("Pot value: ");
    Serial.println(potValue);
    Serial.print("Simulated condition: ");
    Serial.println(simulatedCondition);

    generateSyntheticAccelerometerData();

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &raw_feature_get_data;

    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res != EI_IMPULSE_OK)
    {
        Serial.print("run_classifier failed: ");
        Serial.println(res);
        return;
    }

    Serial.println("Edge Impulse result:");

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        Serial.print(result.classification[ix].label);
        Serial.print(": ");
        Serial.println(result.classification[ix].value, 5);
    }

    float anomalyScore = 0.0f;

#if EI_CLASSIFIER_HAS_ANOMALY == 1
    anomalyScore = result.anomaly;
#else
    // Kalau impulse hanya classification, pakai confidence label anomali sebagai anomaly score.
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    {
        String label = result.classification[ix].label;
        label.toUpperCase();

        if (label.indexOf("ANOM") >= 0)
        {
            anomalyScore = result.classification[ix].value;
        }
    }
#endif

    float confidence = 0.0f;
    String label = getBestLabel(result, confidence);
    String status = normalizeStatus(label, anomalyScore, confidence);

    Serial.print("Best label: ");
    Serial.println(label);
    Serial.print("Confidence: ");
    Serial.println(confidence, 5);
    Serial.print("Anomaly score: ");
    Serial.println(anomalyScore, 5);
    Serial.print("Status: ");
    Serial.println(status);

    publishResult(simulatedCondition, label, status, confidence, anomalyScore);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(POT_PIN, INPUT);

    randomSeed(analogRead(POT_PIN));

    connectWiFi();

    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    // Payload 1B (~300 byte) melebihi buffer default PubSubClient (256 byte),
    // tanpa ini publish() diam-diam gagal dan dashboard tidak menerima data.
    mqttClient.setBufferSize(512);

    Serial.println("Demo 1B started: Wokwi + Edge Impulse");
    Serial.println("Move potentiometer to simulate normal/warning/anomaly vibration.");
}

void loop()
{
    if (!mqttClient.connected())
    {
        connectMQTT();
    }

    mqttClient.loop();

    unsigned long now = millis();

    if (now - lastInference >= INFERENCE_INTERVAL_MS)
    {
        lastInference = now;
        runEdgeImpulseInference();
    }
}