/* Aqua Tunnel (session 12): the stage's own objects and per-frame events (frame indices 16-19, at1..at4).
 * Sources: build/re/spec_at_*.md (static RE), build/re/spec_milla_moves.md section 6 (dig zones / spots / rewards),
 * original traces in build/orig_traces/at (scripts tests/orig_at). Labels in comments: RE / RV / INFERRED. */
#include "../fp.h"
#include <math.h>
#include <string.h>

static void at2_intro(player_t *p);
/* GV[3] (event 2774): a global frame counter the original never resets; traces set its phase (host FP_AT_GT) */
int aqua_gt_off;
static int GT(void) { return G.frame + aqua_gt_off; }
static int at_act(void) { return !strncmp(G.lvdir, "at", 2) ? G.lvdir[2] - '0' : 0; }
int aqua_level(void) { return at_act(); }

static void set_ens(obj_t *o, const char *name) {
    int a = ens_anim(name);
    if (a >= 0) anim_set(&o->an, &G.ens, a); else o->an.b = NULL;
}

/* ================================================================ Milla's dig spots (AT1, spec_milla_moves 6) */
/* 4171-4182 (frame 17): MILLA.D = 1 while PLAYER (Player_326 at int X, int Y = feet - 14) is inside one of these boxes */
static const short DIGZ1[12][4] = {
    { 6939, 7302, 102, 185 }, { 1479, 1705, 594, 671 }, { 9450, 9654, 181, 262 }, { 3214, 3315, 838, 904 },
    { 8866, 9270, 715, 794 }, { 8197, 8418, 442, 507 }, { 9247, 10206, 611, 729 }, { 10607, 10744, 656, 711 },
    { 10523, 10563, 659, 713 }, { 10413, 11262, 1052, 1154 }, { 11671, 12410, 822, 903 }, { 10388, 10482, 157, 223 } };

static int player_px(const player_t *p) { return (int)floor(p->x); }
static int player_py(const player_t *p) { return (int)floor(p->y - ORIG_REF_DY); }

/* Player_326 (27-px disc at int X, int Y) against the 32x32 spot (hotspot 0,0): box test of the disc's bounding box
 * (the original tests the masks; INFERRED equivalent for a box-shaped spot) */
static int player_on_spot(const player_t *p, const obj_t *s) {
    int px = player_px(p), py = player_py(p);
    return px + 13 >= s->x && px - 13 < s->x + 32 && py + 13 >= s->y && py - 13 < s->y + 32;
}

static void dig_debris(float x, float y) {
    int a = ens_anim("debris");
    obj_t *d = obj_alloc();
    if (!d || a < 0) return;
    int dir = 4 + cross_rand(9);                                   /* pick_random {4..12} (RE) */
    double sp = (30 + ((cross_rand_raw(0x8000) * 5) >> 13)) * 0.125;   /* speed 30-34 (RE); BallMovement px = speed / 8 (INFERRED) */
    double r = dir * 11.25 / 57.29577951308232;
    d->type = OT_DEBRIS; d->alive = 1; d->x = x; d->y = y; anim_set(&d->an, &G.ens, a); d->an.frame = 5;   /* animation 5 (RE) */
    d->vx = (float)(sp * cos(r)); d->vy = (float)(-sp * sin(r));
}

/* a popped-up monitor (reward 4190 / 4194: B = -4, INFERRED as its fall speed; lands on solid ground) */
static void spawn_monitor(int type, const char *anim, float x, float y) {
    obj_t *m = obj_alloc();
    if (!m) return;
    m->type = type; m->alive = 1; m->hp = 1; m->x = m->ox = x; m->y = m->oy = y; m->layer = 2;
    set_ens(m, anim);
    m->vy = -4; m->st = 1;                                         /* st 1: falling (aqua_monitor_fall) */
}

void aqua_monitor_fall(obj_t *m) {
    if (m->st != 1) return;
    m->vy += 0.25f; if (m->vy > 8) m->vy = 8;                       /* gravity INFERRED (spec: loop_monitorsunstuckfromground) */
    float ny = m->y + m->vy;
    if (m->vy > 0 && px_solid(&G.lvl, (int)m->x + 16, (int)(ny + 32))) {
        while (px_solid(&G.lvl, (int)m->x + 16, (int)(ny + 31)) && ny > m->y - 32) ny -= 1;
        m->y = floorf(ny); m->vy = 0; m->st = 0; return;
    }
    m->y = ny;
}

static void dig_reward(obj_t *s, player_t *p) {
    int A = (int)s->A;
    if (A == 3) return;                                            /* no reward event for spot 3 (RE by absence) */
    s->alive = 0; snd_play_orig(306, 220, 128);
    static const signed char DX[4] = { 14, 10, 18, 22 };
    for (int k = 0; k < 4; k++) dig_debris(s->x + DX[k], s->y + 24);
    float dx = (float)(floor(p->x) + 12 * (p->face ? 1 : -1)), dy = (float)floor(p->y - ORIG_REF_DY);   /* DIG object (2255/2256) */
    switch (A) {
    case 0:                                                        /* 4190: keystone monitor pops up + 2 crystals */
        spawn_monitor(OT_MON_KEYSTONE, "mon_key", s->x + 16, s->y + 16);          /* at spot + (16, 16) (RE) */
        for (int i = 0; i < 2; i++) obj_drop_crystal(dx, dy, i);
        break;
    case 1: case 2: case 5:                                        /* 4191 / 4193: 5 crystals (loop_dig_crystals) */
        for (int i = 0; i < 5; i++) obj_drop_crystal(dx, dy, i);
        break;
    case 4: {                                                      /* 4192: a petal (A = 2: thrown, INFERRED) */
        obj_throw_petal(s->x + 16, s->y);
        break;
    }
    case 6:                                                        /* 4194: wood shield monitor pops up */
        spawn_monitor(OT_MON_WOOD, "mon_wood", s->x + 16, s->y + 16);            /* position as 4190 (INFERRED) */
        break;
    }
}

static int dig_hint;                                               /* 4184: blinking "down + attack" at the HUD panel */
int aqua_dig_hint(void) { return dig_hint; }

/* before the movement block: 3313 clears MILLA.D, 4171-4182 set it inside a zone */
void aqua_pre(player_t *p) {
    at2_intro(p);
    if (at_act() != 1 || !chr_is_milla(G.character)) return;
    p->mD = 0;
    int px = player_px(p), py = player_py(p);
    for (int i = 0; i < 12; i++)
        if (px >= DIGZ1[i][0] && px <= DIGZ1[i][1] && py >= DIGZ1[i][2] && py <= DIGZ1[i][3]) { p->mD = 1; break; }
}

