/* SPDX-License-Identifier: GPL-3.0-only
 * The sequencer's tests: timing to the tick, loops of their own length, a pattern change on the bar, mutes kept
 * through it, a live take written where it was heard, the grid pull, undo / redo, no note left on. */
#include "../seq/seq.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static int checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static seq_t S;
static seq_hist H;
#define RATE 44100.0f
static struct { int trk, note, vel, on; uint32_t tick; } log_[4096];
static int nlog, held_notes;
static void on_(seq_t *s, int t, int n, int v) { if (nlog < 4096) { log_[nlog].trk = t; log_[nlog].note = n; log_[nlog].vel = v; log_[nlog].on = 1; log_[nlog].tick = s->tick; nlog++; } held_notes++; }
static void off_(seq_t *s, int t, int n) { if (nlog < 4096) { log_[nlog].trk = t; log_[nlog].note = n; log_[nlog].on = 0; log_[nlog].tick = s->tick; nlog++; } held_notes--; }
static int nlocks_applied, last_lock_n, last_lock_val;
static void lk_(seq_t *s, int t, const seq_lock *l, int n) { (void)s; (void)t; nlocks_applied++; last_lock_n = n; last_lock_val = n ? l[0].value : -1; }
static void reset(float bpm) { seq_init(&S, RATE, bpm, &H); S.note_on = on_; S.note_off = off_; S.locks = lk_; nlog = 0; held_notes = 0; nlocks_applied = 0; }
/* run until the absolute tick reaches t, in 64-frame blocks; returns the frames run */
static uint32_t run_to(uint32_t t) { uint32_t f = 0; while (S.tick < t) { seq_advance(&S, 64); f += 64; } return f; }
/* the absolute tick of an on event: the block's start tick plus where in it */
static int ons(int trk, int note) { int n = 0; for (int i = 0; i < nlog; i++) if (log_[i].on && log_[i].trk == trk && (note < 0 || log_[i].note == note)) n++; return n; }

static void timing_test(void)
{
    printf("timing: a step on every beat, 120 BPM, for 8 bars\n");
    reset(120.0f);
    for (int st = 0; st < 16; st += 4) seq_step_toggle(&S, 0, st, 36, 100);
    seq_play(&S, 1);
    run_to(8 * SEQ_BAR);
    CHECK(ons(0, 36) == 32, "%d kicks for 32 beats", ons(0, 36));
    /* each kick in the block holding its tick: at most one block late (64 frames = 1.4 ms) */
    int worst = 0, k = 0;
    for (int i = 0; i < nlog; i++) if (log_[i].on) {
        int want = k++ * 4 * SEQ_TPS, d = (int)log_[i].tick - want; if (d < 0) d = -d; if (d > worst) worst = d;
    }
    float tpf = 120.0f * 4 * SEQ_TPS / 60.0f / RATE;
    printf("  worst offset %d ticks (a block is %.2f ticks)\n", worst, 64 * tpf);
    CHECK(worst <= (int)ceilf(64 * tpf), "a kick %d ticks off", worst);
    /* the tempo: 8 bars at 120 BPM = 16 s */
    reset(120.0f); seq_play(&S, 1);
    uint32_t f = run_to(8 * SEQ_BAR);
    CHECK(fabsf((float)f / RATE - 16.0f) < 0.005f, "8 bars took %.4f s", (float)f / RATE);
    seq_play(&S, 0);
    CHECK(held_notes == 0, "%d notes left on after STOP", held_notes);
}

static void polymeter_test(void)
{
    printf("polymeter: 16, 12 and 128 steps loop on their own\n");
    reset(140.0f);
    seq_step_toggle(&S, 0, 0, 36, 100);
    seq_step_toggle(&S, 1, 0, 60, 100); seq_set_len(&S, 1, 12);
    seq_step_toggle(&S, 2, 127, 72, 100); seq_set_len(&S, 2, 128);
    seq_play(&S, 1);
    run_to(48 * SEQ_TPS * 4);                                  /* 192 steps */
    CHECK(ons(0, -1) == 12, "16 steps: %d loops in 192 steps", ons(0, -1));
    CHECK(ons(1, -1) == 16, "12 steps: %d loops in 192 steps", ons(1, -1));
    CHECK(ons(2, -1) == 1, "128 steps: %d hits of step 128 in 192 steps", ons(2, -1));
    seq_play(&S, 0);
}

static void switch_test(void)
{
    printf("a pattern change waits for the bar; mutes stay through it\n");
    reset(120.0f);
    seq_step_toggle(&S, 0, 0, 36, 100);                        /* pattern 0: a kick on 1 */
    seq_queue(&S, 0, 1); seq_step_toggle(&S, 0, 8, 38, 100); seq_queue(&S, 0, 0);   /* pattern 1: a snare on 9 */
    seq_step_toggle(&S, 1, 0, 50, 100); seq_step_toggle(&S, 1, 4, 50, 100); seq_copy(&S, 1, 0, 1);
    seq_mute(&S, 1, 1);
    seq_play(&S, 1);
    run_to(SEQ_BAR + 5 * SEQ_TPS);                            /* into bar 2 */
    seq_queue(&S, -1, 1);                                      /* asked mid-bar */
    CHECK(S.t[0].cur == 0, "the change happened at once");
    run_to(2 * SEQ_BAR - 1);
    CHECK(S.t[0].cur == 0, "the change happened before the bar");
    run_to(2 * SEQ_BAR + 1);
    CHECK(S.t[0].cur == 1 && S.t[0].origin == 2 * SEQ_BAR, "pattern %d from tick %u", S.t[0].cur, S.t[0].origin);
    int n0 = ons(0, 38);
    run_to(3 * SEQ_BAR);
    CHECK(ons(0, 38) == n0 + 1, "the new pattern's snare did not play on its step 9");
    CHECK(S.t[1].mute == 1 && ons(1, -1) == 0, "the mute was lost (%d notes on a muted track)", ons(1, -1));
    seq_mute(&S, 1, 0); run_to(4 * SEQ_BAR);
    CHECK(ons(1, -1) > 0, "the unmuted track stayed silent");
    seq_play(&S, 0);
}

