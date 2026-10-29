/* TB3: the Syntax Spider (BossSyntaxSpider_2755, the mask the player hits, with its Body_2753, LavaCannon_2752, 2 x Arm_2754,
 * 2 x LegFront_2756, 2 x LegBack_2751, GrappleLine_2770, BossLavaSplash1_2758 (the right lava fall's splash) / 2_2759 (the left),
 * the arena's two HazardLavaFall_2737 columns, runtime HazardLavaFall2_2757 (lava pieces) and BossSpikeLauncher_2769 (spikes)),
 * then the lava chase (HazardLavaFall3_2777 wall, LevelHeatSky_2779, PlayerBlocker_2780 through the gimmick module).
 * RE handle_frame_47_events 12126-12302 (groups 0x22c1c-0x22c23); RV build/orig_traces/tb_bosses p5_l (Lilac on the left ledge:
 * two full cycles of the walker), p6_l (A 0 at 400: the hanging phase), p7_l (A 0 at 400 and 1100: the end and the chase).
 * spec_tb_bosses.md Part C. No character test (RE).
 *
 * Phases (the RE groups): INTRO (0x22c1d on, 0x22c1e: the mask's O 0..4), WALK (0x22c20 + 0x22c1c: the walker, A 200), BREAK
 * (0x22c22: the walker's defeat), HANG (0x22c1f: the mask on the grapple line, A 30), CHASE (0x22c21: the lava wall).
 * Mask alterables: A HP, D its y (falls to J at P + 0.3 a frame, max 12), J the floor y, K the step, O the intro step, N a counter,
 * H the walk (-1 / 0 / 1), I the attack kind (1: a fall's stream, 0: the cannon), F / L counters, E / G the turn (E 1: turning to G).
 * Body: A the walk phase (A - 4.5 B), B the speed (+ / - 0.2 a frame to 4), C the facing (1 right, -1 left, 0 front), E the walk
 * amplitude (+ / - 0.05), F the attack readiness (0..1). Cannon: B the recoil (-10, + 1 a frame to 10), C the lava held (0..10),
 * D its raise (+ / - 2, 0..18), flag 2 raised.
 * WALK steps (RV p5: 1654 frames a cycle): K 1 / 5 / 11 walk right to the right splash - 100, K 3 / 9 / 13 walk left to the left
 * splash + 100 (12173-12178); K 2 / 6 (right end) and 10 / 14 (left end): I 1, body F up to 1 (12198), then the far fall pours a
 * stream of 10 pieces across the floor (12199-12204: sound 305 at F 1, 304 at F 51, a piece every 3 frames from F 51: cannon C + 1)
 * that the cannon swallows (sound 55); K 4 / 8 / 12 / 16: I 0 once stopped (12179), the cannon fires its 10 pieces (12205-12211:
 * 303, every 3 frames: recoil, a splash at the muzzle), body F falls back (12212), then the next step (12180); K 7 / 15: the cannon
 * raises / lowers (12181-12183: L 1 toggles flag 2, L > 30: next). K > 16: 1 (12184). The turn (12190-12193): stopped on the floor
 * and not facing the arena's middle: E 1, 10 frames facing the front (C 0), then C = G.
 * Lava pieces: 10 px a frame along A; a stream piece is swallowed by the cannon, a cannon piece ends in the far fall (12281 / 12282:
 * the fall's splash, sound 55). A piece on the player: burn 3 (12283). A lava column on the player: burn 5, pushed out (B + / - 5:
 * 12131 / 12132). */
#include "tb_boss.h"
#include <stdio.h>
#include <stdlib.h>

extern void tb_gim_blockers(int solid) __attribute__((weak));                 /* TB3 PlayerBlockers (12260 / 12262) */

enum { PH_OFF, PH_IDLE, PH_INTRO, PH_WALK, PH_BREAK, PH_HANG, PH_CHASE };
#define NLF 16           /* lava pieces */
#define NSP 8            /* spike launchers */
#define NLC 8            /* arena lava columns */
#define NL3 32           /* the chase's lava wall */
#define NPART 8
static struct {
    int on, ph;
    double D, J, K, O, N, P, H, I, F, L, E, G;                  /* the mask */
    double bA, bB, bC, bE, bF, bG;                              /* the body */
    double cB, cC, cD; int cRaise, cFiring;                     /* the cannon */
    double s1A, s2A, s1B, s2B;                                  /* the splashes: home x, visible timer */
    double gX, gY, gA, gB, gC; int gFlip, snd52;                /* the grapple line */
    short pt[NPART]; double pX[NPART], pY[NPART];               /* parts flying off (12225 / 12226) */
    double lfA[NLF]; short lf[NLF]; char lfS[NLF];              /* lava pieces: speed, slot, stream (flag 2) */
    short sp[NSP]; double spA[NSP], spD[NSP], spE[NSP]; char spC[NSP]; int spL;
    short lc[NLC]; int nlc;                                     /* arena columns */
    short l3[NL3]; int nl3; double l3E;                         /* the chase wall, its hurt wait */
    int music, chase_snd, blk, cam;
    double camA, camB;
} S;

