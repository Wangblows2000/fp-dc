/* Pangu Lagoon bosses (session 23): types 2224-2303. Spec: build/re/spec_pl_bosses.md (labels: RE read from the decompiled events,
 * RV measured in the original (build/orig_traces/pl_bosses, Lilac, set_frame 55), INFERRED reasoned).
 * Pangu Lagoon has one encounter: the Holo-Dragon of PL6 (json 56, group 0x22d3e, events 13333-13435). PL1-PL5 hold no boss or
 * miniboss event (RE: no CameraTriggerBoss / MinibossBarrier / Railing rule in handle_frame_51..55; PL3's and PL6's CameraLimitBoss
 * markers frame the stone-door rooms, flow + gimmicks).
 * The dragon (RE 13338-13403) is a rig hanging on BossHoloDragonNeck_3086 (the controller, invisible). Neck alterables: A the wave
 * phase, B the head tilt, D / E the anchor, F the wave amplitude, G the phase speed, H the jaw, I the x speed, J the facing (-1 left,
 * 1 right), K the state (-2 asleep, -1 entrance, 0 flight, 1 laser, 2 claws, 3 flames, 6 defeated, 7 ending), L the transparency
 * (255 invisible: a hologram), M the state timer, N the next attack, O the hit spots destroyed. Every part keeps its placed offset
 * from the neck (B, C); the head parts turn about the neck by the tilt, the 93 body scales ride a travelling wave (scale A = its index
 * from the tail), the 93 hit spots (Q20, A 3) sit on their scales. Destroying 86 of them (O > count - 8) lights Eye2 (Q20, A 20) on
 * the eye; its defeat ends the fight (13404), then the explosions, the white flash, Pangu's arrival and the results (results_begin_boss;
 * the pl6 row's res_wipe ends the stage).
 * Performance (SH4): instance lists are built once at spawn; the rig is O(parts) arithmetic a frame; the hit spots' background test
 * (13392) runs only near the view (INFERRED: the hidden state matters only there). A level without the neck returns at once. */
#include "pl.h"
#include "sb_boss.h"
#include <stdio.h>
#include <stdlib.h>

#define NT (OT_PLB_PLACED_LAST - OT_PLB_NECK + 1)
#define LMAX 32
static short L_[NT][LMAX]; static short nL[NT];
#define F_DEFEATED 0x40                 /* enemy flag 6: defeated (combat.c 2764) */
#define FL_DONE 0x2                    /* alterable flag 1 (13397-13399: a destroyed hit spot counted; 13401 / 13402: Eye2 blink) */
#define NFLAME 40
#define NARM 18

static struct {
    int on, fight, snd333, results, landed, black, lock, ended, burn;
    double A, B, D, E, F, G, H, I, J, K, L, M, N, O;   /* the neck's alterables (RE ctor F 20 G 2 J -1 K -2 L 255 M -360 N 1) */
    int fl2, fl16;                     /* neck flags 1 (the laser's closing sound) and 4 (a claw landed) */
    float rx;                          /* CameraLimitBossRight.x (13333) */
    double flash;                      /* the white flash (LevelCardFade_423 direction 8): opacity 0..255 */
    double pK, pD, pV;                 /* Pangu: K its y anchor, D the bob phase, the fall speed */
    short claw[3], eye2, laser, warn[3], flame[NFLAME], pangu, arm[NARM], fx; int nwarn;
    short neck;
} H;

