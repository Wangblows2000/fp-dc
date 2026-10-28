/* TB6: the arena boss (Syntax's squid rig: BossSyntaxSquidBack_2887, ShoulderRight_2888, ShoulderLeft_2889, Head_2890, 7 x ArmLeft_2891,
 * 7 x ArmRight_2892, Body_2893, ClawRight_2894, ClawLeft_2895, Hitbox_2902 (Q20, A 50)), its attacks (BossLavaLaser_2896, 3 x
 * BossSpikeSphere_2897 + 3 x BossSpikeSphere2_2898, BossLavaSplashLeft1/Right1/Left2/Right2_2899/2901/2903/2904, BossLavaSplashMain_2900),
 * the arena floor (26 x BossTBPlatform_2886, 7 x BossTBPlatform2_2908: solid, heated by the lava) and 5 x BossWaypoint_2905.
 * RE handle_frame_50_events 12565-12726 (group 0x22c9d the fight from 12571, 0x22c9c the end from 12704), constructors, col_9_49 /
 * col_10_49; RV build/orig_traces/tb_bosses x1_l (Lilac, no damage: intro 393-650, the move / attack cycle), x2_l (three rounds by the
 * hitbox's A, the end and the results). spec_tb_bosses.md Part E. No character test in these events (RE): the same fight for all.
 * Body alterables (ctor E 30, F 5, G 15, H 45, I 135, J 1, M 20, N -1, R 1, S -15): A / B the rig's centre, C its rotation (0 upright),
 * D the bob phase (+F a frame), E / G / H / I the arms' swing, phase step and rest angles, J the arms' reach, K the claw spin mode
 * (N 4), M the bob radius (20; 0 while attacking), N the state (-1 the intro, 0 swim to a waypoint, 1 lava pillars, 2 shoulder lava,
 * 3 spike spheres (after a stun), 4 claw spin, 5 the dive), O the state's frame count, P / Q the target, R the next state, S / U the
 * dive's speeds, T the waypoint index. Hitbox: A its HP (50 a round; 60 the third), D 1 stunned, E the rounds won by the player
 * (3: the defeat), F / G the end's timers. */
#include "tb_boss.h"
#include <stdio.h>
#include <stdlib.h>

enum { P_BACK, P_SHR, P_SHL, P_HEAD, P_BODY, P_CLAWR, P_CLAWL, P_N };          /* OT_TBB_SX_PART members (importer TBB_SUB) */
#define NARM 7
static struct {
    int on, fight, end, results, lock, music, voice, toggle;
    double A, B, C, D, E, F, G, H, I, J, K, M, N, O, P, Q, R, S, T, U;          /* body */
    double hA, hD, hE, hF, hG;                                                   /* hitbox */
    int sph_d[3]; double sph_g[3], sph_y[3], sph_home[3], sph_ang[3];            /* spike spheres D / G / F / I / sphere2 E */
    int claw_cd[2];                                                              /* claw A (12608-12611) */
    float hud;                                                                   /* the HUD's shift (12575 / 12581 / 12713) */
    int splash_t[4];                                                             /* splash left2 / right2 B (12654-12659), left1 / right1 shown */
} X;
static obj_t *part(int m) { for (int k = 0; k < tbb_count(OT_TBB_SX_PART); k++) { obj_t *o = tbb_get(OT_TBB_SX_PART, k); if (o && o->par == m) return o; } return NULL; }
static obj_t *hitbox(void) { return tbb_get(OT_TBB_SX_HIT, 0); }
static obj_t *arm(int side, int a) {                                              /* arm segment with index A = a (1..7): placement order 7 - a (RV x1_l: A 7 first) */
    int t = side ? OT_TBB_SX_ARMR : OT_TBB_SX_ARML, n = tbb_count(t);
    return tbb_get(t, n - a);
}
static obj_t *sphere(int k, int big) {                                         /* par 0: BossSpikeSphere k (placement order); par 1: the BossSpikeSphere2 at its x */
    int n = 0; obj_t *s = NULL;
    for (int j = 0; j < tbb_count(OT_TBB_SX_SPHERE); j++) { obj_t *o = tbb_get(OT_TBB_SX_SPHERE, j); if (o && o->par == 0 && n++ == k) { s = o; break; } }
    if (!s || !big) return s;
    for (int j = 0; j < tbb_count(OT_TBB_SX_SPHERE); j++) { obj_t *o = tbb_get(OT_TBB_SX_SPHERE, j); if (o && o->par == 1 && (int)o->ox == (int)s->ox) return o; }
    return NULL;
}
static obj_t *splash(int m) { for (int k = 0; k < tbb_count(OT_TBB_SX_SPLASH); k++) { obj_t *o = tbb_get(OT_TBB_SX_SPLASH, k); if (o && o->par == m) return o; } return NULL; }
int tb_boss_syntax_state(void) { return !X.on ? -100 : X.end ? 10 + (int)X.hG : (int)X.N; }
int tb_boss_syntax_rounds(void) { return X.on ? (int)X.hE : -1; }

