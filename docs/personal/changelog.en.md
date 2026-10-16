# What each phase changed

[ภาษาไทย](changelog.md) · **English**

Details behind the [README](../../README.en.md). The design of the Thai code and every change to upstream files: [lib/Thai/README.md](../../lib/Thai/README.md).

## Phase 1 (1.6.5-th.1): Thai reading

- **Thai word breaking.** Thai has no spaces between words. Lines break between
  dictionary words (37,883 words in th.1, 53,994 since th.4; CC0) and never inside a syllable. Unknown
  names wrap only at likely syllable starts. This covers EPUB and TXT.
- **Stacked vowels and tone marks.** Marks such as `ที่ ปั้ม น้ำ ฟื้น กตัญญู` are
  positioned using the C90 variant-glyph rules (ported from CrosspointTH).
- **Justified Thai without holes.** The gaps between Thai words stretch by at most
  1 px; real spaces take the rest. Nothing is ever stretched inside a word.
- **Thai in every built-in font.** Noto Sans/Serif 12–18 in all styles, the UI
  fonts and the status-bar font, using Sarabun's Thai glyphs.
- **Thai UI language.** *Settings → System → Language → ไทย* (all 515 strings).
- **OTA safety.** "Check for update" looks at this repo's releases, so it can
  never replace this build with stock CrossPoint (which has no Thai).

## Phase 2 (1.6.5-th.2): speed and battery

- **Personal word lists.** Put names and series terms one per line in a
  `thai-words.txt` (UTF-8, `#` for comments) at the card root (all books), in a
  book's folder (one series; epub.lan uses one folder per series), or as
  `<book name>.words.txt` next to the book. They count as dictionary words, so
  `ซาร์ทาเนียลโวลเดอร์ไมเออร์` stays whole instead of breaking at odd places.
  When a list changes, the affected book is laid out again automatically.
- **Thai in any SD font.** SD fonts without Thai (Latin-only `.cpfont`s)
  borrow Thai, including the stacked-mark variants, from the built-in font of
  the nearest size. Lines can look tight with fonts that have a small line
  height; raise line spacing if so.
- **Fewer SD writes.** The reading position is saved every 5 pages and when
  you leave the book or the device sleeps, not on every page turn. A crash or
  dead battery can lose up to 4 pages of position.
- **XTC without big allocations.** Black-and-white XTC pages are drawn
  straight from the card (no 48 KB buffer per page). Grayscale pages fall back
  to black and white if the 96 KB buffer is unavailable, instead of showing an
  error.
- **Fix:** tone marks above `ิ ี ึ ื` (e.g. `ที่`, `เปลี่ยน`) collided with the
  vowel in 1.6.5-th.1.