/* 4183-4194 (after the state groups in the port; the original's order is INFERRED, spec U10) */
static void dig_events(player_t *p) {
    dig_hint = 0;
    if (!chr_is_milla(G.character)) return;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *s = &G.o[i];
        if (!s->alive || s->type != OT_DIGSPOT) continue;
        if (player_on_spot(p, s) && (GT() % 30) > 14) dig_hint = 1;                      /* 4184 */
        if (!(s->fl & 1) && p->state == ST_DIG && p->mfl1 && player_on_spot(p, s)) s->fl |= 1;   /* 4185 */
        if (s->fl & 1) dig_reward(s, p);                                                     /* 4189-4194 */
    }
}


/* ================================================================ shared helpers (RE conventions, spec_at_enemies) */
#define DEFEATED(o) ((o)->fl & 0x40)
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / 57.29577951308232); }
static void ibox(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = o->x; *y0 = o->y; *x1 = o->x + 32; *y1 = o->y + 32; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - (o->flip ? f->w - f->hx : f->hx); *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int box_ov(float a0, float b0, float a1, float b1, float c0, float d0, float c1, float d1) { return a0 < c1 && c0 < a1 && b0 < d1 && d0 < b1; }
/* force_animation keeping the speed counter (RE Active::force_animation) */
static void force_anim(obj_t *o, const char *name) {
    int a = ens_anim(name);
    if (a < 0) return;
    if (o->an.b != &G.ens || o->an.anim != a) { int acc = o->an.acc; anim_set(&o->an, &G.ens, a); o->an.acc = acc; }
}
static int anim_named(const obj_t *o, const char *name) { int a = ens_anim(name); return a >= 0 && o->an.b == &G.ens && o->an.anim == a; }
static int box_sensor(int s, float x0, float y0, float x1, float y1) {   /* a player sensor mask overlapping a box */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) {
        int y = oy + j;
        if (b[j] < a[j] || y < y0 || y >= y1) continue;
        if (ox + b[j] >= x0 && ox + a[j] < x1) return 1;
    }
    return 0;
}
/* PlayerCharacterSensor_375 (direction 0): anim 0 = x -7..8, y -34..12 around (int X, int Y); anim 1 (small) y -13..14 */
static void char_rect(const player_t *p, float *x0, float *y0, float *x1, float *y1) {
    int X = (int)floor(p->x), Y = (int)floor(p->y - ORIG_REF_DY);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}

/* ================================================================ EnemyFirefly_749 (4101-4113) */
/* fields: st = D (0 left, 1 right, 5 defeated), fx/fy = F/G home, av[0] = E bob phase, av[1] = I light timer,
 * av[2] = H light phase, bd = B contact damage, tm = 1 after the first-frame spread (G != 0) */
static void ff_sprite(obj_t *o) {
    if (DEFEATED(o)) { force_anim(o, o->dir >= 16 ? "ff_dead_l" : "ff_dead"); return; }
    static const char *N[] = { "ff0", "ff1", "ff2", "ff16", "ff17", "ff18" };
    int d = o->dir & 31, k = d >= 16 ? 3 + (d - 16 > 2 ? 2 : d - 16) : (d > 2 ? 2 : d);
    force_anim(o, N[k]);
}
static void fireflies_frame(void) {
    int idx[64], n = 0;
    for (int i = G.nobjs - 1; i >= 0 && n < 64; i--) if (G.o[i].alive && G.o[i].type == OT_FIREFLY) idx[n++] = i;   /* selection order: last created first */
    for (int k = 0; k < n; k++) {                                                           /* 4101-4103 */
        obj_t *o = &G.o[idx[k]];
        if (o->st <= 1 && obj_near_view(o, 40)) { o->av[1] = fmod(o->av[1] + 1, 360); o->av[2] = 0; o->bd = 0; }
        if (o->av[1] > 60 && o->av[1] < 120) o->av[2] = 1;
        if (o->av[1] >= 120 && o->av[1] < 240) { o->av[2] = 2; o->bd = 5; }
    }
    for (int k = 0, H = 0; k < n; k++) {                                                    /* 4104: first frame spread */
        obj_t *o = &G.o[idx[k]];
        if (o->tm) continue;
        o->tm = 1; o->fx = o->x; o->fy = o->y; o->av[2] = H;
        o->x = (float)(int)(80 * c_sin(50.0 * H) + o->x); o->av[0] = 30 * H; H++;
    }
    for (int k = 0; k < n; k++) {                                                           /* 4105 / 4106 */
        obj_t *o = &G.o[idx[k]];
        if (o->st != 0) continue;
        o->dir = (int)(16 + o->av[2]) & 31; o->y = (float)(int)(16 * c_sin(o->av[0]) + o->fy); o->av[0] = fmod(o->av[0] + 6, 360);
        if ((GT() & 1) == 0) o->x -= 1;
    }
    { const obj_t *kept = NULL; int turn[64] = {0};                                          /* 4107: F of the first kept instance (FIRST) */
      for (int k = 0; k < n; k++) { obj_t *o = &G.o[idx[k]]; if (o->st != 0) continue;
          double F = kept ? kept->fx : o->fx; if (o->x < F - 80) { turn[k] = 1; if (!kept) kept = o; } }
      for (int k = 0; k < n; k++) if (turn[k]) G.o[idx[k]].st = 1; }
    for (int k = 0; k < n; k++) {                                                           /* 4108 / 4109 */
        obj_t *o = &G.o[idx[k]];
        if (o->st != 1) continue;
        o->dir = (int)o->av[2] & 31; o->y = (float)(int)(16 * c_sin(o->av[0]) + o->fy); o->av[0] = fmod(o->av[0] + 6, 360);
        if ((GT() & 1) == 0) o->x += 1;
    }
    { const obj_t *kept = NULL; int turn[64] = {0};                                          /* 4110 */
      for (int k = 0; k < n; k++) { obj_t *o = &G.o[idx[k]]; if (o->st != 1) continue;
          double F = kept ? kept->fx : o->fx; if (o->x > F + 80) { turn[k] = 1; if (!kept) kept = o; } }
      for (int k = 0; k < n; k++) if (turn[k]) G.o[idx[k]].st = 0; }
    for (int k = 0; k < n; k++) {                                                           /* 4111 / 4112 */
        obj_t *o = &G.o[idx[k]];
        if (DEFEATED(o)) o->st = 5;
        ff_sprite(o);
    }
}

