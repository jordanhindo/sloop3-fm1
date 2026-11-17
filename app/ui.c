/* SPDX-License-Identifier: GPL-3.0-only
 * SLOOP 3's screens (ui.h). Written for this project, after SLOOP's layout (its players know where to look) with
 * what SLOOP's users asked for: text that reads at arm's length. Nothing smaller than font_s (capitals 13 px), the
 * main information in font_m and font_l (18 and 25 px), light on near black. */
#include "ui.h"
#include "gfx.h"
#include "vis.h"
#include "../engine/engine.h"   /* (SP_*: which value is which) */

/* the screen's colours: NIGHT (SLOOP 3's), or SLOOP's palettes (five steps of one colour on black), or your theme */
ui_palette ui_pal = { RGB(8, 8, 12), RGB(30, 30, 40), RGB(52, 52, 68), RGB(64, 64, 80), RGB(255, 255, 255), RGB(160, 160, 180) };
const char *const ui_theme_name[UI_NTHEMES] = { "NIGHT", "GREEN", "AMBER", "CYAN", "RED", "MONO" };
static const uint16_t PAL5[UI_NTHEMES - 1][5] = {
    { RGB(0, 40, 12), RGB(0, 84, 30), RGB(16, 140, 54), RGB(56, 200, 92), RGB(120, 255, 146) },
    { RGB(60, 26, 0), RGB(110, 50, 0), RGB(170, 82, 0), RGB(225, 120, 8), RGB(255, 166, 40) },
    { RGB(0, 30, 50), RGB(0, 62, 96), RGB(16, 112, 160), RGB(56, 172, 222), RGB(140, 222, 255) },
    { RGB(52, 8, 8), RGB(100, 18, 14), RGB(170, 36, 26), RGB(226, 64, 48), RGB(255, 112, 92) },
    { RGB(40, 40, 40), RGB(80, 80, 80), RGB(130, 130, 130), RGB(186, 186, 186), RGB(226, 226, 226) } };
void ui_theme_own(const uint16_t c[5], uint16_t bg)
{
    ui_pal.bg = bg; ui_pal.line = c[0]; ui_pal.panel = gfx_mix(bg, c[0], 14); ui_pal.panel2 = c[1];
    ui_pal.dim = c[3]; ui_pal.txt = c[4];
}
void ui_theme(int i)
{
    static const ui_palette NIGHT = { RGB(8, 8, 12), RGB(30, 30, 40), RGB(52, 52, 68), RGB(64, 64, 80), RGB(255, 255, 255), RGB(160, 160, 180) };
    if (i <= 0 || i >= UI_NTHEMES) { ui_pal = NIGHT; return; }
    ui_theme_own(PAL5[i - 1], 0);
}
#define BG     ui_pal.bg
#define PANEL  ui_pal.panel
#define PANEL2 ui_pal.panel2
#define LINE   ui_pal.line
#define TXT    ui_pal.txt
#define DIM    ui_pal.dim
#define GREEN  RGB(80, 240, 120)
#define RED    RGB(255, 80, 80)
#define YEL    RGB(255, 210, 60)
#define CYAN   RGB(80, 220, 255)

/* the knobs' colours, as on SLOOP: KNOB 1 blue, 2 green, 3 yellow, 4 orange (the colours of tracks 1 .. 4) */
const uint16_t ui_knob_col[4] = { RGB(80, 160, 255), RGB(80, 230, 120), RGB(255, 220, 60), RGB(255, 150, 40) };

#define TOP_H 34                  /* the top bar: y 0 .. 33 */
#define HEAD_Y 37                 /* the track header: y 37 .. 74 */
#define MAIN_Y 80                 /* a page: y 80 .. 177 */
#define TILE_Y 48                 /* a layer's tiles: y 48 .. 177 */
#define FOOT_Y 180                /* the dials: y 180 .. 239 */

static uint16_t dim(uint16_t c) { return gfx_mix(BG, c, 11); }
static char *put_u(char *p, uint32_t v) { char t[12]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *p++ = t[--n]; return p; }
/* text in a box: in the biggest font that fits (from 'big' down to font_s), font_s tightened if need be;
 * align 0 left, 1 centre, 2 right */
static void fit(const gfx_font *big, int x, int y, int w, int h, const char *s, uint16_t c, int align)
{
    const gfx_font *f = big;
    int sp = 0;
    if (f == &font_l && gfx_text_w(f, s) > w) f = &font_m;
    if (f == &font_m && gfx_text_w(f, s) > w) f = &font_s;
    if (f == &font_s) while (sp > -2 && (gfx_spacing(sp), gfx_text_w(f, s)) > w) sp--;
    gfx_spacing(sp);
    if (f == &font_s && gfx_text_w(f, s) > w) {           /* (still too wide: the letters condensed, down to 75 %) */
        gfx_spacing(-1);
        for (int c = 92; c >= 62 && (gfx_condense(c), gfx_text_w(f, s)) > w; c -= 4) { }
    }
    char cut[48];
    if (gfx_text_w(f, s) > w) {                           /* (still too wide: cut at its end, never over its neighbours) */
        int n = 0; while (s[n] && n < 47) { cut[n] = s[n]; n++; }
        cut[n] = 0;
        while (n > 1 && gfx_text_w(f, cut) > w) { do n--; while (n > 1 && ((unsigned char)cut[n] & 0xC0) == 0x80); cut[n] = 0; }
        s = cut;
    }
    int tw = gfx_text_w(f, s), tx = align == 0 ? x : align == 1 ? x + (w - tw) / 2 : x + w - tw;
    gfx_text(f, tx, y + (h - f->cap) / 2, s, c);
    gfx_spacing(0); gfx_condense(100);
}
/* a sound's name in a tile (3.2, SOUNDS): on one line when it fits there without its letters squeezed; else on two,
 * cut at the space that leaves the two most alike (no space: one line squeezed; and what even that would cut, in
 * its middle), so that no name loses its end ("TRAP PLUCK", "XYLOPHONE") */
static void fit2(int x, int y, int w, int h, const char *s, uint16_t c)
{
    int n = 0, at = -1, skip = 1, best = 0x7FFF;
    char a[12], b[12];
    while (s[n]) n++;
    gfx_spacing(-2); int w1 = gfx_text_w(&font_s, s); gfx_spacing(0);
    if (w1 <= w || n > 11) { fit(&font_m, x, y, w, h, s, c, 1); return; }
    for (int i = 1; i < n - 1; i++) if (s[i] == ' ') {
        int j = 0; for (; j < i; j++) a[j] = s[j];
        a[j] = 0;
        int wa = gfx_text_w(&font_s, a), wb = gfx_text_w(&font_s, s + i + 1), m = wa > wb ? wa : wb;
        if (m < best) { best = m; at = i; }
    }
    if (at < 0) {
        gfx_spacing(-1); gfx_condense(62); int wc = gfx_text_w(&font_s, s); gfx_spacing(0); gfx_condense(100);
        if (wc <= w) { fit(&font_m, x, y, w, h, s, c, 1); return; }
        at = (n + 1) / 2; skip = 0;
    }
    int j = 0; for (; j < at; j++) a[j] = s[j];
    a[j] = 0;
    for (j = 0; s[at + skip + j]; j++) b[j] = s[at + skip + j];
    b[j] = 0;
    fit(&font_s, x, y + (h - 28) / 2, w, 13, a, c, 1); fit(&font_s, x, y + (h - 28) / 2 + 15, w, 13, b, c, 1);
}

