/* Jade Creek bosses, acts 1-4 (session 21): types 1430-1479 here; 1480-1535 are act 5's bosses and scenes in jc_boss5.c,
 * reached through these hooks (dispatch kept: types >= OT_JCB5_FIRST go to jc_boss5_*). Spec: build/re/spec_jc_bosses.md.
 * Labels: RE read from the code (handle_frame_40..42_events, test_collisions_41, constructors), RV measured in the original
 * (build/orig_traces/jc_bosses: a1_look / a1_met, a2_look / a2_kill / a2_look_m, a3_look / a3_run / a3_kill / a3_kill_c / a3_run_m,
 * a3_watch, a3_hits), INFERRED reasoned.
 * Encounters (RE; no Classic / Adventure test, no character test in any of them):
 *  - JC1: the Hovercop fly-by (group 0x229d8, 10347-10357): the police hovercraft (layer 0) rides behind the bridge arena and
 *    fires BossMeteor balls; a background scene, no damage.
 *  - JC2 / JC3: the Hovercop chase (0x22a0e always, 0x22a0d once the player passes it, 10484-10519 / 10656-10697 + the shared
 *    ice 10540-10562): ice crystals from charge glows, an ice missile on its hook chain, ice spikes (EffectStickyIce) that hurt 20.
 *    Its turbines are in no qualifier: it cannot be damaged in these acts (RE qualifier dump). JC2: it leaves after 3 missile
 *    cycles (O > 2, flag 6). JC3: Neera rides it (0x22a5e) and it parks at A 8084 (RV).
 *  - JC3: Neera (NPCNeera_2483 + BossNeeraHitbox_2482, Q20, A 120, D 3): trigger 10589 (Player on a CameraTriggerBoss), the ice rain
 *    intro (0x22a59), the fight (0x22a5b 10725-10773 + the hit rules 0x22a5a 10774-10784, ice rings 10789-10792), the end
 *    (10772 / 10773 -> jc_flow_act3_cleared), she leaves on the Hovercop (0x22a5c 10785-10788), then the subtanker scene
 *    (0x22a5d 10799-10805: the moving json layers 4 / 5 that carry the player to the act's end; see sub_frame).
 *  - JC4: nothing (RE: no boss event in handle_frame_43; its CameraTriggerBoss is an off-map template).
 * Performance (SH4): per-type instance lists built once at spawn, the runtime objects (glows, crystals, missiles, spikes, meteors,
 * rings, shields, bubbles) in one index list; a level without these types returns at the first test. */
#include "jc.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void jc_flow_act3_cleared(void) __attribute__((weak));   /* jc_flow.c (master): 10772 loop_next_act_41 + 10773 */
extern int fe_difficulty;                                /* global 12: 0 Hard (frontend.c) */

/* ======================================================================== sprites (tools/import_frame.py JC_BOSS rows) */
enum { S_HC, S_HC_L, S_TB1, S_TB1_L, S_TB2, S_TB2_L, S_PROP, S_SIR1, S_SIR2, S_HOOK2, S_HOOK, S_HOOK_L, S_LIGHT, S_LIGHT_L,
       S_GLOW_L, S_CRYSTAL, S_MIS, S_MIS_L, S_STICKY, S_METEOR, S_ICED, S_ICED7, S_BAR, S_BARP, S_RING, S_NHIT,
       S_N0R, S_N0R_L, S_N7, S_N7_L, S_N7R, S_N7R_L, S_N8R, S_N8R_L, S_N10, S_N10_L, S_N10R, S_N10R_L,
       S_N12, S_N12_L, S_N13, S_N13_L, S_N16, S_N17, S_N17_L, S_N17R, S_N17R_L,
       S_SHW, S_SHW_IN, S_SHE, S_SHE_IN, S_SHWA, S_SHWA_IN, S_SHFF, S_SHFF_IN, S_SHFB, S_SHM, S_SHM_IN, S_N };
static const char *const SN[S_N] = { "jcb_hc", "jcb_hc_l", "jcb_tb1", "jcb_tb1_l", "jcb_tb2", "jcb_tb2_l", "jcb_prop", "jcb_sir1",
    "jcb_sir2", "jcb_hook2", "jcb_hook", "jcb_hook_l", "jcb_light", "jcb_light_l", "jcb_glow_l", "jcb_crystal", "jcb_mis",
    "jcb_mis_l", "jcb_sticky", "jcb_meteor", "jcb_iced", "jcb_iced7", "jcb_bar", "jcb_barp", "jcb_ring", "jcb_nhit",
    "jcb_n0_1", "jcb_n0_17", "jcb_n7_0", "jcb_n7_16", "jcb_n7_1", "jcb_n7_17", "jcb_n8_1", "jcb_n8_17",
    "jcb_n10_0", "jcb_n10_16", "jcb_n10_1", "jcb_n10_17", "jcb_n12_0", "jcb_n12_16", "jcb_n13_0", "jcb_n13_16", "jcb_n16_0",
    "jcb_n17_0", "jcb_n17_16", "jcb_n17_1", "jcb_n17_17",
    "shw", "shw_in", "she", "she_in", "shwa", "shwa_in", "shff", "shff_in", "shfb", "shm", "shm_in" };   /* shields: the act's rows */
static short SA[S_N];
static void set_a(obj_t *o, int k) { if (o && k >= 0 && SA[k] >= 0 && (o->an.b != &G.ens || o->an.anim != SA[k])) anim_set(&o->an, &G.ens, SA[k]); }

/* ======================================================================== per-type lists (placement order) */
#define NT (OT_JCB_END - OT_JCB_HC + 1)
#define LMAX 4
static struct { short n, i[LMAX]; } L[NT];
static int on;                                       /* the level places a Hovercop or Neera */
static short trig[4], ntrig, marks[4];               /* CameraTriggerBoss, CameraLimitBoss L / R / T / B */
static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static obj_t *get(int type, int k) { return k < L[type - OT_JCB_HC].n ? slot(L[type - OT_JCB_HC].i[k]) : NULL; }
static obj_t *one(int type) { return get(type, 0); }
static float marker(int t, int x) { obj_t *o = slot(marks[t - OT_CAMLIM_L]); return o ? (x ? o->x : o->y) : -1e9f; }

