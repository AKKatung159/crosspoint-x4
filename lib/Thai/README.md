# lib/Thai: Thai reading support

Self-contained Thai support for this fork of CrossPoint Reader. Everything Thai
lives here; the rest of the firmware calls in through a few small, tagged hooks
(listed below) so upstream merges stay easy.

| File | What it does |
| --- | --- |
| `ThaiBrk.h/.cpp` | Line-break opportunities: dictionary maximal matching + Thai cluster rules |
| `ThaiDictData.h` | Generated word DAWG in flash (`scripts/thai/gen_thai_dict.py`) |
| `ThaiShaping.h/.cpp` | Stacked vowel/tone glyph selection (WTT 2.0 rules, C90 PUA glyphs) |
| `ThaiJustify.h` | How justified lines share spare width between spaces and Thai word gaps |
| `data/thai_words_extra.txt` | This fork's extra dictionary words (loanwords, fantasy/wuxia terms) |

## How it works

**Word breaking.** Thai has no spaces between words. For each run of Thai text
`ThaiBrk::wordBreakByteOffsets` runs a dynamic program over every dictionary
match and picks the split with the fewest unknown codepoints, then the fewest
segments, then the most even word lengths. It never breaks inside a grapheme
cluster, and Thai Character Cluster rules keep fixed syllable shapes
(`เ-ีย`, `เ-ือ`, `-ัว`, `เ-อ`, `-ั-`, `เ-็-`, silent finals with `์`) whole.
Unknown words, typically transliterated names, get extra break points only at
likely syllable starts. A stranded final consonant is merged back into the
previous segment. Work is done in windows of at most 384 codepoints, about
6 KB of temporary heap.

**Dictionary.** PyThaiNLP `words_th.txt` (CC0) plus `data/thai_words_extra.txt`.
One-letter entries are dropped, and so are entries of 12+ codepoints that are
made up entirely of other entries (mostly phrases such as `ธนาคารแห่งประเทศไทย`).
The result is 53,994 words in a 374 KB DAWG with shared suffixes. It lives in
flash and uses no RAM. (Up to th.3 the limit was 8 codepoints, which also
dropped everyday words such as `ประโยชน์`.)

**Shaping.** `ThaiShaping` swaps in the C90 Private Use Area variant glyphs:
the high tone tier above an upper vowel, left-shifted marks over `ป ฝ ฟ ฬ`, the
descender-less `ญ ฐ`, a lowered `ุ ู` under `ฎ ฏ`, and `ำ` split into `ํ`+`า`.
Shaping runs inside GfxRenderer's draw/measure funnel, and only for fonts that
contain the PUA glyphs. Section caches therefore keep plain Unicode, and an SD
font without the variants still draws unshaped Thai.

**Justification.** A Thai word gap (a break with no visible space) gets at most
1 px of stretch. Real spaces take the rest, up to 10 px each, and anything left
over is shared evenly by the Thai gaps. Nothing is ever added inside a word.

**Fonts.** Every built-in font (Noto Sans/Serif 12–18 in four styles, Ubuntu
UI 10/12, the 8 pt status font) also contains Thai and the C90 variants from
SarabunTH (Sarabun, OFL, with PUA variants encoded by
`scripts/thai/thai_pua_patch.py`). Noto keeps priority for every glyph it has.

## Core hooks

Every change outside `lib/Thai` is tagged `THAI-HOOK(<name>)` or
`PERSONAL-HOOK` (`git grep -n "THAI-HOOK\|PERSONAL-HOOK"`):