/* ======================================================================== the arena floor (12684-12697, col_9_49 / col_10_49, 12705)
 * BossTBPlatform / Platform2: A the heat (30 when a lava blob lands on it, -0.1 a frame to 0); A < 5: frame A; A >= 5 the hot flicker
 * (frames 5 / 6 by g3 / 2); standing on a hot one (J: + 3 a frame on it, 0 off it) for J > 80 hurts 5. Flag 1: a floor piece (y >=
 * the bottom marker - 99, 12684). */
static void plat_frame(player_t *p) {
    for (int t = OT_TBB_PLAT; t <= OT_TBB_PLAT2; t++)
        for (int k = 0; k < tbb_count(t); k++) {
            obj_t *o = tbb_get(t, k); if (!o) continue;
            if (X.fight || X.end) o->A = fmax(0, o->A - 0.1);                    /* 12705 (the fight group) */
            int hot = o->A >= 5;
            if (o->an.b) o->an.frame = hot ? 5 + ((tbb_g3() / 2) & 1) : (int)o->A;   /* 12685 / 12686 */
            int on = p->ground && obj_rect_hit(o, (int)p->x - 6, (int)p->y - 2, (int)p->x + 7, (int)p->y + 3);
            if (on && hot) { o->av[0] += 3; if (o->av[0] > 80) { sbb_hurt(p, 5); o->av[0] = 0; } }   /* 12687-12690 (INFERRED: J + 3 while on it) */
            else if (!on) o->av[0] = 0;                                         /* 12689 */
        }
}
static void plat_heat(const obj_t *blob) {                                      /* col_9_49 / col_10_49: a lava blob on a platform -> A 30 */
    for (int t = OT_TBB_PLAT; t <= OT_TBB_PLAT2; t++)
        for (int k = 0; k < tbb_count(t); k++) { obj_t *o = tbb_get(t, k); if (o && obj_obj_hit(blob, o)) o->A = 30; }
}

/* ======================================================================== lava blobs (BossLavaLaser_2896, ctor A 12: y += A a frame, 12705)
 * Hurt 3 on the character sensor (12707, E 5 the blob's own wait), heat the platform they touch, a splash in the lava (12665). */
static void blob_spawn(float x, float y, double A) {
    obj_t *b = tbb_spawn(OT_TBB_SHOT, TS_SX_LASER, x, y, 1);
    if (b) { b->A = A; b->par = 0; }
}
static void blobs_frame(player_t *p, float lava_y) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_TBB_SHOT || b->par != 0) continue;
        if (b->born == G.frame) continue;                                       /* created objects move from the next frame (RE engine fact) */
        b->y += (float)b->A;
        plat_heat(b);
        if (b->av[0] > 0) b->av[0] -= 1;
        if (b->av[0] <= 0 && sbb_csens_hit(p, b) && sbb_hurt(p, 3)) b->av[0] = 5;   /* 12707 */
        if ((b->A > 0 && b->y > lava_y + 16) || b->y < G.camy - 300 || b->y > G.lvl.h) {   /* 12665: into the lava -> a splash */
            if (b->A > 0) obj_effect(b->x, lava_y, "tbx_splash");
            b->alive = 0;
        }
    }
}

