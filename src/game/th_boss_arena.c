/* TH3 arena (session 21b): GUIGoalMessage_3217 (the goal and its state), GUIEnemiesLeft_3218 (the counter), THCrowd_3213 (the
 * cheering ninjas), the bike-ninja waves, and the hand-over to Spade (sb_spade.c, the SB2 boss's TH3 variant).
 * RE handle_frame_59 group 0x22dd0 (14047-14064), RV build/orig_traces/th_bosses/a1_c (Carol; teleported in at frame 60, the goal
 * forced to A 3 at 500, Spade's hitbox to A 0 at 1100). Labels: RE read from code, RV measured, INFERRED reasoned.
 * Goal alterables: A state (0 waiting, 1 the goal shown, 2 the counter, 3 cleared: the HUD slides away, 4 Spade), B the state
 * timer, C the drop timer, D the ninjas still to drop (ctor 99; Milla 15: 14050), K (enemies left) = dropped masks up + D.
 *  - 14047 / 14048 every 60 frames: every crowd ninja's animation speed 8 + R(30);
 *  - 14049 / 14050 X > 11650, Y > 670, A 0: A 1, the goal shown at the view's centre (frame 1 Carol "DEFEAT 99 NINJAS!", frame 2
 *    Milla, D 15), sound 363, every LevelTHCollapsingPlatform destroyed (the lid over the arena);
 *  - 14051 / 14052 A 1: B + 1; B > 120: A 2, frame 0 (the ninja icon), the counter shown, B 0;
 *  - 14053 / 14054 A >= 1: C + 1; C > 15, D > 0, fewer than 15 masks up: a warning + mask + bike at (12248, 312) (th_enemy.c
 *    th_enemy_bike_drop: the mask's F = x + (D mod 3 - 1) 128, E = 2 (D mod 2) - 1, D -2), D - 1, C 0 (RV: D 98 at 76 and every
 *    16 frames, the 15th at 300; the landings play sound 61);
 *  - 14057-14059 A 2, K <= 0: sound 363, the goal and the counter hidden, A 3, the music faded out (3000 ms) and locked;
 *  - 14060-14063 A 3: B + 1, the HUD slides up while GUIIcon.A < 40 (1 px a frame; RV a1_c f700: gone), B > 150: A 4 (RV 651),
 *    the music unlocked, ML.Y 1 (ControllerMusic: slot 4 = m_boss2 once g4 >= 5), Spade's group on (sbp_drop_start).
 * Not ported (INFERRED harmless): 14055 / 14056 (K < 20: a defeated ninja drops a life petal), the Adventure-only EnemyNinja_3193
 * props and dialogue (g0 2). Sound 363 also plays in Spade's fight (RV 413 / 999 / 1400 / 1743: the crowd, every ~400 frames, INFERRED
 * from the RV times: not ported). */
#include "th_boss.h"
#include "jc.h"                                                                 /* OT_JCG_COLL */
#include <stdio.h>
#include <stdlib.h>

#define AR_X0 11650                                                             /* 14049 */
#define AR_Y0 670
#define AR_DROPX 12248                                                          /* 14054: create at (0x2fd8, 0x138) */
#define AR_DROPY 312
#define AR_MAXUP 15

static struct { int on, A, B, C, D, K, shift, crowd_n; int ga, dg; } AR;

static obj_t *goal(void) { return thb_get(OT_THB_GOAL, 0); }
static obj_t *enleft(void) { return thb_get(OT_THB_ENLEFT, 0); }

void tha_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&AR, sizeof AR); }
    memset(&AR, 0, sizeof AR);
    obj_t *g = goal();
    if (!g) return;
    AR.on = 1; AR.D = 99;                                                       /* ctor D 99 */
    AR.ga = ens_anim("thb_goal"); AR.dg = bank_anim(&G.objs, "hud_digits");
    if (AR.ga >= 0) anim_set(&g->an, &G.ens, AR.ga);
    g->hidden = 1; g->layer = 6;
    obj_t *e = enleft();
    if (e) { e->hidden = 1; e->layer = 6; if (AR.ga >= 0) anim_set(&e->an, &G.ens, AR.ga); }   /* (drawn here: the HUD digits) */
    int ca = ens_anim("thb_crowd");
    AR.crowd_n = thb_count(OT_THB_CROWD);
    for (int k = 0; k < AR.crowd_n; k++) { obj_t *c = thb_get(OT_THB_CROWD, k); if (c && ca >= 0) { anim_set(&c->an, &G.ens, ca); c->c = 14; } }
}

static void kill_lids(void) {                                                    /* 14049: destroy LevelTHCollapsingPlatform (jc_gim.c member 2) */
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_JCG_COLL && o->par == 2) o->alive = 0; }
}