static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static obj_t *get(int t, int k) { int ti = t - OT_PLB_NECK; return ti >= 0 && ti < NT && k < nL[ti] ? slot(L_[ti][k]) : NULL; }
static int cnt(int t) { return nL[t - OT_PLB_NECK]; }
static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (o && a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static obj_t *spawn(int type, const char *nm, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    if (nm) set_s(o, nm);
    return o;
}
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static int is_head(int t) { return t == OT_PLB_BREATH || t == OT_PLB_TTOOTH || t == OT_PLB_BTOOTH || t == OT_PLB_HSCALE || t == OT_PLB_EAR ||
                                   t == OT_PLB_EYE || t == OT_PLB_PUPIL || t == OT_PLB_BROW; }
static int is_body(int t) { return t >= OT_PLB_SCALE2 && t <= OT_PLB_SCALE4; }
static int is_hit(int t) { return t >= OT_PLB_HIT1 && t <= OT_PLB_HIT3; }
int pl_boss_state(void) { return H.on ? (int)H.K : -3; }

/* ======================================================================== spawn */
void pl_boss_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&H, sizeof H); snap_register(L_, sizeof L_); snap_register(nL, sizeof nL); }
    memset(&H, 0, sizeof H); memset(nL, 0, sizeof nL);
    H.on = 0; H.neck = -1;
    for (int i = 0; i < G.nobjs && !H.on; i++) if (G.o[i].alive && G.o[i].type == OT_PLB_NECK) H.on = 1;
    if (!H.on) return;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_CAMLIM_R && H.rx == 0) H.rx = o->x;
        if (o->type == OT_MT_WARN && H.nwarn < 3) { H.warn[H.nwarn++] = (short)i; o->hidden = 1; o->layer = 3; set_s(o, "plb_warn"); continue; }   /* EffectWarning_1528 (json layer 4: over the player) */
        if (o->type < OT_PLB_NECK || o->type > OT_PLB_PLACED_LAST) continue;
        int ti = o->type - OT_PLB_NECK;
        if (nL[ti] < LMAX) L_[ti][nL[ti]++] = (short)i;
        memset(o->av, 0, sizeof o->av); o->st = o->tm = o->c = o->fl = o->dir = 0; o->ang = 0; o->scale = 0; o->bd = 0;
    }
    obj_t *nk = get(OT_PLB_NECK, 0);
    H.neck = (short)(nk - G.o);
    H.A = 0; H.F = 20; H.G = 2; H.J = -1; H.K = -2; H.L = 255; H.M = -360; H.N = 1; H.D = nk->x; H.E = nk->y;   /* RE create_..neck_3086 */
    set_s(nk, "plb_hit1"); nk->hidden = 1; nk->layer = 0;     /* (a sprite only to reach the draw hook: the ending's black backdrop, pass 0) */
    for (int t = OT_PLB_BREATH; t <= OT_PLB_PLACED_LAST; t++) {
        int n = cnt(t);
        for (int k = 0; k < n; k++) {
            obj_t *o = get(t, k);
            if (!o) continue;
            o->av[1] = o->x - nk->x; o->av[2] = o->y - nk->y;  /* B / C: the offset from the neck (RV p1: breath -160 / 144) */
            if (is_body(t)) o->av[0] = n - 1 - k;              /* A: the index from the tail (RV p2: Scale2 #1 A 31 of 32, Scale4 #30 A 0) */
            if (t == OT_PLB_PUPIL) { o->av[3] = o->av[1]; o->av[4] = o->av[2]; }   /* D / E: the pupil's rest (RV p1: -120 / -64) */
            if (is_hit(t)) { o->A = 3; o->fl |= F_NOFLY; o->layer = 2; }          /* RE ctor A 3; flag 16 (RV p3: the beaten spots stay); json layer 3 */
            else o->layer = 1;                                  /* json layer 2: behind the play layer's pieces (RV p1 f1300: the wall covers the head) */
        }
    }
    /* the off-map templates (json y > the frame: the importer leaves them out) */
    for (int k = 0; k < 3; k++) {
        obj_t *c = spawn(OT_PLB_CLAW, "plb_claw", 0, (float)G.lvl.h + 100, 2);
        if (c) { c->hidden = 1; c->A = 999; c->av[3] = 1 - k; H.claw[k] = (short)(c - G.o); }   /* RE ctor A 999; RV p1: D 1, 0, -1 */
    }
    obj_t *e2 = spawn(OT_PLB_EYE2, "plb_eye2", 5726, (float)G.lvl.h + 129, 2);
    if (e2) { e2->hidden = 1; e2->A = 20; e2->fl |= F_NOFLY; H.eye2 = (short)(e2 - G.o); }       /* RE ctor A 20, invisible */
    obj_t *lz = spawn(OT_PLB_LASER, "plb_laser", 5521, (float)G.lvl.h + 120, 1);
    if (lz) { lz->hidden = 1; lz->A = 10; H.laser = (short)(lz - G.o); }
    obj_t *fx = spawn(OT_PLB_FX, "plb_hit1", 0, 0, 7);
    if (fx) { fx->hidden = 1; H.fx = (short)(fx - G.o); }
    for (int k = 0; k < NFLAME; k++) H.flame[k] = -1;
    H.pangu = -1; for (int k = 0; k < NARM; k++) H.arm[k] = -1;
}

