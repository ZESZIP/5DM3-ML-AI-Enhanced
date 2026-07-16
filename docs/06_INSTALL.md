# 06 — Install & Build

## PC toolkit (works today)

```bash
cd /path/to/Cinema-OS-5D3
python3 -m venv .venv
source .venv/bin/activate
pip install -r pc/csp_toolkit/requirements.txt
pytest -q
```

Simulate a camera clip and expand to ProRes Log:

```bash
python -m pc.csp_toolkit.cli simulate --mode CROP1080P100 --seconds 2 --out /tmp/shot.csp
python -m pc.csp_toolkit.cli info --in /tmp/shot.csp
python -m pc.csp_toolkit.cli decode --in /tmp/shot.csp --prores /tmp/shot_log.mov
```

UI preview:

```bash
python3 -m http.server 8765 --directory pc/ui_preview
```

## Firmware cross-build (DryOS / ML-compatible toolchain)

Requirements (host Linux):

- `arm-none-eabi-gcc` (or the Magic Lantern documented ARM GCC)
- `make`
- Python 3 for asset packing

```bash
# After dropping a DryOS stub tree into firmware/platform/5D3.123 (see stubs README)
cd firmware
make PLATFORM=5D3.123 cinema_os
```

Outputs (when toolchain present):

- `dist/autoexec.bin`
- `dist/CINEMA/` modules + UI bitmaps
- `dist/CINEMA-SETUP.FIR` (first-time enable)

### Card layout

```
/autoexec.bin
/CINEMA-SETUP.FIR          # first boot only
/CINEMA/
  modules/
    stream_engine.mo
    csp_rec.mo
    dual_stripe.mo
    cinema_ui.mo
  ui/
    low_poly_bg.bmp
    hex_cap_l.bmp
    hex_cap_r.bmp
  BRIDGE.OK                # optional: created by SSD pack
```

1. Canon menu → firmware 1.2.3  
2. Format card in camera (exFAT for long clips)  
3. Copy files to root  
4. Run `CINEMA-SETUP.FIR` once  
5. Reboot → Delete button opens Cinema OS  

## Verify

- Help/version string contains `CINEMA.OS.CSP`
- CINE tab CODEC shows `CSP STREAM`
- Footer storage shows `CF A` or `CFast A`
- Record 10 s → PC `csp_toolkit info` reports no CRC errors

## Recovery

Remove `autoexec.bin`, power with battery door open for 10 s, boot stock Canon. Re-install Canon 1.2.3 FIR if needed.
