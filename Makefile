# The PC side: tests and the demo render (the FM-1 build comes with hal/fm1)
CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
ENGINE = engine/sound.c engine/fx.c engine/master.c engine/common.c engine/va.c engine/drum.c engine/drumkits.c engine/fm.c engine/lofi.c engine/pd.c engine/wheel.c engine/noise.c engine/voice.c engine/trio.c engine/phys.c engine/fm6.c engine/fm6_patches.c engine/sample.c engine/grain.c engine/slice.c engine/slice_scan.c engine/trackfx.c engine/texture.c engine/engines.c
.PHONY: test demo clean levels fuzz twin denorm fonts
test: build/sound_test build/seq_test build/loop_test build/store_test build/harmony_test build/arp_test build/level_tool build/sloop_test build/midi_test build/ui_shots
	./build/sound_test
	./build/seq_test
	./build/loop_test
	./build/store_test
	./build/harmony_test
	./build/arp_test
	./build/level_tool check
	./build/sloop_test
	./build/midi_test
	mkdir -p build/shots && ./build/ui_shots
build/harmony_test: tests/harmony_test.c app/harmony.c app/harmony.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/harmony_test.c app/harmony.c
build/level_tool: tests/level_tool.c app/presets.c app/preset_data.c app/presets.h $(ENGINE)
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/level_tool.c app/presets.c app/preset_data.c $(ENGINE) -lm
levels: build/level_tool
	./build/level_tool && python3 tools/mkpresets.py app/preset_data.c
build/sloop_test: tests/sloop_test.c app/sloopimport.c app/sloop_kinds.c app/store.c app/presets.c app/preset_data.c app/usersnd.c seq/seq.c $(ENGINE)
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/sloop_test.c app/sloopimport.c app/sloop_kinds.c app/store.c app/presets.c app/preset_data.c app/usersnd.c seq/seq.c $(ENGINE) -lm
build/midi_test: tests/midi_test.c app/midi.c app/midi.h app/edproto.c app/edproto.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/midi_test.c app/midi.c app/edproto.c
build/arp_test: tests/arp_test.c app/arp.c app/arp.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/arp_test.c app/arp.c
build/ui_shots: tests/ui_shots.c app/ui.c app/ui.h app/gfx.c app/gfx.h app/fonts.c app/vis.c app/vis.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/ui_shots.c app/ui.c app/gfx.c app/vis.c app/fonts.c engine/common.c -lm
fonts:                  # the screen's fonts again (app/fonts.c, from Inter Bold: tools/mkfont.py)
	python3 tools/mkfont.py app/fonts.c
build/store_test: tests/store_test.c app/store.c app/store.h app/project.c app/project.h app/psound.c app/psound.h app/xbank.c app/xbank.h seq/seq.c $(ENGINE)
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/store_test.c app/store.c app/project.c app/psound.c app/xbank.c seq/seq.c $(ENGINE) -lm
build/seq_test: tests/seq_test.c seq/seq.c seq/seq.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/seq_test.c seq/seq.c -lm
build/loop_test: tests/loop_test.c seq/loop.c seq/loop.h seq/seq.c seq/seq.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/loop_test.c seq/loop.c seq/seq.c -lm
build/sound_test: tests/sound_test.c $(ENGINE) engine/engine.h core/gb.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ tests/sound_test.c $(ENGINE) -lm
# the denormal hunt (3.1.8): every sound, every effect, their tails, the FPU trapping on a denormal (x86-64)
denorm: build/denorm_test
	./build/denorm_test 12 | tail -3
build/denorm_test: tests/denorm_test.c app/presets.c app/preset_data.c $(ENGINE)
	mkdir -p build && gcc -O2 -g -no-pie -std=gnu11 -w -ffp-contract=off -o $@ tests/denorm_test.c app/presets.c app/preset_data.c $(ENGINE) -lm
# the readers of what comes from outside, fed damaged inputs, under the sanitizers (gcc: its ASan runtime is here)
FUZZSRC = tests/fuzz.c app/project.c app/psound.c app/presets.c app/preset_data.c app/sloopimport.c app/sloop_kinds.c app/usersnd.c app/xbank.c app/edproto.c app/store.c seq/seq.c $(ENGINE)
fuzz: build/fuzz
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/fuzz 3000
build/fuzz: $(FUZZSRC)
	mkdir -p build && gcc -O1 -g -std=c11 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -o $@ $(FUZZSRC) -lm
# the PC twin: fw/main.c itself on a simulated FM-1 (host/), its scenarios (host/twin.sh), under the sanitizers
TWINSRC = host/twin.c host/hal.c app/project.c app/psound.c app/presets.c app/preset_data.c app/sloopimport.c app/sloop_kinds.c app/usersnd.c app/xbank.c app/edproto.c app/store.c app/ui.c app/gfx.c app/vis.c app/fonts.c app/harmony.c app/midi.c app/arp.c app/engdesc.c app/demo_song.c seq/seq.c seq/loop.c $(ENGINE)
twin: build/twin build/fw/fw.area
	sh host/twin.sh 60 "1 2"
# the device firmware area (fw/build.sh): the toolchain lives beside this clone (override PI32V2_LLVM to point
# elsewhere); the e2e steps need playwright-core (NODE_PATH, installed outside the repo) and the Playwright
# headless shell (installed once: node_modules/.bin/playwright install chromium-headless-shell). The system
# Chromium in /opt/meta-chromium cannot be used: it is Meta-managed and refuses to start here.
PI32V2_LLVM ?= $(CURDIR)/../toolchain/bin
FW_NODE_PATH ?= $(CURDIR)/../node_modules
build/fw/fw.area: fw/build.sh $(wildcard fw/* seq/* engine/* app/* hal/fm1/* core/* tools/* assets/*)
	PI32V2_LLVM="$(PI32V2_LLVM)" NODE_PATH="$(FW_NODE_PATH)" sh ./fw/build.sh
build/twin: $(TWINSRC) fw/main.c host/twin.h
	mkdir -p build && gcc -DHOST -O1 -g -std=gnu11 -w -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -pthread -o $@ $(TWINSRC) -lm
demo: build/demo
	./build/demo build/demo.wav
build/demo: host/demo.c app/demo_song.c app/demo_song.h $(ENGINE) engine/engine.h core/gb.h
	mkdir -p build && $(CC) $(CFLAGS) -o $@ host/demo.c app/demo_song.c $(ENGINE) -lm
clean:
	rm -rf build
