/* Trap Hideout gimmicks / traps (session 21b): types 1620-1679 (th.h OT_THG_*). Spec: build/re/spec_th_gimmicks.md.
 * Labels: RE (decompiled events / constructors of frames 56-58 = handle_frame_57..59), RV (original traces
 * build/orig_traces/th_gim), INFERRED (marked).
 * Families: the scan lasers of TH1 (Q33 beams HazardScanLaserSweep / Right / Left / Down / Up and their heads
 * HazardScanLaserHead / Head2-5: 13479-13493, loop_l_56 = 13458-13461, 13541-13545, test_collisions_56), the
 * shurikens of TH2 / TH3 (HazardShuriken + Base + Turn: 13571-13573, foreach_s_57 / 13708, 13694 / 14097 / 14091), the
 * pillar platforms LevelHideoutPlatform1 (the shared path-platform rule of objects.c, F 2 G 64 H 1) + PlatformFlame
 * (13571), the barrel ride GimmickBarrel (+ Debris, 13574-13597, foreach_brl_57, 13688, col_6_57; TH3 13726-13729),
 * THBacklight1 (13532) and THFlame (scenery). LevelTHCollapsingPlatform is jc_gim.c's collapsing platform (member 2).
 * Passes: th_gim_pre (nothing: the pillars run in objects_platforms), th_gim_obj (the families run in th_gim_frame),
 * th_gim_frame (after the player block, in event order), th_gim_draw (the x-scaled beams), th_gim_player (ST_RIDE).
 * Object fields: par = family member (importer THG_SUB, lvobj anim bits 0-7), layer = instance layer (bits 8-14);
 * av[] = class alterables named per family below. */
#include "th.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#endif

#define RAD 57.29577951308232
#define RT0 OT_THG_BEAM
#define NRT (OT_THG_END - RT0 + 1)
#define LMAX 128
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]
#define FB1 0x1000                                 /* beam flag 1 (13460: blocked by Milla's cube this frame) */
#define FH_BG 0x2000                               /* head: overlapping the background last frame (test_collisions_56 edge) */
#define FH_STOP 0x4000                             /* head: speed 0 (13542-13545) */
static struct {
    int on, off;                                   /* off: 13513 removed the lasers */
    short lim[64]; int nlim;                       /* the act's EnemyLimiter_383 slots (heads stop on them) */
    int ride, ride_obj; uint32_t pressed, held;    /* barrel ride (M 98) */
    int hurt0;                                     /* frame the beam's hurt state began (-1: none) */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / RAD); }                  /* Chowdren sin: 0 at exactly 180 */
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / RAD); }   /* cos: 0 at exactly 90 / 270 */
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }
static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static obj_t *spawn(int type, const char *anim, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame; o->par = -1;
    if (anim) { set_s(o, anim); if (o->an.b) anim_restart(&o->an); }
    lst_add(o);
    return o;
}
/* the action point (RE Active::update_action_point + RV las1: 421 / 421 frames of the TH1 sweep beam): with an angle the image's
 * action point and hot spot are each rotated about the image origin in float (x' = ax c + ay s, y' = ay c - ax s), truncated,
 * and subtracted. (relic_act_point rotates the difference instead: up to 1 px off; the head image is not cropped.) */
