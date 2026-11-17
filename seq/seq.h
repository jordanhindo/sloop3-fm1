/* SPDX-License-Identifier: GPL-3.0-only
 * The sequencer (seq.c): 5 tracks (SLOOP 3's; a project file keeps room for 8), 16 patterns each of 1 to 128 steps, events kept to the tick (96 ticks a step:
 * 1/384 of a whole note), so a live take keeps its timing exactly; the grid pull (quantize) is applied when
 * playing, not written: 0 % plays it as played, 100 % on the grid, and it can be changed afterwards.
 *   - each track loops on its own length (polymeter);
 *   - a pattern change waits for the next bar (or happens now): all tracks, or one track alone;
 *   - mutes belong to the performance, not to the patterns: a pattern change keeps them;
 *   - undo / redo of every edit (a take, a step, a clear, a length), 8 deep;
 *   - live recording: a note is written where it was heard (the output's delay taken off), its length when the
 *     key is released; recording over a loop adds to it (overdub), a note played again where it already is
 *     takes its place (3.3.7)
 * 1.0 adds, per step: parameter locks (a sound parameter another value for that step only), a condition (only in
 * a fill, never in a fill, a chance, or 1 in 2 / 3 / 4 loops), ratchets (2 to 4 hits in the step); fills (now, or
 * for the next bar); and the song: a live jam recorded as the bars where patterns and mutes changed, played back
 * while every pattern stays editable. SLOOP 3 adds, per step, its note, velocity and nudge, and swing (global and per
 * track). A step holds the events nearest to it (a note played a little early is its step's).
 * No hardware here: the host tests run it (tests/seq_test.c). The sound is reached through callbacks. */
#pragma once
#include <stdint.h>

#define SEQ_NTRK 5                       /* (RAM: each track is 18 KB of patterns) */
#define SEQ_NPAT 8                       /* SAVE + key 1-8 (3.0B9.1: 12 -> 8, the room for SLOOP's sounds; a file may hold 16:
                                            the others passed over) */
#define SEQ_MAXSTEPS 128
#define SEQ_TPS 96                       /* ticks a step (a step = a 1/16 note) */
#define SEQ_BAR (16 * SEQ_TPS)           /* ticks a bar (4/4) */
#define SEQ_PATEV 128                    /* events a pattern */
#define SEQ_UNDO 8                        /* (8 since 3.0B9: the room for the NOTES page) */
#define SEQ_HELD 10                      /* keys held at once while recording */
#define SEQ_LOCKS 32                     /* parameter locks a pattern */
#define SEQ_SONG 96                      /* song entries */

/* a step's condition (seq_ev.cond) */
enum { SEQ_ALWAYS = 0, SEQ_FILL = 1, SEQ_NOFILL = 2,
       SEQ_CHANCE = 0x40,                /* 0x40 + n: n x 10 % (n 1..9) */
       SEQ_CYCLE = 0x80 };               /* 0x80 + B * 8 + (A - 1): the A-th of every B loops (B 2..4) */
typedef struct { uint16_t tick, len; uint8_t note, vel, cond, ratchet; } seq_ev;   /* tick in the pattern; len in ticks */
/* the ratchet byte: the hits (1..4) in its low bits, the step's ACCENT and SLIDE above (3.0B9) */
#define SEQ_RAT(r) ((r) & 7 ? (r) & 7 : 1)
#define SEQ_SLIDE 0x10                   /* the note goes on into the next one (a MONO / LEG sound glides there) */
#define SEQ_ACCENT 0x20                  /* played hard: velocity 127, and louder still */
/* what note_on's vel carries above its 7 bits: the accent; the note a slide leads into (to glide) */
#define SEQ_ON_ACC 0x100
#define SEQ_ON_SLID 0x200
typedef struct { uint8_t step, param, value; } seq_lock;                      /* value 0..255 = 0..1 */
/* a lock's param: the sound's values (engine.h SP_*, below 48), then the track's mix (as SLOOP's lockable LEVEL, PAN,
 * CHO DLY REV sends, the track's filter: 0..255 over 0..1, the filter -1..1) */
enum { SEQ_LK_LEVEL = 48, SEQ_LK_PAN, SEQ_LK_CHO, SEQ_LK_DLY, SEQ_LK_REV, SEQ_LK_FILT, SEQ_LK_END };
typedef struct { uint16_t n, steps; uint8_t nlock, pad; seq_ev ev[SEQ_PATEV]; seq_lock lk[SEQ_LOCKS]; } seq_pat;   /* events sorted by tick */
typedef struct { uint8_t note, on; uint32_t end; } seq_play_note;              /* a note sounding: its end, absolute */

