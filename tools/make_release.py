#!/usr/bin/env python3
"""Pack out/ into out/release/PS5-KBM-Bridge-v<ver>.zip (ready-to-use download).

  python3 tools/make_release.py 0.12
"""
import hashlib, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out"
ver = sys.argv[1]
tag = ver.replace(".", "")          # 0.12 -> 012, 0.10.4 -> 0104 (same as the ELF names)
name = f"PS5-KBM-Bridge-v{ver}"
files = [f"ps5-kbm-bridge-v{tag}.elf", f"ps5-kbm-diag-v{tag}.elf",
         "etaHEN/payloads/ps5-kbm-bridge.elf", "etaHEN/payloads/ps5-kbm-bridge.elf.auto_start"]
for f in files:
    if not (OUT / f).exists():
        sys.exit(f"make_release: missing out/{f} (run build_wsl.sh first)")
launcher = sorted(p for p in (OUT / "homebrew").rglob("*") if p.is_file()) if (OUT / "homebrew").is_dir() else []
(OUT / "release").mkdir(exist_ok=True)
zpath = OUT / "release" / f"{name}.zip"
readme = (ROOT / "release" / "LEEME-README.txt").read_text(encoding="utf-8").replace("{VERSION}", ver).replace("{TAG}", tag)
sums = []
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for rel in files + [str(p.relative_to(OUT)) for p in launcher]:
        data = (OUT / rel).read_bytes()
        z.writestr(f"{name}/{rel}", data)
        sums.append(f"{hashlib.sha256(data).hexdigest()}  {rel}")
    z.writestr(f"{name}/LEEME-README.txt", readme)
    z.writestr(f"{name}/SHA256SUMS.txt", "\n".join(sums) + "\n")
    for doc in ("LICENSE", "CREDITS.md", "CHANGELOG.md"):
        z.writestr(f"{name}/{doc}", (ROOT / doc).read_bytes())
print(f"release: {zpath.relative_to(ROOT)} ({zpath.stat().st_size // 1024} KB, icon {'included' if launcher else 'NOT included'})")