/* ================================================================ EnemySlime_717 (4114-4129) + EnemySlimeburst_718 (4130-4134, col_8_16) */
/* fields: st = D, fx/fy = F/G, av[0] = E, av[1] = H, av[2] = I, av[3] = J (+-1), av[4] = K, tm = F set */
static void slime_sprite_base(obj_t *o) { force_anim(o, o->dir >= 16 ? "sl_idle_l" : "sl_idle"); }
static int limiter_overlap(const obj_t *o) {
    float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *l = &G.o[i];
        if (!l->alive || l->type != OT_LIMITER) continue;
        if (box_ov(x0, y0, x1, y1, l->x - 21, l->y - 18, l->x - 5, l->y + 14)) return 1;   /* image 698 16x32, hotspot (21,18) */
    }
    return 0;
}
static void spawn_burst(float x, float y) {
    obj_t *b = obj_alloc();
    if (!b) return;
    b->type = OT_SLIMEBURST; b->alive = 1; b->hp = 1; b->x = x; b->y = y; b->layer = 2; b->born = G.frame;
    set_ens(b, "slb"); b->A = 0; b->fl = 0; b->st = 0;
}
static void slimes_frame(player_t *p) {
    int idx[64], n = 0;
    for (int i = G.nobjs - 1; i >= 0 && n < 64; i--) if (G.o[i].alive && G.o[i].type == OT_SLIME) idx[n++] = i;
    for (int k = 0; k < n; k++) {
        obj_t *o = &G.o[idx[k]];
        double *E = &o->av[0], *H = &o->av[1], *I = &o->av[2], *J = &o->av[3], *K = &o->av[4];
        if (o->an.done && o->av[5] == 0) { o->an.acc += 100 - 50; o->av[5] = 1; }              /* RV: a finishing update leaves the counter unchanged */
        if (!o->an.done) o->av[5] = 0;
        if (!o->tm) { o->tm = 1; o->fx = o->x; o->fy = o->y; }                                  /* 4114 */
        int nv = obj_near_view(o, 40);
        if (nv && o->st == 0) { o->x = (float)(int)(20 * c_sin(*E) + o->fx); *E += 2; }        /* 4115 */
        if (nv && o->st == 0 && *K == 0 && *E < 90) *K = 2;                                     /* 4116 */
        if (nv && o->st == 0 && *E >= 180 && limiter_overlap(o)) { *J = -*J; o->dir = (o->dir + 16) & 31; }   /* 4117 */
        if (nv && o->st == 0 && *E >= 180) { o->st = 1; *I = -4; *E = 0; force_anim(o, o->dir >= 16 ? "sl_jump_l" : "sl_jump"); }   /* 4118 */
        if (o->st == 1 && o->an.frame > 5) { o->st = 2; spawn_burst(o->x, o->y + 6); }          /* 4119 */
        if (o->st == 2) { o->x = (float)(int)(o->x + 2 * *J); o->y = (float)(int)(o->fy + *H); *H = fmin(*H + *I, 0); *I += 0.3; }   /* 4120 */
        if (o->st == 2 && *I >= 0 && *H >= 0) { o->st = 3; force_anim(o, o->dir >= 16 ? "sl_land_l" : "sl_land"); anim_restart(&o->an); o->y = (float)(int)o->fy; }   /* 4121 */
        int landing = anim_named(o, o->dir >= 16 ? "sl_land_l" : "sl_land") && !o->an.done;
        if (o->st == 3 && !landing && *K > 0) { o->st = 0; *E = 180; o->fx = o->x; *K -= 1; }   /* 4122 */
        if (o->st == 3 && !landing) { o->st = 0; o->fx = o->x; }                               /* 4123 */
        if (o->st == 0 && !landing && !anim_named(o, o->dir >= 16 ? "sl_jump_l" : "sl_jump")) slime_sprite_base(o);   /* forced anim ends -> base (2.5) */
        else if (o->st == 0 && landing) { }
        if (DEFEATED(o)) { o->st = 4; force_anim(o, "sl_dead"); }                              /* 4127 / 4128 */
        (void)p;
    }
}
static void bursts_frame(player_t *p) {
    float cx0, cy0, cx1, cy1; char_rect(p, &cx0, &cy0, &cx1, &cy1);
    int dead = p->hp <= 0 || p->state == ST_DEAD;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_SLIMEBURST) continue;
        if ((b->fl & 1) && b->an.done) { b->alive = 0; continue; }                           /* 4134 (trigger block, start of the frame) */
        float x0, y0, x1, y1; ibox(b, &x0, &y0, &x1, &y1);
        int ov = !dead && box_ov(cx0, cy0, cx1, cy1, x0, y0, x1, y1);
        if (ov && !(b->fl & 2)) {                                                               /* col_8_16: edge-triggered */
            if (!(b->fl & 1)) b->A = 50;
            if (!p->hurt_pending && !player_invincible(p) && p->invuln <= 0) { player_contact(p, 10); b->A = 999; }
        }
        if (ov) b->fl |= 2; else b->fl &= ~2;
    }
}
static void bursts_timer(void) {                                                                /* 4130-4133 (after the slimes' 4119) */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_SLIMEBURST) continue;
        b->A += 1;                                                                              /* 4130 */
        if (b->A > 40 && !(b->fl & 1)) {                                                         /* 4132 / 4133 */
            if (obj_near_view(b, 50)) snd_play_orig(240, 220, 128);
            force_anim(b, "slb_pop"); anim_restart(&b->an); b->fl |= 1;
        }
    }
}

