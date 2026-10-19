#!/usr/bin/env bash
# PERSONAL: end-to-end Pocket Hub run in eink-qemu (Linux), no device needed.
#
#   scripts/emu/hub_test.sh <eink-qemu dir> <firmware.bin from x4_th_test> [workdir]
#
# Covers: Home -> Apps -> Counter, save on Back, reload, save on sleep, wake,
# reload, 10 hub/app enter-exit cycles (heap), and recovery from a corrupted
# newest save slot. Needs python3 and mtools (mdir/mcopy) on PATH.
# Prints PASS/FAIL per check and exits non-zero on any failure.
set -euo pipefail
EQ=$(realpath "$1") FW=$(realpath "$2") WORK=${3:-$(mktemp -d)}
mkdir -p "$WORK" && cd "$WORK"
export X4MACHINE=x4
drive() { python3 "$EQ/eink-qemu/drive.py" flash.bin sd.img "$@" >/dev/null; }
fails=0
check() {  # check <name> <log> <pattern>
  if grep -q -- "$3" "$2"; then echo "PASS $1"; else echo "FAIL $1 (no '$3' in $2)"; fails=$((fails + 1)); fi
}

python3 "$EQ/eink-qemu/mkflash.py" --machine x4 "$FW" flash.bin >/dev/null
rm -f sd.img

open_counter="press:down wait:2 press:down wait:2 press:confirm wait:6 press:down wait:2 press:confirm wait:6"
cycle="press:confirm wait:5 press:down wait:2 press:confirm wait:5 press:back wait:5 press:back wait:11"
cycles=""
for _ in $(seq 10); do cycles="$cycles $cycle"; done

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

# Run 3: corrupt the newest slot's payload (value 4, seq 2, slot 1); expect the older slot (3).
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