/* runtime objects: one index list (the creators add, each frame drops the dead) */
#define RMAX 128
static short R[RMAX]; static int nR;
static obj_t *spawn(int type, int sk, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = x; o->y = o->oy = o->fy = y; o->layer = layer; o->born = G.frame;
    set_a(o, sk);
    short i = (short)(o - G.o); int k;
    for (k = 0; k < nR && R[k] != i; k++) ;
    if (k == nR) { if (nR < RMAX) R[nR++] = i; else o->alive = 0; }
    return o;
}
static void r_compact(void) {
    int n = 0;
    for (int k = 0; k < nR; k++) { const obj_t *o = &G.o[R[k]]; if (o->alive && o->type >= OT_JCB_HC && o->type <= OT_JCB_END) R[n++] = R[k]; }
    nR = n;
}
static obj_t *r_obj(int k) { return &G.o[R[k]]; }
#define FOR_R(v, T) for (int k_ = 0; k_ < nR; k_++) for (obj_t *v = r_obj(k_); v && v->alive && v->type == (T); v = NULL)

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double dsin(double deg) { double r = fmod(deg, 360.); if (r < 0) r += 360.; return (r == 0. || r == 180.) ? 0. : fd_sin(r / 57.29577951); }
static double dcos(double deg) { double r = fmod(deg, 360.); if (r < 0) r += 360.; return (r == 90. || r == 270.) ? 0. : fd_cos(r / 57.29577951); }
static int act_point(const obj_t *o, float *ax, float *ay) {   /* the current frame's action point (Active vtable +0x18 / +0x20) */
    if (!o || !o->an.b || !o->an.b->act) { if (o) { *ax = o->x; *ay = o->y; } return 0; }
    const bank_t *b = o->an.b; const frame_t *f = anim_frame(&o->an); int fi = (int)(f - b->frames);
    float sc = o->scale != 0 ? o->scale : 1.f;   /* a scaled Active's action point scales about the hot spot */
    int dx = b->act[2 * fi] - f->hx;
    if (o->flip) dx = -dx;
    *ax = floorf(o->x) + (float)(int)(dx * sc); *ay = floorf(o->y) + (float)(int)((b->act[2 * fi + 1] - f->hy) * sc);
    return 1;
}
static int box_bottom(const obj_t *o) { if (!o || !o->an.b) return o ? (int)o->y : 0; const frame_t *f = anim_frame(&o->an); return (int)floorf(o->y) - f->hy + f->h; }
static int vulnerable(const player_t *p) { return p->state != ST_DEAD && p->hp > 0 && !p->hurt_pending && !player_invincible(p); }
static int p326_hit(const obj_t *o) {                               /* Player_326 (the main sensor rows) on o */
    int ox, oy, h; const signed char *x0, *x1;
    return o && o->alive && o->an.b && mv_sensor_rows(SENS_MAIN, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(o, ox, oy, x0, x1, h);
}
static int p326_rect(const player_t *p, float x0, float y0, float x1, float y1) {   /* the 27-px disc at (int X, int Y) against a box */
    double px = PX(p), py = PY(p), cx = fmax(x0, fmin(px, x1 - 1)), cy = fmax(y0, fmin(py, y1 - 1));
    return (cx - px) * (cx - px) + (cy - py) * (cy - py) <= 13.5 * 13.5;
}
static void ice_hurt(player_t *p, int dmg);

/* ======================================================================== state */
enum { M_NONE, M_FLYBY, M_CHASE2, M_CHASE3 };
static struct {
    int mode;                                        /* act behaviour from the placed objects (RE: the act's own event blocks) */
    int g_flyby, g_flyby_once, g_snd;                /* 0x229d8, 0x22a00 (10347 once), 0x229f5 (10353 sound once) */
    int g_chase, g_trig_once, g_snd2;                /* 0x22a0d, 0x22a28 / 0x22a71 (10494 / 10666 once), 0x22a4c / 0x22a78 (sound once) */
    int g_ride, g_intro, g_hits, g_fight, g_leave, g_post, g_snd298, g_snd297, g_restart, g_leave_once;
                                                     /* 0x22a5e, 0x22a59, 0x22a5a, 0x22a5b, 0x22a5c, 0x22a5d, 0x22a70, 0x22a7a, 0x22a79, 0x22a68 */
    int gone;                                        /* the Hovercop and its rig destroyed (10357) */
    double A, B, C, D, E, F, H, I, J, L, O; int fl6;  /* BossHovercop alterables (ctor D 1, H 4, J -56) and flag 6 */
    double tbD[2], propang[2];                       /* turbines' D (siren clock), propellor angles */
    int mly;                                         /* MasterLevel Y (10589 sets 1, 10773 clears) */
    int ended, cleared;                              /* the fight is over (10773); jc_flow_act3_cleared called */
    int front;                                       /* 10785: the rig moved in front (draw order) */
    double sA, sB; int sdx, sdy;                     /* the subtanker scene: LevelParalaxWaterSurface A (0 to -600), B (0 to 600); the layer offset */
} H;
/* NPCNeera_2483 alterables (RV a3_run: D 1, K 1543) and flags 2 (ring thrown), 3 (shielded), 4 (ice rain next), 5 (shield done) */
static struct { double D, E, F, G, H, I, J, K, L, M, N; int fl, anim, dir, inv, born_shield; } N;
#define NF2 0x04
#define NF3 0x08
#define NF4 0x10
#define NF5 0x20
int jc_boss_ml_y(void) { return H.mly; }
float jc_boss_hud_dy(void) { return 0.f; }           /* RE: no loop_gui_shiftup / shiftdown in the boss events of JC1-JC4 */

/* runtime object state kept in av[]: crystal A B C D E (x, y, vx, vy, spikes), missile A C E (x, vx, ring angle), sticky A (age),
 * B (y scale), glow A (direction), ring A (bounces), shield A (kind); flags in fl (bit 1 started / released, bit 2 no burst,
 * bit 6 done) */
#define FL1 0x02
#define FL2 0x04
#define FL6 0x40
#define F_ARMOURED 0x20
#define F_DEFEATED 0x40

/* ======================================================================== Hovercop rig (10484-10493 / 10656-10665 / 10349-10352)
 * JC2 / JC3 (RE 10485-10490 / 10657-10662): turbines at (-52, -31) / (+28, -31), sirens at turbine1 (-22 / -8, -18) and turbine2
 * (+22 / +8, -18), all mirrored for D -1. JC1 (RE 10349, the rig at x scale 0.5, RV a1_met): (-26, -16) / (+14, -16), sirens
 * (-11 / -4, -9) and (+11 / +4, -9). Propellors at the turbines' action points, angle + 10 (float constant) mod 360. */
static void rig_frame(int small) {
    obj_t *hc = one(OT_JCB_HC); if (!hc) return;
    int right = H.D >= 0;
    obj_t *tb[2] = { one(OT_JCB_TB1), one(OT_JCB_TB2) };
    static const int TX[2][2] = { { -52, 28 }, { -26, 14 } }, TY[2] = { -31, -16 };
    static const int SX[2][4] = { { -8, -22, 22, 8 }, { -4, -11, 11, 4 } }, SY[2] = { -18, -9 };   /* siren1..4 */
    static const int ST[4] = { OT_JCB_SIR1, OT_JCB_SIR2, OT_JCB_SIR3, OT_JCB_SIR4 };
    for (int k = 0; k < 2; k++) if (tb[k]) {
        tb[k]->x = hc->x + (right ? TX[small][k] : -TX[small][k]); tb[k]->y = hc->y + TY[small];
        set_a(tb[k], k == 0 ? (right ? S_TB1 : S_TB1_L) : (right ? S_TB2 : S_TB2_L));
    }
    for (int k = 0; k < 4; k++) { obj_t *sr = one(ST[k]); obj_t *t = tb[k < 2 ? 0 : 1]; if (sr && t) { sr->x = t->x + (right ? SX[small][k] : -SX[small][k]); sr->y = t->y + SY[small]; } }
    for (int k = 0; k < 2; k++) {
        obj_t *pr = one(k ? OT_JCB_PROP2 : OT_JCB_PROP1);
        if (pr && tb[k]) { float ax, ay; act_point(tb[k], &ax, &ay); pr->x = ax; pr->y = ay; H.propang[k] = fmod(H.propang[k] + 10., 360.); pr->ang = (float)H.propang[k]; }
    }
    set_a(hc, right ? S_HC : S_HC_L);
    obj_t *li = one(OT_JCB_LIGHT);   /* 10661 / 10662: the light at (+-54, +32), direction 0 / 16 */
    if (li) { li->x = hc->x + (right ? 54 : -54); li->y = hc->y + 32; set_a(li, right ? S_LIGHT : S_LIGHT_L); }
    /* 10491-10493 / 10663-10665 / 10350-10352: the siren clock: turbine D = (D + 5) mod 60; sirens 1 / 4 shown, 2 / 3 hidden,
     * swapped per turbine while its D >= 30 (6 frames each) */
    for (int k = 0; k < 2; k++) H.tbD[k] = fmod(H.tbD[k] + 5., 60.);
    obj_t *s1 = one(OT_JCB_SIR1), *s2 = one(OT_JCB_SIR2), *s3 = one(OT_JCB_SIR3), *s4 = one(OT_JCB_SIR4);
    if (s2) s2->hidden = 1;
    if (s3) s3->hidden = 1;
    if (s1) s1->hidden = 0;
    if (s4) s4->hidden = 0;
    if (H.tbD[0] >= 30) { if (s1) s1->hidden = 1; if (s2) s2->hidden = 0; }
    if (H.tbD[1] >= 30) { if (s4) s4->hidden = 1; if (s3) s3->hidden = 0; }
    /* hook2 (the missile chain): x = hc.x, y = hc box bottom + J (10484 / 10656; RV a2_look f0: 1547 = 1539 + 64 - 56) */
    obj_t *h2 = one(OT_JCB_HOOK2);
    if (h2) { h2->x = hc->x; h2->y = (float)(int)(box_bottom(hc) + H.J); }
}
static void rig_destroy(void) {
    static const int T[] = { OT_JCB_HC, OT_JCB_TB1, OT_JCB_TB2, OT_JCB_PROP1, OT_JCB_PROP2, OT_JCB_SIR1, OT_JCB_SIR2, OT_JCB_SIR3, OT_JCB_SIR4, OT_JCB_LIGHT, OT_JCB_HOOK2 };
    for (unsigned k = 0; k < sizeof T / sizeof T[0]; k++) { obj_t *o = one(T[k]); if (o) o->alive = 0; }
    H.gone = 1;
}

/* ======================================================================== JC1: the fly-by (10347-10357) */
static void flyby_frame(player_t *p) {
    obj_t *hc = one(OT_JCB_HC), *tr = ntrig ? slot(trig[0]) : NULL;
    if (!hc || H.gone) return;
    /* 10347: X > trigger.x and MV.A != 5 (every port character), once */
    if (!H.g_flyby_once && tr && PX(p) > tr->x) { H.g_flyby_once = 1; H.g_flyby = 1; }
    if (!H.g_flyby) return;
    /* 10348: x = 0.5 trigger.x + 0.5 VIEWX - 100 + A, A += 4; y = the water surface box's middle - 32 (RE). The surface
     * (LevelParalaxWaterSurface_14, the flow's band) is approximated by a fit of the RV a1_look y (max error 2.6 px, INFERRED) */
    hc->x = (float)(int)(0.5 * tr->x + 0.5 * (int)G.camx - 100 + H.A); H.A += 4;
    double mlc = water_on() ? water_c() : 1200.;
    hc->y = (float)(int)floor(0.340815 * (int)G.camy - 0.228773 * mlc + 1087.33);
    rig_frame(1);
    /* 10353: once, the Hovercop at x >= VIEWX - 29 (the selection drops x < VIEWX - 29): sound 276 (RV a1_look f166: the
     * trigger frame) */
    if (!H.g_snd && hc->x >= (int)G.camx - 29) { H.g_snd = 1; snd_play_orig(276, 220, 128); }
    /* 10354: x > VIEWX + 80 and C = 0: C 1 */
    if (hc->x > (int)G.camx + 80 && H.C == 0) H.C += 1;
    /* 10355: every 10 frames (g3 % 10) while 0 < C < 6: a BossMeteor at the action point, an explosion (scale 2, angle 90 k),
     * sound 99, C + 1 (RV: 99 at 236, 246, ... 276) */
    if (G.frame % 10 == 0 && H.C > 0 && H.C < 6) {
        float ax, ay; act_point(hc, &ax, &ay);
        spawn(OT_JCB_METEOR, S_METEOR, ax, ay, hc->layer);
        snd_play_orig(99, 220, 128);
        H.C += 1;
        obj_effect(hc->x, hc->y, "explosion");
    }
    /* meteors: RV a1_met: +2.5 / -5 px a frame (BossMeteor_2435's own movement, not in the constructor: INFERRED values) */
    FOR_R(m, OT_JCB_METEOR) {
        m->fx += 2.5f; m->fy -= 5.f; m->x = (float)(int)m->fx; m->y = (float)(int)m->fy;
        if (m->y < (int)G.camy - 40) m->alive = 0;                     /* 10356 */
    }
    /* 10357: x > VIEWX + 466: the rig destroyed */
    if (hc->x > (int)G.camx + 466) rig_destroy();
}

/* ======================================================================== JC2 / JC3: the chase */
static int turbines_alive(void) { return one(OT_JCB_TB1) != NULL || one(OT_JCB_TB2) != NULL; }   /* never destroyed in JC2 / JC3 (RE) */
static void chase_move(player_t *p) {
    obj_t *hc = one(OT_JCB_HC); if (!hc || H.gone) return;
    /* 10484 / 10656: y = B + 5 sin C, C = (C + 2) mod 360, x = A, A += E */
    hc->y = (float)(int)(5 * dsin(H.C) + H.B);
    H.C = fmod(H.C + 2, 360.);
    hc->x = (float)(int)H.A;
    H.A += H.E;
    rig_frame(0);
    /* 10494 / 10666: X > hc.x and Y >= hc.y - 99 (JC3: hc.x + 300 < X), once: the chase group, E = max(9, MV.B + 1);
     * JC3 also shows Neera and B = L (10666) */
    double pb = p->ground ? p->gsp : p->vx;
    if (!H.g_trig_once && (H.mode != M_CHASE3 || G.at_g4 < 10) && PX(p) > hc->x + (H.mode == M_CHASE3 ? 300 : 0) && hc->y - 99 <= PY(p)) {
        H.g_trig_once = 1; H.g_chase = 1;
        H.E = fmax(9, pb + 1);
        if (H.mode == M_CHASE3) { obj_t *n = one(OT_JCB_NEERA); if (n) n->hidden = 0; H.B = H.L; }
    }
}
static int mlb(void) { int w = level_cur()->water; return w > 0 ? w : 1600; }   /* MasterLevel B: the water line (RE 10566 / 10822: 1600) */
static int g4_ok(void) { return H.mode != M_CHASE3 || G.at_g4 < 10; }            /* JC3 10669 / 10670 / 10673: g4 < 10 */
/* the Hovercop's attack cycle (RE 10496-10516 JC2, 10668-10686 JC3; RV a2_look: glows / crystals 4 x 15 frames, missile 165 at
 * H -100, 166 when the hook is down (J 0), 195 at I 30, cycle 255 frames) */
static void chase_attack(player_t *p) {
    obj_t *hc = one(OT_JCB_HC);
    if (!hc || H.gone || !H.g_chase) return;
    double X = PX(p), Y = PY(p);
    int alive = turbines_alive(), jc3 = H.mode == M_CHASE3;
    /* 10496 / 10668: once, hc.x in [X - 499, X): sound 277 (RV a2_look f233) */
    if (!H.g_snd2 && hc->x >= X - 499 && hc->x < X) { H.g_snd2 = 1; snd_play_orig(277, 220, 128); }
    /* 10497 / 10498 / 10669 / 10670: every 15 frames (g3 % 15), not flag 6 (JC2) / turbines up and g4 < 10 (JC3), the
     * Hovercop behind the player within 399 px facing it, hc.y <= Y + 99, H > 0: H - 1 and a charge glow at its action point */
    if (G.frame % 15 == 0 && (jc3 ? alive && g4_ok() : !H.fl6) && hc->y <= Y + 99 && H.H > 0 &&
        ((H.D == 1 && X - 399 <= hc->x && hc->x < X) || (H.D == -1 && X < hc->x && hc->x <= X + 399))) {
        H.H -= 1;
        float ax, ay; act_point(hc, &ax, &ay);
        obj_t *g = spawn(OT_JCB_GLOW, S_GLOW_L, ax, ay, hc->layer);
        if (g) { g->av[0] = H.D; g->flip = H.D > 0; }
    }
    /* 10499 / 10671: glows follow the action point, A = D, direction = the Hovercop's; 10500 / 10672: frame > 3, flag 1 clear:
     * a BossIceCrystal there, C = (3 + R(30) / 10) A, D = -5; glows end with their animation (INFERRED). Direction 0 is drawn
     * as direction 16 mirrored (its table misses an image) */
    FOR_R(g, OT_JCB_GLOW) {
        if (g->an.done) { g->alive = 0; continue; }
        float ax, ay; act_point(hc, &ax, &ay); g->x = ax; g->y = ay; g->av[0] = H.D; g->flip = H.D > 0;
        if (g->an.frame > 3 && !(g->fl & FL1)) {
            obj_t *c = spawn(OT_JCB_CRYSTAL, S_CRYSTAL, g->x, g->y, g->layer);
            double r = cross_rand(30) * 0.1 + 3;
            if (c) { c->av[2] = r * g->av[0]; c->av[3] = -5; }
            g->fl |= FL1;
        }
    }
    /* 10501 (JC2) / 10673 (JC3: ML.Y 0, g4 < 10): H <= 0 counts down */
    if (H.H <= 0 && (!jc3 || (H.mly == 0 && g4_ok()))) H.H -= 1;
    /* 10502 / 10674: H -100 and I 0: I 1, a BossIceMissile at the chain's action point, sound 165, H - 1 */
    obj_t *h2 = one(OT_JCB_HOOK2);
    if (H.H == -100 && H.I == 0) {
        H.I = 1;
        float ax, ay; if (h2) act_point(h2, &ax, &ay); else { ax = hc->x; ay = hc->y; }
        spawn(OT_JCB_MISSILE, S_MIS, ax, ay, hc->layer);
        snd_play_orig(165, 220, 128);
        H.H -= 1;
    }
    /* 10503 / 10675: I 1: the chain goes down (J + 2) with the missile at its action point; 10504 / 10676: J >= 0: I 2, sound 166
     * (165 stopped: the port has no per-sample stop, INFERRED inaudible); 10505 / 10677: 1 < I < 30: I + 1, held, C = 2 D;
     * 10506 / 10678: I 30: I 31, released (flag 1, A = x), sound 195; 10507-10509 / 10679-10681: I to 60, chain up (J - 2) to
     * -56, then I 0 (JC2: O + 1); 10510: O > 2: flag 6 (JC2: it leaves); 10511 / 10682: H < -200: H 4 */
    if (H.I == 1) H.J += 2;
    int hold = H.I == 1 || (H.I > 1 && H.I < 30);
    if (H.I == 1 && H.J >= 0) { H.I = 2; snd_play_orig(166, 220, 128); }
    else if (H.I > 1 && H.I < 30) H.I += 1;
    FOR_R(m, OT_JCB_MISSILE) {
        if (m->fl & FL1) continue;
        if (hold && h2) { float ax, ay; act_point(h2, &ax, &ay); m->x = ax; m->y = ay; }
        if (H.I > 1 && H.I <= 30) m->av[1] = 2 * H.D;
        if (H.I == 30) { m->fl |= FL1; m->av[0] = m->x; }
    }
    if (H.I == 30) { H.I = 31; snd_play_orig(195, 220, 128); }
    else if (H.I > 30 && H.I < 60) H.I += 1;
    if (H.I >= 60) H.J -= 2;
    if (H.I >= 60 && H.J <= -56) { H.I = 0; if (!jc3) H.O += 1; }
    if (!jc3 && H.O > 2 && !H.fl6) H.fl6 = 1;
    if (H.H < -200) H.H = 4;
    /* 10512 / 10513 / 10683 / 10684: turn to the player (beyond 10 px, not overlapping it, not flag 6 / turbines up) */
    int can = jc3 ? alive : !H.fl6;
    if (can && X + 10 < hc->x && H.D != -1 && !p326_hit(hc)) H.D = -1;
    if (can && hc->x < X - 10 && H.D != 1 && !p326_hit(hc)) H.D = 1;
    /* 10514 / 10515 / 10685 / 10686: keep within 200 px ahead: E + 0.2 (< 5) while hc.x <= X + 199, E - 0.2 (> -5) beyond X + 200 */
    if (can && hc->x <= X + 199 && H.E < 5) H.E += 0.2;
    if (can && X + 200 < hc->x && H.E > -5) H.E -= 0.2;
    /* 10516 (JC2): flag 6: D 1, E + 0.2 (it flies off to the right; RV a2_look f1000+) */
    if (!jc3 && H.fl6) { H.D = 1; H.E += 0.2; }
}
/* 10526 / 10696: A capped at MinibossRailing.x - 100 while the turbines are up. No railing in JC2 / JC3: Chowdren's default
 * instance (RV: JC3 8084 in every trace, JC2 never capped); kept as the JC3 value (INFERRED data). 10716: Neera (riding, E 1)
 * jumps off when the cap holds it (RV a3_run f424: E 2) */
static void chase_cap(void) {
    if (H.mode != M_CHASE3 || H.gone || H.g_leave || !turbines_alive() || H.A <= 8084) return;
    H.A = 8084; H.E = 0;
    if (H.g_ride && N.E == 1) { N.E = 2; N.F = 0; }
}

/* the shared ice (RE 10540-10562, JC3 copies 10793-10796): missiles, crystals, spikes (EffectStickyIce) */
static void ice_frame(player_t *p) {
    obj_t *hc = one(OT_JCB_HC);
    double X = PX(p);
    for (int k = 0; k < nR; k++) {
        obj_t *o = &G.o[R[k]];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_JCB_MISSILE: {
            if ((o->fl & FL1) && !(o->fl & FL6)) { o->x = (float)(int)o->av[0]; o->av[0] += o->av[1]; }           /* 10540 */
            if ((o->fl & FL1) && !(o->fl & FL6) && G.frame % 2 == 0 && o->av[1] != 0) {                             /* 10541 / 10542 */
                obj_t *b = spawn(OT_JCB_BUBBLE, -1, o->x + (o->av[1] > 0 ? -16 : 16), o->y, o->layer);
                if (b) { int a = ens_anim("mbubble"); if (a >= 0) anim_set(&b->an, &G.ens, a); else b->alive = 0; b->fx = (float)(o->av[1] * 0.25); }
            }
            if (o->av[1] > 0) { o->av[1] = fmin(o->av[1] + 0.1, 8); set_a(o, S_MIS); }                             /* 10543 */
            else if (o->av[1] < 0) { o->av[1] = fmax(o->av[1] - 0.1, -8); set_a(o, S_MIS_L); }                      /* 10544 */
            /* 10545 / 10546 (10793 / 10794): released, passing the player's x: it bursts (flag 6, hidden): a spike, a white burst,
             * CAM.Z >= 5, sounds 77 and 98 */
            if ((o->fl & FL1) && !(o->fl & FL6) && ((o->av[1] > 0 && X < o->x) || (o->av[1] <= 0 && o->x < X))) {
                o->fl |= FL6; o->hidden = 1;
                spawn(OT_JCB_STICKY, S_STICKY, o->x, o->y, o->layer);
                obj_effect(o->x, o->y, "wburst");
                if (G.cam.Z < 5) G.cam.Z = 5;
                snd_play_orig(77, 220, 128); snd_play_orig(98, 220, 128);
            }
            /* 10547 / 10548: burst: two spikes a frame on a 40 px ring at E -/+ 90 (random angle, no white burst), E + 20;
             * 10550: E >= 180: destroyed (9 frames, 18 spikes) */
            if (o->fl & FL6) {
                for (int s = -1; s <= 1; s += 2) {
                    double a = o->av[2] + 90 * s;
                    obj_t *sp = spawn(OT_JCB_STICKY, S_STICKY, (float)(int)(dcos(a) * 40 + (int)o->x), (float)(int)(dsin(a) * 40 + (int)o->y), o->layer);
                    if (sp) { sp->ang = (float)(cross_rand(61) - 30); sp->fl |= FL2; }
                }
                o->av[2] += 20;
                if (o->av[2] >= 180) { o->alive = 0; break; }
            }
            if (fabsf(o->x - G.camx) > 1200) o->alive = 0;    /* safety: a missile that never passes the player (INFERRED) */
            break; }
        case OT_JCB_CRYSTAL: {
            if (!(o->fl & FL1)) { o->fl |= FL1; o->av[0] = (int)o->x; o->av[1] = (int)o->y; }                        /* 10552 */
            if (!(o->fl & FL6)) {                                                                                      /* 10553 */
                o->x = (float)(int)o->av[0]; o->y = (float)(int)o->av[1];
                o->av[0] += o->av[2]; o->av[1] += o->av[3]; o->av[3] = fmin(o->av[3] + 0.3, 12);
                /* 10555 (JC2: B > hc.B + 64) / 10795 (JC3: B > ML.B) / 10591 (JC3: on the ground, B > ML.B - 64): it lands: flag 6,
                 * hidden, a spike (random angle), E + 1, sound 77, a splash */
                int land = H.mode == M_CHASE3 ? (o->av[1] > mlb() || (o->av[1] > mlb() - 64 && obj_overlaps_bg(o))) : (hc && o->av[1] > H.B + 64);
                if (land) {
                    o->fl |= FL6; o->hidden = 1;
                    obj_t *sp = spawn(OT_JCB_STICKY, S_STICKY, o->x, o->y, o->layer);
                    if (sp) sp->ang = (float)(cross_rand(61) - 30);
                    o->av[4] += 1;
                    snd_play_orig(77, 220, 128);
                    obj_effect(o->x, o->y, "splash");
                }
                if (o->y > G.lvl.h + 64) o->alive = 0;
            } else {
                /* 10556: every 3 frames a spike E * 5 px left / right of the landing point (alternating), E + 1; 10557: E > 12: gone */
                if (G.frame % 3 == 0) {
                    o->av[4] += 1;
                    double e = o->av[4], side = fmod(e, 2) * 2 - 1;
                    obj_t *sp = spawn(OT_JCB_STICKY, S_STICKY, (float)(int)(side * (0.5 * e * 10) + (int)o->x), o->y, o->layer);
                    if (sp) sp->ang = (float)(cross_rand(61) - 30);
                }
                if (o->av[4] > 12) o->alive = 0;
            }
            break; }
        case OT_JCB_STICKY:
            if (!(o->fl & FL2)) { o->fl |= FL2; obj_effect(o->x, o->y, "wburst"); }                                  /* 10549 */
            o->av[1] = o->av[0] <= 100 ? o->av[0] * 0.01 : 1;                                                         /* 10558: y scale */
            o->av[0] += 10;
            if (o->av[0] > 400) o->alive = 0;                                                                          /* 10559 */
            break;
        case OT_JCB_BUBBLE:
            o->y -= 1; o->x += o->fx; o->fx *= 0.9f;
            if (o->an.done || G.frame - o->born > 40) o->alive = 0;
            break;
        }
    }
    /* 10561 / 10796: Player_326 on a grown spike (A > 100: the selection keeps A > 100), OTHERS.E <= 0: 20 damage (MV.R 1,
     * LIFE.B + 20), hc F 5, OTHERS.R 30 with the blend 0xffc0a0 (BGR: an ice-blue tint) */
    int hurt = !vulnerable(p);
    FOR_R(o, OT_JCB_STICKY) {
        if (hurt || o->av[0] <= 100) continue;   /* A after this frame's + 10 (10558 runs first) */
        if (p326_hit(o)) { ice_hurt(p, 20); hurt = 1; }
    }
    if (H.F > 0) H.F -= 1;                                                                                             /* 10562 */
}
static void ice_hurt(player_t *p, int dmg) { player_contact(p, dmg); H.F = 5; p->tint = 30; p->tint_argb = 0xffa0c0ff; }

