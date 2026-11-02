#!/usr/bin/env python3
"""Black-box test of the real HTTP/API code via tests/host_server."""
import json, subprocess, sys, time, urllib.request, os

PORT = int(os.environ.get("KBM_TEST_PORT", "18099"))
BASE = f"http://127.0.0.1:{PORT}"
here = os.path.dirname(os.path.abspath(__file__))
cfg_out = os.path.join(here, "host_config.ini")

def get(path):
    with urllib.request.urlopen(BASE + path, timeout=3) as r:
        return r.status, dict(r.headers), r.read()

def prof(pid):
    return api("/api/state?edit=" + pid)[1]["config"]["edit"]


def api(path):
    try:
        s, h, b = get(path)
    except urllib.error.HTTPError as e:
        s, h, b = e.code, dict(e.headers), e.read()
    return s, json.loads(b)

import tempfile, shutil
pdir = tempfile.mkdtemp(prefix="kbm_profiles_")
env = dict(os.environ, KBM_PROFILES_DIR=pdir)

def post(path, body):
    req = urllib.request.Request(BASE + path, data=body, method="POST", headers={"Content-Type": "text/plain"})
    try:
        with urllib.request.urlopen(req, timeout=3) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        b = e.read()
        try: return e.code, json.loads(b)
        except ValueError: return e.code, b

