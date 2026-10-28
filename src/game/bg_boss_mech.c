/* Battle Glacier: the Brevon mech. Spec: build/re/spec_bg_bosses.md section 3.
 *  - BG4 (json 63): Absolution, the opening fight (RE 14418-14512, group 0x22af2; start events 14609 / 14610). The rig is 25
 *    Actives (off-map templates in the original, created here at the act start when the level has the rows): torso, head, weak
 *    spot, turret, charge orb, abdomen, axle, shoulders, arms, claws, legs, knee, foot and 2 x 4 wings, driven by the controller
 *    BrevonMechAnimations (A the pose phase, B the facing, C / D / E the stand / fly / jump pose weights, H jumps).
 *  - BrevonMechBackground_2444 (json layer 1, scrolling 0.5: RE Layer::init of on_frame_61..65_init): BG4's intro and sky
 *    crossings, BG2's / BG3's set pieces (handle_frame_61 inline + 14212-14216 / 10363; handle_frame_62 inline + 14308-14315) and
 *    their laser barrages. BG6 places it too; nothing there moves it (it stays hidden).
 *  - BrevonMechLaser_2583 + EffectBrevonMechWarning_2582 (14221-14227 / 14316-14320 / 14503-14510): shared by BG2-BG4.
 * Torso alterables: C / D its x / y (y before the pose), E the stand y, F state (-1 intro, 0 stand, 1 fly, 2 land, 3 jump), G
 * sub-state / timer, H the hover y, I the jump's x speed. Turret: B the barrage timer (540), C state, D the barrel's reach. Weak spot:
 * A 50 (Q20, refilled at every hit), C the Q20 hit timer, D 50 frames of invulnerability, E 16 hits, F explosions, G the end timer.
 * Labels: RE decompiled, RV build/orig_traces/bg_bosses (m1_l, m2_l: Lilac with invulnerability; m3_l: Lilac standing still),
 * INFERRED reasoned. */
#include "bg_boss.h"
#include <stdio.h>
#include <stdlib.h>

enum { P_BSH, P_BCLAW, P_BARM, P_BW0, P_BW1, P_BW2, P_BW3, P_FLEG, P_ABD, P_FLEG2, P_FKNEE, P_TORSO, P_HEAD,
       P_FW0, P_FW1, P_FW2, P_FW3, P_FFOOT, P_FSH, P_FARM, P_FCLAW, P_AXLE, P_WEAK, P_CHARGE, P_TURRET, NP };
/* the json order of the templates (433-435 on layer 2, 2814-3282 on layer 3) is their draw order */
static const struct { short type; const char *row; signed char layer; } PD[NP] = {
    { OT_BGB_M_BSHOULDER, "bgm_bsh", 2 }, { OT_BGB_M_BCLAW, "bgm_bclaw", 2 }, { OT_BGB_M_BARM, "bgm_barm", 2 },
    { OT_BGB_M_BWING, "bgm_wing", 3 }, { OT_BGB_M_BWING, "bgm_wing", 3 }, { OT_BGB_M_BWING, "bgm_wing", 3 }, { OT_BGB_M_BWING, "bgm_wing", 3 },
    { OT_BGB_M_FLEG, "bgm_fleg", 3 }, { OT_BGB_M_ABDOMEN, "bgm_abd", 3 }, { OT_BGB_M_FLEG2, "bgm_fleg2", 3 }, { OT_BGB_M_FKNEE, "bgm_fknee", 3 },
    { OT_BGB_M_TORSO, "bgm_torso", 3 }, { OT_BGB_M_HEAD, "bgm_head", 3 },
    { OT_BGB_M_FWING, "bgm_wing", 3 }, { OT_BGB_M_FWING, "bgm_wing", 3 }, { OT_BGB_M_FWING, "bgm_wing", 3 }, { OT_BGB_M_FWING, "bgm_wing", 3 },
    { OT_BGB_M_FFOOT, "bgm_ffoot", 3 }, { OT_BGB_M_FSHOULDER, "bgm_fsh", 3 }, { OT_BGB_M_FARM, "bgm_farm", 3 }, { OT_BGB_M_FCLAW, "bgm_fclaw", 3 },
    { OT_BGB_M_AXLE, "bgm_axle", 3 }, { OT_BGB_M_WEAK, "bgm_weak0", 3 }, { OT_BGB_M_CHARGE, "bgm_charge", 3 }, { OT_BGB_M_TURRET, "bgm_turret", 3 } };

#define OPEN_G4 5                                    /* 14502: the checkpoint after the fight (bg_flow_opening_cleared) */
#define HATCH_SHUT 1696                              /* 14419: the hatch doors' y while the fight runs (placed at 2144) */
#define BG_COEF 0.5f                                 /* RE on_frame_61..65_init: Layer::init(1, 0.5, 0.5) */
#define LASER_DMG 20                                 /* RE 14226: LIFE.B += 20 */
#define TFL_AWAY 0x10                                /* torso flag 4: away from the arena (14609, G 1; cleared at the landing, 14455) */

