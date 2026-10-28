/* Thermal Base gimmicks (session 21c): types 1888-1967 (tb.h OT_TBG_*). Spec: build/re/spec_tb_gimmicks.md.
 * Labels: RE (decompiled events / constructors of frames 44-49 = handle_frame_45..50), RV (original traces
 * build/orig_traces/tb_gim), INFERRED (marked).
 * Families (event numbers in the spec): lifts (11709-11719, 11881, 11919-11929, 12525-12532), key cards + terminals +
 * locked doors (11802-11814, foreach_door_44, 11893; TB4 12448-12459), metal doors + wall lights (11826-11838), fire platforms
 * + flames (11720-11728, 11930-11938), scrolling platforms + masks + flames + push (11729-11752, 11890), pistons (11940 /
 * 11941 / 12068), spin platforms (11940), air vents (no event), lava pits / falls / warnings / magma (11943-11956,
 * foreach_lvf_45, 12249-12279), explosive blocks + switch buttons + destroyer (11957-11965, foreach_ec_45), floor traps
 * (12070-12072, test_collisions_47), conveyors (12388 / 12389), pipe loops (Q10 / Q11 pieces, 12337-12340), crank platforms
 * (12383-12387), giant pressers (12504-12510), turbines / bubbles / water lights (12391-12393, 12511-12519), water levers
 * (12486-12501).
 * Reused systems: objects.c path-platform registry (objects_plat_path: fire / piston / spin / scrolling platforms), the q7 /
 * q8 / q10 / q11 / q23 collision groups (objdefs.c flags), the wall-plate press rule (switches_wall_push), obj_hang_try (water
 * level masks), player_burn (the fire hurt shared with sb_gim.c), obj_explode.
 * Passes: tb_gim_pre (before the movement: the lifts' Y = J, the crank tops), tb_gim_obj (per object: hang handles; the
 * families run in tb_gim_frame), tb_gim_frame (after the player block, in event order), tb_gim_draw (pass 0-2: the x-scaled
 * spin arms; pass 3: the key card HUD).
 * Object fields: par = family member (importer TBG_SUB), layer = instance layer; av[] = class alterables named per family. */
#include "tb.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#endif