static void record_test(void)
{
    printf("live: written where it was heard, length on release, overdub, grid pull\n");
    reset(100.0f);
    seq_play(&S, 1);
    seq_record(&S, 1, 2);
    float tpf = 100.0f * 4 * SEQ_TPS / 60.0f / RATE;
    const uint32_t lat = 384;                                  /* ~8.7 ms behind */
    /* the player hits at heard ticks 1*96+37 (late 37/96 of a step) and 7*96-11 (early), each held 2 steps */
    uint32_t heard[2] = { 96 + 37, 7 * 96 - 11 };
    for (int h = 0; h < 2; h++) {
        uint32_t at = heard[h] + (uint32_t)(lat * tpf + 0.5f);      /* the tick being computed when it is heard */
        run_to(at); seq_key(&S, 64 + h, 90, 1, lat);
        run_to(at + 2 * SEQ_TPS); seq_key(&S, 64 + h, 0, 0, lat);
    }
    seq_record(&S, 0, 2);
    const seq_pat *p = &S.t[2].pat[0];
    CHECK(p->n == 2, "%d events written", p->n);
    int e0 = (int)p->ev[0].tick - (int)heard[0], e1 = (int)p->ev[1].tick - (int)heard[1];
    int bl = (int)ceilf(64 * tpf);
    printf("  written at %u and %u for %u and %u (within a block: %d ticks); lengths %u %u\n",
           p->ev[0].tick, p->ev[1].tick, heard[0], heard[1], bl, p->ev[0].len, p->ev[1].len);
    CHECK(abs(e0) <= bl && abs(e1) <= bl, "written %d and %d ticks off", e0, e1);
    CHECK(abs((int)p->ev[0].len - 2 * SEQ_TPS) <= bl, "length %u for 2 steps", p->ev[0].len);
    /* the grid pull: 0 % plays as played, 100 % on the grid, 50 % half way; the take is untouched */
    run_to(SEQ_BAR * 2); nlog = 0;
    S.t[2].quant = 100; run_to(SEQ_BAR * 3);
    int on100 = -1; for (int i = 0; i < nlog; i++) if (log_[i].on && log_[i].note == 64) on100 = (int)(log_[i].tick % SEQ_BAR);
    CHECK(abs(on100 - 96) <= bl, "100 %%: played at %d, the grid is 96", on100);
    CHECK(p->ev[0].tick == heard[0] + e0, "the grid pull changed the take");
    nlog = 0; S.t[2].quant = 0; run_to(SEQ_BAR * 4);
    int on0 = -1; for (int i = 0; i < nlog; i++) if (log_[i].on && log_[i].note == 64) on0 = (int)(log_[i].tick % SEQ_BAR);
    CHECK(abs(on0 - (int)p->ev[0].tick) <= bl, "0 %%: played at %d, taken at %u", on0, p->ev[0].tick);
    /* overdub: a second take adds */
    seq_record(&S, 1, 2);
    run_to(SEQ_BAR * 4 + 12 * SEQ_TPS); seq_key(&S, 70, 80, 1, 0); run_to(SEQ_BAR * 4 + 13 * SEQ_TPS); seq_key(&S, 70, 0, 0, 0);
    seq_record(&S, 0, 2);
    CHECK(p->n == 3 && p->ev[2].note == 70, "overdub: %d events", p->n);
    /* undo the overdub, then the first take; redo the first */
    CHECK(seq_undo(&S) && p->n == 2, "undo of the overdub: %d events", p->n);
    CHECK(seq_undo(&S) && p->n == 0, "undo of the first take: %d events", p->n);
    CHECK(seq_redo(&S) && p->n == 2, "redo: %d events", p->n);
    seq_play(&S, 0);
    CHECK(held_notes == 0, "%d notes left on", held_notes);
}

static void edit_test(void)
{
    printf("edits: steps, clear, erase a note, copy, length, 16 undos\n");
    reset(120.0f);
    for (int i = 0; i < 20; i++) seq_step_toggle(&S, 3, i % 16, 40 + i, 100);
    CHECK(seq_count(&S, 3) == 20, "%d steps written", seq_count(&S, 3));
    CHECK(seq_step_has(&S, 3, 0, 40) && seq_step_has(&S, 3, 0, 56), "step 1 lost a note");
    seq_step_toggle(&S, 3, 0, 40, 100);
    CHECK(!seq_step_has(&S, 3, 0, 40) && seq_step_has(&S, 3, 0, 56), "a second press did not erase only its note");
    seq_erase_note(&S, 3, 56); CHECK(!seq_step_has(&S, 3, 0, -1), "erase note failed");
    seq_copy(&S, 3, 0, 5); CHECK(S.t[3].pat[5].n == seq_count(&S, 3), "copy failed");
    seq_set_len(&S, 3, 200); CHECK(S.t[3].pat[0].steps == 128, "length %d over 128", S.t[3].pat[0].steps);
    seq_clear(&S, 3); CHECK(seq_count(&S, 3) == 0, "clear failed");
    int u = 0; while (seq_undo(&S)) u++;
    CHECK(u == SEQ_UNDO, "%d undos (the history holds %d)", u, SEQ_UNDO);
    int r = 0; while (seq_redo(&S)) r++;
    CHECK(r == SEQ_UNDO && seq_count(&S, 3) == 0, "%d redos, %d events at the end", r, seq_count(&S, 3));
    /* back and forth, then a new edit forgets the redo */
    reset(120.0f);
    seq_step_toggle(&S, 2, 0, 50, 100); seq_step_toggle(&S, 2, 4, 51, 100); seq_step_toggle(&S, 2, 8, 52, 100);
    seq_undo(&S); seq_undo(&S); seq_redo(&S); seq_undo(&S); seq_redo(&S);
    CHECK(seq_count(&S, 2) == 2 && seq_step_has(&S, 2, 4, 51), "back and forth: %d events", seq_count(&S, 2));
    seq_step_toggle(&S, 2, 12, 53, 100);
    CHECK(!seq_redo(&S) && seq_count(&S, 2) == 3 && !seq_step_has(&S, 2, 8, 52), "a new edit kept the redo");
    seq_undo(&S); seq_undo(&S);
    CHECK(seq_count(&S, 2) == 1 && seq_step_has(&S, 2, 0, 50), "undo after a new edit: %d events", seq_count(&S, 2));
}

