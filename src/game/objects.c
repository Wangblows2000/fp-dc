/* Level objects placed from the recovered frame data. Visuals/animations are the
 * original classes' first animation; behaviour is reconstructed (UNVERIFIED unless
 * noted) because the original logic is compiled event code. */
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../fp.h"

static int A_EXPLODE = -1;
/* recovered sequences (tools/import_frame.py) */
static int A_TURR[6] = { -1, -1, -1, -1, -1, -1 };   /* idle, idle_l, shoot, shoot_l, off, off_l */
static int A_GAN[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };   /* Ganseki: pose, walk, dead, curl, wake; even = right, odd = left */
static int A_PELLET = -1, A_EXPLODE2 = -1, A_SPARKLE = -1, A_HEART = -1, A_CRYSTAL = -1, A_PETAL = -1;

static obj_t *alloc_obj(void) {
    for (int i = 0; i < MAX_OBJS; i++)
        if (!G.o[i].alive) { if (i >= G.nobjs) G.nobjs = i + 1; memset(&G.o[i], 0, sizeof G.o[i]); return &G.o[i]; }
    return NULL;
}

static int A_LOG = -1;
static int plat_type(int t) { return t == OT_PLAT_VALLEY || t == OT_PLAT_MOVE2 || t == OT_PLAT_MOVE3; }
void objects_spawn(void) {
    A_EXPLODE = bank_anim(&G.objs, "explosion");
    { static const char *tn[6] = { "turr_idle", "turr_idle_l", "turr_shoot", "turr_shoot_l", "turr_off", "turr_off_l" };
      for (int i = 0; i < 6; i++) A_TURR[i] = bank_anim(&G.objs, tn[i]); }
    A_PELLET = bank_anim(&G.objs, "pellet"); A_EXPLODE2 = bank_anim(&G.objs, "explosion02");
    { static const char *gn[10] = { "gan_pose", "gan_pose_l", "gan_walk", "gan_walk_l", "gan_dead", "gan_dead_l", "gan_curl", "gan_curl_l", "gan_wake", "gan_wake_l" };
      for (int i = 0; i < 10; i++) A_GAN[i] = bank_anim(&G.objs, gn[i]); }
    A_SPARKLE = bank_anim(&G.objs, "ringsparkle"); A_HEART = bank_anim(&G.objs, "heart");
    A_CRYSTAL = bank_anim(&G.objs, "objectcrystal_3"); A_PETAL = bank_anim(&G.objs, "objectpetal_355");
    memset(G.o, 0, sizeof G.o);
    G.nobjs = 0;
    for (int i = 0; i < G.lvl.nobj; i++) if (G.lvl.objs[i].type == OT_LOG && G.lvl.objs[i].anim != 0xffff) A_LOG = G.lvl.objs[i].anim;
    for (int i = 0; i < G.lvl.nobj; i++) {
        const lvobj_t *lo = &G.lvl.objs[i];
        if (lo->type == OT_START || (lo->type >= OT_PARALLAX && lo->type < OT_PARALLAX + 8)) continue;   /* parallax pieces 20-27 are drawn by the level */
        obj_t *o = alloc_obj();
        if (!o) { plat_log("object limit reached\n"); break; }
        o->type = lo->type; o->alive = 1; o->hp = 1;
        o->x = o->ox = (float)lo->x; o->y = o->oy = (float)lo->y;
        o->t = i * 13;
        if (lo->anim != 0xffff && lo->anim < G.objs.nanims) anim_set(&o->an, &G.objs, lo->anim);
        else o->an.b = NULL;
        if (o->type == OT_TURRETUS) { o->hp = 2; o->st = 0; o->tm = 0; o->dir = 14; }   /* RE: created with direction 14 */
        if (plat_type(o->type)) { o->vx = 0; o->vy = 0; o->fx = o->oy; o->fl = 0; }   /* 3565: I = X, J = Y */
        if (o->type == OT_LOG) { o->alive = 0; continue; }       /* the template log (placed off-screen) */
        if (o->type == OT_GANSEKI) {                   /* RE create_enemygansekijr_835: anim 0, direction 14 (left set), A = 8, B = 5 */
            o->hp = 8; o->fy = 0; o->st = 0; o->tm = 0; o->dir = 14; o->flip = 1; o->vy = 0; o->ox = o->oy = 0; o->c = 0; o->fl = 0;
            if (A_GAN[1] >= 0) anim_set(&o->an, &G.objs, A_GAN[1]);
        }
    }
    /* 3566 / 3567: 12 logs per bridge at (bx + 16k, by + 56), A = k, B = base y */
    int nb = G.nobjs;
    for (int b = 0; b < nb; b++) {
        if (!G.o[b].alive || G.o[b].type != OT_BRIDGE) continue;
        for (int k = 11; k >= 0; k--) {
            obj_t *lg = alloc_obj();
            if (!lg) break;
            lg->type = OT_LOG; lg->alive = 1; lg->hp = 1; lg->par = b; lg->dir = k;
            lg->x = lg->ox = G.o[b].x + 16 * k; lg->y = lg->oy = G.o[b].y + 56;
            if (A_LOG >= 0) anim_set(&lg->an, &G.objs, A_LOG);
        }
    }
}


