#!/usr/bin/env python3
"""Release notes (English + Spanish) for each version, from CHANGELOG.md / CHANGELOG.es.md.

  python3 tools/release_notes.py OUT_DIR [VERSION...]     -> OUT_DIR/v0.12.md, ...
First line of each file = release title ("v0.12: ...").
"""
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = "https://github.com/coru182/PS5-KBM-Bridge"


def sections(path):
    out, cur = {}, None
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"## (v[0-9][0-9.]*)(:.*)?$", line)
        if m:
            cur = m.group(1)
            out[cur] = {"title": line[3:].strip(), "body": []}
        elif line.startswith("## "):
            cur = None
        elif cur:
            out[cur]["body"].append(line)
    return out


def absolute_links(text, ver):
    # relative repo links break on the Releases page: point them at this version's files
    return re.sub(r"\]\((?!https?://|#)([^)]+)\)", lambda m: f"]({REPO}/blob/{ver}/{m.group(1)})", text)


def notes(ver, en, es):
    tag = "v" + ver[1:].replace(".", "")
    e, s = en[ver], es.get(ver)
    parts = [e["title"], "", "\n".join(e["body"]).strip(), ""]
    if s:
        parts += ["---", "", f"### Español: {s['title'].split(':', 1)[1].strip() if ':' in s['title'] else s['title']}", "",
                  "\n".join(s["body"]).strip(), ""]
    parts += ["---", "",
              "**Files / Archivos**", "",
              f"- `PS5-KBM-Bridge-{ver}.zip` (if attached / si está): everything below + install notes / todo lo de abajo + instrucciones.",
              f"- `ps5-kbm-bridge-{tag}.elf`: the bridge / el bridge (elfldr, port/puerto 9021).",
              f"- `ps5-kbm-diag-{tag}.elf`: diagnostics, never creates the virtual controller / diagnóstico, nunca crea el mando virtual.",
              "",
              f"Install / Instalación: [README]({REPO}/blob/{ver}/README.md) · [Español]({REPO}/blob/{ver}/README.es.md). "
              "Experimental, offline play only / experimental, solo para jugar offline."]
    return absolute_links("\n".join(parts).rstrip() + "\n", ver)


def main():
    out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
    en, es = sections(ROOT / "CHANGELOG.md"), sections(ROOT / "CHANGELOG.es.md")
    for ver in sys.argv[2:] or list(en):
        if ver not in en:
            sys.exit(f"release_notes: {ver} not in CHANGELOG.md")
        (out / f"{ver}.md").write_text(notes(ver, en, es), encoding="utf-8")
        print(f"{out / (ver + '.md')}")


main()
