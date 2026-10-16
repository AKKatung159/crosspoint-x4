"""The data a dashboard layout can use (the Jinja2 context), gathered fresh for each render.

Every source is optional: a source that does not answer is None, and layouts
should check it ({% if weather %} ...).
"""
from __future__ import annotations

import datetime as dt

from . import dashboard, stats


def build(now: dt.datetime | None = None, stats_doc: dict | None = None,
          wx: dict | None = ..., svc: dict | None = ..., job: dict | None = ...) -> dict:
    now = now or dt.datetime.now(dashboard.TZ)
    wx = dashboard.weather() if wx is ... else wx
    svc = dashboard.services() if svc is ... else svc
    job = dashboard.translating() if job is ... else job
    reading = stats.summary(stats_doc if stats_doc is not None else stats.load(), now.date())
    for d in reading["week"]:
        day = dt.date.fromisoformat(d["date"])
        d["day_th"] = dashboard.DAYS_SHORT[day.weekday()]
        d["day_en"] = day.strftime("%a")
    peak = max([d["min"] for d in reading["week"]] + [1])
    for d in reading["week"]:
        d["percent_of_peak"] = round(d["min"] * 100 / peak)
    if wx:
        wx = dict(wx, text=dashboard.WEATHER.get(wx.get("code"), ""), place=dashboard.PLACE)
    if job and not job.get("name"):
        job = None
    return {
        "width": dashboard.W,
        "height": dashboard.H,
        "now": now,
        "time": now.strftime("%H:%M"),
        "date_th": f"วัน{dashboard.DAYS[now.weekday()]}ที่ {now.day} {dashboard.MONTHS[now.month - 1]} {now.year + 543}",
        "date_en": f"{now:%A} {now.day} {now:%B %Y}",
        "weather": wx,
        "reading": reading,
        "services": svc,
        "translating": job,
    }


def sample() -> dict:
    """Fixed data for previews and tests (no network)."""
    now = dt.datetime(2026, 10, 17, 21, 35, tzinfo=dashboard.TZ)
    doc = {"v": 1, "sec": 54000, "pages": 3200, "sessions": 40, "finished": 3, "lastBook": "/Books/a.epub",
           "days": {"2026-10-17": [1500, 30], "2026-10-16": [3600, 70], "2026-10-15": [600, 12],
                    "2026-10-13": [2400, 50]},
           "books": {"/Books/a.epub": {"t": "อลิซในแดนมหัศจรรย์", "sec": 7200, "pages": 140, "pct": 42,
                                       "last": "0000000009"}}}
    return build(now, doc, wx={"temp": 31, "code": 61, "max": 33, "min": 26, "rain": 60},
                 svc={"up": 18, "total": 19, "down": ["Jellyfin"]}, job={"name": "The Goblin Emperor", "percent": 37})
