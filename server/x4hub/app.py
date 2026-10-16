"""x4-hub: the server side of the X4's "Dashboard" sleep screen and its reading stats.

  GET  /x4/dashboard.bmp       the active layout, 480x800 2-bit BMP (what the X4 downloads)
  GET  /x4/dashboard.png       the same as PNG, for a browser
  POST /x4/stats               the X4 uploads its reading-stats.json
  GET  /x4/stats               the latest stats, summarised (JSON)
  GET  /dashboard              the layout editor: add, edit, preview and choose layouts
  GET  /                       reading stats for a phone
  /api/layouts..., /api/preview, /api/context   what the editor uses
"""
from __future__ import annotations

import datetime as dt
import html
import io
import logging
import os
from pathlib import Path

from fastapi import Body, FastAPI, HTTPException, Request
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse, Response

from . import bmp2, context, dashboard, layouts, render, stats

log = logging.getLogger("x4hub")
app = FastAPI(title="x4-hub", docs_url=None, redoc_url=None)
TOKEN = os.environ.get("X4_TOKEN", "")  # optional shared secret (header X-X4-Token)
STATIC = Path(__file__).parent / "static"


def _check(request: Request) -> None:
    if TOKEN and request.headers.get("x-x4-token") != TOKEN:
        raise HTTPException(401, "bad token")


def _png(im) -> bytes:
    buf = io.BytesIO()
    im.save(buf, "PNG")
    return buf.getvalue()


def panel_image(html_src: str | None = None, ctx: dict | None = None):
    """The dashboard as the panel will show it (L image in four greys)."""
    name = layouts.get_active()
    src = html_src if html_src is not None else layouts.read(name)
    ctx = ctx if ctx is not None else context.build()
    page = layouts.render_html(src, ctx)
    return render.to_panel(render.screenshot(page), render.wants_dither(src))


def live_image():
    try:
        return panel_image()
    except Exception:  # noqa: BLE001  (a broken layout or Chromium must never leave the X4 blank)
        log.exception("layout render failed; using the built-in fallback")
        return bmp2.preview(dashboard.render())


@app.get("/health")
def health():
    return {"ok": True}


@app.get("/x4/dashboard.bmp")
def dashboard_bmp():
    return Response(bmp2.encode(live_image()), media_type="image/bmp", headers={"Cache-Control": "no-store"})


@app.get("/x4/dashboard.png")
def dashboard_png():
    return Response(_png(live_image()), media_type="image/png", headers={"Cache-Control": "no-store"})


# --- layout editor ----------------------------------------------------------------------------------------------

@app.get("/dashboard")
def editor():
    return FileResponse(STATIC / "editor.html", headers={"Cache-Control": "no-store"})


@app.get("/api/layouts")
def list_layouts():
    return {"active": layouts.get_active(), "layouts": layouts.names()}


@app.get("/api/layouts/{name}")
def get_layout(name: str):
    src = layouts.read(name)
    if src is None:
        raise HTTPException(404, "no such layout")
    return {"name": name, "html": src}


@app.put("/api/layouts/{name}")
async def put_layout(name: str, request: Request):
    body = await request.json()
    try:
        layouts.write(name, str(body.get("html", "")))
    except Exception as e:  # noqa: BLE001  (bad name or a template that does not parse)
        raise HTTPException(400, str(e)) from e
    return {"ok": True}


@app.delete("/api/layouts/{name}")
def delete_layout(name: str):
    if not layouts.delete(name):
        raise HTTPException(404, "only saved layouts can be deleted")
    return {"ok": True}


@app.post("/api/layouts/{name}/activate")
def activate_layout(name: str):
    try:
        layouts.set_active(name)
    except ValueError as e:
        raise HTTPException(404, str(e)) from e
    return {"ok": True}


@app.post("/api/preview")
def preview(body: dict = Body(...)):
    ctx = context.sample() if body.get("sample") else context.build()
    try:
        im = panel_image(str(body.get("html", "")), ctx)
    except Exception as e:  # noqa: BLE001
        return JSONResponse({"error": str(e)}, status_code=400)
    return Response(_png(im), media_type="image/png", headers={"Cache-Control": "no-store"})