/* ---- platforms and bridges (RE build/re/spec_platforms.md): all before Lilac's movement -------------
 * Valley1 / Moving2 / Moving3: X fixed, Y = floor(J + G sin E), E += F degrees per frame; dip S (ease to 9
 * while stood on, decay x0.8 otherwise); one-way flags 0/6 (4737/4738); carry 4061.
 * Fields: ox, oy = I, J; vx = E; vy = S; fx = last N; fl bit 0/6 one-way, bit 2 stood on.
 * Bridge logs: dir = A (index), oy = B (base y), fx = C (target), fy = D (current sag). */
static double p_sin(double n) { return n == 180. ? 0. : sin(n / 57.29577951); }
static void plat_params(int t, double *F, double *Gm) {
    if (t == OT_PLAT_MOVE2) { *F = 2; *Gm = 120; } else if (t == OT_PLAT_MOVE3) { *F = 1; *Gm = 100; } else { *F = 1; *Gm = 10; }
}
static int box_hits_disc27(float bx, float by, float bw, float bh, int px, int py) {   /* Player_326: 27-px disc at (int X, int Y) */
    float cx = fmaxf(bx, fminf((float)px, bx + bw - 1)), cy = fmaxf(by, fminf((float)py, by + bh - 1));
    float dx = cx - px, dy = cy - py;
    return dx * dx + dy * dy <= 13.5f * 13.5f;
}
/* comparison harness: the original's platforms have run n frames before its trace frame 0 */
void objects_plat_pretick(int n) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !plat_type(o->type)) continue;
        double F, Gm; plat_params(o->type, &F, &Gm);
        o->vx = (float)fmod(o->vx + F * n, 360.);
        double N = o->oy + Gm * p_sin(o->vx);
        o->fx = (float)N; o->y = (float)floor(N);
    }
}
void objects_platforms(player_t *p) {
    double MY = p->y - ORIG_REF_DY;
    int PX = (int)p->x, PY = (int)MY;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !plat_type(o->type)) continue;
        double F, Gm; plat_params(o->type, &F, &Gm);
        o->y -= floorf(o->vy);                                    /* 3270 */
        if (!(o->fl & 4)) o->vy -= o->vy * 0.2f;                   /* 3271 */
        o->fl &= ~4;                                              /* 3272 */
        double L = o->fx;                                         /* 3283: L = N */
        o->vx = (float)fmod(o->vx + F, 360.);
        double N = o->oy + Gm * p_sin(o->vx);                     /* 3287 (H = 1) */
        o->dir = (int)(floor(N) - floor(L));                      /* 3288: P */
        o->fx = (float)N;
        o->y = (float)floor(N);                                   /* 2989 */
    }
    /* 4061: carry / snap onto a moving platform (G >= 0, BottomBig 1 px lower, flag 6 of last frame off) */
    if (p->vy >= 0 || p->ground) {
        for (int t = OT_PLAT_VALLEY; t <= OT_PLAT_MOVE3; t++) {
            int k = mv_sensor_obj(p->x, MY + 1, p->N, SENSOR_BOTTOMBIG, t);
            if (k < 0 || (G.o[k].fl & 0x40)) continue;
            p->y += G.o[k].dir; MY = p->y - ORIG_REF_DY;
            if (!p->ground) { p->gsp = p->vx; p->ground = 1; }   /* L = 1; B kept (no landing conversion) */
            p->vy = 0; p->N = 0; p->O = 0;
            player_sync_sensors(p);
            PY = (int)MY;
            break;
        }
    }
    for (int i = 0; i < G.nobjs; i++) {                           /* 4737 / 4738 */
        obj_t *o = &G.o[i];
        if (!o->alive || !plat_type(o->type)) continue;
        o->fl &= ~0x41;
        if (o->y < PY) o->fl |= 0x41;
    }
    /* bridges: 3294-3300 */
    for (int b = 0; b < G.nobjs; b++) {
        obj_t *br = &G.o[b];
        if (!br->alive || br->type != OT_BRIDGE) continue;
        float bx = floorf(br->x), by = floorf(br->y);
        int over = box_hits_disc27(bx, by, 192, 128, PX, PY);
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_LOG && G.o[i].par == b) G.o[i].fx = 0;   /* 3294 */
        if (p->ground && over) {                                  /* 3295-3297 */
            int dx = PX - (int)bx, idx = abs(dx) >> 4; if (idx > 192) idx = 192;
            int a2 = 12 - idx;
            double depth = 16.0 * fmax(0.0, 1.0 - fabs((dx - 96) / 96.0));
            double C = 0, E = 180, D = idx ? 90.0 / idx : 0, Fv = a2 ? 90.0 / a2 : 0;
            for (int n = 0; n < a2; n++) {
                for (int i = 0; i < G.nobjs; i++) { obj_t *lg = &G.o[i]; if (lg->alive && lg->type == OT_LOG && lg->par == b && lg->dir == 11 - n) lg->fx = (float)(depth * p_sin(E)); }
                E -= Fv;
            }
            for (int n = 0; n < idx; n++) {
                for (int i = 0; i < G.nobjs; i++) { obj_t *lg = &G.o[i]; if (lg->alive && lg->type == OT_LOG && lg->par == b && lg->dir == n) lg->fx = (float)(depth * p_sin(180 - C)); }
                C += D;
            }
        }
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *lg = &G.o[i];
            if (!lg->alive || lg->type != OT_LOG || lg->par != b) continue;
            lg->fy += (lg->fx - lg->fy) * 0.2f;                   /* 3298 */
            if (over) lg->y = (float)(int)(lg->oy + lg->fy);      /* 3300 */
        }
    }
    /* 3303 / 3304 / 3307: dip while stood on */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !plat_type(o->type)) continue;
        if (p->ground && mv_sensor_obj(p->x, MY, p->N, SENSOR_BOTTOMBIG, o->type) == i) o->fl |= 4;
        if (o->fl & 4) o->vy += (9 - o->vy) * 0.2f;
        o->y += floorf(o->vy);
    }
}