/* ======================================================================== the rig */
/* 13342: the neck */
static void neck_pos(obj_t *nk) {
    nk->x = (float)(int)H.D;
    nk->y = (float)(int)(sbb_sin(H.A + 10) * H.F + H.E);
    double q = H.F * 0.25;
    H.B = sbb_sin(H.A + 100) * q - fmin(q, 20) - H.H;
}
/* 13343 + 13344: offsets about the anchor, the head turned about the neck, the body's wave */
static void rig(const obj_t *nk) {
    double s = -H.J;
    for (int t = OT_PLB_BREATH; t <= OT_PLB_BROW; t++) {
        if (!(is_head(t) || is_body(t))) continue;
        int n = cnt(t);
        double th = t == OT_PLB_HSCALE || t == OT_PLB_TTOOTH ? H.B - H.H : t == OT_PLB_BTOOTH ? H.B + H.H : H.B;   /* 13344: upper jaw B - H, lower B + H */
        double c = sbb_cos(th), sn = sbb_sin(th);
        for (int k = 0; k < n; k++) {
            obj_t *o = get(t, k);
            if (!o) continue;
            double B = o->av[1], C = o->av[2];
            if (is_body(t)) {                                   /* 13343, then 13344's wave */
                o->x = (float)(int)(s * B + H.D);
                double y0 = (int)(H.E + C);
                o->y = (float)(int)(sbb_sin(15 * o->av[0] + H.A - 135) * H.F + y0);
                o->ang = (float)sbb_nrm(sbb_sin(15 * o->av[0] + H.A - 45) * H.F * 0.125 * s);
                o->flip = 0;
            } else {
                o->x = (float)(int)((sn * C + B * c) * s + nk->x);
                o->y = (float)(int)(c * C - sn * B + nk->y);
                o->ang = t == OT_PLB_BREATH ? 0.f : (float)sbb_nrm(th * s);
                o->flip = H.J > 0;                              /* 13339 / 13340: direction 0 / 1 (the mirror) */
            }
        }
    }
}
/* 13339 / 13340: the pupil looks at the player (RE: x distance, both offsets, clamped +-40 about the rest) */
static void pupil(const player_t *p) {
    obj_t *e = get(OT_PLB_EYE, 0);
    double s = -H.J;
    for (int k = 0; k < cnt(OT_PLB_PUPIL); k++) {
        obj_t *o = get(OT_PLB_PUPIL, k);
        if (!o || !e) continue;
        double dx = PX(p) - e->x;
        o->av[1] = clampd(o->av[3] + s * dx * 0.15, o->av[3] - 40, o->av[3] + 40);
        o->av[2] = clampd(o->av[4] - s * dx * 0.15, o->av[4] - 40, o->av[4] + 40);
    }
}

/* ======================================================================== hits on the player */
static int csens_rect(const player_t *p, int x0, int y0, int x1, int y1) {   /* PlayerCharacterSensor_375 against a box */
    int X = (int)PX(p), Y = (int)PY(p), a0, a1;
    if (player_char_sensor_small(p)) { a0 = Y - 13; a1 = Y + 14; } else { a0 = Y - 34; a1 = Y + 12; }
    return X + 8 >= x0 && X - 7 <= x1 && a1 >= y0 && a0 <= y1;
}
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int hurt(player_t *p, int n, int nokb) {
    if (dead(p) || p->hurt_pending || player_invincible(p)) return 0;
    player_contact(p, n);
    (void)nokb;
    return 1;
}

