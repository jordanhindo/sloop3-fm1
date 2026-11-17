# Sloop 3 free-form looper: UX slices spec (revision 4, as built)

**Status:** revision 4, Nov 17, 2026. **Built**, on branch `loop` of `jordanhindo/sloop3-fm1`. All three slices pass the host tests, the full twin suite and a device build. Not yet run on an FM1. Section 11 lists what the build changed in this spec and what the review of the V0-V2 layer found. The user guide is `docs/LOOPER.md`.

**Base:** Sloop 3 (`github.com/isod89/sloop3-fm1`, pinned `915b54f`, 3.3.7) plus the looper layer V0-V2 (branch `loop`, `f9119bf`). The base looper is specified in "Sloop looper base spec V0-V2" (rev 5). This spec adds three user-facing slices on top: (1) the view of loops on the TRACKS page, (2) per-track undo, (3) timing.

**Goal, in Jordan's words:** really nice, easy and intuitive, but not lacking. A free-form looper inspired by the RC-505, not a clone. Notes only, internal sounds and MIDI. When in doubt, keep Sloop's way (ADR 0002).

## 0. What revision 3 changes, in one table

Revision 2 was checked line by line against the fork code. `make test` passes on the tarball as delivered (loop tests: 154 checks, 0 failed). The check found five places where rev 2 described something the code cannot do or already does. Those drive every change below.

| # | Rev 2 said | The code says | Rev 3 does |
|---|---|---|---|
| 1 | Loops can be 16 bars; "raise the clamp" | A pattern is 128 steps max = 8 bars, and holds 128 notes max. The loop is stored as a pattern, not as separate freeform storage. | **Max stays 8 bars.** 16 is parked (section 7). |
| 2 | A track "holds a loop" or is a "pattern track"; knobs and display switch on that | Nothing in memory or in the save file marks a pattern as a loop. After a close it is an ordinary pattern. The base spec cut "stored loop state" for this reason. | **No loop mode, no stored state.** Two things read from the notes themselves: empty, and played (section 2). |
| 3 | New FILTER/PITCH knob with a new gesture (hold FX + tap SEL) | Sloop already has both: hold FX + knob 4 is the track filter; hold EDIT + knob 3 is transpose (one undo per hold). | **Knob 1 is FILTER only. PITCH stays on hold EDIT + knob 3. The new gesture is cut.** |
| 4 | Later loops close at the bar end, "in phase", notes never moved | Not said: where the loop starts. The prototype starts it at the first note, which shifts every note against the bar on playback. | **Recording starts on a bar line and ends on a bar line** (section 5). Jordan's rule. |
| 5 | Per-track undo "filters the history" | The base spec cut per-track undo because that exact sentence was not an algorithm. | **Exact algorithm given and proven** on the A1/B1/A2 case (section 4). |

Also corrected: SWING is not "nothing to act on" for loops (it acts on any note near an odd step); the space figure both specs quoted measures the sample bank, not the code (section 1); three stale line references.

Jordan's decisions that rev 3 keeps unchanged: TRACKS page instead of a LOOP page, the circle, red and yellow rows, the continuous note lane, the hit pulse, ALGORITHM selects and REC arms, Sloop's mute/solo/clear, SEL+OCT for per-track undo with EDIT+OCT left global, quantized close always on with rounding up, first loop free-form.

Jordan's decisions that rev 3 changes, each with its reason in the section named: 16 bars (3.5, 7), the FILTER/PITCH toggle (3.5).

## 1. Facts from the code that shape everything

All references are to the fork tarball, `fw/main.c` unless noted.