/* the parts */
static obj_t *mask(void) { return tbb_get(OT_TBB_SPIDER, 0); }
static obj_t *body(void) { return tbb_get(OT_TBB_SP_BODY, 0); }
static obj_t *cannon(void) { return tbb_get(OT_TBB_SP_CANNON, 0); }
static obj_t *splash(int k) { for (int i = 0; i < tbb_count(OT_TBB_SPLASH); i++) { obj_t *o = tbb_get(OT_TBB_SPLASH, i); if (o && o->par == k) return o; } return NULL; }
static obj_t *gline(void) { return tbb_get(OT_TBB_SP_GRAPPLE, 0); }
int tb_boss_spider_state(void) { return S.on ? S.ph * 100 + (int)S.K : -1; }

/* a part's row by facing: C 1 -> direction 0, -1 -> 16, 0 -> 24 (12215-12217) */
static void face(obj_t *o, int k0, int k16, int k24, double C) { if (o) tbb_set_a(o, C > 0 ? k0 : C < 0 ? k16 : (k24 >= 0 ? k24 : k0)); }
static int dead(const player_t *p) { return p->hp <= 0 || p->state == ST_DEAD; }

void tbp_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    obj_t *m = mask();
    if (!m) return;
    S.on = 1; S.ph = PH_IDLE;
    S.J = m->y; S.D = m->y - 80;                                                /* RV p5 f0: J 3983 (the placement), D 3903 */
    m->A = 200; m->fl |= TBB_ARM | F_NOFLY; m->bd = 0; m->hidden = 1;            /* ctor A 200; armoured (flag 5) until the intro ends */
    tbb_set_a(m, TS_SP_HEAD11);
    S.bC = 1; S.cB = 10;                                                         /* RV p5 f0: body C 1, cannon B 10 */
    obj_t *s1 = splash(0), *s2 = splash(1);
    S.s1A = s1 ? s1->x : m->x + 448; S.s2A = s2 ? s2->x : m->x - 191;            /* their A = the placement x (RV: 3775 / 3136) */
    if (s1) { tbb_set_a(s1, TS_SP_SPL1); s1->hidden = 1; }
    if (s2) { tbb_set_a(s2, TS_SP_SPL2); s2->hidden = 1; }
    for (int t = OT_TBB_SP_BODY; t <= OT_TBB_SP_GRAPPLE; t++)
        for (int k = 0; k < tbb_count(t); k++) { obj_t *o = tbb_get(t, k); if (o) { o->hidden = 1; o->av[0] = 1 - k; } }   /* alterable A: 1 / 0 (RV p5: the list's .1 / .2) */
    obj_t *gl = gline(); if (gl) tbb_set_a(gl, TS_SP_LINE);
    for (int k = 0; k < NLF; k++) S.lf[k] = -1;
    for (int k = 0; k < NSP; k++) S.sp[k] = -1;
    for (int k = 0; k < NPART; k++) S.pt[k] = -1;
    /* the arena's lava columns (HazardLavaFall_2737 inside the camera box: the gimmick module's type, taken over here (par 1 keeps
     * its TB2 drop rule off them); 12130 scrolls them, 12131 / 12132 burn) and the chase wall (HazardLavaFall3_2777, par 1) */
    float L = tbb_marker(OT_CAMLIM_L, 1), R = tbb_marker(OT_CAMLIM_R, 1);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_TBG_LFALL) continue;
        if (o->par == 0 && o->x >= L - 64 && o->x <= R + 64 && S.nlc < NLC) { S.lc[S.nlc++] = (short)i; o->par = 1; o->hidden = 0; }
        else if (o->par == 1 && o->x < L - 64 && S.nl3 < NL3) S.l3[S.nl3++] = (short)i;
    }
}

/* ---------------------------------------------------------------- lava pieces (HazardLavaFall2_2757: 10 px a frame along A) */
static obj_t *piece_new(float x, float y, double a, int stream) {
    int k; for (k = 0; k < NLF && S.lf[k] >= 0 && tbb_slot(S.lf[k]); k++) ;
    if (k == NLF) return NULL;
    obj_t *o = tbb_spawn(OT_TBB_SHOT, TS_SP_LF2, x, y, 1);
    if (!o) return NULL;
    o->par = 20; S.lf[k] = (short)(o - G.o); S.lfA[k] = a; S.lfS[k] = (char)stream;
    return o;
}
static int pieces_alive(void) { int n = 0; for (int k = 0; k < NLF; k++) { obj_t *o = tbb_slot(S.lf[k]); if (o && o->type == OT_TBB_SHOT && o->par == 20) n++; } return n; }
static void splash_at(int which, float x, float y) {
    obj_t *s = splash(which); if (!s) return;
    s->x = x; s->y = y; s->hidden = 0; anim_restart(&s->an);
    if (which == 0) S.s1B = 5; else S.s2B = 5;
}
static void pieces(player_t *p) {
    obj_t *c = cannon();
    for (int k = 0; k < NLF; k++) {
        obj_t *o = tbb_slot(S.lf[k]);
        if (!o || o->type != OT_TBB_SHOT || o->par != 20) { S.lf[k] = -1; continue; }
        o->x += (float)S.lfA[k];
        if (!S.lfS[k] && S.lfA[k] > 0 && o->x > S.s1A) { splash_at(0, (float)S.s1A, o->y); snd_play_orig(55, 220, 128); o->alive = 0; S.lf[k] = -1; continue; }   /* 12281 */
        if (!S.lfS[k] && S.lfA[k] < 0 && o->x < S.s2A) { splash_at(1, (float)S.s2A, o->y); snd_play_orig(55, 220, 128); o->alive = 0; S.lf[k] = -1; continue; }   /* 12282 */
        if (S.lfS[k] && c && !c->hidden && fabsf(o->x - c->x) < 20) {          /* the stream swallowed at the cannon (col 0x15eb0 x 0x15ff0; RV p5 399: the splash at 3653) */
            splash_at(S.lfA[k] > 0 ? 1 : 0, o->x, o->y); snd_play_orig(55, 220, 128); o->alive = 0; S.lf[k] = -1; continue;
        }
        if (o->x < S.s2A - 200 || o->x > S.s1A + 200) { o->alive = 0; S.lf[k] = -1; continue; }   /* (a safety bound: pieces end at a fall or the cannon) */
        if (!dead(p) && sbb_csens_hit(p, o)) player_burn(p, 3);                 /* 12283: R-hurt 3 + burn (not the fire shield) */
    }
    for (int w = 0; w < 2; w++) {                                               /* a splash shows 5 frames (B 5) */
        double *B = w ? &S.s2B : &S.s1B; obj_t *s = splash(w);
        if (*B > 0 && (*B -= 1) <= 0 && s) s->hidden = 1;
    }
}

