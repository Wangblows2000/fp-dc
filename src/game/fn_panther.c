/* Fortune Night's final boss, the Panther (session 17): the act-5 intro and chase (frame index 33, group 0x2278c, timeline
 * Ship2.B, events 8370-8419) and the act-6 fight (frame index 34, fight group 0x227ba, events 8507-8785): the walking rig, the
 * front cannon (plasma volleys, the laser), the rider's mortar, the back-cannon missiles, the mode-2 wheel stance with its foot
 * missiles, the hitbox HP model (pools 60 / 60 / 70 / 80 / 90 with jaw exposure), the treadmill floor for Lilac / Carol and
 * the plane for Milla, the speed effects, the partner assist (reduced), the defeat, the post-boss scene and the results.
 * Sources: build/re/spec_fn_bosses.md parts F, G, X (with its section U settled here: U-G1 the rig chain, U-G3 the assist,
 * see the spec's "Session 17 additions"), spec_fn_flow.md 5.3 / 5.4, and the original traces T-G1 / T-G2 / T-G3 / T-F1
 * (tools/orig, recorded in session 17: RV). Labels: RE read from code, RV measured in the original, INFERRED reasoned.
 *
 * Everything is attached to the engine types 860-899 (fp.h, tools/import_frame.py FN_PANTHER); the stage flow (level rows,
 * exits, the fixed FN6 camera box, FN6 deaths -> FN5 with g4 2, the results voice / wipe) is level data and fn_flow.c's.
 * Part pointers are cached at spawn: the rig is computed once per frame from the controller's alterables (no per-part
 * searches); one pass per frame moves the runtime projectiles. The hitbox is the only qualifier-20 member (combat.c shared
 * hits); the flash of the other parts follows the hitbox (8636 / 8641). */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEFEATED(o) ((o)->fl & 0x40)
static int R(int n) { return cross_rand(n); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / 57.29577951308232); }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / 57.29577951308232); }
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double sq(double v) { return v * v; }
extern int fe_music_volume;

/* ---- sprites (tools/import_frame.py FNP_ENS) */
enum { S_HEAD, S_JAW, S_JAW7, S_FCAN, S_BCAN, S_BCAN1, S_RIDER, S_RIDER1, S_RIDER2, S_RIDER12, S_RIDER13, S_SHF, S_SHF2, S_HIP, S_KNEE,
       S_LEGFL, S_LEGFR, S_FOOTFL, S_FOOTFR, S_FOOTFR7, S_FOOTFR8, S_RIB, S_TAIL, S_WHEEL, S_HIT, S_LASER, S_TARGET, S_GLOW, S_SBACK,
       S_SLINE, S_PLASMA, S_MISSILE, S_MISSILE_L, S_MBLAST, S_RBUL, S_RBUL_D, S_SHIELD, S_WARN, S_SPARK, S_FLOOR, S_PLANE, S_ASSIST,
       S_ASSIST1, S_AOFF0, S_AOFF1, S_AOFF7, S_AOFF7B, S_ADEF, S_SHIP1, S_SHIP2, S_CABLE, S_CABLE2, S_N };
static const char *const SN[S_N] = { "fp_head", "fp_jaw", "fp_jaw7", "fp_fcan", "fp_bcan", "fp_bcan1", "fp_rider", "fp_rider1", "fp_rider2",
    "fp_rider12", "fp_rider13", "fp_shf", "fp_shf2", "fp_hip", "fp_knee", "fp_legfl", "fp_legfr", "fp_footfl", "fp_footfr", "fp_footfr7",
    "fp_footfr8", "fp_rib", "fp_tail", "fp_wheel", "fp_hit", "fp_laser", "fp_target", "fp_glow", "fp_sback", "fp_sline", "fp_plasma",
    "fp_missile", "fp_missile_l", "fp_mblast", "fp_rbul", "fp_rbul_d", "fp_shield", "fp_warn", "fp_spark", "fp_floor", "fp_plane",
    "fp_assist", "fp_assist1", "fp_aoff0", "fp_aoff1", "fp_aoff7", "fp_aoff7b", "fp_adef", "fp_ship1", "fp_ship2", "fp_cable", "fp_cable2" };