- **A loop is a pattern.** `loop_close` (`seq/loop.c` 79-114) writes the take into the track's current pattern with `seq_set_len` and `seq_add`. Limits: 128 steps (`SEQ_MAXSTEPS`), 128 notes (`SEQ_PATEV`), both in `seq/seq.h`. Notes past 128 are dropped silently.
- **Notes are stored to the tick.** 96 ticks per 16th step (about 1.3 ms at 120 BPM). That is what "full resolution" means here.
- **A free take only happens on an empty track.** `loop_arm` refuses a track with notes (`seq/loop.c` 42). On a track with notes, REC while playing is Sloop's own recording, which is the overdub.
- **Today's later-loop close stretches and re-phases.** While playing, the bar count is rounded to nearest and the notes are scaled to fit (`k = P/d`, line 103), and the loop starts at the moment of the click with its own phase (`loop_closed`, ~3134).
- **The first loop is scaled a little too.** With the transport stopped, `loop_fit` picks 1, 2 or 4 bars and a whole-number BPM (or keeps the set tempo if within 3%), then scales the take to fit exactly. Timing between notes is kept in proportion. This is V0-V2 behaviour and stays.
- **Undo is one shared history, 8 deep** (`seq/seq.c` 45-79). Each entry is a full copy of one pattern, tagged with its track. Any new edit clears redo.
- **SWING and QUANT act at playback, not on stored notes** (`eff_tick`, `seq/seq.c`). SWING defaults to 0. **QUANT defaults to 100 on every track of a project** (`fw/main.c`, "a take on the steps"), so until this build every loop was stored exactly but heard snapped to the steps. Revisions 3.0 to 3.4 of this spec said QUANT defaulted to 0; that was wrong. As built, a take's track goes to QUANT 0 when the take closes, and QUANT on the STEPS page can pull a loop in again without changing it. The line rev 2 cited for swing (1884) is the arpeggiator's swing.
- **Shown track number is not the internal index.** `POS` (line 129) puts the drums 5th on screen while they are index 3 inside. Colours on screen come out blue, green, yellow, pink, orange, as the prototype shows. The code and the twin agree; rev 2's "the code swaps 4/5" was a misreading. Always go through `POS`.
- **Space for code, measured.** The firmware has 278,240 bytes in front of the sample bank (`tools/mkfw.py`, `BANK_AT`). Built with the project's own toolchain:

  | Build | Firmware bytes | Free |
  |---|---|---|
  | SLOOP 3.3.7 as shipped | 269,900 | 8,340 |
  | + looper layer V0-V2 | 270,504 | 7,736 |
  | + slice 3, timing and BARS | 272,012 | 6,228 |
  | + slice 2, per-track undo | 272,600 | 5,640 |
  | + slice 1, the view | 273,932 | 4,308 |

  The three slices cost 3,428 bytes together, more than the 1.5 to 2.5 KB estimated in rev 3.4, and fit. The figure both earlier specs used (472,658 of 475,136) is the total with the sample bank; it never changes with the code. Memory: 72 bytes added.

## 2. Vocabulary

- **Take:** one recording pass on one empty track: arm, play, close.
- **First take:** a take closed while the transport is stopped. It sets the BPM.
- **Later take:** a take captured while the transport is playing. It follows the BPM and the bar grid.
- **Bar grid:** the bar lines of the running transport, the same ones the top line's bar.beat counter shows.
- **Empty track:** its current pattern has no notes. Only an empty track can take a free take. Shown with the **square** number.
- **Played track:** its current pattern has at least one note that is not exactly on a step. Every hand-played take is one. Shown with the **note lane** and the loop knobs. This replaces "loop mode" and "loop track". It is worked out from the notes each time, so it survives save, load and undo with nothing stored.
- **Quantized close:** ending a later take on a bar line so its length is a whole number of bars. The bar is quantized; the notes are not.
- **Per-track undo / global undo:** as in the glossary.

Known corner: a first take of a single note lands exactly on step 1, so it shows as a step cell with Sloop's knobs until a second note is overdubbed. It plays the same either way.

## 3. Slice 1: the view on the TRACKS page

No new page (ADR 0001). Only the TRACKS page changes, and only as listed.

### 3.1 The number: square or circle

- **Empty track:** square number, as today. It tells you where you can record.
- **Track with notes:** the number sits in a **circle** in the track colour, same 21 px spot.
- **Muted:** as today, the colour dims and the MUTE badge shows. The circle stays.

