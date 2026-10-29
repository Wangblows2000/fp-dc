/* Dragon Valley enemies beyond Ganseki / Turretus (RE: build/re/spec_dv_enemies.md). Their hits, contact,
 * flash, defeat and fly-away are the shared rules in combat.c; contact damage uses each enemy's own B (o->bd),
 * which only its events set. Sprites come from the per-level _ens bank (left-facing sets are the *_l anims).
 * Field use: st = D (state), tm = H / I timers, vx = E (x speed), vy = F (y speed), fx = F/G home X or float X,
 * fy = G/H home Y or float Y, dir = facing direction (0 right / 16 left), fl bit 6 = defeated (combat.c). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../fp.h"

#define DEFEATED(o) ((o)->fl & 0x40)

int ens_anim(const char *name) { return G.ens.nframes ? bank_anim(&G.ens, name) : -1; }
static void ens_set(obj_t *o, const char *base) {             /* force_animation in the current facing; counter kept */
    char nm[24];
    snprintf(nm, sizeof nm, "%s%s", base, o->dir == 16 ? "_l" : "");
    int a = ens_anim(nm);
    if (a < 0) a = ens_anim(base);
    if (a < 0) return;
    if (o->an.b != &G.ens || o->an.anim != a) { int acc = o->an.acc; anim_set(&o->an, &G.ens, a); o->an.acc = acc; }
}
static int anim_is(const obj_t *o, const char *base) {
    char nm[24]; snprintf(nm, sizeof nm, "%s%s", base, o->dir == 16 ? "_l" : "");
    int a = ens_anim(nm);
    return o->an.b == &G.ens && (o->an.anim == a || o->an.anim == ens_anim(base));
}
/* "animation playing" = current anim and not finished */
static int anim_playing(const obj_t *o, const char *base) { return anim_is(o, base) && !o->an.done; }

/* image box of the current frame */
static void ibox(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = o->x - 8; *x1 = o->x + 8; *y0 = o->y - 8; *y1 = o->y + 8; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
/* the whole (uncropped) image box when the bank has one (BOX1), else the atlas frame's box */
static void fullbox(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (o->an.b) {
        const frame_t *f = anim_frame(&o->an);
        const bank_t *b = o->an.b;
        if (b->fbox) {   /* the uncropped image (BOX1; RE is_near_border uses the image box, RV bg3_squid) */
            const int16_t *q = b->fbox + 4 * (f - b->frames);
            if (q[2] > 0) {
                int hx = o->flip ? q[2] - q[0] - (o->flip == 2) : q[0];
                *x0 = floorf(o->x) - hx; *y0 = floorf(o->y) - q[1]; *x1 = *x0 + q[2]; *y1 = *y0 + q[3];
                return;
            }
        }
    }
    ibox(o, x0, y0, x1, y1);
}
/* !is_near_border(-n): the whole image box inside the view grown by n (RE FrameObject::is_near_border; RE compares strictly: kept
 * inclusive here, INFERRED tolerance for the port's 1-2 px position differences, e.g. test_th_enemies bike skids RV 93/99/105) */
int obj_near_view(const obj_t *o, int n) {
    float x0, y0, x1, y1; fullbox(o, &x0, &y0, &x1, &y1);
    return x0 >= G.camx - n && x1 <= G.camx + SCREEN_W + n && y0 >= G.camy - n && y1 <= G.camy + SCREEN_H + n;
}
static int obstacle(float x, float y) { return px_solid(&G.lvl, (int)floorf(x), (int)floorf(y)); }   /* Frame::test_obstacle */
static double MVX(const player_t *p) { return p->x; }
static double MVY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / 57.29577951308232); }
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / 57.29577951308232); }

