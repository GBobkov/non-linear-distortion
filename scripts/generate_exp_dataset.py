#!/usr/bin/env python3
"""
Generate Exponential Sine Sweep (ESS) signals with multiple parameter variations.
Based on the method by Angelo Farina.

Manifest format matches the structure of a previous single-tone dataset:
    signal_id, frequency_hz, sample_rate_hz, duration_sec, amplitude, sample_count, relative_path
where frequency_hz is a string representing the sweep range (e.g. "20-200").
"""

import argparse
import csv
import os
import shutil
from pathlib import Path

import numpy as np
from scipy.signal import chirp

# ---------------------------- CONFIGURATION ---------------------------------
SAMPLE_RATE = 192000                     # Hz
FADE_DURATION_SEC = 0.05                 # seconds (attack/release to avoid clicks)
SILENCE_DURATION_SEC = 0.1               # seconds of silence before and after the sweep

# Parameter variations (cartesian product)
DURATIONS_SEC = (1.4,)                    # different sweep lengths
FREQ_STARTS_HZ = (20, 50)                # start frequencies
FREQ_STOPS_HZ = (200, 300)               # stop frequencies
AMPLITUDES = (0.7, 0.8, 0.9)             # peak amplitude (1.0 = full scale)
# ----------------------------------------------------------------------------

def build_ess_signal(
    amplitude: float,
    duration_sec: float,
    sample_rate: int,
    f_start: float,
    f_stop: float,
    fade_sec: float,
    silence_sec: float,
) -> np.ndarray:
    """Generate a single Exponential Sine Sweep (ESS) signal."""
    # Time array for the main sweep
    t_sweep = np.arange(0, duration_sec, 1/sample_rate)
    
    # Exponential (logarithmic) sweep
    sweep = chirp(t_sweep, f0=f_start, f1=f_stop, t1=duration_sec, method='logarithmic')
    sweep *= amplitude
    
    # Silence insertion and fading
    silence_samples = int(round(sample_rate * silence_sec))
    fade_samples = int(round(sample_rate * fade_sec))
    fade_samples = min(fade_samples, len(sweep) // 2)
    
    total_samples = 2 * silence_samples + len(sweep)
    signal = np.zeros(total_samples, dtype=np.float32)
    signal[silence_samples:silence_samples + len(sweep)] = sweep.astype(np.float32)
    
    if fade_samples > 0:
        # Hanning fade in/out
        fade_in = 0.5 * (1.0 - np.cos(np.pi * np.arange(fade_samples) / fade_samples))
        fade_out = fade_in[::-1]
        start = silence_samples
        signal[start:start+fade_samples] *= fade_in
        signal[start+len(sweep)-fade_samples:start+len(sweep)] *= fade_out
    
    return signal

def write_signal(path: Path, signal: np.ndarray):
    with path.open("wb") as f:
        f.write(signal.tobytes())

def generate_dataset(output_dir: Path):
    signals_dir = output_dir / "signals"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    signals_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = output_dir / "manifest.csv"
    with manifest_path.open("w", newline="", encoding="utf-8") as mf:
        writer = csv.writer(mf, lineterminator="\n")
        writer.writerow([
            "signal_id",
            "frequency_hz",        # sweep range as string "f_start-f_stop"
            "sample_rate_hz",
            "duration_sec",
            "amplitude",
            "sample_count",
            "relative_path"
        ])

        for duration in DURATIONS_SEC:
            for f_start in FREQ_STARTS_HZ:
                for f_stop in FREQ_STOPS_HZ:
                    # Skip invalid: start >= stop
                    if f_start >= f_stop:
                        continue
                    for amplitude in AMPLITUDES:
                        # Create unique ID encoding all parameters
                        amp_label = int(round(amplitude * 100))
                        dur_label = int(duration * 10)  # e.g. 1.4 -> 14
                        sig_id = f"ess_d{dur_label:02d}_s{f_start:03d}_e{f_stop:03d}_a{amp_label:02d}"
                        
                        signal = build_ess_signal(
                            amplitude, duration, SAMPLE_RATE,
                            f_start, f_stop,
                            FADE_DURATION_SEC, SILENCE_DURATION_SEC
                        )
                        rel_path = Path("signals") / f"{sig_id}.f32"
                        write_signal(output_dir / rel_path, signal)
                        
                        # frequency_hz as "start-stop" (no commas to avoid CSV breakage)
                        freq_str = f"{f_start}-{f_stop}"
                        writer.writerow([
                            sig_id,
                            freq_str,
                            SAMPLE_RATE,
                            duration,
                            amplitude,
                            len(signal),
                            rel_path.as_posix()
                        ])

def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate varied Exponential Sine Sweep (ESS) dataset."
    )
    default_output = Path(__file__).resolve().parents[1] / "dataset" / "ess_signals"
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
    print(f"ESS dataset generated in: {output_dir}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())