/* ======================================================================== JC3: Neera */
/* her rows: anim 0 / 8 have directions 1 / 17 only, 12 / 13 / 16 directions 0 / 16; a direction the animation lacks shows the
 * closest one (Chowdren's direction fallback) */
static int neera_row(int a, int dir) {
    static const signed char T[18][4] = {   /* directions 0, 1, 16, 17 */
        [0] = { -1, S_N0R, -1, S_N0R_L }, [7] = { S_N7, S_N7R, S_N7_L, S_N7R_L }, [8] = { -1, S_N8R, -1, S_N8R_L },
        [10] = { S_N10, S_N10R, S_N10_L, S_N10R_L }, [12] = { S_N12, -1, S_N12_L, -1 }, [13] = { S_N13, -1, S_N13_L, -1 },
        [16] = { S_N16, -1, S_N16, -1 }, [17] = { S_N17, S_N17R, S_N17_L, S_N17R_L } };
    static const int DV[4] = { 0, 1, 16, 17 };
    if (a < 0 || a > 17) return -1;
    int best = -1, bd = 99;
    for (int k = 0; k < 4; k++) {
        int s = T[a][k]; if (s <= 0) continue;   /* -1 / 0: no such row (no Neera row is sprite 0) */
        int d = abs(((dir - DV[k]) % 32 + 32) % 32); if (d > 16) d = 32 - d;
        if (d < bd) { bd = d; best = s; }
    }
    return best;
}
static void n_anim(obj_t *n, int a) {                /* force_animation: a new animation restarts, a new direction keeps the frame */
    int r = neera_row(a, N.dir);
    if (!n || r < 0 || SA[r] < 0) return;
    if (n->an.b == &G.ens && n->an.anim == SA[r]) { N.anim = a; return; }
    int same = a == N.anim, fr = n->an.frame, acc = n->an.acc, done = n->an.done;
    anim_set(&n->an, &G.ens, SA[r]);
    if (same) { int c = G.ens.anims[SA[r]].count; n->an.frame = fr < c ? fr : c - 1; n->an.acc = acc; n->an.done = done; }
    N.anim = a;
}
static void n_dir(obj_t *n, int d) { if (N.dir != d) { N.dir = d; n_anim(n, N.anim); } }
static void n_face(obj_t *n, const player_t *p) {    /* 10711 / 10712, 10729-10732, 10762 / 10763: 17 (D -1) toward a player left of her */
    if (PX(p) <= (int)n->x) { N.D = -1; n_dir(n, 17); } else { N.D = 1; n_dir(n, 1); }
}
static obj_t *barrier_near(const obj_t *n, int lo, int hi) {   /* an ice barrier with lo < x - n.x <= hi (10751-10754, 10766 / 10767) */
    FOR_R(b, OT_JCB_ICEBAR) { int d = (int)b->x - (int)n->x; if (lo < d && d <= hi) return b; }
    for (int k = 0; k < L[OT_JCB_ICEBAR - OT_JCB_HC].n; k++) { obj_t *b = get(OT_JCB_ICEBAR, k); if (b) { int d = (int)b->x - (int)n->x; if (lo < d && d <= hi) return b; } }
    return NULL;
}
static void shields_follow(const obj_t *n) { FOR_R(s, OT_JCB_NSHIELD) { s->x = n->x; s->y = n->y; } }   /* 10747 */

