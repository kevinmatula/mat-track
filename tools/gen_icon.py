#!/usr/bin/env python3
"""Generates the app icon into assets/: a sharp, angular dark-gray "m" on
white, drawn from straight strokes with mitered (pointed) joins.

    python3 tools/gen_icon.py [preview.png]

icon.svg - for the desktop menu (installed as mat-track.svg)
icon.bmp - 64x64, compiled in as the window / taskbar icon
An optional argument also writes a large preview PNG.
"""
import math
import struct
import sys
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"
SIZE = 64
BACKGROUND = "#FFFFFF"
INK = "#333333"
STROKE = 8.0

# The "m": up the left leg, over two pointed peaks, down the right leg.
OUTLINE = [(14, 54), (14, 21), (23, 12), (32, 21), (41, 12), (50, 21), (50, 54)]
# The middle leg hangs from the valley between the peaks.
MIDDLE_LEG = (32 - STROKE / 2, 21, 32 + STROKE / 2, 54)


def stroke_polygon(points, width):
    """Outline of a thick polyline with butt ends and miter joins."""
    half = width / 2

    def offset(p, q, side):
        dx, dy = q[0] - p[0], q[1] - p[1]
        n = math.hypot(dx, dy)
        nx, ny = -dy / n * side * half, dx / n * side * half
        return (p[0] + nx, p[1] + ny), (q[0] + nx, q[1] + ny)

    def intersect(a1, a2, b1, b2):
        d = (a2[0] - a1[0]) * (b2[1] - b1[1]) - (a2[1] - a1[1]) * (b2[0] - b1[0])
        t = ((b1[0] - a1[0]) * (b2[1] - b1[1]) - (b1[1] - a1[1]) * (b2[0] - b1[0])) / d
        return a1[0] + t * (a2[0] - a1[0]), a1[1] + t * (a2[1] - a1[1])

    sides = []
    for side in (1, -1):
        segs = [offset(points[i], points[i + 1], side) for i in range(len(points) - 1)]
        pts = [segs[0][0]]
        for a, b in zip(segs, segs[1:]):
            pts.append(intersect(*a, *b))
        pts.append(segs[-1][1])
        sides.append(pts)
    return sides[0] + sides[1][::-1]


SHAPE = stroke_polygon(OUTLINE, STROKE)


def write_svg():
    pts = " ".join(f"{x:.2f},{y:.2f}" for x, y in SHAPE)
    x0, y0, x1, y1 = MIDDLE_LEG
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {SIZE} {SIZE}">'
           f'<rect width="{SIZE}" height="{SIZE}" fill="{BACKGROUND}"/>'
           f'<polygon points="{pts}" fill="{INK}"/>'
           f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" fill="{INK}"/></svg>\n')
    (ASSETS / "icon.svg").write_text(svg)


def render(size, supersample=8):
    """Anti-aliased raster of the icon, via PIL if available."""
    from PIL import Image, ImageDraw
    big = size * supersample
    k = big / SIZE
    img = Image.new("RGB", (big, big), BACKGROUND)
    draw = ImageDraw.Draw(img)
    draw.polygon([(x * k, y * k) for x, y in SHAPE], fill=INK)
    x0, y0, x1, y1 = MIDDLE_LEG
    draw.rectangle([x0 * k, y0 * k, x1 * k - 1, y1 * k - 1], fill=INK)
    return img.resize((size, size), Image.LANCZOS)


def write_bmp():
    img = render(SIZE)
    # 24-bit BMP, bottom-up rows, BGR; 64*3 bytes per row is already 4-aligned.
    rows = b"".join(
        bytes(v for x in range(SIZE) for v in reversed(img.getpixel((x, y))))
        for y in reversed(range(SIZE)))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(rows), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, SIZE, SIZE, 1, 24, 0, len(rows), 2835, 2835, 0, 0)
    (ASSETS / "icon.bmp").write_bytes(header + info + rows)


if __name__ == "__main__":
    write_svg()
    write_bmp()
    print("wrote assets/icon.svg and assets/icon.bmp")
    if len(sys.argv) > 1:
        render(256).save(sys.argv[1])
        print(f"wrote preview {sys.argv[1]}")
