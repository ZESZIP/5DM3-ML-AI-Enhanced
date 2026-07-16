# Platform: Canon 5D Mark III firmware 1.2.3

This directory is the integration point for DryOS stubs / linker scripts used when cross-compiling Cinema OS to `autoexec.bin`.

## Expected contents (supplied by your ARM/ML toolchain tree)

- `stubs.S` — DIGIC/DryOS function stubs for 5D3.123  
- `consts.h` — memory map / LV raw buffer pointers  
- `Makefile` — link `autoexec.bin`  
- `ML-SETUP.FIR` or `CINEMA-SETUP.FIR` installer  

Cinema OS modules (`csp_rec`, `stream_engine`, `cinema_ui`, `dual_stripe`) compile against these stubs the same way Magic Lantern modules do, but they implement the **CSP streaming** path instead of MLV deep buffering.

Until the stub tree is present, use:

- `firmware/src/csp_codec.c` + host `Makefile` for codec unit builds  
- `pc/csp_toolkit` as the golden reference encoder/decoder  
