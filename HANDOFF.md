# Handoff: Jordan's looper for the M-VAVE FM-1

Written Nov 21, 2026 by the Claude session that designed and built this with Jordan. It runs in the cloud and cannot
reach Jordan's computer or the FM-1. You are the agent on Jordan's Mac. From here on this is yours.

**Read this file top to bottom before you do anything.** Then read `docs/LOOPER.md` (how the looper is used) and skim
`docs/looper/spec-slices.md` (why it is the way it is). Everything else you need is in this repository.

---

## 1. Where things stand, in six lines

- Jordan is building a **free-form looper** on SLOOP 3, an open-source firmware for the M-VAVE FM-1 (a small synth:
  27 keys, 14 buttons, 7 knobs, a 1.5" screen). Play something, close it, it loops; stack more on other tracks.
- It is **built and pushed**: this repository, branch `loop`, on top of SLOOP 3.3.7 (commit `915b54f`).
- It **passes** SLOOP's own tests, the full PC twin (the real firmware on a simulated FM-1) and a device build.
- It has **never run on a real FM-1.** Jordan got his FM-1 today.
- **Your job now:** get this firmware onto his FM-1 safely, test it with him, and fix what real hardware shows.
- Jordan is **not a coder**. He decides how the instrument should feel. You do everything else.

## 2. How to work with Jordan

These are his standing rules. They matter more than anything technical below.

- **Do every part you can yourself.** Do not ask him to run a command, find a file, install a tool, read a log or
  check something you can check. Ask him only for what needs his hands on the device or his ears, or for a decision
  about how it should feel.
- **Plain words.** No code-speak, no jargon, no commands for him to run. Explain like to a musician.
- **Short, scannable, outcome first.** Bullets. Say what happened and what it means for him.
- **Be honest about what you verified and what you did not.** He would rather hear "I could not test that" than be
  told it works.
- **Do not add features or redesign.** The design was decided with him, one question at a time. If hardware shows a
  decision was wrong, tell him what you saw and let him choose.
- **Decide small things yourself; bring him the real decisions**, one at a time, with your recommendation.
- No em dashes in what you write to him.

## 3. The one thing only Jordan can do

Hands on the device. Everything else is yours.

| Only Jordan | You |
|---|---|
| Plug in the USB cable (it must be a data cable) | Everything on the Mac: cloning, downloads, opening pages, clicking through the installer, saving the backup |
| Hold PLAY while switching the FM-1 on, and pick MIDI MODE on its screen | Checking the FM-1 shows up, reading what the installer says, deciding the next step |
| Put the FM-1 in USB download mode, if m-bootloader is not on it yet | Running M-Flasher, taking the backup, installing m-bootloader |
| Play it, press its buttons, say what he hears and sees | Turning that into a fix, testing the fix, rebuilding, reinstalling |

When you need his hands, give him **one** instruction at a time, in plain words, and say what he should see.

## 4. Getting it onto the FM-1

SLOOP installs from a web page over USB, and only through **m-bootloader**, a small safety program on the FM-1. It
checks every firmware before starting it. If an install is interrupted, the FM-1 opens a recovery menu, so it can
always be reinstalled. Full details: the Install section of `README.md`. The installer page is a wizard and shows
each step itself.

**The looper firmware is already built for you:** `dist/installer-sloop3-looper.html` in this repository. One file,
the firmware inside. Open it in **Chrome or Edge** (it needs WebMIDI; Safari will not work). You do not need to build
anything to install.

Do it in this order:

1. **Clone this repository, branch `loop`.** Check `dist/installer-sloop3-looper.html` is there.
2. **Find out what is on the FM-1.** Ask Jordan to hold PLAY while switching it on. If a menu with MIDI MODE appears,
   m-bootloader is there: go to step 4. If it starts normally, it is not: step 3.
3. **m-bootloader, once.** Use [M-Flasher](https://github.com/jvitkauskas/m-flasher) (it runs on macOS). Jordan puts
   the FM-1 in USB download mode (the installer page shows how). **Back up the whole flash first** and keep the file
   somewhere safe, named with today's date: it is his way back to the factory firmware. Then install
   `fm1-uboot.boot` from the [latest m-bootloader release](https://github.com/jvitkauskas/m-bootloader/releases/latest).
4. **Recommended: install official SLOOP 3 first**, from <https://isod89.github.io/sloop3-fm1/>. Have Jordan play it
   for a minute. This proves the unit, the cable and the install path with firmware other people already run, so
   that anything odd afterwards is the looper and not the device. He can skip this if he wants; say what it buys.
5. **Install the looper build:** open `dist/installer-sloop3-looper.html`, follow the wizard: connect (hold PLAY at
   power-on, MIDI MODE, plug in), **back up** (the page saves the whole flash to Downloads; move it next to the
   first backup), install (about a minute).
6. **Confirm it started.** The TRACKS page should show five tracks. Knob 3 on an empty track should read `BARS`
   and `FREE`. If it reads `STEPS`, the looper build is not what is running.

If anything fails: stop, do not retry blindly, read what the page or the FM-1 says. The recovery menu means the
safety net worked. The backups are the way back. Tell Jordan plainly what happened.

Risk, said honestly: SLOOP 3 is a public beta and unofficial. This fork adds untested-on-hardware code on top. The
bootloader makes a bad install recoverable; it does not make the looper correct.

## 5. Testing it with Jordan

`docs/LOOPER.md` is the user guide; walk him through it as he plays. What to check, in order, and what should
happen:

1. **First loop.** Empty track, REC, play, REC. It loops at once and the BPM changes to fit what he played. The
   track's number turns into a ring that fills as the loop goes round. During the take: a red disc that lights at
   each key.
2. **Second loop, in time.** Another empty track, REC in the middle of a bar. Screen: `REC AT THE BAR`, the row
   blinks red. Recording starts on the bar line. REC again mid-bar: `CLOSING AT THE BAR`. It closes on the bar line
   and stays locked to the first loop. **This is the heart of the project. Ask him: does it sound in time?**
3. **As played.** His notes should sound exactly as he played them, not snapped to a grid.
4. **BARS.** Knob 3 on an empty track: FREE, 1, 2, 4, 8. Set 2, record: it stops by itself after 2 bars. Each
   empty track keeps its own choice.
5. **BARS on the first loop.** New project, BARS on 4, record about 8 seconds: 4 bars at about 120 BPM.
6. **Undo one track.** Hold SEL, press OCT-: only the selected track's last change goes. OCT+ brings it back.
   EDIT + OCT still undoes the last change anywhere.
7. **The 72-second bug.** Leave it on for a few minutes, recording a first loop now and then (new project each
   time). A take must never close by itself with its notes piled on the first beat. See section 8.
8. **Overdub, mute, clear.** REC on a playing loop adds to it (yellow row). Hold GLO + white key 1-5 mutes. Hold
   REC 2 seconds clears.
9. **Readability.** Can he read the ring, the pie fill and the note lane on the small screen?
10. **Feel of the forgiveness.** A first note a hair before the bar line is kept; REC a hair after a bar line counts
    as on it. Too generous, too strict, or right?

Write down what he says in his words. Items 2, 9 and 10 could not be judged without the real device.

There is a 44-second film of all this running on the PC twin, made by the `loopfilm` twin scenario. Jordan has
seen it. You can remake it after any change (section 7).

## 6. What was built

Four commits on top of SLOOP 3.3.7, each complete by itself:

| Commit | What |
|---|---|
| `ce511ab` | The looper layer "V0-V2", as Jordan had it: free-form note takes, BPM from the first loop, arm / close / overdub. Written before this session, by another agent. |
| `f32f128` | **Timing and BARS.** Later loops start and end on bar lines. BARS chooses FREE / 1 / 2 / 4 / 8. Two bugs of the V0-V2 layer fixed (section 8). |
| `1ca4e4b` | **Per-track undo.** SEL + OCT- / OCT+. |
| `08e21ef` | **The view.** Ring and pie, row colours, note lane, FILTER knob. `docs/LOOPER.md`, the spec. |
| `e5caeda` | The `loopfilm` twin scenario. |

Where the code is:

- `seq/loop.c`, `seq/loop.h`: the take. Arm, capture, the two bar lines, the close. No hardware in it.
- `seq/seq.c`: `seq_undo_trk`, `seq_redo_trk`, `seq_heard`, `seq_catch_up`.
- `fw/main.c`: the adapter. REC handling, BARS, SEL + OCT, what the TRACKS page is told to show. One 4,400-line
  file; search for `ltake`, `bars_pick`, `loop_`, `undo_track`, `trk_played`.
- `app/ui.c`: `page_tracks` draws the ring, the pie and the lane. `app/ui.h`: the `UL_*` flags.
- Tests: `tests/loop_test.c`, `tests/seq_test.c`, `tests/ui_shots.c`. Twin scenarios in `host/twin.c`: `loop`,
  `loop2`, `loop3`, `loop4`, `loop5`, `loopwrap`, `loopfilm`.

## 7. Changing it: test, build, measure

**Tests, on the Mac, no special tools:** `make test`. Needs a C compiler, Python 3 with NumPy and Pillow, Node.js.
Must be all green before and after any change. Loop tests: 184 checks. Sequencer: 127.

**The PC twin** runs `fw/main.c` itself on a simulated FM-1: screen, buttons, knobs, sound. It needs
`build/fw/fw.area` to exist (for the sound samples). If you have not done a device build, make one from the
installer: the image is the long base64 block inside `dist/installer-sloop3-looper.html`; decode it to
`build/fw/fw.area`. Then `make build/twin` and `./build/twin <scenario> <outdir> - 1`. `sh host/twin.sh` runs them
all (about 5 minutes; 88 lines, all `ok`, when this was handed over). Scenarios save screenshots (`.ppm`) you can
look at. `loopfilm` writes `film.rgb565` (240x240, 30 a second) and `film.wav`; ffmpeg turns them into a video.

**The device firmware** needs the pi32v2 LLVM toolchain, release `pi32v2-23.1.5-2`
(<https://github.com/jvitkauskas/pi32v2-llvm>). The README documents the Linux x86-64 download and its checksum.
On the Mac: look at that release for a macOS build; if there is none, build in a Linux container. Then
`fw/build.sh /path/to/toolchain/bin` writes `build/fw/installer-fm1.html`: that file is what gets installed.
**After a rebuild, copy it over `dist/installer-sloop3-looper.html` and commit, so the repo always holds the
installer for its own code.**

**Space.** The firmware has 278,240 bytes. This build uses 273,932: **4,308 free.** Check `build/fw/app.bin` after
every device build. Jordan's rule: use the space thoughtfully by writing little code, not by being over-cautious.
Reuse what SLOOP has. Memory (RAM) is the scarcer budget in SLOOP; this work added 72 bytes.

**Likely first tuning knob:** the forgiveness around bar lines is one constant, `LOOP_NEAR` in `seq/loop.h` (half a
step, about 60 ms at 120 BPM). If Jordan finds it too loose or too tight, change that and nothing else.

**Commits:** small, one change each, message says what and why in SLOOP's plain style. Push to `loop`. Keep
`docs/LOOPER.md` true. GPL-3.0: the fork stays public.

## 8. Things you should know before he finds them

**Fixed in this build, never yet seen on hardware. Watch for them:**

- **Takes broke every 71.6 seconds.** The V0-V2 layer divided the FM-1's 60 MHz timer before subtracting; the timer
  wraps every 71.6 s and a take open across the wrap closed by itself with its notes on the first tick. Fixed
  (`loop_us` in `fw/main.c`), covered by the `loopwrap` scenario. Test item 7 checks it for real.
- **Every loop played snapped to the grid.** SLOOP starts every track with QUANT at 100. A take's track now goes to
  QUANT 0 when the take closes. To tighten a loop on purpose: STEPS page, QUANT.

**Known, left as they are:**

- On an empty track, the REC screen's MODE knob (FREE / TEMPO) chooses nothing: REC on an empty track is always a
  free take. That came with the V0-V2 layer.
- An overdub pass where nothing is played still uses one undo step. SLOOP's behaviour.
- A take whose every note lands exactly on a step shows as step cells, not as a lane. A hand cannot do it; a
  sequencer feeding MIDI can.
- A note on a new loop's first tick sounds a few milliseconds late on its first pass only (the close runs just
  after the bar line and plays what the line passed). If Jordan hears a flam on the first downbeat, this is why.
- If he waits a bar before playing, that silent bar is part of the loop. That is the rule working.
- The first loop is fitted to whole bars, so its notes are stretched by a few percent at most, all in proportion.
  Later loops are never stretched.

**Limits by design:** 8 bars and 128 notes a loop (SLOOP's pattern format). Undo holds 8 changes across all tracks
together; a new change anywhere forgets every redo. `LOOP FULL: 128 NOTES` tells him when a take had more.

## 9. The decisions, so you do not reopen them

Each was settled with Jordan. The reasons are in `docs/looper/spec-slices.md`.

- **No separate LOOP page.** Loops show on the TRACKS page (spec, section 3).
- **When in doubt, keep SLOOP's way.** Mute, solo, clear, level, transpose all use SLOOP's own gestures.
- **The first loop is free and sets the tempo. Every later loop starts on the next bar line and ends at the end of
  the bar he is in.** His rule, his words. Quantize the bar, never the notes.
- **A small forgiveness around bar lines.** He approved it.
- **BARS: FREE, 1, 2, 4, 8, chosen before recording, remembered per empty track.** His correction; an earlier draft
  had cut it.
- **Per-track undo is SEL + OCT.** EDIT + OCT stays as SLOOP has it.
- **Max 8 bars, not 16.** 16 needs SLOOP's pattern and save formats changed. Parked; he agreed.
- **No PITCH knob, no FILTER/PITCH toggle.** Transpose is hold EDIT + knob 3, already in SLOOP. Knob 1 is FILTER.
  He agreed.
- **No "loop mode" and nothing new saved.** Which tracks are loops is read from the notes each time.
- **Notes only.** No audio loops, no resampling, no new sounds or effects.

## 10. If he asks for more

Parked ideas, each its own piece of work, none started: 16-bar loops; a live PITCH knob with a readout; notes
appearing on the lane while recording; overdub level (DUB); nudging a loop earlier or later; a LOOP page. Before
building any, check the space (section 7) and ask him what it should feel like.

Upstream SLOOP is under active development (`github.com/isod89/sloop3-fm1`). Merging newer upstream into `loop` will
conflict in `fw/main.c`; do it only when he wants an upstream fix, and run everything in section 7 afterwards.

## 11. Other places things live

- Jordan's Google Drive has the earlier documents: the base spec for V0-V2, the glossary, two decision records, the
  HTML prototype, and several spec revisions. **The repository is the truth.** The Drive copies named "(superseded)"
  are dead. The glossary and the prototype are out of date in places; where they disagree with
  `docs/looper/spec-slices.md`, the spec wins.
- `dist/editor-sloop3-looper.html` is SLOOP's web editor built from this code. It talks to the FM-1 over USB MIDI
  and may be useful to you for looking at a project without asking Jordan to read the screen. See `app/edproto.c`
  for what it can read and do.
