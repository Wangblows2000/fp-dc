/* Thermal Base stage script (session 21c, stage index 6 only): the flow events that belong to no enemy / gimmick / boss family.
 * Types 1792-1823 (tb.h). Spec: build/re/spec_tb_flow.md (labels: RE read from the code, RV seen in the original, INFERRED reasoned;
 * traces build/orig_traces/tb_flow/, rv1.sh).
 * Structure (RE on_frame_45..50_start_events, handle_frame_45..50_events, handle_frame_9 inline): tb1-tb6 = json 45-50 = frame index
 * 44-49. TB1 starts with the card (4307), TB2-TB6 with the walk-in (4490 / 4526); TB1-TB5 end at the right edge (4135 in TB1 / TB2,
 * its copy 12089 in TB3-TB5 -> 3578: next frame); TB6 is the boss arena, ended by the boss's results and the wipe (4560-4586, 5390,
 * 5394, 12566, 8547-8549). The generic parts are level data (levels.c rows tb1-tb6: music slots, cards, checkpoints, water, the
 * route); this file holds:
 *  - the lifts (LevelLift_2649 between a LevelLiftTop_2650 / LevelLiftBottom_2651 marker; RE 11709-11719 (TB1), 11919-11929 (TB2 /
 *    TB3), start 12030 / the inline copies in on_frame_45 / 46_start: a lift on a top marker starts at the top, Q 2):
 *    Q 0 at the bottom, 1 rising, 2 at the top, 3 sinking; flags 2 (stood on this frame), 4 (stopped, wait), 8 (started by the
 *    script), 0x40 (started once: never cleared, every lift rides once). The rider: grounded (MV.L 1), pad read (CTRL.M 0), the
 *    big bottom sensor on it, above it, |x - lift x| <= 31 (11710 / 11920). Moving: J -= / += 8 a frame (11714 / 11717), the
 *    grounded rider moved with it (MV.Y -= / += 8, sensors re-run: 11715 / 11718); at the marker J = its y, Q + 1 / Q 0, sound 49,
 *    CAM.Z += 3, flag 4, R 3 (11716 / 11719). RV tb1_start_lilac / _carol / _milla: rising from frame 162 (Q 1 at 161), 8 px a
 *    frame, at the top (2040) on frame 206, the player standing on it (MV.Y = lift top - 13.5). TB5's three lifts have no markers:
 *    the end elevator of the boss module (12525-12532, CameraTriggerBoss C 1) moves them; this file leaves them still.
 *  - TB1's start ride (11881, the card group 0x21a66, g4 != 0, GUIIcon A <= 0): every lift under the player gets flag 8, Q 1 (RV:
 *    frame 161 of the act-start card; the port's card is longer (192 frames), the ride is timed on the RV frame: tb_flow_card);
 *  - TBWallLightAnim_2644 (TB1 12, TB2 7): 11826 / 12091 force_frame((g3 / 4) mod 12) on every instance, every frame (scenery;
 *    the port's g3 is the stage frame counter, as TH's backlights).
 * Not flow (other modules): MetalDoorBack / Front_2641 / 2643 and the locked doors / keycards / terminals (11827-11838, 11802-11808:
 * the gimmick module's doors), NPCSyntaxEye_2682 (11839-11843), the water levers of TB5 (12491-12501: ML.B moves between 2920 and
 * 3240; level_water_set, levels.c), the Adventure-only branches (g0 = 2). */
#include "tb.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#define TBLOG(...) do { if (getenv("FP_TRACE_SND")) { printf("tbflow "); printf(__VA_ARGS__); printf(" @%d\n", G.frame); } } while (0)   /* host tests */
#else
#define TBLOG(...) do { } while (0)
#endif

#define LMAX 16
#define NWL 64
#define LIFT_V 8                      /* 11714 / 11717: J -/+ 8 a frame */
#define LIFT_GO 161                   /* 11881: RV tb1_start_*: Q 1 on frame 161 of the act-start card (GUIIcon A reaches 0) */
#define LF_ON 2                       /* lift flags (RE Fusion flags 1, 2, 3, 6) */
#define LF_WAIT 4
#define LF_SCRIPT 8
#define LF_USED 0x40
static struct {
    int act;                                   /* 1-6 in Thermal Base (stage index 6), else 0 */
    short lift[LMAX], top[LMAX], bot[LMAX];    /* lift slots and their marker slots (-1: none) */
    int nlift;
    short wl[NWL]; int nwl;                    /* TBWallLightAnim slots */
} T;

