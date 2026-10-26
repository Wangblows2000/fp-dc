/* Sky Battalion enemies (session 19): EnemyHopperMkII_2189 (+ EnemyFragBomb_2103), EnemyMagmeon_2203, EnemyRotorBlade_2184 (+ its
 * Spikes_2183, the SpawnTube_2185, BreakableSpike2_2228, RotorBladeDeath_2231), EnemyDrone_2138 (+ EnemyDroneLaser_2232),
 * EnemyAxleTurret_2139 (+ AxleTurret2_2140, EnemyBullet_2097), EnemyDNACannon_2260 (+ Base_2261, EnemyDNALaser_2290),
 * EnemyPelletizer_2303 (+ the DV EnemyPellet_934), EnemyMagnetizer_2294 (+ Crate_2295, Hand_2292, Line_2293). AT's EnemySlime_717
 * (SB4) is aqua.c's type.
 * Source: build/re/spec_sb_enemies.md. Labels: RE (read from the events / constructors), RV (original traces session 19,
 * build/orig_traces/sb_s19e: hop_idle, hop_hit, mag_idle, rotor_idle, axle_idle, drone_idle, dna_idle, pel_idle, magn_idle),
 * INFERRED (marked). The shared hit / defeat / fly-away rules are combat.c's (2730-2768): these classes are Q20 (sb_enemy_q20).
 * Order: sb_enemy_pre (objects.c, before combat) runs the projectiles' collision handlers; sb_enemy_obj runs each object's own
 * events from the objects.c per-object pass.
 * Performance: every class keeps its instance list (spawn + runtime children, EACH), so no event scans every slot; nothing runs
 * in a level without these classes (S.on).
 * Alterables in av[] (letters as the spec, listed per class); flags: 0x40 defeated (combat.c), F_NOFLY, own flags >= 0x1000. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEFEATED(o) ((o)->fl & 0x40)
#define FL2 0x1000                                 /* "flag 2": this cycle's shot made */
#define FL_EXPL 0x2000                             /* frag bomb exploded (its flag 6) */
#define RAD 57.29577951308232
#define RT0 OT_SBE_FIRST
#define NRT (OT_SBE_LAST - OT_SBE_FIRST + 1)
#define LMAX 96
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
static struct { int on; short lim_x[64], lim_y[64]; int nlim; } S;

static int R(int n) { return cross_rand(n); }
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static double c_sin(double deg) { double r = nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / RAD); }
static double c_cos(double deg) { double r = nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / RAD); }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }

/* ---- sprites: '<base>' direction 0, '<base>_l' direction 16 (a missing _l set falls back to the base) */
static int ens_of(const obj_t *o, const char *base) {
    char nm[24]; snprintf(nm, sizeof nm, "%s%s", base, o->dir == 16 ? "_l" : "");
    int a = ens_anim(nm);
    return a >= 0 ? a : ens_anim(base);
}
static void set_a(obj_t *o, const char *base) {                                 /* force_animation (kept when unchanged) */
    int a = ens_of(o, base);
    if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a);
}
static void restart_a(obj_t *o, const char *base) { int a = ens_of(o, base); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static int a_is(const obj_t *o, const char *base) { return o->an.b == &G.ens && o->an.anim == ens_of(o, base); }
static int a_playing(const obj_t *o, const char *base) { return a_is(o, base) && !o->an.done; }
static void face(obj_t *o, int dir) {                                           /* set_direction: animation, frame and counter kept */
    if (o->dir == dir) return;
    char stem[24] = "";
    if (o->an.b == &G.ens) { snprintf(stem, sizeof stem, "%s", G.ens.anims[o->an.anim].name); size_t n = strlen(stem); if (n > 2 && !strcmp(stem + n - 2, "_l")) stem[n - 2] = 0; }
    o->dir = dir;
    if (!stem[0]) return;
    int a = ens_of(o, stem);
    if (a < 0 || a == o->an.anim) return;
    int fr = o->an.frame, acc = o->an.acc, loops = o->an.loops, done = o->an.done;
    o->an.anim = a; o->an.frame = fr < G.ens.anims[a].count ? fr : G.ens.anims[a].count - 1; o->an.acc = acc; o->an.loops = loops; o->an.done = done;
}
static void act_pt(const obj_t *o, float *x, float *y) { double ax, ay; relic_act_point(o, &ax, &ay); *x = (float)ax; *y = (float)ay; }
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static obj_t *spawn(int type, const char *anim, float x, float y) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = 3; o->born = G.frame; o->par = -1;
    if (anim) set_a(o, anim);
    lst_add(o);
    return o;
}
static void effect(float x, float y, const char *anim, float scale) {           /* an effect object (object bank first, then _ens) */
    const bank_t *b = &G.objs; int a = bank_anim(b, anim);
    if (a < 0) { b = &G.ens; a = ens_anim(anim); }
    if (a < 0) return;
    obj_t *e = obj_alloc();
    if (!e) return;
    e->type = OT_EXPLODE; e->alive = 1; e->x = x; e->y = y; e->layer = 3; e->scale = scale;
    anim_set(&e->an, b, a);
}
static void shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }                    /* CAM.Z = max(CAM.Z, z) */
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }       /* !is_near_border(-n) */
static int csens_hit(const player_t *p, const obj_t *o) {                       /* PlayerCharacterSensor_375 */
    int X = (int)PX(p), Y = (int)PY(p), small = player_char_sensor_small(p);
    return o->an.b && obj_rect_hit(o, X - 7, small ? Y - 13 : Y - 34, X + 9, small ? Y + 15 : Y + 13);
}
/* "R-hurt n": MV.R = 1, LIFE.B += n when MV.R == 0 and OTHERS.E <= 0 (2414 applies it next frame) */
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }
static int r_hurt(player_t *p, int n) { if (!vulnerable(p)) return 0; player_contact(p, n); return 1; }
/* the background or a Q7 (solid) object under the object (the crash tests "overlaps background / Q7"; Q7 near it only) */
static int solid_under(const obj_t *o) {
    if (obj_overlaps_bg(o)) return 1;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *s = &G.o[i];
        if (!s->alive || s == o || !s->an.b || fabsf(s->x - o->x) > 160 || fabsf(s->y - o->y) > 160) continue;
        if (obj_in_group(s->type, OBJGRP_SOLID) && !(s->fl & 1) && obj_obj_hit(o, s)) return 1;
    }
    return 0;
}
static int limiter_hit(const obj_t *o) {                                        /* EnemyLimiter_383: image 16 x 32, hotspot (21, 18) */
    for (int k = 0; k < S.nlim; k++)
        if (fabsf(S.lim_x[k] - o->x) < 96 && fabsf(S.lim_y[k] - o->y) < 96 &&
            obj_rect_hit(o, S.lim_x[k] - 21, S.lim_y[k] - 18, S.lim_x[k] - 5, S.lim_y[k] + 14)) return 1;
    return 0;
}
/* the wreck's crash on the ground (9419 / 9420, 9374, 10073, 10044 ... : explosion, keystone, sound 29, debris), INFERRED
 * debris motion (as combat.c corpse_land) */
