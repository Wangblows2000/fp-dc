/* Enemy combat, shared by every Dragon Valley enemy (RE: build/re/spec_combat.md; original trace combat_t1).
 *
 * Lilac damages enemies in two ways only:
 *  - ShieldInstaShield_407 "slash" objects (OT_SHIELD): created on a skin frame by the hair whip (2021),
 *    uppercut (2032), crouch kick (1831), and re-created whenever none exists by the cyclone (2043) and the
 *    kick (1764/1765). A shield never moves; its whole image rectangle is tested against the enemy's mask.
 *  - body hits while movement Z = 1 (charge/boost, kick wind-up): her big sensor discs and her character sensor.
 * Per frame, after this frame's movement: contact (2731), the hit events in table order (2732 -> 2758), one
 * random eligible enemy each, at most one hit per frame (K), then the shared enemy events 2761-2767.
 * A plain jump never damages an enemy. Hits give no rebound, energy or score. */
#include <math.h>
#include <string.h>
#include "../fp.h"

#define F_ARMOURED 0x20
#define F_DEFEATED 0x40

/* ---- shields: anim 4 of ShieldInstaShield_407 per direction (image rect relative to the creation point) */
typedef struct { signed char x0, x1, y0, y1; } sbox_t;
typedef struct { int dir, speed, n; sbox_t f[6]; const char *anim; } sdef_t;
static const sdef_t SD[] = {
    { 0, 100, 6, { {-1,39,-25,13}, {-1,57,-25,13}, {-1,57,-25,13}, {-1,57,-25,13}, {-1,57,-25,13}, {-1,57,-25,13} }, "slash" },
    { 16, 100, 6, { {-38,2,-25,13}, {-56,2,-25,13}, {-56,2,-25,13}, {-56,2,-25,13}, {-56,2,-25,13}, {-56,2,-25,13} }, "slash_l" },
    { 1, 20, 5, { {-6,34,-39,1}, {-6,34,-39,1}, {-6,44,-49,1}, {2,52,-57,-7}, {2,52,-57,-7} }, "upslash" },
    { 17, 20, 5, { {-33,7,-39,1}, {-33,7,-39,1}, {-43,7,-49,1}, {-51,-1,-57,-7}, {-51,-1,-57,-7} }, "upslash_l" },
    { 31, 100, 4, { {8,40,3,12}, {19,53,0,14}, {7,57,-11,10}, {4,53,-10,6} }, "ckick" },
    { 15, 100, 4, { {-39,-7,3,12}, {-52,-18,0,14}, {-56,-6,-11,10}, {-52,-3,-10,6} }, "ckick_l" },
    { 7, 100, 5, { {6,29,2,25}, {-2,29,-5,25}, {-5,29,-9,25}, {-6,27,-10,23}, {-6,26,-9,22} }, NULL },
    { 9, 100, 5, { {-28,-5,2,25}, {-28,3,-5,25}, {-28,6,-9,25}, {-26,7,-10,23}, {-25,7,-9,22} }, NULL },
    { 8, 100, 6, { {-40,40,-33,7}, {-40,40,-33,7}, {-40,40,-33,7}, {-40,40,-33,7}, {-40,40,-33,7}, {-40,40,-33,7} }, NULL },
};
#define NSD (int)(sizeof SD / sizeof SD[0])
/* obj_t use for shields: dir = index into SD, st = shield A (0 or 2), tm = frame index, c = speed counter,
 * fl bit 0 = finished (destroyed on the next update), vx = speed */
static const sdef_t *sdef(const obj_t *o) { return &SD[o->dir]; }
static int shield_exists(void) {
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SHIELD) return 1;
    return 0;
}
static int spawn_point(const player_t *p, int *px, int *py) {   /* Player_326: (int X, int Y) clamped to the level */
    double X = p->x, Y = p->y - ORIG_REF_DY;
    *px = (int)(X < 0 ? 0 : X > G.lvl.w ? G.lvl.w : X); *py = (int)(Y < 0 ? 0 : Y > G.lvl.h ? G.lvl.h : Y);
    return 1;
}
static void shield_create(const player_t *p, int dir, int A, int speed) {
    int k; for (k = 0; k < NSD && SD[k].dir != dir; k++) ;
    if (k == NSD) return;
    obj_t *o = obj_alloc();
    if (!o) return;
    int px, py; spawn_point(p, &px, &py);
    o->type = OT_SHIELD; o->alive = 1; o->x = (float)px; o->y = (float)py; o->dir = k; o->st = A;
    o->tm = 0; o->c = 0; o->vx = (float)(speed > 0 ? speed : SD[k].speed); o->fl = 0; o->born = G.frame;
    o->an.b = NULL;
    if (SD[k].anim) { int a = bank_anim(&G.objs, SD[k].anim); if (a >= 0) { anim_set(&o->an, &G.objs, a); } }
}
/* Active::update: counter += speed, advance when it goes above 100 (-100); past the last frame the
 * "finished" byte is set and an object with flag 8 is destroyed on the following update */