/* ---------------------------------------------------------------- maths for the pictures */
static float ex2(float x)                                /* 2^x, to about 0.1 %: enough to draw */
{
    if (x < -30.0f) return 0.0f;
    if (x > 30.0f) x = 30.0f;
    int i = (int)(x + 64.0f) - 64; float f = x - (float)i;
    union { float f; uint32_t u; } r;
    r.f = 1.0f + f * (0.6951f + f * (0.2262f + f * 0.0781f));
    r.u += (uint32_t)i << 23;
    return r.f;
}
static float lg2(float x)                                /* log2 x, x > 0 */
{
    union { float f; uint32_t u; } r; r.f = x;
    float e = (float)((int)((r.u >> 23) & 255u) - 127);
    r.u = (r.u & 0x007FFFFFu) | 0x3F800000u;
    float f = r.f - 1.0f;
    return e + f * (1.4425f + f * (-0.7148f + f * 0.2726f));
}
static float sqrt_(float x)                              /* a guess from the exponent, three Newton steps */
{
    if (x <= 0.0f) return 0.0f;
    union { float f; uint32_t u; } r; r.f = x; r.u = (r.u >> 1) + 0x1FC00000u;
    float y = r.f;
    y = 0.5f * (y + x / y); y = 0.5f * (y + x / y); y = 0.5f * (y + x / y);
    return y;
}
static float atan2_(float y, float x)                    /* radians, to about 1e-5 */
{
    float ax = x < 0.0f ? -x : x, ay = y < 0.0f ? -y : y;
    float a = (ax > ay ? ay / (ax + 1e-12f) : ax / (ay + 1e-12f)), s = a * a;
    float r = a * (0.9998660f + s * (-0.3302995f + s * (0.1801410f + s * (-0.0851330f + s * 0.0208351f))));
    if (ay > ax) r = 1.5707963f - r;
    if (x < 0.0f) r = 3.1415927f - r;
    return y < 0.0f ? -r : r;
}
static void sincos_(float a, float *sn, float *cs)       /* a in radians, to about 1e-4 */
{
    float q = a / 1.5707963f; int k = (int)(q + (q >= 0.0f ? 0.5f : -0.5f));
    float z = a - (float)k * 1.5707963f, z2 = z * z;
    float s0 = z * (1.0f - z2 / 6.0f * (1.0f - z2 / 20.0f)), c0 = 1.0f - z2 / 2.0f * (1.0f - z2 / 12.0f);
    int m = ((k % 4) + 4) % 4;
    *cs = m == 0 ? c0 : m == 1 ? -s0 : m == 2 ? -c0 : s0;
    *sn = m == 0 ? s0 : m == 1 ? c0 : m == 2 ? -s0 : -c0;
}

/* ---------------------------------------------------------------- the top bar */
static void info_line(const ui_view *v, int x, int y, int w, int h);
static void top_bar(const ui_view *v)
{
    if (!gfx_visible(0, 0, GFX_W, TOP_H)) return;
    if (v->info) {                                        /* (the tempo stays; the transport and the status give way) */
        char t[8], *q = put_u(t, v->bpm); *q = 0;
        int e = gfx_text(&font_l, 4, 5, t, TXT);
        gfx_text(&font_s, e + 4, 5 + font_l.cap - font_s.cap, "BPM", DIM);
        info_line(v, 100, 2, 136, TOP_H - 4);
        return;
    }
    char b[12], *p;
    /* the tempo */
    p = put_u(b, v->bpm); *p = 0;
    int e = gfx_text(&font_l, 4, 5, b, TXT);
    gfx_text(&font_s, e + 4, 5 + font_l.cap - font_s.cap, "BPM", DIM);
    /* the transport: an icon, the bar and beat, the four beats */
    int x = 104;
    if (v->recording) gfx_disc(x + 7, 13, 7, RED);
    else if (v->playing) gfx_play(x, 6, 15, GREEN);
    else if (v->armed) { if (v->blink) gfx_disc(x + 7, 13, 7, RED); else gfx_rframe(x, 6, 14, 14, 7, 2, RED); }
    else gfx_fill(x + 1, 7, 12, 12, DIM);
    if (v->playing) { p = put_u(b, v->bar); *p++ = '.'; p = put_u(p, v->beat); *p = 0; }
    else { p = b; const char *s = v->armed ? "READY" : "STOP"; while (*s) *p++ = *s++; *p = 0; }
    fit(&font_m, x + 20, 4, 62, font_m.cap, b, v->playing || v->armed ? TXT : DIM, 0);
    for (int i = 0; i < 4; i++) gfx_fill(x + 20 + i * 10, 27, 7, 4, v->playing && v->beat == i + 1 ? TXT : LINE);
    /* the status */
    fit(&font_m, 170, 4, 66, font_m.cap, v->status, v->status_col ? v->status_col : TXT, 2);
}
/* 3.3: what just changed, in the title's line: a quiet line in the accent colour, the page under it untouched */
static void info_line(const ui_view *v, int x, int y, int w, int h)
{
    gfx_fill(x, y, w, h, BG);
    gfx_fill(x, y + h - 2, w, 2, YEL);
    const gfx_font *f = gfx_text_w(&font_m, v->info) <= w - 4 ? &font_m : &font_s;   /* (the larger letters when they fit) */
    fit(f, x + 2, y + (h - 2 - f->cap) / 2, w - 4, f->cap, v->info, TXT, 0);
}
/* ---------------------------------------------------------------- the track header (on the pages) */
static void sound_name(char *nm, const ui_view *v, int t)
{
    char *p = nm; const char *s = v->sound[t] && v->sound[t][0] ? v->sound[t] : "OWN SOUND";
    while (*s && p < nm + 16) *p++ = *s++;
    if (v->edited[t]) *p++ = '*';
    *p = 0;
}
static void track_header(const ui_view *v)
{
    if (!gfx_visible(0, HEAD_Y - 6, GFX_W, 46)) return;
    uint16_t c = v->col[v->trk];
    gfx_rfill(4, HEAD_Y, 38, 38, 7, c);
    char n[2] = { (char)('1' + v->trk), 0 };
    gfx_text_c(&font_l, 23, HEAD_Y + (38 - font_l.cap) / 2, n, BG);
    char nm[20]; sound_name(nm, v, v->trk);
    fit(&font_l, 50, HEAD_Y + 1, 186, 36, nm, TXT, 0);
}
/* ---------------------------------------------------------------- the dials */
static void dial(int cx, int cy, int r, int t, int val, uint16_t c, uint16_t needle)
{
    if (!gfx_visible(cx - r - 1, cy - r - 1, 2 * r + 3, 2 * r + 3)) return;
    /* a ring of 270 degrees open at the bottom, lit up to the value: each pixel's distance (the ring, its edges
     * anti-aliased) and its angle (lit or not) */
    float u = (float)val * 0.01f, ro = (float)r, ri = (float)(r - t);
    for (int y = cy - r - 1; y <= cy + r + 1; y++) for (int x = cx - r - 1; x <= cx + r + 1; x++) {
        if (!gfx_visible(x, y, 1, 1)) continue;
        float dx = (float)x + 0.5f - (float)cx, dy = (float)cy - ((float)y + 0.5f), d = sqrt_(dx * dx + dy * dy);
        float cov = d < ri ? d - ri + 1.0f : d > ro ? ro + 1.0f - d : 1.0f;
        if (cov <= 0.0f) continue;
        if (cov > 1.0f) cov = 1.0f;
        float a = atan2_(dy, dx) * 57.29578f;                 /* degrees, from the right, counter-clockwise */
        float pos = (225.0f - (a < -90.0f ? a + 360.0f : a)) / 270.0f;   /* 0 at its start (down left) .. 1 */
        if (pos < 0.0f || pos > 1.0f) continue;               /* the gap at the bottom */
        uint16_t px = pos <= u + 0.004f ? c : gfx_mix(BG, c, 9);
        gfx_fill(x, y, 1, 1, gfx_mix(BG, px, (unsigned)(cov * 32.0f + 0.5f)));
    }
    /* the needle, from the centre */
    float sn, cs; sincos_((225.0f - 270.0f * u) * 0.017453293f, &sn, &cs);
    for (float s = 0.0f; s < ro - 1.5f; s += 0.5f) {
        float fx = (float)cx + cs * s, fy = (float)cy - sn * s;
        gfx_fill((int)(fx - 0.5f), (int)(fy - 0.5f), 2, 2, needle);
    }
}
static void footer(const ui_view *v)
{
    if (!gfx_visible(0, FOOT_Y, GFX_W, GFX_H - FOOT_Y)) return;
    for (int i = 0; i < 4; i++) {
        const ui_knob *k = &v->knob[i];
        int x = i * 60, top = FOOT_Y;
        if (!k->label || !k->label[0]) continue;
        uint16_t kc = ui_knob_col[i];
        if (k->dial && k->bar >= 0) dial(x + 30, top + 12, 12, 4, k->bar, kc, k->hot ? YEL : TXT);
        else if (k->dial) gfx_rframe(x + 18, top, 24, 24, 12, 2, gfx_mix(BG, kc, 12));
        else gfx_rfill(x + 8, top + 9, 44, 5, 2, kc);
        fit(&font_s, x + 2, top + 26, 56, font_s.cap, k->label, k->hot ? YEL : DIM, 1);
        fit(&font_m, x + 2, top + 40, 56, font_m.cap, k->value, k->hot ? YEL : TXT, 1);   /* (3.3.6: 2 px up, the battery's line under) */
    }
}

