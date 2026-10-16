"""HTML -> the X4's 480x800 four-grey image, with headless Chromium (Playwright).

One browser is kept warm and shared; pages are opened per render. Layouts
cannot reach local files. A layout can ask for dithering (for photographs) with
<meta name="x4-dither" content="on">; otherwise each pixel takes the nearest
grey, which keeps text and flat shapes crisp.
"""
from __future__ import annotations

import io
import queue
import re
import threading
from concurrent.futures import Future

from PIL import Image

from . import bmp2

W, H = 480, 800
# Playwright's sync API belongs to the thread that started it, so one worker
# thread owns the browser and every render is handed to it.
_jobs: "queue.Queue[tuple[str, int, Future]]" = queue.Queue()
_worker: threading.Thread | None = None
_start_lock = threading.Lock()


def _run() -> None:
    from playwright.sync_api import sync_playwright
    pw = sync_playwright().start()
    browser = None
    while True:
        html, timeout_ms, fut = _jobs.get()
        try:
            if browser is None or not browser.is_connected():
                browser = pw.chromium.launch(args=["--no-sandbox", "--disable-dev-shm-usage"])
            page = browser.new_page(viewport={"width": W, "height": H}, device_scale_factor=1)
            try:
                page.route("file://**", lambda route: route.abort())
                page.set_content(html, wait_until="load", timeout=timeout_ms)
                page.evaluate("document.fonts ? document.fonts.ready.then(() => true) : true")
                png = page.screenshot(clip={"x": 0, "y": 0, "width": W, "height": H}, timeout=timeout_ms)
            finally:
                page.close()
            fut.set_result(png)
        except Exception as e:  # noqa: BLE001
            fut.set_exception(e)


_FONT_CSS = None


def _font_css() -> str:
    """Sarabun (regular and bold) as data: URIs, so every layout has it without a network."""
    global _FONT_CSS
    if _FONT_CSS is None:
        import base64
        from pathlib import Path
        fonts = Path(__file__).parent / "fonts"
        faces = []
        for weight, name in ((400, "Sarabun-Regular.ttf"), (700, "Sarabun-Bold.ttf")):
            b64 = base64.b64encode((fonts / name).read_bytes()).decode()
            faces.append(f"@font-face{{font-family:Sarabun;font-weight:{weight};"
                         f"src:url(data:font/ttf;base64,{b64}) format('truetype')}}")
        _FONT_CSS = "<style>" + "".join(faces) + "</style>"
    return _FONT_CSS


def with_fonts(html: str) -> str:
    m = re.search(r"<head[^>]*>", html, re.I)
    if m:
        return html[:m.end()] + _font_css() + html[m.end():]
    return _font_css() + html


def wants_dither(html: str) -> bool:
    return bool(re.search(r'<meta\s+name=["\']x4-dither["\']\s+content=["\']on["\']', html, re.I))


def screenshot(html: str, timeout_ms: int = 8000) -> Image.Image:
    global _worker
    with _start_lock:
        if _worker is None or not _worker.is_alive():
            _worker = threading.Thread(target=_run, name="x4-render", daemon=True)
            _worker.start()
    fut: Future = Future()
    _jobs.put((with_fonts(html), timeout_ms, fut))
    png = fut.result(timeout=timeout_ms / 1000 + 20)
    return Image.open(io.BytesIO(png)).convert("L")


def to_panel(im: Image.Image, dither: bool) -> Image.Image:
    """L image -> L image in the panel's four greys (what the X4 will show)."""
    if not dither:
        return bmp2.preview(im)
    pal = Image.new("P", (1, 1))
    pal.putpalette(sum(([v, v, v] for v in bmp2.LEVELS), []) + [0] * (256 - 4) * 3)
    q = im.convert("RGB").quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG)
    return q.convert("L")
