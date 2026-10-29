/* Pangu Lagoon gimmicks (session 23): types 2144-2223 (pl.h OT_PLG_*). Spec: build/re/spec_pl_gimmicks.md.
 * Labels: RE (decompiled events / constructors of frames 50-55 = handle_frame_51..56, build/re/Frames__event_func_*.a.c,
 * create_*_int__int.a.c, test_collisions_50..55 / col_*_5x), RV (original traces build/orig_traces/pl_gim: ct1 / ct2 cyclone top,
 * tg1, bow2, bo4 breakout, sw5, vb5 vanishing blocks, sd3 stone doors), INFERRED (marked).
 * Families (event numbers in the spec): vanishing blocks (13032-13039 / 13105 / 13144; PL6's single timer 13217-13224 is the
 * boss's), time gates + pieces (12743, 12772-12779, 12849, col_9_50 / col_12_55: the super boost), bow + strings + arrow
 * (12968-12986, 13011, 13157; PL2 / PL5 12864-12890, 12958; col_0_51 / col_16_51), pinwheels (12780-12783, 12850), cyclone
 * tops (12761-12771, 13109 / 13114, col_4_50), breakout ball / paddle / walls (13019-13023, 13042-13056; PL5 13108 / 13119 /
 * 13128 / 13129), lagoon platforms (the objects.c path registry), giant swords + masks (13130-13137, 13146, col_19_51),
 * spinball + chains (13040 / 13041 / 13118 / 13225 / 5845), the hanging PL switch + stone doors (12987-12993, 13012; the
 * door scene 13000-13009 / 13232-13237 / 13330 is pl_flow.c's: pl_flow_door_report), Active_2927 (an invisible Q8 floor, registry only).
 * Reused systems: objects.c path-platform registry (LevelLagoonPlatform1 / 2), the Q7 / Q8 / Q9 groups (objdefs.c flags; a vanished
 * block is flag 0 = ignored by the sensors), fn_gima.c's breakable wall (BreakablePLRock = OT_FN_BWALL member 1, its pieces and the
 * bow arrow's hit: fn_gima_wall_hit), relic_gimb.c's hanging idle switch (OT_RM_IDLESW: the grab; this file moves it, flag
 * RMSW_EXTERNAL), combat.c's Q20 contact (the falling sword's B), player_pose (the arrow ride), the boost code's SKIN
 * flag 8 (p->sboost: the time gates' super boost).
 * Passes: pl_gim_pre (nothing), pl_gim_obj (nothing: the families run in pl_gim_frame), pl_gim_frame (after the player block, in event
 * order), pl_gim_draw (pass 1 / 2: the x-scaled stone doors, the additive rebound effects), pl_gim_player (ST_RIDE: the arrow).
 * Object fields: par = family member (importer PLG_SUB, lvobj anim bits 0-7), layer = instance layer (bits 8-14); av[] = class
 * alterables named per family below. */
#include "pl.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>                                  /* qsort */
#ifdef FP_HOST_ENV
#include <stdio.h>
#endif

#define RAD 57.29577951308232
#define RT0 OT_PLG_VBLOCK
#define NRT (OT_PLG_END - RT0 + 1)
/* per-type instance lists, sized at the act start from the act's own placements (PL5 / PL6 place 70 / 90 time-gate pieces: the old
 * fixed 64 left the rest without sprite and orbit): each type gets max(LMIN, placed) slots of one pool (runtime types keep LMIN) */
#define LMIN 64
#define LPOOL (NRT * LMIN + 128)
static short LST[LPOOL], LOFF[NRT], LCAP[NRT]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[LOFF[(T) - RT0] + _k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]
#define VBMAX 512                                  /* vanishing blocks (PL5 places 389): their own x-sorted list */
#define F1 0x1000                                  /* the object's flag 1 */
#define F2 0x2000                                  /* flag 2 */
#define F5 0x20                                    /* flag 5: armoured (combat.c F_ARMOURED) */
static struct {
    int on;
    int t;                                         /* frames since the act start (the vanishing blocks' A) */
    short vb[VBMAX]; int nvb;                      /* vanishing blocks (slots), sorted by x */
    uint32_t held, prev_held;                      /* this frame's input (jump pressed: the cyclone top's leap) */
    int ride, ride_obj;                            /* 1: the bow arrow (M 99); the arrow slot */
    int door_c, door_enter;                        /* pl_flow.c's door scene: running last frame, doors drawn in front (13002) */
    int top_ride;                                  /* the cyclone top ridden (slot) or -1 */
    int snd328, snd329;                            /* once-per-frame sound latches (13033 / 13035) */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double PY(const player_t *p) { return floor(MY(p)); }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / RAD); }                  /* Chowdren sin: 0 at exactly 180 */
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / RAD); }   /* cos: 0 at exactly 90 / 270 */
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int hurt(const player_t *p) { return p->state == ST_HURT || dead(p); }                      /* M 8 / 9 / 10 */
static void snd(int id) { snd_play_orig(id, 220, 128); }
static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    short *l = &LST[LOFF[t]];
    for (int k = 0; k < NLS[t]; k++) if (l[k] == i) return;
    if (NLS[t] < LCAP[t]) { l[NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LCAP[t]; k++) { const obj_t *o = &G.o[l[k]]; if (!o->alive || o->type != q->type) { l[k] = (short)i; return; } }
}
/* the lists' layout for this act: LCAP[t] = max(LMIN, the type's instances at the start); one pass at load */
static void lst_layout(void) {
    int cnt[NRT] = { 0 }, off = 0;
    for (int i = 0; i < G.nobjs; i++) { int t = G.o[i].type - RT0; if (G.o[i].alive && t >= 0 && t < NRT) cnt[t]++; }
    for (int t = 0; t < NRT; t++) {
        int c = RT0 + t == OT_PLG_VBLOCK ? 0 : cnt[t] > LMIN ? cnt[t] : LMIN;  /* (vanishing blocks: their own list S.vb) */
        if (off + c > LPOOL) c = LPOOL - off;                                  /* (pool full: the extra instances are left out) */
        LOFF[t] = (short)off; LCAP[t] = (short)c; off += c;
    }
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
static int sens_hit(int s, const obj_t *o) {                                    /* a placed player sensor overlapping the object */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int disc_hit(const player_t *p, const obj_t *o) {                        /* PLAYER (Player_326: the 27-px disc; its box, INFERRED) */
    int X = (int)PX(p), Y = (int)PY(p);
    return o->an.b && obj_rect_hit(o, X - 13, Y - 13, X + 14, Y + 14);
}
static int rect_disc(float x0, float y0, float x1, float y1, const player_t *p) {   /* a box (whole image, box collision) vs PLAYER's disc */
    double X = PX(p), Y = PY(p), cx = fmax(x0, fmin(X, x1 - 1)), cy = fmax(y0, fmin(Y, y1 - 1));
    return (cx - X) * (cx - X) + (cy - Y) * (cy - Y) <= 13.5 * 13.5;
}
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p) && p->invuln <= 0; }   /* MV.R == 0, OTHERS.E <= 0 */
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }      /* !is_near_border(-n): inside the view grown by n */

