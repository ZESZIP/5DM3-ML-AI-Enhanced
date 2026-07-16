# 04 — Hardware Modifications (Optional but Recommended)

Software alone cannot beat the CF controller ceiling. These mods are **optional for 1080p25–50** and **strongly recommended for 1080p100 / 4K50**.

> Warranty void, ESD risk, brick risk. Work only on a body you can afford to lose. Prefer reversible cable assemblies before soldering.

## 1. Priority matrix

| Mod | Why | Difficulty | Needed for |
|-----|-----|------------|------------|
| Fast CF (1066x / 160 MB/s rated) | Sustained ~100 MB/s class | Easy | All cinema modes |
| Dual CF+SD stripe | +15–25 MB/s headroom | Software | 1080p100 comfort |
| Active cooling | Long takes without thermal abort | Medium | Any continuous raw-class |
| Dummy battery / D-tap | Stable voltage under load | Easy | Rigged sets |
| **CF→IDE/SATA SSD bridge** | Approach controller max ~160 MB/s + huge media | Hard | **4K50 full height** |
| External HDMI recorder | ProRes proxy + monitoring | Easy | Focus/pull backup |

## 2. Storage path A — Stock CF (baseline)

**Parts**

- Lexar Professional 1066x / 2000x CF **or** SanDisk Extreme Pro CF 160 MB/s
- UDMA7 USB-C reader on the PC (bottleneck is often the reader)

**Why:** Cinema OS targets ≤100 MB/s so a healthy CF path is enough for `FF1080P50` and `CROP4K25`.

## 3. Storage path B — Dual stripe (no soldering)

Enable `dual_stripe` in ADD-ONS. Cinema OS writes even/odd frame packets to CF and SD with a unified CSP index rebuilt on ingest.

**Parts**

- Fast CF as above
- Fast SDHC/SDXC (still ~20 MB/s class on 5D3 — helps, does not double CF)

**Why / limits:** Gains are real but modest. Use when 1080p100 sits near the edge on CF alone.

## 4. Storage path C — SSD bridge (the “CFast A” path)

The mockup footer **CFast A: 128GB** is the product name for an external solid-state pack. The 5D3 has no native CFast slot; we emulate the role with a bridge.

### 4.1 Concept

```
5D3 CF slot ──passive CF extender──► CF-to-IDE/PATA bridge ──► 2.5" SSD
                                      ▲
                                      └── external 5V (critical)
```

Historical community attempts used CF-IDE adapters (e.g. ES&S-class) and IDE SSDs, or CF-extend developer boards. Modern equivalent BOM:

| Item | Example class | Role |
|------|---------------|------|
| CF male extender / eject extender | Rigid or short flex | Exit the CF door without stress |
| CF to 44-pin IDE adapter | UDMA-capable | Protocol bridge |
| 2.5" SSD with IDE or mSATA+converter | 128–512 GB | Media (`CFast A` volume) |
| Buck module 7.4–8.4V → 5V 2A | From dummy battery | Power SSD (camera CF pin power is insufficient) |
| 3D-printed cage | Custom | Mount under camera / on rods |

### 4.2 Why external power is mandatory

CF slot power was designed for flash cards, not spinning or high-draw SSDs. Undervoltage causes CRC errors that look like “codec bugs”. Cinema OS sets flag `SSD_BRIDGE` when the ADD-ONS probe sees the bridge signature file `/CINEMA/BRIDGE.OK`.

### 4.3 Expected gain

- Sustained writes closer to **UDMA7 ceiling (~150–167 MB/s theoretical)**
- Unlocks `CROP4K50_FULL` budget (~140–160 MB/s CSP) with headroom
- Media size: 128–512 GB packs for long-form

### 4.4 Software side

`dual_stripe` path `PATH_SSD_BRIDGE` treats the mounted volume like CF but raises the governor ceiling and labels the UI footer **CFast A**.

## 5. Thermal pack

| Part | Placement | Why |
|------|-----------|-----|
| 20–30 mm 5V blower or axial fan | Bottom plate / battery grip vent path | Forced air across DIGIC zone |
| Graphite thermal pad | DIGIC shield (if opened) | Spread heat to chassis |
| Aluminum cage / cheese plate | Bottom | Heatsink + rig mount |
| Temperature probe (optional I2C) | Near CF bay | Feed Cinema OS governor |

**Wiring:** Fan from dummy battery 5V rail with a switch. Do not steal from HDMI 5V.

**Software:** `stream_engine` reduces quantizer headroom first, then fps, then aborts at critical temperature.

## 6. Power

| Part | Why |
|------|-----|
| Dummy battery + D-Tap / PD trigger | Continuous draw for record + fan + SSD |
| LP-E6 style cells as backup | Hot-swap on run-and-gun |

Voltage sag is a leading cause of “random” ML/Cinema stops.

## 7. Monitoring / proxy (non-invasive)

| Part | Why |
|------|-----|
| HDMI clean out → Atomos / Blackmagic | Focus + safety ProRes proxy |
| External recorder record-trigger | Sync start with camera REC |

Internal CSP remains the grade master; HDMI is 8-bit 4:2:2 — proxy only.

## 8. What not to do

- Do not replace the sensor or DIGIC hoping for 6K60 — pinout/firmware cost dwarfs buying a modern body
- Do not force UHS-I expectations on the SD slot
- Do not run SSD bridge without external 5V
- Do not block the CF door with a hot flex that shortens against the shutter pack

## 9. Recommended build tiers

### Tier 1 — Student / short film (no open body)

Fast CF + dummy battery + HDMI monitor. Software: Cinema OS CSP temporal for 1080p100 lite.

### Tier 2 — Indie cinema

Tier 1 + dual stripe + bottom fan plate.

### Tier 3 — Max squeeze

Tier 2 + powered SSD bridge labeled **CFast A** + graphite pad + cheese plate. Enables serious 4K50-class CSP.