int tb_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 6 ? L->act : 0; }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int lflags(const obj_t *o) { return (int)o->av[1]; }
static void lset(obj_t *o, int f) { o->av[1] = f; }

/* the marker of type t overlapping the lift's 96-px box column (RE check_overlap lift / marker: the 64 x 16 marker at the lift's
 * x - 32, the lift's hot spot 48 px in) */
static int marker(const obj_t *l, int t) {
    int best = -1;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *m = &G.o[i];
        if (!m->alive || m->type != t || m->x + 64 <= l->x - 48 || m->x >= l->x + 48) continue;
        if (best < 0 || fabsf(m->y - l->y) < fabsf(G.o[best].y - l->y)) best = i;
    }
    return best;
}

void tb_flow_spawn(void) {
    memset(&T, 0, sizeof T);
    T.act = tb_flow_act();
    if (!T.act || !objects_have_from(OT_TBF_FIRST)) return;
    int an_l = ens_anim("tblift"), an_w = ens_anim("tbwl");
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_TBF_LIFT || o->type > OT_TBF_WALLLIGHT) continue;
        if (o->x < 0 || o->y < 0 || o->x > G.lvl.w || o->y > G.lvl.h) { o->alive = 0; continue; }   /* off-map templates (TB1's start lift
                                                                                                            stands on the frame's bottom edge, y 2400) */
        if (o->type == OT_TBF_LIFT && T.nlift < LMAX) {
            if (an_l >= 0) anim_set(&o->an, &G.ens, an_l);
            o->st = 0; o->tm = 0; o->av[0] = o->y; lset(o, 0);         /* Q 0, R 0, J = y */
            T.lift[T.nlift++] = (short)i;
        } else if (o->type == OT_TBF_WALLLIGHT && T.nwl < NWL) {
            if (an_w >= 0) anim_set(&o->an, &G.ens, an_w); else o->alive = 0;
            T.wl[T.nwl++] = (short)i;
        } else o->hidden = 1;                                           /* the markers: ctor set_visible(0) */
    }
    for (int k = 0; k < T.nlift; k++) {
        obj_t *l = &G.o[T.lift[k]];
        T.top[k] = (short)marker(l, OT_TBF_LIFTTOP); T.bot[k] = (short)marker(l, OT_TBF_LIFTBOT);
        if (T.top[k] >= 0 && fabsf(G.o[T.top[k]].y - l->y) < 1) l->st = 2;   /* 12030 / the start inline: on its top marker -> Q 2 */
    }
}

