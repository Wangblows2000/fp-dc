/* Thermal Base enemies (session 21c): EnemySeeker_2603 (+ the runtime EnemyHomingBullet_2648), EnemyPushUp_2669 + Hitbox_2663
 * (+ Laser_2670 / Laser2_2695 / Laser3_2696 / Debris_2664), EnemyLavaSquid_2740 (+ Laser_2741 / Debris_2742), EnemyTankbot_2838
 * (+ Debris_2839; its shots are DV's EnemyPellet_934, objects.c), EnemySaucer_2828 + Leg1-4 + EnemySaucerLimiter_2832 (+ Debris_2831;
 * its shots are Relic Maze's GimmickFireball_1192 / EffectEmber_1025, relic_gima.c fireball_fly / fireball_rules).
 * Source: build/re/spec_tb_enemies.md. Labels: RE (events of handle_frame_45..49 = TB1-TB5: 11761-11801, 11683 (foreach_pushup_44),
 * 11969-11991, 12397-12444, 12534-12538, test_collisions_44..48 (col_5_44, col_3_44, col_8_44, col_13_44, col_14_44, col_16_44,
 * col_27_44, col_6_45, col_25_24, 11691), constructors, on_frame_45..49_init qualifier 20), RV (original traces
 * build/orig_traces/tb_enemies), INFERRED.
 * Shared code: src/game/enemy_kit.h (per-type lists, ENS rows of any direction (ek_set_ad, new), R-hurt, limiters, debris); the
 * shared Q20 hits / defeat / fly-away of combat.c (2730-2768) for every Q20 class (Lilac, Carol, Milla alike: no class sets contact
 * damage B, RE: the hurts are their shots'); the corpse landings of combat.c (OD_LANDS: the push-up's head lands like a Ganseki,
 * 11798; OD_LANDS_KS (new): seeker / tank / debris crash like a Turretus with a keystone, 11779 / 12443 / 11987).
 * Order: tb_enemy_pre (objects.c, before combat) = the fireballs (they are not this module's types); tb_enemy_obj = each object's
 * events after the animations advanced (update_objects). Flags: the original's flag 6 = 0x40 (defeated); own flags >= 0x1000. */
#include "tb.h"
#include "enemy_kit.h"

#define DEFEATED(o) ((o)->fl & 0x40)
#define FL1 0x1000                                   /* the class's flag 1 */
#define FL2 0x2000                                   /* flag 2 */
#define FLBG 0x4000                                  /* overlapped the background last frame (test_collisions' edge trigger, 0x800) */
#define FLGONE 0x8000                                /* push-up body beaten (its head flies) */
#define RT0 OT_TBE_FIRST
#define NRT (OT_TBE_LAST - OT_TBE_FIRST + 1)
static ek_list_t LST[NRT], LFB, LEMB;                /* + the saucers' fireballs and their embers */
#define EACH(T, OB) EK_EACH(&LST[(T) - RT0], T, OB)
static struct { int on, shot_frame; ek_limiters_t lim; } S;

static obj_t *spawn(int type, float x, float y, int dir) { return ek_spawn(&LST[type - RT0], type, NULL, x, y, dir); }
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }    /* !is_near_border(-n) */
static double sin0(double deg) { return ek_sin(deg); }                        /* Chowdren sin of degrees (exact 0 at 0 / 180) */
/* the current frame's action point (image action - hotspot; mirrored rows flip it) */
static void act_point(const obj_t *o, double *x, double *y) {
    *x = floorf(o->x); *y = floorf(o->y);
    if (!o->an.b || !o->an.b->act) return;
    const frame_t *f = anim_frame(&o->an); int fi = (int)(f - o->an.b->frames);
    double dx = o->an.b->act[2 * fi] - f->hx, dy = o->an.b->act[2 * fi + 1] - f->hy;
    *x += o->flip ? -dx : dx; *y += dy;
}
/* a finished play-once animation falls back to the idle row (Fusion returns to the stopped animation; RV: the squid / tank cycles
 * continue past their anim 7, which they test with test_animation) */
static int once_over(const obj_t *o, const char *base) { return ek_ad_is(o, base) && o->an.done; }
static void pop(obj_t *s) { snd_play_orig(17, 220, 128); obj_effect(s->x, s->y, "explosion02"); s->alive = 0; }   /* the shots' end (sound 17, explosion02) */
static int bg_edge(obj_t *o) {                                               /* test_collisions overlaps_background, edge-triggered */
    int bg = obj_overlaps_bg(o), hit = bg && !(o->fl & FLBG);
    o->fl = bg ? (o->fl | FLBG) : (o->fl & ~FLBG);
    return hit;
}
static int cube_on(const obj_t *o) { return player_cube_state() >= 4 && player_cube_hits(o); }   /* PlayerReflect A >= 4 on it */
/* BallMovement step (speed / 8 px a frame, the fraction carried as fb_move / RM fireballs) in direction d */
static void ball_step(obj_t *o, int d, double speed) {
    double v = speed * 0.125, a = (d & 31) * 11.25;
    double dx = v * ek_cos(a) + o->fx, dy = -v * ek_sin(a) + o->fy;
    double sx = floor(dx), sy = floor(dy); o->fx = (float)(dx - sx); o->fy = (float)(dy - sy); o->x += (float)sx; o->y += (float)sy;
}