static void act_pt(const obj_t *o, double *x, double *y) {
    if (o->ang == 0 || !o->an.b || !o->an.b->act) { relic_act_point(o, x, y); return; }
    const frame_t *f = anim_frame(&o->an);
    int fi = (int)(f - o->an.b->frames);
    float c = (float)c_cos(o->ang), s = (float)c_sin(o->ang);
    float ax = (float)o->an.b->act[2 * fi], ay = (float)o->an.b->act[2 * fi + 1], hx = (float)f->hx, hy = (float)f->hy;
    float axr = ax * c, ays = ay * s, ayc = ay * c, axs = ax * s, hxc = hx * c, hys = hy * s, hyc = hy * c, hxs = hx * s;
    int rx = (int)(axr + ays) - (int)(hxc + hys), ry = (int)(ayc - axs) - (int)(hyc - hxs);
    *x = (int)o->x + rx; *y = (int)o->y + ry;
}
static void char_rect(const player_t *p, int *x0, int *y0, int *x1, int *y1) {      /* PlayerCharacterSensor (inclusive) */
    int X = (int)PX(p), Y = (int)PY(p), small = player_char_sensor_small(p);
    *x0 = X - 7; *x1 = X + 8; *y0 = small ? Y - 13 : Y - 34; *y1 = small ? Y + 14 : Y + 12;
}
static int sens_hit_bb(const obj_t *o) {                     /* S_BOTTOMBIG on the object */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(SENS_BOTTOMBIG, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int sens_on_bg_side(int s) {                          /* a big side sensor on the background (its Z) */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) if (b[j] >= a[j] && px_span_solid(&G.lvl, ox + a[j], ox + b[j], oy + j)) return 1;
    return 0;
}
static int csens_hit(const player_t *p, const obj_t *o) {
    int x0, y0, x1, y1; char_rect(p, &x0, &y0, &x1, &y1);
    return o->an.b && obj_rect_hit(o, x0, y0, x1 + 1, y1 + 1);
}

/* ================================================================ 1. scan lasers (TH1)
 * beam (Q33): ang = its angle, av[0] A length (ctor 10; image 10 px, x scale A / 10), av[3] / av[4] D / E endpoint, dir 0 dim /
 * 1 burning, par 0 sweep (rides a Head), 1 right (Head2), 2 left (Head3), 3 down (Head4), 4 up (Head5); fl 4 (flag 2) = left
 * out of the 180-frame cycle (the boss beam, 13546). av[5] / av[6] / av[7] = cached blocker distances (background, Q7, origin
 * key) for beams whose head and angle did not move.
 * head: par 0 = Head (static, angle av[1] B), 2..5 = Head2..5 (BallMovement 16 = 2 px a frame, direction 8 / 24 / 0 / 16). */
#define BEAM_MAXD 2048
static const signed char BEAM_HEAD[5] = { 0, 2, 3, 4, 5 };   /* 13485: sweep <- Head, right <- Head2, left <- Head3, down <- Head4, up <- Head5 */
static const short BEAM_ANG[5] = { 0, 0, 180, 270, 90 };     /* 13541 (floats 90 / 180 / 270 read from the binary) */
static int beam_half(const obj_t *o) { return o->dir == 1 ? 2 : 1; }   /* opaque rows: dim 11059 rows 2-4, burning 12512 / 12531 / 11061 rows 1-5 (hotspot row 3) */
/* the cross-section of the beam at distance d: centre + k * normal, k = -h..h */
static int near_ext(const obj_t *o, int m);
static float BA = -1; static double BC = 1, BS = 0;   /* cos / sin of the last beam angle (beam_pt is called per pixel) */
static void beam_pt(const obj_t *o, double d, int k, int *x, int *y) {
    if (o->ang != BA) { BA = o->ang; BC = c_cos(o->ang); BS = c_sin(o->ang); }
    double c = BC, s = BS;                       /* the image pixel (d, k) of the hot spot's row k, its centre rotated (RV las1: a left / up beam
                                                  * ends 1 px nearer its head than a right / down one against the same wall) */
    *x = (int)floor(o->x + (d + 0.5) * c + (k + 0.5) * s);
    *y = (int)floor(o->y - (d + 0.5) * s + (k + 0.5) * c);
}
static int sec_bg(const obj_t *o, int d, int h) {             /* any solid background pixel in the cross-section at d */
    int xa, ya, xb, yb; beam_pt(o, d, -h, &xa, &ya); beam_pt(o, d, h, &xb, &yb);
    if (ya == yb) return px_span_solid(&G.lvl, xa < xb ? xa : xb, xa < xb ? xb : xa, ya);   /* vertical beam: one row */
    if (xa == xb) { for (int y = ya < yb ? ya : yb; y <= (ya < yb ? yb : ya); y++) if (px_solid(&G.lvl, xa, y)) return 1; return 0; }
    for (int k = -h; k <= h; k++) { int x, y; beam_pt(o, d, k, &x, &y); if (px_solid(&G.lvl, x, y)) return 1; }
    return 0;
}
/* Q7 (always-solid objects) across distances [d0, d1): the footprint as sensor rows (<= 24 px wide chunks) */
static int sec_q7(const obj_t *o, int d0, int d1, int h) {
    static signed char X0[64], X1[64];
    int minx = 1 << 30, miny = 1 << 30, maxy = -(1 << 30);
    for (int d = d0; d < d1; d++) for (int k = -h; k <= h; k += 2 * h) {
        int x, y; beam_pt(o, d, k, &x, &y);
        if (x < minx) minx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }
    int H = maxy - miny + 1;
    if (H > 64) H = 64;
    for (int j = 0; j < H; j++) { X0[j] = 63; X1[j] = -1; }
    for (int d = d0; d < d1; d++) for (int k = -h; k <= h; k++) {
        int x, y; beam_pt(o, d, k, &x, &y);
        int j = y - miny, i = x - minx;
        if (j < 0 || j >= H || i < 0 || i > 27) continue;
        if (i < X0[j]) X0[j] = (signed char)i;
        if (i > X1[j]) X1[j] = (signed char)i;
    }
    return objects_sensor_hit(OBJGRP_SOLID, minx, miny, X0, X1, H);
}
static int beam_block_q7(const obj_t *o, int lim, int h) {    /* first blocked distance (Q7), BEAM_MAXD if none below lim */
    for (int d = 0; d < lim; d += 16) {
        int e = d + 16 < lim ? d + 16 : lim;
        if (!sec_q7(o, d, e, h)) continue;
        for (int k = d; k < e; k++) if (sec_q7(o, k, k + 1, h)) return k;
    }
    return BEAM_MAXD;
}
static int beam_block_cube(const obj_t *o, int lim, int h) {  /* Milla's cube (PlayerReflect_463) */
    if (player_cube_state() < 0) return BEAM_MAXD;
    for (int d = 0; d < lim; d++) {
        int xa, ya, xb, yb; beam_pt(o, d, -h, &xa, &ya); beam_pt(o, d, h, &xb, &yb);
        int x0 = xa < xb ? xa : xb, x1 = xa < xb ? xb : xa, y0 = ya < yb ? ya : yb, y1 = ya < yb ? yb : ya;
        if (player_cube_rect(x0, y0, x1 + 1, y1 + 1)) return d;
    }
    return BEAM_MAXD;
}
/* a rotated beam against the background: every screen pixel whose centre maps back into the beam image (u along it, v across:
 * u in [0, lim), v in [-h, h + 1)) is tested; returns the smallest blocked u (the overlap rule of a transformed sprite, INFERRED
 * from RV las1: the forward-mapped pixel set is up to 1 px off on the sweeps) */
static int beam_bg_rot(const obj_t *o, int lim, int h) {
    double c = c_cos(o->ang), s = c_sin(o->ang), X = (int)o->x, Y = (int)o->y;
    double cu[4] = { 0, (double)lim, (double)lim, 0 }, cv[4] = { (double)-h, (double)-h, (double)(h + 1), (double)(h + 1) }, cx[4], cy[4];
    double ymn = 1e9, ymx = -1e9;
    for (int k = 0; k < 4; k++) {
        cx[k] = X + cu[k] * c + cv[k] * s; cy[k] = Y - cu[k] * s + cv[k] * c;
        ymn = fmin(ymn, cy[k]); ymx = fmax(ymx, cy[k]);
    }
    int best = BEAM_MAXD;
    for (int py = (int)floor(ymn); py <= (int)ceil(ymx); py++) {
        double yc = py + 0.5, xl = 1e9, xr = -1e9;
        for (int k = 0; k < 4; k++) {                                   /* the row against the 4 edges */
            int j = (k + 1) & 3; double y0 = cy[k], y1 = cy[j];
            if ((yc < y0 && yc < y1) || (yc > y0 && yc > y1) || y0 == y1) continue;
            double xi = cx[k] + (yc - y0) * (cx[j] - cx[k]) / (y1 - y0);
            xl = fmin(xl, xi); xr = fmax(xr, xi);
        }
        if (xl > xr) continue;
        for (int px = (int)floor(xl) - 1; px <= (int)ceil(xr); px++) {
            double rx = px + 0.5 - X, ry = yc - Y, u = rx * c - ry * s, v = rx * s + ry * c;
            if (u < 0 || u >= lim || v < -h || v >= h + 1 || (int)floor(u) >= best) continue;
            if (px_solid(&G.lvl, px, py)) best = (int)floor(u);
        }
    }
    return best;
}
/* 13484 / loop_l_56: 16 passes of 13458 (background) / 13459 (Q7) / 13460 (Milla's cube: flag 1) / 13461 x 2 (flag 1 and the
 * cube) A - 1 each, then A + 1 when nothing overlaps. Overlap of [0, A) with a blocker first met at distance D <=> D < A. */
static void beam_length(obj_t *o) {
    int h = beam_half(o), A = (int)o->av[0], lim = (A > 0 ? A : 0) + 17;
    double key = ((double)(int)o->x * 65536.0 + (int)o->y) * 8.0 + h + o->ang * 1e12;   /* background distance cache: same origin, angle, width */
    int Dbg;
    if (o->av[7] == key && (o->av[5] < BEAM_MAXD || o->av[6] >= lim)) Dbg = (int)o->av[5];
    else {
        Dbg = BEAM_MAXD;
        if (o->ang != 0 && o->ang != 90 && o->ang != 180 && o->ang != 270) Dbg = beam_bg_rot(o, lim, h);
        else for (int d = 0; d < lim; d++) if (sec_bg(o, d, h)) { Dbg = d; break; }
        o->av[5] = Dbg; o->av[6] = lim; o->av[7] = key;
    }
    int nearb = near_ext(o, 600);                                                       /* objects / the cube only near the view (cost; INFERRED) */
    int Dq = nearb ? beam_block_q7(o, lim, h) : BEAM_MAXD;
    int Dc = nearb && chr_is_milla(G.character) ? beam_block_cube(o, lim, h) : BEAM_MAXD;
    o->fl &= ~FB1;                                                                      /* 13484: flag 1 off */
    for (int it = 0; it < 16; it++) {
        if (Dbg < A) A--;                                                               /* 13458 */
        if (Dq < A) A--;                                                                /* 13459 */
        if (Dc < A) { A--; o->fl |= FB1; }                                              /* 13460 (MV.A 4) */
        for (int t = 0; t < 2; t++) if ((o->fl & FB1) && Dc < A) A--;                   /* 13461 (called twice) */
        if (!(Dbg < A) && !(Dc < A) && !(Dq < A)) A++;                                  /* loop end: free -> A + 1 */
    }
    o->av[0] = A;
    o->av[3] = floor(o->x + A * c_cos(o->ang)); o->av[4] = floor(o->y - A * c_sin(o->ang));
}
/* does the beam's footprint [0, A) touch the rectangle (inclusive)? */
static int beam_rect(const obj_t *o, int x0, int y0, int x1, int y1) {
    int A = (int)o->av[0], h = beam_half(o);
    if (A <= 0) return 0;
    if (o->ang == 0 || o->ang == 90 || o->ang == 180 || o->ang == 270) {         /* the footprint is a rectangle: its two corners */
        int xa, ya, xb, yb; beam_pt(o, 0, -h, &xa, &ya); beam_pt(o, A - 1, h, &xb, &yb);
        int bx0 = xa < xb ? xa : xb, bx1 = xa < xb ? xb : xa, by0 = ya < yb ? ya : yb, by1 = ya < yb ? yb : ya;
        return bx0 <= x1 && x0 <= bx1 && by0 <= y1 && y0 <= by1;
    }
    for (int d = 0; d < A; d++) for (int k = -h; k <= h; k++) {
        int x, y; beam_pt(o, d, k, &x, &y);
        if (x >= x0 && x <= x1 && y >= y0 && y <= y1) return 1;
    }
    return 0;
}
int th_gim_beam_rect(const obj_t *beam, int x0, int y0, int x1, int y1) { return beam && beam->type == OT_THG_BEAM && beam_rect(beam, x0, y0, x1, y1); }   /* (merge: 13488 totems) */
/* 13490-13493 for one burning beam: the character sensor on it */
static int beam_hurt(obj_t *o, player_t *p) {
    if (o->dir != 1 || dead(p)) return 0;
    int x0, y0, x1, y1; char_rect(p, &x0, &y0, &x1, &y1);
    if (!beam_rect(o, x0, y0, x1, y1)) return 0;
    if (vulnerable(p)) player_contact(p, 5);                                             /* 13490: R 0, OTHERS.E <= 0 -> R 1, LIFE.B + 5 */
    else if (p->state == ST_WILDKICK && !p->invinc && !p->hurt_pending && !dead(p)) {    /* 13491: M 44 with OTHERS.D 2 -> hurt, D = E = 0 */
        p->invuln = 0; player_contact(p, 5);
    }
    if (p->state == ST_HURT) {                                                            /* 13492: M 8 -> B = G = 0 */
        p->vx = 0; p->gsp = 0; p->vy = 0;
        if (!p->ground && px_span_solid(&G.lvl, (int)p->x - 7, (int)p->x + 7, (int)floor(p->y) + 1)) p->ground = 1;   /* no knock-back: still on her floor (RV L 1) */
        /* RV lasd_c / lasd_m: without its knock-back the hurt state (M 8) lasts 22 frames from its start, the hits every 7 frames
         * included (the port's rule "5 frames after landing" would free her after each hit) */
        if (S.hurt0 < 0) S.hurt0 = (int)G.frame;
        p->hurt_land_t = (S.hurt0 + 22) - ((int)G.frame - p->state_t) - 5;
    }
    if (p->invuln > 6 && !p->invinc) p->invuln = 6;                                      /* 13493: OTHERS.E = min(E, 6) */
    return 1;
}
int th_gim_beam(obj_t *o, player_t *p) {
    beam_length(o);
    return beam_hurt(o, p);
}
void th_gim_beam_draw(const obj_t *o) {
    if (!o->alive || !o->an.b || o->av[0] <= 0) return;
    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy), L = (float)o->av[0];
    if (sx < -L - 16 || sx > SCREEN_W + L + 16 || sy < -L - 16 || sy > SCREEN_H + L + 16) return;
    r_set_xscale(L / 10.f);
    frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, 0, 0xffffffff);
    r_set_xscale(1.f);
}
static void beam_anim(obj_t *o) {                     /* direction 0: the dim line (11059), 1: burning (3 frames, speed 50) */
    const char *nm = o->dir == 1 ? "thg_beam1" : "thg_beam0";
    if (o->an.anim != ens_anim(nm) || o->an.b != &G.ens) { set_s(o, nm); anim_restart(&o->an); }
}
static obj_t *beam_head(const obj_t *b, int k) {      /* 13485: the k-th beam of a class on the k-th head of its head class */
    obj_t *h; int i = 0;
    EACH(OT_THG_HEAD, h) if (h->par == BEAM_HEAD[b->par] && i++ == k) return h;
    return NULL;
}
static int near_ext(const obj_t *o, int m) {          /* within the view grown by m (the port updates far beams lazily, INFERRED) */
    return o->x > G.camx - m && o->x < G.camx + SCREEN_W + m && o->y > G.camy - m && o->y < G.camy + SCREEN_H + m;
}
static void heads_move(void) {                        /* BallMovement (update_objects, before the events): 2 px a frame */
    obj_t *h;
    EACH(OT_THG_HEAD, h) {
        if (h->par < 2 || (h->fl & FH_STOP)) continue;
        int d = h->dir & 31;
        h->x += d == 0 ? 2 : d == 16 ? -2 : 0; h->y += d == 24 ? 2 : d == 8 ? -2 : 0;
    }
}
static void heads_bounce(void) {                      /* test_collisions_56: a head newly on the background turns round (direction + 16) */
    obj_t *h;
    EACH(OT_THG_HEAD, h) {
        if (h->par < 2) continue;
        if (!obj_overlaps_bg(h)) { h->fl &= ~FH_BG; continue; }
        if (!(h->fl & FH_BG)) { h->fl |= FH_BG; h->dir = (h->dir + 16) & 31; }
    }
}
static void heads_limit(void) {                       /* 13542-13545: a moving head on an EnemyLimiter stops; the limiter goes */
    obj_t *h;
    EACH(OT_THG_HEAD, h) {
        if (h->par < 2) continue;
        for (int k = 0; k < S.nlim; k++) {
            obj_t *l = &G.o[S.lim[k]];
            if (!l->alive || l->type != OT_LIMITER) continue;
            if (fabsf(l->x - h->x) > 64 || fabsf(l->y - h->y) > 64) continue;
            if (obj_rect_hit(h, (int)l->x - 21, (int)l->y - 18, (int)l->x - 5, (int)l->y + 14)) { h->fl |= FH_STOP; l->alive = 0; }   /* EnemyLimiter: 16 x 32, hotspot (21, 18) */
        }
    }
}
static void lasers(player_t *p) {
    obj_t *b, *h;
    /* (g3 * 2) mod 360. The original's g3 (event 1009, + 1 a frame) is not reset by the act start (RV: 107 .. 524604 at the first
     * frame of TH traces), so the phase at the act start is arbitrary there; the port uses its stage frame (INFERRED) */
    long g3 = (long)G.frame;
#ifdef FP_HOST_ENV
    { static int o = -1; if (o < 0) o = getenv("FP_THG_G3") ? atoi(getenv("FP_THG_G3")) : 0; g3 += o; }   /* tests: align with a trace */
#endif
    int ph = (int)((g3 * 2) % 360);
    double B = ph == 180 ? 270 : 270 - 45 * fd_sin(ph / RAD);                    /* 13479 */
    int any = 0; EACH(OT_THG_BEAM, b) if (!(b->fl & 4)) { any = 1; break; }
    if (any) EACH(OT_THG_HEAD, h) if (h->par == 0) { h->av[1] = B; h->ang = (float)B; }
    EACH(OT_THG_BEAM, b) {
        if (b->fl & 4) continue;
        if (b->par == 0) b->dir = (ph >= 91 && ph <= 269) ? 0 : 1;               /* 13482 / 13483: the sweep burns in the other half */
        else b->dir = (ph < 91 || ph > 269) ? 0 : 1;                              /* 13479-13481 */
        beam_anim(b);
    }
    EACH(OT_THG_BEAM, b) beam_length(b);                                          /* 13484 (positions / angles of the last frame) */
    int k[5] = { 0 };
    EACH(OT_THG_BEAM, b) {                                                        /* 13485 */
        if (b->par < 0 || b->par > 4) continue;
        obj_t *hd = beam_head(b, k[b->par]++);
        if (!hd) continue;
        double ax, ay; act_pt(hd, &ax, &ay);
        b->x = (float)(int)ax; b->y = (float)(int)ay;
        if (b->par == 0) b->ang = (float)hd->av[1];
    }
    EACH(OT_THG_BEAM, b) {                                                        /* 13486: dust at the tip of a burning beam on screen */
        if (b->dir != 1 || b->av[0] <= 0) continue;
        double D = b->av[3], E = b->av[4];
        if (D <= G.camx - 20 || D >= G.camx + SCREEN_W + 20 || E <= G.camy - 20 || E >= G.camy + SCREEN_H + 20) continue;
        obj_effect((float)floor(b->x + b->av[0] * c_cos(b->ang)), (float)floor(b->y - b->av[0] * c_sin(b->ang)), "dust");
    }
    if (p->state != ST_HURT) S.hurt0 = -1;
    EACH(OT_THG_BEAM, b) if (near_ext(b, 600)) { beam_hurt(b, p); th_enemy_totem_zap(b); }   /* 13490-13493; 13488: a beam on a totem (th_enemy.c) */
    EACH(OT_THG_BEAM, b) if (b->par >= 1) b->ang = BEAM_ANG[b->par];             /* 13541 */
    heads_limit();                                                                /* 13542-13545 */
}
void th_gim_scan_off(void) {                          /* 13513: every scan laser and head destroyed */
    obj_t *o;
    EACH(OT_THG_BEAM, o) if (!(o->fl & 4)) o->alive = 0;
    EACH(OT_THG_HEAD, o) o->alive = 0;
    S.off = 1;
}

