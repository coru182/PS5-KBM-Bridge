#!/usr/bin/env bash
# Create (or update) the GitHub Releases v0.8 … v0.12 with their changelog
# (English + Spanish) and the ELFs/ZIP found on your PC. Run it in WSL:
#
#   tools/publish_releases.sh [folder with the builds]     (default /mnt/c/Users/Public/PS5)
#
# Needs the GitHub CLI logged in to your account:  sudo apt install gh && gh auth login
# Searches the folder (and subfolders) for ps5-kbm-bridge-vXXX.elf, ps5-kbm-diag-vXXX.elf
# and PS5-KBM-Bridge-vX.Y.zip. A release that already exists gets its title/notes
# updated and any missing files added. Safe to run again.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="coru182/PS5-KBM-Bridge"
SRC="${1:-/mnt/c/Users/Public/PS5}"
NOTES="$ROOT/out/release-notes"
DRY="${DRY_RUN:-0}"        # DRY_RUN=1: only show what it would do

# version  commit (the repo history starts at v0.8)
VERSIONS="
v0.8    a3f1a609149c07a122e8b3dbcc6be3e8f7a95802
v0.9    fdf75748998f695e386b2a254299cdacac371ff3
v0.9.1  30ede3571ddad2005a5bcfc66803c4d86ed0bc05
v0.9.2  2d1cfed44cd3363e35563537d6daf6c7247cbed7
v0.10   19b8efa3f345e9ab91b1931a34a5c105440e1982
v0.10.1 ed0fb3fde55457f7151571ea7efd1e480187217a
v0.10.2 a21a0eb7af39d007fb7e586138b8be47a1f43f0b
v0.10.3 d62cf1ac625712ae312b36e88726f302eb501d77
v0.10.4 48488fab1b1e27a776b4012718aeb68985665bc2
v0.11   c54e24bc6466b35645f0893c8251326bc6ae9c9c
v0.12   1fa7277589df3b7a287b53d73017e6b330c973d1
"

if [[ "$DRY" != "1" ]]; then
  command -v gh >/dev/null || { echo "Falta GitHub CLI: sudo apt install gh  y despues: gh auth login" >&2; exit 1; }
  gh auth status >/dev/null 2>&1 || { echo "No estas logueado: gh auth login" >&2; exit 1; }
fi
[[ -d "$SRC" ]] || { echo "No existe la carpeta $SRC" >&2; exit 1; }

python3 "$ROOT/tools/release_notes.py" "$NOTES" $(echo "$VERSIONS" | awk 'NF{print $1}') >/dev/null

run() { if [[ "$DRY" == "1" ]]; then printf '  (dry)'; printf ' %q' "$@"; echo; else "$@"; fi; }

echo "$VERSIONS" | while read -r ver commit; do
  [[ -z "$ver" ]] && continue
  tag="v$(echo "${ver#v}" | tr -d .)"                     # v0.10.4 -> v0104
  files=(); declare -A seen=()
  while IFS= read -r -d '' f; do
    b="$(basename "$f")"
    [[ -n "${seen[$b]:-}" ]] && continue                  # same file copied to several folders
    seen[$b]=1; files+=("$f")
  done < <(find "$SRC" -type f \( -name "ps5-kbm-bridge-$tag.elf" -o -name "ps5-kbm-diag-$tag.elf" -o -name "PS5-KBM-Bridge-$ver.zip" \) -print0 2>/dev/null | sort -z)
  unset seen
  title="$(head -n1 "$NOTES/$ver.md")"
  echo
  echo "== $title"
  if [[ ${#files[@]} -eq 0 ]]; then echo "   (sin archivos para $ver en $SRC: solo notas)"; else printf '   + %s\n' "${files[@]##*/}"; fi
  if [[ "$DRY" != "1" ]] && gh release view "$ver" -R "$REPO" >/dev/null 2>&1; then
    run gh release edit "$ver" -R "$REPO" --title "$title" --notes-file "$NOTES/$ver.md"
    [[ ${#files[@]} -gt 0 ]] && run gh release upload "$ver" -R "$REPO" --clobber "${files[@]}"
  else
    run gh release create "$ver" -R "$REPO" --target "$commit" --title "$title" --notes-file "$NOTES/$ver.md" "${files[@]}"
  fi
done
echo
echo "Listo: https://github.com/$REPO/releases"