static void step_test(void)
{
    printf("per step: conditions, fills, ratchets, locks\n");
    reset(120.0f);
    seq_step_toggle(&S, 0, 0, 10, 100); seq_step_cond(&S, 0, 0, SEQ_CYCLE + 2 * 8 + 0);   /* 1st of 2 loops */
    seq_step_toggle(&S, 0, 4, 11, 100); seq_step_cond(&S, 0, 4, SEQ_FILL);
    seq_step_toggle(&S, 0, 8, 12, 100); seq_step_cond(&S, 0, 8, SEQ_NOFILL);
    seq_step_toggle(&S, 0, 12, 13, 100); seq_step_ratchet(&S, 0, 12, 4);
    seq_step_toggle(&S, 1, 0, 20, 100); seq_step_cond(&S, 1, 0, SEQ_CHANCE + 5);           /* 50 % */
    seq_step_toggle(&S, 2, 4, 30, 100); seq_lock_set(&S, 2, 4, 6, 200);
    seq_play(&S, 1);
    run_to(8 * SEQ_BAR);
    CHECK(ons(0, 10) == 4, "1 in 2 loops: %d of 8", ons(0, 10));
    CHECK(ons(0, 11) == 0 && ons(0, 12) == 8, "without a fill: fill-only %d, no-fill %d", ons(0, 11), ons(0, 12));
    CHECK(ons(0, 13) == 32, "ratchet x4 over 8 bars: %d hits", ons(0, 13));
    int c = ons(1, 20);
    CHECK(c >= 1 && c <= 7, "50 %% chance: %d of 8", c);
    CHECK(nlocks_applied == 16 && last_lock_n == 0, "locks applied %d times (16: on and off each bar)", nlocks_applied);
    seq_fill(&S, 1); run_to(9 * SEQ_BAR); seq_fill(&S, 0);
    CHECK(ons(0, 11) == 1 && ons(0, 12) == 8, "in a fill: fill-only %d, no-fill %d", ons(0, 11), ons(0, 12));
    seq_fill_next_bar(&S); run_to(10 * SEQ_BAR);
    CHECK(ons(0, 11) == 1, "the fill came early");
    run_to(11 * SEQ_BAR);
    CHECK(ons(0, 11) == 2, "the next-bar fill did not play");
    run_to(12 * SEQ_BAR);
    CHECK(ons(0, 11) == 2, "the next-bar fill lasted");
    int cond, rat;
    CHECK(seq_step_info(&S, 2, 4, &cond, &rat) == 1 && seq_lock_get(&S, 2, 4, 6) == 200, "the lock is not there");
    seq_step_reset(&S, 2, 4);
    CHECK(seq_step_info(&S, 2, 4, &cond, &rat) == 0, "reset kept a lock");
    seq_play(&S, 0);
    CHECK(held_notes == 0, "%d notes left on", held_notes);
}

static void song_test(void)
{
    printf("song: a jam recorded (pattern changes, mutes), played back\n");
    reset(120.0f);
    for (int p = 0; p < 3; p++) { seq_queue(&S, 0, p); seq_step_toggle(&S, 0, 0, 40 + p, 100); }
    seq_queue(&S, 0, 0); seq_step_toggle(&S, 1, 0, 50, 100); seq_copy(&S, 1, 0, 1);
    seq_play(&S, 1);
    seq_song_record(&S, 1);                                      /* from bar 1 */
    run_to(2 * SEQ_BAR + 10); seq_queue(&S, -1, 1);              /* at bar 3: pattern 2 */
    run_to(4 * SEQ_BAR + 10); seq_mute(&S, 1, 1);                /* at bar 5: track 2 muted */
    run_to(5 * SEQ_BAR + 10); seq_queue(&S, 0, 2);               /* at bar 6: track 1 alone to pattern 3 */
    run_to(7 * SEQ_BAR + 10); seq_song_record(&S, 0);             /* the song is bars 1..7 */
    CHECK(S.song_len == 7 && S.song_n == 4, "song of %d bars, %d entries", S.song_len, S.song_n);
    /* play it from the start: the same notes, bar by bar */
    seq_song_play(&S, 1); seq_play(&S, 0); seq_mute(&S, 1, 0); seq_queue(&S, -1, 0);
    seq_play(&S, 1); nlog = 0;
    int want[7] = { 40, 40, 41, 41, 41, 42, 42 }, ok = 1;
    for (int bar = 0; bar < 14; bar++) {
        int before = nlog;
        run_to((uint32_t)(bar + 1) * SEQ_BAR);
        int got = -1, track2 = 0;
        for (int i = before; i < nlog; i++) if (log_[i].on) { if (log_[i].trk == 0) got = log_[i].note; if (log_[i].trk == 1) track2 = 1; }
        int w = want[bar % 7], mute2 = bar % 7 >= 4;
        if (got != w || track2 == mute2) { ok = 0; printf("  bar %d: track 1 played %d (want %d), track 2 %s\n", bar + 1, got, w, track2 ? "on" : "off"); }
    }
    CHECK(ok, "the song did not play back as recorded (twice through)");
    /* editing while the song plays: allowed, heard at once */
    run_to(14 * SEQ_BAR + 10);                                   /* song bar 1 again: pattern 1 */
    seq_step_toggle(&S, 0, 8, 45, 100);
    nlog = 0; run_to(15 * SEQ_BAR);
    CHECK(ons(0, 45) == 1, "an edit during the song was not heard");
    seq_play(&S, 0);
}