static void debris4(const obj_t *o) {                                           /* loop_spawndebris x4 (INFERRED motion, as combat.c corpse_land) */
    int a = ens_anim("debris");
    for (int k = 0; k < 4 && a >= 0; k++) {
        obj_t *d = obj_alloc();
        if (!d) break;
        memset(d, 0, sizeof *d);
        d->type = OT_DEBRIS; d->alive = 1; d->layer = 3; d->x = o->x; d->y = o->y; anim_set(&d->an, &G.ens, a); d->an.frame = 10 - k;
        d->vx = (float)(k - 1.5) * 1.5f; d->vy = -3.f - k * 0.5f;
    }
}
static void crash(obj_t *o, int keystone) {
    snd_play_orig(29, 220, 128); obj_effect(o->x, o->y, "explosion");
    if (keystone) keystone_spawn(o->x, o->y, 0, 0);
    debris4(o);
}
/* the shared crash of a defeated Q20 enemy of this file: falls with the fly-away (combat.c 2765) until it meets the ground */
static int wreck_lands(obj_t *o) { return DEFEATED(o) && o->ky > 0 && solid_under(o); }

int sb_enemy_q20(int t) {                                                       /* RE on_frame_37..39_init qualifier 20 */
    return t == OT_SB_HOPPER || t == OT_SB_MAGMEON || t == OT_SB_ROTOR || t == OT_SB_DRONE || t == OT_SB_AXLE || t == OT_SB_DNA ||
           t == OT_SB_PELLETIZER || t == OT_SB_MAGNET;
}

/* ================================================================ EnemyHopperMkII_2189 (9409-9420, 9769) + EnemyFragBomb_2103
 * A HP 20 (ctor), av: [0] G state (0 wait, 1 jump, 2 landed / throwing, 6 wreck), [1] H facing sign (ctor -1), [2] I jump speed
 * (ctor 2), [3] D ground Y, [4] E float Y, [5] F y speed, [6] V crashes. RV hop_idle: a 117-frame cycle (crouch 17 frames, jump
 * from F -5 by +0.3 at I = 2 / 3 px a frame, land anim 11 20 frames, throw anim 10 frame 9 = the bomb, sound 237). */