/* ================================================================ 1. vanishing blocks (PL4 / PL5; RE 13032-13039, 13105 / 13144)
 * ctor B 14, box collision, animation speed 0 (the frame is B). Start (13105 / 13144): flag 0 on, A = (6 k) mod 210 (k = instance
 * order), move_back. Every frame: A = (A + 1) mod 210 (13032); 60 <= A < 150: flag 0 off (solid), B = max(B - 1, 0) (13034); A >= 150:
 * B = min(B + 1, 14) (13036); B >= 14 with flag 0 off: flag 0 on (13039); visible while B < 14 (13037 / 13038; RV vb5 screenshots:
 * a vanished block is not drawn); sound 328 when A becomes 60 and 329 at 150 for a block inside the view grown by 32 (13033 / 13035,
 * once a frame; RV vb5: one sound every 6 frames). The port evaluates the closed form of that cycle (B(A) below; identical from the
 * second cycle on, INFERRED for a block whose first A falls in 150..162) only for the blocks near the view: the sensors never look
 * further, and 471 per-frame updates are not needed (INFERRED, harmless). */
static int vb_B(int A) { return A < 60 ? 14 : A < 150 ? (A - 59 >= 14 ? 0 : 14 - (A - 59)) : (A - 149 > 14 ? 14 : A - 149); }
static int vb_A(const obj_t *o) { return ((int)o->av[0] + S.t) % 210; }
static void vb_state(obj_t *o, int A) {
    int B = A >= 150 && A <= 162 ? A - 149 : vb_B(A);
    int solid = A >= 60 && A <= 162;                /* flag 0 off from A 60 (13034) until B reaches 14 at A 163 (13039) */
    o->fl = (o->fl & ~1) | (solid ? 0 : 1);
    o->hidden = B >= 14;
    o->av[1] = B;
    if (o->an.b) o->an.frame = B > 13 ? 13 : B;
}
static int vb_cmp_x(const void *a, const void *b) { float d = G.o[*(const short *)a].x - G.o[*(const short *)b].x; return d < 0 ? -1 : d > 0; }
static void vblocks(void) {
    if (!S.nvb) return;
    float x0 = G.camx - 96, x1 = G.camx + SCREEN_W + 96;
    int lo = 0, hi = S.nvb;                         /* first block with x >= x0 (the list is sorted by x) */
    while (lo < hi) { int m = (lo + hi) / 2; if (G.o[S.vb[m]].x < x0) lo = m + 1; else hi = m; }
    for (int k = lo; k < S.nvb; k++) {
        obj_t *o = &G.o[S.vb[k]];
        if (o->x > x1) break;
        if (!o->alive || o->y < G.camy - 160 || o->y > G.camy + SCREEN_H + 160) continue;
        int A = vb_A(o);
        vb_state(o, A);
        if ((A == 60 || A == 150) && in_view(o, 32)) {
            if (A == 60 && !S.snd328) { snd(328); S.snd328 = 1; }
            if (A == 150 && !S.snd329) { snd(329); S.snd329 = 1; }
        }
    }
}
int pl_gim_vblock_solid(int k) {
    for (int j = 0; j < S.nvb; j++) { obj_t *o = &G.o[S.vb[j]]; if ((int)o->av[2] == k) return !(o->fl & 1); }
    return -1;
}

/* ================================================================ 2. time gates (RE 12743, 12772-12779, 12849, col_9_50 / col_12_55)
 * gate (invisible, box collision 19 x 80, hot spot 9, 40): av[0] A = index, av[1] B count, av[3] D state 0 idle / 1 shrink / 2 grow.
 * piece: av[0] A = index mod 10, av[1] B = index / 10 (its gate), av[2] C radius (ctor 50), av[3] D angle (36 A at the start).
 * 12743 (per gate): D = (D + 3) mod 360, x = gate.x + 0.5 C cos D, y = gate.y - C sin D, scale 0.2 sin D + 0.8; 12773 / 12774: behind
 * the player while 90 < D < 270 (move_back), in front from 270 (move_front); 12775: idle gates' pieces anim 0 frame (g / 15) mod 2;
 * 12776-12779: an activated gate's pieces anim 7 and +3 more, C -5 a frame for 10 frames then +5 (max 50) for 10, back to idle.
 * col_9_50: PLAYER on an idle gate, Lilac (MV.A 0): SKIN flag 8 (the super boost: p->sboost), ENERGY 100, gate D 1, sound 324. */
static void tgates(player_t *p) {
    obj_t *g, *q;
    EACH(OT_PLG_TGATE, g) {                                                    /* col_9_50 (test_collisions: before the events) */
        if (g->av[3] == 0 && !dead(p) && rect_disc(g->x - 9, g->y - 40, g->x + 10, g->y + 40, p)) {
            p->sboost = 1; p->energy = 100; g->av[3] = 1; g->av[1] = 0; snd(324);
        }
    }
    EACH(OT_PLG_TGATE, g) {
        EACH(OT_PLG_TPIECE, q) {                                               /* 12772 -> 12743 */
            if (q->av[1] != g->av[0]) continue;
            q->av[3] = fmod(3 + q->av[3], 360.);
            q->x = (float)(int)(c_cos(q->av[3]) * q->av[2] * 0.5 + (int)g->x);
            q->y = (float)(int)((int)g->y - c_sin(q->av[3]) * q->av[2]);
            q->scale = (float)(c_sin(q->av[3]) * 0.2 + 0.8);
            if (!(q->fl & F2) && q->av[3] > 90 && q->av[3] < 270) { q->layer = 2; q->fl |= F2; }   /* 12773: move_back (behind the player) */
            if ((q->fl & F2) && q->av[3] >= 270) { q->layer = 3; q->fl &= ~F2; }                     /* 12774: move_front */
            if (g->av[3] == 0) { set_s(q, "plg_tpiece"); q->an.frame = (G.frame / 15) % 2; }    /* 12775 */
            else {                                                                                  /* 12776 / 12778 */
                set_s(q, "plg_tpiece7"); q->av[3] = fmod(3 + q->av[3], 360.);
                q->av[2] = g->av[3] == 1 ? q->av[2] - 5 : fmin(q->av[2] + 5, 50);
            }
        }
        if (g->av[3] != 0) {
            g->av[1] += 1;
            if (g->av[1] >= 10) { g->av[3] = g->av[3] == 1 ? 2 : 0; g->av[1] = 0; }    /* 12777 / 12779 */
        }
    }
}