| Tag | File | Change |
| --- | --- | --- |
| `words` | `lib/Epub/Epub/ParsedText.cpp` `addWord()` | Thai words are split at `ThaiBrk` offsets into zero-width-gap tokens (same mechanism as CJK); no focus-reading split for Thai |
| `glue` | `ParsedText.cpp` `addWord()` | Thai fragments split by inline markup (`<b>`, `<span>`) keep a break opportunity |
| `justify` | `ParsedText.cpp` `extractLine()` | Counts Thai word gaps; `ThaiJustify::distribute` splits the spare width; the LTR and RTL gap loops use the per-gap value |
| `shape` | `lib/GfxRenderer/GfxRenderer.cpp` | `resolveVisualText()` shapes Thai after bidi for C90-capable fonts; called from `drawText`, `getTextWidth`, `getTextAdvanceX` |
| `tone` | `GfxRenderer.cpp` `renderCharImpl()` | Seats C90 high-tier tone glyphs exactly 1 px above the font's tallest of `ี ึ ื` (`ThaiShaping::highToneAdjustPx`). CrosspointTH's fixed ascender/8 lowering pushed them 2–5 px *into* the vowel in every built-in font; found on a real X4. |
| — | `lib/Epub/Epub/Section.cpp` | `SECTION_FILE_VERSION` 48 → 49, so books laid out by older firmware are laid out again |
| `txt` | `src/activities/reader/TxtReaderActivity.cpp` | TXT wrapping prefers the nearest Thai word break and never cuts a cluster; `CACHE_VERSION` 3 → 4 |
| `fonts` | `lib/EpdFont/scripts/convert-builtin-fonts.sh` | Adds SarabunTH and the Thai/PUA intervals to every built-in font; headers in `builtinFonts/` are regenerated |
| `thai-words` | `src/ThaiUserDictionary.*`, `ReaderActivity.cpp`, `EpubReaderActivity.cpp`, `TxtReaderActivity.cpp` | Loads personal word lists when a book opens (`ThaiBrk::setUserWords`) and drops a book's cached layout when the list's hash changes |
| `thai-fallback` | `lib/EpdFont/EpdFontFamily.*`, `SdCardFontManager.cpp`, `main.cpp`, `GfxRenderer.cpp` | SD fonts borrow Thai/C90 glyphs from the built-in Noto Sans family of the nearest size (`resolveGlyph`, coverage checked in RAM) |
| `progress` (PERSONAL) | `ReaderActivity.*`, EPUB/TXT/XTC readers | Reading position saved every 5 renders and on exit/sleep |
| `rotate` (PERSONAL) | `lib/GfxRenderer/GfxRenderer.*` `drawBitmap1Bit()` | Optional `rotate180` draws a 1-bit BMP upside down in the same box (Pocket SDK `x4_image2`, reversed tarot cards) |
| `xtc` (PERSONAL) | `XtcReaderActivity.cpp` | 1-bit XTC pages streamed into the frame buffer |
| `test-input` (PERSONAL) | `lib/hal/HalGPIO.*`, `main.cpp` | Test builds only: `CMD:PRESS`, `CMD:MEM` over USB |
| `themes` (PERSONAL) | `CrossPointSettings.h`, `SettingsList.h`, `UITheme.cpp`, i18n | Two more `UI_THEME` values (Cover 5, Minimal 6) built by `src/components/themes/personal/CoverTheme.*` |
| `home-summary` (PERSONAL) | `ReaderActivity.*`, EPUB/TXT/XTC readers, `src/HomeSummary.*` | On exit a book writes percent and chapter to `<cache>/home-summary.txt` for the home screen |
| `grey-covers` (PERSONAL) | `lib/Epub/Epub.*`, `BaseTheme.h`, `HomeActivity.cpp` | `generateThumbBmp(height, grey)` writes a 2-bit `thumbg_<h>.bmp`; themes that ask for it get one grey pass over the home cover after the B/W refresh |
| `lyra-apps` (PERSONAL) | `src/components/themes/lyra/LyraTheme.cpp` | The Apps tile's `Stats` icon draws `src/components/icons/apps.h` (lucide layout-grid), so the default theme shows it |
| `ble-port` (PERSONAL) | `lib/BleKeyboardHost/`, `platformio.ini` `[ble_page_turner]`, `EpubReaderActivity.cpp` `remoteShortcut()`, `BleSettingsActivity.cpp`, reader menu | Upstream's Bluetooth page turner (#3733, #3784, #3785, #4074, cherry-picked with `-x`) on this fork's base: `BleKeyboardHost` is vendored from SDK `084fdfb` (see its `VENDORED.md`); no Save Clipping action (no clippings here); Bluetooth shows On/Off in the reader menus like Night mode; host tests include the vendored header |
| `pdf-xtc` (PERSONAL) | `src/network/html/personal/pdfxtc.js`, `FilesPage.html`, `CrossPointWebServer.*` (`/js/pdfxtc.js`) | PDF -> XTC in the browser before upload (port of Epub-Translate's pdfslice/xtc/xtccover); pdf.js from the CDN, saved gzipped to `/pdfjs/` on the card the first time for offline use |
| `home-touch` (PERSONAL) | `BaseTheme.h`, `CoverTheme.*`, `HomeActivity.cpp`, `CoverGridHomeUi.*` | Themes with their own home layout hit-test taps themselves; Cover Grid's tab icons follow the fork's menu order (Apps) |
| `home-fit` (PERSONAL) | `BaseTheme.*`, `LyraTheme.*`, `RoundedRaffTheme.*`, `HomeActivity.*` | One row layout for the home menu (`homeMenuRows`), tightened to fit six rows; drawing and the tap grid share it |
| `touch-buttons` (PERSONAL) | `Activity.*`, `ActivityManager.cpp`, `MappedInputManager.*`, `UITheme.*`, theme `drawButtonHints`, app activity headers | On touch boards, screens that ask keep the hint bar: slot taps/holds act as the front buttons, swipes as the arrows |
| `sans-only` (PERSONAL) | `main.cpp`, `CrossPointSettings.cpp`, `TextSettingsActivity.cpp` | `PERSONAL_SANS_ONLY`: Noto Serif ids map to Noto Sans, the Serif choice is hidden |
| `PERSONAL` | `platformio.ini` | `extra_configs` also loads `platformio.personal.ini` (this fork's `x4_th` and `x4pro_th` envs) |
| `PERSONAL` | `src/network/OtaUpdater.cpp` | Release URL is overridable (`CROSSPOINT_RELEASES_API`), so OTA never swaps this build for stock upstream |

Other additions outside `lib/Thai`: `lib/I18n/translations/thai.yaml` (picked up
automatically by `scripts/gen_i18n.py`), `test/thai/` plus one
`add_subdirectory` line in `test/CMakeLists.txt`, and `scripts/thai/`.

### After an upstream merge

1. Resolve conflicts at the hook sites above. If upstream changed
   `SECTION_FILE_VERSION`, take upstream's value + 1.
2. If upstream changed a font source or `fontconvert.py`, regenerate:
   `bash lib/EpdFont/scripts/convert-builtin-fonts.sh`.
3. If upstream added UI strings, translate them in `thai.yaml` (missing keys fall
   back to English, with a build warning).
4. Run the host tests (`test/thai` and the rest) and `pio run -e x4_th`.

## Known limits

- SD fonts without Thai borrow it from the built-in fonts, but keep their own
  line height, which can be tight for stacked Thai marks.
- Rotated text (`drawTextRotated90CW`) and scaled sup/sub glyphs are not shaped.
- Dictionary breaking can still split unknown names unusually; add them to a
  `thai-words.txt` (see the top-level README).
- Justified lines with no real space may end a few pixels short: Thai word gaps
  stretch at most `kMaxThaiShareGapPx` (3 px) rather than open holes.

## Credits and licences

- Shaping rules and test cases, SarabunTH fonts, the high-tone lowering and the
  dictionary trie format come from
  [JTIAPBNAI/CrosspointTH](https://github.com/JTIAPBNAI/CrosspointTH) (MIT).
- The DP objective (fewest unknowns, then fewest segments) and syllable-aware
  fallback follow
  [kocha01/crosspoint-halo2-custom](https://github.com/kocha01/crosspoint-halo2-custom)
  (MIT), reimplemented here on a flash DAWG with bounded memory.
- Word list: [PyThaiNLP](https://github.com/PyThaiNLP/pythainlp) `words_th.txt`,
  CC0-1.0.
- Sarabun: Cadson Demak / The Sarabun Project, SIL OFL 1.1
  (`lib/EpdFont/builtinFonts/source/SarabunTH/OFL.txt`).