/* ======================================================================== spike spheres (12638-12645, 12681 / 12682, 12710) */
static void spheres_frame(player_t *p, float lava_y) {
    for (int k = 0; k < 3; k++) {
        obj_t *s = sphere(k, 0), *s2 = sphere(k, 1);
        if (!s) continue;
        if (X.sph_d[k]) {                                                       /* 12643: the jump (G + 0.2, F + G), the spikes turn */
            X.sph_g[k] += 0.2; X.sph_y[k] += X.sph_g[k];
            if (X.sph_g[k] > 0 && X.sph_y[k] >= X.sph_home[k]) { X.sph_d[k] = 0; X.sph_y[k] = X.sph_home[k]; }   /* 12645 */
            float py = s->y; s->y = (float)(int)X.sph_y[k];
            if ((py > lava_y) != (s->y > lava_y)) { snd_play_orig(s->y < lava_y ? 252 : 251, 220, 128); obj_effect(s->x, lava_y, "tbx_splash"); }   /* 12681 / 12682 (RV x2_l: 252 out, 251 in) */
        }
        X.sph_ang[k] = sbb_nrm(X.sph_ang[k] + (X.sph_d[k] ? 10 : 2));           /* 12644 (INFERRED turn rate) */
        if (s2) { s2->x = s->x; s2->y = s->y; s2->ang = (float)X.sph_ang[k]; }
        if (s2 && X.sph_d[k] && sbb_csens_hit(p, s2)) {                        /* 12710: hurt 5, bounced */
            if (sbb_hurt(p, 5)) { p->vy = -3; }
        }
    }
}
static void sphere_launch(int k) {
    if (k < 0 || k > 2 || X.sph_d[k]) return;
    X.sph_d[k] = 1; X.sph_y[k] = X.sph_home[k]; X.sph_g[k] = -15;              /* 12638-12640: D 1, F = I, G -15 */
}

/* ======================================================================== the rig (12583-12590, 12612 / 12613, 12698) */
static void rig_place(void) {
    obj_t *body = part(P_BODY);
    if (!body) return;
    double ph = X.D - 45;
    body->x = (float)(int)(X.A + sbb_sin(ph) * X.M * X.J); body->y = (float)(int)(X.B + sbb_cos(ph) * X.M * X.J);   /* 12583 */
    float ang = (float)sbb_nrm(X.C);
    int same[] = { P_BACK, P_HEAD, P_BODY };
    for (int k = 0; k < 3; k++) { obj_t *o = part(same[k]); if (o) { o->x = body->x; o->y = body->y; o->ang = ang; } }
    for (int s = 0; s < 2; s++) {                                               /* shoulders: the rotation -+ 0.4 sin(D - 45) M J */
        obj_t *sh = part(s ? P_SHL : P_SHR);
        if (sh) { sh->x = body->x; sh->y = body->y; sh->ang = (float)sbb_nrm(X.C + (s ? 0.4 : -0.4) * sbb_sin(ph) * X.M * X.J); }
    }
    float ax, ay; tbb_act_point(body, &ax, &ay);
    for (int side = 0; side < 2; side++)                                        /* 12584 / 12585: segment a at (cos u * a (30 - a) -+ 10) J */
        for (int a = 1; a <= NARM; a++) {
            obj_t *o = arm(side, a); if (!o) continue;
            double u = side ? sbb_sin(X.D - X.G * a) * X.E + X.H - X.C : sbb_sin(X.D - 180 - X.G * a) * X.E + X.I - X.C;
            double r = a * (30 - a);
            if (X.K == 1) u = side ? sbb_nrm(X.D + 180) : X.D, r = 60 * a / NARM; /* INFERRED (12587 / 12588 not ported): the spin keeps the arms on a ring */
            o->x = (float)(int)((sbb_cos(u) * r + (side ? 10 : -10)) * X.J + ax);
            o->y = (float)(int)(sbb_sin(u) * r * X.J + ay);
            if (a == NARM) {                                                    /* 12612 / 12613: the claw on the last segment, turned with it */
                obj_t *c = part(side ? P_CLAWR : P_CLAWL);
                if (c) { c->x = o->x; c->y = o->y; c->ang = (float)sbb_nrm(u - (side ? 45 : 135) - X.C + X.C); }
            }
        }
    obj_t *h = hitbox(), *hd = part(P_HEAD);
    if (h && hd) { h->x = hd->x; h->y = hd->y; h->ang = hd->ang; }             /* 12698 */
}