All of this was tested end to end on a real X4 (see [development.md](development.md#testing-on-a-real-x4-over-usb)).

## Phase 3 (1.6.5-th.3): looks and covers

<p align="center">
  <img src="../showcase/home-cover.png" alt="Cover home theme on the X4" width="30%">
  <img src="../showcase/home-minimal.png" alt="Minimal home theme on the X4" width="30%">
</p>

- **Two home themes** (*Settings → Display → UI Theme*, ตั้งค่า → หน้าจอ → ธีมหน้าจอ), Lyra everywhere except
  the home screen:
  - **ปกเต็ม (Cover):** a big cover, the book title, author · current chapter,
    a progress bar with "อ่านแล้ว N%", dots for the last 5 books, and icon
    tiles with short labels.
  - **มินิมอล (ไอคอน) (Minimal):** a bigger cover, the progress bar and icons
    only, with no text at all (the battery and the button hints are icons too).
  - Both use one solid icon set (`scripts/personal/home_icons.py`). Up/Down
    walk the books first, then the tiles. The percent and chapter are saved in
    the book's cache (`home-summary.txt`) when you leave it, so books opened
    only on older firmware show no summary until you read them once.
- **Noto Sans only.** Noto Serif is no longer built in (it saves about 1 MB of
  flash). A saved Serif setting reads as Sans, and SD fonts still work.
  Flash is now 5.04 MB (77%), with about 1.5 MB free.
- **Sharper covers from epub.lan.** Books sent to the X4 (Send to X4 and OPDS)
  have every picture rewritten as a baseline greyscale JPEG at 480×800 or
  smaller. The X4 decodes those quickly and cleanly; progressive or large JPEGs
  used to look blocky (the decoder only reads 1/8 of a progressive JPEG).

Tested on a real X4: both themes, the home summary after reading, and Thai
reading with Sans only.

## 1.6.5-th.4: sharp covers, tidier Thai lines

- **Grey home covers.** The Cover and Minimal themes draw the home cover in 4
  grey levels instead of black-and-white dots: a 2-bit thumbnail
  (`thumbg_<height>.bmp`) plus one grey refresh of just the cover. XTC books
  still use their 1-bit first page.
- **Whole covers.** Covers wider than 0.6 : 1 are shown whole, centred, instead
  of having their sides cut off.
- **Covers come back after a cache wipe.** Home used to need a book's cache to
  build its thumbnail, and otherwise marked the book "no cover" until it was
  opened again. It now reads the cover straight from the EPUB.
- **Fixing books already on the card.** `scripts/personal/x4_fix_covers.py`
  applies epub.lan's picture rewrite to every EPUB on the X4 over Wi-Fi (File
  Transfer mode). It backs each changed book up on the PC first and keeps the
  reading position and home summary (the X4 deletes a book's cache together
  with the book).
- **No more holes in Thai lines.** A Thai word gap now stretches by at most
  3 px; whatever is left stays at the end of the line. Before, a line with no
  real space spread all its slack over the word gaps, which could open 6–8 px
  holes.
- **Bigger dictionary (53,994 words).** The generator used to drop every entry
  of 8+ codepoints that splits into shorter entries, which removed everyday
  words such as `ประโยชน์` (then cut as `ประ|โยชน์` at a line end). The limit
  is now 12. Against deepcut, precision goes from 93.5% to 95.2% and recall
  from 88% to 82% (fewer, safer break points), for 130 KB more flash.
  Books are laid out again once (`SECTION_FILE_VERSION` 50, TXT cache 5).


## Word breaker: why this one

I compared three breakers on 3,000 paragraphs from two Thai books translated by
epub.lan. The reference is PyThaiNLP's `deepcut` (a neural segmenter, independent
of any dictionary). Tool: `scripts/thai/eval_segmentation.py`.

| Breaker | Precision (breaks that are real word boundaries) | Recall (word boundaries offered) |
| --- | --- | --- |
| **This fork** (DP + cluster rules, CC0 dictionary), th.4 | **95.2%** | 82.2% |
| This fork, th.1–th.3 (smaller dictionary) | 93.5% | 87.9% |
| Halo 2 v2.2.15 (DP, ORST dictionary) | 91.5% | 94.4% |
| CrosspointTH v1.6.0-th.1.0 (greedy longest match) | 92.2% | 95.3% |

For line breaking, precision matters most: a false break is a word cut in half
at the end of a line. This fork makes the fewest. It keeps some compounds
(`กินข้าว`) whole, which costs recall. Greedy matching also fails on classic
traps, for example `ไปหามเหสี` → `ไป|หาม|เห|สี`.

## Known limits

- Justified Thai lines with no real space can end a few pixels short of the
  margin (Thai word gaps stretch at most 3 px, `kMaxThaiShareGapPx` in
  `lib/Thai/ThaiJustify.h`).
- The Thai Back/Home button label `« หน้าหลัก` wraps onto two lines in the button bar.
- Peak heap during layout of long Thai chapters is about 6 KB lower than stock, because Thai text is split into finer tokens (same-pages emulator run: 30.9 KB vs 36.6 KB minimum free).
