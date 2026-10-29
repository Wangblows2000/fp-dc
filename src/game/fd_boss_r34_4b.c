/* FD4b (json 73): the final arena. Spec: build/re/spec_fd_bosses_b.md part C (RE handle_frame_73_events 16811-17266, start events
 * 17468-17480; RV build/orig_traces/fd_bosses_b b1_* / b3_*). Labels: RE read from code, RV measured, INFERRED reasoned.
 *  - Intro (group 0x230fa, KingdomStone_3680 D / E): RE 16830 starts it when MV.Y <= 336 and g4 < 2: CTRL.M 1, CAM.M 3, ML.Y 1.
 *    D0 -> D1 once the player stands (16832), D1 the player walks right to Brevon.x - 192, D2 E counts: E 60 the mech group on (16843:
 *    the torso F 1 G 0 H 0: it rises at its placement left of the arena and dashes across it), E 90 Brevon anim 10, D3 Brevon rises
 *    8 px a frame (16846) and is gone once above the mech's head (16847: CTRL.M 0, CAM.M 0, the flash group 0x230f3 on).
 *    Brevon waits on the right tube at the cape (17472: anim 5 frame 52, direction 16).
 *  - The flash (0x230f3, 16869-16875): E 1 a fade card on layer 2 (sound 393), the room's back layers fade out (E < 70), E 70 the
 *    ParallaxSpaceBack backdrops shown, E > 135 off. Port: a white wash over the back layers (INFERRED); the space backdrops are not
 *    exported (VRAM, see the spec): the room stays.
 *  - The mech (groups 0x22af2 / 0x22af4, 17002-17068): BG4's Absolution (bg_boss_mech.c, events 14420-14501, similarity 0.7-0.94)
 *    with FD's numbers: two jumps per landing (17007 / 17008: H < 2), the turret's barrage after every flight (17009: B 540), the
 *    jump's aim 0.03 (17040), the arena J / K 1856 / 3263 (the boss camera limits; RV b1_0 torso J K L M 1856 3263 1876 3243), the
 *    weak spot's 10 hits (start events; RV E 10; BossHealthBar1 x 10 shows them: 16825). The background mech's sky crossing is
 *    timed, not drawn (VRAM, INFERRED: the BG4 RV timing). The turret fires plasma shots (17018; the aimed / acid volleys 17019 /
 *    17020 are not ported: INFERRED).
 *  - Mech defeat (17060): flag 6, ML.Y 0, checkpoint g4 2 (2496, 304); explosions, the head's anim 10 and debris (17062-17064); F 50
 *    (17065): it rises and drifts to the stone's x (17066-17068) while the cut scene group 0x230f4 runs (16876-16905): the music
 *    fades (5000 ms), Brevon leaps out of the mech (E 60, anim 7), the suit group on (E 180, sound 395), white bursts at E 200-375,
 *    D 2: E 100 the boss music, E > 100 the flash again and the suit fight.
 *  - The suit (0x230f5, 17099-17266; BrevonSuitHead_3777 A 30, P 7 lives (17244: every defeat of the head: A 10, C 60, P - 1;
 *    BossHealthBar2 x 14 shows P x 2: 16826), Q 4 / O the attack choice (17129-17137: O 3, 6, 10, 12 for the first four, then
 *    O + 1 + rand(4); O >= 12 -> L 8 and O 0; O % 3 = 1 -> L 4 N 6, O % 3 = 2 -> L 4 N 7)). Port: the choice is RE, the moves
 *    INFERRED (see suit_*). P 0 (17248): L 10, g4 3 (checkpoint 2496, 304), the music stops; L 10 (17203-17214): explosions, white
 *    bursts (M 40-240), small explosions, M > 360 the fade card (sound 76) and L 11; Classic (17218: g0 != 2): L 11 M 120 -> L 12;
 *    L 12 (17240-17243): M 40 a fade card, M > 120 Sign.B 179: the results (results_begin_boss), and when the tally is done
 *    (16812-16814) the next frame is index 8 -> the ending (results_request_end; results.c / ending.c).
 *  - Adventure only (g0 2, not ported): the dialogs, L 11 M 280 -> the Brevon scene (0x230fc) and fight (0x230f7, 17267-17437),
 *    Milla's capture. Restarts (start events): g4 2 -> the mech already beaten (17474), g4 3 -> the suit at L 11 M 100 (17475). */
#include "fd_boss_r34.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- the mech rig (OT_FDB_MECH sub = MP_*, json order of FDB_MP in tools/import_frame.py) */
enum { MP_BSH, MP_BCLAW, MP_BARM, MP_BWING, MP_FLEG, MP_ABD, MP_FLEG2, MP_FKNEE, MP_TORSO, MP_HEAD, MP_FWING, MP_FFOOT, MP_FSH, MP_FARM,
       MP_FCLAW, MP_AXLE, MP_CHARGE, MP_TURRET, MP_N };
static const char *const MP_ROW[MP_N] = { "fdk_bsh", "fdk_bclaw", "fdk_barm", "fdk_wing", "fdk_fleg", "fdk_abd", "fdk_fleg2", "fdk_fknee",
    "fdk_torso", "fdk_head", "fdk_wing", "fdk_ffoot", "fdk_fsh", "fdk_farm", "fdk_fclaw", "fdk_axle", "fdk_charge", "fdk_turret" };
/* ---- the suit (OT_FDB_SUIT_PART sub = SP_*, FDB_SP) */
enum { SP_BGUN, SP_BARM, SP_BLEG, SP_BKNEE, SP_BFOOT, SP_PELVIS, SP_FLEG, SP_FKNEE, SP_FFOOT, SP_TORSO, SP_FARM, SP_FGUN, SP_N };
static const char *const SP_ROW[SP_N] = { "fdu_backgun", "fdu_backarm", "fdu_backleg", "fdu_backknee", "fdu_backfoot", "fdu_pelvis",
    "fdu_frontleg", "fdu_frontknee", "fdu_frontfoot", "fdu_torso", "fdu_frontarm", "fdu_frontgun" };

#define VIEW_W      426                              /* the original's view (exit tests of the mech: RV) */
#define OPEN_G4     2                                /* 17060: the checkpoint after the mech */
#define SUIT_G4     3                                /* 17248: after the suit */
#define CP_X        2496
#define CP_Y        304
#define LASER_DMG   20                               /* 14226 (BG4 = FD's 17xxx counterpart) */
#define WEAK_HITS   10                               /* RV b1_0: E 10; BossHealthBar1 x 10 */
#define FD4B_MECH_MUSIC "bgm/m_boss5.adp"         /* RV b1_0 f 631 / b4_0: m_boss5 from the intro to the suit (levels.c's slot 4 says m_boss7 at g4 < 3) */
#define SUIT_LIVES  7                                /* RE BrevonSuitHead ctor P 7 (RV b1_0) */

enum { PH_OFF, PH_WAIT, PH_INTRO, PH_MECH, PH_MECHDEF, PH_SUIT, PH_SUITDEF, PH_BSCENE, PH_BREVON, PH_END, PH_DONE };
#define BREVON_FLOOR 320                             /* RV b4_0: Brevon's H (the floor) 320 */
#define BREVON_A    160                              /* RE BrevonHitbox_3809 ctor A 160 (RV) */

static struct {
    int on, ph, iD, iE, flashE, flash, results, ended, music;
    int cam_on, dir0; float camA, camB;
    float brx0, bry0;                                /* Brevon's place (the cape) */
    /* mech: torso C D E F G H I, anim A B C D E H, head hC / laser hD / open, turret B C D, weak spot D E F G */
    short mp[MP_N], bw[4], fw[4];
    double C, D, Em, F, Gm, H, I;
    double aA, aB, aC, aD, aE; int aH, slow;
    double hC; int hD, hopen;
    double uB, uD; int uC;
    int wD, wE, wF, wG, dead;
    double bgx; int bgv;                             /* the (undrawn) background mech's crossing */
    float J, K, L, M;
    int mdE;                                         /* 0x230f4 timer (the stone's E in the original) */
    /* suit */
    short sp[SP_N]; float spx[SP_N], spy[SP_N]; float hx0, hy0;
    double sx, sy, svx, svy; int sface, sL, sM, sN, sO, sQ, sP, sInv, sground;
    double sfloor, s7x; float px0[SP_N], py0[SP_N];
    /* Brevon: D state, E timer, I facing, J / K speed, M attack counter; the scene / finish timer (the stone's E), the fade, the bar */
    int bD, bE, bI, bM, bfin, sE, fadeW, barY; double bJ, bK;
} S;

static obj_t *stone(void) { return fdb_get(OT_FDB_STONE, 0); }
static obj_t *brevon(void) { return fdb_get(OT_FDB_BREVON, 0); }
static obj_t *mp(int k) { return fdb_slot(S.mp[k]); }
static obj_t *weak(void) { return fdb_get(OT_FDB_MWEAK, 0); }
static obj_t *shead(void) { return fdb_get(OT_FDB_SUIT_HEAD, 0); }
static obj_t *spart(int k) { return fdb_slot(S.sp[k]); }
static obj_t *bhit(void) { return fdb_get(OT_FDB_BHIT, 0); }
int fd4b_boss_state(void) { return S.on ? S.ph : -1; }

static int hurt(player_t *p, int n) { return sbb_hurt(p, n); }
static void walk(player_t *p, double b) { if (p->ground) { p->gsp = (float)b; if (b) p->face = b > 0 ? 1 : -1; } }

