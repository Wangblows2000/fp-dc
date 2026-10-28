/* Trap Hideout bosses (session 21b): private header of th_boss.c (spawn, lists, shared helpers, dispatch) and the encounter
 * files th_boss_iris.c (TH1 Laser Iris), th_boss_basher.c (TH3 Crystal Basher), th_boss_arena.c (TH3 arena).
 * Source: build/re/spec_th_bosses.md. Labels: RE read from code, RV measured in the original, INFERRED reasoned. */
#ifndef TH_BOSS_H
#define TH_BOSS_H
#include "th.h"
#include "sb_boss.h"                                                            /* the shared boss helpers (sbb_sin / cos, px / py, hurt, draw) */
#include <math.h>
#include <string.h>

#define THB_DEF 0x40                                                            /* enemy flag 6: defeated */
#define THB_ARM 0x20                                                            /* enemy flag 5: armoured */

/* ---- sprites (tools/import_frame.py TH_BOSS rows), looked up once per level */
enum { TS_IRIS, TS_IRIS8, TS_IRIS16, TS_IRIS24, TS_CAN, TS_CAN16, TS_WL, TS_WL16, TS_WR, TS_WR16, TS_LAS, TS_LAS1, TS_DUST,
       TS_DOOR, TS_CB_BASE, TS_CB_HEAD, TS_CB_SEG, TS_CB_SPIKE, TS_CB_SPIKE2, TS_CB_FACE,
       TS_N };
extern short thb_sa[TS_N];

/* ---- per-type instance lists (built once at spawn, in placement order) and the runtime objects */
#define THB_NT (OT_THB_PLACED_LAST - OT_THB_FIRST + 1)
#define THB_LMAX 80
typedef struct { short n, i[THB_LMAX]; } thb_list_t;
extern thb_list_t thb_l[THB_NT];
obj_t *thb_slot(int i);
obj_t *thb_get(int type, int k);                                                /* the k-th placed instance of a type (alive), NULL */
static inline int thb_count(int type) { return thb_l[type - OT_THB_FIRST].n; }
obj_t *thb_spawn(int type, int sk, float x, float y, int layer);               /* a runtime object (kept in the runtime list) */
void   thb_set_a(obj_t *o, int k);                                              /* force the row (restarts only when it changes) */
int    thb_a_is(const obj_t *o, int k);
int    thb_g3(void);                                                            /* global 3: the frame counter (g3 % n rules; INFERRED: the port's frame) */
void   thb_explode_snd(void);                                                   /* loop sound_explode<3..5>: 311 / 319 / 320 */
/* an Active's action point with its rotation (Active::get_action_x / y for an angled Active: the offset from the hot spot
 * turned by the angle, counter-clockwise; RV i1_c: the cannon's tip feeds the laser) */
void   thb_act_point(const obj_t *o, float *ax, float *ay);
/* the railings' arena end (TH1 13531; the same rule as DV1 4871, SB 9925 / 9597 / 9698 / 10112): two ObjectCrystal at every
 * MinibossRailing (y + 12, y + 36), sound 40, railings and barriers destroyed */
void   thb_arena_open(void);
int    thb_rails_alive(void);
void   thb_rails_kill(void);
float  thb_marker(int t, int x);                                                /* CameraLimitBoss L / R / T / B (x: 1 -> x, 0 -> y; -1e9 none) */                                                    /* the railings destroyed (TH3 14094: a restart past the Crystal Basher) */

/* ---- encounters (each returns at once when its classes are not placed) */
void thi_spawn(void), thi_frame(player_t *p); int thi_draw(const obj_t *o, int pass);   /* TH1 Laser Iris (th_boss_iris.c) */
void thc_spawn(void), thc_frame(player_t *p); int thc_draw(const obj_t *o, int pass);   /* TH3 Crystal Basher (th_boss_basher.c) */
void tha_spawn(void), tha_frame(player_t *p); int tha_draw(const obj_t *o, int pass);   /* TH3 arena (th_boss_arena.c) */
float tha_hud_dy(void);
int  tha_state(void), tha_left(void);                                           /* the goal's A (-1: no arena), the enemies left */
#endif