typedef struct {
    seq_pat pat[SEQ_NPAT];
    uint8_t cur, next;                   /* next: SEQ_NONE, or the pattern that starts at the next change point */
    uint8_t mute, quant;                 /* quant: 0..100 % of the way to the grid */
    uint32_t origin;                     /* the absolute tick where the current pattern's loop started */
    seq_play_note sounding[16];
    struct { uint8_t on, note, vel; uint16_t len; uint32_t at; } rat[8];   /* the ratchets' hits still to come */
    int16_t lock_step;                   /* the step whose locks are applied, -1 none */
    uint8_t swing;                       /* the track's own swing, added to the global one (0..100) */
    uint8_t div;                         /* how long a step is (SEQ_DIV_*): the track's own clock */
    uint8_t gate;                        /* its notes' length, % as written (10..200, 100 = as written) */
} seq_track;
/* a track's step: 1/16 is the sequencer's grid; the others run the track's own ticks faster or slower */
enum { SEQ_DIV_32, SEQ_DIV_16T, SEQ_DIV_16, SEQ_DIV_8T, SEQ_DIV_8, SEQ_DIV_4T, SEQ_DIV_4, SEQ_DIV_2, SEQ_DIV_BAR, SEQ_DIV_2BAR, SEQ_NDIV };
extern const char *const seq_div_name[SEQ_NDIV];

typedef struct { uint8_t trk, pat; seq_pat p; } seq_snap;
typedef struct { seq_snap h[SEQ_UNDO]; uint8_t n_undo, n_redo; } seq_hist;   /* undo then redo, in one array */
typedef struct { uint16_t bar; uint8_t mute, pat[SEQ_NTRK]; } seq_song_ev;  /* from this bar: these patterns, mutes */
typedef struct seq {
    seq_track t[SEQ_NTRK];
    float rate, bpm;
    int playing, recording, rec_trk;
    uint64_t pos;                        /* absolute position, ticks << 16 */
    uint32_t tick;                       /* absolute tick of pos */
    uint8_t switch_now;                  /* a queued change waits for: 0 the next bar, 1 the current pattern's end */
    uint8_t lead;                        /* (3.3.7) the track in the hands: with switch_now, its pattern's end is every track's */
    /* recording: the keys held, their notes' start */
    struct { uint8_t on, note, vel; uint32_t start; } held[SEQ_HELD];
    seq_hist *hist;                      /* undo / redo (kept apart: it is big), or none */
    /* fills: held (fill), or for one bar (fill_bar: the bar + 1, 0 none) */
    uint8_t fill;
    uint32_t fill_bar;
    uint32_t rnd;
    /* the song */
    seq_song_ev song[SEQ_SONG];
    uint16_t song_n, song_len;           /* entries; bars (0: no song) */
    uint8_t song_rec, song_play;
    uint32_t song_start;                 /* the bar the song (or its recording) started on */
    uint8_t swing;                       /* every track's swing, 0 (straight) .. 100 (a pair played 75 / 25) */
    uint8_t batch, batch_done;           /* batch: the edits until it is cleared count as one undo */
    void (*note_on)(struct seq *, int trk, int note, int vel);
    void (*note_off)(struct seq *, int trk, int note);
    /* the locks of the step now playing on a track (n = 0: back to the sound's own values) */
    void (*locks)(struct seq *, int trk, const seq_lock *lk, int n);
    void *user;
} seq_t;

#define SEQ_NONE 0xFF