/* ---------------------------------------------------------------- the arena's lava columns (12130-12132) */
static void columns(player_t *p) {
    int scroll = (tbb_g3() * 10) % 128;
    for (int k = 0; k < S.nlc; k++) {
        obj_t *o = tbb_slot(S.lc[k]); if (!o) continue;
        o->y = (float)(o->oy + 112 * o->av[5] + scroll);                       /* 12130: y = B + C + (g3 x 10) mod 128 (C = 112 k) */
        o->hidden = 0;
    }
    if (S.ph < PH_INTRO || dead(p)) return;
    for (int k = 0; k < S.nlc; k++) {
        obj_t *o = tbb_slot(S.lc[k]); if (!o || !sbb_csens_hit(p, o)) continue;
        int left = o->x < S.s2A, right = o->x > S.s1A;
        if (!left && !right) continue;
        if (player_burn(p, 5)) {                                                /* 12131 / 12132: LIFE.B + 5, OTHERS.R 30 */
            float b = left ? 5.f : -5.f;
            if (p->ground) p->gsp = b; else p->vx = b;                          /* MV.B = + / - 5: out of the column */
        }
        break;
    }
}

/* ---------------------------------------------------------------- the walker's rig (12218-12220; offsets fitted to RV p5) */
static void rig(void) {
    obj_t *m = mask(), *b = body(), *c = cannon();
    if (!m) return;
    double C = S.bC, E = S.bE, A = S.bA, sx = floor(m->x), sy = floor(m->y), fc = C ? C : 1;
    if (b) { b->x = (float)sx; b->y = (float)(int)(sy + sbb_sin(A) * 3 * E); face(b, TS_SP_BODY0, TS_SP_BODY16, TS_SP_BODY24, C); }
    if (c) {
        c->x = (float)(int)(sx + fc * (11 + S.cB)); c->y = (float)(int)(sy + S.cD + sbb_sin(45 + A) * 3 * E);
        if (S.cFiring) tbb_set_a(c, fc > 0 ? TS_SP_CAN7_0 : TS_SP_CAN7_16);
        else if (S.cD > 0) tbb_set_a(c, fc > 0 ? TS_SP_CAN1 : TS_SP_CAN17);
        else face(c, TS_SP_CAN0, TS_SP_CAN16, TS_SP_CAN24, C);
    }
    for (int k = 0; k < tbb_count(OT_TBB_SP_ARM); k++) {
        obj_t *o = tbb_get(OT_TBB_SP_ARM, k); if (!o) continue;
        double a = o->av[0], ph = 180 * a + A;
        o->x = (float)(int)(sx + fc * (24 * a - 1) + sbb_sin(ph) * 6 * E); o->y = (float)(int)(sy + 11 + sbb_cos(ph) * 8 * E);
        face(o, TS_SP_ARM0, TS_SP_ARM16, -1, fc);
    }
    for (int k = 0; k < tbb_count(OT_TBB_SP_LEGF); k++) {
        obj_t *o = tbb_get(OT_TBB_SP_LEGF, k); if (!o) continue;
        double a = o->av[0], ph = 180 * a + A;
        o->x = (float)(int)(sx - 44 * fc + 28 * a - 14 + sbb_sin(ph) * 8 * E); o->y = (float)(int)(sy + 2 + fabs(sbb_cos(ph)) * 8 * E - 8 * S.bF);
        face(o, TS_SP_LEGF0, TS_SP_LEGF16, -1, fc);
    }
    for (int k = 0; k < tbb_count(OT_TBB_SP_LEGB); k++) {
        obj_t *o = tbb_get(OT_TBB_SP_LEGB, k); if (!o) continue;
        double a = o->av[0], ph = 180 + A + 180 * a;
        o->x = (float)(int)(sx + 28 * a - 14 + sbb_sin(ph) * 6 * E); o->y = (float)(int)(sy + 2 + fabs(sbb_cos(ph)) * 8 * E - 8 * S.bF);
        face(o, TS_SP_LEGB0, TS_SP_LEGB16, -1, fc);
    }
    if (S.ph == PH_WALK) face(m, TS_SP_HEAD0, TS_SP_HEAD16, TS_SP_HEAD24, C);
}
static void parts_shown(int on) {
    for (int t = OT_TBB_SP_BODY; t <= OT_TBB_SP_LEGB; t++) for (int k = 0; k < tbb_count(t); k++) { obj_t *o = tbb_get(t, k); if (o) o->hidden = !on; }
}

