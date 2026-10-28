/* Jade Creek gimmicks (session 21): types 1370-1429 (jc.h OT_JCG_*). Spec: build/re/spec_jc_gimmicks.md.
 * Labels: RE (decompiled events / constructors of frames 39-43 = handle_frame_40..44), RV (original traces
 * build/orig_traces/jc_gim), INFERRED (marked).
 * Families: the water streams JCWaterStream1-8 (Q26 1-6: the current 10196 / 10197, the splashes 10198-10200; all eight
 * animated by g3, 10195 / 10434 / 10618 / 10935), the leaf platforms GimmickLeafPlatformBack / 1 / 2 (10184-10194, JC5
 * 10924-10934; start 10380-10386), the flying leaves GimmickFlyingLeafPlatform + Mask (10626-10632, JC5 10936), the
 * sliding slabs JCSlidingPlatform + Mask + LandPoint (10219-10229, JC3 / JC5 10633-10643), the collapsing platforms
 * LevelJCCollapsingPlatform + Piece (10457-10461, loop_collapse_40; JC5 10937 / 11259 / 11260), the seed spawners
 * GimmickSeedSpawner + GimmickFlyingSeed (10201-10218, JC3 10619-10625: the player hangs under a seed, M 97), the bubble
 * springs GimmickBubbleSpringSpawner + Back2 / Front2 (10435-10456, col_20_40: the player rolls in a bouncing bubble, M 99),
 * LevelSlipperySurface (10424 / 10425: Carol cannot cling), EffectFallingLeaf. LevelCreekPlatform_2425 is DV's
 * LevelValleyPlatform1 (OT_PLAT_VALLEY, objects.c). LevelIceBarrier (+ Piece) is the Neera arena's (jc_boss.c).
 * Passes: jc_gim_pre (before the movement: leaf / collapse solidity), jc_gim_obj (objects.c per-object pass: nothing,
 * the families run in jc_gim_frame), jc_gim_frame (after the player block, in event order: path step + carry 3283-3289 /
 * 4061, then each family's events), jc_gim_player (ST_RIDE: the seed / bubble rides take the player block's place).
 * Object fields: par = family member (importer JCG_SUB, lvobj anim bits 0-7), layer = instance layer (bits 8-14);
 * av[] = the class alterables named per family below; fl bit 0 = ignored by the sensors (shared meaning), own flags >= 0x1000. */
#include "jc.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#endif

#define RAD 57.29577951308232
#define RT0 OT_JCG_FIRST_
#define OT_JCG_FIRST_ OT_JCG_STREAM
#define NRT (OT_JCG_END - OT_JCG_FIRST_ + 1)
#define LMAX 96
#define F1 0x2000                                  /* the object's flag 1 */
#define F6 0x4000                                  /* flag 6 */
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]
static struct {
    int on, push, ride, ride_obj, ride_m, carry_obj;  /* carry_obj: the Q13 platform 4061 carried her on last frame */           /* ride: 0 none, 1 seed (M 97), 2 bubble (M 99) */
    uint32_t pressed, held;                          /* ST_RIDE: this frame's input (jc_gim_player) */
    int seed_once[4];                                /* 10203-10206 "once" latches */
    short lim_x[64], lim_y[64]; int nlim;            /* the act's EnemyLimiter_383 markers (the flying leaves' stops) */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / RAD); }                  /* Chowdren sin: 0 at exactly 180 */
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / RAD); }   /* cos: 0 at exactly 90 / 270 */
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int R(int n) { return cross_rand(n); }
static int is_carol_char(void) { return G.character == 2 || G.character == 3; }                 /* MV.A 2..3 (on foot / bike) */

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
static void shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }                    /* CAM.Z = max(CAM.Z, z) */
static int near_view(const obj_t *o, int n) { return obj_near_view(o, n); }     /* !is_near_border(-n) */
static void act_pt(const obj_t *o, double *x, double *y) { relic_act_point(o, x, y); }
static int sens_hit(int s, const obj_t *o) {                                    /* a placed player sensor overlapping the object */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int sens_on_bg(int s, int dy) {                                          /* a sensor (moved dy px down) on the background */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) if (b[j] >= a[j] && px_span_solid(&G.lvl, ox + a[j], ox + b[j], oy + j + dy)) return 1;
    return 0;
}
static int sens_grp(int s, int grp, int dy) {                                  /* a placed sensor (dy px lower) on a qualifier group (Q7 / Q8) */
    int ox, oy, h; const signed char *a, *b;
    return mv_sensor_rows(s, &ox, &oy, &a, &b, &h) && objects_sensor_hit(grp, ox, oy + dy, a, b, h);
}
static int disc_hit(const player_t *p, const obj_t *o) {                        /* PLAYER (Player_326: the 27-px disc) */
    static signed char x0[27], x1[27]; static int init;
    if (!init) { init = 1; for (int j = 0; j < 27; j++) { double d = 13.25 * 13.25 - (j - 13) * (j - 13); int w = d < 0 ? -1 : (int)sqrt(d); x0[j] = (signed char)(13 - w); x1[j] = (signed char)(13 + w); } }
    return o->an.b && obj_rows_hit(o, (int)PX(p) - 13, (int)PY(p) - 13, x0, x1, 27);
}
static int limiter_hit(const obj_t *o) {                                        /* EnemyLimiter_383 (16 x 32, hotspot (21, 18)) */
    for (int k = 0; k < S.nlim; k++)
        if (fabsf(S.lim_x[k] - o->x) < 200 && fabsf(S.lim_y[k] - o->y) < 200 &&
            obj_rect_hit(o, S.lim_x[k] - 21, S.lim_y[k] - 18, S.lim_x[k] - 5, S.lim_y[k] + 14)) return 1;
    return 0;
}
static int player_layer(void) { return 2; }                                    /* PLAYER's layer (json 3 of the JC frames = the port's play layer 2) */
int jc_gim_stream_push(void) { return S.push; }

