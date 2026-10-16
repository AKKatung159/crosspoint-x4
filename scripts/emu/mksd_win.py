#!/usr/bin/env python3
"""Make an MBR + FAT32 SD card image for eink-qemu without mkfs.vfat/mtools.

eink-qemu's own mksd.py needs dosfstools and mtools, which Windows lacks. This
does the same job in Python with pyfatfs (pip install pyfatfs "setuptools<81"):
one FAT32 partition at 1 MiB, like a stock card, with a folder copied onto it.

  python scripts/emu/mksd_win.py sd.img 512 books/
"""
import argparse
import os
import shutil
import struct
import tempfile
from pathlib import Path

SECTOR = 512
START = 2048  # partition start sector (1 MiB), as on stock cards


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image")
    ap.add_argument("size_mb", type=int, nargs="?", default=512)
    ap.add_argument("dir_to_copy", type=Path, nargs="?")
    a = ap.parse_args()

    from pyfatfs.PyFat import PyFat
    from pyfatfs.PyFatFS import PyFatFS

    total = a.size_mb * 2048
    vol_sectors = total - START
    with tempfile.TemporaryDirectory() as tmp:
        vol = os.path.join(tmp, "vol.img")
        with open(vol, "wb") as f:
            f.truncate(vol_sectors * SECTOR)
        fat = PyFat()
        fat.mkfs(vol, fat_type=PyFat.FAT_TYPE_FAT32, size=vol_sectors * SECTOR, label="X4SD")
        fat.close()  # mkfs keeps the file open; the FAT is only flushed on close
        if a.dir_to_copy:
            fs = PyFatFS(vol, preserve_case=True)
            try:
                for root, dirs, files in os.walk(a.dir_to_copy):
                    dirs.sort()
                    rel = Path(root).relative_to(a.dir_to_copy).as_posix()
                    target = "/" if rel == "." else "/" + rel
                    if target != "/":
                        fs.makedir(target, recreate=True)
                    for name in sorted(files):
                        with open(os.path.join(root, name), "rb") as src, fs.openbin(
                            f"{target.rstrip('/')}/{name}", "w"
                        ) as dst:
                            shutil.copyfileobj(src, dst)
            finally:
                fs.close()
        with open(a.image, "wb") as img:
            img.truncate(total * SECTOR)
            img.seek(446)
            img.write(struct.pack("<B3sB3sII", 0, b"\xfe\xff\xff", 0x0C, b"\xfe\xff\xff", START, vol_sectors))
            img.seek(510)
            img.write(b"\x55\xaa")
            img.seek(START * SECTOR)
            with open(vol, "rb") as v:
                shutil.copyfileobj(v, img, 1 << 20)
    print(f"{a.image}: {a.size_mb} MiB FAT32" + (f", copied {a.dir_to_copy}" if a.dir_to_copy else ""))


if __name__ == "__main__":
    main()