/* ---- Ganseki Jr. (EnemyGansekiJr_835): events 4376-4392 + the shared enemy events 2731, 2740-2767
 * (build/re/spec_ganseki.md). D = st, F = tm, G = fx, E = vy, C = c, A = hp. 48x48 images, hotspot
 * (23, 36) facing right / (24, 36) left: feet at Y + 12. */
enum { GAN_POSE, GAN_WALK, GAN_DEAD, GAN_CURL, GAN_WAKE };
#define GAN_ARMOURED 0x20
#define GAN_DEFEATED 0x40
static int gan_solid(float x, float y) { return px_solid(&G.lvl, (int)floorf(x), (int)floorf(y)); }   /* Frame::test_obstacle (background, not platforms) */
static void gan_anim(obj_t *o, int k) {             /* force_animation in the current facing; counter kept */
    int a = A_GAN[2 * k + (o->flip ? 1 : 0)];
    if (a < 0) return;
    if (o->an.anim != a || o->an.b != &G.objs) { int acc = o->an.acc; anim_set(&o->an, &G.objs, a); o->an.acc = acc; }
}
static void gan_face(obj_t *o, const player_t *p) {  /* direction (int)(atan2 deg / 11.25); 8 / 24 keep the image */
    double py = p->y - ORIG_REF_DY;
    double a = atan2(o->y - py, p->x - o->x) * 57.29577951; if (a < 0) a += 360;
    int d = (int)(a / 11.25) & 31;
    if (d == 8 || d == 24) return;
    int left = d > 8 && d < 24;
    if (left != o->flip) { o->flip = left; int k = o->an.anim; for (int i = 0; i < 10; i++) if (A_GAN[i] == k) { gan_anim(o, i / 2); break; } }
}
static int gan_near_view(const obj_t *o) {          /* !is_near_border(-50): the box inside the view grown by 50 */
    float hx = o->flip ? 24.f : 23.f, x0 = o->x - hx, y0 = o->y - 36;
    return x0 >= G.camx - 50 && x0 + 48 <= G.camx + SCREEN_W + 50 && y0 >= G.camy - 50 && y0 + 48 <= G.camy + SCREEN_H + 50;
}
static void ganseki_events(obj_t *o, player_t *p, int touch, int hit) {
    int def = (o->fl & GAN_DEFEATED) != 0;
    int waking = o->an.anim == A_GAN[2 * GAN_WAKE] || o->an.anim == A_GAN[2 * GAN_WAKE + 1];
    if (!def && o->st == 1 && waking && o->an.done) {                    /* 4381: wake animation finished */
        o->fx = (float)p->x; gan_face(o, p); gan_anim(o, GAN_WALK); waking = 0;
    }
    /* 2731: contact (Lilac's character sensor), damage B = 5 through the hurt event */
    if (touch && !def && o->c <= 0 && !player_contact_safe(p) && !p->hurt_pending) player_contact(p, 5);
    /* attacks (2740 ground attack: 4 + |B|/10; body hits with Z = 1: |B|/5 + 3); armoured: deflect, sound 30 */
    if (hit && !def && o->c <= 0) {
        int kind = player_attack_kind(p);
        double b = p->ground ? p->gsp : p->vx;
        if (o->fl & GAN_ARMOURED) { o->c = 5; snd_play_orig(30, 220, 128); player_hitstop(p, 5); }
        else if (kind) {
            double dmg = kind == 1 ? 4.0 + fabs(b) / 10.0 : fabs(b) / 5.0 + 3.0;
            o->fy -= (float)dmg;                                    /* A (HP) kept as a double in fy offset */
            if (8.f + o->fy <= 0) o->hp = 0;
            o->c = 5; snd_play_orig(24, 220, 128); player_hitstop(p, 5);
            o->ox = (float)(fmax(3.0, fabs(1.5 * b)) * p->face);    /* knock-back X/Y, used after death (2765) */
            o->oy = (float)(0.5 * (p->ground ? 0. : p->vy));
        }
    }
    if (!def && o->c > 0) o->c--;                                        /* 2762 */
    if (!def && o->hp <= 0) {                                            /* 2764 + 4387/4388 */
        snd_play_orig(23, 220, 128); o->fl |= GAN_DEFEATED; o->hp = 0; o->y -= 3; o->st = 3; gan_anim(o, GAN_DEAD); def = 1;
        G.kills++;
    }
    if (def) {                                                           /* 2765 / 2767 / 4389 */
        o->x = (float)(int)(o->x + o->ox); o->y = (float)(int)(o->y + o->oy);
        o->oy = fminf(o->oy + 0.25f, 15.f);
        if (o->y > G.camy + 290 || o->x < 0 || o->x > G.lvl.w || o->y > G.lvl.h + 64) o->alive = 0;
        return;
    }
    int near = gan_near_view(o);
    if (near) {
        if (!gan_solid(o->x, o->y + 11)) { o->y = (float)(int)(o->y + o->vy); o->vy = fminf(o->vy + 0.5f, 12.f); }   /* 4376 */
        if (gan_solid(o->x, o->y + 11)) { for (int k = 0; k < 12 && gan_solid(o->x, o->y + 11); k++) o->y -= 1; o->vy = 0; }   /* 4377 */
    }
    if (o->st == 0 && near) { o->tm++; gan_face(o, p); o->fl |= GAN_ARMOURED; }   /* 4379 */
    if (o->st == 0 && o->tm > 40) { o->st = 1; o->tm = 0; gan_anim(o, GAN_WAKE); anim_restart(&o->an); o->fl &= ~GAN_ARMOURED; waking = 1; }   /* 4380 */
    if (o->st == 1 && !waking) {
        float hx = o->flip ? 24.f : 23.f;
        if (o->x > o->fx + 4 && !gan_solid(o->x - hx, o->y)) o->x -= 1;                 /* 4382 */
        else if (o->x < o->fx - 4 && !gan_solid(o->x - hx + 48, o->y)) o->x += 1;       /* 4383 */
        o->tm++;                                                                         /* 4384 */
        if (gan_solid(o->x, o->y + 13) && (o->tm > 60 || (o->x >= o->fx - 4 && o->x <= o->fx + 4))) {   /* 4385 / 4386 */
            o->st = 0; gan_anim(o, GAN_CURL); anim_restart(&o->an); o->tm = -10 - cross_rand(40);
        }
    }
}