/* ================================================================ Q13 path (3283-3289) + carry (4061) for the JC platforms
 * Only I / J move these (C = G = 0 in their constructors): X = floor(I), Y = floor(J); O / P = this frame's steps.
 * av[6] I, av[7] J; vx / vy = O / P. */
static void path_step(obj_t *o) {
    float nx = (float)floor(o->av[6]), ny = (float)floor(o->av[7]);
    o->vx = nx - o->x; o->vy = ny - o->y; o->x = nx; o->y = ny;
}
static void carry(player_t *p) {                     /* 4061: G >= 0 or grounded, S_BB 1 px lower on a Q13 platform with flag 6 off */
    static const int Q13[] = { OT_JCG_SLIDE, OT_JCG_FLEAFM };
    if (!(p->vy >= 0 || p->ground) || dead(p) || p->state == ST_HANG || p->state == ST_HELD || p->state == ST_RIDE) { S.carry_obj = -1; return; }
    for (unsigned q = 0; q < sizeof Q13 / sizeof Q13[0]; q++) {
        if (!N(Q13[q])) continue;
        int k = mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, Q13[q]);
        if (k < 0 && p->ground && S.carry_obj >= 0 && G.o[S.carry_obj].alive && G.o[S.carry_obj].type == Q13[q]) {
            /* RV slide2: the player stays on a slab falling 15 px a frame (MV.Y = slab y - 14 every frame), which S_BB 1 px lower
             * cannot reach after the slab's step: the platform she stood on last frame is tested where she will be (INFERRED) */
            const obj_t *c = &G.o[S.carry_obj];
            if (mv_sensor_obj(p->x + c->vx, MY(p) + c->vy + 1, p->N, SENSOR_BOTTOMBIG, Q13[q]) == S.carry_obj) k = S.carry_obj;
        }
        if (k < 0 || (G.o[k].fl & 0x40)) continue;
        p->x += G.o[k].vx; p->y += G.o[k].vy;
        if (!p->ground) { p->gsp = p->vx; p->ground = 1; }
        p->vy = 0; p->N = 0; p->O = 0;
        player_sync_sensors(p);
        S.carry_obj = k;
        return;
    }
    S.carry_obj = -1;
}

/* ================================================================ 1. water streams (RE 10195-10200; RV st_idle / st_walk)
 * 1, 3, 6 carry the grounded player 1 px a frame to the left (10196), 2, 4, 5 to the right (10197): MV.L = 1, M != 11 and
 * S_BOTTOMBIG on the stream (box collision). RV st_idle: X - 1.000 every frame on JC1's stream 1. Landing in a stream
 * (S_BB on a Q26 stream with flag 1 off, L = 1) splashes: flag 1 on, two EffectsWaterHit at (PX -/+ 6, PY + 12), sound 208
 * (10198; RV: sound 208 at the landing and again on the next stream instance); every 5th frame (g3 % 5) a water hit at
 * the sensor, x - 7 + R(15) (10199); off every stream: the flags go (10200). Frame (g3 / 2) mod 6 for all eight. */
static const signed char ST_DIR[9] = { 0, -1, 1, -1, 1, 1, -1, 0, 0 };
static void streams(player_t *p) {
    obj_t *o; int fr = (G.frame / 2) % 6;
    EACH(OT_JCG_STREAM, o) if (o->an.b) { o->an.frame = fr; o->an.acc = 0; }
    S.push = 0;
    if (dead(p)) return;
    int L = p->ground && p->state != ST_RIDE, M11 = p->state == ST_PUSH, left = 0, right = 0, any = 0, fresh = 0;
    EACH(OT_JCG_STREAM, o) {
        if (o->par < 1 || o->par > 6 || !sens_hit(SENS_BOTTOMBIG, o)) continue;
        any = 1; if (!(o->fl & F1)) fresh = 1;
        if (ST_DIR[o->par] < 0) left = 1; else right = 1;
    }
    if (L && !M11) {                                                               /* 10196 / 10197 (in that order) */
        if (left) { p->x -= 1; S.push -= 1; }
        if (right) { p->x += 1; S.push += 1; }
        if (left || right) player_sync_sensors(p);
    }
    if (any && fresh && L) {                                                       /* 10198 */
        EACH(OT_JCG_STREAM, o) if (o->par >= 1 && o->par <= 6 && sens_hit(SENS_BOTTOMBIG, o)) o->fl |= F1;
        obj_effect((float)(PX(p) - 6), (float)(PY(p) + 12), "waterhit");
        obj_effect((float)(PX(p) + 6), (float)(PY(p) + 12), "waterhit");
        snd_play_orig(208, 220, 128);
    }
    if (any && G.frame % 5 == 0) {                                                 /* 10199 */
        int sx, sy; mv_sensor_pos(SENS_BOTTOMBIG, &sx, &sy);
        obj_effect((float)(sx - 7 + R(15)), (float)sy, "waterhit");
    }
    if (!any) EACH(OT_JCG_STREAM, o) o->fl &= ~F1;                                 /* 10200 */
}

