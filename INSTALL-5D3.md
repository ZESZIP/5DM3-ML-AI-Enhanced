# Install 5D3 AI Cinema build (READ THIS)

**If your camera shows `Magic Lantern 2025-06-20` in Help, you have the WRONG files.**
You need build version **`5DM3.AI.Cinema.2026`** (or `2026-07-02...PROC5D3`).

The default GitHub `main` branch was updated to include this build. You can also use branch
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
5. Reboot. Enter LiveView. You should see:
   - Popup: **"5DM3 AI CINEMA 2026 loaded"**
   - LiveView bar tag: **AI-CINEMA** (orange)
6. Press **Delete** → ML menu opens on **Movie** tab → **Cinema Record**

## Verify correct build

Press Delete → **Help** tab. Version must contain **`AI.Cinema`** or **`PROC5D3`** / **2026**.

`ML/modules/cine_ui.mo` must exist on the card (~7 KB). Without it, no Cinema UI loads.

## Download options

| Method | What to get |
|--------|-------------|
| **Easiest** | Download `5DM3-AI-Cinema-INSTALL.zip` from repo root |
| Git | `git clone` then use files at repo root (not `main` old zip from before merge) |
| Build | `cd magic-lantern-src/platform/5D3.123 && make VERSION="5DM3.AI.Cinema.2026" -j4` |

## Cinema Record (main UI)

**Movie → Cinema Record** — full-screen Sony-style scroll:

- Resolution: 720p → 6K
- Aspect: 16:9, 2.39, 2.35, 2:1, 4:3, 1:1
- Bit depth: 14 / 12 / 10 / 8-bit
- FPS: 24, 25, 50, 60, 100, 120
- Anamorphic 2x preview

Joystick/wheel changes values; settings auto-apply and arm RAW recording.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Looks like old ML | Re-copy `autoexec.bin` + full `ML/` from this repo; check Help version |
| No Cinema Record menu | `cine_ui.mo` missing from `ML/modules/` |
| No RAW / log | Open Cinema Record, pick settings, wait for **ARMED**, then record MLV |
| Camera won't boot | Battery out; ensure `autoexec.bin` on card root |
| Module crash on boot | Delete `ML/settings/*.cfg` or format card and reinstall |