static int SA[S_N];
static void setan(obj_t *o, int k) { int a = SA[k]; if (o && a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void restartan(obj_t *o, int k) { int a = SA[k]; if (o && a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }

/* parts in the json instance order of the rig (8556 / 8559 chain) */
enum { P_HEAD, P_JAW, P_FCAN, P_BCAN, P_RIDER, P_SHF, P_SHF2, P_HIP, P_KNEE, P_LEGFL, P_LEGFR, P_FOOTFL, P_FOOTFR, P_N };
static const int PART_TYPE[P_N] = { OT_FNP_HEAD, OT_FNP_JAW, OT_FNP_FCAN, OT_FNP_BCAN, OT_FNP_RIDER, OT_FNP_SHF, OT_FNP_SHF2, OT_FNP_HIP,
                                    OT_FNP_KNEE, OT_FNP_LEGFL, OT_FNP_LEGFR, OT_FNP_FOOTFL, OT_FNP_FOOTFR };
static const int PART_SPR[P_N] = { S_HEAD, S_JAW, S_FCAN, S_BCAN, S_RIDER, S_SHF, S_SHF2, S_HIP, S_KNEE, S_LEGFL, S_LEGFR, S_FOOTFL, S_FOOTFR };

typedef struct { double a[26]; unsigned fl; } alt_t;   /* an object's alterables A..Z and flags */
#define AV(s, c) ((s).a[(c) - 'A'])
#define V(c) AV(FP.v, c)
#define FC(c) AV(FP.fc, c)
#define BC(c) AV(FP.bc, c)
#define RD(c) AV(FP.rd, c)
#define JW(c) AV(FP.jw, c)
#define FF(c) AV(FP.ff, c)
#define HD(c) AV(FP.hd, c)
#define S2(c) AV(FP.s2, c)
enum { NRIB = 7, NTAIL = 6, NFLOOR = 12 };
static struct {
    int on, act;                       /* act 5 (intro / chase) or 6 (fight) */
    obj_t *val, *p[P_N], *rib[NRIB], *tail[NTAIL], *hit, *laser, *target, *glow, *wheel, *sback, *plane, *assist, *aoff, *adef, *aproj;
    obj_t *shield, *ship2;
    int nrib, ntail, floor_i[NFLOOR], nfloor;
    alt_t v, fc, bc, rd, jw, ff, hd, s2;   /* values (controller), front cannon, back cannon, rider, jaw, foot FR, head, shoulder F2 */
    double hD, hE, hF;                 /* hitbox D (exposed), E (flash), F (phase); its A / C are the obj's A / c */
    /* groups */
    int g_fight, g_floor, g_m1, g_m2, g_ret, g_c7, g_rider, g_post, g_plane, g_assist, g_chase;
    int vlatch;                        /* values flag 0 (8508 / 8509) */
    int autod;                         /* CTRL.D (8516: the auto-run press) */
    int snd261, snd193r, voice_bc, flash_t;
    float floor_y, cam_left;
    /* laser */
    int laser_on; float lx, ly, lang;
    /* post-boss (0x227c5) */
    int post_t, fade, fade_dir, res_on, kill_time, once8651, hold;
    /* A5 chase */
    int chase_on, lock8370, shown, milla_pan, mus_boss, ctl_back, taunted, pose;
    double camA;
    /* assist (reduced, see U-G3) */
    int as_I, as_st, as_t, as_hits, as_voice, as_dir, last_hit_assist;
    double as_x, as_y, as_vx;
    /* plane (Milla) */
    double plA, plB, plC;
    int glow_vis, wheel_vis, sb_vis;   /* the glow / wheel / speed back are drawn here (their objs stay hidden for objects_draw) */
    float limR;
    int dbg;
} FP;

static obj_t *spawn(int type, int s, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    if (s >= 0) setan(o, s);
    return o;
}
static void act_point(const obj_t *o, double *x, double *y) { relic_act_point(o, x, y); }   /* get_action_x / y (rotated) */
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {      /* PlayerCharacterSensor_375 */
    int X = (int)PX(p), Y = (int)PY(p);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return o->an.b && obj_rect_hit(o, a, b, c, d); }
static int vulnerable(const player_t *p) { return p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p); }   /* MV.R = 0, OTHERS.E <= 0 */
static int is_milla(void) { return chr_is_milla(G.character); }                 /* MV.A 4 (5 = Torque, not playable) */
static int pm(const player_t *p) { return player_orig_m(p); }

int fn_panther_q20(int type) { return type == OT_FNP_HIT; }                      /* A6 qualifier 20: only the hitbox (RE on_frame_35_init) */

/* ===================================================================================================== spawn */
void fn_panther_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&FP, sizeof FP); }
    memset(&FP, 0, sizeof FP);
    if (!objects_have(OT_FNP_HEAD)) return;
    for (int k = 0; k < S_N; k++) SA[k] = ens_anim(SN[k]);
    obj_t *ribs[16], *tails[16]; int nr = 0, nt = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_FNP_FIRST || o->type > OT_FNP_LAST) continue;
        o->an.b = NULL; o->fl = 0; o->c = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        for (int k = 0; k < P_N; k++) if (o->type == PART_TYPE[k] && !FP.p[k]) { FP.p[k] = o; setan(o, PART_SPR[k]); o->layer = 1; }   /* json layer 2 -> port 1 */
        switch (o->type) {
        case OT_FNP_VALUES: FP.val = o; o->hidden = 1; break;
        case OT_CAMLIM_R: break;
        case OT_FNP_RIBS: if (nr < 16) ribs[nr++] = o; setan(o, S_RIB); o->layer = 1; break;
        case OT_FNP_TAIL: if (nt < 16) tails[nt++] = o; setan(o, S_TAIL); o->layer = 1; break;
        case OT_FNP_HIT: FP.hit = o; setan(o, S_HIT); o->hidden = 1; break;
        case OT_FNP_LASER: FP.laser = o; setan(o, S_LASER); o->hidden = 1; break;
        case OT_FNP_TARGET: FP.target = o; setan(o, S_TARGET); o->hidden = 1; break;   /* invisible aiming point (INFERRED) */
        case OT_FNP_GLOW: FP.glow = o; setan(o, S_GLOW); o->hidden = 1; break;
        case OT_FNP_WHEEL: FP.wheel = o; setan(o, S_WHEEL); o->hidden = 1; o->layer = 0; o->scale = 0.01f; break;   /* json layer 1 (0.8): drawn by us */
        case OT_FNP_FLOOR: if (FP.nfloor < NFLOOR) FP.floor_i[FP.nfloor++] = i; else o->alive = 0; setan(o, S_FLOOR); o->av[0] = 672 - o->x; break;   /* A = values.x - x (RV: x = 672 - A) */
        case OT_FNP_SPEEDBACK: FP.sback = o; setan(o, S_SBACK); o->hidden = 1; break;
        case OT_FNP_PLANE: FP.plane = o; setan(o, S_PLANE); o->hidden = 1; break;
        case OT_FNP_ASSIST: FP.assist = o; setan(o, S_ASSIST); o->hidden = 1; break;
        case OT_FNP_AOFF: FP.aoff = o; setan(o, S_AOFF0); o->hidden = 1; break;
        case OT_FNP_ADEF: FP.adef = o; setan(o, S_ADEF); o->hidden = 1; break;
        case OT_FNP_APROJ: FP.aproj = o; o->hidden = 1; break;
        case OT_FNP_SHIP1: setan(o, S_SHIP1); o->layer = 0; break;
        case OT_FNP_SHIP2: FP.ship2 = o; setan(o, S_SHIP2); o->layer = 0; break;
        case OT_FNP_CABLE: setan(o, S_CABLE); o->layer = 0; break;
        case OT_FNP_CABLE2: setan(o, S_CABLE2); o->layer = 0; break;
        }
    }
    FP.limR = 12943;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_CAMLIM_R) FP.limR = G.o[i].x;   /* CameraLimitBossRight (8372) */
    if (!FP.val || !FP.p[P_HEAD] || !FP.p[P_JAW] || !FP.p[P_FCAN] || !FP.p[P_SHF] || nr < 7 || nt < 1) return;
    if (!FP.p[P_BCAN]) { FP.p[P_BCAN] = spawn(OT_FNP_BCAN, -1, 0, 0, 1); if (FP.p[P_BCAN]) FP.p[P_BCAN]->hidden = 1; }   /* FN5 places no back cannon */
    for (int k = 0; k < P_N; k++) if (!FP.p[k]) return;
    /* ribs A = index 0..6: 0 at the front (the largest x; RV: rib A 0 carries the shoulder, A 6 the hip) */
    for (int a = 0; a < nr - 1; a++) for (int b = a + 1; b < nr; b++) if (ribs[b]->x > ribs[a]->x) { obj_t *t = ribs[a]; ribs[a] = ribs[b]; ribs[b] = t; }
    FP.nrib = nr < NRIB ? nr : NRIB; for (int a = 0; a < FP.nrib; a++) FP.rib[a] = ribs[a];
    FP.ntail = nt < NTAIL ? nt : NTAIL; for (int a = 0; a < FP.ntail; a++) { FP.tail[a] = tails[a]; tails[a]->av[0] = a + 1; }   /* tail A 1..6 (json order; RV: instance A 5 at 120 px from the hip) */
    FP.on = 1;
    FP.act = level_cur()->act;
    FP.floor_y = FP.val->y;
    /* constructors (RE spec G.1): values B 20, E -10; jaw B 71 (C 21: RV T-G1); cannons B 56 C 48 D -1 (front E 30, back A 180) */
    V('B') = 20; V('E') = -10; V('N') = 16; JW('B') = 71; JW('C') = 21;
    FC('B') = 56; FC('C') = 48; FC('D') = -1; FC('E') = 30; BC('A') = 180; BC('B') = 56; BC('C') = 48; BC('D') = -1;
    if (FP.act == 6) {
        V('K') = 216; V('L') = 152;                                             /* INFERRED from RV T-G1 frame 0 (K 219 after 3 frames of 8562) */
        FP.hit->A = 60; FP.hit->bd = 0; FP.hit->fl = F_NOFLY; FP.hit->c = 0; FP.hF = 0;   /* ctor A 60; 8818 F = 0 */
        FP.g_fight = 1; FP.g_m1 = 1; FP.g_c7 = 1; FP.g_assist = 1;             /* on_frame_35_init groups (spec_fn_flow 5.4) */
        if (is_milla()) { FP.g_plane = 1; FP.g_floor = 0; } else FP.g_floor = 1;   /* 8814 / 8815 */
        FP.as_I = 1200;                                                          /* INFERRED (RV T-G1: first partner arrival ~frame 1200) */
        FP.as_dir = G.character == CHR_LILAC ? 1 : 0;                            /* INFERRED: Carol helps Lilac, Lilac the others */
        FP.plB = 200; FP.cam_left = 214;
        FP.shield = spawn(OT_FNP_SHIELD, S_SHIELD, -80, 0, 2); if (FP.shield) FP.shield->hidden = 1;
        for (int k = 0; k < P_N; k++) FP.p[k]->hidden = 0;
        if (FP.ship2) FP.ship2->hidden = 1;
    } else {                                                                     /* A5: the rig waits off-screen, hidden (8373 / 8395 show it) */
        V('K') = FP.p[P_SHF]->x; V('L') = 96;                                   /* RV T-F1: K 11868 (= shoulder F's json x), L 96 at the start */
        for (int k = 0; k < P_N; k++) FP.p[k]->hidden = 1;
        for (int a = 0; a < FP.nrib; a++) FP.rib[a]->hidden = 1;
        for (int a = 0; a < FP.ntail; a++) FP.tail[a]->hidden = 1;
    }
    setan(FP.p[P_RIDER], S_RIDER);
#ifdef FP_HOST_ENV
    FP.dbg = getenv("FP_DBG_FP") != NULL;
#endif
}