/* ================================================================ 2. leaf platforms (RE 10184-10194, start 10380-10386; RV leaf1 / leaf2)
 * back: av[0] A tilt, av[1] B extension (-128 at the start), av[2] C open, av[3] D / av[4] E base, av[5] F active, av[6] G
 * spread, av[7] H opening angle; kx / ky = J / K (the stalk top at B = 0). leaf: av[0] A, av[1] B (= its back's C), par 0 / 1.
 * Placed pairs: the k-th leaf1 / leaf2 rides the k-th back (10189: same selection index). */
static obj_t *leaf_back(const obj_t *l, int k) { (void)l; obj_t *b; int i = 0; EACH(OT_JCG_LEAFBACK, b) if (i++ == k) return b; return NULL; }
static void back_place(obj_t *b) {                   /* 10188: x = D + B cos A, y = E - B sin A, angle A - 90 */
    b->x = (float)(int)(c_cos(b->av[0]) * b->av[1] + b->av[3]);
    b->y = (float)(int)(b->av[4] - c_sin(b->av[0]) * b->av[1]);
    b->ang = (float)(b->av[0] - 90);
}
static void leaves(player_t *p) {
    obj_t *b, *l;
    double px = (int)PX(p), py = (int)PY(p);
    EACH(OT_JCG_LEAFBACK, b) {
        double *A = &b->av[0], *B = &b->av[1], *C = &b->av[2], *F = &b->av[5], *Gs = &b->av[6], *H = &b->av[7];
        if (*F == 0 && b->kx > px - 200 && b->kx < px + 200 && b->ky > py - 300 && b->ky < py + 300) *F = 1;   /* 10184 */
        if (*F == 1 && *B < 0) *B += 8;                                                                  /* 10185 */
        if (*F == 1 && *B == 0 && *H < 110) { *H += 10; *Gs = c_sin(*H) * 96; }                         /* 10186 */
        if (*F == 1 && *B == 0 && *H >= 110) *C = 1;                                                     /* 10187 */
        back_place(b); (void)A;                                                                          /* 10188 */
    }
    int k1 = 0, k2 = 0;
    EACH(OT_JCG_LEAF, l) {                           /* 10189: on the back's action point, A = back A +/- G, angle A - 90, B = back C */
        b = leaf_back(l, l->par ? k2++ : k1++);
        if (!b) continue;
        double ax, ay; act_pt(b, &ax, &ay);
        l->x = (float)(int)ax; l->y = (float)(int)ay;
        l->av[0] = l->par ? b->av[0] - b->av[6] : b->av[0] + b->av[6];
        l->ang = (float)(l->av[0] - 90);
        l->av[1] = b->av[2];
        if (l->av[1] == 1) { l->layer = player_layer(); l->fl &= ~1; }              /* 10190 / 10191: the player's layer (solid), behind */
        else { l->layer = 1; l->fl |= 1; }                                            /* 10192 / 10193: layer 2 (json): not on the player's layer */
    }
    EACH(OT_JCG_LEAFBACK, b) if (b->av[2] > 0 && !near_view(b, 300)) {               /* 10194: far away: closed and lowered again */
        b->av[2] = 0; b->av[7] = 0; b->av[1] = -128; b->av[6] = 0; b->av[5] = 0;
    }
}

/* ================================================================ 3. flying leaves (RE 10626-10632; JC5 10936)
 * mask (the platform, Q8 Q9 Q13, invisible): av[6] I, av[7] J path, av[0] Q vertical speed, av[1] R rest y, av[2] S the
 * leaf's y offset; F1 = stood on this frame. leaf: follows the k-th mask; its angle A = the mask's Q (10626). */
static void flying(player_t *p) {
    obj_t *m, *l;
    int k = 0;
    EACH(OT_JCG_FLEAF, l) {                          /* 10626: the leaf on its mask, A = Q, speed |Q| 20; the mask's flag 1 off; J += Q */
        int i = 0; obj_t *mm = NULL; EACH(OT_JCG_FLEAFM, m) if (i++ == k) mm = m;
        k++;
        if (!mm) continue;
        l->x = mm->x; l->y = (float)(int)(mm->y + mm->av[2]); l->ang = 0;
        l->c = (int)(fabs(mm->av[0]) * 20);
    }
    EACH(OT_JCG_FLEAFM, m) { m->fl &= ~F1; m->av[7] += m->av[0]; }
    if (p->ground && !dead(p)) EACH(OT_JCG_FLEAFM, m)                               /* 10627: stood on from above: flag 1 */
        if (MY(p) < m->y && sens_hit(SENS_BOTTOMBIG, m)) m->fl |= F1;
    EACH(OT_JCG_FLEAFM, m) {
        double *Q = &m->av[0], Rr = m->av[1];
        int lim = limiter_hit(m);
        if ((m->fl & F1) && !lim) *Q = -3;                                           /* 10628: rises */
        if ((m->fl & F1) && lim) *Q = fmin(*Q + 0.1, 0);                             /* 10630: stops under a limiter */
        if (!(m->fl & F1) && m->y < Rr) *Q = fmin(*Q + 0.1, 4);                      /* 10631: sinks back */
        if (*Q > 0 && m->y >= Rr) { *Q = 0; m->av[7] = Rr; }                         /* 10632: home */
    }
    if (G.frame % 4 == 0) EACH(OT_JCG_FLEAFM, m) if (m->av[0] < 0) {                  /* 10629: falling leaves (while it rises: INFERRED reading) */
        obj_t *f = spawn(OT_JCG_FALLLEAF, "jcg_fallleaf", m->x - 50 + R(101), m->y + 22, m->layer);
        if (f) { f->ang = (float)R(360); f->dir = 21 + R(7); }
    }
}

