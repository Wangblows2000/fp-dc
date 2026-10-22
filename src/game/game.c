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
    /* crystals, lives, energy persist across restarts (RV: debug restart kept them) */
    player_reset(&G.p, G.p.spawn_x, G.p.spawn_y);
    objects_spawn();
    G.clear_t = 0; G.frame = 0;
    G.camx = G.p.x - SCREEN_W / 2; G.camy = G.p.y - 150;
    camera_update();
}

static void bank_free(bank_t *b) { free(b->frames); free(b->anims); free(b->seq); memset(b, 0, sizeof *b); }

static void game_unload(void) {
    bank_free(&G.lilac); bank_free(&G.objs);
    free(G.lvl.pieces); free(G.lvl.objs); free(G.lvl.blob);
    memset(&G.lvl, 0, sizeof G.lvl);
    G.npar = 0; G.nobjs = 0;
    r_reset();
}

/* every level lives in its own data dir (own palettes + Lilac bank; tools/build_level.sh) */
int game_load_level(const char *dir) {
    game_unload();
    snprintf(G.lvdir, sizeof G.lvdir, "%s", dir);
    int bad = 0;
    char nm[48];
    for (int i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "%s/pal%d.pal", dir, i); bad |= pal_load_file(nm) < 0; }
    snprintf(nm, sizeof nm, "%s/lilac2", dir); bad |= bank_load(&G.lilac, nm) < 0;
    snprintf(nm, sizeof nm, "%s/%s_obj", dir, dir); bad |= bank_load(&G.objs, nm) < 0;
    snprintf(nm, sizeof nm, "%s/%s", dir, dir); bad |= level_load(&G.lvl, nm) < 0;
    if (bad) { plat_log("level %s: data load failed\n", dir); return -1; }
    int want = 0;
    for (int i = 0; i < G.lvl.nobj; i++)
        if (G.lvl.objs[i].type >= OT_PARALLAX && G.lvl.objs[i].anim != 0xffff && G.lvl.objs[i].anim + 1 > want)
            want = G.lvl.objs[i].anim + 1;
    for (G.npar = 0; G.npar < want && G.npar < 12; G.npar++) {
        snprintf(nm, sizeof nm, "%s/%s_par%d", dir, dir, G.npar);
        if (tex_load_file(&G.par[G.npar], nm) < 0) break;
    }
    G.p.spawn_x = G.p.spawn_y = 0;
    level_start();
    return 0;
}

/* ---- FP_DEBUG_SELECT: debug level select ------------------------------------------------
 * Frame indices are the original's "Setting frame: N" values; names were verified from the
 * stage title card shown after GameManager::set_frame(N) in the running original
 * (tools/orig/goto_frame.sh, RUNTIME_MAP.md "Frame index table"). Stage numbers on the cards
 * depend on the character route, so only names are listed. */
typedef struct { const char *name; int act, frame; const char *dir; } sel_entry_t;
static const sel_entry_t SEL[] = {
    {"DRAGON VALLEY", 1, 20, "dv1"}, {"DRAGON VALLEY", 2, 21, "dv2"}, {"DRAGON VALLEY", 3, 22, "dv3"}, {"DRAGON VALLEY", 4, 23, "dv4"},
    {"AQUA TUNNEL", 1, 16, "at1"}, {"AQUA TUNNEL", 2, 17, "at2"}, {"AQUA TUNNEL", 3, 18, "at3"}, {"AQUA TUNNEL", 4, 19, "at4"},
    {"RELIC MAZE", 1, 24, "rm1"}, {"RELIC MAZE", 2, 25, "rm2"}, {"RELIC MAZE", 3, 26, "rm3"}, {"RELIC MAZE", 4, 27, "rm4"}, {"RELIC MAZE", 5, 28, "rm5"},
    {"FORTUNE NIGHT", 1, 29, "fn1"}, {"FORTUNE NIGHT", 2, 30, "fn2"}, {"FORTUNE NIGHT", 3, 31, "fn3"}, {"FORTUNE NIGHT", 4, 32, "fn4"}, {"FORTUNE NIGHT", 5, 33, "fn5"}, {"FORTUNE NIGHT", 6, 34, "fn6"},
    {"SKY BATTALION", 1, 35, "sb1"}, {"SKY BATTALION", 2, 36, "sb2"}, {"SKY BATTALION", 3, 37, "sb3"}, {"SKY BATTALION", 4, 38, "sb4"},
    {"JADE CREEK", 1, 39, "jc1"}, {"JADE CREEK", 2, 40, "jc2"}, {"JADE CREEK", 3, 41, "jc3"}, {"JADE CREEK", 4, 42, "jc4"}, {"JADE CREEK", 5, 43, "jc5"},
    {"THERMAL BASE", 1, 44, "tb1"}, {"THERMAL BASE", 2, 45, "tb2"}, {"THERMAL BASE", 3, 46, "tb3"}, {"THERMAL BASE", 4, 47, "tb4"}, {"THERMAL BASE", 5, 48, "tb5"}, {"THERMAL BASE", 6, 49, "tb6"},
    {"PANGU LAGOON", 1, 50, "pl1"}, {"PANGU LAGOON", 2, 51, "pl2"}, {"PANGU LAGOON", 3, 52, "pl3"}, {"PANGU LAGOON", 4, 53, "pl4"}, {"PANGU LAGOON", 5, 54, "pl5"}, {"PANGU LAGOON", 6, 55, "pl6"},
    {"TRAP HIDEOUT", 1, 56, "th1"}, {"TRAP HIDEOUT", 2, 57, "th2"}, {"TRAP HIDEOUT", 3, 58, "th3"},
    {"BATTLE GLACIER", 1, 59, "bg1"}, {"BATTLE GLACIER", 2, 60, "bg2"}, {"BATTLE GLACIER", 3, 61, "bg3"}, {"BATTLE GLACIER", 4, 62, "bg4"}, {"BATTLE GLACIER", 5, 63, "bg5"}, {"BATTLE GLACIER", 6, 64, "bg6"},
    {"FINAL DREADNOUGHT", 1, 65, "fd1"}, {"FINAL DREADNOUGHT", 2, 66, "fd2"}, {"FINAL DREADNOUGHT", 3, 67, "fd3"}, {"FINAL DREADNOUGHT", 4, 68, "fd4"},
    {"FINAL DREADNOUGHT", 5, 69, "fd5"}, {"FINAL DREADNOUGHT", 6, 70, "fd6"}, {"FINAL DREADNOUGHT", 7, 71, "fd7"}, {"FINAL DREADNOUGHT", 8, 72, "fd8"},
};
#define NSEL ((int)(sizeof SEL / sizeof SEL[0]))
#define SEL_ROWS 16
static uint8_t sel_have[NSEL];

