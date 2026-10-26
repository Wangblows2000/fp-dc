/* SB1 final boss: the Kujacker (BossKujackerBase_2118 holds the state; Head_2125 is the weak point (Q20, one hit), Head2_2119
 * the neck, Wing1_2122 / Wing2_2123, Leg1 / Leg2, Pilot_2124, nine Tail_2115 feathers with their invisible TailTarget_2120
 * hit boxes (Q20, 3 HP), BossTornado_2131), its BossBolt_2129, BossLightning2_2132 rings, EffectsGust / Gust2 / Dust01, and
 * the EffectWarning_1528 markers of its dash.
 * RE handle_frame_36_events group 0x22840 (on at 9009 = sb_flow_night() 2): 9010-9166 (+ the global 9167-9175 for its
 * effects), loop_lightning_35, foreach_wings1 / 2; RV traces k_kill (idle, blown off by the tornado), k_def (three head hits)
 * and k_end (five head hits, the end) (build/orig_traces/sb_boss). spec_sb_bosses Part A.
 * Base alterables: B wing / bob phase, C wing sweep 60, D tilt, E / F position, G / H velocity, I mode (0 hover, 1 charge,
 * 2 lightning, 3 bolt, 4 dash, -1 / -2 tornado), J bob amplitude 5, K dash direction, L dash turn 4, M timer, N dash passes,
 * O horizontal intent, P vertical intent (0 here), Q attack request (1 beam, 2 bolt, 3 dash, 4 tornado), R attack sequence,
 * S head hits, T beam timer; flags 4 sound latch, 8 tornado latch, 0x10 turning, 0x20 sequence toggle.
 * Pilot C: the end sequence (1-59 slow motion, 60-149 explosions, 150 flash, 151-154 the fly-away); results at its end. */
#include "sb_boss.h"
#include <stdio.h>
#include <stdlib.h>

enum { K_BASE, K_BASE7, K_HEAD, K_HEAD_L, K_HEAD7, K_HEAD7_L, K_HEAD8, K_HEAD11, K_NECK, K_W1, K_W2, K_LEG1, K_LEG1_7, K_LEG1_7L,
       K_LEG2, K_LEG2_7, K_LEG2_7L, K_TAIL, K_TAIL10, K_TT, K_PILOT, K_PILOT7, K_PILOT7L, K_TORNADO, K_BOLT, K_LIGHT2, K_LIGHT2V,
       K_BEAM, K_GUST, K_GUST2,  K_WARN, K_N };
static const char *const KN[K_N] = { "sbk_base", "sbk_base7", "sbk_head", "sbk_head_l", "sbk_head7", "sbk_head7_l", "sbk_head8", "sbk_head11",
    "sbk_neck", "sbk_w1", "sbk_w2", "sbk_leg1", "sbk_leg1_7", "sbk_leg1_7l", "sbk_leg2", "sbk_leg2_7", "sbk_leg2_7l", "sbk_tail", "sbk_tail10",
    "sbk_tt", "sbk_pilot", "sbk_pilot7", "sbk_pilot7_l", "sbk_tornado", "sbk_bolt", "sbk_light2", "sbk_light2v", "sbk_beam", "sbk_gust",
    "sbk_gust2",  "sbk_warn" };
static short KA[K_N];
static void ka(obj_t *o, int k) { if (o && KA[k] >= 0 && (o->an.b != &G.ens || o->an.anim != KA[k])) anim_set(&o->an, &G.ens, KA[k]); }

#define NHIST 16
#define NFX 160
#define F4 4
#define F8 8
#define F10 0x10
#define F20 0x20
static struct {
    int on, night, once272, ended, fx_n, warn, slow0, placed;
    double B, C, D, E, F, G, H, I, J, K, L, M, N, O, Q, R, S, T; int fl;
    double pC, pD;                            /* pilot C / D (the end) */
    double tA;                                /* tornado A (the push) */
    int hit;                                  /* head flag 0x80: this hit is counted */
    double hx[NHIST], hy[NHIST], hd[NHIST];   /* BossTailTracker trail of the dash (age 0 = newest) */
    int ring_t;                               /* frames of the current lightning-ring burst */
    short fx[NFX];                            /* runtime objects (bolts, rings, gusts, dust, warnings) */
} K;

static obj_t *base(void) { return sbb_get(OT_SB_KJ_BASE, 0); }
static obj_t *head(void) { return sbb_get(OT_SB_KJ_HEAD, 0); }
static obj_t *tornado(void) { return sbb_get(OT_SB_TORNADO, 0); }
static int npart(int t) { return sbb_count(t); }