static struct {
    int rig;                                         /* the act has the fight's rows (json 63) */
    int act, phase;                                  /* phase: 0 waiting for the trigger, 1 the fight, 2 over */
    short pi[NP];
    double C, D, E, F, G, H, I; int tfl;             /* torso */
    double aA, aB, aC, aD, aE; int aH, slow;         /* BrevonMechAnimations (slow: the landing's slowed updates, INFERRED) */
    double hC; int hD, hopen;                        /* head: open frame 0..3, laser timer, flag 1 (open) */
    double uB, uD; int uC, ubit;                     /* turret */
    int wD, wE, wF, wG, dead;                        /* weak spot */
    int music, hatch0[4];                            /* ML.Y 1 played; the hatch doors' placed y */
    int bgA, bgB, bgvis;                             /* the background mech's A / B / visible */
    float bgy0;
} M;

static obj_t *part(int k) { obj_t *o = bgb_slot(M.pi[k]); return o && o->type == PD[k].type ? o : NULL; }
static obj_t *bgmech(void) { return bgb_get(OT_BGB_MECHBG, 0); }
int bg_boss_mech_state(void) { return M.rig ? (M.phase == 0 ? -1 : M.phase == 2 ? 9 : (int)M.F) : -9; }
static double cosd(double a) { return sbb_cos(a); }
static double sind(double a) { return sbb_sin(a); }

/* ======================================================================== the background mech (json layer 1, scroll 0.5) */
static float bg_C(const obj_t *b) { return b->x + BG_COEF * G.camx; }    /* the x it shows at, in play-layer coordinates (RV m2_l: C) */
static void bg_show(obj_t *b, int on) { if (b) { b->hidden = !on; M.bgvis = on; } }

static int hurt(player_t *p, int n, const char *why) {
    int r = sbb_hurt(p, n);
#ifdef FP_HOST_ENV
    if (r && getenv("FP_DBG_BGM")) printf("bgm-hurt %d %s %d\n", G.frame, why, n);
#else
    (void)why;
#endif
    return r;
}
/* ---- lasers (BrevonMechLaser_2583: A 50 = scale %, B 8 = speed; RE 14221-14227 / 14316-14320 / 14507-14510) and their warnings
 * (EffectBrevonMechWarning_2582, 14217-14220 / 14503-14506: at the view's edge while the laser comes from outside it) */
static float laser_floor(void) {
    obj_t *t = part(P_TORSO);
    if (M.rig && t) return (float)(M.E + 80);                                /* 14507 / 14508: torso E + 80 */
    return M.act == 2 ? 1336.f : 1260.f;                                     /* 14223 / 14224 (BG2), 14316 / 14317 (BG3) */
}
static void laser_spawn(float x, float y, double ang) {
    obj_t *l = bgb_spawn(OT_BGB_M_LASER, "bgm_laser", x, y, 3);
    if (!l) return;
    l->av[0] = 50; l->av[1] = 8; l->av[2] = x; l->av[3] = y; l->av[4] = ang;   /* ctor A 50, B 8; 14221: C, D = X, Y */
    l->scale = 0.5f; l->ang = (float)ang;
    obj_t *w = bgb_spawn(OT_BGB_M_WARN, "bgm_warn", x, y, 3);
    if (w) { w->av[0] = (double)(l - G.o); w->hidden = 1; }
}
static void lasers(player_t *p) {
    float fl = laser_floor();
    for (int k = 0; k < bgb_runtime_n(); k++) {
        obj_t *l = bgb_runtime(k);
        if (!l) continue;
        if (l->type == OT_BGB_M_WARN) {                                      /* 14503-14506 */
            obj_t *o = bgb_slot((int)l->av[0]);
            if (!o || o->type != OT_BGB_M_LASER) { l->alive = 0; continue; }
            int out = o->x < G.camx || o->x > G.camx + SCREEN_W;
            l->hidden = !out;
            l->x = fminf(fmaxf(o->x, G.camx + 20), G.camx + SCREEN_W - 20); l->y = G.camy + 20;
            continue;
        }
        if (l->type != OT_BGB_M_LASER) continue;
        double *a = l->av;                                                   /* 14222: C += cos(E) B, D -= sin(E) B, scale A %, A + 3 */
        a[2] += cosd(a[4]) * a[1]; a[3] -= sind(a[4]) * a[1];
        l->x = (float)(int)a[2]; l->y = (float)(int)a[3];
        a[0] = fmin(a[0] + 3, 100); l->scale = (float)(a[0] * 0.01); l->ang = (float)a[4];
        if (l->y > fl) { if (a[4] < 0) a[4] += 2; else if (a[4] > 180) a[4] -= 2; }   /* 14223 / 14224: below the floor it turns flat */
        if (l->x < G.camx - 300 || l->x > G.camx + SCREEN_W + 300) { l->alive = 0; continue; }   /* 14225 (BG2: -200, INFERRED one rule) */
        if (sbb_csens_hit(p, l) && hurt(p, LASER_DMG, "laser")) {                 /* 14226: R 0, E <= 0 -> LIFE.B += 20, MV.B / G pushed */
            p->vx += (float)(cosd(a[4]) * a[1]); p->vy -= (float)(sind(a[4]) * a[1]);
        }
    }
}