/* ======================================================================== spawn */
static void mech_hide(int h) {
    for (int k = 0; k < MP_N; k++) { obj_t *o = mp(k); if (o) o->hidden = h || (k == MP_TURRET && S.uC == 0) || (k == MP_CHARGE && !(S.hC >= 3)); }
    for (int k = 0; k < 4; k++) { obj_t *o = fdb_slot(S.bw[k]); if (o) o->hidden = h; o = fdb_slot(S.fw[k]); if (o) o->hidden = h; }
    obj_t *w = weak(); if (w) w->hidden = h || S.wG > 0;
}
static void suit_show(int on) {
    for (int k = 0; k < SP_N; k++) { obj_t *o = spart(k); if (o) o->hidden = !on; }
    obj_t *h = shead(); if (h) h->hidden = !on;
}
void f4b_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    for (int k = 0; k < MP_N; k++) S.mp[k] = -1;
    for (int k = 0; k < 4; k++) S.bw[k] = S.fw[k] = -1;
    for (int k = 0; k < SP_N; k++) S.sp[k] = -1;
    obj_t *s = stone(), *h = shead();
    /* the boss room's ComputerTerminal / 2 (5 placed; no FD4b event moves them: their anim 0 loops, session 26) */
    for (int i = 0; i < fdb_count(OT_FDB_TERM); i++) { obj_t *t = fdb_get(OT_FDB_TERM, i); if (t && t->par < 2) fdb_set(t, t->par == 1 ? "fdb_term1" : "fdb_term0"); }
    if (!s || !h || !fdb_count(OT_FDB_MECH)) return;
    S.on = 1; S.ph = PH_WAIT;
    fdb_set(s, "fdb_stone");
    obj_t *b = brevon();
    if (b) {                                                                    /* 17472: anim 5 frame 52, direction 16, at the cape */
        obj_t *cp = fdb_get(OT_FDB_CAPE, 0);
        if (cp) { b->x = cp->x; b->y = cp->y; }
        fdb_set(b, "fdb_brv5s"); b->flip = 1; S.brx0 = b->x; S.bry0 = b->y;
    }
    /* the mech: one placement per part, four wings each side (json order) */
    int nb = 0, nf = 0;
    for (int i = 0; i < fdb_count(OT_FDB_MECH); i++) {
        obj_t *o = fdb_get(OT_FDB_MECH, i); if (!o || o->par >= MP_N) continue;
        fdb_set(o, MP_ROW[o->par]); o->hidden = 1;
        if (o->par == MP_BWING && nb < 4) S.bw[nb++] = (short)(o - G.o);
        else if (o->par == MP_FWING && nf < 4) S.fw[nf++] = (short)(o - G.o);
        else S.mp[o->par] = (short)(o - G.o);
    }
    obj_t *t = mp(MP_TORSO), *w = weak();
    S.J = fdb_marker(OT_CAMLIM_L, 1); S.K = fdb_marker(OT_CAMLIM_R, 1);
    if (S.J < -1e8f) S.J = 1856;
    if (S.K < -1e8f) S.K = 3263;
    S.L = S.J + 20; S.M = S.K - 20;                                             /* RV b1_0: torso L / M */
    if (t) { S.C = t->x; S.D = S.Em = t->y; }
    S.F = -2; S.aB = 1; S.aC = 1; S.wE = WEAK_HITS;
    if (w) { fdb_set(w, "fdk_weak0"); w->A = 50; w->fl |= F_NOFLY; w->hidden = 1; }
    for (int k = 0; k < fdb_count(OT_FDB_MWARN2); k++) { obj_t *o = fdb_get(OT_FDB_MWARN2, k); if (o) { fdb_set(o, "fdk_warn2"); o->hidden = 1; } }
    /* the suit: the parts keep their placement offsets from the pelvis; the head (an off-map template) sits on the torso */
    obj_t *pv = NULL;
    for (int i = 0; i < fdb_count(OT_FDB_SUIT_PART); i++) { obj_t *o = fdb_get(OT_FDB_SUIT_PART, i); if (o && o->par == SP_PELVIS) pv = o; }
    if (pv) { S.sx = pv->x; S.sy = pv->y; }
    S.sfloor = S.sy; S.sground = 1;
    for (int i = 0; i < fdb_count(OT_FDB_SUIT_PART); i++) {
        obj_t *o = fdb_get(OT_FDB_SUIT_PART, i); if (!o || o->par >= SP_N) continue;
        fdb_set(o, SP_ROW[o->par]); S.sp[o->par] = (short)(o - G.o); S.spx[o->par] = o->x - (float)S.sx; S.spy[o->par] = o->y - (float)S.sy;
        S.px0[o->par] = o->x; S.py0[o->par] = o->y;
    }
    fdb_set(h, "fdu_head"); h->A = 30; h->fl |= F_NOFLY; h->hidden = 1;        /* ctor A 30, P 7, Q 4 (RV b1_0); an off-map template until the suit */
    obj_t *hb = bhit(); if (hb) { hb->A = BREVON_A; hb->fl |= F_NOFLY; hb->x = -9999; }
    for (int k = 0; k < fdb_count(OT_FDB_XHAIR); k++) { obj_t *o = fdb_get(OT_FDB_XHAIR, k); if (o) { fdb_set(o, o->par ? "fdu_xh7" : "fdu_xh6"); o->hidden = 1; } }
    S.sP = SUIT_LIVES; S.sQ = 4; S.sface = 1;
#ifdef FP_HOST_ENV
    if (getenv("FP_F4B_HITS")) S.wE = atoi(getenv("FP_F4B_HITS"));             /* host tests: fewer weak-spot hits / suit lives */
    if (getenv("FP_F4B_LIVES")) S.sP = atoi(getenv("FP_F4B_LIVES"));
#endif
    for (int k = 0; k < fdb_count(OT_FDB_SUIT_WALL); k++) { obj_t *o = fdb_get(OT_FDB_SUIT_WALL, k); if (o) o->hidden = 1; }
    for (int k = 0; k < fdb_count(OT_FDB_HBAR); k++) { obj_t *o = fdb_get(OT_FDB_HBAR, k); if (o) { fdb_set(o, o->par ? "fdu_hb2" : "fdu_hb1"); o->hidden = 0; } }
    /* restarts (start events 17474 / 17475) */
#ifdef FP_HOST_ENV
    if (getenv("FP_G4")) G.at_g4 = atoi(getenv("FP_G4"));                      /* host tests: a restart at that checkpoint */
#endif
    if (G.at_g4 >= SUIT_G4) {
        S.ph = PH_SUITDEF; S.sL = 11; S.sM = 100; S.sP = 0; S.dead = 1; S.wE = 0;
        if (b) b->hidden = 1;
        fdb_cam_lock(S.J, S.K, fdb_marker(OT_CAMLIM_T, 0), fdb_marker(OT_CAMLIM_B, 0));
    } else if (G.at_g4 == OPEN_G4) {
        S.ph = PH_MECH; S.dead = 1; S.wE = 0; S.wF = 21; S.wG = 40; S.F = 0; S.C = s->x; S.aB = -1; S.aC = 1;
        obj_t *hd = mp(MP_HEAD); if (hd) fdb_set(hd, "fdk_head10");
        if (b) b->hidden = 1;
        S.music = 1; music_play(FD4B_MECH_MUSIC);
        fdb_cam_lock(S.J, S.K, fdb_marker(OT_CAMLIM_T, 0), fdb_marker(OT_CAMLIM_B, 0));
    }
}

/* ======================================================================== the flash (0x230f3) */
static void flash_start(void) { S.flash = 1; S.flashE = 0; }
static void flash_frame(void) {
    if (!S.flash) return;
    S.flashE++;
    if (S.flashE == 1) snd_play_orig(393, 220, 128);                           /* 16871: the fade card (media 3144) */
    if (S.flashE > 135) S.flash = 0;                                            /* 16875 */
}
static int flash_alpha(void) {                                                  /* INFERRED: up to E 70, back down by E 135 */
    if (!S.flash) return 0;
    return S.flashE < 70 ? S.flashE * 255 / 70 : (135 - S.flashE) * 255 / 65;
}