/* ================================================================ 4. sliding slabs (RE 10219-10229)
 * slab (Q7 Q13, invisible): av[6] I, av[7] J path, av[0] Q, av[1] R speeds, av[2] S state (0 rest, 1 sliding / falling,
 * 2 on a land point, 3 in water), av[3] T slide end x, av[4] U explosions; the mask (drawn) rides the k-th slab. */
static void slides(player_t *p) {
    obj_t *o, *q;
    if (p->ground && !dead(p)) EACH(OT_JCG_SLIDE, o) if (o->av[2] == 0 && sens_hit(SENS_BOTTOMBIG, o)) {   /* 10219 */
        o->av[2] = 1; o->av[3] = o->x + 150; snd_play_orig(426, 220, 128);
    }
    EACH(OT_JCG_SLIDE, o) {
        double *I = &o->av[6], *J = &o->av[7], *Q = &o->av[0], *Rr = &o->av[1], *St = &o->av[2], *T = &o->av[3], *U = &o->av[4];
        if (*St == 1 && *U < 10) {                                                   /* 10220: 10 explosions down the diagonal */
            obj_effect(o->x + 15 * (float)*U, o->y + 15 * (float)*U, "explosion01"); *U += 1;
        }
        if (*St == 1 && *I < *T) {                                                   /* 10221: slides down the slope */
            *I += *Q; *J += *Rr; *Rr += 0.1; *Q += 0.1;
            int r = R(154); obj_effect(o->x + r, o->y + r, "dust");
        } else if (*St == 1) {                                                       /* 10222: off the slope: falls */
            *I += *Q; *J += *Rr; *Rr = fmin(*Rr + 0.2 + 0.1, 15);
        }
        EACH(OT_JCG_SLIDEL, q) {
            if (!obj_obj_hit(o, q)) continue;
            if (*St == 1 && *I < *T) { q->alive = 0; *T += 180; continue; }          /* 10223: a land point met on the slope goes, the slope extends 180 px */
            if (*St == 1 && o->y > q->y) {                                           /* 10224: lands once its top passes the point (RV slide2: y 974 no, 989 yes) */
                *St = 2; *J = q->y; snd_play_orig(51, 220, 128); shake((float)(0.5 * *Rr)); *Rr = 0;
            }
        }
        if (*St == 2) { *I += *Q; *Q = fmax(*Q - 0.1, 0); }                         /* 10225: slides to a stop */
        if (*St == 2 && *Q > 0) { int on = 0; EACH(OT_JCG_SLIDEL, q) if (obj_obj_hit(o, q)) on = 1; if (!on) *St = 1; }   /* 10226 */
        int wl = level_cur() ? level_cur()->water : 0;                                 /* ML.B; the slab's action point is 55 px below its top */
        if (*St == 1 && wl > 0 && water_on() && *J + 55 >= wl) { *St = 3; *Rr *= 0.5; snd_play_orig(14, 220, 128); }   /* 10227 (RV slide2: sound 14, R 15 -> 7.5) */
        if (*St == 3) { *I += *Q; *J += *Rr; *Q = fmax(*Q - 0.04, 0); *Rr = fmax(*Rr - 0.1, -1); if (*J + 55 < wl) { *J = wl - 55; *Rr = 0; } }   /* 10228 (the surface clamp INFERRED) */
    }
    int k = 0;
    EACH(OT_JCG_SLIDEM, q) { int i = 0; EACH(OT_JCG_SLIDE, o) if (i++ == k) { q->x = o->x; q->y = o->y; } k++; }   /* 10229 */
}

/* ================================================================ 5. collapsing platforms (RE 10457-10461, loop_collapse_40)
 * platform: av[0] A pieces made; piece: av[0] A y, av[1] B fall speed, av[2] C delay. Stood on (L = 1, S_BB on it): flag 6,
 * 8 pieces at once (frame k, C = 6 (k + 1)), sound 426, the platform goes (10458 at A >= 8). A piece waits C frames, then
 * falls (B + 0.3 to 15) and stops being solid (flag 0); it goes once 290 px below the view top. */
/* members: 0 / 1 LevelJCCollapsingPlatform 2456 / 2475 (+ Piece_2457), 2 Trap Hideout's LevelTHCollapsingPlatform_3186 (+ Piece_3187:
 * the same events 13598-13602 / loop_collapse_57, RE session 21b); the sprite rows per member (importer JCG_SUB, TH_GIM rows) */
