#!/usr/bin/env python3
"""Copy researched game profiles into data/.

  python3 tools/import_research.py <dir-with-json>

Each <id>.json is validated with the same rules as gen_builtin.py.
confidence "low" files go to data/unverified/ (NOT built in: those games use
their genre template until someone checks the bindings in game).
"""
import json, shutil, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_builtin as g  # noqa: E402

src = Path(sys.argv[1])
dst_ok, dst_low = g.DATA / "profiles", g.DATA / "unverified"
dst_low.mkdir(exist_ok=True)
for f in sorted(src.glob("[a-z]*.json")):
    d = json.loads(f.read_text(encoding="utf-8"))
    g.load_profile(f, 0)                      # dies on any invalid key/button/size
    low = d.get("confidence") == "low"
    dst = (dst_low if low else dst_ok) / f.name
    other = (dst_ok if low else dst_low) / f.name
    if other.exists():
        other.unlink()
    shutil.copyfile(f, dst)
    print(f"{'UNVERIFIED' if low else 'ok':10s} {d['id']:24s} {len(d['actions']):2d} actions  conf={d.get('confidence')}  native={d.get('native_kbm_ps5')}")
