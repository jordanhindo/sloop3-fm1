/* SPDX-License-Identifier: GPL-3.0-only
 * The free-form looper's tests: the fit's worked literals, the close's
 * exact ticks, the batch and undo behaviour, the cancel paths. Times
 * are injected as absolute microseconds; the literals are hand-computed
 * from the spec (section 4), never recomputed by calling loop_fit or
 * seq_local in the test. */
#include "../seq/loop.h"
#include "../seq/seq.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static int checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static seq_t S;
static seq_hist H;
static loop_t L;
#define RATE 44100.0f
static struct { int trk, note, vel, on; uint32_t tick; } log_[4096];
static int nlog, held_notes;
static void on_(seq_t *s, int t, int n, int v) { if (nlog < 4096) { log_[nlog].trk = t; log_[nlog].note = n; log_[nlog].vel = v; log_[nlog].on = 1; log_[nlog].tick = s->tick; nlog++; } held_notes++; }
static void off_(seq_t *s, int t, int n) { if (nlog < 4096) { log_[nlog].trk = t; log_[nlog].note = n; log_[nlog].on = 0; log_[nlog].tick = s->tick; nlog++; } held_notes--; }
static int ons(int trk, int note) { int n = 0; for (int i = 0; i < nlog; i++) if (log_[i].on && log_[i].trk == trk && (note < 0 || log_[i].note == note)) n++; return n; }
/* run until the absolute tick reaches t, in 64-frame blocks; returns the frames run */
static uint32_t run_to(uint32_t t) { uint32_t f = 0; while (S.tick < t) { seq_advance(&S, 64); f += 64; } return f; }
static void reset(float bpm) { seq_init(&S, RATE, bpm, &H); S.note_on = on_; S.note_off = off_; nlog = 0; held_notes = 0; loop_init(&L); }
static int near_tick(uint32_t a, uint32_t b) { return a >= b ? a - b <= 1 : b - a <= 1; }
/* the pattern event for a note on a track, or -1 */
static int find_ev(int trk, int note, uint32_t *tick, uint32_t *len)
{
    seq_pat *p = &S.t[trk].pat[S.t[trk].cur];
    for (int i = 0; i < p->n; i++) if (p->ev[i].note == note) { *tick = p->ev[i].tick; *len = p->ev[i].len; return i; }
    return -1;
}
static void note_on(int trk, int note, int vel, uint32_t now_us) { loop_note(&L, trk, note, vel, 1, now_us); }
static void note_off(int trk, int note, uint32_t now_us) { loop_note(&L, trk, note, 0, 0, now_us); }
/* a 2 s take at set 92: notes at 0 / 500,000 / 1,500,000 us, closed at 2,000,000 us */
static void take_2s(void)
{
    note_on(0, 60, 100, 0);
    note_on(0, 64, 100, 500000);
    note_off(0, 64, 1500000);
    note_on(0, 67, 100, 1500000);
}

static void fit_test(void)
{
    printf("fit: the worked literals\n");
    int nb; float bpm;
    loop_fit(2000000u, 92.0f, &nb, &bpm);
    CHECK(nb == 1 && bpm == 120.0f, "92, 2 s: %d bars %.1f BPM", nb, bpm);
    loop_fit(1000000u, 92.0f, &nb, &bpm);
    CHECK(nb == 1 && bpm == 240.0f, "92, 1 s: %d bars %.1f BPM", nb, bpm);
    loop_fit(4000000u, 92.0f, &nb, &bpm);
    CHECK(nb == 2 && bpm == 120.0f, "92, 4 s: %d bars %.1f BPM", nb, bpm);
    loop_fit(2040000u, 120.0f, &nb, &bpm);
    CHECK(nb == 1 && bpm == 120.0f, "120, 2.04 s: %d bars %.1f BPM (3%% rule)", nb, bpm);
    loop_fit(500000u, 92.0f, &nb, &bpm);
    CHECK(nb == 1 && bpm == 92.0f, "92, 0.5 s: %d bars %.1f BPM (fallback)", nb, bpm);
    /* 2BAR: the fit's answer only (1 bar / 120); the close rounds nb 1 -> 2 (two_bar_test) */
    loop_fit(2000000u, 92.0f, &nb, &bpm);
    CHECK(nb == 1 && bpm == 120.0f, "92 2BAR fit: %d bars %.1f BPM", nb, bpm);
}

