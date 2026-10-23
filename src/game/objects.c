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
static int A_PELLET = -1, A_EXPLODE2 = -1, A_SPARKLE = -1, A_HEART = -1, A_CRYSTAL = -1, A_PETAL = -1;

static obj_t *alloc_obj(void) {
    for (int i = 0; i < MAX_OBJS; i++)
        if (!G.o[i].alive) { if (i >= G.nobjs) G.nobjs = i + 1; memset(&G.o[i], 0, sizeof G.o[i]); return &G.o[i]; }
    return NULL;
}

void objects_spawn(void) {
    A_EXPLODE = bank_anim(&G.objs, "explosion");
    { static const char *tn[6] = { "turr_idle", "turr_idle_l", "turr_shoot", "turr_shoot_l", "turr_off", "turr_off_l" };
      for (int i = 0; i < 6; i++) A_TURR[i] = bank_anim(&G.objs, tn[i]); }
    A_PELLET = bank_anim(&G.objs, "pellet"); A_EXPLODE2 = bank_anim(&G.objs, "explosion02");
    A_SPARKLE = bank_anim(&G.objs, "ringsparkle"); A_HEART = bank_anim(&G.objs, "heart");
    A_CRYSTAL = bank_anim(&G.objs, "objectcrystal_3"); A_PETAL = bank_anim(&G.objs, "objectpetal_355");
    memset(G.o, 0, sizeof G.o);
    G.nobjs = 0;
    for (int i = 0; i < G.lvl.nobj; i++) {
        const lvobj_t *lo = &G.lvl.objs[i];
        if (lo->type == OT_START || lo->type >= OT_PARALLAX) continue;
        obj_t *o = alloc_obj();
        if (!o) { plat_log("object limit reached\n"); break; }
        o->type = lo->type; o->alive = 1; o->hp = 1;
        o->x = o->ox = (float)lo->x; o->y = o->oy = (float)lo->y;
        o->t = i * 13;
        if (lo->anim != 0xffff && lo->anim < G.objs.nanims) anim_set(&o->an, &G.objs, lo->anim);
        else o->an.b = NULL;
        if (o->type == OT_TURRETUS) { o->hp = 2; o->st = 0; o->tm = 0; o->dir = 14; }   /* RE: created with direction 14 */
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
            o->type != OT_TURRETUS && o->type != OT_PELLET) continue;
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
        if (!o->alive || !o->an.b || o->type == OT_HANG) continue;   /* sensors are invisible */
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
 * springs are still handled by their own contact rules (RV), so OBJGRP_OBSTACLE has no members yet. */
static int in_group(int type, int grp) {
    switch (grp) {
    case OBJGRP_SOLID: return type == OT_MONITOR || type == OT_MON_RINGS || type == OT_MON_1UP || type == OT_MON_INVINC || type == OT_ROCK || type == OT_SPIKES || type == OT_SPRING_R;
    case OBJGRP_NOEDGE: return type == OT_MONITOR || type == OT_MON_RINGS || type == OT_MON_1UP || type == OT_MON_INVINC || type == OT_SPIKES || type == OT_SPRING || type == OT_SPRING_UL ||
                               type == OT_SPRING_UR || type == OT_SPRING_R;
    default: return 0;
    }
}
int objects_sensor_hit(int grp, int ox, int oy, const signed char *x0, const signed char *x1, int h) {
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive || !in_group(o->type, grp) || !o->an.b) continue;
        const frame_t *f = anim_frame(&o->an);
        int bx = (int)floorf(o->x) - (o->flip ? f->w - f->hx : f->hx), by = (int)floorf(o->y) - f->hy;
        if (ox > bx + f->w || ox + 27 < bx || oy >= by + f->h || oy + h <= by) continue;
        for (int j = 0; j < h; j++) {
            int y = oy + j;
            if (x1[j] < x0[j] || y < by || y >= by + f->h) continue;
            for (int x = ox + x0[j]; x <= ox + x1[j]; x++) {
                int lx = x - bx;
                if (lx < 0 || lx >= f->w) continue;
                if (o->flip) lx = f->w - 1 - lx;
                if (frame_mask_px(o->an.b, f, lx, y - by)) return 1;
            }
        }
    }
    return 0;
}
