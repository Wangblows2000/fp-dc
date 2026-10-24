/* Water in Dragon Valley 2 / 3 (build/re/spec_water.md: RE + four original DV3 traces).
 * MasterLevel A = 1, B = 2700 (5109 / 5314); C = B + 3 cos(1.5 t) is only the visual surface (4270).
 * Lilac: OTHERS.H = p->water, OTHERS.J = p->air, OTHERS.Y 26 = p->y26 (swim code). The constant tables (1543-1548)
 * are switched at the start of the next frame (phys_set_water from player_update). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fp.h"

phys_w_t phys_w = { PHYS_ACC_DRY, PHYS_TOP_DRY, PHYS_GRAV_DRY, PHYS_JUMP_DRY, PHYS_JUMP_CUT_DRY, 0.0334, 0.2, 15.0 };
void phys_set_water(int h) {
    if (h) phys_w = (phys_w_t){ 0.056875, 4.0, 0.15, 6.0, 2.0, 0.0217, 0.12, 15.0 };    /* 1545 / 1548 (no character test) */
    else phys_w = (phys_w_t){ PHYS_ACC_DRY, PHYS_TOP_DRY, PHYS_GRAV_DRY, PHYS_JUMP_DRY, PHYS_JUMP_CUT_DRY, 0.0334, 0.2, chr_is_carol(G.character) ? 12.0 : 15.0 };   /* 1543 / 1555 Carol (D 12) / 1547 */
}

#define WB water_line()
/* MasterLevel B per level: DV2 / DV3 2700 (5109 / 5314); Aqua Tunnel 1200 / 1200 / none / 1480 (4309 / 4478 / 4530 / 4685) */
static double water_line(void) { return !strcmp(G.lvdir, "at1") || !strcmp(G.lvdir, "at2") ? 1200.0 : !strcmp(G.lvdir, "at4") ? 1480.0 : 2700.0; }
#define NB 64
typedef struct { int alive, frame; double a, b, d; } bubble_t;
static struct { int icon_c, mus, mus_v, music_cut; int gx; bubble_t bub[NB]; int pa[4], pb[4]; } W;

int water_on(void) { return !strcmp(G.lvdir, "dv2") || !strcmp(G.lvdir, "dv3") || !strcmp(G.lvdir, "at1") || !strcmp(G.lvdir, "at2") || !strcmp(G.lvdir, "at4"); }
double water_c(void) { double a = fmod(1.5 * G.frame, 360.); return WB + 3 * ((a == 90. || a == 270.) ? 0. : fd_cos(a / 57.29577951308232)); }
void water_reset(void) { memset(&W, 0, sizeof W); W.mus_v = 100; snap_register(&W, sizeof W); }

static unsigned wseed = 0x2468ace;
static int wrand(int n) { wseed = wseed * 0x343fd + 0x269ec3; return (int)((((wseed >> 16) & 0x7fff) * (unsigned)n) >> 15); }
static void bubble(double x, double y, double d, int frame) {
    for (int i = 0; i < NB; i++) if (!W.bub[i].alive) { W.bub[i] = (bubble_t){ 1, frame, x, y, d }; return; }
}
static void splash(const player_t *p) {
    obj_effect((float)(int)p->x, (float)(int)water_c(), "splash");
    snd_play_orig(14, 220, 128);
}

