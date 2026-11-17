/* SPDX-License-Identifier: GPL-3.0-only
 * The sequencer (seq.h). Written for this project. */
#include "seq.h"
#include "../core/gb.h"          /* memset, memcpy, memmove: the C library on the PC, core/libc.c on the FM-1 */

/* (3.3.7) what only the interface calls (editing, never while a save has the flash window off): run from the window,
 * the RAM's room kept for what plays (tools/xipcheck.py checks the rule) */
#define SEQ_COLD __attribute__((section(".xtext.seq")))
static float tick_per_frame(const seq_t *s) { return s->bpm * 4.0f * (float)SEQ_TPS / 60.0f / s->rate; }

void seq_init(seq_t *s, float rate, float bpm, seq_hist *hist)
{
    memset(s, 0, sizeof *s);
    s->rate = rate; s->bpm = bpm; s->hist = hist; s->rnd = 0x2545F491u;
    if (hist) hist->n_undo = hist->n_redo = 0;
    for (int t = 0; t < SEQ_NTRK; t++) {
        s->t[t].next = SEQ_NONE; s->t[t].lock_step = -1; s->t[t].div = SEQ_DIV_16; s->t[t].gate = 100;
        for (int p = 0; p < SEQ_NPAT; p++) s->t[t].pat[p].steps = 16;
    }
}
void seq_set_bpm(seq_t *s, float bpm) { s->bpm = bpm < 30.0f ? 30.0f : bpm > 300.0f ? 300.0f : bpm; }

static seq_pat *cur(seq_t *s, int t) { return &s->t[t].pat[s->t[t].cur]; }
/* the step an event belongs to: the nearest (a note played a little early is its step's, as heard; a nudged one
 * keeps its step); the last half step before the end belongs to step 1. -1: past the pattern's end */
static int step_of(const seq_pat *p, uint32_t tick)
{
    uint32_t st = (tick + SEQ_TPS / 2) / SEQ_TPS;
    if (st < p->steps) return (int)st;
    if (st == p->steps && tick < (uint32_t)p->steps * SEQ_TPS) return 0;
    return -1;
}
static uint32_t plen(const seq_track *k) { return (uint32_t)k->pat[k->cur].steps * SEQ_TPS; }
/* DIV: the track's ticks against the sequencer's, num / den */
static const uint8_t DIV_NUM[SEQ_NDIV] = { 2, 3, 1, 3, 1, 3, 1, 1, 1, 1 }, DIV_DEN[SEQ_NDIV] = { 1, 2, 1, 4, 2, 8, 4, 8, 16, 32 };
const char *const seq_div_name[SEQ_NDIV] = { "1/32", "16T", "1/16", "8T", "1/8", "4T", "1/4", "1/2", "1BAR", "2BAR" };
static uint32_t local_of(const seq_track *k, uint32_t tick)
{
    int d = k->div < SEQ_NDIV ? k->div : SEQ_DIV_16;
    return d == SEQ_DIV_16 ? tick : (uint32_t)((uint64_t)tick * DIV_NUM[d] / DIV_DEN[d]);
}
uint32_t seq_local(const seq_t *s, int trk, uint32_t tick) { return local_of(&s->t[trk], tick); }

/* ---------------------------------------------------------------- history */
/* One array for both: h[0 .. n_undo) the states before each edit, h[n_undo .. n_undo + n_redo) the states an undo
 * left. Undo and redo swap a slot with the pattern, so the slot then holds what the other one needs. */
static void swap_pat(seq_pat *a, seq_pat *b)
{
    uint8_t *x = (uint8_t *)a, *y = (uint8_t *)b;
    for (uint32_t i = 0; i < sizeof *a; i++) { uint8_t t = x[i]; x[i] = y[i]; y[i] = t; }
}
static void edit(seq_t *s, int trk, int pat)          /* before an edit: what it was, for undo; redo forgotten */
{
    seq_hist *h = s->hist;
    if (!h) return;
    if (s->batch) { if (s->batch_done) return; s->batch_done = 1; }   /* one gesture, one undo */
    if (h->n_undo == SEQ_UNDO) { memmove(h->h, h->h + 1, sizeof h->h[0] * (SEQ_UNDO - 1)); h->n_undo--; }   /* the oldest goes */
    seq_snap *x = &h->h[h->n_undo++];
    x->trk = (uint8_t)trk; x->pat = (uint8_t)pat; x->p = s->t[trk].pat[pat];
    h->n_redo = 0;
}
int seq_undo(seq_t *s)
{
    seq_hist *h = s->hist;
    if (!h || !h->n_undo) return 0;
    seq_snap *x = &h->h[--h->n_undo];
    swap_pat(&x->p, &s->t[x->trk].pat[x->pat]);
    h->n_redo++;
    return 1;
}
int seq_redo(seq_t *s)
{
    seq_hist *h = s->hist;
    if (!h || !h->n_redo) return 0;
    seq_snap *x = &h->h[h->n_undo++];
    swap_pat(&x->p, &s->t[x->trk].pat[x->pat]);
    h->n_redo--;
    return 1;
}

/* One track's undo and redo, the others' edits passed over. Each entry is one pattern whole, so the entries of two
 * tracks do not depend on each other: the entry taken is swapped with its pattern, then moved to the border between
 * undo and redo past the other tracks' entries (their order kept), where seq_undo / seq_redo expect it. */
static void swap_snap(seq_snap *a, seq_snap *b)
{
    uint8_t t = a->trk; a->trk = b->trk; b->trk = t;
    t = a->pat; a->pat = b->pat; b->pat = t;
    swap_pat(&a->p, &b->p);
}
SEQ_COLD int seq_undo_trk(seq_t *s, int trk)
{
    seq_hist *h = s->hist;
    if (!h) return 0;
    int i = h->n_undo - 1;
    while (i >= 0 && h->h[i].trk != trk) i--;                /* its newest edit */
    if (i < 0) return 0;
    swap_pat(&h->h[i].p, &s->t[trk].pat[h->h[i].pat]);
    for (; i < h->n_undo - 1; i++) swap_snap(&h->h[i], &h->h[i + 1]);
    h->n_undo--; h->n_redo++;
    return 1;
}
SEQ_COLD int seq_redo_trk(seq_t *s, int trk)
{
    seq_hist *h = s->hist;
    if (!h) return 0;
    int i = h->n_undo, end = h->n_undo + h->n_redo;
    while (i < end && h->h[i].trk != trk) i++;               /* the edit it undid last */
    if (i >= end) return 0;
    swap_pat(&h->h[i].p, &s->t[trk].pat[h->h[i].pat]);
    for (; i > h->n_undo; i--) swap_snap(&h->h[i], &h->h[i - 1]);
    h->n_undo++; h->n_redo--;
    return 1;
}