/* ================================================================ whisps (4144-4149, cosmetic) */
static void whisps_frame(void) {
    int nb = G.nobjs;
    if (GT() % 20 == 0)                                                                      /* 4144 */
        for (int i = 0; i < nb; i++) {
            obj_t *m = &G.o[i];
            if (!m->alive || m->type != OT_WHISPMAKER) continue;
            if (m->x < G.camx - 300 || m->x > G.camx + SCREEN_W + 300 || m->y < G.camy - 300 || m->y > G.camy + SCREEN_H + 300) continue;
            obj_t *w = obj_alloc();
            if (!w) break;
            w->type = OT_WHISP; w->alive = 1; w->layer = 2; set_ens(w, "whisp");
            w->dir = (cross_rand_raw(0x10000) >> 10) & 31;                                   /* (LCG >> 26) & 31 */
            w->A = (cross_rand_raw(0x8000) >> 13 & 2) - 1;                                      /* 4145: +-1 */
            w->x = m->x - 160 + cross_rand(321); w->y = m->y - 120 + cross_rand(241);
            w->av[0] = 0; w->st = 0;
        }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *w = &G.o[i];
        if (!w->alive || w->type != OT_WHISP) continue;
        if (GT() % 6 == 0) w->dir = (w->dir + (int)w->A) & 31;                              /* 4146 */
        double r = w->dir * 11.25 / 57.29577951308232;
        w->x += (float)(1.25 * cos(r)); w->y -= (float)(1.25 * sin(r));                         /* BallMovement speed 10 */
        double B = ++w->av[0];                                                                  /* 4147-4149 */
        w->st = B < 64 ? (int)fmin(4 * B, 255) : (int)fmax(0, fmin(256 - 4 * (B - 64), 255));
        if (B >= 128) w->alive = 0;
    }
}
int aqua_obj_alpha(const obj_t *o) { return o->type == OT_WHISP ? o->st : 255; }

/* ================================================================ BreakableAquaBrick_681 (4070-4077, col_13_16 / col_16_16) */
static void brick_pieces(const obj_t *b, double X, double Y) {
    static const signed char PX[4] = { 8, 24, 8, 24 }, PY[4] = { 8, 8, 24, 24 };
    for (int k = 0; k < 4; k++) {
        obj_t *q = obj_alloc();
        if (!q) return;
        q->type = OT_ABPIECE; q->alive = 1; q->layer = 2; set_ens(q, "abpiece");
        q->x = b->x + PX[k]; q->y = b->y + PY[k];
        q->av[0] = X - 2 + (cross_rand_raw(0x10000) >> 13 & 3); q->av[1] = Y - 4 + (cross_rand_raw(0x10000) >> 13 & 3); q->av[2] = 0.3;
        q->fx = q->x; q->fy = q->y; q->ang = (float)((cross_rand_raw(0x10000) >> 13 & 3) * 90);
    }
    /* the four lower pieces (created without alterables, they jump to x = 0, RE 4.3 / U3) are not drawn */
}
static void bricks_frame(player_t *p) {
    int M = player_orig_m(p);
    double B = p->ground ? p->gsp : p->vx, Gv = p->ground ? 0 : p->vy;
    int Q = p->face > 0 ? 1 : -1, Z = player_body_z(p), dead = p->hp <= 0 || p->state == ST_DEAD;
    int pass = (M >= 5 && M <= 6) || M == 26 || (p->bike && fabs(B) > 5);                      /* 4071 */
    int body = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_ABRICK) continue;
        if (pass) o->fl |= 1; else o->fl &= ~1;                                                 /* 4070 / 4071 */
        if (dead) continue;
        float x0 = o->x, y0 = o->y, x1 = o->x + 32, y1 = o->y + 64;
        int side = Z && p->hitstop <= 0 && (box_sensor(SENS_LEFTBIG, x0, y0, x1, y1) || box_sensor(SENS_RIGHTBIG, x0, y0, x1, y1));   /* 4072 a/b */
        int cyc = 0;
        if (M == 26)                                                                             /* 4072 c: the cyclone's shield */
            for (int k = 0; k < G.nobjs && !cyc; k++) if (G.o[k].alive && G.o[k].type == OT_SHIELD && G.o[k].an.b) { float a0, b0, a1, b1; ibox(&G.o[k], &a0, &b0, &a1, &b1); cyc = box_ov(a0, b0, a1, b1, x0, y0, x1, y1); }
        int cube = player_cube_breaks_rock(o);                                                  /* 4072 d / e */
        int tb = Z && p->hitstop <= 0 && (box_sensor(SENS_BOTTOMBIG, x0, y0, x1, y1) || box_sensor(SENS_TOPBIG, x0, y0, x1, y1));   /* 4073 */
        int bike = p->bike && ((B > 5 && box_sensor(SENS_RIGHTBIG, x0, y0, x1, y1)) || (B < -5 && box_sensor(SENS_LEFTBIG, x0, y0, x1, y1)));   /* col_13_16 / col_16_16 */
        if (!(side || cyc || cube || tb || bike)) continue;
        double X, Y;
        if (side || cyc || cube || bike) { X = fmax(3, fabs(1.5 * B)) * Q; X = fmin(fmax(X, -10), 10); Y = 0.5 * Gv; }
        else { X = fmin(fmax(0.5 * B, -10), 10); Y = Gv; }
        if (side || tb || bike) body = 1;
        o->av[0] = X; o->av[1] = Y; o->fl |= 0x40;
    }
    int snd = 0;
    for (int i = 0; i < G.nobjs; i++) {                                                         /* 4075 */
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_ABRICK || !(o->fl & 0x40)) continue;
        if (!snd) { snd_play_orig(13, 220, 128); snd = 1; }
        obj_t b = *o; o->alive = 0;
        brick_pieces(&b, b.av[0], b.av[1]);
    }
    if (body) player_hitstop(p, 5);
    for (int i = 0; i < G.nobjs; i++) {                                                         /* 4076 / 4077 */
        obj_t *q = &G.o[i];
        if (!q->alive || q->type != OT_ABPIECE) continue;
        q->fx += (float)q->av[0]; q->av[1] += q->av[2]; q->fy += (float)q->av[1];
        q->x = (float)(int)q->fx; q->y = (float)(int)q->fy; q->ang += (float)q->av[0];
        if (q->fy > G.camy + 320) q->alive = 0;
    }
}


/* ================================================================ water jets (spec_at_gimmicks_a 2) */
/* WaterJet_740: 32x64 box at (x - 16, y), not solid; the layer lives in o->hidden == 0 / layer (instance layer; AT2's
 * x 1424/1456 jets sit on layer 2 (port 1) until 4395). Overlap with PLAYER's disc: M12 with B = -5 / +5 (4141/4142) */