/* ================================================================ 3. bows (RE 12968-12986 / 13011 / 13157; PL2 / PL5 12864-12890 /
 * 12958: the same rules over every bow; RV bow2). arrow av: [0] A x, [1] B y, [2] C speed, [3] E home x, [4] F home y, [5] G pull,
 * [6] H state (0 idle, 1 pulled back, 2 released, 3 flying, 4 stuck), [7] I string speed; ang = J; tm = K (ride counter), fx = L
 * (the rider's offset along the arrow), fy = N (pause), dir = M (index). string av: [0] A (-5..5, not 0), [1] B (its bow). */
static void ride_pose(player_t *p, const obj_t *a) {                          /* 12971: X = A + L cos J, Y = B - L sin J, OTHERS.Y 50, anim 40 dir 6 */
    double J = a->ang;
    p->x = c_cos(J) * a->fx + a->av[0];
    p->y = a->av[1] - a->fx * c_sin(J) + ORIG_REF_DY;
    p->vx = p->vy = p->gsp = 0; p->ground = 0;
    p->rot = (float)J;                                                         /* OTHERS.Z = J (the skin angle) */
    player_pose(p, 40, 6, -1); player_sync_sensors(p);
}
static void bows(player_t *p) {
    obj_t *a, *s, *w;
    EACH(OT_PLG_ARROW, a) {                                                    /* 12968: the strings and the arrow's position */
        EACH(OT_PLG_STRING, s) {
            if ((int)s->av[1] != a->dir) continue;
            double A = s->av[0];
            s->x = (float)(int)((a->av[3] - 40) + 0.2 * (5 - fabs(A)) * a->av[5]);
            s->y = (float)(int)((A > 1 ? 8 : A <= -1 ? -8 : 8 * A) + a->av[4] + A * -16);
        }
        a->x = (float)(int)a->av[0]; a->y = (float)(int)a->av[1];
    }
    int M = p->state == ST_RIDE && S.ride == 1 ? 99 : -1;
    EACH(OT_PLG_ARROW, a) {
        if (a->av[6] == 0 && p->vy > 0 && !hurt(p) && M != 99 && p->state != ST_RIDE && disc_hit(p, a)) {   /* 12969: grab (MV.G > 0, M not 8 / 9 / 10) */
            a->av[6] = 1; S.ride = 1; S.ride_obj = (int)(a - G.o); M = 99; p->state = ST_RIDE; p->state_t = 0;
            snd(33); a->fx = (float)(p->x - a->av[0]);
        }
    }
    a = S.ride == 1 ? &G.o[S.ride_obj] : NULL;
    if (a && (!a->alive || a->type != OT_PLG_ARROW)) { a = NULL; S.ride = 0; }
    if (a) {
        a->tm++;                                                               /* 12970 (and MV.B = G = 0) */
        if (a->av[6] > 0 && a->av[6] < 4) { ride_pose(p, a); a->tm = 0; }      /* 12971 */
        else if (a->tm > 2) { S.ride = 0; p->state = ST_NORMAL; p->rot = 0; }  /* 12972 */
    }
    EACH(OT_PLG_ARROW, a) {
        if (a->fx < 0) a->fx = (float)fmin(a->fx + 4, 0);                     /* 12973 / 12974: the rider slides to the middle */
        else if (a->fx > 0) a->fx = (float)fmax(a->fx - 4, 0);
        double *A = &a->av[0], *B = &a->av[1], *C = &a->av[2], *G_ = &a->av[5], *H = &a->av[6], *I = &a->av[7];
        if (*H == 1) { *G_ -= 2; *A -= 2; }                                    /* 12975 */
        if (*H == 1 && *G_ <= -50) { *H = 2; snd(325); }                      /* 12976 */
        if (*H == 2) { *A += 15; *G_ += 15; }                                 /* 12977 */
        if (*H == 2 && *G_ >= 0) { *H = 3; *I = 10; *C = 15; }                /* 12978 */
        if (*H >= 3 && *G_ > 0) *I = fmax(*I - 1, -10);                       /* 12979 */
        if (*H >= 3 && *G_ < 0) *I = fmin(*I + 1, 10);                        /* 12980 */
        if (*H >= 3) { *G_ += *I; *I *= 0.9; }                                /* 12981 */
        if (*H >= 3 && *G_ > -1 && *G_ < 1 && fabs(*I) < 1) { *G_ = 0; *I = 0; }   /* 12982 */
        if (a->fy > 0) a->fy -= 1;                                            /* 12983 */
        if (*H == 3 && a->fy <= 0) {                                          /* 12984: fly along J, J - 0.4 a frame */
            double J = a->ang;
            *A = c_cos(J) * *C + *A; *B = *B - *C * c_sin(J); a->ang = (float)(J - 0.4);
        }
        if (*H == 3) {
            a->x = (float)(int)*A; a->y = (float)(int)*B;                     /* (the overlap tests use the moved arrow, INFERRED order) */
            int bg = obj_overlaps_bg(a) || *A < 0 || *A > G.lvl.w || *B > G.lvl.h || *B < -200;   /* (out of the frame: stop, INFERRED safety) */
            if (bg && S.ride == 1 && &G.o[S.ride_obj] == a) {                 /* 12985: the rider leaves with the arrow's speed */
                double J = a->ang;
                S.ride = 0; p->state = ST_NORMAL; p->rot = 0; p->ground = 0;
                p->vx = c_cos(J) * *C; p->vy = -(c_sin(J) * *C);
            }
            if (bg) {                                                          /* 12986: stuck: an invisible Q8 platform at its angle, sound 326 */
                *H = 4;
                obj_t *pl = spawn(OT_PLG_ARROWPLAT, "plg_arrowplat", a->x, a->y, a->layer);
                if (pl) { pl->ang = a->ang; pl->hidden = 1; }
                snd(326);
            }
        }
        if (*H == 3 && a->fy <= 0) {                                          /* col_0_51 / col_16_51: rocks and enemies in its path */
            double J = a->ang, X = c_cos(J) * *C, Y = -(*C * c_sin(J));
            for (int i = 0; i < G.nobjs; i++) {
                w = &G.o[i];
                if (!w->alive || fabsf(w->x - a->x) > 160 || fabsf(w->y - a->y) > 160) continue;
                if (w->type == OT_FN_BWALL) { if (obj_obj_hit(a, w) && fn_gima_wall_hit(w, X, Y)) { player_hitstop(p, 5); a->fy = 3; } }
                else if (enemy_type(w->type) && !(w->fl & (F5 | 0x40)) && w->an.b && obj_obj_hit(a, w)) { w->A = fmin(w->A, 0); w->kx = X; w->ky = Y; a->fy = 3; }
            }
        }
    }
}

