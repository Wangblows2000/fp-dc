/* Final Dreadnought bosses B (rounds 3-4): private header of fd_boss_r34.c (spawn, per-type lists, runtime objects, shared helpers,
 * dispatch) and the encounter files fd_boss_r34_3b.c (FD3b: the Kingdom Stone room, Brevon, the mutated Milla) and
 * fd_boss_r34_4b.c (FD4b: the final arena: Brevon's mech, his suit, Brevon, the end). Spec: build/re/spec_fd_bosses_b.md.
 * Labels: RE read from the decompiled events (build/re), RV measured in the original (build/orig_traces/fd_bosses_b), INFERRED. */
#ifndef FD_BOSS_R34_H
#define FD_BOSS_R34_H
#include "fd.h"
#include "sb_boss.h"                                                            /* shared boss helpers (sbb_sin / cos, px / py, hurt, draw) */
#include <math.h>
#include <string.h>

#define FDB_DEF 0x40                                                            /* enemy flag 6: defeated (combat.c 2764) */
#define FDB_ARM 0x20                                                            /* enemy flag 5: armoured */

/* ---- per-type lists of the placed objects (built once at spawn, in placement order; o->par = the sub id) */
#define FDB_NT (OT_FDB_LAST_USED - OT_FDB_STONE + 1)
#define FDB_LMAX 28
typedef struct { short n, i[FDB_LMAX]; } fdb_list_t;
extern fdb_list_t fdb_l[FDB_NT];
obj_t *fdb_slot(int i);
obj_t *fdb_get(int type, int k);                                                /* the k-th placed instance of a type (alive), NULL */
obj_t *fdb_get_sub(int type, int sub);                                          /* the first placed instance of a type with this sub id */
static inline int fdb_count(int type) { return fdb_l[type - OT_FDB_STONE].n; }

/* ---- sprite rows (tools/import_frame.py FD_BOSS B), looked up once per level; -1 when the act's bank lacks it */
int    fdb_row(const char *name);
void   fdb_set(obj_t *o, const char *row);                                      /* force a row (restarts only when it changes) */
void   fdb_restart(obj_t *o, const char *row);
int    fdb_is(const obj_t *o, const char *row);

/* ---- runtime objects (OT_FDB_RT, st = kind) */
enum { FDB_K_FX, FDB_K_BOUNCE, FDB_K_LASER, FDB_K_SPIN, FDB_K_AURA, FDB_K_SLASH, FDB_K_MINE, FDB_K_BEAM, FDB_K_WARN, FDB_K_KNIFE,
       FDB_K_CMILLA, FDB_K_WAVE, FDB_K_N };
obj_t *fdb_spawn(int kind, const char *row, float x, float y, int layer);
int    fdb_rt_n(void);
obj_t *fdb_rt(int k);                                                           /* the k-th live runtime object */

int    fdb_char(void);                                                          /* the character global (0 Lilac, 2 Carol, 4 Milla; Carol on the bike: 2) */
void   fdb_act_point(const obj_t *o, float *ax, float *ay);
int    fdb_g3(void);                                                            /* global 3: the frame counter (INFERRED: the port's frame) */
void   fdb_explode_snd(void);                                                   /* sound_explode<3..5>: 311 / 319 / 320 */
float  fdb_marker(int t, int x);                                                /* CameraLimitBoss L / R / T / B (x: 1 -> x, 0 -> y; -1e9 none) */
void   fdb_cam_lock(float x0, float x1, float y0, float y1);                    /* the boss bounds (CAM.M 2 style: easing to the limits) */
void   fdb_cam_free(void);

/* scripted input (CTRL.M = 1: the player's buttons are the script's) */
extern int fdb_ctrl;                                                            /* 1: the script holds the controls */
extern uint32_t fdb_keys;                                                       /* the buttons the script presses while fdb_ctrl */

/* ---- encounters (each returns at once when its classes are not placed) */
void f3b_spawn(void), f3b_frame(player_t *p); int f3b_draw(const obj_t *o, int pass); float f3b_hud_dy(void);
void f4b_spawn(void), f4b_frame(player_t *p); int f4b_draw(const obj_t *o, int pass); float f4b_hud_dy(void);
#ifdef FP_HOST_ENV
int  f3b_host_state(char *buf, int n), f4b_host_state(char *buf, int n);
#endif
#endif
