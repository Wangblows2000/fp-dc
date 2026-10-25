/* Relic Maze enemies (session 14): EnemyIris_1191, EnemySlug_1190 (+ EnemyFireSprite_1189), EnemyHopper_1150 (+ the DV
 * spore), EnemyExtractorHead_1166 / Base_1162 (+ the AT rock, EnemySpark_1200), EnemyFireSpitter_1307 (+ MinibossFlame_1264,
 * the shell 1313), EnemySentinel_1474 (+ EnemySentinelSpike_1475), ObjectPot_1206 / ObjectBluePot_1620.
 * Source: build/re/spec_rm_enemies.md (static RE; its traces T1-T9 are not recorded yet). The shared hit / contact / defeat /
 * fly-away / landing rules are combat.c (2731-2767); the classes' own events run here, after them, in the original's order
 * (spec "Execution order"). Every rule is attached to the object type, never to a level.
 * BallMovement (RE BallMovement::update): speed in 1/8 px per frame along direction d (0..31, 11.25 deg), deceleration
 * accelerators[n] / 256 speed units per frame, the fraction carried (av[5], av[6]). Fields per class are listed below. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEFEATED(o) ((o)->fl & 0x40)
static int R(int n) { return cross_rand(n); }                                  /* Random(n): one LCG step */
static double PX(const player_t *p) { return floor(p->x); }                    /* PLAYER = Player_326 at (int X, int Y) */
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double c_sin(double deg) { double r = fmod(deg, 360.); return (r == 0. || r == 180.) ? 0. : fd_sin(deg / 57.29577951308232); }
static double c_cos(double deg) { double r = fmod(deg, 360.); return (r == 90. || r == 270.) ? 0. : fd_cos(deg / 57.29577951308232); }

/* ---- sprites: facing sets '<base>' (direction 0) / '<base>_l' (16); a missing _l set falls back to the base */
static int ens_of(const obj_t *o, const char *base) {
    char nm[24]; snprintf(nm, sizeof nm, "%s%s", base, o->dir == 16 ? "_l" : "");
    int a = ens_anim(nm);
    return a >= 0 ? a : ens_anim(base);
}
static void set_a(obj_t *o, const char *base) {                                 /* force_animation (counter kept when unchanged) */
    int a = ens_of(o, base);
    if (a < 0) return;
    if (o->an.b != &G.ens || o->an.anim != a) { anim_set(&o->an, &G.ens, a); }
}
static void restart_a(obj_t *o, const char *base) { int a = ens_of(o, base); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static int a_is(const obj_t *o, const char *base) { return o->an.b == &G.ens && o->an.anim == ens_of(o, base); }
static int a_playing(const obj_t *o, const char *base) { return a_is(o, base) && !o->an.done; }
static void ibox(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = *x1 = o->x; *y0 = *y1 = o->y; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static void act_pt(const obj_t *o, float *x, float *y) {                        /* the current frame's action point (ACT1, importer) */
    *x = o->x; *y = o->y;
    if (!o->an.b || !o->an.b->act) return;
    const frame_t *f = anim_frame(&o->an);
    int fi = (int)(f - o->an.b->frames);
    *x = floorf(o->x) - f->hx + o->an.b->act[2 * fi]; *y = floorf(o->y) - f->hy + o->an.b->act[2 * fi + 1];
}
static obj_t *spawn(int type, const char *anim, float x, float y) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = 2; o->born = G.frame;
    if (anim) { int a = ens_anim(anim); if (a >= 0) anim_set(&o->an, &G.ens, a); }
    return o;
}
static int obstacle(float x, float y) { return px_solid(&G.lvl, (int)floorf(x), (int)floorf(y)); }   /* Frame::test_obstacle */
/* overlaps background, Q7 (solid) or Q8 (obstacles) */
static int overlaps_level(const obj_t *o) {
    if (obj_overlaps_bg(o)) return 1;
    float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *s = &G.o[i];
        if (!s->alive || s == o || !s->an.b) continue;
        if (!(obj_in_group(s->type, OBJGRP_SOLID) || obj_in_group(s->type, OBJGRP_OBSTACLE_ALL))) continue;
        if (fabsf(s->x - o->x) > 200 || fabsf(s->y - o->y) > 200) continue;
        if (obj_rect_hit(s, (int)x0, (int)y0, (int)x1, (int)y1)) return 1;
    }
    return 0;
}
/* overlaps with an alternative image (force_direction(mask) + collision test, then restored) */
static int mask_overlaps(obj_t *o, const char *mask) {
    int a = ens_anim(mask);
    if (a < 0) return overlaps_level(o);
    animator_t keep = o->an; anim_set(&o->an, &G.ens, a);
    int hit = overlaps_level(o);
    o->an = keep;
    return hit;
}
/* PlayerCharacterSensor_375 */
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {
    int X = (int)PX(p), Y = (int)PY(p);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return o->an.b && obj_rect_hit(o, a, b, c, d); }
