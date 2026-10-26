/* SB3 boss: the satellite (BossSatellite_2286 + Top_2284, Thrusters_2285, eight BossSatelliteArm_2287, eight MinibossUFOBeam2_2288
 * and the main MinibossUFOBeam_1769). RE handle_frame_38_events group 0x22964, events 9878-9927 and the start event 9940 (spec_sb_bosses
 * Part C); RV trace s1 (build/orig_traces/sb_boss). Everyone fights it (no MV.A test).
 * Satellite alterables: A HP 80 (Q20), D state (-2 waiting under the deck, -1 rising, 0 roaming, 1 beam, 2 dive, 6 defeated), E timer,
 * G base y, H bob phase, I rest y, J direction, K home x, L roam half-width 200, R the defeat explosion counter, flag 2 the next
 * attack (set: beam), flag 16 (no death flight). Arm alterables: D radius, E angle, F home radius, X / Y fall velocity, flags 5 / 6. */
#include "sb_boss.h"
#include <stdio.h>
#include <stdlib.h>

#define NARM 8
static struct {
    int on;                             /* group 0x22964 */
    int lock_prev;                      /* the flow's lock state last frame */
    double D, E, G, H, I, J, K, L, R; int fl2;
    double aD[NARM], aE[NARM], aF[NARM], aX[NARM], aY[NARM]; int afl[NARM];   /* afl: 0x20 flag 5, 0x40 flag 6 */
    double bA[NARM]; int bvis[NARM];   /* the arm beams (UFOBeam2 A = y-scale) */
    double mA; int mvis;               /* the main beam */
    double topX, topY; int topfl6;     /* BossSatelliteTop's fall */
    double thrA;                       /* BossSatelliteThrusters' climb */
    int ended;                         /* 9925 ran */
} S;
static obj_t *sat(void) { return sbb_get(OT_SB_SAT, 0); }
static obj_t *arm(int k) { return sbb_get(OT_SB_SAT_ARM, k); }
static obj_t *beam2(int k) { return sbb_get(OT_SB_SAT_BEAM2, k); }

void sbs_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    obj_t *s = sat();
    if (!s) return;
    sbb_set_a(s, SS_SAT); s->A = 80; s->fl |= F_NOFLY;                            /* ctor: A 80, D -2, E -7, J 1, L 200; 9940: flag 16 */
    S.D = -2; S.E = -7; S.J = 1; S.L = 200; S.fl2 = 1;                            /* flag 2 set: the first attack is the beam (RV s1) */
    S.I = s->y; S.G = s->y + 240; S.K = s->x;                                   /* 9940: G = Y, I = G, G += 240, K = X */
    int n = sbb_count(OT_SB_SAT_ARM);
    for (int k = 0; k < n && k < NARM; k++) {                                   /* 9940: i runs from the last-created arm (index n-1-k) */
        int i = n - 1 - k;
        S.aD[k] = (i % NARM) / 2 * 24 + 56; S.aE[k] = (i % 2) * 180.; S.aF[k] = (int)S.aD[k];
        sbb_set_a(arm(k), SS_ARM);
    }
    for (int k = 0; k < sbb_count(OT_SB_SAT_BEAM2) && k < NARM; k++) { sbb_set_a(beam2(k), SS_BEAM2); S.bA[k] = 1; }   /* ctor A 1, invisible */
    obj_t *t = sbb_get(OT_SB_SAT_TOP, 0), *th = sbb_get(OT_SB_SAT_THR, 0), *mb = sbb_get(OT_SB_SAT_BEAM, 0);
    sbb_set_a(t, SS_TOP); sbb_set_a(th, SS_THR); sbb_set_a(mb, SS_BEAM); S.mA = 1;
}
void sbs_pre(player_t *p) { (void)p; }

