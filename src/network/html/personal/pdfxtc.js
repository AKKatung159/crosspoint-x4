// PERSONAL: PDF -> XTC in the browser, before upload (FilesPage "Convert PDF to XTC").
// A port of Epub-Translate's app/pdfslice.py, xtc.py and xtccover.py (github.com/AKKatung159/Epub-Translate):
// each PDF page is rendered by pdf.js, its running header/footer and white margins dropped, and the bodies
// flowed one under another and cut into 800x480 landscape strips in the white gaps between lines (read with
// the device turned sideways: text 1.67x larger). Mode "page" keeps whole pages upright instead. Output is
// 1-bit XTC: the firmware streams XTG pages from the card, while a 2-bit XTCH page needs a 96 KB block.
// The PDF outline becomes the chapter list; long books are split into parts of PER_PART device pages.
(function () {
  'use strict';

  const PDFJS_VERSION = '4.10.38';
  const PDFJS_CDN = `https://cdn.jsdelivr.net/npm/pdfjs-dist@${PDFJS_VERSION}/build/`;
  // Offline copy on the card (device hotspot, no internet): saved there, gzipped, the first time pdf.js
  // loads from the CDN. The version is in the names, so an upgrade fetches fresh files.
  const CARD_DIR = '/pdfjs';
  const CARD_FILES = [
    { name: `pdf-${PDFJS_VERSION}.min.mjs.gz`, cdn: 'pdf.min.mjs' },
    { name: `pdf-${PDFJS_VERSION}.worker.min.mjs.gz`, cdn: 'pdf.worker.min.mjs' },
  ];

  const W = 480, H = 800;            // device page, portrait
  const LONG = H, SHORT = W;         // landscape strip: 800 wide, 480 high
  const INK = 200;                   // darker than this is ink
  const TILE = 16;
  const TEXT_THRESHOLD = 170;        // not 128: anti-aliased strokes of small text stay whole
  const GAP = 14;                    // px between two PDF pages in the flow, like a paragraph
  const PER_PART = 600;              // device pages per file: the firmware keeps 16 B per page in RAM
  const PART_CHAPTERS = 100;         // chapter entries per file: each name is a small heap allocation
  const MAX_CHAPTERS = 64;           // "every N pages" entries when the PDF has no bookmarks
  const MAX_RENDER_PIXELS = 16e6;    // cap for posters and huge scans
  const HEADING = /^\s*((?:บทที่|ตอนที่|ตอน|บท|chapter|CHAPTER|Chapter)\s*[0-9๐-๙]+[^\n]{0,60})/m;
  const NUM = /(บทที่|ตอนที่|ตอน|บท|chapter|ch\.?|part|เล่ม)\s*([0-9๐-๙]+)/i;
  const COVER_FONT = '"Sarabun", "Noto Sans Thai", "Leelawadee UI", Tahoma, sans-serif';

  // ---------------------------------------------------------------- pdf.js

  let pdfjs = null;

  const gzip = (text) =>
    new Response(new Blob([text]).stream().pipeThrough(new CompressionStream('gzip'))).blob();
  const gunzip = (blob) => new Response(blob.stream().pipeThrough(new DecompressionStream('gzip'))).text();
  const moduleUrl = (text) => URL.createObjectURL(new Blob([text], { type: 'text/javascript' }));

  async function cardNames() {
    try {
      const res = await fetch('/api/files?path=' + encodeURIComponent(CARD_DIR));
      return res.ok ? new Set((await res.json()).map((f) => f.name)) : new Set();
    } catch (e) {
      return new Set();
    }
  }

  async function loadFromCard() {
    const urls = [];
    for (const f of CARD_FILES) {
      const res = await fetch('/download?path=' + encodeURIComponent(`${CARD_DIR}/${f.name}`));
      if (!res.ok) throw new Error(`${CARD_DIR}/${f.name}: ${res.status}`);
      urls.push(moduleUrl(await gunzip(await res.blob())));
    }
    const lib = await import(urls[0]);
    lib.GlobalWorkerOptions.workerSrc = urls[1];
    return lib;
  }

  // The CDN copy, once per card: fetched again as text (the browser cache has it), gzipped, uploaded.
  async function saveToCard(onStatus) {
    const have = await cardNames();
    const missing = CARD_FILES.filter((f) => !have.has(f.name));
    if (!missing.length) return;
    if (!have.size) {
      const form = new FormData();
      form.append('name', CARD_DIR.slice(1));
      form.append('path', '/');
      await fetch('/mkdir', { method: 'POST', body: form });     // "already exists" is fine too
    }
    for (const f of missing) {
      if (onStatus) onStatus(`Saving pdf.js to the SD card for offline use (${f.cdn})...`);
      const body = new FormData();
      body.append('file', await gzip(await (await fetch(PDFJS_CDN + f.cdn)).text()), f.name);
      const res = await fetch('/upload?path=' + encodeURIComponent(CARD_DIR), { method: 'POST', body });
      if (!res.ok) throw new Error(`upload ${f.name}: ${res.status} ${await res.text()}`);
    }
  }

  // CDN first (fast, and fills the card copy once); the card copy when the CDN cannot be reached.
  // `source: 'card'` skips the CDN (tests).
  async function loadPdfjs({ onStatus = null, source = 'auto' } = {}) {
    if (pdfjs) return pdfjs;
    let cdnError = null;
    if (source !== 'card') {
      try {
        pdfjs = await import(PDFJS_CDN + 'pdf.min.mjs');
        pdfjs.GlobalWorkerOptions.workerSrc = PDFJS_CDN + 'pdf.worker.min.mjs';
      } catch (e) {
        cdnError = e;
      }
    }
    if (pdfjs) {
      if ('CompressionStream' in window) {
        try {
          await saveToCard(onStatus);
        } catch (e) {
          console.warn('[PdfXtc] could not save pdf.js to the card:', e);   // converting still works
        }
      }
      return pdfjs;
    }
    if (cdnError) console.warn('[PdfXtc] CDN pdf.js unavailable, trying the card:', cdnError);
    try {
      if (!('DecompressionStream' in window)) throw new Error('this browser cannot read the gzipped copy');
      if (onStatus) onStatus('Loading pdf.js from the SD card...');
      pdfjs = await loadFromCard();
      return pdfjs;
    } catch (e) {
      pdfjs = null;
      throw new Error('Cannot load pdf.js: no internet, and no saved copy on the SD card yet ' +
                      `(open this page once while online; ${e.message})`);
    }
  }

  // ---------------------------------------------------------------- grey images ({w, h, d: Uint8Array})

  function blank(w, h) {
    const d = new Uint8Array(w * h);
    d.fill(255);
    return { w, h, d };
  }

  function fromCanvas(canvas) {
    const { width: w, height: h } = canvas;
    const rgba = canvas.getContext('2d').getImageData(0, 0, w, h).data;
    const d = new Uint8Array(w * h);
    for (let i = 0, j = 0; i < d.length; i++, j += 4) {
      d[i] = (rgba[j] * 299 + rgba[j + 1] * 587 + rgba[j + 2] * 114 + 500) / 1000;
    }
    return { w, h, d };
  }

  function toCanvas(g) {
    const c = document.createElement('canvas');
    c.width = g.w;
    c.height = g.h;
    const ctx = c.getContext('2d');
    const img = ctx.createImageData(g.w, g.h);
    for (let i = 0, j = 0; i < g.d.length; i++, j += 4) {
      img.data[j] = img.data[j + 1] = img.data[j + 2] = g.d[i];
      img.data[j + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
    return c;
  }

  // Downscale in halving steps (canvas smoothing alone aliases on big ratios), then once to size.
  function resize(g, nw, nh) {
    nw = Math.max(1, Math.round(nw));
    nh = Math.max(1, Math.round(nh));
    if (nw === g.w && nh === g.h) return g;
    let src = toCanvas(g);
    while (src.width / 2 >= nw && src.height / 2 >= nh) {
      const half = document.createElement('canvas');
      half.width = Math.round(src.width / 2);
      half.height = Math.round(src.height / 2);
      const hc = half.getContext('2d');
      hc.imageSmoothingQuality = 'high';
      hc.drawImage(src, 0, 0, half.width, half.height);
      src = half;
    }
    const out = document.createElement('canvas');
    out.width = nw;
    out.height = nh;
    const oc = out.getContext('2d');
    oc.imageSmoothingQuality = 'high';
    oc.drawImage(src, 0, 0, nw, nh);
    return fromCanvas(out);
  }

  function crop(g, l, t, r, b) {
    const w = r - l, h = b - t, d = new Uint8Array(w * h);
    for (let y = 0; y < h; y++) d.set(g.d.subarray((t + y) * g.w + l, (t + y) * g.w + l + w), y * w);
    return { w, h, d };
  }

  function paste(dst, src, x, y) {
    for (let row = 0; row < src.h; row++) {
      const ty = y + row;
      if (ty < 0 || ty >= dst.h) continue;
      const x0 = Math.max(0, x), x1 = Math.min(dst.w, x + src.w);
      if (x1 <= x0) continue;
      dst.d.set(src.d.subarray(row * src.w + (x0 - x), row * src.w + (x1 - x)), ty * dst.w + x0);
    }
  }

  function fit(g, w, h) {
    const s = Math.min(w / g.w, h / g.h);
    return resize(g, g.w * s, g.h * s);
  }

  // Clockwise when `cw` (top of the text on the right edge of the screen), else counter-clockwise.
  function rotate(g, cw) {
    const out = { w: g.h, h: g.w, d: new Uint8Array(g.d.length) };
    for (let y = 0; y < g.h; y++) {
      for (let x = 0; x < g.w; x++) {
        const v = g.d[y * g.w + x];
        if (cw) out.d[x * out.w + (g.h - 1 - y)] = v;
        else out.d[(g.w - 1 - x) * out.w + y] = v;
      }
    }
    return out;
  }

  function rowInk(g) {
    const rows = new Int32Array(g.h);
    for (let y = 0; y < g.h; y++) {
      let n = 0;
      for (let i = y * g.w, e = i + g.w; i < e; i++) if (g.d[i] < INK) n++;
      rows[y] = n;
    }
    return rows;
  }

  // ---------------------------------------------------------------- page analysis (pdfslice.py)

  function inkBox(g) {
    const rows = rowInk(g);
    const cols = new Int32Array(g.w);
    for (let y = 0; y < g.h; y++) for (let x = 0; x < g.w; x++) if (g.d[y * g.w + x] < INK) cols[x]++;
    let t = -1, b = -1, l = -1, r = -1;
    for (let y = 0; y < g.h; y++) if (rows[y] > 1) { if (t < 0) t = y; b = y; }
    for (let x = 0; x < g.w; x++) if (cols[x] > 1) { if (l < 0) l = x; r = x; }
    return t < 0 || l < 0 ? null : [l, t, r + 1, b + 1];
  }

  function blocks(g) {
    const rows = rowInk(g), out = [];
    let start = -1;
    for (let y = 0; y <= g.h; y++) {
      const ink = y < g.h && rows[y] > 1;
      if (ink && start < 0) start = y;
      if (!ink && start >= 0) { out.push([start, y]); start = -1; }
    }
    return out;
  }

  const median = (xs) => xs.slice().sort((a, b) => a - b)[Math.floor(xs.length / 2)];

  // Rows without a running header/footer: a thin first (last) line in the top (bottom) 12 % of the page,
  // followed (preceded) by a gap clearly wider than the gaps between lines of text.
  function bodyRows(g) {
    const b = blocks(g), h = g.h;
    if (b.length < 4) return b.length ? [b[0][0], b[b.length - 1][1]] : [0, h];
    const gaps = [];
    for (let i = 0; i + 1 < b.length; i++) gaps.push(b[i + 1][0] - b[i][1]);
    const lineGap = median(gaps);
    const tall = median(b.map(([a, e]) => e - a)) * 1.6;
    const wide = Math.max(2.2 * lineGap, h * 0.012);
    let top = 0, bottom = b.length;
    if (b[0][0] < h * 0.12 && b[0][1] - b[0][0] <= tall && b[1][0] - b[0][1] > wide) top = 1;
    const n = b.length;
    if (b[n - 1][1] > h * 0.88 && b[n - 1][1] - b[n - 1][0] <= tall && b[n - 1][0] - b[n - 2][1] > wide) bottom -= 1;
    return [b[top][0], b[bottom - 1][1]];
  }

  // Rows split into strips of at most `limit`, cut in the middle of the white run that ends closest to the
  // limit; with no gap (a picture) the cut overlaps the next strip a little.
  function cutPoints(g, limit) {
    const rows = rowInk(g), h = g.h, strips = [];
    let y = 0;
    while (y < h) {
      if (h - y <= limit) { strips.push([y, h]); break; }
      const lo = y + Math.floor(limit * 0.55);
      let end = -1;
      for (let r = Math.min(h - 1, y + limit); r >= lo; r--) if (rows[r] === 0) { end = r; break; }
      if (end >= 0) {
        let start = end;
        while (start - 1 >= lo && rows[start - 1] === 0) start--;
        const cut = Math.floor((start + end + 1) / 2);
        strips.push([y, cut]);
        y = cut;
      } else {
        const cut = y + limit;
        strips.push([y, cut]);
        y = cut - Math.floor(limit / 12);
      }
    }
    return strips.filter(([a, b]) => { for (let r = a; r < b; r++) if (rows[r] > 0) return true; return false; });
  }

  async function pageContent(page, o) {
    const pw = page.getViewport({ scale: 1 }).width;
    const target = o.mode === 'page' ? SHORT : LONG;
    let scale = Math.max(1, (2 * target) / pw);
    let vp = page.getViewport({ scale });
    if (vp.width * vp.height > MAX_RENDER_PIXELS) {
      scale *= Math.sqrt(MAX_RENDER_PIXELS / (vp.width * vp.height));
      vp = page.getViewport({ scale });
    }
    const canvas = document.createElement('canvas');
    canvas.width = Math.ceil(vp.width);
    canvas.height = Math.ceil(vp.height);
    const ctx = canvas.getContext('2d', { willReadFrequently: true });
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    await page.render({ canvasContext: ctx, viewport: vp }).promise;
    const g = fromCanvas(canvas);
    canvas.width = canvas.height = 0;
    if (o.trim) {
      const [y0, y1] = bodyRows(g);
      g.d.fill(255, 0, y0 * g.w);
      g.d.fill(255, y1 * g.w);
    }
    const box = inkBox(g);
    if (!box) return null;
    const m = o.margin * 2, [l, t, r, b] = box;
    return crop(g, Math.max(0, l - m), Math.max(0, t - m), Math.min(g.w, r + m), Math.min(g.h, b + m));
  }

  function wholePage(content) {
    const canvas = blank(W, H), img = fit(content, W, H);
    paste(canvas, img, Math.floor((W - img.w) / 2), Math.floor((H - img.h) / 2));
    return canvas;
  }

  function stripSource(content, o) {
    let s = LONG / content.w;
    if (o.mode === '3' || o.mode === '4') s = Math.min(s, (Number(o.mode) * SHORT * 0.92) / content.h);
    return resize(content, content.w * s, content.h * s);
  }

  // Page bodies one under the other, like a scroll; full strips leave the top as soon as the next cut is
  // certain, so a page's last lines share a strip with the next page instead of leaving it half empty.
  class Flow {
    constructor(o, emit) { this.o = o; this.emit = emit; this.buf = null; this.count = 0; }

    out(strip) {
      const canvas = blank(LONG, SHORT);
      paste(canvas, strip, Math.floor((LONG - strip.w) / 2), 0);   // top-aligned: the eye starts in one place
      this.emit(rotate(canvas, this.o.rotate === 'cw'));
      this.count++;
    }

    add(img) {
      let y0;
      if (this.buf === null) {
        y0 = 0;
        this.buf = img;
      } else {
        y0 = this.buf.h + GAP;
        const joined = blank(Math.max(this.buf.w, img.w), y0 + img.h);
        paste(joined, this.buf, Math.floor((joined.w - this.buf.w) / 2), 0);
        paste(joined, img, Math.floor((joined.w - img.w) / 2), y0);
        this.buf = joined;
      }
      const cuts = cutPoints(this.buf, SHORT);
      let k = cuts.findIndex(([, bottom]) => bottom > y0);
      if (k < 0) k = cuts.length - 1;
      const start = this.count + Math.max(0, k);
      for (const [top, bottom] of cuts.slice(0, -1)) this.out(crop(this.buf, 0, top, this.buf.w, bottom));
      if (cuts.length) this.buf = crop(this.buf, 0, cuts[cuts.length - 1][0], this.buf.w, this.buf.h);
      return start;
    }

    flush() {
      if (this.buf === null) return;
      for (const [top, bottom] of cutPoints(this.buf, SHORT)) this.out(crop(this.buf, 0, top, this.buf.w, bottom));
      this.buf = null;
    }
  }

  // ---------------------------------------------------------------- 1-bit pages (xtc.py)

  const BAYER = [[0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
                 [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
                 [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
                 [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21]];

  // Per 16x16 tile of a 480x800 page: inside a picture? Dense tiles (>= 60 % not white) form 4-connected
  // regions; a region is a picture when at least a third of its tiles are mostly mid-grey. Text tiles are
  // never mostly mid-grey, so a paragraph next to a picture stays sharp. Pictures then grow into light tiles.
  function pictureTiles(g) {
    const rows = g.h / TILE, cols = g.w / TILE, n = rows * cols;
    const mid = new Uint8Array(n), inked = new Uint8Array(n), light = new Uint8Array(n);
    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        let midN = 0, inkN = 0, min = 255;
        for (let y = r * TILE; y < (r + 1) * TILE; y++) {
          for (let x = c * TILE, i = y * g.w + x; x < (c + 1) * TILE; x++, i++) {
            const v = g.d[i];
            if (v > 40 && v < 215) midN++;
            if (v < 215) inkN++;
            if (v < min) min = v;
          }
        }
        const k = r * cols + c;
        mid[k] = midN >= 0.5 * TILE * TILE ? 1 : 0;
        inked[k] = inkN >= 0.6 * TILE * TILE ? 1 : 0;
        light[k] = min > 120 ? 1 : 0;
      }
    }
    const out = new Uint8Array(n), seen = new Uint8Array(n);
    for (let k0 = 0; k0 < n; k0++) {
      if (!inked[k0] || seen[k0]) continue;
      const region = [], stack = [k0];
      seen[k0] = 1;
      while (stack.length) {
        const k = stack.pop(), r = Math.floor(k / cols), c = k % cols;
        region.push(k);
        for (const [rr, cc] of [[r + 1, c], [r - 1, c], [r, c + 1], [r, c - 1]]) {
          if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
          const kk = rr * cols + cc;
          if (inked[kk] && !seen[kk]) { seen[kk] = 1; stack.push(kk); }
        }
      }
      let midCount = 0;
      for (const k of region) midCount += mid[k];
      if (midCount >= Math.max(2, region.length / 3)) for (const k of region) out[k] = 1;
    }
    for (let step = 0; step < 6; step++) {
      const grow = [];
      for (let k = 0; k < n; k++) {
        if (out[k] || !light[k]) continue;
        const r = Math.floor(k / cols), c = k % cols;
        if ((r > 0 && out[k - cols]) || (r + 1 < rows && out[k + cols]) || (c > 0 && out[k - 1]) ||
            (c + 1 < cols && out[k + 1])) grow.push(k);
      }
      if (!grow.length) break;
      for (const k of grow) out[k] = 1;
    }
    return { out, cols };
  }

  // XTG page: 22-byte header + rows top to bottom, 8 px a byte, MSB leftmost, 1 = white.
  function encodePage(g) {
    const { out: photo, cols } = pictureTiles(g);
    const stride = W / 8, page = new Uint8Array(22 + stride * H);
    const dv = new DataView(page.buffer);
    page.set([0x58, 0x54, 0x47, 0x00]);           // "XTG\0"
    dv.setUint16(4, W, true);
    dv.setUint16(6, H, true);
    dv.setUint32(10, stride * H, true);           // colorMode 0, compression 0, then 8 bytes of zero
    for (let y = 0; y < H; y++) {
      const tileRow = Math.floor(y / TILE) * cols, bayer = BAYER[y % 8];
      for (let xb = 0; xb < stride; xb++) {
        let byte = 0;
        for (let bit = 0; bit < 8; bit++) {
          const x = xb * 8 + bit, v = g.d[y * W + x];
          const thr = photo[tileRow + Math.floor(x / TILE)] ? (bayer[x % 8] + 0.5) * 4 : TEXT_THRESHOLD;
          if (v >= thr) byte |= 0x80 >> bit;
        }
        page[22 + y * stride + xb] = byte;
      }
    }
    return page;
  }

  function isCover(content) {
    const { out } = pictureTiles(wholePage(content));
    let n = 0;
    for (const v of out) n += v;
    return n / out.length >= 0.25;
  }

  // ---------------------------------------------------------------- cover page (xtccover.py)

  const MARGIN = 16;
  // a line may break before any character except Thai vowels/tone marks that follow a consonant
  const joins = (ch) => { const c = ch.codePointAt(0); return (c >= 0x0e31 && c <= 0x0e3a) || c === 0x0e45 || (c >= 0x0e47 && c <= 0x0e4e); };
  const leading = (ch) => { const c = ch.codePointAt(0); return c >= 0x0e40 && c <= 0x0e44; };

  function wrap(ctx, text, width, maxLines) {
    const fits = (s) => ctx.measureText(s).width <= width;
    const lines = [];
    let cur = '';
    for (const word of text.split(/\s+/).filter(Boolean)) {
      const cand = cur ? `${cur} ${word}` : word;
      if (fits(cand)) { cur = cand; continue; }
      if (cur) lines.push(cur);
      cur = '';
      const chars = Array.from(word);
      chars.forEach((ch, i) => {
        const tail = chars.slice(i + 1, i + 3).filter(joins).join('');
        if (cur && !joins(ch) && !leading(cur[cur.length - 1]) && !fits(cur + ch + tail)) {
          lines.push(cur);
          cur = '';
        }
        cur += ch;
      });
    }
    if (cur) lines.push(cur);
    if (lines.length > maxLines) {
      lines.length = maxLines;
      while (lines[maxLines - 1] && !fits(lines[maxLines - 1] + '…')) lines[maxLines - 1] = lines[maxLines - 1].slice(0, -1);
      lines[maxLines - 1] += '…';
    }
    return lines;
  }

  function textBlock(ctx, lines, size, top, gap) {
    let y = top;
    for (const line of lines) {
      ctx.fillText(line, W / 2, y);
      y += Math.round(size * 1.25) + gap;
    }
    return y;
  }

  // Floyd-Steinberg to black and white.
  function dither(g) {
    const e = new Float32Array(g.d), out = new Uint8Array(g.d.length);
    for (let y = 0; y < g.h; y++) {
      for (let x = 0; x < g.w; x++) {
        const i = y * g.w + x, v = e[i], q = v < 128 ? 0 : 255, err = v - q;
        out[i] = q;
        if (x + 1 < g.w) e[i + 1] += (err * 7) / 16;
        if (y + 1 < g.h) {
          if (x > 0) e[i + g.w - 1] += (err * 3) / 16;
          e[i + g.w] += (err * 5) / 16;
          if (x + 1 < g.w) e[i + g.w + 1] += err / 16;
        }
      }
    }
    return { w: g.w, h: g.h, d: out };
  }

  function coverPage(art, title, author, label) {
    const c = document.createElement('canvas');
    c.width = W;
    c.height = H;
    const ctx = c.getContext('2d');
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, W, H);
    ctx.fillStyle = ctx.strokeStyle = '#000';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    let band = 0, labelLines = [];
    if (label) {
      ctx.font = `26px ${COVER_FONT}`;
      labelLines = wrap(ctx, label, W - 2 * MARGIN, 2);
      band = labelLines.length * (Math.round(26 * 1.25) + 4) + 2 * MARGIN;
    }
    if (!art) {
      ctx.lineWidth = 3;
      ctx.strokeRect(MARGIN + 1.5, MARGIN + 1.5, W - 2 * MARGIN - 3, H - 2 * MARGIN - 3);
      ctx.lineWidth = 1;
      ctx.strokeRect(MARGIN + 8.5, MARGIN + 8.5, W - 2 * (MARGIN + 8) - 1, H - 2 * (MARGIN + 8) - 1);
      const inner = W - 2 * (MARGIN + 40), name = title || 'ไม่มีชื่อ';
      let size = 28, lines = [];
      for (const s of [56, 48, 40, 34, 28]) {
        ctx.font = `bold ${s}px ${COVER_FONT}`;
        size = s;
        lines = wrap(ctx, name, inner, 6);
        if (lines.length <= 4 && name.split(/\s+/).every((w) => ctx.measureText(w).width <= inner)) break;
      }
      let y = Math.max(150, 330 - Math.floor((lines.length * (Math.round(size * 1.25) + 8)) / 2));
      y = textBlock(ctx, lines, size, y, 8) + 24;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(W / 2 - 60, y);
      ctx.lineTo(W / 2 + 60, y);
      ctx.stroke();
      if (author) {
        ctx.font = `28px ${COVER_FONT}`;
        textBlock(ctx, wrap(ctx, author, inner, 2), 28, y + 28, 4);
      }
    }
    if (label) {
      ctx.font = `26px ${COVER_FONT}`;
      if (art) {
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(MARGIN, H - band + 4);
        ctx.lineTo(W - MARGIN - 1, H - band + 4);
        ctx.stroke();
        textBlock(ctx, labelLines, 26, H - band + MARGIN, 4);
      } else {
        textBlock(ctx, labelLines, 26, H - band - MARGIN - 8, 4);
      }
    }
    const page = fromCanvas(c);
    if (art) {
      const pic = dither(fit(art, W - 2 * MARGIN, H - 2 * MARGIN - band));
      paste(page, pic, Math.floor((W - pic.w) / 2), MARGIN + Math.floor((H - 2 * MARGIN - band - pic.h) / 2));
    }
    return page;
  }

  // ---------------------------------------------------------------- XTC files (xtc.py Writer)

  const utf8 = new TextEncoder();

  // NUL-padded UTF-8, never cutting a character in half.
  function fixed(text, size) {
    const out = new Uint8Array(size);
    let used = 0;
    for (const ch of Array.from(text || '')) {
      const b = utf8.encode(ch);
      if (used + b.length > size - 1) break;
      out.set(b, used);
      used += b.length;
    }
    return out;
  }

  function partLabel(chapters) {
    const found = [];
    for (const c of chapters) {
      const m = NUM.exec(c.name);
      if (m) found.push([m[1], Number(m[2].replace(/[๐-๙]/g, (d) => d.charCodeAt(0) - 0x0e50))]);
    }
    if (!found.length) return '';
    const nums = found.map(([, n]) => n), lo = Math.min(...nums), hi = Math.max(...nums);
    return lo === hi ? `${found[0][0]} ${lo}` : `${found[0][0]} ${lo}-${hi}`;
  }

  // One file: header, title, author, chapters, page table, then the cover (when given) and pages.
  function xtcFile(pages, title, author, chapters, cover) {
    if (cover) {
      chapters = chapters.map((c) => ({ name: c.name, first: c.first + 1, last: c.last + 1 }));
      pages = [cover, ...pages];
    }
    const n = pages.length;
    chapters = chapters.filter((c) => c.first >= 0 && c.first <= c.last && c.last < n).slice(0, 500);
    const META = 0x38, chapOff = chapters.length ? META + 192 : 0, tableOff = META + 192 + 96 * chapters.length;
    const dataOff = tableOff + 16 * n;
    const head = new Uint8Array(tableOff + 16 * n), dv = new DataView(head.buffer);
    head.set([0x58, 0x54, 0x43, 0x00]);           // "XTC\0"
    head[4] = 1;                                   // version 1.0
    dv.setUint16(6, n, true);
    head[9] = 1;                                   // has metadata
    head[11] = chapters.length ? 1 : 0;
    dv.setBigUint64(0x10, BigInt(META), true);
    dv.setBigUint64(0x18, BigInt(tableOff), true);
    dv.setBigUint64(0x20, BigInt(dataOff), true);
    dv.setUint32(0x30, chapOff, true);
    head.set(fixed(title, 128), META);
    head.set(fixed(author, 64), META + 128);
    chapters.forEach((c, i) => {
      const at = META + 192 + 96 * i;
      head.set(fixed(c.name, 80), at);
      dv.setUint16(at + 0x50, c.first + 1, true);
      dv.setUint16(at + 0x52, c.last + 1, true);
    });
    let off = dataOff;
    pages.forEach((p, i) => {
      const at = tableOff + 16 * i;
      dv.setBigUint64(at, BigInt(off), true);
      dv.setUint32(at + 8, p.length, true);
      dv.setUint16(at + 12, W, true);
      dv.setUint16(at + 14, H, true);
      off += p.length;
    });
    return new Blob([head, ...pages], { type: 'application/octet-stream' });
  }

  function saveParts(pages, stem, title, author, chapters, cover, pageLabel) {
    const n = pages.length, parts = Math.ceil(n / PER_PART), files = [];
    for (let k = 0; k < parts; k++) {
      const s = k * PER_PART, e = Math.min(n, (k + 1) * PER_PART);
      let mine = chapters.filter((c) => c.first < e && c.last >= s)
                         .map((c) => ({ name: c.name, first: Math.max(c.first, s) - s, last: Math.min(c.last, e - 1) - s }));
      if (parts > 1 && (!mine.length || mine[0].first > 0)) {
        const lead = chapters.filter((c) => c.first <= s).pop();
        mine.unshift({ name: lead ? lead.name : title, first: 0, last: mine.length ? mine[0].first - 1 : e - s - 1 });
      }
      const label = partLabel(mine) || pageLabel(s, e);
      if (mine.length > PART_CHAPTERS) {
        const step = mine.length / PART_CHAPTERS;
        const kept = Array.from({ length: PART_CHAPTERS }, (_, i) => mine[Math.floor(i * step)]);
        mine = kept.map((c, i) => ({ name: c.name, first: c.first, last: i + 1 < kept.length ? kept[i + 1].first - 1 : e - s - 1 }));
      }
      const num = String(k + 1).padStart(2, '0');
      const name = parts === 1 ? `${stem}.xtc` : `${stem} ${num}.xtc`;
      const shown = parts === 1 ? title : `${num}/${parts} ${label ? label + ' · ' : ''}${title}`;
      const onCover = parts === 1 ? '' : `${num}/${parts}${label ? ' · ' + label : ''}`;
      const coverBytes = cover ? encodePage(cover(onCover)) : null;
      files.push({ name, blob: xtcFile(pages.slice(s, e), shown, author, mine, coverBytes), pages: e - s + (cover ? 1 : 0) });
    }
    return files;
  }

  // ---------------------------------------------------------------- outline and text marks

  async function outline(pdf) {
    const out = [];
    try {
      const top = (await pdf.getOutline()) || [];
      let items = [];
      for (const item of top) {
        items.push([item, 0]);
        for (const sub of item.items || []) items.push([sub, 1]);
      }
      if (items.length > 3000) items = items.filter(([, level]) => level === 0);
      for (const [item, level] of items) {
        if (!item.title) continue;
        let dest = item.dest;
        if (typeof dest === 'string') dest = await pdf.getDestination(dest);
        if (!Array.isArray(dest) || !dest[0]) continue;
        const idx = typeof dest[0] === 'number' ? dest[0] : await pdf.getPageIndex(dest[0]);
        out.push(['  '.repeat(level) + item.title.trim().slice(0, 60), idx]);
      }
    } catch (e) {
      console.warn('[PdfXtc] outline:', e);          // a broken outline must not stop the conversion
    }
    return out;
  }

  async function textMark(page) {
    try {
      const tc = await page.getTextContent();
      let head = '';
      for (const it of tc.items) {
        head += (it.str || '') + (it.hasEOL ? '\n' : '');
        if (head.length >= 400) break;
      }
      const m = HEADING.exec(head.slice(0, 400));
      return m ? m[1].replace(/\s+/g, ' ').trim() : null;
    } catch (e) {
      return null;                                     // scanned PDFs have no text
    }
  }

  // ---------------------------------------------------------------- convert

  // opts: mode "auto" | "3" | "4" | "page", rotate "cw" | "ccw", trim, cover, margin, onStatus(text).
  // onProgress(pdfPage, pdfPages, devicePages); shouldStop() aborts. Returns [{name, blob, pages}].
  async function convert(file, opts = {}, onProgress = null, shouldStop = null) {
    const o = { mode: 'auto', rotate: 'cw', trim: true, cover: true, margin: 6, ...opts };
    const lib = await loadPdfjs({ onStatus: o.onStatus });
    const pdf = await lib.getDocument({ data: new Uint8Array(await file.arrayBuffer()) }).promise;
    try {
      const n = pdf.numPages;
      const info = ((await pdf.getMetadata().catch(() => null)) || {}).info || {};
      const stem = file.name.replace(/\.pdf$/i, '');
      const title = (info.Title || '').trim() || stem;
      const author = (info.Author || '').trim();
      let marks = (await outline(pdf)).filter(([, p]) => p >= 0 && p < n);
      const useText = marks.length === 0;
      const chapterPages = new Set(marks.map(([, p]) => p));
      const pages = [];
      let art = null;
      const flow = new Flow(o, (g) => pages.push(encodePage(g)));
      const firstOf = [];
      for (let i = 0; i < n; i++) {
        if (shouldStop && shouldStop()) throw new Error('Conversion cancelled');
        const page = await pdf.getPage(i + 1);
        let content;
        try {
          content = await pageContent(page, o);
          if (useText) {
            const mark = await textMark(page);
            if (mark) { marks.push([mark, i]); chapterPages.add(i); }
          }
        } finally {
          page.cleanup();
        }
        if (i === 0 && o.cover && content && isCover(content)) {
          art = content;                               // the cover of every file, not a page of text
          firstOf.push(0);
        } else if (o.mode === 'page') {
          firstOf.push(pages.length);
          if (content) pages.push(encodePage(wholePage(content)));
        } else {
          if (chapterPages.has(i)) flow.flush();       // a chapter starts on a fresh device page
          firstOf.push(content ? flow.add(stripSource(content, o)) : pages.length);
        }
        if (onProgress) onProgress(i + 1, n, pages.length);
        await new Promise((r) => setTimeout(r, 0));    // let the page repaint its progress bar
      }
      flow.flush();
      if (!pages.length && art) pages.push(encodePage(wholePage(art)));
      const total = pages.length;
      if (!total) throw new Error('ไม่พบเนื้อหาใน PDF นี้ (หน้าว่างทั้งหมด?)');
      for (let i = 0; i < firstOf.length; i++) firstOf[i] = Math.min(firstOf[i], total - 1);
      const chapters = marks.map(([t, p], k) => {
        const start = firstOf[p];
        const end = k + 1 < marks.length ? firstOf[marks[k + 1][1]] - 1 : total - 1;
        return { name: t, first: start, last: Math.max(start, end) };
      });
      if (!chapters.length && n > 1) {
        const every = Math.max(10, Math.ceil(n / MAX_CHAPTERS));
        for (let p = 0; p < n; p += every) {
          const end = p + every < n ? firstOf[p + every] - 1 : total - 1;
          chapters.push({ name: `หน้า ${p + 1}`, first: firstOf[p], last: Math.max(firstOf[p], end) });
        }
      }
      const pageLabel = (s, e) => {
        const shown = [];
        firstOf.forEach((f, p) => { if (s <= f && f < e) shown.push(p); });
        if (!shown.length) shown.push(0);
        return `หน้า ${shown[0] + 1}-${shown[shown.length - 1] + 1}`;
      };
      const cover = o.cover ? (label) => coverPage(art, title, author, label) : null;
      return saveParts(pages, stem, title, author, chapters, cover, pageLabel);
    } finally {
      pdf.destroy();
    }
  }

  window.PdfXtc = {
    isPdf: (f) => /\.pdf$/i.test(f.name),
    convert,
    loadPdfjs,
    PDFJS_VERSION,
  };
})();
