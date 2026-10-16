"""The X4 sleep-screen dashboard: 480x800, four greys, Thai.

Every source is optional: if the weather, Gatus or epub.lan does not answer
within a couple of seconds, its block is simply left out.
"""
from __future__ import annotations

import datetime as dt
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from zoneinfo import ZoneInfo

import httpx
from PIL import Image, ImageDraw, ImageFont

from . import stats as statsmod

W, H = 480, 800
M = 28  # side margin
BLACK, DARK, LIGHT, WHITE = 0, 85, 170, 255
FONTS = Path(__file__).parent / "fonts"

TZ = ZoneInfo(os.environ.get("TZ", "Asia/Bangkok"))
LAT = float(os.environ.get("WEATHER_LAT", "13.7563"))
LON = float(os.environ.get("WEATHER_LON", "100.5018"))
PLACE = os.environ.get("WEATHER_PLACE", "กรุงเทพฯ")
GATUS_URL = os.environ.get("GATUS_URL", "http://gatus:8080")
EPUB_URL = os.environ.get("EPUB_URL", "http://epub-translate:8000")

DAYS = ["จันทร์", "อังคาร", "พุธ", "พฤหัสบดี", "ศุกร์", "เสาร์", "อาทิตย์"]
DAYS_SHORT = ["จ", "อ", "พ", "พฤ", "ศ", "ส", "อา"]
MONTHS = ["มกราคม", "กุมภาพันธ์", "มีนาคม", "เมษายน", "พฤษภาคม", "มิถุนายน", "กรกฎาคม", "สิงหาคม", "กันยายน",
          "ตุลาคม", "พฤศจิกายน", "ธันวาคม"]

# WMO weather codes (Open-Meteo) in Thai.
WEATHER = {0: "ฟ้าใส", 1: "แดดเป็นส่วนใหญ่", 2: "มีเมฆบางส่วน", 3: "เมฆมาก", 45: "หมอก", 48: "หมอกน้ำแข็ง",
           51: "ฝนปรอย", 53: "ฝนปรอย", 55: "ฝนปรอยหนัก", 61: "ฝนเล็กน้อย", 63: "ฝนปานกลาง", 65: "ฝนหนัก",
           80: "ฝนไล่ช่วง", 81: "ฝนไล่ช่วง", 82: "ฝนตกหนักเป็นช่วง", 95: "พายุฝนฟ้าคะนอง", 96: "พายุลูกเห็บ",
           99: "พายุลูกเห็บ"}


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "Sarabun-Bold.ttf" if bold else "Sarabun-Regular.ttf"
    layout = ImageFont.Layout.RAQM if _has_raqm() else ImageFont.Layout.BASIC
    return ImageFont.truetype(str(FONTS / name), size, layout_engine=layout)


def _has_raqm() -> bool:
    from PIL import features
    return bool(features.check("raqm"))


# --- sources ----------------------------------------------------------------------------------------------------

_cache: dict[str, tuple[float, object]] = {}


def _cached(key: str, ttl: float, fn):
    hit = _cache.get(key)
    if hit and time.time() - hit[0] < ttl:
        return hit[1]
    try:
        value = fn()
    except Exception:  # noqa: BLE001  (any source may be down; the block is dropped)
        value = hit[1] if hit else None
    _cache[key] = (time.time(), value)
    return value


def weather() -> dict | None:
    def get():
        r = httpx.get("https://api.open-meteo.com/v1/forecast", timeout=3, params={
            "latitude": LAT, "longitude": LON, "timezone": str(TZ), "forecast_days": 1,
            "current": "temperature_2m,weather_code",
            "daily": "temperature_2m_max,temperature_2m_min,precipitation_probability_max"})
        r.raise_for_status()
        j = r.json()
        return {"temp": round(j["current"]["temperature_2m"]), "code": j["current"]["weather_code"],
                "max": round(j["daily"]["temperature_2m_max"][0]), "min": round(j["daily"]["temperature_2m_min"][0]),
                "rain": j["daily"]["precipitation_probability_max"][0]}
    return _cached("weather", 600, get)


def services() -> dict | None:
    def get():
        r = httpx.get(f"{GATUS_URL}/api/v1/endpoints/statuses", timeout=2)
        r.raise_for_status()
        up, down = 0, []
        for e in r.json():
            res = (e.get("results") or [{}])[-1]
            if res.get("success", True):
                up += 1
            else:
                down.append(e.get("name", "?"))
        return {"up": up, "total": up + len(down), "down": down}
    return _cached("gatus", 60, get)


def translating() -> dict | None:
    def get():
        r = httpx.get(f"{EPUB_URL}/api/jobs", timeout=2)
        r.raise_for_status()
        busy = [j for j in r.json() if j.get("status") not in ("done", "error", "failed", "cancelled")]
        if not busy:
            return {"name": "", "percent": -1}
        j = busy[0]
        done, total = j.get("done") or 0, j.get("total") or 0
        return {"name": j.get("name", ""), "percent": int(done * 100 / total) if total else -1}
    return _cached("epub", 60, get)


# --- drawing ----------------------------------------------------------------------------------------------------