static void hopper_spawn(obj_t *o) {
    o->A = 20; o->av[1] = -1; o->av[2] = 2; o->dir = 16; o->av[3] = o->av[4] = floorf(o->y);   /* 9769: D = E = Y */
    set_a(o, "sbh_idle");
}
static void hopper_face_player(obj_t *o, const player_t *p) {
    if (o->x >= PX(p)) { o->av[1] = -1; face(o, 16); } else { o->av[1] = 1; face(o, 0); }
}
static void hopper_frame(obj_t *o, player_t *p) {
    double *Gs = &o->av[0], *H = &o->av[1], *I = &o->av[2], *D = &o->av[3], *E = &o->av[4], *F = &o->av[5];
    if (DEFEATED(o)) {
        if (*Gs != 6) {                                                          /* 9419 (RV hop_hit: the first crash at the defeat) */
            *Gs = 6; o->av[6] = 1; o->ky = -4; o->A = -1; set_a(o, "sbh_wreck");
            snd_play_orig(29, 220, 128); obj_effect(o->x, o->y, "explosion"); debris4(o);
        } else if (wreck_lands(o)) { o->av[6] = 2; crash(o, 1); o->alive = 0; }  /* 9419 / 9420 (RV: the second crash, keystone, removed) */
        return;
    }
    if (*Gs == 2 && a_is(o, "sbh_throw") && o->an.done) { *Gs = 0; hopper_face_player(o, p); }   /* 9417 / 9418 */
    if (*Gs == 0 && in_view(o, 120)) set_a(o, "sbh_crouch");                     /* 9409 */
    if (*Gs == 0 && a_is(o, "sbh_crouch") && o->an.frame >= 6) {                 /* 9410 */
        *Gs = 1; *F = -5; *I = 2 + R(2); *E = floorf(o->y);
    }
    if (*Gs == 1) {                                                              /* 9411 */
        o->y = (float)(int)*E; *E += *F; *F = fmin(*F + 0.3, 15.);
        o->x = (float)(int)(floorf(o->x) + *I * *H);
        if (limiter_hit(o)) o->x = (float)(int)(floorf(o->x) - *I * *H);          /* 9412: a limiter stops the hop (INFERRED: the side test) */
        if (*F > 0 && floorf(o->y) >= *D) { o->y = (float)*D; *Gs = 2; restart_a(o, "sbh_land"); }   /* 9413 */
    }
    if (*Gs == 2 && !a_playing(o, "sbh_land") && !a_is(o, "sbh_throw")) {       /* 9414 / 9415 */
        o->fl &= ~FL2; *H = PX(p) <= o->x ? -1 : 1; face(o, *H < 0 ? 16 : 0); restart_a(o, "sbh_throw");
    }
    if (in_view(o, 150) && a_playing(o, "sbh_throw") && o->an.frame == 9 && !(o->fl & FL2)) {   /* 9416 */
        float ax, ay; act_pt(o, &ax, &ay);
        obj_t *b = spawn(OT_SB_FRAGBOMB, "sbh_bomb", ax, ay);
        if (b) { b->av[0] = 5 * *H; b->av[1] = -5; b->av[2] = b->av[3] = 1; b->dir = R(4); b->layer = o->layer; }
        effect(ax, ay, "wburst", 1.f);
        o->fl |= FL2; snd_play_orig(237, 220, 128);
    }
}
/* frag bomb: av[0] A x speed, [1] B y speed; 9421 move + spin, 9424 R-hurt 5, background: explode (anim 7, sound 312, CAM.Z 3,
 * flag 6), 9425 white bursts, 9426 removed at the end of anim 7. RV hop_idle: y +0.5 a frame, bursts 26 frames after the throw */
static void bomb_frame(obj_t *o, player_t *p) {
    o->x = (float)(int)(floorf(o->x) + o->av[0]); o->y = (float)(int)(floorf(o->y) + o->av[1]);   /* 9421 */
    o->ang = (float)nrm(o->ang - 2 * o->av[0]); o->av[1] += 0.5;
    if (!(o->fl & FL_EXPL) && csens_hit(p, o)) r_hurt(p, 5);                    /* 9424 */
    if (!(o->fl & FL_EXPL) && obj_overlaps_bg(o)) {                              /* test_collisions_36 background handler */
        restart_a(o, "sbh_bombx"); snd_play_orig(312, 220, 128); o->av[0] = o->av[1] = 0; shake(3); o->fl |= FL_EXPL; o->ang = 0;
    }
    if (o->fl & FL_EXPL) {                                                       /* 9425 / 9426 */
        effect(o->x - 20 + R(41), o->y - 30 + R(41), "wburst", 2.f);
        if (o->an.done) o->alive = 0;
    }
    if (o->y > G.lvl.h + 64) o->alive = 0;
}

/* ================================================================ EnemyMagmeon_2203 (9427-9454): the Cacteon walker (loop_cacteon_36 /
 * cacteon_floor) that heats the player. A HP 5; st = D (0 air, 1 landed, 2 walk, 3 jump, 4 defeated), vx / vy = E / F, fx / fy = G / H
 * (float x, y), tm = I walk timer, av[0] J heat. RV mag_idle: walk 1.6 px toward the player, I to 60 then the jump (anim 7, F -6 at
 * its frame 5), gravity +0.25. Heat (9430-9433): +3 a frame on the player's sensor (no fire shield), smoke every 3 frames, a red
 * tint (255 - J), 5 damage when J > 60 (then J = 0, INFERRED); off the sensor J = 0. */
