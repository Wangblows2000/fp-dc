/* Layer-0 parallax (RE + RV: build/re/spec_parallax.md).
 * Every levelparallaxpiece is a SystemBox tiling one image; events 3541-3543 move it each frame:
 *   F += G;  sx = fmod(trunc(D*vl + F - vl), A) in (-A, 0];  sy = trunc(E*vt + C) - vt
 * (vl, vt = view left / top). The box is A * (trunc(427 / A) + 2) wide and B tall; the image tiles inside it.
 * DV2 / DV4 switch pieces 0/1 and 8/9 at view top 1360 (5101 / 5102); piece9's 1080-row strip holds 3 water frames
 * (5094: C = y0 + {-360, 0, -360, -720}[(t / 8) % 4]); the water surface (5103-5105) is a 1x96 strip stretched to
 * the view width at y 2700, 61 % opacity. Relic Maze (spec_rm_flow 4): RM4 / RM5 wrap in y (flag 8); RM3's sky set (flag 64) is
 * destroyed once the player is below y 2400. Images are split into power-of-two chunks (4 bpp, tools/palettize.py).
 * Fortune Night (session 17, spec_fn_flow 4): the 16 BGStar_16 on layer 0 are placed every frame relative to piece0a (7466:
 * x = piece.x + A, y = piece.y + B, INFERRED from the operands); the importer draws them into one image, exported as a piece
 * with flag 1024: drawn once (no tiling) at the parent piece's position (handle in y0) + (bw, bh).
 * Sky Battalion (session 19, spec_sb_flow 4): 9198 replaces 3542 in every SB act, K = (vt + 20 sin(g3 deg) - 20) E + C (the
 * whole sky bobs with the frame counter g3, flag 2048; the port's g3 is the stage frame counter); Zao's airship
 * (LevelParallaxZaoShip_2243, a layer-0 SystemBox at a world position) is drawn once at (D, y0) in world coordinates in its
 * place between the pieces (flag 4096). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fp.h"

#define PMAXIMG 24
#define PMAXCH 32   /* FN piece5 (380 x 400): 18 power-of-two chunks; FN2 5_1802 (380 x 460): 30 */
#define PMAXPIECE 24
typedef struct { int x, y, w, h; tex_t t; } pchunk_t;
typedef struct { int img; float D, E, G; int y0, bw, bh, flags; double F; int handle; } ppiece_t;   /* handle: the Fusion object (file version 1) */
static struct { int w, h, n, fl; pchunk_t ch[PMAXCH]; } IMG[PMAXIMG];   /* fl (file version 2): 1 ARGB4444 (translucent list), 2 half resolution (drawn x2) */
static ppiece_t PC[PMAXPIECE];
static int nimg, npc, has_water, water_y;
static tex_t WT[2];
/* Jade Creek water surface (has_water 2; RE 10368-10373, session 21): LevelParalaxWaterSurface_14 fills the band between the
 * horizon piece's box bottom (piece5_2338, box 270 high) and the water line ML.B; the follower piece (piece6_2374, the water
 * body) is moved to the strip each frame (10369 / 10370). Frames stacked in one atlas (WT[0]). */
static struct { int nfr, fw, fh, hz, hzh, fol; double anchor; int valid; } JW;

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static int32_t rd32(const uint8_t *p) { return (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); }
static float rdf(const uint8_t *p) { uint32_t u = (uint32_t)rd32(p); float f; memcpy(&f, &u, 4); return f; }

void par_free(void) {
    for (int i = 0; i < nimg; i++) for (int k = 0; k < IMG[i].n; k++) if (IMG[i].ch[k].t.w > 0) r_tex_free(&IMG[i].ch[k].t);
    for (int k = 0; k < 2; k++) if (WT[k].w > 0) r_tex_free(&WT[k]);
    memset(IMG, 0, sizeof IMG); memset(WT, 0, sizeof WT); nimg = npc = has_water = 0;
}

