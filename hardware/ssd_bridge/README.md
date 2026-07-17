# SSD Bridge (“CFast A”) Assembly Notes

```
[5D3 CF slot]
     │ short CF extender
     ▼
[CF-to-IDE bridge] ----5V 2A---- [buck from dummy battery]
     │
     ▼
[2.5" SSD]  mounted in cage under camera
```

## Wiring checklist

1. Confirm bridge pinout matches CF True IDE mode (not memory-only).  
2. Do **not** power the SSD from camera CF VCC alone.  
3. Common ground between buck and bridge.  
4. Add ferrite on 5V if HDMI noise appears.  
5. Create `/CINEMA/BRIDGE.OK` (empty file) so Cinema OS labels storage **CFast A**.

## Expected performance

| Metric | Target |
|--------|--------|
| Sustained write | 140–160 MB/s (controller limited) |
| Useful modes | `CROP4K50_FULL`, long `CROP1080P100` |
| UI footer | `CFast A: <size>` |

## Failure modes

| Symptom | Likely cause |
|---------|--------------|
| CRC errors in CSP | Undervoltage / bad cable |
| Camera freeze on insert | Bus contention — power SSD after camera boot |
| Slow as CF card | Bridge not UDMA — replace adapter |