int tb_enemy_q20(int t) {                                                       /* RE on_frame_45..49_init qualifier 20 */
    return t == OT_TBE_SEEKER || t == OT_TBE_PUHIT || t == OT_TBE_SQUID || t == OT_TBE_TANK || t == OT_TBE_SAUCER || t == OT_TBE_DEBRIS;
}

/* ================================================================ EnemySeeker_2603 (RE 11761-11780; RV seeker1)
 * Ctor A 5, H 5. av: [0] D (0 idle, 1 left, 2 right, 3 attack, 4 close, 6 defeated), [1] E base y, [2] F bob angle, [3] G timer,
 * [4] H. In view (80) it flies toward the player (2 px a frame, y = E + 40 sin F, F + 2.5), turning 150 px past him; within 99 px
 * (G <= 0) it stops and opens (anim 7; H 5 -> 0 by 0.5), G counts: at 20 a homing bullet (sound 59, its direction), at 61 it closes
 * (anim 8, H back to 5) and flies again with G 60 counting down (the cooldown). */
static void seeker_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gt = &o->av[3], *H = &o->av[4];
    double X = floorf(o->x), px = ek_px(p), py = ek_py(p);
    if (*D <= 2 && !(ek_ad_is(o, "tbs_close") && !o->an.done)) ek_set_ad(o, "tbs_idle");   /* 11761 (anim 8 ends: INFERRED) */
    if (*D == 3 && !ek_ad_is(o, "tbs_open")) ek_set_ad(o, "tbs_open");             /* 11762 */
    if (*D == 4 && !ek_ad_is(o, "tbs_close") && !ek_ad_is(o, "tbs_idle")) ek_set_ad(o, "tbs_close");   /* 11763 */
    if (DEFEATED(o)) { if (*D < 6) *D = 6; return; }                               /* 11777 */
    int v = in_view(o, 80);
    if (*D == 0 && v && px <= X) *D = 1;                                           /* 11764 */
    if (*D == 0 && v && X < px) *D = 2;                                            /* 11765 */
    for (int s = 1; s <= 2; s++) {
        if (*D == s && v) {                                                        /* 11766 / 11768 */
            o->y = (float)(int)(*E + 40 * sin0(*F)); *F = fmod(*F + 2.5, 360);
            o->x = (float)(floorf(o->x) + (s == 1 ? -2 : 2)); ek_face_d(o, s == 1 ? 16 : 0);
            *Gt = fmax(*Gt - 1, 0);
        }
        X = floorf(o->x);
        if (s == 1 && *D == 1 && X < px - 150) *D = 2;                              /* 11767 */
        if (s == 2 && *D == 2 && px + 150 < X) *D = 1;                              /* 11769 */
    }
    if (*D >= 1 && *D <= 2 && *Gt <= 0 && fabs(px - X) <= 99 && fabs(py - floorf(o->y)) <= 99) { *D = 3; *Gt = 0; }   /* 11770 (the y range INFERRED as x's) */
    if (*D == 3 && *H > 0) *H -= 0.5;                                              /* 11771 */
    if (*D == 3 && *H <= 0) *Gt += 1;                                              /* 11772 (the same frame, RV: G 61 with H 0.5 at 207) */
    if (*D == 3 && *Gt == 20) {                                                    /* 11773 */
        obj_t *b = spawn(OT_TBE_BULLET, floorf(o->x), floorf(o->y), o->dir & 31);
        if (b) { ek_set_ad(b, "tbs_bul"); b->av[0] = o->dir & 31; }
        snd_play_orig(59, 220, 128);
    }
    if (*D == 3 && *Gt > 60) *D = 4;                                               /* 11774 */
    if (*D == 4) *H += 0.5;                                                        /* 11775 */
    if (*D == 4 && *H >= 5) *D = 0;                                                /* 11776 */
}
/* EnemyHomingBullet_2648: BallMovement speed 30 (3.75 px a frame, RV) in its seeker's direction, angle + 30 a frame (11781); gone
 * 40 px off the view (11782); on the background (11691), a Q7 object (col_14_44) or Milla's cube (col_13_44): sound 17, explosion02;
 * on the character sensor: R-hurt 10 and the same end (col_5_44). */
static void bullet_frame(obj_t *b, player_t *p) {
    if (b->born != G.frame) ball_step(b, (int)b->av[0], 30);
    b->ang = (float)ek_nrm(b->ang + 30);                                            /* 11781 */
    if (!in_view(b, 40)) { b->alive = 0; return; }                                 /* 11782 */
    if (ek_csens_hit(p, b) && ek_r_hurt(p, 10, 0)) { pop(b); return; }            /* col_5_44 */
    if (cube_on(b)) { pop(b); return; }                                            /* col_13_44 */
    if (bg_edge(b) || objects_mask_hit_groups(b, 0)) { pop(b); return; }           /* 11691 / col_14_44 */
}

/* ================================================================ EnemyPushUp_2669 + EnemyPushUpHitbox_2663 (RE 11683-11801; RV pushup1)
 * The body (not Q20; ctor direction 1, D 55; one on an EnemyLimiter at the start takes direction 0 and the limiter goes, 11783)
 * stands up (direction 0) or crouches (1) with anim 0, D counting in view (80): at 20 it fires (anim 5, 3 loops: a laser at each
 * loop's frame 0, one push-up a frame, sound 437: direction 0 straight down from its action point + 20, 1 up from - 16), after
 * 80 it flips direction (11786). The invisible head (Q20, A 16) sits at the body's action point (11683); beaten, the body goes
 * (its 6 debris, loop_p_debris) and the head, shown (anim 8), flies away and lands like a Ganseki (11796 / 11798). */