/* 10589: Player_326 on a CameraTriggerBoss (16 x 192, hot spot 0, 0), ML.Y 0, g4 < 10: CAM.M 1, an ice barrier at each
 * CameraLimitBossLeft (x + 24) on the player's layer with a white burst, ML.Y 1, Neera E 0 F 0, the ride off, the intro on.
 * ControllerMusic then plays the act's slot-4 track (10654: ML.Y 1; RV a3_run f595 m_midboss) */
static void neera_trigger(player_t *p) {
    if (H.mly || G.at_g4 >= 10 || H.ended || !one(OT_JCB_NEERA)) return;
    int hit = 0;
    for (int k = 0; k < ntrig && !hit; k++) { obj_t *t = slot(trig[k]); if (t && fabsf(t->x - (float)PX(p)) < 64) hit = p326_rect(p, t->x, t->y, t->x + 16, t->y + 192); }
    if (!hit) return;
    level_lock_markers();
    obj_t *ml = slot(marks[0]);
    if (ml) {
        obj_t *b = spawn(OT_JCB_ICEBAR, S_BAR, ml->x + 24, ml->y, 2);
        (void)b;
        obj_effect(ml->x + 24, ml->y, "wburst");
    }
    H.mly = 1; N.E = 0; N.F = 0; H.g_ride = 0; H.g_intro = 1;
    const char *m = level_music_slot(4);
    if (m) music_play(m);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_JCB")) printf("jcbmusic %d slot4 %s\n", G.frame, m ? m : "-");
#endif
}
/* the ride (0x22a5e, 10710-10720): Neera on the Hovercop (x, y - 50) facing the player; at the cap she jumps off (E 2: anim 10
 * for 30 frames, E 3: anim 7 up 8 px a frame for 16, E 4: hidden) */