/* ================================================================ 4. pinwheels (PL1-PL3; RE 12780-12783, 12850; scenery)
 * av[0] A angle (start 30 k mod 360, move_back), av[1] B speed (ctor 3): angle = A, B = max(B - 0.05, 3), A -= B (12780); the player
 * within 39 px: B = min(B + |MV.B| / 5 + 1, 15) (12781, INFERRED reading of the min); in the cyclone (M 26): + 0.5 (12782); at B 15
 * a RingSparkle at the action point every 5th frame (12783). */
static void pinwheels(player_t *p) {
    obj_t *o;
    EACH(OT_PLG_PINWHEEL, o) {
        if (!in_view(o, 64)) continue;                                         /* (scenery: only near the view, INFERRED) */
        o->ang = (float)o->av[0];
        o->av[1] = fmax(o->av[1] - 0.05, 3); o->av[0] -= o->av[1];
        if (fabs(o->x - PX(p)) <= 39 && fabs(o->y - PY(p)) <= 39) {
            o->av[1] = fmin(o->av[1] + fabs(p->ground ? p->gsp : p->vx) / 5 + 1, 15);
            if (p->state == ST_CYCLONE && o->av[1] < 15) o->av[1] += 0.5;
        }
        if (o->av[1] >= 15 && (G.frame % 5) == 1) obj_effect(o->x + 18 - 24, o->y + 22 - 24, "ringsparkle");   /* action point (42, 22) - hot spot (24, 24) */
    }
}

/* ================================================================ 5. cyclone tops (RE 12761-12771, col_4_50; RV ct1 / ct2)
 * av: [0] A state (0 home, 1 ridden, 2 flying off), [1] B / [2] C speed, [3] D offset / x, [4] E y, [5] F / [6] G home, [7] H ride time.
 * col_4_50: S_BOTTOMBIG on a top at home while falling (MV.G > 0) and not hurt (M 8 / 9 / 10), or in the cyclone (M 26, SKIN.I <= 2):
 * A 1, D = top.x - MV.X, M 26 (the cyclone), sound 7 (RV ct1: 7 + 174), direction 1 (the spin). Ridden: top at (MV.X + D, MV.Y + 16),
 * the cyclone kept going (SKIN.I <= 2: port state_t capped, INFERRED), MV.G = min(G, -1) (RV ct1: rises 1 px a frame), ENERGY + 1
 * (12761); H + 1 (12762); D to 0 by 2 (12763 / 12764); H > 180: direction 2 (12766); released (A 2, B = MV.B, C = MV.G, D / E = its
 * position) by a jump press (12767: MV.G -6, sound 288, a white burst, M 26 again), the ceiling (12768: MV.Y + 4), leaving the cyclone
 * or H > 250 (12769; RV ct1: f340). Flying: x = D, y = E, D + B, E + C, C = min(C + 0.1, 15) (12770); out of the view grown by 100:
 * home, direction 0, H 0 (12771). */
static void top_release(obj_t *o, player_t *p) {
    o->av[0] = 2; o->av[1] = p->ground ? p->gsp : p->vx; o->av[2] = p->ground ? 0 : p->vy; o->av[3] = (int)o->x; o->av[4] = (int)o->y;
    if (S.top_ride == (int)(o - G.o)) S.top_ride = -1;
}
static void tops(player_t *p) {
    obj_t *o, *b;
    int pressed = (S.held & ~S.prev_held & BTN_JUMP) != 0;
    EACH(OT_PLG_CTOP, o) {                                                     /* col_4_50 */
        if (o->av[0] != 0 || S.top_ride >= 0 || dead(p) || p->state == ST_RIDE || p->vy <= 0 || p->ground) continue;
        int M26 = p->state == ST_CYCLONE;
        if ((hurt(p) || M26) && !(M26 && p->state_t <= 52)) continue;
        if (!sens_hit(SENS_BOTTOMBIG, o)) continue;
        o->av[0] = 1; o->av[3] = (int)o->x - PX(p); o->av[7] = 0; S.top_ride = (int)(o - G.o);
        if (p->state != ST_CYCLONE) { p->state = ST_CYCLONE; p->state_t = 0; }
        p->cyclone_used = 1; snd(7); snd(174); set_s(o, "plg_ctop1");
    }
    EACH(OT_PLG_CTOP, o) {
        if (o->av[0] == 1) {
            if (p->state == ST_CYCLONE && p->state_t > 40) p->state_t = 40;    /* SKIN.I <= 2 (12761): the cyclone does not run out (INFERRED mapping) */
            o->x = (float)(int)(p->x + o->av[3]); o->y = (float)(int)(MY(p) + 16);   /* 12761 */
            p->vy = fmin(p->vy, -1); p->ground = 0;
            p->energy = fminf(100.f, p->energy + 1.f);
            if (p->hitstop <= 0) o->av[7] += 1;                               /* 12762 */
            if (o->av[3] < 0) o->av[3] = fmin(o->av[3] + 2, 0);              /* 12763 */
            else if (o->av[3] > 0) o->av[3] = fmax(o->av[3] - 2, 0);         /* 12764 */
            if (o->av[7] > 180) set_s(o, "plg_ctop2");                         /* 12766 */
            if (pressed) {                                                     /* 12767: the leap */
                top_release(o, p); p->vy = -6; p->ground = 0; snd(288);
                p->state = ST_CYCLONE; p->state_t = 0;
                b = spawn(OT_PLG_BURST, "plg_burst", o->x, o->y, 3); if (b) b->layer = 3;
            }
            else if (mv_z(SENSOR_TOPBIG)) { top_release(o, p); p->y += 4; }   /* 12768 */
            else if (p->state != ST_CYCLONE && p->state != ST_ATTACK) top_release(o, p);   /* 12769: M not 26 / 20 */
            else if (o->av[7] > 250) top_release(o, p);                        /* 12769 (RV ct1: H 251 at f340) */
        }
        else if (o->av[0] == 2) {                                              /* 12770 */
            o->x = (float)(int)o->av[3]; o->y = (float)(int)o->av[4];
            o->av[3] += o->av[1]; o->av[4] += o->av[2]; o->av[2] = fmin(o->av[2] + 0.1, 15);
            if (!in_view(o, 100)) { o->av[0] = 0; o->x = (float)o->av[5]; o->y = (float)o->av[6]; set_s(o, "plg_ctop0"); o->av[7] = 0; }   /* 12771 */
        }
    }
    EACH(OT_PLG_BURST, b) if (!b->an.b || b->an.done) b->alive = 0;
}

