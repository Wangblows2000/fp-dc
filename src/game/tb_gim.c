/* Thermal Base gimmicks (session 21c): types 1888-1967 (tb.h OT_TBG_*). Spec: build/re/spec_tb_gimmicks.md.
 * Labels: RE (decompiled events / constructors of frames 44-49 = handle_frame_45..50), RV (original traces
 * build/orig_traces/tb_gim), INFERRED (marked).
 * Families (event numbers in the spec): key cards + terminals +
 * locked doors (11802-11814, foreach_door_44, 11893; TB4 12448-12459), metal doors (11826-11838), fire platforms
 * + flames (11720-11728, 11930-11938), scrolling platforms + masks + flames + push (11729-11752, 11890), pistons (11940 /
 * 11941 / 12068), spin platforms (11940), air vents (no event), lava pits / falls / warnings / magma (11943-11956,
 * foreach_lvf_45, 12249-12279), explosive blocks + switch buttons + destroyer (11957-11965, foreach_ec_45), floor traps
 * (12070-12072, test_collisions_47), conveyors (12388 / 12389), pipe loops (Q10 / Q11 pieces, 12337-12340), crank platforms
 * (12383-12387), giant pressers (12504-12510), turbines / bubbles / water lights (12391-12393, 12511-12519), water levers
 * (12486-12501).
 * Reused systems: objects.c path-platform registry (objects_plat_path: fire / piston / spin / scrolling platforms), the q7 /
 * q8 / q10 / q11 / q23 collision groups (objdefs.c flags), the wall-plate press rule (switches_wall_push), obj_hang_try (water
 * level masks), player_burn (the fire hurt shared with sb_gim.c), obj_explode.
 * (LevelLift* / TBWallLightAnim are tb_flow.c's.) Passes: tb_gim_pre (before the movement: the crank tops), tb_gim_obj (per object: hang handles; the
 * families run in tb_gim_frame), tb_gim_frame (after the player block, in event order), tb_gim_draw (pass 0-2: the x-scaled
 * spin arms; pass 3: the key card HUD).
 * Object fields: par = family member (importer TBG_SUB), layer = instance layer; av[] = class alterables named per family. */
#include "tb.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdlib.h>
#endif

#define RAD 57.29577951308232
#define RT0 OT_TBG_FIRST_
#define NRT (OT_TBG_END - RT0 + 1)
/* per-type instance lists, sized at the act start from the act's own placements (TB5 places 67 conveyors: the old fixed 64 left
 * three without sprite and push): each type gets max(LMIN, placed) slots of one pool (runtime types keep LMIN) */
#define LMIN 64
#define LPOOL (NRT * LMIN + 128)
static short LST[LPOOL], LOFF[NRT], LCAP[NRT]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[LOFF[(T) - RT0] + _k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]
#define F1 0x1000                                  /* the object's flag 1 */
#define F2 0x2000                                  /* flag 2 */
#define F3 0x4000                                  /* flag 3 */
#define F6 0x8000                                  /* flag 6 */
static struct {
    int on, keys, presses, pist_gnd, lever_t;
    int conv_push;                                 /* this frame's conveyor push (tests) */
    int lever_on[2], lever_last;                   /* the water levers held down (12489 / 12497); the last one pulled (1 / 2) */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / RAD); }                  /* Chowdren sin: 0 at exactly 180 */
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int R(int n) { return cross_rand(n); }
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
        int c = cnt[t] > LMIN ? cnt[t] : LMIN;
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
/* PlayerBlocker_2780 (TB3, q7 invisible boxes over the lava-cascade drop): solid from the start; the Syntax spider chase turns
 * them off (12260: flag 0 every frame while the spider's K is 1) and back on once the player is below y 4288 (12262: K + 1).
 * The chase is the bosses module's: it calls this. */
void tb_gim_blockers(int solid) { obj_t *o; EACH(OT_TBG_BLOCKER, o) { if (solid) o->fl &= ~1; else o->fl |= 1; } }
int tb_gim_button_presses(void) { return S.presses; }

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
/* TB3 (RE 12118-12120): a card with A -1 (a dropped card: tb_gim_key_drop) on screen (40) falls, B + 0.4 a frame, y + B; on the
 * background: A 0 and it is lifted out of it (the original's set_y loop). */