/* ---------------------------------------------------------------- pattern edits */
static int insert_i(seq_pat *p, seq_ev e)              /* kept sorted by tick: where it went, -1 if full */
{
    if (p->n >= SEQ_PATEV) return -1;
    int i = p->n;
    while (i > 0 && p->ev[i - 1].tick > e.tick) { p->ev[i] = p->ev[i - 1]; i--; }
    p->ev[i] = e; p->n++;
    return i;
}
static int insert(seq_pat *p, seq_ev e) { return insert_i(p, e) >= 0; }   /* 0 if full */
/* the events back in order after their ticks were moved (stable: equal ticks keep their order) */
static __attribute__((noinline)) void sort_ev(seq_pat *p)
{
    for (int a = 1; a < p->n; a++) { seq_ev x = p->ev[a]; int b = a; while (b > 0 && p->ev[b - 1].tick > x.tick) { p->ev[b] = p->ev[b - 1]; b--; } p->ev[b] = x; }
}
static void remove_at(seq_pat *p, int i) { memmove(&p->ev[i], &p->ev[i + 1], sizeof(seq_ev) * (size_t)(p->n - i - 1)); p->n--; }

void seq_set_len(seq_t *s, int trk, int steps)
{
    if (steps < 1) steps = 1;
    if (steps > SEQ_MAXSTEPS) steps = SEQ_MAXSTEPS;
    seq_pat *p = cur(s, trk);
    if (p->steps == steps) return;
    edit(s, trk, s->t[trk].cur);
    p->steps = (uint16_t)steps;                         /* events past the end stay: a longer length finds them */
}
int seq_step_has(const seq_t *s, int trk, int step, int note)
{
    const seq_pat *p = &s->t[trk].pat[s->t[trk].cur];
    for (int i = 0; i < p->n; i++)
        if (step_of(p, p->ev[i].tick) == step && (note < 0 || p->ev[i].note == note)) return 1;
    return 0;
}
static void locks_drop(seq_pat *p, int step)
{
    for (int i = p->nlock - 1; i >= 0; i--)
        if (p->lk[i].step == step) { memmove(&p->lk[i], &p->lk[i + 1], sizeof(seq_lock) * (size_t)(p->nlock - i - 1)); p->nlock--; }
}
void seq_step_toggle(seq_t *s, int trk, int step, int note, int vel)
{
    seq_pat *p = cur(s, trk);
    if (step < 0 || step >= SEQ_MAXSTEPS) return;
    edit(s, trk, s->t[trk].cur);
    for (int i = 0; i < p->n; i++)
        if (step_of(p, p->ev[i].tick) == step && p->ev[i].note == note) {
            remove_at(p, i);
            if (!seq_step_has(s, trk, step, -1)) locks_drop(p, step);   /* an empty step keeps nothing */
            return;
        }
    seq_ev e = { (uint16_t)(step * SEQ_TPS), (uint16_t)(SEQ_TPS * 3 / 4), (uint8_t)note, (uint8_t)vel, SEQ_ALWAYS, 1 };
    insert(p, e);
}
int seq_add(seq_t *s, int trk, uint32_t tick, uint32_t len, int note, int vel)
{
    if (trk < 0 || trk >= SEQ_NTRK) return 0;
    seq_pat *p = cur(s, trk);
    uint32_t pl = (uint32_t)p->steps * SEQ_TPS;
    if (!pl) return 0;
    edit(s, trk, s->t[trk].cur);
    tick %= pl; if (len < SEQ_TPS / 8) len = SEQ_TPS / 8; if (len > pl - 1) len = pl - 1;
    seq_ev e = { (uint16_t)tick, (uint16_t)len, (uint8_t)(note & 127), (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel), SEQ_ALWAYS, 1 };
    return insert(p, e);
}
void seq_clear(seq_t *s, int trk)
{
    seq_pat *p = cur(s, trk);
    if (!p->n && !p->nlock) return;
    edit(s, trk, s->t[trk].cur);
    p->n = 0; p->nlock = 0;
}
void seq_erase_note(seq_t *s, int trk, int note)
{
    seq_pat *p = cur(s, trk);
    int any = 0;
    for (int i = 0; i < p->n; i++) if (p->ev[i].note == note) any = 1;
    if (!any) return;
    edit(s, trk, s->t[trk].cur);
    for (int i = p->n - 1; i >= 0; i--) if (p->ev[i].note == note) remove_at(p, i);
}
void seq_copy(seq_t *s, int trk, int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= SEQ_NPAT || to >= SEQ_NPAT) return;
    edit(s, trk, to);
    s->t[trk].pat[to] = s->t[trk].pat[from];
}
int seq_count(const seq_t *s, int trk) { return s->t[trk].pat[s->t[trk].cur].n; }

