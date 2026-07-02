# 2026 UX / Performance Upgrade for the 5D Mark III

This build is a custom compile of [Magic Lantern](https://www.magiclantern.fm/) for the
Canon 5D Mark III (firmware 1.2.3), based on the official
[`magiclantern_simplified`](https://github.com/reticulatedpines/magiclantern_simplified)
source tree at commit `ed3e7c0d`. The full, buildable source (with our changes) lives in
[`magic-lantern-src/`](magic-lantern-src/).

Install is unchanged: copy `autoexec.bin`, `ML-SETUP.FIR` and the `ML/` folder to your
card root. No network, phone app, or PC tool required.

## Phase 3: Sony-style Cinema Record UI (this update)

### One-screen recording setup — `Movie → Cinema Record`

A full-screen scroll menu (Sony-style): big labels, orange highlight row, immediate
apply to crop mode + MLV + FPS when you change any value.

| Row | Options |
|-----|---------|
| **Resolution** | 720p, 1080p, 2.7K, 4K, 6K |
| **Aspect** | 16:9, 2.39:1, 2.35:1, 2:1, 4:3, 1:1 (auto-crops sensor via MLV) |
| **Bit depth** | 14-bit, 12-bit, 10-bit, 8-bit (lossless where supported) |
| **Frame rate** | 24, 25, 50, 60, 100, 120 fps |
| **Anamorphic** | OFF / 2x squeeze (LiveView preview stretch) |

**Controls:** joystick or wheel left/right changes value; up/down changes row; SET applies
immediately. Auto-apply on scroll is ON by default (400 ms debounce after crop changes).

This drives the same backends as the classic menus (`crop_rec`, `mlv_lite`, `fps-engio`) —
nothing removed, just unified. High FPS / 4K / 6K still clamp to what the 5D3 sensor and
your card can sustain.

*Files: `magic-lantern-src/modules/cine_ui/cine_record.c`*

---

## Phase 2: Cinema UI / split preview vs recording

### Recording readout % vs Preview scale %

Two separate controls — do not confuse them:

| Control | Menu path | What it affects |
|---------|-----------|-----------------|
| **Recording readout %** | `Movie > Crop mode > Recording readout %` | Sensor crop / lines read for **recording** (MLV file). Less data = more FPS headroom. |
| **Preview scale %** | `Movie > RAW video > Preview scale %` | **LiveView preview only**. MLV still records at full configured resolution/format. |

Typical workflow: use **Recording readout %** when you need real sensor/bandwidth savings;
use **Preview scale %** when the file is fine but you want a lighter monitor while rolling.

### Color-coded menus (2026 UX)

`Display > Cinema UI > Color-coded menus` (ON by default) tints menu tabs, selection bars,
and submenu headers by category:

- **Orange** — Movie (raw video, crop, FPS)
- **Green** — Shoot
- **Yellow** — Expo
- **Cyan** — Focus
- **Blue** — Display / Overlay
- **Magenta** — Audio
- **Gray** — Prefs / Debug

**Nothing is hidden or moved** — every feature stays in its original menu. The color legend
under `Display > Cinema UI > Color legend` is a quick map, not a replacement menu.

### Cinema dashboard (LiveView bars)

`Display > Cinema UI > Cinema dashboard` adds compact tags on the LiveView top bar:

- FPS (cyan)
- `RO:xx%` when Recording readout % &lt; 100 (orange)
- `PV:xx%` when Preview scale % &lt; 100 (blue)
- `REC` while recording (red)
- `PWR!` when Never power off override is active (red)

**Lite LV while recording** hides non-essential dashboard tags during a take to reduce overlay work.

*New module: `ML/modules/cine_ui.mo`. Core menu tinting: `magic-lantern-src/src/menu.c`*

---

## Phase 1 features (still included)

### 1. Recording readout % — `Movie > Crop mode > Recording readout %`

Percentage dial (**100 / 75 / 50 / 25**) on top of the existing `crop_rec` YRES mechanism.
Lower values request fewer sensor lines for recording. Overridden by Advanced **Target YRES**
if set.

*Files: `magic-lantern-src/modules/crop_rec/crop_rec.c`*

### 2. Never power off (override) — `Prefs > Powersave in LiveView`

Keeps LiveView running by resetting Canon auto power-off timers (LED blinks every 3 s).
**Pull the battery if the body gets too hot.**

*Files: `magic-lantern-src/src/powersave.c`*

### 3. Higher FPS ceiling — `Movie > FPS override > Desired FPS`

Selectable list extended to **120 fps**; hardware still clamps to what the current readout
mode can sustain. Best combined with crop presets and/or reduced recording readout %.

*Files: `magic-lantern-src/src/fps-engio.c`*

## Hardware reality (5D3)

From `README.md` in this repo: 14-bit Bayer, LJ92 lossless, ~145 MB/s practical CF write
budget. Preview downscaling and readout crop **do not** increase card bandwidth — they reduce
DIGIC/sensor work so you can hold higher modes longer. Card speed still caps resolution × FPS ×
bit depth.

## Rebuild

```bash
sudo apt-get install -y gcc-arm-none-eabi
pip3 install docutils

cd magic-lantern-src/platform/5D3.123
make VERSION="2026-07-02.5D3.123.PROC5D3" -j"$(nproc)"
# output: build/autoexec.bin, build/zip/ML/
```

## Safety

- ML loads from the card; delete `autoexec.bin` to disable.
- Power-off override and high FPS/resolution increase heat risk.
- Sustainable raw modes depend on real card write speed — see `README.md` bandwidth formula.
