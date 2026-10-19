#!/usr/bin/env bash
# PERSONAL: end-to-end Pocket Hub run in eink-qemu (Linux), no device needed.
#
#   scripts/emu/hub_test.sh <eink-qemu dir> <firmware.bin from x4_th_test> [workdir]
#
# TAROT_PACK=<dir from scripts/pocket-apps/make_tarot_pack.py> adds the Tarot
# pack to the card; without it the Tarot run checks the "no pack" screen.
#
# Covers: Flashcards (sample deck: index, flip, move, resume), Home -> Apps -> Counter, save on Back, reload, save on sleep, wake,
# reload, 10 hub/app enter-exit cycles (heap), and recovery from a corrupted
# newest save slot. Needs python3 and mtools (mdir/mcopy) on PATH.
# Prints PASS/FAIL per check and exits non-zero on any failure.
set -euo pipefail
REPO=${REPO:-$(cd "$(dirname "$0")/../.." && pwd)}
EQ=$(realpath "$1") FW=$(realpath "$2") WORK=${3:-$(mktemp -d)}
mkdir -p "$WORK" && cd "$WORK"
export X4MACHINE=x4
drive() { python3 "$EQ/eink-qemu/drive.py" flash.bin sd.img "$@" >/dev/null; }
fails=0
check() {  # check <name> <log> <pattern>
  if grep -q -- "$3" "$2"; then echo "PASS $1"; else echo "FAIL $1 (no '$3' in $2)"; fails=$((fails + 1)); fi
}

python3 "$EQ/eink-qemu/mkflash.py" --machine x4 "$FW" flash.bin >/dev/null
rm -rf sd.img sdroot && mkdir -p sdroot/apps/flashcards
cp "$REPO/docs/personal/pocket-apps/flashcards/english-thai.tsv" sdroot/apps/flashcards/
mkdir -p sdroot/apps/nonogram && cp "$REPO/docs/personal/pocket-apps/nonogram/puzzles.tsv" sdroot/apps/nonogram/
if [ -n "${TAROT_PACK:-}" ]; then mkdir -p sdroot/apps/tarot && cp -r "$TAROT_PACK"/. sdroot/apps/tarot/; fi
python3 "$EQ/eink-qemu/mksd.py" sd.img 2050 sdroot >/dev/null

open_hub="press:down wait:2 press:down wait:2 press:confirm wait:6"
# Hub rows: 0 Flashcards, 1 Tarot, 2 2048, 3 Minesweeper, 4 Nonogram, 5 Reading stats, 6 Counter (test builds).
downs() { for _ in $(seq "$1"); do printf 'press:down wait:2 '; done; }
open_app() { echo "$open_hub $(downs "$1")press:confirm wait:6"; }
open_counter=$(open_app 6)
open_nono=$(open_app 4)
open_mines=$(open_app 3)
open_tarot=$(open_app 1)
open_2048=$(open_app 2)
open_deck="$(open_app 0) press:confirm wait:8"
cycle="press:confirm wait:5 $(downs 6)press:confirm wait:5 press:back wait:5 press:back wait:11"
cycles=""
for _ in $(seq 10); do cycles="$cycles $cycle"; done

# Run 0: Flashcards. Index the deck, flip, move to card 4, Back saves, reopen resumes,
# then card 11 (long Thai back) for a wrapping screenshot.
drive run0.txt wait:35 $open_deck shot:flash-front.png press:confirm wait:4 shot:flash-back.png \
  press:down wait:4 press:down wait:4 press:down wait:4 press:back wait:6 press:confirm wait:8 shot:flash-resumed.png \
  press:down wait:4 press:down wait:4 press:down wait:4 press:down wait:4 press:down wait:4 press:down wait:4 press:down wait:4 press:confirm wait:4 \
  shot:flash-long-thai.png press:back wait:6
check "deck indexed" run0.txt "Indexed 15 rows"
check "deck opens at start" run0.txt "english-thai.tsv: 15 cards, at 0"
check "deck position saved" run0.txt "saved english-thai.tsv at 3: ok"
check "deck position resumed" run0.txt "english-thai.tsv: 15 cards, at 3"

# Run G: 2048. Four moves, Back saves, reopen restores the score.
drive runG.txt wait:35 $open_2048 shot:2048-start.png press:left wait:4 press:up wait:4 press:right wait:4 \
  press:down wait:4 shot:2048-moved.png press:back wait:6 press:confirm wait:6 shot:2048-reopened.png press:back wait:5
check "2048 saved" runG.txt "saved score [0-9]*: ok"

