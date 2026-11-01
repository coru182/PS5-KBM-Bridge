#!/usr/bin/env python3
"""Generate src/kbm_builtin_gen.{c,h}: built-in profiles + PS5/PS4 title database.

Inputs (all in data/):
  profiles/*.json   per-game profiles (PC default keys -> PS5 buttons), researched
  templates/*.json  genre templates (fps, tps, melee, racing, generic)
  games.json        name patterns -> profile / template
  native.json       name patterns of games whose PS5 version supports KB+M natively
  titledb/PS5_Titles.tsv, PS4_Titles.tsv   andshrew/PlayStation-Titles (MIT)

Validates everything (keys, buttons, sizes, duplicates) and fails loudly.
"""
import csv, json, re, sys, unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
MAX_ACTIONS = 48
AUTO_ACTIONS = 5          # 4 movement + PS button, added to every profile
LABEL_MAX = 47            # bytes, KbmAction.label is 48

BTN = {"L3": 0x2, "R3": 0x4, "OPTIONS": 0x8, "DPAD_UP": 0x10, "DPAD_RIGHT": 0x20, "DPAD_DOWN": 0x40,
       "DPAD_LEFT": 0x80, "L2": 0x100, "R2": 0x200, "L1": 0x400, "R1": 0x800, "TRIANGLE": 0x1000,
       "CIRCLE": 0x2000, "CROSS": 0x4000, "SQUARE": 0x8000, "PS": 0x10000, "TOUCHPAD": 0x100000,
       "LS_UP": 1 << 32, "LS_DOWN": 1 << 33, "LS_LEFT": 1 << 34, "LS_RIGHT": 1 << 35,
       "MODE_TOGGLE": 1 << 36, "NONE": 0}
RESERVED = {"F8", "F9", "F10", "F11", "F12"}


def die(msg):
    sys.exit(f"gen_builtin: {msg}")


def key_table():
    src = (ROOT / "src" / "kbm_keys.c").read_text()
    out = {}
    for code, name in re.findall(r'\{(0x[0-9a-f]+|KBM_KEY_\w+),"([A-Z0-9_]+)"\}', src):
        if code.startswith("0x"):
            out[name] = int(code, 16)
    virt = {"MOUSE1": 0xF0, "MOUSE2": 0xF1, "MOUSE3": 0xF2, "MOUSE4": 0xF3, "MOUSE5": 0xF4,
            "WHEEL_UP": 0xF8, "WHEEL_DOWN": 0xF9}
    out.update(virt)
    return out


KEYS = key_table()
I18N = json.loads((DATA / "i18n" / "en.json").read_text(encoding="utf-8"))


def norm(s):
    s = s.replace("™", "").replace("®", "").replace("©", "").replace("℠", "")   # before NFKC (™ -> "TM")
    s = unicodedata.normalize("NFKC", s)
    s = "".join(c for c in unicodedata.normalize("NFKD", s) if not unicodedata.combining(c))
    s = s.lower().replace("'", "").replace("’", "").replace("`", "")
    s = re.sub(r"[^a-z0-9]+", " ", s)
    return re.sub(r"\s+", " ", s).strip()


def slug(label, used):
    s = norm(label).replace(" ", "_")[:13] or "a"
    base, i = s, 2
    while s in used:
        s = f"{base[:11]}{i}"
        i += 1
    used.add(s)
    return s


def c_str(s):
    return '"' + "".join(c if (32 <= ord(c) < 127 and c not in '"\\') else
                         "".join(f"\\{b:03o}" for b in c.encode("utf-8")) for c in s) + '"'


def out_mask(spec, where):
    m = 0
    for part in spec.split("+"):
        part = part.strip()
        if part not in BTN:
            die(f"{where}: unknown PS5 output '{part}'")
        m |= BTN[part]
    return m


def key_code(name, where):
    if name in ("", None, "NONE"):
        return 0
    if name in RESERVED:
        die(f"{where}: key {name} is reserved by the bridge")
    if name not in KEYS:
        die(f"{where}: unknown key '{name}'")
    return KEYS[name]


LAYERS = {"foot": 0, "vehicle": 1, "both": 2}