/* "R-hurt n": MV.R = 1, LIFE.B += n when MV.R == 0 and OTHERS.E <= 0 (2414 applies it next frame) */
static int r_hurt(player_t *p, int n) {
    if (p->state == ST_DEAD || p->hurt_pending || player_invincible(p)) return 0;
    player_contact(p, n); return 1;
}
static int cube_on(const obj_t *o) { return player_cube_state() >= 4 && o->an.b && player_cube_hits(o); }   /* CUBE.A >= 4 */
static int is_monitor(int t) { return t == OT_MONITOR || t == OT_MON_RINGS || t == OT_MON_1UP || t == OT_MON_INVINC || t == OT_MON_KEYSTONE ||
                                      (t >= OT_MON_WOOD && t <= OT_MON_WATER) || t == OT_MON_BOMB || t == OT_MON_CRATE; }
static void break_monitors_on(const obj_t *o) {                                 /* col_14_24 / col_13_25 / col_19_27: the monitor breaks (sound 12, icon) */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *m = &G.o[i];
        if (m->alive && is_monitor(m->type) && m->an.b && fabsf(m->x - o->x) < 96 && fabsf(m->y - o->y) < 96 && obj_obj_hit(o, m)) m->fl |= 0x200;   /* the monitor code breaks it (objects.c) */
    }
}
/* BallMovement */
static void ball_move(obj_t *o) {
    double sp = o->av[4];
    if (o->av[7] > 0) { sp -= o->av[7]; if (sp < 0) sp = 0; o->av[4] = sp; }
    int d = (int)o->av[3] & 31;
    double v = sp * 0.125;
    double dx = (d == 0 ? 1 : d == 16 ? -1 : d == 8 || d == 24 ? 0 : fd_cos(d * 11.25 / 57.29577951308232)) * v + o->av[5];
    double dy = (d == 8 ? -1 : d == 24 ? 1 : d == 0 || d == 16 ? 0 : -fd_sin(d * 11.25 / 57.29577951308232)) * v + o->av[6];
    double sx = floor(dx), sy = floor(dy);
    o->av[5] = dx - sx; o->av[6] = dy - sy; o->x += (float)sx; o->y += (float)sy;
}
static void debris(float x, float y, const char *anim, int frame, int dir, double speed) {   /* EffectsDebris_427 (motion: bosses.c) */
    obj_t *d = spawn(OT_DEBRIS, anim, x, y);
    if (!d) return;
    if (d->an.b) { const anim_t *a = &d->an.b->anims[d->an.anim]; d->an.frame = frame < a->count ? frame : a->count - 1; }
    double r = dir * 11.25;
    d->vx = (float)(speed * 0.125 * c_cos(r)); d->vy = (float)(-speed * 0.125 * c_sin(r));
}

/* ================================================================ EnemyIris_1191 (spec 1)
 * st D (0 idle, 1 chase, 2 dive, 3 retract, 4 dead), tm J, av[0] I re-aims, av[1] H dive target Y, av[3] direction, av[4] speed */
static void iris_aim(obj_t *o, const player_t *p) {
    double th = atan2(o->y - (PY(p) - 50), PX(p) - o->x) * 57.29577951308232;
    if (th < 0) th += 360;
    o->av[3] = (int)(th / 11.25);
}
static void iris_frame(obj_t *o, player_t *p) {
    ball_move(o);
    double dxp = fabs(o->x - PX(p)), py = PY(p);
    int inband = o->y >= py - 199 && o->y <= py + 31;
    if (o->st == 0) {
        if (o->tm == 0) o->tm = R(50) + 1;                                      /* 5771 */
        o->tm++;                                                                /* 5772 */
        if (o->tm > 100) { restart_a(o, "ir_blink"); o->tm = 0; }               /* 5773 */
        if (dxp <= 99 && inband) { o->st = 1; o->tm = 0; iris_aim(o, p); o->av[4] = 15; }   /* 5774 */
    }
    if (o->st == 1) {
        o->tm++; set_a(o, "ir_chase");                                          /* 5775 */
        if (o->tm > 25) { o->tm = 0; iris_aim(o, p); o->av[0]++; }             /* 5776 */
        if (o->av[0] >= 3 && dxp <= 39 && inband) { o->st = 2; o->tm = 0; o->av[0] = 0; o->av[1] = py; o->av[4] = 0; }   /* 5777 */
    }
    if (o->st == 2) {
        set_a(o, "ir_dive"); o->tm++;                                           /* 5778 */
        if (o->tm > 30) { o->av[4] = 50; o->av[3] = 24; o->bd = 5; }            /* 5779: straight down, contact 5 */
        if (o->tm > 20 && o->y >= o->av[1] && overlaps_level(o)) { o->st = 3; restart_a(o, "ir_ret"); o->bd = 0; o->av[4] = 0; }   /* 5780 / 5781 (own H; FIRST INFERRED) */
    }
    if (o->st == 3 && a_is(o, "ir_ret") && o->an.done) { o->st = 1; o->tm = 0; iris_aim(o, p); o->av[4] = 15; }   /* 5782 */
    if (o->bd > 0) {                                                            /* col_14_24 / col_24_24: a diving iris breaks monitors and pots */
        break_monitors_on(o);
        for (int i = 0; i < G.nobjs; i++) { obj_t *q = &G.o[i]; if (q->alive && (q->type == OT_POT || q->type == OT_BLUEPOT) && !DEFEATED(q) && q->an.b && obj_obj_hit(o, q)) q->A = 0; }
    }
}
static void iris_dead(obj_t *o) { o->st = 4; o->av[4] = 0; o->bd = 0; set_a(o, "ir_dead"); }   /* 5783 / 5784 */

