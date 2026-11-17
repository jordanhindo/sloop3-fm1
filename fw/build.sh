#!/bin/sh
# Build the firmware for flash: build/fw/fw.area, the plain application area the installer page encrypts and
# installs through m-bootloader's MIDI mode.   fw/build.sh [LLVM_BIN]
set -eu
TC=${1:-${PI32V2_LLVM:-toolchain/bin}}
O=build/fw; rm -rf $O; mkdir -p $O/o
CF="${BOOT_DIAG:+-DBOOT_DIAG} -mcpu=ac791n -O2 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter"
for f in engine/sound.c engine/fx.c engine/common.c engine/va.c engine/drum.c engine/fm.c engine/lofi.c engine/pd.c engine/wheel.c engine/noise.c engine/voice.c engine/trio.c engine/phys.c engine/fm6.c engine/sample.c engine/grain.c engine/trackfx.c engine/texture.c engine/engines.c core/libc.c hal/fm1/audio.c; do
  "$TC/clang" $CF -c $f -o $O/o/$(basename $f .c).o
done
# the interface and the projects: small rather than fast (they run between audio blocks, with time to spare); the
# master bus too (its loops run once a block, on the mix: not per voice); the hardware's drivers
for f in engine/drumkits.c engine/fm6_patches.c engine/slice.c hal/fm1/irq.c hal/fm1/board.c hal/fm1/cpu1.c hal/fm1/adc.c hal/fm1/panel.c hal/fm1/flash.c hal/fm1/usb.c hal/fm1/uart.c hal/fm1/sys.c \
         engine/master.c seq/seq.c seq/loop.c app/store.c app/harmony.c app/arp.c app/midi.c app/psound.c fw/main.c; do
  "$TC/clang" $(echo "$CF" | sed 's/-O2/-Os/') -c $f -o $O/o/$(basename $f .c).o
done
# what runs seldom, or once a screen: the smallest (3.1.6: the room for the visualiser)
for f in engine/slice_scan.c app/demo_song.c app/fonts.c app/presets.c app/preset_data.c app/sloopimport.c app/sloop_kinds.c app/usersnd.c \
         app/xbank.c app/engdesc.c app/project.c app/gfx.c app/vis.c app/ui.c app/edproto.c; do
  "$TC/clang" $(echo "$CF" | sed 's/-O2/-Oz/') -c $f -o $O/o/$(basename $f .c).o
done
"$TC/clang" -mcpu=ac791n -c hal/fm1/ramapp_start.S -o $O/o/start.o
"$TC/clang" -mcpu=ac791n -c hal/fm1/cpu1_entry.S -o $O/o/cpu1_entry.o
"$TC/clang" -mcpu=ac791n -c hal/fm1/irq.S -o $O/o/irq_s.o
RT=$(dirname "$TC")/lib/clang/23/lib/pi32v2/libclang_rt.builtins.a
# the firmware itself, linked for RAM as the RAM apps
"$TC/ld.lld" -T fw/ram.ld --gc-sections -o $O/ram.elf $O/o/*.o "$RT"
"$TC/llvm-objcopy" -O binary --only-section=.entry --only-section=.text --only-section=.data $O/ram.elf $O/ram.bin
"$TC/llvm-objcopy" -O binary --only-section=.xtext $O/ram.elf $O/xcode.bin
# the entry's own words (hal/fm1/ramapp_start.S) on a word: a word stored off it faults at the very start (3.0)
"$TC/llvm-nm" $O/ram.elf | python3 -c "import sys; a=[int(l.split()[0],16) for l in sys.stdin if l.split()[-1]=='__caller_sp']; sys.exit('__caller_sp is not on a word' if not a or a[0]%4 else 0)"
# the flash window's rule: nothing that runs while a save has it off may be in it, or read it
python3 tools/xipcheck.py $O/ram.elf "$TC"
# app.bin: the XIP stub and the RAM image after it, packed (3.2: tools/lz4pack.py, unpacked by fw/unpack.c at the start;
# tests/unpack_test.c runs that same code on the PC over this very image, and over damaged ones)
python3 tools/lz4pack.py $O/ram.bin $O/ram.pk
cc -O1 -g -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o build/unpack_test tests/unpack_test.c
ASAN_OPTIONS=detect_leaks=0 ./build/unpack_test $O/ram.pk $O/ram.bin
"$TC/clang" -mcpu=ac791n -Os -ffreestanding -fno-builtin -Wall -Wextra -c fw/unpack.c -o $O/unpack.o
test -z "$("$TC/llvm-nm" -u $O/unpack.o)" || { echo "fw/unpack.c calls something"; exit 1; }
# BOOT_DIAG: the sum of the RAM image's words, which the stub checks its copy against (fw/xip.S)
XS=""
[ -n "${BOOT_DIAG:-}" ] && XS="-DXSUM=$(python3 -c "
import struct; b=open('$O/ram.bin','rb').read(); b+=bytes(-len(b)%4); print(hex(sum(struct.unpack('<%dI'%(len(b)//4),b))&0xFFFFFFFF))")"
"$TC/clang" -mcpu=ac791n ${BOOT_DIAG:+-DBOOT_DIAG} -DPACKED -DPAYLOAD="\"$O/ram.pk\"" -DXCODE="\"$O/xcode.bin\"" -c fw/xip.S -o $O/xip.o
"$TC/ld.lld" -T fw/xip.ld -o $O/app.elf $O/xip.o $O/unpack.o
"$TC/llvm-objcopy" -O binary $O/app.elf $O/app.bin
# the area: the firmware, then the sample bank at flash 0x48000 (assets/samples.bin, tools/mksamples.py); the projects
# begin at 0x7A000 (app/store.h): the area, from 0x4000, must end before
python3 tools/mkfw.py $O/app.bin $O/fw.area assets/samples.bin
test $(stat -c%s $O/fw.area) -le $((0x78000 - 0x4000)) || { echo "the area would run into your FM6 bank and SYN kits (0x78000, app/xbank.h)"; exit 1; }
"$TC/llvm-size" $O/ram.elf | tail -1
# the installer page: through m-bootloader's MIDI MODE only (3.3.1: the way straight from SLOOP 2.8 is gone)
python3 tools/mkpage.py $O/fw.area "3.3.7" $O/installer-fm1.html
python3 tools/mkeditor.py web/editor.html > /dev/null
cp web/editor.html $O/editor-sloop3.html
# the page against a simulated m-bootloader: the package it serves, decrypted, is fw.area (tests/install_sim.js)
node tests/install_sim.js $O/installer-fm1.html $O/fw.area | tail -1
# the web editor's FM6 bank and SYN kits (tests/editor_test.js)
node tests/editor_test.js web/editor.html | tail -1
# the whole editor in Chromium against the PC twin (tests/editor_e2e.js; EDITOR_E2E=0 to skip)
if [ "${EDITOR_E2E:-1}" = 1 ] && [ -x build/twin ]; then node tests/editor_e2e.js web/editor.html build/e2e | tail -1; fi
# the installer page in Chromium, as a user goes through it, in English and in French (tests/installer_e2e.js; INSTALLER_E2E=0 to skip)
if [ "${INSTALLER_E2E:-1}" = 1 ]; then node tests/installer_e2e.js $O/installer-fm1.html build/inst-e2e | tail -1; fi
