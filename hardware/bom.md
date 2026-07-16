# Hardware Bill of Materials — Cinema OS 5D3

## Tier 1 — Continuous 1080p (no body open)

| Qty | Part | Why |
|-----|------|-----|
| 1 | Canon EOS 5D Mark III @ FW 1.2.3 | Target body |
| 2 | Lexar 1066x / SanDisk Extreme Pro CF 64–128 GB | Sustained ~100 MB/s class writes |
| 1 | UDMA7 USB-C CF reader | Offload without USB2 camera bottleneck |
| 1 | Dummy LP-E6 + D-Tap / PD | Stable voltage for long takes |
| 1 | HDMI monitor / recorder (optional) | Focus + 8-bit proxy |

## Tier 2 — Long takes + 1080p100 comfort

| Qty | Part | Why |
|-----|------|-----|
| 1 | Bottom cheese plate with 20–30 mm 5V fan | Keep DIGIC under thermal abort |
| 1 | Fast SDXC (for dual_stripe) | +15–25 MB/s headroom |
| 1 | Graphite pad sheet | Optional chassis heat spreading |

## Tier 3 — “CFast A” SSD bridge (4K50-class)

| Qty | Part | Why |
|-----|------|-----|
| 1 | CF male eject extender | Exit CF door cleanly |
| 1 | CF ↔ 44-pin IDE / PATA bridge (UDMA capable) | Protocol adapter |
| 1 | 2.5" IDE SSD or mSATA+converter 128–512 GB | Large media pack |
| 1 | 7.4–8.4 V → 5 V 2 A buck | **Required** SSD power |
| 1 | Rod cage / 3D-printed enclosure | Strain relief + cooling airflow |
| 1 | File `CINEMA/BRIDGE.OK` on volume | Cinema OS UI shows **CFast A** |

See `docs/04_HARDWARE_MODS.md` for wiring and safety.

## Consumables

- Spare LP-E6 batteries  
- Compressed air / sensor swab kit (crop modes show dust)  
- Thermal paste/pads rated for electronics (no conductive grease on DIGIC balls)