/* ======================================================================== the attacks */
static obj_t *laser(void) { return slot(H.laser); }
static void laser_rect(const obj_t *lz, int *x0, int *x1) {     /* the beam: from the mouth to 20 px past the view edge (INFERRED: the strip's x scale) */
    int edge = H.J > 0 ? (int)G.camx + SCREEN_W + 20 : (int)G.camx - 20;
    *x0 = (int)fmin(lz->x, edge); *x1 = (int)fmax(lz->x, edge);
}
static void attacks(player_t *p) {
    obj_t *br = get(OT_PLB_BREATH, 0), *lz = laser();
    double X = PX(p);
    /* ---- K 1: the laser (13359-13372) */
    if (H.K == 1) {                                                          /* 13359 */
        H.I = 0; H.F = 40; H.G = 5; H.L = 0; H.M += 1;
        H.D = (9 * H.D + X - 320 * H.J) * 0.1;
        if (br) br->flip = H.J > 0;
    }
    if (H.K == 1 && H.M == 1) { H.M = 2; snd_play_orig(200, 220, 128); }     /* 13360 */
    if (H.K == 1 && H.M == 150) { snd_play_orig(330, 220, 128); H.M += 1; snd_play_orig(169, 220, 128); }   /* 13361 */
    if (H.K == 1 && H.M < 150)                                              /* 13362 / 13363: the edge warnings */
        for (int k = 0; k < H.nwarn; k++) { obj_t *w = slot(H.warn[k]); if (w) { w->x = H.J > 0 ? G.camx + 20 : G.camx + SCREEN_W - 20; w->hidden = 0; } }
    if (H.K == 1 && H.M < 300) H.H = fmin(H.H + 2, 24);                      /* 13364 */
    if (H.K == 1 && H.M >= 300) {                                            /* 13365 */
        H.H = fmax(H.H - 2, 0);
        for (int k = 0; k < H.nwarn; k++) { obj_t *w = slot(H.warn[k]); if (w) w->hidden = 1; }
        if (br) br->hidden = 1;
        if (lz) { lz->hidden = 1; lz->y = (float)G.lvl.h + 200; }
    }
    if (H.K == 1 && H.M == 320) { H.fl2 ^= 1; H.M = 321; }                    /* 13366 */
    if (H.K == 1 && H.M > 330) {                                             /* 13367 / 13368 / 13369 */
        snd_play_orig(H.fl2 ? 332 : 331, 220, 128);
        H.K = 0; H.M = 0; H.N += 1; H.I = p->ground ? p->gsp : p->vx;
    }
    if (H.K == 1 && H.H >= 24 && br) br->hidden = 0;                          /* 13370: the mouth glow */
    if (H.K == 1 && H.M > 150 && H.M < 300 && br && lz) {                    /* 13371 / 13372 */
        lz->y = br->y;
        lz->x = H.J > 0 ? (float)fmax(br->x, G.camx - 20) : (float)fmin(br->x, G.camx + SCREEN_W + 20);
        lz->flip = H.J > 0; lz->hidden = 0;
        H.M += 1;
        G.cam.Z = 12;
    }
    if (lz && !lz->hidden && H.K < 6) {
        int x0, x1; laser_rect(lz, &x0, &x1);
        if (csens_rect(p, x0, (int)lz->y - 80, x1, (int)lz->y + 79) && !dead(p)) {
            if (p->state != ST_BOOST) {                                       /* 13373: pushed 10 px a frame (not MV.M 9 / 5) */
                int nx = (int)(p->x + 10 * H.J), y = (int)PY(p);
                if (!px_span_solid(&G.lvl, nx - 8, nx + 8, y) && !px_span_solid(&G.lvl, nx - 8, nx + 8, y - 20)) p->x += (float)(10 * H.J);
                if (p->invuln <= 4) p->vy = fminf(p->vy, 0);                    /* MV.G 0 */
            }
            /* 13374: R 1, LIFE.C 1 (no knock-back), LIFE.B + 3. The original loses 0.6 half-petal a burn (RV p7: 14 -> 5 in 15 burns);
             * the port's petals are whole (player_damage: amount / 5), so the burns' remainder is carried here: 3 (no loss) or 5 (one) */
            if (!dead(p) && !p->hurt_pending && !player_invincible(p)) { H.burn += 3; hurt(p, H.burn >= 5 ? 5 : 3, 1); if (H.burn >= 5) H.burn -= 5; }
            if (p->state == ST_HURT) { p->vx = 0; p->vy = fminf(p->vy, 0); }   /* no knock-back: the beam carries her (13373) */
            if (p->state == ST_HURT && p->invuln > 4 && !p->invinc) p->invuln = 4;   /* RV p7: OTHERS.E 4 after each burn, a hit every 5 frames */
        }
    }
    /* ---- K 2: the claws (13375-13383) */
    if (H.K == 2) H.M += 1;                                                  /* 13375 */
    if (H.K == 2 && H.M == 1) H.M = 2;                                       /* 13376 */
    for (int k = 0; k < 3 && H.K == 2; k++) {
        obj_t *c = slot(H.claw[k]);
        if (!c) continue;
        double d64 = 64 * c->av[3];
        if (H.M < 70) {                                                       /* 13377: over the player, fading in */
            c->y = (float)((int)PY(p) - 220);
            c->x = (float)(int)(((c->x - d64) * 9 + X) * 0.1 + d64);
            c->av[5] = fmax(255 - 10 * H.M, 0); c->hidden = 0;
        }
        if (H.M >= 70 && H.M < 80) { c->y -= 4; H.fl16 = 0; }                 /* 13378: the wind-up */
        if (H.M >= 80) c->y = (float)fmin(c->y + 20, H.E);                     /* 13379: the slam */
        if (c->y >= H.E && !H.fl16) { H.fl16 = 1; if (G.cam.Z < 4) G.cam.Z = 4; snd_play_orig(132, 220, 128); }   /* 13380 */
        if (H.M > 150) c->av[5] = fmin(15 * (H.M - 150), 255);                /* 13381: fading out (INFERRED direction) */
        if (H.M >= 80 && H.M < 150 && c->an.b && !c->hidden) {                /* 13382: Player_326 on a claw: LIFE.B + 10 */
            const frame_t *f = anim_frame(&c->an);
            int x0 = (int)c->x - f->hx, y0 = (int)c->y - f->hy;
            if (csens_rect(p, x0 + 8, y0, x0 + f->w - 8, y0 + f->h - 1)) hurt(p, 10, 0);
        }
    }
    if (H.K == 2 && H.M > 180) {                                             /* 13383: to the laser's closing phase */
        H.N = 2; H.K = 1; H.M = 300;
        for (int k = 0; k < 3; k++) { obj_t *c = slot(H.claw[k]); if (c) { c->y = (float)G.lvl.h + 100; c->hidden = 1; } }
    }
    /* ---- K 3: the flames (13384-13389) */
    if (H.K == 3) H.M += 1;                                                  /* 13384 */
    if (H.K == 3 && fmod(H.M, 10) == 5) {                                     /* 13385: a flame 500 px over the player */
        H.M += 1;
        int k; for (k = 0; k < NFLAME && slot(H.flame[k]); k++) ;
        obj_t *f = k < NFLAME ? spawn(OT_PLB_FLAME, "plb_flame", (float)X, (float)(PY(p) - 500), 3) : NULL;
        if (f) {
            H.flame[k] = (short)(f - G.o);
            f->av[0] = sbb_cos(5 * H.M) * 120;                              /* A */
            f->av[1] = -120 - cross_rand(20);                                /* B: -120 - (rand * 5 >> 13) */
            f->av[3] = 255;                                                  /* D (ctor) */
            f->an.frame = cross_rand(8) % (f->an.b ? G.ens.anims[f->an.anim].count : 1);   /* ctor: a random direction of 32 (INFERRED: a random frame) */
        }
    }
    for (int k = 0; k < NFLAME; k++) {
        obj_t *f = slot(H.flame[k]);
        if (!f || f->type != OT_PLB_FLAME) { H.flame[k] = -1; continue; }
        if (f->av[2] == 0) {                                                  /* 13386: held over the player, fading in */
            f->x = (float)(int)(X + f->av[0]); f->y = (float)(int)(PY(p) + f->av[1]);
            f->av[3] -= 10;
            if (f->av[3] <= -200) {                                           /* 13387: launched at the player (32 directions, speed 100) */
                f->av[2] = 1;
                double a = atan2(-(PY(p) - f->y), X - f->x) * SB_RAD; if (a < 0) a += 360;
                int dir = (int)(a / 11.25);
                f->vx = (float)(12.5 * cos(dir * 11.25 / SB_RAD)); f->vy = (float)(-12.5 * sin(dir * 11.25 / SB_RAD));
            }
        } else {
            f->x += f->vx; f->y += f->vy;                                     /* BallMovement: speed / 8 px a frame */
            if (f->x < G.camx - 80 || f->x > G.camx + SCREEN_W + 80 || f->y < G.camy - 80 || f->y > G.camy + SCREEN_H + 80) { f->alive = 0; H.flame[k] = -1; continue; }   /* 13388 */
        }
        if (f->av[3] < 128 && f->an.b) {                                      /* contact: LIFE.B 10 (RV p7 2440; the rule INFERRED) */
            const frame_t *fr = anim_frame(&f->an);
            int x0 = (int)f->x - fr->hx, y0 = (int)f->y - fr->hy;
            if (csens_rect(p, x0 + 6, y0 + 8, x0 + fr->w - 6, y0 + fr->h - 4)) hurt(p, 10, 0);
        }
    }
    if (H.K == 3 && H.M > 210) { H.N = 0; H.K = 1; H.M = 300; }               /* 13389 */
}