static int disc_box(const player_t *p, float x0, float y0, float x1, float y1) {
    int px = (int)p->x, py = (int)floor(p->y - ORIG_REF_DY);
    float cx = fmaxf(x0, fminf((float)px, x1 - 1)), cy = fmaxf(y0, fminf((float)py, y1 - 1));
    return (cx - px) * (cx - px) + (cy - py) * (cy - py) <= 13.5f * 13.5f;
}
static int wh_age;
static void jets_frame(player_t *p) {
    int M = player_orig_m(p), px = (int)p->x, py = (int)floor(p->y - ORIG_REF_DY);
    int gt = GT();
    obj_t *first = NULL;
    for (int i = G.nobjs - 1; i >= 0; i--) {                                                  /* selection order: newest first */
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_WJET) { if (o->an.b) o->an.frame = gt % 16; }                        /* 4136 */
        else if (o->type == OT_WJET_B) { if (o->an.b) o->an.frame = (gt / 2) % 16; }
        if (o->type != OT_WJET || o->layer != 2 || first) continue;
        if (disc_box(p, o->x - 16, o->y, o->x + 16, o->y + 64)) first = o;
    }
    int lilac = !chr_is_carol(G.character) && !chr_is_milla(G.character);
    if (first && M != 9 && !(lilac && M == 21) && p->state != ST_DEAD) {                      /* 4141 / 4142 */
        player_shake(p, px < (int)first->x ? -5 : 5);
        snd_play_orig(14, 220, 128);
    }
    /* WaterJetRight (4137-4140): zone by position, A = spawn x */
    int active = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_WJET_R) continue;
        double A = o->fx;
        if (A < G.camx + 490) o->x = (float)(A + 8 * (gt % 8));                               /* 4137 scroll */
        if (o->y >= py - 15 && o->y <= py + 15 && px - 100 < A && A < px + 40) active = 1;
    }
    static int was;
    if (active && !was && p->cY != 8) { snd_play_orig(14, 220, 128); snd_play_orig(7, 220, 128); }   /* 4138 (once) */
    was = active;
    if (active && M != 9 && p->state != ST_DEAD) {                                            /* 4139 */
        if (chr_is_milla(G.character)) p->mE = 0;
        if (p->state != ST_ROLL) { p->state = ST_ROLL; p->state_t = 0; p->tun_lock = 0; }
        if (p->ground) p->gsp = 15; else p->vx = 15;
        obj_t *w = obj_alloc();                                                               /* EffectsWaterHit at PLAYER (+-16) */
        if (w) { w->type = OT_WATERHIT; w->alive = 1; w->layer = 2; set_ens(w, "waterhit"); anim_restart(&w->an);
                 w->A = cross_rand(33) - 16; w->av[0] = cross_rand(33) - 16; w->x = (float)(int)(p->x + w->A); w->y = (float)(int)(p->y - ORIG_REF_DY + w->av[0]); }
    }
    for (int i = 0; i < G.nobjs; i++) {                                                       /* 4140: follow her, self-destroy */
        obj_t *w = &G.o[i];
        if (!w->alive || w->type != OT_WATERHIT) continue;
        if (!w->an.b || w->an.done) { w->alive = 0; continue; }
        w->x = (float)(int)(p->x + w->A); w->y = (float)(int)(p->y - ORIG_REF_DY + w->av[0]);
    }
    (void)wh_age;
}
/* WaterJetMaker (AT2, 4453-4455): started by the Chomper's defeat (aqua_maker_start) */
static int maker_on;
void aqua_maker_start(void) { maker_on = 1; }
static void maker_frame(void) {
    if (!maker_on) return;
    int nb = G.nobjs;
    for (int i = 0; i < nb; i++) {
        obj_t *m = &G.o[i];
        if (!m->alive || m->type != OT_WJET_MAKER) continue;
        m->A += 16; m->x += 16;                                                                   /* 4453 */
        if (m->A >= 128) {                                                                        /* 4454 */
            for (int k = 0; k < 2; k++) { obj_t *j = obj_alloc(); if (!j) break;
                j->type = OT_WJET_R; j->alive = 1; j->layer = 1; j->x = m->x; j->y = m->y + 32 * k; j->fx = m->x; set_ens(j, "wjetr"); }
            m->A -= 128;
        }
        if (m->x >= 7301) m->alive = 0;                                                           /* 4455 */
    }
}

/* ================================================================ JellySpring_704 (4067-4069) */
/* av[0] = A (0..40), fl bit 1 = pressed; scale x 1 + 0.01A, y 1 - 0.02A about the hotspot (32, 31) */
void aqua_jelly_box(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    double sx = 1 + 0.01 * o->av[0], sy = 1 - 0.02 * o->av[0];
    *x0 = (float)floor(o->x - 32 * sx); *x1 = (float)floor(o->x + 48 * sx); *y0 = (float)floor(o->y - 31 * sy); *y1 = (float)floor(o->y + sy) + 1;
}
static void jelly_frame(player_t *p) {
    int any = 0; obj_t *fire = NULL;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_JELLY) continue;
        float x0, y0, x1, y1; aqua_jelly_box(o, &x0, &y0, &x1, &y1);
        if (p->state != ST_DEAD && box_sensor(SENS_BOTTOMBIG, x0, y0, x1, y1)) { any = 1; o->av[0] = fmin(o->av[0] + 1, 40); o->fl |= 2; }   /* 4067 */
    }
    if (any) return;
    for (int i = G.nobjs - 1; i >= 0; i--) {                                                  /* 4068: first selected (newest) */
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_JELLY) continue;
        if (o->av[0] > 20 && (o->fl & 2)) { if (!fire) fire = o; o->fl &= ~2; }
    }
    if (fire) player_launch(p, 0, 9 + 0.1 * fire->av[0], 212);                                 /* G = -9 - 0.1 A, M40, sound 212 */
    for (int i = 0; i < G.nobjs; i++) {                                                       /* 4069 */
        obj_t *o = &G.o[i];
        if (o->alive && o->type == OT_JELLY && o->av[0] > 0) o->av[0] = fmax(o->av[0] - 2, 0);
    }
}