/* ================================================================ EnemySlug_1190 + EnemyFireSprite_1189 (spec 2)
 * st D (0 air, 1 landed, 2 walk, 3 spit, 4 dead), vx E, vy F, fx / fy G / H floats, tm I walk timer, av[0] J spit counter,
 * fl 0x10 on ground (flag 4) */
#define SL_GND 0x10
static void slug_frame(obj_t *o, player_t *p) {
    if (o->st == 1 && obj_near_view(o, 50)) {                                   /* 5734 -> loop_slug_24: 5610 / 5611 */
        set_a(o, "sl_walk"); o->vx = o->x < PX(p) ? 0.4f : -0.4f; o->st = 2;
    }
    if (o->st == 2 && obj_near_view(o, 50) && o->vy < 0.5f) o->tm++;
    if (o->fy == 0) { o->fy = o->y; o->fx = o->x; }                             /* 5735 */
    o->fx += o->vx; o->fy += o->vy; o->x = (float)(int)o->fx; o->y = (float)(int)o->fy; o->bd = 0;   /* 5736 */
    o->fl &= ~SL_GND;                                                           /* 5737-5741: feet mask (image 2500) */
    if (mask_overlaps(o, "sl_mask")) o->fl |= SL_GND;
    if (!(o->fl & SL_GND)) o->vy = fminf(o->vy + 0.25f, 12.f);                  /* 5742 */
    { float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);                        /* 5746 / 5747 */
      if (o->vx < 0 && obstacle(x0 - 4, y1 - 8)) o->vx = -o->vx;
      else if (o->vx > 0 && obstacle(x1 + 4, y1 - 8)) o->vx = -o->vx; }
    if (o->st == 0 && (o->fl & SL_GND) && o->vy >= 0 && !a_playing(o, "sl_walk")) { set_a(o, "sl_idle"); o->vx = 0; o->st = 1; }   /* 5751 */
    if (o->st < 4 && (o->fl & SL_GND) && o->vy >= 0) { o->vy = 0; o->fy -= 1; o->y = (float)(int)o->fy; }   /* 5752: 1 px up per frame */
    if (o->st < 4) { if (o->vx > 0) o->dir = 0; else if (o->vx < 0) o->dir = 16; }   /* 5753 / 5754 */
    if (o->st == 2 && o->tm > 30) { o->st = 3; o->tm = -90; o->av[0] = 0; o->dir = o->x < PX(p) ? 0 : 16; }   /* 5757 / 5758 */
    if (o->st == 3 && o->vx != 0) { restart_a(o, "sl_spit"); o->vx = 0; }       /* 5759 */
    if (o->st == 3 && a_playing(o, "sl_spit") && o->an.frame >= 4 && o->av[0] < 9) {   /* 5760 / 5761: one ember per frame */
        float ax, ay; act_pt(o, &ax, &ay);
        int J = (int)o->av[0];
        obj_t *s = spawn(OT_FSPRITE, "fsprite", ax, ay);
        o->av[0] = J + 2;
        if (s) { s->av[3] = o->dir == 0 ? 2 + J : 14 - J; s->av[4] = 36 - 1.5 * o->av[0]; s->av[7] = 120 / 256.0; }   /* BallMovement dec 19 */
        snd_play_orig(59, 220, 128);
    }
    if (o->st == 3 && a_is(o, "sl_spit") && o->an.done) o->st = 0;              /* 5762 */
    if (o->st == 2) set_a(o, "sl_walk");
    else if (o->st == 3 && !a_is(o, "sl_spit")) restart_a(o, "sl_spit");
}
static void fsprite_frame(obj_t *o, player_t *p) {
    if (o->born == G.frame) return;
    ball_move(o);
    if (o->av[4] == 0 && !(o->fl & 1)) {                                        /* 5766 / 5767: ignites */
        if (obj_near_view(o, 50)) snd_play_orig(60, 220, 128);
        restart_a(o, "fsprite_burn"); o->fl |= 1;
    }
    if (o->fl & 1) { if (++o->A > 64) { o->alive = 0; return; } }              /* 5768 / 5769 */
    if ((o->fl & 1) && o->an.done) { o->alive = 0; return; }                    /* 5770 */
    if ((o->fl & 1) && csens_hit(p, o) && r_hurt(p, 10)) o->av[4] = 0;          /* col_17_24: only burning */
    if (cube_on(o)) o->av[3] = (int)(o->av[3] + 16) & 31;                       /* col_11_24: bounce(1) (reversal INFERRED) */
}

