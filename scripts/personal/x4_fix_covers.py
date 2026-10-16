#!/usr/bin/env python3
"""Make the pictures in EPUBs already on an X4 sharp: rewrite each JPEG/PNG as greyscale, baseline,
fitted to 480x800 (the rules epub.lan uses for books it sends). The firmware drops a book's cache when
the book is deleted, so the reading position and home summary are saved first and written back; the
rest of the cache (cover thumbnails, pictures, layout) is rebuilt on the device.

The X4 must be in File Transfer mode (Join network). Every EPUB that changes is first saved to
--backup, then the new file is uploaded under a temporary name, the original is deleted, and the new
file is renamed into its place, so an interrupted run never leaves a book missing from the card.

  python x4_fix_covers.py --host 192.168.0.224 --backup ./x4-epub-backup [--dry-run] [PATH ...]

Needs Pillow and httpx. Without PATHs it walks the whole card (skipping hidden folders).
"""
from __future__ import annotations

import argparse
import io
import sys
import zipfile
from pathlib import Path

import httpx
from PIL import Image, ImageOps

MAX_W, MAX_H = 480, 800
JPEG_QUALITY = 85


def _fit(im: Image.Image) -> Image.Image:
    if im.width <= MAX_W and im.height <= MAX_H:
        return im
    scale = min(MAX_W / im.width, MAX_H / im.height)
    return im.resize((max(1, round(im.width * scale)), max(1, round(im.height * scale))), Image.LANCZOS)


def _grey(im: Image.Image) -> Image.Image:
    im = ImageOps.exif_transpose(im)
    if im.mode in ("RGBA", "LA") or (im.mode == "P" and "transparency" in im.info):
        im = Image.alpha_composite(Image.new("RGBA", im.size, (255, 255, 255, 255)), im.convert("RGBA"))
    return im.convert("L")


def rework(data: bytes) -> bytes:
    """The picture for the X4, or `data` itself when it is already fine or not a JPEG/PNG."""
    try:
        with Image.open(io.BytesIO(data)) as src:
            fmt = src.format
            progressive = bool(src.info.get("progressive") or src.info.get("progression"))
            small = src.width <= MAX_W and src.height <= MAX_H
            out = io.BytesIO()
            if fmt == "JPEG":
                if src.mode == "L" and small and not progressive:
                    return data
                _fit(_grey(src)).save(out, "JPEG", quality=JPEG_QUALITY, optimize=True, progressive=False)
                return out.getvalue()
            if fmt == "PNG":
                if src.mode in ("L", "1") and small:
                    return data
                _fit(_grey(src)).save(out, "PNG", optimize=True)
                return out.getvalue()
    except Exception:
        pass
    return data


def convert_epub(data: bytes) -> tuple[bytes, int]:
    """(new EPUB, number of pictures changed). The mimetype entry stays first and stored."""
    zin = zipfile.ZipFile(io.BytesIO(data))
    out = io.BytesIO()
    changed = 0
    with zipfile.ZipFile(out, "w") as zout:
        for info in zin.infolist():
            body = zin.read(info)
            if info.filename == "mimetype":
                zout.writestr(info, body, compress_type=zipfile.ZIP_STORED)
                continue
            if info.filename.lower().endswith((".jpg", ".jpeg", ".png")):
                new = rework(body)
                if new is not body:
                    changed += 1
                body = new
            zout.writestr(info, body, compress_type=zipfile.ZIP_DEFLATED)
    return out.getvalue(), changed


def cache_dir(path: str) -> str:
    """/.crosspoint/epub_<std::hash(path)>: libstdc++'s 32-bit _Hash_bytes (MurmurHash2, seed 0xc70f6907)."""
    d, m = path.encode(), 0x5BD1E995
    n, i = len(d), 0
    h = (0xC70F6907 ^ n) & 0xFFFFFFFF
    while n >= 4:
        k = (int.from_bytes(d[i:i + 4], "little") * m) & 0xFFFFFFFF
        k = ((k ^ (k >> 24)) * m) & 0xFFFFFFFF
        h = ((h * m) & 0xFFFFFFFF) ^ k
        i, n = i + 4, n - 4
    if n == 3:
        h ^= d[i + 2] << 16
    if n >= 2:
        h ^= d[i + 1] << 8
    if n >= 1:
        h = ((h ^ d[i]) * m) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * m) & 0xFFFFFFFF
    return f"/.crosspoint/epub_{h ^ (h >> 15)}"