/* ===================================================================================================== the rig (8554-8560 / 8385-8392) */
static void gait(double blend) {                                                 /* 8555 (8386 + 8387: x (1 - ShoulderF2.A) in A5) */
    double p = 0.5 * V('M');
    V('A') = (100 - 140 * sq(c_sin(p - 110))) * blend; V('C') = (60 - 140 * sq(c_sin(p - 130))) * blend;
    V('B') = (120 * sq(c_sin(p - 90)) - 70) * blend;   V('D') = (100 * sq(c_sin(p - 90)) - 100) * blend;
    V('G') = (160 * sq(c_sin(p - 80)) - 140) * blend;  V('E') = (120 * sq(c_sin(p - 70)) - 60) * blend;
    V('F') = (10 - 60 * sq(c_sin(p - 80))) * blend;
}
static void place_at_ap(obj_t *child, const obj_t *parent) { double x, y; act_point(parent, &x, &y); child->x = (float)(int)x; child->y = (float)(int)y; }
static void rig(double amp, double blend) {
    double I = V('I'), K = V('K'), L = V('L'), M = V('M'), sI = c_sin(I), cI = c_cos(I);
    for (int a = 0; a < FP.nrib; a++) {                                          /* 8554 (U-G1, RE + RV T-G1) */
        double s = c_sin(20 * a + M);
        FP.rib[a]->x = (float)(int)(K + amp * s * sI - 20 * a * cI);
        FP.rib[a]->y = (float)(int)(L + amp * s * cI + 20 * a * sI);
        FP.rib[a]->ang = (float)I;
    }
    gait(blend);                                                                 /* 8555 */
    obj_t **P = FP.p;                                                            /* 8556 angles */
    P[P_SHF]->ang = (float)(V('A') + I); P[P_LEGFR]->ang = (float)(V('C') + I); P[P_FOOTFR]->ang = (float)(V('F') + I);
    P[P_HIP]->ang = (float)(V('B') + I); P[P_KNEE]->ang = (float)(V('E') + I); P[P_LEGFL]->ang = (float)(V('D') + I);
    P[P_FOOTFL]->ang = (float)(V('G') + I); P[P_SHF2]->ang = (float)(0.5 * V('A') - 45 + I); P[P_HEAD]->ang = (float)I;
    P[P_RIDER]->ang = (float)I; P[P_FCAN]->ang = (float)(I + FC('A')); P[P_BCAN]->ang = (float)(I + BC('A')); P[P_JAW]->ang = (float)(I + JW('A'));
    obj_t *r0 = FP.rib[0], *r6 = FP.rib[FP.nrib - 1];
    place_at_ap(P[P_SHF], r0);                                                   /* 8557: shoulder F at rib 0's action point */
    { double s = c_sin(M - 20);                                                  /*       head: the rib before rib 0 (20 (A - 1) + M) */
      P[P_HEAD]->x = (float)(int)(K + amp * s * sI); P[P_HEAD]->y = (float)(int)(L + amp * s * cI); }
    P[P_BCAN]->x = r0->x; P[P_BCAN]->y = r0->y;                                  /*       back cannon on rib 0 */
    P[P_HIP]->x = r6->x; P[P_HIP]->y = r6->y;                                    /* 8558: hip on rib 6 */
    place_at_ap(P[P_LEGFR], P[P_SHF]); place_at_ap(P[P_FOOTFR], P[P_LEGFR]);    /* 8559 */
    for (int a = 0; a < FP.ntail; a++) {
        double A = FP.tail[a]->av[0], w = 30 * c_sin(20 * A + V('J')) - I;
        FP.tail[a]->x = (float)(int)(P[P_HIP]->x - 24 * A * c_cos(w)); FP.tail[a]->y = (float)(int)(P[P_HIP]->y - 24 * A * c_sin(w));
    }
    place_at_ap(P[P_KNEE], P[P_HIP]); place_at_ap(P[P_LEGFL], P[P_KNEE]); place_at_ap(P[P_FOOTFL], P[P_LEGFL]);
    P[P_SHF2]->x = P[P_SHF]->x; P[P_SHF2]->y = P[P_SHF]->y;
    place_at_ap(P[P_RIDER], P[P_HEAD]);
    P[P_FCAN]->x = (float)(int)(P[P_HEAD]->x + FC('B') * cI + FC('C') * sI); P[P_FCAN]->y = (float)(int)(P[P_HEAD]->y - FC('B') * sI + FC('C') * cI);
    if (FP.glow && FP.glow_vis) place_at_ap(FP.glow, P[P_FCAN]);
    if (FP.shield && !FP.shield->hidden) { FP.shield->x = P[P_RIDER]->x; FP.shield->y = P[P_RIDER]->y; }
    P[P_JAW]->x = (float)(int)(P[P_HEAD]->x + JW('B') * cI + JW('C') * sI); P[P_JAW]->y = (float)(int)(P[P_HEAD]->y - JW('B') * sI + JW('C') * cI);
    if (FP.hit) { FP.hit->x = P[P_JAW]->x; FP.hit->y = P[P_JAW]->y; }
    V('M') = fmod(V('M') - V('N'), 360); V('J') = fmod(V('J') - V('N'), 360);  /* 8560 */
}
static void parts_visible(int vis) {
    for (int k = 0; k < P_N; k++) FP.p[k]->hidden = !vis;
    for (int a = 0; a < FP.nrib; a++) FP.rib[a]->hidden = !vis;
    for (int a = 0; a < FP.ntail; a++) FP.tail[a]->hidden = !vis;
}

/* ===================================================================================================== projectiles */
static void fx(float x, float y, const char *name) { obj_effect(x, y, name); }
static obj_t *ball_new(int type, int s, double x, double y, int dir, double speed, int layer) {   /* BallMovement: speed 1/8 px per frame */
    obj_t *o = spawn(type, s, (float)x, (float)y, layer);
    if (o) { o->av[3] = dir & 31; o->av[4] = speed; o->av[5] = o->av[6] = 0; o->av[7] = 0; o->dir = dir & 31; }
    return o;
}
static void ball_move(obj_t *o) {
    double sp = o->av[4];
    if (o->av[7] > 0) { sp -= o->av[7]; if (sp < 0) sp = 0; o->av[4] = sp; }
    int d = (int)o->av[3] & 31;
    double v = sp * 0.125, a = d * 11.25;
    double dx = c_cos(a) * v + o->av[5], dy = -c_sin(a) * v + o->av[6];
    double sx = floor(dx), sy = floor(dy);
    o->av[5] = dx - sx; o->av[6] = dy - sy; o->x += (float)sx; o->y += (float)sy;
}
static int solid_here(const obj_t *o) { return o->y >= FP.floor_y || obj_overlaps_bg(o); }   /* the floor objects' top (Q 0x218c0) or a backdrop */
static void plasma_new(void) {                                                   /* 8575: BossPlasmaShot_2040 at the muzzle (speed 100) */
    double x, y; act_point(FP.p[P_FCAN], &x, &y);
    const player_t *p = &G.p;
    double tx = is_milla() && FP.target ? FP.target->x : PX(p), ty = is_milla() && FP.target ? FP.target->y : PY(p);
    double th = atan2(-(ty - y), tx - x) * 57.29577951308232; if (th < 0) th += 360;   /* aimed at the player (INFERRED: the decompile drops the atan2 operands) */
    ball_new(OT_FNP_PLASMA, S_PLASMA, x, y, (int)(th / 11.25), 100, 2);
    fx((float)x, (float)y, "fp_spark");                                          /* EffectGunSpark_1887 */
    snd_play_orig(436, 220, 128);
}
static void missile_new(double x, double y, int dir, double speed, int turning) {   /* BossMissile_2041 (speed 70, deceleration 30) */
    obj_t *m = ball_new(OT_FNP_MISSILE, S_MISSILE, x, y, dir, speed, 2);
    if (!m) return;
    m->av[7] = turning ? 0 : 1.25;                                               /* deceleration 30 = 320 / 256 per frame (INFERRED from the accelerator table) */
    m->fl = turning ? 2 : 0; m->av[1] = dir; m->tm = 0; m->st = 0;               /* flag 1: turning down; F = direction */
}

/* ===================================================================================================== A6: floor / plane modes */
static void floor_mode(player_t *p) {
    int M = pm(p);
    if (G.character == CHR_CAROL_BIKE) p->rev = p->rev;                         /* 8507: MV.E = 0.074 (bike acceleration, not modelled) */
    if (!p->ground && !FP.vlatch) { FP.vlatch = 1; if (p->vx > -100) p->vx -= 10; }   /* 8508: leaving the floor: B -= 10 */
    if (p->ground && FP.vlatch) { FP.vlatch = 0; p->gsp += 10; }                 /* 8509: landing: B += 10 */
    if (M == 9 && p->x < G.camx + 16) p->x = G.camx + 16;                        /* 8512 */
    if (p->ground && M == 5) p->x += 10;                                         /* 8513: the boost runs with the world */
    if (p->ground && M == 5 && p->state == ST_CHARGE) p->x = fmax(p->x - 10, G.camx);   /* 8514: charging (OTHERS.N < 30) */
    if (p->ground && M != 8 && M != 9 && p->hitstop <= 0) p->x -= 10;            /* 8518: the treadmill */
    if (p->ground && M == 4) p->skidding = 0;                                    /* 8519: M 4 -> 0 */
    if (p->state == ST_WALL && p->vx < 0 && p->x <= G.cam_nC + 18) { p->x = G.cam_nC + 18; p->vx = -1; }   /* 8523 (OTHERS.Y 7) */
}
static void floor_scroll(int plane) {                                            /* 8520-8522 / 8529-8531 */
    V('O') += 15;
    float vx = FP.val->x;
    for (int k = 0; k < FP.nfloor; k++) {
        if (FP.floor_i[k] < 0) continue;
        obj_t *f = &G.o[FP.floor_i[k]];
        if (!f->alive || f->type != OT_FNP_FLOOR) { FP.floor_i[k] = -1; continue; }
        f->av[0] += 15; f->x = (float)(int)(vx - f->av[0]);
        if (!plane) f->y = (float)(int)FP.val->y;
    }
    if (V('O') >= 128) {                                                         /* 8521: a new piece at the values (A = O) */
        V('O') -= 128;
        int slot = -1;
        for (int k = 0; k < FP.nfloor && slot < 0; k++) if (FP.floor_i[k] < 0) slot = k;
        if (slot < 0 && FP.nfloor < NFLOOR) slot = FP.nfloor++;
        obj_t *f = slot >= 0 ? spawn(OT_FNP_FLOOR, S_FLOOR, vx, FP.val->y, 2) : NULL;
        if (f) { f->av[0] = V('O'); f->x = (float)(int)(vx - f->av[0]); FP.floor_i[slot] = (int)(f - G.o); }
    }
    for (int k = 0; k < FP.nfloor; k++) {                                        /* 8522: gone past x -128 */
        if (FP.floor_i[k] < 0) continue;
        obj_t *f = &G.o[FP.floor_i[k]];
        if (f->av[0] > 800) { f->alive = 0; FP.floor_i[k] = -1; }
    }
}
static void plane_mode(player_t *p) {                                            /* 8524-8528 (Milla on PlayerAssist's plane) */
    if (!FP.plane || !FP.assist) return;
    if (G.frame < 4) { p->x = 520; p->y = 170 + ORIG_REF_DY; FP.plA = 520; FP.plB = 200; }   /* 8524 (g3 < 4) */
    uint32_t h = p->held;
    if (h & BTN_UP) FP.plB = fmax(FP.plB - 2, 70);                               /* 8526 (CTRL.A) */
    if (h & BTN_DOWN) FP.plB = fmin(FP.plB + 2, 230);                            /* 8527 (CTRL.B) */
    double oldy = FP.plane->y;
    FP.assist->hidden = 0; FP.assist->x = (float)(int)p->x;                      /* 8525 */
    FP.assist->y = (float)(int)(FP.plB + 8 * c_cos(2 * FP.plC + 90));
    FP.plC = fmod(FP.plC + 0.7, 360);
    FP.plane->x = (float)(int)p->x; FP.plane->y = FP.assist->y;
    if (p->ground && fabs((p->y) - oldy) <= 3) p->y = FP.plane->y;               /* standing on the platform: carried (INFERRED) */
    G.cam.H = (float)(100 - 0.5 * PY(p));
    if (p->x > G.lvl.w) p->x = G.lvl.w;
    setan(FP.assist, S_ASSIST);
}