@app.get("/api/context")
def get_context(sample: bool = False):
    ctx = context.sample() if sample else context.build()
    return Response(_json(ctx), media_type="application/json", headers={"Cache-Control": "no-store"})


def _json(obj) -> bytes:
    import json
    return json.dumps(obj, ensure_ascii=False, indent=2, default=str).encode("utf-8")


@app.post("/x4/stats")
async def upload_stats(request: Request):
    _check(request)
    try:
        doc = await request.json()
    except ValueError as e:
        raise HTTPException(400, "not JSON") from e
    if not isinstance(doc, dict) or doc.get("v") != 1:
        raise HTTPException(400, "not an X4 reading-stats file")
    stats.save(doc)
    return {"ok": True}


@app.get("/x4/stats")
def get_stats():
    return stats.summary(stats.load(), dt.datetime.now(dashboard.TZ).date())


@app.get("/", response_class=HTMLResponse)
def page():
    s = stats.summary(stats.load(), dt.datetime.now(dashboard.TZ).date())
    peak = max([d["min"] for d in s["week"]] + [1])
    bars = "".join(
        f'<div class="bar"><span style="height:{max(4, d["min"] * 100 // peak) if d["min"] else 0}%"'
        f'{" class=today" if i == 6 else ""}></span><small>{dashboard.DAYS_SHORT[dt.date.fromisoformat(d["date"]).weekday()]}</small></div>'
        for i, d in enumerate(s["week"]))
    books = "".join(
        f'<li><b>{html.escape(b["title"])}</b><span>{b["min"]} นาที'
        f'{f" · {b["percent"]}%" if b["percent"] >= 0 else ""}{" · อ่านจบ" if b["done"] else ""}</span></li>'
        for b in s["books"])
    received = (dt.datetime.fromtimestamp(s["received"], dashboard.TZ).strftime("%d/%m %H:%M")
                if s["received"] else "ยังไม่เคยซิงก์")
    return f"""<!doctype html><html lang="th"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>X4 · สถิติการอ่าน</title>
<style>
:root{{--ink:#111;--grey:#d9d9d9;--mid:#777;--bg:#fff}}
@media (prefers-color-scheme:dark){{:root{{--ink:#f2f2f2;--grey:#2c2c2c;--mid:#999;--bg:#121212}}}}
*{{box-sizing:border-box}}body{{margin:0;font:16px/1.5 Sarabun,"Noto Sans Thai",system-ui,sans-serif;background:var(--bg);color:var(--ink)}}
main{{max-width:640px;margin:0 auto;padding:24px 16px 48px}}h1{{font-size:28px;margin:0 0 20px}}
.big{{font-size:56px;font-weight:700;line-height:1}}.cards{{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin:20px 0}}
.card{{background:var(--grey);border-radius:18px;padding:16px;text-align:center}}.card b{{display:block;font-size:28px}}
.week{{display:flex;gap:10px;height:160px;align-items:flex-end;background:var(--grey);border-radius:18px;padding:16px}}
.bar{{flex:1;display:flex;flex-direction:column;justify-content:flex-end;height:100%;text-align:center}}
.bar span{{display:block;background:var(--mid);border-radius:6px}}.bar span.today{{background:var(--ink)}}
ul{{list-style:none;padding:0}}li{{display:flex;justify-content:space-between;gap:12px;padding:12px 0;border-bottom:2px solid var(--grey)}}
li span{{color:var(--mid);white-space:nowrap}}footer{{color:var(--mid);font-size:14px;margin-top:24px}}
</style></head><body><main>
<h1>สถิติการอ่าน</h1>
<div>วันนี้</div><div class="big">{s["today_min"]} นาที</div><div>{s["today_pages"]} หน้า</div>
<div class="cards"><div class="card"><b>{s["streak"]}</b>วันต่อเนื่อง</div>
<div class="card"><b>{s["finished"]}</b>อ่านจบ</div><div class="card"><b>{s["total_hours"]}</b>ชั่วโมงทั้งหมด</div></div>
<h2>7 วันที่ผ่านมา</h2><div class="week">{bars}</div>
<h2>หนังสือล่าสุด</h2><ul>{books or "<li>ยังไม่มีข้อมูล</li>"}</ul>
<footer>ซิงก์ล่าสุด {received} · <a href="/dashboard">แดชบอร์ด</a></footer>
</main></body></html>"""
