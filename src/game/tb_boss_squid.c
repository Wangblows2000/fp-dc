/* TB3: the RoboSquid in the flooded loop room (BossRoboSquid_2765 with its Glass_2764 (the key card inside), Jet_2766 and 2 x 7 arm
 * balls (LeftArm_2762 / RightArm_2763); EnemyHomingBullet_2648 shots). RE handle_frame_47_events 12095-12128 (+ the shared bullet
 * events 11781 / 11782 and the collision col_21_44); RV build/orig_traces/tb_bosses q2_l (Lilac running the loop: the swim and the
 * shots), q3_l (A set to 0 at 300 and 800: both defeats). spec_tb_bosses.md Part D. No character test (RE).
 * Squid alterables: A HP (50; 30 after the first defeat), D the bob / arm angle (+ F a frame), E 40 / G 30 / H 30 / I 150 the arm
 * wave, J / K its base position (x = J, y = K + cos(2D) Q), L the x speed (J + L a frame, decays 0.1 a frame to 0), N the swim
 * direction, O the phase (0 swimming, 1 the first defeat, 2 fleeing), P a counter, R 1 when destroyed, T the debris count, U the
 * shots left. Flag 2 (0x4 here: act) set once it is within 150 px of the view (12097).
 * - 12098: x = J, y = K + cos(2D) Q, then J + L; the glass follows (glass B = O, C = R: 12099).
 * - 12111: every 100 frames (global 3) with O 0: U = 2. 12112 / 12113: every 30 frames (15 once fleeing) with U > 0, in the view
 *   (100), alive and N = -1 / 1: L = -4 / 4, the jet behind it (A 30 / -30, B -10, angle 315 / 215, anim 7 shown), a
 *   homing bullet from the jet's last position + / - 30, y - 10, direction 28 / 20 at speed 50 (6.25 px a frame), sound 28, U - 1.
 * - 12109 / 12110: past the glass's D (its start x) + / - 240 it turns (N = -N); fleeing (O > 0): U = 4 (RV q3 423 / 547).
 * - 12116: the first defeat (A 0: the shared 2764) with O 0: anim 10, O 1, the key card dropped (A -1, B -4: the gimmick module's
 *   falling card), sound 134, T 5 -> five debris (frame 35 + T, direction 2T + 4, speed 50 + R(30)). 12122: anim 10 frame 3: two
 *   explosion01 (x -/+ 30, y - 10, scale 2). 12121 / 12123: P + 1 a frame; past 60: alive again, A 30, F 8, Q 2, O 2.
 *   12124: O >= 1 and alive: anim 11, L = 4 N (it swims on without stopping).
 * - 12126-12128: the second defeat (O 2): anim 8, an explosion06 every 7 frames at x - 30 + R(61), y - 40 + R(81) (P + 1; the
 *   explosion sound), at P 18: hidden, R 1, sound 38, explosion01; the arms fly off (12102: x + X, y + Y, Y + 0.2; hidden past the
 *   view + 50: 12103), the glass hidden (12100). The squid does not fly away (no fly-away: RV q3, it stays in place).
 * Arms (12101 / 12104), segment A = 1..7 (placement order): u = D - G A, v = sin(u) E + I (left) / + H (right), x = squid x + cos(v)
 * A (20 - A) -/+ 10, y = squid y + 25 + sin(v) A (20 - A) (RV q2 f0: the + 25 fits all seven); X / Y = (arm - squid) / 10, Y - 5. */
#include "tb_boss.h"
#include <stdio.h>
#include <stdlib.h>

extern obj_t *tb_gim_key_drop(float x, float y) __attribute__((weak));        /* the gimmick module's dropped ObjectKeyCard (A -1) */

#define NARM 7
#define NBUL 16
static struct {
    int on, act, O, R, T, U, N, fl8, jet_vis, gone;
    double D, F, Q, J, K, L, P, home, jA, jB, jang;
    short arm[2][NARM]; double ax[2][NARM], ay[2][NARM]; char afl[2][NARM], afree[2];
    short bul[NBUL];
} S;

static obj_t *squid(void) { return tbb_get(OT_TBB_SQUID, 0); }
static obj_t *glass(void) { return tbb_get(OT_TBB_SQ_GLASS, 0); }
static obj_t *jet(void) { return tbb_get(OT_TBB_SQ_JET, 0); }
int tb_boss_squid_state(void) { return S.on ? (S.gone ? 3 : S.O) : -1; }