static void pushup_fire(void) {                                                    /* 11789 / 11790: one random push-up a frame */
    obj_t *o, *c[EK_LMAX]; int n = 0;
    EACH(OT_TBE_PUSHUP, o) if (!(o->fl & FLGONE) && ek_ad_is(o, "tbp_fire") && o->an.frame == 0 && !(o->fl & FL2) && !o->hidden) c[n++] = o;
    if (!n) return;
    o = c[cross_rand(n)];
    o->fl |= FL2; snd_play_orig(437, 220, 128);
    double ax, ay; act_point(o, &ax, &ay);
    int down = (o->dir & 31) == 0;
    obj_t *l = spawn(OT_TBE_PULAS, (float)ax, (float)(ay + (down ? 20 : -16)), down ? 24 : 8);
    if (l) ek_set_ad(l, "tbp_las");
}
static void pushup_frame(obj_t *o) {
    double *D = &o->av[0];
    obj_t *h = ek_partner(o);
    if (o->fl & FLGONE) return;
    if (h && h->type == OT_TBE_PUHIT && !DEFEATED(h)) {                           /* 11683: the head at the action point */
        double ax, ay; act_point(o, &ax, &ay); h->x = (float)(int)ax; h->y = (float)(int)ay;
        o->inv = h->inv;                                                           /* 11792 / 11793 (the head's hit flash shown on the body, INFERRED) */
    }
    if (!h || h->type != OT_TBE_PUHIT || DEFEATED(h)) {                            /* 11794: beaten */
        o->fl |= FLGONE | 0x40; o->hidden = 1; o->inv = 0;
        for (int k = 0; k < 6; k++) {                                              /* loop_p_debris (11670): 6 pieces up the body, frame B */
            obj_t *d = spawn(OT_TBE_PUDEB, floorf(o->x) - 20 + 8 * k, floorf(o->y) - 5 - 12 * k, 0);
            if (!d) break;
            ek_set_ad(d, "tbp_deb"); ek_force_frame(d, k % 5);
            obj_pinball_start(d, 4 + (k * 7 + cross_rand(4)) % 9, 50);            /* (PinballMovement 50; directions INFERRED) */
        }
        return;
    }
    if (in_view(o, 80) && !(ek_ad_is(o, "tbp_fire") && !o->an.done)) { o->fl &= ~FL2; *D += 1; }   /* 11785 (a finished anim 5 counts: RV flips 102 frames apart) */
    if (*D > 80) {                                                                 /* 11786 */
        o->dir = (o->dir & 31) == 0 ? 1 : 0; *D = 0; ek_restart_ad(o, "tbp_up"); o->fl &= ~FL1;
    }
    if (*D == 20 && !(o->fl & FL1)) { ek_restart_ad(o, "tbp_fire"); *D += 1; o->fl |= FL1; }   /* 11787 */
    if (once_over(o, "tbp_fire")) {                                                /* 11788: back to anim 0, held on its last frame */
        ek_set_ad(o, "tbp_up"); ek_force_frame(o, 99); o->an.done = 1;
    }
    if (S.shot_frame != G.frame) { S.shot_frame = G.frame; pushup_fire(); }
    if ((o->fl & FL2) && ek_ad_is(o, "tbp_fire") && o->an.frame > 0) o->fl &= ~FL2;   /* 11791 */
}
static void puhit_frame(obj_t *h) {
    if (DEFEATED(h)) {                                                             /* 11796: shown, anim 8; the shared fly-away + landing */
        if (h->hidden) { h->hidden = 0; ek_set_ad(h, "tbp_head8"); }
        return;
    }
    obj_t *o = ek_partner(h);
    if (!o || o->type != OT_TBE_PUSHUP) h->alive = 0;                              /* (more heads than bodies) */
}
/* EnemyPushUpLaser_2670: BallMovement 60 (7.5 px, RV) straight down / up; on the background (col_27_44): two ground waves
 * (Laser2, directions 0 / 16) and sound 264; hurts 10 (col_5_44), Milla's cube pops it (col_16_44); 40 px off the view it goes */
static void pulas_frame(obj_t *l, player_t *p) {
    if (l->born != G.frame) ball_step(l, l->dir, 60);
    if (!in_view(l, 40)) { l->alive = 0; return; }                                 /* 11795 */
    if (ek_csens_hit(p, l) && ek_r_hurt(p, 10, 0)) { pop(l); return; }            /* col_5_44 */
    if (cube_on(l)) { pop(l); return; }                                            /* col_16_44 */
    if (bg_edge(l)) {                                                              /* col_27_44 */
        for (int k = 0; k < 2; k++) {                                              /* (their 11799 / 11800 run this frame: A -3, a diamond, RV) */
            obj_t *w = spawn(OT_TBE_PULAS2, l->x, l->y, k ? 16 : 0);
            if (!w) continue;
            w->hidden = 1; ek_set_ad(w, "tbp_las2"); w->fl |= FLBG; w->av[0] = -3;
            obj_t *d = spawn(OT_TBE_PULAS3, w->x, w->y, 0); if (d) ek_restart_ad(d, "tbp_las3");
        }
        snd_play_orig(264, 220, 128); l->alive = 0;
    }
}
/* EnemyPushUpLaser2_2695 (invisible): BallMovement 40 (5 px, RV) along the floor / ceiling; A + 1, every third frame a diamond
 * (11799 / 11800, RV: A -3 -2 -1); off the background (11801) or 40 px off the view it goes; the cube stops it (col_8_44) */
