/* SPDX-License-Identifier: GPL-3.0-only
 * The free-form looper: one open take, its capture buffer, the fit
 * and the close. Moved from ft_fit / ft_close / ft_ev in fw/main.c.
 * Time is injected as absolute monotonic microseconds (now_us); the
 * module stores t0_us at the first captured note-on, so the
 * arm-to-first-note gap never counts. The module never formats a
 * message and never calls say(): it returns bars, the caller formats
 * them. */
#include "loop.h"

#define LOOP_MAX 256
#define LOOP_MIN_US 400000u          /* a take shorter than 0.4 s is empty */
#define LOOP_MAX_US 24000000u        /* the 24 s auto-close (strictly greater) */

static void clear_take(loop_t *l) { l->armed_trk = -1; l->rec = 0; l->n = 0; }

void loop_init(loop_t *l) { loop_cancel(l); }   /* loop_cancel on a fresh instance */
void loop_cancel(loop_t *l) { clear_take(l); }

/* the fit, moved from ft_fit: try 1, 2 and 4 bars; the bar count whose
 * tempo is nearest the tempo set wins; within 3% the set tempo is kept;
 * if no bar count is valid, the fallback is nb=1 and the set BPM */
void loop_fit(uint32_t d_us, float set_bpm, int *bars, float *bpm)
{
    static const int NB[3] = { 1, 2, 4 };
    float best = 1e9f;
    *bars = 1; *bpm = set_bpm;
    for (int i = 0; i < 3; i++) {
        float b = (float)NB[i] * 4.0f * 60.0e6f / (float)d_us;
        if (b < 40.0f || b > 250.0f) continue;
        float r = b > set_bpm ? b / set_bpm : set_bpm / b;
        if (r < best) { best = r; *bars = NB[i]; *bpm = b; }
    }
    if (best < 1.03f) *bpm = set_bpm;                              /* near enough: the tempo set */
    else *bpm = (float)(int)(*bpm + 0.5f);
}

int loop_arm(loop_t *l, seq_t *s, int trk)
{
    if (s->recording) return 0;                    /* grid recording in progress */
    if (l->rec) return 0;                          /* a capture open */
    if (seq_count(s, trk) != 0) return 0;          /* a content track */
    l->armed_trk = (int8_t)trk;
    l->arm_pat = (int8_t)s->t[trk].cur;
    return 1;
}

void loop_note(loop_t *l, int trk, int note, int vel, int down, uint32_t now_us)
{
    if (trk != l->armed_trk) return;               /* only the armed track's notes */
    if (down) {
        if (!l->rec) { l->n = 0; l->t0_us = now_us; l->rec = 1; }   /* the first captured note-on */
        if (l->n == LOOP_MAX) return;              /* the buffer is full: dropped */
        loop_ev *e = &l->ev[l->n++];
        e->on = now_us - l->t0_us; e->off = 0; e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->open = 1;
    } else {
        for (int i = (int)l->n - 1; i >= 0; i--)   /* close the most recent open event of the same note */
            if (l->ev[i].open && l->ev[i].note == (uint8_t)note) {
                l->ev[i].off = now_us - l->t0_us; l->ev[i].open = 0;
                return;
            }
    }
}

/* the follow-mode bar count: bars at the current BPM, as the close counts them —
 * the same formula, same three clamps (1..8, the div clamp, 2BAR odd-round) */
int loop_follow_bars(seq_t *s, int trk, uint32_t d_us)
{
    float bar_us = 60000000.0f * 4.0f / s->bpm;
    int nb = (int)(d_us / bar_us + 0.5f);             /* round(d / bar_us) */
    if (nb < 1) nb = 1;
    if (nb > 8) nb = 8;
    int max_nb = 128 / seq_steps_per_bar(s, trk);     /* the div clamp, before k */
    if (nb > max_nb) nb = max_nb;
    if (seq_local(s, trk, SEQ_BAR) < SEQ_TPS && (nb & 1)) nb++;   /* 2BAR follow: odd nb rounds up to even */
    return nb;
}

int loop_close(loop_t *l, seq_t *s, uint32_t now_us)
{
    int trk = l->armed_trk;
    uint32_t d = now_us - l->t0_us;
    if (!l->n || d < LOOP_MIN_US) { clear_take(l); return 0; }   /* empty take; the incoming batch untouched */
    if (s->t[trk].cur != l->arm_pat) { clear_take(l); return -1; }   /* the pattern changed mid-take */
    int was_playing = s->playing;
    int nb; float bpm;
    int two_bar = seq_local(s, trk, SEQ_BAR) < SEQ_TPS;   /* a bar maps to fewer than one step: only 2BAR */
    if (!was_playing) {
        loop_fit(d, s->bpm, &nb, &bpm);
        if (two_bar && nb == 1) {                  /* a 1-bar fit would be 0 steps: round up to 2 bars */
            nb = 2;
            bpm = (float)nb * 240000000.0f / (float)d;
        }
        seq_set_bpm(s, bpm);
    } else {
        nb = loop_follow_bars(s, trk, d);
    }
    /* the whole close is one undo step: save the incoming batch, set it, restore after the last seq_add */
    uint8_t b0 = s->batch, bd0 = s->batch_done;
    s->batch = 1; s->batch_done = 0;
    int steps = (int)(seq_local(s, trk, (uint32_t)nb * SEQ_BAR) / SEQ_TPS);
    seq_set_len(s, trk, steps);
    float P = (float)(nb * SEQ_BAR), k = P / (float)d;
    for (int i = 0; i < l->n; i++) {
        uint32_t off = l->ev[i].open ? d : l->ev[i].off;
        uint32_t on_tick = (uint32_t)((float)l->ev[i].on * k + 0.5f);
        uint32_t off_tick = (uint32_t)((float)off * k + 0.5f);
        seq_add(s, trk, seq_local(s, trk, on_tick), seq_local(s, trk, off_tick - on_tick), l->ev[i].note, l->ev[i].vel);
    }
    s->batch = b0; s->batch_done = bd0;
    if (!was_playing) seq_play(s, 1);
    clear_take(l);
    return nb;
}

int loop_poll(loop_t *l, seq_t *s, uint32_t now_us)
{
    if (l->rec && now_us - l->t0_us > LOOP_MAX_US) return loop_close(l, s, now_us);
    return 0;
}