/* ---------------------------------------------------------------- the tiles of a layer */
static void layer_title(const ui_view *v)
{
    if (!gfx_visible(0, 0, GFX_W, 46)) return;
    int e = gfx_text(&font_l, 4, 8, v->title, v->title_col ? v->title_col : TXT), x = e + 8, w = 236 - x;
    if (v->info) { info_line(v, x, 4, w, 34); gfx_fill(4, 41, 232, 1, LINE); return; }
    if (!v->sub[0]) { gfx_fill(4, 41, 232, 1, LINE); return; }
    /* the line beside the title: on one line if it fits, else two, cut at its " · " */
    if (gfx_text_w(&font_s, v->sub) <= w) gfx_text(&font_s, x, 8 + font_l.cap - font_s.cap, v->sub, DIM);
    else {
        char a[40], b[40]; int i = 0, j = 0, cut = -1;
        for (int k = 0; v->sub[k]; k++) if (v->sub[k] == ' ' && (unsigned char)v->sub[k + 1] == 0xC2) { cut = k; break; }
        if (cut < 0) {                                        /* (squeezed if it fits so; else left out) */
            int n = 0; while (v->sub[n]) n++;
            if (gfx_text_w(&font_s, v->sub) - 2 * n <= w) fit(&font_s, x, 8 + font_l.cap - font_s.cap, w, font_s.cap, v->sub, DIM, 0);
            gfx_fill(4, 41, 232, 1, LINE); return;
        }
        for (; i < cut && i < 39; i++) a[i] = v->sub[i];
        a[i] = 0;
        const char *r = v->sub + cut + 4;                     /* past " · " (the dot is two bytes) */
        while (*r == ' ') r++;
        for (; r[j] && j < 39; j++) b[j] = r[j];
        b[j] = 0;
        fit(&font_s, x, 6, w, font_s.cap, a, DIM, 0);
        fit(&font_s, x, 23, w, font_s.cap, b, DIM, 0);
    }
    gfx_fill(4, 41, 232, 1, LINE);
}
static void tiles(const ui_view *v)
{
    int nc = v->cols == 5 ? 5 : 4, pitch = nc == 5 ? 47 : 59, tw = nc == 5 ? 44 : 56;
    for (int i = 0; i < 16; i++) {
        const ui_tile *t = &v->tile[i];
        int x = 3 + (i % nc) * pitch, y = TILE_Y + (i / nc) * 33, w = tw, h = 30;
        if (!gfx_visible(x - 4, y - 4, w + 8, h + 8) || t->style == T_NONE) continue;
        uint16_t c = t->col ? t->col : TXT, fill = PANEL, tc = DIM;
        switch (t->style) {
        case T_ON: fill = c; tc = BG; break;
        case T_DIM: fill = PANEL2; tc = TXT; break;
        case T_FRAME: fill = PANEL; tc = c; break;
        case T_WHITE: fill = TXT; tc = BG; break;
        case T_RED: fill = RED; tc = TXT; break;
        default: break;
        }
        gfx_rfill(x, y, w, h, 4, fill);
        if (t->flags & TF_BAR) gfx_fill(x + 4, y, w - 8, 3, c);
        if (t->style == T_FRAME) gfx_rframe(x, y, w, h, 4, 2, (t->flags & TF_BLINK) && !v->blink ? dim(c) : c);
        if (t->flags & TF_PLAY) gfx_rframe(x, y, w, h, 4, 2, TXT);
        if (t->flags & TF_LEVEL) {                            /* the track's level, as GLOBAL's faders lying down */
            uint16_t lc = t->style == T_ON ? BG : c;
            gfx_fill(x + 4, y + h - 6, w - 8, 3, t->style == T_ON ? dim(c) : PANEL2);
            gfx_fill(x + 4, y + h - 6, (w - 8) * (t->lvl > 100 ? 100 : t->lvl) / 100, 3, lc);
        }
        if (t->flags & TF_WRAP) fit2(x + 3, y, w - 6, h, t->text, tc);
        else fit(&font_m, x + 3, y, w - 6, t->flags & TF_LEVEL ? h - 7 : h, t->text, tc, 1);
        uint16_t mk = t->style == T_ON || t->style == T_WHITE ? BG : YEL;
        if (t->flags & TF_DOT) gfx_fill(x + w - 8, y + 4, 4, 4, mk);
        if (t->flags & TF_FILL) gfx_fill(x + 4, y + 4, 4, 4, t->style == T_ON ? BG : CYAN);
        if (t->flags & TF_NOFILL) { gfx_fill(x + 4, y + 4, 4, 4, t->style == T_ON ? BG : CYAN); gfx_fill(x + 9, y + 4, 4, 4, t->style == T_ON ? BG : CYAN); }
    }
}

/* FX held: the 27 punch-ins as the keys are, the white keys in rows of four, each black key above, between the two
 * white keys it sits between (the one after a row's last white key at that row's right end) */
static void punch_tiles(const ui_view *v)
{
    static const uint8_t AFTER[11] = { 0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14 };   /* the white key before each black one */
    for (int i = 0; i < 16; i++) {
        const ui_tile *t = &v->tile[i];
        int x = 3 + (i % 4) * 59, y = TILE_Y + (i / 4) * 33 + 15, w = 56, h = 17;
        if (!gfx_visible(x - 4, y - 4, w + 8, h + 8)) continue;
        int on = t->style == T_WHITE;
        gfx_rfill(x, y, w, h, 3, on ? TXT : PANEL2);
        fit(&font_s, x + 2, y, w - 4, h, t->text, on ? BG : TXT, 1);
    }
    for (int j = 0; j < 11; j++) {
        const ui_tile *t = &v->btile[j];
        int a = AFTER[j], r = a / 4, sl = a % 4, w = 46, h = 14;
        int x = sl < 3 ? 3 + sl * 59 + 57 - w / 2 : 236 - w, y = TILE_Y + r * 33;
        if (!gfx_visible(x - 4, y - 4, w + 8, h + 8)) continue;
        int on = t->style == T_WHITE;
        gfx_rfill(x, y, w, h, 3, on ? TXT : PANEL);
        gfx_rframe(x, y, w, h, 3, 1, on ? TXT : LINE);
        fit(&font_s, x + 2, y, w - 4, h, t->text, on ? BG : DIM, 1);
    }
}

