# 01 — Canon 5D Mark III Hardware Analysis (Cinema Focus)

## 1. Platform identity

| Item | Spec |
|------|------|
| Model | Canon EOS 5D Mark III (EOS 5D3) |
| Sensor | Full-frame CMOS ~36×24 mm, **5760×3840** photosites (~22.3 MP) |
| CFA | Bayer RGGB, native **14-bit** ADC |
| Readout | Multi-channel (8-channel stills path); video LiveView uses subsampled / binned modes |
| Processor | **DIGIC 5+** (single) |
| Firmware target | **1.2.3** (Magic Lantern / Cinema OS ABI) |
| Storage | CF Type I **UDMA7** + SD/SDHC/SDXC (slow controller) |
| I/O | HDMI (clean 1080p 4:2:2 8-bit on ≥1.2.1), mic, headphone, USB 2.0, PC sync |

Cinema OS treats the camera as a **sensor → DRAM → compressed stream → storage** machine, not as Canon’s H.264 camcorder mode.

## 2. What actually limits cinema recording

### 2.1 Sensor → DRAM (not the bottleneck)

Magic Lantern measurements on 5D3 show EDMAC/memory copies of cropped raw buffers in the **~700 MB/s** class. That is enough to move 1080p and many 3K/4K crop windows at cinema frame rates **into RAM**.

Implication: we do **not** need to invent a new sensor; we need to:

1. Program the LiveView / crop registers for the desired geometry (lessons from `crop_rec`)
2. DMA the Bayer window with minimal CPU touch
3. Compress and write without letting DRAM fill

### 2.2 DRAM buffer (the reason “no buffering” matters)

Unlike a cinema camera with gigabytes of write cache, the 5D3 buffer is on the order of **hundreds of MB** at best for raw bursts. At 4K-class bitrates that is only a few seconds.

**Cinema OS policy:** *streaming mode* — compress frame *N* and issue the write before frame *N+1* lands. The “buffer” is at most **1–2 frame slots** for DMA double-buffering, never a multi-second backlog. If the card cannot keep up, recording **stops cleanly** instead of silently dropping mid-take.

### 2.3 Card controllers (the real wall)

| Path | Practical sustained write | Notes |
|------|---------------------------|-------|
| CF UDMA7 | **~85–110 MB/s** | Best stock path; Lexar 1066x / SanDisk Extreme Pro class |
| SD | **~15–25 MB/s** | Controller-limited; not UHS-I capable like modern bodies |
| CF + SD stripe | **~100–130 MB/s** | Cinema OS `dual_stripe` module; complexity + dual readers |
| CF→IDE/SSD bridge | **up to controller ceiling ~160 MB/s** | Hardware mod; see `04_HARDWARE_MODS.md` |

**Product requirement ≤100 MB/s** aligns with reliable CF continuous operation and leaves headroom for filesystem overhead.

## 3. Sensor modes relevant to cinema

Canon’s stock movie mode does not unlock full sensor readout for video. Community research (ML `crop_rec`, full-res LV experiments) established:

| Geometry | Typical use | Continuous feasibility |
|----------|-------------|------------------------|
| 1920×1080 (3×3 bin, full frame) | Narrative FF look | Excellent with compression |
| 1920×1080 / 1920×960 HFPS | 50/60/100-class slow-mo | Needs lower bpp + CSP temporal |
| ~2560×1090 → ~3584×1320 | Super35-ish crop | Continuous at 10–12 bit + lossless/CSP |
| ~3840×1600–2160 | “4K” crop | 25p continuous with CSP; 50p needs bridge or height cut |
| ~4096×2560 / full ~5796×3870 | Ultra crop / full-res LV | Burst or low fps only on stock storage |

**Important honesty for 4K50p / 1080p100:**

- **1080p100** is approachable as a high-FPS crop/bin preset with **12-bit CSP + temporal** under 100 MB/s.
- **4K50p** at full UHD height is **storage-bound**. Cinema OS offers:
  - `CROP4K50_LITE` (e.g. 3840×1600 @ 50p) on fast CF, or
  - `CROP4K50_FULL` only when the **SSD bridge** reports ≥150 MB/s sustained.

## 4. Live preview while recording

Stock DIGIC paths already maintain LiveView YUV for LCD/HDMI while raw sits in a separate buffer. Cinema OS keeps preview by:

1. Leaving Canon’s LV pipeline running (or ML “Frozen LV” / reduced preview when write bandwidth is tight)
2. Drawing Cinema OS HUD in the BMP overlay plane (not into the Bayer stream)
3. Preferring HDMI clean feed for focus pulling; internal LCD for status

Preview must **never** re-encode the CSP stream for the LCD — that would steal CPU from the write path.

## 5. Thermal & power envelope

Long continuous raw-class recording heats DIGIC, sensor PCB, and the CF controller. Failures look like:

- Sudden stops / ERR99-class faults
- Rising noise / pink frames
- Card disconnects

Cinema OS includes a thermal governor (throttle fps / widen quantization before abort). Hardware cooling mods are documented because software alone cannot remove joules.

## 6. Lessons extracted from Magic Lantern practice

| ML discovery | Cinema OS use |
|--------------|---------------|
| Address LiveView raw buffer + EDMAC crop | `stream_engine` capture path |
| Don’t DNG in-camera; dump custom container | **CSP** instead of MLV/DNG |
| LJ92 lossless ~1.5–2:1 | Baseline; CSP adds temporal for HFPS |
| 10/12-bit reduces entropy → better ratios | Default **12-bit** cinema profile |
| Variable buffering helps burst | **Rejected** for “continuous guaranteed” mode |
| Dual card spanning | Evolved into **dual_stripe** striping |
| crop_rec register presets | Catalogued as Cinema OS mode IDs |

## 7. What we will not pretend DIGIC can do

- Real-time ProRes / BRAW encode of 12-bit Bayer at 4K50
- Full-sensor 5.7K 24p continuous to stock CF without extreme compression
- Silent “infinite buffer” when write speed < capture rate

Those workloads belong on the **PC toolkit** after CSP ingest.
