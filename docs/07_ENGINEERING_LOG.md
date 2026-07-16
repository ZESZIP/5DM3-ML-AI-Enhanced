# 07 — Engineering Log: How Cinema OS Was Designed

This is the full rationale dump: hardware physics, Magic Lantern lessons, codec choices, UI, and hardware mods.

## 1. Starting question

> Can a Canon 5D Mark III deliver continuous high-end 1080p100 / 4K50 12-bit log footage under 100 MB/s with live preview, then finish as ProRes Log on a PC?

Short answer: **yes for 1080p100 and 4K25-class; 4K50 full-height needs an SSD bridge; “no buffering” means streaming writes, not a bigger DRAM fantasy.**

## 2. What the 5D3 actually is

- 22 MP full-frame Bayer, 14-bit ADC, DIGIC 5+
- LiveView already maintains a raw window in DRAM while showing YUV on LCD/HDMI
- EDMAC can crop/copy that window at hundreds of MB/s
- CF UDMA7 is the real ceiling (~85–110 MB/s sustained on good cards)
- SD is a trickle (~20 MB/s)
- Buffer RAM is small → burst-then-stall is fine for stills, fatal for cinema

Magic Lantern proved you can steal the raw LV buffer and dump it. Their continuous raw work (EDMAC crops, MLV, LJ92, 10/12-bit, crop_rec) is the empirical foundation.

## 3. What we deliberately changed vs Magic Lantern

| ML pattern | Cinema OS choice | Why |
|------------|------------------|-----|
| MLV + LJ92 | **CSP** container + spatial/temporal EG | Tuned CBR + HFPS temporal |
| Variable deep buffering | **Zero-buffer stream** (1–2 ping-pong slots) | Honors “no buffering” / continuous guarantee |
| Classic ML menus | **Cinema shell** (mockup tabs + copper select) | Operator UX for film sets |
| Card spanning | **dual_stripe + SSD bridge path** | Explicit storage tiers |
| DNG/ProRes nowhere on cam | PC **csp_toolkit → ProRes Log** | DIGIC cannot be a ProRes DSP |

## 4. Codec design (CSP)

In-camera ProRes/MP4 of 12-bit Bayer at these rates is a non-starter: wrong color domain, too much CPU, still card-bound.

CSP pipeline:

1. Capture Bayer window (12-bit truncate from 14)  
2. Optional temporal residual vs previous frame (HFPS)  
3. Split RGGB planes → left/above predictor → Exp-Golomb  
4. Hard byte budget per frame from `target_mibs`  
5. Write immediately; if over budget at `q_max`, **stop cleanly**

Offline:

1. Decode → demosaic → color matrix → **CSP-Log12**  
2. `ffmpeg prores_ks` 422 HQ (or PNG sequence fallback)

## 5. Mode physics (why the UI can lie a little)

The mockup shows `UHD 4160x2560 [4K]` — that is the **creative target**. The stream engine maps it to:

- `CROP4K50_FULL` when SSD bridge (≥150 MB/s class) is present  
- otherwise `CROP4K50_LITE` / `CROP4K25` automatically  

1080p100 fits in ≤95 MB/s with temporal CSP on fast CF (or CF+SD stripe for comfort).

## 6. Live preview

Preview is Canon’s LV YUV / HDMI path, **not** a decode of CSP. HUD is BMP overlay. When write headroom is thin, Frozen/reduced LV (ML lesson) frees bus time.

## 7. Hardware path to “CFast A”

5D3 has no CFast slot. We define **CFast A** as the product name for a powered CF→IDE SSD pack:

- Required because CF pin power cannot feed a hungry SSD  
- Unlocks the only honest full-height 4K50 path  
- Footer label switches when `CINEMA/BRIDGE.OK` exists  

Fans + dummy battery are not cosmetics; they are continuity insurance.

## 8. Repository layout after the rebuild

Previous tree was an ML source dump + binary modules. It was replaced with:

- Specs that tell the truth about bandwidth  
- Portable C codec + stream engine  
- Working Python encode/decode/ProRes path  
- Interactive UI matching the amber mockup  
- Hardware BOM / bridge / thermal guides  

On-camera `autoexec.bin` still needs an ARM DryOS stub tree (`firmware/platform/5D3.123`) — the **codec and UX are implemented and testable now**; the stub glue is the remaining device-flash step on a machine with the ML toolchain.

## 9. Validation performed

- Pytest: pack12, spatial lossless, temporal roundtrip, mode governor, CSP header magic  
- Host `firmware` Makefile builds `libcinema_csp.a`  
- UI preview static site under `pc/ui_preview`  

## 10. Practical shooting recipe

1. Tier 1 hardware (fast CF + dummy battery)  
2. Arm `CROP1080P100` or `CROP4K25`  
3. Record `.csp` continuously with live LV  
4. Ingest: `python -m pc.csp_toolkit decode --in clip.csp --prores clip_log.mov`  
5. Grade in Resolve from ProRes Log (or DNG export if you extend the toolkit)