static void song_hand_test(void)
{
    printf("song by hand: entries, their patterns and bars, as SLOOP's SONG screen\n");
    reset(120.0f);
    for (int p = 0; p < 3; p++) { seq_queue(&S, 0, p); seq_step_toggle(&S, 0, 0, 40 + p, 100); }
    seq_queue(&S, 0, 0);
    seq_song_set_count(&S, 3);                                   /* 3 entries of 4 bars, what plays now */
    CHECK(S.song_n == 3 && S.song_len == 12 && S.song[2].bar == 8, "3 entries: %d, %d bars", S.song_n, S.song_len);
    seq_song_set_pat(&S, 1, -1, 1); seq_song_set_pat(&S, 2, 0, 2);
    CHECK(S.song[1].pat[0] == 1 && S.song[1].pat[1] == 1 && S.song[2].pat[0] == 2 && S.song[2].pat[1] == 0, "patterns set");
    seq_song_set_len(&S, 0, 2); seq_song_set_len(&S, 1, 1);      /* 2 + 1 + 4 bars */
    CHECK(S.song_len == 7 && S.song[1].bar == 2 && S.song[2].bar == 3 && seq_song_len_of(&S, 2) == 4, "lengths: %d bars", S.song_len);
    CHECK(!seq_song_set_len(&S, 1, 1) && seq_song_set_len(&S, 1, 99) && seq_song_len_of(&S, 1) == SEQ_SONG_BARS, "clamped");
    seq_song_set_len(&S, 1, 1);
    seq_song_play(&S, 1); seq_play(&S, 1); nlog = 0;
    int want[7] = { 40, 40, 41, 42, 42, 42, 42 }, ok = 1;
    for (int bar = 0; bar < 7; bar++) {
        int before = nlog; run_to((uint32_t)(bar + 1) * SEQ_BAR);
        int got = -1; for (int i = before; i < nlog; i++) if (log_[i].on && log_[i].trk == 0) got = log_[i].note;
        int at = seq_song_at(&S);
        if (got != want[bar]) { ok = 0; printf("  bar %d: %d (want %d), entry %d\n", bar + 1, got, want[bar], at); }
    }
    CHECK(ok, "the song made by hand did not play as set");
    run_to(7 * SEQ_BAR + 10); CHECK(seq_song_at(&S) == 0, "entry %d at the loop", seq_song_at(&S));
    seq_play(&S, 0);
    seq_song_set_count(&S, 2); CHECK(S.song_n == 2 && S.song_len == 3, "2 entries: %d bars", S.song_len);
    seq_song_set_count(&S, 0); CHECK(!S.song_n && !S.song_len && !S.song_play, "no song");
}

static void move_test(void)
{
    printf("NOTES: MOVE and COPY a note (its locks with it), undone\n");
    reset(120.0f);
    uint8_t n = 50; seq_step_write(&S, 0, 2, &n, 1, 100);
    seq_lock_set(&S, 0, 2, 5, 200);
    seq_pat *p = &S.t[0].pat[S.t[0].cur];
    int at = seq_ev_move(&S, 0, 0, 1, 0);
    CHECK(at == 0 && p->ev[0].tick == 3 * SEQ_TPS && seq_lock_get(&S, 0, 3, 5) == 200 && seq_lock_get(&S, 0, 2, 5) < 0, "moved with its lock");
    at = seq_ev_move(&S, 0, 0, 0, 1);
    CHECK(at == 1 && p->n == 2 && p->ev[1].tick == 4 * SEQ_TPS && seq_lock_get(&S, 0, 4, 5) == 200 && seq_lock_get(&S, 0, 3, 5) == 200, "copied after itself");
    CHECK(seq_ev_move(&S, 0, 0, -4, 0) < 0, "not before the pattern");
    seq_set_len(&S, 0, 5);
    CHECK(seq_ev_move(&S, 0, 1, 1, 0) < 0, "not past its end");
    seq_undo(&S); seq_undo(&S);
    CHECK(p->n == 1 && p->ev[0].tick == 3 * SEQ_TPS, "copy undone: %d", p->n);
    seq_undo(&S);
    CHECK(p->ev[0].tick == 2 * SEQ_TPS && seq_lock_get(&S, 0, 2, 5) == 200, "move undone");
}

static void lenmax_test(void)
{
    printf("NOTES LEN as SLOOP: POLY on under later notes (4 at most), MONO to the next note, the pattern's end\n");
    reset(120.0f);
    uint8_t c = 36; seq_step_write(&S, 0, 0, &c, 1, 100);
    uint8_t ch[3] = { 60, 64, 67 }; seq_step_write(&S, 0, 4, ch, 3, 100);
    uint8_t one = 72; seq_step_write(&S, 0, 8, &one, 1, 100);
    int why = -1;
    int m = seq_ev_len_max(&S, 0, 0, 0, &why);
    CHECK(m == 4 * SEQ_TPS && why == 0, "MONO: to the next note (%d, %d)", m, why);
    m = seq_ev_len_max(&S, 0, 0, 1, &why);
    CHECK(m == 16 * SEQ_TPS && why == 3, "POLY: over a chord of 3 (%d, %d)", m, why);
    seq_pat *p = &S.t[0].pat[S.t[0].cur];
    for (int i = 1; i < 4; i++) p->ev[i].len = 6 * SEQ_TPS;           /* the chord on to step 10: with step 8's note, 4 */
    m = seq_ev_len_max(&S, 0, 0, 1, &why);
    CHECK(m == 8 * SEQ_TPS && why == 1, "POLY: 4 notes at step 9 (%d, %d)", m, why);
}

