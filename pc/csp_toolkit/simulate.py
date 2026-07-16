"""Generate synthetic Bayer frames and write CSP clips under a bitrate budget."""

from __future__ import annotations

from pathlib import Path

import numpy as np

from .container import CspWriter
from .modes import MODES, CinemaMode


def make_bayer_frame(width: int, height: int, t: float, seed: int = 0) -> np.ndarray:
    """Soft gradient + moving bar — compresses like real low-ISO footage."""
    rng = np.random.default_rng(seed + int(t * 1000))
    yy, xx = np.mgrid[0:height, 0:width]
    base = 900 + 1800 * (xx / max(1, width - 1)) + 900 * (yy / max(1, height - 1))
    bar_x = int((0.2 + 0.6 * ((np.sin(t * 2.0) + 1) * 0.5)) * width)
    base[:, max(0, bar_x - 40) : min(width, bar_x + 40)] += 800
    noise = rng.normal(0, 12, size=(height, width))
    frame = np.clip(base + noise, 0, 4095).astype(np.uint16)
    # Emphasize Bayer channel offsets slightly
    frame[0::2, 0::2] = np.clip(frame[0::2, 0::2] + 40, 0, 4095)  # R
    frame[1::2, 1::2] = np.clip(frame[1::2, 1::2] + 20, 0, 4095)  # B
    return frame


def simulate_clip(
    mode: CinemaMode | str,
    seconds: float,
    out_path: str | Path,
    width_override: int | None = None,
    height_override: int | None = None,
) -> dict:
    if isinstance(mode, str):
        mode = MODES[mode]
    w = width_override or mode.width
    h = height_override or mode.height
    # Keep sim sizes practical for CI while preserving aspect for huge modes
    if w * h > 1920 * 1080:
        scale = (1920 * 1080 / (w * h)) ** 0.5
        w = max(64, int(w * scale) // 2 * 2)
        h = max(64, int(h * scale) // 2 * 2)

    nframes = max(1, int(seconds * mode.fps_num / mode.fps_den))
    writer = CspWriter(
        path=Path(out_path),
        width=w,
        height=h,
        fps_num=mode.fps_num,
        fps_den=mode.fps_den,
        compression=mode.compression,
        mode_id=mode.mode_id,
        target_mibs=mode.target_mibs,
    )
    writer.open()
    total = 0
    peak = 0
    for i in range(nframes):
        t = i / max(1, mode.fps_num)
        frame = make_bayer_frame(w, h, t, seed=i)
        info = writer.write_frame(frame)
        total += info["bytes"]
        peak = max(peak, info["bytes"])
    writer.close()
    avg_mibs = (total / max(1, nframes)) * mode.fps_num / mode.fps_den / (1024 * 1024)
    return {
        "path": str(out_path),
        "mode": mode.mode_id,
        "frames": nframes,
        "width": w,
        "height": h,
        "avg_mib_s": avg_mibs,
        "peak_frame_bytes": peak,
        "under_100": avg_mibs <= 100.0,
    }
