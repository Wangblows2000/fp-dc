/* Pangu Lagoon stage script (session 23, stage index 7 only, Lilac): the flow events that belong to no enemy / gimmick / boss family.
 * Types 2048-2079 (pl.h). Spec: build/re/spec_pl_flow.md (labels: RE read from the code, RV seen in the original, INFERRED reasoned;
 * traces build/orig_traces/pl_flow/, rv1.sh).
 * Structure (RE on_frame_51..56_start_events, handle_frame_51..56_events, handle_frame_9 inline): pl1-pl6 = json 51-56 = frame index
 * 50-55, Lilac only (Classic I 6 -> 50 for g1 = 0; every start writes g1 = 0: level_def_t.lilac_only). PL1 starts with the card (4307),
 * PL2-PL6 with the walk-in (4489 / 6051 / 4526); PL1 / PL2 / PL4 / PL5 end at the right edge (4135 -> 3578: next frame); PL6 ends with
 * the Holo-Dragon's results and the wipe (13226, 13228-13230 -> frame 8). The generic parts are level data (levels.c rows pl1-pl6:
 * music slots, cards, checkpoints, PL3's door-room camera, the route; importer: the loop pieces, the parallax); this file holds:
 *  - the stone-door exits (RE 13009 + 12997-13006 in PL3, 13330 + 13231-13237 in PL6; group 0x22cf0 is on from the frame's init):
 *    once Player_326 stands between StoneDoor1 and StoneDoor2 (x), StoneDoor1's A < 0.4 (the door at least 60 % open: the gimmick
 *    module's switch / door, pl_flow_door_report), MV.M 0, grounded: stop the player (loop_stop_player), pause group off, 0x221a0 on.
 *    Then every frame: the HUD shifts up (12998), the player walks to StoneDoor1.x + 48 at 2 px a frame (13000 / 13001) and turns to
 *    the door (13002: skin animation 40, direction 24; the port keeps her standing: INFERRED), StoneDoor1.C + 1 (13003); at C = 120 a
 *    LevelCardFade2 with CTRL.M 1 (13004), its alpha + 4 a frame (13005); at alpha 255 (C > 120): PL3 g4 -1, next frame (13006 /
 *    handle_frame_53 inline = the shared act change); PL6 13236 (g4 < 3: checkpoint 3 at (4480, 208)) and 13237 (MV (6080, 24),
 *    CTRL.M 0, loop_start_player, group 0x22d3d: CAM C 4928, E / F the frame size, H -40 every frame (13331); 13332 removes the fade).
 *    Music: PL3 12997 stop_channel(31) at the start of the walk, 13007 fadeout_music(6000) on entering the door room; PL6 13330
 *    fadeout_music(2000). The port fades with the shared act-end fade (game.c act_fade_step: + 2 / + 4, INFERRED for 13005's + 4).
 *  - PL6's door room camera (13329: Player_326 between the CameraLimitBossLeft / Right markers and within 399 px above the bottom
 *    marker -> CAM.M 1 on entering; the arena's bounds replace it after the door).
 * Not flow (other modules): the stone doors / hanging switch (12987-12993, 13012: pl_gim.c reports the door to this file), the
 * Holo-Dragon fight and its ending (group 0x22d3e, 13333-13435: pl_boss.c), the Adventure-only dialog with Pangu (group 0x22d39,
 * 13247-13326: 13227 turns it on only for g0 = 2), PLWiiUSign_2995 (12845: shown only when the global W = 1; RV gv32 = 0: never). */
#include "pl.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#define PLLOG(...) do { if (getenv("FP_TRACE_SND")) { printf("plflow "); printf(__VA_ARGS__); printf(" @%d\n", G.frame); } } while (0)   /* host tests */
#else
#define PLLOG(...) do { } while (0)
#endif