static void close_test(void)
{
    printf("close: a 2 s take at set 92 -> 1 bar, 120 BPM, the notes at exact ticks\n");
    reset(92.0f);
    seq_set_len(&S, 1, 32);                       /* track 1 holds a 32-step pattern (A3) */
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused on an empty track");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d bars", nb);
    CHECK(S.bpm == 120.0f, "BPM %.1f, not 120", S.bpm);
    CHECK(S.playing, "the transport did not start");
    CHECK(S.t[0].pat[S.t[0].cur].steps == 16, "armed track %d steps", S.t[0].pat[S.t[0].cur].steps);
    CHECK(S.t[1].pat[S.t[1].cur].steps == 32, "other track %d steps (A3)", S.t[1].pat[S.t[1].cur].steps);
    CHECK(seq_count(&S, 0) == 3, "%d events", seq_count(&S, 0));
    uint32_t tick, len;
    CHECK(find_ev(0, 60, &tick, &len) >= 0 && tick == 0, "note 60 at tick %u", tick);
    CHECK(find_ev(0, 64, &tick, &len) >= 0 && near_tick(tick, 384) && near_tick(len, 768),
          "note 64 at tick %u len %u (want 384 / 768)", tick, len);
    CHECK(find_ev(0, 67, &tick, &len) >= 0 && near_tick(tick, 1152) && near_tick(len, 384),
          "note 67 at tick %u len %u (want 1152 / 384)", tick, len);
}

static void two_bar_test(void)
{
    printf("2BAR: the fit says 1 bar / 120, the close gives 2 bars / 240 / 1 step\n");
    reset(92.0f);
    seq_set_div(&S, 0, SEQ_DIV_2BAR);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused on an empty 2BAR track");
    note_on(0, 60, 100, 0);
    note_off(0, 60, 1000000);
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 2, "close returned %d bars", nb);
    CHECK(S.bpm == 240.0f, "BPM %.1f, not 240", S.bpm);
    CHECK(S.t[0].pat[S.t[0].cur].steps == 1, "2BAR pattern %d steps", S.t[0].pat[S.t[0].cur].steps);
    uint32_t tick, len;
    CHECK(find_ev(0, 60, &tick, &len) >= 0 && tick == 0 && len == 48,
          "note 60 at tick %u len %u (want 0 / 48)", tick, len);
}

