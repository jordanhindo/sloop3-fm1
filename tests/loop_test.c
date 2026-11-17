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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused on an empty track");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused on an empty 2BAR track");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    int nb = loop_close(&L, &S, 399999u);
    CHECK(nb == 0, "399,999 us closed as %d bars", nb);
    CHECK(S.bpm == 92.0f && !S.playing && seq_count(&S, 0) == 0, "399,999 us changed something");
    CHECK(L.armed_trk == -1 && L.rec == 0, "empty take left the take open");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    nb = loop_close(&L, &S, 400000u);
    CHECK(nb == 1, "400,000 us returned %d", nb);
    CHECK(S.bpm == 92.0f, "400,000 us BPM %.1f (fallback)", S.bpm);
    CHECK(seq_count(&S, 0) == 1, "400,000 us: %d events", seq_count(&S, 0));
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    note_on(0, 64, 100, 100000);
    loop_cancel(&L);
    CHECK(L.armed_trk == -1 && L.rec == 0 && L.n == 0, "cancel left the take open");
    CHECK(seq_count(&S, 0) == 0 && S.bpm == 92.0f && !S.playing, "cancel changed the project");
    /* a fresh arm works after a cancel, and sees no stale events from the cancelled take */
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "re-arm refused after cancel");
    note_on(0, 60, 100, 0);
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && seq_count(&S, 0) == 1, "re-take after cancel: %d bars, %d events", nb, seq_count(&S, 0));
}

static void empty_test(void)
{
    printf("empty take: no notes, and under 0.4 s, change nothing\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    S.batch = 1; S.batch_done = 1;               /* an open batch: the empty exit leaves it untouched */
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 0, "no-note take returned %d", nb);
    CHECK(S.batch == 1 && S.batch_done == 1, "empty take touched the batch");
    CHECK(S.bpm == 92.0f && !S.playing && H.n_undo == 0, "empty take changed something");
    CHECK(L.armed_trk == -1 && L.rec == 0, "empty take left the take open");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1, "close returned %d", nb);
    CHECK(H.n_undo == 1, "the close took %d undo steps", H.n_undo);
    CHECK(S.batch == 0 && S.batch_done == 0, "batch not restored to 0 (batch=%d done=%d)", S.batch, S.batch_done);
    CHECK(seq_undo(&S) && seq_count(&S, 0) == 0, "undo did not remove the take");
    /* entry (0, 0): a following edit increments n_undo separately */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    take_2s();
    loop_close(&L, &S, 2000000u);
    int u0 = H.n_undo;
    seq_step_toggle(&S, 0, 4, 60, 100);
    CHECK(H.n_undo == u0 + 1, "following edit did not take its own undo step (%d -> %d)", u0, H.n_undo);
    /* entry (1, 1): a following edit joins the restored batch */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 0, "arm accepted on a content track");
    CHECK(L.armed_trk == -1, "a refused arm opened a take");
    reset(92.0f);
    S.recording = 1;                             /* grid recording in progress */
    CHECK(loop_arm(&L, &S, 0, 0) == 0, "arm accepted while grid recording");
    S.recording = 0;
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused after grid recording stopped");
    /* armed but not capturing: arming a second empty track moves the arm */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused on empty track 0");
    CHECK(loop_arm(&L, &S, 1, 0) == 1, "arm-move refused");
    CHECK(L.armed_trk == 1, "the arm did not move to track 1");
    CHECK(L.arm_pat == S.t[1].cur, "arm_pat not updated on the move");
    /* a capture open refuses a move */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    note_on(0, 60, 100, 0);
    CHECK(L.rec == 1 && L.n == 1, "the capture did not start");
    CHECK(loop_arm(&L, &S, 1, 0) == 0, "arm-move accepted during a capture");
    CHECK(L.armed_trk == 0, "a refused move changed the arm");
    /* only the armed track's notes start the capture */
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
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
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
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

