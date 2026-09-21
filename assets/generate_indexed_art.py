#!/usr/bin/env python3
"""Generate the ROM-budget indexed-color art for You Have Got Pizza.

PRG32 cartridges are capped at 64 KiB total (code + rodata + data + audio
block -- see PRG32_CART_MAX_KIB / PRG32_CART_RAM_KIB), so a full 320x200
painted background at 4 or 8 bits per pixel (16000-64000 bytes) would consume
the *entire* budget by itself. Real '90s hardware never stored full-screen
raster backgrounds for exactly this reason: it used small, cheaply repeated
tiles and sprites. This script draws that same kind of small, palette-limited
art -- character sprite sheets, a handful of repeating background tiles, and
one painted title emblem -- as exact-color PNGs ready for PRG32's own
`tools/prg32_image_convert.py`.

Two packing styles are used on purpose, to exercise both entry points PRG32
offers for compact sprite assets (see prg32_sprite_draw_indexed /
prg32_sprite_draw_bitplanes in prg32.h):

  * "indexed"   -- packed N-bit-per-pixel indices, N in {1,2,4,8}.
  * "bitplanes" -- classic Amiga/C64-style bitplanes, one 1-bit mask per
                   palette bit.

Every image is flattened onto a solid magenta (255, 0, 255) key color before
saving, and that color is always the first pixel painted, so it lands at
palette index 0 in prg32_image_convert.py's first-seen-order palette. Sprites
that need transparency pass --transparent-index 0; opaque tiles don't.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "png" / "rom"
OUT_DIR.mkdir(parents=True, exist_ok=True)

KEY = (255, 0, 255)  # transparency key; always painted first => palette index 0

# ---------------------------------------------------------------------------
# Actor palette (professor + both student shirt colors share one design
# vocabulary: outline, two skin tones, two garment tones per character, a
# highlight, and a couple of small accents).
# ---------------------------------------------------------------------------
INK = (18, 14, 22)
SKIN = (255, 209, 161)
SKIN_SH = (204, 157, 115)
HI = (255, 255, 255)
COAT = (54, 96, 214)
COAT_SH = (32, 58, 150)
COAT_TRIM = (150, 190, 255)
BLUE_SHIRT = (150, 205, 232)
BLUE_SHIRT_SH = (96, 150, 190)
MAGENTA_SHIRT = (223, 101, 186)
MAGENTA_SHIRT_SH = (165, 60, 140)
HAIR = (92, 60, 38)
SHOE_SH = (55, 35, 22)
BLUSH = (255, 150, 170)
CHALK = (255, 214, 89)

# Ingredient palette.
CRUST = (222, 178, 110)
DOUGH = (245, 208, 140)
DOUGH_HI = (255, 235, 190)
DOUGH_SH = (200, 160, 100)
SAUCE = (196, 40, 35)
SAUCE_HI = (230, 90, 70)
SAUCE_SH = (140, 20, 20)
CHEESE = (255, 214, 92)
CHEESE_HI = (255, 240, 170)
CHEESE_SH = (210, 160, 50)
BASIL = (58, 140, 70)
BASIL_HI = (120, 195, 110)
BASIL_SH = (30, 90, 45)

# Environment (stone / ladder / arch / plate) palette.
MORTAR = (35, 30, 40)
STONE_LT = (168, 168, 178)
STONE = (120, 120, 132)
STONE_DK = (78, 78, 90)
STONE_DEEP = (48, 48, 58)
WOOD_LT = (168, 120, 70)
WOOD = (122, 84, 45)
WOOD_DK = (80, 54, 28)
GOLD_LT = (200, 168, 90)
GOLD = (150, 120, 60)
GOLD_DK = (100, 80, 40)
WINDOW = (20, 18, 26)
PLATE = (238, 238, 232)
PLATE_SH = (170, 170, 165)


def flatten(img: Image.Image) -> Image.Image:
    """Composite an RGBA frame onto the magenta key color."""
    bg = Image.new("RGB", img.size, KEY)
    bg.paste(img, (0, 0), img)
    return bg


def hframes(frames: list[Image.Image]) -> Image.Image:
    w, h = frames[0].size
    sheet = Image.new("RGB", (w * len(frames), h), KEY)
    for i, frame in enumerate(frames):
        sheet.paste(frame, (i * w, 0))
    return sheet


def grid(frames_by_row: list[list[Image.Image]]) -> Image.Image:
    w, h = frames_by_row[0][0].size
    cols = len(frames_by_row[0])
    rows = len(frames_by_row)
    sheet = Image.new("RGB", (w * cols, h * rows), KEY)
    for r, row in enumerate(frames_by_row):
        for c, frame in enumerate(row):
            sheet.paste(frame, (c * w, r * h))
    return sheet


# ---------------------------------------------------------------------------
# Professor and student walk-cycle sprites (12x16, 4 horizontal frames).
# ---------------------------------------------------------------------------
def professor_frame(frame: int) -> Image.Image:
    img = Image.new("RGBA", (12, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([3, 0, 8, 4], fill=SKIN)
    d.point((4, 2), fill=INK)
    d.point((7, 2), fill=INK)
    d.rectangle([3, 0, 8, 1], fill=HAIR)
    d.rectangle([1, 4, 10, 11], fill=COAT)
    d.rectangle([1, 4, 10, 5], fill=COAT_TRIM)
    d.rectangle([5, 5, 6, 11], fill=HI)
    d.rectangle([1, 8, 2, 10], fill=COAT_SH)
    d.rectangle([9, 8, 10, 10], fill=COAT_SH)
    if frame % 2 == 0:
        d.rectangle([0, 6, 1, 10], fill=SKIN)
        d.rectangle([10, 7, 11, 11], fill=SKIN)
        d.rectangle([2, 12, 4, 15], fill=SHOE_SH)
        d.rectangle([7, 12, 9, 14], fill=SHOE_SH)
    else:
        d.rectangle([0, 7, 1, 11], fill=SKIN)
        d.rectangle([10, 6, 11, 10], fill=SKIN)
        d.rectangle([2, 12, 4, 13], fill=SHOE_SH)
        d.rectangle([7, 12, 9, 15], fill=SHOE_SH)
    if frame == 2:
        d.point((9, 1), fill=CHALK)
    if frame == 3:
        d.point((2, 1), fill=CHEESE)
    return flatten(img)


def student_frame(frame: int, shirt, shirt_sh) -> Image.Image:
    img = Image.new("RGBA", (12, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([3, 1, 8, 5], fill=SKIN)
    d.point((4, 3), fill=INK)
    d.point((7, 3), fill=INK)
    d.rectangle([4, 5, 7, 5], fill=BLUSH)
    d.rectangle([2, 0, 9, 1], fill=HAIR)
    d.rectangle([1, 6, 10, 11], fill=shirt)
    d.rectangle([1, 9, 10, 10], fill=shirt_sh)
    if frame % 2 == 0:
        d.rectangle([0, 6, 1, 8], fill=SKIN)
        d.rectangle([10, 8, 11, 10], fill=SKIN)
        d.rectangle([2, 12, 4, 15], fill=SHOE_SH)
        d.rectangle([8, 12, 10, 14], fill=SHOE_SH)
    else:
        d.rectangle([0, 8, 1, 10], fill=SKIN)
        d.rectangle([10, 6, 11, 8], fill=SKIN)
        d.rectangle([2, 12, 4, 14], fill=SHOE_SH)
        d.rectangle([8, 12, 10, 15], fill=SHOE_SH)
    return flatten(img)


# ---------------------------------------------------------------------------
# Ingredient icons (28x12, 4 kinds x 4 animation frames, row = kind).
# ---------------------------------------------------------------------------
def ingredient_frame(kind: int, frame: int) -> Image.Image:
    img = Image.new("RGBA", (28, 12), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    base, hi, sh = [
        (DOUGH, DOUGH_HI, DOUGH_SH),
        (SAUCE, SAUCE_HI, SAUCE_SH),
        (CHEESE, CHEESE_HI, CHEESE_SH),
        (BASIL, BASIL_HI, BASIL_SH),
    ][kind]
    d.rounded_rectangle([1, 2, 26, 9], radius=3, fill=base, outline=INK)
    d.rounded_rectangle([2, 3, 25, 5], radius=2, fill=hi)
    d.rounded_rectangle([2, 7, 25, 8], radius=1, fill=sh)
    bob = frame % 2
    if kind == 0:
        d.ellipse([5, 4 - bob, 9, 7 - bob], fill=CRUST, outline=INK)
        d.ellipse([18, 4 + bob, 22, 7 + bob], fill=CRUST, outline=INK)
    elif kind == 1:
        for i, x in enumerate([5, 11, 17, 22]):
            yy = 3 + ((frame + i) % 3)
            d.line([x, yy, x + 2, yy + 3], fill=SAUCE_SH, width=1)
    elif kind == 2:
        for i, x in enumerate([6, 14, 21]):
            yy = 3 + ((frame + i) % 2)
            d.rectangle([x, yy, x + 2, yy + 4], fill=CHEESE_HI)
    else:
        for i, x in enumerate([5, 12, 19]):
            yy = 3 + ((frame + i) % 2)
            d.ellipse([x, yy, x + 5, yy + 4], fill=BASIL_HI, outline=BASIL_SH)
    return flatten(img)


# ---------------------------------------------------------------------------
# Environment tiles: skyline arch, stone platform brick, ladder rung, plate.
# ---------------------------------------------------------------------------
def arch_tile() -> Image.Image:
    img = Image.new("RGB", (20, 26), GOLD)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 19, 25], fill=GOLD)
    d.rectangle([0, 0, 19, 2], fill=GOLD_LT)
    d.rectangle([0, 22, 19, 25], fill=GOLD_DK)
    d.rounded_rectangle([4, 4, 15, 20], radius=4, fill=WINDOW)
    d.line([2, 2, 2, 24], fill=GOLD_LT, width=1)
    d.line([17, 2, 17, 24], fill=GOLD_DK, width=1)
    return img


def stone_tile() -> Image.Image:
    img = Image.new("RGB", (16, 10), STONE)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 15, 1], fill=STONE_LT)
    d.rectangle([0, 8, 15, 9], fill=STONE_DEEP)
    d.line([0, 4, 6, 4], fill=STONE_DK)
    d.line([7, 4, 15, 4], fill=STONE_DK)
    d.line([6, 0, 6, 4], fill=STONE_DK)
    d.line([11, 4, 11, 9], fill=STONE_DK)
    return img


def ladder_tile() -> Image.Image:
    img = Image.new("RGB", (14, 12), KEY)
    d = ImageDraw.Draw(img)
    d.rectangle([1, 0, 3, 11], fill=WOOD)
    d.rectangle([1, 0, 2, 11], fill=WOOD_LT)
    d.rectangle([10, 0, 12, 11], fill=WOOD)
    d.rectangle([11, 0, 12, 11], fill=WOOD_DK)
    d.rectangle([1, 4, 12, 6], fill=WOOD_LT)
    d.rectangle([1, 5, 12, 6], fill=WOOD_DK)
    return img


def plate_tile() -> Image.Image:
    img = Image.new("RGBA", (80, 24), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([0, 4, 79, 23], fill=PLATE, outline=PLATE_SH)
    d.ellipse([6, 6, 73, 19], fill=PLATE_SH)
    d.ellipse([9, 7, 70, 18], fill=PLATE)
    d.ellipse([16, 2, 64, 20], fill=DOUGH, outline=CRUST)
    d.ellipse([20, 4, 60, 18], fill=SAUCE)
    d.ellipse([23, 6, 57, 16], fill=CHEESE)
    for cx, cy in [(30, 9), (40, 12), (50, 8), (35, 14), (46, 15)]:
        d.ellipse([cx - 3, cy - 2, cx + 3, cy + 2], fill=BASIL)
    return flatten(img)


def title_emblem() -> Image.Image:
    """A small painted pizza emblem, deliberately using >16 colors so the
    conversion step below packs it at 8 bits per pixel instead of 4 -- this
    is the one asset in the set that exercises PRG32's 256-color path."""
    img = Image.new("RGBA", (56, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy, r = 28, 24, 22
    rings = [
        (r, (222, 178, 110)), (r - 2, DOUGH), (r - 5, (250, 222, 160)),
        (r - 8, SAUCE_SH), (r - 10, SAUCE), (r - 12, SAUCE_HI),
        (r - 14, CHEESE_SH), (r - 16, CHEESE), (r - 17, CHEESE_HI),
    ]
    for radius, color in rings:
        d.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=color)
    d.pieslice([cx - r, cy - r, cx + r, cy + r], 260, 320, fill=(30, 22, 14))
    for ang, dist, size, color in [
        (20, 12, 3, SAUCE), (70, 10, 3, SAUCE), (140, 13, 3, SAUCE),
        (200, 9, 3, SAUCE), (250, 11, 3, SAUCE),
        (40, 14, 3, BASIL), (110, 8, 3, BASIL_HI), (170, 11, 3, BASIL),
        (300, 13, 3, BASIL_SH),
    ]:
        px = cx + dist * math.cos(math.radians(ang))
        py = cy + dist * math.sin(math.radians(ang)) * 0.9
        d.ellipse([px - size, py - size, px + size, py + size], fill=color,
                  outline=(30, 60, 25) if color != SAUCE else (100, 10, 10))
    d.arc([cx - r, cy - r, cx + r, cy + r], 0, 360, fill=INK, width=2)
    return flatten(img)


def life_icon() -> Image.Image:
    img = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.pieslice([0, 0, 7, 7], 20, 340, fill=DOUGH, outline=INK)
    d.pieslice([1, 1, 6, 6], 20, 340, fill=SAUCE)
    d.point((3, 2), fill=CHEESE)
    d.point((4, 4), fill=CHEESE)
    return flatten(img)


def save(img: Image.Image, name: str) -> Path:
    path = OUT_DIR / name
    img.save(path)
    return path


def main() -> None:
    save(hframes([professor_frame(i) for i in range(4)]),
         "rom_professor_4frames_12x16.png")
    save(hframes([student_frame(i, BLUE_SHIRT, BLUE_SHIRT_SH) for i in range(4)]),
         "rom_student_blue_4frames_12x16.png")
    save(hframes([student_frame(i, MAGENTA_SHIRT, MAGENTA_SHIRT_SH) for i in range(4)]),
         "rom_student_magenta_4frames_12x16.png")
    save(grid([[ingredient_frame(kind, frame) for frame in range(4)] for kind in range(4)]),
         "rom_ingredients_4x4_28x12.png")
    save(arch_tile(), "rom_tile_arch_20x26.png")
    save(stone_tile(), "rom_tile_stone_16x10.png")
    save(ladder_tile(), "rom_tile_ladder_14x12.png")
    save(plate_tile(), "rom_tile_plate_80x24.png")
    save(title_emblem(), "rom_title_emblem_56x48.png")
    save(life_icon(), "rom_life_icon_8x8.png")
    print(f"wrote ROM art to {OUT_DIR}")


if __name__ == "__main__":
    main()