void sbk_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&K, sizeof K); }
    memset(&K, 0, sizeof K);
    obj_t *b = base();
    if (!b) return;
    for (int k = 0; k < K_N; k++) KA[k] = (short)ens_anim(KN[k]);
    K.C = 60; K.J = 5; K.L = 4; K.K = -1; K.E = b->x; K.F = b->y; K.warn = -1; K.placed = 1;   /* RV k_def f0: C 60, J 5, L 4, K -1 */
    static const int TY[] = { OT_SB_KJ_BASE, OT_SB_KJ_HEAD, OT_SB_KJ_HEAD2, OT_SB_KJ_LEG1, OT_SB_KJ_LEG2, OT_SB_KJ_PILOT, OT_SB_KJ_TAIL,
                              OT_SB_KJ_TARGET, OT_SB_KJ_WING1, OT_SB_KJ_WING2, OT_SB_KJ_CHARGE };
    for (unsigned j = 0; j < sizeof TY / sizeof TY[0]; j++)
        for (int k = 0; k < npart(TY[j]); k++) {
            obj_t *o = sbb_get(TY[j], k); if (!o) continue;
            o->hidden = 1; o->av[0] = npart(TY[j]) - 1 - k;                    /* A: index from the last created */
            if (TY[j] == OT_SB_KJ_TAIL) o->av[1] = (k - 4) * 20.0;              /* B: the fan angle (RV k_end: -80 .. 80 left to right) */
        }
    obj_t *h = head(); if (h) { h->A = 1; h->fl |= F_NOFLY; }
    for (int k = 0; k < npart(OT_SB_KJ_TARGET); k++) { obj_t *o = sbb_get(OT_SB_KJ_TARGET, k); if (o) { o->A = 3; o->fl |= F_NOFLY; } }
    obj_t *t = tornado(); if (t) t->hidden = 1;
}
void sbk_pre(player_t *p) { (void)p; }

/* ---- runtime objects */
static obj_t *fx_new(int type, int k, float x, float y, int layer) {
    if (K.fx_n >= NFX) return NULL;
    obj_t *o = sbb_spawn(type, -1, x, y, layer);
    if (!o) return NULL;
    ka(o, k);
    K.fx[K.fx_n++] = (short)(o - G.o);
    return o;
}
static void fx_kill(int k) { obj_t *o = sbb_slot(K.fx[k]); if (o) o->alive = 0; K.fx[k] = K.fx[--K.fx_n]; }
static void ring(const obj_t *b, int strike) {                                  /* loop_lightning_35: 16 spokes, A = 22.5 k */
    for (int k = 0; k < 16; k++) {
        obj_t *o = fx_new(OT_SB_KJ_LIGHTNING, strike ? K_LIGHT2V : K_LIGHT2, b->x, b->y, b->layer);
        if (o) { o->av[0] = 22.5 * k; o->av[1] = 0; o->av[2] = K.E; o->av[3] = K.F; o->av[4] = strike; }
    }
}
static void fx_frame(player_t *p) {
    obj_t *t = tornado();
    for (int k = 0; k < K.fx_n; k++) {
        obj_t *o = sbb_slot(K.fx[k]);
        if (!o) { K.fx[k--] = K.fx[--K.fx_n]; continue; }
        switch (o->type) {
        case OT_SB_KJ_BOLT:                                                     /* 9172-9174: grows for 50 frames, then flies at the player */
            if (o->av[0] < 100) { o->av[0] = fmin(o->av[0] + 2, 100); o->scale = (float)(0.01 * o->av[0]); }
            else if (o->av[0] == 100) {
                double a = atan2(-(sbb_py(p) - o->y), sbb_px(p) - o->x) * SB_RAD; if (a < 0) a += 360;
                int dir = (int)(a / 11.25); o->av[1] = dir * 11.25; o->av[0] = 101;   /* direction of 32, speed 50 (6.25 px) */
            }
            if (o->av[0] > 100) {
                o->fx += (float)(6.25 * sbb_cos(o->av[1])); o->fy -= (float)(6.25 * sbb_sin(o->av[1]));
                o->x = o->ox + o->fx; o->y = o->oy + o->fy;
                if (K.pC <= 0 && sbb_csens_hit(p, o) && sbb_vulnerable(p)) sbb_hurt(p, 10);
                if (o->x < G.camx - 600 || o->x > G.camx + SCREEN_W + 600 || o->y < G.camy - 600 || o->y > G.camy + SCREEN_H + 600) { fx_kill(k--); continue; }
            }
            break;
        case OT_SB_KJ_LIGHTNING:                                                /* rings: x = E + cos A * B, y = F - sin A * B, B + 62 */
            o->x = (float)(int)(o->av[2] + sbb_cos(o->av[0]) * o->av[1]); o->y = (float)(int)(o->av[3] - sbb_sin(o->av[0]) * o->av[1]);
            o->ang = o->av[4] ? 0 : (float)o->av[0];
            o->av[1] += 62;
            if (o->av[4] && sbb_csens_hit(p, o) && sbb_vulnerable(p) && sbb_hurt(p, 20)) snd_play_orig(371, 220, 128);   /* 9075 */
            if (o->av[1] > 2400 || o->x < G.camx - 64 || o->x > G.camx + SCREEN_W + 64 || o->y < G.camy - 64 || o->y > G.camy + SCREEN_H + 64) { fx_kill(k--); continue; }   /* 9170 B > 2400 (and off screen: they only go outwards) */
            break;
        case OT_SB_KJ_GUST:                                                     /* 9104: spiral up the tornado */
            if (t) {
                double A = o->av[0];
                o->y = (float)(int)(t->y - 4 * A); o->x = (float)(int)((0.5 * A + 20) * sbb_sin(12 * G.frame - 10 * A) + t->x);
                o->scale = (float)fmin((5 + A) * 0.05, 1.5);
            }
            if (o->an.done) { fx_kill(k--); continue; }
            break;
        case OT_SB_KJ_GUST2:                                                    /* 9167 / 9168 */
            o->x = (float)(int)(o->x + o->av[0]); o->scale = (float)(1 - o->av[1]); o->av[1] += 0.05;
            if (o->av[1] >= 1) { fx_kill(k--); continue; }
            break;
        }
    }
}