/* ================================================================ EnemyHopper_1150 (spec 3)
 * st D (0 ground, 1 crouch, 2 jump, 3 dead), tm E wait, vx F side sign, vy G y speed, fx / fy H / I floats, av[0] J,
 * fl 0x08 left wall (3), 0x10 feet (4), 0x20 right wall (5), 0x1000 spores fired (12) */
static void hopper_frame(obj_t *o, player_t *p) {
    o->fl &= ~(0x08 | 0x10 | 0x20);                                             /* 5714 -> loop_hopper_24: probe masks */
    if (mask_overlaps(o, "hp_m9")) o->fl |= 0x08;
    if (mask_overlaps(o, "hp_m8")) {
        o->fl |= 0x10;
        int n = o->vy > 1 ? (int)o->vy : 1;                                     /* loop_hopper_floor_24: max(1, int G) passes */
        for (int k = 0; k < n && mask_overlaps(o, "hp_m8"); k++) { o->y -= 1; o->fy -= 1; }
    }
    if (mask_overlaps(o, "hp_m7")) o->fl |= 0x20;
    if (o->st == 0) {
        if (!mask_overlaps(o, "hp_m10")) { o->y = o->fy; o->vy = fminf(o->vy + 0.25f, 12.f); o->fy += o->vy; }   /* 5715-5717: falling */
        if (!a_playing(o, "hp_land")) set_a(o, "hp_idle");                     /* 5718 */
        if (obj_near_view(o, 200) && !a_playing(o, "hp_land") && a_is(o, "hp_idle") && o->an.frame == 3 && !(o->fl & 0x1000)) {   /* 5719 */
            o->fl |= 0x1000; o->av[0] = 2;
            for (int k = 0; k < 2; k++) if (o->av[0] > 0) {                      /* loop_hopper_spore_24: one spore each way, 3 px/frame */
                float ax, ay; act_pt(o, &ax, &ay);
                double J = o->av[0], c = c_cos(180 * J);
                obj_t *s = spawn(OT_SPORE, "spore", (float)(o->x + 10 * c), ay);
                if (s) { s->fx = s->x; s->fy = s->y; s->vx = (float)(3 * c); s->vy = 0; }
                o->av[0] = J - 1;
            }
        }
        if (obj_near_view(o, 100)) o->tm++;                                     /* 5720 */
        if (o->tm > 0) { o->st = 1; o->fx = o->x; o->fy = o->y; o->dir = PX(p) < o->x ? 16 : 0; o->fl &= ~0x1000; restart_a(o, "hp_crouch"); }   /* 5721 / 5722 */
    }
    if (o->st == 1) {
        set_a(o, "hp_crouch");                                                  /* 5723 */
        if (o->an.frame >= 9) { o->st = 2; o->vy = -7; o->vx = o->dir == 0 ? 1 : -1; }   /* 5724 */
    }
    if (o->st == 2) {
        set_a(o, "hp_air"); o->x = o->fx; o->y = o->fy; o->vy = fminf(o->vy + 0.25f, 12.f); o->fy += o->vy;   /* 5725 */
        if ((o->vx < 0 && !(o->fl & 0x08)) || (o->vx > 0 && !(o->fl & 0x20))) o->fx += o->vx;   /* 5726 */
        { float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1); if (o->vy < 0 && obstacle(o->x, y0 + 8)) o->vy = 0; }   /* 5727 */
        if (o->vy >= 0 && (o->fl & 0x10)) { restart_a(o, "hp_land"); o->st = 0; o->tm = -80; snd_play_orig(54, 220, 128); }   /* 5728 */
    }
}
static void hopper_dead(obj_t *o) {
    if (o->st != 3) {                                                           /* 5729 / 5730: two debris (directions INFERRED: up-left / up-right) */
        debris(o->x, o->y, "debris_hop", 1, 10, 50 + R(30)); debris(o->x, o->y, "debris_hop", 0, 6, 50 + R(30));
        o->st = 3;
    }
    set_a(o, "hp_dead");                                                        /* 5731 */
}