/* ================================================================ red springs (2997-3004), objects.c calls aqua_spring */
static void spring_press(obj_t *o, const char *idle, const char *pressed) {
    (void)idle; int a = ens_anim(pressed); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); }
}
void aqua_spring(obj_t *o, player_t *p) {
    if (p->state == ST_DEAD || p->state == ST_HURT) return;                                     /* M not 9 / 10 (INFERRED: 10 = hurt variant) */
    float x0, y0, x1, y1; ibox(o, &x0, &y0, &x1, &y1);
    double B = p->ground ? p->gsp : p->vx, Gv = p->ground ? 0 : p->vy;
    switch (o->type) {
    case OT_RSPRING_U:                                                                            /* 2997: G = -18 (port: -17.75 after gravity) */
        if (Gv >= 0 && box_sensor(SENS_BOTTOMBIG, x0, y0, x1, y1)) { player_spring(p, 0, 17.75, y0 - 2 + 0.25f); spring_press(o, "rs_up", "rs_up_b"); }
        break;
    case OT_RSPRING_D:                                                                            /* 3000: G = 18 (+ gravity) */
        if (Gv <= 0 && box_sensor(SENS_TOPBIG, x0, y0, x1, y1)) { player_launch(p, 0, -18.25, 5); spring_press(o, "rs_down", "rs_down_b"); }
        break;
    case OT_RSPRING_R:                                                                            /* 2998: pushes right, B = 14, O = 5 */
        if (B <= 0 && box_sensor(SENS_LEFTBIG, x0, y0, x1, y1)) { player_spring_side(p, 14); spring_press(o, "rs_right", "rs_right_b"); }
        break;
    case OT_RSPRING_L:                                                                            /* 2999: B = -14, O = -5 */
        if (B >= 0 && box_sensor(SENS_RIGHTBIG, x0, y0, x1, y1)) { player_spring_side(p, -14); spring_press(o, "rs_left", "rs_left_b"); }
        break;
    case OT_RSPRING_UR:                                                                           /* 3001 / 3002: (14, -14) (port -13.75) */
        if ((Gv >= 0 && box_sensor(SENS_BOTTOMBIG, x0, y0, x1, y1)) || (!p->ground && B <= 0 && box_sensor(SENS_LEFTBIG, x0, y0, x1, y1))) {
            player_spring(p, 14.0, 13.75, p->ground || Gv >= 0 ? y0 - 2 : p->y); spring_press(o, "rs_ur", "rs_ur_b");
        }
        break;
    }
    if (o->an.done) { int a = ens_anim(o->type == OT_RSPRING_U ? "rs_up" : o->type == OT_RSPRING_D ? "rs_down" : o->type == OT_RSPRING_R ? "rs_right" : o->type == OT_RSPRING_L ? "rs_left" : "rs_ur"); if (a >= 0) anim_set(&o->an, &G.ens, a); }
}

/* ================================================================ MonitorBomb_535 / MonitorCrate_364 (spec_at_gimmicks_a 5) */
/* bomb: fl bit 8 armed (0x100), bit 7 beep latch (0x80); tm = F fuse. Explosion = OT_BOMBX (MonitorIcon frame 9):
 * area (x - 8, y - 4) .. (x + 40, y + 36), rises 1 px/frame, A = age, 32 frames */
static void blast(float x, float y, int age0) {
    obj_t *e = obj_alloc();
    if (e) { e->type = OT_BOMBX; e->alive = 1; e->x = x; e->y = y; e->A = age0; e->an.b = NULL; e->st = 0; e->layer = 2; }
}
void aqua_blast(float x, float y) { blast(x, y, 0); }   /* a MonitorIcon forced to frame 9 (AT4 bubbles 4628 / 4630) */
static void bomb_explode(float x, float y, int age0) {
    blast(x, y, age0);
    obj_drop_crystal(x + 16, y + 16, 0);                                                          /* 3250 / 3251: one crystal up (INFERRED speed) */
}
void aqua_monitor_broken(const obj_t *m) { if (m->type == OT_MON_BOMB) bomb_explode(m->x, m->y, 1); }   /* generic break path (2627: A 1 on its first frame) */
static void bombs_frame(player_t *p) {
    int nb = G.nobjs;
    for (int i = 0; i < nb; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_MON_BOMB) continue;
        if (!(o->fl & 0x100)) {                                                                   /* 3245 / 4056 */
            int arm = p->state != ST_DEAD && (box_sensor(SENS_LEFTBIG, o->x, o->y, o->x + 32, o->y + 32) || box_sensor(SENS_RIGHTBIG, o->x, o->y, o->x + 32, o->y + 32) ||
                                              box_sensor(SENS_TOPBIG, o->x, o->y, o->x + 32, o->y + 32) || box_sensor(SENS_BOTTOMBIG, o->x, o->y, o->x + 32, o->y + 32));
            for (int k = 0; k < G.nobjs && !arm; k++) { const obj_t *e = &G.o[k]; if (e->alive && enemy_type(e->type) && e->an.b) { float a0, b0, a1, b1; ibox(e, &a0, &b0, &a1, &b1); arm = box_ov(a0, b0, a1, b1, o->x, o->y, o->x + 32, o->y + 32); } }
            if (arm) { o->fl |= 0x100; o->tm = 0; set_ens(o, "mon_bomb_arm"); snd_play_orig(172, 220, 128); }
            continue;
        }
        if (o->an.frame == 1 && !(o->fl & 0x80)) { o->fl |= 0x80; snd_play_orig(173, 200, 128); }   /* 3246 / 3247 */
        if (o->an.frame >= 2) o->fl &= ~0x80;
        o->tm += 1; o->av[7] = o->tm + 10;                                                         /* 3248: force_speed(F + 10) */
        if (o->tm >= 120) { obj_t m = *o; o->alive = 0; bomb_explode(m.x, m.y, 0); }               /* 3249 / 3250 */
    }
    for (int i = 0; i < G.nobjs; i++) {                                                           /* explosions: 2630-2672, 2760, 4074, 3243, col_9_13 */
        obj_t *e = &G.o[i];
        if (!e->alive || e->type != OT_BOMBX) continue;
        if (e->st == 0) { e->st = 1; snd_play_orig(38, 230, 128); camera_shake(3); }              /* 2669 */
        float x0 = e->x - 8, y0 = e->y - 4, x1 = e->x + 40, y1 = e->y + 36;
        if (((int)e->A % 4) == 1) obj_effect(e->x, e->y, "explosion06");                           /* 2670 */
        { float cx0 = p->x - 7, cx1 = p->x + 9, cy1 = p->y - ORIG_REF_DY + 13, cy0 = p->y - ORIG_REF_DY - 34;   /* col_9_13 (OTHERS.D = 0) */
          if (p->state != ST_DEAD && p->state != ST_HURT && !p->invinc && !p->hurt_pending && box_ov(cx0, cy0, cx1, cy1, x0, y0, x1, y1)) player_contact(p, 20); }
        if (!(e->fl & 2)) {                                                                        /* 2760 (once): enemies A -= 15 */
            e->fl |= 2;
            for (int k = 0; k < G.nobjs; k++) { obj_t *q = &G.o[k];
                if (!q->alive || !enemy_type(q->type) || (q->fl & 0x20) || !q->an.b) continue;
                float a0, b0, a1, b1; ibox(q, &a0, &b0, &a1, &b1);
                if (box_ov(a0, b0, a1, b1, x0, y0, x1, y1)) { q->c = 5; q->ky = -3; q->kx = cross_rand(5) - 2; snd_play_orig(24, 220, 128); q->A -= 15; q->inv = 1; } }
        }
        for (int k = 0; k < G.nobjs; k++) { obj_t *q = &G.o[k];                                    /* 4074 bricks; 3243 monitors after 10 frames */
            if (!q->alive) continue;
            if (q->type == OT_ABRICK && box_ov(q->x, q->y, q->x + 32, q->y + 64, x0, y0, x1, y1)) { q->fl |= 0x40; q->av[1] = -3; q->av[0] = cross_rand(5) - 2; p->hitstop = 5; }
            if (e->A > 10 && q != e && (q->type == OT_MONITOR || q->type == OT_MON_RINGS || q->type == OT_MON_1UP || q->type == OT_MON_INVINC || q->type == OT_MON_KEYSTONE ||
                (q->type >= OT_MON_WOOD && q->type <= OT_MON_WATER) || q->type == OT_MON_BOMB || q->type == OT_MON_CRATE) &&
                box_ov(q->x, q->y, q->x + 32, q->y + 32, x0, y0, x1, y1)) q->fl |= 0x200;   /* broken by the generic path in objects.c */
        }
        e->A += 1; if (e->A < 32) e->y -= 1;                                                       /* 2630 / 2631 */
        if (e->A >= 32) e->alive = 0;                                                              /* 2672 */
    }
}


