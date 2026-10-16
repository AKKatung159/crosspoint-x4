import datetime as dt
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
os.environ.setdefault("DATA_DIR", str(Path(__file__).parent / "_data"))

from fastapi.testclient import TestClient  # noqa: E402

from x4hub import app as appmod, bmp2, dashboard, stats  # noqa: E402

NOW = dt.datetime(2026, 10, 17, 21, 35, tzinfo=dashboard.TZ)
DOC = {"v": 1, "sec": 54000, "pages": 3200, "sessions": 40, "finished": 3, "lastBook": "/Books/a.epub",
       "days": {"2026-10-17": [1500, 30], "2026-10-16": [3600, 70], "2026-10-15": [600, 12]},
       "books": {"/Books/a.epub": {"t": "อลิซในแดนมหัศจรรย์", "sec": 7200, "pages": 140, "pct": 42, "last": "0000000009"}}}


def test_bmp_is_2bit_480x800():
    data = bmp2.encode(dashboard.render(NOW, DOC, wx=None, svc=None, job=None))
    assert data[:2] == b"BM"
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    assert (w, h, bpp) == (480, 800, 2)
    assert bmp2.decode(data).size == (480, 800)


def test_stats_summary_streak_and_week():
    s = stats.summary(DOC, NOW.date())
    assert s["today_min"] == 25 and s["streak"] == 3 and s["finished"] == 3
    assert [d["min"] for d in s["week"]][-3:] == [10, 60, 25]
    assert s["last_title"] == "อลิซในแดนมหัศจรรย์" and s["last_percent"] == 42


def test_upload_and_page():
    c = TestClient(appmod.app)
    assert c.post("/x4/stats", json={"nope": 1}).status_code == 400
    assert c.post("/x4/stats", json=DOC).json() == {"ok": True}
    assert c.get("/x4/stats").json()["finished"] == 3
    assert "สถิติการอ่าน" in c.get("/").text


def test_render_sample(tmp_path):
    """Writes a sample for CI to keep as an artifact (server/tests/_out/dashboard.png)."""
    out = Path(__file__).parent / "_out"
    out.mkdir(exist_ok=True)
    im = dashboard.render(NOW, DOC, wx={"temp": 31, "code": 61, "max": 33, "min": 26, "rain": 60},
                          svc={"up": 18, "total": 19, "down": ["Jellyfin"]}, job={"name": "The Goblin Emperor", "percent": 37})
    bmp2.preview(im).save(out / "dashboard.png")