/* ---------------------------------------------------------------- TRACKS */
static void step_cell(int x, int y, int w, int h, uint8_t f, uint16_t c)
{
    if (f & UC_OUT) return;
    uint16_t fill = f & UC_NOTE ? c : PANEL2;
    if ((f & UC_PLAY) && !(f & UC_NOTE)) fill = LINE;
    gfx_fill(x, y, w, h, fill);
    if (f & UC_PLAY) gfx_fill(x, y + h + 1, w, 2, TXT);
    if (f & (UC_LOCK | UC_COND | UC_RAT)) gfx_fill(x, y - 3, w, 2, YEL);
}
/* the circle's fill: the part of the disc from the top, clockwise, to f / 64 of a turn */
static int sin64(int i)
{
    static const uint8_t T[17] = { 0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127 };
    i &= 63;
    return i <= 16 ? T[i] : i <= 32 ? T[32 - i] : i <= 48 ? -T[i - 32] : -T[64 - i];
}
static void pie(int cx, int cy, int r, int f, uint16_t c)
{
    if (f <= 0 || !gfx_visible(cx - r, cy - r, 2 * r + 1, 2 * r + 1)) return;
    int ux = sin64(f), uy = sin64(f + 16);                 /* where the fill ends (y up) */
    for (int dy = -r; dy <= r; dy++) {
        int x0 = 0, n = 0;
        for (int dx = -r; dx <= r + 1; dx++) {
            int in = dx <= r && dx * dx + dy * dy <= r * r;
            if (in && f < 64) { int cr = ux * -dy - uy * dx; in = f <= 32 ? dx >= 0 && cr > 0 : dx >= 0 || cr > 0; }
            if (in) { if (!n) x0 = dx; n++; }
            else if (n) { gfx_fill(cx + x0, cy + dy, n, 1, c); n = 0; }
        }
    }
}
static void page_tracks(const ui_view *v)
{
    for (int t = 0; t < v->ntrk && t < 5; t++) {
        int y = HEAD_Y + t * 28;
        if (!gfx_visible(0, y - 6, GFX_W, 34)) continue;
        uint16_t c = v->col[t], cc = v->mute[t] ? dim(c) : c;
        uint8_t lp = v->loop[t];
        /* the row: red while a take records on it (blinking while it waits for its bar line), yellow while overdubbed */
        uint16_t row = t == v->trk ? PANEL : BG;
        if ((lp & UL_REC) || ((lp & UL_WAIT) && v->blink)) row = gfx_mix(BG, RED, 11);
        else if (lp & UL_DUB) row = gfx_mix(BG, YEL, 9);
        if (row != BG) gfx_rfill(2, y, 236, 27, 5, row);
        char n[2] = { (char)('1' + t), 0 };
        int ny = y + 3 + (21 - font_s.cap) / 2;
        if (lp & UL_FIRST) {                                 /* a first take: no tempo yet, a red disc, lighter at each key */
            gfx_disc(15, y + 13, 10, lp & UL_HIT ? gfx_mix(RED, TXT, 16) : RED); gfx_text_c(&font_s, 15, ny, n, BG);
        } else if (lp & UL_CIRCLE) {                         /* a track that holds notes: its number in a ring, filled as its loop goes round */
            gfx_disc(15, y + 13, 10, cc); gfx_disc(15, y + 13, 8, row);
            pie(15, y + 13, 8, v->pie[t], gfx_mix(row, cc, 16));
            gfx_text_c(&font_s, 15, ny, n, TXT);
        }
        else if (v->mute[t]) { gfx_rframe(5, y + 3, 21, 21, 4, 2, cc); gfx_text_c(&font_s, 15, ny, n, cc); }
        else { gfx_rfill(5, y + 3, 21, 21, 4, c); gfx_text_c(&font_s, 15, ny, n, BG); }
        /* the sound and its kind, a badge */
        char nm[20]; sound_name(nm, v, t);
        const char *badge = v->rec[t] ? "REC" : v->solo[t] ? "SOLO" : v->mute[t] ? "MUTE" : 0;
        uint16_t bc = v->solo[t] && !v->rec[t] ? YEL : RED;
        int bw = badge ? gfx_text_w(&font_s, badge) + 8 : 0;
        int e = gfx_text(&font_s, 31, y + 3, nm, t == v->trk ? TXT : DIM);
        if (v->kind[t] && e + 6 + gfx_text_w(&font_s, v->kind[t]) < 234 - bw) gfx_text(&font_s, e + 6, y + 3, v->kind[t], cc);
        if (badge) gfx_text_r(&font_s, 234, y + 3, badge, bc);
        /* the steps, the level */
        if (lp & UL_LANE) {                                  /* played notes: each where it is in the whole loop, and the playhead */
            gfx_fill(31, y + 19, 180, 5, PANEL2);
            for (int i = 0; i < 128; i++) if ((v->cells[t][i >> 3] >> (i & 7)) & 1u) gfx_fill(31 + i * 180 / 128, y + 18, 2, 7, cc);
            if (v->head[t] < 128) gfx_fill(31 + v->head[t] * 180 / 128, y + 16, 1, 11, TXT);
        }
        else for (int i = 0; i < 16; i++) step_cell(31 + i * 11 + (i / 4) * 2, y + 19, 9, 5, v->cells[t][i], cc);
        gfx_fill(214, y + 19, 20, 5, LINE);
        gfx_fill(214, y + 19, 20 * v->level[t] / 100, 5, cc);
    }
}

