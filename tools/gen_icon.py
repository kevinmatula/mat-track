#!/usr/bin/env python3
"""Generates the app icon into assets/: a tiny 4x4 heatmap in the app's
grays on black.

    python3 tools/gen_icon.py

icon.svg - for the desktop menu (installed as mat-track.svg)
icon.bmp - 64x64, compiled in as the window / taskbar icon
"""
import struct
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"
SIZE, CELL, GAP, MARGIN = 64, 12, 3, 4
LEVELS = ["#111111", "#3A3A3A", "#6E6E6E", "#B0B0B0", "#FFFFFF"]  # as in the heatmap
GRID = [  # a streak that builds up
    [0, 1, 0, 2],
    [1, 2, 3, 1],
    [2, 3, 4, 3],
    [3, 4, 4, 4],
]


def cells():
    for r, row in enumerate(GRID):
        for c, level in enumerate(row):
            yield MARGIN + c * (CELL + GAP), MARGIN + r * (CELL + GAP), LEVELS[level]


def write_svg():
    rects = "".join(
        f'<rect x="{x}" y="{y}" width="{CELL}" height="{CELL}" fill="{color}"/>'
        for x, y, color in cells())
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {SIZE} {SIZE}">'
           f'<rect width="{SIZE}" height="{SIZE}" fill="#000000"/>{rects}</svg>\n')
    (ASSETS / "icon.svg").write_text(svg)


def write_bmp():
    px = [[(0, 0, 0)] * SIZE for _ in range(SIZE)]
    for x0, y0, color in cells():
        rgb = tuple(int(color[i:i + 2], 16) for i in (1, 3, 5))
        for y in range(y0, y0 + CELL):
            for x in range(x0, x0 + CELL):
                px[y][x] = rgb
    # 24-bit BMP, bottom-up rows, BGR, rows already 4-byte aligned (64*3).
    rows = b"".join(bytes(v for (r, g, b) in reversed_row for v in (b, g, r))
                    for reversed_row in reversed(px))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(rows), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, SIZE, SIZE, 1, 24, 0, len(rows), 2835, 2835, 0, 0)
    (ASSETS / "icon.bmp").write_bytes(header + info + rows)


if __name__ == "__main__":
    write_svg()
    write_bmp()
    print("wrote assets/icon.svg and assets/icon.bmp")
