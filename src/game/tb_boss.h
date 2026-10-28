/* Thermal Base bosses (session 21c): private header of tb_boss.c (spawn, lists, shared helpers, dispatch) and the encounter files
 * tb_boss_shrimp.c (TB1 RoboShrimp), tb_boss_pusher.c (TB2 wall pusher), tb_boss_spider.c (TB3 Syntax Spider), tb_boss_squid.c
 * (TB3 RoboSquid), tb_boss_syntax.c (TB6 arena). Source: build/re/spec_tb_bosses.md. Labels: RE read from code, RV measured in the
 * original (build/orig_traces/tb_bosses), INFERRED reasoned. */
#ifndef TB_BOSS_H
#define TB_BOSS_H
#include "tb.h"
#include "sb_boss.h"                                                            /* the shared boss helpers (sbb_sin / cos, px / py, hurt, draw) */
#include <math.h>
#include <string.h>

#define TBB_DEF 0x40                                                            /* enemy flag 6: defeated */
#define TBB_ARM 0x20                                                            /* enemy flag 5: armoured */

/* ---- sprites (tools/import_frame.py TB_BOSS rows), looked up once per level */
enum { TS_SH_HEAD, TS_SH_HEAD8, TS_SH_SEG, TS_SH_SEG8, TS_SH_SEG9, TS_SH_TAIL, TS_SH_TAIL9, TS_SH_LT0, TS_SH_LT1,
       TS_TRAP, TS_WP, TS_WP16,
       TS_EXP6, TS_SHADE,
       TS_SX_BACK, TS_SX_SHR, TS_SX_SHR7, TS_SX_SHL, TS_SX_SHL7, TS_SX_HEAD, TS_SX_BODY, TS_SX_BODY8, TS_SX_CLAWR, TS_SX_CLAWL,
       TS_SX_ARML, TS_SX_ARMR, TS_SX_HIT, TS_SX_HIT8, TS_SX_LASER, TS_SX_SPH, TS_SX_SPH2, TS_SX_SPL1, TS_SX_SPL2, TS_PLAT,
       TS_SQ, TS_SQ8, TS_SQ10, TS_SQ11, TS_SQG, TS_SQG1, TS_SQJ, TS_SQJ7, TS_SQARM, TS_SQHB,
       TS_SP_HEAD0, TS_SP_HEAD16, TS_SP_HEAD24, TS_SP_HEAD7, TS_SP_HEAD8_0, TS_SP_HEAD8_16, TS_SP_HEAD8_24, TS_SP_HEAD11, TS_SP_BODY0, TS_SP_BODY16, TS_SP_BODY24, TS_SP_BODY8_0, TS_SP_BODY8_16, TS_SP_BODY8_24, TS_SP_CAN0, TS_SP_CAN16, TS_SP_CAN24, TS_SP_CAN1, TS_SP_CAN17, TS_SP_CAN7_0, TS_SP_CAN7_16, TS_SP_ARM0, TS_SP_ARM16, TS_SP_LEGF0, TS_SP_LEGF16, TS_SP_LEGB0, TS_SP_LEGB16, TS_SP_LINE, TS_SP_SPL1, TS_SP_SPL2, TS_SP_LF2, TS_SP_LAUNCH, TS_SP_LAUNCH7, TS_SP_LAUNCH8,
       TS_N };
extern short tbb_sa[TS_N];

/* ---- per-type instance lists (built once at spawn, in placement order) and the runtime objects */
#define TBB_NT (OT_TBB_PLACED_LAST - OT_TBB_SHRIMP + 1)
#define TBB_LMAX 32
typedef struct { short n, i[TBB_LMAX]; } tbb_list_t;
extern tbb_list_t tbb_l[TBB_NT];
obj_t *tbb_slot(int i);
obj_t *tbb_get(int type, int k);                                                /* the k-th placed instance of a type (alive), NULL */
static inline int tbb_count(int type) { return tbb_l[type - OT_TBB_SHRIMP].n; }
obj_t *tbb_spawn(int type, int sk, float x, float y, int layer);               /* a runtime object (kept in the runtime list) */
void   tbb_set_a(obj_t *o, int k);                                              /* force the row (restarts only when it changes) */
int    tbb_a_is(const obj_t *o, int k);
int    tbb_g3(void);                                                            /* global 3: the frame counter (g3 % n rules; INFERRED: the port's frame) */
void   tbb_explode_snd(void);                                                   /* loop sound_explode<3..5>: 311 / 319 / 320 */
void   tbb_act_point(const obj_t *o, float *ax, float *ay);                     /* an Active's action point, turned by its angle */
float  tbb_marker(int t, int x);                                                /* CameraLimitBoss L / R / T / B (x: 1 -> x, 0 -> y; -1e9 none) */
/* the gates' arena end (TB1 11878, TB2 12009; the rule of DV1 4871, SB 9925, TH1 13531): two ObjectCrystal at every railing of
 * the given type (y + 12, y + 36, sent behind: move_back), sound 40, the railings and the MinibossBarriers destroyed */
void   tbb_gates_open(int rail_type);
int    tbb_rails_alive(int rail_type);
/* a placed object (any type) whose collision box overlaps o's: the cross-module "overlap" of the RE (switches of the gimmick module) */

/* ---- encounters (each returns at once when its classes are not placed) */
void tbs_spawn(void), tbs_frame(player_t *p); int tbs_draw(const obj_t *o, int pass);   /* TB1 RoboShrimp (tb_boss_shrimp.c) */
void tbw_spawn(void), tbw_frame(player_t *p); int tbw_draw(const obj_t *o, int pass);   /* TB2 wall pusher (tb_boss_pusher.c) */
void tbx_spawn(void), tbx_frame(player_t *p); int tbx_draw(const obj_t *o, int pass);   /* TB6 arena (tb_boss_syntax.c) */
float tbx_hud_dy(void);
void tbq_spawn(void), tbq_frame(player_t *p); int tbq_draw(const obj_t *o, int pass);
void tbp_spawn(void), tbp_frame(player_t *p); int tbp_draw(const obj_t *o, int pass);   /* TB3 Syntax Spider (tb_boss_spider.c) */
extern uint32_t tbb_held;                                                       /* this frame's held buttons (tb_boss_input) */   /* TB3 RoboSquid (tb_boss_squid.c) */
void tbl_spawn(void), tbl_frame(player_t *p);                                  /* TB5 end elevator (tb_boss_lift.c) */
#endif