void tbq_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    obj_t *s = squid(), *g = glass(), *j = jet();
    if (!s) return;
    S.on = 1;
    tbb_set_a(s, TS_SQ); s->A = 50; s->fl |= F_NOFLY; s->bd = 0;
    S.F = 5; S.Q = 5; S.J = s->x; S.K = s->y; S.N = 1; S.jA = 30; S.jB = -10;   /* ctor: E 40 F 5 G 30 H 30 I 150 J / K the placement, N 1, Q 5 (RV q2 f0) */
    S.home = g ? g->x : s->x;                                                    /* glass D = its x (RV q3 f0: 4192) */
    if (g) tbb_set_a(g, TS_SQG);
    if (j) { tbb_set_a(j, TS_SQJ); j->hidden = 1; }
    for (int side = 0; side < 2; side++)
        for (int k = 0; k < NARM; k++) {
            obj_t *a = tbb_get(side ? OT_TBB_SQ_ARMR : OT_TBB_SQ_ARML, k);
            S.arm[side][k] = a ? (short)(a - G.o) : -1;
            if (a) tbb_set_a(a, TS_SQARM);
        }
    for (int k = 0; k < NBUL; k++) S.bul[k] = -1;
}

static void shoot(obj_t *j, int n) {                                            /* 12112 (n -1) / 12113 (n 1) */
    S.L = n < 0 ? -4 : 4;
    float bx = j ? j->x : 0, by = j ? j->y : 0;                                  /* the jet's position before this frame's 12114 */
    S.jA = n < 0 ? 30 : -30; S.jB = -10; S.jang = n < 0 ? 315 : 215;
    if (j) { j->hidden = 0; j->ang = (float)S.jang; tbb_set_a(j, TS_SQJ7); anim_restart(&j->an); }
    S.jet_vis = 1;
    int k; for (k = 0; k < NBUL && tbb_slot(S.bul[k]) && tbb_slot(S.bul[k])->type == OT_TBB_SHOT; k++) ;
    if (k < NBUL && j) {
        obj_t *b = tbb_spawn(OT_TBB_SHOT, TS_SQHB, bx + (n < 0 ? 30 : -30), by - 10, j->layer);
        if (b) {
            double a = (n < 0 ? 28 : 20) * 11.25, v = 50 / 8.0;                 /* BallMovement direction 28 / 20, speed 50 (px = speed / 8) */
            b->par = 10; b->av[0] = v * sbb_cos(a); b->av[1] = -v * sbb_sin(a); b->fx = b->x; b->fy = b->y;
            S.bul[k] = (short)(b - G.o);
        }
    }
    snd_play_orig(28, 220, 128);
    S.U -= 1;
}

static void bullets(player_t *p) {                                              /* 11781 / 11782 / col_21_44 */
    for (int k = 0; k < NBUL; k++) {
        obj_t *b = tbb_slot(S.bul[k]);
        if (!b || b->type != OT_TBB_SHOT || b->par != 10) { S.bul[k] = -1; continue; }
        b->fx += (float)b->av[0]; b->fy += (float)b->av[1]; b->x = floorf(b->fx); b->y = floorf(b->fy);
        b->ang = (float)fmod(b->ang + 30.0, 360.0);                              /* 11781: angle + 30 a frame */
        if (!obj_near_view(b, 40)) { b->alive = 0; S.bul[k] = -1; continue; }     /* 11782 */
        if (obj_overlaps_bg(b)) { snd_play_orig(17, 220, 128); obj_effect(b->x, b->y, "explosion02"); b->alive = 0; S.bul[k] = -1; continue; }   /* col_21_44 */
        if (sbb_csens_hit(p, b) && sbb_hurt(p, 5)) {                            /* the player's hurt (INFERRED: as SB's EnemyBullet) */
            snd_play_orig(17, 220, 128); obj_effect(b->x, b->y, "explosion02"); b->alive = 0; S.bul[k] = -1;
        }
    }
}

static void arms(const obj_t *s) {
    for (int side = 0; side < 2; side++)
        for (int k = 0; k < NARM; k++) {
            obj_t *a = tbb_slot(S.arm[side][k]);
            if (!a) continue;
            if (!S.R) {                                                         /* 12101 / 12104: attached (B 0) */
                double A = k + 1, u = S.D - 30 * A, v = sbb_sin(u) * 40 + (side ? 30 : 150), r = A * (20 - A);
                double x = sbb_cos(v) * r + floorf(s->x) + (side ? 10 : -10), y = sbb_sin(v) * r + floorf(s->y) + 25;
                a->x = (float)(int)x; a->y = (float)(int)y;
                S.ax[side][k] = (a->x - floorf(s->x)) * 0.1; S.ay[side][k] = (a->y - floorf(s->y)) * 0.1 - 5;
            } else if (!S.afl[side][k]) {                                       /* 12102 / 12103 */
                a->x = (float)(int)(a->x + S.ax[side][k]); a->y = (float)(int)(a->y + S.ay[side][k]); S.ay[side][k] += 0.2;
                if (!obj_near_view(a, 50)) { S.afl[side][k] = 1; a->hidden = 1; }
            }
        }
}