static void arms_anim(int k) { for (int a = 0; a < NARM; a++) { obj_t *o = arm(a); if (o && !(S.afl[a] & SB_DEF)) sbb_set_a(o, k); } }
static int any_arm_e0(void) { for (int a = 0; a < sbb_count(OT_SB_SAT_ARM) && a < NARM; a++) if (S.aE[a] == 0.) return 1; return 0; }
static void beam_hits(player_t *p) {                                             /* 9895 / 9896 (D 1, 90 < E < 180) */
    int px = (int)sbb_px(p), py = (int)sbb_py(p), hit = 0;
    for (int k = 0; k < NARM; k++) {
        obj_t *b = beam2(k);
        if (!b || !S.bvis[k]) continue;
        int bx = (int)b->x; if (px > bx + 15 || px < bx - 15 || py + 23 < (int)b->y) continue;
        hit = 1; break;
    }
    if (hit) sbb_hurt(p, 10);
    obj_t *mb = sbb_get(OT_SB_SAT_BEAM, 0), *a0 = arm(0);
    if (mb && (int)mb->x + 29 >= px && (int)mb->x - 29 <= px && a0 && (int)a0->y - 23 <= py) sbb_hurt(p, 10);
}

void sbs_frame(player_t *p) {
    obj_t *s = sat();
    if (!s) return;
    int na = sbb_count(OT_SB_SAT_ARM); if (na > NARM) na = NARM;
    int nb = sbb_count(OT_SB_SAT_BEAM2); if (nb > NARM) nb = NARM;
    double PXv = sbb_px(p);
    obj_t *mb = sbb_get(OT_SB_SAT_BEAM, 0), *top = sbb_get(OT_SB_SAT_TOP, 0), *thr = sbb_get(OT_SB_SAT_THR, 0);
    int rails = sbb_rails();
    /* 9878 / 9879 (the flow runs the camera lock, sb_flow.c): the group goes on with the lock and off when the player backs out */
    int e = sbb_lock_edge(&S.lock_prev);
    if (e > 0 && rails > 0) S.on = 1;
    if (e < 0 && rails > 0) S.on = 0;
    if (S.on) {
        if (S.D == -2 && s->x - 79 <= PXv) { S.D += 1; snd_play_orig(7, 220, 128); sbb_fight_start(); }   /* 9880 */
        for (int a = 0; a < na; a++) if (!(S.afl[a] & SB_DEF)) { obj_t *o = arm(a); if (o) o->av[7] = S.aE[a] > 180; }   /* 9882 / 9883: front / back */
        if (S.D == -1) {                                                        /* 9884 */
            for (int a = 0; a < na; a++) S.aE[a] = sbb_fmod360(S.aE[a] + 18);
            double e = S.E + 0.1; S.G += S.E; S.E = e > 0 ? 0 : e;
        }
        if (S.D == -1 && S.E == 0) S.D += 1;                                    /* 9885 */
        if (S.D == 0) {                                                         /* 9886 */
            for (int a = 0; a < na; a++) S.aE[a] = sbb_fmod360(S.aE[a] + 6);
            S.H = sbb_fmod360(S.H + 5); S.E += 1; s->x = (float)(int)(s->x + 3 * S.J);
        }
        if (S.D == 0 && s->x < S.K - S.L) S.J = 1;                              /* 9887 */
        if (S.D == 0 && s->x > PXv + 80 && s->x > S.K + S.L) S.J = -1;          /* 9888 */
        if (S.D == 0 && S.E > 120 && any_arm_e0() && S.fl2) {                   /* 9889: the beam */
            S.fl2 ^= 1; S.E = 0; S.D += 1; snd_play_orig(165, 220, 128);
        }
        if (S.D == 1 && S.E < 250) S.E += 1;                                    /* 9890 */
        if (S.D == 1 && S.E < 20) for (int a = 0; a < na; a++) S.aD[a] += (S.aF[a] - 40) * 0.1;   /* 9891 */
        if (S.D == 1 && S.E == 20) {                                            /* 9892 */
            arms_anim(SS_ARM10); sbb_set_a(s, SS_SAT7); S.E += 1;
            snd_play_orig(166, 220, 128);                                       /* stop_sample(165): the samples are short here */
        }
        if (S.D == 1 && S.E == 80) {                                            /* 9893 */
            S.E += 1;
            for (int k = 0; k < nb; k++) S.bvis[k] = 1;
            S.mvis = 1; sbb_set_a(s, SS_SAT8); arms_anim(SS_ARM8);
            for (int k = 0; k < nb; k++) { obj_t *b = beam2(k); if (b) obj_effect(b->x, b->y + 16, "wburst"); }   /* EffectsWhiteBurst at each beam (INFERRED: the decompiled offsets are garbled) */
            obj_effect(s->x, s->y + 40, "wburst");
            snd_play_orig(169, 220, 128);
        }
        if (S.D == 1 && S.E > 80 && S.E < 180) { for (int k = 0; k < nb; k++) S.bA[k] = fmin(S.bA[k] + 0.5, 5); S.mA = fmin(S.mA + 0.5, 5); }   /* 9894 */
        if (S.D == 1 && S.E > 90 && S.E < 180) beam_hits(p);                     /* 9895 / 9896 */
        if (S.D == 1 && S.E == 180) { S.E += 1; sbb_set_a(s, SS_SAT); arms_anim(SS_ARM); }   /* 9897 */
        if (S.D == 1 && S.E > 180) { S.mA = fmax(S.mA - 0.5, 1); for (int k = 0; k < nb; k++) S.bA[k] = fmax(S.bA[k] - 0.5, 1); }   /* 9898 */
        if (S.D == 1 && S.E > 180 && S.mA == 1) S.mvis = 0;                      /* 9899 (the A test INFERRED, as 9900) */
        if (S.D == 1 && S.E > 180) for (int k = 0; k < nb; k++) if (S.bA[k] == 1) S.bvis[k] = 0;   /* 9900 */
        if (S.D == 1 && S.E >= 180 && S.E < 200) for (int a = 0; a < na; a++) S.aD[a] += (S.aF[a] - 40) * -0.1;   /* 9901 */
        if (S.D == 1 && S.E == 200) { for (int a = 0; a < na; a++) S.aD[a] = S.aF[a]; S.E = 0; S.D = 0; }   /* 9902 */
        if (S.D == 0 && S.E > 120 && any_arm_e0() && !S.fl2) {                  /* 9903: the dive */
            S.fl2 ^= 1; S.E = 0; S.D = 2; snd_play_orig(7, 220, 128);
        }
        if (S.D == 2 && !(G.frame & 1)) for (int a = 0; a < na; a++) { obj_t *o = arm(a); if (o) obj_effect(o->x, o->y, "ringsparkle"); }   /* 9904 (g3 even) */
        if (S.D == 2) { for (int a = 0; a < na; a++) S.aE[a] = sbb_fmod360(S.aE[a] + 12); S.E += 1; }   /* 9905 */
        if (S.D == 2 && S.E > 30) { for (int a = 0; a < na; a++) S.aE[a] = sbb_fmod360(S.aE[a] + 6); S.G = fmin(S.G + 3, S.I + 120); }   /* 9906 */
        if (S.D == 2 && S.E == 70) { S.J = 1; arms_anim(SS_ARM8); }              /* 9907 */
        if (S.D == 2 && S.E == 70 && s->x >= PXv) S.J = -1;                      /* 9908 */
        if (S.D == 2 && S.E > 70 && S.E < 120) s->x = (float)(int)(s->x + 5 * S.J);   /* 9909 */
        if (S.D == 2 && S.E > 120) S.G = fmax(S.G - 6, S.I);                     /* 9910 */
        if (S.D == 2 && S.E > 120 && S.G <= S.I) { S.D = 0; arms_anim(SS_ARM); S.E = 0; }   /* 9911 */
        if ((s->fl & SB_DEF) && S.D != 6) {                                     /* 9912 */
            S.D = 6; S.mvis = 0; for (int k = 0; k < nb; k++) S.bvis[k] = 0;
            sbb_set_a(s, SS_SAT10); arms_anim(SS_ARM); S.E = 0; sbb_music_fadeout();
        }
        if (S.D == 6) S.E += 1;                                                 /* 9913 */
        if (S.D == 6) {                                                         /* 9914-9917: the arms explode, the outer ones first */
            double need = S.E == 10 ? 128 : S.E == 30 ? 104 : S.E == 50 ? 80 : S.E == 70 ? -1e9 : 1e9;
            int any = 0;
            for (int a = 0; a < na; a++) {
                obj_t *o = arm(a);
                if (!o || (S.afl[a] & SB_DEF) || S.aF[a] < need) continue;
                obj_effect(o->x, o->y, "explosion01"); S.afl[a] |= SB_DEF; any = 1;
            }
            if (any) snd_play_orig(38, 220, 128);
        }
        for (int a = 0; a < na; a++) {                                          /* 9918 / 9919: thrown outwards */
            obj_t *o = arm(a);
            if (!o || !(S.afl[a] & SB_DEF) || (S.afl[a] & SB_ARM)) continue;
            S.afl[a] |= SB_ARM; S.aX[a] = (int)o->x >= (int)s->x ? S.aF[a] * 0.1 : S.aF[a] * -0.1;
        }
        for (int a = 0; a < na; a++) {                                          /* 9920: the fall */
            obj_t *o = arm(a);
            if (!o || !(S.afl[a] & SB_DEF)) continue;
            o->x = (float)(int)(o->x + S.aX[a]); o->y = (float)(int)(o->y + S.aY[a]);
            S.aY[a] = fmin(S.aY[a] + 0.25, 15);
            o->ang += (float)fmax(2 * S.aX[a], 8);                              /* Z += 1 and the turn (INFERRED from the 2765 rule) */
            if (o->y > G.lvl.h + 64) { o->hidden = 1; }
        }
    }
    if (S.D == 6 && S.E == 90) {                                                /* 9921 (outside the group) */
        S.E += 1; s->hidden = 1; S.topfl6 = 1; S.topY = -3; S.topX = 2;
        snd_play_orig(38, 220, 128); obj_effect(s->x, s->y, "shade_at"); obj_effect(s->x, s->y, "explosion01");
    }
    if (S.on && S.topfl6 && top) {                                              /* 9922 */
        top->x = (float)(int)(top->x + S.topX); top->y = (float)(int)(top->y + S.topY);
        S.topY = fmin(S.topY + 0.4, 15); top->ang += 1;
        if (top->y > G.lvl.h + 64) top->hidden = 1;
    }
    if (S.D == 6 && S.E >= 90 && S.R < 45) {                                    /* 9923: the spiral of explosions (outside the group) */
        double r = S.R, ex = 2 * r == 180 ? 8 : sbb_sin(2 * r) * 40 + 8;
        obj_effect((float)(int)(ex * sbb_sin(132 * r) + s->x), (float)(int)(s->y - sbb_sin(5 * r) * 80), "explosion06");
        S.R += 1;
    }
    if (S.on && S.D == 6 && S.E >= 90 && thr) { thr->y = (float)(int)(thr->y - S.thrA); S.thrA += 0.2; if (thr->y < -64) thr->hidden = 1; }   /* 9924 */
    if (S.on && S.D == 6 && S.E > 120 && sbb_bars() > 0 && !S.ended) { S.ended = 1; sbb_arena_end(1, 0x20); }   /* 9925 */
    if (S.on) {                                                                 /* 9926: an arm in anim 8 touching the player */
        for (int a = 0; a < na; a++) { obj_t *o = arm(a); if (o && !o->hidden && sbb_a_is(o, SS_ARM8) && sbb_csens_hit(p, o)) { sbb_hurt(p, 10); break; } }
    }
    if (S.D != 6) {                                                             /* 9927 (outside the group): the rig */
        S.H = sbb_fmod360(S.H + 10);
        s->y = (float)(int)(S.G + 3 * sbb_sin(S.H));
        for (int a = 0; a < na; a++) {
            obj_t *o = arm(a); if (!o) continue;
            int d = (int)S.aD[a];
            o->x = (float)(int)(d * sbb_cos(S.aE[a]) + s->x);
            o->y = (float)(int)(d * sbb_sin(S.aE[a]) * 0.2 + s->y);
            o->scale = (float)(d * sbb_sin(S.aE[a]) * -0.002 + 1);
        }
        if (thr) { thr->x = s->x; thr->y = (float)(int)(3 * sbb_sin(S.H) + s->y); }
        if (top) { top->x = s->x; top->y = (float)(int)(3 * sbb_sin(S.H) + s->y); }
        for (int k = 0; k < nb; k++) {                                          /* UFOBeam2 k at arm k's action point (12, 38; hotspot 12, 24) */
            obj_t *b = beam2(k), *o = arm(k); if (!b || !o) continue;
            float sc = o->scale != 0 ? o->scale : 1.f;
            b->x = o->x; b->y = (float)(int)(o->y + 14 * sc);
        }
        if (mb) { mb->x = s->x; mb->y = s->y + 32; }
    }
    /* visibility + FP_TRACK mirror */
    for (int k = 0; k < nb; k++) { obj_t *b = beam2(k); if (b) { b->hidden = !S.bvis[k]; b->av[0] = S.bA[k]; } }
    if (mb) { mb->hidden = !S.mvis; mb->av[0] = S.mA; }
    for (int a = 0; a < na; a++) { obj_t *o = arm(a); if (o) { o->av[0] = S.aD[a]; o->av[1] = S.aE[a]; o->av[2] = S.aF[a]; o->av[3] = S.afl[a]; } }
    s->av[0] = S.D; s->av[1] = S.E; s->av[2] = S.G; s->av[3] = S.H; s->av[4] = S.J; s->av[5] = S.on; s->av[6] = S.fl2; s->av[7] = S.R;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_SBS")) printf("sbs %d on %d D %.0f E %.1f G %.1f H %.0f x %.0f y %.0f A %.0f\n", G.frame, S.on, S.D, S.E, S.G, S.H, s->x, s->y, s->A);
#endif
}

