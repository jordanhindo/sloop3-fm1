/* SPDX-License-Identifier: GPL-3.0-only
 * The free-form looper: one open take, its capture buffer, the fit
 * and the close. Hardware-free (seq/ layer), unit-testable with
 * injected times. The caller owns the loop_t; fw/main.c holds one
 * static instance (ltake). Moved from ft_fit / ft_close / ft_ev in
 * fw/main.c, not rewritten — except the close, which sets only the
 * armed track's length, batches the whole close into one undo step,
 * and cancels if the pattern changed mid-take. */
#ifndef SEQ_LOOP_H
#define SEQ_LOOP_H
#include "seq.h"
#include <stdint.h>

typedef struct { uint32_t on, off; uint8_t note, vel, open; } loop_ev; /* 12 B; on/off: a first take's are elapsed µs since t0, a later take's the sequencer's ticks */
typedef struct {
    int8_t   armed_trk;   /* -1 = no take open */
    uint8_t  rec;         /* capturing */
    uint16_t n;           /* events captured */
    uint32_t t0_us;       /* absolute time of the first note (a first take) */
    int8_t   arm_pat;     /* the track's pattern index at arm time (close cancels if it changed) */
    uint8_t  follow;      /* a later take (armed while playing): cut out of the bar grid along bar lines */
    uint8_t  target;      /* the bars chosen: 0 FREE, or 1, 2, 4, 8 */
    uint8_t  closing;     /* (follow) REC pressed: the end line is the next bar line */
    uint8_t  full;        /* the last close dropped notes: the pattern holds SEQ_PATEV */
    uint32_t start, end;  /* (follow) the start and end lines, the sequencer's ticks */
    loop_ev  ev[256];     /* the capture buffer, moved from ft_ev */
} loop_t;

#define LOOP_NEAR (SEQ_TPS / 2)   /* a hair's breadth: half a step either side of a bar line counts as on it */

void loop_init(loop_t *l);
void loop_fit(uint32_t d_us, float set_bpm, int *bars, float *bpm); /* pure: 1/2/4 bars, 3% rule, 40-250; always returns a pair; fallback bars=1, bpm=set_bpm */
int  loop_arm(loop_t *l, seq_t *s, int trk, int target); /* 1 if armed, 0 if refused (content track, a capture open, or Q.recording); arming a second track while one is armed but not capturing moves the arm. Playing: a later take, recording from the next bar line (a line just passed, within LOOP_NEAR, is that line) */
void loop_note(loop_t *l, int trk, int note, int vel, int down, uint32_t now);   /* now: µs (a first take), or the tick the note was heard on (a later take: seq_heard) */
int  loop_max_bars(const seq_t *s, int trk);             /* the most a take on this track may be: 8 bars, fewer where its DIV holds fewer in 128 steps */
void loop_stop(loop_t *l, seq_t *s);                     /* (follow) REC pressed while recording: it ends on the next bar line (a line just passed, within LOOP_NEAR, is that line) */
int  loop_close(loop_t *l, seq_t *s, uint32_t now_us);  /* bars closed, 0 = empty take, -1 = pattern changed mid-take (cancelled) */
void loop_cancel(loop_t *l);                            /* drops the take */
int  loop_poll(loop_t *l, seq_t *s, uint32_t now_us);   /* a first take: the 24 s auto-close. A later take: recording from the start line, closed on the end line. Bars closed, 0 = nothing closed, -1 = cancelled */
#endif