/* ---------------------------------------------------------------- INTRO (12129, 12133-12172) */
static void intro(player_t *p) {
    obj_t *m = mask(); if (!m) return;
    double PY = sbb_py(p);
    if (S.ph == PH_IDLE) {
        float T = tbb_marker(OT_CAMLIM_T, 0), L = tbb_marker(OT_CAMLIM_L, 1), R = tbb_marker(OT_CAMLIM_R, 1);
        double PX = sbb_px(p);
        if (!(PY > T) || dead(p) || PX < L - 400 || PX > R + 64) return;      /* 12129: Player Y below the top limit (INFERRED: near the arena) */
        S.ph = PH_INTRO; level_lock_markers(); snd_play_orig(284, 220, 128);   /* CAM.M 1, sound 284 */
        m->y = (float)S.D; m->hidden = 0; tbb_set_a(m, TS_SP_HEAD11); anim_restart(&m->an);
        if (!S.music) { S.music = 1; const char *mu = level_music_slot(4); if (mu) music_play(mu); }   /* ML.Y 1 -> the stage's slot 4 (m_midboss) */
        return;
    }
    if (S.O == 0 && m->an.done) S.O = 1;                                       /* 12135: anim 11 played */
    if (S.O == 1) S.O = 2;                                                      /* 12136-12143: the hooks (INFERRED: not drawn) */
    if (S.O == 2) S.O = 3;                                                      /* 12144 */
    if (S.O == 3) {                                                             /* 12145: N + 1; the parts come down on the hooks */
        S.N += 1; tbb_set_a(m, TS_SP_HEAD24);
        parts_shown(1); rig();
        double lift = (80 - fmin(S.N, 80)) * 1.3;                               /* INFERRED: the hooks' descent (RV p5: the body 104 px up at 86, in place at 166) */
        for (int t = OT_TBB_SP_BODY; t <= OT_TBB_SP_LEGB; t++) for (int k = 0; k < tbb_count(t); k++) { obj_t *o = tbb_get(t, k); if (o) o->y -= (float)lift; }
        if (S.N > 80) { S.O = 4; S.N = 0; m->fl &= ~TBB_ARM; S.ph = PH_WALK; }    /* 12170: O 4, group 0x22c1c, armour off */
    }
}