static void pulas2_frame(obj_t *w) {
    if (w->born != G.frame) ball_step(w, w->dir, 40);
    if (!in_view(w, 40) || cube_on(w)) { w->alive = 0; return; }
    w->av[0] += 1;                                                                 /* 11799 */
    if (w->av[0] >= 0) {                                                           /* 11800 */
        w->av[0] = -3; obj_t *d = spawn(OT_TBE_PULAS3, w->x, w->y, 0);
        if (d) { ek_restart_ad(d, "tbp_las3"); d->tm = 0; }
    }
    if (!obj_overlaps_bg(w)) w->alive = 0;                                         /* 11801 */
}
/* EnemyPushUpLaser3_2696: the diamond (anim 4); hurts 10 + explosion02 (col_3_44, it stays); gone after 26 frames (RV) */
static void pulas3_frame(obj_t *d, player_t *p) {
    if (++d->tm >= 26 || (ek_ad_is(d, "tbp_las3") && d->an.done && d->tm > 26)) { d->alive = 0; return; }
    if (ek_csens_hit(p, d) && ek_r_hurt(p, 10, 0)) obj_effect(d->x, d->y, "explosion02");   /* col_3_44 */
}
static void pudeb_frame(obj_t *d) {
    obj_pinball_step(d, 25);                                                       /* (gravity INFERRED as the pellets') */
    if (d->y > G.camy + SCREEN_H + 64 || !in_view(d, 200)) d->alive = 0;
}

/* ================================================================ EnemyLavaSquid_2740 (RE 11969-11991; RV squid2)
 * Ctor A 15, F 1, G -1 (facing). av: [0] D (0 idle, 1 aim, 2 dash, 3 turn), [1] E, [2] F (wiggle), [3] G, [4] H (float x), [5] I
 * (dash speed). In view (100) it wiggles (direction (1 + G) 8 + 16 + F); E 30 anim 11 (F = -F), E 50 a laser (anim 7: at its first
 * frame, sound 73, a white burst, laser speed 6 G), E 80 a second one and D 1: 30 frames anim 11 facing, then the dash backwards
 * (I = -8 G, 0.2 a frame toward 0) until I 0 or an EnemyLimiter it moves to (then D 3: anim 9 for 15 frames, G = -G). Beaten: two
 * halves (Debris_2742 frames 0 / 1, the knock-back Y -/+ 4) fly away as Q20 corpses and crash with a keystone (11987). */