static void boundary_test(void)
{
    printf("boundaries: 399,999 us is empty, 400,000 us closes, poll at 24 s\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    int nb = loop_close(&L, &S, 399999u);
    CHECK(nb == 0, "399,999 us closed as %d bars", nb);
    CHECK(S.bpm == 92.0f && !S.playing && seq_count(&S, 0) == 0, "399,999 us changed something");
    CHECK(L.armed_trk == -1 && L.rec == 0, "empty take left the take open");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    nb = loop_close(&L, &S, 400000u);
    CHECK(nb == 1, "400,000 us returned %d", nb);
    CHECK(S.bpm == 92.0f, "400,000 us BPM %.1f (fallback)", S.bpm);
    CHECK(seq_count(&S, 0) == 1, "400,000 us: %d events", seq_count(&S, 0));
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    CHECK(loop_poll(&L, &S, 24000000u) == 0, "poll closed at exactly 24 s");
    CHECK(L.rec == 1, "poll dropped the take at exactly 24 s");
    int pb = loop_poll(&L, &S, 24000001u);
    CHECK(pb > 0, "poll did not close at 24,000,001 us (%d)", pb);
    CHECK(L.armed_trk == -1, "poll left the take open");
}

static void cancel_test(void)
{
    printf("cancel: PLAY during capture drops the take, keeps the old pattern\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    note_on(0, 64, 100, 100000);
    loop_cancel(&L);
    CHECK(L.armed_trk == -1 && L.rec == 0 && L.n == 0, "cancel left the take open");
    CHECK(seq_count(&S, 0) == 0 && S.bpm == 92.0f && !S.playing, "cancel changed the project");
    /* a fresh arm works after a cancel, and sees no stale events from the cancelled take */
    CHECK(loop_arm(&L, &S, 0) == 1, "re-arm refused after cancel");
    note_on(0, 60, 100, 0);
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && seq_count(&S, 0) == 1, "re-take after cancel: %d bars, %d events", nb, seq_count(&S, 0));
}

static void empty_test(void)
{
    printf("empty take: no notes, and under 0.4 s, change nothing\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    S.batch = 1; S.batch_done = 1;               /* an open batch: the empty exit leaves it untouched */
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 0, "no-note take returned %d", nb);
    CHECK(S.batch == 1 && S.batch_done == 1, "empty take touched the batch");
    CHECK(S.bpm == 92.0f && !S.playing && H.n_undo == 0, "empty take changed something");
    CHECK(L.armed_trk == -1 && L.rec == 0, "empty take left the take open");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    nb = loop_close(&L, &S, 300000u);
    CHECK(nb == 0 && seq_count(&S, 0) == 0, "short take wrote events");
    CHECK(L.armed_trk == -1 && L.rec == 0, "short take left the take open");
}

static void undo_batch_test(void)
{
    printf("undo: the close is one step; the batch is restored to its entry value\n");
    /* entry (0, 0): no batch open */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d", nb);
    CHECK(H.n_undo == 1, "the close took %d undo steps", H.n_undo);
    CHECK(S.batch == 0 && S.batch_done == 0, "batch not restored to 0 (batch=%d done=%d)", S.batch, S.batch_done);
    CHECK(seq_undo(&S) && seq_count(&S, 0) == 0, "undo did not remove the take");
    /* entry (0, 0): a following edit increments n_undo separately */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    take_2s();
    loop_close(&L, &S, 2000000u);
    int u0 = H.n_undo;
    seq_step_toggle(&S, 0, 4, 60, 100);
    CHECK(H.n_undo == u0 + 1, "following edit did not take its own undo step (%d -> %d)", u0, H.n_undo);
    /* entry (1, 1): a following edit joins the restored batch */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    take_2s();
    S.batch = 1; S.batch_done = 1;
    nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d", nb);
    CHECK(H.n_undo == 1, "the close took %d undo steps", H.n_undo);
    CHECK(S.batch == 1 && S.batch_done == 1, "batch not restored to (1, 1) (batch=%d done=%d)", S.batch, S.batch_done);
    u0 = H.n_undo;
    seq_step_toggle(&S, 0, 4, 60, 100);
    CHECK(H.n_undo == u0, "following edit took its own undo step (%d -> %d)", u0, H.n_undo);
    /* entry (1, 0): a following edit pushes a snapshot */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    take_2s();
    S.batch = 1; S.batch_done = 0;
    nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d", nb);
    CHECK(H.n_undo == 1, "the close took %d undo steps", H.n_undo);
    CHECK(S.batch == 1 && S.batch_done == 0, "batch not restored to (1, 0) (batch=%d done=%d)", S.batch, S.batch_done);
    u0 = H.n_undo;
    seq_step_toggle(&S, 0, 4, 60, 100);
    CHECK(H.n_undo == u0 + 1, "following edit did not push a snapshot (%d -> %d)", u0, H.n_undo);
}

static void early_return_test(void)
{
    printf("early return: a 1-bar div-16 take on a default 16-step pattern is still one undo step\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    CHECK(S.t[0].pat[0].steps == 16, "default pattern is not 16 steps");
    note_on(0, 60, 100, 0);
    note_off(0, 60, 1000000);
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d", nb);
    CHECK(H.n_undo == 1, "the close took %d undo steps", H.n_undo);
    CHECK(seq_count(&S, 0) == 1, "the take was not written");
    CHECK(seq_undo(&S) && seq_count(&S, 0) == 0, "undo did not remove the take");
}

static void arm_test(void)
{
    printf("arm: refused on a content track, moves while armed, only the armed track starts the capture\n");
    reset(92.0f);
    seq_step_toggle(&S, 0, 0, 60, 100);          /* content on track 0 */
    CHECK(loop_arm(&L, &S, 0) == 0, "arm accepted on a content track");
    CHECK(L.armed_trk == -1, "a refused arm opened a take");
    reset(92.0f);
    S.recording = 1;                             /* grid recording in progress */
    CHECK(loop_arm(&L, &S, 0) == 0, "arm accepted while grid recording");
    S.recording = 0;
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused after grid recording stopped");
    /* armed but not capturing: arming a second empty track moves the arm */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused on empty track 0");
    CHECK(loop_arm(&L, &S, 1) == 1, "arm-move refused");
    CHECK(L.armed_trk == 1, "the arm did not move to track 1");
    CHECK(L.arm_pat == S.t[1].cur, "arm_pat not updated on the move");
    /* a capture open refuses a move */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    CHECK(L.rec == 1 && L.n == 1, "the capture did not start");
    CHECK(loop_arm(&L, &S, 1) == 0, "arm-move accepted during a capture");
    CHECK(L.armed_trk == 0, "a refused move changed the arm");
    /* only the armed track's notes start the capture */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(1, 60, 100, 0);
    CHECK(L.rec == 0 && L.n == 0, "another track's note started the capture");
    note_off(1, 60, 100000);
    CHECK(L.n == 0, "a stray note-off captured something");
    note_on(0, 60, 100, 200000);
    CHECK(L.rec == 1 && L.t0_us == 200000u && L.n == 1, "the armed track's note did not start the capture");
}

static void pattern_changed_test(void)
{
    printf("pattern changed mid-take: the close cancels, nothing changes\n");
    reset(92.0f);
    seq_set_len(&S, 1, 32);                       /* track 1's length, to prove it is untouched */
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    note_off(0, 60, 1000000);
    int u0 = H.n_undo;
    S.t[0].cur = 1;                               /* the pattern changed mid-take */
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == -1, "pattern-changed close returned %d", nb);
    CHECK(S.bpm == 92.0f, "BPM changed on a cancelled take");
    CHECK(S.t[0].pat[0].steps == 16 && S.t[0].pat[1].steps == 16, "a pattern length changed");
    CHECK(S.t[1].pat[S.t[1].cur].steps == 32, "track 1's length changed");
    CHECK(S.batch == 0 && S.batch_done == 0, "the batch was touched");
    CHECK(H.n_undo == u0, "n_undo changed on a cancelled take");
    CHECK(S.t[0].pat[0].n == 0 && S.t[0].pat[1].n == 0, "events were written on a cancelled take");
    CHECK(L.armed_trk == -1 && L.rec == 0, "the take was not dropped");
    CHECK(!S.playing, "the transport started on a cancelled take");
}

/* ---- V1: multi-track arm/close/overdub ---- */

static void later_close_test(void)
{
    printf("later close: while playing, a close on track 2 leaves the BPM and track 1 alone\n");
    reset(120.0f);
    seq_add(&S, 0, 0, 96, 60, 100);               /* track 1 already has a loop */
    seq_add(&S, 0, 384, 96, 64, 100);
    seq_set_len(&S, 0, 16);
    seq_play(&S, 1);
    run_to(4 * SEQ_BAR);
    int steps0 = S.t[0].pat[S.t[0].cur].steps, n0 = seq_count(&S, 0);
    CHECK(loop_arm(&L, &S, 1) == 1, "arm refused on empty track 2 while playing");
    note_on(1, 67, 100, 0);
    note_off(1, 67, 1000000);
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "later close returned %d bars", nb);
    CHECK(S.bpm == 120.0f, "later close changed the BPM to %.1f", S.bpm);
    CHECK(S.playing, "later close stopped the transport");
    CHECK(S.t[0].pat[S.t[0].cur].steps == steps0 && seq_count(&S, 0) == n0,
          "track 1 changed (steps %d, %d events)", S.t[0].pat[S.t[0].cur].steps, seq_count(&S, 0));
    CHECK(S.t[1].pat[S.t[1].cur].steps == 16, "track 2 is %d steps", S.t[1].pat[S.t[1].cur].steps);
    CHECK(seq_count(&S, 1) == 1, "track 2 has %d events", seq_count(&S, 1));
}

