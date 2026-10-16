#!/usr/bin/env python3
"""Compare Thai line-break candidates against reference word segmentations.

Each breaker is a command that reads UTF-8 lines on stdin and writes them back
with '|' at every break opportunity (test/thai/thai_seg_cli.cpp is ours).
References come from PyThaiNLP: `newmm` (dictionary maximal matching, shares
our word list, so it flatters us) and `deepcut` (a CNN trained on the BEST
corpus, independent of any dictionary).

For line breaking, *precision* matters most: a break the reference does not
have is a word cut in half at a line end. Recall measures how many word
boundaries are offered (finer tokens wrap and justify more evenly).

Usage:
  python scripts/thai/eval_segmentation.py BOOK.epub [--lines N] \
      --breaker ours=build/test/thai/thai_seg_cli.exe --breaker other=path/to/cli
"""
from __future__ import annotations

import argparse
import html
import random
import shlex
import re
import subprocess
import time
import zipfile

THAI = re.compile(r"[ก-๛]")


def paragraphs(epub: str) -> list[str]:
    out = []
    with zipfile.ZipFile(epub) as z:
        for name in z.namelist():
            if not name.endswith((".xhtml", ".html", ".htm")):
                continue
            doc = z.read(name).decode("utf-8", "ignore")
            for p in re.findall(r"<p\b[^>]*>(.*?)</p>", doc, flags=re.S):
                text = html.unescape(re.sub(r"<[^>]+>", "", p)).strip()
                text = re.sub(r"\s+", " ", text)
                if len(THAI.findall(text)) >= 20:
                    out.append(text)
    return out


def boundaries(tokens: list[str]) -> set[int]:
    """Character offsets between adjacent tokens, ignoring whitespace tokens."""
    pos, out = 0, set()
    for t in tokens:
        pos += len(t)
        out.add(pos)
    return out


def thai_internal(text: str, offsets: set[int]) -> set[int]:
    """Keep only offsets with Thai letters on both sides (word-internal decisions)."""
    return {o for o in offsets if 0 < o < len(text) and THAI.match(text[o - 1]) and THAI.match(text[o])}


def run_breaker(cmd: str, lines: list[str]) -> tuple[list[set[int]], float]:
    data = ("\n".join(lines) + "\n").encode("utf-8")
    t0 = time.perf_counter()
    out = subprocess.run(shlex.split(cmd), input=data, capture_output=True, check=True).stdout.decode("utf-8")
    dt = time.perf_counter() - t0
    result = []
    for line, seg in zip(lines, out.splitlines()):
        assert seg.replace("|", "") == line, "breaker changed the text"
        result.append(thai_internal(line, boundaries(seg.split("|"))))
    return result, dt


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("epub", nargs="+")
    ap.add_argument("--lines", type=int, default=1500)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--breaker", action="append", required=True, help="name=command")
    ap.add_argument("--refs", default="newmm,deepcut")
    a = ap.parse_args()

    from pythainlp.tokenize import word_tokenize

    paras = [p for e in a.epub for p in paragraphs(e)]
    random.Random(a.seed).shuffle(paras)
    lines = paras[: a.lines]
    chars = sum(len(l) for l in lines)
    print(f"{len(lines)} paragraphs, {chars} characters from {len(paras)} candidates")

    refs = {}
    for engine in a.refs.split(","):
        t0 = time.perf_counter()
        refs[engine] = [thai_internal(l, boundaries(word_tokenize(l, engine=engine, keep_whitespace=True)))
                        for l in lines]
        print(f"reference {engine}: {sum(map(len, refs[engine]))} Thai-Thai boundaries "
              f"({time.perf_counter() - t0:.1f}s)")

    print()
    header = f"{'breaker':<14}{'breaks':>8}{'ms':>8}"
    for engine in refs:
        header += f"{engine + ' P':>12}{engine + ' R':>12}"
    print(header)
    for spec in a.breaker:
        name, cmd = spec.split("=", 1)
        got, dt = run_breaker(cmd, lines)
        row = f"{name:<14}{sum(map(len, got)):>8}{dt * 1000:>8.0f}"
        for engine, ref in refs.items():
            tp = sum(len(g & r) for g, r in zip(got, ref))
            nb = sum(map(len, got))
            nr = sum(map(len, ref))
            row += f"{100 * tp / max(nb, 1):>11.1f}%{100 * tp / max(nr, 1):>11.1f}%"
        print(row)


if __name__ == "__main__":
    main()