/* ================================================================ 6. breakout rooms (PL4 13019-13023 / 13042-13056, PL5 13108 /
 * 13119 / 13128 / 13129; RV bo4). ball av: [0] A x speed (ctor -2), [1] B y speed (2), [2] C paddle offset; fl F1 flag 1 (awake);
 * tm = its paddle slot. The k-th ball goes with the k-th paddle and mask (placed together); walls: invisible 16 x 128 boxes (left hot
 * spot 15, 64; right 0, 64). Awake once inside the view grown by 70 (13046); moves x + A, y + B while inside the view grown by 180
 * (13047); C = wall.x - x + 64 for a left wall within [x - 63, x), wall.x - x - 64 for a right one within (x, x + 63] (13019 / 13020;
 * RV bo4: C 5 .. 45 / -40); paddle and mask x = ball.x + C, paddle A = A (1 - |C|) only while |C| < 1 (13042); the player standing on
 * the paddle moves with A (13043). Bounces: the ball's lower probe (16 x 2 px under it) on the background or the paddle: B = -2 and the
 * paddle flashes (13050 / 13054); the upper probe on the background: B = 2 (13052); A >= 0 with a right wall's x - 15 <= x: A = -A,
 * EffectRebound (no sound: the RV bo4 327s are the obelisk laser's, 13076); A <= 0 with x <= left wall's x + 15: the same (13021-13023;
 * RV bo4 flips near x 2030 / 2250). Damage 5: the mask (spiked
 * ends) unless the metal shield (13044), the ball unless the water shield (13045). Rebound effects: additive, alpha - 4 a frame (13053,
 * 13055 / 13056). */
static void bwall_box(const obj_t *w, float *x0, float *y0, float *x1, float *y1) {
    *x0 = w->x - (w->par ? 0 : 15); *x1 = *x0 + 16; *y0 = w->y - 64; *y1 = w->y + 64;
}
static int bg_rect(int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; y++) if (px_span_solid(&G.lvl, x0, x1 - 1, y)) return 1;
    return 0;
}
static int rect_obj(const obj_t *o, int x0, int y0, int x1, int y1) { return o && o->alive && o->an.b && obj_rect_hit(o, x0, y0, x1, y1); }
static void breakout(player_t *p) {
    obj_t *b, *w, *r;
    EACH(OT_PLG_BBALL, b) {
        obj_t *pd = b->tm >= 0 && G.o[b->tm].alive && G.o[b->tm].type == OT_PLG_BPLAT ? &G.o[b->tm] : NULL;
        obj_t *mk = b->dir >= 0 && G.o[b->dir].alive && G.o[b->dir].type == OT_PLG_BMASK ? &G.o[b->dir] : NULL;
        if (!(b->fl & F1) && in_view(b, 70)) b->fl |= F1;                     /* 13046 */
        if ((b->fl & F1) && in_view(b, 180)) { b->x += (float)b->av[0]; b->y += (float)b->av[1]; }   /* 13047 */
        int x = (int)b->x, y = (int)b->y;
        b->av[2] = 0;                                                          /* 13048 / 13019 / 13020 */
        EACH(OT_PLG_BWALL, w) {
            if (fabsf(w->y - b->y) > 80) continue;
            int wx = (int)w->x;
            if (w->par == 0 && x - 63 <= wx && wx < x) b->av[2] = wx - x + 64;
            if (w->par == 1 && x < wx && wx <= x + 63) b->av[2] = wx - x - 64;
        }
        if (pd) {                                                              /* 13042 */
            pd->x = (float)(int)(b->x + b->av[2]); if (mk) mk->x = pd->x;
            double c = fabs(b->av[2]); pd->av[0] = (c < 1 ? 1 - c : 0) * b->av[0];
            if (!dead(p) && p->ground && pd->av[0] != 0 && sens_hit(SENS_BOTTOMBIG, pd)) { p->x += pd->av[0]; player_sync_sensors(p); }   /* 13043 */
        }
        if (b->av[1] > 0 && (bg_rect(x - 8, y + 16, x + 8, y + 18) || rect_obj(pd, x - 8, y + 16, x + 8, y + 18))) {   /* 13049 / 13050 / 13054 */
            b->av[1] = -2; if (pd) { set_s(pd, "plg_bplat7"); anim_restart(&pd->an); }
        }
        else if (b->av[1] < 0 && bg_rect(x - 8, y - 18, x + 8, y - 16)) b->av[1] = 2;   /* 13051 / 13052 */
        EACH(OT_PLG_BWALL, w) {                                                /* 13021-13023 */
            float x0, y0, x1, y1; bwall_box(w, &x0, &y0, &x1, &y1);
            if (b->y < y0 || b->y >= y1) continue;
            int wx = (int)w->x;
            if (w->par == 1 && b->av[0] >= 0 && x >= wx - 15 && x < wx) {
                b->av[0] = -b->av[0]; r = spawn(OT_PLG_REBOUND, "plg_reb0", (float)(wx - 1), b->y, 3); if (r) r->hidden = 1;
            }
            else if (w->par == 0 && b->av[0] <= 0 && x <= wx + 15 && x > wx) {
                b->av[0] = -b->av[0]; r = spawn(OT_PLG_REBOUND, "plg_reb1", (float)(wx + 1), b->y, 3); if (r) r->hidden = 1;
            }
        }
        if (pd && pd->an.b && pd->an.done) set_s(pd, "plg_bplat");
        if (!dead(p) && vulnerable(p)) {
            if (mk && p->shield != 3 && disc_hit(p, mk)) player_contact(p, 5);  /* 13044 */
            else if (p->shield != 4 && disc_hit(p, b)) player_contact(p, 5);    /* 13045 */
        }
    }
    EACH(OT_PLG_REBOUND, r) { r->av[0] += 4; if (r->av[0] >= 255) r->alive = 0; }   /* 13053 / 13055 / 13056 */
}