int par_load(const char *dir) {
    par_free();
    char fn[64]; size_t sz;
    snprintf(fn, sizeof fn, "%s/%s_par.bin", dir, dir);
    uint8_t *d = plat_load_file(fn, &sz);
    if (!d) return -1;
    if (sz < 12 || memcmp(d, "FPPX", 4)) { free(d); return -1; }
    int ni = rd16(d + 4), np = rd16(d + 6), ver = rd16(d + 10), rec = ver >= 1 ? 26 : 24; has_water = rd16(d + 8);
    size_t off = 12;
    if (ni > PMAXIMG || np > PMAXPIECE) { free(d); return -1; }
    for (int i = 0; i < ni; i++) {
        int ih = ver >= 2 ? 8 : 6;
        if (off + ih > sz) goto bad;
        IMG[i].w = rd16(d + off); IMG[i].h = rd16(d + off + 2); IMG[i].n = rd16(d + off + 4); IMG[i].fl = ver >= 2 ? rd16(d + off + 6) : 0; off += ih;
        if (IMG[i].n > PMAXCH || off + 8u * IMG[i].n > sz) goto bad;
        for (int k = 0; k < IMG[i].n; k++, off += 8) {
            IMG[i].ch[k].x = rd16(d + off); IMG[i].ch[k].y = rd16(d + off + 2); IMG[i].ch[k].w = rd16(d + off + 4); IMG[i].ch[k].h = rd16(d + off + 6);
            char nm[64]; snprintf(nm, sizeof nm, (IMG[i].fl & 1) ? "%s/%s_pa%d_%d" : "%s/%s_px%d_%d", dir, dir, i, k);   /* _pa: ARGB4444 chunks */
            if (tex_load_file_skip_empty(&IMG[i].ch[k].t, nm) < 0) IMG[i].ch[k].t.id = -1;
        }
        nimg = i + 1;
    }
    for (int i = 0; i < np; i++, off += rec) {
        if (off + rec > sz) goto bad;
        PC[i].img = rd16(d + off); PC[i].D = rdf(d + off + 2); PC[i].E = rdf(d + off + 6); PC[i].G = rdf(d + off + 10);
        PC[i].y0 = rd32(d + off + 14); PC[i].bw = rd16(d + off + 18); PC[i].bh = rd16(d + off + 20); PC[i].flags = rd16(d + off + 22);
        PC[i].F = 0; PC[i].handle = ver >= 1 ? rd16(d + off + 24) : 0;
        if (PC[i].img >= nimg) goto bad;
        npc = i + 1;
    }
    if (has_water == 2 && off + 16 <= sz) {
        water_y = rd32(d + off); JW.nfr = rd16(d + off + 4); JW.fw = rd16(d + off + 6); JW.fh = rd16(d + off + 8);
        JW.hz = rd16(d + off + 10); JW.hzh = rd16(d + off + 12); JW.fol = rd16(d + off + 14); JW.valid = 0;
        char nm[64]; snprintf(nm, sizeof nm, "%s/%s_pv0", dir, dir);   /* 4 bpp (build_level.sh --pal4=1:_pv) */
        if (JW.nfr < 1 || tex_load_file(&WT[0], nm) < 0) has_water = 0;
    } else if (has_water == 1 && off + 4 <= sz) {
        water_y = rd32(d + off);
        for (int k = 0; k < 2; k++) { char nm[64]; snprintf(nm, sizeof nm, "%s/%s_pw%d", dir, dir, k); if (tex_load_file(&WT[k], nm) < 0) has_water = 0; }
    } else has_water = 0;
    free(d);
    return 0;
bad:
    free(d); par_free(); plat_log("parallax %s: bad file\n", fn);
    return -1;
}

void par_update(void) {
    for (int i = 0; i < npc; i++) {
        PC[i].F += PC[i].G;                                                   /* 3541 (once per gameplay frame) */
        if ((PC[i].flags & 64) && G.p.y - ORIG_REF_DY > 2400) PC[i].flags |= 128;           /* RM3 6298: the sky set is destroyed below y 2400 (until the act reloads) */
    }
}