/* ---------------------------------------------------------------- the sound's pages */
#define VIS_X 8
#define VIS_W 224
#define VIS_Y (MAIN_Y + 22)
#define VIS_H 72
static void curve(const float *y, uint16_t c, int area)
{
    int prev = -1;
    for (int i = 0; i < VIS_W; i++) {
        float f = y[i] < 0.0f ? 0.0f : y[i] > 1.0f ? 1.0f : y[i];
        int yy = VIS_Y + VIS_H - 1 - (int)(f * (float)(VIS_H - 2) + 0.5f);
        if (area) gfx_fill(VIS_X + i, yy, 1, VIS_Y + VIS_H - yy, dim(c));
        int a = prev < 0 ? yy : prev, lo = a < yy ? a : yy, hi = a < yy ? yy : a;
        gfx_fill(VIS_X + i, lo - 1, 1, hi - lo + 2, c);
        prev = yy;
    }
}
static void page_title(const ui_view *v)
{
    if (!gfx_visible(0, MAIN_Y - 4, GFX_W, 22)) return;
    /* the page's name, and the track's engine at the right if both fit (else the name alone, squeezed if need be) */
    const char *k = v->kind[v->trk];
    int kw = k ? gfx_text_w(&font_s, k) : 0, nw = gfx_text_w(&font_s, v->page_name);
    if (k && nw + 10 + kw <= 228) { gfx_text(&font_s, 6, MAIN_Y + 2, v->page_name, DIM); gfx_text_r(&font_s, 234, MAIN_Y + 2, k, v->col[v->trk]); }
    else fit(&font_s, 6, MAIN_Y + 2, 228, font_s.cap, v->page_name, DIM, 0);
}
static int vis_on(void) { return gfx_visible(0, VIS_Y - 2, GFX_W, VIS_H + 4); }
static void env_shape(const ui_view *v, float *y, float scale, float base)
{
    float S = v->prm[SP_SUS] * 0.01f;
    int a = 4 + v->prm[SP_ATK] / 2, d = 4 + v->prm[SP_DEC] / 2, h = 40, r = 4 + v->prm[SP_REL] * 6 / 10;
    for (int i = 0; i < VIS_W; i++) {
        float l = 0.0f;
        if (i < a) l = (float)i / (float)a;
        else if (i < a + d) { float u = 1.0f - (float)(i - a) / (float)d; l = S + (1.0f - S) * u * u * u; }
        else if (i < a + d + h) l = S;
        else if (i < a + d + h + r) { float u = 1.0f - (float)(i - a - d - h) / (float)r; l = S * u * u * u; }
        y[i] = base + scale * l;
    }
}
static void page_osc(const ui_view *v)
{
    /* ANALOG's oscillators: two cycles of its WAVE (SAW SQR TRI SIN PWM), osc 2 DTN above as MIX says, the noise */
    page_title(v);
    if (!vis_on()) return;
    int w = (v->prm[SP_E0] * 4 + 50) / 100;
    float ratio = ex2((float)v->prm[SP_E1] * 1.27f * (1.0f / 1200.0f)), m2 = v->prm[SP_E2] * 0.01f, m1 = 1.0f - m2, mx = 0.001f;
    float pw = 0.5f, nz = v->prm[SP_E3] * 0.004f;
    uint32_t r = 0x2545F491u;
    float y[VIS_W];
    for (int i = 0; i < VIS_W; i++) {
        float t = (float)i * (2.0f / VIS_W), val = 0.0f;
        for (int k = 0; k < 2; k++) {
            float ph = k ? t * ratio + 0.25f : t; ph -= (float)(int)ph;
            float o = w == 1 ? (ph < 0.5f ? 1.0f : -1.0f) : w == 2 ? (ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph)
                    : w == 3 ? 0.0f : w == 4 ? (ph < pw * 0.6f ? 1.0f : -1.0f) : 2.0f * ph - 1.0f;
            if (w == 3) { float sn, cs; sincos_(ph * 6.2831853f, &sn, &cs); o = sn; }
            val += (k ? m2 : (v->prm[SP_E2] ? m1 : 1.0f)) * o;
            if (!v->prm[SP_E2]) break;
        }
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        val += nz * (float)(int32_t)r * (1.0f / 2147483648.0f);
        y[i] = val; if (val > mx) mx = val; if (-val > mx) mx = -val;
    }
    for (int i = 0; i < VIS_W; i++) y[i] = 0.5f + 0.48f * y[i] / mx;
    gfx_fill(VIS_X, VIS_Y + VIS_H / 2, VIS_W, 1, LINE);
    curve(y, v->col[v->trk], 0);
}
static void page_env(const ui_view *v)
{
    page_title(v);
    if (!vis_on()) return;
    float y[VIS_W]; env_shape(v, y, 1.0f, 0.0f);
    curve(y, v->col[v->trk], 1);
}
static void page_envdest(const ui_view *v)
{
    /* the filter's envelope: how far it opens the cutoff (up) or closes it (down), over the note */
    page_title(v);
    if (!vis_on()) return;
    float y[VIS_W]; env_shape(v, y, 0.48f * (v->prm[SP_EFLT] - 50) * 0.02f, 0.5f);
    gfx_fill(VIS_X, VIS_Y + VIS_H / 2, VIS_W, 1, LINE);
    curve(y, v->col[v->trk], 0);
}
static void page_filter(const ui_view *v)
{
    /* the filter's response, 20 Hz .. 20 kHz across, +12 .. -36 dB: low / band / high mixed as the engine does */
    page_title(v);
    if (!vis_on()) return;
    float y[VIS_W];
    int ft = (v->prm[SP_FTYPE] * 2 + 50) / 100;                      /* ANALOG: LP BP HP, CUT, RES */
    float k = 2.0f - 1.855f * v->prm[SP_E5] * 0.01f;
    float lpw = ft == 0 ? 1.0f : 0.0f, hpw = ft == 2 ? 1.0f : 0.0f, bpw = ft == 1 ? k : 0.0f;
    float co = 0.585f + v->prm[SP_E4] * 0.0906f;                     /* (octaves above 20 Hz: 30 Hz .. 16 kHz) */
    for (int i = 0; i < VIS_W; i++) {
        float w2 = ex2(2.0f * ((float)i * (10.0f / VIS_W) - co)), a = 1.0f - w2, den = a * a + k * k * w2;
        float lp = 1.0f / den, bp = w2 / den, hp = w2 * w2 / den;          /* squared magnitudes */
        float m = lpw * sqrt_(lp) + bpw * sqrt_(bp) + hpw * sqrt_(hp);
        y[i] = (6.0206f * lg2(m > 1e-6f ? m : 1e-6f) + 36.0f) * (1.0f / 48.0f);
    }
    gfx_fill(VIS_X, VIS_Y + VIS_H - 1 - (int)(36.0f / 48.0f * (VIS_H - 2)), VIS_W, 1, LINE);   /* 0 dB */
    curve(y, v->col[v->trk], 1);
}
static void page_voice(const ui_view *v)
{
    /* the sources beside the oscillators: the sub (an octave down), the noise, the unison's spread */
    page_title(v);
    if (!vis_on()) return;
    uint16_t c = v->col[v->trk];
    static const char *const L[3] = { "SUB", "NOISE", "WIDTH" };
    int val[3] = { v->prm[4], v->prm[5], v->prm[15] };
    for (int i = 0; i < 3; i++) {
        int y = VIS_Y + 6 + i * 23;
        gfx_text(&font_s, 10, y + 2, L[i], DIM);
        gfx_rfill(100, y, 132, 17, 4, PANEL);
        if (val[i] > 0) gfx_rfill(100, y, 132 * val[i] / 100 < 8 ? 8 : 132 * val[i] / 100, 17, 4, c);
    }
}
static void page_fx(const ui_view *v)
{
    page_title(v);
    if (!vis_on()) return;
    /* the reverb's tail behind, the echoes in front: 4 beats across */
    int len = 30 + v->rev_size * 190 / 100, sr = v->send_rev;
    for (int i = 0; i < len && i < VIS_W; i++) {
        float u = 1.0f - (float)i / (float)len; int hh = (int)((float)(VIS_H - 6) * u * u * (0.25f + 0.75f * sr * 0.01f));
        gfx_fill(VIS_X + i, VIS_Y + VIS_H - hh, 1, hh, PANEL2);
    }
    float lvl, fb = v->dly_fb * 0.0095f, send = v->send_dly * 0.01f;
    int step = v->dly_beats8 * 56 / 8;
    gfx_fill(VIS_X, VIS_Y, 5, VIS_H, TXT);                          /* the note */
    lvl = send > 0.0f ? 0.25f + 0.6f * send : 0.0f;                  /* (drawn bigger than heard: to be seen) */
    for (int x = step; step > 0 && x < VIS_W - 4 && lvl > 0.04f; x += step) {
        int hh = (int)((float)VIS_H * lvl);
        gfx_fill(VIS_X + x, VIS_Y + VIS_H - hh, 5, hh, v->col[v->trk]);
        lvl *= fb;
    }
}
static void page_lfo(const ui_view *v)
{
    /* the LFO's wave, two cycles, drawn as large as its depths add up (flat at 0: it does nothing) */
    page_title(v);
    if (!vis_on()) return;
    int db = 0; for (int k = SP_LPIT; k <= SP_LSHP; k++) db += v->prm[k] > 50 ? v->prm[k] - 50 : 50 - v->prm[k];
    float y[VIS_W], dep = (float)(2 * db + v->prm[SP_LAMP]) * 0.01f;
    int wv = (v->prm[SP_LWAVE] * 4 + 50) / 100;
    uint32_t r = 0x2545F491u; float sh = 0.0f; int cyc = -1;
    if (dep > 1.0f) dep = 1.0f;
    for (int i = 0; i < VIS_W; i++) {
        float t = (float)i * (2.0f / VIS_W), ph = t - (float)(int)t, l;
        switch (wv) {
        case 0: { float sn, cs; sincos_(ph * 6.2831853f, &sn, &cs); l = sn; break; }
        case 1: l = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
        case 2: l = 1.0f - 2.0f * ph; break;
        case 3: l = ph < 0.5f ? 1.0f : -1.0f; break;
        default:
            if ((int)(t * 4.0f) != cyc) { cyc = (int)(t * 4.0f); r ^= r << 13; r ^= r >> 17; r ^= r << 5; sh = (float)(int32_t)r * (1.0f / 2147483648.0f); }
            l = sh; break;
        }
        y[i] = 0.5f + 0.46f * l * (0.15f + 0.85f * dep);
    }
    gfx_fill(VIS_X, VIS_Y + VIS_H / 2, VIS_W, 1, LINE);
    curve(y, dep > 0.0f ? v->col[v->trk] : LINE, 0);
}
static void page_kit(const ui_view *v)
{
    /* the drum track's 16 pads, as the keys: the one last played in white */
    page_title(v);
    for (int i = 0; i < 16; i++) {
        int x = 3 + (i % 4) * 59, y = MAIN_Y + 20 + (i / 4) * 20, w = 56, h = 18;
        if (!gfx_visible(x - 4, y - 4, w + 8, h + 8)) continue;   /* (a glyph may lean out of its box) */
        int on = i == v->pad_hit, mu = (v->pad_mute >> i) & 1;
        gfx_rfill(x, y, w, h, 3, on ? TXT : PANEL);
        if (mu) gfx_rframe(x, y, w, h, 3, 1, RED);
        fit(&font_s, x + 2, y, w - 4, h, v->pad_names ? v->pad_names[i] : "", mu && !on ? RED : on ? BG : DIM, 1);
    }
}
static void page_mixer(const ui_view *v)
{
    char b[8], *p;
    page_title(v);
    for (int t = 0; t < v->ntrk && t < 5; t++) {
        int x = 4 + t * 47;
        if (!gfx_visible(x - 2, MAIN_Y + 16, 48, 82)) continue;
        uint16_t c = v->mute[t] ? dim(v->col[t]) : v->col[t];
        if (t == v->trk) gfx_rframe(x - 2, MAIN_Y + 18, 48, 80, 6, 2, TXT);
        char n[2] = { (char)('1' + t), 0 };
        gfx_rfill(x, MAIN_Y + 20, 44, 18, 4, c);
        gfx_text_c(&font_s, x + 22, MAIN_Y + 20 + (18 - font_s.cap) / 2, n, BG);
        int bh = 36, by = MAIN_Y + 41, lh = bh * v->level[t] / 100;
        gfx_rfill(x + 10, by, 24, bh, 4, PANEL);
        if (lh > 0) gfx_rfill(x + 10, by + bh - lh, 24, lh, lh >= 8 ? 4 : 0, c);
        if (v->mute[t]) gfx_text_c(&font_s, x + 22, MAIN_Y + 80, "OFF", RED);
        else if (v->solo[t]) gfx_text_c(&font_s, x + 22, MAIN_Y + 80, "SOLO", YEL);
        else { p = put_u(b, v->level[t]); *p = 0; gfx_text_c(&font_s, x + 22, MAIN_Y + 80, b, TXT); }
    }
}
static void bar_row(int y, const char *label, int val, int bipolar, uint16_t c)
{
    gfx_text(&font_s, 10, y + 1, label, DIM);
    gfx_rfill(110, y, 122, 15, 4, PANEL);
    if (!bipolar) { if (val > 0) gfx_rfill(110, y, 122 * val / 100 < 8 ? 8 : 122 * val / 100, 15, 4, c); return; }
    gfx_fill(170, y - 2, 2, 19, LINE);                          /* the centre: off */
    int w = 60 * (val < 0 ? -val : val) / 100;
    if (val < 0) gfx_rfill(171 - w, y, w, 15, 3, c);
    else if (val > 0) gfx_rfill(171, y, w, 15, 3, c);
}
static void page_master(const ui_view *v)
{
    /* the master bus: the DJ filter (left low-pass, right high-pass), DUST, DUCK; and the track's own filter */
    page_title(v);
    if (!vis_on()) return;
    static const char *const L[4] = { "DJ FILTER", "DUST", "DUCK", "TRACK" };
    int val[4] = { v->mfilt, v->dust, v->duck, v->tfilt };
    for (int i = 0; i < 4; i++) bar_row(VIS_Y + 2 + i * 18, L[i], val[i], i == 0 || i == 3, i == 3 ? v->col[v->trk] : ui_knob_col[i]);
}
static void page_play(const ui_view *v)
{
    /* how the track plays: POLY, MONO or LEGATO; the glide between two notes, drawn */
    page_title(v);
    if (!vis_on()) return;
    uint16_t c = v->col[v->trk];
    static const char *const M[4] = { "POLY", "MONO", "LEG", "UNI" };
    int m = (v->prm[SP_VOICE] * 3 + 50) / 100;
    for (int i = 0; i < 4; i++) {
        int x = 6 + i * 58, on = i == m;
        gfx_rfill(x, VIS_Y + 2, 54, 24, 5, on ? c : PANEL);
        fit(&font_s, x + 2, VIS_Y + 2, 50, 24, M[i], on ? BG : DIM, 1);
    }
    int g = v->prm[SP_GLIDE], y0 = VIS_Y + 64, y1 = VIS_Y + 38, x0 = 70, w = 2 + g * 130 / 100;
    gfx_fill(VIS_X + 4, y0, x0 - VIS_X - 4, 3, TXT);
    for (int i = 0; i < w; i++) { int y = y0 + (y1 - y0) * i / w; gfx_fill(x0 + i, y, 2, 3, g ? c : TXT); }
    gfx_fill(x0 + w, y1, VIS_X + VIS_W - x0 - w - 4, 3, TXT);
}
static void page_arp(const ui_view *v)
{
    /* the arpeggio: 16 notes as it would play the chord held (C E G when nothing is) */
    page_title(v);
    if (!vis_on()) return;
    uint16_t c = v->col[v->trk];
    gfx_fill(VIS_X, VIS_Y + VIS_H - 1, VIS_W, 1, LINE);
    for (int i = 0; i < 16 && i < v->arp_n; i++) {
        int x = VIS_X + i * 14, h = v->arp_seq[i];
        if (h == 255) { gfx_fill(x + 3, VIS_Y + VIS_H - 4, 8, 2, LINE); continue; }
        int y = VIS_Y + VIS_H - 6 - h * (VIS_H - 10) / 100;
        gfx_rfill(x + 1, y, 12, 6, 2, c);
    }
}
static void page_drums(const ui_view *v)
{
    /* every sound of the kit, its 16 steps: the sound the keys write on in its colour, the others dim */
    if (gfx_visible(0, 0, GFX_W, 46)) {
        gfx_text(&font_l, 4, 8, "DRUMS", v->col[v->trk]);
        if (v->pad_names) gfx_text_r(&font_m, 236, 8 + font_l.cap - font_m.cap, v->pad_names[v->drum_sel & 15], TXT);
        gfx_fill(4, 41, 232, 1, LINE);
    }
    for (int r = 0; r < 16; r++) {
        int y = 46 + r * 8;
        if (!gfx_visible(0, y - 3, GFX_W, 11)) continue;
        int sel = r == v->drum_sel;
        if (sel) gfx_fill(0, y - 1, GFX_W, 9, PANEL);
        for (int i = 0; i < 16; i++) {
            int x = 8 + i * 14 + (i / 4) * 2, on = (v->drum_row[r] >> i) & 1;
            uint16_t f = on ? (sel ? v->col[v->trk] : TXT) : (i == v->drum_at ? LINE : PANEL2);
            if (!on && sel) f = PANEL2;
            gfx_fill(x, y, 12, 7, f);
            if (i == v->drum_at && r == 0) gfx_fill(x, y - 3, 12, 2, TXT);   /* the playhead, above the grid */
        }
    }
}
static void page_generic(const ui_view *v)
{
    /* an engine's page: each knob's value as a bar, in its colour, its name and value beside */
    page_title(v);
    if (!vis_on()) return;
    for (int i = 0; i < 4; i++) {
        const ui_knob *k = &v->knob[i];
        if (!k->label || !k->label[0]) continue;
        int y = VIS_Y + 2 + i * 18;
        gfx_text(&font_s, 10, y + 1, k->label, DIM);
        gfx_rfill(76, y, 104, 15, 4, PANEL);
        if (k->bar > 0) gfx_rfill(76, y, 104 * k->bar / 100 < 8 ? 8 : 104 * k->bar / 100, 15, 4, ui_knob_col[i]);
        fit(&font_s, 184, y, 50, 15, k->value, TXT, 2);
    }
}
/* NOTES: the steps shown (16, or zoomed: 8 32 64) as a piano roll; each note a bar as long as it lasts, the one edited outlined in white,
 * the step edited a lighter column, the beats ruled, the playhead a line; an accent a white cap, a slide a dash on
 * to where the next note starts, a soft note dimmer, a ghost hollow */