static const char *const COLL_ROW[3] = { "jcg_coll0", "jcg_coll1", "thg_coll" }, *const COLL_PIECE[3] = { "jcg_cpiece", "jcg_cpiece", "thg_cpiece" };
static void collapses(player_t *p) {
    obj_t *o;
    if (p->ground && !dead(p)) EACH(OT_JCG_COLL, o) if (sens_hit(SENS_BOTTOMBIG, o)) {   /* 10457 */
        o->fl |= F6;
        for (int n = 0; n < 8; n++) {
            obj_t *c = spawn(OT_JCG_CPIECE, COLL_PIECE[o->par >= 0 && o->par <= 2 ? o->par : 0], o->x, o->y, o->layer);
            if (!c) break;
            c->an.frame = (int)o->av[0]; c->an.acc = 0;
            o->av[0] += 1;
            c->av[0] = c->y; c->av[2] = 6 * o->av[0];
        }
        snd_play_orig(426, 220, 128);
    }
    EACH(OT_JCG_COLL, o) if (o->av[0] >= 8) o->alive = 0;                             /* 10458 */
    EACH(OT_JCG_CPIECE, o) {
        if (o->av[2] > 0) o->av[2] -= 1;                                              /* 10459 */
        if (o->av[2] > 0) continue;                                                   /* RV coll2: the first piece falls 5 frames after the platform breaks */
        o->av[1] = fmin(o->av[1] + 0.3, 15); o->av[0] += o->av[1]; o->y = (float)(int)o->av[0]; o->fl |= 1;   /* 10460 */
        if (o->y > G.camy + 290) o->alive = 0;                                        /* 10461 */
    }
}

/* ================================================================ 6. slippery surfaces (RE 10424 / 10425): Carol's claws slip
 * MV.A 2..3 (Carol, on foot or on the bike) and PLAYER on a surface: her cling cooldown PSC.M = max(M, 3); clinging (M 47):
 * M = 1, T = U = 1 (she drops off the wall). Nothing for Lilac or Milla. */
static void slippery(player_t *p) {
    obj_t *o;
    if (!is_carol_char() || dead(p)) return;
    EACH(OT_JCG_SLIP, o) {
        if (!disc_hit(p, o)) continue;
        if (p->cM < 3) p->cM = 3;
        if (p->state == ST_WALL) { p->state = ST_NORMAL; p->state_t = 0; p->m1 = 1; }
#ifdef FP_HOST_ENV
        { static int dbg = -1; if (dbg < 0) dbg = getenv("FP_DBG_JCG") != NULL; if (dbg) printf("jcg slip %d cM %d st %d\n", G.frame, p->cM, p->state); }
#endif
    }
}

/* ================================================================ 7. seeds (RE 10201-10218; JC3 10619-10625)
 * spawner: F1 = the player within 31 px (10202). seed: av[0] A x, av[1] B y, av[2] C x speed, av[3] D y speed, av[4] E swing,
 * av[5] F swing speed; F1 = the ridden seed. Touching a spawner (M not 97 / 9 / 10) makes 4 seeds at (PX, PY - 20) once:
 * F 2 C 0.7 B D -3; F -2 C 0.85 B D -2.5; F -2 C 0.9 B D -3.5; and the ridden one (C = B, D -2): M 97 (10203-10206). */