class X4:
    def __init__(self, host: str):
        self.base = f"http://{host}"
        self.c = httpx.Client(timeout=httpx.Timeout(30, read=600, write=600))

    def ls(self, path: str) -> list[dict]:
        r = self.c.get(f"{self.base}/api/files", params={"path": path})
        return r.json() if r.status_code == 200 else []

    def epubs(self, path: str = "/"):
        for e in self.ls(path):
            p = path.rstrip("/") + "/" + e["name"]
            if e["isDirectory"]:
                if not e["name"].startswith("."):
                    yield from self.epubs(p)
            elif e["name"].lower().endswith(".epub"):
                yield p

    def get(self, path: str) -> bytes:
        r = self.c.get(f"{self.base}/download", params={"path": path})
        r.raise_for_status()
        return r.content

    def put(self, folder: str, name: str, data: bytes) -> None:
        r = self.c.post(f"{self.base}/upload", params={"path": folder}, files={"file": (name, data)})
        if r.status_code != 200 or "success" not in r.text.lower():
            raise RuntimeError(f"upload {name}: {r.status_code} {r.text}")

    def get_optional(self, path: str) -> bytes | None:
        r = self.c.get(f"{self.base}/download", params={"path": path})
        return r.content if r.status_code == 200 else None

    def mkdir(self, parent: str, name: str) -> None:
        self.c.post(f"{self.base}/mkdir", data={"name": name, "path": parent})

    def delete(self, path: str) -> bool:
        return self.c.post(f"{self.base}/delete", data={"path": path}).status_code == 200

    def rename(self, path: str, name: str) -> None:
        r = self.c.post(f"{self.base}/rename", data={"path": path, "name": name})
        if r.status_code != 200:
            raise RuntimeError(f"rename {path}: {r.status_code} {r.text}")


KEPT = ("progress.bin", "home-summary.txt")  # what survives the swap; the rest is rebuilt


def save_state(x4: X4, book: str) -> dict[str, bytes]:
    cache = cache_dir(book)
    return {n: d for n in KEPT if (d := x4.get_optional(f"{cache}/{n}")) is not None}


def restore_state(x4: X4, book: str, state: dict[str, bytes]) -> None:
    if not state:
        return
    cache = cache_dir(book)
    x4.mkdir("/.crosspoint", cache.rsplit("/", 1)[1])
    for name, data in state.items():
        x4.put(cache, name, data)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--host", default="192.168.0.224")
    ap.add_argument("--backup", type=Path, required=True, help="folder for the original EPUBs")
    ap.add_argument("--dry-run", action="store_true", help="report what would change, upload nothing")
    ap.add_argument("paths", nargs="*", help="EPUB paths on the card (default: all)")
    a = ap.parse_args()

    x4 = X4(a.host)
    books = a.paths or list(x4.epubs())
    for book in books:
        folder, name = book.rsplit("/", 1)
        folder = folder or "/"
        try:
            data = x4.get(book)
            new, changed = convert_epub(data)
        except Exception as e:  # unreadable zip, network error: leave the book alone
            print(f"skip  {book}: {e}")
            continue
        if not changed:
            print(f"ok    {book}")
            continue
        print(f"fix   {book}: {changed} pictures, {len(data) // 1024} -> {len(new) // 1024} KB")
        if a.dry_run:
            continue
        backup = a.backup / folder.lstrip("/") / name
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            backup.write_bytes(data)
        tmp = name[:-5] + ".x4new.epub"
        x4.delete(f"{folder.rstrip('/')}/{tmp}")  # a leftover from an interrupted run
        x4.put(folder, tmp, new)
        state = save_state(x4, book)
        (backup.parent / (name + ".state")).mkdir(exist_ok=True)
        for n, d in state.items():  # also on the PC, in case the run stops halfway
            (backup.parent / (name + ".state") / n).write_bytes(d)
        if not x4.delete(book):  # the firmware also removes the book's cache here
            raise RuntimeError(f"could not delete {book}; the new copy is {tmp}")
        x4.rename(f"{folder.rstrip('/')}/{tmp}", name)
        restore_state(x4, book, state)
        print(f"      kept: {', '.join(state) or 'nothing (never opened)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
