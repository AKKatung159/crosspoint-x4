#!/usr/bin/env python3
"""PERSONAL: build the SD-card Tarot pack for the Pocket Hub.

  python3 scripts/pocket-apps/make_tarot_pack.py <scans dir> <out dir>

<scans dir> holds one image per card named like deck.tsv's file column
(00_Fool.jpg, Cups_03.jpg, ...), e.g. the Rider-Waite-Smith 1909 scans from
Wikimedia Commons (public domain). <out dir> receives deck.tsv and
cards/<file>.bmp: 1-bit, 300x520, dithered. Copy <out dir> to /apps/tarot/.
Needs Pillow.
"""
import shutil
import sys
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps

SIZE = (300, 520)  # fits the portrait screen above the name line
DECK = Path(__file__).resolve().parents[2] / "docs/personal/pocket-apps/tarot/deck.tsv"


def convert(src: Path, dst: Path) -> None:
    img = ImageOps.exif_transpose(Image.open(src)).convert("L")
    img = ImageOps.fit(ImageOps.autocontrast(img, cutoff=1), SIZE, Image.LANCZOS)
    img = ImageEnhance.Contrast(img).enhance(1.2)
    img.convert("1").save(dst)  # Floyd-Steinberg; 1 bpp BMP, palette black/white


def main() -> int:
    scans, out = Path(sys.argv[1]), Path(sys.argv[2])
    (out / "cards").mkdir(parents=True, exist_ok=True)
    shutil.copy(DECK, out / "deck.tsv")
    missing = []
    for line in DECK.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        name = line.split("\t", 1)[0]
        src = next((p for p in scans.glob(name + ".*")), None)
        if src is None:
            missing.append(name)
            continue
        convert(src, out / "cards" / (name + ".bmp"))
    print(f"wrote {out}; missing scans: {missing or 'none'}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
