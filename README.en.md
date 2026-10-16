# CrossPoint X4 Thai

[ภาษาไทย](README.md) · **English**

Personal firmware for the **Xteink X4**, built on
[CrossPoint Reader 1.6.5](https://github.com/crosspoint-reader/crosspoint-reader/tree/1.6.5):
proper Thai reading and a home screen built around the book cover.

<p align="center">
  <img src="docs/showcase/home-cover.png" alt="Cover home theme with Alice's Adventures in Wonderland" width="30%">
  <img src="docs/showcase/read-thai.png" alt="A Thai page of Alice in Wonderland" width="30%">
  <img src="docs/showcase/home-minimal.png" alt="Minimal home theme, icons only" width="30%">
</p>

<sub>Screenshots from a real X4. The book is *Alice's Adventures in Wonderland* (Project Gutenberg, public domain) and its Thai translation made with epub.lan.</sub>

## Features

- **Thai that reads right.** Lines break between words, never inside one.
  Tone marks and stacked vowels sit in the right place, and justified lines
  have no big holes. The menus are in Thai too.
- **Your own words.** Put character and place names in `thai-words.txt` and
  they never get split.
- **Thai in any font.** SD-card fonts without Thai borrow it from the built-in
  font.
- **Two home screens.**
  - **ปกเต็ม (Cover):** a big cover, the chapter and how far you are.
  - **มินิมอล (Minimal):** the cover and icons, no text.
- **Sharp grey covers.** Covers are shown in 4 grey levels. Books sent from
  epub.lan (Send to X4, or OPDS at `http://epub.lan/opds`) come with pictures
  the X4 decodes cleanly.
- **Gentler on the SD card and battery.** The reading position is saved every
  5 pages, and XTC pages stream straight from the card.

## Install

Download the latest `crosspoint-…-x3-x4.bin` from
[Releases](https://github.com/AKKatung159/crosspoint-x4/releases) and flash it
with the [CrossPoint web flasher](https://crosspointreader.com/#flash-tools)
(**X4 → Custom .bin**). Then pick **Settings → System → Language → ไทย**.
Other ways to install, and how to go back: [install.en.md](docs/personal/install.en.md).

## What's next

| Phase | Plan |
| --- | --- |
| 4 | A status bar with widgets (clock, battery, chapter progress), and a homelab dashboard image as the sleep screen. |
| 5 | Reading stats (time, pages, finished books) synced to the homelab, and updates straight from this repo. |

Phases 1–3 are done; see [what each phase changed](docs/personal/changelog.en.md).

## More

- [What each phase changed](docs/personal/changelog.en.md), word-breaker comparison and known limits
- [Installing and rolling back](docs/personal/install.en.md)
- [Building, tests, testing on a real X4, emulator](docs/personal/development.md)
- [How the Thai code works](lib/Thai/README.md)
- [Licences](docs/personal/licences.md) (all MIT, CC0 or OFL)
- [Upstream CrossPoint README](docs/upstream-README.md)