/* ======================================================================== the hit spots and Eye2 (13390-13403) */
static void spots(void) {
    static const int ST[3] = { OT_PLB_SCALE2, OT_PLB_SCALE3, OT_PLB_SCALE4 };
    int total = cnt(OT_PLB_HIT1) + cnt(OT_PLB_HIT2) + cnt(OT_PLB_HIT3);
    for (int g = 0; g < 3; g++) {
        int ht = OT_PLB_HIT1 + g, n = cnt(ht);
        for (int k = 0; k < n; k++) {
            obj_t *h = get(ht, k), *s = get(ST[g], k < cnt(ST[g]) ? k : cnt(ST[g]) - 1);
            if (!h) continue;
            if (s) { h->x = s->x; h->y = s->y - (g == 2 ? 18 : 24); }          /* 13390 */
            int def = (h->fl & F_DEFEATED) != 0;
            if (!def) {                                                        /* 13391 / 13392 (the background test near the view only) */
                int near = h->x > G.camx - 64 && h->x < G.camx + SCREEN_W + 64 && h->y > G.camy - 64 && h->y < G.camy + SCREEN_H + 64;
                h->hidden = near && obj_overlaps_bg(h);
                if (h->hidden && h->c < 2) h->c = 2;                           /* hidden in a wall: no hits (INFERRED) */
            }
            if (def && !(h->fl & FL_DONE)) {                                   /* 13397-13399 */
                h->fl |= FL_DONE;
                snd_play_orig(45, 220, 128);                                   /* (stop_sample 23: the shared defeat sound is replaced) */
                obj_effect(h->x, h->y, "explosion02");
                h->hidden = 1; H.O += 1;
            }
        }
    }
    obj_t *e2 = slot(H.eye2), *ey = get(OT_PLB_EYE, 0);
    if (e2 && ey && H.K < 6 && H.O > total - 8) {                             /* 13400: Eye2 on the eye */
        e2->av[3] += 1; e2->flip = ey->flip; e2->ang = ey->ang; e2->x = ey->x; e2->y = ey->y;
        if (!e2->av[6]) { e2->av[6] = 1; e2->hidden = 0; }
    }
    if (e2 && e2->av[6] && !(e2->fl & F_DEFEATED)) {
        if (e2->av[3] > 10) { e2->fl ^= FL_DONE; e2->av[3] = 0; e2->hidden = (e2->fl & FL_DONE) != 0; }   /* 13401 / 13402: the blink */
        if (e2->c > 0) { e2->av[3] = 9; e2->fl &= ~FL_DONE; e2->hidden = 0; }                             /* 13403 (INFERRED: while hit) */
    }
}

