"""
generate_font_dataset.py - Génère rapidement un petit dataset de lettres A–Z
en images PNG (binarisées) pour entraîner le MLP.

Usage (depuis nn/):
    python generate_font_dataset.py --out dataset/train --size 32 --per-font 2

Par défaut, tente d'utiliser la police DejaVuSans ou une police PIL par défaut.
Installe Pillow si nécessaire: pip install pillow
"""

import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
DEFAULT_FONTS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "C:\\Windows\\Fonts\\arial.ttf",
    "C:\\Windows\\Fonts\\arialbd.ttf",
]


def load_font(size):
    for path in DEFAULT_FONTS:
        if Path(path).exists():
            try:
                return ImageFont.truetype(path, size=size)
            except Exception:
                continue
    return ImageFont.load_default()


def render_letter(letter, size, font):
    img = Image.new("L", (size, size), color=255)  # white background
    draw = ImageDraw.Draw(img)
    try:
        bbox = draw.textbbox((0, 0), letter, font=font)
        w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    except AttributeError:
        # Pillow sans textbbox/textsize: fallback approximatif
        mask = Image.new("L", (size, size))
        d2 = ImageDraw.Draw(mask)
        d2.text((0, 0), letter, font=font, fill=255)
        bbox = mask.getbbox()
        if bbox:
            w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
        else:
            w, h = size // 2, size // 2
    x = (size - w) // 2
    y = (size - h) // 2
    draw.text((x, y), letter, font=font, fill=0)  # black text
    arr = np.array(img, dtype=np.uint8)
    arr = (arr < 128).astype(np.uint8) * 255  # binarise
    return Image.fromarray(arr, mode="L")


def main():
    ap = argparse.ArgumentParser(description="Génère un dataset synthétique A–Z en PNG.")
    ap.add_argument("--out", default="dataset/train", help="Dossier de sortie racine (créé si absent).")
    ap.add_argument("--size", type=int, default=32, help="Taille (pixels) de la tuile carrée générée.")
    ap.add_argument("--per-font", type=int, default=2, help="Images par lettre et par police.")
    args = ap.parse_args()

    out_root = Path(args.out)
    out_root.mkdir(parents=True, exist_ok=True)

    font = load_font(args.size - 4)
    fonts = [font]

    for letter in LETTERS:
        (out_root / letter).mkdir(parents=True, exist_ok=True)
        idx = 0
        for f in fonts:
            for k in range(args.per_font):
                img = render_letter(letter, size=args.size, font=f)
                img_path = out_root / letter / f"{letter}_{idx}.png"
                img.save(img_path)
                idx += 1

    print(f"Dataset généré dans {out_root} (taille {args.size}x{args.size})")


if __name__ == "__main__":
    main()