/* ================================================================ 2. shurikens (TH2 13571-13573 / 13708, TH3 13725 / 14097)
 * base: av[0] A direction (deg), av[1] B speed (ctor 4); x += B cos A, y -= B sin A each frame; on a turn marker's exact
 * position A = (A + 270) mod 360 and one more step. shuriken: on the k-th base, angle + 16 a frame, Q20 armoured (13694:
 * flag 5), A 99; the character sensor on it: R-hurt 10 (13573). */
static void base_step(obj_t *o) {
    o->x = (float)(int)(o->av[1] * c_cos(o->av[0]) + (int)o->x);
    o->y = (float)(int)((int)o->y - c_sin(o->av[0]) * o->av[1]);
}
static void shurikens(player_t *p) {
    obj_t *b, *t, *s;
    EACH(OT_THG_SHBASE, b) base_step(b);                                          /* 13571 */
    EACH(OT_THG_SHBASE, b) {                                                      /* foreach_s_57 / 13708 */
        int hit = 0; EACH(OT_THG_SHTURN, t) if ((int)t->x == (int)b->x && (int)t->y == (int)b->y) { hit = 1; break; }
        if (!hit) continue;
        b->av[0] = fmod(b->av[0] + 270, 360);
        base_step(b);
    }
    int k = 0;
    EACH(OT_THG_SHUR, s) {                                                        /* 13572 */
        s->ang = (float)fmod(s->ang + 16, 360);
        int i = 0; EACH(OT_THG_SHBASE, b) if (i++ == k) { s->x = b->x; s->y = b->y; break; }
        k++;
    }
    if (!dead(p)) EACH(OT_THG_SHUR, s) if (csens_hit(p, s) && vulnerable(p)) { player_contact(p, 10); break; }   /* 13573 */
}