/* ---- the parts (9116 / 9117 / 9120 / 9121 / 9124-9140; fits of the RV traces where the neck chain is not decoded) */
static int wall_at(int sens) {                                                  /* the player's big side sensor on solid background */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(sens, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) for (int x = ox + a[j]; x <= ox + b[j]; x++) if (px_solid(&G.lvl, x, oy + j)) return 1;
    return 0;
}
static void rot(double D, double lx, double ly, double *x, double *y) {        /* Active angle D (degrees, counter-clockwise) */
    *x = lx * sbb_cos(D) + ly * sbb_sin(D); *y = -lx * sbb_sin(D) + ly * sbb_cos(D);
}
static void place_parts(void) {
    obj_t *b = base(), *h = head(), *pl = sbb_get(OT_SB_KJ_PILOT, 0), *l1 = sbb_get(OT_SB_KJ_LEG1, 0), *l2 = sbb_get(OT_SB_KJ_LEG2, 0);
    if (!b) return;
    double bx, by, D, dx, dy;
    int dash = K.I == 4;
    if (!dash) {
        bx = (int)K.E; by = (int)((0.5 * K.J - 12.5) * sbb_sin(K.B) + K.F); D = K.D;   /* 9120 */
    } else { bx = (int)K.hx[6]; by = (int)K.hy[6]; D = K.hd[6]; }                     /* 9131 / 9132: the tracker of age 6 */
    b->x = (float)bx; b->y = (float)by; b->ang = (float)D;
    if (pl) { pl->x = b->x; pl->y = b->y; pl->ang = (float)D; }
    if (!dash) {
        if (h) {                                                                /* head: RV fit (k_end, |err| <= 15 px) */
            double ly = -13.35 - 14.34 * sbb_sin(K.B), lx = -0.66 + 2.48 * sbb_sin(K.B);
            rot(-D, lx, ly, &dx, &dy); h->x = (float)(int)(bx + dx); h->y = (float)(int)(by + dy); h->ang = (float)D;
        }
        if (l1) { rot(D, -26.5, 24.5, &dx, &dy); l1->x = (float)(int)(bx + dx); l1->y = (float)(int)(by + dy); l1->ang = (float)(D - 10 + 10 * sbb_sin(K.B)); }   /* 9116 angles */
        if (l2) { rot(D, 21, 24.5, &dx, &dy); l2->x = (float)(int)(bx + dx); l2->y = (float)(int)(by + dy); l2->ang = (float)(D + 10 - 10 * sbb_sin(K.B)); }
        for (int s = 0; s < 2; s++) {                                           /* foreach_wings1 / 2 */
            int ty = s ? OT_SB_KJ_WING2 : OT_SB_KJ_WING1;
            for (int k = 0; k < npart(ty); k++) {
                obj_t *w = sbb_get(ty, k); if (!w) continue;
                double A = w->av[0], wB = sbb_sin(K.B - 10 * A) * K.C;
                if (!s) { w->x = (float)(int)(bx - 40 * A * sbb_cos(wB + D) - 32 * sbb_cos(D)); w->y = (float)(int)(by + 40 * A * sbb_sin(wB + D) + 32 * sbb_sin(D));
                          w->ang = (float)(sbb_sin(K.B - 30 - 10 * A) * K.C + D); }
                else    { w->x = (float)(int)(bx + 40 * A * sbb_cos(wB - D) + 32 * sbb_cos(D)); w->y = (float)(int)(by + 40 * A * sbb_sin(wB - D) - 32 * sbb_sin(D));
                          w->ang = (float)(D - sbb_sin(K.B - 30 - 10 * A) * K.C); }   /* (mirror of wings1: INFERRED) */
                w->hidden = 0;
            }
        }
        for (int k = 0; k < npart(OT_SB_KJ_HEAD2); k++) {                      /* the neck: between the base and the head (INFERRED) */
            obj_t *n = sbb_get(OT_SB_KJ_HEAD2, k); if (!n || !h) continue;
            double f = (n->av[0] + 1) / (npart(OT_SB_KJ_HEAD2) + 1);
            n->x = (float)(int)(bx + (h->x - bx) * f); n->y = (float)(int)(by + (h->y - by) * f);
        }
    } else {
        if (h) { h->x = (float)(int)K.E; h->y = (float)(int)K.F; h->ang = (float)K.D; }   /* 9124 */
        for (int k = 0; k < npart(OT_SB_KJ_HEAD2); k++) {                      /* 9125-9130: trackers 1-5 */
            obj_t *n = sbb_get(OT_SB_KJ_HEAD2, k); if (!n) continue;
            int a = 1 + (int)n->av[0]; if (a > 5) a = 5;
            n->x = (float)(int)K.hx[a]; n->y = (float)(int)K.hy[a];
        }
        if (l1) { l1->x = b->x; l1->y = b->y; l1->ang = (float)D; }
        if (l2) { l2->x = b->x; l2->y = b->y; l2->ang = (float)D; }
        for (int s = 0; s < 2; s++) {                                           /* 9133-9140: wings on trackers 7 / 9 / 11 / 13 */
            int ty = s ? OT_SB_KJ_WING2 : OT_SB_KJ_WING1;
            for (int k = 0; k < npart(ty); k++) {
                obj_t *w = sbb_get(ty, k); if (!w) continue;
                int a = 7 + 2 * (int)w->av[0]; if (a >= NHIST) a = NHIST - 1;
                w->x = (float)(int)K.hx[a]; w->y = (float)(int)K.hy[a]; w->ang = (float)K.hd[a];
                w->hidden = (K.K < 0) != (s == 1);                              /* 9118 / 9119: one wing set */
            }
        }
    }
    for (int k = 0; k < npart(OT_SB_KJ_TAIL); k++) {                           /* tails at the base: 9116 / 9117 / 9131 / 9132 */
        obj_t *t = sbb_get(OT_SB_KJ_TAIL, k); if (!t) continue;
        double a;
        if (dash) a = K.K < 0 ? 0.25 * t->av[1] + D - 80 : -0.25 * t->av[1] + D - 300;
        else if (K.I >= 1 && K.I <= 3) a = (sbb_sin(K.B) * K.C * 0.005 + 1) * t->av[1] + D;
        else a = (sbb_sin(K.B - 60) * K.C * 0.004 + 1) * t->av[1] + D;
        t->x = b->x; t->y = b->y; t->ang = (float)a;
        obj_t *tt = sbb_get(OT_SB_KJ_TARGET, k);                               /* its target at the feather tip (action point 0, -93; RV) */
        if (tt) { tt->x = (float)(int)(b->x - 93 * sbb_sin(a)); tt->y = (float)(int)(b->y - 93 * sbb_cos(a)); }
    }
}
static void show_all(int on) {
    static const int TY[] = { OT_SB_KJ_BASE, OT_SB_KJ_HEAD, OT_SB_KJ_HEAD2, OT_SB_KJ_LEG1, OT_SB_KJ_LEG2, OT_SB_KJ_PILOT, OT_SB_KJ_TAIL,
                              OT_SB_KJ_WING1, OT_SB_KJ_WING2 };
    for (unsigned j = 0; j < sizeof TY / sizeof TY[0]; j++) for (int k = 0; k < npart(TY[j]); k++) { obj_t *o = sbb_get(TY[j], k); if (o) o->hidden = !on; }
}
static void poses(void) {
    int dash = K.I == 4, r = K.K > 0;
    ka(base(), dash ? K_BASE7 : K_BASE);
    obj_t *h = head();
    if (h) ka(h, dash ? (r ? K_HEAD7 : K_HEAD7_L) : (K.I >= 1 && K.I <= 3) || K.I < 0 ? K_HEAD11 : K.hit ? K_HEAD8 : K_HEAD);
    ka(sbb_get(OT_SB_KJ_LEG1, 0), dash ? (r ? K_LEG1_7 : K_LEG1_7L) : K_LEG1);
    ka(sbb_get(OT_SB_KJ_LEG2, 0), dash ? (r ? K_LEG2_7 : K_LEG2_7L) : K_LEG2);
    ka(sbb_get(OT_SB_KJ_PILOT, 0), dash ? (r ? K_PILOT7 : K_PILOT7L) : K_PILOT);
    for (int k = 0; k < npart(OT_SB_KJ_TAIL); k++) ka(sbb_get(OT_SB_KJ_TAIL, k), K_TAIL);
    for (int k = 0; k < npart(OT_SB_KJ_HEAD2); k++) ka(sbb_get(OT_SB_KJ_HEAD2, k), K_NECK);
    for (int k = 0; k < npart(OT_SB_KJ_WING1); k++) ka(sbb_get(OT_SB_KJ_WING1, k), K_W1);
    for (int k = 0; k < npart(OT_SB_KJ_WING2); k++) ka(sbb_get(OT_SB_KJ_WING2, k), K_W2);
    for (int k = 0; k < npart(OT_SB_KJ_TARGET); k++) ka(sbb_get(OT_SB_KJ_TARGET, k), K_TT);
}