/* ======================================================================== defeat and ending (13404-13435) */
static void pangu_spawn(void) {
    obj_t *pg = spawn(OT_PLB_PANGU, "plb_pangu", 6200, 1700, 2);              /* 13415: NPCPangu x 6200, K 1700 */
    if (!pg) return;
    pg->flip = 1;                                                              /* RV p6: direction 16 */
    H.pangu = (short)(pg - G.o); H.pK = 1700; H.pD = 4; H.pV = 0;
    for (int k = 0; k < NARM; k++) {
        obj_t *a = spawn(OT_PLB_ARM, k < 9 ? "plb_larm" : "plb_rarm", 6200, 1700, 2);
        if (a) { H.arm[k] = (short)(a - G.o); a->av[0] = k % 9; a->flip = 1; }
    }
}
static void pangu_frame(void) {
    obj_t *pg = slot(H.pangu);
    if (!pg) return;
    H.pD += 2;                                                                 /* RV p6: D + 2 a frame */
    if (H.M >= 250 && H.pK < 1765) { H.pV += 0.5; H.pK = fmin(H.pK + H.pV, 1765); }   /* the drop (RV p6: 1700 until ~M 245, 1765 at ~M 280; INFERRED gravity) */
    pg->x = 6200; pg->y = (float)(int)(H.pK + (H.pK < 1765 ? 8 * sbb_sin(H.pD) : 0));   /* 13428: y = K + 8 sin(J) (RV: J + 2 a frame) */
    for (int k = 0; k < NARM; k++) {                                          /* 13431 / 13432: the arms (INFERRED: hanging chains) */
        obj_t *a = slot(H.arm[k]);
        if (!a) continue;
        int i = (int)a->av[0], side = k < 9 ? -1 : 1;
        double sw = H.pK < 1765 ? 4 * sbb_sin(H.pD * 2 - 25 * i) : 0;     /* (RV p6 f2300: legs spread to the floor, ~110 px apart) */
        a->x = (float)(int)(pg->x + side * (8 + 5.5 * i) + sw);
        a->y = (float)(int)(pg->y + 8 + 10 * i);
        a->an.frame = i == 8 ? 1 : 0;
    }
}
static void ending(player_t *p) {
    obj_t *e2 = slot(H.eye2);
    if (H.K < 6 && e2 && (e2->fl & F_DEFEATED)) {                             /* 13404 */
        H.K = 6; H.H = 24; H.G = 0; H.M = 0;
        obj_t *pu = get(OT_PLB_PUPIL, 0); if (pu) pu->scale = 0.01f;
        music_fade(60, 0);                                                     /* fadeout_music(1000) + lock_music */
        obj_t *br = get(OT_PLB_BREATH, 0), *lz = laser();
        if (br) br->hidden = 1;
        if (lz) lz->hidden = 1;
        for (int k = 0; k < H.nwarn; k++) { obj_t *w = slot(H.warn[k]); if (w) w->hidden = 1; }
        for (int k = 0; k < 3; k++) { obj_t *c = slot(H.claw[k]); if (c) c->hidden = 1; }
        snd_play_orig(334, 220, 128);                                          /* (stop_sample 330-332) */
#ifdef FP_HOST_ENV
        if (getenv("FP_TRACE_SND")) printf("plboss defeat @%d\n", G.frame);
#endif
    }
    if (H.K == 6) {
        H.M += 1;
        if (H.M > 100) {
            if (G.frame % 6 == 0) {                                            /* 13406: explosions, the shake */
                snd_play_orig(cross_rand(2) ? 311 : 99, 220, 128);
                G.cam.Z = (float)fmin(G.cam.Z + 10, 10);
            }
            obj_t *ey = get(OT_PLB_EYE, 0);                                     /* 13407 / 13408-13410: white bursts on the eye and the body */
            if (ey && (G.frame & 1)) obj_effect(ey->x - 100 + cross_rand(200), ey->y - 100 + cross_rand(200), "wburst");
            int t = OT_PLB_SCALE2 + cross_rand(3), n = cnt(t);
            obj_t *s = n ? get(t, cross_rand(n)) : NULL;
            if (s && (G.frame & 1) && s->x > G.camx - 50 && s->x < G.camx + SCREEN_W + 50) obj_effect(s->x - 50 + cross_rand(100), s->y - 50 + cross_rand(100), "wburst");
        }
        if (H.M == 400) {                                                      /* 13411: the white flash, K 7 */
            snd_play_orig(76, 220, 128);
            H.flash = 255; H.K = 7; H.M = 0;
        }
        return;
    }
    if (H.K != 7) return;
    H.M += 1;
    if (H.M < 100) H.flash = fmax(H.flash - 16, 0);                           /* 13412: the flash clears, 16 a frame */
    if (H.M == 20 && !H.black) {                                               /* 13415: the scene (Lilac at 6000, Pangu, the dark arena) */
        H.black = 1; H.lock = 1;
        for (int t = OT_PLB_BREATH; t <= OT_PLB_PLACED_LAST; t++)
            for (int k = 0; k < cnt(t); k++) { obj_t *o = get(t, k); if (o) o->hidden = 1, o->c = 2; }
        if (e2) e2->hidden = 1;
        p->x = 6000; p->y = 1770 + ORIG_REF_DY; p->vx = p->vy = 0; p->gsp = 0; p->face = 1; p->ground = 0;
        if (p->state != ST_DEAD) p->state = ST_NORMAL;
        /* RV p6: CAM A 6100, B 1740 and (13416) H -60: the view centred on (6100, 1680), fixed (bounds the size of the view, no easing) */
        camera_boss_bounds(6100 - SCREEN_W / 2, 1680 - SCREEN_H / 2, 6100 + SCREEN_W / 2, 1680 + SCREEN_H / 2);
        G.cam.bC = G.cam.lC; G.cam.bD = G.cam.lD; G.cam.bE = G.cam.lE; G.cam.bF = G.cam.lF;
        pangu_spawn();
    }
    if (H.black) {
        pangu_frame();
        obj_t *pg = slot(H.pangu);
        if (!H.landed && pg && H.pK >= 1765) { H.landed = 1; snd_play_orig(51, 220, 128); G.cam.Z = 3; }   /* 13423 */
        if (H.M == 310) { snd_play_orig(29, 220, 128); if (pg) obj_effect(pg->x - 13, pg->y - 15, "explosion02"); }   /* 13424 */
        if (H.M == 330) { snd_play_orig(29, 220, 128); if (pg) obj_effect(pg->x + 6, pg->y + 8, "explosion02"); }    /* 13425 */
    }
    if (H.M == 421 && !H.results) {                                           /* RV p4 / p6: the results 421 frames into K 7 (0x2219e) */
        H.results = 1; H.lock = 0;                                             /* (the results drive the player from here) */
        results_begin_boss(game_timer());
#ifdef FP_HOST_ENV
        if (getenv("FP_TRACE_SND")) printf("plboss results @%d\n", G.frame);
#endif
    }
}