/* ---- BG2 / BG3 set pieces (the act number is level data: levels.c .act) */
static void bg_setpiece(player_t *p) {
    obj_t *b = bgmech();
    if (!b) return;
    double X = sbb_px(p), Y = sbb_py(p);
    if (M.act == 2) {
        if (M.bgA == 0 && b->x < X) { M.bgA = 1; snd_play_orig(295, 220, 128); bg_show(b, 1); }          /* inline: A 0 -> 1 */
        if (M.bgA == 1) { b->x += 3; b->y -= 2; M.bgB++; }
        if (M.bgA == 1 && M.bgB > 100) {                                                                /* 14212 */
            M.bgA = 2; M.bgB = 0; float ax, ay; bgb_act_point(b, &ax, &ay);
            obj_t *f = bgb_spawn(OT_BGB_FX, "bgm_burst", ax, ay, -1); if (f) f->av[7] = 1;   /* EffectPlasmaBurst_3091 on layer 1 */
            snd_play_orig(294, 220, 128); G.cam.Z = 10;
        }
        if (M.bgA == 2) { b->x += 20; M.bgB++; if (M.bgB > 60) bg_show(b, 0); }                          /* 10363 */
        if (X > 12000 && M.bgA < 10) { bg_show(b, 0); M.bgA = 10; snd_play_orig(342, 220, 128); M.bgB = 0; }
        if (M.bgA == 10 && M.bgB < 140) {
            M.bgB++;
            if (M.bgB > 30 && M.bgB % 20 == 0) {                                                        /* 14216 */
                M.bgB++; laser_spawn((float)(10 * (M.bgB - 1) + 12000), G.camy - 150, 210); snd_play_orig(408, 220, 128);
                if (G.cam.Z < 2) G.cam.Z = 2;
            }
        }
    } else if (M.act == 3) {
        if (M.bgA == 0 && X > 6200) { snd_play_orig(294, 220, 128); G.cam.Z = 10; M.bgA = 1; bg_show(b, 1); }
        if (M.bgA == 1) { b->x += 20; M.bgB++; if (M.bgB > 100) bg_show(b, 0); }
        if (X > 6250 && Y > 900 && M.bgA < 10) { M.bgA = 10; M.bgB = 0; snd_play_orig(342, 220, 128); bg_show(b, 0); }
        if (X > 8000 && M.bgA < 11 && M.bgB > 0) { M.bgA = 11; M.bgB = 0; snd_play_orig(342, 220, 128); }
        if (M.bgA >= 10 && M.bgB < 110) {
            M.bgB++;
            if (M.bgB > 30 && M.bgB % 20 == 0) {                                                        /* 14314 / 14315 (INFERRED: 14216's rule) */
                M.bgB++; laser_spawn((float)(10 * (M.bgB - 1) + (M.bgA == 10 ? 6000 : 8000)), G.camy - 150, 210); snd_play_orig(408, 220, 128);
                if (G.cam.Z < 2) G.cam.Z = 2;
            }
        }
    }
}

/* ======================================================================== Absolution */
static void kill_rig(void) { for (int k = 0; k < NP; k++) { obj_t *o = part(k); if (o) o->alive = 0; } }
static void spawn_rig(void) {
    for (int k = 0; k < NP; k++) {
        obj_t *o = bgb_spawn(PD[k].type, PD[k].row, -1000, -1000, PD[k].layer);
        M.pi[k] = o ? (short)(o - G.o) : -1;
        if (o) { o->hidden = 1; o->hp = 1; }
    }
}
static int hatch_n(void) { return bgb_count(OT_BGB_HATCH); }

void bgm_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&M, sizeof M); }
    memset(&M, 0, sizeof M);
    for (int k = 0; k < NP; k++) M.pi[k] = -1;
    M.act = level_cur() ? level_cur()->act : 0;
    obj_t *b = bgmech();
    if (b) { b->layer = -1; bgb_set(b, "bgm_bg"); M.bgy0 = b->y; bg_show(b, 0); }  /* hidden until a set piece shows it (RV m1_l f100) */
    for (int k = 0; k < hatch_n() && k < 4; k++) { obj_t *h = bgb_get(OT_BGB_HATCH, k); if (h) { M.hatch0[k] = (int)h->y; bgb_set(h, "bgb_hatch"); } }
    for (int k = 0; k < bgb_count(OT_BGB_MWARN2); k++) bgb_set(bgb_get(OT_BGB_MWARN2, k), "bgm_warn2");
    M.rig = ens_anim("bgm_torso") >= 0;
    if (!M.rig) return;
    if (G.at_g4 >= OPEN_G4) { M.phase = 2; return; }                         /* 14610: a restart past the fight: no mech */
    spawn_rig();
    /* 14609: wings B = 3.. (their index), torso D = E = Y, C = X, F = -1, flag 4; weak spot flag 16 (no fly-away) */
    M.C = -234; M.D = M.E = 1763; M.F = -1; M.tfl = TFL_AWAY; M.H = 0;      /* RV m2_l f0: the template at (-234, 1763) */
    M.aB = 1; M.aC = 1; M.aE = 0; M.aD = 0;                                  /* RV m2_l f0: anim A 0 B 1 C 1 */
    M.wE = 16;
    obj_t *w = part(P_WEAK); if (w) { w->A = 50; w->fl |= F_NOFLY; }
}