/* ---------------------------------------------------------------- per step: locks, condition, ratchet */
void seq_locks_put(seq_t *s, int trk, int pat, const uint8_t *lk, int n)   /* (the web editor: a pattern's locks, all) */
{
    if (trk < 0 || trk >= SEQ_NTRK || pat < 0 || pat >= SEQ_NPAT) return;
    seq_pat *p = &s->t[trk].pat[pat];
    p->nlock = 0;
    for (int i = 0; i < n && p->nlock < SEQ_LOCKS; i++)
        if (lk[3 * i] < p->steps && lk[3 * i + 1] < SEQ_LK_END) { seq_lock *l = &p->lk[p->nlock++]; l->step = lk[3 * i]; l->param = lk[3 * i + 1]; l->value = lk[3 * i + 2]; }
}
void seq_lock_set(seq_t *s, int trk, int step, int param, int value)
{
    seq_pat *p = cur(s, trk);
    if (step < 0 || step >= SEQ_MAXSTEPS || param < 0 || param > 255) return;
    int i = 0;
    while (i < p->nlock && !(p->lk[i].step == step && p->lk[i].param == param)) i++;
    if (value < 0) {
        if (i == p->nlock) return;
        edit(s, trk, s->t[trk].cur);
        memmove(&p->lk[i], &p->lk[i + 1], sizeof(seq_lock) * (size_t)(p->nlock - i - 1)); p->nlock--;
        return;
    }
    if (i == p->nlock && p->nlock >= SEQ_LOCKS) return;
    /* a knob turned held on a step makes many small changes: one undo step for the first only */
    if (i == p->nlock) { edit(s, trk, s->t[trk].cur); p->nlock++; }
    p->lk[i].step = (uint8_t)step; p->lk[i].param = (uint8_t)param; p->lk[i].value = (uint8_t)(value > 255 ? 255 : value);
}
int seq_lock_get(const seq_t *s, int trk, int step, int param)
{
    const seq_pat *p = &s->t[trk].pat[s->t[trk].cur];
    for (int i = 0; i < p->nlock; i++) if (p->lk[i].step == step && p->lk[i].param == param) return p->lk[i].value;
    return -1;
}
static void step_set(seq_t *s, int trk, int step, int cond, int rat)
{
    seq_pat *p = cur(s, trk);
    int first = 1;
    for (int i = 0; i < p->n; i++) {
        if (step_of(p, p->ev[i].tick) != step) continue;
        if (first) { edit(s, trk, s->t[trk].cur); first = 0; }
        if (cond >= 0) p->ev[i].cond = (uint8_t)cond;
        if (rat >= 0) p->ev[i].ratchet = (uint8_t)((p->ev[i].ratchet & 0x30u) | (uint32_t)rat);   /* (its flags kept) */
    }
}
int seq_step_flags(const seq_t *s, int trk, int step)
{
    const seq_pat *p = &s->t[trk].pat[s->t[trk].cur];
    for (int i = 0; i < p->n; i++) if (step_of(p, p->ev[i].tick) == step) return p->ev[i].ratchet & (SEQ_SLIDE | SEQ_ACCENT);
    return 0;
}
void seq_step_flag(seq_t *s, int trk, int step, int flag, int on)
{
    seq_pat *p = cur(s, trk);
    int first = 1;
    for (int i = 0; i < p->n; i++) {
        if (step_of(p, p->ev[i].tick) != step || !(p->ev[i].ratchet & flag) == !on) continue;
        if (first) { edit(s, trk, s->t[trk].cur); first = 0; }
        p->ev[i].ratchet = (uint8_t)(on ? p->ev[i].ratchet | flag : p->ev[i].ratchet & ~flag);
    }
}
SEQ_COLD void seq_ev_set(seq_t *s, int trk, int i, int note, int len, int vel, int flags)
{
    seq_pat *p = cur(s, trk);
    if (i < 0 || i >= p->n) return;
    seq_ev *e = &p->ev[i];
    int nl = len < 0 ? e->len : len < SEQ_TPS / 8 ? SEQ_TPS / 8 : len > (int)p->steps * SEQ_TPS ? (int)p->steps * SEQ_TPS : len;
    int nn = note < 0 ? e->note : note > 127 ? 127 : note, nv = vel < 0 ? e->vel : vel < 1 ? 1 : vel > 127 ? 127 : vel;
    int nr = flags < 0 ? e->ratchet : (e->ratchet & 7) | (flags & 0x30);
    if (nn == e->note && nl == e->len && nv == e->vel && nr == e->ratchet) return;
    edit(s, trk, s->t[trk].cur);
    e->note = (uint8_t)nn; e->len = (uint16_t)nl; e->vel = (uint8_t)nv; e->ratchet = (uint8_t)nr;
}
void seq_ev_del(seq_t *s, int trk, int i)
{
    seq_pat *p = cur(s, trk);
    if (i < 0 || i >= p->n) return;
    edit(s, trk, s->t[trk].cur);
    remove_at(p, i);
}
/* a note a step on or back, or copied to just after itself (as SLOOP's MOVE and COPY): its locks go with it when it was
 * its step's only note (copied: they are copied), unless the step it goes to has its own. Where it is now, -1 if it
 * would leave the pattern (or the pattern is full) */
SEQ_COLD int seq_ev_move(seq_t *s, int trk, int i, int dsteps, int copy)
{
    seq_pat *p = cur(s, trk);
    if (i < 0 || i >= p->n) return -1;
    seq_ev e = p->ev[i];
    int from = step_of(p, e.tick), span = (e.len + SEQ_TPS / 2) / SEQ_TPS;
    int d = copy ? (span < 1 ? 1 : span) : dsteps;
    int t = (int)e.tick + d * SEQ_TPS, end = (int)p->steps * SEQ_TPS;
    if (!d || t < 0 || t >= end || (copy && p->n >= SEQ_PATEV) || from < 0) return -1;
    int to = step_of(p, (uint32_t)t), alone = 1, theirs = 0;
    for (int j = 0; j < p->n; j++) if (j != i && step_of(p, p->ev[j].tick) == from) alone = 0;
    for (int j = 0; j < p->nlock; j++) if (p->lk[j].step == to) theirs = 1;
    edit(s, trk, s->t[trk].cur);
    if (!copy) remove_at(p, i);
    e.tick = (uint16_t)t;
    int at = insert_i(p, e);
    if (!theirs && (alone || copy)) {
        int n = p->nlock;
        for (int j = 0; j < n; j++) if (p->lk[j].step == from) {
            if (!copy) p->lk[j].step = (uint8_t)to;
            else if (p->nlock < SEQ_LOCKS) { p->lk[p->nlock] = p->lk[j]; p->lk[p->nlock].step = (uint8_t)to; p->nlock++; }
        }
    }
    return at;
}
/* (SLOOP 2.8.3's rules) how long note i may last: to the pattern's end (why 3); a MONO / LEGATO sound (poly 0) to the
 * next note after its step (why 0); a POLY one on under the later notes, until a step would have more than 4 notes
 * sounding (why 1) */
SEQ_COLD int seq_ev_len_max(const seq_t *s, int trk, int i, int poly, int *why)
{
    const seq_pat *p = &s->t[trk].pat[s->t[trk].cur];
    if (i < 0 || i >= p->n) return 0;
    const seq_ev *e = &p->ev[i];
    int at = (int)e->tick, end = (int)p->steps * SEQ_TPS, max = end - at, w = 3, from = step_of(p, e->tick);
    if (!poly) {
        for (int j = 0; j < p->n; j++) {
            int st = step_of(p, p->ev[j].tick);
            if (j == i || st == from || (int)p->ev[j].tick <= at) continue;
            if ((int)p->ev[j].tick - at < max) { max = (int)p->ev[j].tick - at; w = 0; }
        }
    } else {
        for (int st = from + 1; st < (int)p->steps && st * SEQ_TPS - at < max; st++) {
            int t0 = st * SEQ_TPS, n = 0;
            for (int j = 0; j < p->n; j++) {
                if (j == i) continue;
                int a = (int)p->ev[j].tick, b = a + (int)p->ev[j].len;
                if (step_of(p, p->ev[j].tick) == st || (a < t0 && b > t0)) n++;
            }
            if (n >= 4) { max = t0 - at; w = 1; break; }
        }
    }
    if (why) *why = w;
    return max < SEQ_TPS / 4 ? SEQ_TPS / 4 : max;
}
/* a pattern rewritten from outside (the web editor's piano roll): first 0 empties it (one undo) and sets its length,
 * then its notes come (tick, len, note, vel), a few at a time; its locks stay. Its notes now */