/* ---- Dragonfly (EnemyDragonfly_942, events 4778-4807) */
void dragonfly_spawn(obj_t *o) { o->A = 4; o->vx = -2; o->dir = 16; o->st = 0; o->fx = o->x; o->fy = o->y; o->tm = 0; o->bd = 0; ens_set(o, "df_fly"); }
static void df_turn(obj_t *o) { o->vx = -o->vx; o->dir = o->dir == 16 ? 0 : 16; o->st = 1; ens_set(o, "df_turn"); anim_restart(&o->an); }
void dragonfly_update(obj_t *o, player_t *p) {
    if (DEFEATED(o)) { ens_set(o, "df_dead"); return; }                                    /* 4805 / 4806 */
    float x0, y0, x1, y1;
    if (o->st == 0) { ens_set(o, "df_fly"); o->x = (float)(int)(o->x + o->vx); }           /* 4779 */
    ibox(o, &x0, &y0, &x1, &y1);
    if (o->st == 0 && o->vx < 0 && (obstacle(x0, o->y) || o->x < o->fx - 200)) df_turn(o);            /* 4780 / 4781 */
    else if (o->st == 0 && o->vx > 0 && (obstacle(x1, o->y) || o->x > o->fx + 200)) df_turn(o);       /* 4782 / 4783 */
    if (o->st == 1 && !anim_playing(o, "df_turn")) o->st = 0;                              /* 4784 */
    if (o->st == 1 && !obj_near_view(o, 50)) o->st = 0;                                    /* 4785 */
    double mx = MVX(p), my = MVY(p);
    if (o->st == 0 && my - 150 < o->y && o->y < my && p->state != ST_HURT && p->state != ST_DEAD) {   /* 4786 / 4787 */
        if (fabs(o->x - (mx + (my - o->fy))) < 10) { o->st = 2; o->dir = 16; o->vx = -2; o->tm = 0; }
        else if (fabs(o->x - (mx - (my - o->fy))) < 10) { o->st = 2; o->dir = 0; o->vx = 2; o->tm = 0; }
    }
    if (o->st == 2) {                                                                      /* 4788-4793 */
        ens_set(o, "df_wind"); o->tm++; o->y = (float)(int)(o->fy - 0.5 * o->tm);
        double line = o->vx > 0 ? mx - (my - o->fy) : mx + (my - o->fy);
        if (o->x > line + 2) o->x -= 1; else if (o->x < line - 2) o->x += 1;
        if (o->tm > 30) { o->st = 3; o->tm = 0; }
    }
    if (o->st == 3) {                                                                      /* 4794-4800 */
        if (o->tm < 15) o->tm++;
        else if (o->tm == 15) { snd_play_orig(10, 220, 128); o->tm = 16; }
        if (o->tm >= 15) {
            ens_set(o, "df_dive"); o->bd = 5;
            o->x += o->vx < 0 ? -5 : 5; o->y += 5;
        }
        ibox(o, &x0, &y0, &x1, &y1);
        if (obstacle(o->x, y1) || o->y > o->fy + 150) { o->st = 4; o->tm = 0; }
    }
    if (o->st != 3 && o->bd > 0) o->bd = 0;                                                /* 4801 */
    if (o->st == 4) {                                                                      /* 4802-4804 */
        if (o->tm < 50) o->tm++;
        else { o->y -= 3; ens_set(o, "df_wind"); }
        if (o->y <= o->fy) { o->y = (float)(int)o->fy; o->st = 0; o->tm = 0; }
    }
}

/* ---- StarFlounder (EnemyStarFlounder_1020, events 5032-5044) */
void flounder_spawn(obj_t *o) { o->A = 4; o->vx = -1; o->dir = 16; o->st = 0; o->fx = o->x; o->fy = o->y; o->tm = 0; o->bd = 0; ens_set(o, "sf_swim"); }
void flounder_update(obj_t *o, player_t *p) {
    if (DEFEATED(o)) { o->st = 5; ens_set(o, "sf_dead"); return; }                          /* 5042 / 5043 */
    if (o->st == 0 && obj_near_view(o, 50)) {                                              /* 5033 */
        ens_set(o, "sf_swim");
        o->x = (float)(int)(o->x + o->vx); o->y = (float)(int)(o->fy + 8.0 * c_sin(o->tm)); o->tm = (o->tm + 2) % 360; o->bd = 0;
    }
    /* 5034 / 5035: only touched from the side opposite the swim direction */
    int sx0 = (int)floor(p->x) - 7, sx1 = (int)floor(p->x) + 8, sy0 = (int)floor(MVY(p)) - 34, sy1 = (int)floor(MVY(p)) + 12;
    if (obj_rect_hit(o, sx0, sy0, sx1, sy1) && ((o->dir == 0 && p->x < o->x) || (o->dir == 16 && o->x < p->x))) o->bd = 5;
    float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);
    int turn = 0;
    if (o->st == 0 && o->vx < 0 && ((obj_overlaps_bg(o) && obstacle(x0, o->y)) || o->x < o->fx - 200)) turn = 1;   /* 5036 / 5037 */
    else if (o->st == 0 && o->vx > 0 && ((obj_overlaps_bg(o) && obstacle(x1, o->y)) || o->x > o->fx + 200)) { turn = 1; o->bd = 0; }   /* 5038 / 5039 */
    if (turn) { o->vx = -o->vx; o->dir = o->dir == 16 ? 0 : 16; o->st = 1; ens_set(o, "sf_turn"); anim_restart(&o->an); }
    if (o->st == 1 && (!anim_playing(o, "sf_turn") || !obj_near_view(o, 50))) o->st = 0;   /* 5040 / 5041 */
}