/* late in the frame: 4150-4168 (entry/exit, air counter, countdown), 4396/4397 + 4464-4473 (bubble patches) */
void water_events(player_t *p) {
    if (!water_on()) return;
    double Y = p->y - ORIG_REF_DY;
    int dead = p->state == ST_DEAD, L = p->ground;
    if (!p->water && !dead && !L && Y >= WB + 8) {                        /* 4150 enter */
        p->water = 1; p->vy *= 0.2; p->vx *= 0.5; p->air = -30; splash(p);
        if (p->shield == 2) { p->shield = 0; p->shield_c = 0; snd_play_orig(11, 220, 128); }   /* 2691: the fire shield goes out */
    } else if (!p->water && dead && Y >= WB + 8) { p->water = 1; p->air = -30; }   /* 4151 */
    else if (p->water && !dead && !L && Y <= WB + 8) {                    /* 4152 leap + 4153 exit */
        if (p->y26) p->vy = -5;
        p->vy *= 1.3; splash(p); p->water = 0; p->air = -30; p->cyc_block = 18; p->cyclone_used = 0;
    } else if (p->water && dead && Y <= WB + 8) { p->water = 0; p->air = -30; }   /* 4154 */
    if (dead && p->water) { p->water = 0; p->air = 0; }                   /* 4155 */
    /* 4156: one "second" without air at t % 60 == 6, deeper than 8 px below the surface, no water shield */
    int t = G.frame;
    if (t % 60 == 6 && p->water && Y > (int)water_c() + 8 && p->shield != 4 && !p->drown) {
        p->air++;
        if (p->air == 5 || p->air == 10 || p->air == 15) snd_play_orig(18, 220, 128);   /* 4157-4159 */
    }
    if (p->air >= 20 && p->water && !W.music_cut) W.music_cut = 1;                    /* 4160: 6 s music fade */
    if (p->air < 20 && W.music_cut) { W.music_cut = 0; W.mus_v = 100; music_volume(100); }   /* 4164 */
    if (W.music_cut && W.mus_v > 0) { W.mus_v = (int)(100 - 100.0 * (++W.mus) / 360); if (W.mus_v < 0) W.mus_v = 0; music_volume(W.mus_v); }
    if (!W.music_cut) W.mus = 0;
    if (p->water && p->air >= 20 && p->air < 31) {                                    /* 4161 / 4162 */
        int v = 7 * p->air - 100; if (v > 100) v = 100;
        if (t % 120 == 6) snd_play_orig(41, v * 255 / 100, 128);
        if (t % 120 == 66) snd_play_orig(42, v * 255 / 100, 128);
    }
    if (p->water && p->air >= 32 && !p->drown && !dead) {                             /* 4163: drowned */
        p->drown = 1; p->state = ST_DEAD; p->dead_t = 0; p->ground = 0; p->vy = 0; p->vx = 0; p->hp = 0;
        music_stop();
    }
    /* bubble patches (DV3): PCS overlap -> J -= 1 per frame down to -30; J >= 20 -> 18 (4396 / 4397); timers 4464-4466 */
    int k = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_BUBBLEPATCH) continue;
        int px = (int)p->x, py = (int)Y;
        int over = !dead && px + 8 > o->x - 16 && px - 7 < o->x + 16 && py + 12 > o->y - 31 && py - 34 < o->y + 1;
        if (over && p->shield != 4) { if (p->air > -30) p->air--; if (p->air >= 20) p->air = 18; }
        if (k < 4) {
            if (++W.pa[k] > 15) { W.pa[k] = 0; bubble(o->x - 8 + wrand(17), o->y - 8, (int)(-10 - wrand(11)) * 0.1, 0); }
            if (++W.pb[k] > 50) { W.pb[k] = 0; bubble(o->x - 8 + wrand(17), o->y - 8, (int)(-10 - wrand(11)) * 0.1, 1 + wrand(3)); }
        }
        k++;
    }
    /* breath bubbles (4281 / 4468): every 48 frames a coin toss, from the mouth */
    if (t % 48 == 0 && p->water && !dead && Y > WB && wrand(2)) bubble(p->x - 2 + 4 * p->face + wrand(5), Y - 15, -1.0, 0);
    for (int i = 0; i < NB; i++) if (W.bub[i].alive) { W.bub[i].b += W.bub[i].d; if (W.bub[i].b < WB) W.bub[i].alive = 0; }   /* 4470 / 4473 */
    /* GUIAir slide 4166 / 4167 */
    if (p->water && W.icon_c < 16) W.icon_c++;
    if (!p->water && W.icon_c > 0) W.icon_c--;
}