int seq_pat_put(seq_t *s, int trk, int pat, int first, int steps, const uint8_t *notes, int n) { return seq_pat_put2(s, trk, pat, first, steps, notes, n, 6); }
SEQ_COLD int seq_pat_put2(seq_t *s, int trk, int pat, int first, int steps, const uint8_t *notes, int n, int w)
{
    if (trk < 0 || trk >= SEQ_NTRK || pat < 0 || pat >= SEQ_NPAT) return -1;
    seq_pat *p = &s->t[trk].pat[pat];
    if (!first) {
        uint8_t b = s->batch; s->batch = 0; edit(s, trk, pat); s->batch = b;
        p->n = 0; p->steps = (uint16_t)(steps < 1 ? 1 : steps > SEQ_MAXSTEPS ? SEQ_MAXSTEPS : steps);
    }
    for (int i = 0; i < n; i++) {
        const uint8_t *x = notes + w * i;
        uint32_t t = (uint32_t)(x[0] | x[1] << 8), l = (uint32_t)(x[2] | x[3] << 8);
        if (t >= (uint32_t)p->steps * SEQ_TPS || x[4] > 127) continue;
        l = l < SEQ_TPS / 8 ? SEQ_TPS / 8 : l > (uint32_t)p->steps * SEQ_TPS ? (uint32_t)p->steps * SEQ_TPS : l;
        seq_ev e = { (uint16_t)t, (uint16_t)l, x[4], (uint8_t)(x[5] < 1 ? 1 : x[5] > 127 ? 127 : x[5]), w >= 8 ? x[6] : SEQ_ALWAYS, w >= 8 ? (uint8_t)((x[7] & 7u) < 1 ? (x[7] & 0x30u) | 1u : (x[7] & 7u) > 4 ? (x[7] & 0x30u) | 4u : x[7] & 0x37u) : 1 };
        if (!insert(p, e)) break;
    }
    return p->n;
}
SEQ_COLD int seq_step_write(seq_t *s, int trk, int step, const uint8_t *notes, int n, int vel)
{
    seq_pat *p = cur(s, trk);
    if (step < 0 || step >= p->steps) return -1;
    edit(s, trk, s->t[trk].cur);
    uint16_t len = SEQ_TPS; uint8_t cond = SEQ_ALWAYS, rat = 1;
    for (int i = p->n - 1; i >= 0; i--) if (step_of(p, p->ev[i].tick) == step) { len = p->ev[i].len; cond = p->ev[i].cond; rat = p->ev[i].ratchet; remove_at(p, i); }
    int at = -1;
    for (int k = 0; k < n; k++) {
        seq_ev e = { (uint16_t)(step * SEQ_TPS), len, notes[k], (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel), cond, rat };
        insert(p, e);
    }
    for (int i = 0; i < p->n; i++) if (n && step_of(p, p->ev[i].tick) == step && p->ev[i].note == notes[0]) { at = i; break; }
    return at;
}
void seq_step_cond(seq_t *s, int trk, int step, int cond) { step_set(s, trk, step, cond, -1); }
void seq_step_ratchet(seq_t *s, int trk, int step, int n) { step_set(s, trk, step, -1, n < 1 ? 1 : n > 4 ? 4 : n); }
void seq_step_reset(seq_t *s, int trk, int step)
{
    step_set(s, trk, step, SEQ_ALWAYS, 1);
    seq_pat *p = cur(s, trk);
    locks_drop(p, step);
}
int seq_step_info(const seq_t *s, int trk, int step, int *cond, int *ratchet)
{
    const seq_pat *p = &s->t[trk].pat[s->t[trk].cur];
    if (cond) *cond = SEQ_ALWAYS;
    if (ratchet) *ratchet = 1;
    for (int i = 0; i < p->n; i++)
        if (step_of(p, p->ev[i].tick) == step) { if (cond) *cond = p->ev[i].cond; if (ratchet) *ratchet = SEQ_RAT(p->ev[i].ratchet); break; }
    int n = 0;
    for (int i = 0; i < p->nlock; i++) if (p->lk[i].step == step) n++;
    return n;
}

/* a step's note, velocity, nudge (the first event of the step tells; an edit moves them all) */
int seq_step_get(const seq_t *s, int trk, int step, int *note, int *vel, int *nudge)
{
    const seq_pat *p = &s->t[trk].pat[s->t[trk].cur];
    for (int i = 0; i < p->n; i++) {
        if (step_of(p, p->ev[i].tick) != step) continue;
        int d = (int)p->ev[i].tick - step * SEQ_TPS;
        if (d > SEQ_TPS / 2) d -= (int)p->steps * SEQ_TPS;            /* step 1 played early, from the end */
        if (note) *note = p->ev[i].note;
        if (vel) *vel = p->ev[i].vel;
        if (nudge) *nudge = d;
        return 1;
    }
    return 0;
}
void seq_step_note(seq_t *s, int trk, int step, int delta)
{
    seq_pat *p = cur(s, trk);
    int first = 1;
    for (int i = 0; i < p->n; i++) {
        if (step_of(p, p->ev[i].tick) != step) continue;
        int n = p->ev[i].note + delta;
        if (n < 0 || n > 127) return;
        if (first) { edit(s, trk, s->t[trk].cur); first = 0; }
        p->ev[i].note = (uint8_t)n;
    }
}
void seq_step_vel(seq_t *s, int trk, int step, int vel)
{
    seq_pat *p = cur(s, trk);
    int first = 1;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    for (int i = 0; i < p->n; i++) {
        if (step_of(p, p->ev[i].tick) != step) continue;
        if (first) { edit(s, trk, s->t[trk].cur); first = 0; }
        p->ev[i].vel = (uint8_t)vel;
    }
}
SEQ_COLD void seq_step_nudge(seq_t *s, int trk, int step, int nudge)
{
    seq_pat *p = cur(s, trk);
    if (nudge < -SEQ_TPS / 2 + 1) nudge = -SEQ_TPS / 2 + 1;
    if (nudge > SEQ_TPS / 2 - 1) nudge = SEQ_TPS / 2 - 1;
    int moved = 0;
    for (int i = 0; i < p->n; i++) {
        if (step_of(p, p->ev[i].tick) != step) continue;
        if (!moved) { edit(s, trk, s->t[trk].cur); moved = 1; }
        int t = step * SEQ_TPS + nudge;
        if (t < 0) t += (int)p->steps * SEQ_TPS;
        p->ev[i].tick = (uint16_t)t;
    }
    if (!moved) return;
    sort_ev(p);
}
/* the EDIT layer's knobs: the whole pattern a step later / earlier, twice as long (a copy after itself) or half,
 * every note a semitone up / down */
