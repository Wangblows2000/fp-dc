/* TH1: the Laser Iris (BossLaserIris_3141 + Cannon_3140, WingLeft_3138, WingRight_3139, HazardScanLaserBoss_3142).
 * RE handle_frame_57_events 13502-13531 (fight group 0x22db0 from 13514; arena-end once 0x22db8), constructors; the shared laser
 * rules 13484 / loop_l_56 / 13490-13493 (th_gim.c th_gim_beam, shared with the act's scan lasers); RV build/orig_traces/th_bosses i1_c (Carol: fly-by, fight, kill
 * at f1300), i2_m (Milla: jumps, kill at f1500). spec_th_bosses.md Part A.
 * Iris alterables (ctor A 60 (Q20 HP), D 270): D the cannon angle, E the flap phase, F the flap speed, G / H the base position
 * (x = G, y = H + 5 sin E), I the state (0 asleep, 1 waking, 2 flying ahead, 3 aiming, 4 tracking, 5 firing, 6 defeated),
 * J the state timer, M the x speed, O the sweep phase, Q explosions; flag 3 the flap sound played. The fight's events run for
 * every character (no MV.A test: RE). */
#include "th_boss.h"
#include <stdio.h>
#include <stdlib.h>

static struct {
    int on, fight, opened, gone;                     /* placed; group 0x22db0 on (13513); 0x22db8 (13531 done); the iris destroyed (13529) */
    double D, E, F, G, H, I, J, M, O, Q; int fl3;    /* BossLaserIris */
    double wlX, wlY, wrX, wrY, cX, cY;               /* the parts' X / Y (13529: their fall velocities) */
    double railA;                                    /* MinibossRailing A (13530; every railing the same) */
} I;

static obj_t *iris(void) { return thb_get(OT_THB_IRIS, 0); }
int th_boss_iris_state(void) { return I.on ? (int)I.I : -1; }

void thi_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&I, sizeof I); }
    memset(&I, 0, sizeof I);
    obj_t *o = iris(), *c = thb_get(OT_THB_CANNON, 0), *wl = thb_get(OT_THB_WINGL, 0), *wr = thb_get(OT_THB_WINGR, 0), *l = thb_get(OT_THB_LASER, 0);
    if (l) { thb_set_a(l, TS_LAS); l->av[0] = 10; l->ang = 0; l->dir = 0; }    /* ctor A 10 (av[0] = A for th_gim_beam), direction 0 */
    if (!o) return;
    I.on = 1;
    thb_set_a(o, TS_IRIS24); o->A = 60; o->fl |= F_NOFLY;                       /* ctor: direction 24, A 60, D 270 */
    I.D = 270; I.G = o->x; I.H = o->y;                                          /* RV i1_c f0: G 2480, H 2208 (the placement) */
    if (c) thb_set_a(c, TS_CAN);
    if (wl) thb_set_a(wl, TS_WL);
    if (wr) thb_set_a(wr, TS_WR);
}

/* wing pose (13507): x scale |sin(E / 2)| / 2 + 1 / 2, angle +-30 cos(E + 90) */
static void parts_follow(obj_t *o) {
    obj_t *c = thb_get(OT_THB_CANNON, 0), *wl = thb_get(OT_THB_WINGL, 0), *wr = thb_get(OT_THB_WINGR, 0);
    double xs = fabs(sbb_sin(0.5 * I.E)) * 0.5 + 0.5, a = 30 * sbb_cos(90 + I.E);
    if (c) { c->x = o->x; c->y = o->y; c->ang = (float)sbb_nrm(I.D); }
    if (wl) { wl->x = o->x; wl->y = o->y - 16; wl->av[0] = xs; wl->ang = (float)sbb_nrm(a); }
    if (wr) { wr->x = o->x; wr->y = o->y - 16; wr->av[0] = xs; wr->ang = (float)sbb_nrm(-a); }
}

static void fight_start(obj_t *o) {                  /* 13513 */
    I.fight = 1; I.I = 3; thb_set_a(o, TS_IRIS); I.F = 10; I.J = 0;
    th_gim_scan_off();                                    /* every HazardScanLaser* and head of the act destroyed (th_gim.c) */
}