static int orig_m(const player_t *p) { return player_orig_m(p); }
static void seed_release(player_t *p, obj_t *sd, int m, double g) {
    if (sd) sd->fl &= ~F1;
    S.ride = 0; S.ride_obj = -1;
    p->state = ST_NORMAL; p->state_t = 0; p->rot = 0; p->cY = 0;
    p->ground = 0; p->N = p->O = 0; p->vy = g;
    p->m1 = m == 1; p->springing = m == 40; p->jumping = m == 1;
}
static void seeds(player_t *p) {
    obj_t *o, *sd;
    int near = 0;
    EACH(OT_JCG_SEEDSP, o) {                                                          /* 10201 / 10202 */
        o->fl &= ~F1;
        if (fabs(o->x - PX(p)) <= 31 && fabs(o->y - PY(p)) <= 31) { o->fl |= F1; near = 1; }
    }
    int M = orig_m(p), ok = near && !dead(p) && S.ride != 1 && M != 9 && M != 10 && S.ride != 2;
    static const double SC[3] = { 0.7, 0.85, 0.9 }, SD[3] = { -1, -0.5, -1.5 }, SF[3] = { 2, -2, -2 };
    for (int n = 0; n < 4; n++) {
        if (!ok) { S.seed_once[n] = 0; continue; }
        if (S.seed_once[n]) continue;
        S.seed_once[n] = 1;
        obj_t *s = spawn(OT_JCG_SEED, "jcg_seed", (float)PX(p), (float)(PY(p) - 20), 1);
        if (!s) continue;
        s->av[0] = s->x; s->av[1] = s->y; s->av[2] = 3; s->av[3] = -2; s->av[5] = 2;    /* ctor C 3 D -2 F 2 */
        double B = p->ground ? p->gsp : p->vx;
        if (n < 3) { s->av[5] = SF[n]; s->av[2] = SC[n] * B; s->av[3] += SD[n]; }
        else {                                                                       /* 10206: the ridden seed; 10207: Milla's float off */
            s->fl |= F1; s->av[2] = B; S.ride = 1; S.ride_obj = (int)(s - G.o); S.ride_m = 97;
            p->state = ST_RIDE; p->state_t = 0; p->mA = 0; p->mB = 0; p->mE = 0;
        }
    }
    EACH(OT_JCG_SEED, sd) {
        sd->x = (float)(int)sd->av[0]; sd->y = (float)(int)sd->av[1];                 /* 10208 */
        sd->av[0] += 0.03 * sd->av[4] + sd->av[2]; sd->av[1] += sd->av[3];
        if (sd->av[2] > 3) sd->av[2] -= 0.05;                                         /* 10209 */
        if (sd->av[2] < -3) sd->av[2] += 0.05;                                        /* 10210 */
        sd->ang = (float)sd->av[4]; sd->av[4] += sd->av[5]; sd->av[5] *= 0.995;       /* 10211 */
        if (sd->av[4] > 0) sd->av[5] -= 0.1;                                          /* 10212 */
        if (sd->av[4] < 0) sd->av[5] += 0.1;                                          /* 10213 */
    }
    sd = S.ride == 1 && S.ride_obj >= 0 ? &G.o[S.ride_obj] : NULL;
    if (sd && (!sd->alive || sd->type != OT_JCG_SEED)) { seed_release(p, NULL, 0, 0); sd = NULL; }
    if (sd) {                                                                         /* 10214: hanging under the seed */
        double E = sd->av[4];
        p->x = sd->x + c_sin(E) * 40; p->y = sd->y + c_cos(E) * 40 + ORIG_REF_DY;
        p->rot = (float)nrm(E + 360); p->N = nrm(E); p->ground = 0; p->gsp = 0; p->vx = sd->av[2];
        p->vy = fmin(p->vy + 0.25, 15.25);                                             /* RV seed2j: MV.G keeps falling (0.25 a frame, 15.25) under the seed */
        player_sync_sensors(p);
        sd->av[3] = fmin(sd->av[3] + 0.05, 4);
        int hit = p->water || sens_grp(SENS_BOTTOMBIG, OBJGRP_OBSTACLE, 0);          /* the big sensors' Z (background or a Q7 object), water, S_BB on Q8 */
        for (int s4 = SENS_LEFTBIG; s4 <= SENS_BOTTOMBIG && !hit; s4 += 2) hit = sens_on_bg(s4, 0) || sens_grp(s4, OBJGRP_SOLID, 0);
        if (sd->av[3] > -1 && hit) seed_release(p, sd, 0, p->vy);                    /* 10215: M 0, T = U = 1 (G kept) */
        else if (S.pressed & BTN_JUMP) {                                              /* 10216 / 10217 */
            if (S.held & BTN_DOWN) seed_release(p, sd, 40, 0); else seed_release(p, sd, 1, -4.5);   /* RV seed2j: G -4.5 */
        }
    }
    EACH(OT_JCG_SEED, sd) if (!(sd->fl & F1) && near_view(sd, 80)) sd->alive = 0;     /* 10218: RV seed2j: the three free seeds are gone the next frame */
}

/* ================================================================ 8. bubble springs (RE 10435-10456, col_20_40)
 * spawner: av[0] A timer (+1 a frame; > 100: a bubble at its action point, A 0). bubble (back): av[0] A (0 free, 1 riding,
 * 2 squashed on the ground), av[1] B squash, av[2] C x speed, av[3] D y speed, av[4] E squash angle, av[5] F bounces,
 * av[6] G wobble timer; the front rides the k-th back. */