static int mag_feet(obj_t *o) {                                                  /* force_direction(24): the feet mask on the level */
    animator_t keep = o->an; int a = ens_anim("sbm_feet"), hit;
    if (a < 0) return obj_overlaps_bg(o);
    anim_set(&o->an, &G.ens, a); hit = solid_under(o); o->an = keep;
    return hit;
}
static void magmeon_spawn(obj_t *o) { o->A = 5; o->dir = 0; o->st = 0; o->vx = o->vy = 0; o->fx = o->x; o->fy = o->y; o->tm = 0; set_a(o, "sbm_idle"); }
static void magmeon_frame(obj_t *o, player_t *p) {
    if (DEFEATED(o)) { o->st = 4; set_a(o, "sbm_dead"); if (wreck_lands(o)) { snd_play_orig(50, 220, 128); obj_effect(o->x, o->y, "explosion"); o->alive = 0; } return; }   /* 9452-9454 */
    if (o->st == 1 && in_view(o, 50) && !a_playing(o, "sbm_land")) { o->vx = o->x < PX(p) ? 1.6f : -1.6f; o->st = 2; set_a(o, "sbm_walk"); }
    o->fx += o->vx; o->fy += o->vy; o->x = (float)(int)o->fx; o->y = (float)(int)o->fy;
    int feet = mag_feet(o);
    if (!feet) o->vy = fminf(o->vy + 0.25f, 12.f);                              /* 9438 */
    if (o->vx != 0) {                                                            /* the wall bounce (cacteon 5016) */
        const frame_t *f = o->an.b ? anim_frame(&o->an) : NULL;
        if (f) { float x0 = floorf(o->x) - f->hx, x1 = x0 + f->w, y1 = floorf(o->y) - f->hy + f->h;
            if (obj_overlaps_bg(o) && ((o->vx > 0 && px_solid(&G.lvl, (int)(x1 + 4), (int)(y1 - 8))) || (o->vx < 0 && px_solid(&G.lvl, (int)(x0 + 4), (int)(y1 - 8))))) o->vx = -o->vx; }
    }
    if (o->st == 0 && feet && o->vy >= 0 && !a_playing(o, "sbm_walk")) { restart_a(o, "sbm_land"); o->vx = 0; o->st = 1; }   /* 9443 */
    if (o->st < 4 && feet && o->vy >= 0) { o->vy = 0; o->fy -= 1; o->y = (float)(int)o->fy; }                                 /* 9444 */
    if (o->vx > 0) face(o, 0); else if (o->vx < 0) face(o, 16);                  /* 9445 / 9446 */
    if (o->st == 2 && o->vy < 0.5f) o->tm++;                                     /* 9447 */
    if (o->st == 2 && o->tm > 60) { o->tm = 0; o->st = 3; }                      /* 9448 */
    if (o->st == 3) { set_a(o, "sbm_jump"); o->vx = 0; if (o->an.frame == 5) { o->vy = -6; o->st = 0; } }   /* 9449-9451 */
    if (o->st == 2) set_a(o, "sbm_walk");
    /* heat */
    if (csens_hit(p, o) && !dead(p)) {
        if (!p->hurt_pending && p->shield != 2 && !player_invincible(p)) {      /* 9430 */
            o->av[0] += 3; int c = 255 - (int)fmin(o->av[0], 255.);
            p->tint = 15; p->tint_argb = 0xffff0000u | ((uint32_t)c << 8) | (uint32_t)c;
        }
        if (o->av[0] > 0 && G.frame % 3 == 0) effect((float)PX(p) - 10 + R(21), (float)PY(p) - 10 + R(21), "sbm_smoke", 1.f);   /* 9431 */
        if (o->av[0] > 60 && r_hurt(p, 5)) o->av[0] = 0;                        /* 9433 */
    } else o->av[0] = 0;                                                         /* 9432 */
}

/* ================================================================ EnemyRotorBlade_2184 (9390-9408) + Spikes_2183 + SpawnTube_2185
 * A HP 15; av: [0] D float x, [1] E float y, [2] F x speed, [3] G y speed, [4] H state (0 in the tube, 1 air, 2 rolling),
 * [5] I spin angle, [6] J ground y, [7] K direction; M (roll sound counter) in kx-unused... kept in tm (x100). RV rotor_idle:
 * pops out at G -8 when the player is within 99 px (sound 7), lands on J = Y - 64 (sound 54, CAM.Z 3), rolls with F += 0.1 K
 * (|F| <= 10), M += |F| + 3 (sound 264 past 50), bounces off limiters (sound 51: F = -0.3 F, G -4, toward the player). */