static int key_on_bg(const obj_t *o) { return px_solid(&G.lvl, (int)o->x + 12, (int)o->y + 23) || px_solid(&G.lvl, (int)o->x + 4, (int)o->y + 23) || px_solid(&G.lvl, (int)o->x + 20, (int)o->y + 23); }
obj_t *tb_gim_key_drop(float x, float y) {
    obj_t *k = spawn(OT_TBG_KEY, "tbg_key", x, y, 2);
    if (k) { k->av[0] = -1; S.on = 1; }
    return k;
}
static void keycards(player_t *p) {
    obj_t *o;
    EACH(OT_TBG_KEY, o) if (o->av[0] == -1 && in_view(o, 40)) {
        o->av[1] += 0.4; o->y += (float)o->av[1];                                                 /* 12118 */
        if (key_on_bg(o)) { o->av[0] = 0; o->av[1] = 0; for (int k = 0; k < 64 && key_on_bg(o); k++) o->y -= 1; }   /* 12119 / 12120 */
    }
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

/* ================================================================ 3. metal doors (RE 11826-11838)
 * MetalDoorBack / Front: frame = A (0..10, 0.5 a frame): A + 0.5 (max 10) while the PLAYER is within 59 px (x) of it and
 * on screen (80), else A - 0.5 (min 0); sound 279 when an opening front reaches A 1, 280 when a closing one reaches A 2. Scenery (no q7).
 * (TBWallLightAnim, 11826, is tb_flow.c's.) */
static int mdoor_ywin = 59;
void tb_gim_mdoor_ywin(int y) { mdoor_ywin = y; }   /* Final Dreadnought's lab doors (fd_gim.c): 79 */
static void scenery(player_t *p) {
    obj_t *o;
    int fr;
    EACH(OT_TBG_MDOOR, o) {
        int nearp = !dead(p) && in_view(o, 80) && fabs(PX(p) - o->x) <= 59 && fabs(PY(p) - o->y) <= mdoor_ywin;   /* 11828 / 11829 (FD's lab doors 15641 / 15646: 79) */
        double A = o->av[0];
        if (nearp) { A = fmin(A + 0.5, 10); if (A == 1 && o->par == 1) snd(279); }   /* 11830 / 11835; 11837: the front opening (flag 1) at A 1 */
        else { A = fmax(A - 0.5, 0); if (A == 2 && o->par == 1) snd(280); }          /* 11827 / 11832; 11838: closing (flag 1 off) at A 2 (RV small fixes
                                                                                      * tb4_door: the front at x 24 closing from 10 at 61, 280 at 75; FD's
                                                                                      * lab doors 15650 / 15651 the same) */
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
        if (f->tm >= 0 && f->tm < G.nobjs && G.o[f->tm].alive && (G.o[f->tm].type == OT_TBG_FPLAT || G.o[f->tm].type == OT_TBG_FPLAT80)) o = &G.o[f->tm];
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
 * selection index, RV), spin M = I + 160 sin A, N = J + 160 cos E (A, E - 1; four per hub, the start A = E = 150 - 90 k, RV).
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
 * animated (g3 / 3) mod 4 for c > 100 (11946); at c 150 every lava fall starts (flag 0, D 0; 11947) and slides D + 10 a frame
 * (11948); sound 35 every 10 frames on screen (11950), CAM.Z + 1 (max 3) (11951). A falling segment touching PLAYER: R-hurt 3
 * (B + 3), OTHERS.R 30 (11953 burn), not with the fire shield (player_burn); E 5 holds the knock-back (11954 / 11955).
 * The falls are placed four per spot (64 x 128 images): the k-th of a spot is drawn 128 k lower, so a column 512 px long slides
 * down (RV lava2: Carol on TB2's walkway at 4900, 3314 is hit 22 frames after c 150 by the column poured at 4896, 2624).
 * INFERRED: foreach_lvf_45's stop (D past the instance's A) is taken as D 512 (the column has passed), then it hides.
 * HazardLavaFall3 (TB3's cascade) is its own looping animation (12256-12279 belong to the TB3 finale: bosses). */
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
    int any_snd = 0, hurt = 0;
    EACH(OT_TBG_LFALL, o) {
        if (o->par == 1) continue;                                                                /* LavaFall3: animated scenery */
        if (c == 150) { o->fl |= 1; o->av[3] = 0; }                                              /* 11947 */
        if (!(o->fl & 1)) { o->hidden = 1; o->y = o->oy; continue; }
        o->av[3] += 10;                                                                           /* 11948: D + 10 */
        if (o->av[3] > 512) { o->fl &= ~1; o->hidden = 1; o->y = o->oy; continue; }               /* INFERRED end */
        o->y = (float)(o->oy + 128 * o->av[5] + o->av[3]); o->hidden = 0;
        if (!any_snd && G.frame % 10 == 0 && in_view(o, 100)) { snd(35); any_snd = 1; }           /* 11950 */
        if (in_view(o, 100) && G.cam.Z + 1 <= 3) G.cam.Z += 1;                                    /* 11951 */
        if (!hurt && !dead(p) && fabs(PX(p) - o->x) < 32 + 7 && PY(p) + 14 > o->y && PY(p) - 14 < o->y + 128) hurt = player_burn(p, 3);   /* 11953 */
    }
}

/* ================================================================ 8. explosive blocks, switch buttons, the destroyer
 * (RE 11957-11965, foreach_ec_45; sounds 49 / 38)
 * SwitchButtonLeft / Right (16 x 32, hotspot 0,0): pressed like a wall plate (switches_wall_push: S_LEFTBIG / S_RIGHTBIG with
 * the push M 11 or Carol's cling M 47): hidden, flag 1, sound 49 and THREE ObjectExplosiveBlockDestroyers (RE 11957 right button:
 * at (x, y), (x - 64, y - 64), (x - 128, y - 128), the ctor's C 1; 11958 left button: (x, y), (x + 64, y - 64), (x + 128, y - 128),
 * C -1), one per block row: TB4's left switch (2528,1416) sits a row below its 3 x 2 wall (rows 1256 / 1320), so only the
 * two upper destroyers reach it (the single destroyer at the button the port used to place missed the wall: TB4 bug).
 * Destroyer (invisible 32 x 32, hotspot 16,16): A 0: x + 12 C (11959); on an explosive block: A 1 (11960); A 1: x + 2 C, B + 1
 * (11961: x + C + C); the unlit blocks it overlaps are lit: flag 1, hidden, an explosion at their centre, CAM.Z 8, a sound,
 * the destroyer's B = 0 (11962); B > 30: gone (11963). So it creeps through a wall 2 px a frame, lighting one column of
 * blocks after the other. A lit block (still solid while it burns: hidden objects keep their q7 collision) grows a spiral
 * of explosions, A + 2 a frame (foreach_ec_45: explosion06 at the centre + A (cos, sin)(41 A)) and is destroyed at A > 60
 * (11965). INFERRED (DC cost): one spiral explosion every 4th frame instead of every frame. */
static void buttons(player_t *p) {
    obj_t *b;
    EACH(OT_TBG_BUTTON, b) {
        if (b->hidden) continue;
        if (!switches_wall_push(p, b, b->par == 1 ? SENS_RIGHTBIG : SENS_LEFTBIG)) continue;   /* 11957: Right <- S_RIGHTBIG, 11958: Left <- S_LEFTBIG */
        b->hidden = 1; snd(49); S.presses++;
        int c = b->par == 1 ? 1 : -1;                                                             /* 11957 ctor C 1 / 11958 C -1 */
        for (int k = 0; k < 3; k++) {
            obj_t *d = spawn(OT_TBG_DESTROYER, "tbg_destroyer", floorf(b->x) - 64 * k * c, floorf(b->y) - 64 * k, b->layer);
            if (d) { d->av[2] = c; d->hidden = 1; }
        }
    }
}
static void xblocks(void) {
    obj_t *d, *o;
    EACH(OT_TBG_DESTROYER, d) {
        if (d->av[0] == 0) {
            d->x += 12 * (float)d->av[2];                                                         /* 11959 */
            EACH(OT_TBG_XBLOCK, o) if (box_ov(d, o)) { d->av[0] = 1; break; }                     /* 11960 (lit or not) */
        }
        if (d->av[0] == 1) { d->x += 2 * (float)d->av[2]; d->av[1] += 1; }                         /* 11961 */
    }
    /* 11962 with Chowdren's selection: check_overlap(destroyers A 1, blocks) keeps every A 1 destroyer touching ANY block (lit
     * ones too: hidden objects keep their collision) and the blocks they touch; then the lit blocks are dropped. If an unlit one
     * is left, it is lit and EVERY selected destroyer's B goes back to 0, so one destroyer's hit keeps the other ones alive (RV
     * btn4l: TB4's third column at 2304 is only reached because the row-1256 destroyer's hit at 129 resets the row-1320 one). */
    int lit = 0;
    EACH(OT_TBG_DESTROYER, d) if (d->av[0] == 1) {
        d->fx = 0;
        EACH(OT_TBG_XBLOCK, o) if (box_ov(d, o)) { d->fx = 1; if (!(o->fl & F1)) { o->fl |= F6; lit = 1; } }
    }
    if (lit) {
        EACH(OT_TBG_XBLOCK, o) if (o->fl & F6) { o->fl = (o->fl & ~F6) | F1; o->hidden = 1; obj_effect(o->x + 32, o->y + 32, "explosion01"); }
        EACH(OT_TBG_DESTROYER, d) if (d->av[0] == 1 && d->fx) d->av[1] = 0;
        shake(8); snd(38);
    }
    EACH(OT_TBG_DESTROYER, d) if (d->av[1] > 30 || d->x < -64 || d->x > G.lvl.w + 64) d->alive = 0;   /* 11963 */
    EACH(OT_TBG_XBLOCK, o) if (o->fl & F1) {
        o->av[0] += 2;                                                                            /* 11964 foreach_ec_45 */
        if (((int)o->av[0] % 8) == 2) {
            double a = o->av[0], t = 41 * a / RAD;
            obj_effect(o->x + 32 + (float)(cos(t) * a), o->y + 32 + (float)(sin(t) * a), "explosion06");
        }
        if (o->av[0] > 60) o->alive = 0;                                                          /* 11965 */
    }
}

/* another module's object setting off the explosive blocks its box [x0, x1) x [y0, y1) touches (Battle Glacier 14306: a drill sat;
 * RE: flag 0 set, hidden, explosion01 at the block's centre; the caller makes the shake and the sound). The block's own explosion
 * (11964 / 11965) runs above. Returns the blocks set off. */
int tb_gim_xblock_ignite(float x0, float y0, float x1, float y1) {
    obj_t *o; int n = 0;
    if (!S.on) return 0;
    EACH(OT_TBG_XBLOCK, o) {
        float c0, d0, c1, d1; img_box(o, &c0, &d0, &c1, &d1);
        if (!(o->fl & F1) && x0 < c1 && c0 < x1 && y0 < d1 && d0 < y1) { o->fl |= F1; o->hidden = 1; obj_effect(o->x + 32, o->y + 32, "explosion01"); n++; }
    }
    return n;
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
            if (!dead(p) && fabs(PX(p) - (o->x + 32)) < 48 && fabs(PY(p) + 14 - o->y) < 40 && !player_invincible(p)) player_contact(p, 20);   /* 4 half-petals (5 each) */
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

/* ================================================================ 11. crank platforms (RE 12383-12387; RV crank4)
 * The crank turns with g3 (12384: angle (2 g3) mod 360, the bases / arms / mids follow through their rotated action points; the
 * tops ride the arms' ends). The linkage is reproduced from RV crank4 (the whole 180-frame cycle, g3 1789-1969): per phase
 * k = g3 mod 180, CRANK_TAB[k] = top1 / top2 offsets from their placement, arm1 / arm2 offsets from their base, and the
 * angles (alterable A) of base1 / base2 / arm1 / arm2 and the mids' length A (x scale 0.1 A). The port's g3 is its stage frame
 * (INFERRED phase origin, as TH). 12383 / 12385: K, L = M, N; O / P = the steps. 12386 / 12387: G >= 0, S_BB on a top: MV.X + O,
 * MV.Y + P, the sensors re-placed, L 1 (the tops are q8 q9, not q13: their own carry). */
static const short CRANK_TAB[180][14] = {
  {-152,10,155,-39,3,-128,91,-90,88,45,225,-2,96,236}, {-155,10,155,-42,0,-128,91,-90,90,45,225,0,99,237}, {-158,10,155,-45,-3,-128,91,-90,92,45,225,2,101,238},
  {-162,10,154,-49,-7,-128,90,-90,93,45,225,3,105,239}, {-165,11,153,-52,-10,-127,90,-90,95,45,225,5,108,239}, {-169,10,153,-57,-14,-127,90,-91,96,45,225,6,112,241},
  {-172,11,152,-59,-16,-126,90,-91,98,46,224,8,114,241}, {-176,10,151,-64,-20,-126,89,-92,99,46,224,9,118,242}, {-180,9,150,-68,-23,-126,88,-93,101,46,224,11,122,243},
  {-184,10,149,-72,-27,-125,88,-93,102,47,223,12,126,244}, {-188,10,147,-76,-30,-124,87,-94,104,47,223,14,130,245}, {-192,10,146,-79,-33,-124,86,-94,105,48,222,15,134,245},
  {-196,9,144,-83,-36,-123,85,-95,107,48,222,17,138,246}, {-201,10,141,-88,-40,-121,84,-96,108,49,221,18,142,246}, {-203,10,139,-92,-42,-120,83,-98,110,50,220,20,144,247},
  {-207,9,138,-95,-45,-120,82,-98,111,50,220,21,148,247}, {-212,9,136,-99,-48,-119,81,-99,112,51,219,22,153,248}, {-216,9,132,-104,-51,-117,79,-101,114,52,218,24,157,248},
  {-219,9,130,-106,-53,-116,78,-101,115,53,217,25,160,248}, {-223,8,127,-111,-56,-115,76,-103,116,54,216,26,164,249}, {-226,8,123,-115,-58,-113,74,-105,118,55,215,28,167,248},
  {-230,8,120,-118,-61,-112,72,-105,119,56,214,29,171,248}, {-233,7,117,-122,-63,-111,70,-107,120,57,213,30,174,248}, {-238,5,114,-127,-66,-110,68,-109,121,58,212,31,179,250},
  {-241,6,111,-128,-67,-108,67,-109,122,59,211,32,182,248}, {-245,5,107,-133,-70,-106,65,-111,123,60,210,33,186,249}, {-248,4,103,-136,-72,-105,62,-112,124,61,209,34,189,248},
  {-250,2,100,-139,-73,-104,60,-114,125,62,208,35,192,249}, {-254,1,96,-141,-75,-103,57,-114,126,64,206,36,196,248}, {-257,-1,92,-145,-77,-102,54,-116,127,65,205,37,199,248},
  {-259,-2,88,-147,-78,-101,51,-117,128,66,204,38,201,247}, {-263,-4,85,-150,-80,-100,49,-118,129,68,202,39,206,248}, {-265,-5,80,-152,-81,-98,46,-119,130,69,201,40,208,246},
  {-266,-7,76,-155,-82,-97,43,-121,130,70,200,40,209,247}, {-268,-9,73,-156,-83,-97,40,-121,131,72,198,41,212,246}, {-271,-12,70,-159,-84,-96,38,-123,132,73,197,42,216,247},
  {-274,-14,65,-161,-86,-95,34,-123,132,75,195,42,219,246}, {-274,-16,61,-162,-86,-94,31,-124,133,76,194,43,220,244}, {-276,-18,56,-164,-87,-93,27,-125,133,78,192,43,222,244},
  {-277,-22,53,-165,-87,-93,24,-126,134,79,191,44,224,243}, {-278,-24,49,-166,-88,-92,21,-126,134,81,189,44,226,242}, {-279,-28,45,-168,-89,-92,17,-127,134,82,188,44,228,242},
  {-280,-29,41,-168,-89,-91,14,-127,135,84,186,45,229,240}, {-281,-33,37,-170,-90,-91,10,-128,135,85,185,45,232,240}, {-282,-36,34,-169,-90,-91,7,-127,135,87,183,45,234,238},
  {-282,-40,30,-170,-90,-91,3,-128,135,88,182,45,235,237}, {-282,-43,27,-170,-90,-91,0,-128,135,90,180,45,236,236}, {-282,-46,24,-170,-90,-91,-3,-128,135,92,178,45,237,235},
  {-281,-50,20,-170,-90,-91,-7,-128,135,93,177,45,238,234}, {-282,-53,17,-169,-90,-91,-10,-127,135,95,175,45,240,232}, {-280,-57,13,-168,-89,-91,-14,-127,135,96,174,45,240,229},
  {-280,-61,12,-167,-89,-92,-16,-126,134,98,172,44,242,228}, {-278,-65,8,-166,-88,-92,-20,-126,134,99,171,44,242,226}, {-277,-69,6,-165,-87,-93,-23,-126,134,101,169,44,243,224},
  {-276,-72,2,-164,-87,-93,-27,-125,133,102,168,43,244,222}, {-274,-77,0,-162,-86,-94,-30,-124,133,104,166,43,244,220}, {-273,-81,-2,-162,-86,-95,-33,-124,132,105,165,42,246,219},
  {-271,-86,-4,-159,-84,-96,-36,-123,132,107,163,42,247,216}, {-268,-89,-7,-156,-83,-97,-40,-121,131,108,162,41,246,212}, {-267,-92,-9,-154,-82,-97,-42,-120,130,110,160,40,247,209},
  {-264,-96,-11,-153,-81,-98,-45,-120,130,111,159,40,246,208}, {-262,-101,-12,-151,-80,-100,-48,-119,129,112,158,39,248,206}, {-259,-104,-14,-147,-78,-101,-51,-117,128,114,156,38,247,201},
  {-257,-108,-15,-145,-77,-102,-53,-116,127,115,155,37,248,199}, {-253,-112,-17,-142,-75,-103,-56,-115,126,116,154,36,248,196}, {-251,-116,-18,-138,-73,-104,-58,-113,125,118,152,35,249,192},
  {-248,-119,-20,-136,-72,-105,-61,-112,124,119,151,34,248,189}, {-245,-123,-21,-133,-70,-106,-63,-111,123,120,150,33,249,186}, {-240,-127,-22,-129,-67,-108,-66,-110,122,121,149,32,248,182},
  {-239,-130,-21,-126,-66,-110,-67,-108,121,122,148,31,250,179}, {-234,-133,-23,-121,-63,-111,-70,-106,120,123,147,30,248,174}, {-230,-136,-24,-118,-61,-112,-72,-105,119,124,146,29,248,171},
  {-227,-139,-24,-114,-58,-113,-73,-104,118,125,145,28,248,167}, {-223,-143,-24,-111,-56,-115,-75,-103,116,126,144,26,249,164}, {-218,-146,-25,-107,-53,-116,-77,-102,115,127,143,25,248,160},
  {-216,-148,-25,-104,-51,-117,-78,-101,114,128,142,24,248,157}, {-211,-152,-25,-100,-48,-119,-80,-100,112,129,141,22,248,153}, {-207,-154,-25,-95,-45,-120,-81,-98,111,130,140,21,247,148},
  {-204,-155,-26,-91,-42,-120,-82,-97,110,130,140,20,247,144}, {-200,-157,-26,-89,-40,-121,-83,-97,108,131,139,18,246,142}, {-195,-160,-25,-84,-36,-123,-84,-96,107,132,138,17,246,138},
  {-191,-162,-26,-80,-33,-124,-86,-95,105,132,138,15,245,134}, {-188,-163,-26,-76,-30,-124,-86,-94,104,133,137,14,245,130}, {-184,-165,-26,-72,-27,-125,-87,-93,102,133,137,12,244,126},
  {-180,-166,-25,-68,-23,-126,-87,-93,101,134,136,11,243,122}, {-176,-167,-26,-64,-20,-126,-88,-92,99,134,136,9,242,118}, {-171,-168,-27,-60,-16,-126,-89,-92,98,134,136,8,241,114},
  {-169,-169,-26,-57,-14,-127,-89,-91,96,135,135,6,241,112}, {-164,-169,-27,-53,-10,-127,-90,-91,95,135,135,5,239,108}, {-161,-170,-26,-50,-7,-128,-90,-91,93,135,135,3,239,105},
  {-157,-171,-26,-46,-3,-128,-90,-91,92,135,135,2,238,101}, {-154,-171,-26,-43,0,-128,-90,-91,90,135,135,0,237,99}, {-151,-171,-26,-40,3,-128,-90,-91,88,135,135,-2,236,96},
  {-147,-169,-27,-36,7,-127,-90,-91,87,135,135,-3,232,92}, {-144,-170,-26,-33,10,-128,-90,-91,85,135,135,-5,232,89}, {-141,-169,-26,-29,14,-127,-89,-91,84,135,135,-6,230,86},
  {-138,-169,-26,-27,17,-127,-89,-92,82,134,136,-8,229,84}, {-135,-167,-26,-23,21,-126,-88,-92,81,134,136,-9,226,81}, {-133,-166,-25,-21,24,-126,-87,-93,79,134,136,-11,225,79},
  {-130,-165,-26,-18,27,-125,-87,-93,78,133,137,-12,223,76}, {-127,-163,-26,-15,31,-124,-86,-94,76,133,137,-14,220,74}, {-124,-161,-27,-13,34,-123,-86,-95,75,132,138,-15,217,71},
  {-121,-160,-25,-10,38,-123,-84,-96,73,132,138,-17,216,70}, {-120,-157,-26,-9,40,-121,-83,-97,72,131,139,-18,213,69}, {-119,-156,-25,-6,43,-121,-82,-97,70,130,140,-20,211,67},
  {-116,-153,-26,-4,46,-119,-81,-98,69,130,140,-21,208,64}, {-114,-151,-26,-3,49,-118,-80,-100,68,129,141,-22,205,64}, {-114,-148,-25,-2,51,-117,-78,-101,66,128,142,-24,202,63},
  {-111,-146,-25,0,54,-116,-77,-102,65,127,143,-25,200,62}, {-110,-142,-25,2,57,-114,-75,-103,64,126,144,-26,195,60}, {-109,-140,-23,4,60,-114,-73,-104,62,125,145,-28,193,60},
  {-107,-136,-24,5,62,-112,-72,-105,61,124,146,-29,189,59}, {-106,-133,-23,7,65,-111,-70,-106,60,123,147,-30,186,58}, {-106,-129,-22,7,67,-109,-67,-108,59,122,148,-31,182,59},
  {-105,-128,-21,6,68,-109,-66,-110,58,121,149,-32,181,60}, {-105,-124,-20,7,70,-107,-63,-111,57,120,150,-33,177,60}, {-104,-119,-20,8,72,-105,-61,-112,56,119,151,-34,172,59},
  {-104,-117,-17,9,74,-105,-58,-113,55,118,152,-35,170,61}, {-102,-112,-17,9,76,-103,-56,-115,54,116,154,-36,164,61}, {-102,-107,-16,10,78,-101,-53,-116,53,115,155,-37,160,61},
  {-102,-104,-14,10,79,-101,-51,-117,52,114,156,-38,157,63}, {-101,-100,-13,10,81,-99,-48,-119,51,112,158,-39,153,64}, {-101,-96,-11,10,82,-98,-45,-120,50,111,159,-40,149,65},
  {-102,-93,-8,11,83,-98,-42,-120,50,110,160,-40,146,67}, {-101,-88,-8,11,84,-96,-40,-121,49,108,162,-41,141,67}, {-102,-85,-5,10,85,-95,-36,-123,48,107,163,-42,138,70},
  {-101,-80,-3,10,86,-94,-33,-124,48,105,165,-42,133,72}, {-101,-77,0,11,87,-94,-30,-124,47,104,166,-43,130,74}, {-101,-72,2,11,88,-93,-27,-125,47,102,168,-43,126,76},
  {-102,-69,6,10,88,-93,-23,-126,46,101,169,-44,123,80}, {-101,-65,8,11,89,-92,-20,-126,46,99,171,-44,119,81}, {-101,-60,11,12,90,-91,-16,-126,46,98,172,-44,114,83},
  {-101,-57,13,11,90,-91,-14,-127,45,96,174,-45,111,85}, {-102,-52,16,11,90,-90,-10,-127,45,95,175,-45,107,88}, {-101,-49,19,10,90,-90,-7,-128,45,93,177,-45,104,91},
  {-101,-45,23,11,91,-90,-3,-128,45,92,178,-45,100,95}, {-101,-42,26,11,91,-90,0,-128,45,90,180,-45,97,97}, {-101,-39,29,11,91,-90,3,-128,45,88,182,-45,95,100},
  {-102,-35,33,11,90,-90,7,-127,45,87,183,-45,91,104}, {-101,-32,36,10,90,-90,10,-128,45,85,185,-45,88,107}, {-101,-29,41,11,90,-91,14,-127,45,84,186,-45,85,111},
  {-100,-27,44,11,90,-91,17,-127,46,82,188,-44,83,114}, {-101,-24,49,11,89,-92,21,-126,46,81,189,-44,81,119}, {-102,-22,53,10,88,-93,24,-126,46,79,191,-44,80,123},
  {-101,-18,56,11,88,-93,27,-125,47,78,192,-43,76,126}, {-101,-16,61,11,87,-94,31,-124,47,76,194,-43,74,130}, {-102,-13,64,11,86,-94,34,-123,48,75,195,-42,72,133},
  {-102,-11,69,10,85,-95,38,-123,48,73,197,-42,70,138}, {-101,-8,72,11,84,-96,40,-121,49,72,198,-41,67,141}, {-101,-8,77,10,83,-98,43,-121,50,70,200,-40,67,146},
  {-102,-5,80,11,82,-98,46,-119,50,69,201,-40,65,149}, {-102,-3,84,11,81,-99,49,-118,51,68,202,-39,64,153}, {-102,-2,88,10,79,-101,51,-117,52,66,204,-38,63,157},
  {-102,0,91,10,78,-101,54,-116,53,65,205,-37,61,160}, {-103,1,96,10,76,-103,57,-114,54,64,206,-36,61,164}, {-103,1,101,8,74,-105,60,-114,55,62,208,-35,61,170},
  {-104,4,103,8,72,-105,62,-112,56,61,209,-34,59,172}, {-105,4,108,7,70,-107,65,-111,57,60,210,-33,60,177}, {-106,5,112,7,68,-109,67,-109,58,59,211,-32,60,181},
  {-105,6,113,6,67,-109,68,-109,59,58,212,-31,59,182}, {-105,7,117,6,65,-111,70,-107,60,57,213,-30,58,186}, {-107,8,120,5,62,-112,72,-105,61,56,214,-29,59,189},
  {-108,7,124,3,60,-114,74,-105,62,55,215,-28,60,193}, {-110,9,126,2,57,-114,76,-103,64,54,216,-26,60,195}, {-112,9,130,1,54,-116,78,-101,65,53,217,-25,62,200},
  {-114,9,132,-2,51,-117,79,-101,66,52,218,-24,63,202}, {-115,10,135,-2,49,-118,81,-99,68,51,219,-22,64,205}, {-116,10,137,-4,46,-119,82,-98,69,50,220,-21,64,208},
  {-118,9,140,-7,43,-121,83,-98,70,50,220,-20,67,211}, {-121,10,141,-8,40,-121,84,-96,72,49,221,-18,69,213}, {-122,9,144,-9,38,-123,85,-95,73,48,222,-17,70,216},
  {-125,11,145,-12,34,-123,86,-94,75,48,222,-15,71,217}, {-127,10,147,-15,31,-124,87,-94,76,47,223,-14,74,220}, {-130,10,149,-18,27,-125,88,-93,78,47,223,-12,76,223},
  {-133,9,150,-21,24,-126,88,-93,79,46,224,-11,79,225}, {-135,10,151,-23,21,-126,89,-92,81,46,224,-9,81,226}, {-139,10,153,-26,17,-127,90,-91,82,46,224,-8,84,229},
  {-141,10,153,-29,14,-127,90,-91,84,45,225,-6,86,230}, {-145,10,154,-32,10,-128,90,-90,85,45,225,-5,89,232}, {-148,11,153,-35,7,-127,90,-90,87,45,225,-3,92,232},
};

static void cranks_pre(player_t *p) {
    obj_t *o, *b;
    const short *t = CRANK_TAB[G.frame % 180];
    EACH(OT_TBG_CTOP, o) {
        int k = o->par & 1;
        float nx = o->ox + t[2 * k], ny = o->oy + t[2 * k + 1];
        o->av[2] = nx - o->x; o->av[3] = ny - o->y; o->x = nx; o->y = ny;
    }
    EACH(OT_TBG_CPART, o) {
        int g = o->par & 1, kind = o->par >> 1;                     /* 0 mid, 1 base, 2 arm */
        b = o->tm >= 0 ? &G.o[o->tm] : o;
        if (kind == 1) { o->ang = t[8 + g]; continue; }
        if (kind == 2) { o->x = b->ox + t[4 + 2 * g]; o->y = b->oy + t[5 + 2 * g]; o->ang = t[10 + g]; continue; }
        o->x = b->ox; o->y = b->oy; o->av[0] = t[12 + g];           /* mid: from the base pivot toward the arm, x scale 0.1 A */
        o->ang = (float)(atan2(-(double)t[5 + 2 * g], (double)t[4 + 2 * g]) * RAD); o->hidden = 1;
    }
    if (dead(p) || p->vy < 0) return;
    EACH(OT_TBG_CTOP, o) if (mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, OT_TBG_CTOP) == (int)(o - G.o)) {   /* 12386 / 12387 */
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
 * the bottom the lever is pulled (frame 1) and MasterLevel B runs + 1 a frame to 3240 (lever 1, 12491 / 12492) or - 1 to
 * 2920 (lever 2, 12499 / 12500) with CAM.Z 2 and sound 110 every 0.24 s (level_water_set, levels.c); released, the mask climbs
 * back (12486). */
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
        int down = held && m->y >= m->oy + 8;
        if (down && !S.lever_on[k]) S.lever_last = k + 1;                                          /* INFERRED: the last pulled wins */
        S.lever_on[k] = down;
        if (0) {
        }
    }
    EACH(OT_TBG_LEVER, l) if (l->an.b) l->an.frame = S.lever_on[l->par & 1] ? 1 : 0;
    int b = level_water();                                                                       /* ML.B (levels.c; 0 = dry) */
    if (!b) return;
    int mv = 0;
    if (S.lever_last == 1 && b < 3240) { level_water_set(b + 1); mv = 1; }                       /* 12491 / 12492 */
    else if (S.lever_last == 2 && b > 2920) { level_water_set(b - 1); mv = 1; }                  /* 12499 / 12500 */
    if (mv) { shake(2); if (S.lever_t++ % 14 == 0) snd(110); }                                  /* CAM.Z >= 2; sound 110 every 0.24 s */
}

/* ================================================================ passes */
void tb_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); snap_register(LOFF, sizeof LOFF); snap_register(LCAP, sizeof LCAP); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS); lst_layout();
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
    EACH(OT_TBG_KEY, o) set_s(o, "tbg_key");
    { static const int DT[4] = { OT_TBG_TERM, OT_TBG_LDOOR, OT_TBG_BARRIER, 0 };   /* 11893: B = the selection index per class */
      for (int k = 0; DT[k]; k++) { int idx[3] = { 0, 0, 0 }; EACH(DT[k], o) { int m = DT[k] == OT_TBG_LDOOR ? (o->par > 2 ? 0 : o->par) : 0; o->av[1] = idx[m]++; } } }
    EACH(OT_TBG_TERM, o) set_s(o, "tbg_term");
    EACH(OT_TBG_LDOOR, o) { static const char *const DN[3] = { "tbg_ldback", "tbg_ldfront", "tbg_seal" }; set_s(o, DN[o->par > 2 ? 0 : o->par]); }
    EACH(OT_TBG_BARRIER, o) set_s(o, "tbg_barrier");
    EACH(OT_TBG_MDOOR, o) set_s(o, o->par ? "tbg_mdfront" : "tbg_mdback");
    EACH(OT_TBG_WTLIGHT, o) set_s(o, "tbg_wtlight");
    EACH(OT_TBG_BGTURB, o) set_s(o, "tbg_bgturb");
    /* fire platforms: the start phase. RE on_frame_46_start: E = 189 (selection index mod 2); RV init2 (TB2, trace frame 1 = port
     * frame 1 for the pistons / spins) E 244 at frame 1: E0 240 (+ 189 for odd indexes, INFERRED); RV fire1 (TB1 frame 0): 71 / 242
     * alternate: E0 69 / 240 (INFERRED: one frame before trace frame 0) */
    { int k = 0; EACH(OT_TBG_FPLAT, o) { set_s(o, "tbg_fplat"); o->vx = (float)fmod(240 + (k % 2) * 189, 360.); o->ky = o->ox; k++; } }
    { int k = 0; EACH(OT_TBG_FPLAT80, o) { set_s(o, "tbg_fplat"); o->vx = (float)(k % 2 ? 240 : 69); o->ky = o->ox; k++; } }
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
    {   /* spin: the four of a hub (same placement) A = E = 150 - 90 k (RV init2: 148 / 58 / -32 / 238 or -122 at frame 1) */
        EACH(OT_TBG_SPIN, o) {
            set_s(o, "tbg_spin"); int k = 0;
            EACH(OT_TBG_SPIN, q) { if (q == o) break; if (q->ox == o->ox && q->oy == o->oy) k++; }
            o->kx = 150 - 90 * k; o->vx = (float)o->kx; o->ky = o->ox + 160 * c_sin(o->kx);
        }
        int si = 0;
        EACH(OT_TBG_SPINMASK, o) { set_s(o, "tbg_spinmask"); obj_t *s = nth(OT_TBG_SPIN, si++); o->tm = s ? (int)(s - G.o) : -1; }
        si = 0;
        EACH(OT_TBG_SPINSUP, o) { set_s(o, "tbg_spinsup"); obj_t *s = nth(OT_TBG_SPIN, si++); o->tm = s ? (int)(s - G.o) : -1; }
    }
    EACH(OT_TBG_VENT, o) set_s(o, "tbg_vent");
    EACH(OT_TBG_LAVA, o) set_s(o, "tbg_lava");
    EACH(OT_TBG_LFALL, o) {                                                                      /* av[5]: the segment index of its spot */
        set_s(o, o->par ? "tbg_lfall3" : "tbg_lfall"); if (!o->par) o->hidden = 1;
        int k = 0; EACH(OT_TBG_LFALL, q) { if (q == o) break; if (q->ox == o->ox && q->oy == o->oy && q->par == o->par) k++; }
        o->av[5] = k;
    }
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
    EACH(OT_TBG_CPART, o) {                                                                      /* par: mid1 / mid2, base1 / base2, arm1 / arm2 */
        set_s(o, o->par < 2 ? "tbg_cmid" : "tbg_cbase");
        float best = 1e9f; o->tm = -1;
        EACH(OT_TBG_CPART, q) if ((q->par >> 1) == 1 && (q->par & 1) == (o->par & 1)) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } }
    }
    EACH(OT_TBG_PRESS, o) { set_s(o, o->par ? "tbg_press" : "tbg_ptube"); o->av[0] = o->y; o->av[1] = 180; }
    EACH(OT_TBG_PHIT, o) { set_s(o, "tbg_phit"); float best = 1e9f;
        EACH(OT_TBG_PRESS, q) if (q->par == 1) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->tm = (int)(q - G.o); } } }
    EACH(OT_TBG_TURB, o) set_s(o, "tbg_turb");
    EACH(OT_TBG_LEVER, o) set_s(o, "tbg_lever");
    EACH(OT_TBG_WMASK, o) set_s(o, "tbg_wmask");
    EACH(OT_TBG_BWAVE, o) set_s(o, o->par ? "tbg_bwave1" : "tbg_bwave0");
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBG")) fprintf(stderr, "tbg: keys %d terms %d doors %d barriers %d fplat %d/%d splat %d piston %d spin %d xblock %d conv %d\n",
        N(OT_TBG_KEY), N(OT_TBG_TERM), N(OT_TBG_LDOOR), N(OT_TBG_BARRIER), N(OT_TBG_FPLAT), N(OT_TBG_FPLAT80), N(OT_TBG_SPLAT),
        N(OT_TBG_PISTON), N(OT_TBG_SPIN), N(OT_TBG_XBLOCK), N(OT_TBG_CONV));
