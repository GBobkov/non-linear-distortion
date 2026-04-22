#!/usr/bin/env python3

import argparse
import csv
import math
import os
import struct
import shutil
from pathlib import Path


SAMPLE_RATE = 192000
SIGNAL_DURATION_SEC = 1.1
AMPLITUDES = (0.2, 0.4, 0.6, 0.8)
FADE_DURATION_SEC = 0.05
FREQUENCY_START_HZ = 10
FREQUENCY_STOP_HZ = 500
FREQUENCY_STEP_HZ = 10


def build_signal(frequency_hz: int, amplitude: float) -> list[float]:
    sample_count = int(round(SAMPLE_RATE * SIGNAL_DURATION_SEC))
    fade_samples = int(round(SAMPLE_RATE * FADE_DURATION_SEC))
    fade_samples = min(fade_samples, sample_count // 2)

    signal = []
    for index in range(sample_count):
        time_sec = index / SAMPLE_RATE
        envelope = 1.0

        if fade_samples > 0 and index < fade_samples:
            envelope = 0.5 * (1.0 - math.cos(math.pi * index / fade_samples))
        elif fade_samples > 0 and index >= sample_count - fade_samples:
            tail_index = sample_count - index - 1
            envelope = 0.5 * (1.0 - math.cos(math.pi * tail_index / fade_samples))

        sample = amplitude * envelope * math.sin(2.0 * math.pi * frequency_hz * time_sec)
        signal.append(sample)

    return signal


def write_signal(path: Path, signal: list[float]) -> None:
    with path.open("wb") as output_file:
        for sample in signal:
            output_file.write(struct.pack("<f", sample))


def generate_dataset(output_dir: Path) -> None:
    signals_dir = output_dir / "signals"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    signals_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = output_dir / "manifest.csv"
    with manifest_path.open("w", newline="", encoding="utf-8") as manifest_file:
        writer = csv.writer(manifest_file, lineterminator="\n")
        writer.writerow(
            [
                "signal_id",
                "frequency_hz",
                "sample_rate_hz",
                "duration_sec",
                "amplitude",
                "sample_count",
                "relative_path",
            ]
        )

        for amplitude in AMPLITUDES:
            amplitude_label = int(round(amplitude * 100))
            for frequency_hz in range(FREQUENCY_START_HZ, FREQUENCY_STOP_HZ + 1, FREQUENCY_STEP_HZ):
                signal_id = f"sine_{frequency_hz:03d}hz_amp_{amplitude_label:02d}"
                signal = build_signal(frequency_hz, amplitude)
                relative_path = Path("signals") / f"{signal_id}.f32"
                write_signal(output_dir / relative_path, signal)

                writer.writerow(
                    [
                        signal_id,
                        frequency_hz,
                        SAMPLE_RATE,
                        SIGNAL_DURATION_SEC,
                        amplitude,
                        len(signal),
                        relative_path.as_posix(),
                    ]
                )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a dataset of excitation sine signals up to 500 Hz."
    )
    default_output = Path(__file__).resolve().parents[1] / "dataset" / "excitation_signals"
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=default_output,
        help=f"Directory for manifest and raw float32 signals. Default: {default_output}",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    os.makedirs(output_dir, exist_ok=True)
    generate_dataset(output_dir)
    print(f"Dataset generated in: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