/* ===================================================================================================== A6: the fight (0x227ba) */
static void body_moves(const player_t *p) {                                      /* 8561-8566, mode 2 8620 / 8621 */
    double K = V('K'), vx = G.camx, D = FC('D');
    if (FP.g_m1 && !FP.g_rider) {
        double t1 = fmin(vx + 246, PX(p) - 180), t2 = fmin(vx + 246, PX(p) - 220);
        if (K > t1 && D > -2 && D < 3) V('K') = K -= 1;
        if (K < t2 && D > -2 && D < 3) V('K') = K += 1;
        if (D >= 3 && K > vx + 186) V('K') = K -= 1;
        if (D >= 3 && K < vx + 176) V('K') = K += 1;
    }
    if (FP.g_m1 && FP.g_rider && RD('B') == 0) {
        if (K > vx + 236) { V('K') = K -= 2; FC('E') = -60; }
        if (K < vx + 216) { V('K') = K += 2; FC('E') = -60; }
    }
    if (FP.g_m2) { if (K > vx + 266) V('K') = K - 2; else if (K < vx + 266) V('K') = K + 2; }   /* (spec G.2, INFERRED step) */
}
static void cannon(player_t *p) {                                                /* 8567-8586 (mode 1, group 0x227bd) */
    obj_t *fc = FP.p[P_FCAN];
    if (FC('D') == -1 && FC('E') <= 150 && FP.hF >= 3 && FP.hF < 7 && !(FP.fc.fl & 8)) {   /* 8567 / 8568: the wheel stance */
        if (!FP.snd261) { FP.snd261 = 1; snd_play_orig(261, 220, 128); }
        FP.fc.fl |= 8; FP.g_m1 = 0; FP.g_m2 = 1; FP.g_c7 = 1; snd_play_orig(165, 220, 128);
        return;
    }
    if (FC('D') == -1 && pm(p) != 9) FC('E') += 1;                               /* 8569 */
    if (FC('D') == -1 && FC('E') > 150 && FC('F') < 3) { FC('D') = 0; FC('E') = 0; FC('F') += 1 + R(2); }   /* 8570 */
    if (FC('D') <= 0 && is_milla() && FP.target) {                               /* 8571: Milla aims at the mirrored point */
        FP.target->x = (float)(int)(2 * p->x - fc->x); FP.target->y = (float)(int)(2 * PY(p) - fc->y);
        double th = atan2(-(int)(PY(p) - fc->y), (int)(p->x - fc->x)) * 57.29577951308232; if (th < 0) th += 360;
        FC('A') = (int)th + 30;
    }
    if (FC('D') == 0) { FC('D') = 1; FC('E') = 0; FP.glow_vis = 1; snd_play_orig(191, 220, 128); }   /* 8572 */
    if (FC('D') == 1) { if (FP.glow) { FP.glow->scale = (float)fmax(2 - 0.08 * FC('E'), 0.1); place_at_ap(FP.glow, fc); } FC('E') += 1; }   /* 8573 */
    if (FC('D') == 1 && FC('E') >= 25) { FP.glow_vis = 0; FC('E') = 0; FC('D') = 2; }   /* 8574 */
    if (FC('D') == 2 && pm(p) != 9 && G.frame % 5 == 0) { plasma_new(); FC('E') += 1; }   /* 8575 (g3 mod 5) */
    if (FC('D') == 2 && FC('E') >= 12) { FC('D') = -1; FC('E') = R(60); }       /* 8577 */
    if (FC('D') == -1 && FC('E') > 0 && FC('F') >= 3 && (FP.fc.fl & 8)) {        /* 8578: the laser charge */
        FC('E') = 0; FC('D') = 3; FP.glow_vis = 1; if (FP.glow) FP.glow->scale = 2; snd_play_orig(200, 220, 128);
    }
    if (FC('D') >= 3) { FC('G') = fmod(FC('G') + 3, 360); FC('A') = 70 * c_cos(FC('G')) - 70; }   /* 8579: the barrel sweeps down and back */
    if (FC('D') == 3) { if (FP.glow) { FP.glow->scale = (float)fmax(2 - 0.02 * FC('E'), 0.1); place_at_ap(FP.glow, fc); } FC('E') += 1; }   /* 8580 */
    if (FC('D') == 3 && FC('E') >= 100) { FC('E') = 0; FC('D') = 4; FP.glow_vis = 0; FP.laser_on = 1; snd_play_orig(169, 220, 128); }   /* 8581 */
    if (FC('D') == 4) {                                                          /* 8582: the laser follows the muzzle, gun sparks */
        double x, y; act_point(fc, &x, &y); FP.lx = (float)x; FP.ly = (float)y; FP.lang = fc->ang;
        if (G.frame % 4 == 0) fx((float)x, (float)y, "fp_spark");                /* (one spark every 4 frames: INFERRED density) */
        FC('E') += 1;
    }
    if (FC('D') == 4 && FP.laser_on && pm(p) != 9 && !p->hurt_pending && !player_invincible(p) && p->shield != 3) {   /* 8584: 10 damage (not with the metal shield) */
        double a = FP.lang, ux = c_cos(a), uy = -c_sin(a), dx = PX(p) - FP.lx, dy = PY(p) - 10 - FP.ly;
        double along = dx * ux + dy * uy, across = fabs(-dx * uy + dy * ux);
        if (along > 0 && along < 650 && across < 12 + 17) player_contact(p, 10);   /* laser 24 px high x 650 px; the sensor's half height 17 (INFERRED box) */
    }
    if (FC('D') == 4 && FC('E') >= 200) { FC('D') = 5; FP.laser_on = 0; }       /* 8583 */
    if (FC('D') == 5 && FC('G') <= 3) { FP.fc.fl &= ~8u; FC('A') = 0; FC('G') = 0; FC('D') = -1; }   /* 8585 */
    if (FC('D') == -1 && FC('E') > 0 && FC('F') >= 3 && !(FP.fc.fl & 4)) {      /* 8586: the rider's mortar */
        FC('E') = 0; FC('F') = 0; FP.g_rider = 1; FP.fc.fl ^= 4;
    }
    if (FC('D') == -1 && FC('E') > 0 && FC('F') >= 3 && (FP.fc.fl & 4)) {       /* 8596: the back-cannon volley */
        FC('E') = -120; FC('F') = 0; FP.fc.fl ^= 4; BC('D') += 1; snd_play_orig(254, 220, 128);
    }
}
static void rapid_new(void) {                                                    /* 8590 / 8591: BossRapidBullet_1866 from the rider */
    obj_t *r = FP.p[P_RIDER];
    obj_t *b = spawn(OT_FNP_RBULLET, S_RBUL, r->x, r->y, 2);
    if (!b) return;
    int e = (int)RD('E') & 1;
    b->av[2] = e + 2; b->av[3] = 2 * e - 1; b->av[0] = 0; b->av[1] = 0;          /* C, D (RE), A phase, B counter */
    RD('E') = e ^ 1;
    snd_play_orig(194, 220, 128);
}
static void rider(void) {                                                        /* 8587-8595 (0x227c9) */
    obj_t *r = FP.p[P_RIDER];
    if (!FP.g_rider) return;
    if (RD('B') == 0 && V('K') >= G.camx + 216 && !(FP.rd.fl & 8)) { FP.rd.fl |= 8; snd_play_orig(193, 220, 128); }   /* 8587 */
    if (RD('B') == 0 && V('K') >= G.camx + 216) {                                /* 8588 */
        RD('B') = 1; restartan(r, S_RIDER12); RD('E') = 0; RD('G') = 0;
        if (FP.shield) { FP.shield->hidden = 0; FP.shield->x = r->x; FP.shield->y = r->y; }
    }
    if (RD('B') >= 1) {
        RD('G') += 1;                                                            /* (RV T-G1: anim 12 for 8 frames, 13 for 95, 12 for 8) */
        if (RD('G') == 9) restartan(r, S_RIDER13);                               /* 8589 */
        if (RD('G') >= 13 && RD('G') <= 100 && ((int)RD('G') - 13) % 3 == 0) rapid_new();   /* 8590 / 8591: frames 5 and 8 (RV: one every 3 frames, 30) */
        if (RD('G') == 104) restartan(r, S_RIDER12);                             /* 8592 */
        if (RD('G') >= 112) {                                                    /* 8593-8595 */
            setan(r, S_RIDER); RD('B') = 0; RD('G') = 0; FP.rd.fl &= ~8u; FP.g_rider = 0;
            if (FP.shield) { FP.shield->hidden = 1; FP.shield->x = -80; FP.shield->y = 0; }
        }
    }
}
static void back_cannon(void) {                                                  /* 8597-8607 */
    obj_t *bc = FP.p[P_BCAN];
    if (BC('D') == 0) {
        BC('A') = fmax(BC('A') - 10, 0);                                         /* 8597 */
        if (!FP.voice_bc) {                                                      /* 8598-8601: the character's voice (once each opening) */
            FP.voice_bc = 1;
            snd_play_orig(G.character == CHR_LILAC ? 258 : is_milla() ? 259 : 259, 220, 128);   /* MV.A 0 -> 258, 1 / 3-4 -> 259, 5 -> 452 (RE) */
        }
        if (BC('A') <= 0) { BC('D') = 1; setan(bc, S_BCAN1); BC('F') = 0; BC('E') = 0; }   /* 8602 */
    }
    if (BC('D') >= 0) BC('E') += 1;                                              /* 8603 */
    if (BC('D') == 1 && BC('E') > 60 && G.frame % 10 == 0 && BC('F') <= 10) {   /* 8604: a missile every 10th frame */
        BC('F') += 1; double x, y; act_point(bc, &x, &y);
        missile_new(x, y, ((int)BC('F') % 6) + 3, 70, 0); snd_play_orig(195, 220, 128);
    }
    if (BC('D') == 1 && BC('E') > 120 && BC('F') > 10) { BC('D') = 2; setan(bc, S_BCAN); BC('E') = 0; }   /* 8605 */
    if (BC('D') == 2 && BC('E') > 60) BC('A') += 10;                             /* 8606 */
    if (BC('D') == 2 && BC('A') >= 180) { BC('A') = 180; BC('D') = -1; FP.voice_bc = 0; }   /* 8607 */
}
static void mode2(void) {                                                        /* 8608-8634 (0x227bf) */
    obj_t *ff = FP.p[P_FOOTFR];
    if (FP.g_c7) {
        if (V('I') < 90) { V('I') += 1; V('N') -= 0.1; V('L') -= 0.35; }         /* 8608 */
        if (V('I') >= 45 && FP.wheel) { FP.wheel_vis = 1; FP.wheel->scale = (float)fmin(FP.wheel->scale + 0.1, 1); }   /* 8609 */
        if (V('I') >= 90) { FP.g_c7 = 0; restartan(ff, S_FOOTFR7); snd_play_orig(166, 220, 128); }   /* 8610 (stop 165) */
    }
    if (V('I') >= 90 && FC('D') < 0) { FC('E') = 0; FC('D') = 0; FP.ff.fl ^= 2; }   /* 8622 */
    if (!(FP.ff.fl & 2)) FC('A') = fmin(FC('A') + 4, 0); else FC('A') = fmax(FC('A') - 4, -20);   /* 8623 / 8624 */
    FF('A') += 1;                                                                /* 8625 */
    if (FF('A') > 120 && FF('B') < 24) {
        if (fmod(FF('B'), 6) <= 2) {                                             /* 8626: bursts of three */
            double x, y; act_point(ff, &x, &y);
            V('P') = 30; fx((float)x, (float)y, "explosion02"); snd_play_orig(195, 220, 128);
            missile_new(x, y, 31, 120, 1);
        }
        FF('A') = 100; FF('B') += 1;                                             /* 8627 */
    }
    if (V('P') > 0) V('P') -= 2;                                                 /* 8628 */
    if (HD('A') <= 0 && FF('A') > 150 && !FP.g_ret) { FP.g_ret = 1; snd_play_orig(165, 220, 128); }   /* 8629 */
    if (FP.g_ret) {                                                              /* 8631-8634: the return */
        setan(ff, S_FOOTFR8);
        if (V('I') > 0) { V('I') = fmax(V('I') - 2, 0); V('L') += 0.7; V('N') = fmin(V('N') + 0.2, 16); }
        FP.ff.fl &= ~2u;
        if (FP.wheel) FP.wheel->scale = (float)fmax(FP.wheel->scale - 0.1, 0.4);
        if (V('I') <= 0) {
            FP.g_ret = 0; FP.g_m1 = 1; FP.g_m2 = 0; FP.wheel_vis = 0; FF('A') = FF('B') = 0;
            setan(ff, S_FOOTFR); snd_play_orig(166, 220, 128);
        }
    }
}
static void hits(void) {                                                         /* 8635-8645 */
    obj_t *h = FP.hit, *jaw = FP.p[P_JAW];
    int f6 = DEFEATED(h) != 0;
    if (!f6 && FP.hD == 0 && JW('A') < 0 && FP.hF < 5) JW('A') = fmin(JW('A') + 4, 0);   /* 8635: the mouth closes */
    FP.flash_t = h->c > 1 && !(h->fl & 0x20) ? (G.frame % 2 == 0) : 0;            /* 8636 / 8637: invert every 2 frames */
    if (f6 && FP.hD == 0 && JW('A') > -60) JW('A') = fmax(JW('A') - 4, -60);    /* 8638: the mouth opens */
    if (f6 && FP.hD == 0 && JW('A') <= -60 && FP.hF < 4) { h->fl &= ~0x40; h->A = 1; h->c = 30; FP.hD = 1; f6 = 0; }   /* 8639: exposed */
    if (f6 && FP.hD == 1 && FP.hF < 4) {                                         /* 8640: the next pool */
        h->fl &= ~0x40; FP.hD = 0; h->c = 100; h->A = FP.hF * 10 + 60; FP.hE = 50; f6 = 0;
        snd_play_orig(47, 220, 128); FP.fc.fl |= 8; FP.hF += 1; h->inv = 0;
    }
    if (FP.hE > 1) { FP.hE -= 1; }                                               /* 8641: parts' direction E mod 2 (flash images) */
    if (FP.hE == 1) FP.hE = 0;                                                   /* 8642 */
    if (f6 && FP.hD == 0 && FP.hF >= 4 && !jaw->hidden) {                        /* 8644: the jaw is destroyed */
        jaw->hidden = 1; snd_play_orig(29, 220, 128); snd_play_orig(257, 220, 128); fx(jaw->x, jaw->y, "explosion06");
        for (int k = 0; k < 5; k++) fx((float)(jaw->x - 16 + R(33)), (float)(jaw->y - 16 + R(33)), "debris");
    }
    if (FP.hF >= 4) setan(jaw, S_JAW7);                                          /* 8643 */
    if (f6 && FP.hD == 1 && FP.hF >= 4) {                                        /* 8645: stays exposed */
        h->fl &= ~0x40; h->A = 1; h->c = 100; FP.hD = 1; FP.hE = 50; FP.hF += 1; snd_play_orig(47, 220, 128); FP.fc.fl |= 8; h->inv = 0;
    }
    if (f6 && FP.hD == 0 && FP.hF >= 4) { h->fl &= ~0x40; h->A = 1; h->c = 30; FP.hD = 1; }   /* the broken jaw: exposed at once (8639 needs the mouth, INFERRED) */
}
static void speed_fx(void) {                                                     /* 8646-8650 (mode 2 only) */
    if (!FP.sback) return;
    if (FP.g_m2) { FP.sb_vis = 1; FP.sback->av[0] = fmin(FP.sback->av[0] + 0.5, 80); }
    else FP.sback->av[0] = fmax(FP.sback->av[0] - 2, 0);                         /* (fade-out when the stance ends: INFERRED) */
    if (FP.sback->av[0] <= 0) FP.sb_vis = 0;
    if (FP.g_m2 && G.frame % 2 == 0) spawn(OT_FNP_SLINE, S_SLINE, 660, (float)(G.camy + R(241)), 1);   /* 8647 (rate INFERRED) */
}
static void defeat(player_t *p) {                                                /* 8651 */
    if (FP.once8651 || FP.hF < 7) return;
    FP.once8651 = 1;
    FP.laser_on = 0; FP.g_rider = 0; FP.glow_vis = 0;
    FP.g_post = 1; G.timer_stop = G.frame + 1; FP.kill_time = game_timer();
    FP.hE = 999; if (FP.shield) FP.shield->hidden = 1;
    music_fade(0, 0); music_stop();
    (void)p;
}