/* ---- Cacteon (EnemyCacteon_1013, events 4900/4901, 5003-5031). G, H float X / Y in fx, fy; I walk timer = tm */
#define CA_FEET 0x10
static int ca_mask_hits_level(obj_t *o, const char *mask) {       /* force_direction(mask) then overlaps background / solids / obstacles */
    animator_t keep = o->an;
    int a = ens_anim(mask), hit = 0;
    if (a < 0) return obj_overlaps_bg(o);
    anim_set(&o->an, &G.ens, a);
    hit = obj_overlaps_bg(o);
    if (!hit) {
        const frame_t *f = anim_frame(&o->an);
        int ox = (int)floorf(o->x) - f->hx, oy = (int)floorf(o->y) - f->hy;
        for (int i = 0; i < G.nobjs && !hit; i++) {
            const obj_t *s = &G.o[i];
            if (!s->alive || s == o) continue;
            if (!(obj_in_group(s->type, OBJGRP_SOLID) || obj_in_group(s->type, OBJGRP_OBSTACLE_ALL))) continue;
            float bx0, by0, bx1, by1; ibox(s, &bx0, &by0, &bx1, &by1);
            if (ox < bx1 && bx0 < ox + f->w && oy < by1 && by0 < oy + f->h) hit = 1;
        }
    }
    o->an = keep;
    return hit;
}
void cacteon_spawn(obj_t *o) { o->A = 5; o->dir = 0; o->st = 0; o->vx = 0; o->vy = 0; o->fx = o->x; o->fy = o->y; o->tm = 0; o->bd = 0; ens_set(o, "ca_idle"); }
void cacteon_update(obj_t *o, player_t *p) {
    if (DEFEATED(o)) { o->st = 4; ens_set(o, "ca_dead"); return; }                          /* 5029 / 5030 */
    if (o->st == 1 && obj_near_view(o, 50) && !anim_playing(o, "ca_land")) {                /* 4900 / 4901 */
        int px = (int)floor(p->x);
        o->vx = o->x < px ? 1.6f : -1.6f; o->st = 2; ens_set(o, "ca_walk");
    }
    o->fx += o->vx; o->fy += o->vy; o->x = (float)(int)o->fx; o->y = (float)(int)o->fy; o->bd = 0;   /* 5005 */
    {   /* 5006: body mask (image 900) on Lilac's character sensor */
        animator_t keep = o->an; int a = ens_anim("ca_body");
        if (a >= 0) { anim_set(&o->an, &G.ens, a); int sx0 = (int)floor(p->x) - 7, sx1 = sx0 + 15, sy0 = (int)floor(MVY(p)) - 34, sy1 = sy0 + 46;
            if (obj_rect_hit(o, sx0, sy0, sx1, sy1) && p->state != ST_DEAD) o->bd = 5; }
        o->an = keep;
    }
    o->fl &= ~CA_FEET;                                                                     /* 5007-5011 */
    if (ca_mask_hits_level(o, "ca_feet")) o->fl |= CA_FEET;
    if (!(o->fl & CA_FEET)) o->vy = fminf(o->vy + 0.25f, 12.f);                            /* 5012 */
    {   /* 5016 wall bounce: probes at the box bottom - 8, right edge + 4 / left edge + 4 */
        float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);
        if (obj_overlaps_bg(o) && ((o->vx > 0 && obstacle(x1 + 4, y1 - 8)) || (o->vx < 0 && obstacle(x0 + 4, y1 - 8)))) o->vx = -o->vx;
    }
    for (int i = 0; i < G.nobjs; i++) {                                                    /* 5017-5019 springs */
        obj_t *s = &G.o[i];
        if (!s->alive || (s->type != OT_SPRING_UL && s->type != OT_SPRING_UR)) continue;
        float bx0, by0, bx1, by1; ibox(s, &bx0, &by0, &bx1, &by1);
        float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);
        if (!(x0 < bx1 && bx0 < x1 && y0 < by1 && by0 < y1)) continue;
        if (obj_near_view(o, 0)) snd_play_orig(5, 220, 128);
        if (o->vy >= 0) { o->st = 0; o->vy = -8; o->vx = s->type == OT_SPRING_UL ? -8 : 8; anim_restart(&s->an); }
    }
    if (o->st == 0 && (o->fl & CA_FEET) && o->vy >= 0 && !anim_playing(o, "ca_walk")) {   /* 5020 */
        ens_set(o, "ca_land"); anim_restart(&o->an); o->vx = 0; o->st = 1;
    }
    if (o->st < 4 && (o->fl & CA_FEET) && o->vy >= 0) {                                    /* 5021: 1 px up per frame */
        o->vy = 0; o->fy -= 1; o->y = (float)(int)o->fy;
    }
    if (o->vx > 0) o->dir = 0; else if (o->vx < 0) o->dir = 16;                            /* 5022 / 5023 */
    if (o->st == 2 && o->vy < 0.5f) o->tm++;                                               /* 5024 */
    if (o->st == 2 && o->tm > 60) { o->tm = 0; o->st = 3; }                                /* 5025 */
    if (o->st == 3) { ens_set(o, "ca_jump"); o->vx = 0; if (o->an.frame == 5) { o->vy = -6; o->st = 0; } }   /* 5026-5028 */
    if (o->st == 2) ens_set(o, "ca_walk");
}