/* ================================================================ 7. giant swords (PL5 / PL6; RE 13130-13137, 13146, col_19_51)
 * sword (invisible Q20 hitbox, A 99, flag 5 armoured): av[3] D state, av[5] F timer, av[4] E fall speed; bd = B contact damage.
 * 13130: D 0 and |sword.x - PLAYER.x| <= 39, PLAYER.y - 299 <= sword.y < PLAYER.y: D 1, sound 150; 13131 / 13132: F + 1, F > 20: D 2,
 * E 10; 13133: y + E, B 15; 13134: on the background: D 3, sound 51, B 0. Masks: at their sword, anim 7 (the warning) while D 1
 * (13135-13137). col_19_51: the falling sword breaks a monitor (as the bomb blast). */
static void swords(player_t *p) {
    obj_t *o, *m;
    EACH(OT_PLG_SWORD, o) {
        double D = o->av[3];
        if (D == 0 && !dead(p) && fabs(o->x - PX(p)) <= 39 && o->y < PY(p) && o->y >= PY(p) - 299) { o->av[3] = 1; snd(150); }   /* 13130 */
        if (o->av[3] == 1) { o->av[5] += 1; if (o->av[5] > 20) { o->av[3] = 2; o->av[4] = 10; } }   /* 13131 / 13132 */
        if (o->av[3] == 2) {
            o->y += (float)o->av[4]; o->bd = 15;                               /* 13133 */
            if (obj_overlaps_bg(o) || o->y > G.lvl.h + 200) { o->av[3] = 3; o->bd = 0; snd(51); }   /* 13134 */
            for (int i = 0; i < G.nobjs; i++) {                                /* col_19_51 */
                obj_t *q = &G.o[i]; int t = q->type;
                if (!q->alive || fabsf(q->x - o->x) > 64 || fabsf(q->y - o->y) > 160) continue;
                if (t == OT_MONITOR || t == OT_MON_RINGS || t == OT_MON_1UP || t == OT_MON_INVINC || t == OT_MON_KEYSTONE ||
                    (t >= OT_MON_WOOD && t <= OT_MON_WATER) || t == OT_MON_BOMB || t == OT_MON_CRATE)
                    if (obj_obj_hit(o, q)) q->fl |= 0x200;                     /* (objects.c: the bomb-blast break) */
            }
        }
        if (o->tm >= 0 && G.o[o->tm].alive && G.o[o->tm].type == OT_PLG_SWMASK) {   /* 13135-13137 */
            m = &G.o[o->tm]; m->x = o->x; m->y = o->y;
            set_s(m, o->av[3] == 1 ? "plg_swmask7" : "plg_swmask");
        }
    }
}

/* ================================================================ 8. spinballs (PL4-PL6; RE 13040 / 13118 / 13225, 13041, 5845)
 * ball: av[0] A / av[1] B centre (its start, 5845), av[3] D angle: x = A + 120 cos D, y = B - 120 sin D; chains at radius chain.A
 * (ctor 24 / 48; 72 / 96 for chains 3 / 4, INFERRED: their constructors were not decompiled) on the same angle; D = (D + 2) mod 360.
 * 13041: PLAYER on the ball: 10 damage unless the metal shield. */
static void spinballs(player_t *p) {
    obj_t *o, *c;
    EACH(OT_PLG_SPINBALL, o) {
        double D = o->av[3];
        o->x = (float)(int)(c_cos(D) * 120 + o->av[0]); o->y = (float)(int)(c_sin(D) * -120 + o->av[1]);
        EACH(OT_PLG_CHAIN, c) {
            if (c->tm != (int)(o - G.o)) continue;
            c->x = (float)(int)(c_cos(D) * c->av[0] + o->av[0]); c->y = (float)(int)(o->av[1] - c_sin(D) * c->av[0]);
        }
        o->av[3] = fmod(D + 2, 360.);
        if (vulnerable(p) && p->shield != 3 && disc_hit(p, o)) player_contact(p, 10);   /* 13041 */
    }
}

/* ================================================================ 9. hanging PL switch + stone doors (PL3 / PL6; RE 12987-12993, 13012;
 * RV sd3). door1 (par 0) holds the state: av[0] A (1 closed .. 0 open), av[1] B (the acceleration), F1 flag 1 (opening), F2 flag 2
 * (the end sound played). The idle switch (relic_gimb.c's OT_RM_IDLESW, its own events 5866-5868 absent here: RMSW_EXTERNAL) sits at
 * y = rest + 64 (1 - A) with the PL switch drawn on it (12987); a player hanging on it (Q > 0) while the door is closed opens it: B 0,
 * flag 1, sound 62 (12989; RV sd3: f62, one frame after the grab); A - B (opening) / + B (closing), B + 0.0005 a frame (12990 / 12991;
 * RV sd3: open at f125); sound 132 at either end once (12992 / 12993). Doors 1 / 2 are drawn x-scaled by A from their hot spots, 3 / 4
 * by 1 - A at the action points of 1 / 2, tinted (127 + 128 A, 191 + 64 A, 127 + 127 A) (12988).
 * The scene (13009 PL3 / 13330 PL6: the walk to door1.x + 48, C, the fade, PL3's next act / PL6's arena) is pl_flow.c's: this file
 * reports door1.x / door2.x / A every frame (pl_flow_door_report); while it runs (pl_flow_door_busy) the door closes (flags cleared,
 * B 0) and once she stands at door1.x + 48 doors 1 / 3 / 4 are drawn in front of her (13002). */
