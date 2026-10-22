# Building and testing

How to build this fork on Windows or macOS, run the tests, drive a real X4 over USB, and use the emulator.
Back to the README: [ไทย](../../README.md) · [English](../../README.en.md).

## Build (Windows notes)

```bash
pip install https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip
pio run -e x4_th     # X3/X4  -> .pio/build/x4_th/firmware.bin
pio run -e x4pro_th  # X4 Pro -> .pio/build/x4pro_th/firmware.bin
pio run -e x4c_th    # X4 Classic -> .pio/build/x4c_th/firmware.bin
```

- Run it from **PowerShell or cmd**, not Git Bash: ESP-IDF's installer refuses
  MSYS ("MSys/Mingw is not supported").
- Set `PLATFORMIO_CORE_DIR=C:\pio`. A deep core directory exceeds Windows'
  260-character path limit while unpacking the ESP32 libraries.
- Set `PYTHONUTF8=1` on Thai-locale Windows (code page cp874). Otherwise build
  output containing non-Thai characters crashes PlatformIO's logger.
- Use PlatformIO **6.1.19** (pioarduino) in both your venv and `C:\pio\penv`.
  6.2.x installs SCons 4.11 in the middle of the build, and the link step fails
  with `No module named 'SCons.Tool.FortranCommon'`.

The `x4_th` and `x4pro_th` envs live in `platformio.personal.ini`. They are upstream's
`gh_release` and `x4pro-gh_release` profiles plus this fork's flags (`[personal_flags]`: version, OTA
URL, sans-only fonts, soft clock). `x4pro_th` extends `base`, not `firmware_tuned`, like upstream's
X4 Pro envs: it uses the prebuilt `dio_opi` Arduino libraries, whose TinyUSB is needed for USB Drive,
so `custom_sdkconfig` changes do not reach it.

Current size (1.6.5-th.4, Noto Sans only): 5,189,648 bytes = 77% of the 6.4 MiB
app partition, about 1.5 MB of headroom (stock 1.6.5: 5,589,776 bytes). With the
Pocket Hub, wasm3 and the SDK (1.6.5-th.6) the test build is 5,578,695 bytes (85%).

## Build and emulator on macOS (Docker)

Homebrew's Python 3.14 is too new for PlatformIO, and the test scripts expect
Linux, so build and emulate in a `python:3.12-bookworm` container (OrbStack or
Docker Desktop; arm64 works) with the repos mounted:

```bash
docker run -d --name x4dev -v ~/work:/work -v x4pio:/root/.platformio -w /work python:3.12-bookworm sleep infinity
docker exec x4dev bash -c 'apt-get update && apt-get install -y mtools dosfstools libslirp0 libsdl2-2.0-0 cmake ninja-build libexpat1-dev'
docker exec x4dev bash -c 'pip install uv && uv pip install --system https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip ziglang==0.17.0 pillow'
docker exec x4dev bash -c 'cd /work/crosspoint-x4 && pio pkg install -e x4_th_test && uv pip install --python ~/.platformio/penv/bin/python pioarduino==6.1.19'
docker exec x4dev bash -c 'cd /work/crosspoint-x4 && pio run -e x4_th_test'
```

- `git submodule update --init` first: without `freeink-sdk` the build stops at
  "Can not create a symbolic link for freeink-sdk/libs/…".
- The first build compiles ESP-IDF for the fork's sdkconfig (about an hour on a
  bind mount); later builds take minutes. Never run two builds on one `.pio`,
  and do not kill one half way: the next build then re-checks everything.
- If a build fails with "expected 1 entry, got ['riscv32-esp-elf', 'package.json']",
  the toolchain unpacked half way: delete `~/.platformio/packages/toolchain-riscv32-esp`
  and `~/.platformio/tools/toolchain-riscv32-esp*` and build again.
- For the emulator use the `eink-qemu-riscv32-softmmu-<version>-aarch64-linux-gnu`
  release archive (X4), or `eink-qemu-xtensa-softmmu-<version>-aarch64-linux-gnu` (X4 Pro);
  `drive.py`, `mkflash.py` and `mksd.py` are in its `eink-qemu/` folder.
- Adding or changing an env changes PlatformIO's project checksum, which wipes every
  `.pio/build/<env>`, not just the one you edited.

## Host tests

The tests are plain CMake + GoogleTest. On Windows I used `zig c++` (from
`pip install ziglang`) as the compiler.

```bash
cmake -S test -B build/test -G Ninja && cmake --build build/test && ctest --test-dir build/test
```

`test/thai` has 25 tests (word breaking, shaping, justification). The upstream
suites pass as on stock 1.6.5. On Windows, upstream's
`CssParserTest.FailedPromotionRestoresTheOnlyBackupCache` fails (fake-storage
path matching), and `ChapterHtmlSlimParserTest` does not compile
(`path.c_str()` is `wchar_t*`). Both fail the same way on stock 1.6.5. With a
local `.string()` fix, the latter's 16 tests pass with this fork's layout code.

## Testing on a real X4 over USB

`pio run -e x4_th_test` is the same firmware plus two USB-serial commands:
`CMD:PRESS <back|confirm|left|right|up|down|power> [ms]`, which injects a
button press, and `CMD:MEM`. It also logs at debug level. `CMD:SCREENSHOT`
works on every build. `scripts/device/x4test.py` scripts a test run from the PC:

```bash
python scripts/device/x4test.py COM9 log.txt reset press:down press:confirm wait:5 shot:files.png mem
```