/* ---- later takes (the transport playing): cut out of the bar grid along bar lines, nothing moved ---- */
#define BAR SEQ_BAR
static void poll_to(uint32_t t, int *nb) { while (S.tick < t) { seq_advance(&S, 64); int r = loop_poll(&L, &S, 0); if (r && nb) *nb = r; } }
static void playing_with_track1(float bpm)
{
    reset(bpm);
    seq_add(&S, 0, 0, 96, 60, 100); seq_add(&S, 0, 384, 96, 64, 100); seq_set_len(&S, 0, 16);
    seq_play(&S, 1);
}
static void later_take_test(void)
{
    printf("later take: recording starts on the next bar line, ends on the bar line after REC, nothing moved\n");
    playing_with_track1(120.0f);
    run_to(3 * BAR + 700);                        /* REC in the middle of the 4th bar */
    int steps0 = S.t[0].pat[S.t[0].cur].steps, n0 = seq_count(&S, 0), nb = 0;
    CHECK(loop_arm(&L, &S, 1, 0) == 1, "arm refused on empty track 2 while playing");
    CHECK(L.follow && L.start == 4 * BAR, "the start line is %u, not the next bar line %u", L.start, 4 * BAR);
    CHECK(!L.rec, "recording before the start line");
    loop_note(&L, 1, 72, 100, 1, 3 * BAR + 900); loop_note(&L, 1, 72, 0, 0, 3 * BAR + 990);   /* played in the wait: heard, not recorded */
    poll_to(4 * BAR, &nb);
    CHECK(L.rec && L.n == 0, "at the start line: rec %d, %d notes (want recording, 0 notes)", L.rec, L.n);
    loop_note(&L, 1, 60, 100, 1, 4 * BAR + 10);   loop_note(&L, 1, 60, 0, 0, 4 * BAR + 210);
    loop_note(&L, 1, 64, 100, 1, 5 * BAR + 777);  loop_note(&L, 1, 64, 0, 0, 5 * BAR + 900);
    loop_note(&L, 1, 67, 100, 1, 8 * BAR + 100);  loop_note(&L, 1, 67, 0, 0, 8 * BAR + 300);
    poll_to(8 * BAR + 800, &nb);                  /* REC in the middle of the 9th bar */
    CHECK(nb == 0 && L.rec, "the take closed before REC (nb %d)", nb);
    loop_stop(&L, &S);
    CHECK(L.closing && L.end == 9 * BAR, "the end line is %u, not %u", L.end, 9 * BAR);
    poll_to(9 * BAR - 64, &nb);
    CHECK(nb == 0 && L.rec, "closed before the end line (nb %d)", nb);
    loop_note(&L, 1, 71, 100, 1, 9 * BAR - 50);   /* still recording between REC and the line */
    nlog = 0;
    poll_to(9 * BAR, &nb);
    CHECK(nb == 5, "the close returned %d bars (want 5: bars 5 to 9)", nb);
    CHECK(S.bpm == 120.0f && S.playing, "the BPM or the transport changed (%.1f, playing %d)", S.bpm, S.playing);
    CHECK(S.t[0].pat[S.t[0].cur].steps == steps0 && seq_count(&S, 0) == n0, "track 1 changed");
    CHECK(S.t[1].pat[S.t[1].cur].steps == 80, "track 2 is %d steps (want 80)", S.t[1].pat[S.t[1].cur].steps);
    CHECK(seq_count(&S, 1) == 4, "track 2 has %d notes (want 4: the one in the wait is not recorded)", seq_count(&S, 1));
    uint32_t tick, len;
    CHECK(find_ev(1, 60, &tick, &len) >= 0 && tick == 10 && len == 200, "note 60 at %u len %u (want 10 / 200, to the tick)", tick, len);
    CHECK(find_ev(1, 64, &tick, &len) >= 0 && tick == BAR + 777 && len == 123, "note 64 at %u len %u (want %u / 123)", tick, len, BAR + 777);
    CHECK(find_ev(1, 67, &tick, &len) >= 0 && tick == 4 * BAR + 100, "note 67 at %u (want %u)", tick, 4 * BAR + 100);
    CHECK(find_ev(1, 71, &tick, &len) >= 0 && tick == 5 * BAR - 50, "note 71 at %u (want %u)", tick, 5 * BAR - 50);
    CHECK(find_ev(1, 72, &tick, &len) < 0, "the note played in the wait was recorded");
    CHECK(S.t[1].origin == 9 * BAR, "the loop's beginning is at %u, not on the end line %u", S.t[1].origin, 9 * BAR);
    /* in phase from then on: note 60 sounds 10 ticks after the 10th bar line, and again 5 bars later, for 16 passes */
    run_to(9 * BAR + 16 * 5 * BAR + 100);
    int k = 0, bad = 0;
    for (int i = 0; i < nlog; i++) if (log_[i].on && log_[i].trk == 1 && log_[i].note == 60) {
        uint32_t want = 9 * BAR + (uint32_t)k * 5 * BAR + 10;
        if (log_[i].tick + 2 < want || log_[i].tick > want + 2) bad++;
        k++;
    }
    CHECK(k == 17 && !bad, "note 60 sounded %d times, %d off its place (want 17, 0)", k, bad);
}
static void downbeat_test(void)
{
    printf("downbeat: a note on the loop's first tick sounds on the first pass\n");
    playing_with_track1(120.0f);
    int nb = 0;
    run_to(BAR + 300);
    CHECK(loop_arm(&L, &S, 1, 1) == 1, "arm refused");
    poll_to(2 * BAR, &nb);
    loop_note(&L, 1, 67, 100, 1, 2 * BAR); loop_note(&L, 1, 67, 0, 0, 2 * BAR + 96);
    nlog = 0;
    poll_to(3 * BAR, &nb);                        /* 1 bar chosen: it closes itself on the line */
    CHECK(nb == 1, "the take did not close itself after the bar chosen (nb %d)", nb);
    run_to(3 * BAR + 40);
    CHECK(ons(1, 67) == 1, "the downbeat sounded %d times on the first pass (want 1)", ons(1, 67));
    run_to(4 * BAR + 40);
    CHECK(ons(1, 67) == 2, "the downbeat sounded %d times after two passes (want 2)", ons(1, 67));
}
static void allowance_test(void)
{
    printf("allowance: half a step either side of a bar line counts as on it\n");
    int nb = 0; uint32_t tick, len;
    playing_with_track1(120.0f);                  /* arming: a hair after the line starts from that line */
    run_to(4 * BAR + 30);
    loop_arm(&L, &S, 1, 0);
    CHECK(L.start == 4 * BAR, "armed 30 ticks after a line: start %u (want that line, %u)", L.start, 4 * BAR);
    loop_cancel(&L);
    run_to(4 * BAR + 60);
    loop_arm(&L, &S, 1, 0);
    CHECK(L.start == 5 * BAR, "armed 60 ticks after a line: start %u (want the next, %u)", L.start, 5 * BAR);
    /* the first note a hair early: recorded, where it was played (just before the loop comes round) */
    loop_note(&L, 1, 60, 100, 1, 5 * BAR - 60); loop_note(&L, 1, 60, 0, 0, 5 * BAR - 50);
    CHECK(L.n == 0, "a note 60 ticks before the start line was recorded");
    loop_note(&L, 1, 62, 100, 1, 5 * BAR - 30); loop_note(&L, 1, 62, 0, 0, 5 * BAR + 40);
    CHECK(L.n == 1, "a note 30 ticks before the start line was not recorded");
    poll_to(5 * BAR + 30, &nb);
    loop_stop(&L, &S);                            /* REC a hair after the start line: at least a bar */
    CHECK(L.end == 6 * BAR, "REC just after the start line: end %u (want one bar, %u)", L.end, 6 * BAR);
    poll_to(6 * BAR, &nb);
    CHECK(nb == 1, "close returned %d bars (want 1)", nb);
    CHECK(find_ev(1, 62, &tick, &len) >= 0 && tick == BAR - 30, "the early note is at %u (want %u)", tick, BAR - 30);
    /* ending: a hair after a line ends on that line, at once */
    playing_with_track1(120.0f);
    run_to(BAR + 200); loop_arm(&L, &S, 1, 0); nb = 0;
    poll_to(2 * BAR + 10, &nb); loop_note(&L, 1, 60, 100, 1, 2 * BAR + 5); loop_note(&L, 1, 60, 0, 0, 2 * BAR + 90);
    poll_to(5 * BAR + 30, &nb);
    loop_stop(&L, &S);
    CHECK(L.end == 5 * BAR, "REC 30 ticks after a line: end %u (want that line, %u)", L.end, 5 * BAR);
    poll_to(5 * BAR + 34, &nb);
    CHECK(nb == 3, "it did not close at once (nb %d, want 3)", nb);
    playing_with_track1(120.0f);
    run_to(BAR + 200); loop_arm(&L, &S, 1, 0); nb = 0;
    poll_to(2 * BAR + 10, &nb); loop_note(&L, 1, 60, 100, 1, 2 * BAR + 5); loop_note(&L, 1, 60, 0, 0, 2 * BAR + 90);
    poll_to(5 * BAR + 60, &nb);
    loop_stop(&L, &S);
    CHECK(L.end == 6 * BAR, "REC 60 ticks after a line: end %u (want the next, %u)", L.end, 6 * BAR);
    loop_stop(&L, &S);                            /* a second REC in the wait does nothing */
    CHECK(L.end == 6 * BAR && L.rec, "a second REC changed the take");
}
static void bars_chosen_test(void)
{
    printf("bars chosen: 1, 2, 4, 8 close by themselves; FREE runs to 8; REC sooner gives a shorter loop\n");
    int nb = 0;
    playing_with_track1(120.0f);
    run_to(BAR + 200); loop_arm(&L, &S, 1, 4);
    CHECK(L.end - L.start == 4 * BAR, "4 chosen: %u bars", (L.end - L.start) / BAR);
    poll_to(2 * BAR + 20, &nb); loop_note(&L, 1, 60, 100, 1, 2 * BAR + 5); loop_note(&L, 1, 60, 0, 0, 2 * BAR + 90);
    poll_to(6 * BAR - 64, &nb);
    CHECK(nb == 0, "closed before the 4 bars were up");
    poll_to(6 * BAR, &nb);
    CHECK(nb == 4 && S.t[1].pat[S.t[1].cur].steps == 64, "4 chosen: closed %d bars, %d steps", nb, S.t[1].pat[S.t[1].cur].steps);
    playing_with_track1(120.0f);
    run_to(BAR + 200); loop_arm(&L, &S, 1, 4); nb = 0;
    poll_to(2 * BAR + 20, &nb); loop_note(&L, 1, 60, 100, 1, 2 * BAR + 5); loop_note(&L, 1, 60, 0, 0, 2 * BAR + 90);
    poll_to(3 * BAR + 500, &nb); loop_stop(&L, &S);          /* REC in the 2nd bar of 4 */
    poll_to(4 * BAR, &nb);
    CHECK(nb == 2, "REC in the 2nd bar of 4: %d bars (want 2)", nb);
    playing_with_track1(120.0f);                  /* FREE: left running, it closes itself after 8 bars */
    run_to(BAR + 200); loop_arm(&L, &S, 1, 0); nb = 0;
    poll_to(2 * BAR + 20, &nb); loop_note(&L, 1, 60, 100, 1, 2 * BAR + 5); loop_note(&L, 1, 60, 0, 0, 2 * BAR + 90);
    poll_to(10 * BAR - 64, &nb);
    CHECK(nb == 0, "FREE closed before 8 bars");
    poll_to(10 * BAR, &nb);
    CHECK(nb == 8 && S.t[1].pat[S.t[1].cur].steps == 128, "FREE left running: %d bars, %d steps (want 8, 128)", nb, S.t[1].pat[S.t[1].cur].steps);
    reset(120.0f); seq_set_div(&S, 0, SEQ_DIV_32);   /* a 1/32 track holds 4 bars */
    CHECK(loop_max_bars(&S, 0) == 4, "1/32: %d bars at most (want 4)", loop_max_bars(&S, 0));
    seq_play(&S, 1); run_to(200); loop_arm(&L, &S, 0, 8);
    CHECK(L.end - L.start == 4 * BAR, "1/32, 8 chosen: %u bars (want 4)", (L.end - L.start) / BAR);
    loop_cancel(&L);
    reset(120.0f); seq_set_div(&S, 0, SEQ_DIV_8);    /* 1/8: steps = bars x 8, the ticks halved */
    seq_play(&S, 1); run_to(200); loop_arm(&L, &S, 0, 1); nb = 0;
    poll_to(BAR + 10, &nb); loop_note(&L, 0, 64, 100, 1, BAR + 384); loop_note(&L, 0, 64, 0, 0, BAR + 768);
    poll_to(2 * BAR, &nb);
    uint32_t tick, len;
    CHECK(nb == 1 && S.t[0].pat[S.t[0].cur].steps == 8, "1/8: %d bars, %d steps (want 1, 8)", nb, S.t[0].pat[S.t[0].cur].steps);
    CHECK(find_ev(0, 64, &tick, &len) >= 0 && tick == 192 && len == 192, "1/8: note at %u len %u (want 192 / 192)", tick, len);
    reset(120.0f); seq_set_div(&S, 0, SEQ_DIV_2BAR); /* 2BAR: an even count */
    seq_play(&S, 1); run_to(200); loop_arm(&L, &S, 0, 1);
    CHECK(L.end - L.start == 2 * BAR, "2BAR, 1 chosen: %u bars (want 2)", (L.end - L.start) / BAR);
}
static void later_edge_test(void)
{
    printf("later take edges: nothing played, the transport stopped, a full loop\n");
    int nb = 0;
    playing_with_track1(120.0f);
    run_to(BAR + 200); loop_arm(&L, &S, 1, 1);
    poll_to(3 * BAR, &nb);
    CHECK(nb == 0 && L.armed_trk < 0 && seq_count(&S, 1) == 0, "nothing played: nb %d, armed %d, %d notes", nb, L.armed_trk, seq_count(&S, 1));
    CHECK(H.n_undo == 0 || H.h[H.n_undo - 1].trk != 1, "an empty later take left an undo step");
    playing_with_track1(120.0f);
    run_to(BAR + 200); loop_arm(&L, &S, 1, 0);
    seq_play(&S, 0);
    CHECK(loop_poll(&L, &S, 0) == -1 && L.armed_trk < 0, "the transport stopped: the take was not cancelled");
    playing_with_track1(120.0f);                  /* 130 notes: 128 kept, and told */
    run_to(BAR + 200); loop_arm(&L, &S, 1, 2); nb = 0;
    poll_to(2 * BAR + 4, &nb);
    for (int i = 0; i < 130; i++) { loop_note(&L, 1, 40 + i % 40, 100, 1, 2 * BAR + 10 + (uint32_t)i * 20); loop_note(&L, 1, 40 + i % 40, 0, 0, 2 * BAR + 20 + (uint32_t)i * 20); }
    poll_to(4 * BAR, &nb);
    CHECK(nb == 2 && seq_count(&S, 1) == 128 && L.full, "130 notes: %d bars, %d kept, full %d (want 2, 128, 1)", nb, seq_count(&S, 1), L.full);
    int u = H.n_undo;
    CHECK(seq_undo(&S) && seq_count(&S, 1) == 0 && H.n_undo == u - 1, "a later take is not one undo step");
    reset(120.0f); seq_play(&S, 1); run_to(5000);
    CHECK(seq_heard(&S, 0) == S.tick, "seq_heard without latency is not the tick");
    CHECK(seq_heard(&S, 441) == S.tick - 8, "seq_heard 441 frames back: %u (want %u, as seq_key counts)", seq_heard(&S, 441), S.tick - 8);
}
static void second_close_test(void)
{
    printf("second take: while playing, it leaves the BPM\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && S.bpm == 120.0f, "first close: %d bars, %.1f BPM", nb, S.bpm);
    run_to(200);
    CHECK(loop_arm(&L, &S, 1, 0) == 1 && L.follow, "re-arm refused, or not a later take");
    nb = 0;
    poll_to(BAR + 4, &nb); loop_note(&L, 1, 67, 100, 1, BAR + 100); loop_note(&L, 1, 67, 0, 0, BAR + 400);
    poll_to(BAR + 900, &nb); loop_stop(&L, &S); poll_to(2 * BAR, &nb);
    CHECK(nb == 1, "second close returned %d bars", nb);
    CHECK(S.bpm == 120.0f, "second close re-derived the BPM to %.1f", S.bpm);
}
static void first_take_bars_test(void)
{
    printf("first take, bars chosen: the take is that many bars and the tempo is worked out from it\n");
    static const struct { int target; uint32_t us; int nb; float bpm; } T[] = {
        { 4, 8000000u, 4, 120.0f }, { 2, 8000000u, 2, 60.0f }, { 8, 8000000u, 8, 240.0f },
        { 8, 5000000u, 2, 96.0f },                 /* 384 BPM is out of range: the FREE fit (2 bars at 96, nearest the 120 set) */
        { 0, 8000000u, 4, 120.0f } };
    for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) {
        reset(120.0f);
        CHECK(loop_arm(&L, &S, 0, T[i].target) == 1 && !L.follow, "arm refused");
        note_on(0, 60, 100, 0); note_off(0, 60, 500000);
        note_on(0, 64, 100, T[i].us / 2); note_off(0, 64, T[i].us / 2 + 500000);
        int nb = loop_close(&L, &S, T[i].us);
        CHECK(nb == T[i].nb && S.bpm == T[i].bpm, "%d chosen, %.1f s: %d bars at %.1f (want %d at %.1f)", T[i].target, T[i].us / 1e6, nb, S.bpm, T[i].nb, T[i].bpm);
        uint32_t tick = 0, len = 0;
        CHECK(find_ev(0, 64, &tick, &len) >= 0 && near_tick(tick, (uint32_t)T[i].nb * BAR / 2), "the note halfway is at %u (want %u)", tick, (uint32_t)T[i].nb * BAR / 2);
    }
    reset(120.0f);                                /* the transport started under a first take: cancelled, no tempo to derive */
    loop_arm(&L, &S, 0, 0); note_on(0, 60, 100, 0); note_off(0, 60, 500000);
    seq_play(&S, 1);
    CHECK(loop_close(&L, &S, 2000000u) == -1 && seq_count(&S, 0) == 0, "a first take closed while playing was not cancelled");
}