static void shield_tick(obj_t *o) {
    if (o->fl & 1) { o->alive = 0; return; }
    o->c += (int)o->vx;
    if (o->c > 100) {
        o->c -= 100;
        if (o->tm + 1 >= sdef(o)->n) o->fl |= 1; else o->tm++;
    }
}
static void shield_rect(const obj_t *o, int *x0, int *y0, int *x1, int *y1) {
    const sbox_t *b = &sdef(o)->f[o->tm];
    *x0 = (int)o->x + b->x0; *x1 = (int)o->x + b->x1; *y0 = (int)o->y + b->y0; *y1 = (int)o->y + b->y1;
}

/* creators (events 1764/1765, 1831, 2021, 2032, 2043) */
static void shield_creators(player_t *p) {
    int fr = player_anim_frame(p);
    if (p->state == ST_ATTACK || p->state == ST_UPPERCUT || p->state == ST_CROUCHATK) {
        int trig = p->state == ST_CROUCHATK ? 1 : 2;
        if (fr != trig) p->sh_latch = 0;                       /* 2023: flag cleared off the trigger frame */
        else if (!p->sh_latch) {
            p->sh_latch = 1;
            if (p->state == ST_ATTACK) { shield_create(p, p->face > 0 ? 0 : 16, 0, 100); snd_play_orig(21, 220, 128); }
            else if (p->state == ST_UPPERCUT) { shield_create(p, p->face > 0 ? 1 : 17, 0, 0); snd_play_orig(289, 220, 128); p->cyclone_used = 0; }
            else { shield_create(p, p->face > 0 ? 31 : 15, 0, 100); snd_play_orig(290, 220, 128); }
        }
    } else p->sh_latch = 0;
    if (p->state == ST_CYCLONE && !shield_exists()) shield_create(p, 8, 2, 0);
    if (p->state == ST_DIVEKICK && !shield_exists()) shield_create(p, p->face > 0 ? 7 : 9, 2, 0);
    /* 2022: during the whip every shield takes Lilac's facing (mirrored in place) */
    if (p->state == ST_ATTACK)
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || o->type != OT_SHIELD) continue;
            int d = sdef(o)->dir;
            if ((d == 0 || d == 16) && (p->face > 0) != (d == 0)) {
                int k; for (k = 0; k < NSD && SD[k].dir != (p->face > 0 ? 0 : 16); k++) ;
                o->dir = k;
                if (SD[k].anim) { int a = bank_anim(&G.objs, SD[k].anim); if (a >= 0) { anim_set(&o->an, &G.objs, a); } }
            }
        }
}

/* ---- enemies */
int enemy_type(int t) { return t == OT_GANSEKI || t == OT_TURRETUS || t == OT_DRAGONFLY; }
static int candidates[MAX_OBJS], ncand;

/* Lilac's character sensor (PlayerCharacterSensor_375, direction 0): anim 0 = x -7..8, y -34..12; anim 1 = y -13..14 */
static void char_rect(const player_t *p, int *x0, int *y0, int *x1, int *y1) {
    int X = (int)floor(p->x), Y = (int)floor(p->y - ORIG_REF_DY);
    *x0 = X - 7; *x1 = X + 8;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 14; } else { *y0 = Y - 34; *y1 = Y + 12; }
}
static int sensor_hits(const obj_t *o, int s) {
    int ox, oy, h; const signed char *x0, *x1;
    if (!mv_sensor_rows(s, &ox, &oy, &x0, &x1, &h)) return 0;
    return obj_rows_hit(o, ox, oy, x0, x1, h);
}
typedef enum { AT_BOTTOM, AT_TOP, AT_SHIELD, AT_LEFT, AT_RIGHT, AT_CHAR, AT_P326 } atk_t;
/* collect enemies overlapped by an attacker; armoured: keep only flag-5 ones (deflect events) */
static void collect(const player_t *p, atk_t a, int armoured, const obj_t *sh) {
    ncand = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !enemy_type(o->type) || !o->an.b) continue;
        if (o->fl & F_DEFEATED) continue;
        if (armoured ? !(o->fl & F_ARMOURED) : (o->c > 0 || (o->fl & F_ARMOURED))) continue;
        int hit = 0;
        switch (a) {
        case AT_BOTTOM: hit = sensor_hits(o, SENS_BOTTOMBIG); break;
        case AT_TOP: hit = sensor_hits(o, SENS_TOPBIG); break;
        case AT_LEFT: hit = sensor_hits(o, SENS_LEFTBIG); break;
        case AT_RIGHT: hit = sensor_hits(o, SENS_RIGHTBIG); break;
        case AT_P326: hit = sensor_hits(o, SENS_MAIN); break;
        case AT_CHAR: { int x0, y0, x1, y1; char_rect(p, &x0, &y0, &x1, &y1); hit = obj_rect_hit(o, x0, y0, x1, y1); break; }
        case AT_SHIELD: { int x0, y0, x1, y1; shield_rect(sh, &x0, &y0, &x1, &y1); hit = obj_rect_hit(o, x0, y0, x1, y1); break; }
        }
        if (hit) candidates[ncand++] = i;
    }
}
static obj_t *pick(void) { return ncand ? &G.o[candidates[ncand > 1 ? cross_rand(ncand) : (cross_rand(1), 0)]] : NULL; }

