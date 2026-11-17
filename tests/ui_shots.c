/* SPDX-License-Identifier: GPL-3.0-only
 * SLOOP 3's screens drawn on the PC (ui.c, gfx.c), as the firmware draws them: a tile at a time. Checks that the
 * tiles put together give exactly the screen drawn whole, and writes each screen (build/shots/NAME.ppm). */
#include "../app/ui.h"
#include "../engine/engine.h"
#include "../app/gfx.h"
#include "../app/vis.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>

#define TW 120
#define TH 16
static uint16_t whole[GFX_W * GFX_H], tiled[GFX_W * GFX_H], tbuf[TW * TH];
static int checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void shoot(const ui_view *v, const char *name)
{
    gfx_target(whole, 0, 0, GFX_W, GFX_H); ui_draw(v);
    for (int ty = 0; ty < GFX_H; ty += TH) for (int tx = 0; tx < GFX_W; tx += TW) {
        gfx_target(tbuf, tx, ty, TW, TH); ui_draw(v);
        for (int y = 0; y < TH; y++) memcpy(&tiled[(ty + y) * GFX_W + tx], &tbuf[y * TW], TW * 2);
    }
    int bad = -1;
    for (int i = 0; i < GFX_W * GFX_H; i++) if (whole[i] != tiled[i]) { bad = i; break; }
    CHECK(bad < 0, "%s: the tiles differ from the whole screen at %d,%d", name, bad % GFX_W, bad / GFX_W);
    char fn[64]; snprintf(fn, sizeof fn, "build/shots/%s.ppm", name);
    FILE *f = fopen(fn, "wb"); fprintf(f, "P6 %d %d 255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        uint16_t c = whole[i];
        unsigned char rgb[3] = { (unsigned char)((c >> 11) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void knob(ui_view *v, int i, const char *l, const char *val, int bar, int hot)
{
    v->knob[i].label = l; snprintf(v->knob[i].value, sizeof v->knob[i].value, "%s", val); v->knob[i].bar = (int8_t)bar;
    v->knob[i].hot = (uint8_t)hot; v->knob[i].dial = 1;
}
static void tile(ui_view *v, int i, const char *text, int style, int flags, uint16_t col)
{
    snprintf(v->tile[i].text, sizeof v->tile[i].text, "%s", text); v->tile[i].style = (uint8_t)style; v->tile[i].flags = (uint8_t)flags; v->tile[i].col = col;
}
static const uint16_t C[5] = { RGB(80, 160, 255), RGB(80, 230, 120), RGB(255, 220, 60), RGB(255, 150, 40), RGB(240, 120, 220) };
static const char *const PADS[16] = { "KICK", "KICK2", "SNARE", "CLAP", "HAT", "OPEN", "PEDAL", "RIM",
                                      "SNR 2", "TOM L", "TOM H", "CRASH", "RIDE", "SHAKE", "CONGA", "BELL" };
/* the visualiser: a made-up mix at 22 kHz (a kick on the beats, a bass, chords, hats, a little stereo) */
static int8_t vring[2 * VIS_RING]; static uint8_t vmem[VIS_MEM];
static double vsig(double t, int ch)
{
    double b = t * 2.0, ph = b - floor(b), k = ph < 0.3 ? sin(2 * M_PI * (50 + 90 * exp(-ph * 30)) * ph) * exp(-ph * 9) : 0;
    double bass = 0.25 * (2 * fmod(t * 55.0, 1.0) - 1) * (0.6 + 0.4 * exp(-ph * 4));
    double ch3 = 0.12 * (sin(2 * M_PI * 220 * t) + sin(2 * M_PI * 277.2 * t + ch) + sin(2 * M_PI * 329.6 * t + 2 * ch));
    double hh = fmod(b * 2, 1.0) < 0.08 ? 0.15 * sin(t * 91234.5 + ch * 7) * sin(t * 71234.1) : 0;
    return 0.5 * k + bass + ch3 * (ch ? 0.8 : 1.1) + hh;
}
static void vis_shots(void)
{
    vis_in in; memset(&in, 0, sizeof in); in.ring = vring;
    for (int t = 0; t < 5; t++) in.col[t] = C[t == 3 ? 4 : t == 4 ? 3 : t];
    in.bpm10 = 1200; in.playing = 1;
    for (int st = 0; st < VIS_N; st++) {
        memset(vmem, 0xAB, sizeof vmem);
        for (int f = 0; f < 60; f++) {
            double t0 = f * 0.04 + st * 0.013;
            for (int i = 0; i < VIS_RING; i++) { double t = t0 + i / 22050.0; for (int c = 0; c < 2; c++) { double v = vsig(t, c) * 127; vring[c * VIS_RING + i] = (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v); } }
            in.w = 0; in.style = (uint8_t)st; in.name = f > 55 ? 10 : 0;
            double beats = t0 * 2.0; in.beats256 = (uint32_t)(beats * 256); in.bar = (uint16_t)(beats / 4); in.beat = (uint8_t)((int)beats & 3);
            for (int t = 0; t < 5; t++) { in.lvl[t] = (float)(0.15 + 0.5 * fabs(sin(t0 * (1 + t) + t))); if ((f + t * 3) % (4 + t) == 0) { in.hit[t]++; in.vel[t] = (uint8_t)(70 + 11 * t); in.note[t] = (uint8_t)(48 + 5 * t); } }
            if (f % 12 == 0) in.kick++;
            vis_frame(vmem, &in);
        }
        ui_view v; memset(&v, 0, sizeof v); v.page = UI_VIS;
        char nm[32]; snprintf(nm, sizeof nm, "vis-%02d", st + 1); shoot(&v, nm);
    }
}
static void splash_shots(void)
{
    static const uint16_t ms[6] = { 200, 500, 800, 1200, 1700, 2500 };
    for (int i = 0; i < 6; i++) {
        { const uint16_t c[5] = { C[0], C[1], C[2], C[4], C[3] }; vis_splash(ms[i], "3.1.6", c); }
        gfx_target(whole, 0, 0, GFX_W, GFX_H); vis_splash_draw();
        for (int ty = 0; ty < GFX_H; ty += TH) for (int tx = 0; tx < GFX_W; tx += TW) {
            gfx_target(tbuf, tx, ty, TW, TH); vis_splash_draw();
            for (int y = 0; y < TH; y++) memcpy(&tiled[(ty + y) * GFX_W + tx], &tbuf[y * TW], TW * 2);
        }
        int bad = -1; for (int k = 0; k < GFX_W * GFX_H; k++) if (whole[k] != tiled[k]) { bad = k; break; }
        CHECK(bad < 0, "splash %d: the tiles differ at %d,%d", ms[i], bad % GFX_W, bad / GFX_W);
        char fn[64]; snprintf(fn, sizeof fn, "build/shots/splash-%04u.ppm", ms[i]);
        FILE *f = fopen(fn, "wb"); fprintf(f, "P6 %d %d 255\n", GFX_W, GFX_H);
        for (int k = 0; k < GFX_W * GFX_H; k++) { uint16_t c = whole[k]; unsigned char rgb[3] = { (unsigned char)((c >> 11) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31) }; fwrite(rgb, 1, 3, f); }
        fclose(f);
    }
}
int main(void)
{
    static const char *const S[5] = { "ROUND BASS", "SOFT PAD", "BRIGHT LEAD", "808", "WIDE PAD" };
    static const char *const K[5] = { "ANALOG", "ANALOG", "ANALOG", "DRUMS", "ANALOG" };
    ui_view v; memset(&v, 0, sizeof v);
    v.ntrk = 5; v.trk = 0; v.bpm = 92; v.playing = 1; v.bar = 2; v.beat = 4; v.blink = 1;
    snprintf(v.status, sizeof v.status, "P1");
    for (int t = 0; t < 5; t++) { v.sound[t] = S[t]; v.kind[t] = K[t]; v.col[t] = C[t]; v.level[t] = (uint8_t)(81 - t * 6); }
    static const char *const P[5] = { "x.xx.x.xx.x.x.x.", "x...............", "..x...x...x.x...", "x...x...x...x...", "x.......x......." };
    for (int t = 0; t < 5; t++) for (int i = 0; i < 16; i++) v.cells[t][i] = (uint8_t)((P[t][i] == 'x' ? UC_NOTE : 0) | (i == 6 ? UC_PLAY : 0));
    v.cells[0][5] |= UC_LOCK; v.mute[4] = 1; v.solo[2] = 0; v.edited[1] = 1;
    knob(&v, 0, "SWING", "0", 0, 0); knob(&v, 1, "LEVEL", "81", 81, 0); knob(&v, 2, "STEPS", "16", 12, 0); knob(&v, 3, "PAN", "0", 50, 0);
    v.batt = 72;                                   /* (3.3.6) the battery's line along the bottom */
    v.page = UI_TRACKS; shoot(&v, "01-tracks");
    { ui_view u = v; u.batt = 22; shoot(&u, "01b-batt-22"); u.batt = 5; shoot(&u, "01c-batt-5"); u.batt_usb = 3; shoot(&u, "01d-batt-usb"); }
    v.recording = 1; v.rec[0] = 1; snprintf(v.status, sizeof v.status, "REC"); v.status_col = RGB(255, 80, 80);
    shoot(&v, "02-tracks-rec"); v.recording = 0; v.rec[0] = 0; snprintf(v.status, sizeof v.status, "P1→3"); v.status_col = RGB(255, 210, 60);

    /* STEPS: SEQ held */
    v.page = UI_LAYER; snprintf(v.title, sizeof v.title, "STEPS"); v.title_col = C[0]; snprintf(v.sub, sizeof v.sub, "TRACK 1 · 1/4");
    static const char *const N[16] = { "A1", "", "A1", "A2", "", "A1", "", "G2", "A1", "", "A2", "", "G1", "", "G2", "" };
    for (int i = 0; i < 16; i++) tile(&v, i, N[i][0] ? N[i] : (char[3]){ (char)('0' + (i + 1) / 10), (char)('0' + (i + 1) % 10), 0 } + ((i + 1) < 10), N[i][0] ? T_ON : T_EMPTY, i == 6 ? TF_PLAY : 0, C[0]);
    v.tile[5].flags |= TF_DOT; v.tile[10].flags |= TF_FILL;
    knob(&v, 0, "NOTE", "A1", 40, 0); knob(&v, 1, "QUANT", "50", 50, 0); knob(&v, 2, "SWING", "0", 0, 0); knob(&v, 3, "STEPS", "16", 12, 0);
    shoot(&v, "03-steps");
    /* a step held */
    snprintf(v.title, sizeof v.title, "STEP 6"); snprintf(v.sub, sizeof v.sub, "LOCK CUT 80 · 1 OF 2");
    v.tile[5].style = T_WHITE;
    knob(&v, 0, "NOTE", "A1", 40, 0); knob(&v, 1, "VEL", "100", 78, 0); knob(&v, 2, "RATCH", "X3", 66, 0); knob(&v, 3, "NUDGE", "-8", 42, 0);
    shoot(&v, "04-pas");
    /* MIX: GLO held */
    snprintf(v.title, sizeof v.title, "MIX"); v.title_col = 0; snprintf(v.sub, sizeof v.sub, "MUTE  SOLO  FILL  TAP");
    for (int i = 0; i < 16; i++) tile(&v, i, "", T_NONE, 0, 0);
    snprintf(v.sub, sizeof v.sub, "1-5 MUTE · 6-10 SOLO");
    for (int t = 0; t < 5; t++) { char b[10]; snprintf(b, 10, "%d", t + 1); tile(&v, t, b, t == 3 ? T_EMPTY : T_ON, 0, C[t]); snprintf(b, 10, "S%d", t + 1); tile(&v, 5 + t, b, t == 1 ? T_ON : T_EMPTY, TF_BAR, t == 1 ? RGB(255, 210, 60) : C[t]); }
    tile(&v, 10, "FILL", T_EMPTY, 0, 0); tile(&v, 11, "BAR", T_EMPTY, 0, 0);
    for (int t = 0; t < 5; t++) { v.tile[t].flags |= TF_LEVEL; v.tile[t].lvl = (uint8_t)(81 - t * 9); }
    v.cols = 5; tile(&v, 12, "", T_NONE, 0, 0); tile(&v, 13, "TAP 16", T_EMPTY, 0, 0); tile(&v, 14, "92", T_DIM, 0, 0); tile(&v, 15, "", T_NONE, 0, 0);
    v.tile[1].flags |= TF_PLAY; v.tile[6].flags |= TF_PLAY; snprintf(v.sub, sizeof v.sub, "1-5 MUTE · 6-10 SOLO · ALGO: TRACK");
    knob(&v, 0, "LVL 2", "75", 75, 0); knob(&v, 1, "PAN 2", "-12", 38, 0); knob(&v, 2, "DLY 2", "20", 20, 0); knob(&v, 3, "REV 2", "35", 35, 0);
    shoot(&v, "05-mix"); v.cols = 0;
    /* SONG: SAVE held */
    snprintf(v.title, sizeof v.title, "SONG"); snprintf(v.sub, sizeof v.sub, "PATTERN ON THE BAR");
    for (int i = 0; i < 12; i++) { char b[6]; snprintf(b, 6, "%d", i + 1); tile(&v, i, b, i == 0 ? T_ON : i < 3 ? T_DIM : T_EMPTY, 0, C[0]); }
    tile(&v, 2, "3", T_FRAME, TF_BLINK, RGB(255, 210, 60));
    tile(&v, 12, "LOOP", T_DIM, 0, 0); tile(&v, 13, "REC", T_EMPTY, 0, RGB(255, 80, 80)); tile(&v, 14, "", T_EMPTY, 0, 0); tile(&v, 15, "ALL", T_DIM, 0, 0);
    for (int i = 0; i < 4; i++) v.knob[i].label = 0;
    shoot(&v, "06-song");
    /* PROJETS: HOME held */
    snprintf(v.title, sizeof v.title, "PROJECTS"); snprintf(v.sub, sizeof v.sub, "CPU 46%% · 12 VOICES");
    for (int i = 0; i < 16; i++) { char b[6]; snprintf(b, 6, "%d", i + 1); tile(&v, i, i < 8 ? b : "", i < 8 ? (i == 1 ? T_FRAME : (0x0B >> i) & 1 ? T_DIM : T_EMPTY) : T_NONE, 0, C[0]); }
    shoot(&v, "07-projets");
    /* EFFACER: EDIT held */
    snprintf(v.title, sizeof v.title, "ERASE"); snprintf(v.sub, sizeof v.sub, "OCT- UNDO 3 · OCT+ REDO 0");
    static const char *const EN[16] = { "F2", "G2", "A2", "B2", "C3", "D3", "E3", "F3", "G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4" };
    for (int i = 0; i < 16; i++) tile(&v, i, EN[i], i == 2 || i == 9 ? T_DIM : T_EMPTY, 0, 0);
    for (int i = 0; i < 4; i++) v.knob[i].label = 0;
    shoot(&v, "08-effacer");

    /* the sound's pages: track 2, a pad */
    v.page = UI_OSC; v.trk = 1; snprintf(v.status, sizeof v.status, "P1"); v.status_col = 0;
    static const uint8_t PAD[16] = { 0, 58, 60, 40, 20, 15, 62, 30, 75, 0, 72, 60, 75, 68, 0, 60 };
    for (int i = 0; i < 16; i++) v.prm[i] = PAD[i];
    snprintf(v.page_name, sizeof v.page_name, "OSC 1/5");
    knob(&v, 0, "WAVE", "0", 0, 0); knob(&v, 1, "OSC2", "+2", 58, 0); knob(&v, 2, "FINE", "+10", 60, 0); knob(&v, 3, "MIX", "40", 40, 0);
    shoot(&v, "10-osc");
    v.page = UI_FILTER; snprintf(v.page_name, sizeof v.page_name, "FILTER 2/5");
    knob(&v, 0, "CUT", "62", 62, 0); knob(&v, 1, "RESO", "30", 30, 0); knob(&v, 2, "TYPE", "LP", 0, 0); knob(&v, 3, "DRIVE", "0", 0, 0);
    shoot(&v, "11-filtre");
    v.page = UI_VOICE; snprintf(v.page_name, sizeof v.page_name, "VOICE 3/5");
    knob(&v, 0, "SUB", "20", 20, 0); knob(&v, 1, "NOISE", "15", 15, 0); knob(&v, 2, "WIDTH", "60", 60, 0); knob(&v, 3, "VOICE", "3", 66, 0);
    shoot(&v, "12-voix");
    v.page = UI_ENV; snprintf(v.page_name, sizeof v.page_name, "ENV 1/2");
    knob(&v, 0, "ATK", "72", 72, 0); knob(&v, 1, "DEC", "60", 60, 0); knob(&v, 2, "SUS", "75", 75, 0); knob(&v, 3, "REL", "68", 68, 0);
    shoot(&v, "13-env");
    v.page = UI_ENVDEST; snprintf(v.page_name, sizeof v.page_name, "ENV DEST 2/2");
    knob(&v, 0, "FLT", "+50", 75, 0); for (int i = 1; i < 4; i++) v.knob[i].label = 0;
    shoot(&v, "14-envdest");
    v.page = UI_FX; snprintf(v.page_name, sizeof v.page_name, "FX"); v.dly_beats8 = 6; v.dly_fb = 45; v.rev_size = 82; v.send_dly = 35; v.send_rev = 50;
    knob(&v, 0, "ECHO", "35", 35, 0); knob(&v, 1, "REV", "50", 50, 0); knob(&v, 2, "TIME", "1/8.", 75, 0); knob(&v, 3, "FDBK", "45", 45, 0);
    shoot(&v, "15-fx");
    v.page = UI_KIT; v.trk = 3; v.engine = 1; v.pad_names = PADS; v.pad_hit = 2; snprintf(v.page_name, sizeof v.page_name, "KIT · 16 SOUNDS");
    knob(&v, 0, "TUNE", "0", 50, 0); knob(&v, 1, "DEC", "55", 55, 0); knob(&v, 2, "TONE", "50", 50, 0); knob(&v, 3, "DRIVE", "35", 35, 0);
    shoot(&v, "16-kit");
    v.page = UI_MIXER; v.trk = 1; snprintf(v.page_name, sizeof v.page_name, "GLOBAL");
    knob(&v, 0, "BPM", "92", 31, 0); knob(&v, 1, "SWING", "20", 20, 0); knob(&v, 2, "VOL 5", "57", 57, 0); knob(&v, 3, "CPU", "46%", 46, 0);
    shoot(&v, "17-global");
    v.page = UI_SOON; snprintf(v.page_name, sizeof v.page_name, "LFO"); snprintf(v.sub, sizeof v.sub, "THE LFO IS COMING SOON");
    for (int i = 0; i < 4; i++) v.knob[i].label = 0;
    shoot(&v, "18-bientot");
    v.page = UI_TRACKS; v.trk = 0; knob(&v, 0, "SWING", "0", 0, 0); knob(&v, 1, "LEVEL", "81", 81, 0); knob(&v, 2, "STEPS", "16", 12, 0); knob(&v, 3, "PAN", "0", 50, 0);
    v.info = "PATTERN 3 ON THE BAR"; shoot(&v, "19-message"); v.info = 0;   /* (3.3: what just changed: the title's line) */
    v.toast = "AGAIN TO ERASE"; shoot(&v, "19b-popup"); v.toast = 0;          /* (what matters: the popup) */
    v.toast = "CPU OVERLOAD: VOICES CUT"; v.toast_kind = 1; shoot(&v, "19c-error"); v.toast = 0; v.toast_kind = 0;
    v.saving = 1; v.save_pct = 45; shoot(&v, "20-sauvegarde"); v.saving = 0;
    v.playing = 0; v.armed = 1; v.blink = 1; shoot(&v, "21-pret");
    /* beta 2: KEY (SEL held), ROLL (ARP held), the LFO, GLOBAL with the click */
    v.page = UI_LAYER; v.trk = 1; v.toast = 0; v.saving = 0; v.playing = 1; v.armed = 0;
    snprintf(v.title, sizeof v.title, "KEY"); v.title_col = RGB(80, 230, 120); snprintf(v.sub, sizeof v.sub, "KEY D MIN");
    static const char *const CHD[16] = { "Gm", "A", "A#", "C", "Dm", "Edim", "F", "Gm", "A", "A#", "C", "Dm", "Edim", "F", "Gm", "A" };
    for (int i = 0; i < 16; i++) tile(&v, i, CHD[i], i == 4 || i == 11 ? T_ON : T_DIM, 0, RGB(80, 230, 120));
    knob(&v, 0, "CHORD", "TRIAD", 20, 0); knob(&v, 1, "SCALE", "MIN", 13, 0); knob(&v, 2, "KEYS", "OFF", 0, 0); knob(&v, 3, "TRANS", "0", 50, 0);
    shoot(&v, "22-key");
    v.info = "KEY E MIN"; shoot(&v, "22b-key-info"); v.info = 0;
    snprintf(v.title, sizeof v.title, "ROLL"); v.title_col = RGB(80, 220, 255); snprintf(v.sub, sizeof v.sub, "KEY HELD · REPEATS");
    for (int i = 0; i < 16; i++) tile(&v, i, PADS[i], i == 4 ? T_WHITE : T_EMPTY, 0, 0);
    knob(&v, 0, "RATE", "1/16", 25, 0); for (int i = 1; i < 4; i++) v.knob[i].label = 0;
    v.trk = 3; shoot(&v, "23-roll"); v.trk = 1;
    v.page = UI_LFO; snprintf(v.page_name, sizeof v.page_name, "LFO 1/2"); v.prm[SP_LRATE] = 72; v.prm[SP_LWAVE] = 0; v.prm[SP_LPIT] = 62; v.prm[SP_LFLT] = 90; v.prm[SP_LSHP] = 50; v.prm[SP_LAMP] = 0;
    knob(&v, 0, "RATE", "3.7", 72, 0); knob(&v, 1, "WAVE", "SIN", 10, 0); v.knob[2].label = 0; v.knob[3].label = 0;
    shoot(&v, "24-lfo");
    v.page = UI_MIXER; snprintf(v.page_name, sizeof v.page_name, "GLOBAL");
    knob(&v, 0, "BPM", "92", 23, 0); knob(&v, 1, "SWING", "20", 20, 0); knob(&v, 2, "CLICK", "REC", 50, 0); knob(&v, 3, "VOL 5", "57", 57, 0);
    shoot(&v, "25-global-click");
    /* beta 3: FX held (the punch-ins), MASTER 2/2, JEU 4/4, a free take */
    v.page = UI_PUNCH; v.title_col = RGB(255, 150, 40); snprintf(v.title, sizeof v.title, "FX"); snprintf(v.sub, sizeof v.sub, "FADE 3 · OCT- REV · OCT+ DLY");
    static const char *const PKW[16] = { "1/4", "1/8", "1/16", "1/32", "STUT", "REV", "STOP", "HALF", "LP", "HP", "PHONE", "CRUSH", "ALIAS", "GATE", "ECHO", "WOBBL" };
    static const char *const PKB[11] = { "-LOW", "-MID", "-HIGH", "RWND", "OCT+", "FLANG", "PAN", "FREEZ", "8T", "MONO", "FADE" };
    for (int i = 0; i < 16; i++) tile(&v, i, PKW[i], i == 2 ? T_WHITE : T_DIM, 0, 0);
    for (int j = 0; j < 11; j++) { snprintf(v.btile[j].text, sizeof v.btile[j].text, "%s", PKB[j]); v.btile[j].style = T_DIM; }
    knob(&v, 0, "FILT", "LP 42", 29, 0); knob(&v, 1, "DUST", "35", 35, 0); knob(&v, 2, "DUCK", "60", 60, 0); knob(&v, 3, "FILT P", "OFF", 50, 0);
    shoot(&v, "26-fx-punch");
    v.btile[7].style = T_WHITE; v.tile[2].style = T_DIM; snprintf(v.sub, sizeof v.sub, "FADE 3 · THROW REV");
    shoot(&v, "27-fx-freeze");
    v.page = UI_MASTER; snprintf(v.page_name, sizeof v.page_name, "MASTER 2/2"); v.mfilt = -42; v.dust = 35; v.duck = 60; v.tfilt = 30;
    knob(&v, 3, "FILT P", "HP 30", 65, 0);
    shoot(&v, "28-master");
    v.page = UI_PLAY; snprintf(v.page_name, sizeof v.page_name, "PLUS 4/5"); v.prm[SP_VOICE] = 67; v.prm[SP_GLIDE] = 40; v.prm[SP_PRIO] = 0;
    knob(&v, 0, "MODE", "LEG", 83, 0); knob(&v, 1, "GLIDE", "162ms", 40, 0); knob(&v, 2, "PRIO", "LAST", 17, 0); v.knob[3].label = 0;
    shoot(&v, "29-jeu");
    v.page = UI_TRACKS; snprintf(v.status, sizeof v.status, "TAKE"); v.status_col = RGB(255, 80, 80); v.playing = 0;
    knob(&v, 0, "SWING", "0", 0, 0); knob(&v, 1, "LEVEL", "81", 81, 0); knob(&v, 2, "STEPS", "16", 12, 0); knob(&v, 3, "PAN", "0", 50, 0);
    v.info = "TAKE 3.4s · 2 BARS 96"; shoot(&v, "30-prise"); v.info = 0;
    /* the looper on TRACKS: a loop going round (its ring 5 / 8 filled, its lane and playhead), a take recording, a
     * take waiting for its bar line, an overdub, a first take with a key just played, a muted loop */
    { ui_view u = v; u.playing = 1; snprintf(u.status, sizeof u.status, "P1"); u.status_col = 0;
      knob(&u, 0, "FILTER", "OFF", 50, 0); knob(&u, 2, "BARS", "4", 50, 0);
      static const uint8_t AT[6] = { 0, 21, 40, 66, 90, 117 };
      for (int t = 0; t < 5; t++) { u.loop[t] = 0; u.pie[t] = 0; u.head[t] = 255; }
      memset(u.cells[0], 0, 16); for (int i = 0; i < 6; i++) u.cells[0][AT[i] >> 3] |= (uint8_t)(1u << (AT[i] & 7));
      u.loop[0] = UL_CIRCLE | UL_LANE; u.pie[0] = 40; u.head[0] = 80;
      memset(u.cells[1], 0, 16); u.loop[1] = UL_CIRCLE | UL_LANE | UL_REC; u.pie[1] = 20;
      memset(u.cells[2], 0, 16); u.loop[2] = UL_CIRCLE | UL_LANE | UL_WAIT; u.pie[2] = 50;
      memcpy(u.cells[3], u.cells[0], 16); u.loop[3] = UL_CIRCLE | UL_LANE | UL_DUB; u.pie[3] = 64; u.head[3] = 10; u.rec[3] = 1;
      memcpy(u.cells[4], u.cells[0], 16); u.loop[4] = UL_CIRCLE | UL_LANE; u.pie[4] = 8;
      shoot(&u, "31-looper");
      u.loop[1] = UL_REC | UL_FIRST | UL_HIT; u.loop[2] = UL_REC | UL_FIRST; u.blink = 0; shoot(&u, "32-looper-first"); }
    /* beta 4: DRUMS (SEQ tapped on the drums), the arpeggiator, the sends */
    v.toast = 0; v.playing = 1; v.trk = 3; v.page = UI_DRUMS; v.pad_names = PADS; v.drum_sel = 4; v.drum_at = 6;
    static const char *const DG[16] = { "x...x...x...x...", "", "....x.......x...", "............x..x", "..x...x...x...x.", "......x.......x.", "", "", "", "", "", "x...............", "", "", "", "" };
    for (int r = 0; r < 16; r++) { v.drum_row[r] = 0; for (int i = 0; DG[r][i]; i++) if (DG[r][i] == 'x') v.drum_row[r] |= (uint16_t)(1u << i); }
    knob(&v, 0, "SOUND", "HAT", 26, 0); knob(&v, 1, "QUANT", "100", 100, 0); knob(&v, 2, "GATE", "100", 50, 0); knob(&v, 3, "STEPS", "16", 12, 0);
    shoot(&v, "31-drums");
    v.trk = 1; v.page = UI_ARP; snprintf(v.page_name, sizeof v.page_name, "ARP 1/2"); v.arp_n = 16;
    static const uint8_t UD[16] = { 0, 44, 100, 44, 0, 44, 100, 44, 0, 44, 100, 44, 0, 44, 100, 44 };
    memcpy(v.arp_seq, UD, 16);
    knob(&v, 0, "MODE", "UPDN", 21, 0); knob(&v, 1, "RATE", "1/16", 50, 0); knob(&v, 2, "OCT", "1", 0, 0); knob(&v, 3, "GATE", "50", 50, 0);
    shoot(&v, "32-arp");
    v.page = UI_FX; snprintf(v.page_name, sizeof v.page_name, "SENDS 1/6"); v.send_dly = 30; v.send_rev = 25; v.dly_beats8 = 6; v.dly_fb = 45; v.rev_size = 82;
    knob(&v, 0, "DST", "10", 10, 0); knob(&v, 1, "CHO", "40", 40, 0); knob(&v, 2, "ECHO", "30", 30, 0); knob(&v, 3, "REV", "25", 25, 0);
    shoot(&v, "33-envois");
    /* beta 5: another engine's page */
    v.page = UI_GENERIC; v.trk = 1; snprintf(v.page_name, sizeof v.page_name, "OPS 1/4"); static const char *const KD[5] = { "ANALOG", "DIGITAL", "ANALOG", "DRUMS", "ANALOG" }; v.kind[1] = KD[1]; v.sound[1] = "PIANO FM";
    knob(&v, 0, "ALG", "4", 43, 0); knob(&v, 1, "R2", "1", 9, 0); knob(&v, 2, "R3", "1", 9, 0); knob(&v, 3, "R4", "12", 97, 0);
    shoot(&v, "34-digital");
    v.info = "KEYS · DIGITAL · 9/74"; shoot(&v, "35-preset"); v.info = 0;
    /* beta 6: the drums' sound page */
    v.page = UI_KIT; v.trk = 3; v.pad_hit = 2; v.pad_mute = 0x0081; snprintf(v.page_name, sizeof v.page_name, "SOUND 2/2 · SNARE");
    knob(&v, 0, "SOUND", "SNARE", 13, 0); knob(&v, 1, "TUNE", "-3", 38, 0); knob(&v, 2, "DEC", "+2", 62, 0); knob(&v, 3, "MUTE", "OFF", 0, 0);
    shoot(&v, "39-son-batterie");
    /* beta 8: MIDI */
    v.page = UI_GENERIC; v.trk = 0; snprintf(v.page_name, sizeof v.page_name, "MIDI 4/5");
    knob(&v, 0, "IN", "TRACKS", 50, 0); knob(&v, 1, "OUT", "SEQ", 100, 0); knob(&v, 2, "SYNC", "EXT", 100, 0); knob(&v, 3, "CLK", "OFF", 0, 0);
    shoot(&v, "40-midi");
    v.info = "MIDI CLOCK · 124 BPM"; shoot(&v, "41-midi-toast"); v.info = 0;
    snprintf(v.page_name, sizeof v.page_name, "MIDI 5/5");
    knob(&v, 0, "DRUM", "GM", 0, 0); knob(&v, 1, "USB", "OK", -1, 0); knob(&v, 2, "GOT", "312", -1, 0); knob(&v, 3, "TEMPO", "124", -1, 0);
    for (int i = 1; i < 4; i++) v.knob[i].dial = 0;
    shoot(&v, "42-midi-2");
    /* beta 8.4: the REC screen */
    v.page = UI_GENERIC; v.trk = 0; v.armed = 1; snprintf(v.page_name, sizeof v.page_name, "REC READY");
    knob(&v, 0, "MODE", "TEMPO", 100, 0); knob(&v, 1, "BARS", "2", 25, 0); knob(&v, 2, "START", "CLICK", 100, 0); knob(&v, 3, "QUANT", "ON", 100, 0);
    for (int i = 0; i < 4; i++) v.knob[i].dial = 1;
    shoot(&v, "43-rec");
    snprintf(v.page_name, sizeof v.page_name, "COUNT-IN 3"); v.info = "COUNT-IN: ONE BAR"; shoot(&v, "44-decompte"); v.info = 0; v.armed = 0;
    /* beta 9: NOTES */
    v.page = UI_NOTES; v.trk = 0; snprintf(v.page_name, sizeof v.page_name, "NOTES 1-16");
    { static const uint8_t X[] = { 0, 8, 16, 24, 28, 32, 40, 48, 56, 16, 16 }, L[] = { 6, 6, 4, 2, 2, 8, 8, 4, 8, 4, 4 }, P[] = { 45, 45, 48, 52, 50, 45, 55, 52, 57, 52, 55 }, F[] = { 0, NR_SLIDE, NR_ACC, NR_GHOST, NR_SOFT, NR_SEL, 0, 0, NR_ACC, 0, 0 };
      v.nr_n = sizeof X; for (int i = 0; i < v.nr_n; i++) { v.nr[i].x = X[i]; v.nr[i].len = L[i]; v.nr[i].pitch = P[i]; v.nr[i].flags = F[i]; } }
    v.nr_lo = 43; v.nr_hi = 58; v.nr_cur = 8; v.nr_play = 37;
    knob(&v, 0, "STEP", "9", 56, 0); knob(&v, 1, "NOTE", "A2", 35, 0); knob(&v, 2, "LEN", "2", 12, 0); knob(&v, 3, "FORCE", "NORM", 75, 0);
    shoot(&v, "45-notes");
    /* NOTES zoomed out (32 steps), the note's sheet open */
    for (int i = 0; i < v.nr_n; i++) { v.nr[i].x = (uint8_t)(v.nr[i].x / 2); v.nr[i].len = (uint8_t)(v.nr[i].len / 2 ? v.nr[i].len / 2 : 1); }
    v.nr_cols = 32; v.nr_cur = 8; v.nr_play = 18; snprintf(v.page_name, sizeof v.page_name, "SHEET 9");
    knob(&v, 0, "LINE", "MOVE", 60, 0); knob(&v, 1, "STEP", "9", 25, 0); knob(&v, 2, "OCT+", "--", -1, 0); knob(&v, 3, "OCT-", "CLOSE", -1, 0);
    v.knob[2].dial = v.knob[3].dial = 0;
    shoot(&v, "45b-notes-feuille"); v.nr_cols = 0;
    /* SONG 2/2: the song by hand */
    v.page = UI_SONG; v.trk = 1; v.title_col = 0x07FF; snprintf(v.title, sizeof v.title, "SONG"); snprintf(v.sub, sizeof v.sub, "2/2 · SONG · ALL");
    { static const uint8_t P[] = { 0, 0, 1, 2, 1 }, F[] = { SR_PLAY, 0, SR_SEL | SR_MIX, SR_EMPTY, SR_MUTE }; static const uint16_t B[] = { 4, 8, 2, 64, 16 };
      v.sg_n = 5; for (int i = 0; i < 5; i++) { v.sg[i].num = (uint8_t)(i + 1); v.sg[i].pat = P[i]; v.sg[i].flags = F[i]; v.sg[i].bars = B[i]; v.sg[i].at = 1; } }
    knob(&v, 0, "LINE", "3/5", 50, 0); knob(&v, 1, "PAT", "P2", 14, 0); knob(&v, 2, "BARS", "2", 3, 0); knob(&v, 3, "LINES", "5", 5, 0);
    shoot(&v, "46-song");
    v.sg_n = 0; knob(&v, 3, "LINES", "0", 0, 0); v.knob[0].label = v.knob[1].label = v.knob[2].label = 0; shoot(&v, "47-song-vide");
    gb_tables_init(); vis_shots(); splash_shots();
    printf("ui: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
