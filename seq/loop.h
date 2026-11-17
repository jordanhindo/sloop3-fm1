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

typedef struct { uint32_t on, off; uint8_t note, vel, open; } loop_ev; /* 12 B, same size as ft_ev (not the same shape: ft_ev carries a per-event trk; loop_ev drops it, the module filters on armed_trk); on/off are elapsed µs since t0 */
typedef struct {
    int8_t   armed_trk;   /* -1 = no take open */
    uint8_t  rec;         /* capturing */
    uint16_t n;           /* events captured */
    uint32_t t0_us;       /* absolute time of the first note */
    int8_t   arm_pat;     /* the track's pattern index at arm time (close cancels if it changed) */
    loop_ev  ev[256];     /* the capture buffer, moved from ft_ev */
} loop_t;

void loop_init(loop_t *l);
void loop_fit(uint32_t d_us, float set_bpm, int *bars, float *bpm); /* pure: 1/2/4 bars, 3% rule, 40-250; always returns a pair; fallback bars=1, bpm=set_bpm */
int  loop_arm(loop_t *l, seq_t *s, int trk);            /* 1 if armed, 0 if refused (content track, a capture open, or Q.recording); arming a second track while one is armed but not capturing moves the arm */
void loop_note(loop_t *l, int trk, int note, int vel, int down, uint32_t now_us);
int  loop_follow_bars(seq_t *s, int trk, uint32_t d_us); /* follow-mode bars: round(d / bar_us) at the current BPM, clamped 1..8, the div clamp, 2BAR odd-round — the same count the close uses, and loop_say's preview */
int  loop_close(loop_t *l, seq_t *s, uint32_t now_us);  /* bars closed, 0 = empty take, -1 = pattern changed mid-take (cancelled) */
void loop_cancel(loop_t *l);                            /* drops the take */
int  loop_poll(loop_t *l, seq_t *s, uint32_t now_us);   /* 24 s auto-close; delegates to loop_close; bars closed, 0 = nothing closed, -1 propagates (pattern changed mid-take) */
#endif