srv = subprocess.Popen([os.path.join(here, "host_server"), str(PORT), cfg_out], stdout=subprocess.PIPE, text=True, env=env)
try:
    assert "custom profiles loaded: 0" in srv.stdout.readline()
    assert "listening" in srv.stdout.readline()
    s, h, body = get("/")
    assert s == 200 and b"PS5 KBM Bridge" in body and h["Access-Control-Allow-Origin"] == "*"
    s, st = api("/api/state")
    assert s == 200 and st["runtime"]["title"] == "PPSA04289" and len(st["config"]["profiles"]) >= 7
    assert st["runtime"]["title_name"].lower() == "resident evil 2" and st["runtime"]["title_source"] == "game"
    assert st["runtime"]["auto_profile"] == "re2r" and st["config"]["edit"]["id"] == "re2r"
    assert all(set(p) >= {"id", "name", "kind", "conf", "mod"} and "actions" not in p for p in st["config"]["profiles"])
    assert "MOUSE1" in st["keys"] and "F12" not in st["keys"]
    assert st["runtime"]["profile"] == "re2r" and st["runtime"]["hz"] == 250
    assert st["runtime"]["pad"] == ""

    assert api("/api/profile?id=re2r&sens=14&invert_y=1&smooth_ms=999")[1]["ok"]
    p = prof("re2r")
    assert p["sens"] == 14 and p["invert_y"] == 1 and p["smooth_ms"] == 60

    # bind R (reload) onto aim slot 0 -> reload loses R
    s, r = api("/api/bind?profile=re2r&action=aim&slot=0&key=R")
    assert r["ok"] and r.get("moved_from") == "reload", r
    assert api("/api/bind?profile=re2r&action=aim&slot=0&key=F12")[1]["ok"] is False
    assert api("/api/bind?profile=nope&action=aim&slot=0&key=R")[1]["ok"] is False

    # capture flow (fake press of H)
    assert api("/api/capture?profile=requiem&action=heal&slot=1")[1]["ok"]
    for _ in range(30):
        cap = api("/api/state")[1]["runtime"]["capture"]
        if not cap["active"]: break
        time.sleep(0.1)
    assert cap["result"] == "H", cap
    heal = [a for a in prof("requiem")["actions"] if a["id"] == "heal"][0]
    assert heal["keys"] == ["Q", "H"], heal

    # title rules -> active profile switches
    assert api("/api/title?tid=PPSA04289&target=requiem")[1]["ok"]
    rt = api("/api/state")[1]["runtime"]
    assert rt["profile"] == "requiem" and rt["title_source"] == "rule" and rt["auto_profile"] == "re2r"
    assert api("/api/title?tid=PPSA04289&target=bogus")[1]["ok"] is False
    t = [x for x in api("/api/state")[1]["config"]["titles"] if x["tid"] == "PPSA04289"][0]
    assert t["name"].lower() == "resident evil 2" and t["target"] == "requiem"
    # lookup + profile_get
    lk = api("/api/lookup?tid=PPSA01923")[1]
    assert lk["known"] == 1 and lk["name"] == "Fortnite" and lk["native_suggested"] == 1 and lk["native"] == 0, lk
    assert api("/api/lookup?tid=CUSA04885")[1]["profile"] == "tps"
    g = api("/api/profile_get?id=elden_ring")[1]
    assert g["kind"] == "game" and g["conf"] == 3 and any(a["id"] == "ps" for a in g["actions"])
    assert api("/api/profile_get?id=nope")[1]["ok"] is False
    assert api("/api/global?auto_native=1")[1]["ok"] and api("/api/lookup?tid=PPSA01923")[1]["native"] == 1
    assert api("/api/global?auto_native=0")[1]["ok"]
    # language: system language (simulated es), override, back to auto
    st = api("/api/state")[1]
    assert st["config"]["lang"] == "auto" and st["runtime"]["sys_lang"] == "es" and st["runtime"]["lang"] == "es", st["runtime"]
    assert api("/api/global?lang=en")[1]["ok"]
    st = api("/api/state?edit=re2r")[1]
    assert st["config"]["lang"] == "en" and st["runtime"]["lang"] == "en"
    rl = [a for a in st["config"]["edit"]["actions"] if a["id"] == "reload"][0]
    assert rl["label"] == "Recargar" and rl["label_en"] == "Reload", rl
    assert [p for p in st["config"]["profiles"] if p["id"] == "generic"][0]["name_en"] == "Generic (all buttons)"
    # vehicle layers: a key only moves off actions that are active at the same time
    g = api("/api/state?edit=gta_trilogy_de")[1]["config"]["edit"]
    assert g["layers"] == 26 and {a["id"]: a["layer"] for a in g["actions"]}["accelerate"] == 1   # car|bike|air bits
    r = api("/api/bind?profile=gta_trilogy_de&action=car_fire&slot=1&key=Q")[1]
    assert r["ok"] and r.get("moved_from") == "look_left", r
    k = {a["id"]: a["keys"] for a in api("/api/state?edit=gta_trilogy_de")[1]["config"]["edit"]["actions"]}
    assert k["car_fire"] == ["MOUSE1", "Q"] and k["look_left"] == ["", ""] and k["swap_phone"][0] == "Q", k
    assert api("/api/reset?profile=gta_trilogy_de")[1]["ok"]
    # manual mode switch only for profiles with a vehicle mode (current title: RE2 -> none)
    assert api("/api/layer?l=1")[1]["ok"] is False
    assert api("/api/global?notify_mode=1")[1]["ok"] and api("/api/state")[1]["config"]["notify_mode"] == 1
    g5 = api("/api/state?edit=gta5")[1]["config"]["edit"]
    assert g5["layers"] == 26 and any(a["id"] == "veh_fire" and a["out"] == "L1+R1" and a["layer"] == 1 for a in g5["actions"])
    assert api("/api/global?lang=bogus")[1]["ok"] and api("/api/state")[1]["config"]["lang"] == "auto"
    assert api("/api/global?inject_hz=120&auto_activate=0&default_profile=generic")[1]["ok"]
    c = api("/api/state")[1]["config"]
    assert c["inject_hz"] == 120 and c["effective_hz"] == 120 and c["auto_activate"] == 0 and c["default_profile"] == "generic"
    assert api("/api/global?idle_exit_min=30")[1]["ok"] and api("/api/state")[1]["config"]["idle_exit_min"] == 30
    assert api("/api/global?idle_exit_min=-5")[1]["ok"] and api("/api/state")[1]["config"]["idle_exit_min"] == 0
    assert api("/api/state")[1]["config"]["open_settings_on_launch"] == 1
    assert api("/api/global?open_settings_on_launch=0")[1]["ok"] and api("/api/state")[1]["config"]["open_settings_on_launch"] == 0
    assert api("/api/global?open_settings_on_launch=1")[1]["ok"]
    assert api("/api/profile?id=requiem&ads_pct=220&curve=40&deadzone=24")[1]["ok"]
    rq = prof("requiem")
    assert rq["curve"] == 40 and rq["deadzone"] == 24 and "zona muerta" in rq["tips"] and "dead zone" in rq["tips_en"], rq
    assert api("/api/profile?id=requiem&curve=500")[1]["ok"] and prof("requiem")["curve"] == 100
    assert api("/api/profile?id=requiem&curve=40")[1]["ok"]
    rq = prof("requiem")
    assert rq["ads_pct"] == 220
    assert prof("re2r")["ads_pct"] == 100
    assert api("/api/global?pointer_sens=99")[1]["ok"] and api("/api/state")[1]["config"]["pointer_sens"] == 50
    assert api("/api/pointer?on=1")[1]["ok"] and api("/api/state")[1]["runtime"]["pointer_mode"] == 1
    assert api("/api/pointer?on=0")[1]["ok"] and api("/api/state")[1]["runtime"]["pointer_mode"] == 0
    assert api("/api/bind?profile=re2r&action=aim&slot=1&key=F9")[1]["ok"] is False
    assert api("/api/bind?profile=re2r&action=aim&slot=1&key=F11")[1]["ok"] is False
    assert api("/api/bind?profile=re2r&action=aim&slot=1&key=F10")[1]["ok"] is False
    assert api("/api/autostart?on=1")[1]["ok"] and api("/api/state")[1]["runtime"]["autostart_on"] == 1
    assert api("/api/autostart?on=0")[1]["ok"] and api("/api/state")[1]["runtime"]["autostart_on"] == 0
    assert api("/api/reset?profile=re2r")[1]["ok"]
    assert prof("re2r")["sens"] == 8 and prof("re2r")["mod"] == 0
    assert api("/api/nothing")[0] == 404

    ini = open(cfg_out).read()
    assert "notify_mode=1" in ini and "curve=40" in ini and "deadzone=24" in ini
    assert "lang=auto" in ini and "idle_exit_min=0" in ini and "ads_pct=220" in ini and "pointer_sens=50" in ini
    assert "PPSA04289=requiem" in ini and "[profile.re2r]" not in ini and "[profile.requiem]" in ini and "[profile.elden_ring]" not in ini and "key.heal=Q,H" in ini and "inject_hz=120" in ini

    # custom profiles: export a built-in, change it, import it back; import a new one; delete
    s, h, raw = get("/api/export?id=gta5")
    ex = json.loads(raw)
    assert s == 200 and ex["format"] == "ps5-kbm-profile" and ex["id"] == "gta5" and ex["no_auto_move"] is True
    assert any(a["ps5"] == "TRIANGLE+MODE_TOGGLE" and a["pc"][0] == "F" for a in ex["actions"]), ex["actions"]
    for pid in [p["id"] for p in api("/api/state")[1]["config"]["profiles"]]:
        s, h, raw = get("/api/export?id=" + pid)
        assert s == 200 and json.loads(raw)["id"] == pid
    ex["sens"] = 31
    ex["name"] = "GTA V (mío)"
    s, r = post("/api/import", json.dumps(ex, ensure_ascii=False).encode())
    assert s == 200 and r["ok"] and r["id"] == "gta5", r
    g = prof("gta5")
    assert g["sens"] == 31 and g["custom"] == 1 and g["overrides"] == 1 and g["mod"] == 0 and g["name"] == "GTA V (mío)"
    new = {"id": "my_game", "name": "Mi juego", "title_ids": ["PPSA04289"],
           "actions": [{"id": "jump", "label": "Saltar", "label_en": "Jump", "ps5": "CROSS", "pc": ["SPACE"]}]}
    s, r = post("/api/import", json.dumps(new).encode())
    assert r["ok"] and r["id"] == "my_game"
    assert api("/api/title?tid=PPSA04289&target=")[1]["ok"]          # drop the rule so the Title ID applies
    rt = api("/api/state")[1]["runtime"]
    assert rt["profile"] == "my_game" and rt["title_source"] == "custom", rt
    mg = prof("my_game")
    assert [a["id"] for a in mg["actions"]] == ["move_up", "move_down", "move_left", "move_right", "jump", "ps"] and mg["title_ids"] == ["PPSA04289"]
    assert sorted(os.listdir(pdir)) == ["gta5.json", "my_game.json"]
    assert json.load(open(os.path.join(pdir, "my_game.json")))["actions"][4]["label_en"] == "Jump"
    s, r = post("/api/import", b'{"id":"x","actions":[{"ps5":"CROSS","pc":["F10"]}]}')
    assert s == 400 and not r["ok"] and "reserved" in r["error"]
    s, r = post("/api/import", b"{" + b" " * 40000 + b"}")
    assert s == 413
    s, r = post("/api/state", b"{}")
    assert s == 404
    assert api("/api/delete_profile?id=re2r")[1]["ok"] is False      # built-in, not custom
    assert api("/api/delete_profile?id=gta5")[1]["ok"]
    g = prof("gta5")
    assert g["custom"] == 0 and g["sens"] != 31 and sorted(os.listdir(pdir)) == ["my_game.json"]
    assert api("/api/bind?profile=my_game&action=jump&slot=1&key=J")[1]["ok"]

    assert api("/api/retake")[1]["ok"]
    s, h, raw = get("/api/diag")
    rep = raw.decode()
    assert s == 200 and h["Content-Type"].startswith("text/plain") and rep.startswith("=== PS5 KBM Bridge diagnostics ===")
    assert "--- config.ini ---" in rep and "[global]" in rep and "my_game (new, 6 actions) PPSA04289" in rep and "firmware: host test" in rep
    # robustness: garbage requests must not kill the server
    import socket
    for junk in [b"\x00\xff\xfe\r\n\r\n", b"GET\r\n\r\n", b"POST / HTTP/1.1\r\n\r\n", b"GET /api/bind?" + b"a=" * 3000 + b" HTTP/1.0\r\n\r\n"]:
        k = socket.create_connection(("127.0.0.1", PORT), timeout=3); k.sendall(junk)
        try: k.recv(4096)
        except Exception: pass
        k.close()
    assert api("/api/state")[0] == 200

    assert api("/api/stop")[1]["ok"]
    srv.wait(timeout=5)
    assert "retakes=1" in srv.stdout.read()
    # restart: the custom profile file is loaded again
    srv = subprocess.Popen([os.path.join(here, "host_server"), str(PORT), "-", "PPSA04289"], stdout=subprocess.PIPE, text=True, env=env)
    assert "custom profiles loaded: 1" in srv.stdout.readline()
    assert "listening" in srv.stdout.readline()
    assert api("/api/state")[1]["runtime"]["profile"] == "my_game"
    assert api("/api/stop")[1]["ok"]
    srv.wait(timeout=5)
    print("PASS: HTTP API (state, profile, bind+move, capture, titles, global, idle, autostart, reset, custom profiles export/import/delete, 404, junk, stop, INI persisted)")
finally:
    if srv.poll() is None: srv.kill()
    shutil.rmtree(pdir, ignore_errors=True)