void tbq_frame(player_t *p) {
    if (!S.on) return;
    obj_t *s = squid(), *g = glass(), *j = jet();
    bullets(p);
    if (!s) return;
    int g3 = tbb_g3(), def = (s->fl & TBB_DEF) != 0;
    if (!S.act && obj_near_view(s, 150)) S.act = 1;                             /* 12097 */
    if (S.act) {                                                                /* 12098 */
        S.D = fmod(S.D + S.F, 360.0);
        s->x = (float)(int)S.J; s->y = (float)(int)(sbb_cos(2 * S.D) * S.Q + S.K);
        S.J += S.L;
        if (g) { g->x = s->x; g->y = s->y; }
    }
    if (g) { if (S.O >= 1) tbb_set_a(g, TS_SQG1); g->hidden = S.R ? 1 : 0; }     /* 12099 / 12100 / 12125 */
    if (S.act) arms(s);
    if (S.L < 0) S.L = fmin(S.L + 0.1, 0); else if (S.L > 0) S.L = fmax(S.L - 0.1, 0);   /* 12107 / 12108 */
    if ((S.N > 0 && s->x > S.home + 240) || (S.N < 0 && s->x < S.home - 240)) {   /* 12109 / 12110 */
        S.N = -S.N; if (S.O > 0) S.U = 4;
    }
    if (S.O == 0 && g3 % 100 == 0) S.U = 2;                                     /* 12111 */
    if (S.act && !def && S.U > 0 && obj_near_view(s, 100) && ((S.O == 0 && g3 % 30 == 0) || (S.O > 0 && g3 % 15 == 0))) shoot(j, S.N);   /* 12112 / 12113 */
    if (j) {                                                                    /* 12114 / 12115 */
        j->x = (float)(int)(s->x + S.jA); j->y = (float)(int)(s->y + S.jB);
        if (S.jet_vis && tbb_a_is(j, TS_SQJ7) && j->an.done) { S.jet_vis = 0; j->hidden = 1; }
        if (S.R) j->hidden = 1;
    }
    if (def && S.O == 0) {                                                      /* 12116 */
        tbb_set_a(s, TS_SQ10); anim_restart(&s->an); S.O = 1;
        if (tb_gim_key_drop) { obj_t *k = tb_gim_key_drop(s->x, s->y); if (k) k->av[1] = -4; }
        snd_play_orig(134, 220, 128); S.T = 5;
    }
    if (S.T > 0) {                                                              /* 12117 */
        obj_t *d = obj_alloc(); int a = ens_anim("debris");
        if (d && a >= 0) {
            d->type = OT_DEBRIS; d->alive = 1; d->x = s->x; d->y = s->y; anim_set(&d->an, &G.ens, a); d->an.frame = (int)(35 + S.T);
            double dir = (2 * S.T + 4) * 11.25, v = (50 + cross_rand(30)) / 8.0;
            d->vx = (float)(v * sbb_cos(dir)); d->vy = (float)(-v * sbb_sin(dir));
        }
        S.T -= 1;
    }
    if (def && S.O == 1) S.P += 1;                                              /* 12121 */
    if (S.O == 1 && !S.fl8 && tbb_a_is(s, TS_SQ10) && s->an.frame == 3) {      /* 12122 */
        S.fl8 = 1;
        obj_effect(s->x - 30, s->y - 10, "explosion"); obj_effect(s->x + 30, s->y - 10, "explosion");   /* (scale 2: drawn at 1) */
    }
    if (def && S.O == 1 && S.P > 60) { s->fl &= ~TBB_DEF; s->A = 30; S.P = 0; S.F = 8; S.Q = 2; S.O = 2; def = 0; }   /* 12123 */
    if (S.O >= 1 && !def) { tbb_set_a(s, TS_SQ11); S.L = 4 * S.N; }            /* 12124 */
    if (def && S.O == 2) {                                                      /* 12126-12128 */
        tbb_set_a(s, TS_SQ8);
        if (S.P < 18 && g3 % 7 == 0) {
            obj_effect(s->x - 30 + cross_rand(61), s->y - 40 + cross_rand(81), "explosion06"); tbb_explode_snd(); S.P += 1;
        }
        if (S.P >= 18 && !S.R) {
            S.R = 1; S.gone = 1; s->hidden = 1; snd_play_orig(38, 220, 128); obj_effect(s->x, s->y, "explosion");
        }
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBQ")) printf("tbq %d px %.0f py %.0f x %.0f y %.0f J %.1f L %.1f N %d O %d P %.0f U %d A %d R %d def %d act %d\n", G.frame,
        sbb_px(p), sbb_py(p), s->x, s->y, S.J, S.L, S.N, S.O, S.P, S.U, (int)s->A, S.R, def, S.act);
#endif
}

int tbq_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type >= OT_TBB_SQUID && o->type <= OT_TBB_SQ_ARMR) { if (!o->hidden) sbb_draw(o, 0xffffffff); return 1; }
    return 0;
}