/* ---------------------------------------------------------------- WALK (12171-12221) */
static void walk(player_t *p) {
    obj_t *m = mask(), *c = cannon(); if (!m) return;
    int g3 = tbb_g3();
    if (S.K == 0) {                                                             /* 12171 / 12172: the fall to the floor */
        S.D = fmin(S.D + S.P, S.J); S.P = fmin(S.P + 0.3, 12);
        if (S.D >= S.J && S.bF <= 0) { S.bF = 1; S.K = 1; voice_play("sx_lost"); }   /* 12172 (RV 188: sx_lost) */
    }
    double x = floor(m->x);
    int K = (int)S.K;
    if (K == 1 || K == 5 || K == 11) { S.H = 1; if (x >= S.s1A - 100) { S.H = 0; S.K += 1; } }        /* 12173 / 12174 */
    K = (int)S.K;
    if (K % 4 == 2 && S.F <= 0) S.I = 1;                                        /* 12175 (the mask's F: not while turning) */
    if (K % 4 == 2 && S.cC >= 10 && pieces_alive() == 1) S.K += 1;              /* 12176 */
    K = (int)S.K;
    if (K == 3 || K == 9 || K == 13) { S.H = -1; if (x <= S.s2A + 100) { S.H = 0; S.K += 1; } }       /* 12177 / 12178 (after 12175: RV p5 1117-1128) */
    K = (int)S.K;
    if (K >= 4 && K % 4 == 0 && S.bB == 0) S.I = 0;                              /* 12179 */
    if (K >= 4 && K % 4 == 0 && S.bF <= 0) S.K += 1;                            /* 12180 */
    K = (int)S.K;
    if (K == 7 || K == 15) {                                                    /* 12181-12183 */
        S.L += 1;
        if (S.L == 1) S.cRaise = !S.cRaise;
        if (S.L > 30) { S.L = 0; S.K += 1; }
    }
    if (S.K > 16) S.K = 1;                                                      /* 12184 */
    /* 12185-12189: the walk speed and amplitude */
    if (S.H == -1) { S.bB = fmax(S.bB - 0.2, -4); S.bE = fmin(S.bE + 0.05, 1); }
    if (S.H == 1) { S.bB = fmin(S.bB + 0.2, 4); S.bE = fmin(S.bE + 0.05, 1); }
    if (S.H != -1 && S.bB < 0) S.bB = fmin(S.bB + 0.2, 0);
    if (S.H != 1 && S.bB > 0) S.bB = fmax(S.bB - 0.2, 0);
    if (S.H == 0) S.bE = fmax(S.bE - 0.05, 0);
    if (fabs(S.bB) < 1e-9) S.bB = 0;
    /* 12190 / 12191: the turn (on the floor, not turning): toward the arena's middle */
    if (S.D >= S.J && S.E == 0) {
        if (S.bC != 1 && x <= S.s2A + 100) { S.E = 1; S.G = 1; S.F = 0; }
        else if (S.bC != -1 && x > S.s2A + 100 && S.H != 0) { S.E = 1; S.G = -1; S.F = 0; }
    }
    if (S.E == 1) { S.F += 1; S.bC = 0; }                                       /* 12192 */
    if (S.E == 1 && S.F > 10) { S.F = 0; S.E = 0; S.bC = S.G; }                 /* 12193 */
    /* 12196 / 12197: the cannon's raise */
    S.cD = S.cRaise ? fmin(S.cD + 2, 18) : fmax(S.cD - 2, 0);
    /* 12198-12212: the attacks */
    if (S.I == 1) S.bF = fmin(S.bF + 0.1, 1);                                  /* 12198 */
    if (S.I == 1 && S.bF >= 1 && S.cC < 10 && S.E == 0) {                       /* 12199-12204: the far fall's stream */
        int w = S.bC > 0 ? 0 : 1;                                               /* facing right: the right fall (splash 1) pours left */
        if (S.F == 0) snd_play_orig(305, 220, 128);                             /* 12201 (RV p5 297) */
        S.F += 1;
        if (S.F == 51) snd_play_orig(304, 220, 128);                            /* 12202 (RV 347) */
        if (S.F > 50 && g3 % 3 == 0) {                                         /* 12203 / 12204 */
            obj_t *s = splash(w); double sx = w == 0 ? S.s1A : S.s2A;
            if (s) { s->x = (float)sx; s->y = (float)(S.J + 9); s->hidden = 0; }
            obj_t *pc = piece_new((float)sx + (w == 0 ? -10 : 10), (float)(S.J + 9), w == 0 ? -10 : 10, 1);
            (void)pc; S.cC += 1;
        }
    }
    if (S.I == 0 && S.bF > 0 && S.cC > 0 && g3 % 3 == 0 && c) {                /* 12205-12210: the cannon fires */
        if (S.cC >= 10) snd_play_orig(303, 220, 128);                           /* 12209 (RV p5 572) */
        double fc = S.bC ? S.bC : 1;
        S.cC -= 1; S.cB = -10; S.cFiring = 1;
        splash_at(fc < 0 ? 0 : 1, c->x + (float)(fc * 24), c->y);              /* 12205 / 12206: the muzzle splash */
        piece_new(c->x + (float)(fc * 24), (float)(int)(c->y), 10 * fc, 0);
    }
    if (S.cB < 10) S.cB += 1;                                                   /* 12211 */
    if (S.cC <= 0) S.cFiring = 0;
    if (S.I == 0 && S.cB >= 10 && S.bF > 0) { S.bF = fmax(S.bF - 0.1, 0); if (S.bF < 1e-9) { S.bF = 0; S.F = 0; } }   /* 12212 */
    /* 12214: the mask walks; 12218-12220: the rig */
    m->x = (float)(floor(m->x) + S.bB); S.bA -= 4.5 * S.bB;
    m->y = (float)(int)(S.D + sbb_sin(S.bA) * 3 * S.bE + 8 * S.bF);
    rig();
    if (m->fl & TBB_DEF) {                                                      /* 12221: the walker beaten -> BREAK */
        S.ph = PH_BREAK; S.L = 0; S.I = 0; S.H = 0; S.bB = 0; S.bE = 0;
        for (int t = OT_TBB_SPIDER; t <= OT_TBB_SP_LEGB; t++) for (int k = 0; k < tbb_count(t); k++) { obj_t *o = tbb_get(t, k); if (!o) continue;
            o->ang = (float)(t == OT_TBB_SP_LEGF || t == OT_TBB_SP_BODY ? 30 * S.bC : -30 * S.bC); }   /* 12222: the parts tilt */
        for (int k = 0; k < NLF; k++) { obj_t *o = tbb_slot(S.lf[k]); if (o) o->alive = 0; S.lf[k] = -1; }
    }
}

/* ---------------------------------------------------------------- BREAK (12222-12226): explosions, then the parts fly off */
static void brk(void) {
    obj_t *m = mask(), *b = body(); if (!m) return;
    int g3 = tbb_g3();
    tbb_set_a(m, S.bC < 0 ? TS_SP_HEAD8_16 : S.bC > 0 ? TS_SP_HEAD8_0 : TS_SP_HEAD8_24);   /* 12223: anim 8 */
    if (b) tbb_set_a(b, S.bC < 0 ? TS_SP_BODY8_16 : S.bC > 0 ? TS_SP_BODY8_0 : TS_SP_BODY8_24);
    if (g3 % 7 == 0) {                                                          /* 12224: an explosion06 every 7 frames, L + 1 */
        float ex = b ? b->x : m->x, ey = b ? b->y : m->y;
        obj_effect(ex - 40 + cross_rand(81), ey - 30 + cross_rand(61), "explosion06"); tbb_explode_snd();
        S.L += 1;
    }
    if (S.L > 12) {                                                             /* 12225: the parts fly off; the mask goes on (A 30) */
        obj_effect(b ? b->x : m->x, b ? b->y : m->y, "explosion"); snd_play_orig(38, 220, 128);
        double C = S.bC ? S.bC : 1;
        int n = 0;
        for (int t = OT_TBB_SP_BODY; t <= OT_TBB_SP_LEGB; t++) for (int k = 0; k < tbb_count(t) && n < NPART; k++) {
            obj_t *o = tbb_get(t, k); if (!o) continue;
            S.pt[n] = (short)(o - G.o);
            S.pX[n] = t == OT_TBB_SP_LEGB ? -8 * C : t == OT_TBB_SP_CANNON ? 8 * C : t == OT_TBB_SP_BODY ? 6 * C : t == OT_TBB_SP_ARM ? 3 * C : -6 * C;
            S.pY[n] = t == OT_TBB_SP_LEGB || t == OT_TBB_SP_CANNON ? -4 : t == OT_TBB_SP_ARM ? -10 : -8;
            n++;
        }
        m->fl &= ~TBB_DEF; m->A = 30; m->fl |= TBB_ARM;                          /* A 30, armoured until it drops the spikes (12236) */
        S.K = 0; S.F = 0; S.L = 0; S.ph = PH_HANG; m->ang = 0;
        S.gX = floor(m->x); S.gY = floor(m->y); S.gA = 10;
    }
}
static void flyoff(void) {                                                      /* 12226 */
    for (int n = 0; n < NPART; n++) {
        obj_t *o = tbb_slot(S.pt[n]); if (!o) continue;
        o->x = (float)(int)(o->x + S.pX[n]); o->y = (float)(int)(o->y + S.pY[n]); S.pY[n] += 0.3;
        if (!obj_near_view(o, 100)) { o->hidden = 1; S.pt[n] = -1; }
    }
}

