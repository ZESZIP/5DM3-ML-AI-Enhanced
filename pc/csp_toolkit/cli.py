"""CLI: simulate / info / decode Cinema Stream Pack."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .container import CspReader
from .modes import MODES, select_mode
from .prores import export_prores, export_png_sequence
from .simulate import simulate_clip


def cmd_simulate(args: argparse.Namespace) -> int:
    result = simulate_clip(args.mode, args.seconds, args.out)
    print(json.dumps(result, indent=2))
    if args.mode in MODES and MODES[args.mode].target_mibs <= 100 and not result["under_100"]:
        # Soft warning for pathological noise; still exit 0 if file written
        print("warning: average bitrate exceeded 100 MiB/s on this synthetic content", file=sys.stderr)
    return 0


def cmd_info(args: argparse.Namespace) -> int:
    r = CspReader(Path(args.input))
    r.open()
    info = dict(r.header)
    info["frames"] = len(r.frames)
    if r.frames:
        avg = sum(len(f.payload) for f in r.frames) / len(r.frames)
        fps = info["fps_num"] / max(1, info["fps_den"])
        info["avg_frame_bytes"] = int(avg)
        info["avg_mib_s"] = avg * fps / (1024 * 1024)
    print(json.dumps(info, indent=2))
    return 0


def cmd_decode(args: argparse.Namespace) -> int:
    if args.prores:
        path = export_prores(args.input, args.prores, max_frames=args.max_frames)
        print(f"wrote {path}")
    if args.png_dir:
        paths = export_png_sequence(args.input, args.png_dir, max_frames=args.max_frames)
        print(f"wrote {len(paths)} pngs to {args.png_dir}")
    if not args.prores and not args.png_dir:
        print("specify --prores and/or --png-dir", file=sys.stderr)
        return 2
    return 0


def cmd_select(args: argparse.Namespace) -> int:
    m = select_mode(args.measured_mibs, bridge_present=args.bridge)
    print(json.dumps({"mode_id": m.mode_id, "ui_label": m.ui_label, "target_mibs": m.target_mibs}, indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="csp_toolkit", description="Cinema OS CSP toolkit")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("simulate", help="Write a synthetic .csp clip")
    s.add_argument("--mode", default="CROP1080P100", choices=sorted(MODES.keys()))
    s.add_argument("--seconds", type=float, default=1.0)
    s.add_argument("--out", required=True)
    s.set_defaults(func=cmd_simulate)

    s = sub.add_parser("info", help="Inspect a .csp file")
    s.add_argument("--in", dest="input", required=True)
    s.set_defaults(func=cmd_info)

    s = sub.add_parser("decode", help="Decode to ProRes Log and/or PNG")
    s.add_argument("--in", dest="input", required=True)
    s.add_argument("--prores")
    s.add_argument("--png-dir")
    s.add_argument("--max-frames", type=int, default=None)
    s.set_defaults(func=cmd_decode)

    s = sub.add_parser("select-mode", help="Pick mode for measured card speed")
    s.add_argument("--measured-mibs", type=float, required=True)
    s.add_argument("--bridge", action="store_true")
    s.set_defaults(func=cmd_select)

    return p


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
