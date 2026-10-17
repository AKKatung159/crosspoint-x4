# Debian 12 pinned so the library names below stay valid.
FROM python:3.12-slim-bookworm

ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 DATA_DIR=/data TZ=Asia/Bangkok \
    PLAYWRIGHT_BROWSERS_PATH=/ms-playwright
WORKDIR /srv

# Only what Playwright's headless shell needs (no X server, no font packages: layouts get Sarabun
# embedded, DejaVu covers the rest), plus libraqm for Thai in the Pillow fallback renderer.
# Kept small on purpose: the full `playwright install --with-deps` image was 2.2 GB.
RUN apt-get update && apt-get install -y --no-install-recommends \
      libraqm0 libnss3 libnspr4 libatk1.0-0 libatk-bridge2.0-0 libcups2 libdrm2 libxkbcommon0 \
      libxcomposite1 libxdamage1 libxfixes3 libxrandr2 libgbm1 libasound2 libpango-1.0-0 libcairo2 \
      libdbus-1-3 libxshmfence1 fonts-dejavu-core \
 && rm -rf /var/lib/apt/lists/*
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt \
 && python -m playwright install --only-shell chromium \
 && rm -rf /root/.cache

COPY x4hub ./x4hub
RUN useradd --uid 1000 --create-home app && mkdir -p /data && chown app /data
USER app
VOLUME /data
EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=5s --start-period=20s \
  CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/health', timeout=4)"

CMD ["uvicorn", "x4hub.app:app", "--host", "0.0.0.0", "--port", "8000"]