/* drawing (9881-9885 order: the thrusters and the top behind the satellite, arms with E <= 180 behind it, the others in front) */
int sbs_draw(const obj_t *o, int pass) {
    (void)pass;
    switch (o->type) {
    case OT_SB_SAT_TOP: case OT_SB_SAT_THR: case OT_SB_SAT_ARM: case OT_SB_SAT_BEAM: case OT_SB_SAT_BEAM2: return 1;   /* drawn with the satellite */
    case OT_SB_SAT: {
        int na = sbb_count(OT_SB_SAT_ARM), nb = sbb_count(OT_SB_SAT_BEAM2);
        for (int k = 0; k < nb && k < NARM; k++) { const obj_t *b = beam2(k); if (b && !b->hidden) sbb_draw_yscaled(b, S.bA[k]); }
        const obj_t *mb = sbb_get(OT_SB_SAT_BEAM, 0); if (mb && !mb->hidden) sbb_draw_yscaled(mb, S.mA);
        sbb_draw(sbb_get(OT_SB_SAT_THR, 0), 0xffffffff); sbb_draw(sbb_get(OT_SB_SAT_TOP, 0), 0xffffffff);
        for (int a = 0; a < na && a < NARM; a++) { const obj_t *r = arm(a); if (r && !r->av[7]) sbb_draw(r, 0xffffffff); }
        sbb_draw(o, 0xffffffff);
        for (int a = 0; a < na && a < NARM; a++) { const obj_t *r = arm(a); if (r && r->av[7]) sbb_draw(r, 0xffffffff); }
        return 1; }
    }
    return 0;
}