/* ================================================================ stage flow (spec_at_flow) */
/* 3.1 normal camera bounds: C = 0, E = sign.x + 213 (AT2: the frame width while g4 < 10; after the Chomper C = 5931,
 * E = width + 50) */
void aqua_camera_bounds(void) {
    switch (at_act()) {
    case 1: G.cam_nE = 12480 + 213; break;
    case 2: if (G.at_g4 >= 10) { G.cam_nC = 6144 - 213; G.cam_nE = (float)G.lvl.w + 50; } else G.cam_nE = (float)G.lvl.w; break;
    case 3: G.cam_nE = 856 + 213; break;
    case 4: G.cam_nE = 7424 + 213; break;
    }
}
static int cam_locked;
static void boss_camera(player_t *p) {                          /* 3.2: 4198/4199, 4408/4409, 4599 */
    int px = (int)p->x, py = (int)floor(p->y - ORIG_REF_DY);
    switch (at_act()) {
    case 1:
        if (!cam_locked && px > 11704 && py < 903) { cam_locked = 1; camera_boss_bounds(11368, 600, 12479, 903); }
        else if (cam_locked && px < 11704 - 240 && py < 903) { cam_locked = 0; camera_normal_bounds(); G.cam_m2 = 1; }
        break;
    case 2:
        if (G.at_g4 >= 10) break;
        if (!cam_locked && px > 6032 && py > 976) { cam_locked = 1; camera_boss_bounds(6032, 976, 6863, 1231); }
        else if (cam_locked && py < 976) { cam_locked = 0; camera_normal_bounds(); G.cam_m2 = 1; }
        break;
    case 4:
        if (!cam_locked && G.at_g4 < 10 && px > 6464) { cam_locked = 1; camera_boss_bounds(6464, 8, 7039, 319); }
        break;
    }
}
void aqua_release_camera(void) { cam_locked = 0; camera_normal_bounds(); G.cam_m2 = 1; }
void aqua_release_camera_keep(void) { camera_normal_bounds(); G.cam_m2 = 1; }   /* worm head landing: CAM.M 2, U 0, the trigger flag stays on */
int aqua_cam_locked(void) { return cam_locked; }   /* 4451 (Chomper defeat) */

static void checkpoint(int g4, float x, float y) {              /* g4 / g5 / g6 + loop_cp_16 (restart values) */
    G.at_g4 = g4; G.at_g5 = x; G.at_g6 = y;
    G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
}
static void checkpoints(player_t *p) {                          /* 6.1 */
    double X = p->x;
    switch (at_act()) {
    case 1:
        if (X > 7540 && X < 7640 && G.at_g4 < 2) checkpoint(2, 7600, 1100);       /* 4196 */
        if (X > 11250 && X < 11350 && G.at_g4 < 3) checkpoint(3, 11300, 988);     /* 4197 */
        break;
    case 2:
        if (X > 5640 && X < 5740 && G.at_g4 < 3) checkpoint(3, 5720, 270);        /* 4405 (g1 != 1) */
        break;
    case 4:
        if (X > 3264 && X < 3364 && G.at_g4 < 2) checkpoint(2, 3264, 200);       /* 4598 */
        break;
    }
}
/* AT2 intro (6.2, group 0x22134): the left corridor's current carries her as a ball until 4395 */
static int intro_on;
static void at2_intro(player_t *p) {
    if (at_act() != 2 || !intro_on) return;
    double X = p->x, Y = p->y - ORIG_REF_DY;
    if (G.at_g4 <= 1 && X < 1500) {                                                /* 4393 / 4394 */
        if (p->state != ST_ROLL) { p->state = ST_ROLL; p->state_t = 0; }
        if (p->ground) p->gsp = 15; else p->vy = 15 - 10 * (p->water ? 1 : 0);
        p->in_lock = 1;
    }
    if (G.at_g4 == 1 && p->water && X > 1240 && Y > 1480) {                        /* 4395 */
        p->in_lock = 0; intro_on = 0;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_WJET) G.o[i].layer = 2;
        G.at_g4 = 2; G.at_g5 = 1536; G.at_g6 = 1120;
    }
}
static void kill_items_in(float x0, float x1, float y0, float y1, int jelly) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->x < x0 || o->x > x1 || o->y < y0 || o->y > y1) continue;
        if (o->type == OT_CRYSTAL || (jelly && (o->type == OT_JELLY || o->type == OT_PETAL))) o->alive = 0;
    }
}
static void flow_spawn(void) {
    cam_locked = 0; intro_on = at_act() == 2;
    if (at_act() == 1) {                                                           /* 4318 / 4319: crystal sets per character */
        if (!chr_is_milla(G.character)) kill_items_in(7579, 7788, 732, 1129, 0);
        else kill_items_in(7790, 8050, 929, 1133, 0);
    }
    if (at_act() == 2) kill_items_in(5260, 5467, 379, 814, 1);                      /* 4484 / 4485 (g1 != 1) */
}

