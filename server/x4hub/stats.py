"""Reading stats uploaded by the X4 (its /.crosspoint/reading-stats.json), kept as the latest copy."""
from __future__ import annotations

import datetime as dt
import json
import os
import threading
import time
from pathlib import Path

DATA = Path(os.environ.get("DATA_DIR", "/data"))
_lock = threading.Lock()


def _path() -> Path:
    return DATA / "reading-stats.json"


def save(doc: dict) -> None:
    doc = dict(doc)
    doc["received"] = time.time()
    DATA.mkdir(parents=True, exist_ok=True)
    tmp = _path().with_suffix(".tmp")
    with _lock:
        tmp.write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")
        tmp.replace(_path())


def load() -> dict:
    try:
        return json.loads(_path().read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def summary(doc: dict, today: dt.date) -> dict:
    """The numbers the dashboard and the web page show."""
    days = doc.get("days") or {}

    def day(d: dt.date) -> tuple[int, int]:
        v = days.get(d.isoformat()) or [0, 0]
        return int(v[0]), int(v[1])

    week = [(today - dt.timedelta(days=6 - i)) for i in range(7)]
    streak = 0
    start = 0 if day(today)[0] > 0 else 1
    for back in range(start, 400):
        if day(today - dt.timedelta(days=back))[0] <= 0:
            break
        streak += 1
    books = doc.get("books") or {}
    last = books.get(doc.get("lastBook", "")) or {}
    recent = sorted(books.values(), key=lambda b: b.get("last", ""), reverse=True)
    return {
        "today_min": day(today)[0] // 60,
        "today_pages": day(today)[1],
        "week": [{"date": d.isoformat(), "min": day(d)[0] // 60, "pages": day(d)[1]} for d in week],
        "streak": streak,
        "total_hours": int(doc.get("sec", 0)) // 3600,
        "total_pages": int(doc.get("pages", 0)),
        "finished": int(doc.get("finished", 0)),
        "sessions": int(doc.get("sessions", 0)),
        "last_title": last.get("t", ""),
        "last_percent": last.get("pct", -1),
        "last_min": int(last.get("sec", 0)) // 60,
        "books": [{"title": b.get("t", ""), "min": int(b.get("sec", 0)) // 60, "percent": b.get("pct", -1),
                   "done": b.get("done", "")} for b in recent[:12]],
        "received": doc.get("received"),
    }
