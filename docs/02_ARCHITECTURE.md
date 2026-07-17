# 02 — Cinema OS Architecture

## 1. Design goals

1. **Continuous recording** for short-form → feature work (minutes, not seconds)
2. **≤100 MB/s** sustained write on stock CF (bridge optional for extreme modes)
3. **Live preview** (LCD + HDMI) during record
4. **12-bit log-capable** pipeline → ProRes Log offline
5. **UI** matching the amber cinema mockup (not classic ML chrome)
6. **Hardware-positive** — SSD bridge, cooling, dual stripe are first-class citizens

## 2. Layer cake

```
┌─────────────────────────────────────────────────────────┐
│  cinema_ui          BMP shell: SETTINGS/PHOTO/CINE/...  │
├─────────────────────────────────────────────────────────┤
│  csp_rec            Mode arming, CSP mux, audio sync    │
├─────────────────────────────────────────────────────────┤
│  stream_engine      Capture → compress → write (0-buf)  │
├─────────────────────────────────────────────────────────┤
│  dual_stripe        CF / SD / SSD path abstraction      │
├─────────────────────────────────────────────────────────┤
│  DryOS + DIGIC stubs (bootloader / autoexec host)       │
└─────────────────────────────────────────────────────────┘
                         │ CF/SSD *.csp
                         ▼
┌─────────────────────────────────────────────────────────┐
│  pc/csp_toolkit     Decode → debayer → CSP-Log → ProRes │
└─────────────────────────────────────────────────────────┘
```

## 3. Zero-buffer stream engine

```
  LV VSync
     │
     ▼
  [slot A] raw Bayer window (EDMAC crop)
     │
     ├─► preview path (Canon YUV) ── LCD/HDMI
     │
     ▼
  CSP compress (in place / scratch)
     │
     ▼
  dual_stripe.write(frame)     # must finish before next VSync + slack
     │
     ▼
  [slot B] captures next frame (ping-pong)
```

**Rules:**

- Max resident compressed frames waiting on I/O: **0** (writer blocks compressor)
- Max raw slots: **2** (ping-pong)
- If `write_deadline` missed → `STREAM_UNDERRUN` → stop + flush index
- No “record until buffer full then crawl” mode in cinema profile

## 4. Module responsibilities

### `stream_engine`

- Owns EDMAC crop dimensions for the armed mode
- Runs CSP compress callback with a hard byte budget per frame
- Publishes live metrics: write MB/s, last frame bytes, thermal flag

### `csp_rec`

- Maps UI mode → register preset + CSP header metadata
- Applies CSP-Log tone curve tag (encoding is linear sensor; log applied in PC *or* optional light pre-emphasis)
- Interleaves audio blocks (16-bit PCM) on a secondary timeline

### `dual_stripe`

- `PATH_CF`, `PATH_SD`, `PATH_STRIPE`, `PATH_SSD_BRIDGE`
- Stripe = even frames CF / odd frames SD (or MB-level striping) with a recovery index in the CSP footer

### `cinema_ui`

- Full-screen Delete-menu replacement
- Tabs: SETTINGS / PHOTO / **CINE** / ADD-ONS / HACKS
- CINE list: LUT, FRAME RATE, Resolution, CODEC, AUDIO, ASPECT RATIO
- Selection chrome: copper hex/chevron bar per mockup
- Footer: battery, clock, storage (`CFast A` when bridge present, else `CF A`)

## 5. Boot sequence

1. `autoexec` loads Cinema OS core
2. Probe storage paths + short write benchmark (~2–4 s)
3. Select default mode whose **worst-case CBR** ≤ 90% of measured write
4. Show splash → idle LiveView with HUD
5. User opens menu → arms CSP → REC starts stream_engine

## 6. Host / PC half

The camera never produces ProRes. The PC toolkit:

1. Validates CSP index
2. Decodes I/P frames → 12-bit Bayer
3. Debayers (optional quality tiers)
4. Applies **CSP-Log12** curve + 5D3 color matrix
5. Exports ProRes 422 HQ / 4444 via `ffmpeg` (or image sequence for Resolve)

## 7. Relationship to Magic Lantern

Cinema OS **reuses the proven DIGIC control surface** (stubs, EDMAC, LV hooks) that ML pioneered, but:

- Container = **CSP**, not MLV
- Write strategy = **hard streaming**, not variable deep buffering
- UI = **cinema shell**, not ML menu tree
- HFPS strategy = **temporal CSP**, not only LJ92 stills compression

Firmware C in this repo is structured to compile against a DryOS/ML-compatible toolchain (`platform/5D3.123`). Until cross-built `autoexec.bin` artifacts are published, the **executable source of truth for the codec** is the Python reference + C portable core under `firmware/src`.