#endif
}

void tb_gim_pre(player_t *p) {
    if (!S.on) return;
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
#ifdef FP_HOST_ENV
    { static int dbg = -1; if (dbg < 0) dbg = getenv("FP_DBG_TBG") != NULL;
      if (dbg) printf("tbg %d keys %d water %d lever %d %d conv %d\n", G.frame, S.keys, level_water(), S.lever_on[0], S.lever_on[1], S.conv_push); }
#endif
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
    if (pass == 1) {                                                                             /* crank mids: x scale 0.1 A (12384) */
        obj_t *o;
        EACH(OT_TBG_CPART, o) {
            if ((o->par >> 1) != 0 || !o->an.b) continue;
            float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
            if (sx < -300 || sx > SCREEN_W + 300 || sy < -300 || sy > SCREEN_H + 300) continue;
            r_set_xscale((float)o->av[0] * 0.1f); frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, 0, 0xffffffff); r_set_xscale(1.f);
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

/* ---- for tb_boss_pusher.c (TB_BOSS block; RE 12007): the wall pusher's barrier, not the player, presses TB2's switch. A button
 * overlapping the barrier (its 16 x 48 marker box: no image) is pressed (hidden, counted; the boss plays sound 49), and the boss
 * places three ObjectExplosiveBlockDestroyers at the switch, - 64 / - 64 and - 128 / - 128 (ctor C 1: they run right, 11959). */
int tb_gim_button_at(const obj_t *q) {
    obj_t *b; float x0 = floorf(q->x), y0 = floorf(q->y), x1 = x0 + 16, y1 = y0 + 48;
    if (q->an.b) img_box(q, &x0, &y0, &x1, &y1);
    EACH(OT_TBG_BUTTON, b) {
        float c0, d0, c1, d1; img_box(b, &c0, &d0, &c1, &d1);
        if (!b->hidden && x0 < c1 && c0 < x1 && y0 < d1 && d0 < y1) { b->hidden = 1; S.presses++; return 1; }
    }
    return 0;
}
void tb_gim_blast(float x, float y) { obj_t *d = spawn(OT_TBG_DESTROYER, "tbg_destroyer", x, y, 3); if (d) { d->av[2] = 1; d->hidden = 1; S.on = 1; } }   /* ctor C 1: x + 12 a frame */
