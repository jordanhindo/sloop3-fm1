# The looper

A free-form looper on top of SLOOP 3: play something, close it, and it loops. Stack more on the other tracks.
Notes only (the FM-1's own sounds and MIDI). Nothing here changes how SLOOP works when you do not record a take.

## Making loops

**The first loop sets the tempo.** With the transport stopped, select an empty track (ALGORITHM knob), press **REC**,
and play. Your first note starts the take. Press **REC** again to close it: the loop starts playing at once and the
tempo is worked out from how long you played.

**Every loop after that lines up to the bar.** While the first loop plays, select another empty track and press
**REC**:

1. The screen says `REC AT THE BAR`. Recording starts on the next bar line. What you play before the line is heard
   but not recorded.
2. Play. Each note is kept exactly where you played it: nothing is snapped to the steps, nothing is stretched.
3. Press **REC**. The screen says `CLOSING AT THE BAR` and recording runs on to the end of the bar you are in.
4. The loop is exactly the bars in between, and it starts looping on that bar line, in time with the others.

A hair early or late is forgiven: a first note just before the bar line is kept, and REC pressed just after a bar
line counts as on it (about 60 ms at 120 BPM).

**PLAY** while a take is waiting or recording cancels it.

## BARS: how long

On the TRACKS page, **knob 3 is BARS** when the selected track is empty. Choose before you record:

| BARS | What happens |
|---|---|
| **FREE** | You decide as you go: it records until you press REC, and stops on the bar line. At most 8 bars. |
| **1, 2, 4, 8** | Exactly that many bars. A later loop stops by itself there. Press REC sooner for a shorter loop. |

For the **first** loop there is no tempo yet, so a number does something different: the take *is* that many bars,
and the tempo is worked out from it. (The same 8 seconds are 4 bars at 120 BPM or 2 bars at 60: you say which.) On
FREE, SLOOP picks 1, 2 or 4 bars, whichever is nearest the tempo set.

Each empty track remembers its own choice, so you can decide ahead. All are FREE again after power-on or a load.
While a take is armed with the transport stopped, the REC screen's BARS knob (knob 2) is the same choice.

Once a track holds a loop, knob 3 shows its length and steps it through 1, 2, 4, 8 bars. Shortening hides the notes
past the end; lengthening brings them back. No note moves.

## What the TRACKS page shows

- **Square number:** an empty track. You can record a loop here.
- **Number in a ring:** the track holds notes. The ring fills like a pie as its loop goes round.
- **Red row:** a take is recording on that track. It blinks while it waits for its bar line, and during that time
  the ring fills once a bar, so you can see the line coming.
- **Red disc:** the first take. It lights at each key you play.
- **Yellow row:** you are overdubbing that track.
- **Note lane:** on a track played by hand, the 16 step cells give way to one lane showing every note where it is in
  the whole loop, with a line for the playhead.
- **Knobs** on an empty or a played track: FILTER, LEVEL, BARS, PAN. FILTER is the track's own filter (the same one
  as FX held + knob 4): left darkens, right thins, OFF in the middle. A track programmed on the steps keeps SWING,
  LEVEL, STEPS, PAN.

## Adding, undoing, clearing

| To do this | Do this |
|---|---|
| Overdub a loop | Select it, press **REC** while it plays, play, press **REC** again |
| Undo the selected track only | Hold **SEL**, press **OCT-** (redo: **OCT+**) |
| Undo the last change anywhere | Hold **EDIT**, press **OCT-** (redo: **OCT+**), as always |
| Mute / solo | Hold **GLO**, white key 1-5 / 6-10, as always |
| Clear the track | Hold **REC** for 2 seconds, as always |
| Transpose a loop | Hold **EDIT**, turn knob 3, as always |
| Tighten a loop onto the steps | STEPS page, QUANT. A take starts on QUANT 0 (as played); turning it back to 0 restores it |

Limits worth knowing: a loop holds 128 notes (`LOOP FULL: 128 NOTES` tells you when a take had more) and is at most
8 bars. Undo remembers 8 changes across all tracks together, and a new change anywhere forgets every redo.

## For developers

- `seq/loop.c`, `seq/loop.h`: the take (arm, capture, the two bar lines, the close). No hardware; tested by
  `tests/loop_test.c`.
- `seq/seq.c`: `seq_undo_trk`, `seq_redo_trk`, `seq_heard`, `seq_catch_up`; tested by `tests/seq_test.c`.
- `fw/main.c`: the adapter (REC, BARS, SEL + OCT, the TRACKS view's data). `app/ui.c`: the ring, the pie, the lane.
- Twin scenarios: `loop`, `loop2`, `loop3`, `loop4`, `loop5`, `loopwrap` (`host/twin.c`).
- A "played track" is worked out from the notes (one of them is off the steps), so nothing new is saved and it is the
  same after a save, a load or an undo. A take whose every note falls exactly on a step shows as step cells.
- The design and its reasons: `docs/looper/spec-slices.md`.