static void hit_common(player_t *p, obj_t *e, int C, double X, double Y, double dmg) {
    p->hit_i++;
    e->c = C; e->kx = X; e->ky = Y; e->A -= dmg;
    e->inv = 1;                                      /* set_shader(21): invert */
    snd_play_orig(24, 220, 128);
    p->hitstop = 5; p->hs_off = 0;
}
static void deflect(player_t *p, int C) {           /* armoured: every overlapping armoured enemy C, sound 30 */
    p->hit_i++;
    for (int k = 0; k < ncand; k++) G.o[candidates[k]].c = C;
    snd_play_orig(30, 220, 128);
    p->hitstop = 5; p->hs_off = 0;
}

static int any_body_overlap(const player_t *p) {     /* 2761: any of the big discs / character sensor on an enemy */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !enemy_type(o->type) || !o->an.b) continue;
        int x0, y0, x1, y1; char_rect(p, &x0, &y0, &x1, &y1);
        if (sensor_hits(o, SENS_LEFTBIG) || sensor_hits(o, SENS_RIGHTBIG) || sensor_hits(o, SENS_TOPBIG) ||
            sensor_hits(o, SENS_BOTTOMBIG) || obj_rect_hit(o, x0, y0, x1, y1)) return 1;
    }
    return 0;
}

/* landing explosion (Ganseki 4390, Dragonfly 4807; Turretus 4776 adds the keystone and debris, sound 29) */
static void corpse_land(obj_t *o) {
    if (o->type == OT_TURRETUS) { snd_play_orig(29, 220, 128); obj_effect(o->x, o->y, "explosion"); }
    else { snd_play_orig(50, 220, 128); obj_effect(o->x, o->y, "explosion"); }
    o->alive = 0;
}