### 3.2 The fill

- **First take, while capturing:** solid red circle. Each key press flashes it lighter for about 0.2 s. No steady pulse, so nothing looks like a tempo.
- **Track with notes, transport playing:** ring in the track colour that pie-fills clockwise from the top. One full fill is one pass of that track's loop.
- **Later take, from REC press to the close:** ring that fills **once per bar**. This is the bar clock: it shows the bar line coming, both for the start and for the end.
- **Transport stopped:** ring, no fill.

### 3.3 Row colour

- Dark red row while a take is recording on that track. During the wait for the first bar line the row blinks dark red, so waiting and recording look different.
- Dark yellow row while overdubbing that track (`Q.recording && Q.rec_trk == t`).
- Back to normal the moment it ends. The existing REC badge and the red TAKE status stay as they are.

### 3.4 The note lane

- On a **played track**, the strip of 16 step cells is replaced by one continuous lane in the same place. Each note is a small mark at its position across the whole loop. A playhead line crosses it as the loop plays.
- The lane shows the whole loop at once. (The cells only ever show the 16 steps around the playhead.)
- Notes parked past the loop's end are not drawn.
- All other tracks keep the step cells. The level bar stays where it is.
- While a take is open, the lane is empty and the notes appear at the close. The existing "TAKE 3.2s · 2 BARS 120" readout keeps running. Rev 2 did not say; drawing a growing lane during capture costs flash and adds nothing the readout lacks.

### 3.5 The knobs

The knob row follows the selected track.

| | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
|---|---|---|---|---|
| Empty track or played track | **FILTER** | LEVEL | **BARS** | PAN |
| Step-programmed track | SWING | LEVEL | STEPS | PAN (as today) |

- **FILTER** is Sloop's existing per-track filter, the same value hold FX + knob 4 already turns (`S.trk[trk].filt`). Centre is OFF, left darkens, right thins, shown the way Sloop shows it. Rev 2's "0 dark to 100 open" does not match the filter Sloop has.
- **PITCH is not on this knob.** Transposing a loop is hold EDIT + knob 3, which exists today and counts one whole turn as one undo. Reasons for cutting the toggle:
  1. ADR 0002: invent a gesture only when Sloop has none. Sloop has one.
  2. A PITCH knob that rewrites notes costs one undo entry per click. Eight clicks would wipe the whole shared undo history, including other tracks. That breaks slice 2.
  3. A PITCH knob that only shifts playback needs a new saved value per track, which the save format does not have.
  4. Hold FX + tap SEL fights the tap logic: pressing a second layer button resets the "was this a tap" timer, so both pages try to open on release.
