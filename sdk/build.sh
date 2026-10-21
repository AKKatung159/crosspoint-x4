#!/usr/bin/env sh
# Builds a Pocket SDK app: sdk/build.sh <app dir> [more sources]
# Compiles <app dir>/*.c with zig (https://ziglang.org, or `pip install ziglang`)
# into <app dir>/app.wasm. Copy the folder (manifest.json, app.wasm, data) to /apps/ on the card.
set -e
APP=$1
SDK=$(cd "$(dirname "$0")" && pwd)
if [ -z "$ZIG" ]; then
  if command -v zig >/dev/null 2>&1; then ZIG=zig; else ZIG="python3 -m ziglang"; fi
fi
$ZIG cc --target=wasm32-wasi -mexec-model=reactor -Os -I"$SDK/include" \
  -Wl,-z,stack-size=4096 -Wl,--initial-memory=65536 \
  -o "$APP/app.wasm" "$APP"/*.c
echo "built $APP/app.wasm ($(wc -c < "$APP/app.wasm") bytes)"