static obj_t *door_part(int k) { obj_t *o; EACH(OT_PLG_SDOOR, o) if (o->par == k) return o; return NULL; }
static void doors(player_t *p) {
    obj_t *d1 = door_part(0), *d2 = door_part(1), *o, *sw = NULL;
    if (!d1) return;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_RM_IDLESW) { sw = &G.o[i]; break; }
    double *A = &d1->av[0], *B = &d1->av[1];
    if (sw) {                                                                  /* 12987 */
        float ny = (float)(int)((1 - *A) * 64 + sw->av[0]), dy = ny - sw->y;
        int held = p->state == ST_HANG && p->hang_x == sw->x && p->hang_y == sw->y;
        sw->y = ny;
        if (held && dy != 0) { p->hang_y = sw->y; p->y += dy; player_sync_sensors(p); }   /* the hanging player rides the handle (4062) */
        EACH(OT_PLG_PLSWITCH, o) { o->x = sw->x; o->y = sw->y; }
        if (!(d1->fl & F1) && held) { *B = 0; d1->fl |= F1; d1->fl &= ~F2; snd(62); }   /* 12989 */
    }
    if (d1->fl & F1) { *A = fmax(*A - *B, 0); *B += 0.0005; }                /* 12990 */
    else { *A = fmin(*A + *B, 1); *B += 0.0005; }                            /* 12991 */
    if (!(d1->fl & F2) && (((d1->fl & F1) && *A <= 0) || (!(d1->fl & F1) && *A >= 1))) { snd(132); d1->fl |= F2; }   /* 12992 / 12993 */
    int rep = d2 != NULL;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_PLDOOR")) rep = 0;                                      /* host tests: pl_flow.c's stand-in report replaces this one */
#endif
    if (rep) pl_flow_door_report(d1->x, d2->x, (float)*A);                     /* 13009 / 13330 run in pl_flow.c (the walk, the fade, the exit) */
    int busy = pl_flow_door_busy();
    if (busy && !S.door_c) { d1->fl &= ~(F1 | F2); *B = 0; }                   /* the scene starts: the door closes behind her (13009 / 13330) */
    if (busy && !S.door_enter && fabs(PX(p) - (d1->x + 48)) < 1) {          /* 13002: there; doors 1 / 3 / 4 drawn in front of her */
        S.door_enter = 1; EACH(OT_PLG_SDOOR, o) if (o->par != 1) o->layer = 3;
    }
    S.door_c = busy;
}

