"""CSP-Log12 curve and 5D3-oriented color helpers."""

from __future__ import annotations

import numpy as np

# Approximate camera RGB -> Rec.709 matrix (placeholder calibrated slot).
COLOR_MATRIX_5D3 = np.array(
    [
        [1.55, -0.45, -0.10],
        [-0.15, 1.35, -0.20],
        [0.05, -0.55, 1.50],
    ],
    dtype=np.float64,
)


def linearize(raw: np.ndarray, black: int = 2047, white: int = 15000) -> np.ndarray:
    x = (raw.astype(np.float64) - black) / max(1.0, float(white - black))
    return np.clip(x, 0.0, 1.0)


def csp_log12(y: np.ndarray) -> np.ndarray:
    """Map linear 0..1 to CSP-Log12 0..1."""
    return np.clip(np.log10(1.0 + 99.0 * y) / np.log10(100.0), 0.0, 1.0)


def csp_log12_inverse(c: np.ndarray) -> np.ndarray:
    return np.clip((np.power(10.0, c * np.log10(100.0)) - 1.0) / 99.0, 0.0, 1.0)


def to_code12(y: np.ndarray) -> np.ndarray:
    return np.round(csp_log12(y) * 4095.0).astype(np.uint16)


def apply_color_matrix(rgb: np.ndarray, matrix: np.ndarray | None = None) -> np.ndarray:
    m = COLOR_MATRIX_5D3 if matrix is None else matrix
    flat = rgb.reshape(-1, 3)
    out = flat @ m.T
    return np.clip(out.reshape(rgb.shape), 0.0, 1.0)
