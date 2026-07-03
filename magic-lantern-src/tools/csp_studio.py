#!/usr/bin/env python3
"""CSP Studio — desktop decoder for Canon 5D3 Cinema OS .CSP streams.

Polished UI for decompressing in-camera CSP/CIX recordings to MLV or ProRes.
Uses the same codec routines as tools/cinepack_to_mlv.py.
"""
from __future__ import annotations

import os
import sys
import threading
import tempfile
import queue
import struct
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Reuse codec from sibling script
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

import cinepack_to_mlv as csp  # noqa: E402

# --- Theme (glass / low-poly cinema palette) ---
BG = "#0d1117"
PANEL = "#161b22"
ACCENT = "#ff6600"
ACCENT2 = "#00a2ff"
TEXT = "#f0f6fc"
MUTED = "#8b949e"
SUCCESS = "#4cd964"
WARN = "#ffcc00"
FONT_UI = ("Segoe UI", 10)
FONT_TITLE = ("Segoe UI Semibold", 14)
FONT_MONO = ("Consolas", 9)


class CSPStudio(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("CSP Studio — Cinema OS Decoder")
        self.geometry("880x620")
        self.minsize(760, 520)
        self.configure(bg=BG)

        self.input_path = tk.StringVar()
        self.output_path = tk.StringVar()
        self.format_var = tk.StringVar(value="MLV")
        self.status_var = tk.StringVar(value="Drop a .CSP / .CIX / .MLV file or browse to begin.")
        self.progress_var = tk.DoubleVar(value=0.0)

        self._log_q: queue.Queue[str] = queue.Queue()
        self._worker: threading.Thread | None = None

        self._build_ui()
        self.after(100, self._drain_log)

    def _build_ui(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TFrame", background=BG)
        style.configure("Card.TFrame", background=PANEL)
        style.configure("TLabel", background=BG, foreground=TEXT, font=FONT_UI)
        style.configure("Card.TLabel", background=PANEL, foreground=TEXT, font=FONT_UI)
        style.configure("Muted.TLabel", background=PANEL, foreground=MUTED, font=FONT_UI)
        style.configure("Title.TLabel", background=BG, foreground=TEXT, font=FONT_TITLE)
        style.configure("Accent.TButton", font=FONT_UI, padding=8)
        style.map("Accent.TButton", background=[("active", ACCENT)])

        header = ttk.Frame(self, style="TFrame")
        header.pack(fill=tk.X, padx=24, pady=(20, 8))
        ttk.Label(header, text="CSP Studio", style="Title.TLabel").pack(side=tk.LEFT)
        ttk.Label(
            header,
            text="Cinema OS · CSP codec · PC decompress",
            style="Muted.TLabel",
        ).pack(side=tk.LEFT, padx=(12, 0))

        card = ttk.Frame(self, style="Card.TFrame", padding=20)
        card.pack(fill=tk.X, padx=24, pady=8)

        self._row_file(card, "Source", self.input_path, self._browse_in)
        self._row_file(card, "Export", self.output_path, self._browse_out)

        fmt_row = ttk.Frame(card, style="Card.TFrame")
        fmt_row.pack(fill=tk.X, pady=(12, 0))
        ttk.Label(fmt_row, text="Output format", style="Card.TLabel").pack(side=tk.LEFT)
        for label in ("MLV", "ProRes MOV"):
            ttk.Radiobutton(
                fmt_row,
                text=label,
                value="ProRes MOV" if "ProRes" in label else "MLV",
                variable=self.format_var,
                command=self._on_format_change,
            ).pack(side=tk.LEFT, padx=(16, 0))

        btn_row = ttk.Frame(card, style="Card.TFrame")
        btn_row.pack(fill=tk.X, pady=(16, 0))
        self.decode_btn = tk.Button(
            btn_row,
            text="Decompress CSP → Export",
            bg=ACCENT,
            fg="#000",
            activebackground="#ff8533",
            activeforeground="#000",
            relief=tk.FLAT,
            padx=16,
            pady=8,
            font=FONT_UI,
            command=self._start_decode,
        )
        self.decode_btn.pack(side=tk.LEFT)

        info = ttk.Label(
            card,
            text="Targets: 6K25 ~90 MB/s · 4K ~85 MB/s · 1080p120 ~100 MB/s",
            style="Muted.TLabel",
        )
        info.pack(anchor=tk.W, pady=(12, 0))

        prog_frame = ttk.Frame(self, style="TFrame")
        prog_frame.pack(fill=tk.X, padx=24, pady=4)
        self.prog = ttk.Progressbar(prog_frame, variable=self.progress_var, maximum=100)
        self.prog.pack(fill=tk.X)
        ttk.Label(prog_frame, textvariable=self.status_var, style="Muted.TLabel").pack(anchor=tk.W, pady=(6, 0))

        log_card = ttk.Frame(self, style="Card.TFrame", padding=12)
        log_card.pack(fill=tk.BOTH, expand=True, padx=24, pady=(8, 20))
        ttk.Label(log_card, text="Log", style="Card.TLabel").pack(anchor=tk.W)
        self.log = tk.Text(
            log_card,
            height=12,
            bg="#010409",
            fg=TEXT,
            insertbackground=TEXT,
            font=FONT_MONO,
            relief=tk.FLAT,
            wrap=tk.WORD,
        )
        self.log.pack(fill=tk.BOTH, expand=True, pady=(8, 0))

    def _row_file(self, parent, label, var, cmd):
        row = ttk.Frame(parent, style="Card.TFrame")
        row.pack(fill=tk.X, pady=4)
        ttk.Label(row, text=label, width=8, style="Card.TLabel").pack(side=tk.LEFT)
        entry = tk.Entry(row, textvariable=var, bg="#0d1117", fg=TEXT, insertbackground=TEXT, relief=tk.FLAT)
        entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(8, 8), ipady=6)
        tk.Button(row, text="Browse…", command=cmd, bg=PANEL, fg=TEXT, relief=tk.FLAT, padx=10).pack(side=tk.RIGHT)

    def _browse_in(self):
        p = filedialog.askopenfilename(
            title="Open CSP / CIX / MLV",
            filetypes=[
                ("Cinema streams", "*.CSP *.CIX *.csp *.cix"),
                ("MLV", "*.MLV *.mlv"),
                ("All", "*.*"),
            ],
        )
        if p:
            self.input_path.set(p)
            base, _ = os.path.splitext(p)
            ext = ".MOV" if self.format_var.get() == "ProRes MOV" else ".MLV"
            self.output_path.set(base + "_decoded" + ext)
            self._probe(p)

    def _browse_out(self):
        fmt = self.format_var.get()
        if fmt == "ProRes MOV":
            p = filedialog.asksaveasfilename(defaultextension=".mov", filetypes=[("QuickTime", "*.mov")])
        else:
            p = filedialog.asksaveasfilename(defaultextension=".mlv", filetypes=[("MLV", "*.mlv")])
        if p:
            self.output_path.set(p)

    def _on_format_change(self):
        inp = self.input_path.get()
        if inp:
            base, _ = os.path.splitext(inp)
            ext = ".MOV" if self.format_var.get() == "ProRes MOV" else ".MLV"
            self.output_path.set(base + "_decoded" + ext)

    def _log(self, msg: str):
        self._log_q.put(msg)

    def _drain_log(self):
        while True:
            try:
                line = self._log_q.get_nowait()
            except queue.Empty:
                break
            self.log.insert(tk.END, line + "\n")
            self.log.see(tk.END)
        self.after(100, self._drain_log)

    def _probe(self, path: str):
        try:
            with open(path, "rb") as f:
                head = f.read(128)
            if len(head) < 8:
                return
            magic = struct.unpack_from("<I", head, 0)[0]
            if magic in (csp.CSP_MAGIC, csp.CIX_MAGIC):
                w, h = struct.unpack_from("<HH", head, 8)
                fps = struct.unpack_from("<I", head, 12)[0] if len(head) >= 16 else 0
                target = head[18] if len(head) > 18 else 0
                kind = "CSP" if magic == csp.CSP_MAGIC else "CIX"
                self.status_var.set(f"{kind} {w}×{h} @ {fps/1000:.3f} fps · target ~{target} MB/s")
            elif magic == csp.CINEPACK_MAGIC:
                w, h = struct.unpack_from("<II", head, 8)
                self.status_var.set(f"CINEPACK frame {w}×{h}")
            else:
                self.status_var.set("MLV or other — will scan for CSP payloads")
        except OSError as e:
            self.status_var.set(str(e))

    def _set_busy(self, busy: bool):
        self.decode_btn.configure(state=tk.DISABLED if busy else tk.NORMAL)
        if busy:
            self.progress_var.set(5)
        else:
            self.progress_var.set(0)

    def _start_decode(self):
        if self._worker and self._worker.is_alive():
            return
        inp = self.input_path.get().strip()
        out = self.output_path.get().strip()
        if not inp or not os.path.isfile(inp):
            messagebox.showerror("CSP Studio", "Select a valid source file.")
            return
        if not out:
            messagebox.showerror("CSP Studio", "Select an export path.")
            return
        self.log.delete("1.0", tk.END)
        self._set_busy(True)
        self._worker = threading.Thread(target=self._decode_worker, args=(inp, out), daemon=True)
        self._worker.start()

    def _decode_worker(self, inp: str, out: str):
        try:
            self._log(f"Reading {inp}")
            with open(inp, "rb") as f:
                data = f.read()
            magic = struct.unpack_from("<I", data, 0)[0]
            prores = self.format_var.get() == "ProRes MOV"
            self.progress_var.set(20)

            if magic == csp.CSP_MAGIC:
                self._log("CSP stream detected")
                if prores:
                    with tempfile.NamedTemporaryFile(suffix=".MLV", delete=False) as tmp:
                        tmp_path = tmp.name
                    csp.stream_to_mlv(data, tmp_path, csp.CSP_MAGIC, csp.CSP_FRAME_MAGIC, "CSP")
                    self.progress_var.set(60)
                    hdr = struct.unpack_from("<IHHI", data, 0, 12)
                    fps = struct.unpack_from("<I", data, 12)[0]
                    w, h = hdr[2], hdr[3]
                    raw_path = tmp_path + ".raw"
                    with open(tmp_path, "rb") as mlv:
                        mlv_data = mlv.read()
                    pos = 0
                    raw_all = bytearray()
                    while pos + 8 < len(mlv_data):
                        bt, bs = struct.unpack_from("<II", mlv_data, pos)
                        if bt == csp.MLV_BLOCK_VIDF and bs > 64:
                            raw_all += mlv_data[pos + 64 : pos + bs]
                        if bt == csp.MLV_BLOCK_EOF:
                            break
                        pos += bs
                    with open(raw_path, "wb") as rf:
                        rf.write(raw_all)
                    self.progress_var.set(80)
                    csp.mlv_to_prores(raw_path, out, w, h, fps)
                    os.unlink(tmp_path)
                    os.unlink(raw_path)
                else:
                    csp.stream_to_mlv(data, out, csp.CSP_MAGIC, csp.CSP_FRAME_MAGIC, "CSP")
            elif magic == csp.CIX_MAGIC:
                self._log("CIX stream detected")
                csp.stream_to_mlv(data, out, csp.CIX_MAGIC, csp.CIX_FRAME_MAGIC, "CIX")
            elif magic == csp.CINEPACK_MAGIC:
                pixels, w, h, q = csp.decode_cinepack(data)
                with open(out, "wb") as f:
                    f.write(csp.pixels_to_raw_bytes(pixels))
                self._log(f"Standalone CNPK {w}×{h} q={q}")
            else:
                self._log("Scanning MLV for embedded CSP…")
                csp.rewrite_mlv(data, out)

            self.progress_var.set(100)
            self._log(f"Done → {out}")
            self.after(0, lambda: self.status_var.set(f"Exported {os.path.basename(out)}"))
            self.after(0, lambda: messagebox.showinfo("CSP Studio", f"Export complete:\n{out}"))
        except Exception as e:
            self._log(f"ERROR: {e}")
            self.after(0, lambda: messagebox.showerror("CSP Studio", str(e)))
        finally:
            self.after(0, lambda: self._set_busy(False))


def main():
    app = CSPStudio()
    app.mainloop()


if __name__ == "__main__":
    main()