/* ---- the fight */
static void hover(player_t *p) {                                               /* 9023-9042 (I 0) */
    double PX = sbb_px(p);
    obj_t *t = tornado(); double TY = t ? t->y : 1208;
    if (K.O == 1 && K.G < 10) K.G += 0.2;                                       /* 9023 / 9024 */
    if (K.O == -1 && K.G > -10) K.G -= 0.2;
    if (K.O == 0 && K.G < 0) K.G += 0.2;                                        /* 9029 / 9030 */
    if (K.O == 0 && K.G > 0) K.G -= 0.2;
    K.E += K.G; K.F += K.H; K.D = fmax(fmin(-5 * K.G, 45), -45);               /* 9033 */
    if (K.S > 1 && K.Q != 3 && K.pC <= 0) K.T += 1;                             /* 9034 / 9035: the beam after two hits (RV k_end: T holds while
                                                                                 * flying off (Q 3) and from the final hit on) */
    if (K.T > 30) { K.Q = 1; K.T = -500; }
    if (K.O == 0 && K.Q == 0 && K.M > -999) K.O = 1;                            /* 9036-9038: chase the player */
    if (K.E < PX - 80 && K.D > 0 && K.Q == 0 && K.M > -999) { K.G += 0.2; K.O = 1; }
    if (K.E > PX + 80 && K.D < 0 && K.Q == 0 && K.M > -999) { K.G -= 0.2; K.O = -1; }
    double ty = K.E >= 1170 ? TY - 120 : TY - 280;                              /* 9039-9042: height above the tornado */
    if (ty < K.F) K.F -= 1;
    if (K.F < ty) K.F += 2;                                                     /* (separate events: both may run) */
}
static void attacks(player_t *p) {
    obj_t *b = base(), *h = head(), *t = tornado();
    double TY = t ? t->y : 1208;
    if (K.I <= 0 && K.C > 45 && !(K.fl & F4) && K.B > 300) { snd_play_orig(273, 220, 128); K.fl |= F4; }   /* 9044 / 9045: wing beat */
    if (K.I <= 0 && (K.fl & F4) && K.B < 90) K.fl &= ~F4;
    if (K.I == 0 && K.Q == 0) K.M += 1;                                         /* 9047 / 9048: the next attack */
    if (K.I == 0) {
        double thr = 20 * K.S <= 50 ? 200 - 20 * K.S : 150;
        if (K.M > thr) { K.R += 1; K.M = sbb_rand(70) - 40; }
    }
    if (K.R == 1) { K.Q = 2; K.R = (sbb_rand(2) ? 2 : 0) + 1 + 1; }            /* 9049-9053 */
    if (K.R == 3) { K.Q = 2; K.R = 4; }
    if (K.R == 5) { K.Q = 2; K.R = 6; }
    if (K.R == 7 && !(K.fl & F20)) { K.fl ^= F20; K.R = 0; K.Q = 4; }
    else if (K.R == 7) { K.fl ^= F20; K.Q = 4; K.R = (sbb_rand(2) + 1) * 2; }
    /* Q 1: the lightning beam (9054-9075) */
    if (K.Q == 1) { if (K.G < 0) { K.G = fmin(K.G + 0.4, 0); K.O = 0; } else if (K.G > 0) { K.G = fmax(K.G - 0.4, 0); K.O = 0; } }
    if (K.Q == 1 && !K.once272) { K.once272 = 1; snd_play_orig(272, 220, 128); }
    if (K.Q == 1 && K.G == 0) {
        K.I = 1; K.fl &= ~F4; K.Q = 0; K.B = 180; K.C = 60; K.J = 5;
        if (h) h->fl |= SB_ARM;
    }
    if (K.I == 1) {
        if (K.F > TY - 200) K.F -= 2;                                          /* 9058 / 9059 */
        K.E += K.G;
        if (K.B > 40 && K.B < 140 && !(K.fl & F4)) { snd_play_orig(271, 220, 128); K.fl |= F4; K.ring_t = 0; }   /* 9063 */
        if (K.B > 40 && K.B < 140) K.B -= (((G.frame + 1) % 5) * -3 + 9);     /* 9064: the charge (B jitters back; a global counter: phase RV k_def 1877-1883) */
        if ((K.fl & F4) && K.ring_t < 14 && b) { ring(b, 0); K.ring_t++; }     /* 9067 / loop_lightning_35 (14 rings: RV k_def 1877-1890) */
        if (K.B >= 170 && K.B < 180) { K.I = 2; K.fl &= ~F4; }                  /* 9070 */
    }
    if (K.I == 2) {
        if (K.B > 180 && K.B < 300) K.B -= 2;                                   /* 9071 */
        if (K.B > 180 && K.B < 300 && !(K.fl & F4)) {                           /* 9072: thunder (dark sky flash) */
            K.fl |= F4; snd_play_orig(270, 220, 128); if (G.cam.Z < 20) G.cam.Z = 20;
        }
        if ((K.fl & F4) && K.B > 180 && K.B < 300 && b && (G.frame & 1)) ring(b, 1);   /* 9067 / 9069: the strikes (every other frame: INFERRED cap) */
        if (K.B >= 0 && K.B < 20) { K.I = 0; K.fl &= ~F4; K.B = 200; if (h) h->fl &= ~SB_ARM; }   /* 9074 */
    }
    /* Q 2: the bolt (9076-9080) */
    if (K.Q == 2) { if (K.G < 0) { K.G = fmin(K.G + 0.2, 0); K.O = 0; } else if (K.G > 0) { K.G = fmax(K.G - 0.2, 0); K.O = 0; } }
    if (K.Q == 2 && K.G == 0) {
        K.I = 3; K.B = 180; K.C = 60; K.J = 5; K.Q = 0;
        if (b) { obj_t *o = fx_new(OT_SB_KJ_BOLT, K_BOLT, b->x, b->y + 80, b->layer); if (o) { o->av[0] = 0; o->scale = 0.01f; } }   /* RV: base + (0, 80) */
        snd_play_orig(274, 220, 128);
    }
    if (K.I == 3) { K.B += 1; if (K.B >= 160 && K.B < 180) { K.fl &= ~F8; K.I = 0; K.B = 0; } }
    /* Q 3: the dash (9081-9092) */
    if (K.Q == 3) { K.G += 0.2; K.O = 1; K.K = -1; }
    if (K.Q == 3 && K.E > G.camx + 926) { K.M = 0; K.B = 0; K.D = 0; K.I = 4; K.Q = 0; }
    if (K.I == 4) {
        if (K.M == 0) {                                                         /* 9083-9085: a pass starts at the warned height */
            K.M += 1;
            obj_t *w = sbb_slot(K.warn);
            double wy = w && w->type == OT_SB_WARNING ? w->y : K.placed ? 1192 : sbb_py(p);   /* (the first pass: SB1's three placed warnings, y 1192: RV F 1162) */
            K.placed = 0;
            K.E = K.K < 0 ? G.camx + 626 : G.camx - 200; K.F = wy - 30;
            snd_play_orig(K.K < 0 ? 268 : 269, 220, 128);
            if (w && w->type == OT_SB_WARNING) w->alive = 0;
            K.warn = -1;
        }
        K.G = 16 * K.K; K.E += sbb_cos(K.D) * K.G; K.F -= K.G * sbb_sin(K.D);  /* 9086 */
        if (h && sbb_vulnerable(p) && sbb_csens_hit(p, h)) {                    /* 9087: 10, knocked up */
            player_contact(p, 10); p->ground = 0; p->vy = -8; p->y -= 4; if (G.cam.Z < 6) G.cam.Z = 6; snd_play_orig(23, 220, 128);
        }
        if (!(K.fl & F10) && ((K.K < 0 && K.E <= sbb_px(p)) || (K.K > 0 && K.E >= sbb_px(p)))) K.fl |= F10;   /* 9088 */
        if (K.fl & F10) { K.M += 1; K.D = fmax(fmin(K.D + K.L, 90), -90); }    /* 9089 */
        if (K.N < 3 && K.M > 60 && K.warn < 0) {                                /* 9083: the next pass is marked at the player */
            obj_t *o = fx_new(OT_SB_WARNING, K_WARN, (float)sbb_px(p), (float)sbb_py(p), b ? b->layer : 2); if (o) K.warn = (int)(o - G.o);
        }
        if (K.M > 100) {                                                        /* 9090 */
            K.fl &= ~F10; K.M = 0; K.D = 0; K.K = -K.K; K.N += 1; K.L = (fmod(K.N, 2) * 2 - 1) * K.L;
        }
        if (K.N > 3) {                                                          /* 9091 / 9092: back over the ship */
            for (int k = 0; k < npart(OT_SB_KJ_TARGET); k++) { obj_t *o = sbb_get(OT_SB_KJ_TARGET, k); if (o) { o->fl &= ~SB_DEF; o->A = 3; } }
            K.E = sbb_px(p); K.G = 0; K.H = 10; K.I = 0; K.F = (tornado() ? tornado()->y : 1208) - 500;   /* (RV k_def 877: F 708) */
            K.N = 0; K.J = 5; K.hit = 0;
            if (h) { h->fl &= ~(SB_DEF | 0x80); h->A = 1; }
        }
    }
    if (K.Q == 3 && K.I != 4 && !K.placed && K.warn < 0 && K.E > sbb_px(p) + 320) {   /* 9083: flying off, the first pass is marked at the player */
        obj_t *o = fx_new(OT_SB_WARNING, K_WARN, (float)sbb_px(p), (float)sbb_py(p), b ? b->layer : 2); if (o) K.warn = (int)(o - G.o);   /* (E > PX + 320: RV k_def 1261 / 1262, 2169 / 2170) */
    }
    {   /* 9084 / 9085: the marker sits at the screen edge the next pass comes in from (RV k_def x 1107 / 1493 around the player at 1300: INFERRED camx + 19 / camx + W - 19) */
        obj_t *w = sbb_slot(K.warn);
        if (w && w->type == OT_SB_WARNING) { double side = (K.I == 4 && K.M > 0) ? -K.K : K.K; w->x = (float)(side < 0 ? G.camx + SCREEN_W - 19 : G.camx + 19); }
    }
    /* Q 4: the tornado (9093-9113) */
    if (K.Q == 4) { if (K.G < 0) { K.G = fmin(K.G + 0.4, 0); K.O = 0; } else if (K.G > 0) { K.G = fmax(K.G - 0.4, 0); K.O = 0; } }
    if (K.Q == 4 && K.G == 0) { K.M = 0; K.I = -1; K.Q = 0; K.fl &= ~F8; K.tA = 0; if (t && b) { t->x = b->x; } }
    if (K.I == -1 || K.I == -2) {
        if (K.I == -1) {
            if (K.F > TY - 200) K.F -= 2;                                       /* 9096-9099 */
            K.J = fmin(K.J + 0.1, 12);
            if (K.C < 90) K.C += 1;
            if (K.J >= 12 && !(K.fl & F8)) { snd_play_orig(267, 220, 128); K.fl |= F8; }
        }
        if (K.J >= 12 && t) {
            obj_t *o = fx_new(OT_SB_KJ_GUST, K_GUST, t->x, t->y, t->layer);   /* 9100 / 9101 */
            if (o) o->av[0] = sbb_rand((int)fmin(0.5 * K.M + 1, 30));
            K.M += 1;
            if (!(G.frame & 1)) {                                               /* 9102 / 9103: dust and streaks */
                obj_effect((float)(t->x - 10 + sbb_rand(21)), t->y, (G.frame & 2) ? "sbk_dust" : "sbk_dust_l");
                obj_t *g2 = fx_new(OT_SB_KJ_GUST2, K_GUST2, t->x, (float)(t->y - sbb_rand((int)fmin(1.4 * K.M, 120) + 1)), t->layer);
                if (g2) { g2->av[0] = (G.frame & 2) ? 15 + sbb_rand(8) : -15 - sbb_rand(8); g2->av[1] = fmax(0.8 - 0.025 * K.M, 0); }
            }
            if (K.tA != 0 && !wall_at(K.tA > 0 ? SENS_RIGHTBIG : SENS_LEFTBIG)) { p->x += K.tA; player_sync_sensors(p); }   /* 9106: MV.X + A (the push; a wall stops it) */
            if (sbb_px(p) < t->x) K.tA = fmax(K.tA - 0.2, -6); else K.tA = fmin(K.tA + 0.2, 6);   /* 9107 / 9108 (Milla's shield 9109 / 9110: not modelled) */
        }
        if (K.I == -1 && K.M > 250) { K.M = 0; K.I = -2; }                      /* 9105 */
        if (K.I == -2) {                                                        /* 9111-9113 */
            K.J = fmax(K.J - 0.1, 5); if (K.C > 60) K.C -= 1;
            if (K.J <= 5 && K.C <= 60) K.I = 0;
        }
    }
}
static void head_hit(void) {                                                   /* 9147: a hit on the head: the dash; 9149 the fifth */
    obj_t *h = head();
    if (!h || !(h->fl & SB_DEF) || K.hit) return;
    K.hit = 1; K.Q = 3; K.I = 0; K.C = 60; K.J = 5; K.M = 0; K.S += 1;
    snd_play_orig(47, 220, 128);
    if (K.S >= 5) {                                                             /* 9149: the end */
        music_stop(); K.Q = 0; K.O = 0; K.I = 0; K.M = -9999; K.pC = 1; K.slow0 = G.frame;
    }
}
static void ending(player_t *p) {                                              /* 9150-9166 */
    obj_t *b = base();
    if (K.pC > 0 && K.pC < 150 && b && ((K.pC < 60 && (G.frame - K.slow0) % 6 == 0) || (K.pC >= 60 && (int)K.pC % 5 == 3))) {   /* 9150 / 9151 (RV k_end: every
                                                                                 * 6 frames in the slow motion, then every 5, 3451-3536) */
        obj_effect((float)(b->x - 40 + sbb_rand(81)), (float)(b->y - 40 + sbb_rand(81)), "explosion06");
        static const int SX[3] = { 98, 99, 311 }; snd_play_orig(SX[sbb_rand(3)], 220, 128);
    }
    if (K.pC > 0 && K.pC < 150) {
        if (K.pC < 60) { if ((G.frame - K.slow0) % 6 == 0) K.pC += 1; }       /* slow motion: the group every 6th frame (RV k_end 3101-3449) */
        else K.pC += 1;
        if (K.pC >= 60) K.J = 15;                                               /* 9155 */
    }
    if (K.pC == 150) {                                                          /* 9156: the flash */
        snd_play_orig(76, 220, 128); G.cam.Z = fminf(G.cam.Z + 10, 10); K.pC = 151; K.pD = 0;
    }
    if (K.pC == 151) {
        K.pD += 1;
        if (K.pD >= 16) {                                                       /* 9159: the scene is set again */
            K.pC = 152; K.E = 1556; K.F = 1052; K.G = K.H = 0; K.J = 5;   /* (RV k_end 3554; D runs on: 152 at D 17) */
            p->x = 1460; p->y = 1192 + ORIG_REF_DY; p->vx = p->vy = p->gsp = 0;
        }
    }
    if (K.pC == 154) K.pD += 1;                                                 /* (RV k_end: D 0 on the frame C turns 154, 3701) */
    if (K.pC == 152 && ++K.pD > 100) { K.pC = 153; K.pD = 0; }                  /* 9160 / 9161 */
    if (K.pC == 153 && ++K.pD >= 64) { K.pC = 154; K.pD = 0; }                  /* 9162 / 9163 */
    if (K.pC == 154) {
        if (K.pD > 120) K.H -= 0.5;                                             /* 9164: it flies off */
        if (b && b->y < 400 && !K.ended) { K.ended = 1; K.on = 0; results_begin_boss(G.frame); }   /* 9165 / 9166 */
    }
}