void thi_frame(player_t *p) {
    obj_t *l = thb_get(OT_THB_LASER, 0);
    /* the shared laser events run before the boss's own block (RE handler order 13484, 13490-13493 < 13502) */
    if (l) {
        th_gim_beam(l, p);                                                      /* the length (16 steps), the burn (5), the knock-back off */
        thb_beam_dust(l);                                                       /* 13486 / 13489 */
        th_enemy_totem_zap(l);                                                  /* 13488: a totem in the beam breaks */
    }
    if (!I.on) return;
    obj_t *o = iris();
    double PX = sbb_px(p);
    if (o) {
        o->x = (float)(int)I.G; o->y = (float)(int)(I.H + 5 * sbb_sin(I.E)); I.G += I.M;   /* 13502 */
        if (I.E <= 90) I.fl3 = 0;                                                       /* 13503 */
        if (I.I < 7 && !I.fl3 && I.E > 90 && obj_near_view(o, 200)) { snd_play_orig(336, 220, 128); I.fl3 = 1; }   /* 13504 / 13505 */
        if (I.E > 90 && I.E < 180) I.E = sbb_fmod360(I.E + I.F);                      /* 13506 */
        if (I.I < 7) { I.E = sbb_fmod360(I.E + I.F); parts_follow(o); }                /* 13507 */
        if (I.I == 0 && o->x - 150 < PX) { I.I = 1; thb_set_a(o, TS_IRIS8); }        /* 13508 */
        if (I.I == 1) {                                                                /* 13509 */
            I.E = fmin(I.E + 15, 90); I.J += 1;
            o->y = (float)(int)(o->y + fmod(I.J, 4));
            parts_follow(o);
        }
        if (I.I == 1 && I.J > 20) { I.I = 2; I.F = 15; }                               /* 13510 */
        if (I.I == 2 && I.J < 300) { I.G += 10; I.J += 1; }                            /* 13511 */
        if (I.I == 2 && I.J >= 300) { thb_set_a(o, TS_IRIS); I.F = 10; }               /* 13512 */
        if (I.I == 2 && I.J >= 300 && o->x - 200 < PX && !I.fight) fight_start(o);    /* 13513 */
    }
    if (!I.fight) return;
    /* ---- group 0x22db0 */
    obj_t *c = thb_get(OT_THB_CANNON, 0);
    if (l && c) { float ax, ay; thb_act_point(c, &ax, &ay); l->x = ax; l->y = ay; l->ang = c->ang; }   /* 13514 */
    if (o) {
        if (I.I == 3) { I.J += 1; if (l) l->dir = 0; I.D = 270 + 45 * sbb_sin(I.O); I.O = sbb_fmod360(I.O + 2); }   /* 13515 */
        if (I.I == 3 && I.J > 90) { I.J = 0; I.I = 4; I.F = 15; }                      /* 13516 */
        if (I.I < 5 && fmod(I.J, 40) == 30) I.M = PX > I.G ? 2 : -2;                   /* 13517 / 13518 */
        if (I.I == 4) { I.D = 270 + 45 * sbb_sin(I.O); I.O = sbb_fmod360(I.O + 4); I.J += 1; if (l) l->dir = 0; }   /* 13519 */
        if (I.I == 4 && I.J > 40) { I.J = 0; I.I = 5; I.M = PX > I.G ? 4 : -4; snd_play_orig(169, 220, 128); }   /* 13520 / 13521 */
        if (I.I == 5) { I.D = 270 + 45 * sbb_sin(I.O); I.O = sbb_fmod360(I.O + 4); if (l) l->dir = 1; I.J += 1; }   /* 13522 */
        if (I.I == 5 && fmod(I.J, 40) == 30) I.M = PX > I.G ? 4 : -4;                  /* 13523 / 13524 */
        if (I.I == 5 && I.J > 120) { I.J = 0; I.I = 3; I.F = 10; I.M *= 0.5; }        /* 13525 */
        if (I.M < 0 && I.G < 4740) I.M = -I.M;                                          /* 13526 */
        if ((o->fl & THB_DEF) && I.I < 6) {                                            /* 13527 */
            I.I = 6; I.M = 0; thb_set_a(o, TS_IRIS16);
            obj_t *wl = thb_get(OT_THB_WINGL, 0), *wr = thb_get(OT_THB_WINGR, 0);
            thb_set_a(c, TS_CAN16); thb_set_a(wl, TS_WL16); thb_set_a(wr, TS_WR16);
            if (l) l->alive = 0;
            I.F = 20;
        }
        if (I.I == 6 && thb_g3() % 7 == 0) {                                           /* 13528 */
            int ex = cross_rand(81), ey = cross_rand(81);
            obj_effect(o->x - 40 + ex, o->y - 40 + ey, "explosion06");
            I.Q += 1;
            thb_explode_snd();
        }
        if (I.I == 6 && I.Q > 15) {                                                    /* 13529 */
            I.wlX = -8; I.wlY = -2; I.wrX = 8; I.wrY = -2; I.cY = 8;
            obj_effect(o->x, o->y, "shade_at"); obj_effect(o->x, o->y, "explosion01");
            snd_play_orig(38, 220, 128);
            o->alive = 0; o = NULL; I.gone = 1;
        }
    }
    if (I.gone) {                                                                      /* 13530: the parts fall, the railings count */
        obj_t *wl = thb_get(OT_THB_WINGL, 0), *wr = thb_get(OT_THB_WINGR, 0);
        if (wl) { wl->x = (float)(int)(wl->x + I.wlX); wl->y = (float)(int)(wl->y + I.wlY); I.wlY += 0.25; }
        if (wr) { wr->x = (float)(int)(wr->x + I.wrX); wr->y = (float)(int)(wr->y + I.wrY); I.wrY += 0.25; }
        if (c) { c->x = (float)(int)(c->x + I.cX); c->y = (float)(int)(c->y + I.cY); I.cY += 0.25; }
        obj_t *q[3] = { wl, wr, c };
        for (int k = 0; k < 3; k++) if (q[k] && q[k]->y > G.camy + SCREEN_H + 400) q[k]->alive = 0;   /* off the view for good (INFERRED: never seen again) */
        if (thb_rails_alive()) I.railA += 1;
    }
    if (I.railA > 30 && !I.opened) { I.opened = 1; thb_arena_open(); }              /* 13531 */
    l = thb_get(OT_THB_LASER, 0);
    if (l) thb_set_a(l, l->dir ? TS_LAS1 : TS_LAS);                                  /* force_direction 1 / 0 (13515 / 13519 / 13522) */
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_THI")) {
        const obj_t *ir = iris(), *lz = thb_get(OT_THB_LASER, 0);
        printf("thi %d px %.0f py %.0f x %.0f y %.0f A %.0f D %.3f E %.0f F %.0f G %.0f H %.0f I %.0f J %.0f M %.1f O %.0f Q %.0f lA %.0f lx %.0f ly %.0f la %.3f ld %d fight %d rail %.0f\n",
            G.frame, PX, sbb_py(p), ir ? ir->x : -1, ir ? ir->y : -1, ir ? ir->A : -1, I.D, I.E, I.F, I.G, I.H, I.I, I.J, I.M, I.O, I.Q,
            lz ? lz->av[0] : -1, lz ? lz->x : -1, lz ? lz->y : -1, lz ? lz->ang : -1, lz ? lz->dir : -1, I.fight, I.railA);
    }
#endif
}

/* draw: the rig in placement order (wings, cannon, iris: the json order 859-862), each with its own scale / angle; the laser
 * x-scaled by A / 10 and turned to the cannon's angle */
int thi_draw(const obj_t *o, int pass) {
    (void)pass;
    switch (o->type) {
    case OT_THB_WINGL: case OT_THB_WINGR: {
        if (!o->an.b || o->hidden) return 1;
        r_set_xscale(o->av[0] > 0 ? (float)o->av[0] : 1.f);
        sbb_draw(o, 0xffffffff);
        r_set_xscale(1.f);
        return 1; }
    case OT_THB_LASER: {
        th_gim_beam_draw(o);
        return 1; }
    case OT_THB_IRIS: case OT_THB_CANNON: sbb_draw(o, 0xffffffff); return 1;
    }
    return 0;
}
