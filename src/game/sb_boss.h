/* Sky Battalion bosses (session 19): private header of src/game/sb_boss.c (spawn, lists, shared rules, the plane) and the
 * encounter files sb_satellite.c (SB3), sb_kujacker.c (SB1), sb_chopper.c / sb_spade.c (SB2), sb_grappler.c (SB4).
 * Source: build/re/spec_sb_bosses.md. Labels: RE read from code, RV measured in the original, INFERRED reasoned. */
#ifndef SB_BOSS_H
#define SB_BOSS_H
#include "../fp.h"
#include <math.h>
#include <string.h>

#define SB_DEF 0x40                                                             /* enemy flag 6: defeated */
#define SB_ARM 0x20                                                             /* enemy flag 5: armoured (combat.c deflects) */
#define SB_RAD 57.29577951308232

/* ---- sprites (tools/import_frame.py SBBOSS_ENS), looked up once per level */
enum { SS_SAT, SS_SAT7, SS_SAT8, SS_SAT10, SS_ARM, SS_ARM7, SS_ARM8, SS_ARM10, SS_TOP, SS_THR, SS_BEAM2, SS_BEAM,
       SS_PLANE, SS_COP0, SS_COP2, SS_COP3, SS_COP4, SS_COMPASS, SS_DUST,
       SS_N };
extern short sbb_sa[SS_N];

/* ---- per-type instance lists (built once at spawn, in placement order) */
#define SBB_NT (OT_SBB_LAST - OT_SBB_FIRST + 1)
#define SBB_LMAX 12
typedef struct { short n, i[SBB_LMAX]; } sbb_list_t;
extern sbb_list_t sbb_l[SBB_NT];
obj_t *sbb_slot(int i);
obj_t *sbb_get(int type, int k);                                                /* the k-th placed instance of a type (alive), NULL */
static inline int sbb_count(int type) { return sbb_l[type - OT_SBB_FIRST].n; }

/* ---- shared helpers */
static inline int sbb_rand(int n) { return cross_rand(n); }
static inline double sbb_px(const player_t *p) { return floor(p->x); }
static inline double sbb_py(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static inline double sbb_nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static inline double sbb_sin(double deg) { double r = sbb_nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / SB_RAD); }
static inline double sbb_cos(double deg) { double r = sbb_nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / SB_RAD); }
static inline double sbb_fmod360(double v) { return fmod(v, 360.); }        /* Fusion's mod: keeps the sign of v */
void   sbb_set_a(obj_t *o, int k);                                              /* force_animation (restarts only if changed) */
void   sbb_restart_a(obj_t *o, int k);
int    sbb_a_is(const obj_t *o, int k);
obj_t *sbb_spawn(int type, int sk, float x, float y, int layer);
int    sbb_vulnerable(const player_t *p);                                       /* MV.R = 0, OTHERS.E <= 0, not dead */
int    sbb_hurt(player_t *p, int n);                                            /* MV.R = 1, LIFE.B += n (if vulnerable), CAM.Z >= 3 */
int    sbb_csens_hit(const player_t *p, const obj_t *o);                        /* PlayerCharacterSensor_375 on o */
int    sbb_p326_hit(const obj_t *o);                                            /* Player_326 (the main sensor rows) on o */
float  sbb_marker(int t, int x);                                                /* CameraLimitBoss L / R / T / B (x: 1 -> x, 0 -> y) */
int    sbb_rails(void), sbb_bars(void);                                         /* MinibossRailing / MinibossBarrier alive (the arena gates) */
void   sbb_draw(const obj_t *o, uint32_t col);                                  /* scaled / rotated / flashing sprite */
void   sbb_draw_yscaled(const obj_t *o, double ys);                             /* a beam: its image stretched down by ys */

/* ---- MasterLevel / music / camera / the arena end */
extern int sbb_mly;                                                             /* MasterLevel Y (1: a boss fight; the stage music switch) */
void   sbb_fight_start(void);                                                   /* ML.Y = 1, the boss track (ControllerMusic: play_music(4)) */
void   sbb_music_fadeout(void);                                                 /* fadeout_music(1500) + lock_music */
void   sbb_arena_end(int crystals, int flag);                                   /* 9925 / 9597 / 9698 / 10112: crystals, gates, compass, sb_flow_ship_done(flag) */
int    sbb_lock_edge(int *prev);                                                /* the flow's boss lock (CAM.M 1) switched on (1) / off (-1) this frame */
void   sbb_compass_show(void);

/* ---- encounters (each returns at once when its boss is not placed) */
void sbs_spawn(void), sbs_pre(player_t *p), sbs_frame(player_t *p); int sbs_draw(const obj_t *o, int pass);   /* SB3 satellite */
void sbk_spawn(void), sbk_pre(player_t *p), sbk_frame(player_t *p); int sbk_draw(const obj_t *o, int pass);   /* SB1 Kujacker */
void sbc_spawn(void), sbc_pre(player_t *p), sbc_frame(player_t *p); int sbc_draw(const obj_t *o, int pass);   /* SB2 chopper */
void sbc_pose_parts(int right);                                                 /* the chopper parts' directions (Spade's carrier) */
void sbp_spawn(void), sbp_pre(player_t *p), sbp_frame(player_t *p); int sbp_draw(const obj_t *o, int pass);   /* SB2 Spade */
void sbp_drop_start(void);                                                      /* TH3: the arena hands over (Spade drops in) */
int  sbp_state(void);                                                           /* the mask's F (-2: not started) */
void sbg_spawn(void), sbg_pre(player_t *p), sbg_frame(player_t *p); int sbg_draw(const obj_t *o, int pass);   /* SB4 grappler */
#endif
