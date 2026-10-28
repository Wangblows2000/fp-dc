/* Final Dreadnought stage script (session 24, stages 10-13 = rounds 1-4, two frames each, json 66-73 = fd1a fd1b fd2a fd2b fd3a fd3b fd4a
 * fd4b = frame index 65-72): the flow events that belong to no enemy / gimmick / boss family. Types 2560-2623 (fd.h).
 * Spec: build/re/spec_fd_flow.md (labels: RE read from the code, RV seen in the original (traces build/orig_traces/fd_flow/, rv1.sh),
 * INFERRED reasoned).
 * Structure (RE on_frame_66..73_start_events, handle_frame_66..73_events, handle_frame_9 inline): every character plays every round;
 * each round is its own stage (s 10-13) with its own results at the end of its b frame (handle_frame_67 / 69 / 71 / 73: 4560-4586,
 * 5394, the wipe 8547-8549 -> frame 8 -> I 10 -> 67, 11 -> 69, 12 -> 71, 13 -> 83 = the ending). The generic parts are level data
 * (levels.c rows fd1a-fd4b: music slots, the card, checkpoints, the exits, fd4a's exit hold, the route; importer: cards, parallax).
 * This file holds:
 *  - fd1a's entry (handle_frame_66 card group / 15298-15301): Lilac and Carol held left of the frame (X -40) through the card, then
 *    Sign.D counts the frames after it: explosions at the hangar wall every 7 frames while D < 160 (98 / 99 / 311), the wall breaks at
 *    D 160 (354), at D > 180 the HUD and timer come on and the player is thrown in (B 10); Milla (g1 4) is held at (128, -60) instead
 *    and falls in when the card ends (MV.G 5; Sign.D does not count for her);
 *  - the a frames whose original entry is a transporter ride from the left edge (RV fd2a / fd3a_start: MV held at -40, carried by the
 *    transporter network after the card): a fresh start places the player at the ride's end (INFERRED: the ride is not replayed).
 *  - FD3's loop-layer pieces (types 2560-2569; RE on_frame_70_init Q10 / Q11): their instance layer (json 3 = play layer, 4 = in
 *    front) and the off-map templates removed at the act start; gimmicks.c layers_refresh / layers_update make them solid by MV.P.
 * Not flow (other modules): the transporters, sirens and the rest of the gimmicks, the enemies, the bosses (they start the b frames'
 * results with results_begin_boss; fd4b's leads to the ending), the Adventure-only dialogs (DialogBox / DialogData, groups of the
 * cut-scene path) and power cards. */
#include "fd.h"
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#define FDLOG(...) do { if (getenv("FP_TRACE_SND")) { printf("fdflow "); printf(__VA_ARGS__); printf(" @%d\n", G.frame); } } while (0)   /* host tests */
#else
#define FDLOG(...) do { } while (0)
#endif

#define HOLD_X   -40           /* handle_frame_66 card group: MV.X -40 (Lilac / Carol) */
#define MILLA_X  128           /* g1 4: MV.X 128, MV.Y -60, MV.G 5 */
#define MILLA_Y  -60
#define WALL_D   160           /* 15300: Sign.D 160 -> the hangar wall breaks (sound 354) */
#define LAUNCH_D 180           /* 15301: Sign.D > 180 -> HUD / timer on, B 10 */

static struct { int act, entry, D, thrown; float y0; } F;

int fd_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage >= 10 && L->stage <= 13 ? (L->stage - 10) * 2 + L->act : 0; }

/* the end of the transporter ride that carries the player in from the left edge (RV frame 300: MV after landing), by act 1-8 */
static const struct { int act; float x, y; } RIDE[] = { { 3, 320, 690 }, { 5, 96, 394 } };

static int is_loop(int t) { return t >= OT_FDF_LOOP01 && t <= OT_FDF_LOOP15; }