SEQ_COLD void seq_shift(seq_t *s, int trk, int dir)
{
    seq_pat *p = cur(s, trk);
    if (!p->n && !p->nlock) return;
    edit(s, trk, s->t[trk].cur);
    int32_t len = (int32_t)p->steps * SEQ_TPS, d = dir > 0 ? SEQ_TPS : -SEQ_TPS;
    for (int i = 0; i < p->n; i++) if (p->ev[i].tick < len) p->ev[i].tick = (uint16_t)(((int32_t)p->ev[i].tick + d + len) % len);
    for (int i = 0; i < p->nlock; i++) p->lk[i].step = (uint8_t)(((int)p->lk[i].step + (dir > 0 ? 1 : -1) + p->steps) % p->steps);
    sort_ev(p);
}
SEQ_COLD void seq_resize(seq_t *s, int trk, int dir)
{
    seq_pat *p = cur(s, trk);
    if (dir > 0) {
        if (p->steps * 2 > SEQ_MAXSTEPS) return;
        edit(s, trk, s->t[trk].cur);
        int n = p->n, len = p->steps * SEQ_TPS;
        /* 3.1.8: a second half kept by a halving is found again as it was; only an empty one takes the copy. (Until
         * 3.1.7 the copy was laid over it: every note of the first half twice in the second; and the copies went
         * after the notes past the end, the pattern no longer in order until the project was loaded again.) */
        int kept = 0;
        for (int i = 0; i < n; i++) if (p->ev[i].tick >= len && p->ev[i].tick < 2 * len) kept = 1;
        for (int i = 0; i < p->nlock; i++) if (p->lk[i].step >= p->steps && p->lk[i].step < 2 * p->steps) kept = 1;
        if (!kept) {
            for (int i = 0; i < n && p->n < SEQ_PATEV; i++) if (p->ev[i].tick < len) { seq_ev e = p->ev[i]; e.tick = (uint16_t)(e.tick + len); p->ev[p->n++] = e; }
            int nl = p->nlock;
            for (int i = 0; i < nl && p->nlock < SEQ_LOCKS; i++) if (p->lk[i].step < p->steps) { seq_lock l = p->lk[i]; l.step = (uint8_t)(l.step + p->steps); p->lk[p->nlock++] = l; }
            sort_ev(p);
        }
        p->steps = (uint16_t)(p->steps * 2);
    } else {
        if (p->steps < 2) return;
        edit(s, trk, s->t[trk].cur);
        p->steps = (uint16_t)(p->steps / 2);                 /* the second half stays, past the end: x2 finds it */
    }
}
SEQ_COLD void seq_transpose(seq_t *s, int trk, int semis)
{
    seq_pat *p = cur(s, trk);
    if (!p->n) return;
    for (int i = 0; i < p->n; i++) { int n = p->ev[i].note + semis; if (n < 0 || n > 127) return; }
    edit(s, trk, s->t[trk].cur);
    for (int i = 0; i < p->n; i++) p->ev[i].note = (uint8_t)(p->ev[i].note + semis);
}
void seq_set_div(seq_t *s, int trk, int div)
{
    seq_track *k = &s->t[trk];
    div = div < 0 ? 0 : div >= SEQ_NDIV ? SEQ_NDIV - 1 : div;
    uint32_t before = local_of(k, s->tick);
    k->div = (uint8_t)div;
    uint32_t d = local_of(k, s->tick) - before;              /* the pattern goes on from the same place */
    k->origin += d;
    for (int i = 0; i < 16; i++) k->sounding[i].end += d;
    for (int i = 0; i < 8; i++) k->rat[i].at += d;
}
void seq_set_gate(seq_t *s, int trk, int gate) { s->t[trk].gate = (uint8_t)(gate < 10 ? 10 : gate > 200 ? 200 : gate); }
void seq_set_swing(seq_t *s, int trk, int swing)
{
    swing = swing < 0 ? 0 : swing > 100 ? 100 : swing;
    if (trk < 0) s->swing = (uint8_t)swing; else s->t[trk].swing = (uint8_t)swing;
}

/* ---------------------------------------------------------------- transport */
static void all_off(seq_t *s, int trk)
{
    seq_track *k = &s->t[trk];
    for (int i = 0; i < 16; i++) if (k->sounding[i].on) { k->sounding[i].on = 0; if (s->note_off) s->note_off(s, trk, k->sounding[i].note); }
    for (int i = 0; i < 8; i++) k->rat[i].on = 0;
    if (k->lock_step >= 0) { k->lock_step = -1; if (s->locks) s->locks(s, trk, 0, 0); }
}
void seq_play(seq_t *s, int on)
{
    for (int t = 0; t < SEQ_NTRK; t++) all_off(s, t);
    s->pos = 0; s->tick = 0; s->fill_bar = 0;
    for (int t = 0; t < SEQ_NTRK; t++) {
        seq_track *k = &s->t[t];
        if (k->next != SEQ_NONE) { k->cur = k->next; k->next = SEQ_NONE; }
        k->origin = 0;
    }
    s->playing = on;
    s->song_start = 0;
    if (on && s->song_play && s->song_n) {                       /* a song starts as it was recorded */
        for (int t = 0; t < SEQ_NTRK; t++) { s->t[t].cur = s->song[0].pat[t]; s->t[t].mute = (uint8_t)((s->song[0].mute >> t) & 1u); }
    }
    if (!on) { s->recording = 0; s->song_rec = 0; memset(s->held, 0, sizeof s->held); }
}
uint32_t seq_bar(const seq_t *s) { return s->tick / SEQ_BAR; }
int seq_steps_per_bar(const seq_t *s, int trk)
{
    int d = s->t[trk].div < SEQ_NDIV ? s->t[trk].div : SEQ_DIV_16, n = 16 * DIV_NUM[d] / DIV_DEN[d];
    return n < 1 ? 1 : n;
}
uint32_t seq_step_of(const seq_t *s, int trk)
{
    const seq_track *k = &s->t[trk];
    return (local_of(k, s->tick) - k->origin) % plen(k) / SEQ_TPS;
}
void seq_queue(seq_t *s, int trk, int pat)
{
    if (pat < 0 || pat >= SEQ_NPAT) return;
    if (trk < 0 && s->playing) {                     /* (3.3.7) every track: if one changes, all start together at the
                                                        change point (one already on it too): never out of step */
        int any = 0; for (int t = 0; t < SEQ_NTRK; t++) if (s->t[t].cur != pat) any = 1;
        for (int t = 0; t < SEQ_NTRK; t++) s->t[t].next = any ? (uint8_t)pat : SEQ_NONE;
        return;
    }
    for (int t = 0; t < SEQ_NTRK; t++) {
        if (trk >= 0 && t != trk) continue;
        if (!s->playing) { s->t[t].cur = (uint8_t)pat; s->t[t].next = SEQ_NONE; }
        else s->t[t].next = s->t[t].cur == pat ? SEQ_NONE : (uint8_t)pat;
    }
}
void seq_mute(seq_t *s, int trk, int on) { s->t[trk].mute = (uint8_t)(on != 0); }
void seq_fill(seq_t *s, int on) { s->fill = (uint8_t)(on != 0); }
void seq_fill_next_bar(seq_t *s) { s->fill_bar = s->playing ? seq_bar(s) + 2u : 1u; }   /* the bar after this one */
static int filling(const seq_t *s, uint32_t t) { return s->fill || (s->fill_bar && t / SEQ_BAR + 1u == s->fill_bar); }

