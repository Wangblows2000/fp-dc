/* Battle Glacier bosses (session 24): private header of bg_boss.c (spawn, lists, shared helpers, dispatch) and the encounter files
 * bg_boss_sparky.c (BG4 Sparky boss, BG5 Milla's Sparky ride), bg_boss_mech.c (BG4 Absolution, the background mech's lasers of
 * BG2-BG4), bg_boss_dail.c (BG6 Prince Dail on the Shade Beast). Source: build/re/spec_bg_bosses.md.
 * Labels: RE read from the decompiled events, RV measured in the original (build/orig_traces/bg_bosses), INFERRED reasoned. */
#ifndef BG_BOSS_H
#define BG_BOSS_H
#include "bg.h"
#include "sb_boss.h"                                                            /* shared boss helpers (sbb_sin / cos, px / py, hurt, draw) */
#include <math.h>
#include <string.h>

#define BGB_DEF 0x40                                                            /* enemy flag 6: defeated (combat.c) */
#define BGB_ARM 0x20                                                            /* enemy flag 5: armoured */

/* ---- per-type instance lists of the placed classes (built once at spawn, in placement order) */
#define BGB_NT (OT_BGB_PLACED_LAST - OT_BGB_SPARKY + 1)
#define BGB_LMAX 16
typedef struct { short n, i[BGB_LMAX]; } bgb_list_t;
extern bgb_list_t bgb_l[BGB_NT];
obj_t *bgb_slot(int i);
obj_t *bgb_get(int type, int k);                                                /* the k-th placed instance of a type (alive), NULL */
static inline int bgb_count(int type) { return bgb_l[type - OT_BGB_SPARKY].n; }
obj_t *bgb_spawn(int type, const char *row, float x, float y, int layer);      /* a runtime object (kept in the runtime list) */
void   bgb_set(obj_t *o, const char *row);                                      /* force a sprite row (restarts only when it changes) */
void   bgb_set_i(obj_t *o, int anim);                                           /* ... by row index (ens_anim) */
int    bgb_is(const obj_t *o, const char *row);
int    bgb_g3(void);                                                            /* global 3: the frame counter (INFERRED: the port's frame) */
void   bgb_explode_snd(void);                                                   /* loop sound_explode<3..5>: 311 / 319 / 320 */
void   bgb_act_point(const obj_t *o, float *ax, float *ay);                     /* an Active's action point (hot-spot offset turned by the angle) */
int    bgb_runtime_n(void);
obj_t *bgb_runtime(int k);                                                      /* the k-th live runtime object (2510-2559) */
/* the miniboss arena end shared with DV1 4871 / TH1 13531 / SB 9925: two ObjectCrystal at every MinibossRailing (y + 12, y + 36),
 * sound 40, railings and barriers destroyed (only those inside [x0, x1]) */
void   bgb_arena_open(float x0, float x1);
float  bgb_marker(int t, int x);                                                /* CameraLimitBoss L / R / T / B (x: 1 -> x, 0 -> y; -1e9 none) */
int    bgb_milla(void);                                                         /* MV.A == 4 */

/* ---- encounters (each returns at once when its classes are not placed) */
void bgs_spawn(void), bgs_frame(player_t *p); int bgs_draw(const obj_t *o, int pass); int bgs_input(uint32_t *held); int bgs_player_hidden(void);
void bgm_spawn(void), bgm_frame(player_t *p); int bgm_draw(const obj_t *o, int pass); float bgm_hud_dy(void);
void bgd_spawn(void), bgd_frame(player_t *p); int bgd_draw(const obj_t *o, int pass); int bgd_input(uint32_t *held); float bgd_hud_dy(void);
int  bgd_player_hidden(void);
obj_t *bgm_bot_target(void), *bgd_bot_target(void);                           /* the hittable part now (host test bot) */
#endif