static void draw_image(int ii, int x, int y, int clipy0, int clipy1) {
    int s = (IMG[ii].fl & 2) ? 2 : 1;                     /* half-resolution chunks (SB mist): texel coordinates x 2 on screen */
    uint32_t col = (IMG[ii].fl & 1) ? 0xfeffffff : 0xffffffff;   /* ARGB4444: drawn through the translucent list */
    for (int k = 0; k < IMG[ii].n; k++) {
        const pchunk_t *c = &IMG[ii].ch[k];
        if (c->t.id < 0) continue;
        int cw = c->w * s, chh = c->h * s;
        int cy0 = y + c->y * s, cy1 = cy0 + chh, cx = x + c->x * s;
        if (cx >= SCREEN_W || cx + cw <= 0 || cy1 <= 0 || cy0 >= SCREEN_H) continue;
        int v0 = 0, h = chh;                              /* clip to the box (piece6 160 of 170 rows) */
        if (cy0 < clipy0) { v0 = clipy0 - cy0; h -= v0; cy0 = clipy0; }
        if (cy0 + h > clipy1) h = clipy1 - cy0;
        if (h <= 0) continue;
        r_sprite(&c->t, 0, v0 / s, c->w, h / s, (float)cx, (float)cy0, (float)cw, (float)h, 0, col);
    }
}

int par_count(void) { return npc; }
/* scripted pieces (RM3 Claw chase 6256 / 6257 / 6290, src/game/rm_claw.c): by Fusion handle, the drift G (F += G per
 * frame) and the hidden flag 512 (a hidden piece is not drawn); flag 128 = destroyed */
void par_set_drift(int handle, float g) { for (int i = 0; i < npc; i++) if (PC[i].handle == handle) PC[i].G = g; }
void par_set_hidden(int handle, int hidden) { for (int i = 0; i < npc; i++) if (PC[i].handle == handle) PC[i].flags = (PC[i].flags & ~512) | (hidden ? 512 : 0); }
void par_destroy(int handle) { for (int i = 0; i < npc; i++) if (PC[i].handle == handle) PC[i].flags |= 128; }

int par_hidden(int handle) { for (int i = 0; i < npc; i++) if (PC[i].handle == handle) return (PC[i].flags & 512) != 0; return 0; }

static void draw_pieces_pass(int gframe, int front);
/* 10368: C = (horizon box bottom - ML.B) / 70.0001 (world y). C < 0: the strip hangs above its anchor, scale |C| up to 1, the anchor
 * (10373: C <= -1) ML.B + (C + 1) 70, alpha 255 (10369); C >= 0: below ML.B at half scale, alpha 128 (10370). The follower piece
 * sits at the anchor (10369; 10370's vtable+0x20 read as the surface y, INFERRED) */