/* where an event plays: its tick pulled towards the grid by quant % (the take itself is not changed), then the
 * swing: every second step late, by up to half a step (100: a pair played 75 / 25) */
static uint32_t eff_tick(const seq_ev *e, uint32_t q, uint32_t len, uint32_t swing)
{
    int32_t t = e->tick, g = (t + SEQ_TPS / 2) / SEQ_TPS * SEQ_TPS;
    int32_t x = t + (g - t) * (int32_t)q / 100;
    if (swing && ((g / SEQ_TPS) & 1)) x += (int32_t)(swing * (SEQ_TPS / 2) / 100u);
    return (uint32_t)(x % (int32_t)len);
}
static uint32_t rnd(seq_t *s) { uint32_t x = s->rnd; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return s->rnd = x; }
/* does a step with this condition play, at absolute tick t, in loop number `loop` of its pattern? */
static int cond_ok(seq_t *s, uint8_t c, uint32_t t, uint32_t loop)
{
    if (c == SEQ_ALWAYS) return 1;
    (void)t;                                                  /* (fills on the sequencer's bar: its tick now) */
    if (c == SEQ_FILL) return filling(s, s->tick);
    if (c == SEQ_NOFILL) return !filling(s, s->tick);
    if ((c & 0xC0u) == SEQ_CHANCE) return rnd(s) % 10u < (uint32_t)(c & 0x0Fu);
    if (c & SEQ_CYCLE) { uint32_t b = (c >> 3) & 7u, a = (c & 7u) + 1u; return b >= 2 && loop % b == a - 1u; }
    return 1;
}
static void start_note(seq_t *s, int trk, int note, int vel, uint32_t t, uint32_t len)
{
    seq_track *k = &s->t[trk];
    for (int j = 0; j < 16; j++)                                 /* the same note still sounding ends first */
        if (k->sounding[j].on && k->sounding[j].note == note) { k->sounding[j].on = 0; if (s->note_off) s->note_off(s, trk, note); }
    int slot = 0;
    for (int j = 0; j < 16; j++) if (!k->sounding[j].on) { slot = j; break; }
    if (k->sounding[slot].on && s->note_off) s->note_off(s, trk, k->sounding[slot].note);   /* 16 at once: the first goes */
    k->sounding[slot].on = 1; k->sounding[slot].note = (uint8_t)note; k->sounding[slot].end = t + (len ? len : 1);
    if (s->note_on) s->note_on(s, trk, note, vel);
}
/* SLIDE: a note goes on until the next note of the pattern starts (and a little past it: the two touch); the note
 * after a slide is told so (a MONO / LEGATO sound glides to it) */
static int next_of(const seq_pat *p, int i, uint32_t len, uint32_t *gap)
{
    uint32_t best = 0xFFFFFFFFu; int at = -1;
    for (int j = 0; j < p->n; j++) {
        if (j == i || p->ev[j].tick >= len) continue;
        uint32_t d = (p->ev[j].tick + len - p->ev[i].tick) % len;
        if (d && d < best) { best = d; at = j; }
    }
    *gap = best; return at;
}
static uint32_t slide_len(const seq_pat *p, int i, uint32_t len)
{
    uint32_t g; return next_of(p, i, len, &g) >= 0 ? g + SEQ_TPS / 8u : p->ev[i].len;
}
static int slid_into(const seq_pat *p, int i, uint32_t len)
{
    for (int j = 0; j < p->n; j++) {
        if (j == i || !(p->ev[j].ratchet & SEQ_SLIDE) || p->ev[j].tick >= len) continue;
        uint32_t g; if (next_of(p, j, len, &g) >= 0 && (p->ev[j].tick + g) % len == p->ev[i].tick) return 1;
    }
    return 0;
}
/* the notes of track t in absolute ticks [a, b), b - a below the pattern's length */
static void play_range(seq_t *s, int trk, uint32_t a, uint32_t b)
{
    seq_track *k = &s->t[trk];
    const seq_pat *p = &k->pat[k->cur];
    uint32_t len = plen(k), r = (a - k->origin) % len, loop = (a - k->origin) / len;
    /* the ends first: a note played again on the tick where its last one ends is ended, then started */
    for (int i = 0; i < 16; i++)
        if (k->sounding[i].on && (int32_t)(k->sounding[i].end - b) < 0) {
            k->sounding[i].on = 0; if (s->note_off) s->note_off(s, trk, k->sounding[i].note);
        }
    /* the locks of the step playing now: applied when it starts, the sound's own values back after it */
    if (p->nlock || k->lock_step >= 0) {
        int step = (int)(((b - 1u) - k->origin) % len / SEQ_TPS);
        if (step != k->lock_step) {
            seq_lock here[SEQ_LOCKS]; int n = 0;
            for (int i = 0; i < p->nlock; i++) if (p->lk[i].step == step) here[n++] = p->lk[i];
            if (n || k->lock_step >= 0) { if (s->locks) s->locks(s, trk, here, n); }
            k->lock_step = (int16_t)(n ? step : -1);
        }
    }
    /* the ratchets' later hits */
    for (int i = 0; i < 8; i++)
        if (k->rat[i].on && (int32_t)(k->rat[i].at - b) < 0) {
            k->rat[i].on = 0;
            if (!k->mute) start_note(s, trk, k->rat[i].note, k->rat[i].vel, k->rat[i].at, k->rat[i].len);
        }
    if (k->mute) return;
    for (int i = 0; i < p->n; i++) {
        const seq_ev *e = &p->ev[i];
        if (e->tick >= len) continue;                    /* past a shortened end: kept, not played */
        uint32_t sw = (uint32_t)s->swing + k->swing; if (sw > 100) sw = 100;
        if (DIV_NUM[k->div < SEQ_NDIV ? k->div : SEQ_DIV_16] == 3) sw = 0;   /* (triplets are never swung) */
        uint32_t et = eff_tick(e, k->quant, len, sw), d = (et + len - r) % len, t = a + d;
        if (t >= b) continue;
        if (!cond_ok(s, e->cond, t, loop + (r + d >= len))) continue;
        uint32_t nr = SEQ_RAT(e->ratchet), gap = SEQ_TPS / nr;
        uint32_t l = nr > 1 ? (e->len < gap ? e->len : gap * 3u / 4u) : e->len;
        if (k->gate && k->gate != 100) { l = l * k->gate / 100u; if (l < 4u) l = 4u; }
        int v = e->vel | ((e->ratchet & SEQ_ACCENT) ? SEQ_ON_ACC : 0);
        if (e->ratchet & SEQ_SLIDE) l = slide_len(p, i, len);           /* on into the next note */
        if (slid_into(p, i, len)) v |= SEQ_ON_SLID;
        start_note(s, trk, e->note, v, t, l);
        for (uint32_t h = 1; h < nr; h++)
            for (int j = 0; j < 8; j++) if (!k->rat[j].on) {
                k->rat[j].on = 1; k->rat[j].note = e->note; k->rat[j].vel = (uint8_t)(e->vel * (8u - h) / 8u);
                k->rat[j].len = (uint16_t)l; k->rat[j].at = t + h * gap;
                break;
            }
    }
}
/* the next point where a queued change happens, after a (the track's ticks; g: the sequencer's tick then) */
static uint32_t global_of(const seq_track *k, uint32_t local)    /* a track's tick as the sequencer's (rounded up) */
{
    int d = k->div < SEQ_NDIV ? k->div : SEQ_DIV_16;
    return d == SEQ_DIV_16 ? local : (uint32_t)(((uint64_t)local * DIV_DEN[d] + DIV_NUM[d] - 1u) / DIV_NUM[d]);
}
static uint32_t change_point(const seq_t *s, int trk, uint32_t a, uint32_t g)
{
    const seq_track *k = &s->t[trk];
    if (s->switch_now && !s->song_play) {
        /* (3.3.7) at the end of the lead track's pattern, the same moment for every track (each its own end: tracks
         * of other lengths drifted apart) */
        const seq_track *l = &s->t[s->lead < SEQ_NTRK ? s->lead : 0];
        uint32_t la = local_of(l, g), len = plen(l), r = (la - l->origin) % len;
        uint32_t c = local_of(k, global_of(l, r ? la + len - r : la));
        return c > a ? c : a;
    }
    uint32_t c = local_of(k, (g + SEQ_BAR - 1) / SEQ_BAR * SEQ_BAR);   /* the sequencer's next bar */
    return c > a ? c : a;
}

