"""Cinema mode catalog — mirrors firmware stream_engine catalog."""

from __future__ import annotations

from dataclasses import dataclass

from . import codec


@dataclass(frozen=True)
class CinemaMode:
    mode_id: str
    width: int
    height: int
    fps_num: int
    fps_den: int
    compression: int
    target_mibs: int
    needs_bridge: bool
    ui_label: str


MODES: dict[str, CinemaMode] = {
    "FF1080P25": CinemaMode(
        "FF1080P25", 1920, 1080, 25, 1, codec.COMP_SPATIAL, 45, False, "1920x1080 [FF25]"
    ),
    "FF1080P50": CinemaMode(
        "FF1080P50", 1920, 1080, 50, 1, codec.COMP_SPATIAL, 70, False, "1920x1080 [FF50]"
    ),
    "CROP1080P100": CinemaMode(
        "CROP1080P100",
        1920,
        1080,
        100,
        1,
        codec.COMP_TEMPORAL,
        95,
        False,
        "1920x1080 [100P]",
    ),
    "CROP4K25": CinemaMode(
        "CROP4K25", 3840, 2160, 25, 1, codec.COMP_TEMPORAL, 90, False, "UHD 3840x2160 [4K25]"
    ),
    "CROP4K50_LITE": CinemaMode(
        "CROP4K50_LITE",
        3840,
        1600,
        50,
        1,
        codec.COMP_TEMPORAL,
        95,
        False,
        "UHD 3840x1600 [4K50L]",
    ),
    "CROP4K50_FULL": CinemaMode(
        "CROP4K50_FULL",
        3840,
        2160,
        50,
        1,
        codec.COMP_TEMPORAL,
        150,
        True,
        "UHD 4160x2560 [4K]",
    ),
}


def select_mode(measured_mibs: float, bridge_present: bool = False) -> CinemaMode:
    ceiling = measured_mibs * 0.9
    if not bridge_present:
        ceiling = min(ceiling, 100.0)
    else:
        ceiling = min(ceiling, 160.0)
    # Prefer richer modes
    for mode_id in (
        "CROP4K50_FULL",
        "CROP4K50_LITE",
        "CROP4K25",
        "CROP1080P100",
        "FF1080P50",
        "FF1080P25",
    ):
        m = MODES[mode_id]
        if m.needs_bridge and not bridge_present:
            continue
        if m.target_mibs <= ceiling:
            return m
    return MODES["FF1080P25"]
