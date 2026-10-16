# Building and testing

How to build this fork on Windows, run the tests, drive a real X4 over USB, and use the emulator.
Back to the [README](../../README.md).

## Build (Windows notes)

```bash
pip install https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip
pio run -e x4_th     # -> .pio/build/x4_th/firmware.bin
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

The `x4_th` env lives in `platformio.personal.ini`. It is upstream's
`gh_release` profile plus this fork's version and OTA URL.

Current size (1.6.5-th.4, Noto Sans only): 5,189,648 bytes = 77% of the 6.4 MiB
app partition, about 1.5 MB of headroom (stock 1.6.5: 5,589,776 bytes).

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

## Emulator

`scripts/emu/` runs the [eink-qemu](https://github.com/serialx/eink-qemu) 0.5.0
Windows build (riscv32, machine `x4`) without WSL:

```bash
python <eink-qemu>/eink-qemu/mkflash.py --machine x4 .pio/build/x4_th/firmware.bin flash.bin
python scripts/emu/mksd_win.py sd.img 512 books/       # FAT32 card; pyfatfs, no mtools
python scripts/emu/drive_win.py <eink-qemu> flash.bin sd.img log.txt wait:30 press:confirm wait:8 press:confirm wait:45 press:down wait:10 shot:page.png
```

Put `.crosspoint/settings.json` with `{"language":"TH","textAntiAliasing":0}` in
`books/` to boot in Thai. The X4 panel model shows only the anti-aliasing gray
planes in screen dumps, so turn anti-aliasing off for clean captures.