static void retake_test(void)
{
    printf("re-take: no pre-existing events survive an erase and a new take\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && seq_count(&S, 0) == 3, "first take: %d bars, %d events", nb, seq_count(&S, 0));
    seq_clear(&S, 0);                             /* erase the old loop first */
    CHECK(seq_count(&S, 0) == 0, "erase left %d events", seq_count(&S, 0));
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "re-arm refused on the erased track");
    nb = 0;                                       /* playing now: a later take, from the line just passed (tick 0) */
    loop_note(&L, 0, 72, 100, 1, 0); loop_note(&L, 0, 72, 0, 0, 500);
    poll_to(900, &nb); loop_stop(&L, &S); poll_to(BAR, &nb);
    CHECK(nb == 1, "second close returned %d bars", nb);
    CHECK(seq_count(&S, 0) == 1, "re-take has %d events (want 1)", seq_count(&S, 0));
    uint32_t tick, len;
    CHECK(find_ev(0, 72, &tick, &len) >= 0 && tick == 0, "the re-take's note is not there");
    CHECK(find_ev(0, 60, &tick, &len) < 0 && find_ev(0, 64, &tick, &len) < 0 && find_ev(0, 67, &tick, &len) < 0,
          "a pre-existing event survived the re-take");
}

/* ---- V2: the BPM rule ---- */

