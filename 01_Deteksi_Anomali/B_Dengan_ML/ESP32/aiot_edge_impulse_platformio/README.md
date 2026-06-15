# Proyek PlatformIO AIoT Edge Impulse + MPU6050

Proyek ini berisi dua mode firmware:

1. `*_collect` — firmware pengumpulan data untuk Edge Impulse Data Forwarder
2. `*_infer` — firmware inferensi Edge Impulse + output ke dashboard MQTT

Environment yang didukung:

| Board | Env pengumpulan data | Env inferensi |
|---|---|---|
| ESP32 DevKit V1 Type-C | `esp32dev_collect` | `esp32dev_infer` |
| ESP32-C3 Super Mini | `esp32c3_collect` | `esp32c3_infer` |
| Seeed XIAO ESP32-S3 | `xiao_esp32s3_collect` | `xiao_esp32s3_infer` |

## Pengkabelan (Wiring)

| MPU6050 | ESP32 DevKit V1 | ESP32-C3 Super Mini | XIAO ESP32-S3 |
|---|---:|---:|---:|
| VCC | 3V3 | 3V3 | 3V3 |
| GND | GND | GND | GND |
| SDA | GPIO21 | GPIO4* | GPIO5 / D4 |
| SCL | GPIO22 | GPIO5* | GPIO6 / D5 |

\* Sebagian board ESP32-C3 Super Mini memakai GPIO8/GPIO9 untuk I2C. Jika scanner tidak menemukan MPU6050, ubah `I2C_SDA_PIN` dan `I2C_SCL_PIN` di `platformio.ini`.

## Firmware Pengumpulan Data

Upload:

```bash
pio run -e esp32dev_collect -t upload
pio run -e esp32c3_collect -t upload
pio run -e xiao_esp32s3_collect -t upload
```

Jalankan Edge Impulse Data Forwarder:

```bash
edge-impulse-data-forwarder --frequency 50
```

Nama axis:

```text
accX, accY, accZ
```

Saran label:

| Label | Aksi |
|---|---|
| normal | Letakkan board diam di atas meja |
| warning | Goyangan ringan / getaran sedang |
| anomaly | Goyangan kuat / benturan / getaran agresif |

## Pengaturan Impulse di Edge Impulse

Rekomendasi:

| Item | Pengaturan |
|---|---|
| Input | Time series |
| Frekuensi | 50 Hz |
| Window size | 2000 ms |
| Window increase | 500 ms |
| Processing block | Spectral Analysis |
| Learning block | Classification |
| Kelas | normal, warning, anomaly |

Setelah training:

```text
Deployment -> Arduino library -> Build -> Download .zip
```

Ekstrak library ke dalam folder `lib/`, contohnya:

```text
lib/DemoAIoTUkara1_inferencing/
```

Lalu sesuaikan baris `#include` di `src/infer_main.cpp` bila nama header
hasil export berbeda:

```cpp
// GANTI sesuai nama library Edge Impulse hasil export
#include <DemoAIoTUkara1_inferencing.h>
```

## Firmware Inferensi

Edit konfigurasi WiFi & MQTT langsung di bagian atas `src/infer_main.cpp`:

```cpp
// ==== WiFi & MQTT config (edit sesuai kebutuhan) ====
const char *WIFI_SSID = "TEST_SSID";
const char *WIFI_PASSWORD = "PASSWORD";

const char *MQTT_HOST = "mqtt-dashboard.com";
const int MQTT_PORT = 1883;
const char *MQTT_TOPIC = "aiot/demo/1b/ml";
```

Upload:

```bash
pio run -e esp32dev_infer -t upload
pio run -e esp32c3_infer -t upload
pio run -e xiao_esp32s3_infer -t upload
```

Topik output MQTT:

```text
aiot/demo/1b/ml
```

Contoh payload:

```json
{
  "demo": "1B",
  "method": "edge_impulse_real_mpu6050",
  "board": "ESP32_DEVKIT_V1",
  "device_id": "abcd1234",
  "label": "NORMAL",
  "status": "NORMAL",
  "confidence": 0.91,
  "anomaly_score": 0.03,
  "features": {
    "mean_magnitude": 9.82,
    "peak_magnitude": 10.10
  },
  "last_acc_sample": [0.02, -0.01, 9.81]
}
```

## Catatan Build

Library Edge Impulse SDK menghasilkan ratusan file objek. Agar perintah
linker tidak melebihi batas OS (`Argument list too long`), daftar objek
dialihkan ke response file lewat skrip `scripts/use_response_file.py`
(otomatis dipanggil via `extra_scripts` di `platformio.ini`). Tidak perlu
diubah.

## Troubleshooting

### MPU6050 tidak ditemukan

- Periksa VCC/GND.
- Periksa SDA/SCL.
- Untuk ESP32-C3 Super Mini, coba GPIO8/GPIO9 bila GPIO4/GPIO5 tidak jalan.

### Data forwarder tidak membaca serial

- Tutup Serial Monitor PlatformIO terlebih dahulu.
- Firmware collect hanya mencetak baris CSV numerik.
- Gunakan `edge-impulse-data-forwarder --frequency 50`.

### Build inferensi gagal

- Pastikan library Arduino Edge Impulse ada di dalam folder `lib/`.
- Pastikan baris `#include` di `src/infer_main.cpp` sesuai dengan nama
  header yang dihasilkan.

### Build ESP32-C3 gagal saat memasang toolchain

- ESP32-C3 (RISC-V) memerlukan toolchain terpisah yang diunduh saat build
  pertama. Jika muncul `No space left on device`, kosongkan ruang disk lalu
  ulangi build.