/* ---- SporePod (EnemySporePod_1012, events 4988-4998) + EnemySporeProjectile_1022 (4999-5002, col handlers).
 * Pod: st = D, vy = E, fx = rest Y (H), fy = float Y (G), tm = spores left (F), fl bit 2 = touched the limiter */
void sporepod_spawn(obj_t *o) { o->A = 8; o->st = 0; o->vy = 0; o->fx = o->y; o->fy = o->y; o->tm = 0; o->bd = 0; ens_set(o, "sp_rest"); }
static void spore_create(const obj_t *pod, int F) {
    obj_t *s = obj_alloc();
    if (!s) return;
    const frame_t *f = pod->an.b ? anim_frame(&pod->an) : NULL;
    float ay = f ? floorf(pod->y) - f->hy + 13 : pod->y - 17;          /* action point (23, 13) */
    double t1 = 90.0 * F + 45, t2 = 90.0 * (F - 1) + 45;
    s->type = OT_SPORE; s->alive = 1; s->born = G.frame;
    s->fx = (float)(pod->x + 4 * c_cos(t1)); s->fy = (float)(ay - 4 * c_sin(t1));
    s->vx = (float)(2 * c_cos(t2)); s->vy = (float)(-3 - c_sin(t2));
    s->x = floorf(s->fx); s->y = floorf(s->fy);
    int a = ens_anim("spore"); if (a >= 0) anim_set(&s->an, &G.ens, a);
}
void sporepod_update(obj_t *o, player_t *p) {
    (void)p;
    if (DEFEATED(o)) { o->st = 4; ens_set(o, "sp_dead"); return; }                          /* 4988 / 4996 */
    if (o->st == 0 && obj_near_view(o, 100) && !anim_playing(o, "sp_close")) {              /* 4989 */
        o->vy = -8; o->st = 1; o->fy = o->fx = o->y; o->fl &= ~0x06;
    }
    if (o->st == 1) {                                                                      /* 4990 / 4992 / 4993 */
        o->y = (float)(int)o->fy; o->fy += o->vy; o->vy = fminf(o->vy + 0.2f * ((o->fl & 4) ? 1 : 0), -0.2f);
        for (int i = 0; i < G.nobjs; i++) {
            const obj_t *l = &G.o[i];
            if (!l->alive || l->type != OT_LIMITER) continue;
            if (fabsf(l->x - o->x) < 24 && o->y - 30 <= l->y + 16 && o->y + 26 >= l->y) o->fl |= 4;   /* pod box vs the 32x32 marker (INFERRED size) */
        }
        if (o->vy >= -0.2f) {
            o->tm = 2; o->st = 2; ens_set(o, "sp_open");
            for (int k = 0; k < 3; k++) if (o->tm > 0) { spore_create(o, o->tm); o->tm--; }   /* loop_sporepod x3: 2 spores */
        }
    } else if (o->st == 2) {                                                               /* 4994 / 4995 */
        o->y = (float)(int)o->fy; o->fy += o->vy; o->vy = fminf(o->vy + 0.025f, 2.f);
        if (o->y >= o->fx) { ens_set(o, "sp_close"); anim_restart(&o->an); o->st = 0; o->y = o->fx; }
    } else if (o->st == 0 && !anim_playing(o, "sp_close")) ens_set(o, "sp_rest");
}
/* spore: fx, fy float position (A, B), vx, vy velocity (C, D); fl bit 0 burst, bit 1 sound done */
void spore_update(obj_t *o, player_t *p) {
    if (!(o->fl & 1)) {
        o->x = (float)(int)o->fx; o->y = (float)(int)o->fy; o->fx += o->vx; o->fy += o->vy; o->vy = fminf(o->vy + 0.1f, 4.f);   /* 4999 */
        int burst = obj_overlaps_bg(o);                                                    /* col_23 / col_6 */
        int sx0 = (int)floor(p->x) - 7, sy0 = (int)floor(MVY(p)) - 34;
        if (!burst && p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p) && obj_rect_hit(o, sx0, sy0, sx0 + 15, sy0 + 46)) {
            player_contact(p, 5); burst = 1;                                               /* col_9: R = 1, life B += 5 */
        }
        if (burst) { o->fl |= 1; int a = ens_anim("spore_burst"); if (a >= 0) anim_set(&o->an, &G.ens, a); }
    }
    if ((o->fl & 1) && !(o->fl & 2) && obj_near_view(o, 80)) { snd_play_orig(313, 220, 128); camera_shake(2); o->fl |= 2; }   /* 5000 / 5001 */
    if ((o->fl & 1) && o->an.done) o->alive = 0;                                           /* 5002 */
    if (o->y > G.lvl.h + 64) o->alive = 0;
}