/* ---- runtime objects: one pass (8652-8667 + the collision handlers) */
static void runtime_pass(player_t *p) {
    int dead = p->state == ST_DEAD || p->hp <= 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_FNP_PLASMA || o->type > OT_FNP_INSTA) continue;
        if (o->born == G.frame) continue;                                        /* created by this frame's events: moved next frame */
        switch (o->type) {
        case OT_FNP_PLASMA:                                                      /* col_14_34 / col_18_34 */
            if (is_milla() && FP.target) {                                       /* 8658: re-aimed at the target */
                double th = atan2(-(FP.target->y - o->y), FP.target->x - o->x) * 57.29577951308232; if (th < 0) th += 360;
                o->av[3] = (int)(th / 11.25);
            }
            ball_move(o); o->dir = (int)o->av[3];
            if (solid_here(o)) { fx(o->x, o->y, "explosion02"); o->alive = 0; break; }
            if (!dead && csens_hit(p, o) && vulnerable(p)) { player_contact(p, 5); o->alive = 0; break; }
            if (player_cube_state() >= 4 && player_cube_hits(o)) { o->alive = 0; break; }   /* col_20_34: Milla's cube absorbs it */
            if (o->x < G.camx - 64 || o->x > G.camx + SCREEN_W + 64 || o->y < G.camy - 64) o->alive = 0;
            break;
        case OT_FNP_MISSILE:
            if (o->st == 0) {                                                    /* flying */
                if (!(o->fl & 2)) o->av[1] = o->av[3];                           /* 8659 */
                ball_move(o);
                if (!(o->fl & 2) && o->av[4] <= 19) o->fl |= 2;                  /* 8661 */
                if (o->fl & 2) { o->av[4] += 2; o->av[7] = 0; }                  /* 8662 */
                if (!(o->fl & 2) && o->av[1] > 0 && G.frame % 2 == 0) o->av[3] = ((int)o->av[3] + 31) & 31;   /* 8663 */
                if ((o->fl & 2) && (int)o->av[3] != 16 && G.frame % 4 == 0) o->av[3] = ((int)o->av[3] + 31) & 31;   /* 8664 */
                int d = (int)o->av[3]; setan(o, d > 8 && d < 24 ? S_MISSILE_L : S_MISSILE); o->ang = (float)(d > 8 && d < 24 ? d * 11.25 - 180 : d * 11.25);
                int hit = solid_here(o) || (is_milla() && !dead && csens_hit(p, o));   /* col_6_34 / col_11_34 */
                if (player_cube_state() >= 4 && player_cube_hits(o)) { snd_play_orig(219, 220, 128); hit = 1; }   /* col_10_34 */
                if (hit) {                                                       /* flag 6 -> the blast (8665) */
                    if (is_milla()) snd_play_orig(38, 220, 128);
                    o->st = 1; o->tm = 0; o->ang = 0; setan(o, S_MBLAST); fx(o->x, o->y, "explosion06"); snd_play_orig(29, 220, 128);
                }
                if (o->x < G.camx - 200 || o->x > G.camx + SCREEN_W + 200 || o->y < G.camy - 400) o->alive = 0;
            } else {                                                             /* the blast */
                o->tm++;
                if (o->tm < 30 && !dead && csens_hit(p, o) && vulnerable(p) && p->shield != 2) { player_contact(p, 10); p->vy = -8; p->ground = 0; }   /* 8666 */
                if (o->tm >= 45) o->alive = 0;                                   /* 8667 */
            }
            break;
        case OT_FNP_RBULLET:                                                     /* the A3 mortar copy (8652-8656, col_0_34) */
            if (o->av[0] == 0) {
                o->y -= 16; o->av[1] += 1;
                if (o->av[1] > 50) {                                             /* 8652: falls at x + 16 C D with a ground warning */
                    o->av[0] = 1; o->av[1] = 0; o->x = (float)(int)(o->x + 16 * o->av[2] * o->av[3]); setan(o, S_RBUL_D);
                    obj_t *w = spawn(OT_FNP_WARN, S_WARN, o->x, (float)(V('L') + 120), 2);
                    if (w) { w->av[0] = i; if (is_milla()) w->y = G.camy + 20; }   /* 8655: Milla's warning at the view top */
                }
            } else {
                o->y += 16;
                if (solid_here(o)) {                                             /* 8656 */
                    fx(o->x, o->y, "explosion02"); snd_play_orig(45, 220, 128); o->alive = 0; break;
                }
            }
            if (!dead && csens_hit(p, o) && !p->hurt_pending) {                  /* col_0_34: 1 damage, no invulnerability test */
                snd_play_orig(45, 220, 128); fx(o->x, o->y, "explosion02"); player_contact(p, 1); o->alive = 0; break;
            }
            if (o->y > G.lvl.h + 64) o->alive = 0;
            break;
        case OT_FNP_WARN: {                                                      /* gone with its bullet (INFERRED) */
            int b = (int)o->av[0];
            if (b < 0 || b >= G.nobjs || !G.o[b].alive || G.o[b].type != OT_FNP_RBULLET) o->alive = 0;
            break;
        }
        case OT_FNP_SLINE:                                                       /* 8648 / 8650 */
            o->x -= 32; if (o->x < -51 + G.camx - 214 + 214) o->alive = 0; if (!FP.g_m2 && !FP.g_post) o->alive = 0;
            break;
        case OT_FNP_INSTA:                                                       /* the partner's attack (ShieldInstaShield_407, flag 4) */
            if (++o->tm > 6) o->alive = 0;
            break;
        }
    }
    /* the rider's shield touching the player: 5 damage (8501 / the 8062 copy) */
    if (FP.shield && !FP.shield->hidden && !dead && csens_hit(p, FP.shield) && vulnerable(p)) player_contact(p, 5);
}