static int limiter_ahead(const obj_t *o, double v) {                             /* 11980 / 11981: on an EnemyLimiter lying in the direction of motion */
    for (int k = 0; k < S.lim.n; k++)
        if (fabsf(S.lim.x[k] - o->x) < 96 && fabsf(S.lim.y[k] - o->y) < 96 && (v < 0 ? S.lim.x[k] - 13 < o->x : S.lim.x[k] - 13 > o->x) &&
            obj_rect_hit(o, S.lim.x[k] - 21, S.lim.y[k] - 18, S.lim.x[k] - 5, S.lim.y[k] + 14)) return 1;
    return 0;
}
static void squid_dir(obj_t *o, int wiggle) { int d = (int)((1 + o->av[3]) * 8 + 16 + (wiggle ? o->av[2] : 0)); ek_face_d(o, d & 31); }
static void squid_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gf = &o->av[3], *H = &o->av[4], *I = &o->av[5];
    (void)p;
    if (DEFEATED(o)) {                                                             /* 11985 / 11986 */
        for (int k = 0; k < 2; k++) {
            obj_t *d = spawn(OT_TBE_DEBRIS, o->x, o->y, o->dir & 31);
            if (!d) break;
            d->av[7] = 0; ek_set_ad(d, "tbq_deb"); ek_force_frame(d, k); d->A = 0; d->kx = o->kx; d->ky = o->ky + (k ? 4 : -4);
        }
        o->alive = 0; return;
    }
    if (once_over(o, "tbq_fire")) ek_set_ad(o, "tbq_idle");                       /* (a finished anim 7 returns to 0, INFERRED) */
    int v = in_view(o, 100), fire = ek_ad_is(o, "tbq_fire");
    if (*D == 0 && v) { *E += 1; squid_dir(o, 1); }                               /* 11969 */
    if (*D == 0 && *E > 30 && !ek_ad_is(o, "tbq_swim") && !fire) { ek_set_ad(o, "tbq_swim"); *F = -*F; }   /* 11970 */
    if (*D == 0 && *E == 50) { ek_restart_ad(o, "tbq_fire"); *E += 1; o->fl &= ~FL1; }   /* 11971 */
    if (*D == 0 && *E == 80) { ek_restart_ad(o, "tbq_fire"); *D = 1; o->fl &= ~FL1; *E = 0; *H = floorf(o->x); }   /* 11972 */
    fire = ek_ad_is(o, "tbq_fire");
    if (fire && !(o->fl & FL1)) {                                                  /* 11973 */
        o->fl |= FL1;
        double ax, ay; act_point(o, &ax, &ay);
        obj_t *l = spawn(OT_TBE_SQLAS, (float)ax, (float)ay, 0);
        if (l) { ek_set_ad(l, "tbq_las"); l->av[0] = 6 * *Gf; }
        snd_play_orig(73, 220, 128); obj_effect((float)ax, (float)ay, "wburst");
    }
    if (*D == 1 && !fire && v && !(o->fl & FL2)) { squid_dir(o, 0); ek_set_ad(o, "tbq_swim"); *E += 1; }   /* 11974 */
    if (*D == 1 && *E > 30) { *D = 2; ek_restart_ad(o, "tbq_fire"); *I = -8 * *Gf; }   /* 11975 (the fire flag stays: no shot) */
    if (*D == 2 && v) { o->x = (float)(int)*H; *H += *I; }                         /* 11976 */
    if (*D == 2 && *I > 0) *I = fmax(*I - 0.2, 0);                                 /* 11977 */
    if (*D == 2 && *I < 0) *I = fmin(*I + 0.2, 0);                                 /* 11978 */
    if (*D == 2 && fabs(*I) < 1e-9) { *I = 0; *D = 0; *E = 0; }                    /* 11979 */
    if (*D == 2 && *I != 0 && limiter_ahead(o, *I)) { o->fl |= FL2; *D = 0; *E = 0; *I = 0; }   /* 11980 / 11981 (I 0 INFERRED) */
    if (*D == 1 && !fire && v && (o->fl & FL2)) { *D = 3; *E = 0; }               /* 11982 */
    if (*D == 3) { ek_set_ad(o, "tbq_turn"); *E += 1; }                            /* 11983 */
    if (*D == 3 && *E > 15) { o->fl &= ~FL2; *D = 1; *E = 0; *Gf = -*Gf; }         /* 11984 */
}
/* EnemyLavaSquidLaser_2741: x + A a frame (11989), gone 100 px off the view (11990); R-hurt 10 (col_25_24, it stays); Milla's cube
 * sends it back once (col_6_45: flag 1, A = -A) */
static void sqlas_frame(obj_t *l, player_t *p) {
    l->x = (float)(floorf(l->x) + l->av[0]);                                       /* 11989 */
    ek_face_d(l, l->av[0] < 0 ? 16 : 0);
    if (!in_view(l, 100)) { l->alive = 0; return; }                                /* 11990 */
    if (!(l->fl & FL1) && cube_on(l)) { l->fl |= FL1; l->av[0] = -l->av[0]; }      /* col_6_45 */
    if (ek_csens_hit(p, l)) ek_r_hurt(p, 10, 0);                                   /* col_25_24 */
}

/* ================================================================ EnemyTankbot_2838 (RE 12425-12444; RV tank4)
 * Ctor A 15, F -1. av: [0] D (0 walk, 1 turn, 2 fire, 6 beaten), [1] E, [2] F facing, [3] G cooldown. In view (100) it walks 2 px
 * a frame; on an EnemyLimiter (E <= 0) it stops 30 frames and turns (E 10); with G <= 0 and the player 1-199 px in front (y within
 * -149 / +99 of him) or right after a hit (C > 0, facing him) it fires (anim 7, 21 frames: pellets at its frames 8 and 15, sound 28,
 * PinballMovement 50 in its direction), then G 90. Beaten (12441): the tracks stay (Debris_2839), sound 311, explosion01; the
 * turret (anim 8) flies away and crashes with a keystone. */
static void tank_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gt = &o->av[3];
    if (*F == 1) ek_face_d(o, 0); else if (*F == -1) ek_face_d(o, 16);                 /* 12425 / 12426 */
    if (DEFEATED(o) || (o->A <= 0 && *D != 6)) {
        if (*D != 6) {                                                             /* 12441 */
            obj_t *t = spawn(OT_TBE_TDEB, o->x, o->y, o->dir & 31);
            if (t) ek_set_ad(t, "tbt_deb");
            snd_play_orig(311, 220, 128); obj_effect(o->x, o->y, "explosion"); *D = 6;
        }
        ek_set_ad(o, "tbt_dead");                                                  /* 12442 */
        return;
    }
    int fire = ek_ad_is(o, "tbt_fire") && !o->an.done, lim = ek_limiter_hit(&S.lim, o);
    double X = floorf(o->x), Y = floorf(o->y), px = ek_px(p), py = ek_py(p);
    if (*D == 0 && in_view(o, 100)) { ek_set_ad(o, "tbt_walk"); o->x = (float)(X + 2 * *F); }   /* 12427 */
    if (*D == 0 && *E > 0) *E -= 1;                                                /* 12428 */
    else if (*D == 0 && lim) *D = 1;                                               /* 12429 */
    if (*D == 1) { ek_set_ad(o, "tbt_idle"); *E += 1; }                            /* 12430 */
    if (*D == 1 && *E > 30) { *D = 0; *E = 10; *F = -*F; ek_face_d(o, *F > 0 ? 0 : 16); }   /* 12431 */
    X = floorf(o->x);
    if (*D == 0 && *Gt <= 0 && o->c > 0 && !lim && !ek_dead(p)) {                  /* 12432 / 12433 */
        *D = 2; *F = px <= X ? -1 : 1; ek_face_d(o, *F > 0 ? 0 : 16); ek_restart_ad(o, "tbt_fire"); fire = 1;
    }
    if (*D < 2 && *Gt <= 0 && !lim && !ek_dead(p) && Y >= py - 149 && Y <= py + 99 &&   /* 12434 / 12435 */
        ((*F > 0 && px > X && px - X <= 199) || (*F < 0 && px < X && X - px <= 199))) { *D = 2; ek_restart_ad(o, "tbt_fire"); fire = 1; }
    int fr = fire ? o->an.frame : -1;
    if (fr != 8 && fr != 15) o->fl &= ~FL2;                                        /* 12436 */
    else if (!(o->fl & FL2)) {                                                     /* 12437 */
        double ax, ay; act_point(o, &ax, &ay);
        snd_play_orig(28, 220, 128); obj_spawn_pellet((float)ax, (float)ay, o->dir & 31, o->layer); o->fl |= FL2;
    }
    if (*D == 2 && !fire) { *D = 0; *E = 0; *Gt = 90; ek_set_ad(o, "tbt_idle"); }  /* 12438 (12439, MV.M 9: not ported) */
    if (*Gt > 0) *Gt -= 1;                                                         /* 12440 */
}

