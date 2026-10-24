/* Aqua Tunnel (session 12): the stage's own objects and per-frame events (frame indices 16-19, at1..at4).
 * Sources: build/re/spec_at_*.md (static RE), build/re/spec_milla_moves.md section 6 (dig zones / spots / rewards),
 * original traces in build/orig_traces/at (scripts tests/orig_at). Labels in comments: RE / RV / INFERRED. */
#include "../fp.h"
#include <math.h>
#include <string.h>

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
        if (player_on_spot(p, s) && (G.frame % 30) > 14) dig_hint = 1;                      /* 4184 */
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
        if ((G.frame & 1) == 0) o->x -= 1;
    }
    { const obj_t *kept = NULL; int turn[64] = {0};                                          /* 4107: F of the first kept instance (FIRST) */
      for (int k = 0; k < n; k++) { obj_t *o = &G.o[idx[k]]; if (o->st != 0) continue;
          double F = kept ? kept->fx : o->fx; if (o->x < F - 80) { turn[k] = 1; if (!kept) kept = o; } }
      for (int k = 0; k < n; k++) if (turn[k]) G.o[idx[k]].st = 1; }
    for (int k = 0; k < n; k++) {                                                           /* 4108 / 4109 */
        obj_t *o = &G.o[idx[k]];
        if (o->st != 1) continue;
        o->dir = (int)o->av[2] & 31; o->y = (float)(int)(16 * c_sin(o->av[0]) + o->fy); o->av[0] = fmod(o->av[0] + 6, 360);
        if ((G.frame & 1) == 0) o->x += 1;
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
    if (G.frame % 20 == 0)                                                                      /* 4144 */
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
        if (G.frame % 6 == 0) w->dir = (w->dir + (int)w->A) & 31;                              /* 4146 */
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

/* ================================================================ spawn / per frame / per object */
void aqua_spawn(void) {
    dig_hint = 0;
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
            slime_sprite_base(o); break;
        case OT_WHISPMAKER: o->an.b = NULL; if (off) o->alive = 0; break;                       /* invisible */
        case OT_ABRICK: if (off) { o->alive = 0; break; } set_ens(o, "abrick"); o->fl = 0; break;
        case OT_ABRICK2: if (off) { o->alive = 0; break; } set_ens(o, "abrick2"); o->fl = 0; break;
        }
    }
}

void aqua_update(player_t *p) {
    if (!at_act()) return;
    bursts_frame(p);                         /* 4134 (trigger block), col_8_16, 4130-4133 */
    bricks_frame(p);                         /* 4070-4077 */
    fireflies_frame();                       /* 4101-4113 */
    slimes_frame(p);                         /* 4114-4129 */
    whisps_frame();                          /* 4144-4149 */
    dig_events(p);
}

int aqua_obj_update(obj_t *o, player_t *p) {
    (void)p;
    switch (o->type) {
    case OT_DIGSPOT: case OT_FIREFLY: case OT_SLIME: case OT_WHISPMAKER: case OT_ABRICK: case OT_ABRICK2:
    case OT_SLIMEBURST: case OT_WHISP: case OT_ABPIECE: return 1;    /* handled by the per-frame passes above */
    case OT_MON_KEYSTONE: case OT_MON_WOOD: aqua_monitor_fall(o); return 0;   /* the monitor code still runs */
    }
    return 0;
}
