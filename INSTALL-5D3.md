# Install 5D3 Cinema OS build (READ THIS)

**If your camera shows `Magic Lantern 2025-06-20` in Help, you have the WRONG files.**
You need build version **`CINE.AI.ENHANCED.5D3`** (or `CINEMA.OS.2026.5D3`).

The default GitHub `main` branch includes this build. You can also use branch
`cursor/5d3-2026-ux-performance-upgrade-4a83`.

## Quick install

1. **Firmware:** Canon 5D Mark III must be **1.2.3**
2. Format CF/SD card **in camera** (FAT32)
3. Copy these to the **card root** (not in a subfolder):

```
autoexec.bin
ML-SETUP.FIR
ML/          (entire folder)
```

4. **First time only:** Canon menu → firmware version → run update with **ML-SETUP.FIR**
5. Reboot. **First boot:** full-screen setup wizard with progress bar (hardware scan, CF test, benchmark, profile). Enter LiveView if prompted.
6. **Next boots:** silent calibration, then ready.
7. Press **Delete** → brief **CINE AI ENHANCED** splash → modern OS (no classic ML menu chrome)

## Verify correct build

Press Delete → **Help** tab. Version must contain **`CINE.AI.ENHANCED`** or **`CINEMA.OS`**.

`ML/modules/cine_ui.mo` must exist on the card (~7 KB). Without it, no Cinema Record UI loads.

## Download options

| Method | What to get |
|--------|-------------|
| **Easiest** | Download `5DM3-AI-Cinema-INSTALL.zip` from repo root |
| Git | `git clone` then use files at repo root |
| Build | `cd magic-lantern-src/platform/5D3.123 && make VERSION="CINEMA.OS.2026.5D3" -j4` |

## Cinema OS (Delete menu)

Five operational pages on a black nav bar (wheel left/right to switch):

| Tab | Color | Purpose |
|-----|-------|---------|
| SETTINGS | Yellow | Prefs |
| PHOTO | Green | Shoot |
| **CINE** | **Orange** | Recording sensing / settings list |
| ADD-ONS | Blue | Modules |
| HACKS | Purple | Debug |

**CINE page** — full orange canvas, chunky 56px rows, 9 scrollable settings. Footer shows live **CF MB/s** and auto **PROFILE**. SET opens linked ML control.

### Write engine (boot)

On boot (~10s): arms all cinema hacks (never power off, small hacks, SRM memory, ML preview, card test, governor), runs a **quiet CF benchmark**, and auto-applies the best MLV profile for your card speed. Re-probes every 90s in LiveView. LiveView bar shows CF speed tag.

Toggle shell: `Display → Cinema UI → Cinema OS shell` (ON by default).

## Cinema Record (main recorder)

**Movie → Cinema Record** — full-screen orange recorder:

| Row | Options |
|-----|---------|
| **POWER MODE** | MANUAL, **BEAST 4K25**, **BEAST 1080p120** |
| **Resolution** | 720p → 6K |
| **Aspect** | 16:9, 2.39, 2.35, 2:1, 4:3, 1:1 |
| **Bit depth** | 14 / 12 / 10 / 8-bit MLV |
| **Frame rate** | 24, 25, 50, 60, 100, 120 |
| **Anamorphic** | OFF / 2x squeeze preview |

**BEAST 4K25** — UHD crop, 10-bit, 25 fps, 50% preview (optimized for ~4K raw).
**BEAST 1080p120** — 3×3 crop, 50% readout, 10-bit, 120 fps target, 25% preview.
Set Canon menu to **720p 50/60** for high-FPS beast mode.

Joystick/wheel changes values; settings auto-apply. Footer shows **ARMED** when RAW is enabled.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Looks like old ML | Re-copy `autoexec.bin` + full `ML/` from this repo; check Help version |
| No Cinema Record menu | `cine_ui.mo` missing from `ML/modules/` |
| No RAW / log | Open Cinema Record, pick BEAST or manual settings, wait for **ARMED**, record MLV |
| Pink/broken preview | Lower Preview scale %; try Preview mode = ML in mlv_lite |
| Camera won't boot | Battery out; ensure `autoexec.bin` on card root |
| Module crash on boot | Delete `ML/settings/*.cfg` or format card and reinstall |