/* ---------------------------------------------------------------- HANG (12227-12249): the mask on the grapple line, the spikes */
static int sp_new(obj_t *m) {
    int k; for (k = 0; k < NSP && S.sp[k] >= 0 && tbb_slot(S.sp[k]); k++) ;
    if (k == NSP) return 0;
    obj_t *o = tbb_spawn(OT_TBB_SHOT, TS_SP_LAUNCH, (float)(floor(m->x) - 8 + cross_rand(17)), (float)(S.J + 96), 1);
    if (!o) return 0;
    o->par = 21; S.sp[k] = (short)(o - G.o); S.spC[k] = 0; S.spA[k] = 0;
    return 1;
}
static int sp_count(void) { int n = 0; for (int k = 0; k < NSP; k++) { obj_t *o = tbb_slot(S.sp[k]); if (o && o->type == OT_TBB_SHOT && o->par == 21) n++; else S.sp[k] = -1; } return n; }
static void hang(player_t *p) {
    obj_t *m = mask(), *gl = gline(); if (!m) return;
    int g3 = tbb_g3(), K = (int)S.K;
    double mid = (S.s1A + S.s2A) * 0.5;
    if (K != 0) { S.F += 1; }                                                   /* 12227 */
    if (K == 0) { S.F += 1; S.gB = mid; S.gC = S.J - 96; }                      /* 12227: the target: the middle, J - 96 */
    if ((K == 0 && S.F > 15) || K > 0) {                                        /* 12228: the mask eases to the target */
        S.gX = (14 * S.gX + S.gB) / 15; S.gY = (14 * S.gY + S.gC) / 15;
        m->x = (float)(int)S.gX; m->y = (float)(int)S.gY;
    }
    if (K == 0 && S.F == 30) tbb_set_a(m, TS_SP_HEAD24);                      /* 12229: facing the front */
    if (K == 0 && S.F > 40 && !S.snd52) { S.snd52 = 1; snd_play_orig(52, 220, 128); }   /* 12230 (RV p6 527) */
    if (gl && (K > 0 || S.F > 40)) {                                            /* 12231 / 12232: the line from the ceiling, A + 20 to 648 */
        gl->hidden = 0; gl->x = m->x; gl->y = m->y + 8;
        if (K < 6) S.gA = fmin(S.gA + 20, 648);
    }
    if (K == 0 && S.F > 70) { S.F = 0; S.K = 1; S.gB = S.s1A - 64; S.gFlip = 0; }   /* 12233 */
    K = (int)S.K;
    if (K == 1) {
        if (S.F == 30) snd_play_orig(305, 220, 128);                            /* 12235 */
        if (S.F > 30 && g3 % 6 == 0 && sp_count() < 8 && sp_new(m)) {          /* 12236: a spike launcher dropped, sound 71 */
            tbb_set_a(m, TS_SP_HEAD7); snd_play_orig(71, 220, 128); m->fl &= ~TBB_ARM;
        }
        if (S.F > 110) { S.F = 0; S.K = 2; S.gB = mid; tbb_set_a(m, TS_SP_HEAD24); }   /* 12238 */
    }
    for (int k = 0; k < NSP; k++) {                                             /* 12237 / 12294-12296 */
        obj_t *o = tbb_slot(S.sp[k]); if (!o || o->type != OT_TBB_SHOT || o->par != 21) { S.sp[k] = -1; continue; }
        if (S.spC[k] == 0) { o->y -= 8; if (o->y <= m->y + 16) { S.spA[k] = 20 * S.spL + 200; S.spL += 1; S.spC[k] = 1; tbb_set_a(o, TS_SP_LAUNCH); } }   /* rises to the mask */
        else if (S.spC[k] == 1) { o->x = m->x; o->y = m->y + 16; }             /* 12295: carried under the mask */
        else {                                                                  /* 12296: shot along A at B (12 px a frame: INFERRED) */
            double B = 12; S.spD[k] += B * sbb_cos(S.spA[k]); S.spE[k] -= B * sbb_sin(S.spA[k]);
            o->x = (float)(int)S.spD[k]; o->y = (float)(int)S.spE[k]; o->ang = (float)S.spA[k];
            if (obj_overlaps_bg(o) || !obj_near_view(o, 100)) { snd_play_orig(17, 220, 128); obj_effect(o->x, o->y, "explosion02"); o->alive = 0; S.sp[k] = -1; continue; }
            if (!dead(p) && sbb_csens_hit(p, o)) sbb_hurt(p, 5);              /* INFERRED: a harmful spike */
        }
        if (K == 2 && S.spC[k] == 1) {                                          /* 12240 / 12241: the fan from both ends toward the middle */
            if (S.F * 0.5 + 180 >= S.spA[k] || S.spA[k] >= 360 - S.F * 0.5) tbb_set_a(o, TS_SP_LAUNCH8);
            if (S.F * 0.5 + 160 >= S.spA[k] || S.spA[k] >= 380 - S.F * 0.5) {
                S.spC[k] = 2; S.spD[k] = o->x; S.spE[k] = o->y; snd_play_orig(28, 220, 128); tbb_set_a(o, TS_SP_LAUNCH7);
            }
        }
    }
    if (K == 2 && S.F > 240) {                                                  /* 12242 / 12243: back to a side, alternating */
        S.gFlip = !S.gFlip; S.gB = S.gFlip ? S.s2A + 64 : S.s1A - 64; S.K = 1; S.F = 0; S.spL = 0;
    }
    if ((m->fl & TBB_DEF) && S.K != 6) {                                        /* 12244: beaten: the launchers gone, K 6, music out */
        for (int k = 0; k < NSP; k++) { obj_t *o = tbb_slot(S.sp[k]); if (o) { obj_effect(o->x, o->y, "explosion02"); o->alive = 0; } S.sp[k] = -1; }
        S.K = 6; S.gB = mid; S.L = 0; tbb_set_a(m, TS_SP_HEAD8_24); sbb_music_fadeout();
    }
    if (S.K == 6) {
        S.gA = fmax(S.gA - 20, 0);                                              /* 12245 */
        if (S.L < 12 && g3 % 7 == 0) { obj_effect(m->x - 30 + cross_rand(61), m->y - 30 + cross_rand(61), "explosion06"); tbb_explode_snd(); }   /* 12246 */
        if (g3 % 7 == 0) S.L += 1;                                              /* 12247 */
        if (S.L > 2) S.gC -= S.L;                                               /* 12248: pulled up */
        if (S.L > 16) {                                                         /* 12249: the chase */
            m->hidden = 1; if (gl) gl->hidden = 1;
            S.K = 0; S.F = 0; S.ph = PH_CHASE; snd_play_orig(310, 220, 128);
            tbb_heat_show(0);                                                   /* 12249: the heat sky shown, alpha 0, scale 43 x 24 */
            level_release_camera(0);
            S.camA = G.cam.A; S.camB = G.cam.B; S.cam = 1;
        }
    }
}

