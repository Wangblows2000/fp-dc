/* Layer-0 parallax (RE + RV: build/re/spec_parallax.md).
 * Every levelparallaxpiece is a SystemBox tiling one image; events 3541-3543 move it each frame:
 *   F += G;  sx = fmod(trunc(D*vl + F - vl), A) in (-A, 0];  sy = trunc(E*vt + C) - vt
 * (vl, vt = view left / top). The box is A * (trunc(427 / A) + 2) wide and B tall; the image tiles inside it.
 * DV2 / DV4 switch pieces 0/1 and 8/9 at view top 1360 (5101 / 5102); piece9's 1080-row strip holds 3 water frames
 * (5094: C = y0 + {-360, 0, -360, -720}[(t / 8) % 4]); the water surface (5103-5105) is a 1x96 strip stretched to
 * the view width at y 2700, 61 % opacity. Relic Maze (spec_rm_flow 4): RM4 / RM5 wrap in y (flag 8); RM3's sky set (flag 64) is
 * destroyed once the player is below y 2400. Images are split into power-of-two chunks (4 bpp, tools/palettize.py). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fp.h"

#define PMAXIMG 24
#define PMAXCH 16
#define PMAXPIECE 24
typedef struct { int x, y, w, h; tex_t t; } pchunk_t;
typedef struct { int img; float D, E, G; int y0, bw, bh, flags; double F; } ppiece_t;
static struct { int w, h, n; pchunk_t ch[PMAXCH]; } IMG[PMAXIMG];
static ppiece_t PC[PMAXPIECE];
static int nimg, npc, has_water, water_y;
static tex_t WT[2];

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
    int ni = rd16(d + 4), np = rd16(d + 6); has_water = rd16(d + 8);
    size_t off = 12;
    if (ni > PMAXIMG || np > PMAXPIECE) { free(d); return -1; }
    for (int i = 0; i < ni; i++) {
        if (off + 6 > sz) goto bad;
        IMG[i].w = rd16(d + off); IMG[i].h = rd16(d + off + 2); IMG[i].n = rd16(d + off + 4); off += 6;
        if (IMG[i].n > PMAXCH || off + 8u * IMG[i].n > sz) goto bad;
        for (int k = 0; k < IMG[i].n; k++, off += 8) {
            IMG[i].ch[k].x = rd16(d + off); IMG[i].ch[k].y = rd16(d + off + 2); IMG[i].ch[k].w = rd16(d + off + 4); IMG[i].ch[k].h = rd16(d + off + 6);
            char nm[64]; snprintf(nm, sizeof nm, "%s/%s_px%d_%d", dir, dir, i, k);
            if (tex_load_file_skip_empty(&IMG[i].ch[k].t, nm) < 0) IMG[i].ch[k].t.id = -1;
        }
        nimg = i + 1;
    }
    for (int i = 0; i < np; i++, off += 24) {
        if (off + 24 > sz) goto bad;
        PC[i].img = rd16(d + off); PC[i].D = rdf(d + off + 2); PC[i].E = rdf(d + off + 6); PC[i].G = rdf(d + off + 10);
        PC[i].y0 = rd32(d + off + 14); PC[i].bw = rd16(d + off + 18); PC[i].bh = rd16(d + off + 20); PC[i].flags = rd16(d + off + 22);
        PC[i].F = 0;
        if (PC[i].img >= nimg) goto bad;
        npc = i + 1;
    }
    if (has_water && off + 4 <= sz) {
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
    for (int k = 0; k < IMG[ii].n; k++) {
        const pchunk_t *c = &IMG[ii].ch[k];
        if (c->t.id < 0) continue;
        int cy0 = y + c->y, cy1 = cy0 + c->h, cx = x + c->x;
        if (cx >= SCREEN_W || cx + c->w <= 0 || cy1 <= 0 || cy0 >= SCREEN_H) continue;
        int v0 = 0, h = c->h;                             /* clip to the box (piece6 160 of 170 rows) */
        if (cy0 < clipy0) { v0 = clipy0 - cy0; h -= v0; cy0 = clipy0; }
        if (cy0 + h > clipy1) h = clipy1 - cy0;
        if (h <= 0) continue;
        r_sprite(&c->t, 0, v0, c->w, h, (float)cx, (float)cy0, (float)c->w, (float)h, 0, 0xffffffff);
    }
}

int par_count(void) { return npc; }

void par_draw(int gframe) {
    int vl = (int)G.camx, vt = (int)G.camy;
    for (int i = 0; i < npc; i++) {
        const ppiece_t *p = &PC[i];
        if ((p->flags & 1) && vt >= 1360) continue;      /* 5101 / 5102 */
        if ((p->flags & 2) && vt < 1360) continue;
        if (p->flags & 128) continue;                     /* destroyed (RM3 6298) */
        if ((p->flags & 16) && vl >= 7544) continue;     /* AT2 4398 / 4399 (at exactly 7544 the first frame's choice: left set) */
        if ((p->flags & 32) && vl <= 7544) continue;
        int A = p->bw, ih = IMG[p->img].h, iw = IMG[p->img].w;   /* box width (AT4 piece6_853: 400 for a 384-px image) */
        double C = p->y0;
        if (p->flags & 4) { static const int off[4] = { -360, 0, -360, -720 }; C += off[(gframe / 8) % 4]; }   /* 5094 */
        int sx = (int)fmod(trunc(p->D * vl + p->F - vl), (double)A);
        int sy = (int)trunc(p->E * vt + C) - vt;
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