/* ================================================================ EnemySaucer_2828 + Leg1-4 + EnemySaucerLimiter_2832 (RE 12397-12422;
 * RV saucer4). Ctor A 22, H 32, I 1, J 180; start E = x, F = y. av: [0] D step angle, [1] E float x, [2] F base y, [3] G speed,
 * [4] I direction, [5] J timer, [6] K attack timer. G accelerates 0.1 a frame toward 3 I; a saucer limiter reverses I (flag 1 while
 * on it). D = (90 + D + 1.4 G) mod 90, x = E += G, y = F - 10 sin 2D; legs at x -/+ 32 sin D (1 / 3, on the ground) and x -/+ 32
 * cos D (2 / 4, y = F - 20 sin 2D). In view (50) with J > 180: K counts (anim 0 direction 1), fireballs straight up from its action
 * point at K 40 / 55 / 70 / 85 (sound 46), K > 90: J 0. Beaten: the dome and the legs become Q20 debris pieces (Debris_2831) flying
 * with the legs' B / C (RV -2 -2, -4 1, 4 -1, 2 2) and crashing (12420 / 12421 / 12537). */
static const double LEGV[4][2] = { { -2, -2 }, { -4, 1 }, { 4, -1 }, { 2, 2 } };   /* RV saucer4: the legs' B / C */
static int slim_hit(const obj_t *o) {                                             /* 12399: on a saucer limiter (16 x 32 image, hotspot centre) */
    obj_t *l;
    if (!o->an.b) return 0;
    const frame_t *f = anim_frame(&o->an);                                          /* (box against box: RV saucer4 turns at x 1122 on the limiter at 1152) */
    int x0 = (int)floorf(o->x) - f->hx, y0 = (int)floorf(o->y) - f->hy;
    EACH(OT_TBE_SLIM, l) if (x0 < (int)l->x + 8 && x0 + f->w > (int)l->x - 8 && y0 < (int)l->y + 16 && y0 + f->h > (int)l->y - 16) return 1;
    return 0;
}
static void saucer_frame(obj_t *o) {
    double *D = &o->av[0], *E = &o->av[1], *Fy = &o->av[2], *Gs = &o->av[3], *I = &o->av[4], *J = &o->av[5], *K = &o->av[6];
    obj_t *leg[4] = { 0 }, *l;
    for (int k = 0; k < 4; k++) EACH(OT_TBE_LEG1 + k, l) if (l->par == (int)(o - G.o)) { leg[k] = l; break; }
    if (DEFEATED(o)) {                                                             /* 12415 / 12411-12414 */
        obj_t *d = spawn(OT_TBE_DEBRIS, o->x, o->y, 0);
        if (d) { d->av[7] = 1; ek_set_ad(d, "tba_deb"); ek_force_frame(d, 0); d->A = 0; d->kx = o->kx; d->ky = o->ky; }
        for (int k = 0; k < 4; k++) if (leg[k]) {
            obj_t *q = spawn(OT_TBE_DEBRIS, leg[k]->x, leg[k]->y, 0);
            if (q) { q->av[7] = 1; ek_set_ad(q, "tba_deb"); ek_force_frame(q, (k == 0 || k == 3) ? 1 : 2); q->A = 0; q->kx = LEGV[k][0]; q->ky = LEGV[k][1]; }
            leg[k]->alive = 0;
        }
        o->alive = 0; return;
    }
    if (*I == 1) *Gs = fmin(*Gs + 0.1, 3);                                         /* 12397 */
    if (*I == -1) *Gs = fmax(*Gs - 0.1, -3);                                       /* 12398 */
    int on = slim_hit(o);                                                          /* 12399 (foreach_saucer_47: flag 2) */
    if (!(o->fl & FL1) && on) { o->fl |= FL1; *I = -*I; }                          /* 12400 */
    if ((o->fl & FL1) && !on) o->fl &= ~FL1;                                       /* 12401 */
    *D = fmod(90 + *D + 1.4 * *Gs, 90);                                            /* 12402 */
    *E += *Gs; o->x = (float)(int)*E; o->y = (float)(int)(*Fy - 10 * sin0(*D + *D));
    {                                                                              /* 12403: the legs (pairing: the nearest placement) */
        double X = floorf(o->x), H = 32, s = sin0(*D), c = sin0(*D - 90), c2 = sin0(*D + 90), up = 20 * sin0(*D + *D);
        if (leg[0]) { leg[0]->x = (float)(int)(X - H * s); leg[0]->y = (float)(int)*Fy; }
        if (leg[1]) { leg[1]->x = (float)(int)(X + H * c); leg[1]->y = (float)(int)(*Fy - up); }
        if (leg[2]) { leg[2]->x = (float)(int)(X + H * s); leg[2]->y = (float)(int)*Fy; }
        if (leg[3]) { leg[3]->x = (float)(int)(X + H * c2); leg[3]->y = (float)(int)(*Fy - up); }
        *J += 1;
    }
    if (in_view(o, 50) && *K == 0 && *J > 180) { *K = 1; o->dir = 1; }            /* 12404 */
    else if (*K > 0) *K += 1;                                                      /* 12405 */
    if (*K == 40 || *K == 55 || *K == 70 || *K == 85) {                            /* 12406-12409 */
        *K += 1;
        double ax, ay; act_point(o, &ax, &ay);
        obj_t *f = ek_spawn(&LFB, OT_FBALL, NULL, (float)ax, (float)ay, 8);
        if (f) { int a = ens_anim("fball"); if (a >= 0) anim_set(&f->an, &G.ens, a); f->av[3] = 8; f->av[4] = 40; f->fl |= FB_EXTERNAL; }   /* 12423: direction 8 (up), speed 40 */
        snd_play_orig(46, 220, 128);
        if (*K == 86) o->dir = 0;
    }
    if (*K > 90) { *K = 0; *J = 0; }                                               /* 12410 */
    ek_set_ad(o, o->dir == 1 ? "tba_open" : "tba_body");
}
static void leg_frame(obj_t *l) {
    obj_t *o = ek_partner(l);
    if (!o || o->type != OT_TBE_SAUCER) l->alive = 0;                              /* 12416-12419 (legs without a saucer) */
}