/* ================================================================ EnemyExtractorHead_1166 / Base_1162, EnemySpark_1200 (spec 4)
 * head: st D (0 idle, 1 drill, 2 retract, 3 aim / shoot, 6 dead), tm E; base: par = the head's index */
static void exhead_frame(obj_t *o, player_t *p) {
    o->fl |= F_NOFLY;                                                           /* 5787 */
    if (DEFEATED(o)) {
        if (o->st != 6) {                                                       /* 5802 */
            o->st = 6; snd_play_orig(29, 220, 128); keystone_spawn(o->x, o->y + 14, -3, 0); o->hidden = 1; o->tm = 0;
            obj_effect(o->x, o->y, "explosion06");
        }
        if ((G.frame & 1) == 0 && o->tm < 4) { o->tm++; obj_effect((float)(o->x - 5 + R(11)), (float)(o->y + 14 - 8 + R(17)), "explosion06"); }   /* 5803 */
        return;
    }
    int nv = obj_near_view(o, 150);
    if (nv && o->st == 0) { set_a(o, "ex_idle"); o->tm++; }                     /* 5788 / 5789 */
    if (nv && o->st == 0 && o->tm > 50 && o->an.frame == 0) { o->tm = 0; o->st = 1; }   /* 5790 */
    if (o->st == 1) {
        set_a(o, "ex_drill");                                                   /* 5791 */
        if (o->an.frame == 17) {
            if (nv) { float n = G.cam.Z + 2; G.cam.Z = n < 2 ? n : 2; }        /* 5792: shake */
            o->tm++; float ax, ay; act_pt(o, &ax, &ay); rock_at_new(ax, ay); snd_play_orig(35, 220, 128);   /* 5793 / 6175 */
        }
        if (o->tm > 40) { o->st = 2; o->tm = -50; restart_a(o, "ex_ret"); }     /* 5794 */
    }
    if (o->st == 2 && a_is(o, "ex_ret") && o->an.done) { o->st = 3; set_a(o, "ex_idle"); }   /* 5795 */
    if (o->st == 3) {
        if ((nv && o->tm < 8) || (o->tm > 8 && o->tm < 60)) o->tm++;            /* 5796 */
        if (o->tm == 8) { o->dir = PX(p) <= o->x ? 16 : 0; restart_a(o, "ex_aim"); o->tm++; }   /* 5797-5799 */
        if (o->tm == 40) {                                                      /* 5800: the spark */
            restart_a(o, "ex_shoot"); o->tm++;
            float ax, ay; act_pt(o, &ax, &ay);
            obj_t *s = spawn(OT_SPARK, o->dir == 16 ? "spark_l" : "spark", ax, ay);
            if (s) { s->dir = o->dir; s->av[0] = ax; s->av[1] = ay; s->av[2] = 0; s->av[3] = 0; }
            obj_effect(ax, ay, "wburst"); snd_play_orig(67, 220, 128);
        }
        if (o->tm > 40 && a_is(o, "ex_shoot") && o->an.done) { o->st = 0; o->dir = 0; o->tm = 15; }   /* 5801 */
    }
}
static void exbase_frame(obj_t *b) {                                            /* 5786 / 6174: mirrors its head */
    if (b->par < 0 || b->par >= G.nobjs) return;
    const obj_t *h = &G.o[b->par];
    if (!h->alive || h->type != OT_EXHEAD) return;
    b->x = h->x; b->y = h->y; b->dir = h->dir;
    if (h->an.b == &G.ens) {
        char nm[24]; const char *hn = h->an.b->anims[h->an.anim].name;
        snprintf(nm, sizeof nm, "eb%s", hn + 2);                                 /* ex_* -> eb_* */
        int a = ens_anim(nm);
        if (a >= 0) { if (b->an.anim != a || b->an.b != &G.ens) anim_set(&b->an, &G.ens, a); b->an.frame = h->an.frame < b->an.b->anims[a].count ? h->an.frame : 0; }
    }
}
static void spark_frame(obj_t *o, player_t *p) {
    o->x = (float)(int)o->av[0]; o->y = (float)(int)o->av[1]; o->av[0] += o->av[2]; o->av[1] += o->av[3];   /* 5804 */
    if (o->av[2] == 0) { o->av[2] = o->dir == 16 ? -2 : 2; o->av[3] = -4; }    /* 5805 / 5806 */
    if (o->dir != 24) {
        o->av[3] = fmin(o->av[3] + 0.4, 12);                                    /* 5807 */
        if (obstacle(o->x, o->y + 4)) { o->dir = 24; set_a(o, "spark_run"); o->av[2] *= 3; o->av[3] = 0; }   /* 5808 */
    } else {
        if (G.frame % 4 == 0) obj_effect(o->x, o->y, "wburst");                 /* 5809 */
        if (obstacle(o->x, o->y + 4)) { for (int k = 0; k < 8 && obstacle(o->x, (float)o->av[1] + 4); k++) o->av[1] -= 1; }   /* 5810 (U2: per-pass test INFERRED) */
        else { for (int k = 0; k < 8 && !obstacle(o->x, (float)o->av[1] + 4); k++) o->av[1] += 1; }   /* 5811 */
    }
    if (!obj_near_view(o, 100)) { o->alive = 0; return; }                       /* 5812 */
    if (csens_hit(p, o)) r_hurt(p, 10);                                         /* col_25_24 */
}

