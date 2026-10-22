/* Game state, camera, recovered-level drawing, HUD. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../fp.h"

game_t G;

#ifndef FP_LEVEL
#define FP_LEVEL "dv1"
#endif

static void level_start(void) {
    for (int i = 0; i < G.lvl.nobj; i++)
        if (G.lvl.objs[i].type == OT_START) { G.p.spawn_x = G.lvl.objs[i].x; G.p.spawn_y = G.lvl.objs[i].y; }
    /* snap spawn to the floor below the start marker */
    float sy, a;
    if (floor_probe(&G.lvl, G.p.spawn_x, G.p.spawn_y - 32, 400, 1, &sy, &a)) G.p.spawn_y = sy;
    G.p.crystals = 0;
    player_reset(&G.p, G.p.spawn_x, G.p.spawn_y);
    objects_spawn();
    G.clear_t = 0; G.frame = 0;
    G.camx = G.p.x - SCREEN_W / 2; G.camy = G.p.y - 150;
    camera_update();
}

int game_init(void) {
    memset(&G, 0, sizeof G);
    int bad = 0;
    char nm[32];
    for (int i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "pal%d.pal", i); bad |= pal_load_file(nm) < 0; }
    bad |= bank_load(&G.lilac, "lilac2") < 0;
    snprintf(nm, sizeof nm, "%s_obj", FP_LEVEL); bad |= bank_load(&G.objs, nm) < 0;
    bad |= level_load(&G.lvl, FP_LEVEL) < 0;
    if (bad) { plat_log("FATAL: data load failed\n"); return -1; }
    int want = 0;
    for (int i = 0; i < G.lvl.nobj; i++)
        if (G.lvl.objs[i].type >= OT_PARALLAX && G.lvl.objs[i].anim != 0xffff && G.lvl.objs[i].anim + 1 > want)
            want = G.lvl.objs[i].anim + 1;
    for (G.npar = 0; G.npar < want && G.npar < 12; G.npar++) {
        snprintf(nm, sizeof nm, "%s_par%d", FP_LEVEL, G.npar);
        if (tex_load_file(&G.par[G.npar], nm) < 0) break;
    }
    G.sfx.jump = snd_load("sfx_jump.wav");
    G.sfx.crystal = snd_load("sfx_crystal.wav");
    G.sfx.hit = snd_load("sfx_hit.wav");
    G.sfx.explode = snd_load("sfx_explode.wav");
    G.sfx.attack = snd_load("sfx_attack.wav");
    G.sfx.hurt = snd_load("sfx_hurt.wav");
    G.sfx.boost = snd_load("sfx_boost.wav");
    level_start();
    music_play("stage1.pcm");
    return 0;
}

void camera_update(void) {
    const player_t *p = &G.p;
    float lead = p->ground ? p->gsp * cosf(p->angle) : p->vx;
    float tx = p->x - SCREEN_W / 2 + fmaxf(-48.f, fminf(48.f, lead * 8.f));
    float ty = p->y - 150;
    G.camx += (tx - G.camx) * 0.15f;
    if (p->ground) G.camy += (ty - G.camy) * 0.2f;
    else { if (ty < G.camy - 40) G.camy = ty + 40; if (ty > G.camy + 24) G.camy = ty - 24; }
    float maxx = G.lvl.w - SCREEN_W, maxy = G.lvl.h - SCREEN_H;
    if (G.camx > maxx) G.camx = maxx;
    if (G.camx < 0) G.camx = 0;
    if (G.camy > maxy) G.camy = maxy;
    if (G.camy < 0) G.camy = 0;
}

void game_update(const input_t *in) {
    if (in->pressed & BTN_START) G.paused = !G.paused;
    if (G.paused) return;
    G.frame++;
    if (G.clear_t) { if (++G.clear_t > 300) level_start(); return; }
    player_update(&G.p, in);
    objects_update();
    camera_update();
}

/* Parallax bands: positions/images from the level's levelparallaxpiece objects.
 * Scroll factors are RECONSTRUCTED (the original moves them in event code). */