static void neera_ride(player_t *p) {
    obj_t *n = one(OT_JCB_NEERA), *hc = one(OT_JCB_HC);
    if (!n || !H.g_ride) return;
    n_face(n, p);
    if (N.E == 0 && !n->hidden) N.E = 1;                                         /* 10713 (shown by 10666) */
    if (N.E == 0 || N.E == 1) n_anim(n, 0);
    if (N.E < 3 && hc) { n->x = hc->x; n->y = hc->y - 50; }                      /* 10715 */
    if (N.E == 2) { n_anim(n, 10); N.F += 1; }                                   /* 10717 */
    if (N.E == 2 && N.F > 30) { N.F = 0; N.E = 3; }                              /* 10718 */
    if (N.E == 3) { n_anim(n, 7); n->y = (float)((int)n->y - 8); N.F += 1; }   /* 10719 */
    if (N.E == 3 && N.F > 16) { N.E = 4; n->hidden = 1; }                       /* 10720 */
}
/* the intro (0x22a59, 10721-10724): every 16 frames an ice crystal falls at trigger.x - 300 + 100 E (top limit y, D 12), E 0..6,
 * then the fight: G = trigger.x, H = top limit y, Neera shown */
static void neera_intro(void) {
    obj_t *n = one(OT_JCB_NEERA), *tr = ntrig ? slot(trig[0]) : NULL;
    if (!n || !H.g_intro) return;
    N.F += 1;
    float top = marker(OT_CAMLIM_T, 0);
    if (N.F > 10 && N.E < 7) {
        obj_t *c = spawn(OT_JCB_CRYSTAL, S_CRYSTAL, (float)(int)((tr ? tr->x : n->x) - 300 + 100 * N.E), top, n->layer);
        if (c) c->av[3] = 12;
        N.F = -5; N.E += 1;
    } else if (N.F > 10 && N.E >= 7) { N.E += 1; N.F = -5; }
    if (N.E >= 8) {
        N.E = 0; N.F = 0; H.g_intro = 0; H.g_fight = 1; H.g_hits = 1;
        N.G = tr ? tr->x : n->x; N.H = top;
        n->hidden = 0;
    }
}
static void ring_throw(obj_t *n) {                   /* 10738 / 10739: a BossIceRing at the action point, direction 16 / 0 */
    float ax, ay; act_point(n, &ax, &ay);
    obj_t *r = spawn(OT_JCB_RING, S_RING, ax, ay, n->layer);
    if (r) { r->vx = N.D < 0 ? -5.f : 5.f; r->vy = 0; }   /* BallMovement speed 40 (5 px a frame; RE ctor) */
    N.fl |= NF2;
}
static void shield_make(obj_t *n) {                  /* 10742-10746: N 0 wood (0), 1 earth (57), 2 water (3), 3 flame (2), 4 magnetic (1) */
    static const int SK[5][2] = { { S_SHW_IN, S_SHW }, { S_SHE_IN, S_SHE }, { S_SHWA_IN, S_SHWA }, { S_SHFF_IN, S_SHFF }, { S_SHM_IN, S_SHM } };
    static const int SND[5] = { 0, 57, 3, 2, 1 };   /* RE play_id args; wood 0 (RV a3_hits f780 / f1144: id 0) */
    int k = (int)N.N; if (k < 0 || k > 4) return;
    N.fl |= NF3;
    obj_t *s = spawn(OT_JCB_NSHIELD, SK[k][0], n->x, n->y, n->layer);
    if (s) s->av[0] = k;
    if (k == 3) { obj_t *b = spawn(OT_JCB_NSHIELD, S_SHFB, n->x, n->y, n->layer); if (b) { b->av[0] = 5; b->par = 1; } }   /* flame back (behind her) */
    snd_play_orig(SND[k], 220, 128);
    N.M = 2; N.L = 30;
    obj_t *h = one(OT_JCB_NHIT); if (h) h->fl |= F_ARMOURED;
}
/* the fight (0x22a5b, 10725-10773) */
static void neera_fight(player_t *p) {
    obj_t *n = one(OT_JCB_NEERA), *h = one(OT_JCB_NHIT), *tr = ntrig ? slot(trig[0]) : NULL;
    if (!n || !H.g_fight) return;
    int iced = 0;                                    /* PlayerIceDeath shown (10733 / 10734 / 10750): not ported (spec) */
    /* 10725: x = G, y = H; G += I; H = min(H + J, K); the hitbox follows */
    n->x = (float)(int)N.G; n->y = (float)(int)N.H;
    N.G += N.I; N.H = fmin(N.H + N.J, N.K);
    if (h) { h->x = n->x; h->y = n->y; }
    if (N.E == 0 && !H.g_snd298) { H.g_snd298 = 1; snd_play_orig(298, 220, 128); }                                 /* 10726 */
    if (N.E == 0) { N.J = 12; if (N.anim != 7) { n_anim(n, 7); n->an.frame = 3; } }                               /* 10727 (frame 9 > last: the last) */
    if (N.E == 0 && N.K <= N.H) { N.J = 0; N.E = 1; n_anim(n, 10); snd_play_orig(33, 220, 128); }                  /* 10728 */
    if (N.E == 1 && (fe_difficulty < 1 || N.F < 10)) n_face(n, p);                                                  /* 10729-10732 */
    if (N.E == 1 && !iced) N.F += 1;                                                                                  /* 10733 */
    if (N.E == 1 && N.F >= 15) n_anim(n, 0);                                                                          /* 10735 */
    if (N.E == 1 && N.F > 30 && !(N.fl & (NF3 | NF5))) { N.F = 0; N.E = 3; n_anim(n, 13); N.fl |= NF5; }          /* 10736 */
    if (N.E == 1 && N.F > 30 && (N.fl & (NF3 | NF5))) { N.F = 0; N.E = 2; n_anim(n, 12); N.fl &= ~NF2; }           /* 10737 */
    if (N.E == 2 && n->an.frame == 3 && !(N.fl & NF2)) ring_throw(n);                                                /* 10738 / 10739 */
    if (N.E == 2 && n->an.done) { N.E = 4; N.F = 0; }   /* (RV E 2: 31-35 frames; port 34-35: the counter carried over) */                                                                 /* 10740 */
    if (N.E == 3) N.N = cross_rand(4);                                                                                /* 10741: (seed >> 29) & 3 */
    if (N.E == 3 && n->an.frame == 3 && !(N.fl & NF3)) shield_make(n);                                               /* 10742-10746 */
    shields_follow(n);                                                                                                /* 10747 */
    if (N.E == 3 && n->an.done) { N.E = 2; N.F = 0; n_anim(n, 12); N.fl &= ~NF2; }                                   /* 10748 */
    if (N.E == 4 && N.F < 15) { n_anim(n, 10); N.F += 1; }                                                            /* 10749 */
    if (N.E == 4) {                                                                                                   /* 10751 / 10752 */
        if (barrier_near(n, 0, 199)) { N.D = -1; n_dir(n, 17); }
        else if (barrier_near(n, -200, -1)) { N.D = 1; n_dir(n, 1); }
    }
    if (N.E >= 4 && N.E <= 5 && N.I > 0 && barrier_near(n, 0, 47)) N.I = 0;                                          /* 10753 */
    if (N.E >= 4 && N.E <= 5 && N.I < 0 && barrier_near(n, -48, -1)) N.I = 0;                                         /* 10754 */
    if (N.E == 4 && N.F == 15) { n_anim(n, 7); N.J = -8; N.F = 16; N.I = 5 * N.D; snd_play_orig(8, 220, 128); }   /* 10755: the jump */
    if (N.E == 4 && N.F > 15) N.J = fmin(N.J + 0.25, 15);                                                            /* 10756 */
    if (N.E >= 4 && N.E <= 5 && N.F > 15 && N.J > 0 && N.K <= N.H) {                                                 /* 10757: lands */
        N.J = 0; N.E = 1; n_anim(n, 10); N.F = 0; N.I = 0; snd_play_orig(33, 220, 128);
    }
    if (N.E == 4 && N.F > 15 && N.J > 0 && (N.fl & NF4) && !H.g_snd297) { H.g_snd297 = 1; snd_play_orig(297, 220, 128); }   /* 10758 */
    if (N.E == 4 && N.F > 15 && N.J > 0 && (N.fl & NF4)) {                                                           /* 10759: the ice rain */
        N.fl &= ~NF4; N.J = -4; N.E = 5; N.I = ((tr ? (int)tr->x : (int)n->x) - (int)n->x) * 0.01;
    }
    if (N.E == 5) { n_anim(n, 16); N.J = fmin(N.J + 0.15, 15); }                                                     /* 10760 */
    if (N.E == 5 && G.frame % 5 == 0) {                                                                               /* 10761 */
        obj_t *c = spawn(OT_JCB_CRYSTAL, S_CRYSTAL, n->x, n->y, n->layer);
        if (c) c->av[2] = (2 * fmod(N.F, 2) - 1) * 5;
        N.F += 1;
    }
    if (N.E == 6) n_face(n, p);                                                                                       /* 10762 / 10763 */
    if (N.E == 6) {                                                                                                   /* 10764: knocked back by the hit */
        n_anim(n, 8);
        double kx = h ? h->kx : 0, ky = h ? h->ky : 0;
        if (h && (h->fl & F_DEFEATED)) ky += 0.5;    /* the shared fly-away (2765) ran twice on the defeated hitbox (RV a3_kill J) */
        N.J = ky; N.I = kx; N.F = 0; N.E += 1;
    }
    if (N.E == 7) { N.J = fmin(N.J + 0.25, 15); N.F += 1; }                                                          /* 10765 */
    if (N.E == 7 && N.I > 0 && barrier_near(n, 0, 47)) N.I = 0;                                                      /* 10766 */
    if (N.E == 7 && N.I < 0 && barrier_near(n, -48, -1)) N.I = 0;                                                    /* 10767 */
    if (N.E == 7 && N.F > 18 && N.J > 0 && N.K <= N.H) {
        if (h && !(h->fl & F_DEFEATED)) { N.J = 0; N.E = 4; n_anim(n, 10); N.F = 0; N.I = 0; snd_play_orig(33, 220, 128); }   /* 10768 */
        else {                                                                                                        /* 10769: defeated */
            N.J = 0; N.F = 0; N.E = 8; N.I = 0;
            double ang = atan2((double)((int)n->y - (int)PY(p)), (double)((int)PX(p) - (int)n->x)) * 57.29577951;
            if (ang < 0) ang += 360;
            N.dir = (int)(ang / 11.25);
            snd_play_orig(33, 220, 128);
            music_fade(90, 0);                                                                                        /* fadeout_music 1500 ms */
            H.g_post = 1; jc_enemy_hover_start();                                                                     /* 0x22a5d: the subtanker scene */
            snd_play_orig(299, 220, 128);
        }
    }
    if (N.E == 8) {
        float rl = marker(OT_CAMLIM_R, 1);                                                                            /* 10770: the right barrier(s) */
        for (int k = 0; k < L[OT_JCB_ICEBAR - OT_JCB_HC].n; k++) { obj_t *b = get(OT_JCB_ICEBAR, k); if (b && b->x >= rl - 199) b->alive = 0; }
        FOR_R(b, OT_JCB_ICEBAR) if (b->x >= rl - 199) b->alive = 0;
        N.F += 1; n_anim(n, 17); N.H = N.K; n->y = (float)(int)N.K;                                                   /* 10771 */
    }
    if (N.E == 8 && N.F > 30 && !H.ended) {                                                                           /* 10772 / 10773 */
        H.ended = 1; H.g_fight = 0; H.g_hits = 0; H.mly = 0; H.g_chase = 0; H.g_leave = 1;
        if (jc_flow_act3_cleared) { jc_flow_act3_cleared(); H.cleared = 1; }                                         /* g4 10, m_stage5b, CAM.M 2 / U 0, Sign x 0 */
        else { G.at_g4 = 10; level_release_camera(0); }                                                               /* (before the flow merge) */
    }
}
/* the hit rules (0x22a5a, 10774-10784) on BossNeeraHitbox (C: the shared hit timer, 5 on the hit frame -> 4 here) */
static void neera_hits(void) {
    obj_t *n = one(OT_JCB_NEERA), *h = one(OT_JCB_NHIT);
    if (!n || !h || !H.g_hits) return;
    int shielded = (N.fl & NF3) != 0;
    if (h->c > 1 && !shielded) N.inv = 1;                                                                    /* 10774: invert shader */
    if (h->c == 4 && shielded && N.L <= 0 && N.M <= 0) {                                                     /* 10776 / 10777: it breaks */
        FOR_R(s, OT_JCB_NSHIELD) s->alive = 0;
        N.fl &= ~NF3; h->c = 30; N.inv = 0; snd_play_orig(11, 220, 128);
    }
    if (h->c <= 1) N.inv = 0;                                                                                /* 10778 */
    if (h->c == 4 && shielded && N.L <= 0 && N.M > 0) {                                                      /* 10779 / 10780: it holds */
        FOR_R(s, OT_JCB_NSHIELD) anim_restart(&s->an);   /* force_animation 0, then back (INFERRED: a restart) */
        N.M -= 1; N.inv = 0; snd_play_orig(315, 220, 128); N.L = 30;
    }
    if (N.L > 0) { N.L -= 1; h->fl |= F_ARMOURED; }                                                         /* 10781 */
    if ((h->fl & F_ARMOURED) && !(N.fl & NF3) && N.L <= 0) h->fl &= ~F_ARMOURED;                            /* 10782 */
    if (h->A < h->av[0] * 35 && !(h->fl & F_DEFEATED)) {                                                  /* 10783: a third of her HP */
        h->av[0] -= 1; N.fl |= NF4; N.E = 6; snd_play_orig(296, 220, 128); N.fl &= ~NF5;   /* loop "n_hit_<0|1>": no such loop in the frame (no voice) */
    }
    if ((h->fl & F_DEFEATED) && N.E < 6) {                                                                   /* 10784: defeated */
        h->av[0] -= 1; N.fl |= NF4; N.E = 6; snd_play_orig(25, 220, 128);
        FOR_R(r, OT_JCB_RING) { obj_effect(r->x, r->y, "wburst"); r->alive = 0; }
    }
}
/* the leave (0x22a5c, 10785-10788): the rig in front, the Hovercop flies right (D 1, E + 0.2 to 10); Neera rides again once
 * it passes her (E 0: at hc - 50, anim 0, direction 1, behind the Hovercop) */