static void origin_test(void)
{
    printf("origin: set as the adapter does, the first event fires in the next block\n");
    reset(120.0f);
    seq_play(&S, 1);
    run_to(2 * SEQ_BAR);                          /* the transport is running */
    CHECK(loop_arm(&L, &S, 1) == 1, "arm refused on empty track 2 while playing");
    note_on(1, 67, 100, 0);
    note_off(1, 67, 1000000);
    uint32_t tick = S.tick;                       /* snapshot before the close, as the adapter does */
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d bars", nb);
    S.t[1].origin = seq_local(&S, 1, tick);       /* the adapter's origin write */
    nlog = 0;
    seq_advance(&S, 64);                          /* one 64-frame block */
    CHECK(ons(1, 67) == 1, "the first event did not fire in the next block (%d note-ons)", ons(1, 67));
}

static void div8_test(void)
{
    printf("div 1/8: a take gets steps = bars x 8, events at the right local ticks\n");
    reset(120.0f);
    seq_set_div(&S, 0, SEQ_DIV_8);
    seq_play(&S, 1);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused on an empty 1/8 track");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d bars", nb);
    CHECK(S.bpm == 120.0f, "BPM %.1f", S.bpm);
    CHECK(S.t[0].pat[S.t[0].cur].steps == 8, "1/8 pattern %d steps (want 8)", S.t[0].pat[S.t[0].cur].steps);
    uint32_t tick, len;
    CHECK(find_ev(0, 64, &tick, &len) >= 0 && near_tick(tick, 192) && near_tick(len, 384),
          "note 64 at tick %u len %u (want 192 / 384)", tick, len);
    CHECK(find_ev(0, 67, &tick, &len) >= 0 && near_tick(tick, 576),
          "note 67 at tick %u (want 576)", tick);
}