static void explode(float x, float y) {
    obj_t *e = alloc_obj();
    if (!e) return;
    e->type = OT_EXPLODE; e->alive = 1; e->x = x; e->y = y;
    if (A_EXPLODE >= 0) anim_set(&e->an, &G.objs, A_EXPLODE);
    snd_play(G.sfx.explode, 220, 128);
}

/* ---- Turretus + EnemyPellet: recovered event logic (RE, docs/RE_NOTES.md "Turretus") ---------------- */
#define TURR_FIRE_FRAME 7          /* RE event 4773: shoot animation frame that releases the pellet */
#define TURR_PERIOD     60         /* RE event 4771: E > 60 starts a shot */
#define TURR_RELOAD     (-40)      /* RE event 4771: E = -40 after a shot starts */
#define TURR_NEAR       200        /* RE event 4770: E counts only while within 200 px of the 426x240 view */
#define PELLET_SPEED    50.f       /* RE create_enemypellet_934: PinballMovement max speed 50, gravity 25 */
#define PELLET_GRAV     25.f
#define PELLET_DAMAGE   10         /* RE col_4_20: GUILifeBaseMeter.B += 10 (2 half-petals on Normal) */

static int dir_left(int d) { d &= 31; return d > 8 && d < 24; }   /* 32-direction image set: 16 = left */

static void set_anim_keep(obj_t *o, int a) {         /* force_animation: frame 0, speed counter kept */
    if (a < 0) return;
    if (o->an.b != &G.objs || o->an.anim != a) { int acc = o->an.acc; anim_set(&o->an, &G.objs, a); o->an.acc = acc; }
}

static void spawn_pellet(float x, float y, int dir) {
    obj_t *e = alloc_obj();
    if (!e) return;
    e->type = OT_PELLET; e->alive = 1; e->x = floorf(x); e->y = floorf(y); e->fx = e->fy = 0; e->born = G.frame;
    float a = (float)(dir & 31) * 11.25f * 3.14159265f / 180.f;   /* PinballMovement::set_direction */
    e->vx = PELLET_SPEED * cosf(a); e->vy = -PELLET_SPEED * sinf(a);
    if (A_PELLET >= 0) anim_set(&e->an, &G.objs, A_PELLET);
}

/* PinballMovement::update (RE; floor split of the position reproduces the logged path exactly):
 * vy += gravity/10; speed = |v| (<= 100); position += v/10 with the fraction carried */
static void pellet_move(obj_t *o) {
    o->vy += PELLET_GRAV / 10.f;
    float sp = sqrtf(o->vx * o->vx + o->vy * o->vy);
    if (sp > 100.f) { o->vx *= 100.f / sp; o->vy *= 100.f / sp; }
    float dx = o->vx / 10.f + o->fx, dy = o->vy / 10.f + o->fy;
    float ix = floorf(dx), iy = floorf(dy);
    o->fx = dx - ix; o->fy = dy - iy; o->x += ix; o->y += iy;
}

/* RE (events 2632-2670): MonitorCrystals spawns 7 crystals (loop_spawn_crystals, event 1323),
 * MonitorRings spawns petals (loop_spawn_rings, 6 per loop), Monitor1up an extra life, MonitorInvincibility
 * sets PlayerOthers D = 1, E = 1800 (30 s) and plays the invincibility music. The thrown items' motion
 * (their own movement in the original) is INFERRED. */
