"""Decode CSP → log RGB preview / ProRes via ffmpeg when available."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

from . import codec
from .container import CspReader
from .log_curve import apply_color_matrix, csp_log12, linearize


def frame_to_log_rgb8(
    raw: np.ndarray,
    black: int = 2047,
    white: int = 15000,
) -> np.ndarray:
    # For synthetic 12-bit frames, treat full code range as linear sensor.
    lin = linearize(raw, black=0, white=4095)
    # Rebuild approximate RGB via demosaic then log
    rgb = codec.debayer_bilinear((lin * 4095).astype(np.uint16))
    rgb = apply_color_matrix(rgb)
    logc = csp_log12(rgb)
    return (np.clip(logc, 0, 1) * 255).astype(np.uint8)


def export_png_sequence(csp_path: str | Path, out_dir: str | Path, max_frames: int | None = None) -> list[Path]:
    reader = CspReader(Path(csp_path))
    reader.open()
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    paths: list[Path] = []
    for i, frame in enumerate(reader.decode_all()):
        if max_frames is not None and i >= max_frames:
            break
        rgb = frame_to_log_rgb8(frame, reader.header["black_level"], reader.header["white_level"])
        p = out / f"frame_{i:06d}.png"
        Image.fromarray(rgb, mode="RGB").save(p)
        paths.append(p)
    return paths


def export_prores(
    csp_path: str | Path,
    out_mov: str | Path,
    max_frames: int | None = None,
) -> Path:
    """Export ProRes Log. Uses ffmpeg if present; otherwise writes a PNG sequence sibling."""
    out_mov = Path(out_mov)
    ffmpeg = shutil.which("ffmpeg")
    with tempfile.TemporaryDirectory(prefix="csp_prores_") as tmp:
        seq = export_png_sequence(csp_path, tmp, max_frames=max_frames)
        if not seq:
            raise RuntimeError("no frames decoded")
        reader = CspReader(Path(csp_path))
        reader.open()
        fps = max(1, reader.header["fps_num"] // max(1, reader.header["fps_den"]))
        if not ffmpeg:
            # Fallback: copy first/last proof frames next to target
            proof = out_mov.with_suffix(".png")
            Image.open(seq[0]).save(proof)
            sidecar = out_mov.with_suffix(".pngseq.txt")
            sidecar.write_text(
                f"ffmpeg not found; wrote proof frame {proof}\n"
                f"frames={len(seq)} fps={fps}\n"
                "Install ffmpeg for true ProRes: "
                "ffmpeg -y -framerate FPS -i frame_%06d.png -c:v prores_ks -profile:v 3 out.mov\n"
            )
            return proof
        pattern = str(Path(tmp) / "frame_%06d.png")
        out_mov.parent.mkdir(parents=True, exist_ok=True)
        cmd = [
            ffmpeg,
            "-y",
            "-framerate",
            str(fps),
            "-i",
            pattern,
            "-c:v",
            "prores_ks",
            "-profile:v",
            "3",  # 422 HQ
            "-pix_fmt",
            "yuv422p10le",
            "-color_primaries",
            "bt709",
            "-color_trc",
            "bt709",
            "-colorspace",
            "bt709",
            str(out_mov),
        ]
        subprocess.run(cmd, check=True, capture_output=True)
    return out_mov