void seq_init(seq_t *s, float rate, float bpm, seq_hist *hist);
void seq_set_bpm(seq_t *s, float bpm);
void seq_play(seq_t *s, int on);                         /* on: from the start of every pattern; off: all notes end */
void seq_advance(seq_t *s, uint32_t frames);             /* plays the events of the next frames */
void seq_advance_by(seq_t *s, uint64_t inc);            /* by inc (ticks << 16): a clock from outside */
uint32_t seq_bar(const seq_t *s);                        /* bars since PLAY */
int seq_steps_per_bar(const seq_t *s, int trk);          /* the track's steps in a bar (its DIV), at least 1 */
uint32_t seq_step_of(const seq_t *s, int trk);           /* the track's step now */
uint32_t seq_local(const seq_t *s, int trk, uint32_t tick);   /* a tick of the sequencer as the track's own tick */
void seq_set_div(seq_t *s, int trk, int div);            /* playing: the track goes on from where it is */
void seq_set_gate(seq_t *s, int trk, int gate);
/* patterns */
void seq_queue(seq_t *s, int trk, int pat);              /* trk < 0: every track; at the next change point */
void seq_set_len(seq_t *s, int trk, int steps);          /* the current pattern's length (an undoable edit) */
void seq_step_toggle(seq_t *s, int trk, int step, int note, int vel);   /* a step written / erased (undoable) */
int seq_step_has(const seq_t *s, int trk, int step, int note);         /* note < 0: any note */
void seq_clear(seq_t *s, int trk);                       /* the current pattern emptied (undoable) */
void seq_erase_note(seq_t *s, int trk, int note);        /* every event of that note (undoable) */
void seq_copy(seq_t *s, int trk, int from, int to);      /* a pattern copied (undoable) */
int seq_add(seq_t *s, int trk, uint32_t tick, uint32_t len, int note, int vel);   /* an event, as played (undoable); 0 if full */
int seq_count(const seq_t *s, int trk);                  /* events in the current pattern */
/* live */
void seq_mute(seq_t *s, int trk, int on);
void seq_record(seq_t *s, int on, int trk);              /* a take starts (one undo step) / ends */
void seq_key(seq_t *s, int note, int vel, int down, uint32_t latency_frames);   /* writes it if recording */
/* per step (the current pattern): locks, condition, ratchet (undoable edits) */
void seq_lock_set(seq_t *s, int trk, int step, int param, int value);   /* value -1: that lock removed */
int seq_lock_get(const seq_t *s, int trk, int step, int param);         /* -1: none */
void seq_step_cond(seq_t *s, int trk, int step, int cond);
void seq_step_ratchet(seq_t *s, int trk, int step, int n);
int seq_step_flags(const seq_t *s, int trk, int step);                 /* SEQ_SLIDE | SEQ_ACCENT of the step */
void seq_step_flag(seq_t *s, int trk, int step, int flag, int on);     /* (undoable) */
/* the NOTES page: the events one by one (the current pattern, in time order: an index into its ev[]) */
void seq_ev_set(seq_t *s, int trk, int i, int note, int len, int vel, int flags);   /* -1: unchanged; undoable */
void seq_ev_del(seq_t *s, int trk, int i);
int seq_ev_move(seq_t *s, int trk, int i, int dsteps, int copy);
int seq_ev_len_max(const seq_t *s, int trk, int i, int poly, int *why);
int seq_pat_put(seq_t *s, int trk, int pat, int first, int steps, const uint8_t *notes, int n);
int seq_pat_put2(seq_t *s, int trk, int pat, int first, int steps, const uint8_t *notes, int n, int w);   /* w 8: + cond, ratchet */
void seq_locks_put(seq_t *s, int trk, int pat, const uint8_t *lk, int n);   /* lk: step, param, value */   /* notes: 6 bytes each (tick len note vel) */   /* SLOOP's rules: how long it may last, why (0 a note ahead, 1 4 notes, 3 the end) */   /* a step on / back, or copied after itself: its index now, -1 */
int seq_step_write(seq_t *s, int trk, int step, const uint8_t *notes, int n, int vel);  /* the step's notes replaced;
                                                                                          its index of notes[0], -1 */
void seq_step_reset(seq_t *s, int trk, int step);        /* its locks, condition and ratchet cleared */
int seq_step_info(const seq_t *s, int trk, int step, int *cond, int *ratchet);   /* the number of locks */
int seq_step_get(const seq_t *s, int trk, int step, int *note, int *vel, int *nudge);   /* 0: an empty step */
void seq_step_note(seq_t *s, int trk, int step, int delta);   /* every note of the step, semitones */
void seq_step_vel(seq_t *s, int trk, int step, int vel);
void seq_step_nudge(seq_t *s, int trk, int step, int nudge);  /* off the grid, in ticks (-47 .. +47) */
void seq_set_swing(seq_t *s, int trk, int swing);             /* trk < 0: the global swing */
void seq_shift(seq_t *s, int trk, int dir);                   /* every step one later (dir > 0) / earlier */
void seq_resize(seq_t *s, int trk, int dir);                  /* x2 (a copy after itself) / half */
void seq_transpose(seq_t *s, int trk, int semis);
/* fills and the song */
void seq_fill(seq_t *s, int on);
void seq_fill_next_bar(seq_t *s);
void seq_song_record(seq_t *s, int on);                  /* on: from the next bar; off: its length is the bars recorded */
void seq_song_play(seq_t *s, int on);
uint32_t seq_song_bar(const seq_t *s);                   /* the song's bar now */
/* the song by hand (SONG 2/2): an entry's bars (to the next), set (the later ones move), its pattern (trk < 0: every
 * track), how many entries (more: copies of the last, 4 bars each), the entry playing (-1 none) */
#define SEQ_SONG_BARS 64
int seq_song_len_of(const seq_t *s, int i);
int seq_song_set_len(seq_t *s, int i, int bars);
void seq_song_set_pat(seq_t *s, int i, int trk, int pat);
void seq_song_set_count(seq_t *s, int n);
int seq_song_at(const seq_t *s);
/* history */
uint32_t seq_heard(const seq_t *s, uint32_t latency_frames);   /* the tick the player hears now: the output's delay behind the one computed */
void seq_catch_up(seq_t *s, int trk, uint32_t from);            /* the track's notes from that tick (the sequencer's) to now, played at once: a pattern written behind the playhead */
int seq_undo(seq_t *s);                                  /* 1 if something was undone */
int seq_redo(seq_t *s);
