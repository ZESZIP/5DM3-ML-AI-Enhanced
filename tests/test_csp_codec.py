import numpy as np
import pytest

from pc.csp_toolkit import codec
from pc.csp_toolkit.container import CspReader, CspWriter
from pc.csp_toolkit.modes import MODES, select_mode
from pc.csp_toolkit.simulate import make_bayer_frame, simulate_clip


def test_pack12_roundtrip():
    s = np.arange(16, dtype=np.uint16) * 17
    s &= 0x0FFF
    data = codec.pack12(s)
    back = codec.unpack12(data, len(s))
    assert np.array_equal(s, back)


def test_spatial_lossless_small():
    raw = make_bayer_frame(64, 48, 0.2)
    payload = codec.encode_spatial(raw, quant=0)
    back = codec.decode_spatial(payload, raw.shape, quant=0)
    assert np.array_equal(raw, back)


def test_temporal_roundtrip(tmp_path):
    path = tmp_path / "t.csp"
    w, h = 80, 48
    enc_w = CspWriter(
        path=path,
        width=w,
        height=h,
        fps_num=50,
        compression=codec.COMP_TEMPORAL,
        mode_id="TEST",
        target_mibs=95,
    )
    enc_w.open()
    frames = [make_bayer_frame(w, h, i * 0.05, seed=i) for i in range(8)]
    for f in frames:
        enc_w.write_frame(f)
    enc_w.close()

    reader = CspReader(path)
    reader.open()
    decoded = list(reader.decode_all())
    assert len(decoded) == 8
    # Lossless I + reversible P path when quant stays 0
    for a, b in zip(frames, decoded):
        assert np.array_equal(a, b)


def test_simulate_under_budget(tmp_path):
    out = tmp_path / "s.csp"
    # Tiny geometry proxy for 1080p100 budget behavior
    result = simulate_clip(
        "CROP1080P100",
        seconds=0.05,
        out_path=out,
        width_override=160,
        height_override=96,
    )
    assert out.exists()
    assert result["frames"] >= 1
    assert result["avg_mib_s"] < 100


def test_mode_select_respects_cap():
    m = select_mode(95, bridge_present=False)
    assert m.target_mibs <= 100
    assert m.mode_id in MODES
    # Bridge ceiling = min(160, measured*0.9); need measured high enough for 150 MiB/s mode
    m2 = select_mode(175, bridge_present=True)
    assert m2.mode_id == "CROP4K50_FULL"


def test_crc_header_magic(tmp_path):
    out = tmp_path / "h.csp"
    simulate_clip("FF1080P25", 0.04, out, width_override=64, height_override=48)
    data = out.read_bytes()
    assert data[:4] == b"CSP1"