static void put_test(void)
{
    printf("a pattern rewritten from the editor: its notes, its length, its locks kept, one undo\n");
    reset(120.0f);
    uint8_t c = 40; seq_step_write(&S, 0, 0, &c, 1, 100); seq_lock_set(&S, 0, 3, 5, 77);
    uint8_t nt[12] = { 0, 0, 96, 0, 60, 90,  192, 0, 192, 0, 64, 110 };
    int n = seq_pat_put(&S, 0, 0, 0, 32, nt, 1); n = seq_pat_put(&S, 0, 0, 1, 32, nt + 6, 1);
    const seq_pat *p = &S.t[0].pat[0];
    CHECK(n == 2 && p->steps == 32 && p->ev[0].note == 60 && p->ev[1].tick == 192 && p->ev[1].len == 192 && seq_lock_get(&S, 0, 3, 5) == 77, "rewritten (%d)", n);
    seq_undo(&S);
    CHECK(p->n == 1 && p->ev[0].note == 40 && p->steps == 16, "undone (%d %d)", p->n, p->steps);
}

static void sloop_test(void)
{
    printf("SLOOP 3: a step's note, velocity, nudge; swing; one undo a gesture; the nearest step\n");
    reset(120.0f);
    seq_step_toggle(&S, 1, 4, 48, 100);
    seq_step_note(&S, 1, 4, 3); seq_step_vel(&S, 1, 4, 60);
    int n = 0, v = 0, d = 99;
    CHECK(seq_step_get(&S, 1, 4, &n, &v, &d) && n == 51 && v == 60 && d == 0, "note %d vel %d nudge %d", n, v, d);
    seq_step_nudge(&S, 1, 4, -20);
    CHECK(seq_step_get(&S, 1, 4, 0, 0, &d) && d == -20 && seq_step_has(&S, 1, 4, 51) && !seq_step_has(&S, 1, 3, -1), "nudged early: %d, still step 5", d);
    seq_step_nudge(&S, 1, 0, -10);                              /* an empty step: nothing */
    seq_step_toggle(&S, 1, 0, 40, 100); seq_step_nudge(&S, 1, 0, -10);
    CHECK(seq_step_get(&S, 1, 0, 0, 0, &d) && d == -10 && S.t[1].pat[0].ev[S.t[1].pat[0].n - 1].tick == 16 * SEQ_TPS - 10, "step 1 early wraps to the end: %d", d);
    /* played where it was nudged to */
    seq_play(&S, 1); nlog = 0; run_to(SEQ_BAR + 1);
    int at = -1; for (int i = 0; i < nlog; i++) if (log_[i].on && log_[i].note == 51) { at = (int)log_[i].tick; break; }
    CHECK(at >= 4 * SEQ_TPS - 20 && at < 4 * SEQ_TPS - 20 + 8, "a nudged step played at %d", at);
    /* swing: the second step of each pair late by half a step at 100 */
    reset(120.0f);
    for (int st = 0; st < 4; st++) seq_step_toggle(&S, 2, st, 60, 100);
    seq_set_swing(&S, -1, 100); seq_play(&S, 1); nlog = 0; run_to(SEQ_BAR);
    int t1 = -1, t2 = -1; for (int i = 0, k = 0; i < nlog; i++) if (log_[i].on) { if (k == 1) t1 = (int)log_[i].tick; if (k == 2) t2 = (int)log_[i].tick; k++; }
    CHECK(t1 >= SEQ_TPS + SEQ_TPS / 2 && t1 < SEQ_TPS + SEQ_TPS / 2 + 8 && t2 >= 2 * SEQ_TPS && t2 < 2 * SEQ_TPS + 8, "swing 100: steps at %d, %d", t1, t2);
    seq_set_swing(&S, -1, 30); seq_set_swing(&S, 2, 90);
    CHECK(S.swing == 30 && S.t[2].swing == 90, "swing kept");
    /* a gesture: several edits, one undo */
    reset(120.0f);
    seq_step_toggle(&S, 0, 2, 50, 100);
    S.batch = 1; S.batch_done = 0;
    seq_step_ratchet(&S, 0, 2, 3); seq_step_vel(&S, 0, 2, 40); seq_step_note(&S, 0, 2, 2);
    S.batch = 0;
    CHECK(H.n_undo == 2, "%d undos for a step and a gesture", H.n_undo);
    seq_undo(&S);
    CHECK(seq_step_get(&S, 0, 2, &n, &v, 0) && n == 50 && v == 100, "the gesture undone at once: %d %d", n, v);
    /* a note played a little early belongs to the step it was played for */
    reset(120.0f);
    seq_ev e = { (uint16_t)(8 * SEQ_TPS - 6), 40, 62, 90, SEQ_ALWAYS, 1 };
    S.t[3].pat[0].ev[0] = e; S.t[3].pat[0].n = 1;
    CHECK(seq_step_has(&S, 3, 8, 62) && !seq_step_has(&S, 3, 7, -1), "an early note is its step's");
    /* EDIT's knobs: shift, x2 / half, transpose */
    reset(120.0f);
    seq_step_toggle(&S, 1, 0, 40, 100); seq_step_toggle(&S, 1, 15, 47, 100); seq_lock_set(&S, 1, 15, 6, 10);
    seq_shift(&S, 1, 1);
    CHECK(seq_step_has(&S, 1, 1, 40) && seq_step_has(&S, 1, 0, 47) && seq_lock_get(&S, 1, 0, 6) == 10, "shift right wraps the last step to the first");
    seq_shift(&S, 1, -1);
    CHECK(seq_step_has(&S, 1, 0, 40) && seq_step_has(&S, 1, 15, 47), "shift left back");
    seq_resize(&S, 1, 1);
    CHECK(S.t[1].pat[0].steps == 32 && seq_step_has(&S, 1, 16, 40) && seq_step_has(&S, 1, 31, 47) && seq_count(&S, 1) == 4, "x2: %d steps, %d events", S.t[1].pat[0].steps, seq_count(&S, 1));
    seq_resize(&S, 1, -1);
    CHECK(S.t[1].pat[0].steps == 16, "half");
    /* 3.1.8: twice as long again: the second half as it was (not the first half's copy over it), the notes in order */
    seq_resize(&S, 1, 1);
    CHECK(S.t[1].pat[0].steps == 32 && seq_count(&S, 1) == 4, "half then x2: %d events, not 4", seq_count(&S, 1));
    seq_resize(&S, 1, -1); seq_resize(&S, 1, -1);                      /* 8 steps: three quarters past the end */
    seq_resize(&S, 1, 1); seq_resize(&S, 1, 1);
    { const seq_pat *p = &S.t[1].pat[0]; int ok = 1; for (int i = 1; i < p->n; i++) if (p->ev[i - 1].tick > p->ev[i].tick) ok = 0;
      CHECK(ok && p->steps == 32 && p->n == 4, "quarter then x2 x2: %d events (4), in order: %d", p->n, ok); }
    seq_resize(&S, 1, -1);
    seq_transpose(&S, 1, 2);
    CHECK(seq_step_has(&S, 1, 0, 42) && seq_step_has(&S, 1, 15, 49), "transpose");
}