static void parts_hidden(int hid) {
    for (int k = 0; k < tbb_count(OT_TBB_SX_PART); k++) { obj_t *o = tbb_get(OT_TBB_SX_PART, k); if (o && o->par != P_BODY) o->hidden = hid; }
    for (int s = 0; s < 2; s++) for (int a = 1; a <= NARM; a++) { obj_t *o = arm(s, a); if (o) o->hidden = hid; }
}

void tbx_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&X, sizeof X); }
    memset(&X, 0, sizeof X);
    obj_t *body = part(P_BODY), *h = hitbox();
    for (int t = OT_TBB_PLAT; t <= OT_TBB_PLAT2; t++)
        for (int k = 0; k < tbb_count(t); k++) { obj_t *o = tbb_get(t, k); if (o) { tbb_set_a(o, TS_PLAT); o->A = 0; } }
    if (!body || !h) return;
    X.on = 1;
    X.A = body->x; X.B = body->y; X.E = 30; X.F = 5; X.G = 15; X.H = 45; X.I = 135; X.J = 1; X.M = 20; X.N = -1; X.R = 1; X.S = -15;   /* ctor (RV x1_l f60) */
    X.hA = 50;
    tbb_set_a(h, TS_SX_HIT); h->A = 50; h->hidden = 1; h->fl |= F_NOFLY; h->c = 5;
    static const int PS[P_N] = { TS_SX_BACK, TS_SX_SHR, TS_SX_SHL, TS_SX_HEAD, TS_SX_BODY, TS_SX_CLAWR, TS_SX_CLAWL };
    for (int m = 0; m < P_N; m++) tbb_set_a(part(m), PS[m]);
    for (int s = 0; s < 2; s++) for (int a = 1; a <= NARM; a++) tbb_set_a(arm(s, a), s ? TS_SX_ARMR : TS_SX_ARML);
    for (int k = 0; k < 3; k++) {
        obj_t *s = sphere(k, 0), *s2 = sphere(k, 1);
        if (s) { tbb_set_a(s, TS_SX_SPH); X.sph_home[k] = X.sph_y[k] = s->y; }
        if (s2) tbb_set_a(s2, TS_SX_SPH2);
    }
    for (int m = 0; m < 4; m++) { obj_t *o = splash(m); if (o) { tbb_set_a(o, m < 2 ? TS_SX_SPL1 : TS_SX_SPL2); o->hidden = 1; } }
    rig_place();
}

