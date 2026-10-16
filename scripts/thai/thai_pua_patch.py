#!/usr/bin/env python3
# From JTIAPBNAI/CrosspointTH v1.6.0-th.1.0 (MIT), scripts/thai_pua_patch.py.
"""Encode Thai C90 PUA variant glyphs (U+F700-F71A) into a font's cmap.

Thai fonts ship contextual variant glyphs (lowered tone marks, left-shifted
upper vowels for ascender consonants, descender-less YO YING / THO THAN) as
unencoded glyphs reachable only through OpenType GSUB. The firmware's bitmap
font pipeline rasterizes by codepoint, so this script maps those variants to
the de-facto C90 PUA codepoints first. Run it on each style, then convert
with fontconvert_sdcard.py including the (0xF700-0xF71A) interval.

C90 PUA layout:
  F700       THO THAN without descender        (base.descless)
  F701-F704  SARA I/II/UE/UEE shifted left     (upper.left)
  F705-F709  tone marks + thanthakhat low+left (top.lowleft)
  F70A-F70E  tone marks + thanthakhat lowered  (top.low)
  F70F       YO YING without descender         (base.descless)
  F710-F712  MAI HAN-AKAT, NIKHAHIT, MAITAIKHU shifted left (upper.left)
  F713-F717  tone marks + thanthakhat shifted left (top.left)
  F718-F71A  SARA U/UU/PHINTHU lowered         (lower.low)
"""
import sys
from fontTools.ttLib import TTFont

# Candidate glyph names per PUA codepoint, first match wins. Covers the
# Cadson Demak naming used by Sarabun and the AAT-era uniXXXX.variant names.
PUA_CANDIDATES = {
    0xF700: ["thoThanthai.less", "uni0E10.descless", "uni0E10.less"],
    0xF701: ["uni0E34.narrow", "uni0E34.left"],
    0xF702: ["uni0E35.narrow", "uni0E35.left"],
    0xF703: ["uni0E36.narrow", "uni0E36.left"],
    0xF704: ["uni0E37.narrow", "uni0E37.left"],
    # lowleft: prefer a dedicated variant, fall back to the lowered form
    0xF705: ["uni0E48.lowleft", "uni0E48.small"],
    0xF706: ["uni0E49.lowleft", "uni0E49.small"],
    0xF707: ["uni0E4A.lowleft", "uni0E4A.small"],
    0xF708: ["uni0E4B.lowleft", "uni0E4B.small"],
    0xF709: ["uni0E4C.lowleft", "uni0E4C.small"],
    0xF70A: ["uni0E48.low", "uni0E48.small"],
    0xF70B: ["uni0E49.low", "uni0E49.small"],
    0xF70C: ["uni0E4A.low", "uni0E4A.small"],
    0xF70D: ["uni0E4B.low", "uni0E4B.small"],
    0xF70E: ["uni0E4C.low", "uni0E4C.small"],
    0xF70F: ["yoYingthai.less", "uni0E0D.descless", "uni0E0D.less"],
    0xF710: ["uni0E31.narrow", "uni0E31.left"],
    0xF711: ["uni0E4D.narrow", "uni0E4D.left"],
    0xF712: ["uni0E47.narrow", "uni0E47.left"],
    0xF713: ["uni0E48.narrow", "uni0E48.left"],
    0xF714: ["uni0E49.narrow", "uni0E49.left"],
    0xF715: ["uni0E4A.narrow", "uni0E4A.left"],
    0xF716: ["uni0E4B.narrow", "uni0E4B.left"],
    0xF717: ["uni0E4C.narrow", "uni0E4C.left"],
    0xF718: ["uni0E38.small", "uni0E38.low"],
    0xF719: ["uni0E39.small", "uni0E39.low"],
    0xF71A: ["uni0E3A.small", "uni0E3A.low"],
}


def patch(in_path, out_path):
    font = TTFont(in_path)
    glyph_names = set(font.getGlyphOrder())
    added, missing = 0, []
    for table in font["cmap"].tables:
        if not table.isUnicode():
            continue
        for cp, candidates in PUA_CANDIDATES.items():
            name = next((n for n in candidates if n in glyph_names), None)
            if name is None:
                missing.append(cp)
                continue
            if cp not in table.cmap:
                table.cmap[cp] = name
                added += 1
    font.save(out_path)
    print(f"{in_path}: +{added} cmap entries -> {out_path}")
    for cp in sorted(set(missing)):
        print(f"  warning: no variant glyph for U+{cp:04X}", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} in.ttf out.ttf")
    patch(sys.argv[1], sys.argv[2])
