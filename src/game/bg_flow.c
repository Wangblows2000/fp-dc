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
#include "bg_boss.h"                         /* bgb_get: the BossSparky instances (the arrow sign) */
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

static struct { int act, dbg, arrow; } F;          /* arrow: BG4's BGArrowSign_3346 slot or -1 */
/* BG2's fly-by plane (small fixes; RE 14241 / 4874-4877, RV small fixes bg2_102 / c_jumppad_bg2: sound 102 at 120 for the teleport to
 * x 8330 at 60). 14241 (once, MV.X > 1130, MV.A != 4: not for Milla): a LevelBackgroundPlane2_2227 on json layer 0 at (view x - 320,
 * Y + 280), direction 4; A = its x, B = its y, C -5, F 4. 4874 (every frame): x = trunc((view x + A)(0.05 C + 0.7)), y = trunc((view y
 * + B)(0.05 C + 0.7)), A + 4 dir + 8 - 2 C (34), B + 0.25. 4875: x >= view x - 49 (once, its flag 0): sound 102. 4876: past it, F - 0.05
 * (to 0) and the camera's shake Z = min(3, Z + F). 4877: x > view x + 626: destroyed. Its anim 0 direction 4 loops 4 images (speed 100). */
static struct { int on, done, snd, frame; double A, B, C, Fs; int x, y; } PL;

int bg_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 9 ? L->act : 0; }

void bg_flow_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&PL, sizeof PL); }
    memset(&F, 0, sizeof F); F.arrow = -1; memset(&PL, 0, sizeof PL);
    F.act = bg_flow_act();
    if (!F.act) return;
    for (int i = 0; i < G.nobjs; i++) {                       /* the loop pieces' off-map templates (one of each class per frame) */
        obj_t *o = &G.o[i];
        if (o->alive && o->type >= OT_BGF_LOOP1 && o->type <= OT_BGF_ARROW && (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h)) o->alive = 0;
        if (o->alive && o->type == OT_BGF_ARROW && F.arrow < 0) { F.arrow = i; o->layer = 1; }   /* (json layer 2 of the mapped frame: layer 1) */
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

/* BG4's arrow sign (session 26; RE 14547, every frame of BG4's group): set_direction(the BossSparky list's size - 1, i.e. the Sparky
 * instances: 1 while it lives, 0 once it is destroyed); its anim 0 loops 12 frames in both directions (the frame is kept) */
static void arrow_frame(void) {
    obj_t *a = &G.o[F.arrow];
    if (!a->alive || a->type != OT_BGF_ARROW) { F.arrow = -1; return; }
    int n = 0;
    for (int k = 0; k < bgb_count(OT_BGB_SPARKY); k++) n += bgb_get(OT_BGB_SPARKY, k) != NULL;
    int an = ens_anim(n > 0 ? "bgf_arrow1" : "bgf_arrow0");
    if (an >= 0 && (a->an.b != &G.ens || a->an.anim != an)) { int fr = a->an.frame, acc = a->an.acc; anim_set(&a->an, &G.ens, an); a->an.frame = fr; a->an.acc = acc; }
    a->dir = n;
}
static void plane_frame(const player_t *p) {
    int vl = (int)G.camx, vt = (int)G.camy;
    if (PL.on) {                                                              /* 4874 */
        double k = 0.05 * PL.C + 0.7;
        PL.x = (int)((vl + PL.A) * k); PL.y = (int)((vt + PL.B) * k);
        PL.A += (4 << 2) + 8 - 2 * PL.C; PL.B += 0.25;
        if (PL.x >= vl - 49) {
            if (!PL.snd) { PL.snd = 1; snd_play_orig(102, 220, 128); }       /* 4875 */
            PL.Fs = PL.Fs - 0.05 > 0 ? PL.Fs - 0.05 : 0;                      /* 4876 */
            G.cam.Z = (float)(G.cam.Z + PL.Fs < 3 ? G.cam.Z + PL.Fs : 3);
        }
        if (PL.x > vl + 626) PL.on = 0;                                        /* 4877 */
        PL.frame = (PL.frame + 1) & 3;                                         /* speed 100: an image a frame */
    }
    if (!PL.done && p->x > 1130 && !chr_is_milla(G.character)                 /* 14241 (the view holding the player: after a */
        && p->x >= G.camx && p->x <= G.camx + SCREEN_W) {                     /* teleport the port's camera arrives a frame later) */
        PL.done = 1; PL.on = 1; PL.frame = 0;
        PL.x = vl - 320; PL.y = (int)(280 + (p->y - ORIG_REF_DY));
        PL.A = PL.x; PL.B = PL.y; PL.C = -5; PL.Fs = 4;
        BGLOG("plane at %d %d", PL.x, PL.y);
    }
}
void  bg_flow_frame(player_t *p) {
    if (F.act == 2) plane_frame(p);
    if (F.arrow >= 0) arrow_frame();
#ifdef FP_HOST_ENV
    { const char *e = getenv("FP_DBG_BGOPEN");                 /* host stand-in for bg_boss.c's call: at this frame number */
      if (F.act == 4 && e && G.frame >= atoi(e) && !F.dbg) { F.dbg = 1; bg_flow_opening_cleared(); } }
#endif
}
void  bg_flow_draw(int pass) {
    if (pass != 0 || !PL.on) return;                                          /* json layer 0, after its pieces */
    int a = ens_anim("bgf_plane");
    if (a < 0) return;
    const anim_t *an = &G.ens.anims[a];
    const frame_t *f = &G.ens.frames[G.ens.seq[an->first + PL.frame % (an->count ? an->count : 1)]];
    float sx = (float)PL.x - floorf(G.camx), sy = (float)PL.y - floorf(G.camy);
    if (sx < -260 || sx > SCREEN_W + 60 || sy < -260 || sy > SCREEN_H + 60) return;
    frame_draw(&G.ens, f, sx, sy, 0, 0xffffffff);
}
int   bg_flow_obj(obj_t *o, player_t *p) { (void)p; return o->type >= OT_BGF_LOOP1 && o->type <= OT_BGF_ARROW; }   /* static pieces, the arrow sign (bg_flow_frame) */
float bg_flow_hud_dy(void) { return 0.f; }