/* ================================================================ EnemyFireSpitter_1307, MinibossFlame_1264, shell (spec 5)
 * av[0] D walk sign, tm E fire timer, fl 0x04 shell made (flag 2) */
obj_t *relic_flame_new(float x, float y, double C, double D) {                 /* MinibossFlame_1264: A, B = x, y; C, D speed */
    obj_t *f = spawn(OT_FLAME, "flame", x, y);
    if (!f) return NULL;
    f->av[0] = x; f->av[1] = y; f->av[2] = C; f->av[3] = D;
    return f;
}
static void flame_new(float x, float y, double C, double E) {
    obj_t *f = relic_flame_new(x, y, C, 0);
    if (!f) return;
    f->av[4] = E;
    f->ang = (float)(90 * R(4));                                                /* the spitter's: angle Random-4 x 90 (the golem's stays 0) */
}
static void fspitter_frame(obj_t *o, player_t *p) {
    (void)p;
    if (DEFEATED(o)) {
        if (!(o->fl & 0x04)) {                                                  /* 6189: the shell stays, the body flies */
            obj_t *s = spawn(OT_FSSHELL, o->dir == 16 ? "fs_shell_l" : "fs_shell", o->x, o->y);
            if (s) s->layer = o->layer;
            o->tm = 0; o->fl &= ~F_NOFLY; o->fl |= 0x04;
            debris(o->x, o->y, "debris_hop", 0, 8, 50 + R(30));                 /* 6190 (frame 16) */
        }
        set_a(o, "fs_dead"); o->ang = 0;
        return;
    }
    int turning = a_playing(o, "fs_turn");
    if (!turning) o->x += (float)(2 * o->av[0]);                                /* 6176 */
    o->fl |= F_NOFLY;
    if (obj_near_view(o, 80)) {                                                 /* 6177 / 6178: sticks to the floor */
        o->y += 1;
        float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);
        if (obstacle(o->x, y1) || overlaps_level(o)) o->y -= 1;
    }
    for (int i = 0; i < G.nobjs; i++) {                                         /* col_18_26: limiters */
        const obj_t *l = &G.o[i];
        if (!l->alive || l->type != OT_LIMITER || !l->an.b || !obj_obj_hit(o, l)) continue;
        if ((o->av[0] > 0 && o->x < l->x) || (o->av[0] < 0 && l->x < o->x)) o->av[0] = -o->av[0];
    }
    if (o->av[0] > 0 && o->dir != 0) { o->dir = 0; restart_a(o, "fs_turn"); }  /* 6180 / 6181 */
    else if (o->av[0] < 0 && o->dir != 16) { o->dir = 16; restart_a(o, "fs_turn"); }
    turning = a_playing(o, "fs_turn");
    if ((obj_near_view(o, 60) && o->tm < 50) || o->tm >= 30) o->tm++;          /* 6182 */
    if (o->tm > 0 && o->tm < 50 && a_is(o, "fs_walk")) set_a(o, "fs_open");    /* 6183 */
    if (o->tm >= 50 && a_is(o, "fs_open")) set_a(o, "fs_walk");                 /* 6184 */
    if (o->tm >= 50 && o->tm <= 51) { snd_play_orig(70, 220, 128); o->tm = 52; }   /* 6185 */
    if (o->tm >= 50 && o->tm % 3 == 0) {                                        /* 6186 / 6187 */
        float ax, ay; act_pt(o, &ax, &ay);
        flame_new(ax, ay, turning ? -(3 - 0.6 * o->an.frame) * o->av[0] : 5 * o->av[0], 0);
    }
    if (o->tm > 210) o->tm = -90;                                               /* 6188 */
    if (!turning && !a_is(o, "fs_open") && !a_is(o, "fs_walk")) set_a(o, "fs_walk");
}
static void flame_frame(obj_t *o, player_t *p) {
    o->x = (float)(int)o->av[0]; o->y = (float)(int)o->av[1]; o->av[0] += o->av[2]; o->av[1] += o->av[3];   /* 5983 / 6351 */
    if (o->an.done) { o->alive = 0; return; }                                   /* object flag 0x8 (U4: destroy at the end, INFERRED) */
    if (!(o->fl & 1) && o->an.frame <= 7 && p->shield != 2 && csens_hit(p, o) && r_hurt(p, 5)) { p->tint = 30; p->tint_argb = 0xffffa080; }   /* 5984 / 6352 / 7111: burn tint, blend 0x80a0ff (BGR) */
    if (o->an.frame < 8) break_monitors_on(o);                                  /* col_13_25 / col_15_26 */
}

