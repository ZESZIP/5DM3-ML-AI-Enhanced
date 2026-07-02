# 2026 UX / Performance Upgrade for the 5D Mark III

This build is a custom compile of [Magic Lantern](https://www.magiclantern.fm/) for the
Canon 5D Mark III (firmware 1.2.3), based on the official
[`magiclantern_simplified`](https://github.com/reticulatedpines/magiclantern_simplified)
source tree at commit `ed3e7c0d`. The full, buildable source (with our changes) lives in
[`magic-lantern-src/`](magic-lantern-src/).

Install is unchanged: copy `autoexec.bin`, `ML-SETUP.FIR` and the `ML/` folder to your
card root. No network, phone app, or PC tool required.

## Phase 6: Cine AI Enhanced — boot wizard & pro menu shell

### First startup
Full-screen wizard with **progress bar**:
1. Hardware scan (5D3 + firmware)
2. CF/SD detection + integrity test
3. Card write benchmark
4. Arm all recording hacks
5. Auto-apply best MLV profile
6. Save config

### Delete menu (Trash)
- **No classic Magic Lantern chrome** (gray footer, ML scroll arrows, version spam)
- **850ms branded splash:** film icon + **CINE AI ENHANCED** + *A branch of Magic Lantern*
- Five-page nav + chunky pro lists + orange **CINE** canvas
- Footer: `CINE AI ENHANCED | Branch of Magic Lantern`
- Closing menu re-calibrates profile from your settings

### Day-to-day
Pick options on the **CINE** page → OS auto-calibrates crop/MLV/FPS → press record.

*Files: `cinema_boot.c`, `cinema_os.c`, `menu.c`*

---

## Phase 5: Five-page Cinema OS UI + adaptive buffer governor

### Global navigation (Delete menu)

Persistent **matte black** top bar with five pages:

| Page | Color | ML backend |
|------|-------|------------|
| **SETTINGS** | Yellow | Prefs |
| **PHOTO** | Green | Shoot |
| **CINEMATIC** | Orange | Movie |
| **ADD-ONS** | Blue | Modules |
| **HACKS** | Purple | Debug |

Wheel **left/right** switches pages. Active tab shows colored underline.

### CINEMATIC page (spec layout)

Full **orange canvas** with scrollable recording rows:

- RESOLUTION, FRAME RATE, CODEC/FORMAT, GAMMA CURVE
- SHUTTER, APERTURE, ISO/GAIN, WHITE BALANCE, AUDIO MONITOR

Each row: boxed abbreviation, `TITLE | live value`, chevron. White selection pill.
Right-edge scrollbar. **SET** opens the linked ML submenu.

### Dynamic data-rate governor

When recording **4K 14-bit MLV**, monitors CF write speed (`raw.write.speed`).
If throughput drops below sustained threshold, recording **continues** and format
adapts automatically:

1. 14-bit native → **14-bit LJ92**
2. Still underrun → **12-bit LJ92**

The **CODEC/FORMAT** row updates live (e.g. `12-bit MLV RAW (adaptive)`).

*Files: `cinema_os.c`, `cinema_governor.c`, `menu.c`, `mlv_lite.c`*

---

## Phase 4: Cinema OS shell + BEAST power modes

### Cinema OS — your own menu on top of ML

Press **Delete** and you get **Cinema OS 2026** instead of classic ML chrome (toggle:
`Display → Cinema UI → Cinema OS shell`, ON by default):

- **56px colored tab bar** — big ML icons, short labels (CINE, SHOOT, EXPO, FOCUS, …)
- **CINE tab** — full orange workspace, white bold menu rows, black/orange selection invert
- Other tabs — black content area with colored accent stripe matching category
- Taller 40px menu rows for touch-friendly readability on the 720×480 LCD

Core ML menus are unchanged underneath — Cinema OS is a presentation layer on `menu.c`.

*Files: `magic-lantern-src/src/cinema_os.c`, `cinema_os.h`, hooks in `menu.c`*

### BEAST power modes — `Movie → Cinema Record → POWER MODE`

One-tap optimized profiles that arm crop + MLV + FPS + preview together:

| Mode | What it sets |
|------|----------------|
| **BEAST 4K25** | UHD crop, ~3840 window, 16:9, **10-bit**, 25 fps, 50% preview, small hacks ON |
| **BEAST 1080p120** | 3×3 crop, 50% readout, 1920, **10-bit**, 120 fps target, 25% preview |
| **MANUAL** | Your scroll selections (resolution, aspect, depth, FPS, anamorphic) |

BEAST modes also enable `raw.small.hacks` and aggressive preview downscale so DIGIC work
stays lower while the MLV file keeps full configured resolution. **1080p120** still needs
Canon **720p 50/60** in the body menu; the 5D3 may clamp sustained FPS below 120.

*Files: `magic-lantern-src/modules/cine_ui/cine_record.c`*

---

## Phase 3: Sony-style Cinema Record UI

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
make VERSION="CINEMA.OS.2026.5D3" -j"$(nproc)"
# output: build/autoexec.bin, build/zip/ML/
```

## Safety

- ML loads from the card; delete `autoexec.bin` to disable.
- Power-off override and high FPS/resolution increase heat risk.
- Sustainable raw modes depend on real card write speed — see `README.md` bandwidth formula.