static void rotor_spawn(obj_t *o) { o->A = 15; o->av[7] = -1; o->layer = 2; set_a(o, "sbr_roll"); o->bd = 0; }
static obj_t *rotor_spikes(const obj_t *o) { obj_t *s; EACH(OT_SB_ROTORSPK, s) if (s->par == (int)(o - G.o)) return s; return NULL; }
static void rotor_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gv = &o->av[3], *H = &o->av[4], *I = &o->av[5], *J = &o->av[6], *K = &o->av[7];
    obj_t *sp = rotor_spikes(o);
    if (DEFEATED(o)) {
        if (sp) {                                                                /* 9400: the spikes break (sound 13, CAM.Z 4) */
            snd_play_orig(13, 220, 128); shake(4);
            for (int k = 0; k < 4; k++) {
                obj_t *b = spawn(OT_SB_SPIKEBIT, "sbr_bit", sp->x + (k & 1 ? 28 : -28), sp->y + (k & 2 ? 16 : -16));
                if (b) { b->vx = (k & 1 ? 1.f : -1.f) * (2 + R(3)); b->vy = -4.f + R(3); b->ang = (float)(90 * k); }
            }
            sp->alive = 0;
        }
        set_a(o, "sbr_hub");
        if (wreck_lands(o)) { crash(o, 1); o->alive = 0; }                      /* 9403-9406 (INFERRED: the hub's crash as the others) */
        return;
    }
    if (*H == 0 && fabs(PX(p) - floorf(o->x)) <= 99 && !dead(p)) {              /* 9391 */
        *H = 1; *Gv = -8; *E = floorf(o->y) - 8; *J = floorf(o->y) - 64; *D = floorf(o->x); snd_play_orig(7, 220, 128);
        *K = PX(p) < o->x ? -1 : 1; o->bd = 5;                                   /* 9393 / 9394, 9399 (contact 5, INFERRED via 2731) */
    }
    if (*H == 1) {                                                               /* 9392 */
        *I = nrm(*I + 16); o->y = (float)(int)*E; *E += *Gv; *Gv += 0.3; o->x = (float)(int)*D; *D += *F;
        *K = PX(p) < o->x ? -1 : 1;
        if (*Gv > 0 && *E >= *J) { o->y = (float)*J; *H = 2; snd_play_orig(54, 220, 128); shake(3); }   /* 9395 */
    } else if (*H == 2 && in_view(o, 300)) {                                     /* 9396-9398 */
        *I = nrm(360 + *I - 16 * *K); o->x = (float)(int)*D; *D += *F;
        *F += 0.1 * *K; if (*F > 10) *F = 10; if (*F < -10) *F = -10;
        o->y = (float)(int)(*J + R(3));                                          /* INFERRED: the 0-2 px wobble (RV y 920-922) */
        o->tm += (int)((fabs(*F) + 3) * 100);
        if (o->tm > 5000) { snd_play_orig(264, 220, 128); o->tm = 0; }
        if (G.frame % 3 == 0) effect(o->x, o->y + 24, "sbr_spark", 1.f);
        if (limiter_hit(o) && *F * *K > 0) {                                     /* the limiter bounce (RV: sound 51) */
            *H = 1; *F = -0.3 * *F; *Gv = -4; *E = floorf(o->y); snd_play_orig(51, 220, 128);
        }
    }
    o->layer = *H == 0 ? 2 : 3;                                                  /* 9390: behind the tube while inside (move_back) */
    o->ang = (float)*I;
    if (sp) { sp->x = o->x; sp->y = o->y; sp->ang = o->ang; sp->layer = o->layer; sp->hidden = *H == 0; }
}

/* ================================================================ EnemyDrone_2138 (9359-9375) + EnemyDroneLaser_2232
 * A HP 5, av: [0] D state (0 in the tube, 1 rising, 2 flying), [1] E x speed, [2] F y speed, [3] G float x, [4] H float y,
 * [5] I direction (ctor 1), [6] K, [7] L home. RE: out of the tube when the player is 0..199 px to the right and above it
 * (F -5, +0.1 a frame; propeller speed |F| x 10), then sound 127 and the flight at E = 4 I accelerating by 0.1 to 8, a laser
 * spark left every 3 frames; back home off the view (300). No RV yet (drone_idle: the player was below, no trigger, RV). */
static void drone_spawn(obj_t *o) { o->A = 5; o->fx = 0; o->av[5] = 1; o->av[6] = o->x; o->av[7] = o->y; o->layer = 2; set_a(o, "sbd_fly"); }
static void drone_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gx = &o->av[3], *H = &o->av[4], *I = &o->av[5];
    if (DEFEATED(o)) { set_a(o, "sbd_dead"); if (wreck_lands(o)) { crash(o, 1); o->alive = 0; } return; }   /* 9372-9375 */
    if (*D == 0) {
        int X = (int)floorf(o->x), P = (int)PX(p);
        if (X < P && P - 199 <= X && floorf(o->y) > PY(p) && !dead(p)) { *Gx = X; *F = -5; *H = floorf(o->y); *D = 1; o->layer = 3; }   /* 9360 */
    }
    if (o->fx > 0) anim_tick(&o->an, (int)o->fx);                               /* the forced propeller speed (fx) */
    if (*D == 1 || *D == 2) {                                                    /* 9361 */
        *Gx += *E; *H += *F; o->x = (float)(int)*Gx; o->y = (float)(int)*H;
    }
    if (*D == 1) {
        o->fx = *F < -1 ? (float)(fabs(*F) * 10) : 50;                           /* 9362 / 9364 force_speed (the table speed is 0) */
        *F += 0.1;                                                               /* 9363 */
        if (*F > -1 && o->an.frame == 0) {                                       /* 9365 */
            set_a(o, "sbd_dash"); snd_play_orig(127, 220, 128); o->fx = 0; *D = 2; *F = 0; *E = 4 * *I;
        }
    }
    if (*D == 2) {
        if (G.frame % 3 == 0) { obj_t *l = spawn(OT_SB_DLASER, "sbd_laser", o->x, o->y); if (l) l->layer = 3; }   /* 9366 */
        if (*I == 1) *E = fmin(*E + 0.1, 8.); else *E = fmax(*E - 0.1, -8.);  /* 9369 / 9370 */
        if (!in_view(o, 300)) { *D = 0; *E = 0; *F = 0; o->x = (float)o->av[6]; o->y = (float)o->av[7]; o->layer = 2; set_a(o, "sbd_fly"); }   /* 9371 */
    }
}
static void dlaser_frame(obj_t *o, player_t *p) {                                /* 9367 (removed at the end of anim 0), 9368 */
    if (o->an.done) { o->alive = 0; return; }
    if (csens_hit(p, o) && p->shield != 3) r_hurt(p, 5);
}