/* ================================================================ spawn / passes */
static void pair(int child, int owner) {                                           /* each child takes the nearest owner (by placement) */
    obj_t *o, *q;
    EACH(child, o) { float best = 1e9f; EACH(owner, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->par = (int)(q - G.o); } } }
}
static void ctor(obj_t *o) {                                                       /* the constructors' values (create_* / ctor RE) */
    memset(o->av, 0, sizeof o->av); o->st = o->tm = 0; o->fl = 0; o->dir = 0; o->an.b = NULL; o->par = -1; o->layer = 2; o->hidden = 0;
    o->A = 0; o->c = 0; o->bd = 0; o->absorbed = 0; o->fx = o->fy = 0; o->ang = 0;
    ek_list_add(&LST[o->type - RT0], o, o->type);
    switch (o->type) {
    case OT_TBE_SEEKER: o->A = 5; o->av[4] = 5; o->av[1] = floorf(o->y); ek_set_ad(o, "tbs_idle"); break;
    case OT_TBE_PUSHUP: o->av[0] = 55; o->dir = 1; ek_set_ad(o, "tbp_up"); break;
    case OT_TBE_PUHIT: o->A = 16; o->hidden = 1; ek_set_ad(o, "tbp_head"); break;
    case OT_TBE_SQUID: o->A = 15; o->av[2] = 1; o->av[3] = -1; o->fl = FL2; ek_set_ad(o, "tbq_idle"); break;   /* RV squid2: flag 2 on from the start (the first aim turns it round) */
    case OT_TBE_TANK: o->A = 15; o->av[2] = -1; o->dir = 16; ek_set_ad(o, "tbt_idle"); break;
    case OT_TBE_SAUCER: o->A = 22; o->av[4] = 1; o->av[5] = 180; o->av[1] = floorf(o->x); o->av[2] = floorf(o->y); ek_set_ad(o, "tba_body"); break;
    case OT_TBE_LEG1: case OT_TBE_LEG1 + 3: ek_set_ad(o, "tba_leg1"); break;
    case OT_TBE_LEG1 + 1: case OT_TBE_LEG1 + 2: ek_set_ad(o, "tba_leg2"); break;
    case OT_TBE_SLIM: o->hidden = 1; break;
    default: o->alive = 0; break;
    }
}
/* a runtime instance of a placed TB class (Final Dreadnought's spawner pods drop tank bots, RE 15246): the constructor's values; the
 * module starts if the level placed none (S.on, limiters) */
obj_t *tb_enemy_create(int type, float x, float y) {
    if (type < OT_TBE_FIRST || type > OT_TBE_SLIM) return NULL;
    if (!S.on) { S.on = 1; S.shot_frame = -1; ek_limiters_collect(&S.lim); }
    obj_t *o = spawn(type, x, y, 0);
    if (o) ctor(o);
    return o;
}
/* EnemyHomingBullet_2648 (the seeker's shot, 11781 / 11782 / col_5_44: BallMovement 30 in direction dir, hurts 10): also Final
 * Dreadnought's spider beamers' (RE 16694) */