static void throw_items(const obj_t *m, int type, int n) {
    int a = type == OT_CRYSTAL ? A_CRYSTAL : A_PETAL;
    for (int i = 0; i < n; i++) {
        obj_t *e = alloc_obj();
        if (!e) return;
        float ang = (float)(-150 + 120 * i / (n > 1 ? n - 1 : 1)) * 3.14159265f / 180.f;
        e->type = OT_ITEM_FLY; e->st = type; e->alive = 1; e->x = m->x; e->y = m->y - 16;
        e->vx = 3.f * cosf(ang); e->vy = 3.f * sinf(ang) - 1.f;
        if (a >= 0) anim_set(&e->an, &G.objs, a);
    }
}
static void monitor_reward(const obj_t *o, player_t *p) {
    switch (o->type) {
    case OT_MONITOR: throw_items(o, OT_CRYSTAL, 7); break;
    case OT_MON_RINGS: throw_items(o, OT_PETAL, 6); break;
    case OT_MON_1UP: p->lives++; music_play("bgm/m_1up.adp"); G.mus_resume_t = 240; break;   /* jingle length INFERRED */
    case OT_MON_INVINC: p->invuln = 1800; p->invinc = 1; music_play("bgm/m_invincibility.adp"); break;
    }
}

/* one-shot effect object (removed when its animation ends) */
static void spawn_effect(float x, float y, int anim) {
    if (anim < 0) return;
    obj_t *e = alloc_obj();
    if (!e) return;
    e->type = OT_EXPLODE; e->alive = 1; e->x = x; e->y = y;
    anim_set(&e->an, &G.objs, anim);
}

/* item reach (RE 1288/1289/2543): item position within X +-19 and Y - up .. Y + 19 of (int X, int Y) */
static int item_in_reach(const obj_t *o, const player_t *p, int up) {
    int ix = (int)floor(p->x), iy = (int)floor(p->y - ORIG_REF_DY), cx = (int)floorf(o->x), cy = (int)floorf(o->y);
    return cx >= ix - 19 && cx <= ix + 19 && cy >= iy - up && cy <= iy + 19;
}

static void explode2(float x, float y) {
    obj_t *e = alloc_obj();
    if (!e) return;
    e->type = OT_EXPLODE; e->alive = 1; e->x = x; e->y = y;
    if (A_EXPLODE2 >= 0) anim_set(&e->an, &G.objs, A_EXPLODE2);
    else if (A_EXPLODE >= 0) anim_set(&e->an, &G.objs, A_EXPLODE);
    snd_play_orig(80, 220, 128);   /* RV: id 80 with the pellet hit */
}

/* Turretus events 4767-4774 in their original order, after this frame's animation update */
static void turretus_events(obj_t *o, const player_t *p, float bx0, float by0, float bx1, float by1) {
    int L = dir_left(o->dir);
    if (o->st == 0) set_anim_keep(o, A_TURR[0 + L]);                                   /* 4767 */
    if (o->st == 0 && p->x < o->x) o->dir = 15;                                        /* 4768 */
    if (o->st == 0 && o->x < p->x) o->dir = 1;                                         /* 4769 */
    L = dir_left(o->dir);
    if (o->st == 0) set_anim_keep(o, A_TURR[0 + L]);
    int near = bx0 > G.camx - TURR_NEAR && bx1 < G.camx + SCREEN_W + TURR_NEAR &&
               by0 > G.camy - TURR_NEAR && by1 < G.camy + SCREEN_H + TURR_NEAR;
    if (o->st == 0 && near) o->tm++;                                                   /* 4770 */
    if (o->st == 0 && o->tm > TURR_PERIOD && p->hp > 0) { o->tm = TURR_RELOAD; o->st = 1; }   /* 4771 */
    if (o->st == 1) set_anim_keep(o, A_TURR[2 + L]);                                   /* 4772 */
    if (o->st == 1 && o->an.anim == A_TURR[2 + L] && o->an.frame == TURR_FIRE_FRAME) { /* 4773 */
        snd_play_orig(28, 220, 128);
        spawn_pellet(o->x + (L ? -18.f : 18.f), o->y - 14.f, o->dir);   /* action point (RE image data) */
        o->st = 2;
    }
    if (o->st == 2 && (o->an.anim != A_TURR[2 + L] || o->an.done)) o->st = 0;          /* 4774 */
    if (getenv("FP_DBG_TURR") && fabs(o->x - p->x) < 300) plat_log("turr f%d x%.0f st%d tm%d an%d fr%d acc%d\n", G.frame, o->x, o->st, o->tm, o->an.anim, o->an.frame, o->an.acc);
}

static int overlap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1) {
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

/* object box from its current frame (hotspot-relative image rect), shrunk a little */
static void obj_box(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = o->x - 8; *x1 = o->x + 8; *y0 = o->y - 16; *y1 = o->y; return; }
    const frame_t *f = anim_frame(&o->an);
    int m = o->type == OT_PELLET ? 0 : 2;          /* pellets: whole image box (small sprite) */
    *x0 = o->x - f->hx + m; *y0 = o->y - f->hy + m;
    *x1 = *x0 + f->w - 2 * m; *y1 = *y0 + f->h - 2 * m;
}

/* pixel test of the integer rect [rx0, rx1) x [ry0, ry1) against the object's current image mask
 * (Chowdren sprite collision: alpha != 0); objects without a sprite use their box */