/* ======================================================================== the mech (bg_boss_mech.c's Absolution with FD's numbers) */
static void laser_spawn(float x, float y, double ang) {
    obj_t *l = fdb_spawn(FDB_K_LASER, "fdk_laser", x, y, 3);
    if (!l) return;
    l->av[0] = 50; l->av[1] = 8; l->av[2] = x; l->av[3] = y; l->av[4] = ang; l->scale = 0.5f; l->ang = (float)ang;
    obj_t *w = fdb_spawn(FDB_K_WARN, "fdk_warn", x, y, 3);
    if (w) { w->av[0] = (double)(l - G.o); w->hidden = 1; }
}
static void projectiles(player_t *p) {
    float fl = (float)(S.Em + 80);                                              /* 14507: torso E + 80 */
    for (int k = 0; k < fdb_rt_n(); k++) {
        obj_t *l = fdb_rt(k);
        if (!l) continue;
        if (l->st == FDB_K_WARN) {
            obj_t *o = fdb_slot((int)l->av[0]);
            if (!o || o->type != OT_FDB_RT || o->st != FDB_K_LASER) { l->alive = 0; continue; }
            int out = o->x < G.camx || o->x > G.camx + SCREEN_W;
            l->hidden = !out; l->x = fminf(fmaxf(o->x, G.camx + 20), G.camx + SCREEN_W - 20); l->y = G.camy + 20;
        } else if (l->st == FDB_K_LASER) {
            double *a = l->av;                                                  /* 14222 */
            a[2] += sbb_cos(a[4]) * a[1]; a[3] -= sbb_sin(a[4]) * a[1];
            l->x = (float)(int)a[2]; l->y = (float)(int)a[3];
            a[0] = fmin(a[0] + 3, 100); l->scale = (float)(a[0] * 0.01); l->ang = (float)a[4];
            if (l->y > fl) { if (a[4] < 0) a[4] += 2; else if (a[4] > 180) a[4] -= 2; }
            if (l->x < G.camx - 300 || l->x > G.camx + SCREEN_W + 300) { l->alive = 0; continue; }
            if (sbb_csens_hit(p, l) && hurt(p, LASER_DMG)) { p->vx += (float)(sbb_cos(a[4]) * a[1]); p->vy -= (float)(sbb_sin(a[4]) * a[1]); }
        } else if (l->st == FDB_K_WAVE) {                                       /* shockwaves (14482-14485) */
            l->av[1] += 1; l->x += l->vx;
            if (l->av[1] > 50) { l->alive = 0; continue; }
            if (sbb_csens_hit(p, l)) hurt(p, 10);
        } else if (l->st == FDB_K_BOUNCE) {                                     /* plasma shots (17018; INFERRED 10 damage) */
            l->fx += l->vx; l->fy += l->vy; l->x = floorf(l->fx); l->y = floorf(l->fy);
            if (!obj_near_view(l, 64)) { l->alive = 0; continue; }
            if (sbb_csens_hit(p, l) && hurt(p, 10)) { obj_effect(l->x, l->y, "explosion02"); l->alive = 0; }
        } else if (l->st == FDB_K_MINE) {                                       /* debris (the mech's head) */
            l->vy += 0.3f; l->x += l->vx; l->y += l->vy; if (!obj_near_view(l, 32)) l->alive = 0;
        }
    }
}
static void shockwaves(void) {                                                  /* 14457: 6 x (front claw, back claw, foot) at E + 125 */
    static const int at[3] = { MP_FCLAW, MP_BCLAW, MP_FFOOT };
    for (int k = 0; k < 6; k++)
        for (int j = 0; j < 3; j++) {
            obj_t *c = mp(at[j]); float ax = (float)S.C, ay;
            if (c) fdb_act_point(c, &ax, &ay);
            int left = k & 1;
            obj_t *s = fdb_spawn(FDB_K_WAVE, left ? "shock_l" : "shock", ax, (float)(S.Em + 125), 3);
            if (!s) return;
            double sp = ((k / 2) * 8 + 16) / 8.0;
            s->vx = (float)(left ? -sp : sp); s->av[0] = 1 + 0.25 * (k / 2); s->av[1] = 30;
        }
}
static void land(void) {
    S.F = 0; S.aC = 0; S.aE = 1; S.aA = S.aB * -90 + 90;
    snd_play_orig(406, 220, 128); shockwaves(); if (G.cam.Z < 5) G.cam.Z = 5;
    S.slow = 18;
}
static void rig_pose(void) {
    obj_t *t = mp(MP_TORSO); if (!t) return;
    double A = S.aA, B = S.aB, Wc = S.aC, Wd = S.aD, We = S.aE;
    int hi = A > 180;
    double a1 = hi ? 40 : 8, a2 = hi ? -60 : -12, tj = hi ? 128 : 50;
    double s60 = sbb_sin(A + 60 * B);
    double ang[MP_N]; memset(ang, 0, sizeof ang);
    double dy_head = 4 * sbb_sin(A - 60 * B) * (Wc + We), dy_sh = 4 * s60 * (Wc + We), dy_abd = 4 * sbb_sin(A + 120 * B) * (Wc + We);
    double dy_t = 8 * sbb_sin(A) * Wc + tj * sbb_sin(A) * We + 8 * sbb_sin(A) * Wd;
    ang[MP_FARM]  = (8 * s60 + 323) * Wc + (a1 * s60 + 323) * We + 353 * Wd;
    ang[MP_BARM]  = (-12 * s60 + 230) * Wc + (a2 * s60 + 230) * We + 335 * Wd;
    ang[MP_FLEG2] = (8 * s60 + 56) * Wc + (a1 * s60 + 56) * We + 194 * Wd;
    ang[MP_FKNEE] = (4 * s60 - 15) * (Wc + We) + 75 * Wd;
    ang[MP_FLEG]  = 300 * (Wc + We) + 15 * Wd;
    ang[MP_FCLAW] = ang[MP_BCLAW] = 135 * Wd;
    ang[MP_FFOOT] = 90 * Wd;
    double sgn = B > 0 ? -1 : 1; int flip = B < 0;
    t->x = (float)(int)S.C; t->y = (float)(int)(S.D + dy_t); t->flip = flip; t->ang = 0;
    obj_t *o;
    for (int k = 0; k < MP_N; k++) if ((o = mp(k)) && k != MP_TURRET && k != MP_CHARGE && k != MP_TORSO) { o->flip = flip; o->ang = (float)sbb_fmod360(sgn * ang[k]); }
    for (int k = 0; k < 4; k++) {                                               /* wings: base -15 B (B = 3..6), the flap */
        double wb = 3 + k, base = -15 * wb, fl = 4 * sbb_sin(A + wb * 15 * B) * (Wc + We) + (15 * sbb_sin(15 * A - 15 * wb) + 15) * Wd;
        float wa = (float)sbb_fmod360(sgn * (base + fl));
        if ((o = fdb_slot(S.bw[k]))) { o->flip = flip; o->ang = wa; }
        if ((o = fdb_slot(S.fw[k]))) { o->flip = flip; o->ang = wa; }
    }
    double sx = -B;
    if ((o = mp(MP_BSH))) { o->x = t->x - (float)(20 * sx); o->y = (float)(int)(t->y + 17 + dy_sh); }
    if ((o = mp(MP_FSH))) { o->x = t->x + (float)(86 * sx); o->y = (float)(int)(t->y + 17 + dy_sh); }
    if ((o = mp(MP_ABD))) { o->x = t->x + (float)(74 * sx); o->y = (float)(int)(t->y - 10 + dy_abd); }
    float ax, ay;
    obj_t *h = mp(MP_HEAD);
    if (h) { fdb_act_point(t, &ax, &ay); h->x = ax; h->y = (float)(int)(ay + dy_head); }
    obj_t *fs = mp(MP_FSH), *bs = mp(MP_BSH);
    if (fs) {
        if ((o = mp(MP_AXLE))) { o->x = fs->x; o->y = fs->y; }
        if ((o = mp(MP_FARM))) { o->x = fs->x; o->y = fs->y; }
        for (int k = 0; k < 4; k++) if ((o = fdb_slot(S.fw[k]))) { o->x = fs->x; o->y = fs->y; }
    }
    if (bs) {
        if ((o = mp(MP_BARM))) { o->x = bs->x; o->y = bs->y; }
        for (int k = 0; k < 4; k++) if ((o = fdb_slot(S.bw[k]))) { o->x = bs->x; o->y = bs->y; }
    }
    static const signed char CH[][2] = { { MP_FCLAW, MP_FARM }, { MP_BCLAW, MP_BARM }, { MP_FLEG, MP_ABD }, { MP_FLEG2, MP_FLEG }, { MP_FKNEE, MP_FLEG2 }, { MP_FFOOT, MP_FKNEE } };
    for (unsigned j = 0; j < sizeof CH / sizeof CH[0]; j++) {
        obj_t *c = mp(CH[j][0]), *pa = mp(CH[j][1]);
        if (c && pa) { fdb_act_point(pa, &ax, &ay); c->x = ax; c->y = ay; }
    }
    double cl = S.D + 56;                                                       /* 14481 (17047: 56) */
    if ((o = mp(MP_FCLAW)) && o->y > cl) o->y = (float)cl;
    if ((o = mp(MP_BCLAW)) && o->y > cl) o->y = (float)cl;
    if ((o = mp(MP_FFOOT)) && o->y > cl - 8) o->y = (float)(cl - 8);
    int hf = (int)S.hC;
    if (h && !fdb_is(h, "fdk_head10")) h->an.frame = hf >= 2 ? 1 : 0;           /* 2 images kept (VRAM): closed / open */
    obj_t *w = weak();
    if (w && h) { static const char *WR[4] = { "fdk_weak0", "fdk_weak1", "fdk_weak2", "fdk_weak3" }; fdb_set(w, WR[hf & 3]); w->x = h->x; w->y = h->y; w->flip = flip; }
    obj_t *c = mp(MP_CHARGE);
    if (c && h) { fdb_act_point(h, &ax, &ay); c->x = ax; c->y = ay; }
    obj_t *u = mp(MP_TURRET);
    if (u) { u->x = t->x - (float)(40 * B); u->y = t->y + (float)S.uD; u->ang = (float)(270 - 60 * sbb_cos(S.uB)); }
}
static void turret(void) {                                                      /* 17014-17025 (BG 14433-14442) */
    obj_t *u = mp(MP_TURRET);
    if (!u) return;
    if (S.uC == 0 && S.F == 1 && S.H != 0 && S.uB > 0) { S.uC = 1; snd_play_orig(165, 220, 128); }
    else if (S.uC == 1) { S.uD += 2; if (S.uD >= 60) { S.uD = 60; S.uC = 2; } }
    else if (S.uC == 2) {
        if (fmod(S.uB, 15) == 0 && !S.dead) {                                   /* 17018: B % 15 -> a plasma shot (sound 192) */
            snd_play_orig(192, 220, 128);
            float sx, sy; fdb_act_point(u, &sx, &sy);
            double a = atan2(sy - u->y, u->x - sx) * SB_RAD; if (a < 0) a += 360;
            int dir = ((int)(a / 11.25) + 16) & 31;
            obj_t *s = fdb_spawn(FDB_K_BOUNCE, "fdk_plasma", sx, sy, 3);
            if (s) { double sp = 100 / 8.0; s->vx = (float)(sp * sbb_cos(dir * 11.25)); s->vy = (float)(-sp * sbb_sin(dir * 11.25)); s->fx = sx; s->fy = sy; }
        }
        if (S.uB > 1) S.uB = fmax(S.uB - 3, 1);
        if (S.uB == 1 || S.dead) { S.uC = 3; snd_play_orig(295, 220, 128); }
    } else if (S.uC == 3) { S.uD = fmax(S.uD - 3, 0); if (S.uD <= 0) { S.uC = 0; S.uB = 0; } }
}
static void mech_fight(player_t *p) {
    obj_t *w = weak(), *h = mp(MP_HEAD);
    double X = sbb_px(p);
    int upd = S.slow <= 0 || G.frame % 3 == 0; if (S.slow > 0 && upd) S.slow--;
    if (upd) S.aA = sbb_fmod360(S.aA + 3 * S.aB + 360);
    if (S.F == 0 && S.ph == PH_MECH) {                                          /* 17006-17009 */
        if (upd) { S.aC = fmin(S.aC + 0.02, 1); S.aE = fmax(S.aE - 0.02, 0); S.aD = fmax(S.aD - 0.02, 0); }
        if (S.aC >= 1 && S.aH < 2 && !S.dead) {
            S.aC = 0; S.aE = 1; S.aA = S.aB * -90 + 90; S.F = 3; S.Gm = 0; S.I = 0;
            S.hopen = 1; snd_play_orig(342, 220, 128); S.aH += 1; S.hD = 0;
        } else if (S.aC >= 1 && S.aH >= 2 && !S.dead) {                         /* 17009: B 540 */
            S.F = 1; S.H = 0; snd_play_orig(295, 220, 128); S.Gm = 0; S.aH = 0; S.uB = 540;
        }
    }
    if (S.F == 1) {
        if (upd) { S.aC = fmax(S.aC - 0.02, 0); S.aD = fmin(S.aD + 0.02, 1); S.aE = fmax(S.aE - 0.02, 0); }
        double hov = S.Em - 98;
        if (S.Gm == 0) {
            if (S.H == 0) { S.D = fmax(S.D - 4 * S.aC, hov); if (S.aC <= 0) { S.D = hov; S.H = hov; } }
            else if (S.uC != 0 || S.uB > 0) S.C += S.aB * 5;
            else S.C += S.aB * 30;
            if (S.H != 0 && S.uC == 0 && S.uB <= 0 && (S.C < G.camx - 950 || S.C > G.camx + VIEW_W + 800)) {
                S.Gm = 1; S.C = -800; S.D = S.Em + 50;
                S.bgx = S.aB < 0 ? -954 : 1403;                                 /* the background mech's crossing (BG4 RV; not drawn) */
            }
        } else if (S.Gm == 1) {
            S.bgx += -20 * S.aB;
            if ((S.aB < 0 && S.bgx > 1426) || (S.aB > 0 && S.bgx < -1017)) { S.Gm = 2; S.C = G.camx + S.bgx; }
        } else if (S.Gm == 2) {
            S.C += S.aB * 30; S.D = S.Em + 50;
            if (S.C < G.camx - 1200 || S.C > G.camx + VIEW_W + 1200) { S.Gm = 3; S.aB = -S.aB; S.D = S.H = hov; }
        } else {
            S.C += S.aB * 30;
            if (S.D > S.Em) S.D = S.H;
            if (fabs(S.C - X) < 300) { S.F = 2; S.Gm = 0; snd_play_orig(295, 220, 128); }
        }
    }
    if (S.F == 2 && upd) {
        S.aC = fmin(S.aC + 0.04, 1); S.aD = fmax(S.aD - 0.04, 0);
        S.D += S.aC * 8; S.C += S.aB * 10;
        if (S.aD <= 0) land();
    }
    if (S.F == 3) {                                                             /* 17040-17045: the jump */
        int air = S.aA > 180;
        if (S.Gm > 3 && S.aE >= 1 && air && S.I == 0) S.I = fmax(fmin((S.aB * -250 + X - S.C) * 0.03 + p->vx, 7), -7);
        if (air) { S.C += S.I; S.hD += 1; }
        S.Gm += 1; S.C = fmax(fmin(S.C, S.K), S.J);
        if (S.hD > 10 && !S.dead && h) {
            S.hD = -10; float ax, ay; fdb_act_point(h, &ax, &ay);
            laser_spawn(ax, ay, -120 * S.aB + 90); snd_play_orig(408, 220, 128);
        }
        if (S.Gm > 130) { S.F = 0; S.Gm = 0; S.I = 0; S.hopen = 0; snd_play_orig(407, 220, 128); if (G.cam.Z < 5) G.cam.Z = 5; }
    }
    if (S.F == 50) {                                                            /* 17066-17068: rises, drifts to the stone */
        S.aC = fmax(S.aC - 0.02, 0); S.aD = fmin(S.aD + 0.02, 1); S.D += S.aC * -4;
        obj_t *st = stone(); if (st) { if (S.C <= st->x) S.C = fmin(S.C + 5, st->x); else S.C = fmax(S.C - 5, st->x); }
    }
    if (S.F != 1 && S.F != 50 && S.D > S.Em) S.D = S.Em;
    for (int k = 0; k < fdb_count(OT_FDB_MWARN2); k++) {                        /* 17030-17032: the low pass's warning column */
        obj_t *wn = fdb_get(OT_FDB_MWARN2, k); if (!wn) continue;
        int show = S.F == 1 && S.Gm == 2 && h && !(h->x >= G.camx - 99 && h->x < G.camx + SCREEN_W + 100);
        wn->hidden = !show;
        if (show) wn->x = S.aB > 0 ? G.camx + 20 : G.camx + SCREEN_W - 20;
    }
    S.hC = S.hopen ? fmin(S.hC + 0.5, 3) : fmax(S.hC - 0.5, 0);
    turret();
    rig_pose();
    mech_hide(S.F == 1 && S.Gm == 1);
    if (h && !h->hidden && (S.F != 1 || S.Gm == 1) && S.uB <= 0 && S.aD < 1 && !S.dead && sbb_csens_hit(p, h) && hurt(p, 5)) {   /* 17037 / 17038 */
        p->vx = (float)(S.aB * 15); if (p->x < S.L) p->x = S.L; if (p->x > S.M) p->x = S.M;
    }
    if (!w) return;
    if (w->c > 0 && S.wD <= 0 && S.wE > 0 && !S.dead) { S.wD = 50; snd_play_orig(47, 220, 128); w->A = 50; S.wE -= 1; }   /* 17057-17059 */
    if (S.wD > 1) { w->c = 3; w->inv = 1; S.wD--; } else if (S.wD == 1) { w->c = 0; S.wD = 0; w->inv = 0; }
    if (S.wE <= 0 && !S.dead) {                                                 /* 17060 */
        S.dead = 1; w->fl |= FDB_DEF; S.wD = 99999; music_fade(120, 0); level_checkpoint(OPEN_G4, CP_X, CP_Y);
    }
    if (S.dead && S.wF <= 20 && G.frame % 7 == 0 && h) {                         /* 17062 */
        obj_effect(h->x - 50 + cross_rand(101), h->y - 50 + cross_rand(101), "explosion06"); fdb_explode_snd(); S.wF++;
    }
    if (S.dead && S.wF > 20 && S.wG == 0) {                                     /* 17063 */
        if (h) fdb_set(h, "fdk_head10");
        snd_play_orig(99, 220, 128); obj_effect(w->x, w->y, "explosion01");
        for (int k = 0; k < 8; k++) {
            obj_t *d = fdb_spawn(FDB_K_MINE, "debris", w->x, w->y, 3); if (!d) break;
            double an = cross_rand(360), sp = (80 + cross_rand(30)) / 8.0;
            d->an.frame = k % 5; d->vx = (float)(sp * sbb_cos(an)); d->vy = (float)(-sp * sbb_sin(an));
        }
        S.wG = 1; snd_play_orig(343, 220, 128); S.uB = 0;
    }
    if (S.wG > 0 && S.wG < 45 && G.frame % 3 == 0) obj_effect(w->x - 20 + cross_rand(41), w->y - 20 + cross_rand(41), "dust");
    if (S.wG > 0) S.wG++;
    if (S.dead && S.wG > 45 && S.F != 50 && S.ph == PH_MECH) {                  /* 17065: F 50, sound 295 -> 0x230f4 */
        S.F = 50; S.H = 0; S.Gm = 0; snd_play_orig(295, 220, 128); S.ph = PH_MECHDEF; S.mdE = 0;
        music_fade(300, 0);                                                     /* 16876: fadeout_music(5000) */
    }
}