@dataclass
class Canvas:
    im: Image.Image = field(default_factory=lambda: Image.new("L", (W, H), WHITE))

    def __post_init__(self):
        self.d = ImageDraw.Draw(self.im)

    def text(self, xy, s, size, bold=False, fill=BLACK, anchor="la"):
        self.d.text(xy, s, font=font(size, bold), fill=fill, anchor=anchor)

    def width(self, s, size, bold=False) -> int:
        return int(self.d.textlength(s, font=font(size, bold)))

    def fit(self, s, size, maxw, bold=False) -> str:
        if self.width(s, size, bold) <= maxw:
            return s
        while s and self.width(s + "…", size, bold) > maxw:
            s = s[:-1]
        return s + "…"

    def card(self, y, h, fill=LIGHT):
        self.d.rounded_rectangle([M, y, W - M, y + h], 18, fill=fill)

    def bar(self, x, y, w, h, percent):
        self.d.rounded_rectangle([x, y, x + w, y + h], h // 2, fill=WHITE)
        if percent > 0:
            self.d.rounded_rectangle([x, y, x + max(h, int(w * min(percent, 100) / 100)), y + h], h // 2, fill=BLACK)


def render(now: dt.datetime | None = None, stats_doc: dict | None = None,
           wx: dict | None = ..., svc: dict | None = ..., job: dict | None = ...) -> Image.Image:
    """The dashboard as an L image. Sources default to live; tests pass them in."""
    now = now or dt.datetime.now(TZ)
    wx = weather() if wx is ... else wx
    svc = services() if svc is ... else svc
    job = translating() if job is ... else job
    st = statsmod.summary(stats_doc if stats_doc is not None else statsmod.load(), now.date())
    c = Canvas()

    # Time, date, weather.
    c.text((M - 4, 18), now.strftime("%H:%M"), 104, bold=True)
    c.text((M, 150), f"วัน{DAYS[now.weekday()]}ที่ {now.day} {MONTHS[now.month - 1]} {now.year + 543}", 28)
    if wx:
        c.text((W - M, 34), f"{wx['temp']}°", 64, bold=True, anchor="ra")
        c.text((W - M, 112), WEATHER.get(wx["code"], ""), 22, anchor="ra")
    y = 200
    if wx:
        c.text((M, y), f"{PLACE}  สูง {wx['max']}° ต่ำ {wx['min']}°  โอกาสฝน {wx['rain']}%", 22, fill=DARK)
    y += 48

    # Reading.
    c.card(y, 300)
    c.text((M + 22, y + 16), "การอ่านวันนี้", 22, fill=DARK)
    c.text((M + 22, y + 42), f"{st['today_min']} นาที", 52, bold=True)
    c.text((W - M - 22, y + 24), f"ต่อเนื่อง {st['streak']} วัน", 22, anchor="ra")
    c.text((W - M - 22, y + 56), f"อ่านจบ {st['finished']} เล่ม", 22, anchor="ra")
    cy, ch = y + 116, 70
    peak = max([d["min"] for d in st["week"]] + [1])
    slot = (W - 2 * M - 44) // 7
    for i, d in enumerate(st["week"]):
        x = M + 22 + i * slot + 8
        bw = slot - 16
        c.d.rounded_rectangle([x, cy, x + bw, cy + ch], 6, fill=WHITE)
        h = 0 if d["min"] == 0 else max(6, d["min"] * ch // peak)
        if h:
            c.d.rounded_rectangle([x, cy + ch - h, x + bw, cy + ch], 6, fill=BLACK if i == 6 else DARK)
        wd = DAYS_SHORT[dt.date.fromisoformat(d["date"]).weekday()]
        c.text((x + bw // 2, cy + ch + 6), wd, 18, bold=i == 6, anchor="ma")
    if st["last_title"]:
        c.text((M + 22, y + 222), c.fit(st["last_title"], 24, W - 2 * M - 44, bold=True), 24, bold=True)
        if st["last_percent"] >= 0:
            c.bar(M + 22, y + 264, W - 2 * M - 120, 12, st["last_percent"])
            c.text((W - M - 22, y + 256), f"{st['last_percent']}%", 22, anchor="ra")
    y += 300 + 18

    # Homelab.
    c.card(y, 150 if job and job["name"] else 96)
    c.text((M + 22, y + 16), "Homelab", 22, fill=DARK)
    if svc:
        ok = not svc["down"]
        c.text((M + 22, y + 44), f"ออนไลน์ {svc['up']}/{svc['total']}", 30, bold=True)
        if not ok:
            c.text((W - M - 22, y + 50), c.fit("ล่ม: " + ", ".join(svc["down"]), 20, 220), 20, anchor="ra")
        else:
            c.text((W - M - 22, y + 50), "ทุกอย่างปกติ", 20, anchor="ra")
    else:
        c.text((M + 22, y + 44), "ติดต่อ Gatus ไม่ได้", 24)
    if job and job["name"]:
        c.text((M + 22, y + 96), c.fit("กำลังแปล: " + job["name"], 20, W - 2 * M - 44), 20)
        if job["percent"] >= 0:
            c.bar(M + 22, y + 126, W - 2 * M - 44, 10, job["percent"])

    c.text((W // 2, H - 34), f"อัปเดต {now:%H:%M} · x4.lan", 18, fill=DARK, anchor="ma")
    return c.im