#define RAD 57.29577951308232
#define RT0 OT_TBG_LIFT
#define NRT (OT_TBG_END - RT0 + 1)
#define LMAX 64
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]
#define F1 0x1000                                  /* the object's flag 1 */
#define F2 0x2000                                  /* flag 2 */
#define F3 0x4000                                  /* flag 3 */
#define F6 0x8000                                  /* flag 6 */
static struct {
    int on, keys, presses, start_ride, pist_gnd;
    int conv_push;                                 /* this frame's conveyor push (tests) */
    int elev, elev_trig;                           /* TB5 elevator: C (0 waiting, 1 rising, 2 up), the CameraTriggerBoss slot */
    int lever_on[2];                               /* the water levers pulled (12489 / 12497) */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / RAD); }                  /* Chowdren sin: 0 at exactly 180 */
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / RAD); }   /* cos: 0 at exactly 90 / 270 */
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int R(int n) { return cross_rand(n); }
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
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }       /* !is_near_border(-n) */
static void snd(int id) { snd_play_orig(id, 220, 128); }
static int sens_hit(int s, const obj_t *o) {                                    /* a placed player sensor overlapping the object */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int disc_hit(const player_t *p, const obj_t *o) {                        /* PLAYER (Player_326: the 27-px disc) */
    static signed char x0[27], x1[27]; static int init;
    if (!init) { init = 1; for (int j = 0; j < 27; j++) { double d = 13.25 * 13.25 - (j - 13) * (j - 13); int w = d < 0 ? -1 : (int)sqrt(d); x0[j] = (signed char)(13 - w); x1[j] = (signed char)(13 + w); } }
    return o->an.b && obj_rows_hit(o, (int)PX(p) - 13, (int)PY(p) - 13, x0, x1, 27);
}
static void img_box(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {   /* the current image's box */
    if (!o->an.b) { *x0 = *x1 = o->x; *y0 = *y1 = o->y; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int box_ov(const obj_t *a, const obj_t *b) {
    float a0, b0, a1, b1, c0, d0, c1, d1; img_box(a, &a0, &b0, &a1, &b1); img_box(b, &c0, &d0, &c1, &d1);
    return a0 < c1 && c0 < a1 && b0 < d1 && d0 < b1;
}
/* the n-th member (placement order) of a type */
static obj_t *nth(int T, int n) { obj_t *o; int i = 0; EACH(T, o) if (i++ == n) return o; return NULL; }

int tb_gim_keycards(void) { return S.keys; }
int tb_gim_button_presses(void) { return S.presses; }

/* ================================================================ 1. lifts (RE 11709-11719, 11881; RV lift1 / lift2)
 * LevelLift_2649 (96 x 96 box, q7 q13; J its target y: Y = floor(J) in the platform block), LiftTop / LiftBottom (invisible
 * 64 x 16 boxes). av[0] J, av[1] Q (0 at the bottom, 1 rising, 2 at the top, 3 sinking), av[2] R (arrival countdown).
 * Start (on_frame_*_start): a lift on a LiftTop has Q 2. 11709: flag 1 off. 11710: grounded, S_BB on it, PLAYER above it
 * within 31 px and Q even: flag 1. 11711: R > 0, Q even: R - 1. 11712: flag 2, not 1 / 3, R <= 0, Q even: flag 2 off.
 * 11713: flag 1, not 2 / 6, Q even: flag 6, Q + 1. 11714 / 11717: Q 1: J - 8, Q 3: J + 8. 11715 / 11718: S_BB on it, grounded:
 * MV.Y -/+ 8 (and the sensors re-placed). 11716 / 11719: arrival on its top / bottom (J past it): J = its y, Q 2 / 0, sound 49,
 * CAM.Z + 3, flag 2, R 3. 11881 (TB1): the intro card done, no checkpoint, PLAYER in a lift: Q 1, flag 3 (the start ride).
 * Flag 6 is never cleared: each lift travels once (RV lift2: back on it at the bottom, it stays). */
static obj_t *lift_end(const obj_t *l, int bottom) {   /* the LiftTop / LiftBottom of its shaft (the 64-px box under the 96-px cabin) */
    obj_t *e;
    EACH(OT_TBG_LIFTEND, e) if (e->par == bottom && fabsf(e->x + 32 - l->x) < 48) return e;
    return NULL;
}
/* TB5's elevator (RE 12524-12532): three lifts without LiftTop / Bottom under the act's CameraTriggerBoss. 12524: MV.X past the
 * trigger's x + 96, its C 0: C 1 (CAM.M 1: the camera script, bosses). 12525: C 1: every lift J - 3; 12529 / 12530: the player
 * on it rises with it (MV.Y - 3); 12532: a lift up at the trigger's y: y = that, C 2, CAM.Z + 3, sound 51 (CAM.M 2, the music
 * lock and the elites riding along belong to the encounter: tb_boss.c). av[1] = -1 marks these lifts. */
static void elevator(player_t *p) {
    obj_t *o;
    if (S.elev_trig < 0 || S.elev_trig >= G.nobjs || !G.o[S.elev_trig].alive) return;
    const obj_t *t = &G.o[S.elev_trig];
    if (S.elev == 0 && !dead(p) && PX(p) > t->x + 96) S.elev = 1;                                  /* 12524 */
    if (S.elev != 1) return;
    int carried = 0;
    EACH(OT_TBG_LIFT, o) if (o->av[1] < 0) {
        o->av[0] -= 3;                                                                            /* 12525 */
        if (!carried && !dead(p) && sens_hit(SENS_BOTTOMBIG, o)) { p->y -= 3; player_sync_sensors(p); carried = 1; }   /* 12529 / 12530 */
        if (o->av[0] <= t->y) { o->av[0] = t->y; S.elev = 2; }                                    /* 12532 */
    }
    if (S.elev == 2) { G.cam.Z += 3; snd(51); }
}
int tb_gim_elevator(void) { return S.elev; }
static void lifts_pre(void) {
    obj_t *o;
    EACH(OT_TBG_LIFT, o) o->y = (float)floor(o->av[0]);                          /* 3289: Y = floor(N), N = J (G 0) */
}
static int on_lift(const player_t *p, const obj_t *o) { return p->ground && sens_hit(SENS_BOTTOMBIG, o); }
static void lifts(player_t *p) {
    obj_t *o;
    EACH(OT_TBG_LIFT, o) o->fl &= ~F1;                                            /* 11709 */
    if (!S.start_ride && !dead(p)) {                                              /* 11881: once, at the act start */
        S.start_ride = 1;
        EACH(OT_TBG_LIFT, o) if (o->av[1] >= 0 && (disc_hit(p, o) || on_lift(p, o))) { o->av[1] = 1; o->fl |= F3; }
    }
    if (S.elev_trig >= 0) elevator(p);
    EACH(OT_TBG_LIFT, o) {
        if (o->av[1] < 0) continue;                                                              /* TB5's elevator */
        int q = (int)o->av[1];
        if (!dead(p) && on_lift(p, o) && PY(p) < o->y && fabs(PX(p) - o->x) <= 31 && q % 2 == 0) o->fl |= F1;   /* 11710 */
        if (o->av[2] > 0 && q % 2 == 0) o->av[2] -= 1;                                                       /* 11711 */
        if ((o->fl & F2) && !(o->fl & (F1 | F3)) && o->av[2] <= 0 && q % 2 == 0) o->fl &= ~F2;              /* 11712 */
        if ((o->fl & F1) && !(o->fl & (F2 | F6)) && q % 2 == 0) { o->fl |= F6; o->av[1] = q + 1; }           /* 11713 */
        q = (int)o->av[1];
        if (q == 1) { o->av[0] -= 8; if (!dead(p) && on_lift(p, o)) { p->y -= 8; player_sync_sensors(p); } }   /* 11714 / 11715 */
        if (q == 3) { o->av[0] += 8; if (!dead(p) && on_lift(p, o)) { p->y += 8; player_sync_sensors(p); } }   /* 11717 / 11718 */
        obj_t *e = q == 1 ? lift_end(o, 0) : q == 3 ? lift_end(o, 1) : NULL;
        if (e && ((q == 1 && o->av[0] <= e->y) || (q == 3 && o->av[0] >= e->y))) {                          /* 11716 / 11719 */
            o->av[0] = e->y; o->av[1] = q == 1 ? 2 : 0;
            snd(49); G.cam.Z += 3; o->fl |= F2; o->av[2] = 3;
        }
    }
}

/* ================================================================ 2. key cards, terminals, locked doors (RE 11802-11814,
 * foreach_door_44, 11893; RV: sounds 37 / 285 / 286 / 279)
 * ObjectKeyCard: av[0] A (0 waiting, 3 flying to the HUD icon), av[1] / av[2] I / J (speed). 11802: a waiting card within 19 px (x) of PLAYER,
 * its y in [Y - 47, Y + 19]: sound 37, A 3, layer 6, I / J = 0.06 x the distance to the icon (|.| >= 1). 11803: A 3: x += I, y += J
 * toward the icon (clamped at it). 11804: on the icon: sound 285, GUIKeyCards + 1, the icon's anim 0 (gone).
 * GUIKeyCardIcon (16, 228) / GUIKeyCards (38, 231) are shown while K > 0 (11805 / 11806).
 * Terminal: av[0] A (0 locked, 1 opening, 2 open), av[1] B (its door index: 11893 numbers each class in selection order),
 * av[2] C (count). 11808: grounded, PLAYER within 39 px x and 0..59 px below the terminal's y, A 0, K > 0: A 1, anim 7,
 * K - 1, sound 286 (PLAYER x within 39, the terminal's y in (Y, Y + 59]). 11809: A 1: C + 1. 11810: A 1, C > 30: A 2, sound 279. foreach_door_44 (11807): every door part with the
 * terminal's B takes its A. 11811: seal A > 0: hidden (an effect). 11812: barrier A > 1: flag 0 (not solid). 11813 / 11814:
 * back / front A > 1: the opening animation (speed 25). The k-th terminal opens the k-th barrier / back / front / seal of
 * the act (all classes are placed in the same order: tb_inventory). */
static void keycards(player_t *p) {
    obj_t *o;
    EACH(OT_TBG_KEY, o) {
        if (o->av[0] == 0) {
            if (dead(p) || (o->fl & 1) || fabs(o->x - PX(p)) > 19 || o->y < PY(p) - 47 || o->y > PY(p) + 19) continue;   /* 11802: card x within 19 of PLAYER x, y in [Y - 47, Y + 19] */
            snd(37); o->av[0] = 3; o->layer = 6;
            o->av[3] = o->x - G.camx; o->av[4] = o->y - G.camy;                                   /* layer 6 is the HUD's: screen coordinates */
            double I = (16 - 12 - o->av[3]) * 0.06, J = (228 - 12 - o->av[4]) * 0.06;
            o->av[1] = fabs(I) < 1 ? (I < 0 ? -1 : 1) : I; o->av[2] = fabs(J) < 1 ? (J < 0 ? -1 : 1) : J;
        }
        if (o->av[0] == 3) {                                                    /* 11803: toward GUIKeyCardIcon at (16, 228), clamped at it */
            double tx = 16 - 12, ty = 228 - 12;
            o->av[3] = o->av[1] < 0 ? fmax(o->av[3] + o->av[1], tx) : fmin(o->av[3] + o->av[1], tx);
            o->av[4] = o->av[2] < 0 ? fmax(o->av[4] + o->av[2], ty) : fmin(o->av[4] + o->av[2], ty);
            o->x = G.camx + (float)o->av[3]; o->y = G.camy + (float)o->av[4] + hud_offset();
            if (o->av[3] == tx && o->av[4] == ty) { snd(285); S.keys++; o->alive = 0; }        /* 11804: on the icon */
        }
    }
}
static void doors(player_t *p) {
    obj_t *t, *d;
    EACH(OT_TBG_TERM, t) {
        if (t->av[0] == 0 && !dead(p) && p->ground && S.keys > 0 && fabs(t->x - PX(p)) <= 39 && t->y > PY(p) && t->y <= PY(p) + 59) {   /* 11808 */
            t->av[0] = 1; set_s(t, "tbg_term7"); S.keys--; snd(286);
        }
        if (t->av[0] == 1) t->av[2] += 1;                                       /* 11809 */
        if (t->av[0] == 1 && t->av[2] > 30) { t->av[0] = 2; snd(279); }         /* 11810 */
        EACH(OT_TBG_LDOOR, d) if (d->av[1] == t->av[1]) d->av[0] = t->av[0];    /* foreach_door_44 */
        EACH(OT_TBG_BARRIER, d) if (d->av[1] == t->av[1]) d->av[0] = t->av[0];
    }
    EACH(OT_TBG_LDOOR, d) {
        if (d->par == 2 && d->av[0] > 0 && !d->hidden) { d->hidden = 1; obj_explode(d->x, d->y); }   /* 11811 */
        if (d->par < 2 && d->an.b) {                                            /* 11813 / 11814: A > 1: opens at speed 25 (frames 0..9) */
            if (d->av[0] > 1) d->av[5] += 25;
            int c = d->an.b->anims[d->an.anim].count, f = (int)(d->av[5] / 100);
            d->an.frame = f >= c ? c - 1 : f; d->an.acc = 0;
        }
    }
    EACH(OT_TBG_BARRIER, d) if (d->av[0] > 1) d->fl |= 1;                       /* 11812: flag 0 (out of the solid group) */
}

/* ================================================================ 3. metal doors, wall lights (RE 11826-11838)
 * MetalDoorBack / Front: frame = A (0..10, 0.5 a frame): A + 0.5 (max 10) while the PLAYER is within 59 px (x) of it and
 * on screen (80), else A - 0.5 (min 0); sound 279 when it starts opening, 280 when it starts closing. Scenery (no q7).
 * TBWallLightAnim: frame (g3 / 4) mod 12 (11826). */
static void scenery(player_t *p) {
    obj_t *o;
    int fr = (G.frame >> 2) % 12;
    EACH(OT_TBG_WLIGHT, o) if (o->an.b) { o->an.frame = fr; }
    EACH(OT_TBG_MDOOR, o) {
        int nearp = !dead(p) && in_view(o, 80) && fabs(PX(p) - o->x) <= 59 && fabs(PY(p) - o->y) <= 59;   /* 11828 / 11829 */
        double A = o->av[0];
        if (nearp) { if (A == 0 && o->par == 1) snd(279); A = fmin(A + 0.5, 10); }   /* 11830 / 11835 / 11837 */
        else { if (A == 10 && o->par == 1) snd(280); A = fmax(A - 0.5, 0); }         /* 11827 / 11832 / 11838 */
        o->av[0] = A;
        if (o->an.b) { int c = o->an.b->anims[o->an.anim].count; int f = (int)A; o->an.frame = f >= c ? c - 1 : f; o->an.acc = 0; }
    }
    fr = (G.frame / 4) % 7;
    EACH(OT_TBG_WTLIGHT, o) if (o->an.b) { o->an.frame = fr; }   /* INFERRED: TBWaterLight cycles like the wall lights */
}

/* ================================================================ 4. fire platforms (RE 11720-11728, 11930-11938; ctor F 2, G 160
 * (2708) / 80 (2665), H 0; RV fire1 / init2)
 * The platform is an objects.c path platform (q12 dip + q13 carry): N = J + G cos E, E += 2. TB2 (on_frame_46_start): R = the
 * selection index mod 2, E = 189 R. Frame (E / 180 + 1) mod 2 (11720). Flames (Flame1 at x - 52, Flame2 at x + 52, RV):
 * A = the platform's E; rising (E < 180) they point down under the platform (y + 31, direction 16), sinking up over it (y - 1,
 * direction 0); the short flame (direction + 1) for 160 < A < 190 and A > 340 / A <= 10 (11724-11726). Sound 282 at E 12 and
 * 192 on screen (11722 / 11723). 11727 / 11728: a flame on screen (50) touching PLAYER burns (player_burn 5, not the fire
 * shield). */
static void fireplats(player_t *p) {
    obj_t *o, *f;
    EACH(OT_TBG_FPLAT, o) {
        double E = o->vx;
        if (o->an.b) { o->an.frame = ((int)(E / 180.) + 1) % 2; o->an.acc = 0; }             /* 11720 */
        if ((E == 12 || E == 192) && in_view(o, 60)) snd(282);                                   /* 11722 / 11723 */
    }
    EACH(OT_TBG_FFLAME, f) {
        o = NULL;
        if (f->tm >= 0 && f->tm < G.nobjs && G.o[f->tm].alive && G.o[f->tm].type == OT_TBG_FPLAT) o = &G.o[f->tm];
        if (!o) { f->hidden = 1; continue; }
        double A = o->vx; if (A < 0) A += 360;
        int down = A < 180, shortf = (A > 160 && A < 190) || A > 340 || A <= 10;
        f->x = o->x + (f->par ? 52 : -52); f->y = o->y + (down ? 31 : -1);
        set_s(f, down ? (shortf ? "tbg_ff17" : "tbg_ff16") : (shortf ? "tbg_ff1" : "tbg_ff0"));
        f->hidden = 0;
        if (!dead(p) && in_view(f, 50) && disc_hit(p, f)) player_burn(p, 5);                   /* 11727 / 11728 */
    }
}

/* ================================================================ 5. scrolling platforms (RE 11729-11752, 11890; RV init2)
 * LevelScrollingPlatform1 (80 x 128 pillars from the floor) / 2 (80 x 192 from the ceiling): q7 q13, invisible; the masks are
 * drawn at them. objects.c path rule with C = G = 0 (X = floor(I), Y = floor(J), 4061 carries O / P). av[0] R (0 moving left,
 * 1 sinking / rising away, 2 coming back), U / V = the act's LevelScrollingPlatformEnd / Start x, T = the starting J.
 * R 0: I - 3 (11730 / 11740); the S_RIGHTBIG sensor on it: MV.X - 3 (11731 / 11741). I <= U: R 1 (11733 / 11743). R 1: J + 4
 * (1) / J - 4 (2) (11734 / 11744); past T + 192 / T - 192: R 2, I = V (11735 / 11745), sound 283 on screen (120)
 * (11736 / 11746). R 2: J - 4 / J + 4 (11737 / 11747); back at T: R 0, J = T (11738 / 11748). Flames: A = R, drawn only at
 * R 0 (11749 / 11750) at the placement offset from their platform; burning like the fire platforms' (11751 / 11752).
 * PlayerPush (11739): PLAYER on it: MV.X + 8, MV.Y + 12. */
static void scrollers(player_t *p) {
    obj_t *o, *f;
    double U = 0, V = 0; int haveU = 0, haveV = 0;
    EACH(OT_TBG_SMARK, o) { if (o->par == 1) { U = o->x; haveU = 1; } else { V = o->x; haveV = 1; } }
    EACH(OT_TBG_SPLAT, o) {
        int up = o->par == 1;                                     /* 2: hangs from the ceiling (sinks upward) */
        double *Rr = &o->av[0], T = o->av[1];
        if (*Rr == 0) {
            o->ox -= 3;                                                                          /* 11730 */
            if (!dead(p) && sens_hit(SENS_RIGHTBIG, o)) { p->x -= 3; player_sync_sensors(p); }   /* 11731 */
            if (haveU && o->ox <= U) *Rr = 1;                                                    /* 11733 */
        } else if (*Rr == 1) {
            o->oy += up ? -4 : 4;                                                                /* 11734 */
            if (up ? o->oy <= T - 192 : o->oy >= T + 192) { *Rr = 2; if (haveV) o->ox = V; if (in_view(o, 120)) snd(283); }   /* 11735 / 11736 */
        } else {
            o->oy += up ? 4 : -4;                                                                /* 11737 */
            if (up ? o->oy >= T : o->oy < T) { *Rr = 0; o->oy = (float)T; }                      /* 11738 */
        }
    }
    EACH(OT_TBG_SMASK, f) if (f->tm >= 0 && f->tm < G.nobjs && G.o[f->tm].alive) { f->x = G.o[f->tm].x; f->y = G.o[f->tm].y; }   /* 11729 */
    EACH(OT_TBG_SFLAME, f) {
        if (f->tm < 0 || f->tm >= G.nobjs || !G.o[f->tm].alive) { f->hidden = 1; continue; }
        const obj_t *pl = &G.o[f->tm];
        f->x = pl->x + (float)f->av[2]; f->y = pl->y + (float)f->av[3]; f->av[0] = pl->av[0];
        f->hidden = f->av[0] != 0;                                                               /* 11749 / 11750 */
        if (!f->hidden && !dead(p) && in_view(f, 50) && disc_hit(p, f)) player_burn(p, 5);      /* 11751 / 11752 */
    }
    EACH(OT_TBG_PUSH, o) if (!dead(p) && fabs(PX(p) - o->x) < 16 + 13 && fabs(PY(p) - o->y) < 16 + 13) { p->x += 8; p->y += 12; player_sync_sensors(p); }   /* 11739 (64 x 32, hotspot 16, 16; the PLAYER disc) */
}

/* ================================================================ 6. pistons and spin platforms (RE 11940 / 11941 / 12068; ctors:
 * piston F 6 G 64, spin B -1 C 160 D 1 F -1 G 160; RV init2)
 * Both are invisible q8 q9 q13 path platforms (objects.c): piston N = J + 64 cos E (E + 6; the start E 180 / 0 alternate by
 * selection index, RV), spin M = I + 160 sin A, N = J + 160 cos E (A, E - 1; four per hub, the start A = E = 270 - 90 k, RV).
 * Masks are drawn at their platform (11940 / 12068); the spin supports at (x, y + 16), angle A + 90, x scale 0.25 C (11940).
 * 11941: CTRL.K 1 (jump pressed), MV.M 1, S_BB on a piston: G += min(0, -0.01 |F| G sin(E + 45)) (the piston's push). */
static void pistons(player_t *p) {
    obj_t *o, *m;
    EACH(OT_TBG_PMASK, m) if (m->tm >= 0 && m->tm < G.nobjs && G.o[m->tm].alive) { m->x = G.o[m->tm].x; m->y = G.o[m->tm].y; }
    EACH(OT_TBG_SPINMASK, m) if (m->tm >= 0 && m->tm < G.nobjs && G.o[m->tm].alive) { m->x = G.o[m->tm].x; m->y = G.o[m->tm].y; }
    EACH(OT_TBG_SPINSUP, m) if (m->tm >= 0 && m->tm < G.nobjs && G.o[m->tm].alive) {
        const obj_t *s = &G.o[m->tm]; m->x = s->x; m->y = s->y + 16; m->ang = (float)(90 + s->kx); m->hidden = 1;   /* drawn by tb_gim_draw (x scale) */
    }
    /* INFERRED trigger: the frame the player leaves a piston with a jump (the port has no CTRL.K here): MV.M 1, rising */
    int was = S.pist_gnd; S.pist_gnd = -1;
    if (dead(p)) return;
    EACH(OT_TBG_PISTON, o) if (p->ground && sens_hit(SENS_BOTTOMBIG, o)) S.pist_gnd = (int)(o - G.o);
    if (was >= 0 && S.pist_gnd < 0 && !p->ground && p->vy < 0 && player_orig_m(p) == 1 && G.o[was].alive) {
        const obj_t *pi = &G.o[was];
        double s = c_sin(45 + pi->vx), d = -0.01 * 6 * 64 * s;
        if (d < 0) p->vy += d;
    }
}

/* ================================================================ 7. lava (RE 11943-11956, foreach_lvf_45; RV lava2 / lava3)
 * HazardLavaPit: frame (g3 / 15) mod 4 (11943); no other event in TB2 / TB3 (no qualifier, RE): the lava under the walkways
 * is scenery (RV lava3: the floor under it holds the player). MagmaFlow: its own looping animation, no event (scenery).
 * Lava falls (g3 mod 240 = c): warnings frame 0 while c < 100 (11944), sound 302 on screen (100) for c in 101..149 (11945),
 * animated (g3 / 3) mod 4 for c > 100 (11946); at c 150 every lava fall starts (flag 0, D 0; 11947), falls D + 10 a frame
 * (11948) as a column under its start (foreach_lvf_45 stops it at the floor), sound 35 every 10 frames on screen (11950),
 * CAM.Z + 1 (max 3) (11951). A falling column touching PLAYER: R-hurt 3 (B + 3), OTHERS.R 30 (11953 burn), not with the
 * fire shield; then E 5 (11954-11955 hold the knock-back). HazardLavaFall3 (TB3's cascade) is its own looping animation
 * (12256-12279 belong to the TB3 finale: bosses). INFERRED: the column's length (the floor under its start, background only). */
static int lava_floor(obj_t *o) {                       /* the background under the fall's start (cached in av[4]) */
    if (o->av[4] > 0) return (int)o->av[4];
    int x = (int)o->x, y = (int)o->y + 8;
    while (y < G.lvl.h && !px_solid(&G.lvl, x, y)) y += 4;
    o->av[4] = y; return y;
}
static void lava(player_t *p) {
    obj_t *o;
    int c = G.frame % 240, fr = (G.frame / 15) % 4;
    EACH(OT_TBG_LAVA, o) if (o->an.b) { o->an.frame = fr; }                 /* 11943 */
    int warn_snd = 0;
    EACH(OT_TBG_LWARN, o) {
        if (c < 100) set_s(o, "tbg_lwarn0");                                                      /* 11944 */
        else { set_s(o, "tbg_lwarn1"); if (o->an.b) o->an.frame = (G.frame / 3) % 4; }   /* 11946 */
        if (c > 100 && c < 150 && !warn_snd && in_view(o, 100) && (c - 101) % 16 == 0) { snd(302); warn_snd = 1; }   /* 11945 */
    }
    int any_snd = 0;
    EACH(OT_TBG_LFALL, o) {
        if (o->par == 1) continue;                                                                /* LavaFall3: animated scenery */
        if (c == 150) { o->fl |= 1; o->av[3] = 0; }                                              /* 11947 */
        if (o->fl & 1) {
            o->av[3] += 10;                                                                       /* 11948: D + 10 */
            int top = (int)o->oy, len = (int)o->av[3], bot = lava_floor(o);
            if (top + len >= bot) { len = bot - top; if (o->av[3] > (bot - top) + 128) o->fl &= ~1; }   /* INFERRED: the column ends at the floor, then it is spent */
            o->y = (float)(top + len - 128); if (o->y < top) o->y = (float)top;
            o->hidden = 0;
            if (!any_snd && G.frame % 10 == 0 && in_view(o, 100)) { snd(35); any_snd = 1; }       /* 11950 */
            if (in_view(o, 100)) { if (G.cam.Z + 1 <= 3) G.cam.Z += 1; }                          /* 11951 */
            if (!dead(p) && p->shield != 2 && fabs(PX(p) - o->x) < 32 + 8 && PY(p) + 14 > top && PY(p) - 14 < top + len) player_burn(p, 3);   /* 11953 */
        } else { o->hidden = 1; o->y = o->oy; }
    }
}

/* ================================================================ 8. explosive blocks, switch buttons, the destroyer
 * (RE 11957-11965, foreach_ec_45; sounds 49 / 38)
 * SwitchButtonLeft / Right (16 x 32): pressed like a wall plate (switches_wall_push: S_LEFTBIG / S_RIGHTBIG with the push M 11
 * or Carol's cling M 47): hidden, sound 49, an ObjectExplosiveBlockDestroyer at the button, C = -1 (left button) / +1 (right): the walls of
 * blocks stand behind the buttons in every placement.
 * Destroyer (invisible 32 x 32): A 0: x + 12 C (11959); on an explosive block: A 1 (11960); A 1: x + 12 C, B + 1 (11961); the
 * blocks it overlaps are lit (11962: CAM.Z 8, sound 38); B > 30: gone (11963). A lit block shakes (A + 2, debris; foreach_ec_45)
 * and is destroyed at A > 60 (11965), with an explosion. INFERRED: the light spreads to the blocks the lit one touches (the
 * walls are stacks of 64 x 64 blocks) so a wall goes at once. */
static void buttons(player_t *p) {
    obj_t *b;
    EACH(OT_TBG_BUTTON, b) {
        if (b->hidden) continue;
        if (!switches_wall_push(p, b, b->par == 1 ? SENS_RIGHTBIG : SENS_LEFTBIG)) continue;   /* 11957: Right <- S_RIGHTBIG, 11958: Left <- S_LEFTBIG */
        b->hidden = 1; snd(49); S.presses++;
        obj_t *d = spawn(OT_TBG_DESTROYER, "tbg_destroyer", b->x + 8, b->y + 16, b->layer);
        if (d) { d->av[2] = b->par == 1 ? 1 : -1; d->hidden = 1; }             /* toward the blocks behind the button (TB2-TB4 placements) */
    }
}
static void light_block(obj_t *k) {
    if (k->fl & F1) return;
    k->fl |= F1;
    obj_t *o; EACH(OT_TBG_XBLOCK, o) if (!(o->fl & F1) && fabsf(o->x - k->x) <= 64 && fabsf(o->y - k->y) <= 64) light_block(o);   /* INFERRED spread */
}
static void xblocks(void) {
    obj_t *d, *o;
    EACH(OT_TBG_DESTROYER, d) {
        d->x += 12 * (float)d->av[2];                                                             /* 11959 / 11961 */
        int hit = 0;
        EACH(OT_TBG_XBLOCK, o) if (!(o->fl & F1) && box_ov(d, o)) { light_block(o); hit = 1; }    /* 11960 / 11962 */
        if (hit) { if (d->av[0] == 0) { shake(8); snd(38); } d->av[0] = 1; }
        if (d->av[0] == 1) d->av[1] += 1;
        if (d->av[1] > 30 || d->x < -64 || d->x > G.lvl.w + 64) d->alive = 0;                     /* 11963 */
    }
    EACH(OT_TBG_XBLOCK, o) if (o->fl & F1) {
        o->av[0] += 2;                                                                            /* foreach_ec_45 */
        if (((int)o->av[0] % 8) == 2) obj_explode(o->x + 8 + R(48), o->y + 8 + R(48));
        if (o->av[0] > 60) { obj_explode(o->x + 32, o->y + 32); o->alive = 0; }                  /* 11965 */
    }
}

/* ================================================================ 9. floor traps (RE 12070-12072, test_collisions_47; RV ftrap4)
 * HazardFloorTrap (64 x 8 plate, q7): stood on (S_BB, grounded): flag 1, animation 7 (sound 173 at its frame 1); flag 1: A + 1;
 * A > 35: it bursts (debris, explosion) and is gone; RV: the player on it is hurt (4 half-petals, the knock-back). */
static void floortraps(player_t *p) {
    obj_t *o;
    EACH(OT_TBG_FTRAP, o) {
        if (!(o->fl & F1) && !dead(p) && p->ground && sens_hit(SENS_BOTTOMBIG, o)) { o->fl |= F1; set_s(o, "tbg_ftrap7"); snd(173); }
        if (!(o->fl & F1)) continue;
        o->av[0] += 1;                                                                            /* 12071 */
        if (o->av[0] > 35) {                                                                      /* 12072 */
            obj_explode(o->x + 32, o->y);
            if (!dead(p) && fabs(PX(p) - (o->x + 32)) < 48 && fabs(PY(p) + 14 - o->y) < 40 && !player_invincible(p)) player_contact(p, 4);
            o->alive = 0;
        }
    }
}

/* ================================================================ 10. conveyors (RE 12388 / 12389; ctors A +2 right / -2 left)
 * Frame B = (B + 0.5 A) mod 8 (12388). 12389: grounded (L 1), CAM.Z 0 (no shake), S_BB on a q31 belt: MV.X + A of the belt. */
static void conveyors(player_t *p) {
    obj_t *o;
    S.conv_push = 0;
    EACH(OT_TBG_CONV, o) {
        double A = o->par < 3 ? 2 : -2;
        o->av[1] = fmod(o->av[1] + 0.5 * A, 8.); if (o->av[1] < 0) o->av[1] += 8;
        if (o->an.b) o->an.frame = (int)o->av[1];
    }
    if (dead(p) || !p->ground || G.cam.Z != 0) return;
    EACH(OT_TBG_CONV, o) if (sens_hit(SENS_BOTTOMBIG, o)) { int A = o->par < 3 ? 2 : -2; p->x += A; S.conv_push = A; player_sync_sensors(p); break; }
}

/* ================================================================ 11. crank platforms (RE 12383-12387)
 * The crank turns with g3: angle = (2 g3) mod 360; the arms and mids follow the base (12384); each top platform's M / N is the
 * crank pin (INFERRED geometry: the base's action end, radius 64), K / L the last ones (12383), O / P the steps (12385).
 * 12386 / 12387: G >= 0, S_BB on a top: MV.X + O, MV.Y + P, sensors re-placed, L 1. */
static void cranks_pre(player_t *p) {
    obj_t *o;
    double a = fmod(2.0 * G.frame, 360.);
    EACH(OT_TBG_CTOP, o) {
        double M = o->ox + 64 * c_cos(a + (o->par ? 180 : 0)), Nn = o->oy + 64 * c_sin(a + (o->par ? 180 : 0)) * 0.5;
        o->av[2] = floor(M) - floor(o->x); o->av[3] = floor(Nn) - floor(o->y);
        o->x = (float)floor(M); o->y = (float)floor(Nn);
    }
    if (dead(p) || p->vy < 0) return;
    EACH(OT_TBG_CTOP, o) if (mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, OT_TBG_CTOP) == (int)(o - G.o)) {
        p->x += o->av[2]; p->y += o->av[3];
        if (!p->ground) { p->gsp = p->vx; p->ground = 1; }
        p->vy = 0; player_sync_sensors(p); break;
    }
}

/* ================================================================ 12. giant pressers (RE 12504-12510; ctor C 640)
 * Presser / tube: A = the start y, B the phase: y = A + max(0, C sin B) (the tube 0.25 C); B + 1.5 a frame, + 1.5 more out of
 * (0, 180) (12505 / 12507); B 3 on screen (200): sound 321, CAM.Z 7 (12508). The hitbox follows the presser (12505); touching
 * PLAYER while the presser comes down (B < 90) crushes: R-hurt 3 (INFERRED: the original's crush rule 12509 / 12510 was not
 * traced). */
static void pressers(player_t *p) {
    obj_t *o, *h;
    EACH(OT_TBG_PRESS, o) {
        o->av[1] = fmod(o->av[1] + 1.5, 360.);
        if (!(o->av[1] > 0 && o->av[1] < 180)) o->av[1] = fmod(o->av[1] + 1.5, 360.);
        double s = c_sin(o->av[1]), C = o->par == 1 ? 640 : 160;
        o->y = (float)floor(o->av[0] + (s > 0 ? C * s : 0));
        if (o->par == 1 && o->av[1] == 3 && in_view(o, 200)) { snd(321); shake(7); }
    }
    EACH(OT_TBG_PHIT, h) {
        if (h->tm < 0 || h->tm >= G.nobjs || !G.o[h->tm].alive) continue;
        const obj_t *pr = &G.o[h->tm];
        h->x = pr->x; h->y = pr->y; h->av[0] = pr->av[1];
        if (!dead(p) && h->av[0] > 0 && h->av[0] < 90 && disc_hit(p, h) && !player_invincible(p)) player_contact(p, 3);
    }
}

/* ================================================================ 13. bubbles, turbines, levers (RE 12391-12393, 12486-12501,
 * 12511-12519) — scenery plus the TB5 water levers.
 * TubeBubbleSpawner: every 8th frame a TubeBubble at x - 40 + R(81), rising 3 + R(50) / 10 a frame for up to 192 px (12391-12393).
 * Water levers: hanging on a GimmickWaterLevelMask (q23) pulls it 8 px a frame down to 8 under its start, MV.Y + 2 (12487); at
 * the bottom the lever is pulled (frame 1, sound 110) and MasterLevel B runs + 1 a frame to 3240 (lever 1, 12491 / 12492) or
 * - 1 to 2920 (lever 2, 12499 / 12500) with CAM.Z 2; released, the mask climbs back (12486). */
static void bubbles(void) {
    obj_t *o;
    EACH(OT_TBG_TBUBSP, o) {
        if (!in_view(o, 64) || G.frame % 8) continue;
        obj_t *b = spawn(OT_TBG_TBUB, "tbg_tbub", o->x - 40 + R(81), o->y, o->layer);
        if (b) { b->av[0] = -(R(50) * 0.1 + 3); b->av[1] = o->y; b->av[2] = R(4); }
    }
    EACH(OT_TBG_TBUB, o) { o->y += (float)o->av[0]; if (o->an.b) o->an.frame = (int)o->av[2]; if (o->y < o->av[1] - 192) o->alive = 0; }
}
static void levers(player_t *p) {
    obj_t *m, *l;
    EACH(OT_TBG_WMASK, m) {
        int k = m->par & 1;
        int held = p->state == ST_HANG && !dead(p) && fabs(PX(p) - m->x) < 32 && fabs(PY(p) - 13 - m->y) < 24;
        if (held && m->y < m->oy + 8) { m->y += 8; if (m->y > m->oy + 8) m->y = m->oy + 8; p->y += 2; }   /* 12487 */
        else if (!held && m->y > m->oy) m->y -= 1;                                                       /* 12486 */
        if (held && m->y >= m->oy + 8 && !S.lever_on[k]) {
            S.lever_on[k] = 1; snd(110);
        }
    }
    EACH(OT_TBG_LEVER, l) if (l->an.b) l->an.frame = S.lever_on[l->par & 1] ? 1 : 0;
    if (!water_on()) return;
    double B = water_line_get();
    if (S.lever_on[0] && B < 3240) { water_line_set(B + 1); shake(2); }                           /* 12492 */
    else if (S.lever_on[1] && !S.lever_on[0] && B > 2920) { water_line_set(B - 1); shake(2); }    /* 12500 */
}

/* ================================================================ passes */
void tb_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS);
    /* objects.c path platforms (3283-3289; RE ctors) */
    objects_plat_path(OT_TBG_FPLAT, 0, 0, 1, 2, 160, 0, 1);                                      /* LevelFirePlatform1_2708: F 2 G 160 H 0, q12 */
    objects_plat_path(OT_TBG_FPLAT80, 0, 0, 1, 2, 80, 0, 1);                                     /* LevelFirePlatform1_2665 (TB1): G 80 */
    objects_plat_path(OT_TBG_PISTON, 0, 0, 1, 6, 64, 0, 0);                                      /* F 6 G 64 H 0 */
    objects_plat_path(OT_TBG_SPIN, -1, 160, 1, -1, 160, 0, 0);                                   /* B -1 C 160 D 1 F -1 G 160 H 0 */
    objects_plat_path(OT_TBG_SPLAT, 0, 0, 1, 0, 0, 1, 0);                                        /* C = G = 0: X = floor(I), Y = floor(J) */
    if (!objects_have_from(RT0)) return;
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < RT0 || o->type > OT_TBG_END) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0x7f;
        float E = o->vx;                                                                         /* the path platforms keep their registry init */
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = -1; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->vx = E;
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x > G.lvl.w || o->y > G.lvl.h) { o->alive = 0; continue; }   /* off-map templates (TB1's start lift sits on the bottom edge: 432, 2400) */
        lst_add(o); S.on = 1;
    }
    if (!S.on) return;
    obj_t *o, *q;
    EACH(OT_TBG_LIFT, o) { set_s(o, "tbg_lift"); o->av[0] = o->y; }
    EACH(OT_TBG_LIFT, o) { obj_t *e = lift_end(o, 0); if (e && fabsf(e->y - o->y) < 16) o->av[1] = 2; }   /* start events: on its top: Q 2 */
    S.elev_trig = -1;
    EACH(OT_TBG_LIFT, o) if (!lift_end(o, 0) && !lift_end(o, 1)) {                                /* no shaft ends: TB5's elevator */
        o->av[1] = -1;
        for (int i = 0; i < G.nobjs && S.elev_trig < 0; i++) if (G.o[i].alive && G.o[i].type == OT_CAMTRIGGER && G.o[i].x >= 0 && G.o[i].y >= 0 && G.o[i].y < o->y) S.elev_trig = i;
    }
    EACH(OT_TBG_KEY, o) set_s(o, "tbg_key");
    { static const int DT[4] = { OT_TBG_TERM, OT_TBG_LDOOR, OT_TBG_BARRIER, 0 };   /* 11893: B = the selection index per class */
      for (int k = 0; DT[k]; k++) { int idx[3] = { 0, 0, 0 }; EACH(DT[k], o) { int m = DT[k] == OT_TBG_LDOOR ? (o->par > 2 ? 0 : o->par) : 0; o->av[1] = idx[m]++; } } }
    EACH(OT_TBG_TERM, o) set_s(o, "tbg_term");
    EACH(OT_TBG_LDOOR, o) { static const char *const DN[3] = { "tbg_ldback", "tbg_ldfront", "tbg_seal" }; set_s(o, DN[o->par > 2 ? 0 : o->par]); }
    EACH(OT_TBG_BARRIER, o) set_s(o, "tbg_barrier");
    EACH(OT_TBG_MDOOR, o) set_s(o, o->par ? "tbg_mdfront" : "tbg_mdback");
    EACH(OT_TBG_WLIGHT, o) set_s(o, "tbg_wlight");
    EACH(OT_TBG_WTLIGHT, o) set_s(o, "tbg_wtlight");
    EACH(OT_TBG_BGTURB, o) set_s(o, "tbg_bgturb");
    /* fire platforms: TB2 start (on_frame_46_start): E = 189 (index mod 2); TB1 (RV fire1: the six alternate 71 / 242) INFERRED 0 / 171 */
    { int k = 0; EACH(OT_TBG_FPLAT, o) { set_s(o, "tbg_fplat"); o->vx = (float)((k % 2) * 189); o->ky = o->ox; k++; } }
    { int k = 0; EACH(OT_TBG_FPLAT80, o) { set_s(o, "tbg_fplat"); o->vx = (float)((k % 2) * 171); o->ky = o->ox; k++; } }
    EACH(OT_TBG_FFLAME, o) {                                                                     /* each flame on its nearest platform */
        float best = 1e9f; int bi = -1;
        for (int T = OT_TBG_FPLAT; T <= OT_TBG_FPLAT80; T += OT_TBG_FPLAT80 - OT_TBG_FPLAT)
            EACH(T, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; bi = (int)(q - G.o); } }
        o->tm = bi; set_s(o, "tbg_ff16");
    }
    EACH(OT_TBG_SPLAT, o) { set_s(o, o->par ? "tbg_splat1" : "tbg_splat0"); o->av[1] = o->oy; o->ky = o->ox; o->fx = o->oy; }   /* 11890: T = J */
    EACH(OT_TBG_SMASK, o) {
        set_s(o, o->par ? "tbg_smask1" : "tbg_smask0");
        float best = 1e9f; EACH(OT_TBG_SPLAT, q) if (q->par == o->par) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } }
    }
    EACH(OT_TBG_SFLAME, o) {
        set_s(o, "tbg_sf0");
        float best = 1e9f; EACH(OT_TBG_SPLAT, q) if (q->par == o->par) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } }
        if (o->tm >= 0) { o->av[2] = o->x - G.o[o->tm].x; o->av[3] = o->y - G.o[o->tm].y; }   /* the placement offset (RV: (+41, +43) / (+41, -44)) */
    }
    { int k = 0; EACH(OT_TBG_PISTON, o) { set_s(o, "tbg_piston"); o->vx = (float)(k % 2 ? 0 : 180); o->ky = o->ox; k++; } }   /* RV init2: E 192 / 12 at its frame 1 */
    EACH(OT_TBG_PMASK, o) { set_s(o, "tbg_pmask"); float best = 1e9f;
        EACH(OT_TBG_PISTON, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } } }
    {   /* spin: the four of a hub (same placement) A = E = 270 - 90 k (RV init2) */
        EACH(OT_TBG_SPIN, o) {
            set_s(o, "tbg_spin"); int k = 0;
            EACH(OT_TBG_SPIN, q) { if (q == o) break; if (q->ox == o->ox && q->oy == o->oy) k++; }
            o->kx = 270 - 90 * k; o->vx = (float)o->kx; o->ky = o->ox + 160 * c_sin(o->kx);
        }
        int si = 0;
        EACH(OT_TBG_SPINMASK, o) { set_s(o, "tbg_spinmask"); obj_t *s = nth(OT_TBG_SPIN, si++); o->tm = s ? (int)(s - G.o) : -1; }
        si = 0;
        EACH(OT_TBG_SPINSUP, o) { set_s(o, "tbg_spinsup"); obj_t *s = nth(OT_TBG_SPIN, si++); o->tm = s ? (int)(s - G.o) : -1; }
    }
    EACH(OT_TBG_VENT, o) set_s(o, "tbg_vent");
    EACH(OT_TBG_LAVA, o) set_s(o, "tbg_lava");
    EACH(OT_TBG_LFALL, o) { set_s(o, o->par ? "tbg_lfall3" : "tbg_lfall"); if (!o->par) o->hidden = 1; }
    EACH(OT_TBG_LWARN, o) set_s(o, "tbg_lwarn0");
    EACH(OT_TBG_MAGMA, o) set_s(o, "tbg_magma");
    EACH(OT_TBG_XBLOCK, o) set_s(o, "tbg_xblock");
    EACH(OT_TBG_BUTTON, o) set_s(o, o->par ? "tbg_btn1" : "tbg_btn0");
    EACH(OT_TBG_FTRAP, o) set_s(o, "tbg_ftrap");
    EACH(OT_TBG_BLOCKER, o) set_s(o, "tbg_blocker");
    EACH(OT_TBG_CONV, o) { static const char *const CN[3] = { "tbg_conv0", "tbg_conv1", "tbg_conv2" }; set_s(o, CN[o->par % 3]); }
    EACH(OT_TBG_LOOP0, o) { static const char *const L0[4] = { "tbl_0_0", "tbl_0_1", "tbl_0_2", "tbl_0_3" }; set_s(o, L0[o->par & 3]); }
    EACH(OT_TBG_LOOP0B, o) { static const char *const L0B[3] = { "tbl_0b_0", "tbl_0b_1", "tbl_0b_2" }; set_s(o, L0B[o->par % 3]); }
    EACH(OT_TBG_LOOP1, o) { static const char *const L1[3] = { "tbl_1_0", "tbl_1_1", "tbl_1_2" }; set_s(o, L1[o->par % 3]); }
    EACH(OT_TBG_LOOP1B, o) set_s(o, "tbl_1b_0");
    EACH(OT_TBG_CTOP, o) set_s(o, "tbg_ctop");
    EACH(OT_TBG_CPART, o) set_s(o, o->par < 2 ? "tbg_cmid" : "tbg_cbase");
    EACH(OT_TBG_PRESS, o) { set_s(o, o->par ? "tbg_press" : "tbg_ptube"); o->av[0] = o->y; o->av[1] = 180; }
    EACH(OT_TBG_PHIT, o) { set_s(o, "tbg_phit"); float best = 1e9f;
        EACH(OT_TBG_PRESS, q) if (q->par == 1) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } } }
    EACH(OT_TBG_TURB, o) set_s(o, "tbg_turb");
    EACH(OT_TBG_LEVER, o) set_s(o, "tbg_lever");
    EACH(OT_TBG_WMASK, o) set_s(o, "tbg_wmask");
    EACH(OT_TBG_BWAVE, o) set_s(o, o->par ? "tbg_bwave1" : "tbg_bwave0");
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBG")) fprintf(stderr, "tbg: lifts %d keys %d terms %d doors %d barriers %d fplat %d/%d splat %d piston %d spin %d xblock %d conv %d\n",
        N(OT_TBG_LIFT), N(OT_TBG_KEY), N(OT_TBG_TERM), N(OT_TBG_LDOOR), N(OT_TBG_BARRIER), N(OT_TBG_FPLAT), N(OT_TBG_FPLAT80), N(OT_TBG_SPLAT),
        N(OT_TBG_PISTON), N(OT_TBG_SPIN), N(OT_TBG_XBLOCK), N(OT_TBG_CONV));
