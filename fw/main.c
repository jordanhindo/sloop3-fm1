/* SPDX-License-Identifier: GPL-3.0-only
 * SLOOP 3.0. SLOOP's way of playing, which its players know, on a firmware written from scratch for the FM-1:
 * float sound, 32 voices on two cores, a sequencer of 128 steps, undo, the screen in big smooth type.
 *
 * As on SLOOP: tracks 1-3 synths (blue, green, yellow), 4 the drums (orange, one sound per white key, in SLOOP's
 * order), and a fifth synth track (pink). ALGORITHM picks the track, PRESETS its sound, SELECT the tempo.
 * A button TAPPED opens its pages (tapped again: the next page; SELECT walks them); HELD it opens its layer: the 16
 * white keys become 16 tiles on the screen (rows of four) and KNOB 1-4 the four dials. A layer held and HOME
 * tapped: the layer stays open (LOCK), both hands free; any other button lets it go.
 *   HOME   tap: TRACKS (each track's sound, steps, level; KNOB swing, level, steps, pan) · hold: the projects (keys
 *          1-8, 16 SAVE NOW); held + SELECT: the menu (SCREEN · LIGHTS, AUDIO, SEQ · SYSTEM, the calibration)
 *   SEQ    tap: the steps (QUANT, GATE; drums: the DRUMS grid, 16 sounds x 16 steps), again: NOTES (a synth track:
 *          the piano roll; keys write the step, KNOB STEP NOTE LEN FORCE, PRESETS the zoom, OCT+ held the note's sheet) · hold: STEPS (KNOB 2 DIV,
 *          3 SWING, 4 the length) (a key a step; a step held: KNOB note, velocity, ratchet, nudge;
 *          PRESETS a lock, ALGORITHM which parameter; OCT+ its condition, OCT- clears it) · drums: black keys
 *          pick the sound, as on SLOOP
 *   GLO    tap: GLOBAL (tempo, swing, click, level), again: TEXTURE 2/6 and 3/6, MIDI 4/6 (IN, OUT, SYNC, CLK),
 *          5/6 (the drums' notes, REMOTE, FX CH, BEND; USB, what came) and REC 6/6 (a track changed while recording: REC stops or goes on; START) · GLO + F#3: MIDI LEARN · hold: MIX (1-5 mute, 6-10 solo, the tracks in order; 11 fill,
 *          12 fill bar, 16 tap tempo; KNOB 1-4 the levels, PRESETS track 5's)
 *   SAVE   tap: SONG, its keys free (again: SONG 2/2, the song's lines by hand: KNOB LINE PAT BARS LINES; again: back) · tapped twice: the project saved · hold: SONG (1-8 the pattern at the next bar, 15 CHAIN: the patterns touched next play in turn, 13 loop / song, 14 SONG REC, 16 every track / this)
 *   EDIT   tap: the engine's pages (OSC, FILTER, VOICE, PLUS: POLY / MONO / LEGATO, GLIDE, PRIO; USER SOUNDS: your 32;
 *          FAVS: your 16 favourites, first in PRESETS; drums: the kit, its sounds)
 *          · hold: ERASE (a key: its note; KNOB 1 shift, KNOB 2 length x2 / half, KNOB 3 transpose; OCT- undo,
 *          OCT+ redo)
 *   ENV    the envelope (ATK DEC SUS REL), then where it goes (the filter)
 *   FX     tap: SENDS (DST, CHO, ECHO, REV), INSERT, INSERT MIX, SLICER, DELAY (TIME FDBK COLR MIX), REV · CHO (SIZE
 *          DAMP CRT CDP), MASTER · hold: PUNCH (a key held: its effect on the whole mix, 27 of them;
 *          KNOB 1 the DJ filter, 2 DUST, 3 DUCK, 4 the track's filter; SELECT the fade; OCT- REV THROW, OCT+
 *          ECHO THROW)
 *   SEL    tap: the track's key, scale, KEYS, chord; again: STRUM, VLEAD · hold: KEY (a key: the key of the song; KNOB chord, scale of
 *          every synth track, KEYS OFF / SNAP / WHITE, transpose); chords on the white keys, CHORD+ on the black
 *   ARP    tap: the arpeggiator (MODE, 15 of them, RATE, OCT, GATE; then SWG, PROB, HOLD, ORD) · hold + a key:
 *          the roll, the key again on the grid (KNOB 1 the rate: 1/8 .. 1/64), recorded
 *   LFO    the LFO (RATE, WAVE, SYNC, FADE), where it goes (PIT FLT SHP AMP), its trigger (TRIG POL PHASE)
 *   GLO    tapped: CLICK OFF / REC / ON (the metronome) on KNOB 3
 *   PLAY, REC (tapped: on / off when let go; held 2 s: the track cleared, nothing else), OCT- / OCT+ the octave (drums: ghost / hard while held). REC stopped
 *          arms it: your first note starts the loop (step 1); on an empty project, a free take: play, then REC
 *          on the "1" after your last bar (1, 2 or 4 bars, the tempo found)
 * The project saves itself (stopped, after 2.5 s without a touch, at most every 20 s), in flash, safe from a power
 * cut; HOME held: the project (3.3.5: one, guaranteed to fit at its fullest): OPEN, NEW, SAVE.
 * MIDI (app/midi.h): USB (class compliant, "FM-1 SLOOP 3") in and out, the TRS jack in; notes on the track of their
 * channel, the controllers, the clock in and out; USB audio out (the FM-1 as a sound card); the web editor (app/edproto.h).
 * At power-on: OCT- held, RESCUE (PLAY an empty project, REC the chip's download mode); OCT- and OCT+, the calibration;
 * PLAY, m-bootloader's menu (its MIDI MODE: the installer). The firmware never returns. */
#include "../hal/fm1/fm1.h"
#include "../hal/fm1/board.h"
#include "../hal/fm1/audio.h"
#include "../hal/fm1/cpu1.h"
#include "../hal/fm1/sys.h"
#include "../hal/fm1/adc.h"
#include "../hal/fm1/panel.h"
#include "../engine/engine.h"
#include "../app/demo_song.h"
#include "../engine/sample.h"
#include "../engine/slice.h"
#include "../engine/fm6.h"
#include "../seq/seq.h"
#include "../seq/loop.h"
#include "../hal/fm1/flash.h"
#include "../app/store.h"
#include "../app/project.h"
#include "../app/ui.h"
#include "../app/gfx.h"
#include "../app/vis.h"
#include "../app/presets.h"
#include "../app/harmony.h"
#include "../app/engdesc.h"
#include "../app/sloopimport.h"
#include "../app/midi.h"
#include "../app/edproto.h"
#include "../app/psound.h"
#include "../app/usersnd.h"
#include "../app/xbank.h"
#include "../hal/fm1/usb.h"
#include "../hal/fm1/uart.h"
#include "../hal/fm1/irq.h"

#define VERSION "3.3.7"
/* COLD: a function of the interface, run from the flash window (fw/ram.ld .xtext); never called while the window is
 * off (a save's flash work: fw/main.c's loop runs only the sound, the panel and the flash then) */
#define COLD __attribute__((section(".xtext.main")))
/* RAMCONST: a table the audio block reads (main.o's other constants are in the window: fw/ram.ld) */
#define RAMCONST __attribute__((section(".data.ramconst")))
#define HALF_FRAMES 256u
#define NTRK 5

static int16_t fxmem[GB_FX_MEM - GB_REV_LOW];   /* (the reverb's first lines: in pmem, low RAM) */
static gb_sound S __attribute__((section(".lowbss")));   /* (snd_init clears it) */
/* (.bootbss: buffers written before they are read, each block; see fw/ram.ld) */
static gb_bus busA __attribute__((section(".bootbss"))), busB __attribute__((section(".bootbss")));
static int32_t abuf[2 * HALF_FRAMES * 2] __attribute__((aligned(32), section(".bootbss")));
static seq_t Q __attribute__((section(".lowbss")));
static loop_t ltake __attribute__((section(".lowbss")));   /* the open free take (seq/loop.c) */
static seq_hist HIST __attribute__((section(".lowbss")));         /* (seq_init clears what is read) */
static int16_t pmem[GB_PUNCH_MEM] __attribute__((section(".lowbss")));   /* the master bus's lines (master_init clears) */
static float base[NTRK][GB_NPARAM];          /* each sound's own values (a step's locks change the live ones) */
static int scope_one;                       /* SAVE + key: 0 every track, 1 the current track alone */
static int sg_cur;                          /* SONG 2/2: the entry the knobs edit */
/* RESCUE: OCT- alone held at power-on, before anything is read from the flash (as SLOOP's USB RESCUE): PLAY starts
 * with an empty project, nothing of the flash read but the settings (a project that would not open stays as it is,
 * and opens again from HOME); REC the chip's USB download mode (M-Flasher). The installer is m-bootloader's MIDI
 * MODE: PLAY held at power-on */
static int rescue, safe_start, safe_hold;           /* safe_hold: a safe start with every place used: no save until one is opened */
/* the quick chain: SAVE held and several pattern keys one after the other: they play in turn, each to its end */
static uint8_t chain[SEQ_NPAT], chain_n, chain_pos, chain_on, chain_arm;   /* chain_arm: CHAIN (key 15) on */
static uint8_t sbuf[STORE_MAX] __attribute__((aligned(4), section(".bootbss")));   /* the project as bytes */
COLD static int sbuf_held(void);     /* (3.1.1) the editor's transfer is in sbuf: nothing else may write it */
COLD static void sbuf_used(void);    /* (3.1.1) sbuf rewritten: the editor's read of a file starts again */
COLD static uint32_t proj_measure(uint32_t *crc);   /* (3.3.5) the project's length and CRC */
static int proj, flash_ok, saving, saving_shown, save_said, saving_snd, save_soon;   /* save_soon: SAVE + a key asked for one */
static uint32_t t_save_soon;   /* save_said: a save ended (1 kept, -1 failed): said once the window is back */
static const char *boot_msg;                 /* (what the first message says, if not the version) */
/* 3.3.2: where the last run was when it stopped (a restart keeps the RAM: .noinit, never cleared; a power-off does
 * not, and its garbage fails the magic). The next start says it: "RESTART IN MIX/FX · ECHO · 3 MIN", the place
 * in the loop, the punch-in running, how long it had played. A few stores a pass: nothing to hear */
enum { W_NONE, W_MIDI, W_SEQ, W_VOICES, W_MIX, W_USB, W_SAVE, W_FLASH, W_SCREEN, W_N };
#define CRUMB_MAGIC 0x43524D42u                                  /* "CRMB" */
static volatile struct { uint32_t magic, where, fx, blocks; } crumb __attribute__((section(".noinit")));
#define crumb_at(w) (crumb.where = (w))
static char crash_msg[64] __attribute__((section(".lowbss")));   /* (.lowbss: not cleared; crumb_report sets it) */
static uint32_t proj_used, saved_crc, saved_len;   /* proj_used: 1 when a saved copy of the project is in the flash (3.3.5: one project) */
enum { HK_OPEN = 0, HK_NEW = 2, HK_SAVE = 4 };      /* (3.3.6) HOME held: the white keys of OPEN, NEW, SAVE (were 1, 8, 16): 1, 3, 5, a key between */
static uint32_t proj_at, saving_len, save_retry, proj_pct, home_arm_t; static int home_arm;          /* (3.3.5) the saved copy's data in the flash; the length being saved; saves begun again */

/* the tracks, as on SLOOP: 1 blue, 2 green, 3 yellow, 4 orange (the drums); 5 pink */
/* the tracks as the player sees them (3.0.1): the four synths 1-4, the drums 5th. Inside, the drums stay track 3 (as
 * SLOOP 2.8's: its projects, ours, the import and the per-pattern drum sounds keep that order): POS[t] is where track t
 * is shown, and since it is a swap, POS[p] is also the track shown at p */
static const uint8_t POS[NTRK] = { 0, 1, 2, 4, 3 };
static uint16_t TRK_COL[NTRK] = { RGB(80, 160, 255), RGB(80, 230, 120), RGB(255, 220, 60), RGB(255, 150, 40), RGB(240, 120, 220) };
/* HOME > SCREEN > COLOR: the built-in palettes (app/ui.h), then your theme (UI_NTHEMES, when there is one: SLOOP 2.7's
 * theme, or the web editor's) with its tracks' colours */
static uint8_t scr_color;
static int theme_ok(void)
{
    const uint8_t *t = us_theme;
    return (t[0] | t[1] << 8 | t[2] << 16 | (uint32_t)t[3] << 24) == 0x324D4854u && (t[36] | t[37] << 8 | t[38] << 16 | (uint32_t)t[39] << 24) == crc32_ieee(0, t, 36);
}
COLD static void color_apply(void)
{
    static const uint16_t DEF[4] = { RGB(80, 160, 255), RGB(80, 230, 120), RGB(255, 220, 60), RGB(255, 150, 40) };
    if (scr_color >= UI_NTHEMES && theme_ok()) {         /* (no theme (yet): the first palette meanwhile) */
        const uint8_t *t = us_theme; uint16_t c[5];
        for (int k = 0; k < 5; k++) c[k] = (uint16_t)(t[12 + 2 * k] | t[13 + 2 * k] << 8);
        ui_theme_own(c, (uint16_t)(t[30] | t[31] << 8));
        for (int k = 0; k < 4; k++) TRK_COL[k] = (uint16_t)(t[22 + 2 * k] | t[23 + 2 * k] << 8);
    } else { ui_theme(scr_color < UI_NTHEMES ? scr_color : 0); for (int k = 0; k < 4; k++) TRK_COL[k] = DEF[k]; }
}
static const char *const PAD[16] = { "KICK", "KICK2", "SNARE", "CLAP", "HAT", "OPEN", "PEDAL", "RIM",
                                     "SNR 2", "TOM L", "TOM H", "CRASH", "RIDE", "SHAKE", "CONGA", "BELL" };
/* the pages; what KNOB 1..4 turn on each sound page (the VA engine's parameters, engine/va.c; -2: the unison's
 * voices, -1: nothing); a drum kit has four */
enum { PG_TRACKS, PG_STEPS, PG_OSC, PG_FILTER, PG_VOICE, PG_ENV, PG_ENVDEST, PG_FX, PG_KIT, PG_GLOBAL, PG_SONG,
       PG_LFO, PG_SEL, PG_ARP, PG_LFODEST, PG_MASTER, PG_PLAY, PG_ARP2, PG_FX2, PG_DSOUND, PG_SEL2, PG_MIDI, PG_MIDI2, PG_NOTES,
       PG_LFO3, PG_INS, PG_INSMIX, PG_SLC, PG_USER, PG_TEX1, PG_TEX2, PG_SONG2, PG_SND, PG_FX3, PG_GREC };
#define NFX_PG 7
static const uint8_t EDIT_PG[6] = { PG_OSC, PG_FILTER, PG_VOICE, PG_PLAY, PG_USER, PG_SND }, FX_PG[NFX_PG] = { PG_FX, PG_INS, PG_INSMIX, PG_SLC, PG_FX2, PG_FX3, PG_MASTER };
static const uint8_t LFO_PG[3] = { PG_LFO, PG_LFODEST, PG_LFO3 };
/* the sound's pages (SLOOP's model, engine.h SP_*): EDIT 1/4 and 2/4 the engine's eight (its own names, engdesc.c),
 * 3/4 how it plays, 4/4 the rest; ENV, ENV DEST; LFO, its depths, its trigger */
static const int8_t P_OSC[4] = { SP_E0, SP_E1, SP_E2, SP_E3 }, P_FLT[4] = { SP_E4, SP_E5, SP_E6, SP_E7 },
                    P_VOI[4] = { SP_VOICE, SP_GLIDE, SP_PRIO, SP_DETUNE }, P_PLY[4] = { SP_GLMODE, SP_FTYPE, SP_DIST, SP_TRIM },
                    P_ENV[4] = { SP_ATK, SP_DEC, SP_SUS, SP_REL }, P_EDS[4] = { SP_EFLT, SP_EPIT, SP_ESHP, SP_ESYNC },
                    P_LFO[4] = { SP_LRATE, SP_LWAVE, SP_LSYNC, SP_LFADE }, P_LFD[4] = { SP_LPIT, SP_LFLT, SP_LSHP, SP_LAMP },
                    P_LF3[4] = { SP_LTRIG, SP_LPOL, SP_LPHASE, -1 }, P_DRUM[4] = { SP_E0, SP_E1, SP_E2, SP_E3 },
                    P_INS[4] = { SP_ITYPE, SP_IA, SP_IB, SP_IC }, P_INSMIX[4] = { SP_IMIX, -1, -1, -1 },
                    P_SLC[4] = { SP_SLCR, SP_SLPAT, SP_SLRATE, SP_SLDEPTH };
/* the track's own effects (the drum track's too): INSERT, its MIX, SLICER */
#define FX_TRACK_PG(pg) ((pg) == PG_INS || (pg) == PG_INSMIX || (pg) == PG_SLC)
/* the track's values every engine shares: how each reads (engdesc.h PF_*) and its choices */
static const char *const N_VOICE[4] = { "POLY", "MONO", "LEG", "UNI" }, *const N_PRIO[3] = { "LAST", "LOW", "HIGH" };
static const char *const N_GLMODE[2] = { "RATE", "TIME" }, *const N_FTYPE[3] = { "LP", "BP", "HP" }, *const N_ONOFF[2] = { "OFF", "ON" };
static const char *const LFO_WAVE[5] = { "SIN", "TRI", "SAW", "SQR", "S&H" }, *const N_TRIG[2] = { "NOTE", "FREE" }, *const N_POL[2] = { "BI", "UNI" };
static const char *const N_LSYNC[11] = { "OFF", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "8T", "1/16", "16T", "1/32" };
static const char *const N_ITYPE[9] = { "OFF", "SOFT", "HARD", "FOLD", "FUZZ", "CRUSH", "PHASR", "FLANG", "CHOR" };
static const char *const N_SLCR[3] = { "OFF", "GATE", "STUT" }, *const N_SLDIV[6] = { "1/8", "1/16", "1/32", "8T", "16T", "32T" };
static const char *const N_IBITS[16] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
static const char *const N_IRATE[16] = { "689", "919", "1.1k", "1.4k", "1.8k", "2.2k", "2.8k", "3.4k", "4.4k", "5.5k", "7.4k", "8.8k",
                                         "11k", "15k", "22k", "44k" };
/* the INSERT's A B C as its type means them: the drives DRIVE TONE LEVEL, CRUSH BITS RATE LPF, the swept RATE DEPTH FDBK */
static const pdesc INS_UI[3][3] = {
    { { "DRIVE", PF_PCT, 0, 0 }, { "TONE", PF_CUT, 0, 0 }, { "LEVEL", PF_LVL, 0, 0 } },
    { { "BITS", PF_LIST8, 16, N_IBITS }, { "RATE", PF_LIST8, 16, N_IRATE }, { "LPF", PF_CUT, 0, 0 } },
    { { "RATE", PF_HZ, 0, 0 }, { "DEPTH", PF_PCT, 0, 0 }, { "FDBK", PF_PCT, 0, 0 } } };
static const pdesc SP_UI[SP_N] = {
    [SP_ATK] = { "ATK", PF_TIME, 0, 0 }, [SP_DEC] = { "DEC", PF_TIME, 0, 0 }, [SP_SUS] = { "SUS", PF_PCT, 0, 0 }, [SP_REL] = { "REL", PF_TIME, 0, 0 },
    [SP_EFLT] = { "FLT", PF_BI, 0, 0 }, [SP_EPIT] = { "PIT", PF_BI, 0, 0 }, [SP_ESHP] = { "SHP", PF_BI, 0, 0 }, [SP_TRIM] = { "TRIM", PF_DB, 0, 0 },
    [SP_LRATE] = { "RATE", PF_HZ, 0, 0 }, [SP_LWAVE] = { "WAVE", PF_LIST, 5, LFO_WAVE }, [SP_LPHASE] = { "PHS", PF_NUM, 0, 0 },
    [SP_LFADE] = { "FADE", PF_TIME, 0, 0 }, [SP_LPIT] = { "PIT", PF_BI, 0, 0 }, [SP_LFLT] = { "FLT", PF_BI, 0, 0 },
    [SP_LSHP] = { "SHP", PF_BI, 0, 0 }, [SP_LAMP] = { "AMP", PF_PCT, 0, 0 }, [SP_LSYNC] = { "SYNC", PF_LIST, 11, N_LSYNC },
    [SP_LTRIG] = { "TRIG", PF_LIST, 2, N_TRIG }, [SP_LPOL] = { "POL", PF_LIST, 2, N_POL }, [SP_ESYNC] = { "SYNC", PF_LIST, 2, N_ONOFF },
    [SP_VOICE] = { "VCE", PF_LIST, 4, N_VOICE }, [SP_GLIDE] = { "GLIDE", PF_TIME, 0, 0 }, [SP_GLMODE] = { "GLMOD", PF_LIST, 2, N_GLMODE },
    [SP_PRIO] = { "PRIO", PF_LIST, 3, N_PRIO }, [SP_ALLOC] = { "ALLOC", PF_NUM, 0, 0 }, [SP_DETUNE] = { "DTUNE", PF_NUM, 0, 0 },
    [SP_FTYPE] = { "TYPE", PF_LIST, 3, N_FTYPE }, [SP_DIST] = { "DST", PF_PCT, 0, 0 },
    [SP_ITYPE] = { "TYPE", PF_LIST, 9, N_ITYPE }, [SP_IA] = { "A", PF_PCT, 0, 0 }, [SP_IB] = { "B", PF_PCT, 0, 0 },
    [SP_IC] = { "C", PF_PCT, 0, 0 }, [SP_IMIX] = { "MIX", PF_PCT, 0, 0 }, [SP_SLCR] = { "MODE", PF_LIST, 3, N_SLCR },
    [SP_SLPAT] = { "PAT", PF_NUM, 0, 0 }, [SP_SLRATE] = { "RATE", PF_LIST, 6, N_SLDIV }, [SP_SLDEPTH] = { "DEPTH", PF_PCT, 0, 0 },
};
static int trk;                                                        /* (the track chosen: below) */
COLD static const pdesc *pdesc_of(int engine, int k)
{
    static const pdesc none = { "", PF_NUM, 0, 0 };
    if (k < 0 || k >= SP_N) return &none;
    if (k >= SP_IA && k <= SP_IC) {                                    /* (as the track's INSERT names them) */
        int ty = gb_pv(S.trk[trk].patch.p, SP_ITYPE);
        if (ty) return &INS_UI[ty < 5 ? 0 : ty == 5 ? 1 : 2][k - SP_IA];
    }
    return k < 8 ? &gb_edesc[engine < ENG_N ? engine : 0].p[k] : &SP_UI[k];
}
/* the values a lock can be on (ALGORITHM, a step held) */
static const int8_t LOCKABLE[] = { SP_E4, SP_E5, SP_EFLT, SP_ATK, SP_DEC, SP_SUS, SP_REL, SP_E0, SP_E1, SP_E2, SP_E3, SP_E6, SP_E7,
                                   SP_EPIT, SP_ESHP, SP_DIST, SP_LRATE, SP_LPIT, SP_LFLT, SP_LSHP, SP_LAMP, SP_GLIDE, SP_DETUNE,
                                   SP_IA, SP_IB, SP_IC, SP_IMIX, SP_SLCR, SP_SLPAT, SP_SLRATE, SP_SLDEPTH,
                                   SEQ_LK_LEVEL, SEQ_LK_PAN, SEQ_LK_CHO, SEQ_LK_DLY, SEQ_LK_REV, SEQ_LK_FILT };
static const int8_t DLOCKABLE[] = { 0, 1, 2, 3, SEQ_LK_LEVEL, SEQ_LK_PAN, SEQ_LK_DLY, SEQ_LK_REV, SEQ_LK_FILT };   /* the drums: their four, the mix */
static const char *const LK_NAME[6] = { "LEVEL", "PAN", "CHO", "DLY", "REV", "FILT" };
COLD static const pdesc *pdesc_of(int engine, int k);
COLD static const char *lock_name(int k) { return k >= SEQ_LK_LEVEL && k < SEQ_LK_END ? LK_NAME[k - SEQ_LK_LEVEL] : pdesc_of(S.trk[trk].patch.engine, k)->name; }
/* the punch-ins' names, in the keys' order (engine.h PK_*): the white keys, then the black */
static const char *const PK_NAME[PK_N] = { "1/4", "1/8", "1/16", "1/32", "STUT", "REV", "STOP", "HALF",
    "LP", "HP", "PHONE", "CRUSH", "ALIAS", "GATE", "ECHO", "WOBBL",
    "-LOW", "-MID", "-HIGH", "RWND", "OCT+", "FLANG", "PAN", "FREEZ", "8T", "MONO", "FADE" };
/* the key, scale and chords of each track (SEL), the roll's rate (ARP), the click */
static harm_cfg harm[NTRK];
static uint8_t strum[NTRK], vlead[NTRK];    /* SEL 2/2: a chord's notes one after the other (0..100: up to 120 ms); the
                                               chords voiced near the one before (0/1) */
static int roll_rate = 1;                   /* 1/8, 1/16, 1/32, 32T, 1/64 */
RAMCONST static const uint16_t ROLL_TICKS[5] = { 192, 96, 48, 32, 24 };
static const char *const ROLL_NAME[5] = { "1/8", "1/16", "1/32", "32T", "1/64" };
/* the arpeggiator (ARP tapped), each track its own: what the keys hold, played one after the other on the grid */
#include "../app/arp.h"
static arp_cfg arpc[NTRK] = { { 0, 3, 1, 50, 0, 100, 0, 0 }, { 0, 3, 1, 50, 0, 100, 0, 0 }, { 0, 3, 1, 50, 0, 100, 0, 0 }, { 0, 3, 1, 50, 0, 100, 0, 0 }, { 0, 3, 1, 50, 0, 100, 0, 0 } };
static int click_mode;                      /* 0 OFF, 1 REC (while a track records, or armed), 2 ON */
/* the REC screen (REC armed, stopped), as SLOOP's: MODE (an empty project: LIBRE, a free take; TEMPO, at the tempo
 * set), BARS (the track's loop: 1, 2 or 4 bars), START (NOTE: the first note starts it; COUNT-IN: PLAY, a bar of
 * clicks, then it records), QUANT (the track's grid pull: ON, or OFF: as played). Kept as the MIDI settings are */
static uint8_t rec_mode, rec_bars = 1, rec_count, rec_keep;   /* rec_bars: 1, 2, 4; rec_keep (3.3.6, GLO REC 6/6): a track changed while recording, the take goes on there (else REC stops) */
static int countin, cin_trk; static uint32_t cin_ticks;     /* the count-in under way, its track, its ticks so far */
static int project_empty(void);
static const char *const CLICK_NAME[3] = { "OFF", "REC", "ON" };
/* the delay's time, in 1/24 of a beat, following the tempo */
static const struct { uint8_t b24; const char *name; } DIV[] = { { 3, "1/32" }, { 6, "1/16" }, { 8, "1/8T" }, { 9, "1/16." },
    { 12, "1/8" }, { 16, "1/4T" }, { 18, "1/8." }, { 24, "1/4" }, { 36, "1/4." } };
#define NDIV ((int)(sizeof DIV / sizeof DIV[0]))
static int dly_div = 6;                     /* a dotted eighth */
/* the delay's COLR and MIX, the reverb's DAMP, the chorus's rate: SLOOP's 0..127 (GLO > DLY, REV/CHO) */
static uint8_t fxv[4] = { 70, 90, 60, 40 };
static void fxv_apply(void)
{
    S.dly_damp = 0.7f * (1.0f - (float)fxv[0] * (1.0f / 127.0f)); S.dly_mix = (float)fxv[1] * (1.0f / 90.0f);
    S.rev_damp = 0.75f * (float)fxv[2] * (1.0f / 127.0f); S.cho_rate = 0.6f * gb_exp2f(((float)fxv[3] - 40.0f) * (1.0f / 24.0f));
}
/* each track's factory sound (presets.c); -1 none; -2 - k: preset k, since edited */
static int preset_origin(int p) { return p >= 0 ? p : p <= -2 ? -p - 2 : -1; }
static float rate_hz;
static uint32_t lsb = 60;

static char *put_u(char *p, uint32_t v) { char t[12]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *p++ = t[--n]; return p; }
static char *put_s(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *put_i(char *p, int v) { if (v > 0) *p++ = '+'; else if (v < 0) { *p++ = '-'; v = -v; } return put_u(p, (uint32_t)v); }
static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static int is_drum(int t) { return S.trk[t].patch.engine == ENG_DRUM; }
/* a sound, as a number (a track's preset_of, a favourite): a factory sound (presets.c), US_BASE + one of your 32
 * places, and (3.2) FB_BASE + a DX7 voice of your FM6 bank (the editor's, or SLOOP 2.8's: PTCH B1..B27), which until
 * then only PTCH reached, without its name. The voices' names: kept in RAM, read one a round (fm6_bank_poll) */
#define FB_BASE 2000
/* (3.3.6) your kits on the drum track: UK_BASE + the kit (USR1..USR4: a sample slot as a kit, named by its sample;
 * SYN1..SYN4), a sound as the factory kits are: chosen in SOUNDS (YOUR KITS), kept with the project, never lost */
#define UK_BASE 3000
COLD static int uk_ok(int kit)
{
    if (kit >= GB_KIT_USR && kit < GB_KIT_USR + 4) return smp_user_nz(kit - GB_KIT_USR) > 0;
    return kit >= GB_KIT_SYN && kit < GB_NKITS;
}
COLD static const char *uk_name(int kit)
{
    static char b[10];
    if (kit >= GB_KIT_USR && kit < GB_KIT_USR + 4 && smp_user_name(kit - GB_KIT_USR, b)[0]) return b;
    return kit >= 0 && kit < GB_NKITS ? gb_dkit_names[kit] : "";
}
#define FB_GAIN 0.75f
static uint32_t fm6_bank_at, fm6_bank_used;
static char fb_nm[FM6_NBANK][11] __attribute__((section(".lowbss")));   /* (cleared at the start, before the first screen) */
static uint8_t fb_scan;
/* an FM6 track's PTCH, in the engine's own range (until 3.2 read on the 0..127 scale of the shared values: B1 was
 * taken for B23, its voice never the one the engine waited for, and every bank voice played as the INIT voice) */
static int fm6_ptch(int t) { return (int)gb_ev(&S.trk[t].patch, 7); }
static int fb_ok(int k) { return k >= 0 && k < FM6_NBANK && fm6_bank_at && ((fm6_bank_used >> k) & 1u); }
COLD static const char *fb_name(int k)
{
    static char b[8];
    if (k < 0 || k >= FM6_NBANK) return "";
    if (fb_nm[k][0]) return fb_nm[k];
    *put_u(put_s(b, "DX7 "), (uint32_t)k + 1u) = 0; return b;
}
COLD static int snd_ok(int f)
{
    return f < 0 ? 0 : f < gb_npresets ? 1 : f >= UK_BASE ? uk_ok(f - UK_BASE) : f >= FB_BASE ? fb_ok(f - FB_BASE) : f >= US_BASE && f < US_BASE + US_N && us_used(f - US_BASE);
}
COLD static const char *snd_name(int f) { return f >= UK_BASE ? uk_name(f - UK_BASE) : f >= FB_BASE ? fb_name(f - FB_BASE) : f >= US_BASE ? us_name(f - US_BASE) : f >= 0 && f < gb_npresets ? gb_presets[f].name : ""; }
/* your favourites (as SLOOP 2.7's): 16 places, the first category of SOUNDS, each on its white key; kept with your sounds */
COLD static int fav_ok(int f) { return snd_ok(f) && f < UK_BASE && (f >= gb_npresets || gb_presets[f].engine != ENG_DRUM); }
/* SOUNDS (3.2; EDIT 6/6, and what PRESETS opens): the sounds by category, 16 a page, each on its white key. The
 * categories: your favourites, the factory sounds kind by kind, your 32 places, the DX7 voices of your bank; on the
 * drum track, the kits. Opened by EDIT, the white keys choose (and play); opened by PRESETS, it shows where the
 * knob is, the keys play as ever, and it goes back by itself (EDIT: it stays, the keys choose) */
enum { SB_FAV = 0, SB_USER = K_N + 1, SB_BANK, SB_UKIT, SB_NCAT };  /* (1 .. K_N: a kind + 1; SB_UKIT: your kits, the drum track's) */
static int uk_at(int i) { return i < 4 ? GB_KIT_USR + i : GB_KIT_SYN + i - 4; }   /* YOUR KITS: USR1-4, then SYN1-4 */
static uint8_t sb_cat, sb_pg, sb_n, sb_was, fav_dirty;             /* sb_n: the category's places; sb_was: the page was shown last round */
static int8_t sb_trk = -1, sb_back = -1;                           /* the track it shows; the page PRESETS opened it from (-1: EDIT did) */
static int16_t sb_o[16];                                           /* the page's sounds (-1: none there) */
static uint32_t sb_t, fav_t;
COLD static void sb_key(int w);
COLD static int sb_list(int c, int first, int16_t *o)              /* category c: its 16 sounds from `first`; its places */
{
    int n = 0;
    for (int i = 0; i < 16; i++) o[i] = -1;
    if (c == SB_FAV) { if (!first) for (int i = 0; i < US_NFAV; i++) if (fav_ok(us_fav[i])) o[i] = (int16_t)us_fav[i]; return US_NFAV; }
    if (c == SB_USER) { for (int i = 0; i < 16; i++) if (first + i < US_N && us_used(first + i)) o[i] = (int16_t)(US_BASE + first + i); return US_N; }
    if (c == SB_BANK) { for (int i = 0; i < 16; i++) if (fb_ok(first + i)) o[i] = (int16_t)(FB_BASE + first + i); return fm6_bank_at && fm6_bank_used ? FM6_NBANK : 0; }
    if (c == SB_UKIT) { for (int i = 0; i < 8 && !first; i++) if (uk_ok(uk_at(i))) o[i] = (int16_t)(UK_BASE + uk_at(i)); return 8; }
    for (int i = 0; i < gb_npresets; i++) if (gb_presets[i].kind == c - 1) { if (n >= first && n < first + 16) o[n - first] = (int16_t)i; n++; }
    return n;
}
COLD static const char *sb_cat_name(int c) { return c == SB_FAV ? "FAVS" : c == SB_USER ? "YOURS" : c == SB_BANK ? "DX7" : c == SB_UKIT ? "YOUR KITS" : gb_kind_name[c - 1]; }
/* the sound f: its place in *cat if it is there (a favourite, while FAVS is shown), else its own category and place */
COLD static int sb_where(int f, int *cat)
{
    if (f < 0) return -1;
    if (*cat == SB_FAV) for (int i = 0; i < US_NFAV; i++) if (us_fav[i] == f) return i;
    if (f >= UK_BASE) { int k = f - UK_BASE; *cat = SB_UKIT; return k >= GB_KIT_SYN ? 4 + k - GB_KIT_SYN : k >= GB_KIT_USR && k < GB_KIT_USR + 4 ? k - GB_KIT_USR : -1; }
    if (f >= FB_BASE) { *cat = SB_BANK; return f - FB_BASE; }
    if (f >= US_BASE) { *cat = SB_USER; return f - US_BASE; }
    if (f >= gb_npresets) return -1;
    int n = 0; for (int i = 0; i < f; i++) n += gb_presets[i].kind == gb_presets[f].kind;
    *cat = gb_presets[f].kind + 1; return n;
}
/* the arpeggiator's run: the notes held (as pressed), what sounds, the grid */
static struct { uint8_t n, note[16], vel[16], keys, latched; int trk; uint32_t grid, at, off_at; uint8_t snd[4], nsnd; uint32_t step, rnd; int walk; } A = { .trk = -1, .rnd = 0x9E3779B9u };
static int arp_on(int t) { return t >= 0 && t < NTRK && !is_drum(t) && arpc[t].mode != AM_OFF; }

/* ---------------------------------------------------------------- MIDI out (app/midi.c; the wires: hal/fm1/usb.c,
 * hal/fm1/uart.c). Every note the firmware starts goes through play_on / play_off: the keys' are sent with OUT
 * KEYS, the sequencer's, the arpeggiator's and the rolls' with OUT SEQ too; MIDI's own notes are never sent back */
static midi_cfg MC;
static int out_seq;                          /* (set while the rolls play: their notes count as the sequencer's) */
static uint32_t midi_n;                      /* the messages come so far, USB and TRS (the screen) */
static void midi_out(uint8_t a, uint8_t b, uint8_t c, int n)
{
    if (!usb_midi_ready()) return;
    uint8_t m[3] = { a, b, c }, pk[4];
    midi_usb_packet(m, n, pk); usb_midi_put(pk);
}
static void out_note(gb_owner o, int t, int n, int vel, int on)
{
    int kind = o >> 8, seqlike = kind == OWN_SEQ || kind == OWN_ARP || (kind == OWN_KEY && out_seq);
    if (MC.out == MIDI_OUT_OFF || t < 0 || t >= NTRK || (kind != OWN_KEY && !seqlike)) return;
    if (seqlike && MC.out != MIDI_OUT_SEQ) return;
    if (is_drum(t)) n = midi_drum_out(n, MC.dmap);
    if (n < 0 || n > 127) return;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    midi_out((uint8_t)((on ? 0x90 : 0x80) | midi_ch_of_track(t)), (uint8_t)n, (uint8_t)(on ? vel : 64), 3);
}
static void play_on(gb_owner o, int t, int n, float vel) { snd_note_on(&S, o, t, n, vel); out_note(o, t, n, (int)(vel * 127.0f + 0.5f), 1); }
static void play_off(gb_owner o, int t, int n) { snd_note_off(&S, o, t, n); out_note(o, t, n, 0, 0); }

/* ---------------------------------------------------------------- the sequencer to the sound */
static uint8_t solo[NTRK];
static int solo_any(void) { for (int t = 0; t < NTRK; t++) if (solo[t]) return 1; return 0; }
static void sq_on(seq_t *s, int t, int n, int v)
{
    (void)s;
    if (t < NTRK && solo_any() && !solo[t]) return;                    /* a solo: the others are not heard */
    if (S.trk[t].patch.engine == ENG_DRUM && ((S.trk[t].patch.dmute >> (n & 15)) & 1u)) return;   /* a drum sound muted */
    float vel = (v & SEQ_ON_ACC) ? 1.25f : (float)(v & 127) * (1.0f / 127.0f);   /* ACCENT: harder than 127 */
    snd_slide(&S, t, (v & SEQ_ON_SLID) != 0);                             /* after a SLIDE: a glide to it (SLOOP's) */
    play_on(GB_OWNER(OWN_SEQ, t), t, n, vel);
    snd_slide(&S, t, 0);
}
static void sq_off(seq_t *s, int t, int n) { (void)s; play_off(GB_OWNER(OWN_SEQ, t), t, n); }
/* the track's mix under a lock (SEQ_LK_*): its own value kept while a lock holds it */
static float mix_own[NTRK][6]; static uint8_t mix_held[NTRK];
RAMCONST static const uint16_t MIX_AT[6] = { __builtin_offsetof(gb_track, level), __builtin_offsetof(gb_track, pan),
    __builtin_offsetof(gb_track, send_cho), __builtin_offsetof(gb_track, send_dly), __builtin_offsetof(gb_track, send_rev), __builtin_offsetof(gb_track, filt) };
static gb_smooth *mix_of(int t, int k) { return (gb_smooth *)((uint8_t *)&S.trk[t] + MIX_AT[k]); }
static void mix_back(int t)                                  /* its own values back */
{
    for (int k = 0; k < 6; k++) if ((mix_held[t] >> k) & 1u) sm_set(mix_of(t, k), mix_own[t][k]);
    mix_held[t] = 0;
}
static float mix_val(int k, int v) { return k == 5 ? (float)v * (2.0f / 255.0f) - 1.0f : (float)v * (1.0f / 255.0f); }
static void sq_locks(seq_t *s, int t, const seq_lock *lk, int n)
{
    (void)s;
    if (t >= NTRK) return;
    float *p = S.trk[t].patch.p;
    for (int i = 0; i < GB_NPARAM; i++) p[i] = base[t][i];                /* the sound's own values back */
    mix_back(t);
    for (int i = 0; i < n; i++) {
        if (lk[i].param < GB_NPARAM) p[lk[i].param] = (float)lk[i].value * (1.0f / 255.0f);
        else if (lk[i].param < SEQ_LK_END) {
            int k = lk[i].param - SEQ_LK_LEVEL;
            gb_smooth *m = mix_of(t, k);
            if (!((mix_held[t] >> k) & 1u)) { mix_own[t][k] = m->tgt; mix_held[t] |= (uint8_t)(1u << k); }
            sm_set(m, mix_val(k, lk[i].value));
        }
    }
}
static void base_from_sound(void) { for (int t = 0; t < NTRK; t++) for (int i = 0; i < GB_NPARAM; i++) base[t][i] = S.trk[t].patch.p[i]; }
COLD static int lock_own(int t, int k)                     /* a lock's starting value: the track's own */
{
    if (k < GB_NPARAM) return (int)(base[t][k] * 255.0f + 0.5f);
    int m = k - SEQ_LK_LEVEL; float x = ((mix_held[t] >> m) & 1u) ? mix_own[t][m] : mix_of(t, m)->tgt;
    return (int)((m == 5 ? (x + 1.0f) * 0.5f : x) * 255.0f + 0.5f);
}
static void base_of_track(int t) { mix_back(t); for (int i = 0; i < GB_NPARAM; i++) base[t][i] = S.trk[t].patch.p[i]; Q.t[t].lock_step = -1; }
/* each pattern's own sound (app/psound.h): the live sound is the current pattern's; a pattern change swaps them */
static psounds PS __attribute__((section(".lowbss")));          /* (ps_all fills it before it is read) */
static int preset_of[NTRK];
COLD static void ps_all(void) { for (int t = 0; t < NTRK; t++) ps_fill(&PS, t, Q.t[t].cur, &S.trk[t].patch, base[t], preset_of[t]); }
static void ps_keep(int t) { ps_take(&PS, t, PS.cur[t], &S.trk[t].patch, base[t], preset_of[t]); }   /* live -> its pattern */
static void ps_follow(void)                         /* (in the audio block: the sequencer may have changed pattern) */
{
    for (int t = 0; t < NTRK; t++) {
        int cur = Q.t[t].cur;
        if (cur == PS.cur[t] || cur >= SEQ_NPAT) continue;
        ps_keep(t);
        ps_give(&PS, t, cur, &S.trk[t].patch, base[t], &preset_of[t]);
        PS.cur[t] = (uint8_t)cur; Q.t[t].lock_step = -1;
    }
}
static void sound_from_base(void)                                        /* (before a save: the own values) */
{
    for (int t = 0; t < NTRK; t++) { mix_back(t); for (int i = 0; i < GB_NPARAM; i++) S.trk[t].patch.p[i] = base[t][i]; Q.t[t].lock_step = -1; }
}

/* ---------------------------------------------------------------- the block, on one core or two */
static void roll_step(void);
static void arp_step(void);
static void strum_step(void);
static int click_on(void);
static float click_amp, click_ph, click_inc;
static uint32_t click_beat = 0xFFFFFFFFu;
static void click_block(int32_t *out)                   /* the metronome: a short tick, higher on the bar's first beat */
{
    if ((Q.playing && click_on()) || countin) {
        uint32_t beat = (Q.playing ? Q.tick : cin_ticks) / (4 * SEQ_TPS);
        if (beat != click_beat) { click_beat = beat; click_amp = 0.25f; click_ph = 0.0f; click_inc = (beat % 4u ? 1100.0f : 1650.0f) * GB_ISR; }
    } else click_beat = 0xFFFFFFFFu;
    if (click_amp < 0.001f) return;
    float g = S.master.cur * 2.0f;
    for (int n = 0; n < GB_BLOCK; n++) {
        float x = gb_sin_turn(click_ph) * click_amp * g * 8388607.0f;
        click_ph += click_inc; click_ph -= click_ph >= 1.0f ? 1.0f : 0.0f;
        click_amp *= 0.9985f;                                 /* (~15 ms) */
        for (int c = 0; c < 2; c++) { int64_t y = (int64_t)out[2 * n + c] + (int32_t)x; out[2 * n + c] = (int32_t)(y > 8388607 ? 8388607 : y < -8388608 ? -8388608 : y); }
    }
}
static void job_voices(uint32_t arg) { (void)arg; snd_render_voices(&S, &busB, 1, 2); }
/* ---------------------------------------------------------------- the device's settings (HOME held, SELECT) */
/* as SLOOP's menu has them: SCREEN and LIGHTS (KNOB ACCEL; LIGHTS, KEYS, NOTES), AUDIO (SPEAKER LOWCUT, USB AUDIO, USB
 * RATE, FILTER RESO), SEQ and SYSTEM (DRUM PREVIEW, 2ND CORE, MIDI LEARN CLEAR, HARDWARE CALIBRATION). Kept with the
 * projects (project_extra.home), taken at power-on only */
enum { HS_ACCEL, HS_LIGHTS, HS_KEYS, HS_NOTES, HS_LOWCUT, HS_USB, HS_RATE, HS_RESO, HS_DPREV, HS_CORE2, HS_N };
static uint8_t hs[HS_N] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
static const uint8_t HS_MAX[HS_N] = { 1, 3, 3, 1, 1, 1, 2, 10, 1, 1 };
static uint8_t cal_ok, cal_btn[14], cal_enc[7]; static int8_t cal_dir[7];   /* the calibration (panel_map) */
static int home_sec;                                   /* HOME held: 0 the projects, 1..3 the settings */
static int cal_step = -1;
/* MIDI: GLO > MIDI 3's REMOTE and FX channels (0 off, 1..16), the bend's range; MIDI LEARN's map (as SLOOP's: 16 CCs,
 * each one value of one track, whatever channel it comes on): cc | track << 7 | code << 10, code 1..48 a sound value
 * (SP_* + 1), 49 LEVEL, 50 PAN, 51..53 the delay, reverb, chorus sends; 0 empty */
static uint8_t m_rmch, m_fxch, m_bend = 2;
#define ML_N 16
enum { ML_LEVEL = 49, ML_PAN, ML_DLY, ML_REV, ML_CHO };
static uint16_t ml_tab[ML_N];
static uint8_t ml_on, ml_pick, ml_trk, ml_code;        /* learning; a value picked (its track, its code) */
COLD static int ml_count(void);
COLD static int apply_user(int t, int k);
COLD static int apply_bank(int t, int k);
COLD static int apply_ukit(int t, int kit);
static int us_place;                                   /* USER SOUNDS: the place KNOB 1 is on */                              /* the calibration under way: its step (-1: none) */
COLD static void home_apply(void);
static int two;
static uint32_t c1_fail;
/* the clock in (SYNC EXT): while its pulses come, they move the sequencer; without them (0.5 s), our own tempo.
 * blk_t: the block's time, the half's start and its place in it (the blocks of a half are made in a burst) */
static midi_clock MK;
static uint32_t blk_t, blk_ticks;
static int ext_on(uint32_t now) { return MC.sync == MIDI_SYNC_EXT && mclk_alive(&MK, now); }
/* the clock out (CLK ON, SYNC INT): 24 pulses a beat at the tempo, stopped too (the other gear knows the tempo
 * before PLAY); START with the downbeat's pulse, STOP */
static uint8_t co_play; static uint32_t co_pulse; static float co_acc;
static void clock_out_block(void)
{
    if (!MC.clk_out || ext_on(blk_t) || !usb_midi_ready()) {
        if (co_play && usb_midi_ready()) midi_out(0xFC, 0, 0, 1);
        co_play = 0; co_acc = 0.0f; return;
    }
    if (Q.playing) {
        uint32_t p = Q.tick / 16u;                                 /* a pulse: 16 ticks (a beat: 384) */
        if (!co_play) { co_play = 1; co_pulse = p; midi_out(0xFA, 0, 0, 1); midi_out(0xF8, 0, 0, 1); return; }
        if (p < co_pulse) co_pulse = p;
        for (int i = 0; co_pulse < p && i < 8; i++) { co_pulse++; midi_out(0xF8, 0, 0, 1); }
        co_pulse = p;
    } else {
        if (co_play) { co_play = 0; midi_out(0xFC, 0, 0, 1); }
        co_acc += Q.bpm * (24.0f / 60.0f) / rate_hz * (float)GB_BLOCK;
        for (int i = 0; co_acc >= 1.0f && i < 8; i++) { co_acc -= 1.0f; midi_out(0xF8, 0, 0, 1); }
        if (co_acc >= 1.0f) co_acc = 0.0f;
    }
}
static void record_now(int t);
__attribute__((noinline)) static void render_block(int32_t *out)
{
    crumb.fx = (uint32_t)(S.pk.fx + 1); crumb.blocks = gb_blocks; crumb_at(W_SEQ);
    gb_flash_ok = (uint8_t)flash_xip_on();               /* (a save has the window off: the sample bank is not read) */
    if (countin) {                                       /* the count-in: a bar of clicks at the tempo, then REC */
        cin_ticks += (uint32_t)(Q.bpm * 4.0f * (float)SEQ_TPS / 60.0f / rate_hz * (float)GB_BLOCK + 0.5f);
        if (cin_ticks >= SEQ_BAR) { countin = 0; record_now(cin_trk); }
    }
    if (Q.playing && ext_on(blk_t)) {
        /* at most twice the tempo (from the pulses' interval once known): a lag caught up, never a jump */
        uint64_t cap = MK.iv ? (uint64_t)blk_ticks * 2u * 65536u / MK.iv
                             : (uint64_t)(uint32_t)(Q.bpm * (24.0f / 60.0f) / rate_hz * (float)GB_BLOCK * 2.0f * 65536.0f);   /* (32 bits: no soft double here) */
        seq_advance_by(&Q, mclk_advance(&MK, blk_t, cap) * 16u);
    } else seq_advance(&Q, GB_BLOCK);
    ps_follow();
    clock_out_block();
    roll_step();
    arp_step();
    strum_step();
    /* the transport, for the master bus: a beat in frames, where this block starts in its bar (the punch-ins' grid) */
    float tpf = Q.bpm * 4.0f * (float)SEQ_TPS / 60.0f / rate_hz;           /* ticks a frame */
    S.beat = (float)(4 * SEQ_TPS) / tpf; S.playing = (uint8_t)Q.playing;
    if (Q.playing) {
        uint64_t in_bar = Q.pos % ((uint64_t)SEQ_BAR << 16);
        float g = (float)(uint32_t)(in_bar >> 8) * (1.0f / 256.0f) / tpf - (float)GB_BLOCK;
        S.bar = (uint32_t)(Q.pos / ((uint64_t)SEQ_BAR << 16));
        if (g < 0.0f) { g += 4.0f * S.beat; S.bar--; }
        S.grid = g;
    } else S.grid = -1.0f;
    for (int t = 0; t < SEQ_NTRK && t < GB_NTRACK; t++) { int w = Q.swing + Q.t[t].swing; S.trk[t].swing = (uint8_t)(w > 100 ? 100 : w); }
    crumb_at(W_VOICES);
    if (two && hs[HS_CORE2] && cpu1_run(job_voices, 0)) {
        snd_render_voices(&S, &busA, 0, 2);
        if (cpu1_wait(4000u) != 0) { cpu1_stop(); two = 0; c1_fail++; snd_render_voices(&S, &busB, 1, 2); }
        crumb_at(W_MIX); snd_mix(&S, &busA, &busB, out);
    } else {
        snd_render_voices(&S, &busA, 0, 1);
        crumb_at(W_MIX); snd_mix(&S, &busA, 0, out);
    }
    crumb_at(W_USB);
    click_block(out);
    if (S.usb_full) {                                    /* USB AUDIO: the output, or (FULL) the mix before MASTER */
        static int32_t u[GB_BLOCK * 2];
        for (int n = 0; n < GB_BLOCK; n++) { u[2 * n] = (int32_t)(S.usb_l[n] * 8388607.0f); u[2 * n + 1] = (int32_t)(S.usb_r[n] * 8388607.0f); }
        usb_audio_put(u, GB_BLOCK);
    } else usb_audio_put(out, GB_BLOCK);
}

/* ---------------------------------------------------------------- a new project: SLOOP's power-on sounds, empty
 * patterns (with_loop: the first loop, to hear something at once) */
static void preload(void);
COLD static void preset_extras(int t, int np);
COLD static void new_project(int with_loop)
{
    seq_init(&Q, rate_hz, 92.0f, &HIST);
    loop_init(&ltake);
    Q.note_on = sq_on; Q.note_off = sq_off; Q.locks = sq_locks;
    for (int t = 0; t < NTRK; t++) Q.t[t].quant = 100;   /* (3.3.7) a take on the steps, as SLOOP 2.8 recorded (QUANT OFF: as played) */
    static const float LVL[NTRK] = { 0.45f, 0.45f, 0.4f, 0.85f, 0.3f };   /* (the sounds' own levels: presets.c) */
    /* SLOOP's: 808 BOOM, RHODES, LOFI FLUTE (SAMPLE); the kit; a pad on track 5 (SLOOP's engine order, place) */
    static const uint8_t SOUND0[NTRK][2] = { { 0, 0 }, { 1, 0 }, { 4, 5 }, { 0xFF, 0 }, { 0, 13 } };
    snd_all_off(&S);
    for (int t = 0; t < NTRK; t++) {
        int np = SOUND0[t][0] == 0xFF ? preset_step(-1, 1, 0) : preset_of_sloop(SOUND0[t][0], SOUND0[t][1]);
        if (np < 0) np = preset_of_sloop(0, 13);                           /* (an engine not here yet) */
        preset_of[t] = np;
        S.trk[t].patch.engine = gb_presets[np].engine;
        preset_apply(&S, t, np); harm[t].transpose = 0; preset_extras(t, np);
        sm_set(&S.trk[t].level, LVL[t]); sm_set(&S.trk[t].pan, t == 2 ? 0.62f : 0.5f);
        solo[t] = 0;
    }
    S.dly_fb = 0.45f; S.rev_fb = 0.82f; dly_div = 6;
    { static const uint8_t FX0[4] = { 70, 90, 60, 40 }; for (int i = 0; i < 4; i++) fxv[i] = FX0[i]; fxv_apply(); }
    S.dust = 0.0f; S.duck = 0.0f; sm_set(&S.mfilt, 0.0f); S.pk_fade = 0; S.cho_depth = 0.5f;
    { static const uint8_t TX0[7] = { 0, 64, 0, 40, 2, 0, 0 }; for (int i = 0; i < 7; i++) S.tx[i] = TX0[i]; }   /* (TEXTURE off) */
    for (int t = 0; t < NTRK; t++) { arpc[t] = (arp_cfg){ 0, 3, 1, 50, 0, 100, 0, 0 }; strum[t] = 0; vlead[t] = 0; }
    for (int t = 0; t < GB_NTRACK; t++) sm_set(&S.trk[t].filt, 0.0f);
    for (int t = 0; t < NTRK; t++) { harm[t].root = 0; harm[t].scale = 0; harm[t].keys = KEYS_OFF; harm[t].chord = CH_OFF; harm[t].transpose = 0; }
    base_from_sound(); ps_all();
    if (with_loop) preload();
}
COLD static void preload(void)
{
    for (int st = 0; st < 16; st += 4) seq_step_toggle(&Q, 3, st, 0, 120);          /* kick */
    seq_step_toggle(&Q, 3, 4, 2, 110); seq_step_toggle(&Q, 3, 12, 2, 110);          /* snare */
    for (int st = 2; st < 16; st += 4) seq_step_toggle(&Q, 3, st, 4, 70);           /* hat */
    static const int BASS[16] = { 33, -1, 33, 45, -1, 33, -1, 43, 33, -1, 45, -1, 31, -1, 43, -1 };
    for (int st = 0; st < 16; st++) if (BASS[st] >= 0) seq_step_toggle(&Q, 0, st, BASS[st], 100);
    static const int CH[4][3] = { { 57, 60, 64 }, { 55, 59, 62 }, { 53, 57, 60 }, { 55, 59, 64 } };
    seq_set_len(&Q, 1, 64);
    for (int b = 0; b < 4; b++) for (int k = 0; k < 3; k++) seq_step_toggle(&Q, 1, b * 16, CH[b][k], 90);
    for (int i = 0; i < Q.t[1].pat[0].n; i++) if (Q.t[1].pat[0].ev[i].tick % (16 * SEQ_TPS) == 0) Q.t[1].pat[0].ev[i].len = 15 * SEQ_TPS;
    HIST.n_undo = HIST.n_redo = 0;                                                   /* not undoable */
}

/* ---------------------------------------------------------------- the keys: white key index, notes */
static int white_of[27];                    /* key 0..26 (F3..G5) -> white index 0..15, or -1 */
static int white_key[16];                   /* white index -> key */
COLD static void keys_map(void)
{
    int w = 0;
    for (int k = 0; k < 27; k++) {
        int n = (53 + k) % 12, white = n == 0 || n == 2 || n == 4 || n == 5 || n == 7 || n == 9 || n == 11;
        white_of[k] = white ? w : -1;
        if (white) white_key[w++] = k;
    }
}
static int oct, page, recording_armed;
static int last_notes[NTRK] = { 45, 57, 69, 0, 57 };   /* each track's last note (what a step writes; drums: the pad) */
#define last_note last_notes[trk]
static int drum_note_of_key(int k) { while (k > 0 && white_of[k] < 0) k--; return white_of[k] < 0 ? 0 : white_of[k]; }
static int is_slice(int t) { return S.trk[t].patch.engine == ENG_SLICE; }
/* a key's note: the drums' pads by white key; SLICE: the lowest key is slice 0 (note 60, engine/slice.c); else F3 up */
static int note_of_key(int k) { return is_drum(trk) ? drum_note_of_key(k) : is_slice(trk) ? 60 + k + 12 * oct : 53 + k + 12 * oct; }
COLD static void note_name(char *p, int t, int n)
{
    static const char NM[12][3] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (is_drum(t)) { p = put_s(p, PAD[n & 15]); *p = 0; return; }
    p = put_s(p, NM[n % 12]);
    if (n < 12) *p++ = '-'; else p = put_u(p, (uint32_t)(n / 12 - 1));
    *p = 0;
}
/* the drum track's black keys pick a sound, as on SLOOP (a pair: the same key again, the other) */
COLD static int drum_black(int k, int cur)
{
    static const int8_t KEY[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
    static const int8_t A[11] = { 0, 2, 7, 3, 4, 5, 9, 11, 12, 13, 15 }, B[11] = { 1, 8, 7, 3, 6, 5, 10, 11, 12, 14, 15 };
    for (int i = 0; i < 11; i++) if (KEY[i] == k) return cur == A[i] ? B[i] : A[i];
    return -1;
}
COLD static int pattern_has_note(int t, int n)
{
    const seq_pat *p = &Q.t[t].pat[Q.t[t].cur];
    for (int i = 0; i < p->n; i++) if (p->ev[i].note == n) return 1;
    return 0;
}

/* ---------------------------------------------------------------- a step's condition, in the order OCT+ turns them */
static const uint8_t CONDS[] = { SEQ_ALWAYS, SEQ_FILL, SEQ_NOFILL,
    SEQ_CYCLE + 2 * 8 + 0, SEQ_CYCLE + 2 * 8 + 1, SEQ_CYCLE + 3 * 8 + 0, SEQ_CYCLE + 3 * 8 + 1, SEQ_CYCLE + 3 * 8 + 2,
    SEQ_CYCLE + 4 * 8 + 0, SEQ_CYCLE + 4 * 8 + 1, SEQ_CYCLE + 4 * 8 + 2, SEQ_CYCLE + 4 * 8 + 3,
    SEQ_CHANCE + 1, SEQ_CHANCE + 2, SEQ_CHANCE + 3, SEQ_CHANCE + 5, SEQ_CHANCE + 7, SEQ_CHANCE + 9 };
#define NCONDS ((int)sizeof CONDS)
COLD static char *cond_name(char *p, int c)       /* in words: the screen says what it does */
{
    if (c == SEQ_ALWAYS) return put_s(p, "ALWAYS");
    if (c == SEQ_FILL) return put_s(p, "FILL");
    if (c == SEQ_NOFILL) return put_s(p, "NO FILL");
    if ((c & 0xC0) == SEQ_CHANCE) { p = put_u(p, (uint32_t)(c & 15) * 10u); return put_s(p, " %"); }
    p = put_u(p, (uint32_t)(c & 7) + 1u); p = put_s(p, " OF "); return put_u(p, (uint32_t)(c >> 3) & 7u);
}
static int hold_step = -1, hold_erase;      /* the step held in SEQ, and whether its release erases it */
static uint16_t hold_more; static int hold_page;   /* other steps held with it (bits of a page): edited together */
static int lock_param = 6;                  /* the parameter PRESETS locks on a step held (the last turned) */

/* ---------------------------------------------------------------- messages, pages, layers
 * 3.3: two kinds. say(): what just changed (a mode, a value, a tip): a line in the page's own title, 1.5 s, nothing
 * covered. say_imp() and say_bad(): what matters (a save done, a confirmation asked, an error): the popup at the top */
static char msg[40], info[40];
static int msg_bad;
static uint32_t msg_at, msg_gen, info_at;   /* msg_gen: a new message with the same text redraws too */
/* shown while msg_live: msg_tick ends it after 1.5 s. (The clock wraps every 72 s: a time is never compared with
 * another farther than that; a message is ended long before) */
static uint8_t msg_live, msg_forever, info_live;
COLD static void say_k(const char *s, int bad)
{
    int i = 0; for (; s[i] && i < (int)sizeof msg - 1; i++) msg[i] = s[i]; msg[i] = 0;
    msg_bad = bad; msg_at = time_ticks(); msg_live = 1; msg_gen++; info_live = 0;
}
COLD static void say_imp(const char *s) { say_k(s, 0); }
COLD static void say(const char *s)
{
    int i = 0; for (; s[i] && i < (int)sizeof info - 1; i++) info[i] = s[i]; info[i] = 0;
    info_at = time_ticks(); info_live = 1; msg_gen++;
}
COLD static void msg_tick(void)
{
    if (msg_live && !msg_forever && (uint32_t)(time_ticks() - msg_at) >= 1500000u * lsb) msg_live = 0;
    if (info_live && (uint32_t)(time_ticks() - info_at) >= 1500000u * lsb) { info_live = 0; msg_gen++; }
}
COLD static void say_bad(const char *s) { say_k(s, 1); }
static uint32_t held_btn, locked;           /* locked: a layer kept open (its button's bit) */
static int sticky = PG_TRACKS;              /* the page a tap opened */
/* the visualiser (app/vis.h): on (HOME tapped on TRACKS), its style (kept with the settings), its name shown (passes) */
static uint8_t vis_on, vis_style, vis_named;
/* the start's logo (3.1.6): drawn in for VIS_SPLASH_MS, or until a button, a key or a knob; the first message after it */
static uint8_t splash_on, splash_bad; static uint32_t splash_t0; static const char *splash_say;
static int8_t vis_ring[2 * GB_VIS_RING];
_Static_assert(GB_VIS_RING == VIS_RING, "the visualiser's ring");
static int edit_page, env_page, lfo_page, fx_page;   /* the page of EDIT / ENV / LFO / FX each button shows */
static int layer_used;                      /* the layer held now did something (its release is no tap) */
static uint32_t t_btn_down, t_input;        /* when the layer's button went down; the last touch */
#define LAYER_BITS ((1u << BTN_SEQ) | (1u << BTN_GLO) | (1u << BTN_SAVE) | (1u << BTN_HOME) | (1u << BTN_EDIT) | (1u << BTN_SEL) | (1u << BTN_ARP) | (1u << BTN_FX))
static uint32_t lay(void) { return (held_btn & LAYER_BITS) | locked; }
static int lay_on(int b) { return (int)((lay() >> b) & 1u); }
static int shown_after(void) { return locked || layer_used || (uint32_t)(time_ticks() - t_btn_down) > 350000u * lsb; }
static int group_of(int pg)                 /* the button whose taps go through these pages */
{
    switch (pg) {
    case PG_OSC: case PG_FILTER: case PG_VOICE: case PG_PLAY: case PG_KIT: case PG_DSOUND: case PG_USER: case PG_SND: return BTN_EDIT;
    case PG_FX: case PG_FX2: case PG_FX3: case PG_MASTER: case PG_INS: case PG_INSMIX: case PG_SLC: return BTN_FX;
    case PG_ARP: case PG_ARP2: return BTN_ARP;
    case PG_SEL: case PG_SEL2: return BTN_SEL;
    case PG_ENV: case PG_ENVDEST: return BTN_ENV;
    case PG_LFO: case PG_LFODEST: case PG_LFO3: return BTN_LFO;
    case PG_GLOBAL: case PG_MIDI: case PG_MIDI2: case PG_TEX1: case PG_TEX2: case PG_GREC: return BTN_GLO;   /* (3.3.6: GLOBAL too: SELECT goes through GLO's pages, KNOB 1 is the tempo there) */
    default: return -1;
    }
}
/* a page's four values (SP_*, -1 none) */
static int is_va(int t) { return S.trk[t].patch.engine == ENG_VA; }
COLD static const int8_t *params_of(int pg, int t)
{
    if (pg == PG_INS) return P_INS;
    if (pg == PG_INSMIX) return P_INSMIX;
    if (pg == PG_SLC) return P_SLC;
    if (is_drum(t)) return P_DRUM;
    switch (pg) {
    case PG_OSC: return P_OSC;
    case PG_FILTER: return P_FLT;
    case PG_VOICE: return P_VOI;
    case PG_PLAY: return P_PLY;
    case PG_ENV: return P_ENV;
    case PG_ENVDEST: return P_EDS;
    case PG_LFO: return P_LFO;
    case PG_LFODEST: return P_LFD;
    case PG_LFO3: return P_LF3;
    default: return P_ENV;
    }
}
/* a value as SLOOP shows it: its time, its frequency, its semitones, its choice, a percent */
COLD static char *time_text(char *b, float ms)
{
    if (ms < 1000.0f) return put_s(put_u(b, (uint32_t)(ms + 0.5f)), "ms");
    uint32_t t = (uint32_t)(ms * 0.01f + 0.5f); char *p = put_u(b, t / 10u); *p++ = '.'; p = put_u(p, t % 10u); return put_s(p, "s");
}
COLD static void param_text(char *b, int engine, int pi, float x)
{
    const pdesc *d = pdesc_of(engine, pi);
    gb_patch tmp; tmp.engine = (uint8_t)engine;
    const gb_pd *r = pi < 8 ? &gb_engines[engine < gb_nengines ? engine : 0]->ed[pi] : &gb_sp[pi];
    int v = gb_iv(x, r->min, r->max);
    (void)tmp;
    char *p = b;
    switch (d->fmt) {
    case PF_LIST:
        if (engine == ENG_SLICE && pi == 1 && S.trk[trk].patch.engine == ENG_SLICE) {   /* (3.3.6) a break cut in the editor: its own slices */
            int src = gb_ev(&S.trk[trk].patch, 0);
            if (src > 0 && src < SLC_SRCS && slc_tabs[src].brk) { p = put_s(b, "EDIT"); break; }
        }
        if (engine == ENG_DRUM && pi == 0 && v >= GB_KIT_USR && v < GB_KIT_USR + 4) { p = put_s(b, uk_name(v)); break; }   /* (3.3.6) a slot as a kit: its sample's name */
        p = put_s(b, d->ch && v - r->min < d->n ? d->ch[v - r->min] : "?"); break;
    case PF_PCT: p = put_u(b, (uint32_t)((v - r->min) * 100 / (r->max > r->min ? r->max - r->min : 1))); break;
    case PF_BI: p = put_i(b, v); break;
    case PF_TIME:
        if (pi == SP_GLIDE && !v) { p = put_s(b, "OFF"); break; }
        if ((pi == SP_ATK || pi == SP_DEC || pi == SP_REL) && gb_pv(S.trk[trk].patch.p, SP_ESYNC)) {   /* a note value */
            static const char *const NV[25] = { "0", "1/96", "1/64", "1/48", "1/43", "1/32", "1/24", "1/21", "1/16", "1/12", "1/11", "1/8",
                                                "1/6", "3/16", "1/4", "1/3", "3/8", "1/2", "2/3", "3/4", "1", "1.5", "2", "3", "4" };
            p = put_s(b, NV[v * 25 >> 7]); break;
        }
        p = time_text(b, gb_time_ms((float)v)); break;
    case PF_HZ: {
        if (pi == SP_LRATE && gb_pv(S.trk[trk].patch.p, SP_LSYNC)) { p = put_s(b, N_LSYNC[gb_pv(S.trk[trk].patch.p, SP_LSYNC)]); break; }
        float hz = gb_lfo_hz((float)v); uint32_t t = (uint32_t)(hz * 100.0f + 0.5f);
        if (t < 1000u) { p = put_u(b, t / 100u); *p++ = '.'; p = put_u(p, t % 100u / 10u); p = put_u(p, t % 10u); }
        else p = put_u(b, (t + 50u) / 100u);
        break; }
    case PF_CUT: { float hz = gb_cut_hz((float)v);
        if (hz < 1000.0f) p = put_u(b, (uint32_t)(hz + 0.5f));
        else { uint32_t t = (uint32_t)(hz * 0.01f + 0.5f); p = put_u(b, t / 10u); *p++ = '.'; p = put_u(p, t % 10u); *p++ = 'k'; }
        break; }
    case PF_SEMI: p = put_i(b, v); break;
    case PF_CENT: p = put_s(put_u(b, (uint32_t)v), "ct"); break;
    case PF_DB: { int h = v; if (h < 0) { *p++ = '-'; h = -h; } p = put_u(p, (uint32_t)h / 2u); if (h & 1) { *p++ = '.'; *p++ = '5'; } break; }
    case PF_LVL: { if (!v) { p = put_s(b, "OFF"); break; }
        int h = v - 112; if (h < 0) { *p++ = '-'; h = -h; } else if (h) *p++ = '+';
        p = put_u(p, (uint32_t)h / 2u); if (h & 1) { *p++ = '.'; *p++ = '5'; } p = put_s(p, "dB"); break; }
    case PF_LIST8: p = put_s(b, d->ch && (v >> 3) < d->n ? d->ch[v >> 3] : "?"); break;
    default: p = put_i(b, v); break;
    }
    *p = 0;
}
/* how far a knob's bar goes: the value over its range, 0..100 */
COLD static int param_bar(int engine, int pi, float x) { (void)engine; (void)pi; return (int)(x * 100.0f + 0.5f); }

/* ---------------------------------------------------------------- the stack: what the firmware really used */
extern uint32_t __stack_low[], __stack_top[];
#define STACK_MARK 0x5A5AA5A5u
static void stack_mark(void)                         /* (from main's frame: everything below it, marked) */
{
#ifndef HOST
    volatile uint32_t here = 0; uint32_t *sp = (uint32_t *)(uintptr_t)&here;
    for (uint32_t *w = __stack_low; w < sp - 32; w++) *w = STACK_MARK;
    (void)here;
#endif
}
COLD static uint32_t stack_used_pct(void)
{
#ifdef HOST
    return 0;
#endif
    uint32_t *w = __stack_low;
    while (w < __stack_top && *w == STACK_MARK) w++;
    return (uint32_t)((uintptr_t)__stack_top - (uintptr_t)w) * 100u / (uint32_t)((uintptr_t)__stack_top - (uintptr_t)__stack_low);
}

static uint8_t key_nn[27];                  /* (each key's notes sounding: the panel's part, below) */
/* ---------------------------------------------------------------- the screen: the view */
static uint32_t load_avg, load_peak, load_max, late;   /* load_max: the worst since power-on */
COLD static void knob(ui_view *v, int i, const char *label, const char *val, int bar, int hot)
{
    ui_knob *k = &v->knob[i];
    k->label = label; k->hot = (uint8_t)hot; k->dial = 1; k->bar = (int8_t)(bar < -1 ? -1 : bar > 100 ? 100 : bar);
    int j = 0; for (; val[j] && j < (int)sizeof k->value - 1; j++) k->value[j] = val[j];
    k->value[j] = 0;
}
COLD static void knob_n(ui_view *v, int i, const char *label, int val, int bar) { char b[12]; *put_u(b, (uint32_t)(val < 0 ? 0 : val)) = 0; knob(v, i, label, b, bar, 0); }
COLD static void tile(ui_view *v, int i, const char *text, int style, int flags, uint16_t col)
{
    ui_tile *t = &v->tile[i];
    int j = 0; for (; text[j] && j < (int)sizeof t->text - 1; j++) t->text[j] = text[j];
    t->text[j] = 0; t->style = (uint8_t)style; t->flags = (uint8_t)flags; t->col = col;
}
static int punch_key = -1;                  /* the key whose punch-in is held (FX + key), -1 none */
static uint32_t loop_said;                                /* (the take display's throttle) */
static void loop_append(int trk, int note, int vel, int down, uint32_t lat);   /* the capture append, below (t_rec_down) */
static uint8_t bars_pick[NTRK];                           /* each empty track's BARS for its next take: 0 FREE, or 1, 2, 4, 8 */
static int punch_of_key(int k) { static const int8_t BLK[27] = { 0, 16, 0, 17, 0, 18, 0, 0, 19, 0, 20, 0, 0, 21, 0, 22, 0, 23, 0, 0, 24, 0, 25, 0, 0, 26, 0 };
                                 return white_of[k] >= 0 ? white_of[k] : BLK[k]; }
COLD static void bip_text(char *b, float x)               /* a one-knob filter: LP 42, OFF, HP 30 */
{
    int v = (int)(x * 100.0f + (x >= 0.0f ? 0.5f : -0.5f));
    if (!v) { put_s(b, "OFF")[0] = 0; return; }
    *put_u(put_s(b, v < 0 ? "LP " : "HP "), (uint32_t)(v < 0 ? -v : v)) = 0;
}
COLD static void fx_knobs(ui_view *v)                     /* FX held, FX 2/2: the master bus and the track's filter */
{
    char b[12];
    bip_text(b, S.mfilt.tgt); knob(v, 0, "FILT", b, (int)(50.0f + 50.0f * S.mfilt.tgt), 0);
    knob_n(v, 1, "DUST", (int)(S.dust * 100.0f + 0.5f), (int)(S.dust * 100.0f + 0.5f));
    knob_n(v, 2, "DUCK", (int)(S.duck * 100.0f + 0.5f), (int)(S.duck * 100.0f + 0.5f));
    bip_text(b, S.trk[trk].filt.tgt); knob(v, 3, "FILT P", b, (int)(50.0f + 50.0f * S.trk[trk].filt.tgt), 0);
    v->mfilt = (int8_t)(S.mfilt.tgt * 100.0f + (S.mfilt.tgt >= 0.0f ? 0.5f : -0.5f));
    v->tfilt = (int8_t)(S.trk[trk].filt.tgt * 100.0f + (S.trk[trk].filt.tgt >= 0.0f ? 0.5f : -0.5f));
    v->dust = (uint8_t)(S.dust * 100.0f + 0.5f); v->duck = (uint8_t)(S.duck * 100.0f + 0.5f);
}
COLD static int level_of(int t) { float l = S.trk[t].level.tgt; return l <= 0.0f ? 0 : l >= 1.0f ? 100 : (int)(l * 100.0f + 0.5f); }
COLD static void step_tiles(ui_view *v)                  /* the 16 steps of the page, as tiles */
{
    int t = trk, at = Q.playing ? (int)seq_step_of(&Q, t) : -1, steps = Q.t[t].pat[Q.t[t].cur].steps, dr = is_drum(t);
    for (int i = 0; i < 16; i++) {
        int st = page * 16 + i;
        char b[8];
        if (st >= steps) { tile(v, i, "", T_NONE, 0, 0); continue; }
        int c, r, nl = seq_step_info(&Q, t, st, &c, &r), note = 0, vel = 0, nud = 0, has = seq_step_get(&Q, t, st, &note, &vel, &nud);
        int mine = dr ? seq_step_has(&Q, t, st, last_note) : has;
        *put_u(b, (uint32_t)st + 1) = 0;
        if (has && !dr) note_name(b, t, note);
        int style = mine ? T_ON : has ? T_DIM : T_EMPTY, flags = 0;
        if (st == at) flags |= TF_PLAY;
        if (has && (nl || nud)) flags |= TF_DOT;
        if (has && c == SEQ_FILL) flags |= TF_FILL;
        else if (has && c != SEQ_ALWAYS) flags |= TF_NOFILL;
        if (has && r > 1) flags |= TF_BAR;
        if (st == hold_step || (hold_more && page == hold_page && ((hold_more >> i) & 1u))) style = T_WHITE;
        tile(v, i, b, style, flags, TRK_COL[t]);
    }
}
/* ---------------------------------------------------------------- NOTES (SEQ tapped twice, a synth track), as SLOOP's:
 * the pattern as a piano roll and one step edited. The keys write the step edited (the keys held together: a chord,
 * POLY; one note otherwise), and when they are all let go it moves on to the next step. KNOB 1 goes note by note (a
 * chord's notes, then the next step, the empty ones too), 2 its pitch, 3 its length in steps, 4 its force (GHOST,
 * SOFT, NORM, HARD, ACC); PRESETS the step's SLIDE; EDIT tapped deletes the note */
static int ne_step, ne_sub, ne_n, ne_wrote;
/* NOTES as SLOOP 2.8's: PRESETS the zoom (8 16 32 64 steps shown); OCT+ held the note's sheet (SLIDE NUDGE FILL MOVE
 * COPY DELETE: KNOB 1 the row, KNOB 2 its value, OCT+ tapped does it, OCT- closes); OCT+ tapped as before */
static const uint8_t NE_ZOOM[4] = { 8, 16, 32, 64 };
static int ne_zoom = 1, ne_sheet, ne_row = 3;
static uint32_t t_octup;
static const char *const NE_ROW[6] = { "SLIDE", "NUDGE", "FILL", "MOVE", "COPY", "DELETE" };
static uint8_t ne_buf[4];
static int ev_step(const seq_pat *p, uint32_t tick)          /* (as the sequencer: the nearest step) */
{
    uint32_t st = (tick + SEQ_TPS / 2) / SEQ_TPS;
    if (st < p->steps) return (int)st;
    return st == p->steps && tick < (uint32_t)p->steps * SEQ_TPS ? 0 : -1;
}
COLD static int ne_count(int st) { const seq_pat *p = &Q.t[trk].pat[Q.t[trk].cur]; int n = 0; for (int i = 0; i < p->n; i++) if (ev_step(p, p->ev[i].tick) == st) n++; return n; }
COLD static int ne_sel(void)                                      /* the note edited: its index in the pattern, -1 none */
{
    const seq_pat *p = &Q.t[trk].pat[Q.t[trk].cur]; int n = 0;
    for (int i = 0; i < p->n; i++) if (ev_step(p, p->ev[i].tick) == ne_step && n++ == ne_sub) return i;
    return -1;
}
COLD static void ne_follow(int at)                             /* the note edited: event at, wherever it went */
{
    const seq_pat *p = &Q.t[trk].pat[Q.t[trk].cur];
    if (at < 0 || at >= p->n) return;
    ne_step = ev_step(p, p->ev[at].tick); ne_sub = 0;
    if (ne_step < 0) ne_step = 0;
    for (int i = 0; i < at; i++) if (ev_step(p, p->ev[i].tick) == ne_step) ne_sub++;
}
COLD static void ne_sheet_do(void)                              /* OCT+ tapped in the sheet: the row done */
{
    int sel = ne_sel();
    if (sel < 0) { say_bad("NO NOTE HERE"); return; }
    Q.batch = 0;
    if (ne_row == 0) { int sl = seq_step_flags(&Q, trk, ne_step) & SEQ_SLIDE; seq_step_flag(&Q, trk, ne_step, SEQ_SLIDE, !sl); say(sl ? "SLIDE OFF" : "SLIDE ON: GLIDES TO THE NEXT"); }
    else if (ne_row == 2) {
        int c = 0; seq_step_info(&Q, trk, ne_step, &c, 0);
        int n = c == SEQ_ALWAYS ? SEQ_FILL : c == SEQ_FILL ? SEQ_NOFILL : SEQ_ALWAYS;
        seq_step_cond(&Q, trk, ne_step, n); say(n == SEQ_FILL ? "FILL: ONLY IN A FILL" : n == SEQ_NOFILL ? "NO FILL: NOT IN A FILL" : "NORMAL");
    } else if (ne_row == 4) {
        int at = seq_ev_move(&Q, trk, sel, 0, 1);
        if (at < 0) { say_bad("NO ROOM AFTER IT"); return; }
        ne_follow(at); ne_row = 3; say("COPIED · KNOB 2 MOVES IT");
    } else if (ne_row == 5) { seq_ev_del(&Q, trk, sel); if (ne_sub > 0) ne_sub--; ne_sheet = 0; say("NOTE DELETED"); }
}
COLD static int notes_on(void) { return sticky == PG_NOTES && !is_drum(trk) && hold_step < 0 && !Q.recording && !recording_armed && !countin; }
COLD static int force_of(int vel, int acc) { return acc ? 4 : vel < 50 ? 0 : vel < 85 ? 1 : vel < 115 ? 2 : 3; }
static const char *const FORCE[5] = { "GHOST", "SOFT", "NORM", "HARD", "ACC" };
/* ---------------------------------------------------------------- HOME: the menu's sections, the calibration */
static const char *const HOME_SEC[5] = { "PROJECT", "SCREEN 1/4", "LIGHTS 2/4", "AUDIO 3/4", "SEQ · SYSTEM 4/4" };
static const int8_t HOME_ROW[5][4] = { { -1, -1, -1, -1 }, { -4, HS_ACCEL, -1, -1 }, { HS_LIGHTS, HS_KEYS, HS_NOTES, -1 },
                                       { HS_LOWCUT, HS_USB, HS_RATE, HS_RESO }, { HS_DPREV, HS_CORE2, -2, -3 } };
static const char *const HS_LABEL[HS_N] = { "ACCEL", "LIGHTS", "KEYS", "NOTES", "SPEAKER", "USB", "RATE", "RESO", "PREVIEW", "CORE 2" };
COLD static const char *home_value(int i, char *b)
{
    static const char *const LV[4] = { "OFF", "LOW", "MID", "HIGH" }, *const KY[4] = { "OFF", "C KEYS", "WHITE", "ALL" };
    static const char *const RT[3] = { "AUTO", "44.1k", "48k" };
    switch (i) {
    case HS_LIGHTS: return LV[hs[i] & 3];
    case HS_KEYS: return KY[hs[i] & 3];
    case HS_USB: return hs[i] ? "FULL" : "MASTER";
    case HS_RATE: return RT[hs[i] % 3];
    case HS_RESO: if (!hs[i]) return "OFF"; *put_u(b, hs[i]) = 0; return b;
    default: return hs[i] ? "ON" : "OFF";
    }
}
static const char *const CAL_BTN[14] = { "OCT-", "OCT+", "FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC" };
static const char *const CAL_ENC[7] = { "SELECT", "ALGORITHM", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4", "PRESETS" };
static uint32_t cal_prev;
COLD static void cal_start(void)
{
    static const uint8_t B[14] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 }, E[7] = { 0, 1, 2, 3, 4, 5, 6 };
    static const int8_t D[7] = { 1, 1, 1, 1, 1, 1, 1 };
    panel_map(B, E, D);                                    /* (the matrix as it is, while it is asked) */
    cal_step = 0; cal_prev = panel_raw_buttons();
    for (int e = 0; e < 7; e++) panel_raw_enc_take(e);
    say_imp("CALIBRATION: PRESS EACH BUTTON NAMED");
}
/* one round of the calibration: the button asked for, then each encoder turned clockwise */
COLD static void cal_tick(void)
{
    uint32_t r = panel_raw_buttons(), np = r & ~cal_prev;
    cal_prev = r;
    if (cal_step < 14) {
        if (!np) return;
        int m = 0; while (!((np >> m) & 1u)) m++;
        for (int i = 0; i < cal_step; i++) if (cal_btn[i] == m) { say_bad("TAKEN ALREADY: ANOTHER"); return; }
        cal_btn[cal_step++] = (uint8_t)m;
        if (cal_step == 14) for (int e = 0; e < 7; e++) panel_raw_enc_take(e);
        return;
    }
    for (int e = 0; e < 7; e++) {
        int d = panel_raw_enc_take(e);
        if (!d) continue;
        int k = cal_step - 14;
        for (int i = 0; i < k; i++) if (cal_enc[i] == e) { say_bad("TAKEN ALREADY: ANOTHER"); return; }
        cal_enc[k] = (uint8_t)e; cal_dir[k] = (int8_t)(d > 0 ? 1 : -1);
        if (++cal_step == 21) {
            if (!panel_map(cal_btn, cal_enc, cal_dir)) { cal_ok = 1; say_imp("CALIBRATION DONE"); } else say_bad("CALIBRATION FAILED");
            cal_step = -1; t_input = time_ticks();
        }
        return;
    }
}
COLD static void home_knob(int sec, int kn, int d)
{
    int i = HOME_ROW[sec][kn];
    if (i == -2) {                                       /* LEARN CLEAR */
        if (d <= 0) return;
        int n = ml_count(); for (int j = 0; j < ML_N; j++) ml_tab[j] = 0;
        char b[32], *p = put_s(b, "MIDI LEARN: "); if (n) { p = put_u(p, (uint32_t)n); p = put_s(p, " CCS FORGOTTEN"); } else p = put_s(p, "NO CC LEARNT"); *p = 0; say(b);
        t_input = time_ticks(); return;
    }
    if (i == -3) { if (d > 0) cal_start(); return; }
    if (i == -4) {                                       /* COLOR: the palettes, then your theme */
        int n = UI_NTHEMES + (theme_ok() ? 1 : 0), c = scr_color + d;
        scr_color = (uint8_t)(c < 0 ? 0 : c > n - 1 ? n - 1 : c); color_apply(); return;
    }
    if (i < 0) return;
    int v = hs[i] + d; hs[i] = (uint8_t)(v < 0 ? 0 : v > HS_MAX[i] ? HS_MAX[i] : v);
    home_apply();
    if (i == HS_RATE) say(hs[i] ? "USB AUDIO: THIS RATE ONLY" : "USB AUDIO: THE COMPUTER'S RATE");
}
COLD static void sb_view(ui_view *v)                    /* SOUNDS: the category, its page, the 16 sounds as tiles */
{
    char b[12], *p;
    int cur = preset_origin(preset_of[trk]), np = (sb_n + 15) / 16, drum = is_drum(trk), isfav = 0;
    v->page = UI_LAYER; v->title_col = TRK_COL[trk]; v->cols = 0;
    put_s(v->title, sb_cat_name(sb_cat))[0] = 0;
    p = put_u(b, (uint32_t)sb_pg + 1u); *p++ = '/'; p = put_u(p, (uint32_t)(np ? np : 1)); *p = 0;
    p = put_s(v->sub, b);
    if (cur >= 0) { p = put_s(p, " · "); p = put_s(p, snd_name(cur)); if (preset_of[trk] < -1) p = put_s(p, " *"); }
    p = put_s(p, sb_back < 0 ? " · KEYS PICK" : " · EDIT: KEYS"); *p = 0;
    for (int j = 0; j < US_NFAV; j++) isfav |= cur >= 0 && us_fav[j] == cur;
    for (int i = 0; i < 16; i++) {
        int f = sb_o[i], fav = 0;
        if (f < 0) {                                     /* a place with nothing: its number (your favourites, your places) */
            char e[4]; *put_u(e, (uint32_t)(sb_pg * 16 + i + 1)) = 0;
            if (sb_cat == SB_UKIT && i < 8) { tile(v, i, gb_dkit_names[uk_at(i)], T_EMPTY, 0, 0); continue; }   /* (an empty slot: its name, dim) */
            tile(v, i, sb_pg * 16 + i < sb_n && (sb_cat == SB_FAV || sb_cat == SB_USER || sb_cat == SB_BANK) ? e : "", sb_pg * 16 + i < sb_n ? T_EMPTY : T_NONE, 0, 0);
            continue;
        }
        if (sb_cat != SB_FAV && !drum) for (int j = 0; j < US_NFAV; j++) fav |= us_fav[j] == f;
        tile(v, i, snd_name(f), f == cur ? T_WHITE : T_DIM, TF_WRAP | (fav ? TF_DOT : 0), TRK_COL[trk]);
    }
    knob(v, 0, "CAT", sb_cat_name(sb_cat), -1, 0);
    knob(v, 1, "PAGE", b, -1, 0);
    if (sb_cat == SB_FAV && isfav) knob(v, 2, "MOVE", "<>", -1, 0); else v->knob[2].label = 0;
    if (drum) v->knob[3].label = 0; else knob(v, 3, "FAV", isfav ? "REMOVE" : "ADD", -1, 0);
    for (int i = 0; i < 4; i++) v->knob[i].dial = 0;
}
/* (3.3.6) the battery: ADC channel 3 through the FM-1's divider, 147.7 a volt (M-VAVE's own three bars sit at 531 /
 * 561 / 591: 3.6 / 3.8 / 4.0 V), smoothed; then the charge a Li-ion cell holds at that voltage (at rest: playing, it
 * reads a few % low), shown with 2 % of hysteresis */
static int32_t batt_raw; static int8_t batt_pct = -1;
COLD static int batt_of(int32_t raw)
{
    static const uint16_t MV[12] = { 3300, 3400, 3500, 3600, 3650, 3700, 3750, 3800, 3900, 4000, 4100, 4200 };
    static const uint8_t PC[12] = { 0, 2, 5, 12, 20, 30, 40, 50, 65, 80, 90, 100 };
    int32_t mv = raw * 677 / 100;
    if (mv <= MV[0]) return 0;
    for (int i = 1; i < 12; i++) if (mv < MV[i]) return PC[i - 1] + (PC[i] - PC[i - 1]) * (mv - MV[i - 1]) / (MV[i] - MV[i - 1]);
    return 100;
}
COLD static void batt_poll(void)
{
    int32_t b = adc_read(ADC_CH_BATT);
    if (b <= 0) return;
    batt_raw = batt_raw ? batt_raw + (b - batt_raw) / 8 : b;
    int p = batt_of(batt_raw);
    if (batt_pct < 0 || p > batt_pct + 1 || p < batt_pct - 1) batt_pct = (int8_t)p;
}
/* a played track: a note of its pattern is off the steps (a take, or a pattern recorded by hand). Worked out from the
 * notes each time: nothing is stored, so it is the same after a save, a load, an undo */
COLD static int trk_played(int t)
{
    const seq_pat *p = &Q.t[t].pat[Q.t[t].cur];
    for (int i = 0; i < p->n; i++) if (p->ev[i].tick % SEQ_TPS) return 1;
    return 0;
}
COLD static void bars_text(char *b, int t)                 /* an empty track's BARS: FREE, or the bars chosen */
{
    if (bars_pick[t]) *put_u(b, bars_pick[t]) = 0; else put_s(b, "FREE")[0] = 0;
}
COLD static void fill_view(ui_view *v)
{
    if (splash_on) { v->page = UI_SPLASH; return; }
    uint32_t now = time_ticks();
    char b[40], *p;
    v->ntrk = NTRK; v->trk = POS[trk];
    v->blink = (uint8_t)((now / (250000u * lsb)) & 1u);
    v->batt = batt_pct < 0 ? 255u : (uint8_t)batt_pct;
    v->batt_usb = usb_midi_ready() ? (uint8_t)(1u + ((now / (250000u * lsb)) & 7u)) : 0u;   /* (a computer: it powers and charges) */
    v->bpm = (uint16_t)(Q.bpm + 0.5f);
    v->bar = (uint16_t)(Q.tick / SEQ_BAR + 1u); v->beat = (uint8_t)(Q.tick / (4 * SEQ_TPS) % 4u + 1u);
    v->playing = (uint8_t)Q.playing; v->recording = (uint8_t)Q.recording; v->armed = (uint8_t)recording_armed;
    /* the status: REC, the song, the pattern and the one waiting for the bar */
    const seq_track *k = &Q.t[trk];
    if (ltake.armed_trk >= 0) { put_s(v->status, "TAKE")[0] = 0; v->status_col = RGB(255, 80, 80); }
    else if (Q.recording) { put_s(v->status, "REC")[0] = 0; v->status_col = RGB(255, 80, 80); }
    else if (Q.song_rec) { put_s(v->status, "SONG REC")[0] = 0; v->status_col = RGB(255, 80, 80); }
    else if (Q.song_play && Q.playing) { p = put_s(v->status, "S "); p = put_u(p, seq_song_bar(&Q) + 1u); *p++ = '/'; p = put_u(p, Q.song_len); *p = 0; v->status_col = RGB(80, 220, 255); }
    else {
        p = v->status; *p++ = 'P'; p = put_u(p, k->cur + 1u);
        if (k->next != SEQ_NONE) { p = put_s(p, "→"); p = put_u(p, k->next + 1u); v->status_col = v->blink ? RGB(255, 210, 60) : RGB(255, 255, 255); }
        *p = 0;
    }
    /* each track */
    for (int t = 0; t < NTRK; t++) {                     /* (each at its place on the screen: POS) */
        const seq_track *q = &Q.t[t];
        int s = POS[t];
        int steps = q->pat[q->cur].steps, at = Q.playing ? (int)seq_step_of(&Q, t) : -1, first = at >= 0 ? at / 16 * 16 : 0;
        int po = preset_origin(preset_of[t]);
        v->col[s] = TRK_COL[t];
        v->sound[s] = po >= 0 ? snd_name(po) : 0; v->edited[s] = (uint8_t)(preset_of[t] < -1);
        v->kind[s] = gb_edesc[S.trk[t].patch.engine < ENG_N ? S.trk[t].patch.engine : 0].name;
        v->mute[s] = q->mute; v->solo[s] = solo[t]; v->rec[s] = (uint8_t)(Q.recording && Q.rec_trk == t);
        v->level[s] = (uint8_t)level_of(t);
        for (int i = 0; i < 16; i++) {
            int st = first + i; uint8_t f = 0;
            if (st >= steps) f = UC_OUT;
            else {
                if (seq_step_has(&Q, t, st, -1)) f |= UC_NOTE;
                int c, r, nl = seq_step_info(&Q, t, st, &c, &r);
                if (nl) f |= UC_LOCK;
                if (c != SEQ_ALWAYS) f |= UC_COND;
                if (r > 1) f |= UC_RAT;
                if (st == at) f |= UC_PLAY;
            }
            v->cells[s][i] = f;
        }
    }
    /* the sound */
    const gb_patch *q = &S.trk[trk].patch;
    v->engine = (uint8_t)(q->engine == ENG_DRUM ? 1 : 0);
    for (int i = 0; i < GB_NPARAM; i++) v->prm[i] = (uint8_t)(base[trk][i] * 100.0f + 0.5f);
    v->dly_beats8 = (uint8_t)(DIV[dly_div].b24 / 3); v->dly_fb = (uint8_t)(S.dly_fb * 100.0f + 0.5f); v->rev_size = (uint8_t)(S.rev_fb * 100.0f + 0.5f);
    v->send_dly = (uint8_t)(S.trk[trk].send_dly.tgt * 100.0f + 0.5f); v->send_rev = (uint8_t)(S.trk[trk].send_rev.tgt * 100.0f + 0.5f);
    v->pad_names = PAD; v->pad_hit = (int8_t)(is_drum(trk) ? last_note & 15 : -1);
    v->pad_mute = is_drum(trk) ? S.trk[trk].patch.dmute : 0;
    /* a layer (SEQ at once, the others after 0.35 s or once used), else the page */
    uint32_t L = lay();
    int shown = shown_after();
    int layer = (hold_step >= 0 || (L & (1u << BTN_SEQ))) ? BTN_SEQ : !shown ? -1 :
                (L & (1u << BTN_GLO)) ? BTN_GLO : (L & (1u << BTN_SAVE)) ? BTN_SAVE : (L & (1u << BTN_HOME)) ? BTN_HOME :
                (L & (1u << BTN_EDIT)) ? BTN_EDIT : (L & (1u << BTN_SEL)) ? BTN_SEL : (L & (1u << BTN_ARP)) ? BTN_ARP :
                (L & (1u << BTN_FX)) ? BTN_FX : -1;
    int pg = sticky, sel_page = 0;
    int rec_screen = (recording_armed || countin) && !Q.playing && !ltake.rec && layer < 0;   /* REC armed: its screen */
    if ((pg == PG_DSOUND || pg == PG_KIT) && !is_drum(trk)) pg = PG_OSC;   /* (a synth track: its own pages) */
    if (layer < 0 && pg == PG_STEPS) layer = BTN_SEQ;                 /* (these pages are tiles too) */
    if (layer < 0 && pg == PG_SONG) layer = BTN_SAVE;
    if (layer < 0 && pg == PG_SEL) { layer = BTN_SEL; sel_page = 1; }
    if (rescue) {                                           /* RESCUE (OCT- at power-on): what each key does */
        v->page = UI_LAYER; v->title_col = RGB(255, 80, 80);
        put_s(v->title, "RESCUE")[0] = 0; put_s(v->sub, "SAFE START · YOUR PROJECT IS KEPT")[0] = 0;
        for (int i = 0; i < 16; i++) tile(v, i, "", T_NONE, 0, 0);
        tile(v, 0, "PLAY", T_ON, 0, RGB(80, 240, 120)); tile(v, 1, "START", T_DIM, 0, 0); tile(v, 2, "EMPTY", T_DIM, 0, 0);
        tile(v, 4, "REC", T_RED, 0, 0); tile(v, 5, "USB", T_DIM, 0, 0); tile(v, 6, "DOWNLD", T_DIM, 0, 0); tile(v, 7, "MODE", T_DIM, 0, 0);
        tile(v, 8, "OFF,", T_FRAME, 0, RGB(255, 210, 60)); tile(v, 9, "PLAY", T_FRAME, 0, RGB(255, 210, 60)); tile(v, 10, "ON:", T_FRAME, 0, RGB(255, 210, 60)); tile(v, 11, "MIDI", T_FRAME, 0, RGB(255, 210, 60));
        for (int i = 0; i < 4; i++) v->knob[i].label = 0;
        return;
    }
    if (cal_step >= 0) {                                    /* the calibration: what to press or turn */
        v->page = UI_GENERIC;
        p = put_s(v->page_name, "CALIBRATE "); p = put_u(p, (uint32_t)cal_step + 1u); p = put_s(p, "/21"); *p = 0;
        knob(v, 0, cal_step < 14 ? "PRESS" : "TURN →", cal_step < 14 ? CAL_BTN[cal_step] : CAL_ENC[cal_step - 14], -1, 1);
        for (int i = 1; i < 4; i++) v->knob[i].label = 0;
        v->knob[0].dial = 0;
    } else if (rec_screen) {                                /* REC: MODE, LONG, START, QUANT */
        v->page = UI_GENERIC;
        if (countin) { p = put_s(v->page_name, "COUNT-IN "); p = put_u(p, 4u - cin_ticks / (4 * SEQ_TPS)); *p = 0; }
        else put_s(v->page_name, "REC READY")[0] = 0;
        int empty = project_empty(), spb = seq_steps_per_bar(&Q, trk), st = k->pat[k->cur].steps;
        knob(v, 0, "MODE", empty ? (rec_mode ? "TEMPO" : "FREE") : "TEMPO", empty ? rec_mode * 100 : 100, 0);
        if (ltake.armed_trk >= 0) { bars_text(b, ltake.armed_trk); knob(v, 1, "BARS", b, bars_pick[ltake.armed_trk] * 100 / 8, 0); }   /* a take: the bars chosen */
        else if (empty && !rec_mode) knob(v, 1, "BARS", "FREE", -1, 0);
        else { if (st % spb == 0) p = put_u(b, (uint32_t)(st / spb)); else { p = put_u(b, (uint32_t)st); *p++ = 'P'; } *p = 0;
               knob(v, 1, "BARS", b, st * 100 / SEQ_MAXSTEPS, 0); }
        knob(v, 2, "START", rec_count ? "CLICK" : "NOTE", rec_count * 100, 0);
        int qv = ltake.armed_trk >= 0 ? 0 : k->quant; if (qv == 100) put_s(b, "ON")[0] = 0; else if (!qv) put_s(b, "OFF")[0] = 0; else { *put_s(put_u(b, (uint32_t)qv), "%") = 0; }
        knob(v, 3, "QUANT", b, qv, 0);
    } else if (layer < 0 && pg == PG_SONG2) {              /* SONG 2/2: the song's entries (as SLOOP's SONG screen) */
        v->page = UI_SONG; v->title_col = RGB(80, 220, 255);
        put_s(v->title, "SONG")[0] = 0;
        p = put_s(v->sub, Q.song_rec ? "2/2 · SONG REC" : Q.song_play ? "2/2 · SONG" : "2/2 · LOOP");
        p = put_s(p, scope_one ? " · THIS TRACK" : " · ALL"); *p = 0;
        int n = Q.song_n;
        if (sg_cur >= n) sg_cur = n ? n - 1 : 0;
        int first = sg_cur > 2 ? sg_cur - 2 : 0, at = seq_song_at(&Q);
        if (first + 5 > n) first = n > 5 ? n - 5 : 0;
        v->sg_n = 0;
        for (int i = first; i < n && v->sg_n < 5; i++) {
            ui_songrow *r = &v->sg[v->sg_n++];
            const seq_song_ev *e = &Q.song[i];
            r->num = (uint8_t)(i + 1); r->pat = e->pat[trk]; r->bars = (uint16_t)seq_song_len_of(&Q, i); r->flags = 0; r->at = 0;
            if (i == sg_cur) r->flags |= SR_SEL;
            for (int t = 0; t < NTRK; t++) if (e->pat[t] != e->pat[trk]) r->flags |= SR_MIX;
            if (!k->pat[e->pat[trk]].n) r->flags |= SR_EMPTY;
            if ((e->mute >> trk) & 1u) r->flags |= SR_MUTE;
            if (i == at) { r->flags |= SR_PLAY; r->at = (uint8_t)(seq_song_bar(&Q) - e->bar); }
        }
        if (n) { p = put_u(b, (uint32_t)sg_cur + 1u); *p++ = '/'; p = put_u(p, (uint32_t)n); *p = 0; } else put_s(b, "-")[0] = 0;
        knob(v, 0, "LINE", b, n > 1 ? sg_cur * 100 / (n - 1) : 0, 0);
        if (n) { const seq_song_ev *e = &Q.song[sg_cur]; *put_u(put_s(b, "P"), (uint32_t)e->pat[trk] + 1u) = 0;
                 knob(v, 1, "PAT", b, e->pat[trk] * 100 / (SEQ_NPAT - 1), 0);
                 int bl = seq_song_len_of(&Q, sg_cur); *put_u(b, (uint32_t)bl) = 0; knob(v, 2, "BARS", b, (bl > 64 ? 64 : bl) * 100 / 64, 0); }
        else { v->knob[1].label = 0; v->knob[2].label = 0; }
        *put_u(b, (uint32_t)n) = 0; knob(v, 3, "LINES", b, n * 100 / SEQ_SONG, 0);
    } else if (layer < 0 && pg == PG_SND) {               /* SOUNDS (3.2): tiles, as a layer */
        sb_view(v);
    } else if (layer >= 0) {
        v->page = UI_LAYER; v->title_col = 0;
        for (int i = 0; i < 16; i++) tile(v, i, "", T_NONE, 0, 0);
        if (layer == BTN_FX) {                                  /* PUNCH: the 27 keys, the master bus */
            v->page = UI_PUNCH; v->title_col = RGB(255, 150, 40);
            put_s(v->title, "FX")[0] = 0;
            p = put_s(v->sub, "FADE "); p = put_u(p, S.pk_fade);
            if (held_btn & ((1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP))) {
                p = put_s(p, " · THROW");
                if (held_btn & (1u << BTN_OCT_DOWN)) p = put_s(p, " REV");
                if (held_btn & (1u << BTN_OCT_UP)) p = put_s(p, " DLY");
            } else p = put_s(p, " · OCT- REV · OCT+ DLY");
            *p = 0;
            int on = punch_key >= 0 ? punch_of_key(punch_key) : -1;
            for (int i = 0; i < 16; i++) tile(v, i, PK_NAME[i], i == on ? T_WHITE : T_DIM, 0, 0);
            for (int j = 0; j < 11; j++) { ui_tile *t = &v->btile[j]; const char *nm = PK_NAME[16 + j]; int c = 0; for (; nm[c] && c < 9; c++) t->text[c] = nm[c]; t->text[c] = 0; t->style = (uint8_t)(16 + j == on ? T_WHITE : T_DIM); }
            fx_knobs(v);
        } else if (layer == BTN_SEQ && hold_step >= 0) {
            int note = 0, vel = 0, nud = 0, c, r;
            seq_step_get(&Q, trk, hold_step, &note, &vel, &nud);
            seq_step_info(&Q, trk, hold_step, &c, &r);
            p = put_s(v->title, "STEP "); p = put_u(p, (uint32_t)hold_step + 1);
            if (hold_more) { int m = 0; for (int i = 0; i < 16; i++) m += (hold_more >> i) & 1; *p++ = '+'; p = put_u(p, (uint32_t)m); }
            *p = 0; v->title_col = TRK_COL[trk];
            int lv = seq_lock_get(&Q, trk, hold_step, lock_param);
            p = put_s(v->sub, "LOCK "); p = put_s(p, lock_name(is_drum(trk) && lock_param < SEQ_LK_LEVEL ? lock_param & 3 : lock_param)); *p++ = ' ';
            if (lv >= 0) p = put_u(p, (uint32_t)(lv * 100 / 255)); else p = put_s(p, "--");
            p = put_s(p, " · "); p = cond_name(p, c); *p = 0;
            step_tiles(v);
            note_name(b, trk, note); knob(v, 0, is_drum(trk) ? "SOUND" : "NOTE", b, note * 100 / 127, 0);
            int fl = seq_step_flags(&Q, trk, hold_step);
            if (fl & SEQ_ACCENT) knob(v, 1, is_drum(trk) ? "LEVEL" : "VEL", "ACC", 100, 1);
            else if (is_drum(trk)) { const char *LV = vel < 50 ? "GHOST" : vel < 85 ? "SOFT" : vel < 115 ? "NORM" : "HARD"; knob(v, 1, "LEVEL", LV, vel * 100 / 127, 0); }
            else knob_n(v, 1, "VEL", vel, vel * 100 / 127);
            if (fl & SEQ_SLIDE) knob(v, 2, "RATCH", "SLIDE", 0, 1);
            else { p = put_s(b, "X"); p = put_u(p, (uint32_t)r); *p = 0; knob(v, 2, "RATCH", b, (r - 1) * 33, 0); }
            int n64 = nud * 64 / SEQ_TPS; *put_i(b, n64) = 0; knob(v, 3, "NUDGE", b, 50 + n64 * 50 / 32, 0);
        } else if (layer == BTN_SEQ && !(L & (1u << BTN_SEQ)) && is_drum(trk)) {   /* DRUMS: the kit's grid */
            v->page = UI_DRUMS; v->drum_sel = (uint8_t)(last_note & 15);
            int steps = k->pat[k->cur].steps, at = Q.playing ? (int)seq_step_of(&Q, trk) - page * 16 : -1;
            v->drum_at = (int8_t)(at >= 0 && at < 16 ? at : -1);
            for (int r = 0; r < 16; r++) { uint16_t m = 0; for (int i = 0; i < 16; i++) if (page * 16 + i < steps && seq_step_has(&Q, trk, page * 16 + i, r)) m |= (uint16_t)(1u << i); v->drum_row[r] = m; }
            knob(v, 0, "SOUND", PAD[last_note & 15], (last_note & 15) * 100 / 15, 0);
            knob_n(v, 1, "QUANT", k->quant, k->quant);
            knob_n(v, 2, "GATE", k->gate, k->gate / 2);
            knob_n(v, 3, "STEPS", k->pat[k->cur].steps, k->pat[k->cur].steps * 100 / SEQ_MAXSTEPS);
        } else if (layer == BTN_SEQ) {
            put_s(v->title, "STEPS")[0] = 0; v->title_col = TRK_COL[trk];
            int pages = (k->pat[k->cur].steps + 15) / 16;
            p = put_s(v->sub, "TRACK "); p = put_u(p, (uint32_t)POS[trk] + 1); p = put_s(p, " · ");
            if (is_drum(trk)) p = put_s(p, PAD[last_note & 15]); else { p = put_u(p, (uint32_t)page + 1); *p++ = '/'; p = put_u(p, (uint32_t)pages); }
            *p = 0;
            step_tiles(v);
            note_name(b, trk, last_note); knob(v, 0, is_drum(trk) ? "SOUND" : "NOTE", b, is_drum(trk) ? (last_note & 15) * 100 / 15 : last_note * 100 / 127, 0);
            if (L & (1u << BTN_SEQ)) {                          /* held: as SLOOP's SEQ + KNOB 1-4 */
                knob(v, 1, "DIV", seq_div_name[k->div], k->div * 100 / (SEQ_NDIV - 1), 0);
                knob_n(v, 2, "SWING", k->swing, k->swing);
            } else { knob_n(v, 1, "QUANT", k->quant, k->quant); knob_n(v, 2, "GATE", k->gate, k->gate / 2); }
            knob_n(v, 3, "STEPS", k->pat[k->cur].steps, k->pat[k->cur].steps * 100 / SEQ_MAXSTEPS);
        } else if (layer == BTN_GLO) {
            put_s(v->title, "MIX")[0] = 0;
            put_s(v->sub, "1-5 MUTE · 6-10 SOLO · ALGO: TRACK")[0] = 0; v->cols = 5;   /* (a track a column: mute above, solo below) */
            for (int t = 0; t < NTRK; t++) {                 /* the tracks in their places: 1-5 mute, 6-10 solo */
                int tm = POS[t], ts = NTRK + POS[t];
                char a[4] = { (char)('1' + POS[t]), 0, 0, 0 }, s2[4] = { 'S', (char)('1' + POS[t]), 0, 0 };
                tile(v, tm, a, Q.t[t].mute ? T_EMPTY : T_ON, TF_LEVEL, TRK_COL[t]); v->tile[tm].lvl = (uint8_t)level_of(t);
                tile(v, ts, s2, solo[t] ? T_ON : T_EMPTY, TF_BAR, solo[t] ? RGB(255, 210, 60) : TRK_COL[t]);
            }
            tile(v, 10, "FILL", Q.fill ? T_WHITE : T_EMPTY, 0, 0);
            tile(v, 11, "BAR", Q.fill_bar ? T_WHITE : T_EMPTY, 0, 0);
            tile(v, 13, "TAP 16", T_EMPTY, 0, 0);
            *put_u(b, v->bpm) = 0; tile(v, 14, b, T_DIM, 0, 0);
            v->tile[trk].flags |= TF_PLAY; v->tile[NTRK + trk].flags |= TF_PLAY;   /* (the track the knobs turn: framed) */
            static char LB[4][8]; static const char *const NM[4] = { "LVL ", "PAN ", "DLY ", "REV " };
            for (int i = 0; i < 4; i++) *put_u(put_s(LB[i], NM[i]), (uint32_t)POS[trk] + 1u) = 0;
            const gb_track *mk = &S.trk[trk];
            knob_n(v, 0, LB[0], level_of(trk), level_of(trk));
            int pan = (int)(mk->pan.tgt * 100.0f + 0.5f) - 50; *put_i(b, pan) = 0; knob(v, 1, LB[1], b, pan + 50, 0);
            int ds = (int)(mk->send_dly.tgt * 100.0f + 0.5f), rs = (int)(mk->send_rev.tgt * 100.0f + 0.5f);
            knob_n(v, 2, LB[2], ds, ds); knob_n(v, 3, LB[3], rs, rs);
        } else if (layer == BTN_SAVE) {
            put_s(v->title, "SONG")[0] = 0;
            if (Q.song_rec) put_s(v->sub, "SONG REC · PLAY YOUR PATTERNS")[0] = 0;
            else if (Q.song_play) { p = put_s(v->sub, "SONG · "); p = put_u(p, Q.song_len); p = put_s(p, " BARS"); *p = 0; }
            else if (chain_arm && !chain_on) put_s(v->sub, "CHAIN: TOUCH THE PATTERNS")[0] = 0;
            else if (chain_on) {
                p = put_s(v->sub, "CHAIN ");
                for (int i = 0; i < chain_n && p < v->sub + 30; i++) { if (i) p = put_s(p, "→"); p = put_u(p, chain[i] + 1u); }
                *p = 0;
            }
            else put_s(v->sub, scope_one ? "PATTERN · THIS TRACK" : "PATTERN · ON THE BAR")[0] = 0;
            for (int i = 0; i < SEQ_NPAT; i++) {
                *put_u(b, (uint32_t)i + 1) = 0;
                if (i == k->cur) tile(v, i, b, T_ON, 0, TRK_COL[trk]);
                else if (i == k->next) tile(v, i, b, T_FRAME, TF_BLINK, RGB(255, 210, 60));
                else tile(v, i, b, k->pat[i].n ? T_DIM : T_EMPTY, 0, 0);
                if (chain_on) for (int c = 0; c < chain_n; c++) if (chain[c] == i) v->tile[i].flags |= TF_BAR;
            }
            tile(v, 12, Q.song_play ? "SONG" : "LOOP", Q.song_play ? T_ON : T_DIM, 0, RGB(80, 220, 255));
            tile(v, 13, "REC", Q.song_rec ? T_RED : T_FRAME, 0, RGB(255, 80, 80));
            tile(v, 14, "CHAIN", chain_arm ? T_ON : T_FRAME, 0, RGB(80, 220, 255));
            tile(v, 15, scope_one ? "ONE" : "ALL", T_DIM, 0, 0);
            for (int i = 0; i < 4; i++) v->knob[i].label = 0;
            ps_keep(trk);                                    /* (the live sound, to compare the patterns with) */
            for (int i = 0; i < SEQ_NPAT; i++) if (i != k->cur && !ps_same(&PS, trk, i, k->cur)) v->tile[i].flags |= TF_DOT;
            p = put_s(b, "P"); p = put_u(p, (uint32_t)k->cur + 1u); p = put_s(p, "→ALL"); *p = 0;
            knob(v, 0, "SOUND", b, -1, 0); v->knob[0].dial = 0;
        } else if (layer == BTN_HOME && home_sec) {             /* the menu: a section's four settings */
            put_s(v->title, "MENU")[0] = 0; put_s(v->sub, HOME_SEC[home_sec])[0] = 0;
            for (int i = 0; i < 16; i++) tile(v, i, "", T_EMPTY, 0, 0);
            for (int i = 0; i < 4; i++) {
                int r = HOME_ROW[home_sec][i];
                if (r == -2) { char c[8]; *put_u(c, (uint32_t)ml_count()) = 0; knob(v, i, "LEARN", ml_count() ? c : "NONE", -1, 0); continue; }
                if (r == -3) { knob(v, i, "CALIBR.", "→", -1, 0); continue; }
                if (r == -4) { knob(v, i, "COLOR", scr_color < UI_NTHEMES ? ui_theme_name[scr_color] : "YOURS", scr_color * 100 / UI_NTHEMES, 0); continue; }
                if (r == -1) { v->knob[i].label = 0; continue; }
                knob(v, i, HS_LABEL[r], home_value(r, b), hs[r] * 100 / HS_MAX[r], 0);
            }
        } else if (layer == BTN_HOME) {
            put_s(v->title, "PROJECT")[0] = 0;
            /* the measures on the four dials, readable: the load (average, worst of the last half second, worst since
             * power-on), the stack used; a late half (a gap in the sound) said beside the title */
            if (late) { p = put_s(v->sub, "LATE "); p = put_u(p, late); *p = 0; }
            else {                                           /* (3.3.5: how full the one project is, measured once a second) */
                static uint32_t t_pct; uint32_t now = time_ticks();
                if (!saving && !sbuf_held() && (uint32_t)(now - t_pct) > 1000000u * lsb) { t_pct = now; uint32_t c; proj_pct = (proj_measure(&c) * 100u + STORE_PMAX - 1u) / STORE_PMAX; }
                p = put_s(v->sub, "MEMORY "); p = put_u(p, proj_pct); p = put_s(p, "%"); *p = 0;
            }
            *put_s(put_u(b, load_avg), "%") = 0; knob(v, 0, "CPU", b, (int)load_avg, 0);
            *put_s(put_u(b, load_peak), "%") = 0; knob(v, 1, "PEAK", b, (int)load_peak, 0);
            *put_s(put_u(b, load_max), "%") = 0; knob(v, 2, "MAX", b, (int)load_max, 0);
            uint32_t sp = stack_used_pct(); *put_s(put_u(b, sp), "%") = 0; knob(v, 3, "STACK", b, (int)sp, 0);
            tile(v, HK_OPEN, "OPEN", proj_used ? T_DIM : T_EMPTY, 0, 0);   /* (3.3.5: one project: the saved one opened again, a new one, saved) */
            tile(v, HK_NEW, "NEW", T_DIM, 0, 0);                   /* (3.3.6: white keys 1, 3, 5, lit) */
            tile(v, HK_SAVE, "SAVE", T_FRAME, 0, TRK_COL[trk]);
        } else if (layer == BTN_SEL) {                           /* KEY: the chord of each white key */
            const harm_cfg *h = &harm[trk];
            put_s(v->title, "KEY")[0] = 0; v->title_col = RGB(80, 230, 120);
            if (is_drum(trk)) {
                put_s(v->sub, "DRUMS · NO SCALE")[0] = 0;
                for (int i = 0; i < 4; i++) v->knob[i].label = 0;
            } else {
                p = put_s(v->sub, "KEY "); p = put_s(p, harm_note_name[h->root]); *p++ = ' '; p = put_s(p, harm_scale_name[h->scale]);
                if (h->transpose) { p = put_s(p, " · "); p = put_i(p, h->transpose); }
                *p = 0;
                for (int i = 0; i < 16; i++) {
                    char lab[12]; harm_chord_label(h, i, lab);
                    uint8_t n[5]; int nn = harm_notes(h, white_key[i], 0, 0, n);
                    int root = nn && ((n[0] - h->transpose) % 12 + 12) % 12 == h->root;
                    tile(v, i, lab[0] ? lab : "-", !nn ? T_EMPTY : root ? T_ON : T_DIM, 0, RGB(80, 230, 120));
                }
                (void)sel_page;                                  /* the same knobs, held or tapped */
                knob(v, 0, "ROOT", harm_note_name[h->root], h->root * 100 / 11, 0);
                knob(v, 1, "SCALE", harm_scale_name[h->scale], h->scale * 100 / (SCALE_N - 1), 0);
                knob(v, 2, "CHORD", harm_chord_name[h->chord], h->chord * 20, 0);
                knob(v, 3, "KEYS", harm_keys_name[h->keys], h->keys * 50, 0);
            }
        } else if (layer == BTN_ARP) {                           /* ROLL */
            put_s(v->title, "ROLL")[0] = 0; v->title_col = RGB(80, 220, 255);
            put_s(v->sub, "KEY HELD · REPEATS")[0] = 0;
            for (int i = 0; i < 16; i++) { int n = is_drum(trk) ? i : note_of_key(white_key[i]); note_name(b, trk, n); tile(v, i, b, key_nn[white_key[i]] ? T_WHITE : T_EMPTY, 0, 0); }
            knob(v, 0, "RATE", ROLL_NAME[roll_rate], roll_rate * 25, 0);
            for (int i = 1; i < 4; i++) v->knob[i].label = 0;
        } else {                                                /* EDIT: erase */
            put_s(v->title, "ERASE")[0] = 0;
            put_s(v->sub, Q.playing ? "KEY HELD: ERASES AS IT PASSES" : "KEY: ITS NOTE · THE WHOLE PATTERN")[0] = 0;
            for (int i = 0; i < 16; i++) {
                int n = is_drum(trk) ? i : note_of_key(white_key[i]);
                note_name(b, trk, n);
                tile(v, i, b, pattern_has_note(trk, n) ? T_DIM : T_EMPTY, 0, 0);
            }
            knob(v, 0, "SHIFT", "<>", -1, 0);
            knob_n(v, 1, "LEN", k->pat[k->cur].steps, -1);
            knob(v, 2, "TRANSP", "<>", -1, 0);
            knob_n(v, 3, "UNDO", HIST.n_undo, -1);
            for (int i = 0; i < 4; i++) v->knob[i].dial = 0;
        }
    } else {
        v->page = UI_SOON;
        static const char *const TITLE[] = { "", "", "OSC 1/6", "FILTER 2/6", "VOICE 3/6", "ENV 1/2", "ENV DEST 2/2", "SENDS 1/7", "KIT", "GLOBAL 1/6", "", "LFO 1/3", "SEL", "ARP 1/2", "LFO DEST 2/3", "MASTER 7/7", "PLUS 4/6", "ARP 2/2", "DELAY 5/7", "SOUND 2/2", "SEL 2/2", "MIDI 4/6", "MIDI 5/6", "NOTES", "LFO TRIG 3/3", "INSERT 2/7", "INSERT MIX 3/7", "SLICER 4/7", "USER SOUNDS 5/6", "TEXTURE 2/6", "TEXTURE 3/6", "SONG", "SOUNDS 6/6", "REV · CHO 6/7" , "REC 6/6" };
        _Static_assert(sizeof TITLE / sizeof TITLE[0] == PG_GREC + 1, "a title for every page");
        put_s(v->page_name, TITLE[pg])[0] = 0;
        if (pg == PG_NOTES && !is_drum(trk)) {
            v->page = UI_NOTES;
            const seq_pat *pp = &k->pat[k->cur];
            if (ne_step >= pp->steps) { ne_step = 0; ne_sub = 0; }
            int nv = NE_ZOOM[ne_zoom], first = ne_step / nv * nv, last = first + nv < pp->steps ? first + nv : pp->steps, sel = ne_sel();
            p = put_s(v->page_name, "NOTES "); p = put_u(p, (uint32_t)first + 1u); *p++ = '-'; p = put_u(p, (uint32_t)last); *p = 0;
            int lo = 127, hi = 0, n = 0, sp = sel >= 0 ? pp->ev[sel].note : last_note;
            for (int i = 0; i < pp->n && n < UI_NOTES_MAX; i++) {
                const seq_ev *e = &pp->ev[i]; int es = ev_step(pp, e->tick);
                if (es < first || es >= last) continue;
                int x = ((int)e->tick - first * SEQ_TPS) * 64 / (nv * SEQ_TPS), l = e->len * 64 / (nv * SEQ_TPS);
                ui_note *u = &v->nr[n++];
                u->x = (uint8_t)(x < 0 ? 0 : x > 63 ? 63 : x); u->len = (uint8_t)(l < 1 ? 1 : l > 64 ? 64 : l); u->pitch = e->note;
                u->flags = (uint8_t)((i == sel ? NR_SEL : 0) | ((e->ratchet & SEQ_ACCENT) ? NR_ACC : 0) | ((e->ratchet & SEQ_SLIDE) ? NR_SLIDE : 0) |
                                     (e->vel < 50 ? NR_GHOST : e->vel < 85 ? NR_SOFT : 0));
                if (e->note < lo) lo = e->note;
                if (e->note > hi) hi = e->note;
            }
            v->nr_n = (uint8_t)n;
            if (!n) lo = hi = sp;
            if (hi - lo > 17) { lo = sp - 9 < 0 ? 0 : sp - 9; hi = lo + 17; if (hi > 127) { hi = 127; lo = 110; } }   /* (too wide: 18 rows around the note edited) */
            while (hi - lo < 11) { if (lo > 0) lo--; if (hi - lo < 11 && hi < 127) hi++; }   /* (12 rows at least: 10 px each) */
            v->nr_lo = (uint8_t)lo; v->nr_hi = (uint8_t)hi; v->nr_cur = (uint8_t)(ne_step - first); v->nr_cols = (uint8_t)nv;
            int at = Q.playing ? (int)seq_step_of(&Q, trk) : -1;
            v->nr_play = (uint8_t)(at >= first && at < last ? (at - first) * 64 / nv : 255);
            if (ne_sheet) {                                  /* the note's sheet: KNOB 1 the row, KNOB 2 its value */
                p = put_s(v->page_name, "SHEET "); p = put_u(p, (uint32_t)ne_step + 1u); *p = 0;
                knob(v, 0, "LINE", NE_ROW[ne_row], ne_row * 20, 0);
                if (sel < 0) knob(v, 1, NE_ROW[ne_row], "--", -1, 0);
                else if (ne_row == 0) { int sl = seq_step_flags(&Q, trk, ne_step) & SEQ_SLIDE; knob(v, 1, "SLIDE", sl ? "ON" : "OFF", sl ? 100 : 0, 0); }
                else if (ne_row == 1) { int nd = 0; seq_step_get(&Q, trk, ne_step, 0, 0, &nd); int n64 = nd * 64 / SEQ_TPS; *put_i(b, n64) = 0; knob(v, 1, "NUDGE", b, 50 + n64 * 50 / 32, 0); }
                else if (ne_row == 2) { int c = 0; seq_step_info(&Q, trk, ne_step, &c, 0); knob(v, 1, "FILL", c == SEQ_FILL ? "FILL" : c == SEQ_NOFILL ? "NO FILL" : "NORMAL", c == SEQ_FILL ? 50 : c == SEQ_NOFILL ? 100 : 0, 0); }
                else if (ne_row == 3) { *put_u(b, (uint32_t)ne_step + 1u) = 0; knob(v, 1, "STEP", b, ne_step * 100 / (pp->steps > 1 ? pp->steps - 1 : 1), 0); }
                else knob(v, 1, NE_ROW[ne_row], "OCT+", -1, 1);
                knob(v, 2, "OCT+", ne_row == 0 || ne_row == 2 ? "CHANGE" : ne_row == 4 ? "COPY" : ne_row == 5 ? "DELETE" : "--", -1, 0);
                knob(v, 3, "OCT-", "CLOSE", -1, 0);
                v->knob[2].dial = v->knob[3].dial = 0;
            } else {
            int cnt = ne_count(ne_step);
            p = put_u(b, (uint32_t)ne_step + 1u); if (cnt > 1) { *p++ = ' '; p = put_u(p, (uint32_t)ne_sub + 1u); *p++ = '/'; p = put_u(p, (uint32_t)cnt); } *p = 0;
            knob(v, 0, "STEP", b, ne_step * 100 / (pp->steps > 1 ? pp->steps - 1 : 1), 0);
            if (sel >= 0) {
                const seq_ev *e = &pp->ev[sel];
                note_name(b, trk, e->note); knob(v, 1, "NOTE", b, e->note * 100 / 127, 0);
                int l10 = e->len * 10 / SEQ_TPS; p = put_u(b, (uint32_t)l10 / 10u); if (l10 % 10) { *p++ = '.'; p = put_u(p, (uint32_t)l10 % 10u); } *p = 0;
                knob(v, 2, "LEN", b, e->len * 100 / (pp->steps * SEQ_TPS), 0);
                int f = force_of(e->vel, e->ratchet & SEQ_ACCENT); knob(v, 3, "FORCE", FORCE[f], f * 25, f == 4);
            } else { knob(v, 1, "NOTE", "--", -1, 0); knob(v, 2, "LEN", "--", -1, 0); knob(v, 3, "FORCE", "--", -1, 0); }
            }
        } else if (pg == PG_TRACKS) {
            v->page = vis_on ? UI_VIS : UI_TRACKS;
            knob_n(v, 0, "SWING", Q.swing, Q.swing);
            knob_n(v, 1, "LEVEL", level_of(trk), level_of(trk));
            int st = k->pat[k->cur].steps, spb = seq_steps_per_bar(&Q, trk);
            if (!seq_count(&Q, trk)) { bars_text(b, trk); knob(v, 2, "BARS", b, bars_pick[trk] * 100 / 8, 0); }   /* an empty track: its next take's */
            else if (trk_played(trk) && st % spb == 0) knob_n(v, 2, "BARS", st / spb, st * 100 / SEQ_MAXSTEPS);       /* a played one: its length */
            else knob_n(v, 2, "STEPS", st, st * 100 / SEQ_MAXSTEPS);
            int pan = (int)(S.trk[trk].pan.tgt * 100.0f + 0.5f) - 50; *put_i(b, pan) = 0; knob(v, 3, "PAN", b, pan + 50, 0);
        } else if (pg == PG_FX) {                              /* the sends, as SLOOP's: DST CHO DLY REV */
            v->page = UI_FX;
            int dst = (int)(base[trk][is_drum(trk) ? SP_E3 : SP_DIST] * 100.0f + 0.5f), cho = (int)(S.trk[trk].send_cho.tgt * 100.0f + 0.5f);
            knob_n(v, 0, "DST", dst, dst); knob_n(v, 1, "CHO", cho, cho);
            knob_n(v, 2, "ECHO", v->send_dly, v->send_dly); knob_n(v, 3, "REV", v->send_rev, v->send_rev);
        } else if (pg == PG_FX2) {
            v->page = UI_FX;
            knob(v, 0, "TIME", DIV[dly_div].name, dly_div * 100 / (NDIV - 1), 0);
            knob_n(v, 1, "FDBK", v->dly_fb, v->dly_fb);
            knob_n(v, 2, "COLR", fxv[0] * 100 / 127, fxv[0] * 100 / 127); knob_n(v, 3, "MIX", fxv[1] * 100 / 127, fxv[1] * 100 / 127);
        } else if (pg == PG_FX3) {                                /* the reverb's SIZE and DAMP, the chorus's rate and depth */
            v->page = UI_GENERIC;
            knob_n(v, 0, "SIZE", v->rev_size, v->rev_size); knob_n(v, 1, "DAMP", fxv[2] * 100 / 127, fxv[2] * 100 / 127);
            float hz = S.cho_rate; char hb[10], *q = put_u(hb, (uint32_t)hz); *q++ = '.'; q = put_u(q, (uint32_t)(hz * 10.0f) % 10u); q = put_s(q, "HZ"); *q = 0;
            knob(v, 2, "CRT", hb, fxv[3] * 100 / 127, 0);
            int cd = (int)(S.cho_depth * 100.0f + 0.5f); knob_n(v, 3, "CDP", cd, cd);
        } else if (pg == PG_ARP || pg == PG_ARP2) {           /* the arpeggiator */
            v->page = UI_ARP;
            const arp_cfg *c = &arpc[trk];
            if (is_drum(trk)) { v->page = UI_SOON; put_s(v->sub, "NO ARP ON THE DRUMS")[0] = 0; for (int i = 0; i < 4; i++) v->knob[i].label = 0; }
            else if (pg == PG_ARP) {
                knob(v, 0, "MODE", ARP_MODE[c->mode], c->mode * 100 / (AM_N - 1), 0);
                knob(v, 1, "RATE", ARP_RATE[c->rate], c->rate * 100 / 6, 0);
                knob_n(v, 2, "OCT", c->oct, (c->oct - 1) * 33);
                knob_n(v, 3, "GATE", c->gate, c->gate);
            } else {
                knob_n(v, 0, "SWG", c->swing, c->swing); knob_n(v, 1, "PROB", c->prob, c->prob);
                knob(v, 2, "HOLD", c->hold ? "ON" : "OFF", c->hold * 100, 0); knob(v, 3, "ORD", c->ord ? "PLAY" : "NOTE", c->ord * 100, 0);
            }
            /* the picture: what it plays of the chord held (C E G when nothing is) */
            static const uint8_t DEMO[3] = { 60, 64, 67 };
            const uint8_t *h = A.trk == trk && A.n ? A.note : DEMO; int hn = A.trk == trk && A.n ? A.n : 3;
            arp_cfg cc = *c; if (cc.mode == AM_OFF) cc.mode = AM_UP;
            uint32_t rr = 12345u; int wk = 0, lo = 127, hi = 0; uint8_t seq16[16];
            for (int i = 0; i < 16; i++) { uint8_t o[4]; int m = arp_pick(&cc, h, hn, (uint32_t)i, &rr, &wk, o); seq16[i] = m ? o[0] : 255; if (m) { if (o[0] < lo) lo = o[0]; if (o[0] > hi) hi = o[0]; } }
            v->arp_n = 16;
            for (int i = 0; i < 16; i++) v->arp_seq[i] = seq16[i] == 255 ? 255 : (uint8_t)(hi > lo ? (seq16[i] - lo) * 100 / (hi - lo) : 50);
        } else if (pg == PG_DSOUND && is_drum(trk)) {          /* the drums: each sound's own tune, decay, mute */
            v->page = UI_KIT;
            int n = last_note & 15; const gb_patch *dp = &S.trk[trk].patch;
            put_s(v->page_name, "SOUND 2/2")[0] = 0;
            knob(v, 0, "SOUND", PAD[n], n * 100 / 15, 0);
            *put_i(b, dp->dtune[n]) = 0; knob(v, 1, "TUNE", b, (dp->dtune[n] + 8) * 100 / 15, 0);
            *put_i(b, dp->ddec[n]) = 0; knob(v, 2, "DEC", b, (dp->ddec[n] + 4) * 100 / 7, 0);
            knob(v, 3, "MUTE", (dp->dmute >> n) & 1u ? "ON" : "OFF", (dp->dmute >> n) & 1u ? 100 : 0, 0);
        } else if (pg == PG_SEL2) {                             /* SEL 2/2: STRUM, VLEAD */
            v->page = UI_GENERIC; put_s(v->page_name, "SEL 2/2 · PLAY")[0] = 0;
            if (is_drum(trk)) for (int i = 0; i < 4; i++) v->knob[i].label = 0;
            else {
                *put_i(b, harm[trk].transpose) = 0; knob(v, 0, "TRANS", b, 50 + harm[trk].transpose * 50 / 24, 0);
                int ms = strum[trk] * 120 / 100; *put_s(put_u(b, (uint32_t)ms), "ms") = 0;
                knob(v, 1, "STRUM", strum[trk] ? b : "OFF", strum[trk], 0);
                knob(v, 2, "VLEAD", vlead[trk] ? "ON" : "OFF", vlead[trk] * 100, 0);
                v->knob[3].label = 0;
            }
        } else if (pg == PG_TEX1 || pg == PG_TEX2) {          /* TEXTURE: the bed under the mix */
            static const char *const SRC[11] = { "OFF", "VINYL", "TAPE", "RAIN", "WIND", "RADIO", "ROOM", "USR1", "USR2", "USR3", "USR4" };
            static const char *const RT[5] = { "1BAR", "2BAR", "4BAR", "8BAR", "16BAR" };
            v->page = UI_GENERIC;
            if (pg == PG_TEX1) {
                knob(v, 0, "SRC", SRC[S.tx[0] % 11], S.tx[0] * 10, 0);
                knob_n(v, 1, "LEVEL", S.tx[1] * 100 / 127, S.tx[1] * 100 / 127);
                int tn = (int8_t)S.tx[2]; p = put_s(b, tn < 0 ? "LP " : tn > 0 ? "HP " : "OFF"); if (tn) p = put_u(p, (uint32_t)(tn < 0 ? -tn : tn)); *p = 0;
                knob(v, 2, "TONE", b, 50 + tn * 50 / 64, 0);
                knob_n(v, 3, "MOVE", S.tx[3] * 100 / 127, S.tx[3] * 100 / 127);
            } else {
                knob(v, 0, "RATE", RT[S.tx[4] % 5], S.tx[4] * 25, 0);
                knob(v, 1, "WHEN", S.tx[5] ? "ALWAYS" : "PLAY", S.tx[5] * 100, 0);
                knob(v, 2, "DUCK", S.tx[6] ? "ON" : "OFF", S.tx[6] * 100, 0);
                v->knob[3].label = 0;
            }
        } else if (pg == PG_MIDI || pg == PG_MIDI2) {         /* MIDI: the settings; then what comes in */
            v->page = UI_GENERIC;
            if (pg == PG_MIDI) {
                knob(v, 0, "IN", midi_in_name[MC.in], MC.in * 50, 0);
                knob(v, 1, "OUT", midi_out_name[MC.out], MC.out * 50, 0);
                knob(v, 2, "SYNC", midi_sync_name[MC.sync], MC.sync * 100, 0);
                knob(v, 3, "CLK", MC.clk_out ? "ON" : "OFF", MC.clk_out * 100, 0);
            } else {
                knob(v, 0, "DRUM", midi_dmap_name[MC.dmap], MC.dmap * 100, 0);
                if (m_rmch) knob_n(v, 1, "REMOTE", m_rmch, m_rmch * 100 / 16); else knob(v, 1, "REMOTE", "OFF", 0, 0);
                if (m_fxch) knob_n(v, 2, "FX CH", m_fxch, m_fxch * 100 / 16); else knob(v, 2, "FX CH", "OFF", 0, 0);
                knob_n(v, 3, "BEND", m_bend, m_bend * 100 / 12);
                p = put_s(v->page_name, "MIDI 5/6 · USB "); p = put_s(p, usb_midi_ready() ? "OK" : "--"); p = put_s(p, " · GOT ");
                p = put_u(p, midi_n % 100000u); *p = 0;           /* (USB and TRS: a keyboard that reaches us) */
            }
        } else if (pg == PG_GREC) {                           /* (3.3.6) REC: a track changed while recording; how a take starts */
            v->page = UI_GENERIC;
            knob(v, 0, "TRACK", rec_keep ? "KEEP" : "STOP", rec_keep * 100, 0);   /* (a track changed while recording: REC STOP or KEEP) */
            knob(v, 1, "START", rec_count ? "CLICK" : "NOTE", rec_count * 100, 0);
            v->knob[2].label = 0; v->knob[3].label = 0;
        } else if (pg == PG_MASTER) {
            v->page = UI_MASTER;
            fx_knobs(v);
        } else if (pg == PG_GLOBAL) {
            v->page = UI_MIXER;
            if (ext_on(now)) { p = put_s(b, "EXT "); p = put_u(p, v->bpm); *p = 0; knob(v, 0, "BPM", b, (v->bpm - 30) * 100 / 270, 0); }
            else knob_n(v, 0, "BPM", v->bpm, (v->bpm - 30) * 100 / 270);
            knob_n(v, 1, "SWING", Q.swing, Q.swing);
            knob(v, 2, "CLICK", CLICK_NAME[click_mode], click_mode * 50, 0);
            static char gl[8]; *put_u(put_s(gl, "LVL "), (uint32_t)POS[trk] + 1u) = 0; knob_n(v, 3, gl, level_of(trk), level_of(trk));
        } else if (pg == PG_USER && !is_drum(trk)) {            /* USER SOUNDS: a place, load, save, erase */
            v->page = UI_GENERIC;
            p = put_u(b, (uint32_t)us_place + 1u); p = put_s(p, us_used(us_place) ? " " : " EMPTY"); *p = 0;
            knob(v, 0, "SLOT", b, us_place * 100 / (US_N - 1), 0);
            knob(v, 1, "LOAD", us_used(us_place) ? us_name(us_place) : "-", -1, 0);
            knob(v, 2, "SAVE", "→", -1, 0);
            knob(v, 3, "ERASE", us_used(us_place) ? "→ →" : "-", -1, 0);
            for (int i = 1; i < 4; i++) v->knob[i].dial = 0;
        } else if ((pg >= PG_OSC && pg <= PG_KIT) || pg == PG_PLAY || FX_TRACK_PG(pg) || ((pg == PG_LFO || pg == PG_LFODEST || pg == PG_LFO3) && !is_drum(trk))) {
            /* the sound's pages: SLOOP's values, its names (the engine's for EDIT 1/4 and 2/4) */
            static const uint8_t UIP[] = { 0, 0, UI_GENERIC, UI_GENERIC, UI_PLAY, UI_ENV, UI_ENVDEST, UI_FX, UI_KIT };
            v->page = FX_TRACK_PG(pg) ? UI_GENERIC : is_drum(trk) ? UI_KIT : pg == PG_PLAY || pg == PG_LFO3 ? UI_GENERIC : pg == PG_LFO || pg == PG_LFODEST ? UI_LFO : UIP[pg];
            if (is_va(trk) && pg == PG_OSC) v->page = UI_OSC;
            if (is_va(trk) && pg == PG_FILTER) v->page = UI_FILTER;
            if (FX_TRACK_PG(pg)) { }                                   /* (its title: TITLE) */
            else if (is_drum(trk)) put_s(v->page_name, "KIT 1/2 · 16 SOUNDS")[0] = 0;
            else if (pg == PG_OSC || pg == PG_FILTER) {                            /* the engine's own two pages */
                int k = pg - PG_OSC;
                p = put_s(v->page_name, gb_edesc[q->engine].page[k]); *p++ = ' '; *p++ = (char)('1' + k); p = put_s(p, "/5"); *p = 0;
            } else if (pg == PG_LFO3) put_s(v->page_name, "LFO TRIG 3/3")[0] = 0;
            const int8_t *P = params_of(pg, trk);
            for (int i = 0; i < 4; i++) {
                int pi = is_drum(trk) && !FX_TRACK_PG(pg) ? i : P[i];
                const pdesc *d = pdesc_of(q->engine, pi);
                if (pi < 0 || !d->name[0] || (pi == SP_FTYPE && !is_va(trk))) { v->knob[i].label = 0; continue; }
                param_text(b, q->engine, pi, base[trk][pi]);
                knob(v, i, d->name, b, param_bar(q->engine, pi, base[trk][pi]), 0);
            }
        } else {
            {
                put_s(v->sub, "NO LFO ON THE DRUMS")[0] = 0;
                for (int i = 0; i < 4; i++) v->knob[i].label = 0;
            }
        }
    }
    v->toast = msg[0] && msg_live ? msg : 0; v->toast_kind = (uint8_t)msg_bad;
    v->info = info[0] && info_live && !v->toast ? info : 0;
    v->saving = (uint8_t)(saving && saving_shown); v->save_pct = saving ? (uint8_t)store_progress() : 0;
}

#define TILE_W 120
#define TILE_H 16
#define NTILES ((GFX_W / TILE_W) * (GFX_H / TILE_H))
static uint16_t tilebuf[TILE_W * TILE_H] __attribute__((aligned(4), section(".bootbss")));
static uint32_t tile_hash[NTILES];
static uint8_t tile_sent[NTILES];
static int tile_i = -1;
static uint32_t t_pass, gen_drawn = 0xFFFFFFFFu;
static ui_view V GB_LOWRAM, W GB_LOWRAM;   /* (W: zeroed and filled each pass; V: copied from it before use) */
COLD static int vis_busy(void);
_Static_assert(sizeof sbuf >= VIS_MEM, "the visualiser's memory in sbuf");
/* the visualiser's picture: what the sound left for it, worked out in sbuf (lent: the editor's read starts again) */
COLD static void vis_pass(void)
{
    static uint32_t b256, t_last;
    uint32_t now = time_ticks(), dt = (now - t_last) / lsb; t_last = now;
    if (Q.playing) b256 = (uint32_t)((Q.pos >> 16) * 256u / (4u * SEQ_TPS));
    else b256 += (uint32_t)((float)(dt > 200000u ? 0u : dt) * Q.bpm * (256.0f / 60e6f));
    vis_in in; in.ring = vis_ring; in.w = S.vis_w; in.kick = S.vis_kick;
    for (int t = 0; t < NTRK; t++) { int i = POS[t]; in.col[i] = TRK_COL[t]; in.lvl[i] = S.trk[t].meter; in.hit[i] = S.vis_hit[t]; in.vel[i] = S.vis_vel[t]; in.note[i] = S.vis_note[t]; }
    in.bpm10 = (uint16_t)(Q.bpm * 10.0f + 0.5f); in.bar = (uint16_t)(Q.pos / ((uint64_t)SEQ_BAR << 16));
    in.beat = (uint8_t)(((Q.pos >> 16) % SEQ_BAR) / (4 * SEQ_TPS)); in.playing = (uint8_t)Q.playing; in.rec = (uint8_t)(Q.recording || recording_armed);
    in.beats256 = b256; in.style = vis_style; in.name = vis_named; if (vis_named) vis_named--;
    sbuf_used(); vis_frame(sbuf, &in);
}
/* one step of drawing: a few tiles drawn (a quarter of a millisecond at most), the first that changed sent (its
 * DMA runs on alone). A pass starts only when the view changed. 1 while a pass is under way */
COLD static int ui_step(void)
{
    if (lcd_tile_busy()) return 1;
    uint32_t now = time_ticks();
    if (tile_i < 0) {
        if ((uint32_t)(now - t_pass) < 40000u * lsb) return 0;       /* 25 passes a second at most */
        t_pass = now;
        uint8_t *z = (uint8_t *)&W; for (uint32_t i = 0; i < sizeof W; i++) z[i] = 0;   /* (the padding too) */
        fill_view(&W);
        if (W.page == UI_SPLASH) {                           /* the start's logo: a picture each pass */
            { uint16_t c[5]; for (int t = 0; t < NTRK; t++) c[POS[t]] = TRK_COL[t]; vis_splash((now - splash_t0) / lsb / 1000u, 0, c);   /* (3.3: the logo alone; the version is in HOME) */ } V = W; gen_drawn = msg_gen; tile_i = 0;
        } else if (W.page == UI_VIS) {                       /* the visualiser: a new picture each pass, in sbuf when free */
            if (vis_busy()) return 0;
            vis_pass(); V = W; gen_drawn = msg_gen; tile_i = 0;
        } else {
            const uint8_t *a = (const uint8_t *)&W, *b = (const uint8_t *)&V;
            uint32_t i = 0; while (i < sizeof W && a[i] == b[i]) i++;
            if (i == sizeof W && gen_drawn == msg_gen) return 0;      /* nothing new to show */
            V = W; gen_drawn = msg_gen; tile_i = 0;
        }
    }
    for (int n = 0; n < 8 && tile_i < NTILES && (uint32_t)(time_ticks() - now) < 250u * lsb; n++) {
        int i = tile_i++, tx = (i % (GFX_W / TILE_W)) * TILE_W, ty = (i / (GFX_W / TILE_W)) * TILE_H;
        gfx_target(tilebuf, tx, ty, TILE_W, TILE_H);
        ui_draw(&V);
        uint32_t h = gfx_finish(tilebuf, TILE_W * TILE_H);
        if (!tile_sent[i] || h != tile_hash[i]) { tile_hash[i] = h; tile_sent[i] = 1; lcd_tile_start(tx, ty, TILE_W, TILE_H, tilebuf); break; }
    }
    if (tile_i >= NTILES) tile_i = -1;
    return 1;
}
/* the screen drawn now, one whole pass (3.1.6: one, not "until nothing is new": the start's logo is new each pass,
 * and a pass sends 77 ms of tiles at 12 MHz, more than the 40 ms between passes: the start never went on) */
COLD static void ui_flush(void)
{
    t_pass = time_ticks() - 40000u * lsb;
    int began = 0;
    while (ui_step()) { usb_midi_poll(); if (tile_i >= 0) began = 1; else if (began) break; }
    while (lcd_tile_busy()) usb_midi_poll();
}

/* ---------------------------------------------------------------- projects */
/* 3.3.5: one project, up to 52 KB, more than any buffer: its bytes are made a part at a time (app/project.c
 * project_part), each time from the project as it is; a save checks that what it wrote is what it began with */
COLD static void proj_extra(project_extra *xp)
{
    sound_from_base();
    project_extra x; memset(&x, 0, sizeof x);
    for (int t = 0; t < NTRK; t++) ps_keep(t);
    x.ps = &PS;
    x.dly_div = (uint8_t)dly_div; x.fade = S.pk_fade;
    for (int t = 0; t < SEQ_NTRK; t++) { x.strum[t] = t < NTRK ? strum[t] : 0; x.vlead[t] = t < NTRK ? vlead[t] : 0; }
    for (int t = 0; t < SEQ_NTRK; t++) { const arp_cfg *c = &arpc[t < NTRK ? t : 0]; uint8_t *a = x.arp[t];
        a[0] = c->mode; a[1] = c->rate; a[2] = c->oct; a[3] = c->gate; a[4] = c->swing; a[5] = c->prob; a[6] = c->hold; a[7] = c->ord; }
    for (int t = 0; t < SEQ_NTRK; t++) {
        int po = t < NTRK ? preset_origin(preset_of[t]) : -1;
        x.preset[t] = (uint8_t)(po < 0 || po > 127 ? 0xFF : (unsigned)po | (preset_of[t] < -1 ? 0x80u : 0u));   /* (an older SLOOP 3's: 7 bits) */
        x.preset16[t] = (uint16_t)(po < 0 ? 0xFFFFu : (unsigned)po | (preset_of[t] < -1 ? 0x8000u : 0u));
    }
    for (int t = 0; t < SEQ_NTRK; t++) {
        const harm_cfg *h = t < NTRK ? &harm[t] : 0;
        x.harm[t][0] = h ? h->root : 0; x.harm[t][1] = h ? h->scale : 0; x.harm[t][2] = h ? h->keys : 0;
        x.harm[t][3] = h ? h->chord : 0; x.harm[t][4] = (uint8_t)(64 + (h ? h->transpose : 0));
    }
    x.midi[0] = MC.in; x.midi[1] = MC.sync; x.midi[2] = MC.out; x.midi[3] = MC.clk_out; x.midi[4] = MC.dmap;
    x.midi[5] = (uint8_t)(rec_mode | rec_keep << 1); x.midi[6] = rec_bars; x.midi[7] = rec_count;   /* (the REC screen's, kept with them; 3.3.6: bit 1 REC KEEP) */
    memset(x.home, 0xFF, sizeof x.home);
    for (int i = 0; i < HS_N; i++) x.home[i] = hs[i];
    for (int i = 0; i < 7; i++) x.tex[i] = S.tx[i];
    for (int i = 0; i < 4; i++) x.fxv[i] = fxv[i];
    x.tex[7] = 0;
    x.home[40] = m_rmch; x.home[41] = m_fxch; x.home[42] = m_bend; x.home[75] = scr_color; x.home[76] = (uint8_t)(0x80u | vis_style);   /* (3.3.6: bit 7, one of the six) */
    for (int i = 0; i < ML_N; i++) { x.home[43 + 2 * i] = (uint8_t)(ml_tab[i] & 0xFF); x.home[44 + 2 * i] = (uint8_t)(ml_tab[i] >> 8); }
    if (cal_ok) { x.home[10] = 1; for (int i = 0; i < 14; i++) x.home[11 + i] = cal_btn[i]; for (int i = 0; i < 7; i++) { x.home[25 + i] = cal_enc[i]; x.home[32 + i] = (uint8_t)(cal_dir[i] > 0); } }
    *xp = x;
}
COLD static uint32_t proj_measure(uint32_t *crc)          /* the project's length and CRC, nothing kept */
{
    project_extra x; proj_extra(&x);
    return (uint32_t)project_part(&Q, &S, &x, sbuf, 0, 0, crc);
}
COLD static uint32_t proj_part(uint32_t lo, uint32_t n)   /* its bytes [lo, lo + n) into sbuf (the caller says sbuf_used) */
{
    project_extra x; proj_extra(&x);
    return (uint32_t)project_part(&Q, &S, &x, sbuf, lo, n, 0);
}
static int midi_kept;                                /* (the MIDI settings set: a project loaded later keeps them) */
/* FM6's bank voices (B1..B27): read from SLOOP's bank for the tracks that play one (the main loop, no save under way) */
static uint8_t fm6_src, syn_src;                   /* (editor: 0 none / the defaults, 1 SLOOP 2.8's, 2 yours) */
static int8_t fm6_tried[NTRK];
COLD static void fm6_bank_poll(void)
{
    if (saving) return;
    for (int t = 0; t < NTRK; t++) {
        if (S.trk[t].patch.engine != ENG_FM6) continue;
        int p = fm6_ptch(t), k = p - FM6_NFACTORY;
        if (k < 0 || fm6_user_id[t] == p || fm6_tried[t] == p) continue;
        fm6_tried[t] = (int8_t)p;
        uint8_t m[128];
        if (fm6_bank_at && ((fm6_bank_used >> k) & 1u) && !flash_read(fm6_bank_at + (uint32_t)k * 128u, m, 128)) fm6_user_set(t, p, m);
    }
    if (fb_scan < FM6_NBANK) {                           /* (3.2) the voices' names (VMEM: its last 10 bytes), one a round */
        int k = fb_scan++; char *n = fb_nm[k]; uint8_t m[128];
        n[0] = 0;
        if (fb_ok(k) && !flash_read(fm6_bank_at + (uint32_t)k * 128u, m, 128)) {
            int e = 0;
            for (int i = 0; i < 10; i++) { int c = m[118 + i] & 127; if (c >= 'a' && c <= 'z') c -= 32; if (c < 32 || c > 95) c = ' '; n[i] = (char)c; if (c != ' ') e = i + 1; }
            n[e] = 0;
        }
    }
}
/* the settings in effect: the low cut, the filters' resonance, the second core */
COLD static void home_apply(void)
{
    S.lowcut = hs[HS_LOWCUT]; S.flt_q = 0.75f + 0.45f * (float)hs[HS_RESO];
    S.usb_full = hs[HS_USB]; usb_audio_rate(hs[HS_RATE]);
}
static int home_had;                                  /* the settings came from a project (else: SLOOP's, at power-on) */
COLD static void home_from(const uint8_t *h)
{
    home_had = 1;
    for (int i = 0; i < HS_N; i++) hs[i] = h[i] <= HS_MAX[i] ? h[i] : hs[i];
    if (h[40] <= 16) m_rmch = h[40];
    if (h[41] <= 16) m_fxch = h[41];
    if (h[42] >= 1 && h[42] <= 12) m_bend = h[42];
    if (h[75] <= UI_NTHEMES) { scr_color = h[75]; color_apply(); }
    if (h[76] & 0x80u) { if ((h[76] & 0x7Fu) < VIS_N) vis_style = (uint8_t)(h[76] & 0x7Fu); } else if (h[76] < 12) vis_style = vis_from12[h[76]];   /* (3.3.6: the six; a style of the twelve, its place now) */
    for (int i = 0; i < ML_N; i++) { uint16_t e = (uint16_t)(h[43 + 2 * i] | h[44 + 2 * i] << 8); ml_tab[i] = e == 0xFFFF || (e >> 10) > ML_CHO || ((e >> 7) & 7) >= NTRK ? 0 : e; }
    if (h[10] == 1) {
        uint8_t b[14], e[7]; int8_t d[7];
        for (int i = 0; i < 14; i++) b[i] = h[11 + i];
        for (int i = 0; i < 7; i++) { e[i] = h[25 + i]; d[i] = h[32 + i] ? 1 : -1; }
        if (!panel_map(b, e, d)) { cal_ok = 1; memcpy(cal_btn, b, 14); memcpy(cal_enc, e, 7); memcpy(cal_dir, d, 7); }
    }
    home_apply();
}
COLD static int us_bank(void);
COLD static int preset_valid(int k, int engine) { return k >= UK_BASE ? engine == ENG_DRUM && k - UK_BASE >= GB_KIT_USR && k - UK_BASE < GB_NKITS : k >= FB_BASE ? k < FB_BASE + FM6_NBANK && engine == ENG_FM6 : k >= US_BASE ? us_used(k - US_BASE) : k < gb_npresets && gb_presets[k].engine == engine; }
COLD static int proj_fetch(uint32_t off, void *dst, uint32_t n) { return flash_read(proj_at + off, dst, n); }
COLD static void load_after(project_extra *xp);
/* the saved project opened (3.3.5: the one), read from the flash a part at a time; none: a new project */
COLD static void load_project(int w)
{
    (void)w;
    if (sbuf_held()) { say_bad("EDITOR BUSY"); return; }
    if (ltake.armed_trk >= 0) { loop_cancel(&ltake); recording_armed = 0; }   /* a load cancels any open take */
    memset(bars_pick, 0, sizeof bars_pick);                                    /* (and every empty track is FREE again) */
    uint32_t n = 0, crc = 0;
    int have = flash_ok && store_proj_find(&proj_at, &n, &crc) == 0;
    proj = 0; safe_hold = 0; proj_used = have ? 1u : 0u;
    project_extra x; memset(&x, 0, sizeof x); x.ps = &PS;
    if (have && project_load_from(&Q, &S, &x, proj_fetch, n, 0) == 0) {
        load_after(&x);
        say("PROJECT OPENED");
    } else {                                                 /* none yet: a new project, as SLOOP's NEW */
        new_project(0); recording_armed = 0;
        saved_crc = 0; saved_len = 0;
        say("NEW PROJECT");
    }
    midi_kept = 1;
}
/* what a project file loaded brings beside the sequencer and the sound */
COLD static void load_after(project_extra *xp)
{
    project_extra x = *xp;
    {
        base_from_sound(); recording_armed = 0;
        for (int t = 0; t < NTRK; t++) solo[t] = 0;
        if (x.dly_div < NDIV) dly_div = x.dly_div;
        S.pk_fade = x.fade;
        if (x.tex[0] != 0xFF) for (int i = 0; i < 7; i++) S.tx[i] = x.tex[i];   /* (TEXTURE: the project's) */
        { static const uint8_t FX0[4] = { 70, 90, 60, 40 }; for (int i = 0; i < 4; i++) fxv[i] = x.fxv[i] <= 127 ? x.fxv[i] : FX0[i]; fxv_apply(); }
        if (!midi_kept && x.midi[0] != 0xFF) {             /* the MIDI settings: the device's, taken at power-on only */
            MC.in = x.midi[0]; MC.sync = x.midi[1]; MC.out = x.midi[2]; MC.clk_out = x.midi[3]; MC.dmap = x.midi[4];
            rec_mode = (uint8_t)(x.midi[5] & 1u); rec_keep = (uint8_t)((x.midi[5] >> 1) & 1u); rec_bars = x.midi[6] == 2 || x.midi[6] == 4 ? x.midi[6] : 1; rec_count = x.midi[7] == 1;
            midi_cfg_check(&MC);
        }
        if (!midi_kept && x.home[0] != 0xFF) home_from(x.home);   /* the device's settings, as the MIDI's */
        for (int t = 0; t < NTRK; t++) { strum[t] = x.strum[t]; vlead[t] = x.vlead[t]; }
        for (int t = 0; t < NTRK; t++) {                    /* (an older project: the arpeggiator off, as it was) */
            const uint8_t *a = x.arp[t]; arp_cfg *c = &arpc[t];
            if (!a[1] && !a[2]) { *c = (arp_cfg){ 0, 3, 1, 50, 0, 100, 0, 0 }; continue; }
            c->mode = a[0] < AM_N ? a[0] : 0; c->rate = a[1] < 7 ? a[1] : 3; c->oct = a[2] >= 1 && a[2] <= 4 ? a[2] : 1;
            c->gate = a[3] >= 5 && a[3] <= 100 ? a[3] : 50; c->swing = a[4] <= 100 ? a[4] : 0; c->prob = a[5] <= 100 ? a[5] : 100;
            c->hold = a[6] ? 1 : 0; c->ord = a[7] ? 1 : 0;
        }
        for (int t = 0; t < NTRK; t++) {
            harm[t].root = x.harm[t][0]; harm[t].scale = x.harm[t][1]; harm[t].keys = x.harm[t][2];
            harm[t].chord = x.harm[t][3]; harm[t].transpose = (int8_t)(x.harm[t][4] - 64);
        }
        for (int t = 0; t < NTRK; t++) {                     /* (bit 7: edited since) */
            int k = x.preset16[t] & 0x7FFF;
            preset_of[t] = x.preset16[t] != 0xFFFF && preset_valid(k, S.trk[t].patch.engine) ? (x.preset16[t] & 0x8000 ? -k - 2 : k) : -1;
            float g = 1.0f;                                 /* (its level: the preset's, or your sound's) */
            if (preset_of[t] != -1 && k >= UK_BASE) g = 1.0f;
            else if (preset_of[t] != -1 && k >= FB_BASE) g = FB_GAIN;
            else if (preset_of[t] != -1 && k >= US_BASE) { us_sound u; if (us_bank() && us_get(sbuf, k - US_BASE, &u) && u.gain) g = (float)u.gain * 0.001f; }
            else if (preset_of[t] != -1) g = preset_gain(k);
            S.trk[t].patch.gain = g;
        }
        for (int t = 0; t < NTRK; t++) {                     /* each pattern's sound: its own, else the track's */
            for (int i = 0; i < SEQ_NPAT; i++) if (!((x.ps_mask[t] >> i) & 1u)) ps_take(&PS, t, i, &S.trk[t].patch, base[t], preset_of[t]);
            PS.cur[t] = Q.t[t].cur;
        }
        saved_len = proj_measure(&saved_crc);                /* what it is now, to know when it changes */
    }
}
/* the project kept: shown = the player asked for it (the bar on the screen); else the autosave, silent */
COLD static void save_project(int shown)
{
    if (!flash_ok) { if (shown) say_bad("NO FLASH"); return; }
    if (saving) {                                         /* (3.2) asked for while something else is being written (your
                                                             favourites, a sound, the autosave): done as soon as that ends,
                                                             no longer dropped without a word */
        if (shown) { save_soon = 1; t_save_soon = time_ticks() - 1600001u * lsb; }
        return;
    }
    if (sbuf_held()) { if (shown) say_bad("EDITOR BUSY"); return; }
    if (safe_hold) { if (shown) say_bad("SAFE START: SAVE IT FROM HOME"); return; }
    uint32_t crc, n = proj_measure(&crc);
    if (n > STORE_PMAX) { say_bad("PROJECT TOO BIG"); return; }   /* (cannot be: the largest the sequencer holds fits, tests/store_test.c) */
    if (!shown && crc == saved_crc && n == saved_len) return;     /* nothing changed */
    if (store_proj_begin(n, crc, 1)) { if (shown) say_bad("SAVE: ERROR"); return; }
    saving = 1; saving_shown = shown; saving_snd = 0; saved_crc = crc; saved_len = n; saving_len = n;
}
/* the save asks for the project's next part (store_step 3): made into sbuf from the project as it is now */
COLD static void proj_feed(void)
{
    uint32_t off = store_proj_need(), n = saving_len - off > (uint32_t)sizeof sbuf ? (uint32_t)sizeof sbuf : saving_len - off;
    sbuf_used();
    proj_part(off, n);
    store_proj_give(sbuf, n);
}

/* ---------------------------------------------------------------- the panel */
/* each key's notes sounding (a chord: up to 5), its track; the last chord each track played (what a step writes) */
static uint8_t key_notes[27][5], key_nn[27], key_vel[27];
static int8_t key_trk[27];
static uint8_t last_chord[NTRK][5], last_chord_n[NTRK] = { 1, 1, 1, 1, 1 };
static struct { uint8_t on; uint32_t next; } roll[27];      /* ARP + a key: it repeats on the grid */
/* the free take (REC on an empty track, then play freely): the notes as played, in microseconds from the first;
 * REC on the "1" after the last bar closes it: 1, 2 or 4 bars at the tempo nearest the one set (kept within 3 %) */
static int project_empty(void) { for (int t = 0; t < NTRK; t++) if (Q.t[t].pat[Q.t[t].cur].n) return 0; return 1; }
static uint32_t roll_clock;                                   /* (ticks, while stopped) */
static void arp_release(void)                               /* what it plays now, ended */
{
    for (int i = 0; i < A.nsnd; i++) {
        play_off(GB_OWNER(OWN_ARP, 0), A.trk, A.snd[i]);
        if (Q.recording && A.trk == Q.rec_trk) seq_key(&Q, A.snd[i], 0, 0, 0);
    }
    A.nsnd = 0;
}
static void arp_add(int t, int note, int vel)
{
    if (A.trk != t) { arp_release(); A.n = 0; A.keys = 0; A.latched = 0; A.trk = t; }
    if (A.latched && !A.keys) { A.n = 0; A.latched = 0; }    /* HOLD: a new chord replaces the one kept */
    A.keys++;                                                 /* (counted even for a note already there) */
    for (int i = 0; i < A.n; i++) if (A.note[i] == note) return;
    if (A.n == 16) return;
    if (!A.n) {                                               /* the first note: now if stopped, on the grid if playing */
        uint32_t iv = ARP_TICKS[arpc[t].rate], now = Q.playing ? Q.tick : roll_clock;
        A.grid = Q.playing ? (now + iv - 1u) / iv * iv : now; A.at = A.grid; A.step = 0; A.walk = 0;
    }
    A.note[A.n] = (uint8_t)note; A.vel[A.n] = (uint8_t)vel; A.n++;
}
static void arp_remove(int t, int note)
{
    if (t != A.trk) return;
    if (A.keys) A.keys--;
    if (arpc[t].hold) { if (!A.keys) A.latched = 1; return; }
    for (int i = 0; i < A.n; i++) if (A.note[i] == note) { for (int j = i; j + 1 < A.n; j++) { A.note[j] = A.note[j + 1]; A.vel[j] = A.vel[j + 1]; } A.n--; break; }
}
static void arp_step(void)                                  /* in the audio block, on the sequencer's ticks */
{
    if (A.trk < 0) return;
    if (!arp_on(A.trk)) { arp_release(); A.n = 0; A.trk = -1; return; }
    const arp_cfg *c = &arpc[A.trk];
    uint32_t now = Q.playing ? Q.tick : roll_clock, iv = ARP_TICKS[c->rate < 7 ? c->rate : 3];
    if (A.nsnd && (int32_t)(now - A.off_at) >= 0) arp_release();
    if (!A.n) { arp_release(); return; }
    if ((int32_t)(now - A.at) < 0) return;
    arp_release();
    uint8_t out[4]; int m = arp_pick(c, A.note, A.n, A.step, &A.rnd, &A.walk, out);
    int vel = A.vel[A.n - 1];
    if (m && (c->prob >= 100 || (A.rnd >> 9) % 100u < c->prob)) {
        for (int i = 0; i < m; i++) {
            play_on(GB_OWNER(OWN_ARP, 0), A.trk, out[i], (float)vel * (1.0f / 127.0f));
            if (Q.recording && A.trk == Q.rec_trk) seq_key(&Q, out[i], vel, 1, 0);
            A.snd[i] = out[i];
        }
        A.nsnd = (uint8_t)m;
        A.off_at = now + iv * (c->gate < 5 ? 5 : c->gate) / 100u;
    }
    A.step++;
    if ((int32_t)(now - A.grid) >= 0) A.grid += ((now - A.grid) / iv + 1u) * iv;   /* (3.3: at once, however far behind: a loop here could run for seconds) */
    A.at = A.grid + ((A.step & 1u) ? iv / 2u * c->swing / 100u : 0u);   /* SWG: the odd notes late */
}
/* STRUM: the notes of a chord after the first, each a little later, started in the audio block */
static struct { uint8_t on, key, note, trk, vel; uint32_t at; } st_q[8];
static uint32_t st_clock;                                     /* frames */
static void strum_step(void)
{
    st_clock += GB_BLOCK;
    for (int i = 0; i < 8; i++) if (st_q[i].on && (int32_t)(st_clock - st_q[i].at) >= 0) {
        st_q[i].on = 0;
        play_on(GB_OWNER(OWN_KEY, 0), st_q[i].trk, st_q[i].note, (float)st_q[i].vel * (1.0f / 127.0f));
        if (Q.recording && st_q[i].trk == Q.rec_trk) seq_key(&Q, st_q[i].note, st_q[i].vel, 1, 0);
    }
}
/* KBD HOLD (ARP: MODE OFF, HOLD ON): the notes let go go on sounding; the next note played with no key down ends
 * them (a new chord), notes played while keys are down join them */
static uint8_t kh_note[16], kh_n; static int8_t kh_trk = -1;
static uint32_t latency_frames(void);
static int kbd_hold(int t) { return t >= 0 && t < NTRK && !is_drum(t) && arpc[t].mode == AM_OFF && arpc[t].hold; }
static void kh_release(void)
{
    for (int i = 0; i < kh_n; i++) {
        play_off(GB_OWNER(OWN_KEY, 0), kh_trk, kh_note[i]);
        if (Q.recording && kh_trk == Q.rec_trk) seq_key(&Q, kh_note[i], 0, 0, latency_frames());
    }
    kh_n = 0;
}
static void key_off(int k, uint32_t lat)
{
    gb_owner o = GB_OWNER(OWN_KEY, 0);
    if (key_nn[k] && kbd_hold(key_trk[k])) {                  /* KBD HOLD: kept sounding */
        if (kh_trk != key_trk[k]) { kh_release(); kh_trk = key_trk[k]; }
        for (int i = 0; i < key_nn[k] && kh_n < 16; i++) kh_note[kh_n++] = key_notes[k][i];
        key_nn[k] = 0; return;
    }
    for (int j = 0; j < 8; j++) if (st_q[j].on && st_q[j].key == k) st_q[j].on = 0;   /* (a strum not yet heard) */
    if (key_nn[k] && arp_on(key_trk[k])) { for (int i = 0; i < key_nn[k]; i++) arp_remove(key_trk[k], key_notes[k][i]); key_nn[k] = 0; return; }
    for (int i = 0; i < key_nn[k]; i++) {
        play_off(o, key_trk[k], key_notes[k][i]);
        loop_append(key_trk[k], key_notes[k][i], 0, 0, lat);
        if (Q.recording && key_trk[k] == Q.rec_trk) seq_key(&Q, key_notes[k][i], 0, 0, lat);
    }
    key_nn[k] = 0;
}
static void key_on(int k, int t, const uint8_t *n, int nn, float vel, uint32_t lat)
{
    gb_owner o = GB_OWNER(OWN_KEY, 0);
    key_trk[k] = (int8_t)t; key_nn[k] = (uint8_t)nn; key_vel[k] = (uint8_t)(vel * 127.0f);
    if (arp_on(t)) { for (int i = 0; i < nn; i++) { key_notes[k][i] = n[i]; arp_add(t, n[i], key_vel[k]); } return; }   /* the arp plays them */
    if (kh_n) {                                               /* KBD HOLD: a note with no key down: the kept ones end */
        int down = 0; for (int j = 0; j < 27; j++) if (j != k && key_nn[j] && key_trk[j] == t) down = 1;
        if (!down || kh_trk != t || !kbd_hold(t)) kh_release();
        else for (int i = 0; i < nn; i++) for (int j = 0; j < kh_n; j++) if (kh_note[j] == n[i]) { kh_note[j] = kh_note[--kh_n]; break; }   /* (played again: its key's now) */
    }
    for (int i = 0; i < nn; i++) {
        key_notes[k][i] = n[i];
        if (i && t < NTRK && strum[t] && !is_drum(t)) {      /* STRUM: later, in turn */
            for (int j = 0; j < 8; j++) if (!st_q[j].on) { st_q[j].on = 1; st_q[j].key = (uint8_t)k; st_q[j].note = n[i]; st_q[j].trk = (uint8_t)t; st_q[j].vel = key_vel[k];
                st_q[j].at = st_clock + (uint32_t)i * (uint32_t)strum[t] * 53u / (uint32_t)(nn > 1 ? nn - 1 : 1); break; }   /* (120 ms at 100) */
            continue;
        }
        play_on(o, t, n[i], vel);
        loop_append(t, n[i], key_vel[k], 1, lat);
        if (Q.recording && t == Q.rec_trk) seq_key(&Q, n[i], key_vel[k], 1, lat);
    }
}
/* the roll: each key held with ARP plays again on the grid (the sequencer's while it plays, a clock of its own
 * while stopped), recorded as it sounds */
static void roll_step(void)
{
    if (!Q.playing) roll_clock += (uint32_t)(Q.bpm * 4.0f * (float)SEQ_TPS / 60.0f / rate_hz * (float)GB_BLOCK + 0.5f);
    uint32_t now = Q.playing ? Q.tick : roll_clock, iv = ROLL_TICKS[roll_rate];
    /* 3.3: PLAY or STOP while a roll or the arp runs: their clock changes (the sequencer's, or their own): they go on
     * from now on the new one (they waited, silent, for the old one's time to come round on the new) */
    static uint8_t clk_play;
    if (clk_play != (uint8_t)Q.playing) {
        clk_play = (uint8_t)Q.playing;
        for (int k = 0; k < 27; k++) if (roll[k].on) roll[k].next = now;
        if (A.n || A.nsnd) { A.grid = now; A.at = now; A.off_at = now; }
    }
    for (int k = 0; k < 27; k++) {
        if (!roll[k].on || !key_nn[k]) continue;
        if ((int32_t)(now - roll[k].next) < 0) continue;
        uint8_t n[5]; int nn = key_nn[k]; float vel = (float)key_vel[k] * (1.0f / 127.0f);
        for (int i = 0; i < nn; i++) n[i] = key_notes[k][i];
        int t = key_trk[k];
        out_seq = 1; key_off(k, 0); key_on(k, t, n, nn, vel, 0); out_seq = 0;
        if ((int32_t)(now - roll[k].next) >= 0) roll[k].next += ((now - roll[k].next) / iv + 1u) * iv;   /* (3.3: at once, as the arp's) */
    }
}
static int click_on(void) { return click_mode == 2 || (click_mode == 1 && (Q.recording || recording_armed)); }

/* ---------------------------------------------------------------- MIDI in: notes, controllers, the clock
 * Played as the keys are: on the track of their channel (midi.h), through its arpeggiator, recorded while REC, a
 * free take or an armed REC started by the first note. Each note held is kept with where it went, so its end
 * reaches it even if the track selected changed; the sustain pedal keeps the ends until it is let go */
COLD static void transport(int on)                       /* PLAY, and the clock in's START / STOP */
{
    if (on) {
        if (Q.playing) return;
        seq_play(&Q, 1);
        if (recording_armed && ltake.armed_trk < 0) { seq_record(&Q, 1, trk); recording_armed = 0; }   /* (a take open stays a take: no grid recording) */
        mclk_start(&MK);                             /* (following a clock: from its next pulse, the downbeat) */
    } else {
        if (!Q.playing) return;
        seq_play(&Q, 0); recording_armed = 0;
    }
}
static void step_write(int st, int on);
static void record_now(int t)                       /* REC armed: the loop from step 1, recording track t */
{
    seq_play(&Q, 1); seq_record(&Q, 1, t); recording_armed = 0; mclk_start(&MK);
}
#define MH 32
static struct { uint8_t on, ch, note, trk, sus, arp; } mh[MH];
static uint16_t sus_on;                             /* the channels whose sustain pedal is down */
static uint8_t ext_start;                           /* START came: the sequencer starts on the next pulse */
static midi_parser mp_usb, mp_trs;
/* ---------------------------------------------------------------- the web editor (app/edproto.h) */
static uint8_t ed_rx[600];                                  /* a SysEx from USB, whole */
static uint8_t ed_raw[620];                                 /* a request unpacked, then its answer's bytes */
static uint8_t ed_tx[1400]; static uint32_t ed_tx_n, ed_tx_at;   /* the answers going out (a frame at a time) */
static struct { uint8_t on, kind, slot, fill; uint32_t total, got, st_n[2]; } ed_wr;   /* a file coming in (into sbuf; 3.3.5: a project
                                                                                          * through sbuf's two halves, to the flash as it comes) */
static struct { uint8_t ok, kind, slot; uint32_t len, crc, lo, wn, at; } ed_rd;   /* the file being read (sbuf holds its bytes [lo, lo + wn);
                                                                                      3.3.5: the saved project read from the flash at at) */
#define ED_HALF 5888u                                       /* (a half of sbuf for an incoming project: 23 pages; the second at 6144) */
/* (3.3.5) an incoming project: the half filled given to the store once it has written the one before */
COLD static void ed_proj_pump(void)
{
    if (!(ed_wr.on && ed_wr.kind == 0) || !store_proj_wants()) return;
    int f = ed_wr.fill;
    if (!ed_wr.st_n[f]) return;
    store_proj_give(sbuf + (uint32_t)f * 6144u, ed_wr.st_n[f]);
    ed_wr.st_n[1 - f] = 0; ed_wr.fill = (uint8_t)(1 - f);
}
static int ed_save_slot = -1;                               /* a project written by the editor: opened once saved */
static uint32_t ed_pending;                                 /* a request in ed_rx, its length (the host waits for its answer) */
static void ed_pump(void)                                   /* the answer out, as the queue takes it */
{
    uint8_t pk[4];
    while (ed_tx_at < ed_tx_n) {
        uint32_t at = ed_tx_at;
        int k = ed_usb_packet(ed_tx, ed_tx_n, at, pk);
        if (!usb_midi_put(pk)) return;
        ed_tx_at = at + (uint32_t)k;
    }
    ed_tx_n = ed_tx_at = 0;
}
static void ed_send(int cmd, uint32_t n) { if (ed_tx_n) return; ed_tx_n = ed_frame(cmd, ed_raw, n, ed_tx, sizeof ed_tx); ed_tx_at = 0; ed_pump(); }
static void ed_err(int code) { ed_raw[0] = (uint8_t)code; ed_send(ED_NONE, 1); }
static void ed_w16(uint8_t *b, int v) { b[0] = (uint8_t)v; b[1] = (uint8_t)((unsigned)v >> 8); }
static void ed_w32(uint8_t *b, uint32_t v) { for (int i = 0; i < 4; i++) b[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t ed_r32(const uint8_t *b) { return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; }
static const gb_pd *ed_pd(int e, int k) { static const gb_pd none = { 0, 0, 0 }; return k < 8 ? &gb_engines[e < gb_nengines ? e : 0]->ed[k] : k < SP_N ? &gb_sp[k] : &none; }
COLD static void ed_request(void);
COLD static int us_bank(void);
/* a user sample slot written from the editor (SLOOP's "FSMP" layout, where SLOOP keeps it: SLOOP 2.8 reads it too) */
static const uint32_t SLOT_AT[SMP_NUSR] = { 0xA0000u, 0xB4000u, 0xC8000u, 0xE7000u };
static struct { uint8_t on, k, cur, phase, buf, flushing; uint32_t got, crc, half; } ed_sm;   /* phase: 1 END's last data, 2 its header */
static uint8_t ed_rep[24]; static uint32_t ed_rep_n; static int ed_rep_cmd;   /* the answer sent once the flash is done */
static struct { uint8_t on, which, phase; uint32_t total, got; } ed_bk;   /* an FM6 bank or SYN kits image being written (phase 1: its sector) */
COLD static int sbuf_held(void) { return ed_wr.on || ed_sm.on || ed_bk.on || ed_bk.phase; }
static uint32_t ed_t_last;           /* (3.1.1) the editor's last request: a transfer left 10 s without one is dropped */
COLD static void sbuf_used(void) { ed_rd.ok = 0; }
/* sbuf needed elsewhere: a save, the editor's transfer, its read of a file (2 s since its last request) */
COLD static int vis_busy(void) { return saving || sbuf_held() || (ed_rd.ok && (uint32_t)(time_ticks() - ed_t_last) < 2000000u * lsb); }
static int ed_job(uint32_t sector, uint32_t off, const uint8_t *d, uint32_t n, int erase)
{
    if (store_raw_begin(sector, off, d, n, erase)) return -1;
    saving = 1; saving_shown = 0; saving_snd = 3;
    return 0;
}
COLD static void mh_end(int i, uint32_t lat)
{
    int t = mh[i].trk, n = mh[i].note;
    mh[i].on = 0;
    if (mh[i].arp) { arp_remove(t, n); return; }
    play_off(GB_OWNER(OWN_MIDI, mh[i].ch), t, n);
    loop_append(t, n, 0, 0, lat);
    if (Q.recording && t == Q.rec_trk) seq_key(&Q, n, 0, 0, lat);
}
COLD static void midi_all_off(int ch)                     /* ch < 0: every channel */
{
    for (int i = 0; i < MH; i++) if (mh[i].on && (ch < 0 || mh[i].ch == ch)) mh_end(i, 0);
    for (int c = 0; c < 16; c++) if (ch < 0 || c == ch) snd_owner_off(&S, GB_OWNER(OWN_MIDI, c));
    if (ch < 0) sus_on = 0; else sus_on &= (uint16_t)~(1u << ch);
}
COLD static void midi_note(int ch, int t, int n, int vel)
{
    if (is_drum(t)) { n = midi_drum_in(n, MC.dmap); if (n < 0) return; }
    uint32_t lat = latency_frames();
    for (int i = 0; i < MH; i++) if (mh[i].on && mh[i].ch == ch && mh[i].note == n) {
        if (!vel) { if ((sus_on >> ch) & 1u) mh[i].sus = 1; else mh_end(i, lat); return; }   /* (the pedal down: on) */
        mh_end(i, lat);                                                  /* struck again: the old one ends */
    }
    if (!vel) return;
    int slot = -1; for (int i = 0; i < MH; i++) if (!mh[i].on) { slot = i; break; }
    if (slot < 0) return;
    if (recording_armed && !Q.playing && !countin) {  /* armed: the first note starts it, as a key does */
        if (ltake.armed_trk < 0 && !rec_count) { record_now(t); lat = 0; }   /* grid arm: no take open */
        /* a take open: nothing here — the note is captured at the append site below */
    }
    mh[slot].on = 1; mh[slot].ch = (uint8_t)ch; mh[slot].note = (uint8_t)n; mh[slot].trk = (uint8_t)t; mh[slot].sus = 0;
    mh[slot].arp = (uint8_t)arp_on(t);
    /* what a step written now takes (STEPS: play a note or a chord, then the step's key), as the keys do */
    int held = 0; for (int i = 0; i < MH; i++) if (i != slot && mh[i].on && !mh[i].sus && mh[i].trk == t) held++;
    if (!held || last_chord_n[t] >= 5) last_chord_n[t] = 0;
    last_chord[t][last_chord_n[t]++] = (uint8_t)n; last_notes[t] = n;
    if (t == trk && hold_step >= 0) {                /* a step held: it takes this note */
        int cur = 0, v = 0, nud = 0;
        if (seq_step_get(&Q, trk, hold_step, &cur, &v, &nud) && !is_drum(t)) seq_step_note(&Q, trk, hold_step, n - cur);
        else step_write(hold_step, 1);
        hold_erase = 0;
    }
    if (mh[slot].arp) { arp_add(t, n, vel); return; }
    play_on(GB_OWNER(OWN_MIDI, ch), t, n, (float)vel * (1.0f / 127.0f));
    loop_append(t, n, vel, 1, lat);
    if (Q.recording && t == Q.rec_trk) seq_key(&Q, n, vel, 1, lat);
}
COLD static void edited(int t) { if (preset_of[t] >= 0) preset_of[t] = -preset_of[t] - 2; }
/* ---------------------------------------------------------------- MIDI LEARN (GLO + F#3), REMOTE, the FX channel */
static const char *ml_label(int t, int code, char *b)
{
    char *p = b; *p++ = 'T'; *p++ = (char)('1' + t); *p++ = ' ';
    p = put_s(p, code == ML_LEVEL ? "LVL" : code == ML_PAN ? "PAN" : code == ML_DLY ? "DLY" : code == ML_REV ? "REV" : code == ML_CHO ? "CHO"
              : pdesc_of(S.trk[t].patch.engine, code - 1)->name);
    *p = 0; return b;
}
COLD static void ml_knob(int t, int code)               /* a value turned while learning: the next CC is for it */
{
    if (!ml_on || code <= 0 || code > ML_CHO) return;
    if (!ml_pick || ml_trk != t || ml_code != code) { char b[24], c[32], *p; ml_label(t, code, b); p = put_s(c, b); p = put_s(p, ": SEND A CC"); *p = 0; say(c); }
    ml_pick = 1; ml_trk = (uint8_t)t; ml_code = (uint8_t)code;
}
static int ml_free_cc(int cc) { return cc && cc < 120 && cc != 1 && cc != 6 && cc != 11 && cc != 32 && cc != 38 && cc != 64 && (cc < 96 || cc > 101); }
COLD static int ml_count(void) { int n = 0; for (int i = 0; i < ML_N; i++) n += ml_tab[i] != 0; return n; }
COLD static int ml_cc(int cc, int val)                  /* a CC: learned now, or a learned one's job done (1) */
{
    if (!ml_free_cc(cc)) { if (ml_on && ml_pick) say_bad("CC CAN'T BE LEARNT"); return 0; }
    if (ml_on && ml_pick) {
        for (int i = 0; i < ML_N; i++) if (ml_tab[i] && ((ml_tab[i] & 127) == cc || (((ml_tab[i] >> 7) & 7) == ml_trk && (ml_tab[i] >> 10) == ml_code))) ml_tab[i] = 0;
        int i = 0; while (i < ML_N && ml_tab[i]) i++;
        if (i == ML_N) { say_bad("MIDI LEARN FULL"); return 1; }
        ml_tab[i] = (uint16_t)(cc | ml_trk << 7 | ml_code << 10);
        char b[24], c[40], *p = put_s(c, "CC "); p = put_u(p, (uint32_t)cc); p = put_s(p, " = "); p = put_s(p, ml_label(ml_trk, ml_code, b)); *p = 0; say(c);
        ml_pick = 0; t_input = time_ticks();
        return 1;
    }
    int hit = 0;
    float x = (float)val * (1.0f / 127.0f);
    for (int i = 0; i < ML_N; i++) {
        uint16_t e = ml_tab[i];
        if (!e || (e & 127) != cc) continue;
        int t = (e >> 7) & 7, c = e >> 10; gb_track *k = &S.trk[t]; hit = 1;
        if (t >= NTRK) continue;
        if (c == ML_LEVEL) sm_set(&k->level, x); else if (c == ML_PAN) sm_set(&k->pan, x);
        else if (c == ML_DLY) sm_set(&k->send_dly, x); else if (c == ML_REV) sm_set(&k->send_rev, x); else if (c == ML_CHO) sm_set(&k->send_cho, x);
        else if (c >= 1 && c <= SP_N) { base[t][c - 1] = k->patch.p[c - 1] = x; edited(t); }
    }
    return hit;
}
/* REMOTE: CC 102..115 on its channel are the buttons by their labels (FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC
 * OCT- OCT+), 64 and up held, below let go, as if pressed on the panel */
static uint32_t rm_held, rm_pr, rm_rel;
/* the web editor's panel (ED_LIVE): its buttons, keys and knobs, as the FM-1's own (3.1.5) */
static uint32_t eb_held, eb_pr, eb_rel, ek_held, ek_pr, ek_rel; static int8_t ee[7]; static uint8_t ed_thr; static int ed_mst = -1, ed_mst_pot;
COLD static void remote_cc(int k, int val)
{
    static const uint8_t ID[14] = { BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_GLO, BTN_HOME, BTN_SAVE, BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC, BTN_OCT_DOWN, BTN_OCT_UP };
    uint32_t b = 1u << ID[k];
    if (val >= 64) { if (!(rm_held & b)) rm_pr |= b; rm_held |= b; }
    else if (rm_held & b) { rm_held &= ~b; rm_rel |= b; }
}
/* the FX channel: its notes F3 .. G5 are the 27 keys with FX held, a note on holds its punch-in, its note off lets go */
static int mfx_key = -1;
COLD static void midi_fx(int note, int on)
{
    if (note < 53 || note > 79) return;
    int k = note - 53;
    if (on) { mfx_key = k; snd_punch(&S, punch_of_key(k)); }
    else if (mfx_key == k) { mfx_key = -1; if (punch_key < 0) snd_punch(&S, -1); }
}
static uint8_t ch_bank[16];                              /* Bank Select (CC 0) latched for the next Program Change */
COLD static void midi_control(int ch, int t, int cc, int val)
{
    float x = (float)val * (1.0f / 127.0f);
    gb_track *k = &S.trk[t];
    midi_cc_dest d = midi_cc(cc);
    switch (d.what) {
    case CC_PARAM:                                   /* a value of the sound, over its range (SLOOP's map) */
        if (is_drum(t) || d.param >= SP_N) return;
        base[t][d.param] = k->patch.p[d.param] = x; edited(t); break;
    case CC_BRIGHT: case CC_RES: {                   /* the engine's cutoff (else the track's filter), its resonance */
        int e = is_drum(t) ? -1 : d.what == CC_BRIGHT ? gb_edesc[k->patch.engine].cut : gb_edesc[k->patch.engine].res;
        if (e >= 0) { base[t][e] = k->patch.p[e] = x; edited(t); }
        else if (d.what == CC_BRIGHT) sm_set(&k->filt, x * 2.0f - 1.0f);   /* 64 off, below a low-pass, above a high-pass */
        break; }
    case CC_LEVEL: sm_set(&k->level, x); break;
    case CC_PAN: sm_set(&k->pan, x); break;
    case CC_SEND_DLY: sm_set(&k->send_dly, x); break;
    case CC_SEND_REV: sm_set(&k->send_rev, x); break;
    case CC_SEND_CHO: sm_set(&k->send_cho, x); break;
    case CC_SUSTAIN:
        if (val >= 64) { sus_on |= (uint16_t)(1u << ch); break; }
        sus_on &= (uint16_t)~(1u << ch);
        for (int i = 0; i < MH; i++) if (mh[i].on && mh[i].sus && mh[i].ch == ch) mh_end(i, latency_frames());
        break;
    case CC_ALL_OFF: midi_all_off(ch); break;
    case CC_RESET:
        k->bend = 0.0f; sus_on &= (uint16_t)~(1u << ch);
        for (int i = 0; i < MH; i++) if (mh[i].on && mh[i].sus && mh[i].ch == ch) mh_end(i, 0);
        break;
    default: break;
    }
}
static void apply_preset(int t, int np);
/* program n: the n-th sound in PRESETS' order (the factory's, then yours); Bank Select before it reaches further, as
 * on SLOOP: bank 1 the sounds 128..255, bank 8 your sounds straight (program n: your place n). The drum track: kit n */
/* (3.3.6) the track chosen (ALGORITHM, the editor). Recording, by GLO REC 6/6: REC stops (the default), or the take
 * goes on, on the new track */
COLD static void select_track(int t)
{
    if (t < 0 || t >= NTRK || t == trk) return;
    trk = t; page = 0; lock_param = is_drum(trk) ? 0 : SP_E4;
    if (!Q.recording) return;
    seq_record(&Q, 0, trk);
    char b[24], *p;
    if (rec_keep) { seq_record(&Q, 1, trk); p = put_s(b, "REC TRACK "); }
    else p = put_s(b, "REC STOPPED · TRACK ");
    p = put_u(p, (uint32_t)POS[trk] + 1u); *p = 0; say(b);
}
COLD static void midi_program(int t, int pc, int bank)
{
    if (is_drum(t)) {
        if (pc < GB_NKITS) { gb_patch_set(&S.trk[t].patch, SP_E0, pc); base[t][SP_E0] = S.trk[t].patch.p[SP_E0]; preset_of[t] = -1; }
        return;
    }
    if (bank == 8) { if (pc < US_N && us_used(pc)) apply_user(t, pc); return; }
    int n = bank * 128 + pc, count;
    for (int i = 0; i < gb_npresets; i++)
        if (gb_presets[i].kind != K_KIT && preset_rank(i, &count) == n + 1) { apply_preset(t, i); return; }
    int nf = 0; for (int i = 0; i < gb_npresets; i++) nf += gb_presets[i].kind != K_KIT;
    for (int k = 0, j = n - nf; k < US_N && j >= 0; k++) if (us_used(k) && !j--) { apply_user(t, k); return; }
}
COLD static void midi_in(const midi_msg *m, uint32_t now)
{
    midi_n++;
    if (m->st >= 0xF8) {                             /* the clock and the transport: with SYNC EXT */
        if (MC.sync != MIDI_SYNC_EXT) return;
        if (m->st == 0xF8) {
            if (ext_start) { ext_start = 0; transport(1); }   /* (then this pulse: the downbeat) */
            mclk_pulse(&MK, now, m->src);
            if (MK.bpm >= 30.0f && (MK.bpm - Q.bpm > 0.05f || Q.bpm - MK.bpm > 0.05f)) seq_set_bpm(&Q, MK.bpm);
        } else if (m->st == 0xFA || m->st == 0xFB) {      /* START: from the top; CONTINUE: on (or from the top) */
            if (m->st == 0xFA && Q.playing) transport(0);
            if (Q.playing) mclk_start(&MK); else ext_start = 1;
        } else if (m->st == 0xFC) { ext_start = 0; if (ltake.armed_trk >= 0) { loop_cancel(&ltake); recording_armed = 0; } transport(0); }
        return;
    }
    if (m->st >= 0xF0) return;
    int ch = m->st & 15, ty = m->st & 0xF0;
    if (m_rmch && ch == m_rmch - 1 && ty == 0xB0 && m->d1 >= 102 && m->d1 < 116) { remote_cc(m->d1 - 102, m->d2); return; }
    if (m_fxch && ch == m_fxch - 1 && (ty == 0x90 || ty == 0x80)) { midi_fx(m->d1, ty == 0x90 && m->d2); return; }
    if (ty == 0xB0 && m->d1 == 0) { ch_bank[ch] = m->d2; return; }
    if (ty == 0xB0 && MC.in != MIDI_IN_OFF && ml_cc(m->d1, m->d2)) return;   /* (a learned CC: the player's, any channel) */
    int t = midi_track_of(&MC, ch, trk, NTRK);
    if (t < 0) return;
    switch (m->st & 0xF0) {
    case 0x90: midi_note(ch, t, m->d1, m->d2); break;
    case 0x80: midi_note(ch, t, m->d1, 0); break;
    case 0xB0: midi_control(ch, t, m->d1, m->d2); break;
    case 0xE0: S.trk[t].bend = (float)((int)(m->d2 << 7 | m->d1) - 8192) * ((float)m_bend / 8192.0f); break;
    case 0xC0: midi_program(t, m->d1, ch_bank[ch]); break;
    default: break;
    }
}
/* the wires, read: often (the main loop, between audio blocks) */
static void midi_poll(void)
{
    usb_midi_poll();
    if (ed_tx_n) ed_pump();
    if (!flash_xip_on()) return;                    /* (a save under way: the messages wait in their queues) */
    uint32_t now = time_ticks();
    uint8_t pk[4]; midi_msg m;
    for (int k = 0; k < 32 && usb_midi_take(pk); k++) {
        int n = midi_usb_bytes(pk);
        for (int i = 0; i < n; i++) {
            int r = midi_byte(&mp_usb, pk[1 + i], &m);
            if (r == MIDI_MSG) { m.src = MIDI_FROM_USB; midi_in(&m, now); }
            else if (r == MIDI_SYSEX && midi_is_soft_key(&mp_usb)) sys_download_mode();   /* (M-Flasher's way in) */
            else if (r == MIDI_SYSEX && !ed_tx_n && mp_usb.sx_len <= sizeof ed_rx) ed_pending = mp_usb.sx_len;   /* the web editor (answered between the halves) */
        }
    }
    for (int k = 0; k < 96; k++) {
        int b = trs_midi_byte(); if (b < 0) break;
        if (midi_byte(&mp_trs, (uint8_t)b, &m) == MIDI_MSG) { m.src = MIDI_FROM_TRS; midi_in(&m, now); }
    }
}
COLD static void ed_request(void)
{
    uint32_t n = 0;
    int cmd = ed_parse(ed_rx, ed_pending, ed_raw, sizeof ed_raw, &n);
    ed_pending = 0;
    if (cmd < 0) return;
    ed_t_last = time_ticks();
    uint8_t *r = ed_raw;
    switch (cmd) {
    case ED_INFO: {
        static const char V[] = VERSION;
        r[0] = ED_PROTO; r[1] = NTRK; r[2] = GB_NPARAM; r[3] = (uint8_t)proj; ed_w16(r + 4, (int)proj_used);
        uint32_t um = 0; for (int k = 0; k < US_N; k++) if (us_used(k)) um |= 1u << k;
        ed_w32(r + 6, um);
        uint32_t i = 0; for (; V[i] && i < 31; i++) r[10 + i] = (uint8_t)V[i];
        r[10 + i] = 0; ed_send(cmd, 11 + i); return;
    }
    case ED_SOUND: case ED_SET: {
        int t = n ? r[0] : 255;
        if (t >= NTRK) { ed_err(ED_ERR_BAD); return; }
        int e = S.trk[t].patch.engine;
        if (cmd == ED_SET) {
            int k = n >= 4 ? r[1] : 255;
            if (k >= SP_N) { ed_err(ED_ERR_BAD); return; }
            const gb_pd *d = ed_pd(e, k);
            int v = (int16_t)(r[2] | r[3] << 8); v = v < d->min ? d->min : v > d->max ? d->max : v;
            float f = gb_nv(v, d->min, d->max);
            base[t][k] = f; S.trk[t].patch.p[k] = f; Q.t[t].lock_step = -1;
            if (preset_of[t] >= 0) preset_of[t] = -preset_of[t] - 2;
            r[1] = (uint8_t)k; ed_w16(r + 2, v); ed_send(cmd, 4); return;
        }
        r[1] = (uint8_t)e; ed_w16(r + 2, preset_of[t]);
        int po = preset_origin(preset_of[t]);
        const char *nm = po >= 0 ? snd_name(po) : "OWN SOUND";
        for (int i = 0; i < 16; i++) r[4 + i] = 0;
        for (int i = 0; i < 16 && nm[i]; i++) r[4 + i] = (uint8_t)nm[i];
        for (int k = 0; k < GB_NPARAM; k++) { const gb_pd *d = ed_pd(e, k); ed_w16(r + 20 + 2 * k, gb_iv(base[t][k], d->min, d->max)); }
        ed_send(cmd, 20 + 2 * GB_NPARAM); return;
    }
    case ED_DESC: {
        int e = n ? r[0] : 255;
        if (e >= ENG_N) { ed_err(ED_ERR_BAD); return; }
        const char *en = gb_edesc[e].name;
        for (int i = 0; i < 12; i++) r[1 + i] = 0;
        for (int i = 0; i < 12 && en[i]; i++) r[1 + i] = (uint8_t)en[i];
        for (int k = 0; k < GB_NPARAM; k++) {
            uint8_t *o = r + 13 + 12 * k; const gb_pd *d = ed_pd(e, k);
            const char *pn = k < 8 ? gb_edesc[e].p[k].name : k < SP_N ? SP_UI[k].name : "";
            ed_w16(o, d->min); ed_w16(o + 2, d->max);
            for (int i = 0; i < 8; i++) o[4 + i] = 0;
            for (int i = 0; i < 8 && pn && pn[i]; i++) o[4 + i] = (uint8_t)pn[i];
        }
        ed_send(cmd, 13 + 12 * GB_NPARAM); return;
    }
    case ED_READ: {
        if (n < 8) { ed_err(ED_ERR_BAD); return; }
        int kind = r[0], slot = r[1]; uint32_t off = ed_r32(r + 2), len = r[6] | r[7] << 8;
        if (saving || sbuf_held() || !flash_ok) { ed_err(ED_ERR_BUSY); return; }
        if (kind > 2 || (kind == 0 && slot != 0) || len > 384) { ed_err(ED_ERR_BAD); return; }
        if (!off || !ed_rd.ok || ed_rd.kind != kind || ed_rd.slot != slot) {   /* (the file's length and CRC, at its start) */
            if (kind == 0) {                                 /* the saved project: read from the flash as asked */
                if (store_proj_find(&ed_rd.at, &ed_rd.len, &ed_rd.crc)) { ed_rd.ok = 0; ed_err(ED_ERR_NONE); return; }
                ed_rd.wn = 0;
            } else if (kind == 1) {
                sbuf_used(); int m = store_sounds_load(sbuf, sizeof sbuf);
                if (m <= 0) { ed_rd.ok = 0; ed_err(ED_ERR_NONE); return; }
                ed_rd.len = (uint32_t)m; ed_rd.crc = crc32_ieee(0, sbuf, (uint32_t)m); ed_rd.lo = 0; ed_rd.wn = (uint32_t)m;
            } else { ed_rd.len = proj_measure(&ed_rd.crc); ed_rd.wn = 0; }   /* the project now: made a part at a time */
            ed_rd.ok = 1; ed_rd.kind = (uint8_t)kind; ed_rd.slot = (uint8_t)slot;
        }
        if (off > ed_rd.len) { ed_err(ED_ERR_BAD); return; }
        if (len > ed_rd.len - off) len = ed_rd.len - off;
        ed_w32(r + 6, ed_rd.len); ed_w32(r + 10, ed_rd.crc);
        if (kind == 0) {
            uint32_t fb[(384 + 8) / 4];
            uint32_t a = ed_rd.at + off, a0 = a & ~3u, pre = a - a0;
            if (len && flash_read(a0, fb, (pre + len + 3u) & ~3u)) { ed_err(ED_ERR_FLASH); return; }
            memcpy(r + 14, (const uint8_t *)fb + pre, len);
        } else {
            if (kind == 2 && (off < ed_rd.lo || off + len > ed_rd.lo + ed_rd.wn)) {
                ed_rd.lo = off; ed_rd.wn = ed_rd.len - off > (uint32_t)sizeof sbuf ? (uint32_t)sizeof sbuf : ed_rd.len - off;
                proj_part(off, ed_rd.wn);
            }
            memcpy(r + 14, sbuf + (off - ed_rd.lo), len);
        }
        ed_send(cmd, 14 + len); return;
    }
    case ED_WBEGIN: {
        if (ed_bk.on || ed_bk.phase) { ed_err(ED_ERR_BUSY); return; }
        if (n < 6) { ed_err(ED_ERR_BAD); return; }
        int kind = r[0], slot = r[1]; uint32_t total = ed_r32(r + 2);
        if (saving || ed_sm.on || !flash_ok) { ed_err(ED_ERR_BUSY); return; }
        if (kind > 1 || (kind == 0 && (slot != 0 || total < 16 || total > STORE_PMAX)) || (kind == 1 && !US_LEN_OK(total))) { ed_err(ED_ERR_BAD); return; }
        if (kind == 0) {                                     /* (3.3.5) a project: to the flash as it comes, its bank erased first */
            if (sbuf_held() || store_proj_begin(total, 0, 0)) { ed_err(ED_ERR_BUSY); return; }
            saving = 1; saving_shown = 1; saving_snd = 0; ed_save_slot = -1;
        }
        ed_wr.on = 1; ed_wr.kind = (uint8_t)kind; ed_wr.slot = (uint8_t)slot; ed_wr.total = total; ed_wr.got = 0; ed_rd.ok = 0;
        ed_wr.fill = 0; ed_wr.st_n[0] = ed_wr.st_n[1] = 0;
        ed_send(cmd, 6); return;
    }
    case ED_WDATA: {
        if (!ed_wr.on || n < 4) { ed_err(ED_ERR_BAD); return; }
        uint32_t off = ed_r32(r), m = n - 4;
        if (off != ed_wr.got || off + m > ed_wr.total) { ed_err(ED_ERR_BAD); return; }
        if (ed_wr.kind == 0) {                               /* (3.3.5) into the half being filled; both full: again later */
            ed_proj_pump();
            if (ed_wr.st_n[ed_wr.fill] + m > ED_HALF) { ed_err(ED_ERR_BUSY); return; }
            memcpy(sbuf + (uint32_t)ed_wr.fill * 6144u + ed_wr.st_n[ed_wr.fill], r + 4, m); ed_wr.st_n[ed_wr.fill] += m; ed_wr.got += m;
            ed_proj_pump();
            ed_w32(r + 4, ed_wr.got); ed_send(cmd, 8); return;
        }
        memcpy(sbuf + off, r + 4, m); ed_wr.got += m;
        ed_w32(r + 4, ed_wr.got); ed_send(cmd, 8); return;
    }
    case ED_WEND: {
        if (!ed_wr.on || n < 4) { ed_err(ED_ERR_BAD); return; }
        if (ed_wr.kind == 0) {                               /* (3.3.5) all in the flash: checked there, then its header */
            if (ed_wr.got != ed_wr.total) { ed_wr.on = 0; store_proj_abort(); saving = 0; ed_err(ED_ERR_CRC); return; }
            ed_proj_pump();
            if (store_proj_given() != ed_wr.total || !store_proj_written() || flash_busy()) { ed_err(ED_ERR_BUSY); return; }   /* (being written: again) */
            ed_wr.on = 0;
            uint32_t keep = proj_at; proj_at = store_proj_target();
            int bad = project_load_from(0, 0, 0, proj_fetch, ed_wr.total, 1);
            proj_at = keep;
            if (bad || store_proj_end(ed_r32(r))) { store_proj_abort(); saving = 0; ed_err(ED_ERR_FILE); return; }
            ed_save_slot = 0;                                /* (read back; if its CRC is the one sent, the header: then opened) */
            ed_send(cmd, 4); return;
        }
        ed_wr.on = 0;
        if (ed_wr.got != ed_wr.total || crc32_ieee(0, sbuf, ed_wr.total) != ed_r32(r)) { ed_err(ED_ERR_CRC); return; }
        if (saving) { ed_err(ED_ERR_BUSY); return; }
        {
            if (sbuf[0] != 'S' || sbuf[1] != '3' || sbuf[2] != 'U' || sbuf[3] != 'S') { ed_err(ED_ERR_FILE); return; }
            if (store_sounds_begin(sbuf, ed_wr.total)) { ed_err(ED_ERR_FLASH); return; }
            us_from_bytes(sbuf, (int)ed_wr.total);
            saving = 1; saving_shown = 1; saving_snd = 1;
        }
        ed_send(cmd, 4); return;
    }
    case ED_LOAD: {
        int slot = n ? r[0] : 255;
        if (slot != 0) { ed_err(ED_ERR_BAD); return; }
        if (saving || sbuf_held()) { ed_err(ED_ERR_BUSY); return; }
        load_project(slot); ed_rd.ok = 0;
        ed_send(cmd, 1); return;
    }
    case ED_PATTERN: {                                        /* a pattern's notes (the editor's MIDI files) */
        int t = n >= 4 ? r[0] : 255, pt = r[1], first = r[2] | r[3] << 8;
        if (t >= NTRK) { ed_err(ED_ERR_BAD); return; }
        if (pt == 255) pt = Q.t[t].cur;
        if (pt >= SEQ_NPAT) { ed_err(ED_ERR_BAD); return; }
        const seq_pat *sp = &Q.t[t].pat[pt];
        int wide = n >= 5 && r[4] == 1, w = wide ? 8 : 6;            /* (protocol 2: + cond, ratchet; the locks after the last) */
        int cnt = sp->n - first; cnt = cnt < 0 ? 0 : cnt > (wide ? 60 : 80) ? (wide ? 60 : 80) : cnt;
        r[0] = (uint8_t)t; r[1] = (uint8_t)pt; ed_w16(r + 2, sp->steps); ed_w16(r + 4, (int)(Q.bpm + 0.5f)); ed_w16(r + 6, sp->n); ed_w16(r + 8, first); r[10] = (uint8_t)cnt;
        for (int i = 0; i < cnt; i++) { const seq_ev *ev = &sp->ev[first + i]; uint8_t *o = r + 11 + w * i; ed_w16(o, ev->tick); ed_w16(o + 2, ev->len); o[4] = ev->note; o[5] = ev->vel; if (wide) { o[6] = ev->cond; o[7] = ev->ratchet; } }
        uint32_t len = 11 + (uint32_t)(w * cnt);
        if (wide && first + cnt >= sp->n) { r[len++] = sp->nlock; for (int i = 0; i < sp->nlock; i++) { r[len++] = sp->lk[i].step; r[len++] = sp->lk[i].param; r[len++] = sp->lk[i].value; } }
        ed_send(cmd, len); return;
    }
    case ED_THEME: {                                          /* your colour theme */
        if (n >= 40) {
            uint8_t *t = r; uint32_t c = crc32_ieee(0, t, 36);
            if ((t[0] | t[1] << 8 | t[2] << 16 | (uint32_t)t[3] << 24) != 0x324D4854u || (t[36] | t[37] << 8 | t[38] << 16 | (uint32_t)t[39] << 24) != c) { ed_err(ED_ERR_FILE); return; }
            if (saving || !us_bank()) { ed_err(ED_ERR_BUSY); return; }
            for (int i = 0; i < 40; i++) us_theme[i] = t[i];
            us_fav_put(sbuf);
            if (store_sounds_begin(sbuf, US_FILE)) { ed_err(ED_ERR_FLASH); return; }
            saving = 1; saving_shown = 0; saving_snd = 1;
            scr_color = UI_NTHEMES; color_apply();
        }
        r[0] = scr_color; for (int i = 0; i < 40; i++) r[1 + i] = us_theme[i];
        ed_send(cmd, 41); return;
    }
    case ED_NOTES: {                                          /* a pattern rewritten (the piano roll) */
        if (n < 7) { ed_err(ED_ERR_BAD); return; }
        int t = r[0], pt = r[1], first = r[2] | r[3] << 8, steps = r[4] | r[5] << 8, cnt = r[6];
        if (t >= NTRK || (pt != 255 && pt >= SEQ_NPAT) || (uint32_t)(7 + 6 * cnt) > n) { ed_err(ED_ERR_BAD); return; }
        if (pt == 255) pt = Q.t[t].cur;
        int tot = seq_pat_put(&Q, t, pt, first, steps, r + 7, cnt);
        r[1] = (uint8_t)pt; ed_w16(r + 2, tot); ed_send(cmd, 4); return;
    }
    case ED_BANK: {                                           /* your FM6 bank and SYN kits (app/xbank.h) */
        int sub = n ? r[0] : 255, which = n > 1 ? r[1] : 255;
        if (sub == 0) { r[1] = fm6_src; ed_w32(r + 2, fm6_bank_at ? fm6_bank_used : 0u); r[6] = syn_src; ed_send(cmd, 7); return; }
        if (sub == 5) {                                       /* a SYN kit heard at once */
            if (which > 3 || n < 2 + XB_KIT_LEN) { ed_err(ED_ERR_BAD); return; }
            xb_kit_unpack(r + 2, gb_dsyn[which], &gb_dsyn_crush[which]); drum_syn_forget(which);
            ed_send(cmd, 2); return;
        }
        if (sub == 6) {                                       /* an FM6 voice heard at once, where that slot plays */
            if (which >= (int)XB_NFM6 || n < 2 + 128) { ed_err(ED_ERR_BAD); return; }
            int c = 0;
            for (int t = 0; t < NTRK; t++) {
                if (S.trk[t].patch.engine != ENG_FM6 || fm6_ptch(t) != FM6_NFACTORY + which) continue;
                fm6_user_set(t, FM6_NFACTORY + which, r + 2); fm6_tried[t] = (int8_t)(FM6_NFACTORY + which); c++;
            }
            r[2] = (uint8_t)c; ed_send(cmd, 3); return;
        }
        if (which > 1) { ed_err(ED_ERR_BAD); return; }
        uint32_t len = which ? XB_SYN_LEN : XB_FM6_LEN;
        if (sub == 1) {                                       /* the image as it plays now */
            if (n < 6) { ed_err(ED_ERR_BAD); return; }
            if (saving || ed_wr.on || ed_sm.on || ed_bk.on || ed_bk.phase) { ed_err(ED_ERR_BUSY); return; }
            uint32_t off = r[2] | r[3] << 8, m = r[4] | r[5] << 8;
            if (off > len) { ed_err(ED_ERR_BAD); return; }
            if (m > 384) m = 384;
            if (m > len - off) m = len - off;
            uint8_t *img = sbuf;
            if (which) for (int k = 0; k < 4; k++) xb_kit_pack(gb_dsyn[k], gb_dsyn_crush[k], img + 16 + k * XB_KIT_LEN);
            else for (uint32_t k = 0; k < XB_NFM6; k++) {
                uint8_t *v = img + 16 + 128 * k;
                if (!fm6_bank_at || !((fm6_bank_used >> k) & 1u) || flash_read(fm6_bank_at + 128u * k, v, 128)) for (int i = 0; i < 128; i++) v[i] = 0;
            }
            xb_head(img, !which, fm6_bank_at ? fm6_bank_used : 0u, len);
            ed_w16(r + 2, (int)off); ed_w16(r + 4, (int)len);
            for (uint32_t i = 0; i < m; i++) r[6 + i] = img[off + i];
            ed_send(cmd, 6 + m); return;
        }
        if (sub == 2) {
            if (saving || ed_wr.on || ed_sm.on || ed_bk.phase) { ed_err(ED_ERR_BUSY); return; }
            if (n < 4 || (uint32_t)(r[2] | r[3] << 8) != len) { ed_err(ED_ERR_BAD); return; }
            ed_bk.on = 1; ed_bk.which = (uint8_t)which; ed_bk.total = len; ed_bk.got = 0;
            ed_send(cmd, 2); return;
        }
        if (sub == 3) {
            if (!ed_bk.on || ed_bk.which != which || n < 4) { ed_err(ED_ERR_BAD); return; }
            uint32_t off = r[2] | r[3] << 8, m = n - 4;
            if (off != ed_bk.got || off + m > ed_bk.total) { ed_err(ED_ERR_BAD); return; }
            memcpy(sbuf + off, r + 4, m); ed_bk.got += m;
            ed_w16(r + 2, (int)ed_bk.got); ed_send(cmd, 4); return;
        }
        if (sub == 4) {
            if (!ed_bk.on || ed_bk.which != which) { ed_err(ED_ERR_BAD); return; }
            ed_bk.on = 0;
            if (ed_bk.got != ed_bk.total || xb_check(sbuf, ed_bk.total, !which, 0)) { ed_err(ED_ERR_CRC); return; }
            if (saving) { ed_err(ED_ERR_BUSY); return; }
            ed_rep[0] = 4; ed_rep[1] = (uint8_t)which; ed_rep_n = 2; ed_rep_cmd = cmd;
            if (ed_job(which ? XB_SYN_AT : XB_FM6_AT, 0, sbuf, ed_bk.total, 1)) { ed_err(ED_ERR_FLASH); return; }
            ed_bk.phase = 1; return;
        }
        ed_err(ED_ERR_BAD); return;
    }
    case ED_SAMPLE: {                                         /* your samples: USR1..USR4 */
        int sub = n ? r[0] : 255, k = n > 1 ? r[1] : 255;
        if (sub == 0) {                                       /* what each slot holds */
            r[0] = 0; r[1] = SMP_NUSR; r[2] = (uint8_t)(SMP_USR_SIZE / 1024u);
            for (int i = 0; i < SMP_NUSR; i++) {
                uint8_t *o = r + 3 + 11 * i; char nm[10]; smp_user_name(i, nm);
                o[0] = (uint8_t)smp_user_nz(i); for (int j = 0; j < 8; j++) o[1 + j] = 0;
                for (int j = 0; j < 8 && nm[j]; j++) o[1 + j] = (uint8_t)nm[j];
                const uint8_t *h = (const uint8_t *)(uintptr_t)(FLASH_WIN + SLOT_AT[i] - 0x4000u);
                uint32_t len = o[0] ? (uint32_t)(h[16] | h[17] << 8 | h[18] << 16 | (uint32_t)h[19] << 24) : 0u;
                ed_w16(o + 9, (int)((len + 1023u) / 1024u));
            }
            ed_send(cmd, 3 + 11 * SMP_NUSR); return;
        }
        if (k >= SMP_NUSR || !flash_ok) { ed_err(ED_ERR_BAD); return; }
        if (saving || ed_bk.on || ed_bk.phase || ed_wr.on) { ed_err(ED_ERR_BUSY); return; }
        ed_rep[0] = (uint8_t)sub; ed_rep[1] = (uint8_t)k; ed_rep_n = 2; ed_rep_cmd = cmd;
        if (sub == 1 || sub == 4) {                           /* BEGIN, ERASE: the slot out, its header's sector erased */
            smp_user_drop(k); snd_kill(&S);
            ed_sm.on = sub == 1; ed_sm.k = (uint8_t)k; ed_sm.cur = 0; ed_sm.got = 0; ed_sm.crc = 0; ed_sm.phase = 0; ed_sm.buf = 0; ed_sm.half = 0;
            for (int i = 0; i < 8192; i++) sbuf[i] = 0xFF;
            if (ed_job(SLOT_AT[k], 0, sbuf, 0, 1)) { ed_sm.on = 0; ed_err(ED_ERR_FLASH); }
            return;
        }
        if (!ed_sm.on || ed_sm.k != k) { ed_err(ED_ERR_BAD); return; }
        if (sub == 2) {                                       /* DATA: off (from the data's start), bytes */
            if (n < 6) { ed_err(ED_ERR_BAD); return; }
            uint32_t off = ed_r32(r + 2), m = n - 6;
            if (off != ed_sm.got || 512u + off + m > SMP_USR_SIZE) { ed_err(ED_ERR_BAD); return; }
            const uint8_t *d = r + 6;
            ed_sm.crc = crc32_ieee(ed_sm.crc, d, m);
            uint32_t abs = 512u + off, flush = 0xFFFFFFFFu, fbuf = 0;
            for (uint32_t i = 0; i < m; i++, abs++) {
                uint32_t sec = abs / 4096u, at = abs % 4096u;
                if (sec != ed_sm.cur) { ed_sm.cur = (uint8_t)sec; ed_sm.buf ^= 1u; for (int j = 0; j < 4096; j++) sbuf[ed_sm.buf * 4096u + (uint32_t)j] = 0xFF; }
                sbuf[ed_sm.buf * 4096u + at] = d[i];
                if (at == 4095u) { flush = sec; fbuf = ed_sm.buf; }
            }
            ed_sm.got += m;
            ed_w32(ed_rep + 2, ed_sm.got); ed_rep_n = 6;
            if (flush != 0xFFFFFFFFu) {                       /* a sector full: written (the next one's bytes in the other half) */
                uint32_t from = flush ? 0u : 512u;
                if (ed_job(SLOT_AT[k] + flush * 4096u, from, sbuf + fbuf * 4096u + from, 4096u - from, flush != 0)) { ed_sm.on = 0; ed_err(ED_ERR_FLASH); }
                return;
            }
            for (uint32_t i = 0; i < ed_rep_n; i++) r[i] = ed_rep[i];
            ed_send(cmd, ed_rep_n); return;
        }
        if (sub == 3) {                                       /* END: the header (480 bytes): checked, then the last data, then it */
            if (n < 2 + 480) { ed_err(ED_ERR_BAD); return; }
            uint8_t *h = sbuf + 8192;
            for (int i = 0; i < 480; i++) h[i] = r[2 + i];
            uint32_t len = h[16] | h[17] << 8 | h[18] << 16 | (uint32_t)h[19] << 24, crc = h[20] | h[21] << 8 | h[22] << 16 | (uint32_t)h[23] << 24;
            if (!smp_user_head_ok(h) || len != ed_sm.got || crc != ed_sm.crc) { ed_sm.on = 0; ed_err(ED_ERR_CRC); return; }
            uint32_t abs = 512u + ed_sm.got;
            ed_sm.phase = 1;
            if (abs % 4096u) {                                /* the last sector, part full */
                uint32_t sec = abs / 4096u, from = sec ? 0u : 512u;
                if (ed_job(SLOT_AT[k] + sec * 4096u, from, sbuf + ed_sm.buf * 4096u + from, abs % 4096u - from, sec != 0)) { ed_sm.on = 0; ed_err(ED_ERR_FLASH); }
                return;
            }
            ed_sm.phase = 2;
            if (ed_job(SLOT_AT[k], 0, h, 480, 0)) { ed_sm.on = 0; ed_err(ED_ERR_FLASH); }
            return;
        }
        ed_err(ED_ERR_BAD); return;
    }
    case ED_NOTES2: {                                         /* a pattern rewritten, cond and ratchet too (8 bytes a note) */
        if (n < 7) { ed_err(ED_ERR_BAD); return; }
        int t = r[0], pt = r[1], first = r[2] | r[3] << 8, steps = r[4] | r[5] << 8, cnt = r[6];
        if (t >= NTRK || (pt != 255 && pt >= SEQ_NPAT) || (uint32_t)(7 + 8 * cnt) > n) { ed_err(ED_ERR_BAD); return; }
        if (pt == 255) pt = Q.t[t].cur;
        int tot = seq_pat_put2(&Q, t, pt, first, steps, r + 7, cnt, 8);
        r[1] = (uint8_t)pt; ed_w16(r + 2, tot); ed_send(cmd, 4); return;
    }
    case ED_LOCKS: {                                          /* a pattern's locks, all of them */
        if (n < 3) { ed_err(ED_ERR_BAD); return; }
        int t = r[0], pt = r[1] == 255 && r[0] < NTRK ? Q.t[r[0]].cur : r[1], cnt = r[2];
        if (t >= NTRK || pt >= SEQ_NPAT || (uint32_t)(3 + 3 * cnt) > n) { ed_err(ED_ERR_BAD); return; }
        seq_locks_put(&Q, t, pt, r + 3, cnt);
        r[1] = (uint8_t)pt; r[2] = Q.t[t].pat[pt].nlock; ed_send(cmd, 3); return;
    }
    case ED_SONG: {                                           /* the song: read (first), or written (first, total, entries) */
        if (n < 1) { ed_err(ED_ERR_BAD); return; }
        int first = r[0];
        if (n >= 4) {                                         /* write: first, total, len (bars, u16), then 8 bytes an entry */
            int total = r[1], cnt = (int)(n - 4) / 8;
            if (total > SEQ_SONG || first + cnt > SEQ_SONG) { ed_err(ED_ERR_BAD); return; }
            for (int i = 0; i < cnt; i++) {
                const uint8_t *o = r + 4 + 8 * i; seq_song_ev *e = &Q.song[first + i];
                e->bar = (uint16_t)(o[0] | o[1] << 8); e->mute = o[2];
                for (int k = 0; k < SEQ_NTRK; k++) e->pat[k] = (uint8_t)(o[3 + k] < SEQ_NPAT ? o[3 + k] : 0);
            }
            Q.song_n = (uint16_t)total; Q.song_len = (uint16_t)(r[2] | r[3] << 8);
        }
        int cnt = Q.song_n - first; cnt = cnt < 0 ? 0 : cnt > 60 ? 60 : cnt;
        r[0] = (uint8_t)first; r[1] = (uint8_t)Q.song_n; ed_w16(r + 2, Q.song_len); r[4] = Q.song_play; r[5] = Q.song_rec; r[6] = (uint8_t)cnt;
        for (int i = 0; i < cnt; i++) { const seq_song_ev *e = &Q.song[first + i]; uint8_t *o = r + 7 + 8 * i; ed_w16(o, e->bar); o[2] = e->mute; for (int k = 0; k < SEQ_NTRK; k++) o[3 + k] = e->pat[k]; }
        ed_send(cmd, 7 + 8 * (uint32_t)cnt); return;
    }
    case ED_HOME: {                                           /* the device's settings: read, or one set (id, value) */
        if (n >= 2) {
            int id = r[0], v = r[1];
            if (id < HS_N) { if (v <= HS_MAX[id]) { hs[id] = (uint8_t)v; home_apply(); } }
            else switch (id) {
            case 32: MC.in = (uint8_t)v; break; case 33: MC.sync = (uint8_t)v; break; case 34: MC.out = (uint8_t)v; break;
            case 35: MC.clk_out = (uint8_t)v; break; case 36: MC.dmap = (uint8_t)v; break;
            case 37: m_rmch = (uint8_t)(v <= 16 ? v : 0); break; case 38: m_fxch = (uint8_t)(v <= 16 ? v : 0); break;
            case 39: if (v >= 1 && v <= 12) m_bend = (uint8_t)v; break;
            case 40: if (v <= UI_NTHEMES && (v < UI_NTHEMES || theme_ok())) { scr_color = (uint8_t)v; color_apply(); } break;
            case 41: if (v < NDIV) dly_div = v; break;
            case 42: case 43: case 44: case 45: if (v <= 127) { fxv[id - 42] = (uint8_t)v; fxv_apply(); } break;
            case 46: rec_mode = (uint8_t)(v == 1); break; case 47: rec_bars = (uint8_t)(v == 2 || v == 4 ? v : 1); break; case 48: rec_count = (uint8_t)(v == 1); break; case 49: rec_keep = (uint8_t)(v == 1); break;
            default: break;
            }
            midi_cfg_check(&MC);
        }
        for (int i = 0; i < HS_N; i++) r[i] = hs[i];
        uint8_t *o = r + 32;
        o[0] = MC.in; o[1] = MC.sync; o[2] = MC.out; o[3] = MC.clk_out; o[4] = MC.dmap; o[5] = m_rmch; o[6] = m_fxch; o[7] = m_bend; o[8] = scr_color;
        o[9] = (uint8_t)dly_div; for (int i = 0; i < 4; i++) o[10 + i] = fxv[i];
        o[14] = rec_mode; o[15] = rec_bars; o[16] = rec_count; o[17] = (uint8_t)cal_ok; o[18] = (uint8_t)ml_count(); o[19] = HS_N;
        for (int i = 0; i < HS_N; i++) o[20 + i] = HS_MAX[i];
        o[20 + HS_N] = rec_keep;                              /* (3.3.6, after the maxima: GLO REC 6/6) */
        ed_send(cmd, 32 + 20 + HS_N + 1); return;
    }
    case ED_NAMES: {                                          /* lists: 0 the factory sounds, 1 the kits, 2 yours, 3 the kinds */
        if (n < 2) { ed_err(ED_ERR_BAD); return; }
        int kind = r[0], first = r[1] | (n >= 3 ? r[2] << 8 : 0), total = kind == 0 ? gb_npresets : kind == 1 ? GB_NKITS : kind == 2 ? US_N : K_N;
        int cnt = total - first; cnt = cnt < 0 ? 0 : cnt > 36 ? 36 : cnt;
        r[0] = (uint8_t)kind; ed_w16(r + 1, first); ed_w16(r + 3, total); r[5] = (uint8_t)cnt;
        for (int i = 0; i < cnt; i++) {
            uint8_t *o = r + 6 + 15 * i; int k = first + i; const char *nm = "";
            o[0] = o[1] = 0;
            if (kind == 0) { nm = gb_presets[k].name; o[0] = gb_presets[k].kind; o[1] = gb_presets[k].engine; }
            else if (kind == 1) nm = gb_dkit_names[k];
            else if (kind == 2) { nm = us_name(k); o[0] = (uint8_t)us_used(k); }
            else nm = gb_kind_name[k];
            int j = 0; for (; j < 13 && nm && nm[j]; j++) o[2 + j] = (uint8_t)nm[j];
            for (; j < 13; j++) o[2 + j] = 0;
        }
        ed_send(cmd, 6 + 15 * (uint32_t)cnt); return;
    }
    case ED_LIVE: {                                           /* the LIVE page: an action (op, args), then the state */
        int op = n ? r[0] : 0;
        int a = n > 1 ? r[1] : 0, b = n > 2 ? r[2] : 0, c = n > 3 ? r[3] : 0;
        switch (op) {
        case 1: if (a < 14) { uint32_t m = 1u << a; if (b) { if (!(eb_held & m)) eb_pr |= m; eb_held |= m; } else if (eb_held & m) { eb_held &= ~m; eb_rel |= m; } } break;   /* a button */
        case 2: if (a < 27) { uint32_t m = 1u << a; if (b) { if (!(ek_held & m)) ek_pr |= m; ek_held |= m; } else if (ek_held & m) { ek_held &= ~m; ek_rel |= m; } } break;   /* a key */
        case 3: if (a < 7) ee[a] = (int8_t)(ee[a] + (int8_t)b); break;                                    /* a knob, b clicks */
        case 4: if (a < NTRK) {                                                                           /* the mix */
                gb_track *k = &S.trk[a]; float x = (float)c * (1.0f / 127.0f);
                if (b == 0) { sm_set(&k->level, x); if (Q.t[a].mute) seq_mute(&Q, a, 0); }
                else if (b == 1) sm_set(&k->pan, x); else if (b == 2) sm_set(&k->send_cho, x); else if (b == 3) sm_set(&k->send_dly, x);
                else if (b == 4) sm_set(&k->send_rev, x); else if (b == 5) sm_set(&k->filt, ((float)c - 64.0f) * (1.0f / 63.0f));
                else if (b == 6) seq_mute(&Q, a, c != 0); else if (b == 7) solo[a] = (uint8_t)(c != 0);
                else if (b == 8 && !is_drum(a)) { gb_patch_set(&k->patch, SP_DIST, c); base[a][SP_DIST] = k->patch.p[SP_DIST]; }
            } break;
        case 5:   /* PLAY: a = 0 stop (cancel the take), a = 1 start (preserve it as a follow-take), a >= 2 toggle (cancel the take, no toggle) */
            if (a == 0) { if (ltake.armed_trk >= 0) { loop_cancel(&ltake); recording_armed = 0; } transport(0); }
            else if (a == 1) transport(1);
            else if (ltake.armed_trk >= 0) { loop_cancel(&ltake); recording_armed = 0; }
            else transport(!Q.playing);   /* no take open: toggle as today (the editor's PLAY is a = 2) */
            break;
        case 6: if (b < SEQ_NPAT) seq_queue(&Q, a < NTRK ? a : -1, b); break;                            /* a pattern, next */
        case 7: if (a < 27) { mfx_key = a; snd_punch(&S, punch_of_key(a)); } else { mfx_key = -1; if (punch_key < 0) snd_punch(&S, -1); } break;
        case 8: ed_mst = a < 127 ? a : -1; break;                                                         /* the master */
        case 9: if (a < NTRK && a != trk) select_track(a); break;
        case 10: if (a < NTRK) { int k = b | c << 8; if (k >= UK_BASE) apply_ukit(a, k - UK_BASE); else if (k >= FB_BASE) apply_bank(a, k - FB_BASE); else if (k >= US_BASE) { if (!is_drum(a)) apply_user(a, k - US_BASE); } else if (k < gb_npresets && (gb_presets[k].kind == K_KIT) == is_drum(a)) apply_preset(a, k); } break;
        case 11: { int dt = -1; for (int t = 0; t < NTRK; t++) if (is_drum(t)) dt = t;      /* a drum pad's TUNE, DECAY */
                 if (dt >= 0 && a < 16) { gb_patch *p = &S.trk[dt].patch; int tu = (int8_t)b, de = (int8_t)c; p->dtune[a] = (int8_t)(tu < -8 ? -8 : tu > 7 ? 7 : tu); p->ddec[a] = (int8_t)(de < -4 ? -4 : de > 3 ? 3 : de); } } break;
        case 12: ed_thr = (uint8_t)(a & 3u); break;                                                        /* the throws */
        case 13: { int bp = a | b << 8; if (bp >= 300 && bp <= 3000) seq_set_bpm(&Q, (float)bp * 0.1f); if (c <= 100) Q.swing = (uint8_t)c; } break;
        case 14: if (a <= 100) S.dust = (float)a * 0.01f; if (b <= 100) S.duck = (float)b * 0.01f; if (c <= 200) sm_set(&S.mfilt, ((float)c - 100.0f) * 0.01f); break;
        case 15: if (a == 0 || a == 1) seq_fill(&Q, a); else if (a == 2) seq_fill_next_bar(&Q); break;
        case 16: if (a < NTRK) { seq_track *st = &Q.t[a];                                                  /* a track's DIV GATE SWING QUANT, its pattern's length */
                 if (b == 0 && c < SEQ_NDIV) st->div = (uint8_t)c; else if (b == 1 && c >= 10 && c <= 200) st->gate = (uint8_t)c;
                 else if (b == 2 && c <= 100) st->swing = (uint8_t)c; else if (b == 3 && c <= 100) st->quant = (uint8_t)c;
                 else if (b == 4 && c >= 1 && c <= SEQ_MAXSTEPS) st->pat[st->cur].steps = (uint16_t)c; } break;
        default: break;
        }
        /* the state */
        uint8_t *o = r + 1;
        o[0] = (uint8_t)((Q.playing ? 1 : 0) | (Q.recording ? 2 : 0) | (recording_armed ? 4 : 0) | (Q.song_play ? 8 : 0) | (Q.song_rec ? 16 : 0) | (Q.fill ? 32 : 0) | (countin ? 64 : 0));
        ed_w16(o + 1, (int)(Q.bpm * 10.0f + 0.5f));
        uint32_t bar = (uint32_t)(Q.pos / ((uint64_t)SEQ_BAR << 16)), beat = (uint32_t)((Q.pos >> 16) % SEQ_BAR) / (4 * SEQ_TPS);
        ed_w16(o + 3, (int)bar); o[5] = (uint8_t)beat; o[6] = (uint8_t)trk; o[7] = (uint8_t)proj; o[8] = (uint8_t)proj_used;
        o[9] = (uint8_t)load_avg; o[10] = (uint8_t)(load_peak > 255 ? 255 : load_peak); o[11] = (uint8_t)snd_voices(&S);
        o[12] = (uint8_t)(ed_mst < 0 ? 255 : ed_mst); o[13] = (uint8_t)(mfx_key >= 0 ? mfx_key : punch_key >= 0 ? punch_key : 255);
        o[14] = (uint8_t)(S.dust * 100.0f + 0.5f); o[15] = (uint8_t)(S.duck * 100.0f + 0.5f); o[16] = (uint8_t)(S.mfilt.tgt * 100.0f + 100.5f);
        o[17] = Q.swing; o[18] = ed_thr; o[19] = (uint8_t)msg_gen; o[20] = (uint8_t)(msg_live ? 1 + msg_bad : info_live ? 1 : 0);
        { const char *m = msg_live ? msg : info; for (int i = 0; i < 24; i++) o[21 + i] = (uint8_t)m[i]; }   /* (the editor shows both kinds) */
        uint8_t *q = o + 45;
        for (int t = 0; t < NTRK; t++, q += 22) {
            const gb_track *k = &S.trk[t]; const seq_track *st = &Q.t[t];
            q[0] = st->cur; q[1] = st->next; q[2] = st->mute; q[3] = solo[t];
            q[4] = (uint8_t)(k->level.tgt * 127.0f + 0.5f); q[5] = (uint8_t)(k->pan.tgt * 127.0f + 0.5f);
            q[6] = (uint8_t)(k->send_cho.tgt * 127.0f + 0.5f); q[7] = (uint8_t)(k->send_dly.tgt * 127.0f + 0.5f); q[8] = (uint8_t)(k->send_rev.tgt * 127.0f + 0.5f);
            q[9] = (uint8_t)(k->filt.tgt * 63.0f + 64.5f); q[10] = k->patch.engine; ed_w16(q + 11, preset_of[t] < -1 ? 0x8000 | preset_origin(preset_of[t]) : preset_of[t] < 0 ? 0xFFFF : preset_of[t]);
            float mt = k->meter; q[13] = (uint8_t)(mt <= 0.0f ? 0 : mt >= 1.0f ? 127 : (int)(mt * 127.0f));
            uint16_t used = 0; for (int i = 0; i < SEQ_NPAT; i++) if (st->pat[i].n) used |= (uint16_t)(1u << i);
            ed_w16(q + 14, used); q[16] = (uint8_t)(Q.playing ? seq_step_of(&Q, t) : 255); q[17] = (uint8_t)st->pat[st->cur].steps;
            q[18] = st->div; q[19] = st->gate; q[20] = st->swing; q[21] = st->quant;
        }
        {   /* the drum pads' TUNE (+8) and DECAY (+4), a nibble each */
            int dt = -1; for (int t = 0; t < NTRK; t++) if (is_drum(t)) dt = t;
            for (int i = 0; i < 16; i++) q[i] = dt < 0 ? 0x84 : (uint8_t)((S.trk[dt].patch.dtune[i] + 8) << 4 | (S.trk[dt].patch.ddec[i] + 4));
        }
        r[0] = (uint8_t)op;
        ed_send(cmd, 1 + 45 + 22 * NTRK + 16); return;
    }
    default: ed_err(ED_ERR_BAD); return;
    }
}
/* a flash job of the editor done (ok) or failed: the next step, or the answer */
COLD static void ed_bank_apply(void);
COLD static void ed_job_done(int ok)
{
    if (ed_bk.phase) {                                        /* your FM6 bank or SYN kits: in the flash, now they play */
        ed_bk.phase = 0;
        if (!ok) { ed_err(ED_ERR_FLASH); return; }
        ed_bank_apply();
        for (uint32_t i = 0; i < ed_rep_n; i++) ed_raw[i] = ed_rep[i];
        ed_send(ed_rep_cmd, ed_rep_n); return;
    }
    flash_xip_forget(SLOT_AT[ed_sm.k], SMP_USR_SIZE);
    if (!ok) { ed_sm.on = 0; ed_sm.phase = 0; ed_err(ED_ERR_FLASH); return; }
    if (ed_sm.phase == 1) {                                   /* END: the last data written, now the header */
        ed_sm.phase = 2;
        if (ed_job(SLOT_AT[ed_sm.k], 0, sbuf + 8192, 480, 0)) { ed_sm.on = 0; ed_err(ED_ERR_FLASH); }
        return;
    }
    if (ed_sm.phase == 2) {                                   /* the header: the slot is there */
        ed_sm.phase = 0; ed_sm.on = 0;
        flash_xip_forget(SLOT_AT[ed_sm.k], SMP_USR_SIZE);
        smp_user_scan(ed_sm.k, (const uint8_t *)(uintptr_t)(FLASH_WIN + SLOT_AT[ed_sm.k] - 0x4000u));
        grain_forget(); ed_rep[2] = (uint8_t)smp_user_nz(ed_sm.k); ed_rep_n = 3;
        say_imp(smp_user_nz(ed_sm.k) ? "SAMPLE RECEIVED" : "SAMPLE: BAD SLOT");
    }
    for (uint32_t i = 0; i < ed_rep_n; i++) ed_raw[i] = ed_rep[i];
    ed_send(ed_rep_cmd, ed_rep_n);
}
/* an image just written (sbuf still holds it): it plays from now */
COLD static void ed_bank_apply(void)
{
    if (ed_bk.which) {
        for (int k = 0; k < 4; k++) { xb_kit_unpack(sbuf + 16 + k * XB_KIT_LEN, gb_dsyn[k], &gb_dsyn_crush[k]); drum_syn_forget(k); }
        syn_src = 2; say_imp("SYN KITS SAVED");
    } else {
        uint32_t u = 0; xb_check(sbuf, XB_FM6_LEN, 1, &u);
        fm6_bank_at = XB_FM6_AT + 16u; fm6_bank_used = u; fm6_src = 2; fb_scan = 0;
        for (int t = 0; t < NTRK; t++) { fm6_tried[t] = -1; if (S.trk[t].patch.engine == ENG_FM6 && fm6_ptch(t) >= FM6_NFACTORY) fm6_user_id[t] = -1; }
        say_imp("FM6 BANK SAVED · PRESETS: DX7");
    }
}
COLD static int plus_held(void)                                    /* CHORD+: the black keys held */
{
    uint32_t keys = (panel_keys() | ek_held); int pl = 0;
    for (int k = 0; k < 27; k++) if (keys & (1u << k)) pl |= harm_plus_of_key(k);
    return pl;
}
COLD static int notes_for_key(int k, uint8_t *out)                 /* what a key plays on the current track */
{
    if (is_drum(trk)) { out[0] = (uint8_t)drum_note_of_key(k); return 1; }
    if (is_slice(trk)) { int n = 60 + k + 12 * oct; out[0] = (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n); return 1; }   /* (a slice a key: no scale, no chord) */
    int nn = harm_notes(&harm[trk], k, oct, harm[trk].chord != CH_OFF ? plus_held() : 0, out);
    if (vlead[trk] && nn > 1 && last_chord_n[trk] > 1) {      /* VLEAD: each note to the octave nearest the last chord */
        int c = 0; for (int i = 0; i < last_chord_n[trk]; i++) c += last_chord[trk][i];
        c /= last_chord_n[trk];
        for (int i = 0; i < nn; i++) { int x = out[i]; while (x - c > 6 && x >= 12) x -= 12; while (c - x > 6 && x <= 115) x += 12; out[i] = (uint8_t)x; }
        for (int i = 1; i < nn; i++) { uint8_t x = out[i]; int j = i; while (j > 0 && out[j - 1] > x) { out[j] = out[j - 1]; j--; } out[j] = x; }
        for (int i = 1; i < nn; i++) if (out[i] == out[i - 1] && out[i] <= 115) out[i] += 12;   /* (no note twice) */
    }
    return nn;
}
/* the step written with the last chord played (on) or taken off (!on) */
static void step_write(int st, int on)
{
    for (int i = 0; i < last_chord_n[trk]; i++)
        if (on != seq_step_has(&Q, trk, st, last_chord[trk][i])) seq_step_toggle(&Q, trk, st, last_chord[trk][i], 100);
}
static uint32_t t_half;                     /* when the last half started (for the recording's delay) */
static uint32_t latency_frames(void)
{
    /* the half we filled last plays after the one playing now: what is heard is behind the sequencer by the
     * rest of the playing half plus a whole half; plus ~2 ms for the key's scan and debounce */
    uint32_t el = (time_ticks() - t_half) / lsb;                              /* us */
    uint32_t el_f = el * 441u / 10000u; if (el_f > HALF_FRAMES) el_f = HALF_FRAMES;
    return 2u * HALF_FRAMES - el_f + 90u;
}
COLD static int steps_on_keys(void) { return lay_on(BTN_SEQ); }
/* a column's lights: the faint ones glow_us of its turn, the full ones all of it (hal/fm1/panel.c, from the interrupt) */
static uint8_t glow_us = 5;
static void led_dwell(void) { }                       /* (3.3: the timer's interrupt keeps the lights: panel_tick) */
static void led_(int id, int on) { panel_led(id, on); }
COLD static void leds(void)
{
    /* a landmark, a light of HOME > LIGHTS: faint, a few microseconds of each column's time (panel_glow; until 3.2 a
     * full light a frame in 8 or 16: with frames some milliseconds long, a flash). What blinks, by the clock */
    uint32_t bl = time_ticks() / (125000u * lsb);       /* (eighths of a second) */
    panel_glow_clear();
    for (int k = 0; k < 27; k++) led_(14 + k, 0);
    uint32_t L = lay();
    if (!L && (sticky == PG_SONG || sticky == PG_SONG2)) L = 1u << BTN_SAVE;
    if (steps_on_keys() || (!L && sticky == PG_STEPS)) {   /* the page's steps, the playhead */
        uint32_t now = Q.playing ? seq_step_of(&Q, trk) : 0xFFFF;
        for (int w = 0; w < 16; w++) {
            int st = page * 16 + w;
            int on = seq_step_has(&Q, trk, st, is_drum(trk) ? last_note : -1);
            if ((uint32_t)st == now) on = !on;
            led_(14 + white_key[w], on); if (!(w & 3)) panel_glow(14 + white_key[w]);
        }
    } else if (L & (1u << BTN_GLO)) {
        for (int t = 0; t < NTRK; t++) led_(14 + white_key[POS[t]], !Q.t[t].mute);
        for (int t = 0; t < NTRK; t++) led_(14 + white_key[NTRK + POS[t]], solo[t]);
        led_(14 + white_key[10], Q.fill); led_(14 + white_key[11], Q.fill_bar != 0);
        for (int w = 0; w < 16; w += 4) panel_glow(14 + white_key[w]);
    } else if (L & (1u << BTN_FX)) {                    /* PUNCH: the effect held */
        if (punch_key >= 0) led_(14 + punch_key, 1);
        for (int w = 0; w < 16; w += 4) panel_glow(14 + white_key[w]);
    } else if ((L & (1u << BTN_HOME)) && !(L & ((1u << BTN_SAVE) | (1u << BTN_EDIT)))) {   /* (3.3.6) HOME: OPEN NEW SAVE lit; the one asked again blinks */
        int armed = home_arm && (uint32_t)(time_ticks() - home_arm_t) < 3000000u * lsb ? home_arm - 1 : -1;
        static const uint8_t HK[3] = { HK_OPEN, HK_NEW, HK_SAVE };
        for (int i = 0; i < 3; i++) {
            int w = HK[i], on = w != HK_OPEN || proj_used;
            if (w == armed) led_(14 + white_key[w], (bl >> 1) & 1u);
            else if (on) led_(14 + white_key[w], 1); else panel_glow(14 + white_key[w]);
        }
    } else if (L & ((1u << BTN_SAVE) | (1u << BTN_HOME) | (1u << BTN_EDIT))) {
        if (L & (1u << BTN_SAVE)) {                     /* (3.3.6) the 8 patterns, 1-4 and 5-8 each faint to full; the one playing blinks; 13-16 faint */
            int cur = Q.t[trk].cur;
            for (int w = 0; w < SEQ_NPAT && w < 8; w++) panel_level(14 + white_key[w], w == cur ? ((bl >> 1) & 1u ? 4 : 0) : 1 + (w & 3));
            for (int w = 12; w < 16; w++) panel_glow(14 + white_key[w]);
        } else for (int w = 0; w < 16; w += 4) panel_glow(14 + white_key[w]);
    } else {
        uint32_t keys = (panel_keys() | ek_held);
        for (int k = 0; k < 27; k++) if (keys & (1u << k)) led_(14 + k, 1);
        if (!L && sticky == PG_SND && sb_back < 0 && sb_was) {   /* SOUNDS: the keys that hold a sound faint, the track's lit */
            int cur = preset_origin(preset_of[trk]);
            for (int w = 0; w < 16; w++) if (sb_o[w] >= 0) { if (sb_o[w] == cur) led_(14 + white_key[w], 1); else panel_glow(14 + white_key[w]); }
        } else if (is_drum(trk)) for (int w = 0; w < 16; w += 4) panel_glow(14 + white_key[w]);
    }
    uint32_t beat_ph = Q.tick % (4 * SEQ_TPS);
    led_(BTN_PLAY, Q.playing ? beat_ph < SEQ_TPS : 0);
    led_(BTN_REC, Q.recording || (ltake.rec && (bl & 1u)) || (!ltake.rec && recording_armed && ((bl >> 2) & 1u)));
    led_(BTN_OCT_DOWN, oct < 0); led_(BTN_OCT_UP, oct > 0);
    for (int b = 2; b < 12; b++) led_(b, ((locked >> b) & 1u) && ((bl >> 1) & 1u));   /* a locked layer blinks */
    /* HOME > LIGHTS: for playing in the dark, every button glows, the keys too as KEYS says, at the LIGHTS level (the
     * part of a column's 40 us they are lit: led_dwell); NOTES: a synth track's sounding notes light their keys */
    if (hs[HS_NOTES] && !is_drum(trk)) for (int k = 0; k < 27; k++) if (key_nn[k]) led_(14 + k, 1);
    static const uint8_t GLOW[4] = { 5, 3, 6, 12 };
    glow_us = GLOW[hs[HS_LIGHTS] & 3]; panel_glow_us(glow_us);
    if (hs[HS_LIGHTS]) {
        for (int b = 0; b < 14; b++) panel_glow(b);
        for (int k = 0; k < 27; k++) {
            int n = (53 + k) % 12, white = n == 0 || n == 2 || n == 4 || n == 5 || n == 7 || n == 9 || n == 11;
            if (hs[HS_KEYS] == 3 || (hs[HS_KEYS] == 2 && white) || (hs[HS_KEYS] == 1 && n == 0)) panel_glow(14 + k);
        }
    }
}
static uint32_t taps[4]; static int ntaps;
COLD static void tap_tempo(void)
{
    uint32_t now = time_ticks();
    if (ntaps && (uint32_t)(now - taps[(ntaps - 1) & 3]) > 2000000u * lsb) ntaps = 0;    /* a pause: start again */
    taps[ntaps & 3] = now; ntaps++;
    if (ntaps < 2) { say("TAP"); return; }
    int n = ntaps < 4 ? ntaps : 4;
    uint32_t span = taps[(ntaps - 1) & 3] - taps[(ntaps - n) & 3];
    float bpm = 60.0f * (float)(n - 1) * (float)lsb * 1000000.0f / (float)span;
    seq_set_bpm(&Q, (float)(int)(bpm + 0.5f));
    char b[16]; *put_u(put_s(b, "TAP "), (uint32_t)(Q.bpm + 0.5f)) = 0; say(b);
}
/* ERASE (EDIT + a key held) while playing: the note out of each step the playhead passes, as an MPC does: hold the
 * hat's key for a bar and that bar's hats are gone (one undo for the hold) */
static uint8_t er_on[27], er_n[27], er_note[27][5]; static int16_t er_last[27];
COLD static void erase_pass(void)
{
    if (!Q.playing || !lay_on(BTN_EDIT)) { for (int k = 0; k < 27; k++) er_on[k] = 0; return; }
    uint32_t held = (panel_keys() | ek_held); int st = (int)seq_step_of(&Q, trk), any = 0;
    for (int k = 0; k < 27; k++) {
        if (!er_on[k]) continue;
        if (!((held >> k) & 1u)) { er_on[k] = 0; continue; }
        if (er_last[k] == st) continue;
        er_last[k] = (int16_t)st;
        for (int i = 0; i < er_n[k]; i++) if (seq_step_has(&Q, trk, st, er_note[k][i])) {
            if (!Q.batch) { Q.batch = 1; Q.batch_done = 0; }
            seq_step_toggle(&Q, trk, st, er_note[k][i], 0); any = 1;
        }
    }
    if (any) say("ERASED");
}
/* SAVE + a key 1-8 (3.0.2): tapped, that pattern at the next bar, as it is (an empty one: a new pattern, no notes,
 * the sound playing); held 0.6 s, the pattern playing copied into it first, notes and sound (undo: EDIT + OCT-), then
 * the project written */
static int pat_key = -1, pat_w, pat_done;
static uint32_t pat_t0;
COLD static void pat_go(int w, int copy)
{
    int from = Q.t[trk].cur, empty = 1;
    if (copy && w == from) copy = 0;                  /* (held on the one playing: nothing to copy) */
    for (int t = 0; t < NTRK; t++) {
        if ((scope_one && t != trk) || w == Q.t[t].cur) continue;
        if (copy) { seq_copy(&Q, t, Q.t[t].cur, w); ps_keep(t); ps_copy(&PS, t, Q.t[t].cur, w); }
        else if (!Q.t[t].pat[w].n) { ps_keep(t); ps_copy(&PS, t, Q.t[t].cur, w); }   /* a new one: the sound playing */
        if (Q.t[t].pat[w].n) empty = 0;
    }
    chain_n = 0; chain_on = 0; Q.switch_now = 0; seq_queue(&Q, scope_one ? trk : -1, w);
    char b[40], *p = put_s(b, "PATTERN "); p = put_u(p, (uint32_t)w + 1u);
    if (copy) { p = put_s(p, ": COPY OF "); p = put_u(p, (uint32_t)from + 1u); save_soon = 1; t_save_soon = time_ticks(); }
    else if (empty) p = put_s(p, ": NEW, EMPTY");
    p = put_s(p, " ON THE BAR"); *p = 0; say(b);
}
COLD static void pat_tick(uint32_t rel)
{
    if (pat_key < 0) return;
    if (rel & (1u << pat_key)) { if (!pat_done) pat_go(pat_w, 0); pat_key = -1; return; }
    if (!pat_done && (uint32_t)(time_ticks() - pat_t0) > 600000u * lsb) { pat_done = 1; pat_go(pat_w, 1); }
}
COLD static void keys(void)
{
    uint32_t rel, pr = panel_key_edges(&rel);
    pr |= ek_pr; rel |= ek_rel; ek_pr = ek_rel = 0;
    pat_tick(rel);
    if (pr | rel) t_input = time_ticks();
    uint32_t L = lay();
    if (!L && (sticky == PG_SONG || sticky == PG_SONG2)) L = 1u << BTN_SAVE;     /* the SONG pages (SAVE tapped): their keys, hands free */
    for (int k = 0; k < 27; k++) {
        if ((rel & (1u << k)) && white_of[k] >= 0 && hold_more && page == hold_page) hold_more &= (uint16_t)~(1u << white_of[k]);
        if ((rel & (1u << k)) && hold_step >= 0 && white_of[k] >= 0 && page * 16 + white_of[k] == hold_step) {
            if (hold_erase) step_write(hold_step, 0);
            hold_step = -1; hold_more = 0; Q.batch = 0;
        }
        if ((rel & (1u << k)) && (L & (1u << BTN_GLO)) && white_of[k] == 10) seq_fill(&Q, 0);
        if ((rel & (1u << k)) && k == punch_key) {          /* the punch-in let go: another still held, or none */
            uint32_t still = (L & (1u << BTN_FX)) ? (panel_keys() | ek_held) & ~(1u << k) : 0u;
            punch_key = -1;
            for (int j = 0; j < 27; j++) if (still & (1u << j)) { punch_key = j; break; }
            snd_punch(&S, punch_key >= 0 ? punch_of_key(punch_key) : -1);
        }
        if (rel & (1u << k)) { roll[k].on = 0; key_off(k, latency_frames()); }
        /* CHORD+: a black key pressed or let go changes the chords held, under the fingers */
        if (((pr | rel) & (1u << k)) && harm_plus_of_key(k) && !is_drum(trk) && harm[trk].chord != CH_OFF && !(L & ~(1u << BTN_ARP))) {
            for (int j = 0; j < 27; j++) if (key_nn[j] && key_trk[j] == trk && white_of[j] >= 0) {
                uint8_t n[5]; int nn = notes_for_key(j, n); float vel = (float)key_vel[j] * (1.0f / 127.0f);
                key_off(j, latency_frames()); key_on(j, trk, n, nn, vel, latency_frames());
            }
        }
        if (!(pr & (1u << k))) continue;
        key_off(k, latency_frames());                        /* a press in a layer plays nothing */
        int w = white_of[k];
        if (L & (1u << BTN_FX)) {                            /* PUNCH: this key's effect, while held */
            layer_used = 1; punch_key = k; snd_punch(&S, punch_of_key(k));
            continue;
        }
        if (L & (1u << BTN_SEQ)) {                           /* STEPS: a step placed now, or held to edit */
            layer_used = 1;
            if (w < 0) {
                if (is_drum(trk)) { int s = drum_black(k, last_note & 15); if (s >= 0) last_note = s; }
                else { static const int8_t PGK[4] = { 1, 3, 5, 8 }; for (int i = 0; i < 4; i++) if (PGK[i] == k && i * 16 < Q.t[trk].pat[Q.t[trk].cur].steps) page = i; }
                continue;
            }
            int st = page * 16 + w;
            if (st >= Q.t[trk].pat[Q.t[trk].cur].steps) continue;
            if (hold_step >= 0) {                            /* another step held: it joins (a note put if empty) */
                if (st != hold_step) { hold_more |= (uint16_t)(1u << w); hold_page = page; if (!seq_step_has(&Q, trk, st, -1)) step_write(st, 1); hold_erase = 0; }
                continue;
            }
            hold_more = 0;
            hold_step = st;
            Q.batch = 1; Q.batch_done = 0;                   /* (the step's edits from now: one undo) */
            if (seq_step_has(&Q, trk, st, last_chord[trk][0])) hold_erase = 1;
            else { step_write(st, 1); hold_erase = 0; }
            continue;
        }
        if (L & (1u << BTN_GLO)) {                           /* MIX */
            layer_used = 1;
            if (k == 1) { ml_on = (uint8_t)!ml_on; ml_pick = 0; say(ml_on ? "MIDI LEARN: TURN A KNOB" : "MIDI LEARN DONE"); continue; }   /* (F#3, as SLOOP) */
            if (w >= 0 && w < NTRK) seq_mute(&Q, POS[w], !Q.t[POS[w]].mute);
            if (w >= NTRK && w < 2 * NTRK) solo[POS[w - NTRK]] = (uint8_t)!solo[POS[w - NTRK]];
            if (w == 10) seq_fill(&Q, 1);
            if (w == 11) { if (Q.fill_bar) Q.fill_bar = 0; else seq_fill_next_bar(&Q); }
            if (w == 15) tap_tempo();
            continue;
        }
        if (L & (1u << BTN_HOME)) {                          /* PROJECT (3.3.5: one) */
            layer_used = 1;
            if (w != HK_OPEN && w != HK_NEW && w != HK_SAVE) continue;
            if (saving) { say_bad("A MOMENT: SAVING"); continue; }
            uint32_t now = time_ticks(); int again = home_arm == w + 1 && (uint32_t)(now - home_arm_t) < 3000000u * lsb;
            home_arm = 0;
            if (w == HK_SAVE) {                              /* SAVE (after a safe start: over the saved one, asked twice) */
                if (safe_hold && proj_used && !again) { home_arm = HK_SAVE + 1; home_arm_t = now; say_imp("AGAIN TO SAVE OVER IT"); continue; }
                safe_hold = 0; save_project(1); continue;
            }
            if (w == HK_OPEN) {                              /* OPEN: the saved one again (what changed since: lost, asked twice) */
                if (!proj_used) { say_bad("NOTHING SAVED YET"); continue; }
                uint32_t crc, n = proj_measure(&crc);
                if (!safe_hold && (crc != saved_crc || n != saved_len) && !again) { home_arm = HK_OPEN + 1; home_arm_t = now; say_imp("AGAIN: CHANGES LOST"); continue; }
                load_project(0); continue;
            }
            if (!again) { home_arm = HK_NEW + 1; home_arm_t = now; say_imp("AGAIN FOR A NEW ONE"); continue; }   /* NEW */
            new_project(0); recording_armed = 0; say("NEW PROJECT");
            continue;
        }
        if (L & (1u << BTN_SAVE)) {                          /* SONG */
            layer_used = 1;
            if (w == 12 || k == 3) {
                seq_song_play(&Q, !Q.song_play);
                if (Q.song_play) { if (Q.song_n) say("SONG ON THE BAR"); else say_bad("NO SONG: REC"); } else say("LOOP");
                continue;
            }
            if (w == 13 || k == 1) {
                if (Q.song_rec) { seq_song_record(&Q, 0); say(Q.song_len ? "SONG KEPT" : "EMPTY SONG"); }
                else { seq_song_record(&Q, 1); say("SONG REC ON THE BAR"); }
                continue;
            }
            if (w == 15 || k == 5) { scope_one = !scope_one; say(scope_one ? "PATTERNS: THIS TRACK ONLY" : "PATTERNS: EVERY TRACK"); continue; }
            if (w == 14) {                                   /* CHAIN: on, the patterns touched next play in turn */
                chain_arm = !chain_arm; chain_n = 0; chain_on = 0; Q.switch_now = 0;
                say(chain_arm ? "CHAIN: TOUCH THE PATTERNS IN ORDER" : "CHAIN DONE · THE PATTERN GOES ON");
                continue;
            }
            if (w < 0 || w >= SEQ_NPAT) continue;
            if (!chain_arm) {                                /* a key: tapped, that pattern; held, a copy into it (pat_tick) */
                pat_key = k; pat_w = w; pat_t0 = time_ticks(); pat_done = 0;
                continue;
            }
            if (!chain_n) { Q.switch_now = 0; seq_queue(&Q, scope_one ? trk : -1, w); }
            if (chain_n < SEQ_NPAT) chain[chain_n++] = (uint8_t)w;
            if (chain_n >= 2) { chain_on = 1; chain_pos = 0; Q.switch_now = 1; seq_song_play(&Q, 0); }
            {
                char b[40], *p = put_s(b, "CHAIN ");
                for (int i = 0; i < chain_n && p < b + 30; i++) { if (i) p = put_s(p, "→"); p = put_u(p, chain[i] + 1u); }
                *p = 0; say(b);
            }
            continue;
        }
        if (L & (1u << BTN_SEL)) {                           /* KEY: any key sets the key of the song */
            layer_used = 1;
            int pc = (53 + k) % 12;
            for (int t = 0; t < NTRK; t++) if (!is_drum(t)) harm[t].root = (uint8_t)pc;
            char b[16]; char *p = put_s(b, "KEY "); p = put_s(p, harm_note_name[pc]); *p = 0; say(b);
            continue;
        }
        if (!L && sticky == PG_SND && sb_back < 0 && sb_was && w >= 0) sb_key(w);   /* SOUNDS: the tile's sound, then its note */
        uint8_t nts[5]; int nn = notes_for_key(k, nts);
        if ((L & (1u << BTN_EDIT)) && is_drum(trk) && w < 0) {   /* ERASE + a black key: the sound selected muted */
            layer_used = 1; uint16_t *m = &S.trk[trk].patch.dmute; *m ^= (uint16_t)(1u << (last_note & 15));
            char b[24], *p = put_s(b, PAD[last_note & 15]); p = put_s(p, (*m >> (last_note & 15)) & 1u ? " MUTED" : " PLAYS"); *p = 0; say(b);
            continue;
        }
        if ((L & (1u << BTN_EDIT)) && Q.playing) {          /* ERASE playing: where the playhead passes, while held */
            layer_used = 1; er_on[k] = 1; er_last[k] = -1; er_n[k] = (uint8_t)nn;
            for (int i = 0; i < nn; i++) er_note[k][i] = nts[i];
            continue;
        }
        if (L & (1u << BTN_EDIT)) {                          /* ERASE: that note (its chord) out of the pattern */
            layer_used = 1; int any = 0;
            for (int i = 0; i < nn; i++) if (pattern_has_note(trk, nts[i])) { if (!any) { if (!Q.batch) { Q.batch = 1; Q.batch_done = 0; } } seq_erase_note(&Q, trk, nts[i]); any = 1; }
            if (any) say("ERASED");
            continue;
        }
        if (!L && sticky == PG_STEPS && w < 0 && is_drum(trk)) { int s = drum_black(k, last_note & 15); if (s >= 0) { last_note = s; continue; } }
        if (!nn) continue;                                   /* (a black key: CHORD+, or silent with KEYS WHITE) */
        if (!L && notes_on()) {                              /* NOTES: the keys write the step edited */
            if (!ne_wrote) { ne_n = 0; Q.batch = 1; Q.batch_done = 0; }
            if (gb_vmode(S.trk[trk].patch.p) != VM_POLY) ne_n = 0;   /* (MONO, LEG: one note) */
            for (int i = 0; i < nn && ne_n < 4; i++) ne_buf[ne_n++] = nts[i];
            seq_step_write(&Q, trk, ne_step, ne_buf, ne_n, 100); ne_sub = 0; ne_wrote = 1;
        }
        for (int i = 0; i < nn; i++) last_chord[trk][i] = nts[i];
        last_chord_n[trk] = (uint8_t)nn; last_note = nts[0];
        float vel = 0.8f;
        if (is_drum(trk)) { if (held_btn & (1u << BTN_OCT_DOWN)) vel = 0.35f; else if (held_btn & (1u << BTN_OCT_UP)) vel = 1.0f; }
        uint32_t lat = latency_frames();
        if (recording_armed && !Q.playing && !countin && !(L & ~(1u << BTN_ARP))) {   /* armed: the first note starts it */
            if (ltake.armed_trk < 0 && !rec_count) { record_now(trk); lat = 0; }   /* grid arm: no take open (this note: step 1; COUNT-IN: notes only sound) */
            /* a take open: nothing here — the note is captured at the append site in key_on */
        }
        key_on(k, trk, nts, nn, vel, lat);
        if (L & (1u << BTN_ARP)) {                           /* ROLL: again on the grid while held */
            layer_used = 1;
            uint32_t now = Q.playing ? Q.tick : roll_clock, iv = ROLL_TICKS[roll_rate];
            roll[k].on = 1; roll[k].next = (now / iv + 1u) * iv;
        }
    }
}
COLD static void notes_advance(void)                              /* NOTES: every key let go: the next step */
{
    if (!ne_wrote || (panel_keys() | ek_held)) return;
    ne_wrote = 0; ne_n = 0; Q.batch = 0;
    int st = Q.t[trk].pat[Q.t[trk].cur].steps; ne_step = (ne_step + 1) % (st > 0 ? st : 1); ne_sub = 0;
}
static uint32_t t_rec_down; static uint8_t rec_hint;
/* the take's adapter helpers (the module is seq/loop.c; these say what it returns, and keep the UI flag in sync) */
/* a first take's clock, in µs: the 60 MHz timer wraps every 71.6 s, so the ticks are subtracted before they are divided
 * (anchored anew until the capture starts: a take is at most 24 s) */
static uint32_t loop_t0;
static uint32_t loop_us(void) { uint32_t t = time_ticks(); if (!ltake.rec) loop_t0 = t; return (uint32_t)(t - loop_t0) / lsb; }
static void loop_append(int trk, int note, int vel, int down, uint32_t lat)   /* a note on/off, into the open take (RAM: roll calls it) */
{
    if (ltake.armed_trk < 0) return;
    if (!ltake.follow && !ltake.rec && Q.playing) loop_arm(&ltake, &Q, ltake.armed_trk, ltake.target);   /* the transport started under an armed take: a later take */
    uint8_t was_rec = ltake.rec;
    loop_note(&ltake, trk, note, vel, down, ltake.follow ? seq_heard(&Q, lat) : loop_us());
    if (!was_rec && ltake.rec) t_rec_down = 0;   /* the capture started: the hold-to-erase timer is off */
}
COLD static void loop_closed(int nb, int t)   /* say what a close returned */
{
    recording_armed = 0;
    if (nb < 0) { say("TAKE CANCELLED"); return; }
    if (nb == 0) { say_bad("EMPTY TAKE"); return; }
    Q.t[t].quant = 0;                            /* a take plays as it was played (a project's tracks start on QUANT 100; STEPS: QUANT pulls it in again) */
    if (ltake.full) { say_imp("LOOP FULL: 128 NOTES"); return; }
    char b[32], *p = put_s(b, "TAKE: "); p = put_u(p, (uint32_t)nb); p = put_s(p, nb > 1 ? " BARS · " : " BAR · ");
    p = put_u(p, (uint32_t)(Q.bpm + 0.5f)); p = put_s(p, " BPM"); *p = 0; say(b);
}
COLD static void loop_say(void)                                      /* a first take: the seconds, and the loop it would make now; a later take: where it is */
{
    char b[40], *p;
    if (ltake.follow) {
        if (!ltake.rec) { say("REC AT THE BAR"); return; }
        if (ltake.closing) { say("CLOSING AT THE BAR"); return; }
        p = put_s(b, "TAKE · BAR "); p = put_u(p, (Q.tick - ltake.start) / SEQ_BAR + 1u);
        if (ltake.target) { p = put_s(p, " OF "); p = put_u(p, (ltake.end - ltake.start) / SEQ_BAR); }
        *p = 0; say(b); return;
    }
    uint32_t d = loop_us() - ltake.t0_us;
    int nb; float bpm;
    loop_fit(d > 400000u ? d : 400000u, Q.bpm, &nb, &bpm);
    if (ltake.target) { float t = (float)ltake.target * 240000000.0f / (float)(d > 400000u ? d : 400000u); if (t >= 40.0f && t <= 250.0f) { nb = ltake.target; bpm = t; } }
    p = put_s(b, "TAKE ");
    p = put_u(p, d / 1000000u); *p++ = '.'; p = put_u(p, d / 100000u % 10u); p = put_s(p, "s · ");
    p = put_u(p, (uint32_t)nb); p = put_s(p, " BARS "); p = put_u(p, (uint32_t)(bpm + 0.5f)); *p = 0;
    say(b);
}
/* SEL + OCT- / OCT+: the selected track's undo / redo, the other tracks' edits left alone (EDIT + OCT: the last edit
 * anywhere, as before). Refused while that track is being recorded: an undo under a take would put notes back below it */
COLD static void undo_track(int redo)
{
    layer_used = 1;
    if (ltake.armed_trk == trk || (Q.recording && Q.rec_trk == trk)) { say_bad("STILL RECORDING"); return; }
    if (redo ? seq_redo_trk(&Q, trk) : seq_undo_trk(&Q, trk)) say(redo ? "REDONE" : "UNDONE");
    else say_bad(redo ? "NOTHING TO REDO" : "NOTHING TO UNDO");
}
/* BARS, an empty track's choice for its next take: FREE, 1, 2, 4, 8 (as many as the track's DIV holds) */
COLD static void bars_turn(int t, int d)
{
    static const uint8_t B[5] = { 0, 1, 2, 4, 8 };
    int i = 0; while (i < 4 && B[i] != bars_pick[t]) i++;
    i += d; i = i < 0 ? 0 : i > 4 ? 4 : i;
    while (B[i] > loop_max_bars(&Q, t)) i--;
    bars_pick[t] = B[i];
    if (ltake.armed_trk == t && !ltake.rec) loop_arm(&ltake, &Q, t, B[i]);   /* armed, not yet recording: the take's too */
}
COLD static void open_page(int b)
{
    int pg = sticky;
    if (b == BTN_HOME && sticky == PG_TRACKS && !vis_on) { vis_on = 1; vis_named = 25; S.vis_ring = vis_ring; return; }
    if (vis_on) { vis_on = 0; S.vis_ring = 0; if (b == BTN_HOME) return; }
    switch (b) {
    case BTN_HOME: pg = PG_TRACKS; break;
    case BTN_SEQ:                                        /* STEPS, then NOTES (a synth track), then back */
        if (sticky == PG_STEPS && !is_drum(trk)) { pg = PG_NOTES; ne_step = page * 16; ne_sub = 0; say("NOTES: THE KEYS WRITE THE STEP"); }
        else pg = sticky == PG_STEPS || sticky == PG_NOTES ? PG_TRACKS : PG_STEPS;
        break;
    case BTN_GLO: pg = sticky == PG_GLOBAL ? PG_TEX1 : sticky == PG_TEX1 ? PG_TEX2 : sticky == PG_TEX2 ? PG_MIDI : sticky == PG_MIDI ? PG_MIDI2 : sticky == PG_MIDI2 ? PG_GREC : sticky == PG_GREC ? PG_TRACKS : PG_GLOBAL; break;   /* GLOBAL, TEXTURE, MIDI */
    case BTN_SAVE: {                                     /* tapped: SONG (its keys free; SELECT: SONG 2/2); on the SONG pages,
                                                            the song saved (with its project), as SLOOP 2.8.5's SONG screen
                                                            (3.1.6); twice fast anywhere: the project saved */
        static uint32_t t_last; static int before;
        uint32_t now = time_ticks();
        if (t_last && (uint32_t)(now - t_last) < 450000u * lsb) { t_last = 0; pg = before; save_project(1); break; }
        if (sticky == PG_SONG || sticky == PG_SONG2) { t_last = 0; save_project(1); return; }
        t_last = now; before = sticky; pg = PG_SONG; break;
    }
    case BTN_FX:                                         /* the sends, then the master bus */
        if (group_of(sticky) == BTN_FX) fx_page = (fx_page + 1) % NFX_PG;
        pg = FX_PG[fx_page]; break;
    case BTN_LFO:                                        /* LFO, then where it goes */
        if (is_drum(trk)) { pg = sticky == PG_LFO ? PG_TRACKS : PG_LFO; break; }
        if (group_of(sticky) == BTN_LFO) lfo_page = (lfo_page + 1) % 3;
        pg = LFO_PG[lfo_page]; break;
    case BTN_SEL: pg = sticky == PG_SEL ? PG_SEL2 : PG_SEL; break;
    case BTN_ARP: pg = sticky == PG_ARP ? PG_ARP2 : PG_ARP; break;
    case BTN_EDIT:                                       /* the engine's pages, the next each tap */
        if (sticky == PG_NOTES && !is_drum(trk)) {          /* (NOTES: the note edited deleted) */
            int i = ne_sel();
            if (i < 0) { say_bad("NO NOTE HERE"); return; }
            Q.batch = 0; seq_ev_del(&Q, trk, i); if (ne_sub > 0) ne_sub--; say("NOTE DELETED"); return;
        }
        if (sticky == PG_SND && sb_back >= 0) { sb_back = -1; edit_page = 5; return; }   /* (PRESETS' SOUNDS: it stays, the keys choose) */
        if (is_drum(trk)) { pg = sticky == PG_KIT ? PG_DSOUND : PG_KIT; break; }
        if (group_of(sticky) == BTN_EDIT) edit_page = (edit_page + 1) % (is_drum(trk) ? 4 : 6);
        pg = EDIT_PG[edit_page]; break;
    case BTN_ENV:
        if (is_drum(trk)) { pg = sticky == PG_KIT ? PG_TRACKS : PG_KIT; break; }
        if (group_of(sticky) == BTN_ENV) env_page = (env_page + 1) % 2;
        pg = PG_ENV + env_page; break;
    default: break;
    }
    sticky = pg;
}
COLD static void buttons(void)
{
    uint32_t rel, pr = panel_button_edges(&rel);
    pr |= rm_pr | eb_pr; rel |= rm_rel | eb_rel; rm_pr = rm_rel = eb_pr = eb_rel = 0;   /* (REMOTE: the buttons over MIDI; the editor's) */
    if (!m_rmch) rm_held = 0;
    if (pr | rel) t_input = time_ticks();
    held_btn = panel_buttons() | rm_held | eb_held;
    if (t_octup) {                                                   /* NOTES: OCT+ held, the sheet; let go soon, an octave */
        if (!notes_on()) t_octup = 0;
        else if (rel & (1u << BTN_OCT_UP)) { t_octup = 0; if (oct < 3) oct++; }
        else if ((uint32_t)(time_ticks() - t_octup) > 400000u * lsb) { t_octup = 0; ne_sheet = 1; say("SHEET: KNOB 1 THE ROW · OCT+ DOES IT"); }
    }
    if (ne_sheet && !notes_on()) ne_sheet = 0;
    if (!lay_on(BTN_SEQ) && hold_step >= 0) {                        /* SEQ let go before the step's key */
        if (hold_erase) step_write(hold_step, 0);
        hold_step = -1; hold_more = 0; Q.batch = 0;
    }
    if (!lay_on(BTN_EDIT) && hold_step < 0 && sticky != PG_NOTES) Q.batch = 0;   /* (an ERASE hold: one undo; NOTES: a note) */
    if (!lay_on(BTN_GLO) && Q.fill) seq_fill(&Q, 0);
    if (!lay_on(BTN_FX) && punch_key >= 0) { punch_key = -1; snd_punch(&S, -1); }
    /* the throws: FX + OCT- every reverb send full, FX + OCT+ every delay send (and its repeats), while held */
    sm_set(&S.throw_rev, (lay_on(BTN_FX) && (held_btn & (1u << BTN_OCT_DOWN))) || (ed_thr & 1u) ? 1.0f : 0.0f);
    sm_set(&S.throw_dly, (lay_on(BTN_FX) && (held_btn & (1u << BTN_OCT_UP))) || (ed_thr & 2u) ? 1.0f : 0.0f);
    /* a layer locked open: HOME tapped while it is held; any other button (not PLAY, REC, OCT) lets it go */
    const uint32_t KEEP = (1u << BTN_PLAY) | (1u << BTN_REC) | (1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP);
    if (locked && (pr & ~KEEP)) { locked = 0; pr &= ~LAYER_BITS; rel &= ~LAYER_BITS; layer_used = 1; say("LAYER FREE"); }
    if ((pr & (1u << BTN_HOME)) && (held_btn & LAYER_BITS & ~(1u << BTN_HOME))) {
        locked = held_btn & LAYER_BITS & ~(1u << BTN_HOME); layer_used = 1; say("LOCK: LAYER OPEN"); return;
    }
    if (pr & LAYER_BITS) { layer_used = 0; t_btn_down = time_ticks(); }
    for (int b = 0; b < 14; b++) {
        if (!(pr & (1u << b))) continue;
        switch (b) {
        case BTN_OCT_DOWN:
            if (lay_on(BTN_FX)) layer_used = 1;              /* (FX: the reverb throw, above) */
            else if (lay_on(BTN_EDIT)) { layer_used = 1; if (seq_undo(&Q)) say("UNDONE"); else say_bad("NOTHING TO UNDO"); }
            else if (lay_on(BTN_SEL)) undo_track(0);        /* SEL + OCT-: the selected track's undo alone */
            else if (hold_step >= 0) {
                for (int si = -1; si < 16; si++) { int st = si < 0 ? hold_step : ((hold_more >> si) & 1u) ? hold_page * 16 + si : -1; if (st < 0) continue; seq_step_reset(&Q, trk, st); seq_step_nudge(&Q, trk, st, 0); }
                hold_erase = 0;
            }
            else if (steps_on_keys() || (!lay() && sticky == PG_STEPS)) { layer_used = 1; if (page > 0) page--; }
            else if (ne_sheet && notes_on()) { ne_sheet = 0; t_octup = 0; }   /* (NOTES: the sheet closed) */
            else if (is_drum(trk)) { }                       /* drums: ghost hits while held */
            else if (held_btn & (1u << BTN_OCT_UP)) oct = 0;
            else if (oct > -3) oct--;
            break;
        case BTN_OCT_UP:
            if (lay_on(BTN_FX)) layer_used = 1;              /* (FX: the echo throw) */
            else if (lay_on(BTN_EDIT)) { layer_used = 1; if (seq_redo(&Q)) say("REDONE"); else say_bad("NOTHING TO REDO"); }
            else if (lay_on(BTN_SEL)) undo_track(1);        /* SEL + OCT+: its redo */
            else if (hold_step >= 0) {                       /* the step's condition, the next one */
                hold_erase = 0;
                int c; seq_step_info(&Q, trk, hold_step, &c, 0);
                int i = 0; while (i < NCONDS && CONDS[i] != c) i++;
                for (int si = -1; si < 16; si++) { int st = si < 0 ? hold_step : ((hold_more >> si) & 1u) ? hold_page * 16 + si : -1; if (st >= 0) seq_step_cond(&Q, trk, st, CONDS[(i + 1) % NCONDS]); }
            }
            else if (steps_on_keys() || (!lay() && sticky == PG_STEPS)) { layer_used = 1; if ((page + 1) * 16 < Q.t[trk].pat[Q.t[trk].cur].steps) page++; }
            else if (notes_on() && !lay() && !(held_btn & (1u << BTN_OCT_DOWN))) {   /* NOTES: held, the sheet; tapped, as before */
                if (ne_sheet) ne_sheet_do(); else t_octup = time_ticks() | 1u;
            }
            else if (is_drum(trk)) { }                       /* drums: hard hits while held */
            else if (held_btn & (1u << BTN_OCT_DOWN)) oct = 0;
            else if (oct < 3) oct++;
            break;
        case BTN_PLAY:
            if (ltake.armed_trk >= 0) { loop_cancel(&ltake); recording_armed = 0; say("TAKE CANCELLED"); break; }
            if (countin) { countin = 0; say("REC READY"); break; }               /* (the count-in let go: armed still) */
            if (recording_armed && rec_count && !Q.playing) {   /* COUNT-IN (grid arm only: a take open is cancelled above) */
                countin = 1; cin_ticks = 0; cin_trk = trk; say("COUNT-IN: ONE BAR"); break;
            }
            transport(!Q.playing);
            break;
        case BTN_REC:                                    /* (3.3.6: tapped, it acts when let go; held 2 s, it only erases) */
            t_rec_down = time_ticks() | 1u; rec_hint = 0;
            if (ltake.rec) {                     /* a later take ends on the bar line; a first take now */
                if (ltake.follow) { loop_stop(&ltake, &Q); say("CLOSING AT THE BAR"); }
                else { int t = ltake.armed_trk; loop_closed(loop_close(&ltake, &Q, loop_us()), t); }
                t_rec_down = 0;
            }
            else if (countin) { countin = 0; recording_armed = 0; say("REC OFF"); t_rec_down = 0; }
            break;
        case BTN_ENV: case BTN_LFO: open_page(b); break;
        default: break;
        }
    }
    /* a layer's button tapped (let go soon, nothing done with it): its page */
    int tap = !layer_used && (uint32_t)(time_ticks() - t_btn_down) < 400000u * lsb;
    for (int b = 0; b < 14; b++) if ((rel & (1u << b)) && ((LAYER_BITS >> b) & 1u) && tap) open_page(b);
    if ((rel & (1u << BTN_REC)) && t_rec_down && rec_hint) { t_rec_down = 0; say("ERASE CANCELLED"); }   /* (held, then let go before 2 s: nothing) */
    if ((rel & (1u << BTN_REC)) && t_rec_down) {          /* REC tapped (let go before the erase): REC on or off */
        t_rec_down = 0;
        if (ltake.rec) { /* capturing: nothing (the take closes on REC press) */ }
        else if (ltake.armed_trk >= 0) {                  /* armed but not capturing */
            if (trk == ltake.armed_trk) { loop_cancel(&ltake); recording_armed = 0; say("REC OFF"); }
            else if (loop_arm(&ltake, &Q, trk, bars_pick[trk])) say(Q.playing ? "REC AT THE BAR" : "REC READY · PLAY FREELY");
            else { loop_cancel(&ltake); recording_armed = 0; say("REC OFF"); }   /* a content track: disarm only */
        }
        else if (Q.recording) seq_record(&Q, 0, trk);
        else if (Q.playing) {                             /* playing, no take open */
            if (loop_arm(&ltake, &Q, trk, bars_pick[trk])) { recording_armed = 1; say("REC AT THE BAR"); }
            else seq_record(&Q, 1, trk);
        }
        else if (recording_armed) { recording_armed = 0; say("REC OFF"); }
        else if (loop_arm(&ltake, &Q, trk, bars_pick[trk])) { recording_armed = 1; say("REC READY · PLAY FREELY"); }
        else { recording_armed = 1; say(rec_count ? "REC READY · PLAY: COUNT-IN" : "REC READY · YOUR NOTE STARTS IT"); }
    }
    if (t_rec_down && !rec_hint && (uint32_t)(time_ticks() - t_rec_down) > 500000u * lsb) { rec_hint = 1; say("HOLD REC: ERASE THE TRACK"); }
    if (t_rec_down && (uint32_t)(time_ticks() - t_rec_down) > 2000000u * lsb) {   /* REC held: the track cleared (and nothing else) */
        t_rec_down = 0;
        if (Q.recording) seq_record(&Q, 0, trk);
        seq_clear(&Q, trk); say_imp("TRACK ERASED");
    }
}
/* what a SLOOP sound sets beside the sound: its TRANS (the basses an octave or two down), its arpeggio */
COLD static void extras_from(int t, int trans, const uint8_t *a);
#define SL_AMODE sloop_amode                       /* SLOOP's arpeggiator modes and rates, ours (sloopimport.c) */
#define SL_ARATE sloop_arate
COLD static void preset_extras(int t, int np)
{
    if (is_drum(t)) return;
    extras_from(t, preset_trans(np), preset_arp(np));
}
COLD static void extras_from(int t, int trans, const uint8_t *a)
{
    harm[t].transpose = (int8_t)trans;
    if (a) {                                              /* SLOOP's MODE, RATE (1/4 1/8 1/16 1/32 8T 16T), OCT, GATE */
        arpc[t].mode = SL_AMODE[(a[0] - 1) % 15]; arpc[t].rate = SL_ARATE[(a[1] - 1) % 6];
        arpc[t].oct = (uint8_t)(a[2] - 1 < 1 ? 1 : a[2] - 1 > 4 ? 4 : a[2] - 1); arpc[t].gate = (uint8_t)((a[3] - 1) * 100 / 127);
    } else arpc[t].mode = AM_OFF;
}
/* ---------------------------------------------------------------- your sounds (usersnd.h): read from flash when used */
COLD static int us_bank(void)                          /* the bank in sbuf (no save under way): 1 if there */
{
    if (saving || !flash_ok || sbuf_held()) return 0;
    sbuf_used();
    int n = store_sounds_load(sbuf, sizeof sbuf);
    if (!US_LEN_OK(n)) for (int k = 0; k < US_N; k++) us_put(sbuf, k, 0);
    return 1;
}
COLD static int apply_user(int t, int k)               /* a sound of yours on track t */
{
    us_sound u;
    if (is_drum(t) || !us_bank() || !us_get(sbuf, k, &u)) return 0;
    us_apply(&S, t, &u); preset_of[t] = US_BASE + k; base_of_track(t);
    extras_from(t, u.trans, u.arp[0] ? u.arp : 0);
    return 1;
}
/* (3.3.6) one of your kits on the drum track: the kit at its defaults (no tune or decay kept from the kit before: a
 * break's slices at their pitch), named by its sample */
COLD static int apply_ukit(int t, int kit)
{
    if (!is_drum(t) || !uk_ok(kit)) return 0;
    gb_track *q = &S.trk[t]; gb_patch *p = &q->patch;
    gb_patch_default(p, ENG_DRUM); gb_patch_set(p, SP_E0, kit); p->protect = 1; p->gain = 1.0f;
    q->mono_n = 0; q->mono_cur = -1;
    preset_of[t] = UK_BASE + kit; base_of_track(t);
    return 1;
}
/* a DX7 voice of your bank on track t (3.2): FM6 at its defaults, PTCH on the voice, read at once when it can be */
COLD static int apply_bank(int t, int k)
{
    if (is_drum(t) || !fb_ok(k)) return 0;
    gb_track *q = &S.trk[t]; gb_patch *p = &q->patch;
    gb_patch_default(p, ENG_FM6); gb_patch_set(p, SP_E7, FM6_NFACTORY + k); p->gain = FB_GAIN;
    q->mono_n = 0; q->mono_cur = -1;
    sm_set(&q->send_cho, 0.0f); sm_set(&q->send_dly, 0.0f); sm_set(&q->send_rev, 0.2f);
    preset_of[t] = FB_BASE + k; base_of_track(t); extras_from(t, 0, 0);
    fm6_bank_poll();
    return 1;
}
/* your favourites: kept with your sounds, a moment after the last change (ui_round) */
COLD static void fav_save(void)
{
    if (!us_bank()) return;
    us_fav_put(sbuf);
    if (store_sounds_begin(sbuf, US_FILE)) { say_bad("SAVE: ERROR"); return; }
    saving = 1; saving_shown = 0; saving_snd = 4;
}
COLD static int snd_load(int f)                          /* a sound onto the track chosen: 1 if it is there now */
{
    if (f >= UK_BASE) return apply_ukit(trk, f - UK_BASE);
    if (f >= FB_BASE) return apply_bank(trk, f - FB_BASE);
    if (f >= US_BASE) { if (apply_user(trk, f - US_BASE)) return 1; say_bad(saving ? "SAVING" : "USER SOUND UNREADABLE"); return 0; }
    if (f < 0 || f >= gb_npresets || is_drum(trk) != (gb_presets[f].engine == ENG_DRUM)) return 0;
    preset_apply(&S, trk, f); preset_of[trk] = f; base_of_track(trk); preset_extras(trk, f);
    return 1;
}
COLD static void user_save(int k, int erase)            /* USER SOUNDS: the track's sound into place k (or erased) */
{
    if (is_drum(trk)) return;
    if (!us_bank()) { say_bad(saving ? "SAVING" : "NO FLASH"); return; }
    if (erase) us_put(sbuf, k, 0);
    else {
        us_sound u; int po = preset_origin(preset_of[trk]);
        char nm[13]; const char *src = po >= 0 ? snd_name(po) : 0;
        if (!src || !src[0]) { char *p = put_s(nm, "USER "); p = put_u(p, (uint32_t)k + 1u); *p = 0; src = nm; }
        us_take(&S, trk, base[trk], src, &u);
        u.kind = (uint8_t)(po >= 0 && po < gb_npresets ? gb_presets[po].kind : K_FX); u.trans = harm[trk].transpose;
        if (arpc[trk].mode != AM_OFF) {                 /* (the arpeggiator as a preset keeps it: SLOOP's MODE RATE OCT GATE + 1) */
            int m = 0, r = 2; while (m < 15 && SL_AMODE[m] != arpc[trk].mode) m++;
            for (int i = 0; i < 6; i++) if (SL_ARATE[i] == arpc[trk].rate) r = i;
            u.arp[0] = (uint8_t)(m < 15 ? m + 1 : 2); u.arp[1] = (uint8_t)(r + 1); u.arp[2] = (uint8_t)(arpc[trk].oct + 1);
            u.arp[3] = (uint8_t)(arpc[trk].gate * 127 / 100 + 1);
        }
        us_put(sbuf, k, &u);
    }
    us_fav_put(sbuf);
    if (store_sounds_begin(sbuf, US_FILE)) { say_bad("SAVE: ERROR"); return; }
    us_from_bytes(sbuf, US_FILE);
    saving = 1; saving_shown = 1; saving_snd = 1;
    if (!erase) { preset_of[trk] = US_BASE + k; }
    else for (int t = 0; t < NTRK; t++) if (preset_origin(preset_of[t]) == US_BASE + k) preset_of[t] = -1;   /* (its tracks keep the sound, now their own: 3.1.2) */
}
COLD static void apply_preset(int t, int np)             /* a track's sound: a preset (PRESETS, a MIDI program) */
{
    preset_apply(&S, t, np); preset_of[t] = np; base_of_track(t); preset_extras(t, np);
    int n, r = preset_rank(np, &n);
    char b[40], *p = put_s(b, gb_kind_name[gb_presets[np].kind]); p = put_s(p, " · "); p = put_s(p, gb_presets[np].name);
    p = put_s(p, " · "); p = put_u(p, (uint32_t)r); *p++ = '/'; p = put_u(p, (uint32_t)n); *p = 0;
    if (t != trk) { p = put_s(b, "TRACK "); p = put_u(p, (uint32_t)POS[t] + 1u); p = put_s(p, " · "); p = put_s(p, gb_presets[np].name); *p = 0; }
    say(b);
}
/* ---------------------------------------------------------------- SOUNDS: the categories, the favourites */
COLD static int sb_cat_ok(int c)                         /* a category this track has */
{
    int16_t o[16];
    if (is_drum(trk)) return c == K_KIT + 1 || c == SB_UKIT;
    return c != K_KIT + 1 && c != SB_UKIT && (c == SB_FAV || c == SB_USER || sb_list(c, 0, o) > 0);
}
COLD static void sb_sync(void)                           /* the page that shows the track's sound; else the category kept */
{
    int c = sb_cat, i = sb_where(preset_origin(preset_of[trk]), &c);
    if (i >= 0 && sb_cat_ok(c)) { sb_cat = (uint8_t)c; sb_pg = (uint8_t)(i / 16); }
    else if (!sb_cat_ok(sb_cat)) { sb_cat = (uint8_t)(is_drum(trk) ? K_KIT + 1 : SB_FAV); sb_pg = 0; }
    sb_trk = (int8_t)trk;
}
COLD static void sb_tick(void)                           /* each round: the page's sounds; opened by PRESETS, back after 4 s */
{
    if (sticky != PG_SND) { sb_was = 0; sb_back = -1; return; }
    if (sb_back >= 0 && (uint32_t)(time_ticks() - sb_t) > 4000000u * lsb) { sticky = sb_back; sb_back = -1; sb_was = 0; return; }
    if (!sb_was || sb_trk != trk) { sb_sync(); sb_was = 1; }
    sb_n = (uint8_t)sb_list(sb_cat, sb_pg * 16, sb_o);
}
COLD static void sb_cat_step(int d)                      /* the next / previous category the track has */
{
    int c = sb_cat;
    do c = (c + (d > 0 ? 1 : SB_NCAT - 1)) % SB_NCAT; while (!sb_cat_ok(c));
    sb_cat = (uint8_t)c; sb_pg = 0;
}
COLD static void sb_page_step(int d, int across)         /* a page on; across: into the next category at the end (SELECT) */
{
    int np = (sb_n + 15) / 16, g = sb_pg + (d > 0 ? 1 : -1);
    if (g >= 0 && g < np) { sb_pg = (uint8_t)g; return; }
    if (!across) return;
    if (d < 0 && sb_cat == (is_drum(trk) ? K_KIT + 1 : SB_FAV)) {      /* (left of the first page: the page before SOUNDS) */
        if (is_drum(trk)) sticky = PG_KIT; else { edit_page = 4; sticky = PG_USER; }
        return;
    }
    sb_cat_step(d);
    if (d < 0) { int16_t o[16]; sb_pg = (uint8_t)((sb_list(sb_cat, 0, o) - 1) / 16); }
}
COLD static void sb_key(int w)                           /* a white key (EDIT's SOUNDS): its tile's sound, then its note */
{
    int f = sb_o[w];
    if (f >= 0 && f != preset_of[trk]) snd_load(f);
}
COLD static void fav_changed(void) { fav_dirty = 1; fav_t = time_ticks(); }
COLD static void sb_fav(int d)                           /* KNOB 4: the track's sound into your favourites, or out */
{
    int f = preset_origin(preset_of[trk]), at = -1, room = -1;
    if (is_drum(trk)) return;
    if (!fav_ok(f)) { say_bad("NOT A SAVED SOUND"); return; }
    for (int i = US_NFAV - 1; i >= 0; i--) { if (us_fav[i] == f) at = i; else if (!fav_ok(us_fav[i])) room = i; }
    if (d > 0) {
        if (at >= 0) { say("ALREADY A FAVOURITE"); return; }
        if (room < 0) { say_bad("16 FAVOURITES: REMOVE ONE"); return; }
        us_fav[room] = (uint16_t)f;
        char b[40], *p = put_s(b, "FAV "); p = put_u(p, (uint32_t)room + 1u); p = put_s(p, " · "); p = put_s(p, snd_name(f)); *p = 0; say(b);
    } else {
        if (at < 0) return;
        us_fav[at] = 0xFFFF; say("FAVOURITE REMOVED");
    }
    fav_changed();
}
COLD static void sb_move(int d)                          /* KNOB 3 on FAVS: the track's sound a place left or right */
{
    int f = preset_origin(preset_of[trk]), at = -1;
    if (sb_cat != SB_FAV || f < 0) return;
    for (int i = 0; i < US_NFAV; i++) if (us_fav[i] == f) at = i;
    int to = at + (d > 0 ? 1 : -1);
    if (at < 0 || to < 0 || to >= US_NFAV) return;
    us_fav[at] = us_fav[to]; us_fav[to] = (uint16_t)f; fav_changed();
}
/* PRESETS: the next or previous sound, through the categories (your favourites, each kind, yours, the DX7 voices;
 * the kits on the drum track). It opens SOUNDS where the keys only play (not on the step pages, a layer held, or
 * recording: there a message says the sound, as before 3.2) */
COLD static void choose_preset(int d)
{
    uint32_t L = lay();
    int c = sb_cat, i = sb_where(preset_origin(preset_of[trk]), &c), n, first = -1;
    int16_t o[16];
    if (sticky == PG_SND && sb_was && sb_trk == trk) {     /* (SOUNDS shows another page than the sound's: from the page shown) */
        if (i < 0 || c != sb_cat || i / 16 != sb_pg) { c = sb_cat; i = d > 0 ? sb_pg * 16 - 1 : sb_pg * 16 + 16; }
    } else if (i < 0 || !sb_cat_ok(c)) { c = is_drum(trk) ? K_KIT + 1 : sb_cat_ok(sb_cat) ? sb_cat : SB_FAV; i = d > 0 ? -1 : 16; }
    n = sb_list(c, 0, o);
    if (i > n) i = n;
    for (int tries = 0; tries < 400; tries++) {
        i += d > 0 ? 1 : -1;
        if (i < 0 || i >= n) {                             /* the next category this track has */
            do c = (c + (d > 0 ? 1 : SB_NCAT - 1)) % SB_NCAT; while (!sb_cat_ok(c));
            n = sb_list(c, 0, o); i = d > 0 ? 0 : n - 1; first = -1;
        }
        if (first != (i & ~15)) { first = i & ~15; sb_list(c, first, o); }
        if (o[i & 15] < 0) continue;
        if (!snd_load(o[i & 15])) return;
        sb_cat = (uint8_t)c; sb_pg = (uint8_t)(i / 16); sb_trk = (int8_t)trk; sb_t = time_ticks();
        int plain = !L && hold_step < 0 && !Q.recording && !recording_armed && !countin &&
                    sticky != PG_STEPS && sticky != PG_NOTES && sticky != PG_SONG && sticky != PG_SONG2 && sticky != PG_SEL;
        if (sticky != PG_SND && plain) { sb_back = (int8_t)sticky; sticky = PG_SND; sb_was = 1; }
        if (sticky == PG_SND && !L) { sb_n = (uint8_t)sb_list(sb_cat, sb_pg * 16, sb_o); return; }
        char b[48], *p = put_s(b, sb_cat_name(c)); p = put_s(p, " · "); p = put_s(p, snd_name(o[i & 15]));
        p = put_s(p, " · "); p = put_u(p, (uint32_t)i + 1u); *p++ = '/'; p = put_u(p, (uint32_t)n); *p = 0;
        say(b);
        return;
    }
}
COLD static void sb_knob(int kn, int d)                  /* SOUNDS' knobs: CAT, PAGE, MOVE (a favourite), FAV */
{
    sb_t = time_ticks();
    if (kn == 0) sb_cat_step(d);
    if (kn == 1) sb_page_step(d, 0);
    if (kn == 2) sb_move(d);
    if (kn == 3) sb_fav(d);
}
COLD static void tempo(float b)                         /* SELECT / BPM turned: our tempo (not while a clock leads) */
{
    if (ext_on(time_ticks())) { say("TEMPO: THE MIDI CLOCK"); return; }
    seq_set_bpm(&Q, b);
}
COLD static void set_param(int pi, int d)                 /* a sound value turned on its page: a step of its own */
{
    gb_patch *q = &S.trk[trk].patch;
    const gb_pd *r = pi < 8 ? &gb_engines[q->engine]->ed[pi] : &gb_sp[pi];
    int v = gb_iv(base[trk][pi], r->min, r->max) + d;
    v = v < r->min ? r->min : v > r->max ? r->max : v;
    base[trk][pi] = q->p[pi] = gb_nv(v, r->min, r->max);
    if (pi == SP_VOICE || pi == SP_PRIO) snd_owner_off(&S, GB_OWNER(OWN_KEY, 0));   /* (the notes held so far: let go) */
    ml_knob(trk, pi + 1);
    if (preset_of[trk] >= 0) preset_of[trk] = -preset_of[trk] - 2;       /* edited: from that preset, now its own */
    lock_param = pi;                                                       /* (what a step's lock is on, next) */
}
COLD static void encoders(void)
{
    for (int e = 0; e < 7; e++) {
        int d = panel_enc_take(e) + ee[e]; ee[e] = 0;
        if (!d) continue;
        /* the acceleration: a fast turn goes further (lists and names stay one a click) */
        static uint32_t t_enc[7];
        uint32_t tn = time_ticks(), us = (tn - t_enc[e]) / lsb; t_enc[e] = tn;
        int da = d * (!hs[HS_ACCEL] ? 1 : us < 25000u ? 4 : us < 60000u ? 2 : 1);
        t_input = tn;
        int kn = e >= ENC_KNOB1 && e <= ENC_KNOB4 ? e - ENC_KNOB1 : -1;
        uint32_t L = lay();
        if (vis_on && sticky == PG_TRACKS && !L && e == ENC_SELECT && hold_step < 0) {   /* the visualiser: SELECT its style */
            vis_style = (uint8_t)(((vis_style + d) % VIS_N + VIS_N) % VIS_N); vis_named = 25; continue;
        }
        if ((L & (1u << BTN_HOME)) && hold_step < 0) {          /* HOME: SELECT the sections, the knobs their settings */
            if (e == ENC_SELECT) { layer_used = 1; home_sec = home_sec + d < 0 ? 0 : home_sec + d > 4 ? 4 : home_sec + d; continue; }
            if (home_sec && kn >= 0) { layer_used = 1; home_knob(home_sec, kn, d); continue; }
            if (home_sec) continue;
        }
        if (hold_step >= 0) {                                /* a step held: note, velocity, ratchet, nudge; a lock */
            hold_erase = 0;
            if (e == ENC_SELECT) tempo(Q.bpm + (float)da);
            else if (e == ENC_ALGORITHM) {
                const int8_t *ls = is_drum(trk) ? DLOCKABLE : LOCKABLE;
                int nl = is_drum(trk) ? (int)sizeof DLOCKABLE : (int)sizeof LOCKABLE, i = 0;
                while (i < nl && ls[i] != lock_param) i++;
                lock_param = ls[((i + d) % nl + nl) % nl];
            } else for (int si = -1; si < 16; si++) {         /* (every step held: the same change) */
                int st = si < 0 ? hold_step : ((hold_more >> si) & 1u) ? hold_page * 16 + si : -1;
                if (st < 0 || (si >= 0 && st == hold_step)) continue;
                if (e == ENC_PRESETS) {
                    if (is_drum(trk) && lock_param >= 4 && lock_param < SEQ_LK_LEVEL) lock_param &= 3;
                    int v = seq_lock_get(&Q, trk, st, lock_param);
                    if (v < 0) v = lock_own(trk, lock_param);
                    v += 5 * da; v = v < 0 ? 0 : v > 255 ? 255 : v;
                    seq_lock_set(&Q, trk, st, lock_param, v);
                } else if (kn == 0) {
                    if (!is_drum(trk)) { seq_step_note(&Q, trk, st, d); int n; if (seq_step_get(&Q, trk, st, &n, 0, 0)) last_note = n; }
                } else if (kn == 1) {
                    int n, v;
                    if (seq_step_get(&Q, trk, st, &n, &v, 0)) {
                        int acc = seq_step_flags(&Q, trk, st) & SEQ_ACCENT;   /* past the top: ACC; back down: off */
                        if (acc) { if (d < 0) seq_step_flag(&Q, trk, st, SEQ_ACCENT, 0); }
                        else if (v >= 127 && d > 0) seq_step_flag(&Q, trk, st, SEQ_ACCENT, 1);
                        else if (is_drum(trk)) { static const int LV[4] = { 40, 70, 100, 127 }; int i = v < 50 ? 0 : v < 85 ? 1 : v < 115 ? 2 : 3; i += d; i = i < 0 ? 0 : i > 3 ? 3 : i; seq_step_vel(&Q, trk, st, LV[i]); }
                        else seq_step_vel(&Q, trk, st, v + 4 * da);
                    }
                } else if (kn == 2) {                            /* RATCH x1..x4; below x1: SLIDE (a synth's) */
                    int r; seq_step_info(&Q, trk, st, 0, &r); int sl = seq_step_flags(&Q, trk, st) & SEQ_SLIDE;
                    if (sl) { if (d > 0) seq_step_flag(&Q, trk, st, SEQ_SLIDE, 0); }
                    else if (r <= 1 && d < 0 && !is_drum(trk)) { seq_step_flag(&Q, trk, st, SEQ_SLIDE, 1); say("SLIDE: GLIDES TO THE NEXT NOTE"); }
                    else seq_step_ratchet(&Q, trk, st, r + d);
                }
                else if (kn == 3) {
                    int nd = 0; seq_step_get(&Q, trk, st, 0, 0, &nd);
                    int n64 = nd * 64 / SEQ_TPS + d; n64 = n64 < -32 ? -32 : n64 > 31 ? 31 : n64;
                    seq_step_nudge(&Q, trk, st, n64 * SEQ_TPS / 64);
                }
            }
            continue;
        }
        if (e == ENC_ALGORITHM) {
            select_track(POS[((POS[trk] + d) % NTRK + NTRK) % NTRK]);
            if (sticky == PG_NOTES) { if (is_drum(trk)) sticky = PG_STEPS; ne_step = 0; ne_sub = 0; ne_wrote = 0; }
            continue;
        }
        if ((recording_armed || countin) && !Q.playing && !ltake.rec && !L) {   /* the REC screen */
            seq_track *k = &Q.t[trk];
            if (e == ENC_SELECT) tempo(Q.bpm + (float)da);
            if (e == ENC_PRESETS) choose_preset(d);
            if (kn == 0 && project_empty()) { rec_mode = (uint8_t)(d > 0); say(rec_mode ? "AT THE TEMPO SET (SELECT)" : "FREE TAKE: THE TEMPO FOLLOWS YOU"); }
            if (kn == 1 && ltake.armed_trk >= 0) bars_turn(ltake.armed_trk, d);   /* a take armed: FREE, 1, 2, 4, 8 */
            if (kn == 1 && ltake.armed_trk < 0 && !(project_empty() && !rec_mode)) {   /* the loop: 1, 2, 4, 8 bars */
                int spb = seq_steps_per_bar(&Q, trk), bars = k->pat[k->cur].steps / spb;
                static const int B[4] = { 1, 2, 4, 8 }; int i = 0; while (i < 3 && B[i] < bars) i++;   /* (the first >= now) */
                i = d > 0 ? (B[i] > bars ? i : i + 1) : i - 1;
                i = i < 0 ? 0 : i > 3 ? 3 : i;
                if (B[i] * spb <= SEQ_MAXSTEPS) { seq_set_len(&Q, trk, B[i] * spb); rec_bars = (uint8_t)(B[i] < 4 ? B[i] : 4); page = 0; }
            }
            if (kn == 2) { rec_count = (uint8_t)(d > 0); say(rec_count ? "PLAY: A BAR OF CLICKS, THEN REC" : "YOUR FIRST NOTE STARTS IT"); }
            if (kn == 3 && ltake.armed_trk >= 0) say("A TAKE IS AS PLAYED");
            else if (kn == 3) { k->quant = (uint8_t)(d > 0 ? 100 : 0); say(k->quant ? "QUANT ON: ON THE STEPS" : "QUANT OFF: AS PLAYED"); }
            continue;
        }
        if ((L & (1u << BTN_FX)) || (!L && sticky == PG_MASTER)) {  /* the master bus, the track's filter; SELECT the fade */
            if (L) layer_used = 1;
            if (e == ENC_SELECT) {
                if (L) { int f = S.pk_fade + d; S.pk_fade = (uint8_t)(f < 0 ? 0 : f > 15 ? 15 : f); }
                else if (d < 0) { fx_page = NFX_PG - 2; sticky = PG_FX3; }
                continue;
            }
            if (kn == 0 || kn == 3) {
                gb_smooth *f = kn == 0 ? &S.mfilt : &S.trk[trk].filt;
                float x = f->tgt + 0.02f * (float)da; x = x < -1.0f ? -1.0f : x > 1.0f ? 1.0f : x;
                if ((f->tgt < 0.0f && x > 0.0f) || (f->tgt > 0.0f && x < 0.0f) || gb_absf(x) < 0.01f) x = 0.0f;   /* (a stop at OFF) */
                sm_set(f, x);
            }
            if (kn == 1) S.dust = clamp01(S.dust + 0.01f * (float)da);
            if (kn == 2) S.duck = clamp01(S.duck + 0.01f * (float)da);
            continue;
        }
        if ((L & (1u << BTN_SEL)) || (!L && sticky == PG_SEL)) {    /* KEY: the key, scale, keys, chords */
            if (L) layer_used = 1;
            if (e == ENC_SELECT) { tempo(Q.bpm + (float)da); continue; }
            if (is_drum(trk) || kn < 0) continue;
            harm_cfg *h = &harm[trk];
            int layer = L != 0;                              /* (held: the key and scale of every synth track; tapped: this one) */
            if (kn == 0) { int r = (h->root + d + 48) % 12; for (int t = 0; t < NTRK; t++) if (!is_drum(t) && (layer || t == trk)) harm[t].root = (uint8_t)r; }
            if (kn == 1) { int sc = (h->scale + d + SCALE_N * 4) % SCALE_N; for (int t = 0; t < NTRK; t++) if (!is_drum(t) && (layer || t == trk)) harm[t].scale = (uint8_t)sc; }
            if (kn == 2) h->chord = (uint8_t)((h->chord + d + CH_N * 4) % CH_N);
            if (kn == 3) h->keys = (uint8_t)((h->keys + d + KEYS_N * 4) % KEYS_N);
            continue;
        }
        if (!L && (sticky == PG_ARP || sticky == PG_ARP2)) {         /* the arpeggiator */
            arp_cfg *c = &arpc[trk];
            if (e == ENC_SELECT) { sticky = d < 0 ? PG_ARP : PG_ARP2; continue; }
            if (is_drum(trk) || kn < 0) continue;
            if (sticky == PG_ARP) {
                if (kn == 0) { int m = c->mode + d; c->mode = (uint8_t)(m < 0 ? 0 : m >= AM_N ? AM_N - 1 : m); }
                if (kn == 1) { int r = c->rate + d; c->rate = (uint8_t)(r < 0 ? 0 : r > 6 ? 6 : r); }
                if (kn == 2) { int o = c->oct + d; c->oct = (uint8_t)(o < 1 ? 1 : o > 4 ? 4 : o); }
                if (kn == 3) { int g = c->gate + 2 * da; c->gate = (uint8_t)(g < 5 ? 5 : g > 100 ? 100 : g); }
            } else {
                if (kn == 0) { int w = c->swing + 2 * da; c->swing = (uint8_t)(w < 0 ? 0 : w > 100 ? 100 : w); }
                if (kn == 1) { int pb = c->prob + 2 * da; c->prob = (uint8_t)(pb < 0 ? 0 : pb > 100 ? 100 : pb); }
                if (kn == 2) {
                    c->hold = (uint8_t)(d > 0);
                    if (!c->hold) { kh_release(); if (A.trk == trk && !A.keys) { A.n = 0; A.latched = 0; } }
                    if (c->hold && c->mode == AM_OFF) say("KBD HOLD");
                }
                if (kn == 3) c->ord = (uint8_t)(d > 0);
            }
            continue;
        }
        if (L & (1u << BTN_ARP)) {                                   /* ROLL: its rate */
            if (L) layer_used = 1;
            if (e == ENC_SELECT) tempo(Q.bpm + (float)da);
            if (kn == 0) { roll_rate += d; roll_rate = roll_rate < 0 ? 0 : roll_rate > 4 ? 4 : roll_rate; }
            continue;
        }
        if ((L & (1u << BTN_SEQ)) || (!L && sticky == PG_STEPS)) {   /* STEPS */
            if (L) layer_used = 1;
            seq_track *k = &Q.t[trk];
            if (e == ENC_SELECT) tempo(Q.bpm + (float)d);
            if (kn == 0) { if (is_drum(trk)) last_note = ((last_note & 15) + d + 16) % 16; else { int n = last_note + d; last_note = n < 0 ? 0 : n > 127 ? 127 : n; } }
            if (L) {
                if (kn == 1) seq_set_div(&Q, trk, k->div + d);
                if (kn == 2) seq_set_swing(&Q, trk, k->swing + 2 * da);
            } else {
                if (kn == 1) { int q = k->quant + 5 * d; k->quant = (uint8_t)(q < 0 ? 0 : q > 100 ? 100 : q); }
                if (kn == 2) seq_set_gate(&Q, trk, k->gate + 5 * d);
            }
            if (kn == 3) { seq_set_len(&Q, trk, k->pat[k->cur].steps + da); page = 0; }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (!L && sticky == PG_SONG2) {                      /* SONG 2/2: LINE, PAT, BARS, LINES (as SLOOP's entry, section, bars, length) */
            if (e == ENC_SELECT) { if (d < 0) sticky = PG_SONG; continue; }
            if (e == ENC_PRESETS) { choose_preset(d); continue; }
            if (kn == 0) { int c = sg_cur + d, n = Q.song_n; sg_cur = c < 0 || !n ? 0 : c > n - 1 ? n - 1 : c; continue; }
            if (kn < 1 || kn > 3) continue;
            if (Q.song_rec) { say_bad("SONG REC RUNNING"); continue; }
            if (kn == 3) {
                int had = Q.song_n; seq_song_set_count(&Q, Q.song_n + d);
                if (Q.song_n > had) sg_cur = Q.song_n - 1;
                if (sg_cur >= Q.song_n) sg_cur = Q.song_n ? Q.song_n - 1 : 0;
                if (!Q.song_n) say("NO SONG NOW");
            } else if (!Q.song_n) say_bad("KNOB 4: ADD LINES");
            else if (kn == 1) {
                int pp = Q.song[sg_cur].pat[trk] + d; pp = pp < 0 ? 0 : pp > SEQ_NPAT - 1 ? SEQ_NPAT - 1 : pp;
                seq_song_set_pat(&Q, sg_cur, scope_one ? trk : -1, pp);
            } else seq_song_set_len(&Q, sg_cur, seq_song_len_of(&Q, sg_cur) + d);
            continue;
        }
        if ((L & (1u << BTN_SAVE)) || (!L && sticky == PG_SONG)) {  /* SONG: KNOB 1 the track's sound to all its patterns */
            if (L) layer_used = 1;
            if (e == ENC_SELECT) { if (!L && d > 0) sticky = PG_SONG2; else tempo(Q.bpm + (float)d); }
            if (kn == 0 && d > 0) {
                ps_keep(trk); for (int i = 0; i < SEQ_NPAT; i++) ps_copy(&PS, trk, PS.cur[trk], i);
                char b[40], *p = put_s(b, "TRACK "); p = put_u(p, (uint32_t)POS[trk] + 1u); p = put_s(p, " SOUND → ITS 8 PATTERNS"); *p = 0; say(b);
            }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (L & (1u << BTN_GLO)) {                           /* MIX: the levels */
            layer_used = 1;
            gb_track *mk = &S.trk[trk];                      /* the selected track's strip */
            if (e == ENC_SELECT) tempo(Q.bpm + (float)d);
            if (e == ENC_PRESETS) choose_preset(d);
            if (kn == 0) { sm_set(&mk->level, clamp01(mk->level.tgt + 0.01f * (float)da)); if (Q.t[trk].mute) seq_mute(&Q, trk, 0); }
            if (kn == 1) sm_set(&mk->pan, clamp01(mk->pan.tgt + 0.01f * (float)da));
            if (kn == 2) sm_set(&mk->send_dly, clamp01(mk->send_dly.tgt + 0.01f * (float)da));
            if (kn == 3) sm_set(&mk->send_rev, clamp01(mk->send_rev.tgt + 0.01f * (float)da));
            if (kn >= 0) ml_knob(trk, kn == 0 ? ML_LEVEL : kn == 1 ? ML_PAN : kn == 2 ? ML_DLY : ML_REV);
            continue;
        }
        if (L & (1u << BTN_EDIT)) {                          /* ERASE: shift, length, transpose (one undo a hold) */
            if (!Q.batch) { Q.batch = 1; Q.batch_done = 0; }
            layer_used = 1;
            if (kn == 0) seq_shift(&Q, trk, d);
            if (kn == 1) seq_resize(&Q, trk, d);
            if (kn == 2 && !is_drum(trk)) seq_transpose(&Q, trk, d);
            if (e == ENC_SELECT) tempo(Q.bpm + (float)d);
            continue;
        }
        if (L) { if (e == ENC_SELECT) tempo(Q.bpm + (float)d); layer_used = 1; continue; }
        /* the pages */
        int pg = sticky;
        if ((pg == PG_DSOUND || pg == PG_KIT) && !is_drum(trk)) pg = PG_OSC;
        if (e == ENC_SELECT && pg == PG_SND) { sb_t = time_ticks(); sb_page_step(d, 1); continue; }   /* SOUNDS: its pages, category after category */
        if (e == ENC_SELECT) {                               /* a page of several: the previous / next; else the tempo */
            int g = group_of(pg);
            if (g == BTN_EDIT && !is_drum(trk)) { edit_page = edit_page + d < 0 ? 0 : edit_page + d > 5 ? 5 : edit_page + d; sticky = EDIT_PG[edit_page]; }
            else if (g == BTN_FX) { fx_page = fx_page + d < 0 ? 0 : fx_page + d > NFX_PG - 1 ? NFX_PG - 1 : fx_page + d; sticky = FX_PG[fx_page]; }
            else if (g == BTN_ARP) sticky = d < 0 ? PG_ARP : PG_ARP2;
            else if (g == BTN_GLO) {
                static const uint8_t GP[6] = { PG_GLOBAL, PG_TEX1, PG_TEX2, PG_MIDI, PG_MIDI2, PG_GREC };
                int i = 0; while (i < 6 && GP[i] != sticky) i++;
                i = i >= 6 ? 0 : i + d < 0 ? 0 : i + d > 5 ? 5 : i + d; sticky = GP[i];
            }
            else if (g == BTN_ENV && !is_drum(trk)) { env_page = env_page + d < 0 ? 0 : env_page + d > 1 ? 1 : env_page + d; sticky = PG_ENV + env_page; }
            else if (g == BTN_LFO && !is_drum(trk)) { lfo_page = lfo_page + d < 0 ? 0 : lfo_page + d > 2 ? 2 : lfo_page + d; sticky = LFO_PG[lfo_page]; }
            else tempo(Q.bpm + (float)da);
            continue;
        }
        if (pg == PG_NOTES && !is_drum(trk)) {
            const seq_pat *pp = &Q.t[trk].pat[Q.t[trk].cur];
            int steps = pp->steps, sel = ne_sel();
            if (e == ENC_PRESETS) {                          /* the zoom: 8 16 32 64 steps */
                ne_zoom = ne_zoom + (d > 0 ? 1 : -1); ne_zoom = ne_zoom < 0 ? 0 : ne_zoom > 3 ? 3 : ne_zoom;
                char zb[16]; *put_s(put_u(put_s(zb, "ZOOM "), NE_ZOOM[ne_zoom]), " STEPS") = 0; say(zb);
                continue;
            }
            if (ne_sheet) {                                  /* the sheet: KNOB 1 the row, KNOB 2 its value */
                if (kn == 0) { ne_row = ne_row + d < 0 ? 0 : ne_row + d > 5 ? 5 : ne_row + d; continue; }
                if (kn != 1) continue;
                if (sel < 0) { say_bad("NO NOTE HERE"); continue; }
                if (ne_row == 0) seq_step_flag(&Q, trk, ne_step, SEQ_SLIDE, d > 0);
                else if (ne_row == 1) {
                    int nd = 0; seq_step_get(&Q, trk, ne_step, 0, 0, &nd);
                    int n64 = nd * 64 / SEQ_TPS + d; n64 = n64 < -32 ? -32 : n64 > 31 ? 31 : n64;
                    seq_step_nudge(&Q, trk, ne_step, n64 * SEQ_TPS / 64);
                } else if (ne_row == 2) {
                    static const uint8_t FC[3] = { SEQ_ALWAYS, SEQ_FILL, SEQ_NOFILL };
                    int c = 0; seq_step_info(&Q, trk, ne_step, &c, 0);
                    int f = c == SEQ_FILL ? 1 : c == SEQ_NOFILL ? 2 : 0; f = f + d < 0 ? 0 : f + d > 2 ? 2 : f + d;
                    seq_step_cond(&Q, trk, ne_step, FC[f]);
                } else if (ne_row == 3) {
                    Q.batch = 0;
                    int at = seq_ev_move(&Q, trk, sel, d > 0 ? 1 : -1, 0);
                    if (at < 0) say_bad("NO FURTHER"); else ne_follow(at);
                }
                continue;
            }
            if (kn == 0) {                                   /* note by note, the empty steps too */
                Q.batch = 0;
                if (d > 0) { if (ne_sub + 1 < ne_count(ne_step)) ne_sub++; else { ne_step = (ne_step + 1) % steps; ne_sub = 0; } }
                else if (ne_sub > 0) ne_sub--;
                else { ne_step = (ne_step + steps - 1) % steps; int c = ne_count(ne_step); ne_sub = c > 0 ? c - 1 : 0; }
            }
            if (kn == 1) {
                if (sel < 0) { uint8_t n1 = (uint8_t)last_note; seq_step_write(&Q, trk, ne_step, &n1, 1, 100); ne_sub = 0; continue; }
                int n = pp->ev[sel].note + d; n = n < 0 ? 0 : n > 127 ? 127 : n;
                if (!Q.batch) { Q.batch = 1; Q.batch_done = 0; }
                seq_ev_set(&Q, trk, sel, n, -1, -1, -1); last_note = n;
                play_on(GB_OWNER(OWN_PREVIEW, 0), trk, n, 0.7f); play_off(GB_OWNER(OWN_PREVIEW, 0), trk, n);   /* (heard) */
            }
            if (kn == 2 && sel >= 0) {                       /* its length, a step a click (below a step: a quarter) */
                int l = pp->ev[sel].len;
                l = d > 0 ? (l < SEQ_TPS ? SEQ_TPS : l + SEQ_TPS) : (l > SEQ_TPS ? l - SEQ_TPS : SEQ_TPS / 4);
                if (d > 0) {                                 /* (SLOOP's rules: POLY on under later notes, 4 at most; MONO to the next) */
                    int why = 3, mx = seq_ev_len_max(&Q, trk, sel, gb_vmode(S.trk[trk].patch.p) == VM_POLY, &why);
                    if (l > mx) { l = mx; say_bad(why == 0 ? "MONO: A NOTE AHEAD" : why == 1 ? "4 NOTES SOUND THERE" : "THE PATTERN'S END"); }
                }
                if (!Q.batch) { Q.batch = 1; Q.batch_done = 0; }
                seq_ev_set(&Q, trk, sel, -1, l, -1, -1);
            }
            if (kn == 3 && sel >= 0) {                       /* GHOST SOFT NORM HARD ACC (the accent: the step's) */
                static const int LV[4] = { 40, 70, 100, 127 };
                int f = force_of(pp->ev[sel].vel, pp->ev[sel].ratchet & SEQ_ACCENT) + d; f = f < 0 ? 0 : f > 4 ? 4 : f;
                if (!Q.batch) { Q.batch = 1; Q.batch_done = 0; }
                seq_ev_set(&Q, trk, sel, -1, -1, LV[f < 4 ? f : 3], -1);
                seq_step_flag(&Q, trk, ne_step, SEQ_ACCENT, f == 4);
            }
            if (e == ENC_SELECT) tempo(Q.bpm + (float)da);
            continue;
        }
        if (pg == PG_TRACKS) {
            if (kn == 0) seq_set_swing(&Q, -1, Q.swing + 2 * da);
            if (kn == 1) { gb_smooth *l = &S.trk[trk].level; sm_set(l, clamp01(l->tgt + 0.01f * (float)da)); if (Q.t[trk].mute) seq_mute(&Q, trk, 0); }
            if (kn == 2) {
                int st = Q.t[trk].pat[Q.t[trk].cur].steps, spb = seq_steps_per_bar(&Q, trk);
                if (!seq_count(&Q, trk)) bars_turn(trk, d);          /* an empty track: FREE, 1, 2, 4, 8 for its next take */
                else if (trk_played(trk) && st % spb == 0) {         /* a played one: 1, 2, 4, 8 bars (no note moves: SLOOP's length) */
                    int nb = st / spb, to = nb;
                    if (d > 0) { to = 1; while (to <= nb) to *= 2; } else { to = 8; while (to >= nb && to > 1) to /= 2; }
                    if (to <= loop_max_bars(&Q, trk) && to != nb) seq_set_len(&Q, trk, to * spb);
                }
                else seq_set_len(&Q, trk, st + da);
            }
            if (kn == 3) { gb_smooth *pn = &S.trk[trk].pan; sm_set(pn, clamp01(pn->tgt + 0.01f * (float)da)); }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_SND) {                                  /* SOUNDS: CAT, PAGE, MOVE, FAV; PRESETS the next sound */
            if (kn >= 0) sb_knob(kn, d);
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_USER && !is_drum(trk)) {
            static int erase_arm = -1;
            if (kn == 0) { us_place = us_place + d < 0 ? 0 : us_place + d > US_N - 1 ? US_N - 1 : us_place + d; erase_arm = -1; }
            if (kn == 1 && d > 0) { if (us_used(us_place) && apply_user(trk, us_place)) { char b[32], *p = put_s(b, "LOADED: "); p = put_s(p, us_name(us_place)); *p = 0; say(b); } else say_bad("EMPTY SLOT"); }
            if (kn == 2 && d > 0) user_save(us_place, 0);
            if (kn == 3 && d > 0 && us_used(us_place)) {
                if (erase_arm != us_place) { erase_arm = us_place; say_imp("AGAIN TO ERASE"); }
                else { erase_arm = -1; user_save(us_place, 1); }
            }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_FX) {
            gb_track *k = &S.trk[trk];
            if (kn == 0) set_param(is_drum(trk) ? SP_E3 : SP_DIST, da);
            if (kn == 1) sm_set(&k->send_cho, clamp01(k->send_cho.tgt + 0.01f * (float)da));
            if (kn == 2) sm_set(&k->send_dly, clamp01(k->send_dly.tgt + 0.01f * (float)da));
            if (kn == 3) sm_set(&k->send_rev, clamp01(k->send_rev.tgt + 0.01f * (float)da));
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_DSOUND && is_drum(trk)) {
            gb_patch *dp = &S.trk[trk].patch; int n = last_note & 15;
            if (kn == 0) last_note = ((last_note & 15) + d + 16) % 16;
            if (kn == 1) { int t2 = dp->dtune[n] + d; dp->dtune[n] = (int8_t)(t2 < -8 ? -8 : t2 > 7 ? 7 : t2); }
            if (kn == 2) { int t2 = dp->ddec[n] + d; dp->ddec[n] = (int8_t)(t2 < -4 ? -4 : t2 > 3 ? 3 : t2); }
            if (kn == 3) { if (d > 0) dp->dmute |= (uint16_t)(1u << n); else dp->dmute &= (uint16_t)~(1u << n); }
            if (hs[HS_DPREV] && kn <= 2) { int m = last_note & 15; play_on(GB_OWNER(OWN_PREVIEW, 0), trk, m, 0.8f); play_off(GB_OWNER(OWN_PREVIEW, 0), trk, m); }   /* (DRUM PREVIEW: heard) */
            if (e == ENC_PRESETS) choose_preset(d);
            if (e == ENC_SELECT) sticky = d < 0 ? PG_KIT : PG_DSOUND;
            continue;
        }
        if (pg == PG_SEL2) {
            if (e == ENC_SELECT) { sticky = d < 0 ? PG_SEL : PG_SEL2; continue; }
            if (!is_drum(trk) && kn == 0) { int tr = harm[trk].transpose + d; harm[trk].transpose = (int8_t)(tr < -24 ? -24 : tr > 24 ? 24 : tr); }
            if (!is_drum(trk) && kn == 1) { int x = strum[trk] + 2 * da; strum[trk] = (uint8_t)(x < 0 ? 0 : x > 100 ? 100 : x); }
            if (!is_drum(trk) && kn == 2) vlead[trk] = (uint8_t)(d > 0);
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_FX2 || pg == PG_FX3) {
            int fk = -1;
            if (pg == PG_FX2 && kn == 0) { dly_div += d; dly_div = dly_div < 0 ? 0 : dly_div >= NDIV ? NDIV - 1 : dly_div; }
            if (pg == PG_FX2 && kn == 1) { float f = S.dly_fb + 0.01f * (float)da; S.dly_fb = f < 0.0f ? 0.0f : f > 0.95f ? 0.95f : f; }
            if (pg == PG_FX3 && kn == 0) { float f = S.rev_fb + 0.01f * (float)da; S.rev_fb = f < 0.3f ? 0.3f : f > 0.97f ? 0.97f : f; }
            if (pg == PG_FX3 && kn == 3) S.cho_depth = clamp01(S.cho_depth + 0.01f * (float)da);
            if (pg == PG_FX2 && (kn == 2 || kn == 3)) fk = kn - 2;
            if (pg == PG_FX3 && (kn == 1 || kn == 2)) fk = kn + 1;
            if (fk >= 0) { int x = fxv[fk] + da; fxv[fk] = (uint8_t)(x < 0 ? 0 : x > 127 ? 127 : x); fxv_apply(); }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_TEX1 || pg == PG_TEX2) {
            uint8_t *x = S.tx;
            if (pg == PG_TEX1) {
                if (kn == 0) { int v2 = x[0] + d; x[0] = (uint8_t)(v2 < 0 ? 0 : v2 > 10 ? 10 : v2); }
                if (kn == 1) { int v2 = x[1] + da; x[1] = (uint8_t)(v2 < 0 ? 0 : v2 > 127 ? 127 : v2); }
                if (kn == 2) { int v2 = (int8_t)x[2] + da; x[2] = (uint8_t)(int8_t)(v2 < -64 ? -64 : v2 > 63 ? 63 : v2); }
                if (kn == 3) { int v2 = x[3] + da; x[3] = (uint8_t)(v2 < 0 ? 0 : v2 > 127 ? 127 : v2); }
            } else {
                if (kn == 0) { int v2 = x[4] + d; x[4] = (uint8_t)(v2 < 0 ? 0 : v2 > 4 ? 4 : v2); }
                if (kn == 1) x[5] = (uint8_t)(d > 0);
                if (kn == 2) x[6] = (uint8_t)(d > 0);
            }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_MIDI || pg == PG_MIDI2) {
            int was_ext = MC.sync;
            if (pg == PG_MIDI) {
                if (kn == 0) { int x = MC.in + d; MC.in = (uint8_t)(x < 0 ? 0 : x >= MIDI_IN_N ? MIDI_IN_N - 1 : x);
                    say(MC.in == MIDI_IN_TRACKS ? "CHANNEL 1-5: TRACKS · 10: DRUMS" : MC.in == MIDI_IN_SEL ? "ALL CHANNELS: THE TRACK SELECTED" : "MIDI IN OFF"); }
                if (kn == 1) { int x = MC.out + d; MC.out = (uint8_t)(x < 0 ? 0 : x >= MIDI_OUT_N ? MIDI_OUT_N - 1 : x);
                    say(MC.out == MIDI_OUT_KEYS ? "OUT: THE KEYS" : MC.out == MIDI_OUT_SEQ ? "OUT: KEYS, SEQUENCER, ARP" : "MIDI OUT OFF"); }
                if (kn == 2) { MC.sync = (uint8_t)(d > 0 ? MIDI_SYNC_EXT : MIDI_SYNC_INT);
                    say(MC.sync == MIDI_SYNC_EXT ? "FOLLOWS THE MIDI CLOCK (USB, TRS)" : "INTERNAL TEMPO"); }
                if (kn == 3) { MC.clk_out = (uint8_t)(d > 0); say(MC.clk_out ? "CLOCK SENT ON USB" : "CLOCK OUT OFF"); }
            } else if (kn == 0) { MC.dmap = (uint8_t)(d > 0 ? MIDI_DMAP_PADS : MIDI_DMAP_GM); say(MC.dmap ? "DRUMS: NOTES 36 TO 51" : "DRUMS: GM NOTES (36 KICK, 38 SNARE)"); }
            else if (kn == 1) { int x = m_rmch + d; m_rmch = (uint8_t)(x < 0 ? 0 : x > 16 ? 16 : x); say(m_rmch ? "REMOTE: CC 102-115 = THE BUTTONS" : "REMOTE OFF"); }
            else if (kn == 2) { int x = m_fxch + d; m_fxch = (uint8_t)(x < 0 ? 0 : x > 16 ? 16 : x); say(m_fxch ? "FX CH: NOTES 53-79 = THE PUNCH-INS" : "FX CH OFF"); }
            else if (kn == 3) { int x = m_bend + d; m_bend = (uint8_t)(x < 1 ? 1 : x > 12 ? 12 : x); }
            if (was_ext == MIDI_SYNC_EXT && MC.sync != MIDI_SYNC_EXT) ext_start = 0;
            if (MC.in == MIDI_IN_OFF) midi_all_off(-1);
            t_input = time_ticks();
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_GREC) {
            if (kn == 0) { rec_keep = (uint8_t)(d > 0); say(rec_keep ? "TRACK CHANGED: THE TAKE GOES ON THERE" : "TRACK CHANGED: REC STOPS"); }
            if (kn == 1) { rec_count = (uint8_t)(d > 0); say(rec_count ? "PLAY: A BAR OF CLICKS, THEN REC" : "YOUR FIRST NOTE STARTS IT"); }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (pg == PG_GLOBAL) {
            if (kn == 0 && ext_on(time_ticks())) { say("TEMPO: THE MIDI CLOCK"); continue; }
            if (kn == 0) tempo(Q.bpm + (float)da);
            if (kn == 1) seq_set_swing(&Q, -1, Q.swing + 2 * da);
            if (kn == 2) { click_mode += d; click_mode = click_mode < 0 ? 0 : click_mode > 2 ? 2 : click_mode; }
            if (kn == 3) { gb_smooth *l = &S.trk[trk].level; sm_set(l, clamp01(l->tgt + 0.01f * (float)da)); }
            if (e == ENC_PRESETS) choose_preset(d);
            continue;
        }
        if (e == ENC_PRESETS) { choose_preset(d); continue; }
        if (kn < 0 || !((pg >= PG_OSC && pg <= PG_KIT && pg != PG_FX) || pg == PG_PLAY || FX_TRACK_PG(pg) || ((pg == PG_LFO || pg == PG_LFODEST || pg == PG_LFO3) && !is_drum(trk)))) continue;
        int pi = is_drum(trk) && !FX_TRACK_PG(pg) ? kn : params_of(pg, trk)[kn];
        if (pi < 0 || !pdesc_of(S.trk[trk].patch.engine, pi)->name[0] || (pi == SP_FTYPE && !is_va(trk))) continue;
        const gb_pd *r = pi < 8 ? &gb_engines[S.trk[trk].patch.engine]->ed[pi] : &gb_sp[pi];
        int fm = pdesc_of(S.trk[trk].patch.engine, pi)->fmt, ch = fm == PF_LIST || r->max - r->min < 30;   /* (a short range: a step a click) */
        set_param(pi, fm == PF_LIST8 ? 8 * d : ch ? d : da);
    }
}

/* the interface's round (the panel read): from the flash window (COLD), never while a save has it off */
/* the timer's interrupt, 4000 times a second (3.3): a column of the panel each time, the USB port every other */
static void tick(void) { static uint8_t odd; panel_tick(); if ((odd ^= 1u)) usb_tick(); }
static uint32_t t_ui;                       /* the last pass of the panel and the screen (3.3) */
static uint32_t t_pot, t_stat, t_autosave, ld_sum, ld_halves, ld_peak;
COLD __attribute__((noinline)) static void ui_round(void)
{
    if (cal_step >= 0) { cal_tick(); leds(); msg_tick(); panel_dark(); ui_step(); return; }
    if (ed_pending && !ed_tx_n) ed_request();
    if (splash_on && ((time_ticks() - splash_t0) / lsb > VIS_SPLASH_MS * 1000u || held_btn || panel_keys() || ek_held || t_input != splash_t0)) {
        splash_on = 0; if (splash_bad) say_bad(splash_say); else if (splash_say) say_imp(splash_say);
    }
    buttons(); sb_tick(); keys(); notes_advance(); erase_pass(); encoders(); leds();
    fm6_bank_poll();
    msg_tick();
    {                                               /* the clock in: said when it comes and when it goes */
        static int ext_was;
        int e = ext_on(time_ticks());
        if (e != ext_was) { ext_was = e; if (MC.sync == MIDI_SYNC_EXT) { char b[32], *p = put_s(b, e ? "MIDI CLOCK · " : "MIDI CLOCK LOST"); if (e) { p = put_u(p, (uint32_t)(Q.bpm + 0.5f)); p = put_s(p, " BPM"); } *p = 0; say(b); } }
    }
    if (chain_on && Q.song_play) { chain_on = 0; Q.switch_now = 0; }
    uint32_t now = time_ticks();
    if (ltake.armed_trk >= 0 && (ltake.rec || ltake.follow)) {   /* the take: a first one closed after 24 s; a later one starts and ends on its bar lines */
        int t = ltake.armed_trk, nb = loop_poll(&ltake, &Q, loop_us());
        if (nb != 0) loop_closed(nb, t);
        else if ((uint32_t)(now - loop_said) > 400000u * lsb) { loop_said = now; loop_say(); }
    }
    if (save_soon && !saving && (uint32_t)(now - t_save_soon) > 1600000u * lsb) { save_soon = 0; save_project(1); }   /* (SAVE + a key) */
    if (fav_dirty && !saving && flash_ok && !sbuf_held() && (uint32_t)(now - fav_t) > 1200000u * lsb) { fav_dirty = 0; fav_save(); }   /* (3.2: your favourites, once they rest) */
    /* an editor's transfer left halfway (the page closed, the cable out): dropped, sbuf free again (3.1.1) */
    if (saving && ed_wr.on && ed_wr.kind == 0 && (uint32_t)(now - ed_t_last) > 10000000u * lsb) {   /* (3.3.5) a project halfway */
        store_proj_abort(); saving = 0; ed_wr.on = 0; say_bad("PROJECT: STOPPED");
    }
    if (!saving && !ed_bk.phase && (ed_wr.on || ed_sm.on || ed_bk.on) && (uint32_t)(now - ed_t_last) > 10000000u * lsb) {
        ed_wr.on = 0; ed_bk.on = 0;
        if (ed_sm.on) { ed_sm.on = 0; ed_sm.phase = 0; say_bad("SAMPLE: STOPPED"); }
    }
    /* the autosave: stopped, untouched for 2.5 s, at most every 20 s */
    if (!Q.playing && !saving && !ed_wr.on && (uint32_t)(now - t_input) > 2500000u * lsb && (uint32_t)(now - t_autosave) > 20000000u * lsb) {
        t_autosave = now; save_project(0);
    }
    if ((uint32_t)(now - t_pot) > 20000u * lsb) {
        t_pot = now;
        static uint8_t batt_n = 49; if (++batt_n >= 50) { batt_n = 0; batt_poll(); }   /* (3.3.6) the battery, once a second */
        int pot = adc_read(ADC_CH_MASTER);
        if (pot >= 0 && ed_mst >= 0 && (pot > ed_mst_pot + 40 || pot < ed_mst_pot - 40)) ed_mst = -1;   /* (the knob turned: back to it) */
        if (pot >= 0 && ed_mst < 0) { float x = (float)pot * (1.0f / 1023.0f); sm_set(&S.master, 0.5f * x * x); ed_mst_pot = pot; }
        else if (ed_mst >= 0) { float x = (float)ed_mst * (1.0f / 126.0f); sm_set(&S.master, 0.5f * x * x); }
    }
    {                                               /* the delay's time follows the tempo */
        static float dly_want;
        float want = rate_hz * 60.0f / Q.bpm * (float)DIV[dly_div].b24 * (1.0f / 24.0f);
        if (want != dly_want) { dly_want = want; sm_set(&S.dly_time, want); }
    }
    if ((uint32_t)(now - t_stat) > 500000u * lsb && ld_halves) {   /* the load, twice a second */
        t_stat = now;
        load_avg = ld_sum / ld_halves; load_peak = ld_peak;
        static uint32_t windows; if (++windows > 6u && ld_peak > load_max) load_max = ld_peak;   /* (after the first 3 s: the start is not play) */
        ld_sum = 0; ld_halves = 0; ld_peak = 0;
        if (c1_fail) { c1_fail = 0; say_bad("CORE 2: FAILED"); }
    }
    panel_dark();
    ui_step();
}

#ifdef BOOT_DIAG
/* a diagnostic build (fw/build.sh with BOOT_DIAG=1): each step of the start adds a block to a bar across the screen
 * (yellow, white every fifth; green or red for a check) and holds 400 ms, so the blocks seen before a restart say where
 * it stops. In RAM: it must not need the flash window. The panel first, then the second core, then the window's checks:
 * the RAM image's code against the window's copy of it (green: the same), then a call into the window's code */
static void boot_mark(int n, uint16_t c) { lcd_fill(10 + (n - 1) * 11, 104, 8, 32, c); time_wait_us(400000); }
static void boot_step(int n) { boot_mark(n, n % 5 ? RGB(255, 210, 60) : RGB(255, 255, 255)); }
extern const uint32_t __text_end[];
COLD __attribute__((noinline)) static int boot_xcall(int v) { return v * 3 + 1; }   /* (code in the window) */
static void boot_window(int n)
{
    const uint32_t *ram = (const uint32_t *)(uintptr_t)0x01C30000u, *end = __text_end;
    const uint32_t *win = (const uint32_t *)(uintptr_t)*(const volatile uint32_t *)0x02000124u;   /* (fw/xip.S: "r1 = __payload_start") */
    int same = 1;
    for (; ram < end; ram++, win++) if (*ram != *win) { same = 0; break; }
    boot_mark(n, same ? RGB(40, 230, 90) : RGB(255, 40, 40));
    int (*volatile f)(int) = boot_xcall;                  /* (called through a pointer: not folded away) */
    boot_mark(n + 1, f(5) == 16 ? RGB(40, 230, 90) : RGB(255, 40, 40));
}
#define BOOT_STEP(n) boot_step(n)
#else
#define BOOT_STEP(n) ((void)0)
#endif
/* the start's flash work (3.1: out of app_main, which stays in RAM for its loop: this runs once, from the flash
 * window, before any save): the sample bank, the projects (SLOOP 2.8's the first time), the user slots, the
 * settings, the sounds of SLOOP 2.8 kept */
/* the store's flash: kept as a pointer (app/store.c) and read during saves, the window off: in RAM */
static const store_flash SF RAMCONST = { flash_read, flash_erase_start, flash_program_start, flash_busy };
/* (in RAM: an erase or a write keeps the flash window off until it is done; these return with it back on) */
__attribute__((noinline)) static void ram_erase(uint32_t a) { flash_wait_idle(); if (!flash_erase_start(a)) flash_wait_idle(); }
__attribute__((noinline)) static int ram_store_run(void) { int r; while ((r = store_step()) == 1) { } flash_wait_idle(); return r; }
/* (3.3.5) the project saved now, at the start: its parts made between the flash's operations (the window back on) */
COLD static int save_now(void)
{
    uint32_t crc, n = proj_measure(&crc);
    if (n > STORE_PMAX || store_proj_begin(n, crc, 1)) return -1;
    saving_len = n;
    for (;;) {
        int r = ram_store_run();
        if (r != 3) { if (r == 2) { saved_crc = crc; saved_len = n; proj_used = 1; } return r; }
        proj_feed();
    }
}
COLD __attribute__((noinline)) static void boot_flash(void)
{
    /* the projects: SPI0 taken from the flash window (we run from RAM), the last project back */
    smp_bank_set((const uint8_t *)(uintptr_t)(FLASH_WIN + 0x44000u));   /* the sample bank (flash 0x48000, tools/mkfw.py), checked once */
    BOOT_STEP(8);
    flash_ok = flash_open() == 0;
    if (flash_ok) {
        store_init(&SF);
        BOOT_STEP(9);
        {                                                 /* your sounds: ours, else SLOOP 2.8's user presets (once); first (3.1.2):
                                                           * a project's tracks on one of them are known as such when it loads */
            int n = store_sounds_load(sbuf, sizeof sbuf);
            if (n != US_FILE && (n == US_LEN || n == US_FILE_FV || us_from_sloop(flash_read, sbuf + 4096, sbuf) >= 0)) {   /* (a bank without favourites or theme: SLOOP's, once) */
                int had = n == US_FILE_FV;
                us_from_bytes(sbuf, had ? n : US_LEN);        /* (the places used: the favourites check them) */
                if (!had) sloop_favs(flash_read, sbuf + 8000, us_fav);   /* its favourites */
                sloop_theme(flash_read, sbuf + 8000, us_theme);         /* its colour theme */
                us_fav_put(sbuf);
                if (!store_sounds_begin(sbuf, US_FILE)) { int r = ram_store_run(); n = r == 2 ? US_FILE : -1; }
            }
            us_from_bytes(sbuf, US_LEN_OK(n) ? n : 0);
            color_apply();                                /* (your theme, if it is the one chosen) */
        }
        {   /* SLOOP 2.8's staging area (flash 0xE0000-0xE4FFF: the update loader that installed us, its request): left
             * over, the boot code could run it again one day. Erased when not blank (no data of ours is there) */
            static uint32_t a[2], r[2];
            if (!flash_read(0xE0000u, a, 8) && !flash_read(0xE4F00u, r, 8) && (a[0] & a[1] & r[0] & r[1]) != 0xFFFFFFFFu)
                for (uint32_t s = 0xE4000u + 0x1000u; s > 0xE0000u; s -= 0x1000u) ram_erase(s - 0x1000u);
        }
        BOOT_STEP(10);
        proj_used = store_proj_find(0, 0, 0) == 0 ? 1u : 0u;
        if (!proj_used && !safe_start) {                  /* (3.3.5) one project: the newest of the 8 places of 3.0 to 3.3.4, once */
            int old = store_old_latest(), n = old >= 0 ? store_old_load(old, sbuf, sizeof sbuf) : -1;
            project_extra x; memset(&x, 0, sizeof x); x.ps = &PS;
            if (n > 0 && project_load(&Q, &S, &x, sbuf, (uint32_t)n) == 0) {
                load_after(&x);
                if (save_now() == 2) { static char m[40]; char *p = put_s(m, "ONE PROJECT NOW: YOUR PROJECT "); p = put_u(p, (uint32_t)old + 1); *p = 0; boot_msg = m; }
            }
        }
        if (!proj_used && !safe_start) {                  /* SLOOP 2.8's (never written by SLOOP 3): the project */
            uint8_t *b = (uint8_t *)fxmem;                /* (the delay's memory: free until the sound starts) */
            const uint8_t *secs[4]; int have = 0;
            for (int i = 0; i < 4; i++) { secs[i] = sloop_read(flash_read, 1 + i, b + (uint32_t)i * SLOOP_PROJ_LEN) ? b + (uint32_t)i * SLOOP_PROJ_LEN : 0; have |= secs[i] != 0; }
            const uint8_t *w = sloop_read(flash_read, 7, b + 4u * SLOOP_PROJ_LEN) ? b + 4u * SLOOP_PROJ_LEN : 0;
            if (have || w) {
                project_extra x; memset(&x, 0, sizeof x);
                new_project(0);
                if (sloop_import(secs, w, &Q, &S, &x, preset_of)) {
                    base_from_sound(); ps_all();
                    for (int t = 0; t < 3; t++) harm[t].transpose = (int8_t)(x.harm[t][4] - 64);
                    for (int t = 0; t < 3; t++) {                 /* the arpeggiators, as SLOOP's tracks had them */
                        const uint8_t *a = x.arp[t];
                        arpc[t] = (arp_cfg){ a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7] };
                    }
                    if (x.dly_div < NDIV) dly_div = x.dly_div;
                    for (int i = 0; i < 7; i++) S.tx[i] = x.tex[i];   /* (TEXTURE: the working project's) */
                    for (int i = 0; i < 4; i++) fxv[i] = x.fxv[i];      /* (COLR MIX DAMP CRT) */
                    fxv_apply();
                    /* each section's own sounds: its pattern's (the mix stays the working project's) */
                    for (int t = 0; t < 4; t++) {
                        gb_track *k = &S.trk[t];
                        float mx[5] = { k->level.tgt, k->pan.tgt, k->send_cho.tgt, k->send_dly.tgt, k->send_rev.tgt };
                        for (int i = 0; i < 4; i++) if (secs[i]) {
                            int pi = sloop_sound(secs[i], t, &S);
                            ps_take(&PS, t, i, &k->patch, k->patch.p, pi);
                        }
                        sm_set(&k->level, mx[0]); sm_set(&k->pan, mx[1]); sm_set(&k->send_cho, mx[2]); sm_set(&k->send_dly, mx[3]); sm_set(&k->send_rev, mx[4]);
                        ps_give(&PS, t, Q.t[t].cur, &k->patch, base[t], &preset_of[t]); PS.cur[t] = (uint8_t)Q.t[t].cur;
                    }
                    {                                             /* its song, over the sections */
                        int sn = sloop_obj(flash_read, 0, sbuf, 3840u);
                        if (sn > 0) sloop_song(sbuf, sn, secs, &Q);
                    }
                    if (save_now() == 2) boot_msg = "SLOOP 2.8 IMPORTED";
                }
            }
            memset(fxmem, 0, sizeof fxmem);
        }
        BOOT_STEP(11);
        if (!safe_start && proj_used) load_project(0);
        if (safe_start && proj_used) safe_hold = 1;       /* (the empty project never over yours: saved only when asked twice, HOME) */
        BOOT_STEP(12);
        flash_plain_from(0x93000u);                       /* SLOOP's data, in clear through the window: its user slots */
        for (int k = 0; k < SMP_NUSR; k++) {
            static const uint32_t AT[SMP_NUSR] = { 0xA0000u, 0xB4000u, 0xC8000u, 0xE7000u };
            smp_user_scan(k, (const uint8_t *)(uintptr_t)(FLASH_WIN + AT[k] - 0x4000u));
        }
        slice_scan_all();                                 /* (SLICE's tables: BREAK, the user slots; before the sound) */
        BOOT_STEP(13);
        {                                                 /* SYN1..SYN4: SLOOP's (else as SLOOP starts them); its MIDI */
            int n = sloop_obj(flash_read, 0, sbuf, 3840u);
            if (n > 0 && sloop_synkits(sbuf, n, gb_dsyn, gb_dsyn_crush)) syn_src = 1;
            sloop_mset ms;
            if (n > 0 && !home_had) {                     /* (none of ours yet: its MIDI LEARN and MIDI settings) */
                sloop_midi(sbuf, n, ml_tab, &ms);
                if (ms.have) { hs[HS_DPREV] = ms.dprev; hs[HS_RESO] = ms.reso; m_bend = ms.bend; m_fxch = ms.fxch; m_rmch = ms.rmch; MC.dmap = ms.dmap; MC.clk_out = ms.clk; }
            }
        }
        {                                                 /* SLOOP's FM6 bank (B1..B27): where it is, which slots hold one */
            int n = sloop_obj(flash_read, 8, sbuf, 3840u);
            const uint8_t *b = sbuf;
            if (n == 3472 && b[0] == 'F' && b[1] == 'M' && b[2] == '6' && b[3] == 'B' && (b[4] | b[5] << 8) == 1 && (b[6] | b[7] << 8) == 27) {
                fm6_bank_at = sloop_obj_at + 16u; fm6_bank_used = (uint32_t)b[8] | (uint32_t)b[9] << 8 | (uint32_t)b[10] << 16 | (uint32_t)b[11] << 24;
                fm6_src = 1;
            }
        }
        {                                                 /* yours, once saved from the editor (app/xbank.h): over SLOOP 2.8's */
            uint32_t u;
            if (!flash_read(XB_SYN_AT, sbuf, XB_SYN_LEN) && !xb_check(sbuf, XB_SYN_LEN, 0, 0)) {
                for (int k = 0; k < 4; k++) xb_kit_unpack(sbuf + 16 + k * XB_KIT_LEN, gb_dsyn[k], &gb_dsyn_crush[k]);
                syn_src = 2;
            }
            if (!flash_read(XB_FM6_AT, sbuf, XB_FM6_LEN) && !xb_check(sbuf, XB_FM6_LEN, 1, &u)) { fm6_bank_at = XB_FM6_AT + 16u; fm6_bank_used = u; fm6_src = 2; }
        }
        BOOT_STEP(14);
        BOOT_STEP(15);
        sloop_set ss;                                     /* none of ours yet: SLOOP 2.8's menu and calibration */
        if (!home_had && sloop_settings(flash_read, sbuf, &ss)) {
            hs[HS_LOWCUT] = ss.lowcut; hs[HS_LIGHTS] = ss.lights; hs[HS_KEYS] = ss.keys; hs[HS_NOTES] = ss.notes;
            hs[HS_USB] = ss.usb_full; hs[HS_RATE] = ss.usb_rate; hs[HS_CORE2] = ss.core2; hs[HS_ACCEL] = ss.accel;
            if (ss.theme_on && theme_ok()) scr_color = UI_NTHEMES; else if (ss.palette) scr_color = (uint8_t)(1 + ss.palette);   /* (its COLOR, if not the first) */
            color_apply();
            if (ss.cal && !panel_map(ss.btn, ss.enc, ss.dir)) { cal_ok = 1; memcpy(cal_btn, ss.btn, 14); memcpy(cal_enc, ss.enc, 7); memcpy(cal_dir, ss.dir, 7); }
            home_apply();
        }
    }
}

/* (3.3.2) the last run's crumb, at the start: what the first message says if it stopped without a power-off */
COLD static void crumb_report(void)
{
    crash_msg[0] = 0;
    if (crumb.magic == CRUMB_MAGIC && crumb.blocks > 0u && crumb.where < W_N && crumb.fx <= (uint32_t)PK_N) {      /* the last run stopped without a power-off */
        static const char *const WN[W_N] = { "?", "MIDI", "SEQ", "VOICES", "MIX/FX", "USB", "SAVE", "FLASH", "SCREEN" };
        char *p = put_s(crash_msg, (sys_reset_bits() & 4u) ? "FROZE IN " : "RESTART IN "); p = put_s(p, WN[crumb.where]);   /* (3.3.3: the watchdog's restart: it had stopped answering) */
        if (crumb.fx >= 1u && crumb.fx <= (uint32_t)PK_N) { p = put_s(p, " · "); p = put_s(p, PK_NAME[crumb.fx - 1u]); }
        uint32_t sec = crumb.blocks / 689u;               /* (689 blocks of 64 a second at 44.1 kHz) */
        p = put_s(p, " · "); p = sec < 120u ? put_s(put_u(p, sec), " S") : put_s(put_u(p, sec / 60u), " MIN"); *p = 0;
    }
}
/* the start, up to the sound on: from the flash window (3.3.3: the slot's room; nothing here runs with it off) */
COLD __attribute__((noinline)) static void start(uint32_t arg)
{
    lsb = arg && arg < 200u ? arg : 60u;
    time_init(lsb);                                       /* (3.3.4: first: the supplies' steps wait on it. 3.3.3 waited on a timer
                                                           * not yet started: a black screen) */
    cpu0_traps_quiet();                                   /* (3.3.2: whatever an earlier firmware armed) */
    (void)sys_reset_bits();                               /* (3.3.3: why the last run ended, read first) */
    sys_supplies_stock();                                 /* (3.3.3: the supplies as M-VAVE's firmware sets them, before the second core
                                                           * starts: at the boot loader's, some FM-1s read the flash and the RAM wrong) */
    stack_mark();
    crumb_report();                                       /* (the last run, if a restart ended it) */
    crumb.magic = CRUMB_MAGIC; crumb.fx = 0; crumb.blocks = 0; crumb_at(W_NONE);
#ifdef BOOT_DIAG
    lcd_init();
    BOOT_STEP(1);
    int c1 = cpu1_start();
    BOOT_STEP(2);
    boot_window(3);                                       /* (blocks 3 and 4) */
#else
    int c1 = cpu1_start();                                /* first, before any other work */
    lcd_init();
#endif
    panel_init();
    adc_init();
    keys_map();
    BOOT_STEP(5);
    if (c1 == 0 && cpu1_core_id() == 1u) two = 1;
    for (int k = 0; k < FM6_NBANK; k++) fb_nm[k][0] = 0;
    snd_init(&S, fxmem, pmem);
    float rate = (float)AUDIO_RATE_NUM / (float)AUDIO_RATE_DEN;
    rate_hz = rate;
    demo_song song;
    demo_song_init(&song, &S, rate, 120.0f, 0);           /* (its effects' settings; the sounds come next) */
    sm_jump(&S.master, 0.0f);
    midi_cfg_default(&MC);
    mclk_init(&MK, lsb);
    BOOT_STEP(6);
    new_project(1);
    BOOT_STEP(7);
    for (int i = 0; i < 20 * 11; i++) { panel_step(); time_wait_us(60); }   /* (a few frames: the buttons held at power-on) */
    if ((panel_raw_buttons() & 3u) == 1u) {               /* OCT- alone: RESCUE */
        rescue = 1; ui_flush();
        for (;;) {
            panel_step(); time_wait_us(60);
            uint32_t rel, pr = panel_button_edges(&rel);
            if (pr & (1u << BTN_REC)) sys_download_mode();
            if (pr & (1u << BTN_PLAY)) break;
        }
        rescue = 0; safe_start = 1; new_project(0);       /* (empty, as said: not the demo's loop; 3.1.2) */
    }
    boot_flash();
    BOOT_STEP(16);
    mp_usb.sxe = ed_rx; mp_usb.sxe_cap = sizeof ed_rx;
    usb_midi_open();                                      /* (25 ms: before the sound) */
    trs_midi_open();
    splash_bad = (uint8_t)(!two || crash_msg[0]); splash_say = !two ? "CORE 2: SILENT" : crash_msg[0] ? crash_msg : safe_start ? "SAFE START: AN EMPTY PROJECT" : boot_msg;   /* (3.3: no popup with the name and version) */
    if ((panel_raw_buttons() & 3u) == 3u) { cal_start(); if (splash_bad) say_bad(splash_say); }   /* OCT- and OCT+ held: the calibration, as on SLOOP */
    else { splash_on = 1; splash_t0 = time_ticks(); }
    BOOT_STEP(17);
    ui_flush();                                           /* the first screen, whole, before the sound */
    BOOT_STEP(18);
    int a = audio_start(abuf, HALF_FRAMES * 2u);
    if (a != AUDIO_OK) { say_bad(a == AUDIO_ERR_PLL ? "SOUND: UNKNOWN PLL" : "SOUND: ERROR"); msg_forever = 1; ui_flush(); for (;;) ; }
    BOOT_STEP(19);
    usb_by_tick(); irq_start(tick); panel_ticking();       /* (3.3: the panel from the interrupt, the lights steady) */
    sys_wdt_arm();                                        /* (3.3.3: ~4 s without the loop: a restart, which then says where) */
    BOOT_STEP(20);                   /* the USB port from the 2 kHz tick (its audio input's packets) */
}
int app_main(uint32_t arg)
{
    start(arg);
    const uint32_t half_ticks = (uint32_t)((uint64_t)HALF_FRAMES * AUDIO_RATE_DEN * lsb / (AUDIO_RATE_NUM / 1000000u));
    blk_ticks = half_ticks / (HALF_FRAMES / GB_BLOCK);
    t_pot = time_ticks(); t_stat = t_pot; t_autosave = t_pot;
    uint32_t calm = 0, over = 0, t_over = time_ticks() - 6000000u * lsb;
    t_input = t_pot; if (splash_on) splash_t0 = t_input;   /* (the logo from here; a knob turned: t_input moves) */
    uint32_t t_wdt = time_ticks();
    for (;;) {
        if ((uint32_t)(time_ticks() - t_wdt) > 100000u * lsb) { t_wdt = time_ticks(); sys_wdt_feed(); }   /* (every 0.1 s) */
        crumb_at(W_MIDI);
        midi_poll();
        if (audio_half_free()) {
            panel_dark();
            uint32_t t0 = time_ticks();
            t_half = t0;
            int32_t *h = audio_free_half();
            int pl = audio_playing_half();
            for (uint32_t b = 0; b < HALF_FRAMES / GB_BLOCK; b++) {
                if (b) midi_poll();                          /* (the wires read between the blocks too) */
                blk_t = t0 + b * blk_ticks;
                render_block(h + b * GB_BLOCK * 2u);
            }
            if (audio_playing_half() != pl) late++;
            audio_half_done();
            uint32_t pct = (time_ticks() - t0) * 100u / half_ticks;
            ld_sum += pct; ld_halves++; if (pct > ld_peak) ld_peak = pct;
            if (pct > 85u) {                                 /* the load guard (G14) */
                int n = snd_voices(&S), floor = over >= 8u ? 4 : 8;   /* (3.3: long over: down to 4 voices) */
                snd_shed(&S);
                S.voice_cap = n - 1 > floor ? n - 1 : floor;
                calm = 0;
                if (pct > 100u && ++over == 8u && (uint32_t)(t0 - t_over) > 5000000u * lsb) { t_over = t0; say_bad("CPU OVERLOAD: VOICES CUT"); }
            } else if (S.voice_cap && pct < 55u && ++calm >= 64u) {
                calm = 0;
                if (++S.voice_cap >= GB_NVOICE) S.voice_cap = 0;
            }
            if (pct <= 100u) over = 0;
            /* 3.3: the panel and the screen are served between the halves; a sound that takes all the time never
             * leaves them any (the screen stood still, the buttons did nothing: the FM-1 seemed frozen). Past
             * 60 ms without them, they have their turn anyway: a gap in the sound, but the hands keep control */
            if ((uint32_t)(time_ticks() - t_ui) < 60000u * lsb) continue;
        }
        if (chain_on && chain_n >= 2 && Q.playing && !Q.song_play) {   /* (the quick chain: in RAM, a save does not stop it) */
            int rt = scope_one ? trk : 0; Q.lead = (uint8_t)rt;          /* (3.3.7: its pattern's end is the change point) */
            if (Q.t[rt].next == SEQ_NONE) { chain_pos = (uint8_t)((chain_pos + 1u) % chain_n); seq_queue(&Q, scope_one ? trk : -1, chain[chain_pos]); }
        }
        if (saving) {                                       /* a save: one flash operation at a time, never a wait */
            crumb_at(W_SAVE);
            int r = store_step();
            if (r == 3) { if (ed_wr.on && ed_wr.kind == 0) ed_proj_pump(); else proj_feed(); }   /* (the flash idle, the window on: its next part) */
            else if (r == 2 || r < 0) { flash_wait_idle(); saving = 0; save_said = r == 2 ? 1 : r == -2 ? -2 : -1; }
        }
        if (!flash_xip_on()) {                              /* the flash at work: the window is off, only RAM code */
            crumb_at(W_FLASH);
            panel_step();
            if (!saving) flash_busy();
            else led_dwell();
            continue;
        }
        slice_scan_poll();                                  /* (SLICE: a user slot just written, scanned a part at a time) */
        if (save_said) {
            /* (taken and cleared first: what follows may start the next flash job, its own kind set: 3.1.1, the
             * sample's header was written as an autosave and the editor never answered) */
            int said = save_said, snd = saving_snd, eslot = ed_save_slot;
            save_said = 0; saving_snd = 0; ed_save_slot = -1;
            if (snd == 3) ed_job_done(said > 0);
            else if (said > 0 && snd) say_imp(snd == 4 ? "FAVOURITES KEPT" : "USER SOUNDS SAVED");
            else if (eslot >= 0) {                       /* a project from the editor: opened once written */
                if (said > 0) { load_project(0); say_imp("PROJECT RECEIVED"); } else say_bad("PROJECT NOT RECEIVED");
            }
            else if (said > 0) {                         /* (said each time, the autosave's too: the player sees it is kept) */
                proj_used = 1; save_retry = 0;
                if (saving_shown) say_imp("PROJECT SAVED"); else say("PROJECT SAVED (AUTO)");
            }
            else if (said == -2 && save_retry < 3) {     /* (3.3.5) changed while it was written: written again, from now */
                save_retry++; saved_crc = 0;
                if (saving_shown) { save_soon = 1; t_save_soon = time_ticks() - 1000000u * lsb; say("SAVING AGAIN"); }
            }
            else { saved_crc = 0; save_retry = 0; say_bad("SAVE FAILED"); }
        }
        crumb_at(W_SCREEN);
        if (panel_step()) { led_dwell(); panel_dark(); ui_round(); }   /* (3.2: the frame's last column lit as long as the others) */
        else led_dwell();
        t_ui = time_ticks();
    }
}