static void div32_test(void)
{
    printf("div 1/32: a playing close at ~6 bars clamps to 4 bars, 128 steps\n");
    reset(120.0f);
    seq_set_div(&S, 0, SEQ_DIV_32);
    seq_play(&S, 1);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused on an empty 1/32 track");
    note_on(0, 60, 100, 0);
    note_off(0, 60, 6000000);
    int nb = loop_close(&L, &S, 12000000u);
    CHECK(nb == 4, "close returned %d bars (want 4)", nb);
    CHECK(S.bpm == 120.0f, "BPM %.1f", S.bpm);
    CHECK(S.t[0].pat[S.t[0].cur].steps == 128, "1/32 pattern %d steps (want 128)", S.t[0].pat[S.t[0].cur].steps);
    uint32_t tick, len;
    CHECK(find_ev(0, 60, &tick, &len) >= 0 && tick == 0 && near_tick(len, 6144),
          "note 60 at tick %u len %u (want 0 / 6144)", tick, len);
}

static void twobar_follow_test(void)
{
    printf("2BAR follow: 6 s at 120 BPM rounds nb 3 -> 4, 2 steps, BPM untouched\n");
    reset(120.0f);
    seq_set_div(&S, 0, SEQ_DIV_2BAR);
    seq_play(&S, 1);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused on an empty 2BAR track");
    note_on(0, 60, 100, 0);
    note_off(0, 60, 3000000);
    int nb = loop_close(&L, &S, 6000000u);
    CHECK(nb == 4, "close returned %d bars (want 4)", nb);
    CHECK(S.bpm == 120.0f, "BPM %.1f", S.bpm);
    CHECK(S.t[0].pat[S.t[0].cur].steps == 2, "2BAR pattern %d steps (want 2)", S.t[0].pat[S.t[0].cur].steps);
    uint32_t tick, len;
    CHECK(find_ev(0, 60, &tick, &len) >= 0 && tick == 0 && len == 96,
          "note 60 at tick %u len %u (want 0 / 96)", tick, len);
}

static void retake_test(void)
{
    printf("re-take: no pre-existing events survive an erase and a new take\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && seq_count(&S, 0) == 3, "first take: %d bars, %d events", nb, seq_count(&S, 0));
    seq_clear(&S, 0);                             /* erase the old loop first */
    CHECK(seq_count(&S, 0) == 0, "erase left %d events", seq_count(&S, 0));
    CHECK(loop_arm(&L, &S, 0) == 1, "re-arm refused on the erased track");
    note_on(0, 72, 100, 0);
    note_off(0, 72, 1000000);
    nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "second close returned %d bars", nb);
    CHECK(seq_count(&S, 0) == 1, "re-take has %d events (want 1)", seq_count(&S, 0));
    uint32_t tick, len;
    CHECK(find_ev(0, 72, &tick, &len) >= 0 && tick == 0, "the re-take's note is not there");
    CHECK(find_ev(0, 60, &tick, &len) < 0 && find_ev(0, 64, &tick, &len) < 0 && find_ev(0, 67, &tick, &len) < 0,
          "a pre-existing event survived the re-take");
}

/* ---- V2: the BPM rule ---- */

