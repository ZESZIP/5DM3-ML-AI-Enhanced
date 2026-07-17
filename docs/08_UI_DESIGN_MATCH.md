# 08 — UI Design Match (Mockup → Preview v2)

Source art: [`assets/cine_menu_preview.png`](../assets/cine_menu_preview.png)  
Live twin: [`pc/ui_preview/index.html`](../pc/ui_preview/index.html)

## Visual analysis of the mockup

### Atmosphere

- Warm monochrome world: mahogany → burnt orange → amber highlights on near-black charcoal
- Background is a **spacious** low-poly mountain range, not a flat fill
- Peaks read as **hardened crystal / faceted stone**: sharp ridges, triangular lit faces, deep cleft shadows
- Strong directional key light from upper-right: bright orange specular on upper facets, near-black recesses
- Foreground ridges darker; far peaks lower contrast → depth / “air”
- UI chrome sits as a glass-like overlay (readable white type) without boxing the whole screen into cards

### Chrome

| Region | Spec |
|--------|------|
| Bezel | Thin silver LCD frame |
| Top bar | Battery 85% · centered **MENU** · Wi‑Fi + 14:32 |
| Tabs | SETTINGS (gold) · PHOTO (green) · **CINE** (orange + underline) · ADD-ONS (blue) · HACKS (purple), each with a small icon |
| List | Two columns: label left, value right; generous vertical spacing |
| Selection | Elongated **hexagonal copper prism** with pointed ends, glossy bevel, movie-camera glyph on Resolution |
| Defaults | LUT RAW · FRAME RATE 25P · Resolution UHD 4160×2560 [4K] · CODEC MOV · AUDIO STEREO · ASPECT 1.85:1 |
| Footer | 85% · 14:32 · **CFast A: 128GB** |
| Scroll | Thin copper rail on the right edge |

### Hardened crystal selection (critical)

Not a flat orange rectangle and not a soft pill:

1. Plan shape = elongated hexagon / crystal slab (chevron points left & right)
2. Fill = vertical copper metal gradient (highlight → mid amber → deep mahogany)
3. Edge treatment = bright specular on the light-facing bevel, darker facet on the opposite tip
4. Slight outer drop shadow so the slab lifts off the mountain field
5. White label/value with a soft dark text shadow for punch

## Implementation mapping (preview v2)

| Mockup trait | Implementation |
|--------------|----------------|
| Spacious mountains | Multi-layer inline SVG (`far` / `mid` / `near` / `base`) + sun bloom + haze |
| Crystal facets | Separate lit paths (`crystalHi`) and shadow paths on each ridge |
| Motion | Slow `mountain-breathe` scale/pan (presence, not noise) |
| Selection prism | CSS `clip-path` hex + copper gradient + `::before`/`::after` facet bevels + seat animation |
| Tab icons | Inline SVG per tab, accent color + glow, thick underline when active |
| Type | Rajdhani / Sora — cinematic, not Inter/Roboto |
| Scroll rail | Absolute copper thumb on the right |

## Interaction

- ← → change tabs  
- ↑ ↓ move crystal selection  
- Enter / click cycles the value on the selected row  

When Resolution is `UHD 4160x2560 [4K]`, footer shows **CFast A: 128GB** (SSD bridge path); other modes fall back to **CF A**.