static void land(void);
static void mech_end(void);

/* the pose (14468-14480): per part a base angle, plus the stand (C), fly (D) and jump (E) weights' terms; B = 1 mirrors the angles */
static void rig_pose(void) {
    obj_t *t = part(P_TORSO); if (!t) return;
    double A = M.aA, B = M.aB, Wc = M.aC, Wd = M.aD, We = M.aE;
    int hi = A > 180;                                                        /* 14475 / 14476: the jump's two amplitude sets */
    double a1 = hi ? 40 : 8, a2 = hi ? -60 : -12, tj = hi ? 128 : 50;
    double s60 = sind(A + 60 * B);
    double ang[NP]; memset(ang, 0, sizeof ang);
    double dy_head = 4 * sind(A - 60 * B) * (Wc + We), dy_sh = 4 * s60 * (Wc + We), dy_abd = 4 * sind(A + 120 * B) * (Wc + We);
    double dy_t = 8 * sind(A) * Wc + tj * sind(A) * We + 8 * sind(A) * Wd;
    ang[P_FARM]  = (8 * s60 + 323) * Wc + (a1 * s60 + 323) * We + 353 * Wd;
    ang[P_BARM]  = (-12 * s60 + 230) * Wc + (a2 * s60 + 230) * We + 335 * Wd;
    ang[P_FLEG2] = (8 * s60 + 56) * Wc + (a1 * s60 + 56) * We + 194 * Wd;
    ang[P_FKNEE] = (4 * s60 - 15) * (Wc + We) + 75 * Wd;
    ang[P_FLEG]  = 300 * (Wc + We) + 15 * Wd;
    ang[P_FCLAW] = ang[P_BCLAW] = 135 * Wd;
    ang[P_FFOOT] = 90 * Wd;
    for (int k = 0; k < 4; k++) {                                            /* wings: base -15 B (B = 3..6), the flap */
        double wb = 3 + k, base = -15 * wb, fl = 4 * sind(A + wb * 15 * B) * (Wc + We) + (15 * sind(15 * A - 15 * wb) + 15) * Wd;
        ang[P_BW0 + k] = base + fl; ang[P_FW0 + k] = base + fl;
    }
    double sgn = B > 0 ? -1 : 1; int flip = B < 0;                           /* 14477: B 1 -> every A = -A (direction 0); B -1: direction 16 */
    /* positions: torso (C, D + its term); shoulders / abdomen at fixed offsets (14470 / 14471); the rest at their parents' action points */
    t->x = (float)(int)M.C; t->y = (float)(int)(M.D + dy_t); t->flip = flip; t->ang = 0;
    obj_t *o;
    for (int k = 0; k < NP; k++) if ((o = part(k)) && k != P_TURRET && k != P_WEAK && k != P_CHARGE && k != P_TORSO) { o->flip = flip; o->ang = (float)sbb_fmod360(sgn * ang[k]); }
    double sx = -B;                                                          /* B -1: offsets as listed (X - 20 / + 86 / + 74) */
    if ((o = part(P_BSH))) { o->x = t->x - (float)(20 * sx); o->y = (float)(int)(t->y + 17 + dy_sh); }
    if ((o = part(P_FSH))) { o->x = t->x + (float)(86 * sx); o->y = (float)(int)(t->y + 17 + dy_sh); }
    if ((o = part(P_ABD))) { o->x = t->x + (float)(74 * sx); o->y = (float)(int)(t->y - 10 + dy_abd); }
    float ax, ay;
    obj_t *h = part(P_HEAD);
    if (h) { bgb_act_point(t, &ax, &ay); h->x = ax; h->y = (float)(int)(ay + dy_head); }
    obj_t *fs = part(P_FSH), *bs = part(P_BSH);
    if (fs) { const int ks[] = { P_AXLE, P_FW0, P_FW1, P_FW2, P_FW3, P_FARM }; for (int j = 0; j < 6; j++) if ((o = part(ks[j]))) { o->x = fs->x; o->y = fs->y; } }
    if (bs) { const int ks[] = { P_BW0, P_BW1, P_BW2, P_BW3, P_BARM }; for (int j = 0; j < 5; j++) if ((o = part(ks[j]))) { o->x = bs->x; o->y = bs->y; } }
    static const signed char CH[][2] = { { P_FCLAW, P_FARM }, { P_BCLAW, P_BARM }, { P_FLEG, P_ABD }, { P_FLEG2, P_FLEG }, { P_FKNEE, P_FLEG2 }, { P_FFOOT, P_FKNEE } };
    for (unsigned j = 0; j < sizeof CH / sizeof CH[0]; j++) {
        obj_t *c = part(CH[j][0]), *pa = part(CH[j][1]);
        if (c && pa) { bgb_act_point(pa, &ax, &ay); c->x = ax; c->y = ay; }
    }
    double cl = M.D + 56;                                                    /* 14481: claws y <= torso D + 56, the foot <= that - 8 */
    if ((o = part(P_FCLAW)) && o->y > cl) o->y = (float)cl;
    if ((o = part(P_BCLAW)) && o->y > cl) o->y = (float)cl;
    if ((o = part(P_FFOOT)) && o->y > cl - 8) o->y = (float)(cl - 8);
    /* head frame (14488 / 14489: C +- 0.5, force_frame), the weak spot on the head (14480: direction = head frame + (B - 1) * 8), charge */
    int hf = (int)M.hC;
    if (h) { if (!M.dead || M.wG == 0) { if (!bgb_is(h, "bgm_head10")) h->an.frame = hf >= 2 ? 1 : 0; } }   /* 2 images kept (VRAM): closed / open */
    obj_t *w = part(P_WEAK);
    if (w && h) { static const char *WR[4] = { "bgm_weak0", "bgm_weak1", "bgm_weak2", "bgm_weak3" }; bgb_set(w, WR[hf & 3]); w->x = h->x; w->y = h->y; w->flip = flip; }
    obj_t *c = part(P_CHARGE);
    if (c && h) { bgb_act_point(h, &ax, &ay); c->x = ax; c->y = ay; c->hidden = !(M.hC >= 3) || M.dead; }   /* 14490 / 14491 */
    obj_t *u = part(P_TURRET);                                               /* 14442: x = torso X - 40 B, y = torso Y + D, angle 270 - 60 cos B */
    if (u) { u->x = t->x - (float)(40 * B); u->y = t->y + (float)M.uD; u->ang = (float)(270 - 60 * cosd(M.uB)); u->hidden = M.uC == 0; }
}