/* ---- the partner (group 0x227c8; reduced port of 8668-8747: U-G3) */
static void assist(player_t *p) {
    obj_t *a = FP.aoff, *h = FP.hit;
    if (!a || !FP.g_assist) return;
    if (FP.as_st == 0) {
        if (FP.as_I > 2) FP.as_I--;                                              /* 8668 */
        if (FP.as_I == 2 && !FP.g_m2 && !(FC('D') >= 3 && FC('D') <= 5)) {       /* 8669 */
            FP.as_I = 1; FP.as_st = 1; FP.as_t = 0; FP.as_x = G.camx - 40; FP.as_y = h->y; FP.as_hits = 0;
            if (!FP.as_voice) { FP.as_voice = 1; snd_play_orig(126, 220, 128); }  /* 8670 / 8671 (once) */
        }
        a->hidden = 1; return;
    }
    a->hidden = 0;
    double tx = h->x - 40, ty = h->y - 10;
    FP.as_t++;
    if (FP.as_st == 1) {                                                         /* fly in */
        FP.as_x += (tx - FP.as_x) * 0.08; FP.as_y += (ty - FP.as_y) * 0.08;
        setan(a, FP.as_dir ? S_AOFF1 : S_AOFF0);
        if (fabs(tx - FP.as_x) < 8 || FP.as_t > 120) { FP.as_st = 2; FP.as_t = 0; }
    } else if (FP.as_st == 2) {                                                  /* 10 strikes, one every 6 frames (RV: sound 137 x 10) */
        FP.as_x = tx; FP.as_y = ty;
        setan(a, FP.as_dir ? S_AOFF7B : S_AOFF7);
        if (FP.as_t % 6 == 0) {
            snd_play_orig(137, 220, 128);
            obj_t *s = spawn(OT_FNP_INSTA, -1, (float)(FP.as_x + 20), (float)FP.as_y, 2);
            (void)s;
            if (h->c <= 0 && !DEFEATED(h) && !(h->fl & 0x20)) {                  /* the shared hit on Q20 (RV: ~4 HP per strike, C 5) */
                h->c = 5; h->A -= 4; h->inv = 1; snd_play_orig(24, 220, 128); FP.last_hit_assist = G.frame;
            }
            if (++FP.as_hits >= 10) { FP.as_st = 3; FP.as_t = 0; }
        }
    } else {                                                                     /* leave */
        FP.as_x -= 6; FP.as_y -= 2;
        if (FP.as_x < G.camx - 80) { FP.as_st = 0; FP.as_I = 600; a->hidden = 1; }   /* 8687: I = 600 */
    }
    if (FP.as_st && FP.as_st != 3) {                                             /* 8684: knocked away by the panther's shots (sound 25) */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || (o->type != OT_FNP_PLASMA && o->type != OT_FNP_RBULLET)) continue;
            if (fabs(o->x - FP.as_x) < 20 && fabs(o->y - FP.as_y) < 20) { snd_play_orig(25, 220, 128); FP.as_st = 3; FP.as_t = 0; o->alive = 0; break; }
        }
    }
    a->x = (float)(int)FP.as_x; a->y = (float)(int)FP.as_y;
    (void)p;
}