static void neera_leave(void) {
    obj_t *n = one(OT_JCB_NEERA), *hc = one(OT_JCB_HC);
    if (!H.g_leave) return;
    if (!H.g_leave_once) { H.g_leave_once = 1; H.front = 1; }                                                 /* 10785 */
    if (hc && !H.gone) { H.D = 1; H.E = fmin(H.E + 0.2, 10); }                                                /* 10786 */
    if (n && hc && !H.gone && (int)n->x < (int)hc->x) N.E = 0;                                                /* 10787 */
    if (n && hc && !H.gone && N.E == 0) { n->x = hc->x; n->y = hc->y - 50; N.dir = 1; n_anim(n, 0); n->hidden = 0; }   /* 10788 */
    if (hc && !H.gone && hc->x > G.camx + SCREEN_W + 600) { rig_destroy(); if (n) n->alive = 0; }        /* off screen for good (INFERRED cleanup) */
}
/* ice rings (BossIceRing_2481, BallMovement speed 40 = 5 px a frame): 10789 / 10790 a sparkle every 3 frames on a random ring
 * (x, y -16 + R(33)); col_6_41 an ice barrier: moving 0 / 16 it turns (12 / 4), sound 31, then A + 1, a bounce, sound 31 (RV a3_kill
 * f881 / f960: two 31s, then (-4.9, -1) a frame: 11.25 degrees up); col_24_41 Milla's cube (A >= 4): A + 1, bounce, 31;
 * 10791 A > 2: a white burst, gone; 10792 Player_326, OTHERS.E <= 0: 10 damage, hc F 5, OTHERS.R 30, a white burst, gone */
static void rings_frame(player_t *p) {
    int any = 0;
    FOR_R(r, OT_JCB_RING) {
        any = 1;
        if (r->born == G.frame) continue;
        float ox = r->fx, oy = r->fy;
        r->fx += r->vx; r->fy += r->vy; r->x = (float)(int)r->fx; r->y = (float)(int)r->fy;
        obj_t *bar = NULL;
        for (int k = 0; k < L[OT_JCB_ICEBAR - OT_JCB_HC].n && !bar; k++) { obj_t *b = get(OT_JCB_ICEBAR, k); if (b && obj_obj_hit(r, b)) bar = b; }
        if (!bar) FOR_R(b, OT_JCB_ICEBAR) if (!bar && obj_obj_hit(r, b)) bar = b;
        int cube = !bar && player_cube_state() >= 4 && player_cube_hits(r);
        int bg = !bar && !cube && obj_overlaps_bg(r);
        if (bar || cube) {
            r->fx = ox; r->fy = oy; r->x = (float)(int)ox; r->y = (float)(int)oy;   /* test_collisions_save: back to the old position */
            if (bar && r->vy == 0) snd_play_orig(31, 220, 128);
            float s = r->vx > 0 ? -1.f : 1.f;
            r->vx = s * 4.9039264f; r->vy = -0.9754516f;   /* RV: the bounce leaves at 11.25 degrees up (INFERRED for the other cases) */
            r->av[0] += 1; snd_play_orig(31, 220, 128);
        } else if (bg) {
            /* the backdrop (RV a3_hits f1012 / f1091: a slope, x mirrored; f1118 / f1197: the ceiling, A 3: gone): A + 1, bounce,
             * sound 31 (BallMovement on an obstacle; the axis from which move hits, INFERRED) */
            r->x = (float)(int)(ox + r->vx); r->y = (float)(int)oy; int hx = obj_overlaps_bg(r);
            r->fx = ox; r->fy = oy; r->x = (float)(int)ox; r->y = (float)(int)oy;
            if (hx) r->vx = -r->vx; else r->vy = -r->vy;
            r->av[0] += 1; snd_play_orig(31, 220, 128);
        }
        if (r->av[0] > 2) { obj_effect(r->x, r->y, "wburst"); r->alive = 0; continue; }
        if (vulnerable(p) && p326_hit(r)) { ice_hurt(p, 10); obj_effect(r->x, r->y, "wburst"); r->alive = 0; continue; }
        if (fabsf(r->x - G.camx) > 1400 || r->y < G.camy - 600) r->alive = 0;   /* (INFERRED cleanup) */
    }
    if (any && G.frame % 3 == 0) {
        int n = 0; FOR_R(r, OT_JCB_RING) n++;
        int pick = cross_rand(n), k = 0;
        FOR_R(r, OT_JCB_RING) { if (k++ == pick) { obj_effect((float)((int)r->x - 16 + cross_rand(33)), (float)((int)r->y - 16 + cross_rand(33)), "ringsparkle"); } }
    }
}
static void shields_frame(void) {                    /* the shield's own cycle: "in" (anim 7) then its loop (INFERRED) */
    static const int LOOP[6] = { S_SHW, S_SHE, S_SHWA, S_SHFF, S_SHM, S_SHFB };
    FOR_R(s, OT_JCB_NSHIELD) if (s->an.done && !(s->fl & FL1)) { s->fl |= FL1; int k = (int)s->av[0]; if (k >= 0 && k < 6) set_a(s, LOOP[k]); }
}
/* 10798: g4 >= 10 (a restart after the fight): the leave and the subtanker scene on, the barriers gone (once) */
static void neera_restart(void) {
    if (H.mode != M_CHASE3 || H.g_restart || G.at_g4 < 10) return;
    H.g_restart = 1; H.g_leave = 1; H.g_post = 1; H.ended = 1; H.g_ride = 0; jc_enemy_hover_start();
    for (int k = 0; k < L[OT_JCB_ICEBAR - OT_JCB_HC].n; k++) { obj_t *b = get(OT_JCB_ICEBAR, k); if (b) b->alive = 0; }
}

