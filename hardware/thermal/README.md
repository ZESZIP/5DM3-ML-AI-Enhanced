# Thermal Pack

Long CSP recording keeps the DIGIC 5+ and CF controller warm. Magenta frames and random stops are often thermal / power, not “codec bugs”.

## Fan plate

- 20–30 mm blower exhausting out the bottom  
- Intake from battery-grip side vents  
- Power from dummy-battery 5 V rail with switch  
- Aim airflow across the CF bay / DIGIC area

## Software interaction

Cinema OS `stream_engine` thermal flag:

1. Raise quantizer (bitrate still capped)  
2. Drop to next safer mode  
3. Clean stop + flush CSP index  

## Open-body graphite (advanced)

Only if you already service shutters: thin graphite between DIGIC shield and magnesium body. Avoid shorting test pads.