/* ======================================================================== the suit (RE choice and lives, RV moves: b4_0) */
/* RV b4_0 (Classic, Lilac): L -1 the assembly, L 0 a short stand (M > 30: the choice), L 5 a hop (about 100 frames, apex 126 px at
 * M 30, landing 90-170 px on the far side of the player), L 4 a crouch (about 28 frames) then N: L 6 a dash (about 20 frames at
 * 13 px), L 7 a spinning flight across the arena (about 180 frames), L 4 again (N 0) and L 0; L 8 the guns aimed (about 90 frames,
 * crouched), L 9 their volley (about 95 frames). The head sits 22 / 67 px off the pelvis standing, 55 / 45 crouched (L 4, L 8: then
 * the player reaches it). */
static void suit_head_xy(double *hx, double *hy) {                              /* RV b4_0: the head from the pelvis's B / C by L */
    double ox = 22, oy = -67;                                                   /* L 0 / 5 */
    switch (S.sL) {
    case 4: ox = 55; oy = -23; break;
    case 6: ox = 63; oy = -27; break;
    case 8: ox = 50; oy = -39; break;
    case 9: ox = -39; oy = -39; break;
    case 7: ox = 0; oy = -50; break;
    }
    *hx = S.sx + ox * S.sface; *hy = S.sy + oy;
}
static void suit_pose(void) {
    int crouch = S.sL == 4 || S.sL == 8 || S.sL == 9;
    double bob = S.sL == 0 && S.sground ? sbb_sin(S.sM * 12.0) * 1.5 : 0;
    double asm_t = S.sL == -1 ? fmin(1.0, fmax(0.0, (S.mdE - 180) / 195.0)) : 1.0;   /* the assembly (0x230f4 E 180-375, INFERRED) */
    for (int k = 0; k < SP_N; k++) {
        obj_t *o = spart(k); if (!o) continue;
        float ox = S.spx[k], oy = S.spy[k];
        int leg = k == SP_FFOOT || k == SP_BFOOT || k == SP_FKNEE || k == SP_BKNEE || k == SP_FLEG || k == SP_BLEG;
        double lift = crouch && !leg ? 19 : 0, step = 0;
        if (S.sL == 0 && S.sground && (k == SP_FFOOT || k == SP_BFOOT)) step = sbb_sin(S.sM * 12.0 + (k == SP_FFOOT ? 0 : 180)) * 3;
        double tx = S.sx + (S.sface > 0 ? ox : -ox) + step, ty = S.sy + oy + lift - (leg ? 0 : bob);
        if (asm_t < 1) { double fx = S.px0[k], fy = S.py0[k]; tx = fx + (tx - fx) * asm_t; ty = fy + (ty - fy) * asm_t; }
        o->x = (float)tx; o->y = (float)ty;
        o->flip = S.sface < 0; o->ang = S.sL == 7 && !leg ? (float)sbb_fmod360(S.sM * -20.0 * S.sface) : 0;
    }
    obj_t *h = shead();
    if (h && S.sL >= 0) { double hx, hy; suit_head_xy(&hx, &hy); h->x = (float)hx; h->y = (float)(hy - bob); h->flip = S.sface < 0; }
}
static void suit_laser(player_t *p, double speed) {                            /* BrevonSuitLaser_3765 (17200 / 17201: speed 96 / 64, / 8) */
    obj_t *g = spart(S.sM & 16 ? SP_BGUN : SP_FGUN);
    float x = g ? g->x + S.sface * 24 : (float)S.sx, y = g ? g->y : (float)S.sy - 20;
    obj_t *l = fdb_spawn(FDB_K_BEAM, "fdu_laser", x, y, 3);
    if (!l) return;
    double a = atan2(-(sbb_py(p) - y), sbb_px(p) - x) * SB_RAD;
    l->ang = (float)a; l->vx = (float)(speed * sbb_cos(a)); l->vy = (float)(-speed * sbb_sin(a)); l->fx = x; l->fy = y;
    snd_play_orig(341, 220, 128);                                               /* media 0x155 */
}
static void suit_projectiles(player_t *p) {
    for (int k = 0; k < fdb_rt_n(); k++) {
        obj_t *l = fdb_rt(k);
        if (!l) continue;
        if (l->st == FDB_K_BEAM) {                                              /* suit lasers (INFERRED 10 damage) */
            l->fx += l->vx; l->fy += l->vy; l->x = floorf(l->fx); l->y = floorf(l->fy);
            if (!obj_near_view(l, 64) || obj_overlaps_bg(l)) { obj_effect(l->x, l->y, "fdu_lashit"); l->alive = 0; continue; }
            if (sbb_csens_hit(p, l) && hurt(p, 10)) { obj_effect(l->x, l->y, "fdu_lashit"); l->alive = 0; }
        } else if (l->st == FDB_K_SPIN) {                                       /* the spin's aura (follows the suit) */
            l->x = (float)S.sx; l->y = (float)S.sy - 40; l->ang = (float)sbb_fmod360(l->ang - 30 * S.sface);
            if (S.sL != 7 || S.ph != PH_SUIT) { l->alive = 0; continue; }
            if (sbb_csens_hit(p, l)) hurt(p, 20);
        } else if (l->st == FDB_K_SLASH) {
            if (--l->tm <= 0) { l->alive = 0; continue; }
            if (sbb_csens_hit(p, l)) hurt(p, 15);
        }
    }
}
static void suit_choose(player_t *p) {                                         /* 17129-17137 (RE) */
    if (S.sQ > 0) { static const int OQ[5] = { 0, 12, 10, 6, 3 }; S.sO = OQ[S.sQ]; S.sQ--; }
    else S.sO += 1 + cross_rand(4);
    S.sM = 0; S.sface = sbb_px(p) < S.sx ? -1 : 1;
    if (S.sO >= 12) { S.sL = 8; S.sO = 0; return; }                            /* 17137 */
    if (S.sO >= 5 && S.sO % 3 == 1) { S.sL = 4; S.sN = 6; snd_play_orig(347, 220, 128); return; }   /* 17135 (voice br_brevon_strike2) */
    if (S.sO >= 5 && S.sO % 3 == 2) { S.sL = 4; S.sN = 7; snd_play_orig(347, 220, 128); return; }   /* 17136 (voice strike1) */
    S.sL = 5;                                                                   /* the hop (RV: O 3, 6, 9) */
    double X = sbb_px(p), tx = X + (S.sx < X ? 150 : -150);
    tx = fmin(fmax(tx, S.J + 80), S.K - 80);
    S.svx = (tx - S.sx) / 30.0; S.svy = -8.4; S.sground = 0; snd_play_orig(55, 220, 128);
}
static void suit_fight(player_t *p) {
    obj_t *h = shead();
    if (!h) return;
    double X = sbb_px(p);
    float wl = S.J + 40, wr = S.K - 40;
    S.sM++;
    switch (S.sL) {
    case 0:                                                                     /* stand, facing the player */
        S.svx = 0; S.sface = X < S.sx ? -1 : 1;
        if (S.sM > 30) suit_choose(p);
        break;
    case 5:                                                                     /* the hop: up fast, down slowly (RV) */
        if (S.sM > 30) S.svx = 0;
        if (S.sground && S.sM > 30) { S.sL = 0; S.sM = 0; }
        break;
    case 4:                                                                     /* the crouch, then N */
        S.svx = 0;
        if (S.sM == 1 && S.sN) { obj_t *w = fdb_spawn(FDB_K_FX, "fdu_warn", (float)S.sx, (float)S.sy - 110, 3); if (w) w->tm = 28; }
        if (S.sM > 28) {
            if (S.sN == 6) { S.sL = 6; S.sM = 0; S.sface = X < S.sx ? -1 : 1; snd_play_orig(348, 220, 128);
                             obj_t *sl = fdb_spawn(FDB_K_SLASH, "fdu_slash", (float)S.sx + 40 * S.sface, (float)S.sy - 30, 3); if (sl) { sl->tm = 20; sl->flip = S.sface < 0; } }
            else if (S.sN == 7) { S.sL = 7; S.sM = 0; obj_t *sp = fdb_spawn(FDB_K_SPIN, "fdu_spin", (float)S.sx, (float)S.sy - 40, 3); if (sp) sp->tm = 999;
                                  S.s7x = S.sx; S.sground = 0; snd_play_orig(55, 220, 128); }
            else { S.sL = 0; S.sM = 0; }
        }
        break;
    case 6:                                                                     /* the dash (RV: ~300 px in ~20 frames) */
        S.svx = S.sface * 13;
        for (int k = 0; k < fdb_rt_n(); k++) { obj_t *l = fdb_rt(k); if (l && l->st == FDB_K_SLASH) { l->x = (float)S.sx + 40 * S.sface; l->y = (float)S.sy - 30; } }
        if (S.sM > 22 || (S.sface < 0 && S.sx <= wl) || (S.sface > 0 && S.sx >= wr)) { S.sL = 4; S.sN = 0; S.sM = 0; }
        break;
    case 7: {                                                                   /* the spinning flight (RV: x 1900-2830, y 60-240, 180 frames) */
        double t = S.sM, mid = (S.J + S.K) / 2 - 150, R = 460;
        double ph0 = asin(fmax(-1.0, fmin(1.0, (S.s7x - mid) / R))) * SB_RAD;
        double nx = mid + R * sbb_sin(ph0 - t * 2.0), ny = S.sfloor - 90 + 90 * sbb_cos(t * 6.0);
        S.svx = nx - S.sx; S.sy = ny; S.svy = 0;
        if (t > 180) { S.sL = 4; S.sN = 6; S.sM = 0; S.sy = S.sfloor; S.sground = 1; S.svx = 0; }
        break; }
    case 8:                                                                     /* the guns aimed (crouched) */
        S.svx = 0; S.sface = X < S.sx ? -1 : 1;
        if (S.sM > 90) { S.sL = 9; S.sM = 0; }
        break;
    case 9:                                                                     /* the volley (INFERRED: every 10 frames) */
        S.svx = 0;
        if (S.sM % 10 == 5 && S.sM < 90) suit_laser(p, S.sM % 20 == 5 ? 12 : 8);
        if (S.sM > 95) { S.sL = 0; S.sM = 0; }
        break;
    }
    /* body motion: the hop's gravity (0.28 up, at most 2.2 down: RV), the floor (the pelvis's placed y), the walls */
    S.sx += S.svx; if (S.sx < wl) S.sx = wl; if (S.sx > wr) S.sx = wr;
    if (!S.sground && S.sL != 7) {
        S.svy += 0.28; if (S.svy > 2.2) S.svy = 2.2; S.sy += S.svy;
        if (S.sy >= S.sfloor) { S.sy = S.sfloor; S.svy = 0; S.sground = 1; snd_play_orig(38, 220, 128); if (G.cam.Z < 3) G.cam.Z = 3; }
    }
    suit_pose();
#ifdef FP_HOST_ENV
    { static int ah = -1; if (ah < 0) ah = getenv("FP_F4B_AUTOHIT") ? atoi(getenv("FP_F4B_AUTOHIT")) : 0;   /* host tests: a hit every n frames */
      if (ah > 0 && S.sInv <= 0 && G.frame % ah == 0) { h->A -= 4; if (h->A <= 0) h->fl |= FDB_DEF; } }
#endif
    /* the head (Q20): 17244 every defeat: A 10, C 60 frames of invulnerability, P - 1 */
    if (S.sInv > 0) { S.sInv--; h->inv = 1; h->c = 3; if (!S.sInv) { h->inv = 0; h->c = 0; } }
    if ((h->fl & FDB_DEF) && S.sP > 0) { h->fl &= ~FDB_DEF; h->A = 10; S.sInv = 60; S.sP--; snd_play_orig(24, 220, 128); }
    for (int k = 0; k < SP_N; k++) { obj_t *o = spart(k); if (o) o->bd = S.sL == 7 ? 20 : S.sL == 6 ? 15 : 0; }   /* INFERRED contact */
    if (S.sP <= 0) {                                                            /* 17248: L 10, g4 3, the music stops */
        S.ph = PH_SUITDEF; S.sL = 10; S.sM = 0; music_stop(); level_checkpoint(SUIT_G4, CP_X, CP_Y);
        for (int k = 0; k < fdb_rt_n(); k++) { obj_t *l = fdb_rt(k); if (l && l->st != FDB_K_FX) l->alive = 0; }
        for (int k = 0; k < SP_N; k++) { obj_t *o = spart(k); if (o) o->bd = 0; }
        S.svx = 0; S.sground = 1; S.sy = S.sfloor;
    }
}
/* L 10 (17203-17214): explosions (M <= 40), white bursts (M 40-240, sound 38), small explosions, M > 360 the fade card (sound 76) ->
 * L 11; L 11 (17215-17239): M 120 the player stands (MV.M 50, RV x 2624), the suit falls apart, M 280 Brevon steps out (anim 5) -> the
 * scene 0x230fc. (17218's Classic L 12 -> results is never reached: 17217 moves M past 120 the same frame: RV b4_0 f 4201.) */