/* the player may trigger a lift: grounded, alive, the pad read (CTRL.M 0: no card, walk-in, act end, script lock) */
static int rider_ok(const player_t *p) {
    return p->ground && !dead(p) && !p->in_lock && G.card_t >= 192 /* game.c CARD_DONE */ && !G.act_in && !G.clear_t && !G.input_lock && !G.exit_walk;
}
/* the lift whose top the player's big bottom sensor touches (the rider tests of 11710 / 11715), -1 none */
static int under(const player_t *p) {
    int k = mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, OT_TBF_LIFT);
    for (int j = 0; j < T.nlift; j++) if (T.lift[j] == k) return j;
    return -1;
}
static void carry(player_t *p, int on, double dy) {          /* 11715 / 11718: MV.Y += dy, the sensors re-run */
    if (!on) return;
    p->y += dy; p->vy = 0;
    player_sync_sensors(p);
}
static void lift_stop(obj_t *o, int k, double y, int q) {   /* 11716 / 11719 */
    o->av[0] = y; o->st = q; lset(o, lflags(o) | LF_WAIT); o->tm = 3;
    snd_play_orig(49, 230, 128); camera_shake(3);
    TBLOG("lift %d stop %d", k, q);
}
static void lifts(player_t *p) {
    if (!T.nlift) return;
    int u = under(p), k = rider_ok(p) ? u : -1;
    int ride = p->ground && !dead(p);                                    /* the carry's rider: MV.L 1 on the lift (any control state) */
    for (int j = 0; j < T.nlift; j++) {
        obj_t *o = &G.o[T.lift[j]];
        if (!o->alive || (T.top[j] < 0 && T.bot[j] < 0)) continue;    /* no markers (TB5): the boss module's elevator */
        int f = lflags(o) & ~LF_ON;                                      /* 11709 */
        if (j == k && p->y - ORIG_REF_DY < o->y && fabs(p->x - o->x) <= 32 && !(o->st & 1)) f |= LF_ON;   /* 11710 (RE 31; INFERRED 32: a player pushing the TB2 shaft wall stands at +31.9) */
        if (!(o->st & 1) && o->tm > 0) o->tm--;                          /* 11711 */
        if ((f & LF_WAIT) && !(f & (LF_ON | LF_SCRIPT)) && o->tm <= 0 && !(o->st & 1)) f &= ~LF_WAIT;   /* 11712 */
        if ((f & LF_ON) && !(f & (LF_WAIT | LF_USED)) && !(o->st & 1)) { f |= LF_USED; o->st++; TBLOG("lift %d go %d", j, o->st); }   /* 11713 */
        lset(o, f);
        if (o->st == 1) {                                                /* 11714 - 11716 */
            o->av[0] -= LIFT_V; o->y = (float)o->av[0]; carry(p, ride && u == j, -LIFT_V);
            if (T.top[j] >= 0 && o->av[0] <= G.o[T.top[j]].y) lift_stop(o, j, G.o[T.top[j]].y, 2);
        } else if (o->st == 3) {                                         /* 11717 - 11719 */
            o->av[0] += LIFT_V; o->y = (float)o->av[0]; carry(p, ride && u == j, LIFT_V);
            if (T.bot[j] >= 0 && o->av[0] >= G.o[T.bot[j]].y) lift_stop(o, j, G.o[T.bot[j]].y, 0);
        }
        o->y = (float)o->av[0];                                          /* INFERRED: drawn at J (the original's N lags it a frame) */
    }
}

/* the act-start card (game.c card branch, after the player's update): 11881 starts the lifts under the player at the card's
 * GUIIcon end, then the lifts run as in play (the card group keeps the pad off) */
void tb_flow_card(player_t *p) {
    if (!T.act || !T.nlift) return;
    lifts(p);                                                         /* 11709-11719 run before 11881 (handle_frame_45 order) */
    if (G.card_t == LIFT_GO + 1 && G.at_g4 != 0) {                    /* card_t counts from 1 on the act's frame 0 */
        int k = under(p);
        if (k >= 0) { obj_t *o = &G.o[T.lift[k]]; lset(o, lflags(o) | LF_SCRIPT); o->st = 1; TBLOG("lift %d card start", k); }
    }
}

void tb_flow_frame(player_t *p) {
    if (!T.act) return;
    lifts(p);
    int g3 = G.frame;                                                 /* the port's g3: the stage frame counter */
    for (int k = 0; k < T.nwl; k++) { obj_t *o = &G.o[T.wl[k]]; if (o->alive && o->an.b) { o->an.frame = (g3 / 4) % 12; o->an.acc = 0; } }   /* 11826 / 12091 */
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBLIFT")) for (int k = 0; k < T.nlift; k++) { const obj_t *o = &G.o[T.lift[k]]; printf("tblift %d %d %.1f %.1f %d\n", G.frame, k, o->x, o->y, o->st); }
    if (getenv("FP_DBG_TBMUS")) { const char *m = level_music_slot(4); printf("tbmus4 %s\n", m ? m : "-"); }
#endif
}

int tb_flow_lift_state(int k) { return k >= 0 && k < T.nlift ? G.o[T.lift[k]].st : -1; }
void  tb_flow_draw(int pass) { (void)pass; }
int   tb_flow_obj(obj_t *o, player_t *p) { (void)p; return o->type >= OT_TBF_LIFT && o->type <= OT_TBF_WALLLIGHT; }   /* driven by tb_flow_frame */
float tb_flow_hud_dy(void) { return 0.f; }
