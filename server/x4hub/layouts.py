"""Dashboard layouts: Jinja2 HTML templates, one file each.

Built-in layouts ship in x4hub/layouts/; edited and new ones live in
DATA_DIR/layouts/ (a saved copy of a built-in shadows it; deleting the copy
brings the original back). The active layout's name is in DATA_DIR/layouts/active.
"""
from __future__ import annotations

import re
from pathlib import Path

from jinja2.sandbox import SandboxedEnvironment

from .stats import DATA

BUILTIN = Path(__file__).parent / "layouts"
NAME = re.compile(r"^[a-z0-9][a-z0-9-]{0,39}$")
_env = SandboxedEnvironment(autoescape=True)


def _user() -> Path:
    d = DATA / "layouts"
    d.mkdir(parents=True, exist_ok=True)
    return d


def valid(name: str) -> bool:
    return bool(NAME.match(name or ""))


def names() -> list[dict]:
    builtin = {p.stem for p in BUILTIN.glob("*.html")}
    user = {p.stem for p in _user().glob("*.html")}
    active = get_active()
    return [{"name": n, "builtin": n in builtin, "edited": n in builtin and n in user, "active": n == active}
            for n in sorted(builtin | user)]


def read(name: str) -> str | None:
    if not valid(name):
        return None
    for d in (_user(), BUILTIN):
        p = d / f"{name}.html"
        if p.exists():
            return p.read_text(encoding="utf-8")
    return None


def write(name: str, html: str) -> None:
    if not valid(name):
        raise ValueError("name: a-z, 0-9 and -, up to 40")
    compile_template(html)  # refuse templates that do not parse
    (_user() / f"{name}.html").write_text(html, encoding="utf-8")


def delete(name: str) -> bool:
    p = _user() / f"{name}.html"
    if not valid(name) or not p.exists():
        return False
    p.unlink()
    if get_active() == name and read(name) is None:
        set_active("default")
    return True


def get_active() -> str:
    p = _user() / "active"
    name = p.read_text(encoding="utf-8").strip() if p.exists() else "default"
    return name if read(name) is not None else "default"


def set_active(name: str) -> None:
    if read(name) is None:
        raise ValueError("no such layout")
    (_user() / "active").write_text(name, encoding="utf-8")


def compile_template(html: str):
    return _env.from_string(html)


def render_html(html: str, ctx: dict) -> str:
    return compile_template(html).render(**ctx)