#define DOOR_WAIT 120                 /* 13004 / 13006: StoneDoor1.C = 120 -> the fade */
#define DOOR_DX 48                    /* 13000-13002: the walk target StoneDoor1.x + 0x30 */
#define HUD_MAX 80                    /* 12998: loop_gui_shiftup_14 once a frame (the port stops once the HUD is out of view) */
/* PL6: the arena behind the door (RE 13236 / 13237 / 13331) */
#define ARENA_X 6080
#define ARENA_Y 24
#define ARENA_C 4928
#define ARENA_CP_X 4480
#define ARENA_CP_Y 208

enum { DS_ARMED, DS_WALK, DS_FADE, DS_DONE };
static struct {
    int act;                          /* 1-6 in Pangu Lagoon (stage index 7), else 0 */
    int st, c;                        /* door state, StoneDoor1.C */
    float x1, x2, a; int rep;         /* the door pair reported this frame (pl_gim.c): StoneDoor1 / 2 x, StoneDoor1 A; rep = G.frame + 1 */
    int hud, hud_back;                /* HUD shift up (12998), PL6: sliding back down after the door */
    int arena;                        /* PL6: past the door (group 0x22d3d) */
    int room_lock;                    /* PL6 13329: the door-room lock done (once per entry) */
    int cam_prev;                     /* PL3: level_cam_locked() of the last frame (13007's fadeout on entering) */
} D;

int pl_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 7 ? L->act : 0; }
int pl_flow_door_busy(void) { return D.act && (D.st == DS_WALK || D.st == DS_FADE); }
void pl_flow_door_report(float x1, float x2, float a) { D.x1 = x1; D.x2 = x2; D.a = a; D.rep = G.frame + 1; }

void pl_flow_spawn(void) {
    memset(&D, 0, sizeof D);
    D.act = pl_flow_act();
    if (!D.act) return;
    for (int i = 0; i < G.nobjs; i++) {                       /* the loop pieces' off-map templates (PL2 / PL3 x -360 / -488) */
        obj_t *o = &G.o[i];
        if (o->alive && o->type >= OT_PLF_LOOP01 && o->type <= OT_PLF_LOOP12 && (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h)) o->alive = 0;
    }
    layers_refresh(&G.p);                                     /* the Q10 / Q11 pieces take MV.P now (12747-12750 = 3031 / 3032) */
}

static int door_act(void) { return D.act == 3 || D.act == 6; }   /* group 0x22cf0 is switched on by on_frame_53 / 56_init only */

#ifdef FP_HOST_ENV
static void dbg_door(void) {                                  /* host tests: FP_DBG_PLDOOR="x1,x2,a" stands in for pl_gim.c's report */
    const char *e = getenv("FP_DBG_PLDOOR"); float x1, x2, a;
    if (e && D.rep != G.frame + 1 && sscanf(e, "%f,%f,%f", &x1, &x2, &a) == 3) pl_flow_door_report(x1, x2, a);
}
#endif