static void suit_defeat(player_t *p) {
    obj_t *b = brevon();
    S.sM++;
    if (S.sL == 10) {
        if (S.sM < 20) S.sy -= 1;                                               /* RV: the suit lifts 20 px */
        suit_pose();
        if (S.sM <= 40 && S.sM % 4 == 0) { obj_effect((float)S.sx - 40 + cross_rand(81), (float)S.sy - 60 + cross_rand(81), "explosion06"); fdb_explode_snd(); }
        if (S.sM >= 40 && S.sM < 240 && S.sM % 8 == 0) { obj_effect((float)S.sx - 40 + cross_rand(81), (float)S.sy - 60 + cross_rand(81), "wburst"); snd_play_orig(38, 220, 128); }
        if (S.sM >= 40 && S.sM % 4 == 0) obj_effect((float)S.sx - 30 + cross_rand(61), (float)S.sy - 50 + cross_rand(61), "fdu_tiny");
        if (S.sM >= 240 && S.sM % 6 == 0) obj_effect((float)S.sx - 30 + cross_rand(61), (float)S.sy - 50 + cross_rand(61), "explosion02");
        if (S.sM > 360) { snd_play_orig(76, 220, 128); S.sL = 11; S.sM = 0; }
    } else if (S.sL == 11 && G.character == CHR_MILLA) {                       /* RV b5_4: Milla -> L 12 at M 120 (17218) */
        if (S.sM == 120) { S.sL = 12; S.sM = 0; }
    } else if (S.sL == 12) {                                                    /* 17240-17243: M 40 the fade card, M > 120 Sign.B 179 */
        if (S.sM >= 40) S.fadeW = (int)fmin(255, (S.sM - 40) * 255.0 / 60);
        if (S.sM > 120 && !S.results) { S.results = 1; fdb_ctrl = 0; results_begin_boss(game_timer()); S.ph = PH_END; }
    } else if (S.sL == 11) {
        if (S.sM == 120) {                                                      /* 17217 / 17219 */
            fdb_ctrl = 1; fdb_keys = 0;
            if (p->ground) { p->x = (float)(S.sx + (p->x < S.sx ? -42 : 42)); p->gsp = p->vx = 0; player_sync_sensors(p); }   /* RV b4_0: MV.X 2624 (pelvis + 42), MV.M 50 */
            p->face = S.sx < p->x ? -1 : 1;
            for (int k = 0; k < SP_N; k++) { obj_t *o = spart(k); if (o) { o->vx = (float)(cross_rand(9) - 4); o->vy = -(float)(2 + cross_rand(4)); } }
            obj_t *h = shead(); if (h) { h->vx = -3; h->vy = -5; }
            snd_play_orig(38, 220, 128);
        }
        if (S.sM > 120) {                                                       /* the suit's pieces fall away (INFERRED) */
            for (int k = 0; k < SP_N; k++) { obj_t *o = spart(k); if (o && !o->hidden) { o->vy += 0.3f; o->x += o->vx; o->y += o->vy; if (o->y > S.sfloor + 200) o->hidden = 1; } }
            obj_t *h = shead(); if (h && !h->hidden) { h->vy += 0.3f; h->x += h->vx; h->y += h->vy; if (h->y > S.sfloor + 200) h->hidden = 1; }
        }
        if (S.sM == 280 && b) {                                                 /* 17239: Brevon (anim 5) at the pelvis */
            b->hidden = 0; fdb_set(b, "fdb_brv0"); b->x = (float)S.sx; b->y = BREVON_FLOOR; b->flip = p->x < b->x;
            S.ph = PH_BSCENE; S.bE = 0; suit_show(0);
        }
    }
}