/* ================================================================ EnemyAxleTurret_2139 (9376-9388) + AxleTurret2_2140 + EnemyBullet_2097
 * A HP 20; av: [0] D aim angle, [1] E state (0 fire, 1 turn), [2] F counter, [3] G cooldown. RV axle_idle: in view F counts up;
 * anim 10 from F 2, three bullets at D, D + 45, D - 45 (30 px out, speed 5, sound 28) at its frame 8; at F > 70 the turn: D -10
 * and F +10 a frame to 270 (27 frames), then G 9 and the next volley; 96-frame cycle. Bullets: 9389 / test_collisions_36. */
static void axle_spawn(obj_t *o) { o->A = 20; set_a(o, "sba_idle"); }
static void bullet_new(float x, float y, double deg) {
    obj_t *b = spawn(OT_SB_BULLET, "sba_bullet", (float)(x + 30 * c_cos(deg)), (float)(y - 30 * c_sin(deg)));
    if (b) { b->av[0] = deg; b->av[1] = 5; b->fx = b->x; b->fy = b->y; b->ang = (float)deg; }
}
static void axle_frame(obj_t *o, player_t *p) {
    (void)p;
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gc = &o->av[3];
    if (DEFEATED(o)) { set_a(o, "sba_dead"); if (wreck_lands(o)) { crash(o, 1); o->alive = 0; } return; }
    if (*E == 0 && in_view(o, 100)) *F += 1;                                     /* 9377 */
    if (*E == 0 && *F == 1) { *F += 1; o->fl &= ~FL2; restart_a(o, "sba_fire"); }   /* 9378 */
    if (a_playing(o, "sba_fire") && o->an.frame >= 8 && !(o->fl & FL2)) {        /* 9383-9385 */
        bullet_new(o->x, o->y, *D); bullet_new(o->x, o->y, *D + 45); bullet_new(o->x, o->y, *D - 45);
        o->fl |= FL2; snd_play_orig(28, 220, 128);
    }
    if (a_is(o, "sba_fire") && o->an.done) set_a(o, "sba_idle");
    if (*E == 0 && *F > 70) { *E = 1; *F = 0; }                                  /* 9379 */
    if (*E == 1) { *D = nrm(*D - 10); *F += 10; }                                /* 9380 */
    if (*E == 1 && *F >= 270) { *F = 0; *E = 0; *Gc = 9; }                       /* 9381 */
    if (*Gc > 0) *Gc -= 1;                                                       /* 9382 */
}
static void bullet_frame(obj_t *o, player_t *p) {                                /* 9389: D angle, C speed; R-hurt 5 (INFERRED as the pellet) */
    o->fx += (float)(o->av[1] * c_cos(o->av[0])); o->fy -= (float)(o->av[1] * c_sin(o->av[0]));
    o->x = floorf(o->fx); o->y = floorf(o->fy);
    if (csens_hit(p, o) && r_hurt(p, 5)) { o->alive = 0; obj_effect(o->x, o->y, "explosion02"); snd_play_orig(17, 220, 128); return; }
    if (obj_overlaps_bg(o)) { o->alive = 0; obj_effect(o->x, o->y, "explosion02"); snd_play_orig(17, 220, 128); return; }   /* col_20_20 */
    if (!in_view(o, 300)) o->alive = 0;
}

/* ================================================================ EnemyDNACannon_2260 (9840-9856) + Base_2261 + EnemyDNALaser_2290
 * A HP 20; av: [0] E state (0 wait, 1 firing, 5 wrecked), [1] F counter, [2] G facing (ctor 1 right), [3] Q smoke counter.
 * RV dna_idle: 181-frame cycle: F counts in view, anim 11 (charge) past 50, at 80 anim 10, white burst, sound 265, 60 frames of
 * fire with a laser every 2 frames (8 px a frame along G); it turns to the player only between F 30 and 80 when the player is
 * 20+ px to that side and lower (RE 9842 / 9843: cannon.Y < player.Y). Defeat (9849-9852): anim 8 in place (no fly-away), smoke
 * every 7 frames, then keystone + explosion + sound 38 and hidden (INFERRED: after 60 smoke frames). */