/* ======================================================================== the frame */
void pl_boss_pre(player_t *p) { (void)p; }
void pl_boss_frame(player_t *p) {
    if (!H.on) return;
    obj_t *nk = slot(H.neck);
    if (!nk) return;
    double X = PX(p);
#ifdef FP_HOST_ENV
    { static int once; const char *e = getenv("FP_PLB_O"); if (e && !once && G.frame >= atoi(e)) { once = 1; H.O = 85; } }   /* tests: 85 spots down */
    if (G.frame <= 1) {}
#endif
    /* ---- asleep (13333-13336): the player inside the arena (past the right CameraLimitBoss marker) */
    if (H.K == -2) {
        float rx = H.rx;                                                        /* CameraLimitBossRight.x (found at spawn) */
        if (H.M < 120 && (H.M >= 0 || X > rx)) H.M += 1;                      /* 13333 (RV p1: M + 1 a frame from the arena entry) */
        if (H.M >= 120 && !H.snd333) { H.snd333 = 1; snd_play_orig(333, 220, 128); }   /* 13334 */
        if (H.M >= 120) H.lock = 1;                                            /* 13335: CTRL.M 1 */
        if (H.M >= 120 && p->ground && (p->ground ? p->gsp : p->vx) == 0 && !dead(p)) {   /* 13336: the fight */
            H.fight = 1; H.D = X + 100; H.M = 0; H.K = -1;
#ifdef FP_HOST_ENV
            if (getenv("FP_TRACE_SND")) printf("plboss fight @%d\n", G.frame);
#endif
        }
    }
    if (H.fight) {
        H.A = sbb_fmod360(H.A + H.G);                                          /* 13338 */
        pupil(p);                                                              /* 13339 / 13340 */
        if (H.K >= 6) for (int k = 0; k < cnt(OT_PLB_PUPIL); k++) { obj_t *o = get(OT_PLB_PUPIL, k); if (o) { o->av[1] = o->av[3]; o->av[2] = o->av[4]; } }   /* 13341 */
        neck_pos(nk);                                                          /* 13342 */
        rig(nk);                                                               /* 13343 / 13344 */
        /* ---- the entrance (13347-13352) */
        if (H.K == -1) {
            H.L = fmax(H.L - 1, 0); H.G = 0.5;                                 /* 13347 (OTHERS.Y 50: the skin's animation 41 direction 24) */
            if (!dead(p)) player_pose(p, 41, 24, -1);
            if (H.L <= 0) H.M += 1;                                            /* 13348 */
            if (H.L == 1) { snd_play_orig(330, 220, 128); const char *m = level_music_slot(4); if (m) music_play(m); }   /* 13349: ML.Y 1, unlock_music */
            if (H.L <= 0 && H.M < 120) { H.H = fmin(H.H + 2, 24); G.cam.Z = 12; }   /* 13350: the roar */
            if (H.L <= 0 && H.M > 120) H.H = fmax(H.H - 2, 0);               /* 13351 */
            if (H.M > 140) { H.K = 0; H.M = 0; snd_play_orig(331, 220, 128); H.lock = 0; }   /* 13352: CTRL.M 0 */
        }
        /* ---- flight (13354-13358) */
        if (H.K == 0) {
            H.D += H.I; H.M += 1;                                              /* 13354 */
            if (H.J < 0) { H.F = 20 - 6 * H.I; H.G = 2 - 0.2 * H.I; H.I = fmax(H.I - 0.15, -15); }   /* 13355 */
            else { H.F = 6 * H.I + 20; H.G = 0.2 * H.I + 2; H.I = fmin(H.I + 0.15, 15); }         /* 13356 */
            if ((H.J < 0 && H.D <= X - 2600) || (H.J > 0 && H.D >= X + 2600) || H.M > 420) H.L = fmin(H.L + 4, 255);   /* 13357: fading off */
            if (H.L >= 255) {                                                  /* 13358: turn, the next attack */
                H.M = 0; H.J = -H.J; H.K = H.N;
            }
        }
        attacks(p);
        spots();
        ending(p);
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_PLB")) {
        obj_t *e2 = slot(H.eye2), *lz = laser();
        printf("plb %d px %.0f py %.0f K %.0f M %.0f N %.0f O %.0f L %.0f D %.1f E %.0f F %.1f I %.2f J %.0f H %.0f nx %.0f ny %.0f e2A %.0f e2h %d lz %d lx %.0f hp %d st %d fl %.0f\n",
               G.frame, X, PY(p), H.K, H.M, H.N, H.O, H.L, H.D, H.E, H.F, H.I, H.J, H.H, nk->x, nk->y, e2 ? e2->A : -1, e2 ? e2->hidden : -1,
               lz ? !lz->hidden : -1, lz ? lz->x : -1, p->hp, p->state, H.flash);
    }
#endif
}
int pl_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }       /* every rule runs in pl_boss_frame */
int pl_boss_q20(int type) { return is_hit(type) || type == OT_PLB_EYE2; }     /* RE: the hit spots and Eye2 take the shared hits (A 3 / 20) */
int pl_boss_input(uint32_t *held) { if (!H.on || !H.lock) return 0; *held = 0; return 1; }   /* CTRL.M 1 (13335 .. 13352, the ending) */
int pl_boss_player_hidden(void) { return 0; }
float pl_boss_hud_dy(void) { return 0.f; }