static int obj_rect_hit(const obj_t *o, int rx0, int ry0, int rx1, int ry1) {
    if (!o->an.b) {
        float x0, y0, x1, y1; obj_box(o, &x0, &y0, &x1, &y1);
        return rx0 < x1 && x0 < rx1 && ry0 < y1 && y0 < ry1;
    }
    const frame_t *f = anim_frame(&o->an);
    int ox = (int)floorf(o->x) - (o->flip ? f->w - f->hx : f->hx), oy = (int)floorf(o->y) - f->hy;
    int x0 = rx0 > ox ? rx0 : ox, x1 = rx1 < ox + f->w ? rx1 : ox + f->w;
    int y0 = ry0 > oy ? ry0 : oy, y1 = ry1 < oy + f->h ? ry1 : oy + f->h;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int lx = x - ox;
            if (o->flip) lx = f->w - 1 - lx;
            if (frame_mask_px(o->an.b, f, lx, y - oy)) return 1;
        }
    return 0;
}

/* comparison harness: objects of the original trace have been animated n ticks before its frame 0 */
void objects_pretick(int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].an.b) anim_tick(&G.o[i].an, -1);
}

void objects_update(void) {
    player_t *p = &G.p;
    float px0 = p->x - 7, px1 = p->x + 7, py0 = p->y - 30, py1 = p->y;
    float sx0 = floorf(p->x) - 7, sx1 = floorf(p->x) + 8, sy0 = floorf(p->y - ORIG_REF_DY) - 34, sy1 = floorf(p->y - ORIG_REF_DY) + 12;
    float ax0, ay0, ax1, ay1;
    int atk = player_attack_box(p, &ax0, &ay0, &ax1, &ay1);
    int dead = p->hp <= 0;

    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        o->t++;
        /* sleep far off-screen objects (Fusion-style "inactive if far") */
        if (fabsf(o->x - (G.camx + SCREEN_W / 2)) > SCREEN_W * 1.5f && o->type != OT_EXPLODE &&
            o->type != OT_TURRETUS && o->type != OT_PELLET && !(o->type == OT_GANSEKI && o->st != 0)) continue;
        /* objects created by this frame's events are first moved/tested next frame (RE frame order:
         * update_objects, then events) */
        if (o->type == OT_PELLET && o->born == G.frame) continue;
        if (o->type == OT_PELLET) pellet_move(o);                 /* update_objects: movement, then animation */
        anim_tick(&o->an, -1);
        float bx0, by0, bx1, by1;
        obj_box(o, &bx0, &by0, &bx1, &by1);
        /* enemies and hazards hit Lilac's PlayerCharacterSensor_375 (RE: 15x46, hotspot 7,34 at (int X,
         * int Y), i.e. 48..2 px above the feet; RV turretus_jumpinto: passing 2 px above a Turretus is safe) */
        int hurt_touch = !dead && overlap(sx0, sy0, sx1, sy1, bx0, by0, bx1, by1) && obj_rect_hit(o, (int)sx0, (int)sy0, (int)sx1, (int)sy1);
        int hit = !dead && atk && overlap(ax0, ay0, ax1, ay1, bx0, by0, bx1, by1);
        switch (o->type) {
        case OT_CRYSTAL:
            /* RE (events 2543 + loop_collect_item 1289/1291): not while hurt (M8) or defeated; the crystal's
             * position is within X +-19 and Y - 39 .. Y + 19 of Lilac's (int X, int Y): energy + 5 (max 100),
             * the crystal counter - 1 (RV: counts down from 200), a RingSparkle at the crystal, sound 27 (RV) */
            if (!dead && p->state != 5 /* ST_HURT */ && item_in_reach(o, p, 39)) {
                o->alive = 0; p->energy = fminf(PHYS_ENERGY_MAX, p->energy + PHYS_ENERGY_CRYSTAL);
                if (--p->crystals <= 0) { p->crystals = CRYSTALS_PER_LIFE; p->lives++; }   /* extra life INFERRED */
                snd_play_orig(27, 200, 128);
                spawn_effect(o->x, o->y, A_SPARKLE);
            }
            break;
        case OT_PETAL:
            /* RE (1288): within X +-19, Y - 31 .. Y + 19: sound 37 and the petal flies off (state A = 3) to
             * the HUD petal counter (2527); touching the counter (2530/2531) gives +1 half-petal (+2 on
             * Casual), sound 4 and two hearts. Flight speeds INFERRED (RV: collected 29 frames after the
             * touch in build/re/crystal_watch). RV: no pickup at full health. */
            if (o->st == 0) {
                if (!dead && p->state != 5 && p->hp < 14 && item_in_reach(o, p, 31)) {
                    o->st = 3; o->vx = -1; o->vy = -1; snd_play_orig(37, 200, 128);
                }
            } else {
                float hx = G.camx + 16, hy = G.camy + 24;         /* HUD petal counter (RUNTIME_MAP "HUD") */
                o->vx -= 0.5f; o->vy -= 0.5f;
                o->x = fmaxf(o->x + o->vx, hx); o->y = fmaxf(o->y + o->vy, hy);
                if (o->x <= hx + 1 && o->y <= hy + 1) {
                    o->alive = 0;
                    extern int fe_difficulty;
                    p->hp += fe_difficulty >= 3 ? 2 : 1; if (p->hp > 14) p->hp = 14;
                    snd_play_orig(4, 220, 128);
                    spawn_effect(hx, hy, A_HEART); spawn_effect(hx + 8, hy - 4, A_HEART);
                }
            }
            break;
        case OT_DRAGONFLY:
            /* hover patrol (reconstructed) */
            o->x = o->ox + sinf(o->t * 0.015f) * 40.f;
            o->y = o->oy + sinf(o->t * 0.06f) * 6.f;
            o->flip = cosf(o->t * 0.015f) > 0;  /* art faces left; mirror when moving right */
            if (hit) { o->alive = 0; G.kills++; explode(o->x, o->y); player_bounce(p, -4.f); break; }
            if (hurt_touch) player_hurt(p, o->x);
            break;
        case OT_PELLET: {
            /* RE col_4_20: the pellet hits Lilac's sensor (PlayerCharacterSensor_375, 15x46, hotspot 7,34 at
             * her reference point) unless she is invulnerable; it is destroyed with effectsexplosion02 */
            /* (RV: the hit frames of hurt_turretus & co. are matched with the unfloored X and the pellet's whole
             * image box; the original tests the masks, not reproduced yet) */
            if (!dead && !player_invincible(p) && overlap(p->x - 7, p->y - 48, p->x + 8, p->y - 2, bx0, by0, bx1, by1)) {
                player_damage(p, PELLET_DAMAGE); o->alive = 0; explode2(o->x, o->y); break;
            }
            /* INFERRED: removed on solid ground or well outside the view (destroy events not yet recovered) */
            if (solid_at(&G.lvl, o->x, o->y)) { o->alive = 0; explode2(o->x, o->y); break; }
            if (o->x < G.camx - 400 || o->x > G.camx + SCREEN_W + 400 || o->y > G.camy + SCREEN_H + 400) o->alive = 0;
            break;
        }
        case OT_GANSEKI:
            ganseki_events(o, p, hurt_touch, hit);
            break;
        case OT_TURRETUS:
            turretus_events(o, p, bx0, by0, bx1, by1);
            if (hit && o->t > 0) { if (--o->hp <= 0) { o->alive = 0; G.kills++; explode(o->x, o->y); } else o->t = -20; player_bounce(p, -4.f); break; }
            if (hurt_touch) player_hurt(p, o->x);
            break;
        case OT_ROCK: case OT_MONITOR: case OT_MON_RINGS: case OT_MON_1UP: case OT_MON_INVINC:
            if (hit) {
                obj_t m = *o;                /* the freed slot is reused by the thrown items */
                o->alive = 0; explode(m.x, m.y - 8);
                monitor_reward(&m, p);
                player_bounce(p, -4.f);
            }
            break;
        case OT_ITEM_FLY:
            /* crystals / petals thrown out of a monitor: ballistic, settle on the ground, then normal items */
            o->vy += 0.2f; o->x += o->vx; o->y += o->vy;
            if (o->vy > 0 && solid_at(&G.lvl, o->x, o->y + 4)) { o->vy = -o->vy * 0.5f; o->vx *= 0.8f; if (o->vy > -0.5f) { o->vy = 0; o->vx = 0; } }
            if (++o->tm > 120 || (o->vx == 0 && o->vy == 0) ||
                (o->tm >= 12 && item_in_reach(o, p, o->st == OT_CRYSTAL ? 39 : 31)))   /* collectable mid-flight (INFERRED) */ {
                o->type = o->st; o->st = 0; o->tm = 0;     /* becomes a normal crystal / petal */
            }
            break;
        case OT_SPIKES:
            if (hurt_touch) { player_hurt(p, p->x - p->face); }
            break;
        case OT_SPRING: {
            /* RV (spring_yellow_up): fires on the frame the feet reach the spring's top edge (untrimmed
             * box) and puts them at top + 0.25 */
            float top = by0 - 2;
            if (!dead && !p->ground && p->vy >= 0 && px1 > bx0 && px0 < bx1 && p->y >= top && p->y - 30 < by1) {
                player_spring(p, 0, PHYS_SPRING, top + 0.25f); anim_restart(&o->an);
            }
            break;
        }
        case OT_SPRING_UL: case OT_SPRING_UR:
            /* RV (spring_yellow_upleft): same top-edge rule, feet put exactly on the top edge */
            if (!dead && !p->ground && p->vy >= 0 && px1 > bx0 && px0 < bx1 && p->y >= by0 - 2 && p->y - 30 < by1) {
                player_spring(p, o->type == OT_SPRING_UL ? -PHYS_SPRING_DX : PHYS_SPRING_DX, PHYS_SPRING_DY, by0 - 2);
                anim_restart(&o->an);
            }
            break;
        case OT_SPRING_R: case OT_SPRING_L:
            /* pushes away from its wall when touched while moving into it. RV (spring_yellow_right): the
             * original fires at X 6104.77 but not 6107.43 for a spring ending at 6088, so the player's
             * half-width against it is 16.8..19.4 (18 used), not the +-7 body box */
            if (!dead && p->x - 18 < bx1 + 2 && p->x + 18 > bx0 - 2 && py1 > by0 && py0 < by1 && (o->type == OT_SPRING_R ? (p->ground ? p->gsp : p->vx) <= 0 : (p->ground ? p->gsp : p->vx) >= 0)) {
                player_spring_side(p, o->type == OT_SPRING_R ? PHYS_SPRING_GSP : -PHYS_SPRING_GSP);
                anim_restart(&o->an);
            }
            break;
        case OT_HANG: {
            /* hang sensor 50x15, hotspot (25, 0); Lilac's PlayerSensorTopBig_332 is 11x11 centred at
             * (floor X, floor Yref - 13) (RV), tested after this frame's move (RE event 4065) */
            double ry = p->y - ORIG_REF_DY;
            double tx = floor(p->x), ty = floor(ry) - 13;
            if (player_can_hang(p) && overlap(tx - 5, ty - 5, tx + 6, ty + 6, o->x - 25, o->y, o->x + 25, o->y + 15))
                player_hang(p, o->x, o->y);
            break;
        }
        case OT_SIGN:
            /* sign_390 / sign_781 are camera-bound markers (events 2860-2863), not goals */
            break;
        case OT_EXPLODE:
            if (!o->an.b || o->an.done) o->alive = 0;
            break;
        }
    }
}