/* ================================================================ 3. the barrel ride (TH2 13574-13597 / foreach_brl_57 / 13688 / col_6_57,
 * TH3 13726-13729 + the shared 13584-13597)
 * barrel: av[0] F x speed, av[1] G y speed, av[2] I (0 resting, 1 ridden, 2 rolling free), av[3] D / av[4] E position (start: x, y),
 * av[5] J enemy-hit cooldown, av[6] K rolled distance (sound 264 every 40), av[7] L debris frame; fl 0x40 = flag 6 (breaking).
 * The player lands on it from above (M 98, ST_RIDE here): X = D, Y = E - 54, steered by Left / Right (F -/+ 0.15, max 15). */
#define FBR6 0x40
static int test_obst(double x, double y) { return px_solid(&G.lvl, (int)x, (int)y); }   /* Frame::test_obstacle (background) */
static void shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }
static void ride_end(player_t *p, int jumped) {
    S.ride = 0; S.ride_obj = -1;
    p->state = ST_NORMAL; p->state_t = 0; p->ground = 0; p->N = p->O = 0;
    if (!jumped) { p->vy = 0; p->m1 = 0; p->jumping = 0; }                              /* M 0, T = S = 1 */
}
static void barrel_hit_enemies(obj_t *b) {            /* foreach_brl_57 (13557): one random Q20 enemy on a fast barrel */
    static short cand[32]; int n = 0;
    for (int i = 0; i < G.nobjs && n < 32; i++) {
        obj_t *e = &G.o[i];
        if (!e->alive || !e->an.b || !enemy_type(e->type) || e->c > 0 || (e->fl & 0x60) || e->hidden) continue;
        if (fabsf(e->x - b->x) > 120 || fabsf(e->y - b->y) > 120 || !obj_obj_hit(b, e)) continue;
        cand[n++] = (short)i;
    }
    if (!n) return;
    obj_t *e = &G.o[cand[n > 1 ? cross_rand(n) : (cross_rand(1), 0)]];
    e->c = 5; e->kx = b->av[0]; e->ky = b->av[1]; e->A -= fabs(b->av[0]) / 3 + 3; e->inv = 1;   /* C 5, X = F, Y = G, A - (|F| / 3 + 3), shader 21 */
    snd_play_orig(24, 220, 128);
    b->av[5] = 5;                                                                     /* J 5 */
}
static void barrels(player_t *p) {
    obj_t *b;
    EACH(OT_THG_BARREL, b) {                                                          /* 13574 */
        if (b->av[2] < 1 || b->av[5] > 2 || !obj_near_view(b, 96)) continue;        /* I >= 1, J <= 2 (RV th2b: 2 still frames after each enemy hit) */
        double *F = &b->av[0], *Gv = &b->av[1], *D = &b->av[3], *E = &b->av[4];
        *D += *F; b->x = (float)(int)*D; *E += *Gv; b->y = (float)(int)*E;
        *F = fmax(fmin(*F, 15), -15);                                                 /* F + H (H stays 0), clamped */
        b->ang = (float)fmod(b->ang - 1.5 * *F + 3600, 360);
        b->av[6] += fabs(0.5 * *F);
        /* foreach_brl_57: pushed out of the floor (6 tries of 2 px), gravity, walls */
        for (int k = 0; k < 6; k++) if (test_obst(*D, 38 + *E)) { *Gv = 0; *E -= 2; b->y -= 2; }
        if (!test_obst(*D, 39 + *E)) *Gv = fmin(*Gv + 0.5, 15);
        if (test_obst(*D - 38, 8 + *E) && *F < 0) { *F *= -0.5; snd_play_orig(51, 220, 128); }
        if (test_obst(*D + 38, 8 + *E) && *F > 0) { *F *= -0.5; snd_play_orig(51, 220, 128); }
        if (fabs(*F) > 2 && b->av[5] <= 0) barrel_hit_enemies(b);                     /* 13557 */
    }
    EACH(OT_THG_BARREL, b) if (b->av[6] > 40) { b->av[6] = 0; snd_play_orig(264, 220, 128); }   /* 13575 */
    int M = player_orig_m(p);
    if (S.ride == 0 && M != 98 && p->vy > 0 && !p->ground && !dead(p))              /* 13578 / 13579: landing from above */
        EACH(OT_THG_BARREL, b) {
            if (b->av[2] == 1 || (b->fl & FBR6) || !sens_hit_bb(b) || !(p->y - ORIG_REF_DY < b->av[4] - 48)) continue;
            b->av[2] = 1; b->av[0] = p->ground ? p->gsp : p->vx;
            S.ride = 1; S.ride_obj = (int)(b - G.o); p->state = ST_RIDE; p->state_t = 0; p->vy = 0;
            break;
        }
    b = S.ride ? &G.o[S.ride_obj] : NULL;
    if (b && (!b->alive || b->type != OT_THG_BARREL)) { ride_end(p, 0); b = NULL; }
    if (b) {                                                                          /* 13580: on the barrel */
        p->x = b->av[3]; p->y = b->av[4] - 54 + ORIG_REF_DY; p->vy = 0; p->ground = 0; p->N = p->O = 0;
        player_sync_sensors(p);
        shake((float)((fabs(b->av[0]) - 5) * 0.2));
        double aF = fabs(b->av[0]);
        if (aF > 2 || (S.held & (BTN_LEFT | BTN_RIGHT))) player_pose(p, 40, p->face > 0 ? 14 : 30, (int)(aF * 3 + 14));   /* 13581 / 13582 */
        else player_pose(p, 0, p->face > 0 ? 0 : 16, -1);                            /* 13583: OTHERS.Y 99, the idle animation */
        if (S.held & BTN_RIGHT) { b->av[0] = fmin(b->av[0] + 0.15, 15); p->face = 1; }   /* 13584 */
        if (S.held & BTN_LEFT) { b->av[0] = fmax(b->av[0] - 0.15, -15); p->face = -1; }  /* 13585 */
    }
    EACH(OT_THG_BARREL, b) {                                                          /* 13586 / 13587: friction while J <= 2 */
        if (b->av[2] < 1 || b->av[5] > 2) continue;
        if (b->av[0] > 0) b->av[0] = fmax(b->av[0] - 0.05, 0); else if (b->av[0] < 0) b->av[0] = fmin(b->av[0] + 0.05, 0);
    }
    EACH(OT_THG_BARREL, b) {                                                          /* 13588: a YellowSpringUp throws it up */
        if (b->av[2] < 1) continue;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *s = &G.o[i];
            if (!s->alive || s->type != OT_SPRING || fabsf(s->x - b->x) > 96 || fabsf(s->y - b->y) > 96 || !obj_obj_hit(b, s)) continue;
            b->av[1] = -11; anim_restart(&s->an); obj_effect(s->x, s->y, "thg_sbeam");
        }
    }
    EACH(OT_THG_BARREL, b) if (b->av[5] > 0) b->av[5] -= 1;                          /* 13589 */
    b = S.ride ? &G.o[S.ride_obj] : NULL;
    if (b && (S.pressed & BTN_JUMP)) {                                                /* 13591: jump off (N = 0): G = -jump, B = F */
        ride_end(p, 1);
        p->vy = -PHYS_JUMP; p->vx = b->av[0]; p->jumping = 1; p->m1 = 1;
        snd_play_orig(8, 220, 128);
        b = NULL;
    }
    if (b && (sens_on_bg_side(SENS_LEFTBIG) || sens_on_bg_side(SENS_RIGHTBIG))) { ride_end(p, 0); b = NULL; }   /* 13592 / 13593 */
    if (b && (b->fl & FBR6)) { ride_end(p, 0); b = NULL; }                           /* 13594 */
    EACH(OT_THG_BARREL, b) if (b->av[2] == 1 && !(S.ride && S.ride_obj == (int)(b - G.o))) b->av[2] = 2;   /* 13595 */
    EACH(OT_THG_BARREL, b) if (b->fl & FBR6) {                                        /* 13596 / 13597: breaks into 14 pieces */
        obj_t *d = spawn(OT_THG_BDEBRIS, "thg_bdebris", b->x, b->y, b->layer);
        if (d) { d->an.frame = (int)b->av[7]; d->an.acc = 0; cross_rand(15); obj_pinball_start(d, 1 + cross_rand(15), 50); }   /* ctor: direction pick_random(1..15) twice, PinballMovement 50, gravity 25 */
        b->av[7] += 1;
        if (b->av[7] >= 14) b->alive = 0;
    }
    obj_t *d;
    EACH(OT_THG_BDEBRIS, d) {
        obj_pinball_step(d, 25);
        if (!obj_near_view(d, 64)) d->alive = 0;                                      /* removal INFERRED */
    }
}
void th_gim_barrel_break(obj_t *b);
void th_gim_barrels_hit_by(const obj_t *h) {           /* 13688: a flying RocketPuncher fist on barrels (th_enemy.c calls it) */
    obj_t *b; EACH(OT_THG_BARREL, b) if (fabsf(b->x - h->x) < 96 && fabsf(b->y - h->y) < 96 && obj_obj_hit(h, b)) th_gim_barrel_break(b);
}
void th_gim_barrel_break(obj_t *b) { if (b && b->type == OT_THG_BARREL && !(b->fl & FBR6)) { b->fl |= FBR6; snd_play_orig(278, 220, 128); snd_play_orig(311, 220, 128); } }   /* 13688 */