void tha_frame(player_t *p) {
    if (!AR.on) return;
    obj_t *g = goal(), *e = enleft();
    if (!g) return;
    if (thb_g3() % 60 == 0)                                                     /* 14047 / 14048 */
        for (int k = 0; k < AR.crowd_n; k++) { obj_t *c = thb_get(OT_THB_CROWD, k); if (c) c->c = 8 + cross_rand(30); }
    double X = sbb_px(p), Y = sbb_py(p);
#ifdef FP_HOST_ENV
    { const char *s = getenv("FP_THA_A3"); if (s && G.frame == atoi(s) && AR.A >= 1 && AR.A < 3) { AR.A = 3; AR.B = 0; } }   /* tests: the RV SETV goal A 3 */
#endif
    if (AR.A == 0 && X > AR_X0 && Y > AR_Y0) {                                  /* 14049 / 14050 */
        AR.A = 1; AR.B = 0; AR.C = 0;
        int milla = chr_is_milla(G.character);
        if (milla) AR.D = 15;
        g->hidden = 0; g->x = (float)(SCREEN_W / 2); g->y = 40;
        if (g->an.b) { g->an.frame = milla ? 2 : 1; g->an.done = 1; }
        snd_play_orig(363, 220, 128);
        kill_lids();
    }
    if (AR.A == 1) {                                                            /* 14051 / 14052 */
        AR.B += 1;
        if (AR.B > 120) { AR.A = 2; AR.B = 0; if (g->an.b) g->an.frame = 0; if (e) e->hidden = 0; }
    }
    int up = th_enemy_bikes_alive();
    if (AR.A >= 1) {                                                            /* 14053 / 14054 */
        AR.C += 1;
        if (AR.C > 15 && AR.D > 0 && AR.A <= 2 && up < AR_MAXUP) {
            AR.C = 0;
            double F = AR_DROPX + (fmod(AR.D, 3) - 1) * 128, E = 2 * fmod(AR.D, 2) - 1;
            if (th_enemy_bike_drop(AR_DROPX, AR_DROPY, F, E)) { AR.D -= 1; up += 1; }
        }
    }
    AR.K = up + AR.D;
    if (AR.A == 2 && AR.K <= 0) {                                               /* 14057-14059 */
        snd_play_orig(363, 220, 128);
        g->hidden = 1; if (e) e->hidden = 1;
        AR.A = 3; AR.B = 0;
        music_fade(180, 0);                                                     /* fadeout_music(3000) + lock_music */
    }
    if (AR.A == 3) {                                                            /* 14060-14063 */
        AR.B += 1;
        if (AR.shift < 40) AR.shift += 1;                                       /* loop_gui_shiftup while GUIIcon.A < 40 */
        if (AR.B > 150) { AR.A = 4; sbb_fight_start(); sbp_drop_start(); }      /* ML.Y 1 (slot 4), Spade's group 0x228ef */
    }
    g->av[0] = AR.A; g->av[1] = AR.B; g->av[2] = AR.C; g->av[3] = AR.D; g->av[4] = AR.K;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_THA")) {
        int na = 0, nc = 0; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive) { na++; nc += G.o[i].type == OT_CRYSTAL; }
        printf("tha %d A %d B %d C %d D %d K %d up %d shift %d sp %d objs %d cr %d\n", G.frame, AR.A, AR.B, AR.C, AR.D, AR.K, up, AR.shift, sbp_state(), na, nc);    }
#endif
}

int tha_state(void) { return AR.on ? AR.A : -1; }
int tha_left(void) { return AR.on ? AR.K : -1; }
float tha_hud_dy(void) { return AR.on ? -(float)AR.shift : 0.f; }

/* the goal / counter are GUI objects (layer 6): drawn at their view position. RV a1_c f200: the icon left of the view centre
 * (hot spot), the counter's two digits right-aligned to x 233, bottom 51 (the HUD digits: the same images, INFERRED) */
int tha_draw(const obj_t *o, int pass) {
    if (o->type == OT_THB_CROWD) return 0;                                      /* the generic sprite (json layer 1: the 0.8 pass) */
    if (o->type != OT_THB_GOAL && o->type != OT_THB_ENLEFT) return 0;
    if (pass != 2 || o->hidden || !AR.on) return 1;
    if (o->type == OT_THB_GOAL) { if (o->an.b) frame_draw(o->an.b, anim_frame(&o->an), o->x, o->y, 0, 0xffffffff); return 1; }
    if (AR.dg < 0) return 1;
    const anim_t *an = &G.objs.anims[AR.dg];
    int v = AR.K < 0 ? 0 : AR.K, n = v >= 10 ? 2 : 1; float x = 233;
    if (v > 99) { v = 99; n = 2; }
    for (int i = 0; i < n; i++) {
        const frame_t *f = &G.objs.frames[G.objs.seq[an->first + v % 10]];
        x -= f->w; frame_draw(&G.objs, f, x + f->hx, 51 - f->h + f->hy, 0, 0xffffffff); v /= 10;
    }
    return 1;
}