Before flashing, back up the whole flash
(`esptool --chip esp32c3 --port COM9 read-flash 0 0x1000000 backup.bin`).
Then write the app with `write-flash 0x10000 firmware.bin`. If the device was
running from the second OTA slot (Halo 2 does), also run
`erase-region 0xe000 0x2000` so the bootloader picks slot 0. To restore
everything exactly: `write-flash 0 backup.bin`.

Windows pitfalls the driver handles: pyserial raises DTR/RTS when it opens the
port, and the C3's USB-Serial/JTAG takes that as a reset, so the driver opens
the port with both lines low. The port also re-enumerates on every reset.
Separately, Git Bash rewrites `-F path=/` into `C:/Program Files/Git/`, so set
`MSYS_NO_PATHCONV=1` when you curl the device's web API.

Getting files onto the card of a real X4 without a card reader: the C3 has no
USB OTG, so there is no USB drive mode. Use File transfer → Create hotspot, join
the `CrossPoint-Reader` network and `POST /upload?path=<folder>` (multipart
`file`) to `192.168.4.1`; the server refuses to overwrite, so `POST /delete`
(`path`) first. Remove the hotspot from the computer's known networks afterwards,
or it may rejoin it.

## Emulator

`scripts/emu/` runs the [eink-qemu](https://github.com/serialx/eink-qemu) 0.5.0
Windows build (riscv32, machine `x4`) without WSL:

```bash
python <eink-qemu>/eink-qemu/mkflash.py --machine x4 .pio/build/x4_th/firmware.bin flash.bin
python scripts/emu/mksd_win.py sd.img 512 books/       # FAT32 card; pyfatfs, no mtools
python scripts/emu/drive_win.py <eink-qemu> flash.bin sd.img log.txt wait:30 press:confirm wait:8 press:confirm wait:45 press:down wait:10 shot:page.png
```

The X4 Pro (eink-qemu 0.8.1+, `qemu-system-xtensa`) runs the same way in Linux/Docker. Its card needs
at least 2050 MB with mksd.py's default clusters, and touch is driven through the monitor:

```bash
python3 <eink-qemu>/eink-qemu/mkflash.py --machine x4pro .pio/build/x4pro_th/firmware.bin flash.bin
python3 <eink-qemu>/eink-qemu/mksd.py sd.img 2050 books/
python3 <eink-qemu>/eink-qemu/drive.py flash.bin sd.img log.txt wait:40 shot:home.png \
  'hmp:qom-set /machine/gt911 tap 240,420' wait:6 shot:library.png \
  'hmp:qom-set /machine/gt911 tap 160,195' wait:25 shot:page.png press:down wait:8 shot:page2.png \
  'hmp:qom-set /machine/gt911 tap home' wait:6
```

On a fresh card's Lyra home, `240,420` is Library and `160,195` the first book in it; `tap home` is the
capacitive Home key, and `press:up|down|power` are the side keys.

Put `.crosspoint/settings.json` with `{"language":"TH","textAntiAliasing":0}` in
`books/` to boot in Thai. The X4 panel model shows only the anti-aliasing gray
planes in screen dumps, so turn anti-aliasing off for clean captures.

### The web upload page in the emulator (PDF to XTC)

The device's real web server can be reached from a browser on the Mac. QEMU's user network forwards the
HTTP and WebSocket ports (the page uses HTTP port + 1 for uploads), and OrbStack makes the container
reachable as `x4dev.orb.local`:

```bash
docker exec -w /scratch/run x4dev sh -c 'export X4MACHINE=x4 \
  QEMU_EXTRA="-nic user,model=esp32_wifi,hostfwd=tcp::8080-:80,hostfwd=tcp::8081-:81"; \
  python3 <eink-qemu>/eink-qemu/drive.py flash.bin sd.img web.txt wait:40 \
  press:down wait:2 press:down wait:2 press:down wait:2 press:confirm wait:6 \
  press:confirm wait:12 press:confirm wait:25 wait:3500'
# then open http://x4dev.orb.local:8080/files on the Mac
```

The steps open File Transfer, Join a Network and the emulator's open `PICSimLabWifi`. Once a book has been
opened, Home lists it first, so add one more `press:down`. `drive.py` holds QEMU's only monitor connection,
so a second script cannot press keys in the same session: stop it and start another (writes to `sd.img`
are kept). When stopping it with `pkill -f`, keep the pattern out of the same `sh -c` line, or the shell
kills itself.

The converter itself (`src/network/html/personal/pdfxtc.js`) runs in any page that loads it: a static page
next to a PDF (`python3 -m http.server`) is enough to check page and chapter counts against
Epub-Translate's Python converter on the same file.

## Releasing

`.github/workflows/personal.yml` runs the host tests and builds `x4_th`, `x4pro_th` and `x4c_th` on every
PR that changes more than `docs/` and `*.md` (a `changes` job decides), and on every tag. A push to `main`
does not build again (the PR did); x4-hub tests and its image follow `server/` changes. A new push to a PR
cancels its run in progress. Compiled objects (`build_cache_dir = .cache`) are cached per env, so a build
compiles only what changed.

1. Bump `version` in `platformio.personal.ini` (e.g. `1.6.5-th.5`).
2. Write `docs/personal/releases/<version>.md`: Thai first, then English inside
   `<details><summary>English</summary> … </details>`.
3. Commit, then `git tag <version> && git push origin <version>`.

The tag build checks that the tag matches the version, then publishes the
release with `crosspoint-<version>-x3-x4.bin` and `crosspoint-<version>-x4pro.bin`, each with its
SHA-256. These are the names each model's OTA asks for (`src/network/OtaUpdater.cpp`).