static void page_notes(const ui_view *v)
{
    /* the whole screen, as DRUMS: the page's name in the track's colour, the track's number beside, then the roll
     * from under the title to the dials; the black keys' rows a shade darker, each C ruled and named */
    uint16_t c = v->col[v->trk];
    if (gfx_visible(0, 0, GFX_W, 46)) {
        fit(&font_l, 4, 8, 190, font_l.cap, v->page_name, c, 0);
        gfx_rfill(204, 6, 32, 30, 6, c);
        char n[2] = { (char)('1' + v->trk), 0 };
        gfx_text_c(&font_m, 220, 6 + (30 - font_m.cap) / 2, n, BG);
        gfx_fill(4, 41, 232, 1, LINE);
    }
    const int x0 = 30, w = 206, y0 = 45, h = FOOT_Y - 4 - y0;
    if (!gfx_visible(0, y0 - 2, GFX_W, h + 4)) return;
    int lo = v->nr_lo, hi = v->nr_hi > v->nr_lo ? v->nr_hi : v->nr_lo + 1, rows = hi - lo + 1, rh = h / rows;
    if (rh < 3) rh = 3;
    int top = y0 + (h - rh * rows) / 2;
    int cols = v->nr_cols ? v->nr_cols : 16, rule = cols > 32 ? 16 : 4;   /* (64 steps: a line a bar) */
    for (int n = lo; n <= hi; n++) {
        int y = top + (hi - n) * rh, k = n % 12;
        int black = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
        gfx_fill(x0, y, w, rh, black ? BG : PANEL);
        if (k == 0) gfx_fill(x0, y + rh - 1, w, 1, LINE);
    }
    if (v->nr_cur < cols) { int cx = x0 + v->nr_cur * w / cols, cw = x0 + (v->nr_cur + 1) * w / cols - cx;
        gfx_fill(cx, top, cw < 2 ? 2 : cw, rh * rows, gfx_mix(PANEL2, TXT, 5)); }
    for (int b = 0; b <= cols; b += rule) gfx_fill(x0 + b * w / cols, top, 1, rh * rows, b % (rule * 4) ? LINE : DIM);
    /* the names: each C, else the lowest; the edited note's row in white */
    static const char NM[12][3] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int named = 0, sel_p = -1;
    for (int i = 0; i < v->nr_n && i < UI_NOTES_MAX; i++) if (v->nr[i].flags & NR_SEL) sel_p = v->nr[i].pitch;
    int ys[16], ny = 0;                                   /* (the edited note's name first; no name over another: 3.1.2) */
    for (int pass = 0; pass < 2; pass++) for (int n = lo; n <= hi; n++) {
        if (pass == 0 ? n != sel_p : (n == sel_p || (n % 12 && !(n == lo && !named && hi - lo < 12)))) continue;
        if (n % 12 == 0) named = 1;
        char b[8], *p = b; { const char *q = NM[n % 12]; while (*q) *p++ = *q++; }   /* (the octave below C0: -1, 3.1.4) */
        if (n < 12) { *p++ = '-'; *p++ = '1'; } else p = put_u(p, (uint32_t)(n / 12 - 1));
        *p = 0;
        int y = top + (hi - n) * rh + rh / 2 - font_s.cap / 2;
        if (y < y0) y = y0;
        if (y > top + rows * rh - font_s.cap) y = top + rows * rh - font_s.cap;
        int clash = 0; for (int i = 0; i < ny; i++) if (y - ys[i] < font_s.cap + 2 && ys[i] - y < font_s.cap + 2) clash = 1;
        if (clash || ny >= 16) continue;
        ys[ny++] = y;
        fit(&font_s, 0, y, 28, font_s.cap, b, n == sel_p ? TXT : DIM, 2);
    }
    for (int i = 0; i < v->nr_n && i < UI_NOTES_MAX; i++) {
        const ui_note *n = &v->nr[i];
        if (n->pitch < lo || n->pitch > hi) continue;
        int x = x0 + n->x * w / 64, nw = n->len * w / 64, y = top + (hi - n->pitch) * rh;
        if (nw < 4) nw = 4;
        if (x + nw > x0 + w) nw = x0 + w - x;
        int bh = rh > 3 ? rh - 1 : rh, by = y;
        if (n->flags & NR_GHOST) gfx_rframe(x, by, nw, bh, 2, 1, c);
        else gfx_rfill(x, by, nw, bh, 2, (n->flags & NR_SOFT) ? gfx_mix(BG, c, 18) : c);
        if (n->flags & NR_ACC) gfx_fill(x + 1, by, nw - 2, bh > 5 ? 3 : 1, TXT);
        if (n->flags & NR_SLIDE) gfx_fill(x + nw, by + bh / 2, 8, 2, TXT);
        if (n->flags & NR_SEL) gfx_rframe(x - 2, by - 2, nw + 4, bh + 4, 3, 2, TXT);
    }
    if (v->nr_play < 64) gfx_fill(x0 + v->nr_play * w / 64, top, 2, rh * rows, GREEN);
}
/* SONG 2/2: five entries of the song, each its number, its pattern in the track's colour (dim: no notes there; a
 * dot: the other tracks play others; a bar under: the track muted), its length as a bar (64 bars: the width) and in
 * words, the one the knobs edit framed, the one playing marked with how far it has gone */
