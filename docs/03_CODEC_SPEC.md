# 03 — CSP (Cinema Stream Pack) Codec Specification v1

## 1. Goals

| Goal | Mechanism |
|------|-----------|
| Fit cinema modes under **100 MB/s** | CBR byte budget per frame |
| Survive DIGIC CPU limits | Integer-only predictors + Exp-Golomb |
| Preserve gradeability | 12-bit Bayer, optional lossless I-frames |
| Simple PC decode | Single-file `.csp` with frame index |
| Offline ProRes Log | Metadata tags for CSP-Log12 + color matrix |

## 2. Container

Little-endian. Extension: `.csp`

### 2.1 File header (256 bytes)

| Offset | Type | Field |
|--------|------|-------|
| 0 | char[4] | Magic `CSP1` |
| 4 | u16 | Version (=1) |
| 6 | u16 | Header size (=256) |
| 8 | u16 | Width |
| 10 | u16 | Height |
| 12 | u16 | FPS numerator |
| 14 | u16 | FPS denominator |
| 16 | u8 | Bit depth (10/12/14) |
| 17 | u8 | Bayer (`0=RGGB`) |
| 18 | u8 | Compression (`0=PACK12`, `1=SPATIAL`, `2=TEMPORAL`) |
| 19 | u8 | Log tag (`0=linear`, `1=CSP_LOG12`) |
| 20 | u32 | Flags (see below) |
| 24 | u16 | Black level |
| 26 | u16 | White level |
| 28 | u32 | ISO |
| 32 | u32 | Shutter microseconds |
| 36 | i32[9] | Color matrix Q16 (camera→XYZ or camera→Rec709) |
| 72 | char[32] | Mode ID ASCII (`CROP1080P100`, …) |
| 104 | u32 | Reserved |
| 108 | u64 | Creation Unix time |
| 116 | u32 | Audio sample rate (0=none) |
| 120 | u16 | Audio channels |
| 122 | u16 | Audio bits |
| 124 | u8[132] | Reserved / padding to 256 |

**Flags**

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `DUAL_STRIPE` | Frames interleaved across two media |
| 1 | `SSD_BRIDGE` | Recorded via external bridge |
| 2 | `LOSSLESS_I` | I-frames are mathematically lossless |
| 3 | `HAS_INDEX` | Trailer index present |

### 2.2 Frame packet

| Offset | Type | Field |
|--------|------|-------|
| 0 | char[4] | Magic `CSFR` |
| 4 | u32 | Frame index |
| 8 | u64 | Timestamp µs from start |
| 16 | u8 | Type (`0=I`, `1=P`) |
| 17 | u8 | Quantizer (0=lossless … 8=heavy) |
| 18 | u16 | Reserved |
| 20 | u32 | Payload size |
| 24 | u32 | CRC32 of payload |
| 28 | u8[payload] | Compressed Bayer |

### 2.3 Optional audio packet

Magic `CSAU`, then `u32 nsamples`, `u32 bytes`, PCM payload.

### 2.4 Trailer index

Magic `CSIX`, `u32 nframes`, then `nframes × {u64 file_offset, u32 size, u8 type}`.

## 3. Compression modes

### 3.1 `PACK12` (fast path)

Pack two 12-bit samples into 3 bytes. No entropy coding.  
Use when write headroom is large (e.g. 1080p25 on 1000x CF).

### 3.2 `SPATIAL` (default cinema)

Per Bayer plane (R, G1, G2, B):

1. Scanline predictor: `pred = left` (first pixel of line = above, first of frame = black_level)
2. Residual `r = sample - pred` (signed)
3. Apply quantizer `q`: `r' = r >> q` (q=0 lossless)
4. Encode `|r'|` with Exp-Golomb; sign bit separate

Planes are concatenated. This is LJ92-inspired but simpler for a tiny ARM encoder.

### 3.3 `TEMPORAL` (HFPS / 4K50)

- **I-frame** every `GOP` frames (default 12): same as `SPATIAL`
- **P-frame**: residual against previous *decoded* frame, then spatial predict on residual field
- Rate controller raises `q` when projected MB/s > budget

## 4. Rate control (CBR)

```
budget_bytes = floor(target_mibs * 1024 * 1024 / fps)
```

After encode, if `payload > budget_bytes` and `q < q_max`, increment `q` and re-encode from the raw slot (raw kept until accepted). If still over at `q_max`, **abort frame** and stop recording (never silently drop in cinema profile).

Default budgets: see `05_BITRATE_BUDGET.md`.

## 5. CSP-Log12 (grading curve)

Sensor data stays linear in the file when `log_tag=0`. Recommended offline transform before ProRes:

```
y = normalized linear (black/white subtracted, 0..1)
CSP_Log12(y) = clamp( (log10(1 + 99*y) / log10(100)) , 0, 1)
code = round(CSP_Log12(y) * (2^12 - 1))
```

When `log_tag=1`, the encoder may write pre-emphasized values (still 12-bit) for noisy high-ISO HFPS to stabilize compression; decode reverses the curve before debayer if flagged.

## 6. Why not ProRes in-camera

ProRes needs DCT blocks, target bitrate machines, and typically YUV 10-bit after debayer — all CPU-heavy and wrong for Bayer archival. CSP keeps the camera dumb/fast; the PC is smart/slow.

## 7. Interop

| Tool | Role |
|------|------|
| `pc/csp_toolkit` | Reference encode/decode/ProRes |
| Resolve / Premiere | Import ProRes or DNG sequence export |
| ffmpeg | ProRes encode backend (`-c:v prores_ks`) |