static void door_frame(player_t *p) {
    if (D.st == DS_ARMED) {                                   /* 13009 / 13330 */
        if (D.rep != G.frame + 1 || G.clear_t || G.act_in) return;
        if (!(p->x > D.x1 && p->x < D.x2 && D.a < 0.4f && p->state == ST_NORMAL && p->ground && p->hp > 0)) return;
        D.st = DS_WALK; D.c = 0;
        G.input_lock = 1; G.timer_stop = G.frame + 1;         /* loop_stop_player, CTRL.M; the pause / timer group 0x2205a off */
        p->gsp = p->vx = p->vy = 0;
        if (D.act == 3) music_stop();                         /* 12997: stop_channel(31) */
        else music_fade(120, 0);                              /* 13330: fadeout_music(2000) */
        PLLOG("door start x %.0f a %.2f", p->x, D.a);
        return;
    }
    if (D.st != DS_WALK && D.st != DS_FADE) return;
    if (D.hud < HUD_MAX) D.hud++;                             /* 12998 */
    D.c++;                                                    /* 13003 */
    float tx = D.x1 + DOOR_DX;                                /* 13000 / 13001: walk to the door (2 px a frame), 13002 there */
    if (fabs(p->x - tx) > 2.0) { p->face = p->x < tx ? 1 : -1; if (p->ground) p->gsp = p->x < tx ? 2 : -2; else p->vx = p->x < tx ? 2 : -2; }
    else { p->x = tx; p->gsp = 0; p->vx = 0; }
    if (D.st == DS_WALK && D.c >= DOOR_WAIT) {                /* 13004: the fade (CTRL.M 1) */
        D.st = DS_FADE; G.clear_t = 1; G.exit_b = 0; G.exit_to = NULL;
        PLLOG("door fade");
    }
    if (D.st == DS_FADE) {
        p->gsp = 0; p->vx = 0;                                /* (the shared fade's 3311 walk: the door holds her, 13002) */
        if (D.act == 6 && act_fade_step(G.clear_t) == 255) {  /* 13236 / 13237: into the arena instead of the next act */
            if (G.at_g4 < 3) { G.at_g4 = 3; G.at_g5 = ARENA_CP_X; G.at_g6 = ARENA_CP_Y;
                               G.act_lives = p->lives; G.act_crystals = p->crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals; }   /* loop_cp_16 */
            p->x = ARENA_X; p->y = ARENA_Y + ORIG_REF_DY; p->gsp = p->vx = p->vy = 0; p->ground = 0;
            G.clear_t = 0; G.input_lock = 0; G.timer_stop = 0;  /* loop_start_player, CTRL.M 0 (the fade goes with 13332) */
            D.st = DS_DONE; D.arena = 1; D.hud_back = 1;
            level_release_camera(0); camera_reset();
            G.cam_nC = ARENA_C; G.cam_nE = (float)G.lvl.w; G.cam_nF = (float)G.lvl.h; G.cam.H = -40.f;   /* 13331 */
            G.cam_m2 = 0; camera_update();
            PLLOG("arena g4 %d", G.at_g4);
        }
    }
}

void pl_flow_frame(player_t *p) {
    if (!D.act) return;
#ifdef FP_HOST_ENV
    dbg_door();
#endif
    if (D.act == 3) {                                         /* 13007: fadeout_music(6000) on entering the door room (CAM.M 1: levels.c CAM_PL3) */
        int lk = level_cam_locked();
        if (lk && !D.cam_prev && D.st == DS_ARMED) { music_fade(360, 0); PLLOG("room lock 3"); }
        D.cam_prev = lk;
    }
    if (D.act == 6 && !D.arena) {                             /* 13329: the door room's lock, once per entry */
        float l = 0, r = 0, b = 0; int have = 0;
        for (int i = 0; i < G.nobjs; i++) {
            const obj_t *o = &G.o[i];
            if (!o->alive) continue;
            if (o->type == OT_CAMLIM_L) { l = o->x; have |= 1; } else if (o->type == OT_CAMLIM_R) { r = o->x; have |= 2; } else if (o->type == OT_CAMLIM_B) { b = o->y; have |= 4; }
        }
        double Y = p->y - ORIG_REF_DY;
        int in = have == 7 && p->x > l && p->x < r && Y < b && Y >= b - 399;
        if (in && !D.room_lock && !level_cam_locked()) { level_lock_markers(); PLLOG("room lock"); }
        D.room_lock = in;
    }
    if (D.arena) {                                            /* 13331 (group 0x22d3d): the arena's bounds every frame */
        G.cam_nC = ARENA_C; G.cam_nE = (float)G.lvl.w; G.cam_nF = (float)G.lvl.h;
        if (D.hud > 0) D.hud--;                               /* the HUD returns (INFERRED: the boss module's 13353 shifts it down) */
    }
    if (door_act()) door_frame(p);
}

void  pl_flow_draw(int pass) { (void)pass; }
int   pl_flow_obj(obj_t *o, player_t *p) { (void)p; return o->type >= OT_PLF_LOOP01 && o->type <= OT_PLF_LOOP12; }   /* static pieces */
float pl_flow_hud_dy(void) { return D.act ? -(float)D.hud : 0.f; }
