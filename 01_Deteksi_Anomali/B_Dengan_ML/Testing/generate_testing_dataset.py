import math
import random
import csv
import os

# =========================
# CONFIG
# =========================

OUTPUT_DIR = "edge_impulse_testing_dataset"

SAMPLE_RATE = 50          # Hz
DURATION_SEC = 2          # detik per file/sample
N = SAMPLE_RATE * DURATION_SEC

FILES_PER_CLASS = 15

CLASSES = ["normal", "warning", "anomaly", "borderline"]

random.seed(2027)


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

    phase_x = random.uniform(0, math.pi)
    phase_y = random.uniform(0, math.pi)
    phase_z = random.uniform(0, math.pi)

    for i in range(N):
        t = i / SAMPLE_RATE
        timestamp_ms = int(i * (1000 / SAMPLE_RATE))

        if label == "normal":
            acc_x = noise(0.20)
            acc_y = noise(0.20)
            acc_z = 9.8 + noise(0.20)

        elif label == "warning":
            fx = random.uniform(2.5, 4.2)
            fy = random.uniform(3.0, 5.4)
            fz = random.uniform(4.0, 6.4)

            acc_x = sine(fx, t, random.uniform(0.9, 1.6), phase_x) + noise(0.35)
            acc_y = sine(fy, t, random.uniform(0.7, 1.3), phase_y) + noise(0.35)
            acc_z = 9.8 + sine(fz, t, random.uniform(0.8, 1.5), phase_z) + noise(0.35)

        elif label == "anomaly":
            fx = random.uniform(7.0, 10.5)
            fy = random.uniform(8.0, 11.5)
            fz = random.uniform(8.5, 12.5)

            acc_x = sine(fx, t, random.uniform(3.0, 5.2), phase_x) + noise(1.0)
            acc_y = sine(fy, t, random.uniform(2.5, 4.8), phase_y) + noise(1.0)
            acc_z = 9.8 + sine(fz, t, random.uniform(3.5, 5.8), phase_z) + noise(1.2)

            acc_x = add_random_spike(acc_x, 0.10, 3.0, 8.0)
            acc_y = add_random_spike(acc_y, 0.08, 3.0, 7.0)
            acc_z = add_random_spike(acc_z, 0.10, 4.0, 9.0)

        elif label == "borderline":
            # Kondisi abu-abu antara warning dan anomaly.
            # Bisa dipakai untuk test manual.
            fx = random.uniform(5.0, 7.2)
            fy = random.uniform(5.5, 7.8)
            fz = random.uniform(6.0, 8.3)

            acc_x = sine(fx, t, random.uniform(1.8, 3.0), phase_x) + noise(0.65)
            acc_y = sine(fy, t, random.uniform(1.5, 2.8), phase_y) + noise(0.65)
            acc_z = 9.8 + sine(fz, t, random.uniform(1.8, 3.2), phase_z) + noise(0.75)

            acc_x = add_random_spike(acc_x, 0.03, 2.0, 5.0)
            acc_y = add_random_spike(acc_y, 0.03, 2.0, 5.0)
            acc_z = add_random_spike(acc_z, 0.03, 2.0, 6.0)

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

            filename = f"{label}_test_{idx:03d}.csv"
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

    print("Testing dataset generated successfully.")
    print(f"Output folder : {OUTPUT_DIR}")
    print(f"Classes       : {CLASSES}")
    print(f"Files/class   : {FILES_PER_CLASS}")
    print(f"Sample rate   : {SAMPLE_RATE} Hz")
    print(f"Duration      : {DURATION_SEC} seconds")
    print(f"Samples/file  : {N}")
    print(f"Manifest      : {manifest_path}")


if __name__ == "__main__":
    main()