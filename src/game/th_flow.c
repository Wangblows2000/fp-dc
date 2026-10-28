/* Trap Hideout stage script (session 21b, stage index 8 only): the flow events that belong to no enemy / gimmick / boss family.
 * Types 1536-1559. Spec: build/re/spec_th_flow.md (labels: RE read from the code, RV seen in the original, INFERRED reasoned;
 * traces build/orig_traces/th_flow/, rv1.sh).
 * Structure (RE on_frame_57..59_start_events, handle_frame_57..59_events, handle_frame_9 inline): th1-th3 = json 57-59 = frame
 * index 56-58, Carol / Milla only (Classic: Jade Creek's end, MenuSequence.I 5, sends g1 != 0 here and g1 = 0 to Thermal Base;
 * after Trap Hideout I 8 sends g1 != 0 to Thermal Base: levels.c level_route_next). The generic parts are level data (levels.c rows
 * th1-th3: music slots, cards, checkpoints, TH1's shaft camera, the boss results' wipe); this file holds:
 *  - TH1's start (13533): the player stays at the start point + 16 (269, -19), MV.G 0, while the act-start card runs with g4 = 1 and
 *    the LevelCardFade's B is below 96 (RV th1_start_carol / _milla: Y -19 for frames 0-119, B 96 and G 0.25 at frame 120: the fade's
 *    B counts from about frame 24), then falls down the shaft (RV: lands at frame 165, MV.Y 242.75);
 *  - the stage-wide scenery: ThBacklight1_3113 (13532 and its handle_frame_57 copy: frame = (g3 / 10) mod 6 every frame; the port's
 *    g3 is the stage frame counter), ThFlame_3176 (a torch flame: its own looping animation, speed 16; 13691 / 13692 / 14084-14087
 *    only stop it during the pause), ThSpotlight_3209 (TH3, json layer 0, invisible from its constructor: 14099 stores A = x, B = y,
 *    C = 60 x its index newest first; 14082 every frame x = view left 0.85 + A, y = view top 0.88 + B, angle = 45 sin(1.5 g3 + C)
 *    degrees; 14083 once g4 >= 5 (the Crystal Basher beaten): the spotlights and LevelParallaxPiece2_3208 shown, piece0_3192 hidden;
 *    14099 hides piece2 at the act start);
 *  - TH3's Crystal Basher end (13931, flow part): g4 5, restart (560, 170), the camera released (th_flow_basher_cleared, called by
 *    the boss module, which owns the fight, the railings' crystals and the sound).
 * The th3 arena (GUIGoalMessage_3217 / GUIEnemiesLeft_3218, ThCrowd_3213, the bike-ninja waves: group 0x22dd0, 14047-14064) and the
 * dialog / Adventure scenes (13796-13877) are not flow elements: the arena belongs to the boss module; the Adventure-only branches
 * (g0 = 2: 13796, 13797 / 13798, the dialog 13799-13848) are skipped like jc_flow.c / sb_flow.c. */
#include "th.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#define THLOG(...) do { if (getenv("FP_TRACE_SND")) { printf("thflow "); printf(__VA_ARGS__); printf(" @%d\n", G.frame); } } while (0)   /* host tests */
#else
#define THLOG(...) do { } while (0)
#endif

#define TH1_HOLD 120          /* 13533: G = 0 while LevelCardFade B < 96 (RV: B reaches 96 at frame 120 of the act; the fall starts there) */
#define TH3_PIECE0 3192       /* LevelParallaxPiece0_3192 (hidden by 14083) */
#define TH3_PIECE2 3208       /* LevelParallaxPiece2_3208 (hidden by 14099, shown by 14083) */
#define NLIST 96
static struct {
    int act;                          /* 1-3 in Trap Hideout (stage index 8), else 0 */
    double y0;                        /* TH1: the held MV position */
    short bl[NLIST], fl[NLIST], sp[16]; int nbl, nfl, nsp;   /* per-type lists (object slots) */
    int spots;                        /* 14083 done */
} T;

int th_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 8 ? L->act : 0; }

/* the instance's lvobj anim word (the importer: layer << 8 | 0x8000 json layer 1 of a mapped frame) */
static int lvobj_anim(const obj_t *o, int k) {
    int n = 0;
    for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == k) return G.lvl.objs[j].anim; }
    return 0;
}

void th_flow_spawn(void) {
    memset(&T, 0, sizeof T);
    T.act = th_flow_act();
    if (!T.act || !objects_have_from(OT_THF_FIRST)) return;
    static const char *const ROW[3] = { "thbl", "thfl", "thsp" };
    int seen[3] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_THF_BACKLIGHT || o->type > OT_THF_SPOT) continue;
        int k = o->type - OT_THF_BACKLIGHT, a = lvobj_anim(o, seen[k]++);
        o->layer = (a & 0x8000) ? -1 : (a >> 8) & 0x7f; o->an.b = NULL;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        int an = ens_anim(ROW[k]);
        if (an < 0) { o->alive = 0; continue; }
        if (k == 2) {                                 /* json layer 0: drawn by the parallax hook, not objects_draw (no animator) */
            if (T.nsp < 16) { T.sp[T.nsp++] = (short)i; o->par = an; o->fx = o->x; o->fy = o->y; }   /* 14099: A = x, B = y */
            continue;
        }
        anim_set(&o->an, &G.ens, an);
        if (k == 0 && T.nbl < NLIST) T.bl[T.nbl++] = (short)i;
        if (k == 1 && T.nfl < NLIST) T.fl[T.nfl++] = (short)i;
    }
    for (int k = 0; k < T.nsp; k++) G.o[T.sp[k]].av[0] = 60.0 * (T.nsp - 1 - k);   /* 14099: C = 60 x the index, newest instance first */
    if (T.act == 3) { par_set_hidden(TH3_PIECE2, 1); par_set_hook(TH3_PIECE2, th_flow_spot_draw); }   /* 14099 */
}