/* ======================================================================== hooks */
/* ======================================================================== JC3: the subtanker scene (0x22a5d, 10799-10805; RV a3_kill /
 * a3_sub). Json layers 4 / 5 (port 3 / 5: the jcsub* / jcsubtanker* pieces, 21 torpedo launchers) move by (B, A + 40 sin g3) with
 * A -2 a frame to -600 and B +1 to 600 from the defeat frame (RV a3_sub: A -2 B 1 at E 8's first frame, B 600 at +600); their
 * objects move with them (RV: the launchers' layer x / y stay constant). The player is on layer 4 at MV.X > 10580 (10804 / 10805,
 * MV.X: the global x; RV a3_kill f1371: Player x 10248 = MV.X 10584 - B 336, y - (A + bob)) and then tests that layer's backdrops
 * only (level_t.mv: <lvdir>_mvl.bin, tools/import_frame.py write_mvl); 10801: grounded there with B < 600, MV.X + 1 (RV a3_sub
 * f1408-1633: MV.X + 1 a frame standing); 10803: on layer 3, CAM.C = the boss's left limit. The sine uses the port's frame counter
 * for g3 (the bob's phase, INFERRED). */
#define SUB_X 10580
static struct { uint8_t *d; char dir[8]; int nobj; short obj[48]; } SUB;
static void sub_load(void) {
    if (SUB.d && strcmp(SUB.dir, G.lvdir)) { free(SUB.d); SUB.d = NULL; }
    if (!SUB.d) {
        char fn[40]; size_t sz;
        snprintf(fn, sizeof fn, "%s/%s_mvl.bin", G.lvdir, G.lvdir);
        SUB.d = plat_load_file(fn, &sz);
        if (SUB.d && (sz < 16 || memcmp(SUB.d, "MVL1", 4))) { free(SUB.d); SUB.d = NULL; }
        if (!SUB.d) return;
        snprintf(SUB.dir, sizeof SUB.dir, "%s", G.lvdir);
    }
    const uint8_t *d = SUB.d;
    int pl = d[4], nobj = d[6] | d[7] << 8, tw = d[8] | d[9] << 8, th = d[10] | d[11] << 8, nu = d[12] | d[13] << 8, tx0 = d[14] | d[15] << 8;
    size_t off = 16;
    SUB.nobj = 0;
    for (int k = 0; k < nobj; k++, off += 10) {                /* the layer's objects as placed: u16 type, s32 x, y */
        int t = d[off] | d[off + 1] << 8;
        int32_t x = (int32_t)(d[off + 2] | d[off + 3] << 8 | d[off + 4] << 16 | (uint32_t)d[off + 5] << 24);
        int32_t y = (int32_t)(d[off + 6] | d[off + 7] << 8 | d[off + 8] << 16 | (uint32_t)d[off + 9] << 24);
        for (int i = 0; i < G.nobjs && SUB.nobj < 48; i++) {
            const obj_t *o = &G.o[i];
            if (o->alive && o->type == t && (t < OT_JCB_HC || t > OT_JCB_END) && (int)o->ox == x && (int)o->oy == y) {   /* (the rig places itself) */
                int dup = 0; for (int j = 0; j < SUB.nobj; j++) dup |= SUB.obj[j] == i;
                if (!dup) { SUB.obj[SUB.nobj++] = (short)i; break; }
            }
        }
    }
    off = (off + 3) & ~(size_t)3;                                  /* (padded: the malloc'd file is aligned) */
    G.lvl.mv.tiles = (const uint16_t *)(d + off); off += (size_t)nu * 32;
    G.lvl.mv.smap = d + off; off += (size_t)tw * th;
    G.lvl.mv.pmap = d + off;
    G.lvl.mv.tw = tw; G.lvl.mv.th = th; G.lvl.mv.tx0 = tx0; G.lvl.mv.nuniq = nu; G.lvl.mv.x0 = SUB_X + 1; G.lvl.mv.on = 0;
    (void)pl;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_JCB")) { printf("jcbsub %d of %d objects:", SUB.nobj, nobj); for (int k = 0; k < SUB.nobj; k++) printf(" %d", G.o[SUB.obj[k]].type); printf("\n"); }
#endif
}
static void sub_frame(player_t *p) {
    if (!H.g_post || !SUB.d) return;
    if (H.sA > -600) H.sA -= 2;                                                                      /* 10799 */
    if (H.sB < 600) H.sB += 1;                                                                       /* 10800 */
    int on4 = p->x > SUB_X;                                                                          /* 10804 / 10805 */
    if (on4 && p->ground && H.sB < 600 && p->state != ST_DEAD) p->x += 1;                           /* 10801 */
    int g = G.frame % 360;
    int dx = (int)H.sB, dy = (int)((g != 180 ? dsin(g) * 40 : 0) + H.sA);                           /* 10802 */
    int ddx = dx - H.sdx, ddy = dy - H.sdy;
    for (int k = 0; k < SUB.nobj; k++) { obj_t *o = slot(SUB.obj[k]); if (o) { o->x += ddx; o->y += ddy; } }
    H.sdx = dx; H.sdy = dy;
    G.lvl.lay_dx[3] = G.lvl.lay_dx[5] = (int16_t)dx; G.lvl.lay_dy[3] = G.lvl.lay_dy[5] = (int16_t)dy;
    G.lvl.mv.on = 1; G.lvl.mv.dx = dx; G.lvl.mv.dy = dy;
    if (!on4) { float l = marker(OT_CAMLIM_L, 1); if (l > -1e8f) G.cam_nC = l; }                    /* 10803 */
}
int jc_boss_player_front(void) { return on && H.g_post && SUB.d && G.p.x > SUB_X; }