/* ======================================================================== draw */
static uint32_t alpha_col(double coef) { int a = 255 - (int)clampd(coef, 0, 255); return ((uint32_t)a << 24) | 0xffffff; }
int pl_boss_draw_obj(const obj_t *o, int pass) {
    (void)pass;
    if (!H.on) return 1;
    switch (o->type) {
    case OT_PLB_NECK:                                                          /* pass 0: the ending's dark arena (INFERRED: the backdrop's black) */
        if (H.black) r_rect(0, 0, SCREEN_W, SCREEN_H, 0xff000000);
        return 1;
    case OT_PLB_FX:                                                            /* the white flash (13411 / 13412) */
        if (H.flash > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)(int)H.flash << 24) | 0xffffff);
        return 1;
    case OT_PLB_LASER: {                                                       /* the beam strip stretched to the view edge */
        if (o->hidden || !o->an.b) return 1;
        int x0, x1; laser_rect(o, &x0, &x1);
        r_set_xscale((float)(x1 - x0) / 10.f);
        frame_draw_rot(o->an.b, anim_frame(&o->an), floorf(o->x - G.camx), floorf(o->y - G.camy), 0, o->flip, 0xffffffff);
        r_set_xscale(1.f);
        return 1; }
    case OT_PLB_CLAW: if (!o->hidden) sbb_draw(o, alpha_col(o->av[5])); return 1;
    case OT_PLB_FLAME: sbb_draw(o, alpha_col(o->av[3])); return 1;
    case OT_PLB_EYE2: case OT_PLB_PANGU: case OT_PLB_ARM: sbb_draw(o, 0xffffffff); return 1;
    }
    if (o->type == OT_MT_WARN) return 0;
    if (o->hidden) return 1;
    if (is_head(o->type) || is_body(o->type) || is_hit(o->type)) { if (H.L < 255) sbb_draw(o, alpha_col(H.L)); return 1; }   /* 13345: the hologram's transparency */
    return 1;
}