static void sel_scan(void) {           /* levels.txt: one built level dir per line */
    size_t n; char *d = plat_load_file("levels.txt", &n);
    memset(sel_have, 0, sizeof sel_have);
    if (!d) return;
    for (size_t i = 0; i < n;) {
        size_t j = i; while (j < n && d[j] != '\n' && d[j] != '\r') j++;
        for (int k = 0; k < NSEL; k++)
            if (strlen(SEL[k].dir) == j - i && !memcmp(SEL[k].dir, d + i, j - i)) sel_have[k] = 1;
        i = j + 1;
    }
    free(d);
}

static void sel_update(const input_t *in) {
    if (G.sel_msg_t) G.sel_msg_t--;
    if (in->pressed & BTN_DOWN) G.sel = (G.sel + 1) % NSEL;
    if (in->pressed & BTN_UP) G.sel = (G.sel + NSEL - 1) % NSEL;
    if (in->pressed & BTN_RIGHT) G.sel = G.sel + SEL_ROWS < NSEL ? G.sel + SEL_ROWS : NSEL - 1;
    if (in->pressed & BTN_LEFT) G.sel = G.sel - SEL_ROWS >= 0 ? G.sel - SEL_ROWS : 0;
    if (G.sel < G.sel_top) G.sel_top = G.sel;
    if (G.sel >= G.sel_top + SEL_ROWS) G.sel_top = G.sel - SEL_ROWS + 1;
    if (in->pressed & (BTN_JUMP | BTN_START)) {
        if (!sel_have[G.sel]) { G.sel_msg_t = 90; return; }
        if (game_load_level(SEL[G.sel].dir) == 0) G.in_select = 0; else G.sel_msg_t = 90;
    }
}

static void sel_draw(void) {
    char buf[48];
    r_begin();
    r_rect(0, 0, SCREEN_W, SCREEN_H, 0xff101830);
    text_draw(12, 8, "DEBUG LEVEL SELECT", 0xffffe040, 2);
    text_draw(12, 24, "FRAME = ORIGINAL SET FRAME INDEX", 0xff8090b0, 1);
    for (int r = 0; r < SEL_ROWS && G.sel_top + r < NSEL; r++) {
        int i = G.sel_top + r;
        uint32_t c = i == G.sel ? 0xffffffff : sel_have[i] ? 0xffa0c0ff : 0xff505868;
        snprintf(buf, sizeof buf, "%s %d", SEL[i].name, SEL[i].act);
        if (i == G.sel) r_rect(8, 36 + r * 11, SCREEN_W - 16, 10, 0xff304070);
        text_draw(14, 38 + r * 11, buf, c, 1);
        snprintf(buf, sizeof buf, "F%d", SEL[i].frame);
        text_draw(170, 38 + r * 11, buf, c, 1);
        text_draw(206, 38 + r * 11, sel_have[i] ? "READY" : "NOT CONVERTED", c, 1);
    }
    text_draw(12, SCREEN_H - 14, G.sel_msg_t ? "LEVEL DATA NOT ON DISC" : "A START  UP DOWN  LEFT RIGHT PAGE", G.sel_msg_t ? 0xffff6060 : 0xff8090b0, 1);
    r_end();
}

