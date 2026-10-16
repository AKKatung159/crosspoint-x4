import datetime as dt
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
os.environ.setdefault("DATA_DIR", str(Path(__file__).parent / "_data"))

from fastapi.testclient import TestClient  # noqa: E402

from x4hub import app as appmod, bmp2, context, dashboard, layouts, render, stats  # noqa: E402

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


def test_builtin_layouts_render_on_the_panel():
    """Every built-in layout renders with Chromium to 480x800 in the four panel greys."""
    out = Path(__file__).parent / "_out"
    out.mkdir(exist_ok=True)
    for item in layouts.names():
        src = layouts.read(item["name"])
        im = render.to_panel(render.screenshot(layouts.render_html(src, context.sample())), render.wants_dither(src))
        assert im.size == (480, 800)
        assert set(im.getdata()) <= set(bmp2.LEVELS)
        im.save(out / f"layout-{item['name']}.png")


def test_layout_editor_api():
    c = TestClient(appmod.app)
    names = [l["name"] for l in c.get("/api/layouts").json()["layouts"]]
    assert {"default", "clock"} <= set(names)
    assert c.put("/api/layouts/BAD NAME", json={"html": "x"}).status_code == 400
    assert c.put("/api/layouts/broken", json={"html": "{% if %}"}).status_code == 400
    assert c.put("/api/layouts/t-1", json={"html": "<b>{{ time }}</b>"}).json() == {"ok": True}
    assert c.post("/api/layouts/t-1/activate").json() == {"ok": True}
    assert c.get("/api/layouts").json()["active"] == "t-1"
    r = c.post("/api/preview", json={"html": "<b>{{ time }}</b>", "sample": True})
    assert r.status_code == 200 and r.content[1:4] == b"PNG"
    assert c.delete("/api/layouts/default").status_code == 404  # built-ins stay
    assert c.delete("/api/layouts/t-1").json() == {"ok": True}
    assert c.get("/api/layouts").json()["active"] == "default"
    assert c.get("/x4/dashboard.bmp").content[:2] == b"BM"
    assert "แดชบอร์ด" in c.get("/dashboard").text