void combat_frame(player_t *p) {
    int dead = p->hp <= 0 || p->state == ST_DEAD;
    /* shields age with update_objects (before events), then the creators run */
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SHIELD && G.o[i].born != G.frame) shield_tick(&G.o[i]);
    if (!dead) shield_creators(p);
    double B = p->ground ? p->gsp : p->vx, Gv = p->ground ? 0.0 : p->vy;
    int Q = p->face > 0 ? 1 : -1, Z = player_body_z(p);
    obj_t *e;
    if (!dead) {
        /* 2731 contact: Z = 0, not attacking, R = 0, E <= 0; one random overlapping enemy with B > 0, C <= 0 */
        if (!player_contact_safe(p)) {
            ncand = 0;
            int x0, y0, x1, y1; char_rect(p, &x0, &y0, &x1, &y1);
            for (int i = 0; i < G.nobjs; i++) {
                obj_t *o = &G.o[i];
                if (o->alive && enemy_type(o->type) && o->an.b && obj_rect_hit(o, x0, y0, x1, y1)) candidates[ncand++] = i;
            }
            if ((e = pick()) && e->kx == e->kx) {
                int Bd = e->type == OT_TURRETUS ? 0 : 5;   /* contact damage B: Ganseki 4391, Dragonfly 4796 = 5; Turretus 0 */
                if (Bd > 0 && e->c <= 0 && !(e->fl & F_DEFEATED)) player_contact(p, Bd);
            }
        }
        if (Z && Gv >= 0 && p->hitstop <= 0 && p->hit_i < 4) {                                  /* 2732 / 2733 */
            collect(p, AT_BOTTOM, 0, NULL); if ((e = pick())) hit_common(p, e, 5, 0.5 * B, Gv, fabs(Gv) / 5 + 3);
            if (p->hitstop <= 0) { collect(p, AT_BOTTOM, 1, NULL); if (ncand) deflect(p, 5); }
        }
        if (Z && Gv < 0 && p->hitstop <= 0 && p->hit_i < 4) {                                   /* 2736 / 2737 */
            collect(p, AT_TOP, 0, NULL); if ((e = pick())) hit_common(p, e, 5, 0.5 * B, Gv, fabs(Gv) / 5 + 3);
            if (p->hitstop <= 0) { collect(p, AT_TOP, 1, NULL); if (ncand) deflect(p, 5); }
        }
        if (p->hitstop <= 0) {                                                                    /* 2740 / 2741 */
            for (int i = 0; i < G.nobjs && p->hitstop <= 0; i++) {
                obj_t *s = &G.o[i];
                if (!s->alive || s->type != OT_SHIELD) continue;
                collect(p, AT_SHIELD, 0, s);
                if ((e = pick())) {
                    double kx = fmax(3.0 - s->st, fabs(1.5 * B)) * Q;
                    hit_common(p, e, 5 + s->st, kx, 0.5 * Gv, (4 - s->st) + fabs(B) / 10);
                    break;
                }
                collect(p, AT_SHIELD, 1, s);
                if (ncand) { deflect(p, 5 + s->st); break; }
            }
        }
        if (Z && p->hitstop <= 0 && p->hit_i < 4) {                                              /* 2753-2756 */
            for (int side = 0; side < 2 && p->hitstop <= 0; side++) {
                collect(p, side ? AT_RIGHT : AT_LEFT, 0, NULL);
                if ((e = pick())) hit_common(p, e, 5, fmax(3.0, fabs(1.5 * B)) * Q, 0.5 * Gv, fabs(B) / 5 + 3);
                else { collect(p, side ? AT_RIGHT : AT_LEFT, 1, NULL); if (ncand) deflect(p, 5); }
            }
        }
        if (Z && p->hitstop <= 0 && p->hit_i < 4) {                                              /* 2757 */
            collect(p, AT_CHAR, 0, NULL);
            if ((e = pick())) hit_common(p, e, 5, fmax(3.0, fabs(1.5 * B)) * Q, Gv, 1);
        }
        if (Z && p->state == ST_BOOST) {                                                      /* 2758: boost dash bonus */
            for (int i = 0; i < G.nobjs; i++) {
                obj_t *o = &G.o[i];
                if (o->alive && enemy_type(o->type) && o->an.b && o->c == 5 && !(o->fl & (F_ARMOURED | F_DEFEATED)) && sensor_hits(o, SENS_MAIN)) {
                    o->A -= 20; snd_play_orig(314, 220, 128);
                }
            }
        }
        if (Z && p->hitstop <= 0 && p->hit_i < 4) {                                              /* 2759 */
            collect(p, AT_CHAR, 1, NULL); if (ncand) deflect(p, 5);
        }
        /* 2761 (and 2038 / 1997: every frame in the cyclone and the boost) */
        if (p->hit_i > 0 && (!Z || !any_body_overlap(p))) p->hit_i = 0;
        if (p->state == ST_CYCLONE || p->state == ST_BOOST) p->hit_i = 0;
    }
    /* shared enemy events 2762-2767 */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !enemy_type(o->type)) continue;
        if (!(o->fl & F_DEFEATED) && o->c > 0) o->c--;                                          /* 2762 */
        if ((!(o->fl & F_DEFEATED) && o->c == 1) || (o->fl & F_DEFEATED)) o->inv = 0;           /* 2763 */
        if (o->A <= 0 && !(o->fl & F_DEFEATED)) {                                                /* 2764 */
            G.kills++;                   /* test/trace counter only (no counterpart in the original) */
            snd_play_orig(23, 220, 128); o->fl |= F_DEFEATED; o->A = 0; o->ky -= 3; o->st = o->type == OT_DRAGONFLY ? 5 : 3;
        }
        if (o->fl & F_DEFEATED) {                                                                /* 2765 fly-away */
            o->x = (float)(int)(o->x + o->kx); o->y = (float)(int)(o->y + o->ky);
            o->ky = fmin(o->ky + 0.25, 15.0);
            o->ang += (float)fmax(8.0, 2.0 * o->kx);
            if (o->absorbed > 0) { o->absorbed--; obj_effect(o->x, o->y, "objectcrystal_3"); }   /* 2766 (crystal drop: motion INFERRED) */
            if (o->x < 0 || o->x > G.lvl.w || o->y > G.lvl.h || o->y < 0) { o->alive = 0; continue; }   /* 2767 */
            if (o->y > G.camy + 290) { o->alive = 0; continue; }                                 /* 4389 / 4777 */
            if (o->ky > 0 && obj_overlaps_bg(o)) corpse_land(o);                        /* 4390 / 4776 / 4807 */
        }
    }
}

/* visible shields (hair whip, uppercut, crouch kick) are drawn behind Lilac */
void combat_draw_shields(int behind) {
    (void)behind;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_SHIELD || !o->an.b) continue;
        const bank_t *b = o->an.b;
        const anim_t *a = &b->anims[o->an.anim];
        int k = o->tm < a->count ? o->tm : a->count - 1;
        frame_draw(b, &b->frames[b->seq[a->first + k]], floorf(o->x - G.camx), floorf(o->y - G.camy), 0, 0xffffffff);
    }
}