#endif
}

void tb_gim_pre(player_t *p) {
    if (!S.on) return;
    if (N(OT_TBG_LIFT)) lifts_pre();
    if (N(OT_TBG_CTOP)) cranks_pre(p);
}
int tb_gim_obj(obj_t *o, player_t *p) {
    if (o->type < RT0 || o->type > OT_TBG_END) return 0;
    if (o->type == OT_TBG_WMASK) obj_hang_try(o, p);                                             /* q23 hang handle (4064-4066) */
    if (o->type == OT_TBG_FX && --o->tm <= 0) o->alive = 0;
    return 1;
}
void tb_gim_frame(player_t *p) {
    if (!S.on) return;
    if (N(OT_TBG_LIFT)) lifts(p);
    if (N(OT_TBG_FPLAT) || N(OT_TBG_FPLAT80)) fireplats(p);
    if (N(OT_TBG_SPLAT)) scrollers(p);
    if (N(OT_TBG_PISTON) || N(OT_TBG_SPIN)) pistons(p);
    if (N(OT_TBG_KEY)) keycards(p);
    if (N(OT_TBG_TERM)) doors(p);
    scenery(p);
    if (N(OT_TBG_LAVA) || N(OT_TBG_LFALL) || N(OT_TBG_LWARN)) lava(p);
    if (N(OT_TBG_BUTTON)) buttons(p);
    if (N(OT_TBG_DESTROYER) || N(OT_TBG_XBLOCK)) xblocks();
    if (N(OT_TBG_FTRAP)) floortraps(p);
    if (N(OT_TBG_CONV)) conveyors(p);
    if (N(OT_TBG_PRESS)) pressers(p);
    if (N(OT_TBG_TBUBSP) || N(OT_TBG_TBUB)) bubbles();
    if (N(OT_TBG_WMASK)) levers(p);
}
void tb_gim_draw(int pass) {
    if (!S.on) return;
    if (pass == 1) {                                                                             /* spin supports: x-scaled, rotated (11940) */
        obj_t *o;
        EACH(OT_TBG_SPINSUP, o) {
            if (!o->an.b || o->tm < 0) continue;
            float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
            if (sx < -200 || sx > SCREEN_W + 200 || sy < -200 || sy > SCREEN_H + 200) continue;
            r_set_xscale(0.25f * 160); frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, 0, 0xffffffff); r_set_xscale(1.f);
        }
    }
    if (pass == 3 && S.keys > 0) {                                                               /* 11805 / 11806: the key card HUD */
        int a = ens_anim("tbg_kicon");
        float dy = hud_offset();
        if (a >= 0) { const anim_t *an = &G.ens.anims[a]; const frame_t *f = &G.ens.frames[G.ens.seq[an->first]]; frame_draw(&G.ens, f, 16, 228 + dy, 0, 0xffffffff); }
        char b[8]; snprintf(b, sizeof b, "x%d", S.keys); text_draw(30, 224 + dy, b, 0xffffffff, 1);
    }
}
int tb_gim_q20(int type) { (void)type; return 0; }
