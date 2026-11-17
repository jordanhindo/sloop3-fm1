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

static void clear_take(loop_t *l) { l->armed_trk = -1; l->rec = 0; l->n = 0; l->follow = 0; l->closing = 0; }

void loop_init(loop_t *l) { loop_cancel(l); l->full = 0; }
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

/* the most bars a take may be: 8, fewer where the track's DIV holds fewer in 128 steps; 2BAR: at least 2 */
int loop_max_bars(const seq_t *s, int trk)
{
    int m = 128 / seq_steps_per_bar(s, trk);
    return m > 8 ? 8 : m < 1 ? 1 : m;
}
static int two_bar(const seq_t *s, int trk) { return seq_local(s, trk, SEQ_BAR) < SEQ_TPS; }   /* a bar maps to fewer than one step: only 2BAR */
/* the bar line a press at `tick` means: the next one; one just passed (within LOOP_NEAR) is that one */
static uint32_t line_of(uint32_t tick)
{
    uint32_t r = tick % SEQ_BAR;
    return r < LOOP_NEAR ? tick - r : tick - r + SEQ_BAR;
}

int loop_arm(loop_t *l, seq_t *s, int trk, int target)
{
    if (s->recording) return 0;                    /* grid recording in progress */
    if (l->rec) return 0;                          /* a capture open */
    if (seq_count(s, trk) != 0) return 0;          /* a content track */
    l->armed_trk = (int8_t)trk;
    l->arm_pat = (int8_t)s->t[trk].cur;
    l->target = (uint8_t)target; l->n = 0; l->closing = 0; l->full = 0;
    l->follow = (uint8_t)(s->playing != 0);
    if (l->follow) {                               /* a later take: from the next bar line, for the bars chosen (FREE: the most) */
        int nb = loop_max_bars(s, trk);
        if (target && target < nb) nb = target;
        if (two_bar(s, trk) && (nb & 1)) nb++;
        l->start = line_of(s->tick);
        l->end = l->start + (uint32_t)nb * SEQ_BAR;
    }
    return 1;
}

void loop_note(loop_t *l, int trk, int note, int vel, int down, uint32_t now)
{
    if (trk != l->armed_trk) return;               /* only the armed track's notes */
    if (down) {
        if (l->follow) {                           /* a later take: what is heard from the start line (a hair early: kept) to the end line */
            if ((int32_t)(now + LOOP_NEAR - l->start) < 0 || (int32_t)(now - l->end) >= 0) return;
        } else if (!l->rec) { l->n = 0; l->t0_us = now; l->rec = 1; }   /* the first captured note-on */
        if (l->n == LOOP_MAX) return;              /* the buffer is full: dropped */
        loop_ev *e = &l->ev[l->n++];
        e->on = l->follow ? now : now - l->t0_us; e->off = 0; e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->open = 1;
    } else {
        for (int i = (int)l->n - 1; i >= 0; i--)   /* close the most recent open event of the same note */
            if (l->ev[i].open && l->ev[i].note == (uint8_t)note) {
                l->ev[i].off = l->follow ? now : now - l->t0_us; l->ev[i].open = 0;
                return;
            }
    }
}

void loop_stop(loop_t *l, seq_t *s)
{
    if (!l->follow || !l->rec) return;
    uint32_t e = line_of(s->tick);
    if ((int32_t)(e - l->start) <= 0) e = l->start + SEQ_BAR;                   /* at least a bar */
    if (two_bar(s, l->armed_trk) && (((e - l->start) / SEQ_BAR) & 1)) e += SEQ_BAR;
    if ((int32_t)(e - l->end) < 0) l->end = e;
    l->closing = 1;
}

/* a later take's close: the bars between its two lines, each note where it was heard; nothing scaled */
static int close_follow(loop_t *l, seq_t *s)
{
    int trk = l->armed_trk;
    if (!l->n) { clear_take(l); return 0; }
    if (s->t[trk].cur != l->arm_pat) { clear_take(l); return -1; }
    int nb = (int)((l->end - l->start) / SEQ_BAR);
    uint32_t P = (uint32_t)nb * SEQ_BAR;
    uint8_t b0 = s->batch, bd0 = s->batch_done;
    s->batch = 1; s->batch_done = 0;
    seq_set_len(s, trk, (int)(seq_local(s, trk, P) / SEQ_TPS));
    for (int i = 0; i < l->n; i++) {
        uint32_t on = l->ev[i].on, off = l->ev[i].open || (int32_t)(l->ev[i].off - l->end) > 0 ? l->end : l->ev[i].off;
        int32_t rel = (int32_t)(on - l->start);
        if (rel < 0) rel += (int32_t)P;                                         /* a hair early: just before the loop comes round */
        if (!seq_add(s, trk, seq_local(s, trk, (uint32_t)rel), seq_local(s, trk, off - on), l->ev[i].note, l->ev[i].vel)) l->full = 1;
    }
    s->batch = b0; s->batch_done = bd0;
    s->t[trk].origin = seq_local(s, trk, l->end);                               /* its beginning sits on the end line */
    seq_catch_up(s, trk, l->end);                                               /* what the line has already passed sounds now: the downbeat is not lost */
    clear_take(l);
    return nb;
}

int loop_close(loop_t *l, seq_t *s, uint32_t now_us)
{
    if (l->follow) return close_follow(l, s);
    int trk = l->armed_trk;
    uint32_t d = now_us - l->t0_us;
    if (!l->n || d < LOOP_MIN_US) { clear_take(l); return 0; }   /* empty take; the incoming batch untouched */
    if (s->t[trk].cur != l->arm_pat) { clear_take(l); return -1; }   /* the pattern changed mid-take */
    if (s->playing) { clear_take(l); return -1; }                    /* the transport was started under a first take: no tempo to derive */
    int nb; float bpm;
    loop_fit(d, s->bpm, &nb, &bpm);
    if (l->target && l->target <= loop_max_bars(s, trk)) {   /* the bars chosen: the tempo is theirs (out of 40..250: the fit) */
        float b = (float)l->target * 240000000.0f / (float)d;
        if (b >= 40.0f && b <= 250.0f) { nb = l->target; bpm = (float)(int)(b + 0.5f); }
    }
    if (two_bar(s, trk) && (nb & 1)) {             /* an odd count would end on half a step: round up */
        nb++;
        bpm = (float)nb * 240000000.0f / (float)d;
    }
    seq_set_bpm(s, bpm);
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
        if (!seq_add(s, trk, seq_local(s, trk, on_tick), seq_local(s, trk, off_tick - on_tick), l->ev[i].note, l->ev[i].vel)) l->full = 1;
    }
    s->batch = b0; s->batch_done = bd0;
    seq_play(s, 1);
    clear_take(l);
    return nb;
}

int loop_poll(loop_t *l, seq_t *s, uint32_t now_us)
{
    if (l->armed_trk < 0) return 0;
    if (l->follow) {
        if (!s->playing) { clear_take(l); return -1; }                           /* the transport stopped under it */
        if (!l->rec && (int32_t)(s->tick - l->start) >= 0) l->rec = 1;          /* the start line: recording */
        if (l->rec && (int32_t)(s->tick - l->end) >= 0) return close_follow(l, s);
        return 0;
    }
    if (l->rec && now_us - l->t0_us > LOOP_MAX_US) return loop_close(l, s, now_us);
    return 0;
}