static void bubble_pop(player_t *p, obj_t *b) {      /* 10452 / 10453: the bubble bursts (sound 69, EffectsWhiteBurst_31), M 0 */
    obj_effect(b->x, b->y, "wburst");
    snd_play_orig(69, 220, 128);
    b->alive = 0;
    if (S.ride == 2 && S.ride_obj == (int)(b - G.o)) {
        S.ride = 0; S.ride_obj = -1; p->state = ST_NORMAL; p->state_t = 0; p->m1 = 0; p->rot = 0;
    }
}
static void bubbles(player_t *p) {
    obj_t *o, *b, *f;
    EACH(OT_JCG_BUBSP, o) {                                                          /* 10435 / 10436 */
        o->av[0] += 1;
        if (o->av[0] > 100) {
            o->av[0] = 0;
            double ax, ay; act_pt(o, &ax, &ay);
            obj_t *nb = spawn(OT_JCG_BUBBACK, "jcg_bub00", (float)ax, (float)ay, o->layer);
            if (nb) { spawn(OT_JCG_BUBFRONT, "jcg_bubf0", (float)ax, (float)ay, o->layer); nb->par = (int)(o - G.o); }
        }
    }
    EACH(OT_JCG_BUBBACK, b) {
        double *A = &b->av[0];
        if (*A == 0) b->y -= 3;                                                       /* 10437: rises */
        if (*A == 0 && near_view(b, 60) && G.frame % 10 == 0) obj_effect(b->x, b->y, "ringsparkle");   /* 10438 */
        if (*A == 0 && near_view(b, 60)) b->av[6] += 1;                               /* 10439 */
        if (*A == 0 && b->y < -50) { b->alive = 0; continue; }                        /* 10440 */
        if (*A == 0 && b->av[6] > 20) { b->av[6] = -60 - R(60); set_s(b, "jcg_bub70"); anim_restart(&b->an); }   /* 10441: wobble */
        if (*A == 0 && b->an.done && b->an.anim == ens_anim("jcg_bub70")) set_s(b, "jcg_bub00");
        /* col_20_40: PLAYER on a free bubble that has left its spawner, M not 99 / 9 / 10: captured */
        if (*A == 0 && !dead(p) && S.ride == 0 && disc_hit(p, b) && orig_m(p) != 9 && orig_m(p) != 10) {
            obj_t *sp = b->par >= 0 ? &G.o[b->par] : NULL;
            if (!sp || !sp->alive || !obj_obj_hit(b, sp)) {
                *A = 1; b->av[2] = p->ground ? p->gsp : p->vx; b->av[3] = p->ground ? 0 : p->vy;
                snd_play_orig(7, 220, 128);
                S.ride = 2; S.ride_obj = (int)(b - G.o); S.ride_m = 99; p->state = ST_RIDE; p->state_t = 0;
            }
        }
    }
    b = S.ride == 2 && S.ride_obj >= 0 ? &G.o[S.ride_obj] : NULL;
    if (b && (!b->alive || b->type != OT_JCG_BUBBACK)) { S.ride = 0; S.ride_obj = -1; p->state = ST_NORMAL; b = NULL; }
    if (b) {
        double *A = &b->av[0], *C = &b->av[2], *D = &b->av[3];
        if (*A == 1) {                                                                /* 10443: moving with the player, gravity 0.2 (max 15) */
            if (S.held & BTN_LEFT) *C = fmax(*C - 0.2, -8);                           /* 10446 */
            if (S.held & BTN_RIGHT) *C = fmin(*C + 0.2, 8);                           /* 10447 */
            *D = fmin(*D + 0.2, 15);
            mv_t m = { p->x, MY(p), *C, *D, 0, 0, 0, 0, 0, 0, 0, 0 };
            int landed = (mv_core(&m) & MV_LANDED) != 0;
            p->x = m.X; p->y = m.Y + ORIG_REF_DY; p->ground = 0; p->N = p->O = 0; p->vx = *C; p->vy = *D;
            if (m.B == 0 && *C != 0) *C = 0;                                          /* a wall stops it (the movement's B) */
            b->x = (float)(int)PX(p); b->y = (float)(int)PY(p);
            player_sync_sensors(p);
            if (*D < 0 && sens_on_bg(SENS_TOPBIG, 0)) { snd_play_orig(72, 220, 128); *D = -*D; }   /* 10448: the ceiling */
            if (*D > 0 && (landed || sens_on_bg(SENS_BOTTOMBIG, 24) || sens_grp(SENS_BOTTOMBIG, OBJGRP_SOLID, 24))) {   /* 10449: the ground (or a Q7 object): squash */
                *A = 2; b->av[4] = 0; set_s(b, "jcg_bub01"); b->y += 32; *C = 0; shake((float)(0.25 * *D)); snd_play_orig(72, 220, 128);
            }
        } else if (*A == 2) {                                                         /* 10450 / 10451 */
            b->av[1] = c_sin(b->av[4]) * 0.6; b->av[4] += 10;                       /* E + 10 (RE; RV bub2 shows 8-12 grounded frames: open) */
            double ax, ay; act_pt(b, &ax, &ay); p->x = (int)ax; p->y = (int)ay + ORIG_REF_DY; player_sync_sensors(p);
            if (b->av[4] >= 180) {
                *A = 1; b->av[4] = 0; *D = fmax(-4 - *D, -15);                        /* RE (RV bub2 launches -4.2 / -7.6 / -11.6: open) */ set_s(b, "jcg_bub00"); b->y -= 32; b->av[1] = 0;
                p->vy = *D; b->av[5] += 1;
            }
        }
        if (b->av[5] > 2 || (S.pressed & BTN_JUMP)) bubble_pop(p, b);                 /* 10453: 3 bounces, or jump */
    }
    int k = 0;
    EACH(OT_JCG_BUBFRONT, f) {                                                         /* 10455: the front on its back, squashed by B */
        int i = 0; obj_t *bb = NULL; EACH(OT_JCG_BUBBACK, b) if (i++ == k) bb = b;
        k++;
        if (!bb) { f->alive = 0; continue; }
        f->x = bb->x; f->y = bb->y; set_s(f, bb->an.anim == ens_anim("jcg_bub01") ? "jcg_bubf1" : "jcg_bubf0");
        f->scale = (float)(1 + bb->av[1]);
    }
}

/* ================================================================ 9. falling leaves (create_effectfallingleaf_2396: BallMovement
 * speed 20 = 2.5 px a frame in a random direction of 21..27, no deceleration): gone off the view */
static void fall_leaves(void) {
    obj_t *o;
    EACH(OT_JCG_FALLLEAF, o) {
        double a = o->dir * 11.25;
        o->fx += (float)(2.5 * c_cos(a)); o->fy -= (float)(2.5 * c_sin(a));
        o->x = o->ox + floorf(o->fx); o->y = o->oy + floorf(o->fy);
        if (!near_view(o, 64)) o->alive = 0;
    }
}