int game_init(void) {
    memset(&G, 0, sizeof G);
    G.sfx.jump = snd_load("sfx_jump.wav");
    G.sfx.crystal = snd_load("sfx_crystal.wav");
    G.sfx.hit = snd_load("sfx_hit.wav");
    G.sfx.explode = snd_load("sfx_explode.wav");
    G.sfx.attack = snd_load("sfx_attack.wav");
    G.sfx.hurt = snd_load("sfx_hurt.wav");
    G.sfx.boost = snd_load("sfx_boost.wav");
#ifdef FP_DEBUG_SELECT
    G.debug_select = 1;
#endif
    const char *lv = FP_LEVEL;
#ifdef FP_HOST_ENV
    if (getenv("FP_SELECT")) G.debug_select = 1;
    if (getenv("FP_LEVEL")) lv = getenv("FP_LEVEL");
#endif
    music_play("stage1.pcm");
    if (G.debug_select) { sel_scan(); G.in_select = 1; return 0; }
    return game_load_level(lv);
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
    if (G.in_select) { sel_update(in); return; }
    if (in->pressed & BTN_START) G.paused = !G.paused;
    /* debug select: attack while paused returns to the level list */
    if (G.paused && G.debug_select && (in->pressed & BTN_ATTACK)) { G.paused = 0; sel_scan(); G.in_select = 1; return; }
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

/* ---- HUD: original GUI images (object bank anims hud_*), layout measured in the original at its
 * native 426x240 by template matching a screenshot (RUNTIME_MAP "HUD"). All coordinates = image top-left. */
static int hud_anim[6] = { -2 };
static void hud_img(int which, int frame, float x, float y, int w_override) {
    if (hud_anim[0] == -2) {
        static const char *nm[6] = { "hud_frame", "hud_fill", "hud_petals", "hud_icon", "hud_clock", "hud_digits" };
        for (int i = 0; i < 6; i++) hud_anim[i] = bank_anim(&G.objs, nm[i]);
    }
    int a = hud_anim[which];
    if (a < 0) return;
    const anim_t *an = &G.objs.anims[a];
    if (frame < 0) frame = 0;
    if (frame >= an->count) frame = an->count - 1;
    const frame_t *f = &G.objs.frames[G.objs.seq[an->first + frame]];
    int w = w_override >= 0 ? w_override : f->w;
    if (w <= 0) return;
    r_sprite(&G.objs.tex, f->u, f->v, w, f->h, x, y, w, f->h, 0, 0xffffffff);
}

static void hud_number(int v, int digits, float x, float y) {
    for (int i = digits - 1; i >= 0; i--) { hud_img(5, v % 10, x + i * 10, y, -1); v /= 10; }
}

void hud_draw(void) {
    const player_t *p = &G.p;
    /* energy meter: fill = guienergy alt A / 100 as x scale (RV en_xs); shine loop while full (RV en_img) */
    int full = p->energy >= PHYS_ENERGY_MAX;
    int fill_frame = full ? (G.frame / 2) % 20 : 0;      /* anim 1, speed 50 = one frame per 2 ticks */
    hud_img(0, 0, 8, 8, -1);                             /* frame interior is opaque: fill goes on top */
    hud_img(1, fill_frame, 16, 10, (int)(100.f * p->energy / PHYS_ENERGY_MAX + 0.5f));
    /* life petals: counter value = half-petals 0..14 (RV GUIRings_125, max 14) */
    if (p->hp > 0) hud_img(2, p->hp, 8, 19, -1);
    /* character head + crystal icon, lives (2 digits), crystals (3 digits) */
    hud_img(3, 0, 128, 8, -1);
    hud_number(p->lives < 0 ? 0 : p->lives > 99 ? 99 : p->lives, 2, 144, 8);
    hud_number(p->crystals < 0 ? 0 : p->crystals > 999 ? 999 : p->crystals, 3, 176, 8);
    /* clock + stage timer mm:ss:cc (RV GUITimerM/S/Milli counters; colons are part of the clock image) */
    hud_img(4, 0, 322, 0, -1);
    int cs = G.frame * 100 / FPS, mm = cs / 6000, ss = cs / 100 % 60;
    hud_number(mm > 99 ? 99 : mm, 2, 343, 8);
    hud_number(ss, 2, 369, 8);
    hud_number(cs % 100, 2, 395, 8);
    char buf[32];
    if (G.clear_t) text_draw(SCREEN_W / 2 - 66, 100, "STAGE CLEAR", 0xffffe040, 3);
    if (G.paused) text_draw(SCREEN_W / 2 - 34, 100, "PAUSED", 0xffffffff, 3);
#ifdef FP_PROFILE
    snprintf(buf, sizeof buf, "U%u D%u", (unsigned)G.prof_update_us, (unsigned)G.prof_draw_us);
    text_draw(8, SCREEN_H - 12, buf, 0xff80ff80, 1);
#else
    (void)buf;
#endif
}

void game_draw(void) {
    if (G.in_select) { sel_draw(); return; }
    r_begin();
    draw_parallax();
    draw_pieces(0, 2);
    objects_draw(0);
    player_draw(&G.p);
    draw_pieces(3, 7);
    hud_draw();
    r_end();
}
