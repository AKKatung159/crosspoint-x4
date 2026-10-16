#!/usr/bin/env python3
"""Preview the X4's web pages on the PC, without a device.

Serves src/network/html/*.html with the personal theme put in exactly as
scripts/build_html.py does, and answers the read-only APIs from the captured
fixtures in scripts/personal/web_fixtures/ (writes just answer OK).

  python scripts/personal/web_preview.py [port]      # then open http://localhost:8765/
"""
from __future__ import annotations

import http.server
import json
import sys
from pathlib import Path
from urllib.parse import urlparse

REPO = Path(__file__).resolve().parents[2]
HTML = REPO / "src" / "network" / "html"
FIX = Path(__file__).resolve().parent / "web_fixtures"
PAGES = {"/": "HomePage.html", "/files": "FilesPage.html", "/settings": "SettingsPage.html", "/fonts": "FontsPage.html"}
API = {"/api/status": "status", "/api/settings": "settings", "/api/files": "files", "/api/fonts": "fonts",
       "/api/opds": "opds", "/api/wifi": "wifi"}


def page(name: str) -> bytes:
    content = (HTML / name).read_text(encoding="utf-8")
    css = (HTML / "personal" / "theme.css").read_text(encoding="utf-8")
    js = (HTML / "personal" / "theme.js").read_text(encoding="utf-8")
    return content.replace("<!-- PERSONAL:THEME -->", f"<style>{css}</style><script>{js}</script>").encode("utf-8")


class Handler(http.server.BaseHTTPRequestHandler):
    def _send(self, code: int, body: bytes, ctype: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):  # noqa: N802
        path = urlparse(self.path).path
        if path in PAGES:
            return self._send(200, page(PAGES[path]), "text/html; charset=utf-8")
        if path in API and (FIX / f"{API[path]}.json").exists():
            return self._send(200, (FIX / f"{API[path]}.json").read_bytes(), "application/json")
        if path == "/js/jszip.min.js":
            return self._send(200, (HTML / "js" / "jszip.min.js").read_bytes(), "application/javascript")
        return self._send(404, b"not found", "text/plain")

    def do_POST(self):  # noqa: N802
        self.rfile.read(int(self.headers.get("Content-Length") or 0))
        return self._send(200, json.dumps({"ok": True}).encode(), "application/json")

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print(f"http://localhost:{port}/")
    http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