static double jw_C;
static void jw_update(int vt) {
    JW.valid = 0;
    if (has_water != 2) return;
    for (int i = 0; i < npc; i++) if (PC[i].handle == JW.hz && !(PC[i].flags & 128)) {
        double top = trunc(PC[i].E * vt + PC[i].y0);
        jw_C = (top + JW.hzh - water_y) / 70.0001;
        JW.anchor = jw_C < -1 ? water_y + (jw_C + 1) * 70 : water_y;
        JW.valid = 1;
    }
}
int par_water_band(double *anchor, double *C) { jw_update((int)G.camy); if (!JW.valid) return 0; *anchor = JW.anchor; *C = jw_C; return 1; }   /* host tests */
static void jw_draw(int gframe, int vt) {
    if (!JW.valid) return;
    double sc = jw_C < 0 ? fmin(-jw_C, 1.0) : jw_C * 0.5;
    int h = (int)(JW.fh * sc), y = (int)JW.anchor - vt;
    if (h <= 0) return;
    int k = (gframe / 4) % 8; if (k >= JW.nfr) k = JW.nfr - 1;       /* anim speed 25: a frame every 4 (RV jc3_start ws_afr; the 8th shown as the 7th, VRAM) */
    r_sprite(&WT[0], 0, k * JW.fh, JW.fw, JW.fh, 0, (float)(jw_C < 0 ? y - h : y), (float)JW.fw, (float)h, 0, jw_C < 0 ? 0xffffffff : 0x80ffffff);
}
void par_draw(int gframe) { draw_pieces_pass(gframe, 0); }
void par_draw_front(void) { draw_pieces_pass(G.frame, 1); }   /* flag-256 pieces (frame layer 3, in front of the player) */
static void draw_pieces_pass(int gframe, int front) {
    int vl = (int)G.camx, vt = (int)G.camy;
    if (!front) jw_update(vt);
    for (int i = 0; i < npc; i++) {
        const ppiece_t *p = &PC[i];
        if (((p->flags & 256) != 0) != front || (p->flags & 512)) continue;
        if ((p->flags & 1) && vt >= 1360) continue;      /* 5101 / 5102 */
        if ((p->flags & 2) && vt < 1360) continue;
        if (p->flags & 128) continue;                     /* destroyed (RM3 6298) */
        if ((p->flags & 16) && vl >= 7544) continue;     /* AT2 4398 / 4399 (at exactly 7544 the first frame's choice: left set) */
        if ((p->flags & 32) && vl <= 7544) continue;
        if (p->flags & 1024) {                            /* FN BGStar set: once, at the parent piece (7466) */
            const ppiece_t *q = NULL; for (int k = 0; k < npc && !q; k++) if (PC[k].handle == p->y0 && !(PC[k].flags & 1024)) q = &PC[k];
            if (!q) continue;
            int qx = (int)fmod(trunc(q->D * vl + q->F - vl), (double)q->bw), qy = (int)trunc(q->E * vt + q->y0) - vt;
            draw_image(p->img, qx + p->bw, qy + p->bh, -32768, 32767);
            continue;
        }
        if (p->flags & 4096) { draw_image(p->img, (int)p->D - vl, p->y0 - vt, -32768, 32767); continue; }   /* SB Zao's airship (world) */
        int A = p->bw, ih = IMG[p->img].h, iw = IMG[p->img].w;   /* box width (AT4 piece6_853: 400 for a 384-px image) */
        double C = p->y0;
        double bvt = vt;
        if (p->flags & 2048) bvt = vt + 20.0 * sin(gframe * 0.017453292519943295) - 20.0;   /* SB 9198 (g3 = 180: sin 0 exactly, the same value) */
        if (p->flags & 4) { static const int off[4] = { -360, 0, -360, -720 }; C += off[(gframe / 8) % 4]; }   /* 5094 */
        if (JW.valid && p->handle == JW.fol) { C = JW.anchor; bvt = 0; }   /* JC 10369 / 10370: the water body follows the strip */
        int sx = (int)fmod(trunc(p->D * vl + p->F - vl), (double)A);
        int sy = (int)trunc(p->E * bvt + C) - vt;
        int boxw = A * ((427 / A) + 2), y1 = sy + p->bh;
        if (p->flags & 8) {                               /* AT3 / AT4 (4532 / 4527): endless in y as well */
            int B = p->bh;
            sy = (int)fmod(trunc(p->E * vt + C - vt), (double)B);
            y1 = sy + B * ((241 / B) + 2);
        }
        if (y1 <= 0 || sy >= SCREEN_H) continue;
        for (int ty = sy; ty < y1; ty += ih)               /* tiles down the box (piece13), clipped at its bottom */
            for (int bx = sx; bx < sx + boxw && bx < SCREEN_W; bx += A)
                for (int tx = bx; tx < bx + A && tx < SCREEN_W; tx += iw)   /* the image tiles inside each box period */
                    draw_image(p->img, tx, ty, sy, y1);
    }
    if (front) return;
    if (has_water == 2) { jw_draw(gframe, vt); return; }
    if (has_water) {                                      /* 5103-5105 */
        double C = (water_y - (vt + 120)) / 220.0;
        double sc = fmin(fabs(C), 2.0);
        int h = (int)(96 * sc);
        if (h > 0) {
            int y = water_y - vt;
            if (C > 0) r_sprite(&WT[0], 0, 0, 1, 96, 0, (float)(y - h), (float)SCREEN_W, (float)h, 0, 0x9cffffff);
            else       r_sprite(&WT[1], 0, 0, 1, 96, 0, (float)y, (float)SCREEN_W, (float)h, 0, 0x9cffffff);
        }
    }
}