/* ---- post-boss (0x227c5, 8748-8785) and the results */
static void post_boss(player_t *p) {
    obj_t *hd = FP.p[P_HEAD];
    FP.post_t++;
    if (FP.post_t == 1) {                                                        /* 8749 / 8750 */
        setan(FP.p[P_RIDER], S_RIDER1); FC('D') = -2; FC('E') = 0; FF('A') = 0;
        if (BC('D') >= 0 && BC('D') < 2) { BC('D') = 2; BC('E') = 99; }
    }
    if (HD('A') == 1 && G.frame == FP.post_t + 0) { }
    if (HD('C') == 0) {
        if (HD('A') < 60 && FP.post_t % 6 == 0) {                                /* 8751 / 8752 (RV: A + 1 every 6 frames below 60) */
            fx((float)(hd->x + R(81)), (float)(hd->y - 40 + R(81)), "explosion02");
            fx((float)(hd->x + R(81)), (float)(hd->y - 40 + R(81)), "explosion06");
            HD('A') += 1;
            static const int S[3] = { 311, 98, 99 }; snd_play_orig(S[R(3)], 220, 128);   /* "sound_explode" + rand(3) */
            if (HD('A') == 1) { ach_boss_finish(); if (G.frame - FP.last_hit_assist < 8) ach_earn(21); }   /* 8790-8801 (21: the partner's strike, INFERRED window) */
            if (HD('A') == 60) { HD('B') = 640; snd_play_orig(256, 220, 128); }  /* 8757 (RV: B 640, -4 a frame) */
        } else if (HD('A') >= 60) {
            if (G.frame % 4 == 0 && HD('A') < 100) {                             /* 8753-8755 */
                fx((float)(hd->x + R(81)), (float)(hd->y - 40 + R(81)), "explosion06"); fx((float)(hd->x + R(81)), (float)(hd->y - 40 + R(81)), "debris");
                HD('A') += 1;
            }
            if (G.frame % 5 == 0) { static const int S[3] = { 311, 98, 99 }; snd_play_orig(S[R(3)], 220, 128); }   /* 8756 */
            if (HD('B') > 0) HD('B') -= 4;
            if (HD('B') <= 0) { HD('C') = 1; HD('D') = 0; snd_play_orig(76, 230, 128); FP.fade = 0; FP.fade_dir = 1; }   /* 8760 */
        }
    }
    if (HD('C') == 1) {
        if (FP.fade_dir > 0) FP.fade = FP.fade + 20 > 255 ? 255 : FP.fade + 20;  /* 8761 */
        HD('D') += 1;
        if (FP.fade >= 255 && FP.fade_dir > 0) {                                 /* 8762 */
            FP.fade_dir = 0; G.input_lock = 1; FP.g_floor = 0; FP.g_fight = 0; FP.g_assist = 0; FP.g_plane = 0;
            p->vx = 0; p->gsp = 0; if (FP.aoff) FP.aoff->hidden = 1; FP.sb_vis = 0;
            par_set_drift(1629, 0); par_set_drift(1640, -0.25f);                 /* piece0a G 0, boat1 G + 0.1 */
            if (G.character == CHR_CAROL_BIKE) player_bike_off(p, 0);            /* 8763 */
            if (FP.assist) FP.assist->hidden = !is_milla();
        }
        if (HD('D') >= 100) { HD('C') = 2; HD('D') = 1; FP.fade_dir = -1; }     /* (RV T-G2: C 2 99 frames after the flash) */
    }
    if (HD('C') == 2) {
        if (FP.fade_dir < 0) { FP.fade = FP.fade - 4 < 0 ? 0 : FP.fade - 4; if (FP.fade <= 0) FP.fade_dir = 0; }   /* 8772 / 8773 */
        if (HD('D') == 130 && FP.hold < 113) FP.hold++;                          /* (RV: D waits ~113 frames near 130; INFERRED pose wait) */
        else HD('D') += 1;
        if (HD('D') > 160 && !FP.res_on) {                                       /* 8785: Sign.B 179 -> the results */
            FP.res_on = 1; G.input_lock = 0; if (p->state != ST_DEAD) p->state = ST_NORMAL;
            results_begin_boss(FP.kill_time);
        }
    }
}

/* ===================================================================================================== A5: intro and chase */
static void boss_music(void) {                                                    /* stop_music(4), play_music(4, 0, 1), reset_music(5) */
    const char *m = level_music_slot(4);
    music_fade(0, 0); music_stop(); music_play(m ? m : "bgm/m_boss.adp");
}
static void chase(player_t *p) {
    alt_t *S = &FP.s2;
    double *B = FP.ship2 ? &FP.ship2->av[1] : &FP.hd.a[1];                       /* Ship2.B (timeline) */
    const float limR = FP.limR;
    if (!FP.lock8370 && V('K') < G.camx + 426) { FP.lock8370 = 1; fn_flow_panther_view(); level_lock_markers(); }   /* 8370 */
    if (!FP.chase_on && p->x > limR - 300 - 20 * (p->ground ? p->gsp : p->vx)) {   /* 8372 */
        FP.chase_on = 1; G.timer_stop = G.frame + 1; G.input_lock = 1; music_fade(180, 0);   /* timer / pause off, CTRL.M 1, fadeout_music(3000) */
        if (is_milla()) {                                                        /* 8373: the panther at once */
            V('K') = G.camx + SCREEN_W + 1000; parts_visible(1); V('N') = 0; V('L') += 10; AV(*S, 'A') = 1;
        } else *B = 270;                                                         /* 8379 */
    }
    if (!FP.chase_on) return;
    if (*B < 60) *B += 1;                                                        /* 8374 */
    else if (*B >= 270) *B += 1;                                                 /* 8380 */
    double b = *B;
    if (b == 20 && is_milla()) { FP.pose = 1; voice_play_once("milla_gasp"); }   /* 8375 (mi_milla_gasp, not converted: silent) */
    if (b >= 60 && b <= 61 && is_milla()) {                                      /* 8376-8378: the camera pans to the panther, then on */
        if (!FP.milla_pan) { FP.milla_pan = 1; FP.camA = G.camx + 213; }
        FP.camA += 15; G.cam.M3 = 1; G.cam.A = (float)FP.camA; G.cam.B = G.camy + 120;
        if (b == 60) *B = 61;
        if (b == 61 && FP.camA >= V('K') + 200) { *B = 439; p->x = V('K') + 200; FP.mus_boss = 1; boss_music(); G.cam.M3 = 0; }   /* RV T-F1 (Milla): X = K + 200 */
    }
    if (b == 360 && !FP.shown) { FP.shown = 1; V('K') = G.camx - 700; parts_visible(1); }   /* 8395 */
    if (b > 360 && AV(*S, 'B') == 0) {
        if (V('K') < G.camx - 50) V('K') += 16 - 16 * AV(*S, 'A');               /* 8396 */
        else if (AV(*S, 'A') < 1) { AV(*S, 'A') = fmin(AV(*S, 'A') + 0.025, 1); V('L') += 0.25; V('K') += 8 - 8 * AV(*S, 'A'); }   /* 8404 */
        V('N') = fmax(V('N') - 0.25, 0);                                         /* 8405 */
    }
    if (b > 100 && b < 360) G.cam.Z = (float)fmin(G.cam.Z + 0.2, 10);            /* 8397 */
    if (b == 280 || b == 320 || b == 330 || b == 340 || b == 350 || b == 360) snd_play_orig(13, 220, 128);   /* 8398-8403 */
    if (b == 400 && !FP.mus_boss) { FP.mus_boss = 1; boss_music(); }   /* 8408: m_boss */
    if (b == 440 && !FP.taunted) { FP.taunted = 1; snd_play_orig(193, 220, 128); setan(FP.p[P_RIDER], S_RIDER2); }   /* 8407 */
    if (b >= 484 && p->ground && p->state != ST_BOOST && !FP.ctl_back) p->gsp += 0.1;   /* 8414 */
    if (b == 484) { level_release_camera(0); G.cam_nF = 239; }                   /* 8415 / 8416 */
    if (b == 490 && !FP.ctl_back) {                                              /* 8409-8413: control back */
        FP.ctl_back = 1; G.input_lock = 0; FP.pose = 0;
        if (G.character == CHR_LILAC) { p->energy = 100; snd_play_orig(9, 220, 128); p->face = 1; if (p->ground) p->gsp = 12; else p->vx = 12; }   /* M 5 boost: the port gives its speed (RV B 12), not the boost state (INFERRED) */
        fx((float)p->x, (float)PY(p), "wburst"); snd_play_orig(126, 220, 128);
    }
    if (b == 520) { AV(*S, 'B') = 1; V('N') = 3; V('M') = 90; setan(FP.p[P_RIDER], S_RIDER); }   /* 8417 */
    if (AV(*S, 'B') == 1) { V('N') = fmin(V('N') + 0.2, 15); V('K') += V('N'); AV(*S, 'A') = fmax(AV(*S, 'A') - 0.05, 0); }   /* 8418 / 8419 */
    if (G.character == CHR_CAROL_BIKE && p->ground && fabs(p->gsp) < 0.05) player_bike_off(p, 1);   /* 8383 */
    if (G.character == CHR_CAROL_BIKE && b < 360) { if (p->ground) p->gsp *= 0.9; }   /* 8382 */
    if (b >= 300 && b < 490 && p->ground && fabs(p->gsp) < 0.05) FP.pose = 1;    /* 8384: M 50 standing pose */
}

