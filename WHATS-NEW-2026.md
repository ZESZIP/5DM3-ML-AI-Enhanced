# 2026 UX / Performance Upgrade for the 5D Mark III

This build is a custom compile of [Magic Lantern](https://www.magiclantern.fm/) for the
Canon 5D Mark III (firmware 1.2.3), based on the official
[`magiclantern_simplified`](https://github.com/reticulatedpines/magiclantern_simplified)
source tree at commit `ed3e7c0d` (the exact commit used for the `2025-06-20.5D3.123`
nightly that shipped in this repo before this update). The full, buildable source
(with our changes applied) now lives in [`magic-lantern-src/`](magic-lantern-src/).

Nothing here changes how you install ML: copy `autoexec.bin`, `ML-SETUP.FIR` and the
`ML/` folder to the root of your card exactly as before. It's still a pure
"install and play" SD-card package - no network connection, phone app, or PC
companion tool is required at any point.

## What changed and why

The goal was to reduce the load ML/the 5D3's DIGIC5 puts on itself during LiveView
and recording, so you can push resolution/FPS further, plus give you explicit
control over the camera's own protective auto-shutoff behavior. Three targeted,
low-level firmware changes were made (all in C, in the same style/APIs as the rest
of the codebase - nothing bolted on from outside):

### 1. LiveView Resolution (%) - `Movie menu > Crop mode > LiveView Resolution`

A new quick control living right in the existing `crop_rec` module menu (module
`crop_rec.mo`). Instead of hand-dialing an absolute pixel count, turn the wheel to
pick **100% / 75% / 50% / 25%** of the current crop mode's resolution ceiling.
Lower values mean the sensor readout hook requests fewer lines, which means less
data for DIGIC to demosaic/compress/write - a lighter LiveView feed and more
headroom for sustained high-resolution or high-FPS recording. It's built directly
on top of the existing (already field-tested) "Target YRES" register-override
mechanism in `crop_rec.c`, so it inherits the same safety bounds; it's just a
friendlier, percentage-based way to drive it with a dial instead of typing pixels.
The old "Target YRES" field is still there (now filed under "Advanced") for anyone
who wants to fine-tune an exact pixel count instead.

*Files: `magic-lantern-src/modules/crop_rec/crop_rec.c`*

### 2. Never power off (override) - `Prefs menu > Powersave in LiveView`

A new toggle that continuously resets Canon's own Auto Power Off / 30-minute
LiveView timers, **regardless of what "Auto power off" is set to in the Canon
menu**. Turn this on and the camera will keep LiveView/recording running rather
than shutting itself down on its own - the LED blinks every 3 seconds as a
reminder that this is active. This is on top of the pre-existing behavior where ML
already suspends Auto Power Off automatically while raw video is recording; this
new switch makes it unconditional and independent from the Canon menu setting.

This is explicitly an "I accept the risk" switch: with it on there is **no**
thermal or power-saving safety net left. If the body starts feeling too hot to
comfortably hold, pull the battery - exactly like the plan you described. We did
not attempt to fake or bypass genuine SD/CF write-speed limits (that would just
corrupt your recordings); this only touches the auto-shutoff/timeout behavior.

*Files: `magic-lantern-src/src/powersave.c`*

### 3. Higher FPS ceiling - `Movie menu > FPS override > Desired FPS`

The selectable FPS list has been extended past the previous 70 fps ceiling, up to
**120 fps** (75/80/85/90/96/100/110/120), reusing the exact same, already-proven
FPS-override engine (`fps-engio.c`) that ML has always used for the 5D3. That
engine automatically clamps whatever you dial in to the fastest frame rate the
current sensor readout mode can actually sustain (`FPS_TIMER_A/B` hardware
limits), so this is an honest ceiling raise, not a promise that every mode can hit
120 fps flat-out. In practice, the higher entries are most useful when combined
with a reduced-resolution `crop_rec` preset and/or a lower "LiveView Resolution %"
above 1080p/720p native, since that's what actually shortens the frame period on
this sensor. In native, uncropped 720p you should expect roughly up to ~60-67 fps
in practice; the crop/binned presets are where the extra headroom pays off.

*Files: `magic-lantern-src/src/fps-engio.c`*

## Where the source lives / how to rebuild

The full ML source tree (all camera platforms, unmodified except for the three
files above) is under [`magic-lantern-src/`](magic-lantern-src/). To rebuild just
the 5D3.123 package:

```bash
# one-time toolchain setup (Debian/Ubuntu)
sudo apt-get install -y gcc-arm-none-eabi
pip3 install docutils

cd magic-lantern-src/platform/5D3.123
make -j"$(nproc)"
# output: build/zip/autoexec.bin, build/zip/ML/, build/zip/ML-SETUP.FIR
```

Copy `build/zip/autoexec.bin`, `build/zip/ML-SETUP.FIR` and `build/zip/ML/` to the
root of the workspace (replacing the ones here) to refresh the install package.
See `magic-lantern-src/developer_guide/04_00_building_ML.md` for more detail on the
build system.

## Safety notes (please read)

- This is still Magic Lantern: **it runs alongside Canon's firmware, from the card,
  and can be fully removed by deleting `autoexec.bin`** (or just booting without
  the card). It does not flash or permanently modify the camera.
- "Never power off (override)" and pushing FPS/resolution further than stock both
  increase the chance of the camera getting hot during long sessions. If it's ever
  too hot to comfortably hold, remove the battery.
- As always with raw video on the 5D3, actual sustainable resolution/FPS/bit-depth
  combinations are ultimately limited by your card's real write speed - see
  `README.md` in this repo for the bandwidth formula used to plan a shoot.
