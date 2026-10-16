#!/usr/bin/env python3
"""Generate lib/Thai/ThaiDictData.h: a minimized Thai word DAWG for line breaking.

Sources (all pure-Thai, whitespace-free entries are kept):
  * PyThaiNLP words_th.txt (CC0-1.0) -- the main word list, ~62k words.
    https://github.com/PyThaiNLP/pythainlp/blob/dev/pythainlp/corpus/words_th.txt
  * lib/Thai/data/thai_words_extra.txt -- this fork's own additions (loanwords,
    technical terms); one word per line, '#' starts a comment.

Layout (byte addressed, little endian), identical node format to the trie used by
JTIAPBNAI/CrosspointTH, but identical subtrees are shared (DAWG), which roughly
halves the size:
  uint8_t flags    bit7 = a word may end here, bits 0-6 = edge count (<= 127)
  edges[]          4 bytes each: { uint8_t label (codepoint - U+0E00),
                                   uint24 absolute child offset }, sorted by label
The root node is at offset 0. The array is constexpr, so it stays in flash.

Usage:
  python scripts/thai/gen_thai_dict.py [--words-th PATH] [--extra PATH] [--out PATH]
If --words-th is omitted, the file is taken from an installed `pythainlp`
package, or downloaded from GitHub.
"""
from __future__ import annotations

import argparse
import pathlib
import sys
import urllib.request

WORDS_TH_URL = "https://raw.githubusercontent.com/PyThaiNLP/pythainlp/dev/pythainlp/corpus/words_th.txt"
REPO = pathlib.Path(__file__).resolve().parents[2]


def is_thai_word(w: str) -> bool:
    # Single letters ("ก", "ข" ...) are list entries for the alphabet, not
    # words; as segments they only shred unknown names into fragments.
    return len(w) >= 2 and all(0x0E01 <= ord(c) <= 0x0E5B and ord(c) != 0x0E3F for c in w)


def read_lines(path: pathlib.Path) -> list[str]:
    out = []
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            out.append(line)
    return out


def words_th_source(arg: str | None) -> list[str]:
    if arg:
        return read_lines(pathlib.Path(arg))
    try:
        import pythainlp  # type: ignore

        p = pathlib.Path(pythainlp.__file__).parent / "corpus" / "words_th.txt"
        if p.exists():
            return read_lines(p)
    except ImportError:
        pass
    with urllib.request.urlopen(WORDS_TH_URL) as r:
        return [l.strip() for l in r.read().decode("utf-8-sig").splitlines() if l.strip()]


# 12, not 8: at 8 everyday words such as ประโยชน์ (ประ+โยชน์) were dropped and cut at
# line ends (deepcut precision 93.5% -> 95.2%, recall 88% -> 82%; +130 KB flash).
LONG_WORD = 12


def decomposable(word: str, vocab: set[str]) -> bool:
    """True if `word` splits completely into >= 2 entries of `vocab`."""
    ok = [False] * (len(word) + 1)
    ok[0] = True
    for i in range(len(word)):
        if not ok[i]:
            continue
        for j in range(i + 2, len(word) + 1):
            if (i, j) != (0, len(word)) and word[i:j] in vocab:
                ok[j] = True
    return ok[len(word)]


class Node:
    __slots__ = ("children", "terminal", "key", "offset")

    def __init__(self):
        self.children: dict[int, Node] = {}
        self.terminal = False
        self.key = None
        self.offset = -1


def build(words: list[str]) -> Node:
    root = Node()
    for w in words:
        n = root
        for c in w:
            n = n.children.setdefault(ord(c) - 0x0E00, Node())
        n.terminal = True
    return root


def minimize(root: Node) -> list[Node]:
    """Share identical subtrees; returns unique nodes, children before parents."""
    registry: dict[tuple, Node] = {}
    unique: list[Node] = []

    def visit(n: Node) -> Node:
        for label in list(n.children):
            n.children[label] = visit(n.children[label])
        key = (n.terminal, tuple((l, id(c)) for l, c in sorted(n.children.items())))
        found = registry.get(key)
        if found is not None:
            return found
        registry[key] = n
        unique.append(n)
        return n

    sys.setrecursionlimit(10000)
    canonical_root = visit(root)
    assert canonical_root is root
    return unique


def serialize(root: Node, unique: list[Node]) -> bytes:
    # Root first so the runtime starts at offset 0; the rest in any order.
    order = [root] + [n for n in unique if n is not root]
    off = 0
    for n in order:
        n.offset = off
        off += 1 + 4 * len(n.children)
    assert off < (1 << 24), "DAWG exceeds 24-bit offsets"
    blob = bytearray(off)
    for n in order:
        p = n.offset
        assert len(n.children) <= 127
        blob[p] = len(n.children) | (0x80 if n.terminal else 0)
        p += 1
        for label in sorted(n.children):
            c = n.children[label]
            blob[p] = label
            blob[p + 1] = c.offset & 0xFF
            blob[p + 2] = (c.offset >> 8) & 0xFF
            blob[p + 3] = (c.offset >> 16) & 0xFF
            p += 4
    return bytes(blob)


def emit(blob: bytes, count: int, sources: str, out: pathlib.Path) -> None:
    with out.open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("// Auto-generated by scripts/thai/gen_thai_dict.py -- do not edit.\n")
        fh.write(f"// Sources: {sources}\n")
        fh.write("// PyThaiNLP words_th.txt is CC0-1.0; see lib/Thai/README.md.\n")
        fh.write(f"// {count} words, {len(blob)} bytes. Flash-resident (constexpr rodata).\n")
        fh.write("#pragma once\n\n#include <cstddef>\n#include <cstdint>\n\nnamespace thaiDict {\n\n")
        fh.write(f"constexpr size_t WORD_COUNT = {count};\n")
        fh.write(f"constexpr uint32_t DAWG_SIZE = {len(blob)};\n\n")
        fh.write("constexpr uint8_t DAWG[] = {\n")
        for i in range(0, len(blob), 20):
            fh.write("    " + ",".join(f"0x{b:02X}" for b in blob[i : i + 20]) + ",\n")
        fh.write("};\n\n}  // namespace thaiDict\n")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--words-th")
    ap.add_argument("--extra", default=str(REPO / "lib/Thai/data/thai_words_extra.txt"))
    ap.add_argument("--out", default=str(REPO / "lib/Thai/ThaiDictData.h"))
    a = ap.parse_args()

    base = {w for w in words_th_source(a.words_th) if is_thai_word(w)}
    extra = set()
    if pathlib.Path(a.extra).exists():
        extra = {w for w in read_lines(pathlib.Path(a.extra)) if is_thai_word(w)}
    words = base | extra
    # Very long entries are mostly multi-word names and phrases
    # ("ธนาคารแห่งประเทศไทย"). As a single segment they become one unbreakable
    # line-break token; drop them when other entries cover them completely so
    # the breaker can still wrap between the parts.
    long_words = {w for w in words if len(w) >= LONG_WORD and w not in extra}
    shorter = words - long_words
    dropped = {w for w in long_words if decomposable(w, shorter)}
    words = sorted(words - dropped)
    print(f"dropped {len(dropped)} decomposable entries of >= {LONG_WORD} codepoints")
    root = build(words)
    unique = minimize(root)
    blob = serialize(root, unique)
    emit(blob, len(words), f"PyThaiNLP words_th.txt ({len(base)}), thai_words_extra.txt ({len(extra - base)} new)",
         pathlib.Path(a.out))
    print(f"{len(words)} words, {len(unique)} nodes -> {len(blob)} bytes -> {a.out}")


if __name__ == "__main__":
    main()
