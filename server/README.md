# x4-hub: the dashboard server

The server side of the X4's **Dashboard** sleep screen and its reading stats. Run it anywhere on your network,
put its address in the X4's **Settings → Display → Dashboard URL**, and pick **Sleep Screen → Dashboard**.
Each time the X4 goes to sleep it uploads its reading stats and shows the image this server renders.

```bash
docker run -d --name x4-hub -p 8771:8000 -v x4hub-data:/data \
  -e TZ=Asia/Bangkok -e WEATHER_LAT=13.7563 -e WEATHER_LON=100.5018 -e WEATHER_PLACE=กรุงเทพฯ \
  ghcr.io/akkatung159/x4-hub:latest
```

Then open `http://<server>:8771/dashboard`.

## Layouts

A layout is one HTML file with [Jinja2](https://jinja.palletsprojects.com/templates/) placeholders, drawn by
headless Chromium at 480×800 and reduced to the four greys the panel shows: **#000, #555, #aaa, #fff**. Anything
else is rounded to the nearest of those. `font-family: Sarabun` is always available (Thai and Latin).

The **Dashboard** page (`/dashboard`) lists the layouts, edits their HTML with a live preview (sample or real
data), duplicates them, and sets the one the X4 uses. Built-in layouts (`default`, `clock`) can be edited; the
edited copy is saved next to your own layouts, and "Restore" brings the original back. Templates run in Jinja2's
sandbox and cannot read local files.

For a photograph, add `<meta name="x4-dither" content="on">`: the image is then dithered instead of rounded,
which keeps its tones.

### Variables

| Name | What |
| --- | --- |
| `time`, `now` | `"21:35"`, and the full datetime |
| `date_th`, `date_en` | `"วันเสาร์ที่ 17 ตุลาคม 2569"`, `"Saturday 17 October 2026"` |
| `weather` | `temp`, `text` (Thai), `max`, `min`, `rain` (% chance), `place`, `code` (WMO) — or none |
| `reading` | `today_min`, `today_pages`, `streak`, `finished`, `total_hours`, `total_pages`, `sessions`, `last_title`, `last_percent` (−1 if unknown), `week` (7 days, oldest first: `date`, `min`, `pages`, `day_th`, `day_en`, `percent_of_peak`), `books` (recent: `title`, `min`, `percent`, `done`) |
| `services` | `up`, `total`, `down` (names) from Gatus — or none |
| `translating` | `name`, `percent` of the book epub.lan is translating — or none |
| `width`, `height` | 480, 800 |

The editor's "Variables" panel shows the live values. A source that does not answer is none, so check it:
`{% if weather %}…{% endif %}`.

## Endpoints

| | |
| --- | --- |
| `GET /x4/dashboard.bmp` | the active layout as a 2-bit BMP (what the X4 downloads); `.png` for a browser |
| `POST /x4/stats`, `GET /x4/stats` | the X4 uploads its `reading-stats.json`; the summary |
| `GET /` | reading stats for a phone |
| `GET /dashboard` | the layout editor |
| `GET/PUT/DELETE /api/layouts/{name}`, `POST /api/layouts/{name}/activate`, `POST /api/preview`, `GET /api/context` | what the editor uses |
| `GET /health` | health check |

Optional settings: `GATUS_URL`, `EPUB_URL` (data sources), `X4_TOKEN` (require header `X-X4-Token` on uploads),
`DATA_DIR` (default `/data`). If a layout fails to render, the X4 gets a built-in fallback image instead of nothing.

## Development

```bash
pip install -r requirements.txt pytest && python -m playwright install chromium
DATA_DIR=./data uvicorn x4hub.app:app --port 8771
pytest -q tests
```

CI (`.github/workflows/personal.yml`) runs the tests, keeps sample renders as an artifact, and on `main`
publishes `ghcr.io/akkatung159/x4-hub` (`latest` and `sha-<short>`).
