# 05 — Bitrate Budget (≤100 MB/s Continuous)

## 1. Raw math (before compression)

Uncompressed Bayer bytes/sec ≈ `width * height * bit_depth / 8 * fps`

| Mode | Geometry | bpp | fps | Uncompressed | Need vs 100 MB/s |
|------|----------|-----|-----|--------------|------------------|
| FF1080P25 | 1920×1080 | 12 | 25 | 77.8 MB/s | 1.0× (PACK12 OK) |
| FF1080P50 | 1920×1080 | 12 | 50 | 155.5 MB/s | ~1.6× |
| CROP1080P100 | 1920×1080 | 12 | 100 | 311.0 MB/s | ~3.1× |
| CROP4K25 | 3840×2160 | 12 | 25 | 311.0 MB/s | ~3.1× |
| CROP4K50 | 3840×2160 | 12 | 50 | 622.1 MB/s | ~6.2× |
| UI “4160×2560 4K” | 4160×2560 | 12 | 50 | 799.5 MB/s | ~8.0× |

## 2. CSP target budgets (product)

Cinema OS arms modes with a **hard CBR** under the card probe:

| Mode ID | Compression | Target | Frame budget | Storage |
|---------|-------------|--------|--------------|---------|
| `FF1080P25` | PACK12 or SPATIAL | 45 MB/s | ~1.80 MB | CF |
| `FF1080P50` | SPATIAL | 70 MB/s | ~1.40 MB | CF |
| `CROP1080P100` | TEMPORAL | **95 MB/s** | ~0.95 MB | CF or stripe |
| `CROP4K25` | SPATIAL/TEMPORAL | 90 MB/s | ~3.60 MB | CF fast |
| `CROP4K50_LITE` | TEMPORAL | 95 MB/s | ~1.90 MB | CF (height cut) |
| `CROP4K50_FULL` | TEMPORAL | 150 MB/s | ~3.00 MB | **SSD bridge** |

Product rule: UI may show creative labels (`UHD 4160×2560 [4K]`); stream engine maps to the nearest mode whose budget ≤ `0.9 * measured_write_mibs`.

## 3. Compression ratio expectations

| Content | SPATIAL q=0–1 | TEMPORAL GOP12 |
|---------|---------------|----------------|
| Low ISO, soft bokeh | 1.7–2.2:1 | 2.5–3.5:1 |
| Mid detail ISO 400 | 1.5–1.8:1 | 2.2–3.0:1 |
| High detail ISO 1600 | 1.3–1.6:1 | 1.8–2.5:1 |

1080p100 needs ~3.1:1 average → **TEMPORAL + q adaptation** is mandatory.  
4K50 full needs ~6:1 → **SSD bridge** and/or height reduction.

## 4. Zero-buffer timing

At 100 fps, frame time = 10 ms.

| Stage | Budget |
|-------|--------|
| EDMAC crop done | < 2 ms after exposure window |
| CSP encode | < 5 ms (ARM + DMA assist) |
| Card write of ≤0.95 MB @ 100 MB/s | ≈ 9.5 ms |

Encode and write are pipelined across ping-pong slots: while slot B captures, slot A encodes+writes. The write duration must stay **≤ frame time**; otherwise underrun.

## 5. Governor algorithm (summary)

```
measured = benchmark_mibs()
ceiling = min(100, measured * 0.90)          # product cap
if bridge_present: ceiling = min(160, measured * 0.90)

for mode in preferred_list:
    if mode.cbr_mibs <= ceiling: arm(mode); break
else:
    arm(safest_mode)  # FF1080P25
```

## 6. Audio overhead

48 kHz stereo 16-bit PCM ≈ 0.19 MB/s — negligible; included inside the 100 MB/s envelope.
