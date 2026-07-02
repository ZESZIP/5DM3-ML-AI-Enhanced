#!/usr/bin/env python3
"""5D Mark III recording profile optimizer.

This helper does not patch firmware binaries. It generates practical settings
profiles you can apply manually in Magic Lantern menus.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass, asdict


SENSOR_WIDTH = 5760
SENSOR_HEIGHT = 3840
DEFAULT_CARD_WRITE_MBPS = 145.0


@dataclass
class RecordingTarget:
    width: int
    height: int
    fps: float
    bit_depth: int
    compression_ratio: float
    card_write_mbps: float


@dataclass
class PowerThermalTarget:
    disable_auto_power_off: bool
    force_run_on_failure: bool
    thermal_policy: str
    soft_limit_c: int
    hard_limit_c: int


def bandwidth_mbps(recording: RecordingTarget) -> float:
    raw_bits_per_frame = recording.width * recording.height * recording.bit_depth
    compressed_bytes_per_frame = raw_bits_per_frame / 8.0 / recording.compression_ratio
    return compressed_bytes_per_frame * recording.fps / (1024.0 * 1024.0)


def choose_liveview_percent(width: int, height: int, fps: float) -> int:
    resolution_ratio = (width * height) / float(3840 * 2160)
    if fps >= 120:
        base = 35
    elif fps >= 60:
        base = 45
    elif fps >= 48:
        base = 60
    elif fps >= 30:
        base = 75
    elif fps >= 24:
        base = 80
    else:
        base = 85

    if resolution_ratio > 1.0:
        base -= 10
    elif resolution_ratio < 0.4:
        base += 10

    return max(25, min(100, int(round(base / 5.0) * 5)))


def liveview_load_ratio(percent: int, fps: float) -> float:
    pixel_ratio = (percent / 100.0) ** 2
    fps_ratio = fps / 30.0
    return pixel_ratio * fps_ratio


def validate(
    recording: RecordingTarget,
    power_thermal: PowerThermalTarget,
    allow_unsafe: bool,
) -> list[str]:
    warnings: list[str] = []

    if recording.fps > 120:
        warnings.append("FPS above 120 is not supported by this profile helper.")

    if recording.fps >= 100 and (recording.width > 1920 or recording.height > 1080):
        warnings.append(
            "120fps-class recording is usually only realistic with heavy crop "
            "or reduced dimensions on 5D3."
        )

    if recording.bit_depth == 14 and recording.fps >= 60:
        warnings.append(
            "14-bit high-FPS settings are likely unstable; 10-bit or 12-bit "
            "is generally more reliable."
        )

    required_write = bandwidth_mbps(recording)
    if required_write > recording.card_write_mbps:
        warnings.append(
            f"Required write bandwidth ({required_write:.1f} MB/s) exceeds "
            f"card target ({recording.card_write_mbps:.1f} MB/s)."
        )

    if power_thermal.force_run_on_failure and not allow_unsafe:
        warnings.append(
            "force-run-on-failure requested without --allow-unsafe. "
            "Unsafe action was rejected."
        )

    if power_thermal.thermal_policy == "unsafe-no-cutoff" and not allow_unsafe:
        warnings.append(
            "unsafe-no-cutoff requested without --allow-unsafe. "
            "Unsafe policy was rejected."
        )

    return warnings


def recommendation(
    recording: RecordingTarget,
    power_thermal: PowerThermalTarget,
    liveview_percent: int,
    allow_unsafe: bool,
) -> dict:
    required_write_mbps = bandwidth_mbps(recording)
    headroom = recording.card_write_mbps - required_write_mbps
    load_ratio = liveview_load_ratio(liveview_percent, recording.fps)
    processor_relief = max(0.0, (1.0 - load_ratio) * 100.0)

    safe_force_run = power_thermal.force_run_on_failure and allow_unsafe
    safe_thermal_policy = (
        power_thermal.thermal_policy
        if power_thermal.thermal_policy != "unsafe-no-cutoff" or allow_unsafe
        else "aggressive"
    )

    return {
        "camera": "Canon 5D Mark III",
        "recording": {
            "target": f"{recording.width}x{recording.height}@{recording.fps:.3f}",
            "bit_depth": recording.bit_depth,
            "compression_ratio": recording.compression_ratio,
            "estimated_required_write_mbps": round(required_write_mbps, 2),
            "card_write_mbps": round(recording.card_write_mbps, 2),
            "write_headroom_mbps": round(headroom, 2),
            "sustainable_estimate": headroom >= 0,
        },
        "ui_ux_2026": {
            "live_view": {
                "resolution_percent": liveview_percent,
                "scaled_preview_dimensions": {
                    "width": int(math.floor(SENSOR_WIDTH * liveview_percent / 100.0)),
                    "height": int(math.floor(SENSOR_HEIGHT * liveview_percent / 100.0)),
                },
                "estimated_processor_relief_percent": round(processor_relief, 2),
            },
            "high_fps": {
                "enabled": recording.fps >= 60,
                "target_fps": recording.fps,
                "is_120fps_class": recording.fps >= 100,
            },
            "power": {
                "disable_auto_power_off": power_thermal.disable_auto_power_off,
                "force_run_on_failure": safe_force_run,
            },
            "thermal": {
                "policy": safe_thermal_policy,
                "soft_limit_c": power_thermal.soft_limit_c,
                "hard_limit_c": power_thermal.hard_limit_c,
            },
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate modernized 5D3 recording + UI/UX settings."
    )
    parser.add_argument("--width", type=int, required=True, help="Target frame width.")
    parser.add_argument("--height", type=int, required=True, help="Target frame height.")
    parser.add_argument("--fps", type=float, required=True, help="Target recording FPS.")
    parser.add_argument(
        "--bit-depth",
        type=int,
        default=12,
        choices=[10, 12, 14],
        help="Raw bit depth.",
    )
    parser.add_argument(
        "--compression-ratio",
        type=float,
        default=1.7,
        help="Expected LJ92 compression ratio.",
    )
    parser.add_argument(
        "--card-write-mbps",
        type=float,
        default=DEFAULT_CARD_WRITE_MBPS,
        help="Measured sustained card write speed in MB/s.",
    )
    parser.add_argument(
        "--liveview-percent",
        type=int,
        default=None,
        help="Live View downscale percentage. If omitted, auto-selected.",
    )
    parser.add_argument(
        "--disable-auto-power-off",
        action="store_true",
        help="Generate settings with auto power off disabled.",
    )
    parser.add_argument(
        "--force-run-on-failure",
        action="store_true",
        help="Try to keep recording despite recoverable failures (unsafe).",
    )
    parser.add_argument(
        "--thermal-policy",
        default="balanced",
        choices=["safe", "balanced", "aggressive", "unsafe-no-cutoff"],
        help="Thermal behavior policy.",
    )
    parser.add_argument(
        "--soft-limit-c",
        type=int,
        default=78,
        help="Soft thermal warning threshold in Celsius.",
    )
    parser.add_argument(
        "--hard-limit-c",
        type=int,
        default=85,
        help="Hard thermal limit in Celsius.",
    )
    parser.add_argument(
        "--allow-unsafe",
        action="store_true",
        help="Required to permit unsafe thermal or power overrides.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    liveview_percent = args.liveview_percent
    if liveview_percent is None:
        liveview_percent = choose_liveview_percent(args.width, args.height, args.fps)

    recording = RecordingTarget(
        width=args.width,
        height=args.height,
        fps=args.fps,
        bit_depth=args.bit_depth,
        compression_ratio=args.compression_ratio,
        card_write_mbps=args.card_write_mbps,
    )
    power_thermal = PowerThermalTarget(
        disable_auto_power_off=args.disable_auto_power_off,
        force_run_on_failure=args.force_run_on_failure,
        thermal_policy=args.thermal_policy,
        soft_limit_c=args.soft_limit_c,
        hard_limit_c=args.hard_limit_c,
    )

    warnings = validate(recording, power_thermal, args.allow_unsafe)
    result = recommendation(recording, power_thermal, liveview_percent, args.allow_unsafe)
    result["requested"] = {
        "recording": asdict(recording),
        "power_thermal": asdict(power_thermal),
        "allow_unsafe": args.allow_unsafe,
    }
    result["warnings"] = warnings

    print(json.dumps(result, indent=2, sort_keys=False))
    return 0 if not warnings else 2


if __name__ == "__main__":
    sys.exit(main())
