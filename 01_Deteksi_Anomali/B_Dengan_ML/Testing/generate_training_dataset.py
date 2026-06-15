import math
import random
import csv
import os

# =========================
# CONFIG
# =========================

OUTPUT_DIR = "edge_impulse_training_dataset"

SAMPLE_RATE = 50          # Hz
DURATION_SEC = 2          # detik per file/sample
N = SAMPLE_RATE * DURATION_SEC

FILES_PER_CLASS = 50

CLASSES = ["normal", "warning", "anomaly"]

# Seed supaya hasil bisa direproduksi
random.seed(2026)


# =========================
# HELPER
# =========================

def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def noise(scale):
    return random.uniform(-scale, scale)


def sine(freq, t, amp, phase=0.0):
    return amp * math.sin(2 * math.pi * freq * t + phase)


def add_random_spike(value, probability, min_amp, max_amp):
    if random.random() < probability:
        return value + random.uniform(min_amp, max_amp)
    return value


# =========================
# DATA GENERATOR
# =========================

def generate_window(label):
    rows = []

    # Phase dibuat random supaya setiap file tidak identik
    phase_x = random.uniform(0, math.pi)
    phase_y = random.uniform(0, math.pi)
    phase_z = random.uniform(0, math.pi)

    # Variasi amplitude/frequency antar file
    amp_variation = random.uniform(0.85, 1.15)
    freq_variation = random.uniform(0.90, 1.10)

    for i in range(N):
        t = i / SAMPLE_RATE
        timestamp_ms = int(i * (1000 / SAMPLE_RATE))

        if label == "normal":
            # Sensor relatif diam.
            # Z sekitar gravitasi bumi 9.8 m/s^2, X/Y mendekati 0.
            acc_x = noise(0.12)
            acc_y = noise(0.12)
            acc_z = 9.8 + noise(0.12)

        elif label == "warning":
            # Getaran sedang.
            # Masih relatif aman, tapi sudah ada pola vibrasi.
            fx = 3.0 * freq_variation
            fy = 4.0 * freq_variation
            fz = 5.0 * freq_variation

            acc_x = sine(fx, t, 1.2 * amp_variation, phase_x) + noise(0.25)
            acc_y = sine(fy, t, 0.9 * amp_variation, phase_y) + noise(0.25)
            acc_z = 9.8 + sine(fz, t, 1.1 * amp_variation, phase_z) + noise(0.25)

        elif label == "anomaly":
            # Getaran besar dan tidak stabil.
            # Ada vibrasi kuat + spike sesekali.
            fx = 8.0 * freq_variation
            fy = 9.0 * freq_variation
            fz = 10.0 * freq_variation

            acc_x = sine(fx, t, 4.0 * amp_variation, phase_x) + noise(0.80)
            acc_y = sine(fy, t, 3.5 * amp_variation, phase_y) + noise(0.80)
            acc_z = 9.8 + sine(fz, t, 4.8 * amp_variation, phase_z) + noise(1.00)

            # Spike random agar model melihat pola hentakan/anomali
            acc_x = add_random_spike(acc_x, 0.08, 3.0, 8.0)
            acc_y = add_random_spike(acc_y, 0.08, 3.0, 7.0)
            acc_z = add_random_spike(acc_z, 0.08, 4.0, 9.0)

        else:
            raise ValueError(f"Unknown label: {label}")

        rows.append([timestamp_ms, acc_x, acc_y, acc_z])

    return rows


def write_csv(filepath, rows):
    with open(filepath, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "accX", "accY", "accZ"])
        writer.writerows(rows)


# =========================
# MAIN
# =========================

def main():
    ensure_dir(OUTPUT_DIR)

    manifest_rows = []

    for label in CLASSES:
        label_dir = os.path.join(OUTPUT_DIR, label)
        ensure_dir(label_dir)

        for idx in range(FILES_PER_CLASS):
            rows = generate_window(label)

            filename = f"{label}_train_{idx:03d}.csv"
            filepath = os.path.join(label_dir, filename)

            write_csv(filepath, rows)

            manifest_rows.append([
                filename,
                label,
                SAMPLE_RATE,
                DURATION_SEC,
                N
            ])

    manifest_path = os.path.join(OUTPUT_DIR, "manifest.csv")

    with open(manifest_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "filename",
            "label",
            "sample_rate_hz",
            "duration_sec",
            "sample_count"
        ])
        writer.writerows(manifest_rows)

    print("Training dataset generated successfully.")
    print(f"Output folder : {OUTPUT_DIR}")
    print(f"Classes       : {CLASSES}")
    print(f"Files/class   : {FILES_PER_CLASS}")
    print(f"Sample rate   : {SAMPLE_RATE} Hz")
    print(f"Duration      : {DURATION_SEC} seconds")
    print(f"Samples/file  : {N}")
    print(f"Manifest      : {manifest_path}")


if __name__ == "__main__":
    main()