def load_profile(path, kind):
    """Actions become (id, label, out_mask, key1, key2, layer).
    layer: 0 on foot (default), 1 vehicle, 2 both. Movement and the PS button
    are added automatically as 'both'."""
    d = json.loads(path.read_text(encoding="utf-8"))
    pid = d.get("id") or die(f"{path}: missing id")
    if not re.fullmatch(r"[a-z0-9_]{1,23}", pid):
        die(f"{path}: id '{pid}' must be [a-z0-9_]{{1,23}}")
    acts = d.get("actions") or die(f"{path}: no actions")
    auto = 0 if d.get("no_auto_move") else AUTO_ACTIONS
    if len(acts) + auto > MAX_ACTIONS:
        die(f"{path}: {len(acts)} actions + {auto} automatic > {MAX_ACTIONS}")
    used, out = set(), []
    if not d.get("no_auto_move"):
        for aid, lab, o, k1, k2 in [("move_up", "Avanzar", "LS_UP", "W", "UP"), ("move_down", "Retroceder", "LS_DOWN", "S", "DOWN"),
                                    ("move_left", "Izquierda", "LS_LEFT", "A", "LEFT"), ("move_right", "Derecha", "LS_RIGHT", "D", "RIGHT")]:
            used.add(aid)
            out.append((aid, lab, BTN[o], KEYS[k1], KEYS[k2], 2))
    for i, a in enumerate(acts):
        where = f"{path.name} action {i}"
        lab = (a.get("label") or "").strip() or die(f"{where}: empty label")
        if len(lab.encode("utf-8")) > LABEL_MAX:
            lab = lab.encode("utf-8")[:LABEL_MAX].decode("utf-8", "ignore").rstrip()
        pc = (a.get("pc") or []) + ["", ""]
        aid = a.get("id")
        if aid:
            if not re.fullmatch(r"[a-z0-9_]{1,23}", aid) or aid in used:
                die(f"{where}: bad or duplicate action id '{aid}'")
            used.add(aid)
        else:
            aid = slug(lab, used)
        layer = LAYERS.get(a.get("layer", "foot"))
        if layer is None:
            die(f"{where}: layer must be foot, vehicle or both")
        out.append((aid, lab, out_mask(a.get("ps5", ""), where), key_code(pc[0], where), key_code(pc[1], where), layer))
    # one key -> one action per layer ('both' counts for each layer)
    for L in (0, 1):
        seen = {}
        for aid, _, _, k1, k2, lay in out:
            if lay not in (L, 2) or aid.startswith("move_"):
                continue
            for k in (k1, k2):
                if k and seen.setdefault(k, aid) != aid:
                    die(f"{path}: key 0x{k:02x} used by '{seen[k]}' and '{aid}' in the same layer")
    # arrows are a secondary movement key, unless the PC version uses them on foot for something else
    taken = {k for (aid, _, _, k1, k2, lay) in out if not aid.startswith("move_") and lay != 1 for k in (k1, k2) if k}
    out = [(aid, lab, m, k1, 0 if (aid.startswith("move_") and k2 in taken) else k2, lay) for (aid, lab, m, k1, k2, lay) in out]
    if any(k in taken for k in ((KEYS["LGUI"], KEYS["RGUI"]) + (() if d.get("no_auto_move") else (KEYS["W"], KEYS["A"], KEYS["S"], KEYS["D"])))):
        die(f"{path}: uses W/A/S/D on foot or the Windows keys (reserved for movement / PS button)")
    used.add("ps")
    out.append(("ps", "Botón PS", BTN["PS"], KEYS["LGUI"], KEYS["RGUI"], 2))
    name = (d.get("name") or pid).strip()
    if len(name.encode("utf-8")) > 63:
        name = name.encode("utf-8")[:63].decode("utf-8", "ignore")
    conf = {"high": 3, "medium": 2, "low": 1}.get(d.get("confidence", ""), 0)
    ads = int(d.get("ads_pct", 100) or 100)
    if d.get("kind") == "menu":
        kind = 2
    mouse = {"camera": 0, "dpad": 1}.get(d.get("mouse", "camera"))
    if mouse is None:
        die(f"{path}: mouse must be camera or dpad")
    return {"id": pid, "name": name, "kind": kind, "mouse": mouse, "template": d.get("template", "generic"), "conf": conf,
            "ads": max(25, min(400, ads)), "native": bool(d.get("native_kbm_ps5") is True), "actions": out,
            "notes": (d.get("notes") or "")[:700], "sources": d.get("sources") or [],
            "tips": (d.get("tips") or {}).get("es", ""), "tips_en": (d.get("tips") or {}).get("en", "")}