/* the turret (14433-14442): out (D + 2 to 60), a barrage every 15 of B (B - 3 a frame from 540: 36 shots), back in (D - 3) */
static void turret(void) {
    obj_t *u = part(P_TURRET);
    if (!u) return;
    if (M.uC == 0 && M.F == 1 && M.H != 0 && M.uB > 0) { M.uC = 1; snd_play_orig(165, 220, 128); }          /* 14433 */
    else if (M.uC == 1) { M.uD += 2; if (M.uD >= 60) { M.uD = 60; M.uC = 2; } }                             /* 14434-14436 */
    else if (M.uC == 2) {
        if (fmod(M.uB, 15) == 0 && !M.dead) {                                                                /* 14437 */
            snd_play_orig(192, 220, 128);
            float sx, sy; bgb_act_point(u, &sx, &sy);
            double a = atan2(sy - u->y, u->x - sx) * SB_RAD; if (a < 0) a += 360;   /* direction toward the turret + 16: outward */
            int dir = ((int)(a / 11.25) + 16) & 31;
            obj_t *s = bgb_spawn(OT_BGB_PLASMA, "bgs_plasma", sx, sy, u->layer);
            if (s) { double sp = 100 / 8.0; s->vx = (float)(sp * cosd(dir * 11.25)); s->vy = (float)(-sp * sind(dir * 11.25)); s->dir = dir; }
        }
        if (M.uB > 1) M.uB = fmax(M.uB - 3, 1);                                                             /* 14438 */
        if (M.uB == 1 || M.dead) { M.uC = 3; snd_play_orig(295, 220, 128); }                                 /* 14439 / 14440 */
    } else if (M.uC == 3) { M.uD = fmax(M.uD - 3, 0); if (M.uD <= 0) { M.uC = 0; M.uB = 0; } }              /* 14441 / 14442 */
}

static void shockwaves(void) {                       /* 14457: loop_shockwave_62 x 6, each at the front claw, the back claw and the foot (RV m4_l: */
    static const int at[3] = { P_FCLAW, P_BCLAW, P_FFOOT };                          /* 18 waves at x torso + 155 / - 46 / + 284), y = E + 125 */
    for (int k = 0; k < 6; k++)
        for (int j = 0; j < 3; j++) {
            obj_t *c = part(at[j]); float ax = (float)M.C, ay;
            if (c) bgb_act_point(c, &ax, &ay);
            int left = k & 1;                                                 /* direction k * 16 (0 / 16 alternating) */
            obj_t *s = bgb_spawn(OT_BGB_M_SHOCK, left ? "shock_l" : "shock", ax, (float)(M.E + 125), 3);
            if (!s) return;
            double sp = ((k / 2) * 8 + 16) / 8.0;                             /* speed (k / 2) * 8 + 16: 2 / 3 / 4 px (RV m4_l) */
            s->vx = (float)(left ? -sp : sp); s->av[0] = 1 + 0.25 * (k / 2); s->av[1] = 30; s->bd = 10;   /* y scale, B 30, A 10 */
        }
}
static void shocks(player_t *p) {
    for (int k = 0; k < bgb_runtime_n(); k++) {
        obj_t *s = bgb_runtime(k);
        if (!s || s->type != OT_BGB_M_SHOCK) continue;
        s->av[1] += 1; s->x += s->vx;                                        /* 14482 */
        if (s->av[1] > 50) { s->alive = 0; continue; }                        /* 14484 */
        if (sbb_csens_hit(p, s)) hurt(p, s->bd, "shock");                          /* 14485: A (10) while B <= 50 */
    }
}