static void dna_spawn(obj_t *o) { o->A = 20; o->av[1] = 20; o->av[2] = 1; o->fl |= F_NOFLY; set_a(o, "sbn_idle"); }
static void dna_frame(obj_t *o, player_t *p) {
    double *E = &o->av[0], *F = &o->av[1], *Gd = &o->av[2], *Q = &o->av[3];
    if (DEFEATED(o)) {
        if (*E != 5 && *E != 6) { *E = 5; set_a(o, "sbn_dead"); }
        if (*E == 5) {
            if (G.frame % 7 == 0) effect(o->x - 16 + R(33), o->y - 16 + R(33), "explosion06", 1.f);   /* 9850 */
            if (++*Q > 60) { keystone_spawn(o->x, o->y, 0, 0); obj_effect(o->x, o->y, "explosion"); snd_play_orig(38, 220, 128); *E = 6; o->hidden = 1; }   /* 9851 / 9852 */
        }
        return;
    }
    int iv = in_view(o, 200);
    if (iv && *E == 0) *F += 1;                                                  /* 9841 */
    if (iv && *E == 0 && *F > 30 && *F < 80) {                                   /* 9842 / 9843 */
        if (PX(p) + 20 < floorf(o->x) && floorf(o->y) < PY(p)) { *Gd = -1; face(o, 16); }
        else if (PX(p) - 20 > floorf(o->x) && floorf(o->y) < PY(p)) { *Gd = 1; face(o, 0); }
    }
    if (*E == 0 && *F > 50) set_a(o, "sbn_charge");                              /* 9844 */
    if (*E == 0 && *F > 80) {                                                    /* 9845 */
        set_a(o, "sbn_fire"); *E = 1; *F = 0; float ax, ay; act_pt(o, &ax, &ay); effect(ax, ay, "wburst", 1.f); snd_play_orig(265, 220, 128);
    }
    if (*E == 1) {
        *F += 1;                                                                 /* 9846 */
        if (G.frame % 2 == 0) {                                                  /* 9847 foreach_dna */
            float ax, ay; act_pt(o, &ax, &ay);
            obj_t *l = spawn(OT_SB_DNALASER, "sbn_laser", ax, ay);
            if (l) { l->av[0] = 8 * *Gd; l->A = 8; l->layer = o->layer; }
        }
        if (*F > 60) { set_a(o, "sbn_idle"); *E = 0; *F = 0; }                   /* 9848 */
    }
    obj_t *b; EACH(OT_SB_DNABASE, b) if (fabsf(b->x - o->x) < 4 && fabsf(b->y - o->y) < 4) face(b, o->dir);   /* 9853 */
}
static void dnalaser_frame(obj_t *o, player_t *p) {                              /* 9854 + test_collisions_37 (R-hurt 5 INFERRED; walls end it) */
    o->x = (float)(int)(floorf(o->x) + o->av[0]);
    if (csens_hit(p, o)) r_hurt(p, 5);
    if (obj_overlaps_bg(o) || !in_view(o, 400)) o->alive = 0;
}

/* ================================================================ EnemyPelletizer_2303 (10066-10074)
 * A HP 5; av: [0] D counter, [1] F home Y. RE: in view (100) Y = F + 20 sin(frame % 360) (INFERRED phase: the port's frame
 * counter), D + 1 a frame; D 10: two EnemyPellet_934 at (X -+ 17, Y + 13) in directions 20 / 28 with dust and sound 438 (pose 1);
 * D 60 pose 2, D 80 pose 0, D > 100 D = 0. RV pel_idle: a pair every 100 frames, 20 px above the placement at the start. */
static void pelletizer_spawn(obj_t *o) { o->A = 5; o->av[1] = floorf(o->y); set_a(o, "sbp_0"); }
static void pelletizer_frame(obj_t *o, player_t *p) {
    (void)p;
    double *D = &o->av[0];
    if (DEFEATED(o)) { if (wreck_lands(o)) { crash(o, 1); o->alive = 0; } return; }   /* 10072 / 10073 */
    if (in_view(o, 100)) {                                                       /* 10066 */
        o->y = (float)(int)(o->av[1] + 20 * c_sin((double)(G.frame % 360)));
        *D += 1;
    }
    if (*D == 10) {                                                              /* 10067 / 10068 */
        obj_spawn_pellet(o->x - 17, o->y + 13, 20); effect(o->x - 17, o->y + 13, "sbp_dust", 1.f);
        obj_spawn_pellet(o->x + 17, o->y + 13, 28); effect(o->x + 17, o->y + 13, "sbp_dust", 1.f);
        snd_play_orig(438, 220, 128); set_a(o, "sbp_1");
    }
    if (*D == 60) set_a(o, "sbp_2");                                             /* 10069 */
    if (*D == 80) set_a(o, "sbp_0");                                             /* 10070 */
    if (*D > 100) *D = 0;                                                        /* 10071 */
}

/* ================================================================ EnemyMagnetizer_2294 (10028-10065) + Crate / Hand / Line
 * Placed once (SB4) above the frame top (5976, -293). RE 10028: while its box is in the view grown by 100 and D = 0 it flies right
 * 3 F px a frame (ctor F 1) and counts I; the later states (dive, magnet, crate drop) are not ported. RV magn_idle: with the player at
 * (5976, 450) nothing moved for 900 frames (the view never reaches it). The crate / hand / line follow it (INFERRED offsets from the
 * placement: hand / magnet same point, line + 12, crate + 117). */