/* ================================================================ spawn */
void jc_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS); S.ride_obj = -1; S.carry_obj = -1;
    if (objects_have(OT_PLAT_VALLEY) && ens_anim("jcg_creek") >= 0)                 /* LevelCreekPlatform_2425 = a valley platform drawn from _ens */
        for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_PLAT_VALLEY && !o->an.b) set_s(o, "jcg_creek"); }
    if (!objects_have_from(RT0)) return;
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (o->alive && o->type == OT_LIMITER && S.nlim < 64) { S.lim_x[S.nlim] = (short)o->x; S.lim_y[S.nlim++] = (short)o->y; }
        if (!o->alive || o->type < RT0 || o->type > OT_JCG_END) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0x7f;                                /* the importer's layer (JC json 2 / 3 / 4 -> 1 / 2 / 3) */
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = 0; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        lst_add(o); S.on = 1;
    }
    if (!S.on) return;
    obj_t *o;
    static const char *const STN[9] = { "", "jcg_st1", "jcg_st2", "jcg_st3", "jcg_st4", "jcg_st5", "jcg_st6", "jcg_st7", "jcg_st8" };
    EACH(OT_JCG_STREAM, o) set_s(o, STN[o->par >= 1 && o->par <= 8 ? o->par : 1]);
    /* leaf backs: A = the act's tilt by index (start events 10382-10385 & co: importer JCG_LEAF_TILT -> par = A / 30, 0 = 90),
     * D, E = x, y (10386), J, K = the stalk top (the action point at B = 0), then B = -128 */
    EACH(OT_JCG_LEAFBACK, o) {
        set_s(o, "jcg_lback");
        o->av[0] = o->par ? o->par * 30 : 90; o->av[3] = o->x; o->av[4] = o->y; o->av[1] = 0;
        back_place(o);
        double ax, ay; act_pt(o, &ax, &ay); o->kx = (int)ax; o->ky = (int)ay;
        o->av[1] = -128; back_place(o);
        o->layer = 1;
    }
    EACH(OT_JCG_LEAF, o) { set_s(o, o->par ? "jcg_leaf2" : "jcg_leaf1"); o->fl |= 1; o->layer = 1; }
    EACH(OT_JCG_FLEAF, o) set_s(o, "jcg_fleaf");
    EACH(OT_JCG_FLEAFM, o) { set_s(o, "jcg_fleafm"); o->av[6] = o->x; o->av[7] = o->y; o->av[1] = o->y; }   /* 3565: I, J = x, y; R = J (INFERRED) */
    EACH(OT_JCG_SLIDE, o) { set_s(o, "jcg_slide"); o->av[6] = o->x; o->av[7] = o->y; }
    EACH(OT_JCG_SLIDEM, o) set_s(o, "jcg_slidem");
    EACH(OT_JCG_SLIDEL, o) set_s(o, "jcg_slidel");
    EACH(OT_JCG_COLL, o) set_s(o, COLL_ROW[o->par >= 0 && o->par <= 2 ? o->par : 0]);
    EACH(OT_JCG_SEEDSP, o) set_s(o, "jcg_seedsp");
    EACH(OT_JCG_BUBSP, o) { set_s(o, "jcg_bubsp"); o->av[0] = -1; }
    EACH(OT_JCG_SLIP, o) set_s(o, "jcg_slip");
    EACH(OT_JCG_FALLLEAF, o) { set_s(o, "jcg_fallleaf"); o->dir = 21 + R(7); }
}

/* ================================================================ passes */
void jc_gim_pre(player_t *p) { (void)p; }

int jc_gim_obj(obj_t *o, player_t *p) { (void)p; return o->type >= RT0 && o->type <= OT_JCG_END; }   /* the families run in jc_gim_frame */

void jc_gim_frame(player_t *p) {
    if (!S.on) return;
    obj_t *o;
    EACH(OT_JCG_SLIDE, o) path_step(o);                                                /* 3283-3289 */
    EACH(OT_JCG_FLEAFM, o) path_step(o);
    carry(p);                                                                         /* 4061 */
    if (N(OT_JCG_STREAM)) streams(p);
    if (N(OT_JCG_LEAFBACK)) leaves(p);
    if (N(OT_JCG_SEEDSP) || N(OT_JCG_SEED)) seeds(p);
    if (N(OT_JCG_SLIDE)) slides(p);
    if (N(OT_JCG_SLIP)) slippery(p);
    if (N(OT_JCG_BUBSP) || N(OT_JCG_BUBBACK)) bubbles(p);
    if (N(OT_JCG_COLL) || N(OT_JCG_CPIECE)) collapses(p);
    if (N(OT_JCG_FLEAFM)) flying(p);
    if (N(OT_JCG_FALLLEAF)) fall_leaves();
    S.pressed = 0;
}

/* ST_RIDE (player.c): the seed (M 97) and bubble (M 99) rides replace the player block; this frame's input is kept for the
 * release events, which run with the rides in jc_gim_frame (10214-10217, 10443-10453) */
int jc_gim_player(player_t *p, const input_t *in) {
    if (!S.ride) { p->state = ST_NORMAL; return 0; }
    S.pressed = in->pressed; S.held = in->held; p->held = in->held;
    if (S.ride == 1) player_ride_anim(p, "climb", -1);                                /* 10214: skins animation 9 */
    else player_ride_anim(p, "jump", (int)(fabs(p->vy) * 4 + 20));                    /* 10443: OTHERS.Y 8 (ball), speed |D| 4 + 20 */
    return 1;
}
int jc_gim_ride_m(void) { return S.ride ? S.ride_m : -1; }

void jc_gim_draw(int pass) { (void)pass; }
int  jc_gim_q20(int type) { (void)type; return 0; }