/* ======================================================================== Brevon (0x230fc scene, 0x230f7 fight: 17267-17437) */
/* RE states (Brevon_3701 D, E its timer, I the facing, F / G the position, H the floor 320, J / K the speed, M the attack counter):
 * D 0 the choice (17269-17280: M + 1 (+ rand 0-2 on the easy setting); M % 4 = 2 -> D 7, 3 -> D 3; M >= 16 -> D 11, M 0); D 6 the
 * walk (anim 5) -> D 1 the dash (anim 9, F + 15 I) -> D 2 the slash (J x 0.8, E > 15 -> D 0); D 3 the jump (anim 7, E 12: J 10 I, then
 * K + 0.6) -> D 4 landed (E > 25 -> D 0); D 5 the proxy mines (anim 14, E 7-9, BrevonProxyMine_3808) -> D 3; D 7 (anim 10) -> D 8 the
 * dive at the player (anim 9) -> D 9 its slide; D 11 / 12 / 13 the flight up (anim 13, K -6, sound 390), the wing beams
 * (BrevonWingBeam2_3807 every 2 frames), the drop (E 915 -> D 0). The hitbox (BrevonHitbox_3809, Q20, A 160) follows him; its A
 * is the bar (BrevonCrosshair7 x scale A / 160 in the frame BrevonCrosshair6: 17353 / 17354). A 0 -> D 60 (knocked up, K + 0.4) -> D 61
 * (anim 8, the slide) -> D 62 the character's finish (0x230fe Lilac / 0x230f0 Carol) -> D 63, Sign.B 179 at the stone's E 620 / 520.
 * Port: the states and numbers above (RE) with RV b4_0 / b5_* timings; the walk uses anim 1 (anim 5's 64 forced frames: VRAM), the
 * mines anim 10, the finishes are a timed kneel (INFERRED: the finishers' player moves, the energy meter and the grab D 10 are not
 * ported). */