/* ================================================================ passes */
void pl_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); snap_register(LOFF, sizeof LOFF); snap_register(LCAP, sizeof LCAP); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS); lst_layout(); S.top_ride = -1;
    /* objects.c path platforms (3283-3289; RE ctors): LevelLagoonPlatform1 F 2 G 50 H 1, LevelLagoonPlatform2 E 90 F 2 G 128 H 1 (q12) */
    objects_plat_type(OT_PLG_LPLAT, 2, 50);
    objects_plat_type(OT_PLG_LPLAT2, 2, 128);
    if (!objects_have_from(RT0)) return;
    int seen[NRT] = { 0 }, nvb = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < RT0 || o->type > OT_PLG_END) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0x7f;
        float E = o->vx;                                                       /* the path platforms keep their registry init */
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = -1; o->c = 0; o->dir = -1; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->vx = E;
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x > G.lvl.w || o->y > G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        if (o->type == OT_PLG_VBLOCK) { if (nvb < VBMAX) { S.vb[nvb] = (short)i; o->av[0] = (6 * nvb) % 210; nvb++; } S.on = 1; continue; }   /* 13105 / 13144 */
        lst_add(o); S.on = 1;
    }
    if (!S.on) return;
    obj_t *o, *q;
    /* vanishing blocks: k-th in instance order (A0 = 6 k mod 210); the x-sorted list for the view culling */
    S.nvb = nvb;
    for (int k = 0; k < nvb; k++) { o = &G.o[S.vb[k]]; set_s(o, "plg_vblock"); o->av[2] = k; vb_state(o, vb_A(o)); }
    qsort(S.vb, (size_t)nvb, sizeof S.vb[0], vb_cmp_x);
    { int k = 0; EACH(OT_PLG_TGATE, o) { o->av[0] = k++; o->an.b = NULL; } }   /* 12849: A = index */
    { int k = 0; EACH(OT_PLG_TPIECE, o) { set_s(o, "plg_tpiece"); o->av[0] = k % 10; o->av[3] = 36 * o->av[0]; o->av[1] = k / 10; o->av[2] = 50; o->layer = 3; k++; } }
    { int k = 0; EACH(OT_PLG_ARROW, o) { set_s(o, "plg_arrow"); o->dir = k++; o->av[3] = o->av[0] = (int)o->x; o->av[4] = o->av[1] = (int)o->y; } }   /* 13011 / 12958 */
    { int k = 0; EACH(OT_PLG_STRING, o) { set_s(o, "plg_string"); o->av[0] = (k % 10) - 5; if (o->av[0] == 0) o->av[0] = 5; o->av[1] = k / 10; k++; } }   /* 13011 / 12871 */
    { int k = 0; EACH(OT_PLG_PINWHEEL, o) { set_s(o, "plg_pinwheel"); o->av[0] = fmod(30. * k++, 360.); o->av[1] = 3; } }   /* 12850, ctor B 3 */
    EACH(OT_PLG_CTOP, o) { set_s(o, "plg_ctop0"); o->av[5] = (int)o->x; o->av[6] = (int)o->y; }
    EACH(OT_PLG_BWALL, o) o->an.b = NULL;
    EACH(OT_PLG_BBALL, o) {                                                    /* ctor A -2 B 2; the paddle / mask placed with it */
        set_s(o, "plg_bball"); o->av[0] = -2; o->av[1] = 2; o->tm = -1; o->dir = -1;
        float best = 1e9f;
        EACH(OT_PLG_BPLAT, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } }
        best = 1e9f;
        EACH(OT_PLG_BMASK, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->dir = (int)(q - G.o); } }
    }
    EACH(OT_PLG_BPLAT, o) set_s(o, "plg_bplat");
    EACH(OT_PLG_BMASK, o) set_s(o, "plg_bmask");
    EACH(OT_PLG_LPLAT, o) set_s(o, "plg_lplat");
    EACH(OT_PLG_LPLAT2, o) { set_s(o, "plg_lplat"); o->vx = 90; }             /* ctor E 90 */
    EACH(OT_PLG_SWORD, o) {                                                    /* ctor A 99, 13146 flag 5 */
        set_s(o, "plg_sword"); o->A = 99; o->hp = 99; o->fl |= F5; o->hidden = 1; o->tm = -1;
        EACH(OT_PLG_SWMASK, q) if (fabsf(q->x - o->x) < 2 && fabsf(q->y - o->y) < 2) o->tm = (int)(q - G.o);
    }
    EACH(OT_PLG_SWMASK, o) set_s(o, "plg_swmask");
    EACH(OT_PLG_SPINBALL, o) { set_s(o, "spinball"); o->av[0] = o->x; o->av[1] = o->y; }   /* 5845: A = x, B = y */
    EACH(OT_PLG_CHAIN, o) {                                                    /* ctor A 24 / 48 (72 / 96 INFERRED); the nearest ball */
        set_s(o, "plg_chain"); o->av[0] = 24 * (o->par + 1);
        float best = 1e9f;
        EACH(OT_PLG_SPINBALL, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } }
    }
    EACH(OT_PLG_PLSWITCH, o) set_s(o, "plg_plswitch");
    EACH(OT_PLG_SDOOR, o) {
        static const char *const DN[4] = { "plg_sdoor0", "plg_sdoor1", "plg_sdoor2", "plg_sdoor3" };
        set_s(o, DN[o->par & 3]); o->hidden = 1;                               /* drawn by pl_gim_draw (x scale) */
        if (o->par == 0) { o->av[0] = 1; o->fl |= F2; }                        /* ctor A 1; 13012: flag 2 (no end sound at the start) */
    }
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_RM_IDLESW && N(OT_PLG_SDOOR)) G.o[i].fl |= RMSW_EXTERNAL;   /* 13012: A = y (relic_gimb.c) */
    EACH(OT_PLG_ACTIVE, o) set_s(o, "plg_active");
}
void pl_gim_pre(player_t *p) { (void)p; }
void pl_gim_frame(player_t *p) {
    if (!S.on) return;
    S.t++; S.snd328 = S.snd329 = 0;
    S.prev_held = S.held; S.held = p->held;
    vblocks();
    if (N(OT_PLG_TGATE)) tgates(p);
    if (N(OT_PLG_ARROW)) bows(p);
    if (N(OT_PLG_PINWHEEL)) pinwheels(p);
    if (N(OT_PLG_CTOP) || N(OT_PLG_BURST)) tops(p);
    if (N(OT_PLG_BBALL) || N(OT_PLG_REBOUND)) breakout(p);
    if (N(OT_PLG_SWORD)) swords(p);
    if (N(OT_PLG_SPINBALL)) spinballs(p);
    if (N(OT_PLG_SDOOR)) doors(p);
#ifdef FP_HOST_ENV
    { static int dbg = -1; if (dbg < 0) dbg = getenv("FP_DBG_PLG") != NULL;
      if (dbg) printf("plg %d ride %d top %d sboost %d en %.2f nobjs %d\n", G.frame, S.ride, S.top_ride, p->sboost, p->energy, G.nobjs); }
#endif
}
int pl_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }   /* (the families run in pl_gim_frame) */
static void draw_xs(const obj_t *o, float xs, uint32_t col) {
    if (!o->an.b || xs <= 0) return;
    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
    if (sx < -200 || sx > SCREEN_W + 200 || sy < -200 || sy > SCREEN_H + 200) return;
    r_set_xscale(xs); frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, 0, col); r_set_xscale(1.f);
}
void pl_gim_draw(int pass) {
    if (!S.on || (pass != 1 && pass != 2)) return;
    obj_t *o;
    if (N(OT_PLG_SDOOR)) {                                                     /* 12988: the doors, x-scaled, tinted by A */
        obj_t *d1 = door_part(0), *d2 = door_part(1);
        double A = d1 ? d1->av[0] : 1;
        uint32_t col = 0xff000000u | (uint32_t)((int)(A * 128 + 127) & 0xff) << 16 | (uint32_t)((int)(64 * A + 191) & 0xff) << 8 | (uint32_t)((int)(A * 127 + 127) & 0xff);
        EACH(OT_PLG_SDOOR, o) {
            if ((pass == 1) != (o->layer < 3)) continue;
            if (o->par == 2 && d1) { o->x = (float)(int)(d1->x + 47 * A); o->y = d1->y; }       /* door 3 at door 1's action point */
            if (o->par == 3 && d2) { o->x = (float)(int)(d2->x - 47 * A); o->y = d2->y; }       /* door 4 at door 2's */
            draw_xs(o, o->par < 2 ? (float)A : (float)(1 - A), o->par < 2 ? col : 0xffffffffu);
        }
    }
    if (pass == 2) {
        EACH(OT_PLG_REBOUND, o) {                                              /* additive (ctor shader 0x20), fading */
            if (!o->an.b) continue;
            int a = 255 - (int)o->av[0]; if (a <= 0) continue;
            r_set_additive(1); frame_draw(o->an.b, anim_frame(&o->an), floorf(o->x - G.camx), floorf(o->y - G.camy), 0, (uint32_t)a << 24 | 0xffffff); r_set_additive(0);
        }
    }
}
int pl_gim_q20(int type) { return type == OT_PLG_SWORD; }
/* ST_RIDE (player.c): the bow arrow (M 99) replaces the player block (12971 places her) */
int pl_gim_player(player_t *p, const input_t *in) {
    if (!S.ride) return 0;
    p->held = in->held;
    const obj_t *a = &G.o[S.ride_obj]; if (a->alive && a->type == OT_PLG_ARROW) ride_pose(p, a); else { S.ride = 0; p->state = ST_NORMAL; }
    return 1;
}
int pl_gim_ride_m(void) { return S.ride == 1 ? 99 : -1; }
int pl_gim_player_hidden(void) { return 0; }