static void fight(player_t *p) {
    obj_t *t = part(P_TORSO), *w = part(P_WEAK), *h = part(P_HEAD), *b = bgmech();
    if (!t) return;
    double X = sbb_px(p);
    if (M.phase == 0) {                                                      /* 14419: g4 < 5, bg A 0, MV.X > 900 */
        if (G.at_g4 < OPEN_G4 && b && M.bgA == 0 && X > 900) {
            for (int k = 0; k < hatch_n() && k < 4; k++) { obj_t *d = bgb_get(OT_BGB_HATCH, k); if (d) d->y = HATCH_SHUT; }
            M.bgA = 1; M.phase = 1;
        } else return;
    }
    if (M.phase != 1) return;
    int upd = M.slow <= 0 || G.frame % 3 == 0; if (M.slow > 0 && upd) M.slow--;
    if (upd) M.aA = sbb_fmod360(M.aA + 3 * M.aB + 360);                       /* 14420 / 14421 */
    if (M.F == -1 && b) {                                                    /* 14422 / 14423: the intro crossing (bg x + 20) */
        bg_show(b, 1); b->x += 20;
        if (bg_C(b) > G.camx + 1426) {
            bg_show(b, 0); M.F = 1; M.G = 3; M.D -= 100; M.H = M.D; M.aC = 0; M.aD = 1; M.aB = -1; M.C = G.camx + 1426;
        }
    }
    if (M.F == 0) {                                                          /* 14424 */
        if (upd) { M.aC = fmin(M.aC + 0.02, 1); M.aE = fmax(M.aE - 0.02, 0); M.aD = fmax(M.aD - 0.02, 0); }
        if (M.aC >= 1 && M.aH < 3 && !M.dead) {
            if (!M.music) { M.music = 1; const char *m = level_music_slot(4); if (m) music_play(m); }   /* 14425: ML.Y 1 (RV m_boss6 at f471) */
            M.aC = 0; M.aE = 1; M.aA = M.aB * -90 + 90; M.F = 3; M.G = 0; M.I = 0;                        /* 14426 */
            M.hopen = 1; snd_play_orig(342, 220, 128); M.aH += 1; M.hD = 0;
        } else if (M.aC >= 1 && M.aH >= 3 && !M.dead) {                      /* 14427 */
            M.F = 1; M.H = 0; snd_play_orig(295, 220, 128); M.G = 0; M.ubit ^= 1; M.aH = M.ubit; M.uB = (1 - M.ubit) * 540;
        } else if (M.dead && M.wG > 45) { M.F = 1; M.H = 0; M.G = 0; snd_play_orig(295, 220, 128); }   /* 14501: flies away */
    }
    if (M.F == 1) {
        if (upd) { M.aC = fmax(M.aC - 0.02, 0); M.aD = fmin(M.aD + 0.02, 1); M.aE = fmax(M.aE - 0.02, 0); }
        double hov = M.E - 98;                                               /* RV m2_l: H 1665 (E 1763) */
        if (M.G == 0) {
            if (M.H == 0) {                                                  /* rise (INFERRED fit of RV m2_l f1012-f1060: D - 4 C a frame) */
                M.D = fmax(M.D - 4 * M.aC, hov);
                if (M.aC <= 0) { M.D = hov; M.H = hov; }
            } else if (M.uC != 0 || M.uB > 0) M.C += M.aB * 5;                /* 14444: drifting while the turret fires */
            else M.C += M.aB * 30;                                           /* 14445: the dash out */
            if (M.H != 0 && M.uC == 0 && M.uB <= 0 && (M.C < G.camx - 950 || M.C > G.camx + SCREEN_W + 800)) {   /* 14446 (RV exits) */
                if (M.dead) { mech_end(); return; }                          /* 14502 */
                M.G = 1; M.C = -800; M.D = M.E + 50; M.tfl |= TFL_AWAY;
                if (b) { b->x = (float)((M.aB < 0 ? -954 : 1403) + BG_COEF * G.camx); b->y = M.bgy0; bg_show(b, 1); }   /* RV m2_l f1111 / f2114 */
            }
        } else if (M.G == 1) {                                               /* 14447: the mech crosses the sky (bg x - 20 B) */
            if (b) {
                b->x += (float)(-20 * M.aB); b->flip = M.aB > 0;
                if ((M.aB < 0 && bg_C(b) > G.camx + 1426) || (M.aB > 0 && bg_C(b) < G.camx - 1017)) { M.G = 2; M.C = bg_C(b); bg_show(b, 0); }   /* 14448 (RV) */
            } else { M.G = 2; M.C = M.aB < 0 ? G.camx + 1426 : G.camx - 1017; }
        } else if (M.G == 2) {                                               /* the low pass (harmless: RV m3_l, RE 14458 needs anim D < 1) */
            M.C += M.aB * 30; M.D = M.E + 50;
            if (M.C < G.camx - 1200 || M.C > G.camx + SCREEN_W + 1200) { M.G = 3; M.aB = -M.aB; M.D = M.H = hov; }   /* RV f1318 / f2323 */
        } else {                                                             /* G 3: the high pass, landing near the player */
            M.C += M.aB * 30;
            if (M.D > M.E) M.D = M.H;                                        /* 14454 */
            if (fabs(M.C - X) < 300) { M.F = 2; M.G = 0; snd_play_orig(295, 220, 128); M.tfl &= ~TFL_AWAY; }   /* 14455 */
        }
    }
    if (M.F == 2 && upd) {                                                   /* 14456 */
        M.aC = fmin(M.aC + 0.04, 1); M.aD = fmax(M.aD - 0.04, 0);
        M.D += M.aC * 8; M.C += M.aB * 10;
        if (M.aD <= 0) land();                                               /* 14457 */
    }
    if (M.F == 3) {                                                          /* 14460-14466: the jump */
        int air = M.aA > 180;
        if (M.G > 3 && M.aE >= 1 && air && M.I == 0) M.I = fmax(fmin((M.aB * -250 + X - M.C) * 0.02 + p->vx, 7), -7);
        if (air) { M.C += M.I; M.hD += 1; }                                  /* 14461 / 14463 */
        M.G += 1; M.C = fmax(fmin(M.C, 3328), 384);                          /* 14462: C within J 384 .. K 3328 */
        if (M.hD > 10 && !M.dead && h) {                                     /* 14464: a laser from the head every 20 frames in the air */
            M.hD = -10; float ax, ay; bgb_act_point(h, &ax, &ay);
            laser_spawn(ax, ay, -120 * M.aB + 90); snd_play_orig(408, 220, 128);
        }
        if (M.G > 130) { M.F = 0; M.G = 0; M.I = 0; M.hopen = 0; snd_play_orig(407, 220, 128); if (G.cam.Z < 5) G.cam.Z = 5; }   /* 14465 / 14466 (RV: 131 frames) */
    }
    if (M.F != 1 && M.D > M.E) M.D = M.E;                                    /* 14467 */
    for (int k = 0; k < bgb_count(OT_BGB_MWARN2); k++) {                     /* 14449-14451: the low pass's warning column at the view edge */
        obj_t *wn = bgb_get(OT_BGB_MWARN2, k); if (!wn) continue;
        int show = M.F == 1 && M.G == 2 && h && !(h->x >= G.camx - 99 && h->x < G.camx + SCREEN_W + 100);
        wn->hidden = !show;
        if (show) wn->x = M.aB > 0 ? G.camx + 20 : G.camx + SCREEN_W - 20;
    }
    M.hC = M.hopen ? fmin(M.hC + 0.5, 3) : fmax(M.hC - 0.5, 0);              /* 14488 / 14489 */
    turret();
    rig_pose();
    for (int k = 0; k < NP; k++) { obj_t *o = part(k); if (o && k != P_TURRET && k != P_CHARGE) o->hidden = (M.F == 1 && M.G == 1) || (k == P_WEAK && M.wG > 0); }
    /* head contact (14458 / 14459): F != 1 or G 1, the turret idle, anim D < 1 -> 5 damage, pushed away (MV.B = B 15, X clamped) */
    if (h && !h->hidden && (M.F != 1 || M.G == 1) && M.uB <= 0 && M.aD < 1 && sbb_csens_hit(p, h) && hurt(p, 5, "head")) {
        p->vx = (float)(M.aB * 15); if (p->x < 560) p->x = 560; if (p->x > 3152) p->x = 3152;
    }
    /* the weak spot (14492-14501) */
    if (w) {
        if (w->c > 0 && M.wD <= 0 && M.wE > 0 && !M.dead) {   /* 14492 (14493: Torque, MV.A 5, needs 5 damage: no Torque here) */
            M.wD = 50; snd_play_orig(47, 220, 128); w->A = 50; M.wE -= 1;
        }
        if (M.wD > 1) { w->c = 3; w->inv = 1; M.wD--; }                      /* 14494 */
        else if (M.wD == 1) { w->c = 0; M.wD = 0; w->inv = 0; }              /* 14495 */
        if (M.wE <= 0 && !M.dead) {                                          /* 14496 */
            M.dead = 1; w->fl |= BGB_DEF; M.wD = 99999; music_fade(120, 0);
        }
        if (M.dead && M.wF <= 20 && G.frame % 7 == 0 && h) {                  /* 14497 */
            obj_effect(h->x - 50 + cross_rand(101), h->y - 50 + cross_rand(101), "explosion06"); bgb_explode_snd(); M.wF++;
        }
        if (M.dead && M.wF > 20 && M.wG == 0) {                              /* 14498 */
            if (h) bgb_set(h, "bgm_head10");
            snd_play_orig(99, 220, 128); obj_effect(w->x, w->y, "explosion01");
            int a = ens_anim("debris");
            for (int k = 0; k < 8 && a >= 0; k++) {
                obj_t *d = obj_alloc(); if (!d) break; memset(d, 0, sizeof *d);
                double an = cross_rand(360), sp = (80 + cross_rand(30)) / 8.0;
                d->type = OT_DEBRIS; d->alive = 1; d->x = w->x; d->y = w->y; d->layer = 3; anim_set(&d->an, &G.ens, a); d->an.frame = k % 5;
                d->vx = (float)(sp * cosd(an)); d->vy = (float)(-sp * sind(an));
            }
            M.wG = 1; snd_play_orig(343, 220, 128); M.uB = 0;
        }
        if (M.wG > 0 && M.wG < 45 && G.frame % 3 == 0) obj_effect(w->x - 20 + cross_rand(41), w->y - 20 + cross_rand(41), "dust");   /* 14499 */
        if (M.wG > 0) M.wG++;                                                /* 14500 */
    }
}

