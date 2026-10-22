/* Level objects: crystals, hornet enemies, checkpoint, goal, explosions. */
#include <math.h>
#include <string.h>
#include "../fp.h"

enum { OT_EXPLODE = 100 };
static int A_HORNET, A_EXPLODE, A_CRYSTAL;

static obj_t *alloc_obj(void) {
    for (int i = 0; i < MAX_OBJS; i++)
        if (!G.o[i].alive) { if (i >= G.nobjs) G.nobjs = i + 1; memset(&G.o[i], 0, sizeof G.o[i]); return &G.o[i]; }
    return NULL;
}

void objects_spawn(void) {
    A_HORNET = bank_anim(&G.objs, "hornet");
    A_EXPLODE = bank_anim(&G.objs, "explode");
    A_CRYSTAL = bank_anim(&G.objs, "crystal");
    memset(G.o, 0, sizeof G.o);
    G.nobjs = 0;
    for (int i = 0; i < G.lvl.nobj; i++) {
        const lvlobj_t *lo = &G.lvl.objs[i];
        if (lo->type == O_PLAYER) continue;
        obj_t *o = alloc_obj();
        if (!o) { plat_log("object limit reached\n"); break; }
        o->type = lo->type; o->alive = 1;
        o->x = o->ox = lo->x; o->y = o->oy = lo->y;
        o->t = i * 17;   /* desynchronise idle motion */
        if (o->type == O_HORNET) o->anim = A_HORNET;
        if (o->type == O_CRYSTAL) o->anim = A_CRYSTAL;
    }
}

static void explode(float x, float y) {
    obj_t *e = alloc_obj();
    if (!e) return;
    e->type = OT_EXPLODE; e->alive = 1; e->x = x; e->y = y; e->anim = A_EXPLODE;
    snd_play(G.sfx.explode, 220, 128);
}

static int overlap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1) {
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

void objects_update(void) {
    player_t *p = &G.p;
    float px0 = p->x - 7, px1 = p->x + 7, py0 = p->y - 30, py1 = p->y;
    float ax0, ay0, ax1, ay1;
    int atk = player_attack_box(p, &ax0, &ay0, &ax1, &ay1);
    int dead = p->hp <= 0;

    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        o->t++; o->tick++;
        switch (o->type) {
        case O_CRYSTAL:
            o->y = o->oy - 6 + sinf(o->t * 0.08f) * 2.f;
            if (!dead && overlap(px0, py0, px1, py1, o->x - 8, o->y - 10, o->x + 8, o->y + 6)) {
                o->alive = 0; p->crystals++;
                snd_play(G.sfx.crystal, 180, 128);
            }
            break;
        case O_HORNET: {
            float nx = o->ox + sinf(o->t * 0.02f) * 48.f;
            o->flip = nx > o->x;               /* sprite faces left by default */
            o->x = nx;
            o->y = o->oy - 8 + sinf(o->t * 0.07f) * 6.f;
            float hx0 = o->x - 12, hx1 = o->x + 12, hy0 = o->y - 22, hy1 = o->y;
            if (dead) break;
            if (atk && overlap(ax0, ay0, ax1, ay1, hx0, hy0, hx1, hy1)) {
                o->alive = 0;
                explode(o->x, o->y);
                if (!p->ground && p->vy > 0) p->vy = -4.f;   /* bounce off */
                break;
            }
            if (overlap(px0, py0, px1, py1, hx0, hy0, hx1, hy1)) player_hurt(p, o->x);
            break;
        }
        case O_CHECKPOINT:
            if (!o->tick || o->flip) break;
            if (!dead && p->x > o->x - 8 && fabsf(p->y - o->y) < 48) {
                o->flip = 1;                       /* activated */
                p->spawn_x = o->x; p->spawn_y = o->y;
                snd_play(G.sfx.crystal, 255, 128);
            }
            break;
        case O_GOAL:
            if (!G.clear_t && !dead && p->x > o->x - 8) G.clear_t = 1;
            break;
        case OT_EXPLODE:
            if (anim_done(&G.objs, o->anim, o->tick)) o->alive = 0;
            break;
        }
    }
}

void objects_draw(void) {
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive) continue;
        float sx = o->x - G.camx, sy = o->y - G.camy;
        if (sx < -64 || sx > SCREEN_W + 64 || sy < -64 || sy > SCREEN_H + 96) continue;
        switch (o->type) {
        case O_CRYSTAL: case O_HORNET: case OT_EXPLODE:
            bank_draw(&G.objs, o->anim, o->tick, sx, sy, o->flip, 0xffffffff);
            break;
        case O_CHECKPOINT:  /* placeholder lamp until the original sprite is identified */
            r_rect(sx - 2, sy - 40, 4, 40, 0xff705030);
            r_rect(sx - 6, sy - 48, 12, 10, o->flip ? 0xff40ff60 : 0xffff4040);
            break;
        case O_GOAL:        /* placeholder goal post */
            r_rect(sx - 2, sy - 64, 4, 64, 0xffc0c0c0);
            r_rect(sx + 2, sy - 64, 24, 16, (G.frame & 16) ? 0xffffd000 : 0xffff8000);
            break;
        }
    }
}