/* ================================================================ 4. pillar flames, backlights */
static void pillar_flames(void) {                     /* 13571: the k-th flame 64 px under the k-th pillar */
    obj_t *f, *pl; int k = 0;
    EACH(OT_THG_PFLAME, f) {
        int i = 0; EACH(OT_THG_PILLAR, pl) if (i++ == k) { f->x = pl->x; f->y = pl->y + 64; break; }
        k++;
    }
}
/* THBacklight1 / THFlame are th_flow.c's (types 1536 / 1537; merge, session 21b): the enums 1624 / 1630 stay unused */

/* ================================================================ spawn */
void th_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS); S.ride_obj = -1; S.hurt0 = -1;
    objects_plat_type(OT_THG_PILLAR, 2, 64);                                      /* LevelHideoutPlatform1: F 2, G 64, H 1 (RE ctor) */
    if (!objects_have_from(RT0)) return;
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (o->alive && o->type == OT_LIMITER && S.nlim < 64) S.lim[S.nlim++] = (short)i;
        if (!o->alive || o->type < RT0 || o->type > OT_THG_END) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0x7f;
        if (o->type != OT_THG_PILLAR) { o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = 0; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av); }
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        lst_add(o); S.on = 1;
    }
    if (!S.on) return;
    obj_t *o;
    static const char *const HN[6] = { "thg_head", "", "thg_head2", "thg_head3", "thg_head4", "thg_head5" };
    static const signed char HD[6] = { 0, 0, 8, 24, 0, 16 };                     /* ctor directions (Head2 8, Head3 24, Head4 0, Head5 16) */
    EACH(OT_THG_HEAD, o) { int m = o->par >= 0 && o->par <= 5 ? o->par : 0; set_s(o, HN[m]); o->dir = HD[m]; }
    EACH(OT_THG_BEAM, o) { o->av[0] = 10; o->dir = 0; beam_anim(o); o->hidden = 1; o->av[7] = -1; }   /* ctor A 10; drawn by th_gim_draw */
    /* pillars: E = selection index * 180 mod 360 (13694 / 14091, newest first = the last placed is index 0) */
    { int n = N(OT_THG_PILLAR), i = 0;
      EACH(OT_THG_PILLAR, o) { if (!o->an.b) set_s(o, "thg_pillar"); o->vx = (float)fmod((n - 1 - i) * 180., 360.); o->vy = 0; o->fx = o->oy; o->fl = 0; i++; } }   /* 3565: I = X, J = Y */
    EACH(OT_THG_PFLAME, o) set_s(o, "thg_pflame");
    EACH(OT_THG_SHBASE, o) { set_s(o, "thg_shbase"); o->av[1] = 4; }             /* ctor B 4 */
    EACH(OT_THG_SHTURN, o) { o->hidden = 1; o->av[0] = o->x; o->av[1] = o->y; }   /* ctor invisible; 13694: A, B = x, y */
    EACH(OT_THG_SHUR, o) { set_s(o, "thg_shur"); o->A = 99; o->hp = 99; o->fl |= 0x20; }   /* ctor A 99; 13694: flag 5 (armoured) */
    /* 14097 (TH3's start events): bases left of the CameraLimitBossRight stay (B 0). TH2 has no such event; its only
     * CameraLimitBossRight is the off-map template at (2424, -192), so "an on-map limit" selects the act (RV th2a: TH2 bases move) */
    { obj_t *cl = NULL; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_CAMLIM_R && G.o[i].x >= 0 && G.o[i].y >= 0) { cl = &G.o[i]; break; }
      if (cl) EACH(OT_THG_SHBASE, o) if (o->x < cl->x) o->av[1] = 0; }
    EACH(OT_THG_BARREL, o) { set_s(o, "thg_barrel"); o->av[3] = o->x; o->av[4] = o->y; }   /* start events (TH2 on_frame_58_start, TH3 59): D, E = x, y */
}