/* ================================================================ spawn / per frame / per object */
void aqua_spawn(void) {
    dig_hint = 0; maker_on = 0;
    if (at_act()) flow_spawn();
    if (!at_act()) return;
    /* dig spots: A = index in reverse creation order (spec 6.2, INFERRED order) */
    int n = 0;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_DIGSPOT) n++;
    for (int i = 0, k = 0; i < G.nobjs; i++) {
        obj_t *s = &G.o[i];
        if (!s->alive || s->type != OT_DIGSPOT) continue;
        s->A = n - 1 - k++; s->fl = 0; set_ens(s, "digspot");
    }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int off = o->x < 0 || o->x >= G.lvl.w || o->y < 0 || o->y >= G.lvl.h;                  /* off-map templates */
        switch (o->type) {
        case OT_FIREFLY:                                                                         /* ctor: anim 0, dir 16, A 3, E 170, I 40 */
            if (off) { o->alive = 0; break; }
            o->A = 3; o->dir = 16; o->st = 0; o->tm = 0; o->bd = 0; memset(o->av, 0, sizeof o->av); o->av[0] = 170; o->av[1] = 40;
            ff_sprite(o); break;
        case OT_SLIME:                                                                           /* ctor: anim 0, dir 16, A 3, E 170, J -1, K 2 */
            if (off) { o->alive = 0; break; }
            o->A = 3; o->dir = 16; o->st = 0; o->tm = 0; o->bd = 0; memset(o->av, 0, sizeof o->av); o->av[0] = 170; o->av[3] = -1; o->av[4] = 2;
            slime_sprite_base(o); o->an.acc = 50; break;                                      /* RV a1_slime: counter 50 at the act start */
        case OT_WHISPMAKER: o->an.b = NULL; if (off) o->alive = 0; break;                       /* invisible */
        case OT_ABRICK: if (off) { o->alive = 0; break; } set_ens(o, "abrick"); o->fl = 0; break;
        case OT_ABRICK2: if (off) { o->alive = 0; break; } set_ens(o, "abrick2"); o->fl = 0; break;
        case OT_WJET: case OT_WJET_R: {                                                          /* lvobj anim = instance layer (mapped) */
            if (off) { o->alive = 0; break; }
            set_ens(o, o->type == OT_WJET ? "wjet" : "wjetr"); o->fx = o->x; break; }
        case OT_WJET_B: if (off) { o->alive = 0; break; } set_ens(o, "wjetb"); break;
        case OT_WJET_MAKER: if (off) { o->alive = 0; break; } set_ens(o, "wjetmk"); o->A = 0; o->layer = 1; break;
        case OT_JELLY: if (off) { o->alive = 0; break; } set_ens(o, "jelly"); o->av[0] = 0; o->fl = 0; break;
        case OT_RSPRING_U: set_ens(o, "rs_up"); if (off) o->alive = 0; break;
        case OT_RSPRING_D: set_ens(o, "rs_down"); if (off) o->alive = 0; break;
        case OT_RSPRING_L: set_ens(o, "rs_left"); if (off) o->alive = 0; break;
        case OT_RSPRING_R: set_ens(o, "rs_right"); if (off) o->alive = 0; break;
        case OT_RSPRING_UR: set_ens(o, "rs_ur"); if (off) o->alive = 0; break;
        case OT_MON_BOMB: set_ens(o, "mon_bomb"); o->fl = 0; o->tm = 0; if (off) o->alive = 0; break;
        case OT_MON_CRATE: set_ens(o, "mon_crate"); if (off) o->alive = 0; break;
        }
    }
    aqua_gim_spawn();
    aqua_boss_spawn();
}

void aqua_update(player_t *p) {
    if (!at_act()) return;
    bursts_frame(p);                         /* 4134 (trigger block), col_8_16, 4130-4133 */
    bombs_frame(p);                          /* 3245-3251 bombs, 2630-2672 / 2760 / 4074 / 3243 / col_9_13 explosions */
    jelly_frame(p);                          /* 4067-4069 (after the platforms) */
    bricks_frame(p);                         /* 4070-4077 */
    aqua_gim_post(p);                        /* 4078-4100 (aqua_gim.c) */
    aqua_boss_update(p);                     /* Iron Worm 4200-4237, Chomper 4410-4463, Chomper Snake 4600-4660 (aqua_boss.c) */
    fireflies_frame();                       /* 4101-4113 */
    slimes_frame(p);                         /* 4114-4129 */
    bursts_timer();                          /* 4130-4133 */
    whisps_frame();                          /* 4144-4149 */
    jets_frame(p);                           /* 4136-4143 (near the end of the frame) */
    checkpoints(p);                          /* 4196 / 4197, 4405, 4598 */
    boss_camera(p);                          /* 4198 / 4199, 4408 / 4409, 4599 */
    maker_frame();                           /* 4453-4455 */
    dig_events(p);
}

int aqua_obj_update(obj_t *o, player_t *p) {
    (void)p;
    if (aqua_gim_obj(o) || aqua_boss_obj(o)) return 1;                 /* aqua_gim.c: run in the platform block */
    switch (o->type) {
    case OT_DIGSPOT: case OT_FIREFLY: case OT_SLIME: case OT_WHISPMAKER: case OT_ABRICK: case OT_ABRICK2:
    case OT_SLIMEBURST: case OT_WHISP: case OT_ABPIECE: case OT_WJET: case OT_WJET_R: case OT_WJET_B: case OT_WJET_MAKER:
    case OT_JELLY: case OT_WATERHIT: case OT_BOMBX: return 1;    /* handled by the per-frame passes above */
    case OT_RSPRING_U: case OT_RSPRING_D: case OT_RSPRING_L: case OT_RSPRING_R: case OT_RSPRING_UR: aqua_spring(o, p); return 1;
    case OT_MON_KEYSTONE: case OT_MON_WOOD: aqua_monitor_fall(o); return 0;   /* the monitor code still runs */
    }
    return 0;
}

/* traces: the original runs the gameplay events through the stage card before a trace's first row (host FP_AT_PRERUN) */
void aqua_prerun(int n) {
    for (int k = 0; k < n; k++) {
        G.frame++;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].an.b) anim_tick(&G.o[i].an, -1);
        fireflies_frame(); slimes_frame(&G.p); bursts_timer(); whisps_frame();
        { int fr = G.frame; G.frame = k + 1 - n; aqua_gim_post(&G.p); G.frame = fr; }   /* the platform block runs during the card (GT continuous up to row 0) */
    }
    G.frame = 0;
}