void tbx_frame(player_t *p) {
    if (!X.on) return;
    obj_t *body = part(P_BODY), *h = hitbox(), *hd = part(P_HEAD);
    double PX = sbb_px(p), PY = sbb_py(p);
    float L = tbb_marker(OT_CAMLIM_L, 1), Rm = tbb_marker(OT_CAMLIM_R, 1), T = tbb_marker(OT_CAMLIM_T, 0), Bm = tbb_marker(OT_CAMLIM_B, 0);
    float lava_y = Bm - 21;                                                     /* the lava's top (HazardLavaPit row at 1938, the bottom marker 1959) */
    plat_frame(p);
    /* 12571: the fight starts once (ML.Y 0): the player below the top marker and grounded */
    if (!X.fight && !X.end && PY > T && p->ground) { X.fight = 1; X.O = 0; }
    if (!X.fight && !X.end) { rig_place(); return; }
    if (X.fight) {
        if (!X.lock) { X.lock = 1; level_lock_markers(); }
        if (X.N == -1) {                                                        /* the intro (12576-12580) */
            X.O += 1;
            if (X.O > 20 && !X.voice) { X.voice = 1; voice_play("sx_still"); }   /* 12574: the stage voice (RV x1_l 413 = O 21) */
            if (X.O < 140 && tbb_g3() % 3 == 0 && hd) obj_effect(hd->x, lava_y + 3, "tbx_splash");   /* 12577 */
            if (X.O == 120 && !X.music) { X.music = 1; const char *m = level_music_slot(4); if (m) music_play(m); }   /* 12578: ML.Y 1 -> play_music(4) (RV m_boss2 at 512) */
            if (X.O > 120) { X.B += X.S; X.S = fmin(X.S + 0.2, 0); }            /* 12579: the rise (RV x1_l 520: B 1993.6, S -13.4) */
            if (X.O > 250) { X.O = 0; X.N = 0; }                                /* 12580 */
            camera_shake(3);
        }
        X.hud = X.N == -1 ? fminf(X.hud + 1, 40) : fmaxf(X.hud - 1, 0);         /* 12575 / 12581: loop_gui_shiftup / shiftdown (INFERRED 1 px a frame) */
        X.D = sbb_fmod360(X.D + X.F); if (X.D < 0) X.D += 360;                  /* 12590 */
        if (X.K == 1) { X.D = sbb_fmod360(X.D + 2 * X.F); if (X.D < 0) X.D += 360; }   /* 12604 */
        /* ---- N 0: swim to the next waypoint (12591-12598) */
        if (X.N == 0) {
            X.O += 1;
            int nw = tbb_count(OT_TBB_WAYPOINT);
            if (X.O == 25 && nw > 0 && !(h && X.hD == 1)) {                     /* 12592: P / Q = waypoint T, T = (T + 1) mod n */
                obj_t *w = tbb_get(OT_TBB_WAYPOINT, nw - 1 - (int)X.T % nw);  /* the waypoint whose A = T (RV x2_l: A 4..0 in placement order) */
                if (w) { X.P = w->x; X.Q = w->y; }
                X.T = fmod(X.T + 1, nw);
            }
            if (X.O > 30) {
                for (int k = 0; k < (h && X.hD == 1 ? 4 : 2); k++) {             /* 12593 x 2 (+ 12594 x 2 stunned: RV x2_l -4 a frame) */
                    double want = atan2(X.Q - X.B, X.A - X.P) * SB_RAD + 90;
                    double d = fmod(want - X.C + 540, 360); if (d < 0) d += 360; d -= 180;
                    X.C += d > 1 ? 1 : d < -1 ? -1 : d;
                }
                double sp = h && X.hD == 1 ? 8 : 5;                             /* 12595 / 12596: 5 px a frame along C + 90 (+ 3 while stunned) */
                X.A += sp * sbb_cos(90 + X.C); X.B += -sp * sbb_sin(90 + X.C);
            }
            X.M = 20;
            if (h && X.hD == 1 && X.B > Bm + 200) { X.N = 3; X.O = 0; }         /* 12597: stunned and under the lava: the spheres */
            else {
                if (X.O > 90 && (X.C < 20 || X.C > 340) && !(h && X.hD == 1)) { X.O = 0; X.N = X.R; }   /* 12598: C as stored (RV x1_l: N 1 at C -100, N 2 at 341) */
                if (X.O > 300 && !(h && X.hD == 1)) { X.O = 0; X.N = X.R; }    /* 12598: the time-out */
            }
        }
        /* ---- N 1: lava pillars from the floor's splash points (12649-12661) */
        else if (X.N == 1) {
            X.O += 1; X.M = 0;
            { double c = fmod(X.C, 360); if (c < 0) c += 360; X.C = c >= 180 ? (X.C * 9 + 360) * 0.1 : X.C * 0.9; }   /* 12647 / 12648 */
            if (X.O == 1) snd_play_orig(305, 220, 128);                         /* 12650 (RV 732) */
            if (X.O == 60) snd_play_orig(304, 220, 128);                        /* 12651 (RV 791) */
            for (int m = 0; m < 2; m++) { obj_t *s = splash(m); if (s) s->hidden = !(X.O > 1 && X.O <= 120); }
            if (X.O > 60 && X.O < 100 && (tbb_g3() & 1))                        /* 12652 / 12653: a blob up (A -12) every other frame */
                for (int m = 0; m < 2; m++) { obj_t *s = splash(m); if (s) blob_spawn(s->x, s->y, -12); }
            if (X.O > 120) { X.N = 0; X.O = 0; X.R = X.toggle ? 4 : 2; X.toggle ^= 1; }   /* 12661: R = ((flag 3) + 1) * 2 */
        }
        /* ---- N 2: the shoulders pour lava (12662-12667) */
        else if (X.N == 2) {
            X.O += 1; X.M = 0;
            double c = fmod(X.C, 360); if (c < 0) c += 360; X.C = c >= 180 ? (X.C * 9 + 360) * 0.1 : X.C * 0.9;   /* 12647 / 12648 */
            if (X.O == 20) snd_play_orig(303, 220, 128);                        /* 12662 (RV 1097) */
            if (X.O > 20 && X.O < 80 && (tbb_g3() & 1))
                for (int s = 0; s < 2; s++) {
                    obj_t *sh = part(s ? P_SHL : P_SHR); if (!sh) continue;
                    float ax, ay; tbb_act_point(sh, &ax, &ay); blob_spawn(ax, ay, 12);
                    tbb_set_a(sh, s ? TS_SX_SHL7 : TS_SX_SHR7);
                }
            if (X.O > 80) {
                tbb_set_a(part(P_SHL), TS_SX_SHL); tbb_set_a(part(P_SHR), TS_SX_SHR);   /* 12666 */
                X.N = 0; X.R = 1; X.O = 0;                                      /* 12667 */
            }
        }
        /* ---- N 3: the spike spheres after a stun (12637-12642) */
        else if (X.N == 3) {
            X.O += 1; X.C = 0;
            if (X.O == 40) sphere_launch(0);
            if (X.O == 90) sphere_launch(1);
            if (X.O == 140) sphere_launch(2);
            if (X.O > 240) {
                if (X.hE >= 2) { X.N = 5; X.hA = 60; }                          /* 12641: the third round: the dive, A 60 */
                else { X.N = 0; X.hA = 50; }                                    /* 12642 */
                X.O = 0; X.hD = 0; X.R = 1;
                if (h) { h->A = X.hA; h->fl &= ~TBB_DEF; }
            }
        }
        /* ---- N 4: the claw spin (12598-12611) */
        else if (X.N == 4) {
            X.O += 1; X.M = 0; X.K = 1;
            X.B -= fmax(0, 15 - X.O * 0.5);                                     /* 12598: up, slowing */
            if (X.O > 30 && X.O <= 160) X.J = fmax(0.3, X.J - 0.05);            /* 12601 (INFERRED floor 0.3) */
            if (X.O > 160) X.J = fmin(1, X.J + 0.05);                           /* 12602 */
            double c = fmod(X.C, 360); if (c < 0) c += 360; X.C = c >= 180 ? (X.C * 9 + 360) * 0.1 : X.C * 0.9;
            for (int s = 0; s < 2; s++) {                                       /* 12608-12611: a claw passing 90 / 270 fires 3 blobs */
                obj_t *cl = part(s ? P_CLAWR : P_CLAWL); if (!cl) continue;
                double d = s ? X.D : sbb_nrm(X.D + 180);
                if (X.claw_cd[s] > 0) X.claw_cd[s]--;
                if (X.claw_cd[s] == 0 && d >= 90 && d < 120) {
                    X.claw_cd[s] = 15;
                    for (int k = 0; k < 3; k++) blob_spawn(cl->x, cl->y - 48 + 24 * k, 12);
                }
            }
            if (X.O > 160 && X.J >= 1) { X.N = 0; X.R = 2; X.O = 0; X.K = 0; }   /* 12603 */
        }
        /* ---- N 5: the dive: a ball bouncing along the arena's walls (12614-12634) */
        else if (X.N == 5) {
            X.O += 1; X.M = 0;
            if (X.O == 1) { parts_hidden(1); tbb_set_a(body, TS_SX_BODY8); if (h) tbb_set_a(h, TS_SX_HIT8); }   /* 12635 */
            /* RV x2_l 1639-1900: 80 frames under the lava, a leap (S -12), then along the walls: each leg accelerates 0.3 a frame to 12
             * and stops at an edge with sound 51 (the floor = the bottom marker - 133, the sides / top 96 px inside the markers: RV 1826 /
             * 1016); O holds on each leg (12620-12633) */
            double fl = Bm - 133, lw = L + 96, rw = Rm - 96, tw = T + 96;
            if (X.O > 80 && X.O < 100) { X.S = -12; X.B += X.S; }               /* 12617 / 12618 */
            if (X.O >= 100 && (tbb_g3() % 5) == 0) snd_play_orig(264, 220, 128);   /* RV: the ball's sound every 5 frames from O 100 */
            int leg = (int)X.O - 100;
            if (leg >= 0 && leg <= 6) {
                X.O -= 1;
                int next = 0;
                switch (leg) {
                case 0: X.S = fmin(X.S + 0.3, 12); X.B += X.S; if (X.S > 0 && X.B >= fl) { X.B = fl; X.S = 0; next = 1; } break;
                case 1: X.U = fmax(X.U - 0.3, -12); X.A += X.U; if (X.A <= lw) { X.A = lw; X.U = 0; next = 1; } break;
                case 2: X.S = fmax(X.S - 0.3, -12); X.B += X.S; if (X.B <= tw) { X.B = tw; X.S = 0; next = 1; } break;
                case 3: X.U = fmin(X.U + 0.3, 12); X.A += X.U; if (X.A >= rw) { X.A = rw; X.U = 0; next = 1; } break;
                case 4: X.S = fmin(X.S + 0.3, 12); X.B += X.S; if (X.B >= fl) { X.B = fl; X.S = 0; next = 1; } break;
                case 5: X.U = fmax(X.U - 0.3, -12); X.A += X.U; if (X.A <= (L + Rm) / 2) { X.A = (L + Rm) / 2; X.U = 0; X.S = -15; X.O += 1; } break;
                case 6: X.S = fmin(X.S + 0.3, 15); X.B += X.S;
                    if (X.S > 0 && X.B >= Bm + 150) {                           /* 12634: back to the rig, under the lava */
                        parts_hidden(0); tbb_set_a(body, TS_SX_BODY); if (h) tbb_set_a(h, TS_SX_HIT);
                        X.N = 0; X.O = -1;
                    }
                    break;
                }
                if (next) { snd_play_orig(51, 220, 128); X.O += 1; }
                X.O += 1;
            }
        }
        { float by0 = body ? body->y : 0;
          rig_place();
          if (body && hd && (by0 > lava_y) != (body->y > lava_y)) {             /* 12671-12680: the rig through the lava: splash, Z 3 (RV x2_l 788 / 1134) */
              snd_play_orig(body->y < lava_y ? 252 : 251, 220, 128); obj_effect(body->x, lava_y, "tbx_splash"); camera_shake(3); } }
        /* ---- the hitbox: contact (12636), the stun (12701-12703), the defeat (12704)
         * 12636 keeps only the bodies whose N IS 5 (its selection loop unlinks N != 5): the hitbox hurts (15, the push away from
         * it) only as the dive's ball (direction 8: image 6006, ~124 px). The rest of the fight it is just the head's 40 x 64
         * target box: RV build/orig_traces/tb_bosses_hit/x3_l (Lilac, no invulnerability, standing in the box at 1415,1314 over
         * the hitbox at 1415,1382 in N 1 for 66 frames: R stays 0, her eight hair whips take A 50 -> 26). (The port used to
         * hurt on N != 5 and give the ball an INFERRED body contact: the head launched every jump attack away.) */
        if (h) {
            if (X.N == 5 && sbb_csens_hit(p, h) && sbb_vulnerable(p)) {
                if (sbb_hurt(p, 15)) {
                    double gy = fmax(-15, fmin(15, 0.5 * (PY - h->y))), gx = fmax(-15, fmin(15, 0.5 * (PX - h->x)));
                    p->vy += (float)gy; if (p->ground) p->gsp += (float)gx; else p->vx += (float)gx;
                }
            }
            if ((h->fl & TBB_DEF) && X.hD == 0) {                               /* 12701 */
                X.hD = 1; snd_play_orig(47, 220, 128); X.hE += 1;
                if (X.N != 0) X.O = fmin(X.O, 40);                              /* 12703: to the arena's centre, under the lava */
                X.N = 0; X.P = (L + Rm) * 0.5; X.Q = Bm + 250; X.R = 3; X.K = 0; X.J = 1;
                if (body && body->an.b != NULL && X.hE < 3) { parts_hidden(0); tbb_set_a(body, TS_SX_BODY); tbb_set_a(h, TS_SX_HIT); }
            }
            if (X.hD == 1 && X.N == 0) { X.O = fmin(X.O, 40); X.P = (L + Rm) * 0.5; X.Q = Bm + 250; }   /* 12703: O held at 40 (RV x2_l), the centre under the lava */
            if (X.hE >= 3) {                                                    /* 12704: the defeat */
                X.fight = 0; X.end = 1; X.hF = 0; X.hG = 0;
                snd_play_orig(309, 220, 128);
                sbb_music_fadeout();
                for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_TBB_SHOT) o->alive = 0; }
                for (int k = 0; k < 3; k++) { obj_t *s = sphere(k, 0), *s2 = sphere(k, 1); if (s) s->alive = 0; if (s2) s2->alive = 0; }
                for (int m = 0; m < 4; m++) { obj_t *s = splash(m); if (s) s->alive = 0; }
            }
        }
        blobs_frame(p, lava_y);
        spheres_frame(p, lava_y);
    }
    if (X.end) {                                                                /* 12711-12715: the parts flash, collapse, explode; the results */
        X.hF += 1;
        if (X.hG == 0) {
            if (X.hF == 1) ach_boss_finish();                                   /* 12718-12726: the finishing-move achievements 10-17 */
            camera_shake((int)fmin(12, 0.1 * X.hF));
            if (X.hF == 270) snd_play_orig(92, 220, 128);                      /* RV x2_l 2171 (F 270; INFERRED its event) */
            if (X.hF > 340 && hd) {                                             /* 12712: the parts close in on the head, white bursts at +-40 */
                obj_effect(hd->x - 40 + cross_rand(81), hd->y - 40 + cross_rand(81), "wburst");
                for (int k = 0; k < tbb_count(OT_TBB_SX_PART); k++) { obj_t *o = tbb_get(OT_TBB_SX_PART, k); if (o && o != hd) { o->x = (o->x * 9 + hd->x) / 10; o->y = (o->y * 9 + hd->y) / 10; } }
                for (int s = 0; s < 2; s++) for (int a = 1; a <= NARM; a++) { obj_t *o = arm(s, a); if (o) { o->x = (o->x * 9 + hd->x) / 10; o->y = (o->y * 9 + hd->y) / 10; } }
            }
            if (X.hF > 360) {                                                   /* 12713: gone; sound 50, explosion01, the HUD up */
                X.hG = 1; X.hF = 0;
                if (hd) obj_effect(hd->x, hd->y, "explosion");
                snd_play_orig(50, 220, 128);
                for (int k = 0; k < tbb_count(OT_TBB_SX_PART); k++) { obj_t *o = tbb_get(OT_TBB_SX_PART, k); if (o) o->alive = 0; }
                for (int s = 0; s < 2; s++) for (int a = 1; a <= NARM; a++) { obj_t *o = arm(s, a); if (o) o->alive = 0; }
                if (h) h->alive = 0;
            }
        } else if (X.hF > 120 && !X.results) { X.results = 1; results_begin_boss(game_timer()); }   /* 12715: Sign.B 179 -> the results */
        X.hud = X.hG ? fminf(X.hud + 1, 40) : X.hud;
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBX")) {
        const obj_t *b = part(P_BODY);
        printf("tbx %d px %.0f py %.0f N %.0f O %.0f R %.0f T %.0f A %.1f B %.1f C %.1f D %.0f J %.2f K %.0f bx %.0f by %.0f hA %.0f hD %.0f hE %.0f hF %.0f hG %.0f fight %d end %d res %d hud %.0f hx %.0f hy %.0f\n",
            G.frame, PX, PY, X.N, X.O, X.R, X.T, X.A, X.B, X.C, X.D, X.J, X.K, b ? b->x : -1, b ? b->y : -1, h ? h->A : -1, X.hD, X.hE, X.hF, X.hG,
            X.fight, X.end, X.results, X.hud, h ? h->x : -1, h ? h->y : -1);
    }
#endif
}
float tbx_hud_dy(void) { return X.on ? -X.hud : 0.f; }

int tbx_draw(const obj_t *o, int pass) {
    (void)pass;
    switch (o->type) {
    case OT_TBB_SX_PART: case OT_TBB_SX_ARML: case OT_TBB_SX_ARMR: case OT_TBB_SX_SPHERE: case OT_TBB_SX_SPLASH: case OT_TBB_PLAT: case OT_TBB_PLAT2:
        if (!o->hidden) {
            const obj_t *h = hitbox();
            obj_t tmp = *o; if (h && X.hD == 1 && X.fight && (tbb_g3() & 2) && o->type != OT_TBB_PLAT && o->type != OT_TBB_PLAT2) tmp.inv = 1;   /* 12702: anim 8 (the hurt flash) */
            sbb_draw(&tmp, 0xffffffff);
        }
        return 1;
    case OT_TBB_SX_HIT: case OT_TBB_WAYPOINT: return 1;
    case OT_TBB_SHOT: sbb_draw(o, 0xffffffff); return 1;
    }
    return 0;
}