static void land(void) {                             /* 14457: F 0, anim C 0 E 1 A = B -90 + 90, sound 406, shockwaves, CAM.Z >= 5 */
    M.F = 0; M.aC = 0; M.aE = 1; M.aA = M.aB * -90 + 90;
    snd_play_orig(406, 220, 128); shockwaves(); if (G.cam.Z < 5) G.cam.Z = 5;
    M.slow = 18;                                     /* RV m2_l f388-f440: the controller's updates every third frame after a landing (INFERRED: GUIPause B 3) */
}
static void mech_end(void) {                         /* 14502: every part and the background mech destroyed, g4 5 + m_stage9b (the flow's part) */
    kill_rig();
    obj_t *b = bgmech(); if (b) b->alive = 0;
    M.phase = 2;
    bg_flow_opening_cleared();
}
static void hatches(void) {                          /* 14511: the background mech gone -> the hatch doors sink 8 a frame (back to their place) */
    if (M.phase != 2) return;
    for (int k = 0; k < hatch_n() && k < 4; k++) { obj_t *d = bgb_get(OT_BGB_HATCH, k); if (d && d->y < M.hatch0[k]) d->y = fminf(d->y + 8, (float)M.hatch0[k]); }
}

#ifdef FP_HOST_ENV
static void dbg(const player_t *p) {
    static int on_ = -1; if (on_ < 0) on_ = getenv("FP_DBG_BGM") != NULL;
    obj_t *w = part(P_WEAK);
    if (on_ && M.phase == 1) printf("bgm %d F %.0f G %.0f C %.0f D %.0f H %.0f aA %.0f aB %.0f aC %.2f aD %.2f aE %.2f aH %d hC %.1f uC %d uB %.0f wE %d wD %d dead %d wc %d px %.0f py %.0f\n",
        G.frame, M.F, M.G, M.C, M.D, M.H, M.aA, M.aB, M.aC, M.aD, M.aE, M.aH, M.hC, M.uC, M.uB, M.wE, M.wD, M.dead, w ? w->c : -1, sbb_px(p), sbb_py(p));
}
#else
static void dbg(const player_t *p) { (void)p; }
#endif