static void page_song(const ui_view *v)
{
    layer_title(v);
    uint16_t c = v->col[v->trk];
    if (!v->sg_n && gfx_visible(4, 70, 232, 60)) {
        gfx_rfill(4, 74, 232, 60, 8, PANEL);
        fit(&font_m, 12, 78, 216, 24, "NO SONG", TXT, 1);
        fit(&font_s, 12, 106, 216, 18, "KNOB 4: ADD LINES", DIM, 1);
        return;
    }
    for (int i = 0; i < v->sg_n && i < 5; i++) {
        const ui_songrow *r = &v->sg[i];
        int y = TILE_Y + i * 26;
        if (!gfx_visible(0, y, GFX_W, 26)) continue;
        int sel = r->flags & SR_SEL;
        if (sel) gfx_rfill(0, y + 1, GFX_W, 24, 4, PANEL);
        char b[12], *p = put_u(b, r->num); *p = 0;
        fit(&font_s, 2, y + 1, 24, 24, b, (r->flags & SR_PLAY) ? GREEN : sel ? TXT : DIM, 2);
        int empty = r->flags & SR_EMPTY;
        gfx_rfill(30, y + 3, 26, 20, 4, empty ? PANEL2 : c);
        p = put_u(b, (uint32_t)r->pat + 1u); *p = 0;
        fit(&font_m, 30, y + 3, 26, 20, b, empty ? c : BG, 1);
        if (r->flags & SR_MIX) gfx_fill(58, y + 4, 4, 4, YEL);
        if (r->flags & SR_MUTE) gfx_fill(30, y + 24, 26, 2, RED);
        int w = (r->bars > 64 ? 64 : r->bars) * 80 / 64 + 4;
        gfx_rfill(66, y + 9, w, 8, 2, empty ? LINE : sel ? c : PANEL2);
        p = put_u(b, r->bars); { const char *q = " BARS"; while (*q) *p++ = *q++; } *p = 0;
        int tx = 66 + w + 6, tw = gfx_text_w(&font_s, b);
        gfx_text(&font_s, tx + tw > 236 ? 236 - tw : tx, y + 6, b, sel ? TXT : DIM);
        if (r->flags & SR_PLAY) {
            int n = r->bars ? r->bars : 1;
            gfx_fill(66, y + 19, (r->at + 1 > n ? n : r->at + 1) * w / n, 2, GREEN);
        }
    }
}
static void page_soon(const ui_view *v)
{
    page_title(v);
    if (!gfx_visible(0, MAIN_Y + 18, GFX_W, 80)) return;
    gfx_rfill(4, MAIN_Y + 22, 232, 64, 8, PANEL);
    fit(&font_m, 12, MAIN_Y + 30, 216, 22, "NEXT VERSION", TXT, 1);
    fit(&font_s, 12, MAIN_Y + 58, 216, 18, v->sub, DIM, 1);
}
static void toast(const ui_view *v)                       /* (3.1.2, as Sam asked: at the top, over the bar; the page stays seen) */
{
    if (v->saving) {
        if (!gfx_visible(4, 2, 232, 42)) return;
        gfx_rfill(4, 2, 232, 42, 8, PANEL2);
        gfx_rframe(4, 2, 232, 42, 8, 2, YEL);
        gfx_text_c(&font_s, 120, 8, "SAVING", YEL);
        gfx_fill(24, 29, 192, 8, LINE);
        gfx_fill(24, 29, 192 * v->save_pct / 100, 8, YEL);
        return;
    }
    if (!v->toast || !v->toast[0] || !gfx_visible(4, 2, 232, 42)) return;
    gfx_rfill(4, 2, 232, 42, 8, PANEL2);
    gfx_rframe(4, 2, 232, 42, 8, 2, v->toast_kind ? RED : GREEN);
    int n = 0; while (v->toast[n]) n++;
    if (gfx_text_w(&font_s, v->toast) - 2 * n <= 220) { fit(&font_m, 10, 2, 220, 42, v->toast, TXT, 1); return; }
    /* too long for a line: two, cut at the space nearest the middle */
    char a[48], b[48]; int cut = -1;
    for (int i = 0; i < n && i < 47; i++) if (v->toast[i] == ' ' && (cut < 0 || (i > n / 2 ? i - n / 2 : n / 2 - i) < (cut > n / 2 ? cut - n / 2 : n / 2 - cut))) cut = i;
    if (cut < 0 || n > 94) { fit(&font_m, 10, 2, 220, 42, v->toast, TXT, 1); return; }
    for (int i = 0; i < cut; i++) a[i] = v->toast[i];
    a[cut] = 0;
    int j = 0; for (const char *r = v->toast + cut + 1; *r && j < 47; r++) b[j++] = *r;
    b[j] = 0;
    fit(&font_s, 10, 5, 220, 17, a, TXT, 1);
    fit(&font_s, 10, 23, 220, 17, b, TXT, 1);
}

