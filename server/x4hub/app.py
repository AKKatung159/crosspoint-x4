"""x4-hub: the homelab side of the X4 (dashboard sleep screen, reading stats).

  GET  /x4/dashboard.bmp   the sleep screen, 480x800 2-bit BMP (what the X4 downloads)
  GET  /x4/dashboard.png   the same as PNG, for a browser
  POST /x4/stats           the X4 uploads its reading-stats.json
  GET  /x4/stats           the latest stats, summarised (JSON)
  GET  /                   a small reading-stats page for phones
"""
from __future__ import annotations

import datetime as dt
import html
import io
import os

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse, Response

from . import bmp2, dashboard, stats

app = FastAPI(title="x4-hub", docs_url=None, redoc_url=None)
TOKEN = os.environ.get("X4_TOKEN", "")  # optional shared secret (header X-X4-Token)


def _check(request: Request) -> None:
    if TOKEN and request.headers.get("x-x4-token") != TOKEN:
        raise HTTPException(401, "bad token")


@app.get("/health")
def health():
    return {"ok": True}


@app.get("/x4/dashboard.bmp")
def dashboard_bmp():
    return Response(bmp2.encode(dashboard.render()), media_type="image/bmp", headers={"Cache-Control": "no-store"})


@app.get("/x4/dashboard.png")
def dashboard_png():
    buf = io.BytesIO()
    bmp2.preview(dashboard.render()).save(buf, "PNG")
    return Response(buf.getvalue(), media_type="image/png", headers={"Cache-Control": "no-store"})


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
<footer>ซิงก์ล่าสุด {received} · <a href="/x4/dashboard.png">ดูหน้าจอพัก</a></footer>
</main></body></html>"""
