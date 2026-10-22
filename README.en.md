# CrossPoint X4 Thai

[ภาษาไทย](README.md) · **English**

Personal firmware for the **Xteink X4** (and **X4 Pro**), built on
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
- **Clock, reading stats and a dashboard.** The time in the status bar, reading stats in the Apps grid,
  and a dashboard sleep screen (HTML layouts you can edit, from `server/`).
- **Gentler on the SD card and battery.** The reading position is saved every
  5 pages, and XTC pages stream straight from the card.
- **Apps (Pocket Hub).** The home "Apps" tile opens a grid of apps: 2048, Minesweeper, Nonogram, Tarot
  (with the Rider-Waite-Smith 1909 cards), Flashcards, Reading stats and Dashboard. Apps are WebAssembly
  on the card, installed without reflashing: get them from
  [x4-pocket-apps](https://github.com/AKKatung159/x4-pocket-apps) or write your own with the Pocket SDK
  ([guide](docs/personal/pocket-apps/README.md)).

<p align="center">
  <img src="docs/showcase/apps-hub.png" alt="The Apps grid, three icons to a row" width="23%">
  <img src="docs/showcase/apps-tarot.png" alt="Tarot: the Two of Cups" width="23%">
  <img src="docs/showcase/apps-minesweeper.png" alt="Minesweeper" width="23%">
  <img src="docs/showcase/apps-flashcards.png" alt="An English-Thai flashcard" width="23%">
</p>

## Install

Download the latest file from [Releases](https://github.com/AKKatung159/crosspoint-x4/releases):
`crosspoint-…-x3-x4.bin` for the X4, or `crosspoint-…-x4pro.bin` for the X4 Pro. Flash it with the
[CrossPoint web flasher](https://crosspointreader.com/#flash-tools) (pick your device, then
**Custom .bin**). Then pick **Settings → System → Language → ไทย**. (The X4 Pro build is tested in the
emulator only so far.)
Other ways to install, and how to go back: [install.en.md](docs/personal/install.en.md).

## What's next

Phases 1–6 and the X4 Pro build are done; see [what each phase changed](docs/personal/changelog.en.md). Possible next steps: waking
up on a timer so the dashboard stays fresh, and having epub.lan write `thai-words.txt` from each series' glossary.

## More

- [What each phase changed](docs/personal/changelog.en.md), word-breaker comparison and known limits
- [Installing and rolling back](docs/personal/install.en.md)
- [Building, tests, testing on a real X4, emulator, releasing](docs/personal/development.md)
- [The dashboard server](server/README.md)
- [How the Thai code works](lib/Thai/README.md)
- [Licences](docs/personal/licences.md) (all MIT, CC0 or OFL)
- [Upstream CrossPoint README](docs/upstream-README.md)