obj_t *tb_enemy_bullet_create(float x, float y, int dir) {
    if (!S.on) { S.on = 1; S.shot_frame = -1; ek_limiters_collect(&S.lim); }
    obj_t *b = spawn(OT_TBE_BULLET, x, y, dir & 31);
    if (b) { ek_set_ad(b, "tbs_bul"); b->av[0] = dir & 31; }
    return b;
}
void tb_enemy_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(&LFB, sizeof LFB); snap_register(&LEMB, sizeof LEMB); }
    memset(&S, 0, sizeof S); memset(LST, 0, sizeof LST); memset(&LFB, 0, sizeof LFB); memset(&LEMB, 0, sizeof LEMB);
    S.shot_frame = -1;
    for (int t = OT_TBE_FIRST; t <= OT_TBE_LAST && !S.on; t++) S.on = objects_have(t);
    if (!S.on) return;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_TBE_FIRST || o->type > OT_TBE_LAST) continue;
        if (o->x < 0 || o->y < 0) { o->alive = 0; continue; }                      /* off-map templates */
        ctor(o);
    }
    /* Fusion pairs a part with its owner by instance order; the placements put each part on its owner (RE tb_inventory) */
    pair(OT_TBE_PUHIT, OT_TBE_PUSHUP);
    { obj_t *o, *h; EACH(OT_TBE_PUSHUP, o) EACH(OT_TBE_PUHIT, h) if (h->par == (int)(o - G.o)) o->par = (int)(h - G.o); }
    for (int k = 0; k < 4; k++) pair(OT_TBE_LEG1 + k, OT_TBE_SAUCER);
    /* 11783: a push-up on an EnemyLimiter turns to direction 0 and the limiter goes (the first frame) */
    { obj_t *o;
      EACH(OT_TBE_PUSHUP, o)
        for (int i = 0; i < G.nobjs; i++) { obj_t *l = &G.o[i];
            if (l->alive && l->type == OT_LIMITER && fabsf(l->x - o->x) < 96 && fabsf(l->y - o->y) < 96 &&
                obj_rect_hit(o, (int)l->x - 21, (int)l->y - 18, (int)l->x - 5, (int)l->y + 14)) { l->alive = 0; o->dir = 0; ek_set_ad(o, "tbp_up"); } } }
    ek_limiters_collect(&S.lim);
}
void tb_enemy_pre(player_t *p) {                                                  /* the saucers' fireballs and embers (Relic Maze's class) */
    if (!S.on) return;
    obj_t *f;
    static const struct { int t, n; } SHOTS[] = { { OT_TBE_BULLET, 40 }, { OT_TBE_PULAS, 40 }, { OT_TBE_PULAS2, 40 }, { OT_TBE_PULAS3, 40 },
                                                  { OT_TBE_SQLAS, 100 }, { OT_TBE_PUDEB, 200 } };
    for (unsigned k = 0; k < sizeof SHOTS / sizeof SHOTS[0]; k++)                  /* off the view they go (11782 / 11795 / 11990): objects.c
                                                                                      leaves objects far from the view asleep, these must not linger */
        EACH(SHOTS[k].t, f) if (!in_view(f, SHOTS[k].n)) f->alive = 0;
    if (!LFB.n && !LEMB.n) return;
    EK_EACH(&LFB, OT_FBALL, f) {
        obj_t *e = fireball_fly(f);                                                /* movement, 5703 */
        if (e) ek_list_add(&LEMB, e, OT_EMBER);
        fireball_rules(f, p, 0);                                                   /* 5710, col_17_25 / col_18_24 / col_23_24, 12424 */
    }
    EK_EACH(&LEMB, OT_EMBER, f) if (f->an.done) f->alive = 0;
}
void tb_enemy_draw(int pass) { (void)pass; }
int tb_enemy_obj(obj_t *o, player_t *p) {
    if (!S.on) return 0;
    switch (o->type) {
    case OT_TBE_SEEKER: seeker_frame(o, p); return 1;
    case OT_TBE_BULLET: bullet_frame(o, p); return 1;
    case OT_TBE_PUSHUP: pushup_frame(o); return 1;
    case OT_TBE_PUHIT: puhit_frame(o); return 1;
    case OT_TBE_PULAS: pulas_frame(o, p); return 1;
    case OT_TBE_PULAS2: pulas2_frame(o); return 1;
    case OT_TBE_PULAS3: pulas3_frame(o, p); return 1;
    case OT_TBE_PUDEB: pudeb_frame(o); return 1;
    case OT_TBE_SQUID: squid_frame(o, p); return 1;
    case OT_TBE_SQLAS: sqlas_frame(o, p); return 1;
    case OT_TBE_TANK: tank_frame(o, p); return 1;
    case OT_TBE_SAUCER: saucer_frame(o); return 1;
    case OT_TBE_LEG1: case OT_TBE_LEG1 + 1: case OT_TBE_LEG1 + 2: case OT_TBE_LEG4: leg_frame(o); return 1;
    case OT_TBE_DEBRIS: return 1;                                                  /* the shared fly-away and crash (combat.c) */
    case OT_TBE_TDEB: return 1;                                                    /* (the tracks stay: INFERRED) */
    default: return 1;
    }
}