static void add_test(void)
{
    printf("seq_add: a free take's notes, as played\n");
    reset(120.0f);
    seq_set_len(&S, 2, 32);
    CHECK(seq_add(&S, 2, 500, 200, 62, 90) && seq_add(&S, 2, 100, 50, 60, 80) && seq_add(&S, 2, 32 * SEQ_TPS + 7, 10, 64, 200), "add refused");
    const seq_pat *p = &S.t[2].pat[0];
    CHECK(p->n == 3 && p->ev[0].tick == 7 && p->ev[0].vel == 127 && p->ev[0].len == SEQ_TPS / 8 && p->ev[1].tick == 100 && p->ev[2].tick == 500 && p->ev[2].len == 200,
          "kept sorted, wrapped, bounded");
    CHECK(seq_undo(&S) && S.t[2].pat[0].n == 2, "one undo an event");
    CHECK(!seq_add(&S, SEQ_NTRK, 0, 10, 60, 100), "a track out of range");
}

static void div_test(void)
{
    printf("DIV and GATE: a track on its own clock\n");
    reset(120.0f);
    for (int st = 0; st < 16; st++) { seq_step_toggle(&S, 0, st, 36, 100); seq_step_toggle(&S, 1, st, 48, 100); seq_step_toggle(&S, 2, st, 60, 100); }
    seq_set_div(&S, 1, SEQ_DIV_8); seq_set_div(&S, 2, SEQ_DIV_32);
    seq_play(&S, 1); run_to(4 * SEQ_BAR);
    CHECK(ons(0, 36) == 64 && ons(1, 48) == 32 && ons(2, 60) == 128, "1/16 %d, 1/8 %d, 1/32 %d in 4 bars", ons(0, 36), ons(1, 48), ons(2, 60));
    /* a triplet track: 3 steps where 1/16 has 2 */
    reset(120.0f);
    for (int st = 0; st < 16; st++) seq_step_toggle(&S, 0, st, 36, 100);
    seq_set_div(&S, 0, SEQ_DIV_16T); seq_play(&S, 1); run_to(4 * SEQ_BAR);
    CHECK(ons(0, 36) == 96, "16T: %d in 4 bars (want 96)", ons(0, 36));
    /* changed while playing: the step goes on from where it was, nothing doubled or lost */
    reset(120.0f);
    for (int st = 0; st < 16; st++) seq_step_toggle(&S, 0, st, 36, 100);
    seq_play(&S, 1); run_to(SEQ_BAR + SEQ_BAR / 2);
    uint32_t at = seq_step_of(&S, 0);
    seq_set_div(&S, 0, SEQ_DIV_8);
    CHECK(seq_step_of(&S, 0) == at, "the step moved: %u -> %u", at, seq_step_of(&S, 0));
    int before = ons(0, 36); run_to(SEQ_BAR * 2 + SEQ_BAR / 2);
    CHECK(ons(0, 36) - before == 8, "a bar at 1/8 after the change: %d notes", ons(0, 36) - before);
    seq_play(&S, 0);
    CHECK(held_notes == 0, "%d notes left on", held_notes);
    /* GATE 50: the notes last half as long */
    reset(120.0f); seq_step_toggle(&S, 0, 0, 36, 100); seq_set_gate(&S, 0, 50);
    seq_play(&S, 1); run_to(SEQ_BAR / 2);
    uint32_t t_on = 0, t_off = 0;
    for (int i = 0; i < nlog; i++) { if (log_[i].on && !t_on) t_on = log_[i].tick + 1; else if (!log_[i].on && t_on && !t_off) t_off = log_[i].tick; }
    CHECK(t_off && t_off - (t_on - 1) <= SEQ_TPS * 3 / 8 + 20 && t_off - (t_on - 1) >= SEQ_TPS * 3 / 8 - 20, "GATE 50: a note of %u ticks (want ~%d)", t_off - (t_on - 1), SEQ_TPS * 3 / 8);
}