static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&H, sizeof H); snap_register(&N, sizeof N); snap_register(&R, sizeof R); snap_register(&nR, sizeof nR); } }
void jc_boss_spawn(void) {
    jc_boss5_spawn();
    if (SUB.d && strcmp(SUB.dir, G.lvdir)) { free(SUB.d); SUB.d = NULL; }   /* another level: the moving layer's file goes */
    snap_once();
    memset(&H, 0, sizeof H); memset(&N, 0, sizeof N); memset(L, 0, sizeof L); nR = 0;
    marks[0] = marks[1] = marks[2] = marks[3] = -1; ntrig = 0; on = 0;
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_JCB_HC && G.lvl.objs[j].type <= OT_JCB_ICEPIECE) on = 1;   /* (JC1's Hovercop waits above the map: y < 0) */
    if (!on) return;
    for (int k = 0; k < S_N; k++) SA[k] = (short)ens_anim(SN[k]);
    /* lvobj anim: instance layer << 8 (tools/import_frame.py JC_BOSS); the k-th object of a type is its k-th placement */
    static uint16_t AN[NT][LMAX]; int na[NT] = { 0 }, seen[NT] = { 0 };
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - OT_JCB_HC; if (t >= 0 && t < NT && na[t] < LMAX) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if (t == OT_CAMTRIGGER && ntrig < 4) trig[ntrig++] = (short)i;
        if (t >= OT_CAMLIM_L && t <= OT_CAMLIM_B && marks[t - OT_CAMLIM_L] < 0) marks[t - OT_CAMLIM_L] = (short)i;
        if (t < OT_JCB_HC || t > OT_JCB_ICEPIECE) continue;
        int ti = t - OT_JCB_HC, an = seen[ti] < na[ti] ? AN[ti][seen[ti]] : (2 << 8); seen[ti]++;
        if (L[ti].n < LMAX) L[ti].i[L[ti].n++] = (short)i;
        o->layer = (an >> 8) & 0xff; o->st = o->tm = o->c = o->fl = o->dir = 0; o->ang = 0; o->scale = 0; o->bd = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL;
        switch (t) {
        case OT_JCB_HC: set_a(o, S_HC); break;
        case OT_JCB_TB1: set_a(o, S_TB1); break;
        case OT_JCB_TB2: set_a(o, S_TB2); break;
        case OT_JCB_PROP1: case OT_JCB_PROP2: set_a(o, S_PROP); break;
        case OT_JCB_SIR1: case OT_JCB_SIR4: set_a(o, S_SIR1); break;
        case OT_JCB_SIR2: case OT_JCB_SIR3: set_a(o, S_SIR2); break;
        case OT_JCB_HOOK: set_a(o, S_HOOK); break;
        case OT_JCB_HOOK2: set_a(o, S_HOOK2); break;
        case OT_JCB_LIGHT: set_a(o, S_LIGHT); break;
        case OT_JCB_ICED: set_a(o, S_ICED); break;
        case OT_JCB_NEERA: break;
        case OT_JCB_NHIT: set_a(o, S_NHIT); o->A = 120; o->av[0] = 3; o->fl |= F_NOFLY; break;   /* ctor A 120, D 3 (RE) */
        case OT_JCB_ICEBAR: set_a(o, S_BAR); break;
        case OT_JCB_ICEPIECE: set_a(o, S_BARP); o->hidden = 1; break;   /* a template (never created: RE) */
        }
    }
    obj_t *hc = one(OT_JCB_HC), *n = one(OT_JCB_NEERA);
    if (n) H.mode = M_CHASE3; else if (hc && hc->layer == 0) H.mode = M_FLYBY; else if (hc) H.mode = M_CHASE2;
    H.D = 1; H.H = 4; H.J = -56;                     /* BossHovercop ctor (RE) */
    if (hc) {
        if (H.mode == M_FLYBY) { hc->scale = 0.5f; for (int t = OT_JCB_TB1; t <= OT_JCB_SIR4; t++) { obj_t *o = one(t); if (o) o->scale = 0.5f; } }   /* 10349 */
        else if (H.mode == M_CHASE2) { H.A = hc->x; H.B = hc->y; }
        else { H.A = hc->x; H.L = hc->y; H.B = -200; hc->y = -200; }   /* JC3: parked above the view until 10666 (RV a3_run f0) */
    }
    if (n) {
        N.D = 1; N.K = 1543; N.dir = 1; N.anim = -1; n_anim(n, 0);   /* instance alterables (RV a3_run f0: D 1, K 1543) */
        H.g_ride = 1;                                                /* 0x22a5e on at the start */
    }
    if (H.mode == M_CHASE3) sub_load();
    neera_restart();
}
void jc_boss_pre(player_t *p) {
    jc_boss5_pre(p);
    if (!on) return;
#ifdef FP_HOST_ENV
    {   /* host tests: FP_JCB_SET="frame:field:value,..." on BossNeeraHitbox (fields A, C, KX, KY), as the tracer's SETV */
        const char *e = getenv("FP_JCB_SET"); obj_t *h = one(OT_JCB_NHIT);
        while (e && *e && h) {
            int f; char k[4] = { 0 }; double v; int nn = 0;
            if (sscanf(e, "%d:%3[A-Z]:%lf%n", &f, k, &v, &nn) < 3 || nn <= 0) break;
            if (f == G.frame) { if (!strcmp(k, "A")) h->A = v; else if (!strcmp(k, "C")) h->c = (int)v; else if (!strcmp(k, "KX")) h->kx = v; else if (!strcmp(k, "KY")) h->ky = v; }
            e += nn; if (*e == ',') e++;
        }
    }
#endif
    r_compact();
    if (H.mode == M_CHASE3) neera_trigger(p);       /* test_collisions_41: 10589 */
}
void jc_boss_frame(player_t *p) {
    jc_boss5_frame(p);
    if (!on) return;
    if (H.mode == M_FLYBY) flyby_frame(p);
    if (H.mode == M_CHASE2 || H.mode == M_CHASE3) {
        neera_restart();
        chase_move(p);
        chase_attack(p);
        chase_cap();
        neera_ride(p);
        neera_intro();
        neera_fight(p);
        neera_hits();
        neera_leave();
        rings_frame(p);
        shields_frame();
        ice_frame(p);
        sub_frame(p);
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_JCB")) {
        const obj_t *hc = one(OT_JCB_HC), *n = one(OT_JCB_NEERA), *h = one(OT_JCB_NHIT);
        int nr = 0, ns = 0, nc = 0, nm = 0; FOR_R(r, OT_JCB_RING) nr++; FOR_R(s, OT_JCB_NSHIELD) ns++; FOR_R(c, OT_JCB_CRYSTAL) nc++; FOR_R(m, OT_JCB_METEOR) nm++;
        printf("jcb %d px %.0f py %.0f hcx %.0f hcy %.0f A %.2f E %.2f H %.0f I %.0f J %.0f O %.0f D %.0f ch %d nE %.0f nF %.0f nx %.0f ny %.0f na %d nd %d hid %d nG %.2f nH %.2f nI %.2f nJ %.2f nL %.0f nM %.0f nfl %x hA %.1f hC %d hD %.0f hfl %x ml %d ride %d intro %d fight %d leave %d rings %d sh %d cr %d met %d g4 %d gone %d post %d sA %.0f sB %.0f sdy %d cU %d cnC %.0f cnE %.0f camx %.0f cA %.0f m2 %d M3 %d cine %d\n",
            G.frame, PX(p), PY(p), hc ? hc->x : -1, hc ? hc->y : -1, H.A, H.E, H.H, H.I, H.J, H.O, H.D, H.g_chase,
            N.E, N.F, n ? n->x : -1, n ? n->y : -1, N.anim, N.dir, n ? n->hidden : -1, N.G, N.H, N.I, N.J, N.L, N.M, N.fl,
            h ? h->A : -1, h ? h->c : -1, h ? h->av[0] : -1, h ? h->fl : 0, H.mly, H.g_ride, H.g_intro, H.g_fight, H.g_leave, nr, ns, nc, nm, G.at_g4, H.gone, H.g_post, H.sA, H.sB, H.sdy, G.cam.U, G.cam_nC, G.cam_nE, G.camx, G.cam.A, G.cam_m2, G.cam.M3, G.cam.cine);
        FOR_R(r, OT_JCB_RING) printf("jcbring %d %d %.0f %.0f %.0f\n", G.frame, (int)(r - G.o), r->x, r->y, r->av[0]);
    }
#endif
}
int jc_boss_obj(obj_t *o, player_t *p) {
    if (o->type >= OT_JCB5_FIRST) return jc_boss5_obj(o, p);
    (void)p;
    return 1;                                        /* every 1430-1479 object runs in jc_boss_frame */
}
int jc_boss_q20(int type) { return type == OT_JCB_NHIT || jc_boss5_q20(type); }   /* BossNeeraHitbox_2482 (RE qualifier 20) */
static void draw_one(const obj_t *o) {
    if (!o || !o->alive || !o->an.b || o->hidden) return;
    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
    if (sx < -480 || sx > SCREEN_W + 480 || sy < -300 || sy > SCREEN_H + 300) return;
    const frame_t *f = anim_frame(&o->an);
    int inv = o->type == OT_JCB_NEERA ? N.inv : 0, add = o->type == OT_JCB_LIGHT;
    float sc = o->scale != 0 ? o->scale : 1.f, xs = 1.f;
    if (o->type == OT_JCB_LIGHT) xs = 8.f;                                   /* LevelHovercraftLight: x scale 8, additive (RE ctor) */
    if (o->type == OT_JCB_STICKY) { if (o->av[1] <= 0) return; sc = (float)o->av[1]; xs = 1.f / sc; }   /* 10558: y scale */
    if (inv) r_set_invert(1);
    if (add) r_set_additive(1);
    r_set_scale(sc); r_set_xscale(xs);
    frame_draw_rot(o->an.b, f, sx, sy, o->ang, o->flip, 0xffffffff);
    r_set_scale(1.f); r_set_xscale(1.f);
    if (add) r_set_additive(0);
    if (inv) r_set_invert(0);
}
/* the rig is drawn as one group at the Hovercop's turn (RE draw order: 10785's move_front order siren3, siren4, turbine2,
 * propellor2, Hovercop, turbine1, propellor1, siren1, siren2, light; Neera riding just behind the Hovercop, 10710 / 10788) */
int jc_boss_draw_obj(const obj_t *o, int pass) {
    if (o->type >= OT_JCB5_FIRST) return jc_boss5_draw_obj(o, pass);
    if (!on) return 1;
    switch (o->type) {
    case OT_JCB_HC: {
        static const int ORD[] = { OT_JCB_HOOK2, OT_JCB_SIR3, OT_JCB_SIR4, OT_JCB_TB2, OT_JCB_PROP2, -1, OT_JCB_HC, OT_JCB_TB1, OT_JCB_PROP1, OT_JCB_SIR1, OT_JCB_SIR2, OT_JCB_LIGHT };
        const obj_t *n = one(OT_JCB_NEERA);
        int riding = n && n->layer == o->layer && (H.g_ride || (H.g_leave && N.E == 0));
        for (unsigned k = 0; k < sizeof ORD / sizeof ORD[0]; k++) {
            if (ORD[k] < 0) { if (riding) draw_one(n); continue; }
            const obj_t *q = one(ORD[k]);
            if (q && q->layer == o->layer) {
                if (ORD[k] == OT_JCB_HOOK2) FOR_R(m, OT_JCB_MISSILE) if (!(m->fl & FL1)) draw_one(m);   /* the held missile on its chain */
                draw_one(q);
            }
        }
        return 1; }
    case OT_JCB_TB1: case OT_JCB_TB2: case OT_JCB_PROP1: case OT_JCB_PROP2: case OT_JCB_SIR1: case OT_JCB_SIR2: case OT_JCB_SIR3:
    case OT_JCB_SIR4: case OT_JCB_LIGHT: case OT_JCB_HOOK2: {
        const obj_t *hc = one(OT_JCB_HC);
        if (!hc || hc->layer != o->layer) draw_one(o);
        return 1; }
    case OT_JCB_NEERA: {
        const obj_t *hc = one(OT_JCB_HC);
        int riding = hc && hc->layer == o->layer && (H.g_ride || (H.g_leave && N.E == 0));
        if (riding) return 1;
        FOR_R(s, OT_JCB_NSHIELD) if (s->par) draw_one(s);   /* the flame shield's back half */
        draw_one(o);
        FOR_R(s, OT_JCB_NSHIELD) if (!s->par) draw_one(s);
        return 1; }
    case OT_JCB_NSHIELD: return 1;                  /* drawn with Neera */
    case OT_JCB_MISSILE: if (!(o->fl & FL1)) return 1; draw_one(o); return 1;
    case OT_JCB_NHIT: case OT_JCB_ICEPIECE: case OT_JCB_HOOK: case OT_JCB_ICED: return 1;
    default: draw_one(o); return 1;
    }
}
int jc_boss_input(uint32_t *held) { return jc_boss5_input(held); }
int jc_boss_player_hidden(void) { return jc_boss5_player_hidden(); }