static void brv_anim(obj_t *b, const char *row) { fdb_set(b, row); }
static void brv_mines(obj_t *b) {
    for (int k = 0; k < 2; k++) {
        obj_t *m = fdb_spawn(FDB_K_MINE, "fdb_mine", b->x + (k ? 20 : -20), b->y - 40, 3);
        if (m) { m->vx = (float)((k ? 3 : -3) + (b->flip ? -1 : 1)); m->vy = -5; m->tm = 0; m->av[7] = 1; }
    }
}
static void brv_projectiles(player_t *p) {
    for (int k = 0; k < fdb_rt_n(); k++) {
        obj_t *l = fdb_rt(k);
        if (!l) continue;
        if (l->st == FDB_K_MINE && l->av[7] == 1) {                            /* proxy mines: land, arm, burst near the player (INFERRED) */
            if (l->vy != 0 || l->y < BREVON_FLOOR - 8) { l->vy += 0.3f; l->x += l->vx; l->y += l->vy; if (l->y >= BREVON_FLOOR - 8) { l->y = BREVON_FLOOR - 8; l->vy = 0; l->vx = 0; fdb_set(l, "fdb_mine1"); } }
            else if (++l->tm > 300 || fabs(l->x - p->x) < 40) {
                obj_effect(l->x, l->y, "explosion02"); fdb_explode_snd();
                if (fabs(l->x - p->x) < 48 && fabs(l->y - (p->y - ORIG_REF_DY)) < 48) hurt(p, 15);
                l->alive = 0;
            }
        } else if (l->st == FDB_K_WAVE && l->av[7] == 2) {                     /* wing beams from above (INFERRED 15) */
            l->y += 12; if (l->y > BREVON_FLOOR + 40) { l->alive = 0; continue; }
            if (sbb_csens_hit(p, l)) hurt(p, 15);
        } else if (l->st == FDB_K_SLASH) {
            if (--l->tm <= 0) { l->alive = 0; continue; }
            if (sbb_csens_hit(p, l)) hurt(p, 15);
        }
    }
}
static void brevon_fight(player_t *p) {
    obj_t *b = brevon(), *hb = bhit();
    if (!b) return;
    double X = p->x, wl = S.J + 89, wr = S.K - 89;                              /* 17300 / 17301: the walls -/+ 0x59 */
#ifdef FP_HOST_ENV
    { static int ah = -1; if (ah < 0) ah = getenv("FP_F4B_AUTOHIT") ? atoi(getenv("FP_F4B_AUTOHIT")) : 0;
      if (ah > 0 && hb && S.bD < 60 && G.frame % ah == 0) { hb->A -= 4; if (hb->A <= 0) hb->fl |= FDB_DEF; } }
#endif
    int def = hb && (hb->fl & FDB_DEF);
    if (S.bD > 0 && S.bD < 60) S.bE++;                                          /* 17281 */
    if (def && S.bD < 60) {                                                     /* the defeat (A 0) */
        S.bD = 60; S.bE = 0; S.sE = 0; S.bJ = (b->x < X ? -2 : 2); S.bK = -4; brv_anim(b, "fdb_brv8");   /* RV b5_0: away from the player */
        music_fade(120, 0); snd_play_orig(38, 220, 128); fdb_explode_snd(); S.bfin = 0;
        for (int k = 0; k < fdb_rt_n(); k++) { obj_t *l = fdb_rt(k); if (l && l->st != FDB_K_FX) l->alive = 0; }
    }
    switch (S.bD) {
    case 0:                                                                     /* 17269-17280 */
        brv_anim(b, "fdb_brv0"); S.bI = X < b->x ? -1 : 1; S.bJ = S.bK = 0;
        if (++S.bE < 20) break;
        S.bE = 0; S.bM += 1;
        if (S.bM >= 16) { S.bM = 0; S.bD = 11; break; }                         /* 17280 */
        if (S.bM % 4 == 2) S.bD = 7;                                            /* 17278 */
        else if (S.bM % 4 == 3) S.bD = 3;                                       /* 17279 */
        else if (S.bM % 4 == 1) S.bD = 5;
        else S.bD = 6;
        break;
    case 6:                                                                     /* the walk (17282 / 17283: E <= 20 -> D 1) */
        brv_anim(b, "fdb_brv1"); S.bI = X < b->x ? -1 : 1; b->x += 2 * S.bI;
        if (S.bE > 20 || fabs(X - b->x) < 70) { S.bD = 1; S.bE = 0; }
        break;
    case 1:                                                                     /* 17284: the dash (anim 9, F + 15 I) */
        brv_anim(b, "fdb_brv9"); b->x += 15 * S.bI;
        if (S.bE > 6) { S.bD = 2; S.bE = 0; S.bJ = 8 * S.bI; obj_t *sl = fdb_spawn(FDB_K_SLASH, "fdu_slash", b->x + 36 * S.bI, b->y - 50, 3); if (sl) { sl->tm = 12; sl->flip = S.bI < 0; } snd_play_orig(347, 220, 128); }
        break;
    case 2:                                                                     /* 17295 / 17296 */
        b->x += (float)S.bJ; S.bJ *= 0.8;
        if (S.bE > 15) { S.bD = 0; S.bE = 0; }
        break;
    case 3:                                                                     /* 17297-17303: the jump */
        if (S.bE == 1) { S.bI = X < b->x ? -1 : 1; brv_anim(b, "fdb_brv0"); }
        if (S.bE == 12) { brv_anim(b, "fdb_brv7"); S.bJ = 10 * S.bI; S.bK = -11.7; snd_play_orig(21, 220, 128); }   /* RV: apex 114 px */
        if (S.bE > 12) {
            S.bK += 0.6; b->x += (float)S.bJ; b->y += (float)S.bK;
            if ((b->x >= wr && S.bJ > 0) || (b->x <= wl && S.bJ < 0)) { S.bJ = -S.bJ; S.bI = -S.bI; }
            if (S.bE > 15 && b->y >= BREVON_FLOOR) { b->y = BREVON_FLOOR; S.bJ = S.bK = 0; S.bD = 4; S.bE = 0; brv_anim(b, "fdb_brv0"); }
        }
        break;
    case 4: if (S.bE > 25) { S.bD = 0; S.bE = 0; } break;                       /* 17305 */
    case 5:                                                                     /* 17308-17311: the mines */
        brv_anim(b, "fdb_brv10");
        if (S.bE == 8) { brv_mines(b); snd_play_orig(28, 220, 128); }
        if (S.bE > 25) { S.bD = 3; S.bE = 0; }
        break;
    case 7:                                                                     /* 17312 / 17313 */
        brv_anim(b, "fdb_brv10"); S.bI = X < b->x ? -1 : 1;
        if (S.bE > 10) {
            S.bD = 8; S.bE = 0;
            double dx = X - b->x, dy = (p->y - ORIG_REF_DY + 32) - b->y, l = sqrt(dx * dx + dy * dy) + 1e-6;
            S.bJ = dx / l * 10; S.bK = dy / l * 10 - 4; snd_play_orig(347, 220, 128);
        }
        break;
    case 8:                                                                     /* 17314: the dive */
        brv_anim(b, "fdb_brv9"); S.bK += 0.4; b->x += (float)S.bJ; b->y += (float)S.bK;
        if (b->x < wl) b->x = (float)wl;
        if (b->x > wr) b->x = (float)wr;
        if (b->y >= BREVON_FLOOR) { b->y = BREVON_FLOOR; S.bD = 9; S.bE = 0; S.bK = 0; }
        break;
    case 9:                                                                     /* 17316 / 17317 */
        b->x += (float)S.bJ; S.bJ *= 0.8;
        if (S.bE > 15) { S.bD = 0; S.bE = 0; }
        break;
    case 11:                                                                    /* 17330-17333: up (K -6, sound 390) */
        brv_anim(b, "fdb_brv13");
        if (S.bE == 15) { S.bK = -6; snd_play_orig(390, 220, 128); }
        if (S.bE > 15) { b->y += (float)S.bK; S.bK = fmin(S.bK + 0.2, 0); if (S.bK >= 0) { S.bD = 12; S.bK = -15; S.bE = 0; } }
        break;
    case 12:                                                                    /* 17334-17338: off the top, the wing beams */
        b->y += (float)S.bK;
        if (S.bE % 2 == 0 && b->y > G.camy - 64) { obj_t *w = fdb_spawn(FDB_K_FX, "fdb_wbeam", b->x, b->y + 30, 3); if (w) w->tm = 8; }
        if (b->y < BREVON_FLOOR - 500) { S.bD = 13; S.bE = 0; S.bK = 0; b->hidden = 1; }
        break;
    case 13:                                                                    /* 17339-17342: the beams rain, then the drop */
        if (S.bE % 40 == 1 && S.bE < 240) {
            obj_t *w = fdb_spawn(FDB_K_WAVE, "fdb_wbeam", (float)X, G.camy - 32, 3);
            if (w) { w->av[7] = 2; w->ang = 270; }
            snd_play_orig(341, 220, 128);
        }
        if (S.bE == 260) { b->hidden = 0; b->x = (float)fmin(fmax(X, wl), wr); b->y = G.camy - 80; S.bK = 0; brv_anim(b, "fdb_brv7"); }
        if (S.bE > 260) { S.bK += 0.8; b->y += (float)S.bK; if (b->y >= BREVON_FLOOR) { b->y = BREVON_FLOOR; S.bD = 4; S.bE = 0; if (G.cam.Z < 5) G.cam.Z = 5; snd_play_orig(38, 220, 128); } }
        break;
    case 60:                                                                    /* 17360-17364 */
        S.bK += 0.4; b->x += (float)S.bJ; b->y += (float)S.bK;
        if (b->x < wl) b->x = (float)wl;
        if (b->x > wr) b->x = (float)wr;
        S.sE++;
        if (S.bK > 0 && b->y >= BREVON_FLOOR) { b->y = BREVON_FLOOR; S.bD = 61; fdb_ctrl = 1; fdb_keys = 0; }
        break;
    case 61: case 62: case 63: {                                                /* 17365-17408: the finish (stone E) */
        b->x += (float)S.bJ; S.bJ *= 0.9; brv_anim(b, "fdb_brv8");
        S.sE++;
        int carol = fdb_char() == CHR_CAROL;                                    /* 0x230f0 (MV.A 2 / 3) else 0x230fe */
        int e_snd = carol ? 350 : 450, e_card = carol ? 440 : 540, e_res = carol ? 520 : 620;
        if (S.sE == 110) { S.bD = 62; S.bfin = b->x < X ? -1 : 1; }              /* RV b5_0: D 62 at the stone's E 110 */
        if (S.bD == 62 && S.sE > 110 && S.sE < 260) {                           /* RV: the player drives him to the wall (x 2196 -> 1847) */
            double wall = S.bfin < 0 ? wl : wr;
            if (fabs(b->x - wall) > 2) b->x += (float)(S.bfin * 3.5);
            p->x = b->x - S.bfin * 8; p->face = S.bfin < 0 ? -1 : 1; player_sync_sensors(p);
            if ((S.bfin < 0 && b->x <= wl) || (S.bfin > 0 && b->x >= wr)) b->x = (float)wall;
        }
        if (S.sE == e_snd) { snd_play_orig(76, 220, 128); S.bD = 63; }
        if (S.sE >= e_snd) S.fadeW = (int)fmin(255, (S.sE - e_snd) * 255.0 / (e_card - e_snd));
        if (S.sE > e_res && !S.results) {                                       /* 17394 / 17408: Sign.B 179 */
            S.results = 1; fdb_ctrl = 0; results_begin_boss(game_timer()); S.ph = PH_END;
        }
        break; }
    }
    if (b->x < wl && S.bD < 60) b->x = (float)wl;
    if (b->x > wr && S.bD < 60) b->x = (float)wr;
    b->flip = S.bI < 0;
    if (hb) {                                                                   /* the hitbox follows Brevon (17268) */
        hb->x = b->x; hb->y = b->y; hb->hidden = 1;
        if (b->an.b) { hb->an = b->an; }
        hb->bd = S.bD >= 60 || b->hidden ? 0 : (S.bD == 1 || S.bD == 8 ? 15 : 10);   /* INFERRED contact */
        if (b->hidden) hb->x = -9999;
    }
    brv_projectiles(p);
}
/* the health bar: BrevonCrosshair6 (the frame) / 7 (the bar, x scale A / 160) slide up into the view's bottom (17353 / 17354) */
static void brevon_bar(void) {
    obj_t *fr = fdb_get_sub(OT_FDB_XHAIR, 0), *bar = fdb_get_sub(OT_FDB_XHAIR, 1), *hb = bhit();
    int on = S.ph == PH_BREVON;
    if (on && S.barY < 40) S.barY++;
    if (fr) { fr->hidden = !on; fr->x = G.camx + 32; fr->y = G.camy + SCREEN_H + 40 - S.barY; fr->layer = 4; }
    if (bar) { bar->hidden = !on; bar->x = G.camx + 3 + 32 - 29; bar->y = G.camy + SCREEN_H + 21 - S.barY; bar->layer = 4; bar->scale = hb ? (float)fmax(0.0, hb->A / 160.0) : 0; }
}

/* ======================================================================== the hit-point tubes (BossHealthBar1 / 2, 16824-16826) */
static void bars(void) {
    for (int k = 0; k < fdb_count(OT_FDB_HBAR); k++) {
        obj_t *o = fdb_get(OT_FDB_HBAR, k); if (!o) continue;
        int idx = (o->par ? 13 : 9) - (int)((k - (o->par ? fdb_count(OT_FDB_HBAR) - 14 : 0)) % (o->par ? 14 : 10));   /* placed top to bottom: A 9..0 / 13..0 */
        int n = o->an.b && o->an.b->anims ? o->an.b->anims[o->an.anim].count : 1;
        if (n > 0) o->an.frame = (uint16_t)(((G.frame / 2) + idx) % n);         /* 16824 */
        int left = o->par ? S.sP * 2 : S.wE;
        if (idx >= left) o->hidden = 1;                                         /* 16825 / 16826: destroyed */
    }
}