void objects_draw(int front) {
    (void)front;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive || !o->an.b || o->type == OT_HANG || o->type == OT_LADDERTOP || o->type == OT_BRIDGE) continue;   /* sensors / LadderTop are invisible */
        float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
        if (sx < -96 || sx > SCREEN_W + 96 || sy < -96 || sy > SCREEN_H + 96) continue;
        frame_draw(&G.objs, anim_frame(&o->an), sx, sy, o->flip, 0xffffffff);
    }
}

/* Collision groups tested by Lilac's sensors (RE: QualifierLists of frame 21, read at runtime):
 *   OBJGRP_SOLID    (+0x218c0) always solid: monitors, YellowSpringRight, spikes (4 directions), the
 *                   spiral-tunnel mask, BreakableRock, MinibossBarrier
 *   OBJGRP_OBSTACLE (+0x218d8) solid while grounded / landed on from above: bridge logs, YellowSpringUp /
 *                   UpLeft / UpRight, LadderTop, LevelValleyPlatform1, LevelMovingPlatform2/3
 *   OBJGRP_NOEDGE   (+0x218f0) clears the edge sensors (no angle taken from objects): monitors, springs,
 *                   bridge logs, ladder tops, spikes, platforms, keystone monitor
 * Ported members: monitors, breakable rock, upward spikes, wall springs (solid); the obstacle group's
 * springs are still handled by their own contact rules (RV); OBJGRP_OBSTACLE: LadderTop so far. */