static void magnet_spawn(obj_t *o) { o->A = 15; o->av[0] = 1; set_a(o, "sbg_body"); }
static void magnet_frame(obj_t *o, player_t *p) {
    (void)p;
    if (DEFEATED(o)) { if (wreck_lands(o)) { crash(o, 1); o->alive = 0; } return; }
    if (in_view(o, 100) && o->av[3] == 0) { o->x += (float)(3 * o->av[0]); o->av[1] += 1; }
    obj_t *c;
    EACH(OT_SB_MCRATE, c) if (c->par == (int)(o - G.o)) { c->x = o->x; c->y = o->y + 117; }
    EACH(OT_SB_MHAND, c) if (c->par == (int)(o - G.o)) { c->x = o->x; c->y = o->y; }
    EACH(OT_SB_MLINE, c) if (c->par == (int)(o - G.o)) { c->x = o->x; c->y = o->y + 12; }
}

/* ================================================================ spawn / passes */
static void spikebit_frame(obj_t *o) {                                           /* 9401 / 9402: C 0.3 gravity, removed below the view */
    o->x += o->vx; o->vy += 0.3f; o->y += o->vy; o->ang += 12;
    if (o->y > G.camy + SCREEN_H + 64) o->alive = 0;
}
void sb_enemy_spawn(void) {
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS);
    for (int t = OT_SBE_FIRST; t <= OT_SBE_LAST && !S.on; t++) S.on = objects_have(t);
    if (!S.on) return;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_LIMITER && S.nlim < 64 && o->x >= 0) { S.lim_x[S.nlim] = (short)o->x; S.lim_y[S.nlim++] = (short)o->y; continue; }
        if (o->type < OT_SBE_FIRST || o->type > OT_SBE_LAST) continue;
        if (o->x < 0 || o->y < -400) { o->alive = 0; continue; }                 /* off-map templates (the lasers) */
        memset(o->av, 0, sizeof o->av); o->st = o->tm = 0; o->fl = 0; o->dir = 0; o->an.b = NULL; o->par = -1; o->layer = 3;
        lst_add(o);
        switch (o->type) {
        case OT_SB_HOPPER: hopper_spawn(o); break;
        case OT_SB_MAGMEON: magmeon_spawn(o); break;
        case OT_SB_ROTOR: rotor_spawn(o); break;
        case OT_SB_ROTORSPK: set_a(o, "sbr_spikes"); o->layer = 2; o->hidden = 1; break;
        case OT_SB_TUBE: set_a(o, "sbr_tube"); o->layer = 3; break;
        case OT_SB_DRONE: drone_spawn(o); break;
        case OT_SB_AXLE: axle_spawn(o); break;
        case OT_SB_AXLE2: set_a(o, "sba_base"); o->layer = 2; break;
        case OT_SB_DNA: dna_spawn(o); break;
        case OT_SB_DNABASE: set_a(o, "sbn_base"); o->layer = 2; break;
        case OT_SB_PELLETIZER: pelletizer_spawn(o); break;
        case OT_SB_MAGNET: magnet_spawn(o); break;
        case OT_SB_MCRATE: set_a(o, "sbg_crate"); break;
        case OT_SB_MHAND: set_a(o, "sbg_hand"); break;
        case OT_SB_MLINE: set_a(o, "sbg_line"); break;
        default: o->alive = 0; break;
        }
    }
    /* pairing by placement (the spikes / magnet parts with their owner: nearest of the type) */
    obj_t *o, *q;
    EACH(OT_SB_ROTORSPK, o) { float best = 1e9f; EACH(OT_SB_ROTOR, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->par = (int)(q - G.o); } } }
    for (int t = OT_SB_MCRATE; t <= OT_SB_MLINE; t++)
        EACH(t, o) { float best = 1e9f; EACH(OT_SB_MAGNET, q) { float d = fabsf(q->x - o->x); if (d < best) { best = d; o->par = (int)(q - G.o); } } }
}
void sb_enemy_pre(player_t *p) { (void)p; }                                     /* (the projectiles' handlers run in their own events) */
int sb_enemy_obj(obj_t *o, player_t *p) {
    if (!S.on) return 0;
    switch (o->type) {
    case OT_SB_HOPPER: hopper_frame(o, p); return 1;
    case OT_SB_FRAGBOMB: bomb_frame(o, p); return 1;
    case OT_SB_MAGMEON: magmeon_frame(o, p); return 1;
    case OT_SB_ROTOR: rotor_frame(o, p); return 1;
    case OT_SB_SPIKEBIT: spikebit_frame(o); return 1;
    case OT_SB_DRONE: drone_frame(o, p); return 1;
    case OT_SB_DLASER: dlaser_frame(o, p); return 1;
    case OT_SB_AXLE: axle_frame(o, p); return 1;
    case OT_SB_BULLET: bullet_frame(o, p); return 1;
    case OT_SB_DNA: dna_frame(o, p); return 1;
    case OT_SB_DNALASER: dnalaser_frame(o, p); return 1;
    case OT_SB_PELLETIZER: pelletizer_frame(o, p); return 1;
    case OT_SB_MAGNET: magnet_frame(o, p); return 1;
    default: return 1;                                                           /* spikes, tube, bases, magnet parts: moved by their owners */
    }
}