/* ---- Ganseki / every Q20 enemy: crystal absorption (col_3_13 + 1290 + 2546). One free crystal per pass;
 * every overlapping living enemy near the view counts it (S += 1, dropped on death by 2766). */
void enemies_absorb_crystals(void) {
    if (!objects_alive_type(OT_CRYSTAL)) return;
    /* the enemies that can absorb, listed once (session 18 perf: was a full scan per crystal in view; same order, and
     * nothing below changes who qualifies) */
    static short en[512]; int ne = 0, all = 0;
    for (int j = 0; j < G.nobjs; j++) { const obj_t *e = &G.o[j]; if (e->alive && enemy_type(e->type) && !DEFEATED(e) && e->an.b) { if (ne < 512) en[ne++] = (short)j; else { all = 1; break; } } }
    if (!ne) return;
    for (int pass = 0; pass < 8; pass++) {
        int cand[32], nc = 0;
        for (int i = 0; i < G.nobjs && nc < 32; i++) {
            obj_t *c = &G.o[i];
            if (!c->alive || c->type != OT_CRYSTAL || c->st != 0 || !obj_near_view(c, 50)) continue;
            for (int k = 0, n = all ? G.nobjs : ne; k < n; k++) {
                obj_t *e = &G.o[all ? k : en[k]];
                if (!e->alive || !enemy_type(e->type) || DEFEATED(e) || !e->an.b) continue;
                float x0, y0, x1, y1; ibox(c, &x0, &y0, &x1, &y1);
                if (obj_rect_hit(e, (int)x0, (int)y0, (int)x1, (int)y1)) { cand[nc++] = i; break; }
            }
        }
        if (!nc) return;
        obj_t *c = &G.o[cand[nc > 1 ? cross_rand(nc) : (cross_rand(1), 0)]];
        float x0, y0, x1, y1; ibox(c, &x0, &y0, &x1, &y1);
        for (int j = 0; j < G.nobjs; j++) {
            obj_t *e = &G.o[j];
            if (e->alive && enemy_type(e->type) && !DEFEATED(e) && e->an.b && obj_rect_hit(e, (int)x0, (int)y0, (int)x1, (int)y1)) e->absorbed++;
        }
        c->alive = 0; obj_effect(c->x, c->y, "ringsparkle");                              /* 2546 */
    }
}