/* ======================================================================== the frame */
static void cam_script(player_t *p) {
    if (!G.cam.M3) return;
    G.cam.A = S.camA; G.cam.B = S.camB; (void)p;
}
void f4b_frame(player_t *p) {
    if (!S.on) return;
    obj_t *s = stone(), *b = brevon();
    flash_frame();
    switch (S.ph) {
    case PH_WAIT:                                                               /* 16830 */
        if (p->y - ORIG_REF_DY <= 336 && G.at_g4 < OPEN_G4 && p->hp > 0) {
            S.ph = PH_INTRO; S.iD = 0; S.iE = 0; fdb_ctrl = 1; fdb_keys = 0;
            G.cam.M3 = 1; S.camA = G.cam.A; S.camB = G.cam.B;
            fdb_cam_lock(S.J, S.K, fdb_marker(OT_CAMLIM_T, 0), fdb_marker(OT_CAMLIM_B, 0));
            if (!S.music) { S.music = 1; music_play(FD4B_MECH_MUSIC); }       /* ML.Y 1 (RV: m_boss5) */
        }
        break;
    case PH_INTRO:
        if (S.iD == 0) { if (p->ground) S.iD = 1; }                             /* 16832 */
        else if (S.iD == 1) {                                                    /* 16833-16835: walk to Brevon.x - 192 */
            float tx = S.brx0 - 192;                                            /* RV b1_0: from the lift (x 2606) MV.B -5 to 2491 */
            if (S.dir0 == 0) S.dir0 = p->x < tx ? 1 : -1;
            walk(p, 5 * S.dir0);
            if (!b || (S.dir0 > 0 ? p->x >= tx : p->x <= tx)) { walk(p, 0); p->face = 1; S.iD = 2; S.iE = 0; }
        } else if (S.iD == 2) {
            S.iE++;
            if (S.iE == 60) { S.F = 1; S.Gm = 0; S.H = 0; S.aB = 1; }              /* 16843: the mech rises and dashes (RV f800) */
            if (S.iE == 90 && b) fdb_set(b, "fdb_brv10");                        /* 16844 */
            if (S.iE > 90) S.iD = 3;
        } else if (S.iD == 3) {                                                  /* 16846 / 16847 */
            obj_t *hd = mp(MP_HEAD);
            if (b) b->y -= 8;
            if (!b || !hd || b->y < hd->y || b->y < G.camy - 120) {
                if (b) { b->hidden = 1; b->x = 0; }
                S.ph = PH_MECH; fdb_ctrl = 0; G.cam.M3 = 0; flash_start();
            }
        }
        if (S.iE >= 60) mech_fight(p);
        S.camA = (float)fmax(S.camA, p->x); S.camB = G.cam.B;
        break;
    case PH_MECH:
        mech_fight(p);
        break;
    case PH_MECHDEF:                                                            /* 0x230f4 (16876-16905; RV b4_0 f 1261-1870) */
        mech_fight(p);
        S.mdE++;
        if (S.mdE == 60 && b) {                                                 /* 16879: Brevon out of the mech (anim 7; RV at 2634,155) */
            obj_t *hd = mp(MP_HEAD); b->hidden = 0; fdb_set(b, "fdb_brv7"); b->x = hd ? hd->x : (float)S.C; b->y = hd ? hd->y : (float)S.D;
            b->vx = 0; b->vy = -2; b->flip = p->x < b->x;
        }
        if (S.mdE > 60 && S.mdE < 375 && b && !b->hidden) {                     /* RV: he drops to y 245 and stands */
            if (b->y < 245) { b->vy += 0.3f; b->y += b->vy; if (b->y >= 245) { b->y = 245; fdb_set(b, "fdb_brv0"); } }
        }
        if (S.mdE == 180) {                                                     /* 16881: the suit group on (sound 0x18b): it gathers at him */
            snd_play_orig(395, 220, 128); S.sL = -1; S.sM = 0; S.sx = b ? b->x : S.sx; S.sy = S.sfloor; S.sface = p->x < S.sx ? -1 : 1;
        }
        if (S.mdE >= 180) suit_pose();
        {   static const short WB[] = { 200, 220, 239, 257, 274, 290, 305, 319, 332, 344, 355, 365, 375 };   /* 16882-16894 */
            for (unsigned k = 0; k < sizeof WB / sizeof WB[0]; k++) if (S.mdE == WB[k]) { obj_effect((float)S.sx - 50 + cross_rand(101), (float)S.sy - 80 + cross_rand(81), "wburst"); snd_play_orig(38, 220, 128); }
        }
        if (S.mdE == 375) {                                                     /* 16894: Brevon inside; the mech gone (16905: its groups off) */
            if (b) b->hidden = 1;
            mech_hide(1); for (int k = 0; k < MP_N; k++) { obj_t *o = mp(k); if (o) o->alive = 0; }
            for (int k = 0; k < 4; k++) { obj_t *o = fdb_slot(S.bw[k]); if (o) o->alive = 0; o = fdb_slot(S.fw[k]); if (o) o->alive = 0; }
            obj_t *w = weak(); if (w) w->alive = 0;
            obj_t *h = shead(); if (h) { h->hidden = 0; h->A = 10; }
        }
        if (S.mdE == 600) { S.music = 2; music_play(FD4B_MECH_MUSIC); }        /* 16904: D 2 E 100 (play_music) */
        if (S.mdE > 600) { S.ph = PH_SUIT; S.sL = 0; S.sM = 0; flash_start(); }   /* 16905 */
        break;
    case PH_SUIT:
        suit_fight(p);
        break;
    case PH_SUITDEF:
        suit_defeat(p);
        break;
    case PH_BSCENE:                                                             /* 0x230fc (16906-16918; RV b4_0: the stone's E 0-112) */
        S.bE++;
        if (b) b->flip = p->x < b->x;
        if (S.bE == 40) { const char *m = level_music_slot(6); if (m) music_play(m); }   /* RV f 4439: m_boss7 */
        if (S.bE > 110) {                                                       /* 16917 / 16918: the fight (0x230f7) */
            S.ph = PH_BREVON; S.bD = 6; S.bE = 0; S.bM = 0; S.bI = b && p->x < b->x ? -1 : 1; fdb_ctrl = 0;
            obj_t *hb = bhit(); if (hb) { hb->A = BREVON_A; hb->fl &= ~FDB_DEF; }
#ifdef FP_HOST_ENV
            if (hb && getenv("FP_F4B_BA")) hb->A = atoi(getenv("FP_F4B_BA"));         /* host tests: a shorter fight */
#endif
        }
        break;
    case PH_BREVON:
        brevon_fight(p);
        break;
    case PH_END:
        if (!S.ended && results_tally_done()) { S.ended = 1; results_request_end(); S.ph = PH_DONE; }   /* 16812-16814 -> 16811 */
        break;
    }
    if ((S.ph >= PH_MECH && S.ph <= PH_SUIT) || (S.ph == PH_INTRO && S.iE >= 60)) projectiles(p);
    if (S.ph == PH_SUIT || S.ph == PH_SUITDEF) suit_projectiles(p);
    brevon_bar();
    bars();
    cam_script(p);
    (void)s;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_F4B")) { obj_t *w = weak(), *h = shead(); printf("f4b %d ph %d iD %d iE %d F %.0f G %.0f C %.0f Dm %.0f aH %d uB %.0f wE %d dead %d wG %d mdE %d sL %d sM %d sO %d sP %d hA %.0f sx %.0f px %.0f py %.0f hp %d res %d wc %d bD %d bE %d bA %.0f bx %.0f sE %d\n",
        G.frame, S.ph, S.iD, S.iE, S.F, S.Gm, S.C, S.D, S.aH, S.uB, S.wE, S.dead, S.wG, S.mdE, S.sL, S.sM, S.sO, S.sP, h ? h->A : -1, S.sx, p->x, p->y, p->hp, S.results, w ? w->c : -1, S.bD, S.bE, bhit() ? bhit()->A : -1, b ? b->x : -1, S.sE); }
#endif
}

int f4b_draw(const obj_t *o, int pass) {
    (void)pass;
    if (!S.on) return 0;
    if (o->type == OT_FDB_MECH && (o->par == MP_BWING || o->par == MP_FWING)) { if (!o->hidden) sbb_draw(o, 0xa0ffffff); return 1; }   /* INFERRED: see-through wings (BG4) */
    if (o->type == OT_FDB_STONE) {
        if (!o->hidden) sbb_draw(o, 0xffffffff);
        int a = flash_alpha();                                                  /* the flash over the back layers (layer 2: 16871) */
        if (a > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)a << 24) | 0xffffff);
        if (S.fadeW > 0) r_fade(S.fadeW / 255.f);                               /* the end's fade card (17214 / 17241 / 17392 / 17406) */
        return 1;
    }
    if (o->type == OT_FDB_RT && o->st == FDB_K_WAVE) { sbb_draw_yscaled(o, o->av[0]); return 1; }
    if (o->type == OT_FDB_SUIT_HEAD && S.sInv > 0 && (G.frame & 2)) return 1;   /* hit flicker (INFERRED) */
    if (o->type == OT_FDB_XHAIR) {                                              /* the bar: the frame, the bar scaled by A / 160 (17354) */
        if (o->hidden || !o->an.b) return 1;
        const frame_t *f = anim_frame(&o->an);
        float sx = floorf(SCREEN_W / 2.f) - (o->par ? 100.f : 128.f), sy = (float)(SCREEN_H + 40 - S.barY) - (o->par ? 21.f : 32.f) + (o->par ? 10.f : 0.f);
        float w = o->par ? (float)f->w * o->scale : (float)f->w;
        if (w > 0) frame_draw_scaled(o->an.b, f, sx, sy, w, (float)f->h, 0xffffffff);
        return 1;
    }
    return 0;
}
float f4b_hud_dy(void) { return 0.f; }
#ifdef FP_HOST_ENV
int f4b_host_state(char *buf, int n) {
    if (!S.on) return 0;
    obj_t *h = shead();
    return snprintf(buf, n, "f4b ph %d F %.0f wE %d dead %d sL %d sP %d hA %.0f res %d end %d ", S.ph, S.F, S.wE, S.dead, S.sL, S.sP, h ? h->A : -1, S.results, S.ended);
}
#endif