/* (3.3.6) the battery: a line 2 px high along the bottom of the screen, as full as the battery (green, yellow under
 * 30 %, red under 15 %, blinking under 8 %); a computer on the USB powers and charges it: a light runs along */
#define BATT_H 2
static void batt_bar(const ui_view *v)
{
    if ((v->batt > 100 && !v->batt_usb) || !gfx_visible(0, GFX_H - BATT_H, GFX_W, BATT_H)) return;
    int y = GFX_H - BATT_H;
    uint16_t track = gfx_mix(BG, DIM, 9);
    if (v->batt_usb) {
        gfx_fill(0, y, GFX_W, BATT_H, gfx_mix(BG, CYAN, 9));
        int x = ((v->batt_usb - 1) & 7) * 40 - 40;
        gfx_fill(x, y, 60, BATT_H, gfx_mix(BG, CYAN, 22)); gfx_fill(x + 15, y, 30, BATT_H, CYAN);
        return;
    }
    int w = (v->batt * GFX_W + 50) / 100;
    uint16_t c = v->batt >= 30 ? gfx_mix(BG, GREEN, 24) : v->batt >= 15 ? YEL : RED;
    if (v->batt < 8 && !v->blink) c = track;
    gfx_fill(0, y, w, BATT_H, c);
    gfx_fill(w, y, GFX_W - w, BATT_H, track);
}
void ui_draw(const ui_view *v)
{
    if (v->page == UI_VIS) { vis_draw(); if (v->info && gfx_visible(0, 0, GFX_W, 26)) info_line(v, 0, 0, GFX_W, 26); batt_bar(v); toast(v); return; }
    if (v->page == UI_SPLASH) { vis_splash_draw(); return; }
    gfx_fill(0, 0, GFX_W, GFX_H, BG);
    if (v->page == UI_LAYER) { layer_title(v); tiles(v); }
    else if (v->page == UI_PUNCH) { layer_title(v); punch_tiles(v); }
    else if (v->page == UI_DRUMS) page_drums(v);
    else if (v->page == UI_SONG) page_song(v);
    else if (v->page == UI_NOTES) page_notes(v);
    else {
        top_bar(v);
        if (v->page == UI_TRACKS) page_tracks(v);
        else {
            track_header(v);
            switch (v->page) {
            case UI_OSC: page_osc(v); break;
            case UI_FILTER: page_filter(v); break;
            case UI_VOICE: page_voice(v); break;
            case UI_ENV: page_env(v); break;
            case UI_ENVDEST: page_envdest(v); break;
            case UI_FX: page_fx(v); break;
            case UI_KIT: page_kit(v); break;
            case UI_MIXER: page_mixer(v); break;
            case UI_LFO: page_lfo(v); break;
            case UI_MASTER: page_master(v); break;
            case UI_PLAY: page_play(v); break;
            case UI_ARP: page_arp(v); break;
            case UI_GENERIC: page_generic(v); break;
            default: page_soon(v); break;
            }
        }
    }
    footer(v);
    if (v->info && (v->page == UI_DRUMS || v->page == UI_SONG || v->page == UI_NOTES) && gfx_visible(0, 0, GFX_W, 26)) info_line(v, 0, 0, GFX_W, 26);
    batt_bar(v);
    toast(v);
}
