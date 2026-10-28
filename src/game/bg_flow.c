/* Battle Glacier stage script (session 24, stage index 9, json 60-65 = bg1-bg6 = frame index 59-64): the flow events that belong to no
 * enemy / gimmick / boss family. Types 2304-2335 (bg.h). Spec: build/re/spec_bg_flow.md (labels: RE read from the code, RV seen in the
 * original, INFERRED reasoned; traces build/orig_traces/bg_flow/, rv1.sh).
 * Structure (RE on_frame_60..65_start_events, handle_frame_60..65_events, handle_frame_9 inline, on_frame_85 / 22695): every character
 * plays Battle Glacier (no g1 write); Classic routes I 6 (g1 != 0), I 7 / I 8 (g1 = 0) to frame 84 (the shoot-'em-up lead-in, not
 * ported) whose end goes to frame 59 = BG1 with g4 0, and I 9 (BG6's results) to frame 65 = Final Dreadnought round 1 (levels.c ROUTE).
 * BG1 starts with the card (4307), BG2-BG6 with the walk-in (4477 / 6051 / 4526); BG1-BG5 end at the right edge (4135 -> 3578), BG6
 * with its boss's results and the wipe (4560 / 5394, 8547-8549 -> frame 8). The generic parts are level data (levels.c rows bg1-bg6:
 * music slots, cards 91-100, checkpoints, BG4's silent start, the route; results.c ACH_STAGE; importer: loop pieces, parallax).
 * This file holds:
 *  - the loop pieces' off-map templates removed at the act start and the loop layer taken at once (14345-14349 = 3031 / 3032);
 *  - BG4's opening: the act starts silent while g4 < 5 (14607 fadeout_music(2000); levels.c .music_fade_g4) and the opening Brevon
 *    mech's end (14502, flow part: g4 5 at (444, 1800) + loop_cp_16, play_music(5)) -> bg_flow_opening_cleared (called by bg_boss.c).
 * Not flow (other modules): the Brevon mech background of BG2 / BG3 (BrevonMechBackground_2444 and its shots, 14210-14216 /
 * 14308-14315), BG4's opening mech fight (14496 / 14502), Sparky, the Chopper, BG6's boss and its ending; the Adventure-only dialogs. */
#include "bg.h"
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#define BGLOG(...) do { if (getenv("FP_TRACE_SND")) { printf("bgflow "); printf(__VA_ARGS__); printf(" @%d\n", G.frame); } } while (0)   /* host tests */
#else
#define BGLOG(...) do { } while (0)
#endif

#define OPEN_G4 5                     /* 14502: the checkpoint after BG4's opening mech */
#define OPEN_X 444
#define OPEN_Y 1800

static struct { int act, dbg; } F;

int bg_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 9 ? L->act : 0; }

void bg_flow_spawn(void) {
    memset(&F, 0, sizeof F);
    F.act = bg_flow_act();
    if (!F.act) return;
    for (int i = 0; i < G.nobjs; i++) {                       /* the loop pieces' off-map templates (one of each class per frame) */
        obj_t *o = &G.o[i];
        if (o->alive && o->type >= OT_BGF_LOOP1 && o->type <= OT_BGF_LOOP3 && (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h)) o->alive = 0;
    }
    layers_refresh(&G.p);                                     /* the Q10 / Q11 pieces take MV.P now */
    BGLOG("act %d g4 %d", F.act, G.at_g4);
}

void bg_flow_opening_cleared(void) {
    if (F.act != 4 || G.at_g4 >= OPEN_G4) return;
    level_checkpoint(OPEN_G4, OPEN_X, OPEN_Y);               /* 14502: g4 5, g5 444, g6 1800, loop_cp_16 */
    const char *m = level_music_slot(5);
    if (m) music_play(m);                                     /* 14502: play_music(5) = m_stage9b (loop_play_music_5_62) */
    BGLOG("opening cleared");
}

void  bg_flow_frame(player_t *p) {
    (void)p;
#ifdef FP_HOST_ENV
    { const char *e = getenv("FP_DBG_BGOPEN");                 /* host stand-in for bg_boss.c's call: at this frame number */
      if (F.act == 4 && e && G.frame >= atoi(e) && !F.dbg) { F.dbg = 1; bg_flow_opening_cleared(); } }
#endif
}
void  bg_flow_draw(int pass) { (void)pass; }
int   bg_flow_obj(obj_t *o, player_t *p) { (void)p; return o->type >= OT_BGF_LOOP1 && o->type <= OT_BGF_LOOP3; }   /* static pieces */
float bg_flow_hud_dy(void) { return 0.f; }
