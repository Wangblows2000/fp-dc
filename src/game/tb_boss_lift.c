/* TB5: the end elevator (three LevelLift_2649 without LevelLiftTop / Bottom markers, the CameraTriggerBoss_389 above them, the boss camera
 * box x 14888-15383, y 144-2727). RE handle_frame_49_events 12524-12532 (the flow module's lift events 11709-11719 need markers; these
 * do not: spec_tb_flow 2). No character test (RE).
 * Trigger C (alterable C of the CameraTriggerBoss): 0 waiting; 1 the ride: X > left marker + 96 -> CAM.M 1 (12524); every lift J - 3 a
 * frame, the camera's bottom F = lift y + 40 (12525); the player clamped to [left + 96, right - 96] and, airborne, lifted 3 px a frame
 * once below the trigger + 80 (12527 / 12529); the elites' glows and lasers in the shaft follow (12528: the enemies module's); the
 * player standing on a lift is carried (12530: the sensors re-run); the elites above the lift - 99 wake (12531: Q 1, the enemies
 * module's); 2 the top: a lift above the trigger -> every lift at the trigger's y, CAM.Z + 3, sound 51, CAM.M 2 (released), the music
 * fades out over 6 s and locks (12532). */
#include "tb_boss.h"
#include <stdio.h>
#include <stdlib.h>

static struct { int on, C; short lift[4], trig; int nlift; } E;
#define LIFT_V 3

void tbl_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&E, sizeof E); }
    memset(&E, 0, sizeof E); E.trig = -1;
    int nmark = 0;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_TBF_LIFTTOP || o->type == OT_TBF_LIFTBOT) nmark++;
        if (o->type == OT_CAMTRIGGER && E.trig < 0) E.trig = (short)i;
    }
    if (E.trig < 0) return;
    for (int i = 0; i < G.nobjs && E.nlift < 4; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_TBF_LIFT) continue;
        int marked = 0;                                                         /* a lift with its own stop markers is the flow module's */
        for (int j = 0; j < G.nobjs && nmark; j++) { const obj_t *m = &G.o[j]; if (m->alive && (m->type == OT_TBF_LIFTTOP || m->type == OT_TBF_LIFTBOT) && fabsf(m->x - o->x) <= 64) marked = 1; }
        if (marked || fabsf(o->x - G.o[E.trig].x) > 160) continue;
        E.lift[E.nlift++] = (short)i;
        o->av[0] = o->y;
    }
    E.on = E.nlift > 0;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBL")) printf("tblspawn trig %d (%.0f,%.0f) lifts %d marks %d\n", E.trig, E.trig >= 0 ? G.o[E.trig].x : 0, E.trig >= 0 ? G.o[E.trig].y : 0, E.nlift, nmark);
#endif
}

void tbl_frame(player_t *p) {
    if (!E.on) return;
    const obj_t *tr = tbb_slot(E.trig);
    if (!tr) return;
    double PX = sbb_px(p), PY = sbb_py(p);
    float L = tbb_marker(OT_CAMLIM_L, 1), R = tbb_marker(OT_CAMLIM_R, 1), T = tbb_marker(OT_CAMLIM_T, 0);
    if (E.C == 0 && PX > L + 96) { E.C = 1; level_lock_markers(); }             /* 12524 */
    if (E.C != 1) return;
    int dead = p->hp <= 0 || p->state == ST_DEAD;
    int on = -1;                                                                /* 12530: the lift under the player's big bottom sensor */
    if (!dead && p->ground) {
        int k = mv_sensor_obj(p->x, p->y - ORIG_REF_DY + 1, p->N, SENSOR_BOTTOMBIG, OT_TBF_LIFT);
        for (int j = 0; j < E.nlift; j++) if (E.lift[j] == k) on = j;
    }
    float ly = 0;
    for (int j = 0; j < E.nlift; j++) {                                         /* 12525 */
        obj_t *o = tbb_slot(E.lift[j]); if (!o) continue;
        o->av[0] -= LIFT_V; o->y = (float)o->av[0]; ly = o->y;
    }
    if (on >= 0) { p->y -= LIFT_V; p->vy = 0; player_sync_sensors(p); }        /* 12530 (the carry) */
    else if (!dead && !p->ground && PY > tr->y + 80) p->y -= LIFT_V;            /* 12529 */
    if (!dead && PY > tr->y + 80) {                                             /* 12527 */
        double x = fmin(fmax(PX, L + 96), R - 96);
        if (x != PX) { p->x = (float)x; if (p->ground) p->gsp = 0; else p->vx = 0; }
    }
    camera_boss_bounds(L, T, R, ly + 40);                                       /* 12525: CAM.F */
    if (ly < tr->y) {                                                           /* 12532 */
        for (int j = 0; j < E.nlift; j++) { obj_t *o = tbb_slot(E.lift[j]); if (o) { o->av[0] = tr->y; o->y = tr->y; } }
        E.C = 2; camera_shake(3); snd_play_orig(51, 220, 128);
        level_release_camera(0);
        music_fade(360, 0);                                                     /* fadeout_music(6000) + lock_music (6 s at 60 frames) */
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBL")) printf("tbl %d px %.0f py %.0f C %d ly %.0f on %d\n", G.frame, PX, PY, E.C, ly, on);
#endif
}
int tb_boss_lift_state(void) { return E.on ? E.C : -1; }
