FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 DATA_DIR=/data TZ=Asia/Bangkok
WORKDIR /srv

# libraqm: Thai shaping (stacked vowels and tone marks) in Pillow.
RUN apt-get update && apt-get install -y --no-install-recommends libraqm0 && rm -rf /var/lib/apt/lists/*
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY x4hub ./x4hub
RUN useradd --uid 1000 --create-home app && mkdir -p /data && chown app /data
USER app
VOLUME /data
EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s \
  CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/health', timeout=4)"

CMD ["uvicorn", "x4hub.app:app", "--host", "0.0.0.0", "--port", "8000"]