void bgm_frame(player_t *p) {
    if (M.rig && G.at_g4 < OPEN_G4 && bgmech())                              /* 14418: CAM.H = min(0, 1912 - g17 / 2 - CAM.B) while the mech lives */
        G.cam.H = fminf(0.f, 1912.f - 0.5f * SCREEN_H - G.cam.B);
    if (M.rig) { fight(p); hatches(); dbg(p); }
    else bg_setpiece(p);
    lasers(p);
    shocks(p);
}
int bgm_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_BGB_MECHBG || (o->type == OT_BGB_FX && o->av[7] == 1)) {   /* json layer 1 at 0.5 */
        if (o->hidden || !o->an.b) return 1;
        float sx = floorf(o->x - (float)trunc(BG_COEF * G.camx)), sy = floorf(o->y - (float)trunc(BG_COEF * G.camy));
        frame_draw(o->an.b, anim_frame(&o->an), sx, sy, o->flip, 0xffffffff);
        return 1;
    }
    if (o->type == OT_BGB_M_SHOCK) { sbb_draw_yscaled(o, o->av[0]); return 1; }
    if (o->type == OT_BGB_M_BWING || o->type == OT_BGB_M_FWING) { if (!o->hidden) sbb_draw(o, 0xa0ffffff); return 1; }   /* INFERRED: the wings' blend (RV pngs: see-through) */
    if (o->type >= OT_BGB_M_FIRST && o->type <= OT_BGB_M_LAST) { if (!o->hidden) sbb_draw(o, 0xffffffff); return 1; }
    return 0;
}
float bgm_hud_dy(void) { return 0.f; }
obj_t *bgm_bot_target(void) {
    if (!M.rig || M.phase != 1 || M.dead || (M.F != 0 && M.F != 3)) return NULL;
    obj_t *w = part(P_WEAK);
    return w && !w->hidden ? w : NULL;
}
