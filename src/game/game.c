/* Game state, camera, background/terrain drawing, HUD. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../fp.h"

game_t G;

static void load_or_die(int r, const char *what) {
    if (r < 0) plat_log("FATAL: failed to load %s\n", what);
}

static void level_start(void) {
    for (int i = 0; i < G.lvl.nobj; i++)
        if (G.lvl.objs[i].type == O_PLAYER) {
            G.p.spawn_x = G.lvl.objs[i].x; G.p.spawn_y = G.lvl.objs[i].y;
        }
    G.p.crystals = 0;
    player_reset(&G.p, G.p.spawn_x, G.p.spawn_y);
    objects_spawn();
    G.clear_t = 0;
    G.camx = G.p.x - SCREEN_W / 2; G.camy = G.p.y - 150;
    camera_update();
}

int game_init(void) {
    memset(&G, 0, sizeof G);
    int bad = 0;
#define L(x, what) do { int _r = (x); load_or_die(_r, what); bad |= _r < 0; } while (0)
    L(bank_load(&G.lilac, "lilac"), "lilac");
    L(bank_load(&G.objs, "objs"), "objs");
    L(tex_load_file(&G.sky, "sky"), "sky");
    L(tex_load_file(&G.mount, "mount"), "mount");
    L(tex_load_file(&G.forest, "forest"), "forest");
    L(tex_load_file(&G.rock, "rock"), "rock");
    L(tex_load_file(&G.log, "log"), "log");
    L(level_load(&G.lvl, "test1"), "level");
#undef L
    if (bad) return -1;
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
    /* vertical: tight on ground, lazy window in the air */
    if (p->ground) G.camy += (ty - G.camy) * 0.2f;
    else {
        if (ty < G.camy - 40) G.camy = ty + 40;
        if (ty > G.camy + 24) G.camy = ty - 24;
    }
    float maxx = G.lvl.w * TILE - SCREEN_W, maxy = G.lvl.h * TILE - SCREEN_H;
    if (G.camx < 0) G.camx = 0;
    if (G.camx > maxx) G.camx = maxx;
    if (G.camy < 0) G.camy = 0;
    if (G.camy > maxy) G.camy = maxy;
}

void game_update(const input_t *in) {
    G.frame++;
    if (in->pressed & BTN_START) G.paused = !G.paused;
    if (G.paused) return;
    if (G.clear_t) {
        if (++G.clear_t > 240) level_start();
        return;
    }
    player_update(&G.p, in);
    objects_update();
    camera_update();
}

/* horizontally repeating parallax layer */
static void layer(const tex_t *t, float factor, float y) {
    float off = fmodf(G.camx * factor, (float)t->srcw);
    for (float x = -off; x < SCREEN_W; x += t->srcw)
        r_sprite(t, 0, 0, t->srcw, t->srch, floorf(x), floorf(y), t->srcw, t->srch, 0, 0xffffffff);
}

static void draw_terrain(void) {
    const level_t *l = &G.lvl;
    int tx0 = (int)floorf(G.camx / TILE), ty0 = (int)floorf(G.camy / TILE);
    int ox = (int)floorf(G.camx), oy = (int)floorf(G.camy);
    for (int ty = ty0; ty <= ty0 + SCREEN_H / TILE + 1; ty++)
        for (int tx = tx0; tx <= tx0 + SCREEN_W / TILE + 1; tx++) {
            int t = level_tile(l, tx, ty);
            if (t == T_EMPTY || ty >= l->h || tx < 0 || tx >= l->w) continue;
            float sx = tx * TILE - ox, sy = ty * TILE - oy;
            int u = (tx * TILE) % 256, v = (ty * TILE) % 192;
            switch (t) {
            case T_SOLID: {
                r_sprite(&G.rock, u, v, TILE, TILE, sx, sy, TILE, TILE, 0, 0xffffffff);
                int above = level_tile(l, tx, ty - 1);
                if (tile_height(above, 0) == 0 || above == T_ONEWAY)
                    r_rect(sx, sy, TILE, 2, 0xff5a8a2a);      /* grass lip */
                break;
            }
            case T_ONEWAY:
                r_sprite(&G.log, (tx * TILE) % (G.log.srcw - TILE), 0, TILE, 14 < G.log.srch ? 14 : G.log.srch,
                         sx, sy, TILE, 14 < G.log.srch ? 14 : G.log.srch, 0, 0xffffffff);
                break;
            case T_SPIKE:
                for (int k = 0; k < 2; k++)
                    for (int s = 0; s < 4; s++)
                        r_rect(sx + k * 8 + s, sy + 16 - (s + 1) * 4, 8 - 2 * s, 4, s ? 0xffd8d8e0 : 0xff808090);
                break;
            default:   /* slopes: one textured 1px column per x */
                for (int cx = 0; cx < TILE; cx++) {
                    int h = tile_height(t, cx);
                    if (h <= 0) continue;
                    r_sprite(&G.rock, u + cx, v + TILE - h, 1, h, sx + cx, sy + TILE - h, 1, h, 0, 0xffffffff);
                    r_rect(sx + cx, sy + TILE - h, 1, 2, 0xff5a8a2a);
                }
            }
        }
}

static const uint8_t font3x5[][3] = {
    /* 0-9 */ {31,17,31},{0,31,0},{29,21,23},{21,21,31},{7,4,31},{23,21,29},{31,21,29},{1,1,31},{31,21,31},{23,21,31},
    /* A-Z */ {30,5,30},{31,21,10},{14,17,17},{31,17,14},{31,21,17},{31,5,1},{14,17,29},{31,4,31},{17,31,17},{8,16,15},
    {31,4,27},{31,16,16},{31,2,31},{31,1,30},{14,17,14},{31,5,2},{14,25,30},{31,5,26},{18,21,9},{1,31,1},{15,16,15},
    {7,24,7},{31,8,31},{27,4,27},{3,28,3},{25,21,19}
};

void text_draw(float x, float y, const char *s, uint32_t argb, int scale) {
    for (; *s; s++, x += 4 * scale) {
        int g = -1;
        if (*s >= '0' && *s <= '9') g = *s - '0';
        else if (*s >= 'A' && *s <= 'Z') g = 10 + *s - 'A';
        else if (*s == ':' ) { r_rect(x + scale, y + scale, scale, scale, argb); r_rect(x + scale, y + 3 * scale, scale, scale, argb); continue; }
        if (g < 0) continue;
        for (int c = 0; c < 3; c++)
            for (int r = 0; r < 5; r++)
                if (font3x5[g][c] & (1 << r)) r_rect(x + c * scale, y + r * scale, scale, scale, argb);
    }
}

void hud_draw(void) {
    char buf[32];
    /* life petals */
    for (int i = 0; i < 6; i++)
        r_rect(8 + i * 10, 8, 8, 8, i < G.p.hp ? 0xffff4080 : 0xff402030);
    snprintf(buf, sizeof buf, "%d", G.p.crystals);
    bank_draw(&G.objs, bank_anim(&G.objs, "crystal"), 0, 14, 36, 0, 0xffffffff);
    text_draw(26, 22, buf, 0xffffffff, 2);
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
    r_sprite(&G.sky, 0, 0, G.sky.srcw, G.sky.srch, 0, 0, SCREEN_W, SCREEN_H, 0, 0xffffffff);
    float py = -G.camy * 0.15f;
    layer(&G.mount, 0.2f, 70 + py);
    layer(&G.forest, 0.4f, 110 + py * 1.5f);
    draw_terrain();
    objects_draw();
    player_draw(&G.p);
    hud_draw();
    r_end();
}