static void second_close_test(void)
{
    printf("second close: while playing, a later close leaves the BPM\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && S.bpm == 120.0f, "first close: %d bars, %.1f BPM", nb, S.bpm);
    /* the transport is playing now; a second take on another track is a follow close */
    CHECK(loop_arm(&L, &S, 1) == 1, "re-arm refused");
    note_on(1, 67, 100, 0);
    note_off(1, 67, 1500000);
    nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "second close returned %d bars", nb);
    CHECK(S.bpm == 120.0f, "second close re-derived the BPM to %.1f", S.bpm);
}

static void rederive_after_stop_test(void)
{
    printf("re-derivation: after a full stop, a new take re-derives the BPM\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && S.bpm == 120.0f, "first close: %d bars, %.1f BPM", nb, S.bpm);
    /* sole loop, re-taken while stopped: erase, stop, arm, play, close */
    seq_clear(&S, 0);
    seq_play(&S, 0);                              /* full stop */
    CHECK(!S.playing, "the transport did not stop");
    CHECK(loop_arm(&L, &S, 0) == 1, "re-arm refused");
    note_on(0, 72, 100, 0);
    note_off(0, 72, 2000000);
    nb = loop_close(&L, &S, 4000000u);            /* 4 s at set 120: 2 bars / 120 BPM */
    CHECK(nb == 2, "re-take close returned %d bars (want 2)", nb);
    CHECK(S.bpm == 120.0f, "re-take BPM %.1f (want 120)", S.bpm);
    CHECK(seq_count(&S, 0) == 1, "re-take has %d events", seq_count(&S, 0));
}

static void bar_rounding_test(void)
{
    printf("bar rounding: a take rounds to the nearest bar at the current BPM\n");
    reset(120.0f);
    seq_play(&S, 1);
    CHECK(loop_arm(&L, &S, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    note_off(0, 60, 1000000);
    int nb = loop_close(&L, &S, 2900000u);        /* 2.9 s at 120 BPM: 1.45 bars -> 1 */
    CHECK(nb == 1, "2.9 s close returned %d bars (want 1)", nb);
    CHECK(S.bpm == 120.0f, "BPM %.1f", S.bpm);
    seq_clear(&S, 0);
    CHECK(loop_arm(&L, &S, 0) == 1, "re-arm refused");
    note_on(0, 60, 100, 0);
    note_off(0, 60, 1000000);
    nb = loop_close(&L, &S, 3100000u);            /* 3.1 s: 1.55 bars -> 2 */
    CHECK(nb == 2, "3.1 s close returned %d bars (want 2)", nb);
    CHECK(S.bpm == 120.0f, "BPM %.1f", S.bpm);
}

static void follow_bars_test(void)
{
    printf("follow bars: the shared helper counts what the close counts\n");
    reset(120.0f);                                 /* 1 bar = 2 s at 120 BPM, div 1/16 */
    CHECK(loop_follow_bars(&S, 0, 2900000u) == 1, "2.9 s -> %d bars (want 1)", loop_follow_bars(&S, 0, 2900000u));
    CHECK(loop_follow_bars(&S, 0, 3100000u) == 2, "3.1 s -> %d bars (want 2)", loop_follow_bars(&S, 0, 3100000u));
    CHECK(loop_follow_bars(&S, 0, 12000000u) == 6, "12 s -> %d bars (want 6)", loop_follow_bars(&S, 0, 12000000u));
    CHECK(loop_follow_bars(&S, 0, 20000000u) == 8, "20 s -> %d bars (want 8, clamped)", loop_follow_bars(&S, 0, 20000000u));
    seq_set_div(&S, 0, SEQ_DIV_32);
    CHECK(loop_follow_bars(&S, 0, 12000000u) == 4, "1/32, 12 s -> %d bars (want 4, div clamp)", loop_follow_bars(&S, 0, 12000000u));
    seq_set_div(&S, 0, SEQ_DIV_2BAR);
    CHECK(loop_follow_bars(&S, 0, 6000000u) == 4, "2BAR, 6 s -> %d bars (want 4, odd rounds up)", loop_follow_bars(&S, 0, 6000000u));
}

int main(void)
{
    fit_test();
    close_test();
    two_bar_test();
    boundary_test();
    cancel_test();
    empty_test();
    undo_batch_test();
    early_return_test();
    arm_test();
    pattern_changed_test();
    later_close_test();
    origin_test();
    div8_test();
    div32_test();
    twobar_follow_test();
    retake_test();
    second_close_test();
    rederive_after_stop_test();
    bar_rounding_test();
    follow_bars_test();
    printf("loop: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
