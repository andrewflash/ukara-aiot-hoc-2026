# UKARA AIOT HOC 2026

Materi demo Deteksi Anomali berbasis AIoT (ESP32 + MQTT).
Membandingkan pendekatan rule-based (tanpa ML) dan Machine Learning
(Edge Impulse) untuk mendeteksi kondisi NORMAL / WARNING / ANOMALY.

## Struktur Proyek

```
01_Deteksi_Anomali/
  A_Tanpa_ML/                  Demo 1A - deteksi rule-based (moving average + threshold)
    Wokwi/                     Simulasi ESP32 + DHT22 (sensor suhu)
  B_Dengan_ML/                 Demo 1B - deteksi dengan ML (Edge Impulse)
    Wokwi/                     Simulasi (suhu) + versi hardware
    ESP32/                     Firmware PlatformIO (collect + inference, MPU6050)
    Model/                     Library Arduino hasil ekspor Edge Impulse (.zip)
    Training/                  Generator dataset training + manifest
    Testing/                   Generator dataset testing + manifest
  Dashboard/
    dashboard.html             Dashboard realtime via MQTT over WebSocket
```

## Alur Demo

```
   [ Sensor ]      [ ESP32 ]            [ MQTT Broker ]      [ Dashboard ]
  DHT22/MPU6050 -> deteksi anomali  ->  topic publish   ->  grafik realtime
                  (rule / ML)           (HiveMQ)
```

## Topik MQTT

```
Demo 1A (rule)  : aiot/demo/1a/rulebased
Demo 1B (ML)    : aiot/demo/1b/ml
```

## Cara Pakai Singkat

### Demo 1A - Tanpa ML
1. Buka folder Wokwi pada A_Tanpa_ML, jalankan simulasi sketch.ino.
2. ESP32 baca suhu DHT22, hitung moving average, terapkan threshold.
3. Status NORMAL/WARNING/ANOMALY dipublish ke MQTT.

### Demo 1B - Dengan ML (Edge Impulse)
1. Kumpulkan data getaran via firmware *_collect + edge-impulse-data-forwarder.
2. Latih model di Edge Impulse (Spectral Analysis + Classification).
3. Ekspor Arduino library, taruh di ESP32/.../lib/, build firmware *_infer.
4. Hasil inferensi dipublish ke MQTT.
   Detail lengkap: lihat README di B_Dengan_ML/ESP32/aiot_edge_impulse_platformio/

### Dashboard
1. Buka Dashboard/dashboard.html di browser.
2. Dashboard subscribe ke kedua topik dan menampilkan grafik realtime.

## Dataset (Demo 1B)

```
python Training/generate_training_dataset.py    -> edge_impulse_training_dataset/
python Testing/generate_testing_dataset.py      -> edge_impulse_testing_dataset/
```

Tiga kelas: normal, warning, anomaly (sample rate 50 Hz, 2 detik per sample).

## Kebutuhan

- ESP32 (DevKit V1 / ESP32-C3 Super Mini / XIAO ESP32-S3) atau simulator Wokwi
- Sensor DHT22 (Demo 1A) / MPU6050 (Demo 1B hardware)
- PlatformIO, Python 3, akun Edge Impulse
- Library: WiFi, PubSubClient, ArduinoJson, DHT