void sbk_frame(player_t *p) {
    obj_t *b = base();
    if (!b) return;
    int n = sb_flow_night();
    if (!K.night && n >= 1) { obj_t *t = tornado(); K.night = 1; K.E = sbb_px(p); K.F = (t ? t->y : 1208) - 500; }   /* 9005: E = player X, F = tornado Y - 500 (RV k_def 61: 708) */
    if (!K.on && !K.ended && n == 2) {                                         /* 9009: the fight */
        K.on = 1; K.H = 8; sbb_fight_start(); show_all(1);
    }
    if (!K.on) { fx_frame(p); return; }
    int frozen = K.pC > 0 && K.pC < 60 && (G.frame - K.slow0) % 6 != 0;
    if (!frozen) {
        if (K.H > 0) K.H -= 0.1;                                                /* 9027 / 9028 (P 0; every mode, one after the other: */
        if (K.H < 0) K.H += 0.1;                                                /* RV k_def H rests at 1.2e-14) */
        if (K.I == 0) hover(p);                                                 /* (the end too: M -9999 stops the chase and the attacks) */
        if (K.pC > 0) { if (K.G < 0) K.G += 0.2; if (K.G > 0) K.G -= 0.2; }       /* 9152 / 9153 (RV k_end: G -0.4 a step) */
        attacks(p);
    }
    if (!frozen) {
        if (K.I <= 0) K.B = sbb_fmod360(K.B + K.J + (K.B > 270 || K.B < 90 ? 0.1 * K.C : 0));   /* 9116; the down-beat 0.1 C faster (RV k_def / k_kill: +11 at J 5 C 60, +19.4 at J 10.4 C 90) */
        else if (K.I <= 3) K.B = sbb_fmod360(K.B + 4);                          /* 9117 */
        if (K.I == 4) {                                                         /* 9124: the trail */
            memmove(K.hx + 1, K.hx, sizeof K.hx - sizeof K.hx[0]); memmove(K.hy + 1, K.hy, sizeof K.hy - sizeof K.hy[0]); memmove(K.hd + 1, K.hd, sizeof K.hd - sizeof K.hd[0]);
            K.hx[0] = K.E; K.hy[0] = K.F; K.hd[0] = K.D;
        } else for (int k = 0; k < NHIST; k++) { K.hx[k] = K.E; K.hy[k] = K.F; K.hd[k] = K.D; }
        place_parts(); poses();
    }
    for (int k = 0; k < npart(OT_SB_KJ_TARGET); k++) {                         /* 9142 / 9143: a target shows while hit, bursts when beaten */
        obj_t *o = sbb_get(OT_SB_KJ_TARGET, k); if (!o) continue;
        if ((o->fl & SB_DEF) && !o->hidden && o->av[2] == 0) { o->av[2] = 1; obj_effect(o->x, o->y, "sbk_exp2"); }
        if (!(o->fl & SB_DEF)) o->av[2] = 0;
        o->hidden = !(o->c > 1 && !(o->fl & SB_DEF));
        if (K.I == 4 || K.pC > 0) o->fl |= SB_ARM; else o->fl &= ~SB_ARM;      /* 9086 / 9057: flag 5 */
    }
    {                                                                           /* 9146 / 9148: the head is shielded (C 5) while a tail target stands or */
        obj_t *h = head(); int guard = K.hit;                                   /* while its hit is counted (RV k_def: head C 5 from the fight start, 237) */
        for (int k = 0; k < npart(OT_SB_KJ_TARGET) && !guard; k++) { obj_t *o = sbb_get(OT_SB_KJ_TARGET, k); if (o && !(o->fl & SB_DEF)) guard = 1; }
        if (h && guard && !(h->fl & SB_DEF)) h->c = 5;
    }
    if (K.pC <= 0) head_hit();
    ending(p);
    fx_frame(p);
    b->av[0] = K.I; b->av[1] = K.Q; b->av[2] = K.S; b->av[3] = K.pC; b->av[4] = K.E; b->av[5] = K.F; b->av[6] = K.M; b->av[7] = K.on;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_SBK")) printf("sbk %d I %.0f Q %.0f R %.0f S %.0f M %.0f B %.0f D %.0f E %.1f F %.1f G %.2f H %.2f N %.0f K %.0f pC %.0f T %.0f fx %d\n",
        G.frame, K.I, K.Q, K.R, K.S, K.M, K.B, K.D, K.E, K.F, K.G, K.H, K.N, K.K, K.pC, K.T, K.fx_n);
#endif
}
int sbk_draw(const obj_t *o, int pass) {
    (void)pass;
    if ((o->type >= OT_SB_KJ_BASE && o->type <= OT_SB_KJ_GUST2) || o->type == OT_SB_WARNING) {
        if (o->type == OT_SB_TORNADO || o->type == OT_SB_KJ_TRACKER || o->type == OT_SB_KJ_CHARGE) return 1;
        sbb_draw(o, 0xffffffff); return 1;
    }
    return 0;
}