/* GeneralWater (blend 20, dst * (1 - (64,48,8))) below the surface, behind the front tiles; extent INFERRED: the view
 * width below int(ML.C) (spec U2) */
void water_draw_tint(void) {
    if (!water_on()) return;
    float y = (float)(int)water_c() - G.camy;
    if (y < SCREEN_H) r_tint(0, y < 0 ? 0 : y, SCREEN_W, SCREEN_H - (y < 0 ? 0 : y), 0x403008);
}
/* GeneralWaterHorizon (additive strip 512x15, hotspot (0,7), x snapped to 64) and the bubbles */
void water_draw_front(void) {
    if (!water_on()) return;
    int a = ens_anim("horizon");
    float y = (float)(int)water_c() - G.camy;
    if (a >= 0 && y > -16 && y < SCREEN_H + 16) {
        animator_t an; memset(&an, 0, sizeof an); anim_set(&an, &G.ens, a);
        an.frame = (int)((long)G.frame * 18 / 100) % G.ens.anims[a].count;
        float x0 = (float)((int)G.camx - (int)G.camx % 64) - G.camx;
        r_set_additive(1);
        for (float x = x0; x < SCREEN_W; x += 512) frame_draw(an.b, anim_frame(&an), x, y, 0, 0xffffffff);
        r_set_additive(0);
    }
    int b = ens_anim("mbubble");
    if (b >= 0) for (int i = 0; i < NB; i++) if (W.bub[i].alive) {
        animator_t an; memset(&an, 0, sizeof an); anim_set(&an, &G.ens, b); an.frame = W.bub[i].frame;
        frame_draw(an.b, anim_frame(&an), (float)(int)W.bub[i].a - G.camx, (float)(int)W.bub[i].b - G.camy, 0, 0xffffffff);
    }
}
/* countdown digits over Lilac (BubbleNumbers 4272-4279) and the GUIAir counter (4168, 4170) */
void water_draw_hud(void) {
    const player_t *p = &G.p;
    if (!water_on()) return;
    if (p->air >= 20 && p->air < 32 && p->water) {
        static const char *dn[6] = { "bnum0", "bnum1", "bnum2", "bnum3", "bnum4", "bnum5" };
        int dir = 5 - (p->air - 20) / 2; if (dir < 0) dir = 0;
        int a = ens_anim(dn[dir]);
        if (a >= 0) {
            animator_t an; memset(&an, 0, sizeof an); anim_set(&an, &G.ens, a);
            an.frame = (G.frame / 2) % G.ens.anims[a].count;
            uint32_t col = 0xffffffff;
            if ((G.frame % 120) >= 66 || (G.frame % 120) < 6) r_set_invert(1);        /* shader 21 after 4162 */
            frame_draw(an.b, anim_frame(&an), (float)(int)p->x - G.camx, (float)(int)(p->y - ORIG_REF_DY) - 40 - G.camy, 0, col);
            r_set_invert(0);
        }
    }
    if (W.icon_c > 0) {
        int a = bank_anim(&G.objs, "hud_air");
        if (a >= 0) {
            double v = p->air < 20 ? fmax(1, 6 - 0.2 * p->air) : ((G.frame / 2) & 1);   /* 4168 / 4170 (blink) */
            if (v > 11) v = 11;
            const anim_t *an = &G.objs.anims[a];
            int fi = (int)v; if (fi >= an->count) fi = an->count - 1;
            const frame_t *f = &G.objs.frames[G.objs.seq[an->first + fi]];
            int tv; const tex_t *t = bank_frame_tex(&G.objs, f, &tv);
            r_sprite(t, f->u, tv, f->w, f->h, -16 + 2 * W.icon_c, 96, f->w, f->h, 0, 0xffffffff);
        }
    }
}