- **BARS** does two jobs, depending on the selected track.
  - **On an empty track it is the choice for the next take: FREE, 1, 2, 4 or 8.** Jordan's decision, restored. FREE is the default.
    - **FREE:** "I don't know yet." The take runs until REC is pressed, up to 8 bars, and ends on the bar line (section 5).
    - **1, 2, 4 or 8:** "I want exactly this many bars." Section 5 says what that does for a first take and for a later take.
    - **Each empty track remembers its own choice** (Jordan's decision), so lengths can be decided ahead: track 2 on 4, track 3 on 8. The choice shows whenever that empty track is selected and can be kept or changed before recording. Cost: 5 bytes of memory. It is not saved with the project; every empty track comes back as FREE after power-on or a project load. Erasing a track leaves its choice where it was.
  - **On a played track it is the loop's length.** It reads the real length (1 to 8). Turning it steps to the next of 1, 2, 4, 8 up or down. It uses Sloop's own length change, so shortening hides the notes past the end and lengthening brings them back. No note moves. Each step is one undo entry, as STEPS is today. There is no FREE here: a recorded loop always has a length.
  - **Max 8, not 16.** See section 7.
  - If a played track's length is not a whole number of bars (someone changed STEPS on the STEPS page), BARS shows the step count instead.
  - An empty track no longer shows STEPS on this row. Step length for programming is on the STEPS page, as today.
- **SWING** is still reachable on the GLOBAL and STEPS pages. It is only off this row for played tracks.

### 3.6 Selecting, arming, mute, solo, clear

All Sloop's, all unchanged. Written down so the acceptance tests can check them.

- ALGORITHM encoder selects track 1 to 5, wrapping.
- REC on an **empty** selected track arms a take. REC on a track with notes while playing starts the overdub. One take open at a time; arming another empty track moves the arm.
- Mute: hold GLO + white key 1-5. Solo: hold GLO + white key 6-10. Clear: hold REC 2 s.

## 4. Slice 2: per-track undo

### 4.1 Gesture

- **Hold SEL + OCT- = undo the selected track. Hold SEL + OCT+ = redo the selected track.**
- Hold EDIT + OCT- / OCT+ stay global undo/redo, untouched.
- Verified free: the OCT handlers (~3242-3271) test FX, then EDIT, and have no SEL branch. Today SEL + OCT just shifts the octave. The new branch goes right after the EDIT one and sets `layer_used`, so SEL does not open its page on release.

### 4.2 Behaviour

- Undoes the newest change on the selected track and skips changes on other tracks. Example: take A1 on track 1, take B1 on track 2, overdub A2 on track 1. Undo on track 1 removes A2, then A1. B1 is never touched. Redo on track 1 brings back A1, then A2.
- Messages: "UNDONE" / "REDONE", and "NOTHING TO UNDO" / "NOTHING TO REDO" as global undo says today.
- **While a recording is in progress on the selected track (from REC press until it closes), per-track undo and redo refuse** with "STILL RECORDING". (PLAY cancels a recording.) Otherwise undo could put notes back under a take that is about to be written.
- Limits, stated so nobody is surprised. These are Sloop's and stay:
  - The history holds 8 changes across all tracks together. Busy work on other tracks pushes the selected track's old changes out.
  - Any new change on any track clears redo for every track.
  - Undoing the first take does not undo the BPM or stop the transport.

### 4.3 The exact algorithm

History layout today: `h[0 .. n_undo)` are undo entries, oldest first. `h[n_undo .. n_undo + n_redo)` are redo entries, with the next one to redo at `h[n_undo]`. Undo and redo swap an entry with the live pattern.

Add two functions in `seq/seq.c` beside `seq_undo` and `seq_redo`:

`seq_undo_trk(s, trk)`:
1. Find the largest `i < n_undo` with `h[i].trk == trk`. None: return 0.
2. Swap `h[i].p` with the live pattern `t[trk].pat[h[i].pat]`.
3. Move entry `i` to index `n_undo - 1`, sliding the entries between down by one, order kept.
4. `n_undo--`, `n_redo++`. Return 1.

`seq_redo_trk(s, trk)`:
1. Find the smallest `j` in `[n_undo, n_undo + n_redo)` with `h[j].trk == trk`. None: return 0.
2. Swap `h[j].p` with the live pattern.
3. Move entry `j` to index `n_undo`, sliding the entries between up by one, order kept.
4. `n_undo++`, `n_redo--`. Return 1.

Why it is safe: each entry is a full copy of one track's pattern, so entries of different tracks do not depend on each other. The moved entry only ever passes entries of other tracks, so every track's own order is kept. Global undo and redo keep working after any mix of the two.

Proof on the example (history shown as undo | redo):
- After A1, B1, A2: `[A-empty, B-empty, A1] |`
- Undo track 1: `[A-empty, B-empty] | [A2]`, track 1 holds A1.
- Undo track 1: `[B-empty] | [A1, A2]`, track 1 is empty.
- Redo track 1: `[B-empty, A-empty] | [A2]`, track 1 holds A1.
- Redo track 1: `[B-empty, A-empty, A1] |`, track 1 holds A2. Track 2 held B1 throughout.

No new memory: the slide reuses the existing byte swap.

## 5. Slice 3: timing (quantized close)

### 5.1 The model

RC-505 loop sync, as Jordan described it: later loops are a whole number of measures and line up with the first. Always on. No setting.

### 5.2 First take: unchanged

Transport stopped, so there is no tempo yet and nothing to count bars against. REC ends the take. The transport starts on the downbeat. The 24 s auto-close stays.

- **BARS on FREE:** as V0-V2. The take is fitted to 1, 2 or 4 bars, whichever gives the tempo nearest the one set, and the BPM is derived.
- If the transport is started from outside (MIDI start) while a first take is armed, the take becomes a later take. If it is started while a first take is already capturing, the take is cancelled: there is no tempo left to derive.
- **BARS on 1, 2, 4 or 8:** the take **is** that many bars. The BPM is worked out from that: bars divided by how long the take lasted. This removes the guess (the same take could be 2 bars at 60 or 4 bars at 120; now Jordan says which). If the answer falls outside 40 to 250 BPM the take falls back to the FREE fit and the screen says so.

### 5.3 Later take: the rule

Jordan's rule: **recording starts at the start of the next full bar and ends at the end of the current full bar.** The loop is exactly the bars in between. Nothing inside is moved or stretched.

1. **REC press arms. Recording starts on the next bar line (the start line).** The screen says "REC AT THE BAR". Notes played during the wait sound but are not recorded. REC again during the wait disarms, PLAY cancels, both as today. This replaces V0-V2's "your note starts it" while playing.
2. **Each recorded note keeps its place on the grid.** It is captured with its transport position, the same position Sloop's own recording would write for that key press.
3. **REC press during recording does not stop at once.** Recording runs on to the next bar line (the end line). The screen says "CLOSING AT THE BAR". During this wait REC does nothing and PLAY cancels.
4. **Length** = end line minus start line, in whole bars. At least 1.
5. **Most it can be:**
   - **BARS on FREE:** 8 bars (less on tracks whose step size is slower than 1/16; the existing limits in `loop_follow_bars` stay).
   - **BARS on 1, 2, 4 or 8:** that many bars from the start line. Recording stops itself there with no REC press. Pressing REC sooner still ends it on the next bar line, giving a shorter loop.
   - Either way, a recording that reaches its limit closes itself on that bar line. This replaces the 24 s auto-close for later takes, since 8 bars is longer than 24 s below 80 BPM.
6. **Nothing played:** if no note was recorded by the end line, "EMPTY TAKE" as today and the track stays empty.
7. **Writing it:** each note goes in at its position minus the start line. Note lengths are as played; a key still held at the end line ends there. No scale factor. One undo step, as today. BPM untouched.
8. **Phase:** the loop's beginning sits exactly on the end line, not on "whenever the close code ran". It starts playing there and stays lined up with every other loop.
9. **The downbeat must sound on the first pass.** A note on the loop's first tick has to play at the end line the first time round, not one loop later.
10. **Full loop:** if the recording holds more than 128 notes, the extra ones are dropped as today, and the screen says "LOOP FULL: 128 NOTES" instead of dropping them silently.

**The hair's-breadth allowance (confirmed by Jordan, Nov 17, 2026).** Hands are never exactly on the line. One constant, half a step (48 ticks, about 60 ms at 120 BPM), used in three places:
- A note played less than that **before the start line** is recorded. Without this, a downbeat hit a touch early is thrown away, and the downbeat is the note that matters most. It is stored where it was played, just before the line, so it plays just before the loop comes round.
- REC pressed less than that **after a bar line while arming** starts recording from that line, with no wait of a whole bar.
- REC pressed less than that **after a bar line while recording** ends on that line, with no extra silent bar.

The size of the allowance is one constant. Its feel can only be judged on the FM1, so tuning it there is expected.

Worked example at 4/4: REC pressed during bar 4. Recording starts on the downbeat of bar 5. REC pressed again during bar 9. Recording ends at the end of bar 9. The loop is 5 bars (5 to 9) and begins looping on the downbeat of bar 10.

A side effect to know: waiting a bar before playing records that silent bar into the loop. That is the rule doing what it says; BARS can shorten the loop afterwards only from the end.

### 5.4 Quantize the bar, not the notes

Kept from rev 2, now true for later takes: the close decides only where the loop is cut. With SWING and QUANT at 0 (the defaults) every note plays where it was played.

## 6. What does not change

- FX: hold FX punch layer, FX + OCT throws, FX tap pages, HOME lock. No new FX gesture exists in rev 3.
- Every Sloop gesture and page when no take is used. The TRACKS knob row is unchanged on step-programmed tracks.
- The save format. Nothing in this spec stores anything new.
- The first-take fit and BPM rules, overdub, cancel, erase: all V0-V2.

One honest exception to "nothing changes for non-loop use": a pattern recorded live with Sloop's own REC is also off the steps, so it also shows the lane and the FILTER/BARS knobs. It is a played track too. STEPS and SWING stay on the STEPS page. And an empty track now shows BARS where STEPS was (3.5).

## 7. Parked, with what would bring each back

- **16-bar loops.** Needs the pattern format changed from 128 steps, and the save format, the STEPS page and the web editor with it. Even then a loop holds 128 notes, 8 per bar over 16 bars. Its own slice if Jordan wants it.
- **PITCH as a live knob with a readout.** Needs a saved per-track transpose value. Back if hold EDIT + knob 3 proves too awkward in real use.
- **Notes appearing on the lane during a take.** Back if the red row and the readout are not enough feedback.
- DUB level, NUDGE, a LOOP page, audio loops: as in rev 2 and the base spec.

## 8. Build order

**Vertical: each slice is built complete (behaviour, screen, tests) and tried before the next one starts.**

Recommended order, Jordan's slice numbers kept:

1. **Slice 3, timing.** It is the one that changes how the loops sound, and the screen states in slice 1 (the bar clock, the waiting row) show timing that has to exist first. **The BARS knob is built here**, with its FREE / 1 / 2 / 4 / 8 choice, because timing is not usable without it. Screen feedback in this slice is the messages ("REC AT THE BAR", "CLOSING AT THE BAR") and the BARS knob.
2. **Slice 2, per-track undo.**
3. **Slice 1, the view:** circle, pie, row colours, lane, FILTER knob.

Space rules, light:
- Build the device firmware once before starting and write down the real free bytes. The 8 KB figure is read from a shipped image, not from a fresh build.
- Write the size down after each slice. No cut list is needed at these sizes. If a slice costs more than 1.5 KB by itself, stop and say so.
- Memory: 5 bytes for the per-track BARS choice. The lane can travel in the 16 bytes per track the step cells already use (`cells[t][16]` in `app/ui.h`, 128 on/off columns). Anything more is reported.

Keeping the code small, without over-building:
- Reuse what Sloop has. Length changes use Sloop's own (`seq_set_len`). FILTER turns Sloop's existing filter value. Undo reuses the existing history and its swap. Overdub, mute, solo and erase are untouched. PITCH is Sloop's own transpose.
- Change the close, do not add a second one. The later-take path loses its stretch code and gains the two bar-line waits.
- Nothing new is saved, so the save format, the web editor and the importer are not touched.
- No settings page, no new page, no new gesture except SEL + OCT.

## 9. Acceptance

"Passes" means the host tests (`make test`) and twin runs. No FM1 has run this code, so hardware feel is listed apart.

### Slice 3
- [ ] Later take: REC mid-bar 4, notes in bars 4 to 9, REC mid-bar 9. The loop is 5 bars. Notes played in bar 4 are not in it. Every stored note equals its captured position minus the start line, to the tick. No scale factor in the later-take path.
- [ ] Same input through a later take and through Sloop's own recording lands each note on the same tick.
- [ ] After the close the loop's first tick falls on the end line and stays on bar lines for 16 further passes.
- [ ] A note on the loop's first tick sounds on the first pass.
- [ ] Allowance, start: a note 30 ticks before the start line is recorded; 60 ticks before is not.
- [ ] Allowance, arming: REC 30 ticks after a bar line starts recording from that line; 60 ticks after waits for the next.
- [ ] Allowance, ending: REC 30 ticks after a bar line closes at once on that line; 60 ticks after waits for the next.
- [ ] During the wait to start: notes sound and are not recorded, REC disarms, PLAY cancels.
- [ ] Nothing recorded by the end line: "EMPTY TAKE", track still empty.
- [ ] Between REC and the end line: notes are still captured, a second REC does nothing, PLAY cancels.
- [ ] BARS on FREE: a take left running closes itself at 8 bars (at 1/16). No 24 s close while playing.
- [ ] BARS on 4, later take: the take closes itself 4 bars after its start line with no REC press. REC pressed during bar 2 gives a 2-bar loop.
- [ ] First take of 8.0 s: BARS on 4 gives 4 bars at 120 BPM; BARS on 2 gives 2 bars at 60 BPM; BARS on 8 gives 8 bars at 240 BPM.
- [ ] First take of 5.0 s with BARS on 8 (384 BPM, out of range): falls back to the FREE fit and says so.
- [ ] BARS on FREE, first take: identical to V0-V2.
- [ ] 129 notes: 128 kept, "LOOP FULL: 128 NOTES" shown.
- [ ] First take: every existing V0-V2 test still passes unchanged.

### Slice 2
- [ ] A1, B1, A2, then undo, undo, redo, redo on track 1: the four states in 4.3, track 2 identical throughout.
- [ ] After any mix of per-track and global undo/redo, undoing everything globally returns every track to its starting state.
- [ ] SEL + OCT does not shift the octave and does not open the SEL page. SEL tap and plain OCT work as today. EDIT + OCT unchanged.
- [ ] Nothing to undo or redo on that track: the two existing messages. Recording in progress on that track: "STILL RECORDING", nothing changes.

### Slice 1
- [ ] Empty track: square. Track with notes: circle. Screenshots for both (`tests/ui_shots.c`).
- [ ] First take capturing: solid red, lighter for about 0.2 s after a key press.
- [ ] Playing: the pie fill equals the track's position in its loop. Later take, from REC press to the close: one fill per bar.
- [ ] Row dark red while a take is open, dark yellow while overdubbing, normal after.
- [ ] Played track: lane with one mark per note at position/length across the strip, playhead moving. Other tracks: step cells exactly as today.
- [ ] Played track selected: FILTER, LEVEL, BARS, PAN. FILTER turns the same value as hold FX + knob 4. BARS steps 1, 2, 4, 8 and moves no note.
- [ ] Empty track selected: same four knobs. BARS steps FREE, 1, 2, 4, 8, starts on FREE, and changes nothing in the project.
- [ ] Track 2 set to 4, track 3 set to 8: each shows its own value when selected again, and each records to its own length.
- [ ] Step-programmed track selected: SWING, LEVEL, STEPS, PAN as today.
- [ ] Save, power off, load: the same tracks show circles and lanes.

### General
- [ ] `make test` green, all existing twin scenarios green.
- [ ] Code size written down after each slice and within the 278,240-byte room; memory added is the 5 bytes in 3.5 and nothing unreported.

### Only on the FM1 (not provable on the twin)
- The half-step allowances feel right.
- The pie and the lane are readable on the 1.5" screen.

## 10. Notes for the builder

Code pointers, checked against the tarball (`fw/main.c` unless noted):

| What | Where |
|---|---|
| TRACKS row drawing (number box, cells, level bar) | `app/ui.c` `page_tracks` ~346-369 |
| TRACKS data per track (cells, rec, mute) | `fill_view` ~1059, loop from ~1084 |
| TRACKS knob labels / knob turns | ~1416-1421 / ~3841-3848 |
| Track filter (hold FX + knob 4) | ~3638-3651, shown at ~855 |
| Shift, double/halve, transpose (hold EDIT + knobs) | ~3750-3757 |
| OCT handlers, where the SEL branch goes | ~3242-3271 |
| Layer tap gate | `layer_used` 704, `LAYER_BITS` 706, tap test ~3290 |
| REC press (closes a take) / REC release (arm, overdub) | ~3279-3284 / ~3293-3309 |
| REC hold erase | ~3310-3315 |
| Mute and solo on the GLO layer | ~3005-3006 |
| Take poll and readout | ~3988-3994, `loop_say` ~3147 |
| Close, fit, bar count | `seq/loop.c` `loop_close` 79, `loop_fit` 23, `loop_follow_bars` 67 |
| Undo history | `seq/seq.c` 45-79 |
| Swing and quant at playback | `seq/seq.c` `eff_tick` ~534 |
| Screen position of a track | `POS` 129, `TRK_COL` 130 |

Corrections to rev 2's pointers: `ft_fit` / `ft_close` no longer exist in `fw/main.c` (they moved to `seq/loop.c`); "REC hold/erase ~3005" is the mute/solo code; "SWING applied 1884" is the arpeggiator.

Things to decide while building, not before:
- How a later take gets its positions (pass the transport tick into `loop_note` in place of microseconds, or convert). The requirement is acceptance test 2 of slice 3.
- How the downbeat is made to sound when the close code runs a few ticks after the end line.
- How the pie is drawn. `gfx_disc` and `gfx_line` exist; there is no arc.

Not verifiable from the tarball: the two commit ids (no git history inside), the twin scenario folders `build/tw/loop` to `loop4` (build output, not shipped), and a fresh device build (toolchain not shipped; the size in section 1 is read from the shipped installer image).

Reference files: the prototype is still the visual reference for the look. It is wrong in three places where this spec wins: it starts a later loop at the first note, where this spec starts it on the next bar line (5.3), it gives each track its own 8-deep undo (4.2), and it shows 16 bars and the knob toggle (3.5). The glossary needs "loop mode", "circle" (pulse) and "quantized close" (start line) brought in line with this revision.

## 11. As built: what changed, and what the review of V0-V2 found

**Found in the V0-V2 layer and fixed:**

1. **Takes broke every 71.6 seconds.** The take's clock divided the FM-1's 60 MHz timer before subtracting. The timer wraps every 71.6 s, and a take open across the wrap closed by itself with its notes piled on the first tick. The base spec said this was safe; it was not. Twin scenario `loopwrap` reproduces it and now passes.
2. **Every loop played snapped to the steps.** See QUANT in section 1. A take's track now goes to QUANT 0 at the close. On the REC screen, QUANT reads OFF while a take is armed.

**Found and left alone (SLOOP's own behaviour, noted for Jordan):**

- On an empty track the REC screen's MODE knob (FREE / TEMPO) no longer chooses anything: V0-V2 makes REC on an empty track always a free take.
- An overdub pass with no notes played still counts as one undo step.
- A take of notes that all fall exactly on a step (a machine can do it, a hand cannot) shows as step cells, since "played" is read from the notes.

**Changed from rev 3.4 while building:**

- The BARS choice on an empty track is also on the REC screen (knob 2) while a take is armed with the transport stopped, since that screen covers TRACKS then.
- Turning BARS on a track whose take is waiting for its bar line changes that take.
- The later-take readout is `TAKE · BAR 2` (or `BAR 2 OF 4`), in place of seconds.
- New sequencer calls: `seq_heard` (the tick a key is heard on, as SLOOP's own recording counts it) and `seq_catch_up` (plays what the bar line has already passed, so the downbeat sounds on the first pass).
- The pie is drawn from a 17-entry sine table; the lane is 128 columns carried in the step cells' 16 bytes.
- Section 8's rule "stop if a slice costs more than 1.5 KB" was met by slice 3 at 1,508 bytes; with 6 KB free the build went on.

**Verified:** `make test` (all green; loop 184 checks, seq 127), the full twin suite (88 lines, 0 failed, including the random-input run), `fw/build.sh` (device image, installer simulation, installer end to end). **Not verified:** anything on a real FM-1; the feel of the half-step allowance; readability on the 1.5" screen.