static void rederive_after_stop_test(void)
{
    printf("re-derivation: after a full stop, a new take re-derives the BPM\n");
    reset(92.0f);
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "arm refused");
    take_2s();
    int nb = loop_close(&L, &S, 2000000u);
    CHECK(nb == 1 && S.bpm == 120.0f, "first close: %d bars, %.1f BPM", nb, S.bpm);
    /* sole loop, re-taken while stopped: erase, stop, arm, play, close */
    seq_clear(&S, 0);
    seq_play(&S, 0);                              /* full stop */
    CHECK(!S.playing, "the transport did not stop");
    CHECK(loop_arm(&L, &S, 0, 0) == 1, "re-arm refused");
    note_on(0, 72, 100, 0);
    note_off(0, 72, 2000000);
    nb = loop_close(&L, &S, 4000000u);            /* 4 s at set 120: 2 bars / 120 BPM */
    CHECK(nb == 2, "re-take close returned %d bars (want 2)", nb);
    CHECK(S.bpm == 120.0f, "re-take BPM %.1f (want 120)", S.bpm);
    CHECK(seq_count(&S, 0) == 1, "re-take has %d events", seq_count(&S, 0));
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
    later_take_test();
    downbeat_test();
    allowance_test();
    bars_chosen_test();
    later_edge_test();
    retake_test();
    second_close_test();
    rederive_after_stop_test();
    first_take_bars_test();
    printf("loop: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