/* TH1's start (13533, see the header). The card branch of game.c runs player_update without input, then this */
void th_flow_card(player_t *p, int left) {
    (void)left;
    if (T.act != 1 || G.at_g4 != 1) return;
    if (G.card_t <= TH1_HOLD) {
        if (!T.y0) T.y0 = p->y;
        p->y = (float)T.y0; p->vy = 0; p->vx = 0; p->gsp = 0; p->ground = 0;
        if (G.card_t == TH1_HOLD) THLOG("th1 fall");
    }
}

void th_flow_frame(player_t *p) {
    if (!T.act) return;
    int g3 = G.frame;                                                               /* the port's g3: the stage frame counter */
    for (int k = 0; k < T.nbl; k++) { obj_t *o = &G.o[T.bl[k]]; if (o->alive && o->an.b) { o->an.frame = (g3 / 10) % 6; o->an.acc = 0; } }   /* 13532 */
    for (int k = 0; k < T.nfl; k++) {                                               /* speed 16: a frame per 100 / 16 ticks, everywhere (json layer 1:
                                                                                       the far-object sleep of update_objects tests world x) */
        obj_t *o = &G.o[T.fl[k]];
        if (o->alive && o->an.b) { int n = o->an.b->anims[o->an.anim].count; o->an.frame = n > 0 ? (g3 * 16 / 100) % n : 0; o->an.acc = 0; }
    }
    /* handle_frame_59 inline (group 0x22dd0, before 14047): g4 >= 5 and MV.X < 11650 -> CAM.F = the frame's height (RE). The Crystal
     * Basher's door raised CAM.F to the door (13886, th_boss_basher.c); without this the camera stayed above the slope after the fight
     * (session 21d user report: the camera stopped following Carol / Milla). The X >= 11650 branch (the arena's F 776 / rising D) is
     * not ported (the arena works without it) */
    if (T.act == 3 && G.at_g4 >= 5 && p && p->x < 11650) G.cam_nF = (float)G.lvl.h;
    if (T.act == 3 && !T.spots && G.at_g4 >= 5) {                                    /* 14083 (once: the repeat guard) */
        T.spots = 1;
        par_set_hidden(TH3_PIECE0, 1); par_set_hidden(TH3_PIECE2, 0);
        THLOG("th3 spotlights");
    }
#ifdef FP_HOST_ENV
    if (T.act == 3 && getenv("FP_DBG_THPAR")) printf("thpar %d %d %d\n", G.frame, par_hidden(TH3_PIECE0), par_hidden(TH3_PIECE2));   /* tests */
    if (getenv("FP_DBG_THMUS")) { const char *m = level_music_slot(4); printf("thmus4 %s\n", m ? m : "-"); }
#endif
}

/* 14082, drawn in the parallax order (par_set_hook: before piece2). The spotlights are json layer 0 objects: not in objects_draw */
void th_flow_spot_draw(void) {
    if (!T.spots) return;
    int vl = (int)G.camx, vt = (int)G.camy;
    for (int k = 0; k < T.nsp; k++) {
        const obj_t *o = &G.o[T.sp[k]];
        if (!o->alive) continue;
        int x = (int)(vl * 0.85 + o->fx) - vl, y = (int)(vt * 0.88 + o->fy) - vt;
        double a = G.frame * 1.5 + o->av[0];
        float ang = a != 180 ? (float)(sin(a / 57.29577951) * 45) : 0;
        if (x < -200 || x > SCREEN_W + 200 || y < -200 || y > SCREEN_H + 200) continue;
        animator_t t; memset(&t, 0, sizeof t); anim_set(&t, &G.ens, o->par);
        (void)t.frame;                                                                 /* its 2 frames (speed 25) are the same image: frame 0 */
        frame_draw_rot(&G.ens, anim_frame(&t), (float)x, (float)y, ang, 0, 0xffffffff);
    }
}

void th_flow_basher_cleared(void) {
    if (T.act != 3) return;
    G.at_g4 = 5; G.at_g5 = 560; G.at_g6 = 170;                                       /* 13931 (the restart point after the Crystal Basher) */
    G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
    level_release_camera(0);                                                         /* CAM.M 2, U 0 */
    THLOG("th3 basher cleared");
}

void  th_flow_draw(int pass) { (void)pass; }
int   th_flow_obj(obj_t *o, player_t *p) { (void)p; return o->type >= OT_THF_BACKLIGHT && o->type <= OT_THF_SPOT; }   /* driven by th_flow_frame */
float th_flow_hud_dy(void) { return 0.f; }