/* ---------------------------------------------------------------- the song */
uint32_t seq_song_bar(const seq_t *s)
{
    uint32_t b = seq_bar(s) - s->song_start;
    return s->song_play && s->song_len ? b % s->song_len : b;
}
static void song_capture(seq_t *s, uint32_t bar)                /* what plays from this bar, if it changed */
{
    seq_song_ev e; e.bar = (uint16_t)(bar - s->song_start); e.mute = 0;
    for (int t = 0; t < SEQ_NTRK; t++) { e.pat[t] = s->t[t].next != SEQ_NONE ? s->t[t].next : s->t[t].cur; if (s->t[t].mute) e.mute |= (uint8_t)(1u << t); }
    if (s->song_n) {
        seq_song_ev *l = &s->song[s->song_n - 1];
        if (l->mute == e.mute && !memcmp(l->pat, e.pat, SEQ_NTRK)) return;
        if (l->bar == e.bar) { *l = e; return; }
    }
    if (s->song_n < SEQ_SONG) s->song[s->song_n++] = e;
}
void seq_song_record(seq_t *s, int on)
{
    if (on) {
        s->song_rec = 1; s->song_play = 0; s->song_n = 0; s->song_len = 0;
        s->song_start = s->playing ? seq_bar(s) + 1u : 0u;       /* from the next bar */
        if (!s->playing) song_capture(s, 0);
    } else if (s->song_rec) {
        s->song_rec = 0;
        uint32_t end = s->playing ? seq_bar(s) + 1u : s->song_start + 1u;
        s->song_len = (uint16_t)(end > s->song_start ? end - s->song_start : 1u);
        if (!s->song_n) s->song_len = 0;
    }
}
void seq_song_play(seq_t *s, int on)
{
    s->song_play = (uint8_t)(on && s->song_n && s->song_len);
    if (s->song_play) { s->song_rec = 0; s->song_start = s->playing ? seq_bar(s) + 1u : 0u; }
}
/* the song by hand (as SLOOP's SONG screen): its entries, each from its bar to the next one's */
int seq_song_len_of(const seq_t *s, int i)
{
    if (i < 0 || i >= s->song_n) return 0;
    int end = i + 1 < s->song_n ? s->song[i + 1].bar : s->song_len;
    return end > s->song[i].bar ? end - s->song[i].bar : 1;
}
SEQ_COLD int seq_song_set_len(seq_t *s, int i, int bars)
{
    if (i < 0 || i >= s->song_n) return 0;
    bars = bars < 1 ? 1 : bars > SEQ_SONG_BARS ? SEQ_SONG_BARS : bars;
    int d = bars - seq_song_len_of(s, i);
    if (!d || (int)s->song_len + d > 65535) return 0;
    for (int j = i + 1; j < s->song_n; j++) s->song[j].bar = (uint16_t)(s->song[j].bar + d);
    s->song_len = (uint16_t)(s->song_len + d);
    return 1;
}
void seq_song_set_pat(seq_t *s, int i, int trk, int pat)
{
    if (i < 0 || i >= s->song_n || pat < 0 || pat >= SEQ_NPAT) return;
    for (int t = 0; t < SEQ_NTRK; t++) if (trk < 0 || t == trk) s->song[i].pat[t] = (uint8_t)pat;
}
/* n entries: more, each a copy of the last for 4 bars (the first: what plays now); fewer, the last ones dropped */
SEQ_COLD void seq_song_set_count(seq_t *s, int n)
{
    n = n < 0 ? 0 : n > SEQ_SONG ? SEQ_SONG : n;
    if (n < s->song_n) {
        int keep = n ? seq_song_len_of(s, n - 1) : 0;
        s->song_n = (uint16_t)n;
        s->song_len = (uint16_t)(n ? s->song[n - 1].bar + keep : 0);
        if (!n) s->song_play = 0;
        return;
    }
    while (s->song_n < n) {
        seq_song_ev e;
        if (s->song_n) e = s->song[s->song_n - 1];
        else { e.mute = 0; for (int t = 0; t < SEQ_NTRK; t++) { e.pat[t] = s->t[t].cur; if (s->t[t].mute) e.mute |= (uint8_t)(1u << t); } }
        if ((int)s->song_len + 4 > 65535) break;
        e.bar = s->song_len;
        s->song[s->song_n++] = e; s->song_len = (uint16_t)(s->song_len + 4);
    }
}
/* the entry playing (the song's bar now), -1 none */
int seq_song_at(const seq_t *s)
{
    if (!s->song_play || !s->song_n || !s->playing || seq_bar(s) < s->song_start) return -1;
    uint32_t b = seq_song_bar(s); int i = 0;
    while (i + 1 < s->song_n && s->song[i + 1].bar <= b) i++;
    return i;
}
/* at a bar line: the song's changes for that bar are queued (they happen on it), a recording takes its picture */
static void song_bar(seq_t *s, uint32_t bar)
{
    if (s->song_rec && bar >= s->song_start) song_capture(s, bar);
    if (s->song_play && s->song_len && bar >= s->song_start) {
        uint32_t sb = (bar - s->song_start) % s->song_len;
        for (int i = 0; i < s->song_n; i++) if (s->song[i].bar == sb) {
            int any = 0; for (int t = 0; t < SEQ_NTRK; t++) if (s->song[i].pat[t] != s->t[t].cur) any = 1;
            for (int t = 0; t < SEQ_NTRK; t++) {
                seq_track *k = &s->t[t];
                k->next = any ? s->song[i].pat[t] : SEQ_NONE;          /* (3.3.7: a change starts them all, together) */
                k->mute = (uint8_t)((s->song[i].mute >> t) & 1u);
            }
        }
    }
}