/* ================================================================ EnemySentinel_1474 + spikes (spec 6)
 * fx / fy E, F home, av[0] G orbit phase, tm H attack timer, av[3] aimed direction */
static void sentinel_frame(obj_t *o, player_t *p) {
    if (DEFEATED(o)) return;
    int opening = a_is(o, "sn_open0") || a_is(o, "sn_open8") || a_is(o, "sn_open16") || a_is(o, "sn_open24");
    if (o->tm < 40 && !opening) {                                               /* 6449: figure eight around home */
        o->x = (float)(o->fx + 20 * c_sin(2 * o->av[0])); o->y = (float)(o->fy + 20 * c_cos(o->av[0]));
        o->av[0] = fmod(o->av[0] + 0.5, 360);
    }
    if ((obj_near_view(o, 60) && o->tm >= 0 && o->tm < 5) || o->tm > 5 || o->tm < 0) o->tm++;   /* 6450 */
    if (o->tm == 5) {
        double th = atan2(o->y - PY(p), PX(p) - o->x) * 57.29577951308232; if (th < 0) th += 360;   /* 6451 */
        int d = ((int)(th / 11.25) + 2) & ~7 & 31; o->av[3] = d;
        if (a_is(o, "sn_eye") && d == 4 * o->an.frame) {                        /* 6452 */
            char nm[12]; snprintf(nm, sizeof nm, "sn_open%d", d); set_a(o, nm); o->av[1] = o->an.frame; o->tm++;
        }
    }
    if (o->tm == 40) {                                                          /* 6453 / 6454 */
        for (int k = -1; k <= 1; k += 2) {
            obj_t *s = spawn(OT_SSPIKE, "sn_spike", o->x, o->y);
            if (s) { s->av[3] = ((int)o->av[3] + 4 * k) & 31; s->av[4] = 70; s->av[7] = 3; s->ang = (float)(s->av[3] * 11.25); }
        }
        set_a(o, "sn_eye"); snd_play_orig(77, 220, 128);
    }
    if (o->tm > 100) o->tm = -80;                                               /* 6455 */
}
static void sspike_frame(obj_t *o, player_t *p) {
    if (o->born == G.frame) return;
    ball_move(o);
    if (o->av[4] == 0 && !(o->fl & 2)) { o->fl |= 2; o->av[7] = 0; o->av[3] = ((int)o->av[3] + 16) & 31; o->ang = (float)(o->av[3] * 11.25); }   /* 6456 */
    if (o->fl & 2) o->av[4] = fmin(fmax(o->av[4] + 2, 2), 70);                  /* 6457: flies back, accelerating */
    if (!obj_near_view(o, 100)) { o->alive = 0; return; }                       /* 6458 */
    if (csens_hit(p, o) && p->shield != 5) r_hurt(p, 5);                        /* col_22_27: earth shield immune */
    if (p->shield == 5 && !(o->fl & 8) && csens_hit(p, o)) {                    /* col_10_27: the earth shield reflects (shield box ~ sensor, INFERRED) */
        o->fl |= 8 | 2; o->av[4] = 70; o->av[7] = 0; o->av[3] = ((int)o->av[3] + 16) & 31; o->ang = (float)(o->av[3] * 11.25); snd_play_orig(101, 220, 128);
    }
    if (!(o->fl & 4)) { for (int i = 0; i < G.nobjs; i++) { obj_t *m = &G.o[i]; if (m->alive && is_monitor(m->type) && m->an.b && obj_obj_hit(o, m)) { m->fl |= 0x200; o->fl |= 4; obj_effect(o->x, o->y, "explosion02"); break; } } }   /* col_19_27 */
}

/* ================================================================ ObjectPot_1206 / ObjectBluePot_1620 (spec 7): st D shard count */
static void pot_frame(obj_t *o) {
    if (!DEFEATED(o)) return;
    if (o->st < 5) {                                                            /* 5711 / 6586 */
        snd_play_orig(213, 220, 128);
        obj_launch_crystal(o->x, o->y, 8);                                      /* the crystal, launched up (direction 8) */
        while (o->st < 5) { debris(o->x, o->y, "debris_pot", o->st % 2, 2 * o->st + 4, 40 + R(30)); o->st++; }   /* loop_pot: 5 shards */
    }
    set_a(o, "pot_broken");                                                     /* 5712 / 6587 */
}