static void accent_slide_test(void)
{
    printf("accent and slide: the flags reach note_on, a slide lasts to the next note, the NOTES page's edits\n");
    reset(120.0f);
    seq_step_toggle(&S, 0, 0, 40, 100); seq_step_toggle(&S, 0, 4, 43, 100); seq_step_toggle(&S, 0, 8, 47, 100);
    seq_step_flag(&S, 0, 0, SEQ_SLIDE, 1); seq_step_flag(&S, 0, 8, SEQ_ACCENT, 1);
    CHECK(seq_step_flags(&S, 0, 0) == SEQ_SLIDE && seq_step_flags(&S, 0, 8) == SEQ_ACCENT && !seq_step_flags(&S, 0, 4), "the flags kept");
    seq_step_ratchet(&S, 0, 0, 3); int r; seq_step_info(&S, 0, 0, 0, &r);
    CHECK(r == 3 && seq_step_flags(&S, 0, 0) == SEQ_SLIDE, "a ratchet keeps the slide (%d)", r);
    seq_step_ratchet(&S, 0, 0, 1);
    seq_play(&S, 1); run_to(SEQ_BAR);
    uint32_t off40 = 0, on43 = 0; int v43 = 0, v47 = 0, v40 = 0;
    for (int i = 0; i < nlog; i++) {
        if (log_[i].on && log_[i].note == 40 && !v40) v40 = log_[i].vel;
        if (log_[i].on && log_[i].note == 43 && !on43) { on43 = log_[i].tick; v43 = log_[i].vel; }
        if (log_[i].on && log_[i].note == 47 && !v47) v47 = log_[i].vel;
        if (!log_[i].on && log_[i].note == 40 && !off40) off40 = log_[i].tick;
    }
    CHECK(off40 > on43, "the slid note ends after the next starts (%u, %u)", off40, on43);
    CHECK((v43 & SEQ_ON_SLID) && !(v40 & SEQ_ON_SLID), "the note after the slide is told (%x)", v43);
    CHECK((v47 & SEQ_ON_ACC) && (v47 & 127) == 100 && !(v43 & SEQ_ON_ACC), "the accent reaches note_on (%x)", v47);
    seq_play(&S, 0);
    /* the NOTES page: a step written as a chord, an event's length and force, one removed */
    const uint8_t ch[3] = { 60, 64, 67 };
    int at = seq_step_write(&S, 0, 4, ch, 3, 90);
    CHECK(at >= 0 && S.t[0].pat[0].ev[at].note == 60 && seq_step_has(&S, 0, 4, 64) && !seq_step_has(&S, 0, 4, 43), "a chord written over the step");
    seq_ev_set(&S, 0, at, -1, 3 * SEQ_TPS, 127, SEQ_ACCENT);
    CHECK(S.t[0].pat[0].ev[at].len == 3 * SEQ_TPS && S.t[0].pat[0].ev[at].vel == 127 && (S.t[0].pat[0].ev[at].ratchet & SEQ_ACCENT), "an event's length and force");
    int n0 = S.t[0].pat[0].n; seq_ev_del(&S, 0, at);
    CHECK(S.t[0].pat[0].n == n0 - 1 && !seq_step_has(&S, 0, 4, 60) && seq_step_has(&S, 0, 4, 64), "one note of the chord removed");
    seq_undo(&S); CHECK(seq_step_has(&S, 0, 4, 60), "and back with undo");
}
/* (3.3.7) a tester's report: "chords just stack with time going", "offset between two tracks with the same steps" */
static int pos_of(int t) { return (int)seq_step_of(&S, t); }
static void loop_stack_test(void)
{
    printf("3.3.7: a chord played again on each loop is written once; the patterns' changes keep the tracks together\n");
    reset(120.0f);
    seq_play(&S, 1); seq_record(&S, 1, 1);
    static const int CH[3] = { 60, 64, 67 };
    for (int loop = 0; loop < 4; loop++) {                 /* the same chord on steps 0 and 8, four times round, a little off */
        for (int h = 0; h < 2; h++) {
            uint32_t at = (uint32_t)loop * SEQ_BAR + (uint32_t)h * 8 * SEQ_TPS + (uint32_t)(loop * 7 % 23);
            run_to(at); for (int i = 0; i < 3; i++) seq_key(&S, CH[i] + h * 2, 90, 1, 0);
            run_to(at + 3 * SEQ_TPS); for (int i = 0; i < 3; i++) seq_key(&S, CH[i] + h * 2, 0, 0, 0);
        }
    }
    run_to(4 * SEQ_BAR + 12 * SEQ_TPS); seq_key(&S, 72, 90, 1, 0); run_to(4 * SEQ_BAR + 13 * SEQ_TPS); seq_key(&S, 72, 0, 0, 0);
    seq_record(&S, 0, 1);
    CHECK(S.t[1].pat[0].n == 7, "the chords played 4 times: %d events (6, and the new note: 7)", S.t[1].pat[0].n);
    seq_play(&S, 0);

    /* all tracks to a pattern on an odd bar: the one already on it starts with the others */
    reset(120.0f);
    for (int t = 0; t < 2; t++) for (int p = 0; p < 2; p++) S.t[t].pat[p].steps = 32;
    S.t[1].cur = 1;
    seq_play(&S, 1);
    run_to(2 * SEQ_BAR + 5 * SEQ_TPS);
    seq_queue(&S, -1, 1);
    run_to(3 * SEQ_BAR + 3 * SEQ_TPS);
    CHECK(S.t[0].cur == 1 && S.t[1].cur == 1, "both on pattern 2 (%d %d)", S.t[0].cur + 1, S.t[1].cur + 1);
    CHECK(pos_of(0) == pos_of(1) && pos_of(0) == 3, "32 steps each: steps %d and %d (3 and 3)", pos_of(0), pos_of(1));
    /* every track already on it: nothing restarts */
    seq_queue(&S, -1, 1);
    CHECK(S.t[0].next == SEQ_NONE && S.t[1].next == SEQ_NONE, "a pattern every track is on: no change");
    seq_play(&S, 0);

    /* a chain (the current pattern's end): tracks of 16 and 12 steps change at the lead's end, together */
    reset(120.0f);
    S.t[1].pat[0].steps = 12; S.t[1].pat[1].steps = 12;
    seq_play(&S, 1); S.switch_now = 1; S.lead = 0;
    run_to(3 * SEQ_TPS);
    seq_queue(&S, -1, 1);
    run_to(SEQ_BAR - 1);
    CHECK(S.t[0].cur == 0 && S.t[1].cur == 0, "nothing changed before the lead's end (%d %d)", S.t[0].cur, S.t[1].cur);
    run_to(SEQ_BAR + 2 * SEQ_TPS);
    CHECK(S.t[0].cur == 1 && S.t[1].cur == 1 && S.t[0].origin == S.t[1].origin, "together at the lead's end (origins %u %u)", S.t[0].origin, S.t[1].origin);
    CHECK(pos_of(0) == 2 && pos_of(1) == 2, "steps %d and %d (2 and 2)", pos_of(0), pos_of(1));
    seq_play(&S, 0);
}
/* one track's undo and redo (SEL + OCT), the other tracks' edits passed over */
static int cnt(int t) { return seq_count(&S, t); }
static void undo_trk_test(void)
{
    printf("undo of one track: A1 B1 A2, then track 1 undone twice and redone twice; track 2 never touched\n");
    seq_init(&S, 44100.0f, 120.0f, &H);
    seq_add(&S, 0, 0, 96, 60, 100);                       /* A1 */
    seq_add(&S, 1, 100, 96, 72, 100);                     /* B1 */
    seq_add(&S, 0, 400, 96, 64, 100);                     /* A2 */
    CHECK(H.n_undo == 3 && cnt(0) == 2 && cnt(1) == 1, "setup: %d undo, %d / %d notes", H.n_undo, cnt(0), cnt(1));
    CHECK(seq_undo_trk(&S, 0) && cnt(0) == 1 && cnt(1) == 1, "undo A2: %d / %d notes (want 1 / 1)", cnt(0), cnt(1));
    CHECK(seq_undo_trk(&S, 0) && cnt(0) == 0 && cnt(1) == 1, "undo A1: %d / %d notes (want 0 / 1)", cnt(0), cnt(1));
    CHECK(H.n_undo == 1 && H.n_redo == 2 && H.h[0].trk == 1, "the history after: %d undo, %d redo, the one left is track %d's", H.n_undo, H.n_redo, H.h[0].trk + 1);
    CHECK(!seq_undo_trk(&S, 0) && cnt(1) == 1, "a third undo of track 1 did something");
    CHECK(seq_redo_trk(&S, 0) && cnt(0) == 1 && S.t[0].pat[0].ev[0].note == 60, "redo A1: %d notes", cnt(0));
    CHECK(seq_redo_trk(&S, 0) && cnt(0) == 2 && S.t[0].pat[0].ev[1].note == 64 && S.t[0].pat[0].ev[1].tick == 400, "redo A2: %d notes", cnt(0));
    CHECK(!seq_redo_trk(&S, 0) && cnt(1) == 1 && H.n_undo == 3 && H.n_redo == 0, "after the redos: %d undo, %d redo, track 2 %d notes", H.n_undo, H.n_redo, cnt(1));
    /* the undo of everything still walks back to the start, after the two kinds were mixed */
    seq_undo_trk(&S, 1);                                  /* B1 undone alone */
    CHECK(cnt(0) == 2 && cnt(1) == 0, "undo of track 2 alone: %d / %d notes (want 2 / 0)", cnt(0), cnt(1));
    CHECK(seq_undo(&S) && cnt(0) == 1, "the undo of all, after: track 1 has %d notes (want 1)", cnt(0));
    CHECK(seq_redo(&S) && cnt(0) == 2 && cnt(1) == 0, "the redo of all gives the last undone back: %d / %d", cnt(0), cnt(1));
    CHECK(seq_redo_trk(&S, 1) && cnt(1) == 1, "track 2's redo, across track 1's: %d notes", cnt(1));
    while (seq_undo(&S)) { }
    CHECK(cnt(0) == 0 && cnt(1) == 0 && H.n_undo == 0, "the undo of all did not reach the start (%d / %d notes)", cnt(0), cnt(1));
    /* an edit anywhere forgets every redo (SLOOP's rule) */
    seq_init(&S, 44100.0f, 120.0f, &H);
    seq_add(&S, 0, 0, 96, 60, 100); seq_undo_trk(&S, 0); seq_add(&S, 1, 0, 96, 72, 100);
    CHECK(!seq_redo_trk(&S, 0) && cnt(0) == 0, "track 1's redo outlived an edit of track 2");
    /* many mixes against a model: each track's own states in a row, its place in them */
    uint32_t r = 12345u; int bad = 0;
    for (int round = 0; round < 300; round++) {
        seq_init(&S, 44100.0f, 120.0f, &H);
        int n[3] = { 1, 1, 1 }, at[3] = { 0, 0, 0 }, st[3][10] = { { 0 }, { 0 }, { 0 } }, edits = 0;
        for (int k = 0; k < 24; k++) {
            r = r * 1664525u + 1013904223u; int t = (int)(r >> 24) % 3, op = (int)(r >> 12) % 3;
            if (op == 0 && edits < 8) {                   /* an edit: one more note (8: what the history holds) */
                seq_add(&S, t, (uint32_t)(cnt(t) * 50 + 7), 40, 60, 100); edits++;
                for (int u = 0; u < 3; u++) n[u] = at[u] + 1;
                st[t][++at[t]] = cnt(t); n[t] = at[t] + 1;
            } else if (op == 1) { int did = seq_undo_trk(&S, t); if (did != (at[t] > 0)) bad++; if (did) at[t]--; }
            else if (op == 2) { int did = seq_redo_trk(&S, t); if (did != (at[t] < n[t] - 1)) bad++; if (did) at[t]++; }
            for (int u = 0; u < 3; u++) if (cnt(u) != st[u][at[u]]) bad++;
            if (H.n_undo + H.n_redo > SEQ_UNDO) bad++;
        }
        while (seq_undo(&S)) { }
        for (int u = 0; u < 3; u++) if (cnt(u)) bad++;
    }
    CHECK(!bad, "300 mixes of edits, undos and redos of three tracks: %d differences from the model", bad);
}
int main(void)
{
    add_test(); div_test();
    timing_test(); polymeter_test(); switch_test(); record_test(); edit_test(); step_test(); song_test(); song_hand_test(); move_test(); lenmax_test(); put_test(); sloop_test(); accent_slide_test(); loop_stack_test(); undo_trk_test();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