# Run M: Minesweeper. Open the centre, move, flag (hold), Back saves.
drive runM.txt wait:35 $open_mines press:confirm wait:5 shot:mines-opened.png press:right wait:3 press:right wait:3 \
  press:right wait:3 press:right wait:3 press:confirm:900 wait:5 shot:mines-flag.png press:back wait:6
check "minesweeper saved" runM.txt "saved state [12]: ok"

# Run N: Nonogram "Heart". Fill two cells, cross one, Back saves, reopen keeps the marks.
drive runN.txt wait:35 $open_nono press:confirm wait:6 shot:nono-start.png press:down wait:3 press:right wait:3 \
  press:confirm wait:3 press:right wait:3 press:confirm wait:3 press:right wait:3 press:confirm:900 wait:4 \
  shot:nono-marked.png press:back wait:6 press:confirm wait:6 shot:nono-resumed.png press:back wait:5
check "nonogram puzzle loaded" runN.txt "puzzle 0 Heart: 10x10"
check "nonogram saved" runN.txt "saved puzzle 0: ok"

# Run T: Tarot. A three-card spread: image, meaning, next card; then a daily card.
drive runT.txt wait:35 $open_tarot press:down wait:2 press:confirm wait:10 shot:tarot-1.png press:confirm wait:5 \
  shot:tarot-1-meaning.png press:right wait:10 shot:tarot-2.png press:back wait:6 press:up wait:2 press:confirm wait:10 \
  shot:tarot-daily.png press:confirm wait:5 shot:tarot-daily-meaning.png press:back wait:5
if [ -n "${TAROT_PACK:-}" ]; then
  check "tarot spread drawn" runT.txt "spread of 3 from 78 cards"
  check "tarot daily drawn" runT.txt "spread of 1 from 78 cards"
  if grep -q "Bad deck row" runT.txt; then echo "FAIL tarot deck rows"; fails=$((fails + 1)); else echo "PASS tarot deck rows"; fi
else
  check "tarot without pack" runT.txt "spread of 3 from 0 cards"
fi

# Run 1: save on Back, reload, save on sleep. Run 2 boots from the slept card.
drive run1.txt wait:35 $open_counter press:up wait:3 press:up wait:3 press:up wait:4 shot:counter3.png \
  press:back wait:6 press:confirm wait:6 shot:counter3-reopened.png press:up wait:4 \
  press:power:1500 wait:20
check "save on Back" run1.txt "saved 3: ok"
check "reload after Back" run1.txt "loaded 3"
check "save on sleep" run1.txt "saved 4: ok"

drive run2.txt wait:35 shot:home-wake.png $open_counter shot:counter4.png press:back wait:5 press:back wait:12 \
  $cycles wait:25 shot:home-after-cycles.png
check "reload after sleep" run2.txt "loaded 4"
# Free heap at the first [MEM] line of each Home visit. The first app run caches
# font glyphs once, so leaks are judged from the second visit to the last.
read -r cold warm last < <(python3 -c '
import re, sys
home, taken, vals = False, False, []
for line in open(sys.argv[1], errors="replace"):
    if "Entering activity:" in line:
        home, taken = line.rstrip().endswith("Home"), False
    m = re.search(r"\[MEM\] Free: (\d+)", line)
    if m and home and not taken:
        vals.append(int(m.group(1)))
        taken = True
print(vals[0], vals[1], vals[-1])' run2.txt)
echo "heap at Home: cold $cold, after first app run $warm, after 10 cycles $last (bytes)"
if [ $((warm - last)) -le 1024 ]; then echo "PASS no heap loss over cycles"; else echo "FAIL heap lost $((warm - last)) B over cycles"; fails=$((fails + 1)); fi

# Run 3: corrupt the newest counter slot's payload (value 4, seq 2, slot 1); expect the older slot (3).
off=$(python3 -c "import struct;print(struct.unpack('<I',open('sd.img','rb').read(512)[454:458])[0]*512)")
mcopy -o -i "sd.img@@$off" ::/.crosspoint/apps/counter.sav counter.sav
python3 - <<'EOF'
d = bytearray(open("counter.sav", "rb").read())
d[20 + 16] ^= 0xFF  # slot 1 (stride 16 + 4), first payload byte
open("counter.sav", "wb").write(d)
EOF
mcopy -o -i "sd.img@@$off" counter.sav ::/.crosspoint/apps/counter.sav
drive run3.txt wait:35 $open_counter shot:counter-recovered.png
check "torn slot falls back" run3.txt "loaded 3"

echo "screenshots and logs in $WORK"
exit $fails
