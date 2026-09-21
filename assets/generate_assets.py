#!/usr/bin/env python3
"""Generate the store-facing reference PNGs and WAV masters for You Have Got Pizza.

This script covers the big, full-resolution art (background, splash, the UI
icon reference sheet) and the WAV sound masters -- copyright-clean documentation
and Store-listing assets. None of it is compiled into the cartridge: PRG32
caps a cartridge at 64 KiB total, so a 320x200 raster background alone would
consume the whole budget. The actual in-game sprite/tile art is small,
palette-limited, and generated separately by assets/generate_indexed_art.py
(see assets/png/rom/ and c/assets_indexed.inc); this script's job is the
larger concept art and the packaged icon/screenshot.
"""
from __future__ import annotations

import json
import math
import struct
import wave
from pathlib import Path
from typing import Iterable, Tuple

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
PNG_DIR = ROOT / "png"
WAV_DIR = ROOT / "wav"
PNG_DIR.mkdir(parents=True, exist_ok=True)
WAV_DIR.mkdir(parents=True, exist_ok=True)

# RGB palette chosen to be close to the RGB565 colours used in the cartridge.
SKY = (132, 207, 255)
STONE = (132, 132, 132)
STONE_DK = (64, 64, 64)
PIAZZA = (198, 198, 192)
DOUGH = (250, 218, 148)
SAUCE = (210, 20, 22)
CHEESE = (255, 232, 84)
BASIL = (42, 142, 68)
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
BLUE = (40, 82, 205)
MAGENTA = (210, 75, 180)
SKIN = (255, 210, 170)
SHADOW = (35, 35, 35)
WOOD = (120, 80, 45)


def save(img: Image.Image, name: str) -> None:
    img.save(PNG_DIR / name)


def draw_background() -> Image.Image:
    img = Image.new("RGB", (320, 200), SKY)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 24, 319, 49], fill=(96, 96, 108))
    for x in range(16, 320, 40):
        d.rectangle([x, 8, x + 17, 49], fill=(132, 132, 96))
        d.rectangle([x + 5, 18, x + 12, 34], fill=BLACK)
    for y in [48, 88, 128, 168]:
        d.rectangle([8, y, 311, y + 4], fill=STONE_DK)
        d.rectangle([8, y + 5, 311, y + 9], fill=STONE)
        for x in range(12, 304, 16):
            d.line([x, y + 6, x, y + 8], fill=SHADOW)
    for x in [32, 104, 188, 272]:
        d.rectangle([x - 6, 48, x - 4, 176], fill=STONE_DK)
        d.rectangle([x + 5, 48, x + 7, 176], fill=STONE_DK)
        for y in range(54, 177, 12):
            d.rectangle([x - 6, y, x + 7, y + 2], fill=STONE)
    d.ellipse([218, 181, 294, 193], fill=WHITE, outline=STONE_DK)
    d.ellipse([230, 177, 282, 190], fill=DOUGH, outline=(180, 130, 70))
    return img


def draw_splash() -> Image.Image:
    img = draw_background()
    d = ImageDraw.Draw(img)
    d.rectangle([22, 30, 298, 152], fill=(15, 22, 28), outline=CHEESE, width=2)
    d.text((48, 48), "YOU HAVE GOT", fill=WHITE)
    d.text((68, 68), "PIZZA", fill=CHEESE)
    # Big symbolic pizza teaching wheel.
    d.ellipse([112, 86, 208, 182], fill=DOUGH, outline=(165, 105, 45), width=3)
    d.pieslice([112, 86, 208, 182], 290, 360, fill=BLACK)
    for cx, cy, col in [(138, 112, SAUCE), (171, 116, SAUCE), (154, 148, BASIL), (188, 150, SAUCE), (132, 160, CHEESE)]:
        d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=col)
    d.text((46, 164), "Low-level programming can be tasty", fill=WHITE)
    return img


def make_ui_icons() -> None:
    img = Image.new("RGBA", (80, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i in range(3):
        x = i * 16 + 2
        d.polygon([(x+7,2),(x+13,7),(x+10,14),(x+4,14),(x+1,7)], fill=CHEESE, outline=SHADOW)
    d.text((52, 4), "x", fill=WHITE)
    d.rectangle([64, 3, 76, 12], fill=BLUE, outline=WHITE)
    save(img, "ui_lives_score_icons.png")


def write_wav(name: str, notes: Iterable[Tuple[float, float]], rate: int = 22050) -> None:
    """Write notes as (frequency Hz, duration seconds). freq=0 means rest."""
    data = bytearray()
    phase = 0.0
    for freq, dur in notes:
        samples = int(rate * dur)
        for n in range(samples):
            if freq <= 0:
                val = 0
            else:
                # Rounded square/sine hybrid: retro, but less harsh than a pure square.
                t = n / rate
                amp = 0.35 * math.sin(2 * math.pi * freq * t + phase)
                amp += 0.12 * math.sin(2 * math.pi * freq * 2 * t + phase)
                # tiny envelope to avoid clicks
                env = min(1.0, n / max(1, int(rate * 0.01)), (samples - n) / max(1, int(rate * 0.02)))
                val = int(max(-1, min(1, amp * env)) * 32767)
            data += struct.pack("<h", val)
        if freq > 0:
            phase += 2 * math.pi * freq * dur
    with wave.open(str(WAV_DIR / name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(data)


def main() -> None:
    save(draw_background(), "background_piazza_320x200.png")
    save(draw_splash(), "splash_you_have_got_pizza_320x200.png")
    make_ui_icons()

    write_wav("sfx_start_jingle.wav", [(523, .08), (659, .08), (784, .10), (1047, .18)])
    write_wav("sfx_climb_up.wav", [(330, .06), (392, .06)])
    write_wav("sfx_climb_down.wav", [(294, .06), (220, .06)])
    write_wav("sfx_collect_dough.wav", [(660, .10)])
    write_wav("sfx_collect_sauce.wav", [(715, .10)])
    write_wav("sfx_collect_cheese.wav", [(770, .10)])
    write_wav("sfx_collect_basil.wav", [(825, .10)])
    write_wav("sfx_pizza_ready.wav", [(784, .08), (988, .08), (1175, .08), (1568, .20)])
    write_wav("sfx_student_collision.wav", [(160, .08), (130, .08), (95, .14)])
    write_wav("sfx_game_over.wav", [(392, .12), (330, .12), (262, .20), (0, .05), (196, .22)])

    manifest = {
        "title": "You Have Got Pizza asset manifest",
        "license": "MIT / original project assets; no third-party copyrighted art or sound",
        "png": sorted(p.name for p in PNG_DIR.glob("*.png")),
        "wav": sorted(p.name for p in WAV_DIR.glob("*.wav")),
        "note": (
            "These are full-resolution reference/Store assets, not the in-game "
            "art. The compiled sprite and tile art (palette-limited, small "
            "enough to fit PRG32's 64 KiB cartridge budget) lives under "
            "png/rom/ and is generated by generate_indexed_art.py; see that "
            "file and c/assets_indexed.inc for the actual gameplay pixel data."
        ),
    }
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