void fd_flow_spawn(void) {
    memset(&F, 0, sizeof F);
    F.act = fd_flow_act();
    if (!F.act) return;
    int loops = 0;
    for (int i = 0; i < G.nobjs; i++) {                          /* FD3's loop pieces: the instance layer (lvobj anim, importer FD_FLOW) */
        obj_t *o = &G.o[i];
        if (!o->alive || !is_loop(o->type)) continue;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* the off-map templates */
        int li = o->t / 13;                                      /* objects_spawn: t = the lvobj index * 13 */
        if (li >= 0 && li < G.lvl.nobj && G.lvl.objs[li].type == o->type) o->layer = (G.lvl.objs[li].anim >> 8) & 0xff;
        loops = 1;
    }
    if (loops) layers_refresh(&G.p);                             /* the Q10 / Q11 pieces take MV.P now (3031 / 3032) */
    if (G.at_g4 != 1) return;                         /* a fresh start / restart of the act (walk-ins and checkpoints: g4 != 1) */
    player_t *p = &G.p;
    if (F.act == 1) {                                           /* fd1a's entry */
        F.entry = chr_is_milla(G.character) ? 2 : 1;
        F.y0 = F.entry == 2 ? (float)(MILLA_Y + ORIG_REF_DY) : p->y;
        p->x = F.entry == 2 ? MILLA_X : HOLD_X; p->y = F.y0; p->vx = p->vy = p->gsp = 0; p->ground = 0;
        FDLOG("entry %d", F.entry);
        return;
    }
    for (unsigned k = 0; k < sizeof RIDE / sizeof RIDE[0]; k++) if (RIDE[k].act == F.act) {
        p->x = RIDE[k].x; p->y = RIDE[k].y + ORIG_REF_DY; p->vx = p->vy = p->gsp = 0; p->ground = 0;
        FDLOG("ride end %d %d", (int)RIDE[k].x, (int)RIDE[k].y);
    }
}

static void hold(player_t *p) {                                 /* the timer group and control come on with the throw-in (15301) */
    p->x = F.entry == 2 ? MILLA_X : HOLD_X; p->y = F.y0; p->vx = p->vy = p->gsp = 0; p->ground = 0;
    G.timer_stop = 1; G.input_lock = 1;
}
static void milla_drop(player_t *p) {                           /* Milla: the card group ends, MV.G 5 drops her (RV: +5, +5.25, ...) */
    F.thrown = 1; p->vy = 5; G.timer_stop = 0; G.frame = 0; G.input_lock = 0;
    FDLOG("milla drop");
}

void fd_flow_card(player_t *p, int left) {
    if (!F.entry || F.thrown) return;
    if (left > 0) { hold(p); return; }
    if (F.entry == 2) milla_drop(p);
}

void fd_flow_frame(player_t *p) {
    if (F.entry == 2 && !F.thrown) milla_drop(p);              /* (a start without the card: at once) */
    if (F.entry == 1 && !F.thrown && p->x != HOLD_X) { F.thrown = 1; G.timer_stop = 0; G.input_lock = 0; }   /* moved away (a debug TP): no entry */
    if (F.entry == 1 && !F.thrown) {                            /* Lilac / Carol: Sign.D after the card */
        hold(p);
        F.D++;
        if (F.D < WALL_D && F.D % 7 == 1) {                     /* 15298 (D mod 7 = 1): a random explosion sound; 15299: explosions and */
            static const short SX[3] = { 98, 99, 311 };         /* debris at the wall (RV fd1a_start: 98 / 99 / 311 every 7 frames) */
            snd_play_orig(SX[cross_rand(3)], 220, 128);
            obj_explode((float)cross_rand(41), (float)(F.y0 - ORIG_REF_DY - 20 + cross_rand(41)));
            camera_shake(3);                                    /* INFERRED amplitude (RV cam_y swings 865..872) */
        }
        if (F.D == WALL_D) { snd_play_orig(354, 230, 128); obj_explode(16, F.y0 - ORIG_REF_DY); FDLOG("wall"); }   /* 15300 */
        if (F.D > LAUNCH_D) {                                   /* 15301: the HUD, pause and timer groups on; thrown in */
            F.thrown = 1; p->vx = 10; p->ground = 0; G.timer_stop = 0; G.frame = 0; G.input_lock = 0;
            FDLOG("thrown");
        }
    }
}
void  fd_flow_draw(int pass) { (void)pass; }
int   fd_flow_obj(obj_t *o, player_t *p) { (void)p; return is_loop(o->type); }   /* static pieces (solidity: gimmicks.c layers_refresh) */
float fd_flow_hud_dy(void) { return 0.f; }