void seq_advance(seq_t *s, uint32_t frames)
{
    if (!s->playing) return;
    seq_advance_by(s, (uint64_t)(uint32_t)(tick_per_frame(s) * 65536.0f + 0.5f) * frames);   /* (32 bits: under 2^32 at any tempo, and no soft double) */
}
void seq_advance_by(seq_t *s, uint64_t inc)
{
    if (!s->playing) return;
    s->pos += inc;
    uint32_t a = s->tick, b = (uint32_t)(s->pos >> 16);
    if (b == a) return;
    /* a bar line in this block: the song first (its changes are then queued for that very line) */
    uint32_t line = (a + SEQ_BAR - 1) / SEQ_BAR * SEQ_BAR;
    if (line < b && (s->song_rec || s->song_play)) song_bar(s, line / SEQ_BAR);
    for (int t = 0; t < SEQ_NTRK; t++) {
        seq_track *k = &s->t[t];
        uint32_t x = local_of(k, a), lb = local_of(k, b);
        while (x < lb) {
            uint32_t y = lb;
            if (k->next != SEQ_NONE) {
                uint32_t c = change_point(s, t, x, a);
                if (c < lb) {
                    if (c > x) play_range(s, t, x, c);
                    k->cur = k->next; k->next = SEQ_NONE; k->origin = c;
                    x = c;
                    continue;
                }
            }
            uint32_t len = plen(k);
            if (y - x > len) y = x + len;                    /* (a block never spans a pattern: guard only) */
            play_range(s, t, x, y);
            x = y;
        }
    }
    s->tick = b;
    if (s->fill_bar && seq_bar(s) + 1u > s->fill_bar) s->fill_bar = 0;   /* that bar is over */
}

void seq_catch_up(seq_t *s, int trk, uint32_t from)
{
    seq_track *k = &s->t[trk];
    uint32_t a = local_of(k, from), b = local_of(k, s->tick), len = plen(k);
    if (!s->playing || (int32_t)(b - a) <= 0) return;
    if (b - a > len) a = b - len;
    play_range(s, trk, a, b);
}

/* ---------------------------------------------------------------- live recording */
void seq_record(seq_t *s, int on, int trk)
{
    if (on && !s->recording) { s->rec_trk = trk; edit(s, trk, s->t[trk].cur); }   /* the whole take: one undo */
    s->recording = on;
    if (!on) memset(s->held, 0, sizeof s->held);
}
uint32_t seq_heard(const seq_t *s, uint32_t latency_frames)
{
    uint32_t lat = (uint32_t)((float)latency_frames * tick_per_frame(s) + 0.5f);
    return s->tick > lat ? s->tick - lat : 0;
}
void seq_key(seq_t *s, int note, int vel, int down, uint32_t latency_frames)
{
    if (!s->playing || !s->recording) return;
    /* where the player heard it: the output's delay behind the tick that was being computed */
    uint32_t lat = (uint32_t)((float)latency_frames * tick_per_frame(s) + 0.5f);
    uint32_t now = local_of(&s->t[s->rec_trk], s->tick > lat ? s->tick - lat : 0);
    if (down) {
        for (int i = 0; i < SEQ_HELD; i++)
            if (!s->held[i].on) { s->held[i].on = 1; s->held[i].note = (uint8_t)note; s->held[i].vel = (uint8_t)vel; s->held[i].start = now; return; }
        return;
    }
    for (int i = 0; i < SEQ_HELD; i++) {
        if (!s->held[i].on || s->held[i].note != note) continue;
        s->held[i].on = 0;
        seq_track *k = &s->t[s->rec_trk];
        uint32_t len = plen(k), st = s->held[i].start;
        int64_t rel = (int64_t)st - (int64_t)k->origin;
        uint32_t local = (uint32_t)(((rel % (int64_t)len) + (int64_t)len) % (int64_t)len);
        uint32_t l = now - st; if (l < SEQ_TPS / 8) l = SEQ_TPS / 8; if (l > len - 1) l = len - 1;
        seq_ev e = { (uint16_t)local, (uint16_t)l, (uint8_t)note, s->held[i].vel, SEQ_ALWAYS, 1 };
        seq_pat *p = &k->pat[k->cur];
        /* (3.3.7) the same note played again within half a step of one already there (a loop played over and over):
         * it takes that one's place, as a step holds a note once; the take goes on adding the other notes */
        for (int j = p->n - 1; j >= 0; j--) {
            if (p->ev[j].note != note) continue;
            uint32_t dt = (p->ev[j].tick + len - local) % len; if (dt > len / 2u) dt = len - dt;
            if (dt < SEQ_TPS / 2u) remove_at(p, j);
        }
        insert(p, e);
        return;
    }
}