def load_titles():
    rows = []
    for fn, prefix in (("PS5_Titles.tsv", 0), ("PS4_Titles.tsv", 1)):
        with open(DATA / "titledb" / fn, encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                tid = r["titleId"][:9]
                if not re.fullmatch(r"(PPSA|CUSA)\d{5}", tid):
                    continue
                rows.append({"tid": tid, "prefix": prefix, "concept": (prefix, r["conceptId"]), "name": r["name"],
                             "n": norm(r["name"]), "region": r.get("region", ""), "content": r.get("contentId", "")})
    return rows


JUNK = re.compile(r"entitlement|\(prod\)|\btest\b|sample|dummy", re.I)
CLASSIC = re.compile(r"-(SLUS|SCUS|SLES|SCES|SLPS|SCPS|SLPM)")


def main():
    profiles = [load_profile(p, 0) for p in sorted((DATA / "profiles").glob("*.json")) if not p.name.startswith("_")]
    templates = [load_profile(p, 1) for p in sorted((DATA / "templates").glob("*.json"))]
    allp = templates + profiles
    ids = [p["id"] for p in allp]
    if len(set(ids)) != len(ids):
        die(f"duplicate profile ids: {[i for i in ids if ids.count(i) > 1]}")
    index = {p["id"]: i for i, p in enumerate(allp)}

    games = json.loads((DATA / "games.json").read_text(encoding="utf-8"))
    native_pats = json.loads((DATA / "native.json").read_text(encoding="utf-8")).get("patterns", [])
    rules = []
    pending = [pid for pid in games["profiles"] if pid not in index]
    if pending:
        print(f"gen_builtin: {len(pending)} games.json entries without a profile file yet (use template): {' '.join(pending)}")
    for pid, pats in games["profiles"].items():
        if pid not in index:
            continue
        rules += [(re.compile(p + r"\Z"), index[pid]) for p in pats]
    for tid_, pats in games["templates"].items():
        rules += [(re.compile(p + r"\Z"), index[tid_]) for p in pats]
    native_re = [re.compile(p + r"\Z") for p in native_pats]
    overrides = {k: v for k, v in games.get("title_overrides", {}).items() if not k.startswith("_")}
    for p in profiles:
        if p["native"]:
            native_re += [re.compile(x + r"\Z") for x in games["profiles"].get(p["id"], [])]

    rows = [r for r in load_titles() if not JUNK.search(r["name"])]
    by_concept = {}
    for r in rows:
        by_concept.setdefault(r["concept"], []).append(r)

    concepts, entries = [], []
    stats = {"assigned_profile": 0, "assigned_template": 0, "native": 0}
    for ckey, rs in by_concept.items():
        names = {r["n"] for r in rs if not CLASSIC.search(r["content"])}
        prof = -1
        ov = next((overrides[r["tid"]] for r in rs if r["tid"] in overrides), None)
        if ov is not None:
            if ov not in index:
                die(f"games.json title_overrides: unknown profile '{ov}'")
            rules_c = [(re.compile(r".*"), index[ov])]
        else:
            rules_c = rules
        for rx, pi in rules_c:
            if any(rx.match(n) for n in names):
                prof = pi
                break
        native = any(rx.match(n) for rx in native_re for n in names)
        if ckey[0] == 1 and prof < 0 and not native:
            continue               # PS4 titles only when we have something for them
        # display name: prefer EN regions, most common spelling
        pref = [r for r in rs if r["region"] in ("UP", "EP")] or rs
        extra = re.compile(r"bonus|demo|trial|beta|soundtrack|guide|upgrade|pack\b|dlc|season pass|playtest", re.I)
        disp = max({r["name"] for r in pref}, key=lambda n: (not extra.search(n), sum(1 for r in pref if r["name"] == n),
                                                             n != n.upper(), -len(n)))
        disp = disp.replace("™", "").replace("®", "").replace("©", "").strip()[:80]
        ci = len(concepts)
        concepts.append((disp, prof, 1 if native else 0))
        if prof >= 0:
            stats["assigned_profile" if allp[prof]["kind"] == 0 else "assigned_template"] += 1
        stats["native"] += native
        for r in rs:
            entries.append((r["prefix"] * 100000 + int(r["tid"][4:]), ci))
    entries = sorted(set(entries))
    if len(concepts) > 65535:
        die("too many concepts for uint16")

    # ---------- emit ----------
    h = ROOT / "src" / "kbm_builtin_gen.h"
    c = ROOT / "src" / "kbm_builtin_gen.c"
    h.write_text(f"""/* GENERATED by tools/gen_builtin.py — do not edit. */
#pragma once
#include <stdint.h>
#define KBM_BUILTIN_COUNT {len(allp)}
#define KBM_TITLE_COUNT {len(entries)}
#define KBM_CONCEPT_COUNT {len(concepts)}
typedef struct {{ const char *id, *label, *label_en; uint64_t out; uint8_t k1, k2, layer; }} KbmBuiltinAction;
typedef struct {{
    const char *id, *name, *name_en, *tmpl, *notes, *tips, *tips_en;   /* tips: in-game settings for a native feel */
    uint8_t kind;        /* 0 = game profile, 1 = template, 2 = PS5 menu (no game running) */
    uint8_t mouse_dpad;  /* mouse movement -> D-pad steps instead of right stick */
    uint8_t confidence;  /* 3 high, 2 medium, 1 low, 0 n/a */
    uint8_t native;      /* PS5 version supports KB+M natively */
    int16_t ads_pct;
    int n;
    const KbmBuiltinAction *a;
}} KbmBuiltinProfile;
typedef struct {{ uint32_t key; uint16_t concept; }} KbmTitleEntry;          /* key = prefix*100000 + number; prefix 0 PPSA, 1 CUSA */
typedef struct {{ uint32_t name_off; int16_t profile; uint8_t native; }} KbmConceptEntry;
extern const KbmBuiltinProfile KBM_BUILTIN[KBM_BUILTIN_COUNT];
extern const KbmTitleEntry KBM_TITLES[KBM_TITLE_COUNT];
extern const KbmConceptEntry KBM_CONCEPTS[KBM_CONCEPT_COUNT];
extern const char KBM_CONCEPT_NAMES[];
""")
    out = ["/* GENERATED by tools/gen_builtin.py — do not edit.",
           " * Title data: andshrew/PlayStation-Titles (MIT), see data/titledb/. */",
           '#include "kbm_builtin_gen.h"', ""]
    missing = sorted({x[1] for p in allp for x in p["actions"] if x[1] not in I18N["labels"]})
    if missing:
        die("no English text in data/i18n/en.json for: " + " | ".join(missing))
    for i, p in enumerate(allp):
        out.append(f"static const KbmBuiltinAction A{i}[] = {{")
        for aid, lab, m, k1, k2, lay in p["actions"]:
            out.append(f"  {{{c_str(aid)}, {c_str(lab)}, {c_str(I18N['labels'][lab])}, 0x{m:x}ull, 0x{k1:02x}, 0x{k2:02x}, {lay}}},")
        out.append("};")
    out.append("const KbmBuiltinProfile KBM_BUILTIN[KBM_BUILTIN_COUNT] = {")
    for i, p in enumerate(allp):
        out.append(f"  {{{c_str(p['id'])}, {c_str(p['name'])}, {c_str(I18N['names'].get(p['name'], p['name']))}, {c_str(p['template'])}, {c_str(p['notes'])}, {c_str(p['tips'])}, {c_str(p['tips_en'])}, "
                   f"{p['kind']}, {p['mouse']}, {p['conf']}, {int(p['native'])}, {p['ads']}, {len(p['actions'])}, A{i}}},")
    out.append("};")
    blob, offs = bytearray(), []
    for disp, _, _ in concepts:
        offs.append(len(blob))
        blob += disp.encode("utf-8") + b"\0"
    out.append("const KbmConceptEntry KBM_CONCEPTS[KBM_CONCEPT_COUNT] = {")
    out += [f"  {{{o},{prof},{nat}}}," for o, (_, prof, nat) in zip(offs, concepts)]
    out.append("};")
    out.append("const KbmTitleEntry KBM_TITLES[KBM_TITLE_COUNT] = {")
    out += [f"  {{{k},{ci}}}," for k, ci in entries]
    out.append("};")
    out.append(f"const char KBM_CONCEPT_NAMES[{len(blob)}] = {{")
    for j in range(0, len(blob), 24):
        out.append("  " + ",".join(str(b) for b in blob[j:j + 24]) + ",")
    out.append("};")
    c.write_text("\n".join(out) + "\n")

    # human-readable report
    rep = [f"profiles={len(profiles)} templates={len(templates)} titles={len(entries)} concepts={len(concepts)}",
           f"concepts with game profile={stats['assigned_profile']} with template={stats['assigned_template']} native={stats['native']}"]
    for p in profiles:
        hits = [d for d, pr, _ in concepts if pr == index[p["id"]]]
        rep.append(f"  {p['id']:24s} conf={p['conf']} actions={len(p['actions']):2d} matched={len(hits)}  {hits[:3]}")
    (ROOT / "data" / "BUILTIN_REPORT.txt").write_text("\n".join(rep) + "\n")
    unv = sorted((DATA / "unverified").glob("*.json"))
    T = {
        "es": dict(file="PROFILES.es.md", other="[English](PROFILES.md)", title="Perfiles incluidos en el bridge",
                   intro=["Generado por `tools/gen_builtin.py`. Cada perfil traduce las **teclas por defecto de la versión de PC** al botón del",
                          "DualSense que hace lo mismo en la versión de PS5. Movimiento (WASD/flechas) y botón PS (tecla Windows) se agregan solos."],
                   db="Base de títulos: {e} Title IDs ({c} juegos) de andshrew/PlayStation-Titles (MIT).",
                   counts="Juegos con perfil propio: {p} entradas · con plantilla por género: {t} · con teclado/mouse nativo según la investigación: {n}.",
                   head="| Perfil | Confianza | Teclado/mouse nativo en PS5 | Acciones | Fuentes |",
                   conf={3: "alta", 2: "media", 1: "baja", 0: "-"}, yes="sí", no="no",
                   tpl_h="Plantillas por género", tpl_i="Se aplican a los juegos reconocidos por nombre que todavía no tienen perfil propio:",
                   games="juegos", unv_h="Sin verificar (no incluidos)",
                   unv_i="Investigación incompleta: estos juegos usan su plantilla hasta que alguien confirme las teclas.", en=False),
        "en": dict(file="PROFILES.md", other="[Español](PROFILES.es.md)", title="Profiles built into the bridge",
                   intro=["Generated by `tools/gen_builtin.py`. Each profile maps the **PC version's default keys** to the DualSense",
                          "button that does the same thing in the PS5 version. Movement (WASD/arrows) and the PS button (Windows key) are added automatically."],
                   db="Title database: {e} Title IDs ({c} games) from andshrew/PlayStation-Titles (MIT).",
                   counts="Games with their own profile: {p} entries · with a genre template: {t} · with native keyboard/mouse according to research: {n}.",
                   head="| Profile | Confidence | Native keyboard/mouse on PS5 | Actions | Sources |",
                   conf={3: "high", 2: "medium", 1: "low", 0: "-"}, yes="yes", no="no",
                   tpl_h="Genre templates", tpl_i="Used for games recognised by name that don't have their own profile yet:",
                   games="games", unv_h="Unverified (not included)",
                   unv_i="Incomplete research: these games use their template until someone confirms the bindings.", en=True),
    }
    for t in T.values():
        md = [f"# {t['title']}", "", t["other"], ""] + t["intro"] + ["",
              t["db"].format(e=len(entries), c=len(concepts)),
              t["counts"].format(p=stats["assigned_profile"], t=stats["assigned_template"], n=stats["native"]), "",
              t["head"], "|---|---|---|---|---|"]
        for p in sorted(profiles, key=lambda q: q["name"].lower()):
            src = " ".join(f"[{i + 1}]({u})" for i, u in enumerate(p["sources"][:6]))
            md.append(f"| {p['name']} | {t['conf'][p['conf']]} | {t['yes'] if p['native'] else t['no']} | {len(p['actions'])} | {src} |")
        md += ["", f"## {t['tpl_h']}", "", t["tpl_i"], ""]
        for p in (q for q in templates if q["kind"] == 1):
            n = sum(1 for _, pr, _ in concepts if pr == index[p["id"]])
            nm = I18N["names"].get(p["name"], p["name"]) if t["en"] else p["name"]
            md.append(f"- **{nm}** (`{p['id']}`): {n} {t['games']}")
        if unv:
            md += ["", f"## {t['unv_h']}", "", t["unv_i"], ""] + [f"- `{f.stem}`" for f in unv]
        (ROOT / "data" / t["file"]).write_text("\n".join(md) + "\n", encoding="utf-8")
    print("\n".join(rep[:2]))
    missing = [p["id"] for p in profiles if not any(pr == index[p["id"]] for _, pr, _ in concepts)]
    if missing:
        print(f"gen_builtin: WARNING profiles with no matching title: {missing}")


if __name__ == "__main__":
    main()
