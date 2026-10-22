# Installing on the X4 / X4 Pro

[ภาษาไทย](install.md) · **English**

Back to the [README](../../README.en.md).

Download the file for your model from this repo's
[Releases](https://github.com/AKKatung159/crosspoint-x4/releases), or build it ([development.md](development.md)):

| Device | File | Chip |
| --- | --- | --- |
| X4, X3 | `crosspoint-<version>-x3-x4.bin` | ESP32-C3 |
| X4 Pro | `crosspoint-<version>-x4pro.bin` | ESP32-S3 |
| X4 Classic | `crosspoint-<version>-x4c.bin` | ESP32-S3 |

Each is a normal app image, the same kind of file as the official `crosspoint-1.6.5-x3-x4.bin` /
`-x4pro.bin`. Never flash the other model's file (OTA and the SD-card update refuse it). The X4 Pro build
is tested in the emulator only so far.

**Keep a rollback image before flashing.** For the X4, from the
[Halo 2 v2.2.15 release](https://github.com/kocha01/crosspoint-halo2-custom/releases/tag/v2.2.15),
`firmware.bin` has SHA-256
`b0156ec5760dce61dafe24bb76f7de58c25a3f0f8ee7f340d69420b09e151929`. For the X4 Pro, the official
`crosspoint-1.6.5-x4pro.bin` from [CrossPoint's releases](https://github.com/crosspoint-reader/crosspoint-reader/releases).

## Option A: web flasher (USB-C, Chrome/Edge)

1. Connect the device by USB-C and wake it.
2. Open https://crosspointreader.com/#flash-tools, choose your model (**X4** or **Xteink X4Pro**), then **Custom .bin**, and pick the file.
3. Wait for it to finish, then let the device reboot. Do not unplug while flashing.

## Option B: from the SD card (no computer connection)

1. Copy the `.bin` to the SD card.
2. On the device: **Settings → System → SD Card Firmware Update**, then pick the
   file. Use this only if the firmware currently running has that menu.

## Option C: esptool

```bash
esptool.py --chip esp32c3 --port COM5 --baud 921600 write_flash 0x10000 crosspoint-1.6.5-th.4-x3-x4.bin
```

On the X4 Pro use `--chip esp32s3` and the `-x4pro.bin` file:

```bash
esptool.py --chip esp32s3 --port COM5 --baud 921600 write_flash 0x10000 crosspoint-1.6.5-th.7-x4pro.bin
```

Use `0x10000` only. Writing elsewhere can break the bootloader or the partition table.

## Rollback

Flash the Halo 2 `firmware.bin` (or the official `crosspoint-1.6.5-x3-x4.bin`)
the same way; on the X4 Pro, the official `crosspoint-1.6.5-x4pro.bin`. Books, progress and settings on the SD card are kept. Each
firmware rebuilds its own layout cache the first time a book is opened.

After installing: **Settings → System → Language → ไทย**. Built-in fonts already
include Thai. SD-card fonts without Thai borrow Thai from the built-in fonts.
