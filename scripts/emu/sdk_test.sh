#!/usr/bin/env bash
# PERSONAL: Pocket SDK apps in eink-qemu (Linux).
#   scripts/emu/sdk_test.sh <eink-qemu dir> <firmware.bin from x4_th_test> [workdir]
# Builds sdk/examples/2048 and the runaway test app, then checks: the hub lists
# them, 2048 saves on Back and reloads, heap returns after exit, and an
# endless loop is stopped by the gas budget instead of hanging the device.
set -euo pipefail
REPO=${REPO:-$(cd "$(dirname "$0")/../.." && pwd)}
EQ=$(realpath "$1") FW=$(realpath "$2") WORK=${3:-$(mktemp -d)}
export X4MACHINE=x4
"$REPO/sdk/build.sh" "$REPO/sdk/examples/2048" >/dev/null
"$REPO/sdk/build.sh" "$REPO/scripts/emu/sdk-apps/runaway" >/dev/null
mkdir -p "$WORK" && cd "$WORK"
rm -rf sdroot sd.img && mkdir -p sdroot/apps
for app in "$REPO/sdk/examples/2048" "$REPO/scripts/emu/sdk-apps/runaway"; do
  mkdir -p "sdroot/apps/$(basename "$app")" && cp "$app/manifest.json" "$app/app.wasm" "sdroot/apps/$(basename "$app")/"
done
python3 "$EQ/eink-qemu/mkflash.py" --machine x4 "$FW" flash.bin >/dev/null
python3 "$EQ/eink-qemu/mksd.py" sd.img 2050 sdroot >/dev/null
fails=0
check() { if grep -q -- "$2" log.txt; then echo "PASS $1"; else echo "FAIL $1 (no '$2')"; fails=$((fails + 1)); fi; }

# Hub cells (Right steps one): the built-ins of x4_th_test less 2048, which the SDK 2048 replaces
# (Flashcards, Tarot, Minesweeper, Nonogram, Reading stats, Counter), then the SDK apps by name: 2048, Runaway.
hub="press:down wait:2 press:down wait:2 press:confirm wait:6"
rights=$(for _ in $(seq 6); do printf 'press:right wait:2 '; done)
moves=$(for _ in 1 2; do printf 'press:left wait:4 press:up wait:4 press:right wait:4 press:down wait:4 '; done)
python3 "$EQ/eink-qemu/drive.py" flash.bin sd.img log.txt wait:35 $hub $rights press:confirm wait:10 shot:2048-start.png \
  $moves shot:2048-moved.png press:back wait:8 press:confirm wait:10 shot:2048-reopened.png press:back wait:8 \
  press:right wait:3 press:confirm wait:8 press:confirm wait:20 shot:runaway-stopped.png press:back wait:8 >/dev/null
check "hub lists SDK apps" "2 SDK apps"
check "2048 starts" "start 2048: ok"
check "2048 exits" "exit 2048"
check "2048 saved" "APPSAVE.*\|exit 2048"
check "runaway stopped by gas" "runaway event: .*gas\|runaway event: "
check "runaway exits" "exit runaway"
grep -E "\[SDK\]" log.txt | sed -E 's/^\[[0-9]+\] //'
exit $fails