/* ===================================================================================================== hooks */
int fn_panther_input(uint32_t *held) {                                           /* the scripted pad: floor auto-run (8516 / 8517), poses */
    if (!FP.on) return 0;
    if (FP.act == 6 && FP.g_floor) {
        uint32_t h = *held;
        int lr = (h & (BTN_LEFT | BTN_RIGHT)) != 0;
        if (G.act_in) FP.autod = 2;                                              /* the walk-in: CTRL.D held (RV T-G1: B + 0.2 a frame to the top speed) */
        else if (FP.autod == 2) FP.autod = 0;                                    /* 5047: control back, the virtual press released (RV: B falls by 0.57 a frame) */
        if (lr && !G.act_in) FP.autod = 0;                                       /* CTRL.C: a direction is held */
        if (FP.autod && !lr) { *held = h | BTN_RIGHT; return 1; }
        return 0;
    }
    if (FP.act == 5 && FP.pose) { *held = 0; return 1; }
    return 0;
}
void fn_panther_frame(player_t *p) {
    if (!FP.on) return;
    if (FP.act == 5) {
        chase(p);
        if (FP.chase_on) {
            double a = AV(FP.s2, 'A');
            rig(32 - 32 * a, 1 - a);                                             /* 8385-8392 (8387 pose blend) */
        }
        goto out;
    }
    /* ---- act 6 */
    if (FP.g_floor) {
        floor_mode(p);
        int M = pm(p), lr = (p->held & (BTN_LEFT | BTN_RIGHT)) != 0 && FP.autod == 0;
        if (!G.act_in && !FP.autod && !lr && p->ground && M != 5 && M != 9 && p->gsp > 10) p->gsp = p->gsp >= 5.05 ? p->gsp - 0.2 : 4.85;   /* 8515 (no press) */
        if (!G.act_in && !lr && p->ground && M != 5 && M != 9 && p->gsp < 10) FP.autod = 1;   /* 8516: CTRL.D = 1 (latched until a direction is pressed) */
        if (FP.autod && p->ground && M != 5 && M != 9 && (FP.autod == 2 || p->gsp < 10)) p->gsp = fmin(p->gsp + 0.2, PHYS_MAXSPD);   /* 8517 (RV T-G1: holds 10.08) */
    }
    if (FP.g_floor || FP.g_plane) floor_scroll(FP.g_plane);
    if (FP.g_plane) plane_mode(p);
    if (FP.g_fight) {
        if (p->x > G.lvl.w - 24) p->x = G.lvl.w - 24;                             /* 8552 */
        if (!is_milla() && p->ground && p->gsp > 10 && pm(p) != 5) { }
        rig(32, 1);                                                              /* 8554-8560 (mode 2 8611-8619: the same chain, U-G2) */
        body_moves(p);
        if (FP.g_m1) { cannon(p); rider(); }
        back_cannon();
        if (FP.g_m2) mode2();
        hits();
        speed_fx();
        defeat(p);
        assist(p);
    } else if (FP.g_post) {
        rig(32, 1);
    }
    runtime_pass(p);
    if (FP.g_post) post_boss(p);
    for (int k = 0; k < P_N; k++) FP.p[k]->inv = 0;                               /* the flash is drawn by fn_panther_draw */
out:;
#ifdef FP_HOST_ENV
    if (FP.dbg) printf("fp %d act %d K %.2f L %.2f M %.1f N %.2f I %.0f hA %.2f hC %d hD %.0f hF %.0f jaw %.0f fcD %.0f fcE %.0f fcF %.0f bcD %.0f rd %d m2 %d post %d hdA %.0f hdC %.0f hdD %.0f res %d chase %d s2B %.0f s2A %.3f as %d px %.2f py %.2f B %.3f\n",
                       G.frame, FP.act, V('K'), V('L'), V('M'), V('N'), V('I'), FP.hit ? FP.hit->A : 0., FP.hit ? FP.hit->c : 0, FP.hD, FP.hF, JW('A'),
                       FC('D'), FC('E'), FC('F'), BC('D'), FP.g_rider, FP.g_m2, FP.g_post, HD('A'), HD('C'), HD('D'), FP.res_on, FP.chase_on,
                       FP.ship2 ? FP.ship2->av[1] : FP.hd.a[1], AV(FP.s2, 'A'), FP.as_st, p->x, p->y - ORIG_REF_DY, p->ground ? p->gsp : p->vx);
    if (FP.dbg && getenv("FP_DBG_FP")[0] == '2') {                               /* every part: x y angle */
        printf("fpp %d", G.frame);
        for (int k = 0; k < P_N; k++) printf(" %d:%.0f,%.0f,%.2f", k, FP.p[k]->x, FP.p[k]->y, FP.p[k]->ang);
        printf(" r0:%.0f,%.0f r6:%.0f,%.0f t0:%.0f,%.0f\n", FP.rib[0]->x, FP.rib[0]->y, FP.rib[FP.nrib - 1]->x, FP.rib[FP.nrib - 1]->y, FP.tail[0]->x, FP.tail[0]->y);
    }
#endif
}

/* drawing: pass 0 / 1 / 2 = objects_draw's passes (after the generic objects), 3 = front (the white fade) */
static void draw_obj_add(const obj_t *o) {                                       /* the hit flash: an additive copy (INFERRED for direction 1) */
    if (!o || !o->an.b || o->hidden) return;
    r_set_additive(1);
    frame_draw_rot(o->an.b, anim_frame(&o->an), floorf(o->x - G.camx), floorf(o->y - G.camy), o->ang, o->flip, 0xff808080);
    r_set_additive(0);
}
void fn_panther_draw(int pass) {
    if (!FP.on) return;
    if (pass == 3) { if (FP.fade > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)FP.fade << 24) | 0xffffff); return; }
    if (pass == 0) {
        if (FP.sback && FP.sb_vis && FP.sback->an.b) {                   /* EffectSpeedBack_2049 (layer 1): scaled over the view, alpha from A */
            int al = (int)(FP.sback->av[0] * 255 / 80) / 2; if (al > 255) al = 255;
            frame_draw_scaled(FP.sback->an.b, anim_frame(&FP.sback->an), 0, 0, SCREEN_W, SCREEN_H, ((uint32_t)al << 24) | 0xffffff);
        }
        if (FP.wheel && FP.wheel_vis && FP.wheel->an.b) {                   /* BossPantherWheel (layer 1, 0.8): under the front foot */
            const obj_t *ff = FP.p[P_FOOTFR];
            r_set_scale(FP.wheel->scale);
            frame_draw_rot(FP.wheel->an.b, anim_frame(&FP.wheel->an), floorf(ff->x - G.camx), floorf(ff->y - G.camy), (float)(-G.frame * 20 % 360), 0, 0xffffffff);
            r_set_scale(1.f);
        }
        if (FP.hE > 1 && ((int)FP.hE & 1)) {                                     /* 8641: the parts' flash direction */
            static const int FL[] = { P_FOOTFL, P_FOOTFR, P_HEAD, P_JAW, P_SHF, P_FCAN, P_HIP, P_LEGFR, P_KNEE, P_LEGFL, P_SHF2 };
            for (unsigned k = 0; k < sizeof FL / sizeof *FL; k++) draw_obj_add(FP.p[FL[k]]);
        }
        if (FP.flash_t) { r_set_invert(1); for (int k = P_HEAD; k <= P_FCAN; k++) if (!FP.p[k]->hidden && FP.p[k]->an.b) frame_draw_rot(FP.p[k]->an.b, anim_frame(&FP.p[k]->an), floorf(FP.p[k]->x - G.camx), floorf(FP.p[k]->y - G.camy), FP.p[k]->ang, 0, 0xffffffff); r_set_invert(0); }   /* 8636 */
        return;
    }
    if (pass != 1) return;
    if (FP.laser_on && FP.laser && FP.laser->an.b) {                             /* BossPantherLaser_2047: x-scaled to the arena (13 segments) */
        const frame_t *f = anim_frame(&FP.laser->an);
        double a = FP.lang, dx = c_cos(a) * 50, dy = -c_sin(a) * 50;
        r_set_additive(1);
        for (int k = 0; k < 13; k++) frame_draw_rot(FP.laser->an.b, f, floorf((float)(FP.lx + k * dx) - G.camx), floorf((float)(FP.ly + k * dy) - G.camy), (float)a, 0, 0xffffffff);
        r_set_additive(0);
    }
    if (FP.glow && FP.glow_vis && FP.glow->an.b) {                          /* the charge glow (scaled, additive) */
        r_set_scale(FP.glow->scale); r_set_additive(1);
        frame_draw_rot(FP.glow->an.b, anim_frame(&FP.glow->an), floorf(FP.glow->x - G.camx), floorf(FP.glow->y - G.camy), 0, 0, 0xffffffff);
        r_set_additive(0); r_set_scale(1.f);
    }
}
int fn_panther_on(void) { return FP.on; }