/* ================================================================ passes */
void th_gim_pre(player_t *p) { (void)p; }
int  th_gim_obj(obj_t *o, player_t *p) { (void)p; return o->type >= RT0 && o->type <= OT_THG_END; }

void th_gim_frame(player_t *p) {
    if (!S.on) return;
    if (N(OT_THG_HEAD) || N(OT_THG_BEAM)) { heads_move(); heads_bounce(); }
    if (N(OT_THG_BEAM)) lasers(p);
    if (N(OT_THG_SHBASE) || N(OT_THG_PFLAME)) { if (N(OT_THG_SHBASE)) shurikens(p); pillar_flames(); }
    if (N(OT_THG_BARREL) || N(OT_THG_BDEBRIS)) barrels(p);
    S.pressed = 0;
}
void th_gim_draw(int pass) {
    if (!S.on || pass != 1) return;
    obj_t *o;
    EACH(OT_THG_BEAM, o) th_gim_beam_draw(o);
}
int  th_gim_q20(int type) { return type == OT_THG_SHUR; }
/* ST_RIDE (player.c): on the barrel (M 98) the player block does not move her (13580 places her); this frame's input is kept
 * for 13582-13585 / 13591, which run in th_gim_frame. Returns 0 when no barrel ride is on (another module's ride). */
int th_gim_player(player_t *p, const input_t *in) {
    if (!S.ride) return 0;
    S.pressed = in->pressed; S.held = in->held; p->held = in->held;
    if (in->held & BTN_RIGHT) p->face = 1;                                            /* the facing events still run (RV th2b: Q -1 on Left) */
    if (in->held & BTN_LEFT) p->face = -1;
    return 1;
}
int th_gim_ride_m(void) { return S.ride ? 98 : -1; }
