# Licences

Back to the README: [ไทย](../../README.md) · [English](../../README.en.md).

| Source | Licence | Used for |
| --- | --- | --- |
| [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) 2.0.0rc | MIT | the base firmware |
| [JTIAPBNAI/CrosspointTH](https://github.com/JTIAPBNAI/CrosspointTH) v1.6.0-th.1.0 | MIT | Thai shaping rules and tests, the SarabunTH PUA mapping (fonts themselves: OFL, see Sarabun below), PUA patch script, Thai UI strings, dictionary trie format |
| [kocha01/crosspoint-halo2-custom](https://github.com/kocha01/crosspoint-halo2-custom) v2.2.15 | MIT | ideas only: the DP word-break objective and syllable-aware fallback (reimplemented) |
| [PyThaiNLP](https://github.com/PyThaiNLP/pythainlp) `words_th.txt` | CC0-1.0 | the Thai dictionary |
| [Sarabun](https://github.com/cadsondemak/Sarabun) | SIL OFL 1.1 | Thai glyphs in the built-in fonts; committed as `SarabunTH-*.ttf`, modified (PUA cmap entries added by `scripts/thai/thai_pua_patch.py`) (`lib/EpdFont/builtinFonts/source/SarabunTH/OFL.txt`) |

`crosspoint-thai-wordcut` is binary-only and is not used. CrosspointTH's
dictionary (libthai, LGPL-2.1) is not used either.