/* ---------------------------------------------------------------- the heat sky during the chase (session 26; RE 12249-12268)
 * LevelHeatSky_2779's alpha byte (tb_boss.c tbb_heat; additive over the view): 12249 0, shown; 12252 (K 0) + 1 to 255; 12260 (K 1):
 * d = |HazardLavaFall3.x - view x| / 2 for the list's back instance (Frame::add_object stores the newest in ObjectList +0: the last
 * placed, the front column at 1983), 55 if d > 200, 235 if d < 20, else 255 - trunc(d); K 2 with X <= LevelLavaCascadeEnd.x + 49:
 * 12266 (alpha > 55) - 2 down to 55, 12267 (alpha < 55) + 1; 12268 (X past it) - 1 down to 0. */
static void heat_wall(void) {
    obj_t *h = tbb_heat(), *w = S.nl3 ? tbb_slot(S.l3[S.nl3 - 1]) : NULL;
    if (!h || !w) return;
    double d = abs((int)floorf(w->x) - (int)G.camx) * 0.5;
    h->av[0] = d > 200 ? 55 : d < 20 ? 235 : fmax(0, fmin(255, 255 - (int)d));
}
static void heat_end(int X) {
    obj_t *h = tbb_heat(), *c = tbb_get(OT_TBB_CASCEND, 0);
    if (!h || !c) return;
    int a = (int)h->av[0], cx = (int)floorf(c->x) + 49;
    if (255 - a < 200 && X <= cx) { int k = 257 - a; a = 255 - (k > 200 ? 200 : k); }   /* 12266 */
    if (255 - a > 200 && X <= cx) a = 254 - a > 199 ? a + 1 : 55;                    /* 12267 */
    if (cx < X) a = a ? a - 1 : 0;                                                     /* 12268 */
    h->av[0] = a;
}