static int in_group(int type, int grp) {
    switch (grp) {
    case OBJGRP_SOLID: return type == OT_MONITOR || type == OT_MON_RINGS || type == OT_MON_1UP || type == OT_MON_INVINC || type == OT_ROCK || type == OT_SPIKES || type == OT_SPRING_R;
    case OBJGRP_OBSTACLE: case OBJGRP_OBSTACLE_ALL:
        return type == OT_LADDERTOP || type == OT_LOG || type == OT_PLAT_VALLEY || type == OT_PLAT_MOVE2 || type == OT_PLAT_MOVE3;
    case OBJGRP_NOEDGE: return type == OT_LADDERTOP || type == OT_LOG || type == OT_PLAT_VALLEY || type == OT_PLAT_MOVE2 || type == OT_PLAT_MOVE3 || type == OT_MONITOR || type == OT_MON_RINGS || type == OT_MON_1UP || type == OT_MON_INVINC || type == OT_SPIKES || type == OT_SPRING || type == OT_SPRING_UL ||
                               type == OT_SPRING_UR || type == OT_SPRING_R;
    default: return 0;
    }
}
static int sensor_hit_any(int grp, int type, int ox, int oy, const signed char *x0, const signed char *x1, int h) {
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive || !o->an.b || (type >= 0 ? o->type != type : !in_group(o->type, grp))) continue;
        if (grp == OBJGRP_OBSTACLE && (o->fl & 1)) continue;      /* flag 0: one-way platform below Lilac's reference (4738) */
        const frame_t *f = anim_frame(&o->an);
        int bx = (int)floorf(o->x) - (o->flip ? f->w - f->hx : f->hx), by = (int)floorf(o->y) - f->hy;
        if (ox > bx + f->w || ox + 27 < bx || oy >= by + f->h || oy + h <= by) continue;
        if (o->type == OT_LOG) {                                   /* initialize_active(true): collision box = image rect */
            for (int j = 0; j < h; j++) {
                int y = oy + j;
                if (x1[j] < x0[j] || y < by || y >= by + f->h) continue;
                if (ox + x1[j] >= bx && ox + x0[j] < bx + f->w) return i;
            }
            continue;
        }
        for (int j = 0; j < h; j++) {
            int y = oy + j;
            if (x1[j] < x0[j] || y < by || y >= by + f->h) continue;
            for (int x = ox + x0[j]; x <= ox + x1[j]; x++) {
                int lx = x - bx;
                if (lx < 0 || lx >= f->w) continue;
                if (o->flip) lx = f->w - 1 - lx;
                if (frame_mask_px(o->an.b, f, lx, y - by)) return i;
            }
        }
    }
    return -1;
}
int objects_sensor_hit(int grp, int ox, int oy, const signed char *x0, const signed char *x1, int h) {
    return sensor_hit_any(grp, -1, ox, oy, x0, x1, h) >= 0;
}
int objects_sensor_hit_type(int type, int ox, int oy, const signed char *x0, const signed char *x1, int h) {
    return sensor_hit_any(-1, type, ox, oy, x0, x1, h);
}