static void draw_parallax(void) {
    int k = 0;
    float vmax = (float)(G.lvl.h - SCREEN_H);
    for (int i = 0; i < G.lvl.nobj; i++) {
        const lvobj_t *o = &G.lvl.objs[i];
        if (o->type < OT_PARALLAX || o->anim >= G.npar) continue;
        const tex_t *t = &G.par[o->anim];
        float fx = k == 0 ? 0.f : 0.04f + 0.07f * k;
        float y = (float)o->y - (vmax > 0 ? G.camy / vmax : 0) * 0.55f * vmax * (k == 0 ? 0.f : 1.f);
        if (k == 0) {    /* sky: stretch to the screen */
            r_sprite(t, 0, 0, t->srcw, t->srch, 0, 0, SCREEN_W, SCREEN_H, 0, 0xffffffff);
        } else if (y < SCREEN_H && y + t->srch > 0) {
            float off = fmodf(G.camx * fx, (float)t->srcw);
            for (float x = -off; x < SCREEN_W; x += t->srcw)
                r_sprite(t, 0, 0, t->srcw, t->srch, floorf(x), floorf(y), t->srcw, t->srch, 0, 0xffffffff);
        }
        k++;
    }
}

static void draw_pieces(int lmin, int lmax) {
    const level_t *l = &G.lvl;
    int cx = (int)floorf(G.camx), cy = (int)floorf(G.camy);
    for (int i = 0; i < l->npieces; i++) {
        const piece_t *p = &l->pieces[i];
        if (p->layer < lmin || p->layer > lmax || (p->pad & 1)) continue;   /* bit0: invisible (blend alpha 0) */
        int sx = p->x - cx, sy = p->y - cy;
        if (sx >= SCREEN_W || sy >= SCREEN_H || sx + p->w <= 0 || sy + p->h <= 0) continue;
        r_sprite(&l->atlas[p->atlas], p->u, p->v, p->w, p->h, (float)sx, (float)sy, p->w, p->h, 0, 0xffffffff);
    }
}

static const uint8_t font3x5[][3] = {
    {31,17,31},{0,31,0},{29,21,23},{21,21,31},{7,4,31},{23,21,29},{31,21,29},{1,1,31},{31,21,31},{23,21,31},
    {30,5,30},{31,21,10},{14,17,17},{31,17,14},{31,21,17},{31,5,1},{14,17,29},{31,4,31},{17,31,17},{8,16,15},
    {31,4,27},{31,16,16},{31,2,31},{31,1,30},{14,17,14},{31,5,2},{14,25,30},{31,5,26},{18,21,9},{1,31,1},{15,16,15},
    {7,24,7},{31,8,31},{27,4,27},{3,28,3},{25,21,19}
};

void text_draw(float x, float y, const char *s, uint32_t argb, int scale) {
    for (; *s; s++, x += 4 * scale) {
        int g = -1;
        if (*s >= '0' && *s <= '9') g = *s - '0';
        else if (*s >= 'A' && *s <= 'Z') g = 10 + *s - 'A';
        else if (*s == ':') { r_rect(x + scale, y + scale, scale, scale, argb); r_rect(x + scale, y + 3 * scale, scale, scale, argb); continue; }
        if (g < 0) continue;
        for (int c = 0; c < 3; c++)
            for (int r = 0; r < 5; r++)
                if (font3x5[g][c] & (1 << r)) r_rect(x + c * scale, y + r * scale, scale, scale, argb);
    }
}

void hud_draw(void) {
    char buf[32];
    for (int i = 0; i < 6; i++)
        r_rect(8 + i * 10, 8, 8, 8, i < G.p.hp ? 0xffff4080 : 0xff402030);
    snprintf(buf, sizeof buf, "%d", G.p.crystals);
    text_draw(8, 22, buf, 0xffffffff, 2);
    int secs = G.frame / FPS;
    snprintf(buf, sizeof buf, "%d:%02d", secs / 60, secs % 60);
    text_draw(SCREEN_W - 50, 8, buf, 0xffffffff, 2);
    if (G.clear_t) text_draw(SCREEN_W / 2 - 66, 100, "STAGE CLEAR", 0xffffe040, 3);
    if (G.paused) text_draw(SCREEN_W / 2 - 34, 100, "PAUSED", 0xffffffff, 3);
#ifdef FP_PROFILE
    snprintf(buf, sizeof buf, "U%u D%u", (unsigned)G.prof_update_us, (unsigned)G.prof_draw_us);
    text_draw(8, SCREEN_H - 12, buf, 0xff80ff80, 1);
#endif
}

void game_draw(void) {
    r_begin();
    draw_parallax();
    draw_pieces(0, 2);
    objects_draw(0);
    player_draw(&G.p);
    draw_pieces(3, 7);
    hud_draw();
    r_end();
}
