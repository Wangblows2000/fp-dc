/* Level objects placed from the recovered frame data. Visuals/animations are the
 * original classes' first animation; behaviour is reconstructed (UNVERIFIED unless
 * noted) because the original logic is compiled event code. */
#include <math.h>
#include <string.h>
#include "../fp.h"

static int A_EXPLODE = -1;

static obj_t *alloc_obj(void) {
    for (int i = 0; i < MAX_OBJS; i++)
        if (!G.o[i].alive) { if (i >= G.nobjs) G.nobjs = i + 1; memset(&G.o[i], 0, sizeof G.o[i]); return &G.o[i]; }
    return NULL;
}

void objects_spawn(void) {
    A_EXPLODE = bank_anim(&G.objs, "explosion");
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
        if (o->type == OT_TURRETUS) o->hp = 2;
    }
}

static void explode(float x, float y) {
    obj_t *e = alloc_obj();
    if (!e) return;
    e->type = OT_EXPLODE; e->alive = 1; e->x = x; e->y = y;
    if (A_EXPLODE >= 0) anim_set(&e->an, &G.objs, A_EXPLODE);
    snd_play(G.sfx.explode, 220, 128);
}

static int overlap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1) {
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

/* object box from its current frame (hotspot-relative image rect), shrunk a little */
static void obj_box(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = o->x - 8; *x1 = o->x + 8; *y0 = o->y - 16; *y1 = o->y; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = o->x - f->hx + 2; *y0 = o->y - f->hy + 2;
    *x1 = *x0 + f->w - 4;   *y1 = *y0 + f->h - 4;
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
        o->t++;
        /* sleep far off-screen objects (Fusion-style "inactive if far") */
        if (fabsf(o->x - (G.camx + SCREEN_W / 2)) > SCREEN_W * 1.5f && o->type != OT_EXPLODE) continue;
        anim_tick(&o->an, -1);
        float bx0, by0, bx1, by1;
        obj_box(o, &bx0, &by0, &bx1, &by1);
        int touch = !dead && overlap(px0, py0, px1, py1, bx0, by0, bx1, by1);
        int hit = !dead && atk && overlap(ax0, ay0, ax1, ay1, bx0, by0, bx1, by1);
        switch (o->type) {
        case OT_CRYSTAL:
            if (touch) { o->alive = 0; p->crystals++; snd_play(G.sfx.crystal, 160, 128); }
            break;
        case OT_PETAL:
            if (touch) { o->alive = 0; if (p->hp < 6) p->hp++; snd_play(G.sfx.crystal, 220, 128); }
            break;
        case OT_DRAGONFLY:
            /* hover patrol (reconstructed) */
            o->x = o->ox + sinf(o->t * 0.015f) * 40.f;
            o->y = o->oy + sinf(o->t * 0.06f) * 6.f;
            o->flip = cosf(o->t * 0.015f) > 0;  /* art faces left; mirror when moving right */
            if (hit) { o->alive = 0; G.kills++; explode(o->x, o->y); player_bounce(p, -4.f); break; }
            if (touch) player_hurt(p, o->x);
            break;
        case OT_TURRETUS:
            if (hit && o->t > 0) { if (--o->hp <= 0) { o->alive = 0; G.kills++; explode(o->x, o->y); } else o->t = -20; player_bounce(p, -4.f); break; }
            if (touch) player_hurt(p, o->x);
            break;
        case OT_ROCK: case OT_MONITOR:
            if (hit) {
                o->alive = 0; explode(o->x, o->y - 8);
                if (o->type == OT_MONITOR) p->crystals += 10;   /* reward UNVERIFIED */
                player_bounce(p, -4.f);
            }
            break;
        case OT_SPIKES:
            if (touch) { player_hurt(p, p->x - p->face); }
            break;
        case OT_SPRING:
            if (touch && p->vy >= 0) { player_spring(p, PHYS_SPRING); anim_restart(&o->an); }
            break;
        case OT_SIGN:
            if (!G.clear_t && !dead && p->x > o->x) G.clear_t = 1;
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
        if (!o->alive || !o->an.b) continue;
        float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
        if (sx < -96 || sx > SCREEN_W + 96 || sy < -96 || sy > SCREEN_H + 96) continue;
        frame_draw(&G.objs, anim_frame(&o->an), sx, sy, o->flip, 0xffffffff);
    }
}
