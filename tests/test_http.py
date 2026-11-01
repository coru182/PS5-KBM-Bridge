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

srv = subprocess.Popen([os.path.join(here, "host_server"), str(PORT), cfg_out], stdout=subprocess.PIPE, text=True)
try:
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

    assert api("/api/profile?id=re2r&sens=14&invert_y=1&smooth_ms=999")[1]["ok"]
    p = prof("re2r")
    assert p["sens"] == 14 and p["invert_y"] == 1 and p["smooth_ms"] == 60

    # bind R (reload) onto aim slot 0 -> reload loses R
    s, r = api("/api/bind?profile=re2r&action=aim&slot=0&key=R")
    assert r["ok"] and r.get("moved_from") == "Recargar", r
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
    assert api("/api/global?inject_hz=120&auto_activate=0&default_profile=generic")[1]["ok"]
    c = api("/api/state")[1]["config"]
    assert c["inject_hz"] == 120 and c["effective_hz"] == 120 and c["auto_activate"] == 0 and c["default_profile"] == "generic"
    assert api("/api/global?idle_exit_min=30")[1]["ok"] and api("/api/state")[1]["config"]["idle_exit_min"] == 30
    assert api("/api/global?idle_exit_min=-5")[1]["ok"] and api("/api/state")[1]["config"]["idle_exit_min"] == 0
    assert api("/api/state")[1]["config"]["open_settings_on_launch"] == 1
    assert api("/api/global?open_settings_on_launch=0")[1]["ok"] and api("/api/state")[1]["config"]["open_settings_on_launch"] == 0
    assert api("/api/global?open_settings_on_launch=1")[1]["ok"]
    assert api("/api/profile?id=requiem&ads_pct=220")[1]["ok"]
    rq = prof("requiem")
    assert rq["ads_pct"] == 220
    assert prof("re2r")["ads_pct"] == 100
    assert api("/api/global?pointer_sens=99")[1]["ok"] and api("/api/state")[1]["config"]["pointer_sens"] == 50
    assert api("/api/pointer?on=1")[1]["ok"] and api("/api/state")[1]["runtime"]["pointer_mode"] == 1
    assert api("/api/pointer?on=0")[1]["ok"] and api("/api/state")[1]["runtime"]["pointer_mode"] == 0
    assert api("/api/bind?profile=re2r&action=aim&slot=1&key=F9")[1]["ok"] is False
    assert api("/api/bind?profile=re2r&action=aim&slot=1&key=F11")[1]["ok"] is False
    assert api("/api/autostart?on=1")[1]["ok"] and api("/api/state")[1]["runtime"]["autostart_on"] == 1
    assert api("/api/autostart?on=0")[1]["ok"] and api("/api/state")[1]["runtime"]["autostart_on"] == 0
    assert api("/api/reset?profile=re2r")[1]["ok"]
    assert prof("re2r")["sens"] == 8 and prof("re2r")["mod"] == 0
    assert api("/api/nothing")[0] == 404

    ini = open(cfg_out).read()
    assert "idle_exit_min=0" in ini and "ads_pct=220" in ini and "pointer_sens=50" in ini
    assert "PPSA04289=requiem" in ini and "[profile.re2r]" not in ini and "[profile.requiem]" in ini and "[profile.elden_ring]" not in ini and "key.heal=Q,H" in ini and "inject_hz=120" in ini

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
    print("PASS: HTTP API (state, profile, bind+move, capture, titles, global, idle, autostart, reset, 404, junk, stop, INI persisted)")
finally:
    if srv.poll() is None: srv.kill()