/* ---------------------------------------------------------------- CHASE (12250-12279) */
static void chase(player_t *p) {
    int g3 = tbb_g3(), K = (int)S.K;
    double PX = sbb_px(p), PY = sbb_py(p);
    if (K == 0) {                                                               /* 12252: the camera looks back at the wall (2128, 3936) */
        S.F += 1;
        S.camA = (S.camA * 19 + 2128) * 0.05; S.camB = (S.camB * 19 + 3936) * 0.05;
        G.cam.M3 = 1; G.cam.A = (float)S.camA; G.cam.B = (float)S.camB;
        camera_shake((int)fmin(S.F * 0.05, 6));
        obj_t *h = tbb_heat(); if (h) h->av[0] = fmin(h->av[0] + 1, 255);        /* 12252: the heat sky's alpha byte + 1 (to 255) */
    }
    if ((K == 0 && S.F > 180) || K > 0) {                                       /* 12253: the wall comes at 10 px a frame */
        for (int k = 0; k < S.nl3; k++) { obj_t *o = tbb_slot(S.l3[k]); if (o) o->x += 10; }
        if (!S.chase_snd) { S.chase_snd = 1; snd_play_orig(309, 220, 128); }   /* 12254 (RV p7 1394) */
        if (g3 % 5 == 0) snd_play_orig(99, 220, 128);                          /* 12254: the rumble every 5 frames */
    }
    if (K == 0 && S.F > 230) {                                                  /* 12258: the run: the camera back on the player */
        S.K = 1; G.cam.M3 = 0; G.cam_m2 = 1; S.cam = 0;
        if (tb_gim_blockers) { tb_gim_blockers(0); S.blk = 1; }                /* 12260 (the gimmick module: passable) */
    }
    K = (int)S.K;
    if (K == 1) heat_wall();                                                    /* 12260 */
    if (K == 1 && PY > 4288) {                                                  /* 12262: down in the end room */
        S.K = 2; if (tb_gim_blockers) tb_gim_blockers(1);
    }
    if ((int)S.K == 2) heat_end((int)PX);                                       /* 12266-12268 */
    if (K == 1 && !dead(p) && p->face > 0 && (tbb_held & BTN_RIGHT)) {        /* 12261: running right with the wall close behind: B + 0.1 to 11 */
        int close = 0;
        for (int k = 0; k < S.nl3 && !close; k++) { obj_t *o = tbb_slot(S.l3[k]); if (o && o->x + 4 >= PX - 239) close = 1; }
        double B = p->ground ? p->gsp : p->vx;
        if (close && B < 11) { if (p->ground) p->gsp = (float)(B + 0.1); else p->vx = (float)(B + 0.1); }
    }
    if (K > 1) for (int k = 0; k < S.nl3; k++) { obj_t *o = tbb_slot(S.l3[k]); if (o && o->x > PX - 300) o->x -= 1280; }   /* 12269 (INFERRED gate) */
    /* 12271-12279: the wall on the player: burn, carried forward (B 11, G 4) */
    if (S.l3E > 0) S.l3E -= 1;
    if (K == 1 && !dead(p) && PX <= 7488) {
        float front = -1e9f, top = 1e9f, bot = -1e9f;                          /* the wall: x-scaled 4 (12249): from x - 316 to x + 4 */
        for (int k = 0; k < S.nl3; k++) { obj_t *o = tbb_slot(S.l3[k]); if (!o) continue; front = fmaxf(front, o->x + 4); top = fminf(top, o->y - 32); bot = fmaxf(bot, o->y + 32); }
        if (PX <= front && PY >= top && PY <= bot && S.l3E <= 0 && player_burn(p, 3)) S.l3E = 5;
    }
    if (S.l3E > 1 && !dead(p)) {                                                /* 12277 */
        if (p->ground) { p->vx = p->gsp; p->ground = 0; }
        p->vx = fmaxf(p->vx, 11.f); p->vy = fmaxf(p->vy, 4.f);
    }
}

void tbp_frame(player_t *p) {
    if (!S.on) return;
    columns(p);
    pieces(p);
    if (S.ph <= PH_INTRO) intro(p);
    else if (S.ph == PH_WALK) walk(p);
    else if (S.ph == PH_BREAK) { brk(); }
    if (S.ph == PH_HANG) { flyoff(); hang(p); }
    else if (S.ph == PH_CHASE) { flyoff(); chase(p); }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBP")) {
        obj_t *m = mask();
        float wx = -1; for (int k = 0; k < S.nl3; k++) { obj_t *o = tbb_slot(S.l3[k]); if (o && o->x > wx) wx = o->x; }
        printf("tbp %d px %.0f py %.0f ph %d K %.0f O %.0f x %.0f y %.0f A %d H %.0f I %.0f F %.0f L %.0f bB %.1f bC %.0f bF %.1f cB %.0f cC %.0f cD %.0f lf %d def %d wall %.0f nl3 %d nlc %d\n",
            G.frame, sbb_px(p), sbb_py(p), S.ph, S.K, S.O, m ? m->x : -1, m ? m->y : -1, m ? (int)m->A : -1, S.H, S.I, S.F, S.L, S.bB, S.bC, S.bF, S.cB, S.cC, S.cD,
            pieces_alive(), m && (m->fl & TBB_DEF) ? 1 : 0, wx, S.nl3, S.nlc);
    }
#endif
}

int tbp_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type >= OT_TBB_SPIDER && o->type <= OT_TBB_SPLASH) {
        if (o->hidden) return 1;
        if (o->type == OT_TBB_SP_GRAPPLE) {                                     /* the line: x-scaled by A / 10 (12228) */
            const frame_t *f = o->an.b ? anim_frame(&o->an) : NULL;
            if (f) { float h = (float)fmax(S.gA, 1); frame_draw_scaled(o->an.b, f, floorf(o->x - G.camx) - f->w / 2, floorf(o->y - G.camy) - h, (float)f->w, h, 0xffffffff); }
            return 1;
        }
        sbb_draw(o, 0xffffffff); return 1;
    }
    return 0;
}
