#!/usr/bin/env python3
"""
Generate multitone excitation signals for loudspeaker non-linearity measurement.
Recommended by Klippel for intermodulation distortion analysis.
Supports multiple numbers of tones (sweep over tone counts).

Manifest format matches the structure of a previous single-tone dataset:
    signal_id, frequency_hz, sample_rate_hz, duration_sec, amplitude, sample_count, relative_path
where frequency_hz is a comma-separated list of all multitone frequencies.
"""

import argparse
import csv
import os
import shutil
from pathlib import Path

import numpy as np

# ---------------------------- CONFIGURATION ---------------------------------
SAMPLE_RATE = 192000          # Hz
DURATION_SEC = 1.4            # seconds
FADE_DURATION_SEC = 0.05      # seconds attack/release
AMPLITUDES = (0.7, 0.8, 0.9)  # peak amplitude of the multitone sum

# Multitone frequency range (logarithmic distribution)
F_MIN_HZ = 50
F_MAX_HZ = 250
NUM_TONES_MIN = 6             # minimum number of sine components
NUM_TONES_MAX = 15            # maximum number of sine components
NUM_TONES_STEP = 1            # step when iterating over tone counts

# Distribution: 'log' or 'lin' (log gives better bin separation at low frequencies)
DISTRIBUTION = 'log'
# ----------------------------------------------------------------------------

def generate_multitone_frequencies(
    f_min: float,
    f_max: float,
    num_tones: int,
    distribution: str = 'log'
) -> list[float]:
    """Generate frequencies for multitone signal."""
    if distribution == 'log':
        # Logarithmic spacing: f = f_min * (f_max/f_min)^(i/(num_tones-1))
        freqs = f_min * (f_max / f_min) ** (np.linspace(0, 1, num_tones))
    else:
        # Linear spacing
        freqs = np.linspace(f_min, f_max, num_tones)
    # Optional: add small random offset to avoid harmonic clustering (commented)
    # freqs += np.random.uniform(-1, 1, num_tones) * (freqs[1]-freqs[0])*0.1
    return [float(f) for f in freqs]

def build_multitone_signal(
    frequencies: list[float],
    amplitude: float,
    duration_sec: float,
    sample_rate: int,
    fade_sec: float
) -> np.ndarray:
    """Generate multitone signal with Hanning fade in/out and peak normalisation."""
    n_samples = int(round(sample_rate * duration_sec))
    fade_samples = int(round(sample_rate * fade_sec))
    fade_samples = min(fade_samples, n_samples // 2)

    # Time array
    t = np.arange(n_samples) / sample_rate

    # Sum sine waves with equal amplitude per component
    signal_sum = np.zeros(n_samples, dtype=np.float64)
    for f in frequencies:
        signal_sum += np.sin(2.0 * np.pi * f * t)

    # Normalise to desired peak amplitude (to avoid clipping)
    peak = np.max(np.abs(signal_sum))
    if peak > 0:
        signal_sum = signal_sum / peak * amplitude
    else:
        signal_sum = np.zeros_like(signal_sum)

    # Apply Hanning fade in/out
    if fade_samples > 0:
        window = np.ones(n_samples)
        # Fade in
        fade_in = 0.5 * (1.0 - np.cos(np.pi * np.arange(fade_samples) / fade_samples))
        window[:fade_samples] = fade_in
        # Fade out
        fade_out = 0.5 * (1.0 - np.cos(np.pi * np.arange(fade_samples) / fade_samples))
        window[-fade_samples:] = fade_out[::-1]  # reverse
        signal_sum = signal_sum * window

    return signal_sum.astype(np.float32)

def write_signal(path: Path, signal: np.ndarray) -> None:
    """Write float32 signal to binary file."""
    with path.open("wb") as f:
        f.write(signal.tobytes())

def generate_dataset(output_dir: Path) -> None:
    """Generate multitone signals for all tone counts, all amplitudes,
    writing a manifest with columns:
        signal_id, frequency_hz, sample_rate_hz, duration_sec,
        amplitude, sample_count, relative_path
    """
    # Create output directories
    signals_dir = output_dir / "signals"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    signals_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = output_dir / "manifest.csv"
    with manifest_path.open("w", newline="", encoding="utf-8") as mf:
        writer = csv.writer(mf, lineterminator="\n")
        writer.writerow([
            "signal_id",
            "frequency_hz",          # <-- имя колонки остаётся как у вас
            "sample_rate_hz",
            "duration_sec",
            "amplitude",
            "sample_count",
            "relative_path"
        ])

        num_tones_range = range(NUM_TONES_MIN, NUM_TONES_MAX + 1, NUM_TONES_STEP)
        for num_tones in num_tones_range:
            frequencies = generate_multitone_frequencies(
                F_MIN_HZ, F_MAX_HZ, num_tones, DISTRIBUTION
            )
            freq_id_str = "_".join(f"{int(round(f))}" for f in frequencies)
            # Используем пробел вместо запятой, чтобы не ломать CSV-парсер
            freq_manifest_str = " ".join(f"{f:.2f}" for f in frequencies)

            for amplitude in AMPLITUDES:
                amp_label = int(round(amplitude * 100))
                signal_id = f"multitone_t{num_tones:02d}_a{amp_label:02d}_{freq_id_str}"
                signal = build_multitone_signal(
                    frequencies, amplitude, DURATION_SEC, SAMPLE_RATE, FADE_DURATION_SEC
                )
                rel_path = Path("signals") / f"{signal_id}.f32"
                write_signal(output_dir / rel_path, signal)

                writer.writerow([
                    signal_id,
                    freq_manifest_str,   # теперь "50.00 69.28 95.14 …"
                    SAMPLE_RATE,
                    DURATION_SEC,
                    amplitude,
                    len(signal),
                    rel_path.as_posix()
                ])

def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate multitone dataset with variable number of tones."
    )
    default_output = Path(__file__).resolve().parents[1] / "dataset" / "multitone_signals"
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=default_output,
        help=f"Output directory. Default: {default_output}"
    )
    return parser.parse_args()

def main():
    args = parse_args()
    output_dir = args.output_dir.resolve()
    os.makedirs(output_dir, exist_ok=True)
    generate_dataset(output_dir)
    print(f"Multitone dataset generated in: {output_dir}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())