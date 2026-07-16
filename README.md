# Cinema OS 5D3 — Continuous 12-bit Log Cinema Pipeline

**Target platform:** Canon EOS 5D Mark III (firmware 1.2.3, DIGIC 5+)  
**Mission:** Continuous high-end cinema recording (1080p100 / 4K50-class modes), live preview, custom **CSP** container under **≤100 MB/s**, then offline decompress to **ProRes Log** on a PC.

This repository replaces the previous Magic Lantern UI overlay dump with a purpose-built cinema stack:

| Layer | What it is |
|-------|------------|
| `firmware/` | On-camera Cinema OS shell, zero-buffer stream engine, CSP recorder, dual-card striping |
| `pc/csp_toolkit/` | Python encode/decode + ProRes Log export |
| `pc/ui_preview/` | Interactive menu matching the cinema UI mockup |
| `docs/` | Hardware physics, architecture, codec spec, mods, install |
| `hardware/` | SSD bridge + thermal BOM and wiring notes |

## Why a custom codec (not ProRes / MP4 in-camera)

The 5D Mark III can move raw Bayer from the sensor buffer over EDMAC at ~700 MB/s, but **card write** is the wall:

- CF UDMA7 practical sustained ≈ **85–110 MB/s**
- SD slot ≈ **15–25 MB/s**
- Internal DRAM buffer is small → **buffering is not a strategy** for long takes

ProRes and H.264 need heavy DSP the DIGIC 5+ path does not expose for arbitrary 12-bit Bayer. Magic Lantern proved the winning pattern: **dump lightly packed/compressed Bayer to a custom file, finish the expensive work on a PC**. Cinema OS follows that pattern with a new container (**CSP — Cinema Stream Pack**) tuned for:

1. **Streaming writes** (one frame compressed → written immediately; no multi-second DRAM queue)
2. **Hard CBR budget ≤100 MB/s** so continuous recording is physics-legal
3. **12-bit CSP-Log** encoding that expands cleanly to ProRes 422 HQ / 4444 Log offline

## Headline cinema modes (realistic)

| Mode ID | Sensor geometry | Bit depth | Target write | Continuous on |
|---------|-----------------|-----------|--------------|---------------|
| `FF1080P25` | 1920×1080 3×3 bin FF | 12-bit CSP | ~45 MB/s | CF UDMA7 |
| `FF1080P50` | 1920×1080 3×3 bin FF | 12-bit CSP | ~70 MB/s | CF 1066x+ |
| `CROP1080P100` | 1920×1080 1:1 / 3×3 HFPS | 12-bit CSP + temporal | ~95 MB/s | CF fast **or** CF+SD stripe |
| `CROP4K25` | 3840×2160 / 4096×2160 crop | 12-bit CSP | ~90 MB/s | CF fast + Frozen LV |
| `CROP4K50` | 3840×1600–2160 crop | 12-bit CSP + temporal | ~95–160 MB/s | **SSD bridge recommended** |

> Full-sensor 4160×2560 @ 50p continuous is **not** available on stock CF alone. The UI can present “UHD 4K” as the creative target; the stream engine auto-selects the nearest physics-legal profile (see `docs/05_BITRATE_BUDGET.md`).

## Quick start (PC toolkit)

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r pc/csp_toolkit/requirements.txt
pytest -q
python -m pc.csp_toolkit.cli simulate --mode CROP1080P100 --seconds 3 --out /tmp/demo.csp
python -m pc.csp_toolkit.cli decode --in /tmp/demo.csp --prores /tmp/demo_log.mov
```

Open the menu preview:

```bash
python3 -m http.server 8765 --directory pc/ui_preview
# → http://127.0.0.1:8765
```

## Camera install (high level)

1. Canon firmware **1.2.3**
2. Build / flash Cinema OS bootloader package (see `docs/06_INSTALL.md`)
3. Format CF (exFAT for long takes), copy `autoexec.bin` + `CINEMA/` tree
4. First boot runs CF benchmark → arms a mode under 100 MB/s
5. Delete button → Cinema OS menu (SETTINGS / PHOTO / **CINE** / ADD-ONS / HACKS)

## Documentation map

1. [`docs/01_HARDWARE_ANALYSIS.md`](docs/01_HARDWARE_ANALYSIS.md) — 5D3 sensor, DIGIC, memory, cards  
2. [`docs/02_ARCHITECTURE.md`](docs/02_ARCHITECTURE.md) — Cinema OS layers vs Magic Lantern lessons  
3. [`docs/03_CODEC_SPEC.md`](docs/03_CODEC_SPEC.md) — CSP binary format + algorithms  
4. [`docs/04_HARDWARE_MODS.md`](docs/04_HARDWARE_MODS.md) — SSD bridge, fans, power  
5. [`docs/05_BITRATE_BUDGET.md`](docs/05_BITRATE_BUDGET.md) — math for ≤100 MB/s continuous  
6. [`docs/06_INSTALL.md`](docs/06_INSTALL.md) — build, flash, verify  

## Design reference

Menu chrome follows [`assets/cine_menu_preview.png`](assets/cine_menu_preview.png): amber/copper cinema palette, tabbed shell, hexagonal selection on the active CINE row, dual status bars with storage (CFast/SSD when the bridge is fitted). Interactive twin: `pc/ui_preview/`.

## License & safety

- Firmware sources are intended for **your own** 5D Mark III. Flashing unsigned code risks brick; keep a known-good Canon 1.2.3 recovery path.
- CSP tooling is Apache-2.0 unless noted.
- This project stands on research published by the Magic Lantern community (EDMAC crops, LJ92, mlv_lite, crop_rec). Cinema OS is a new architecture, not a rebranded ML menu.