/* ================================================================ hooks */
int relic_keeps_state(int t) { return (t >= OT_IRIS && t <= OT_BLUEPOT) || relic_boss_keeps(t); }
void relic_spawn(void) {
    int nhead = 0, heads[16];
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_IRIS || o->type > OT_BLUEPOT) continue;
        if (o->x < 0 || o->x >= G.lvl.w || o->y < 0 || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        o->st = 0; o->tm = 0; o->bd = 0; o->dir = 0; memset(o->av, 0, sizeof o->av); o->fx = o->fy = 0; o->vx = o->vy = 0;
        switch (o->type) {
        case OT_IRIS: o->A = 1; set_a(o, "ir_idle"); o->av[3] = (cross_rand_raw(0x10000) >> 10) & 31; break;   /* ctor: random direction, stopped */
        case OT_SLUG: o->A = 15; set_a(o, "sl_idle"); break;
        case OT_HOPPER: o->A = 15; o->dir = 16; set_a(o, "hp_idle"); o->fx = o->x; o->fy = o->y; break;   /* ctor: dir 14 (left set) */
        case OT_FSPITTER: o->A = 10; o->av[0] = 1; set_a(o, "fs_walk"); break;
        case OT_SENTINEL: o->A = 10; o->fx = o->x; o->fy = o->y; set_a(o, "sn_eye"); break;   /* home E, F (RM5 6525 U6: INFERRED the same) */
        case OT_EXHEAD: o->A = 25; o->absorbed = 10; set_a(o, "ex_idle"); if (nhead < 16) heads[nhead++] = i; break;   /* S = 10 */
        case OT_EXBASE: o->A = 40; set_a(o, "eb_idle"); o->par = -1; break;
        case OT_POT: case OT_BLUEPOT: o->A = 1; o->fl |= F_NOFLY; set_a(o, "pot"); break;   /* act start: flag 16 (the broken pot stays) */
        }
    }
    for (int i = 0, k = 0; i < G.nobjs; i++) {                                  /* 5786: base k <-> head k (index pairing) */
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_EXBASE) continue;
        if (k < nhead) b->par = heads[k++];
    }
}
void relic_frame(player_t *p) {
    relic_gima_post(p);                                                         /* acts 1-2 gimmicks (relic_gima.c) */
    rmboss_frame(p);                                                            /* golems (relic_boss.c), after the shared combat rules */
    rm_mantalith_frame(p);                                                      /* RM5 end: Kingdom Stone scene, Mantalith, post-results (rm_mantalith.c) */
    if (!objects_have_from(OT_IRIS) && !objects_have(OT_ROCK_AT)) return;
    rocks_at_frame(p);                                                          /* 4230-4232 (extractor rocks) */
    for (int i = 0; i < G.nobjs; i++) {                                         /* col_3_24: Milla's shield breaks rocks */
        obj_t *r = &G.o[i];
        if (r->alive && r->type == OT_ROCK_AT && !(r->fl & 1) && r->tm > 10 && cube_on(r)) rock_at_break(r);
    }
    for (int i = 0; i < G.nobjs; i++) {                                         /* col_10_24: Milla's shield reflects spores */
        obj_t *s = &G.o[i];
        if (s->alive && s->type == OT_SPORE && !(s->fl & 8) && cube_on(s)) { s->fl |= 8; s->vx = -s->vx; s->vy = -s->vy; }
    }
}
int relic_obj_update(obj_t *o, player_t *p) {
    if (o->type >= OT_GEYE && o->type <= OT_CAMLIM_B) return rmboss_obj(o, p);
    if (o->type >= OT_MT_FIRST && o->type <= OT_MT_LAST) return rm_mantalith_obj(o, p);
    if (o->type >= OT_RAIL2 && o->type <= OT_LL1L02) return relic_gima_obj(o, p);
    if (o->type > OT_LL1L02) return 0;
    switch (o->type) {
    case OT_IRIS: if (DEFEATED(o)) iris_dead(o); else iris_frame(o, p); return 1;
    case OT_SLUG: if (DEFEATED(o)) { o->st = 4; set_a(o, "sl_dead"); } else slug_frame(o, p); return 1;   /* 5763 / 5764 */
    case OT_HOPPER: if (DEFEATED(o)) hopper_dead(o); else hopper_frame(o, p); return 1;
    case OT_EXHEAD: exhead_frame(o, p); return 1;
    case OT_EXBASE: exbase_frame(o); return 1;
    case OT_FSPITTER: fspitter_frame(o, p); return 1;
    case OT_SENTINEL: sentinel_frame(o, p); return 1;
    case OT_POT: case OT_BLUEPOT: pot_frame(o); return 1;
    case OT_FSPRITE: fsprite_frame(o, p); return 1;
    case OT_SPARK: spark_frame(o, p); return 1;
    case OT_FLAME: flame_frame(o, p); return 1;
    case OT_SSPIKE: sspike_frame(o, p); return 1;
    case OT_FSSHELL: return 1;
    }
    return 0;
}
