CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -D_DEFAULT_SOURCE
CORE := ../src/kbm_engine.c ../src/kbm_keys.c ../src/kbm_config.c ../src/kbm_custom.c ../src/kbm_builtin_gen.c

GEN := ../src/kbm_builtin_gen.c ../src/kbm_builtin_gen.h

.PHONY: all test clean
all: test_kbm test_usbfs
# generated sources (also done by build_wsl.sh)
../src/kbm_builtin_gen.c ../src/kbm_builtin_gen.h: ../tools/gen_builtin.py $(wildcard ../data/*.json ../data/profiles/*.json ../data/templates/*.json)
	cd .. && python3 tools/gen_builtin.py
../src/kbm_webui_gen.h: ../web/index.html ../tools/embed_web.py
	cd .. && python3 tools/embed_web.py
test_kbm: test_kbm.c $(CORE) ../src/hid_boot.c | $(GEN)
	$(CC) $(CFLAGS) -o $@ test_kbm.c $(CORE) ../src/hid_boot.c
test_usbfs: test_usbfs.c ../src/kbm_usbfs.c $(CORE) fake_usb_ioctl.h | $(GEN)
	$(CC) $(CFLAGS) -DKBM_HOST_TEST -I. -o $@ test_usbfs.c ../src/kbm_usbfs.c $(CORE)
test: all
	./test_kbm
	python3 -c "import json;d=json.load(open('config_test.json'));e=d['edit'];assert len(d['profiles'])>=7 and e['id']=='requiem' and any(a['id']=='ps' and a['keys']==['LGUI','RGUI'] for a in e['actions']);print('PASS: config JSON parses,',len(d['profiles']),'profiles')"
	./test_usbfs
clean:
	rm -f test_kbm test_usbfs config_test.json host_server host_config.ini ui_*.png

host_server: host_server.c ../src/kbm_http.c ../src/kbm_api.c ../src/kbm_profiles_io.c $(CORE) ../src/kbm_webui_gen.h
	$(CC) $(CFLAGS) -pthread -o $@ host_server.c ../src/kbm_http.c ../src/kbm_api.c ../src/kbm_profiles_io.c $(CORE)
