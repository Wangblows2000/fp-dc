/* Jade Creek (session 21): shared declarations of the jc_*.c modules. Engine hooks are in src/fp.h ("Jade Creek");
 * each module adds its own type enum and helpers here, in its own block. Stage index 5, json 40-44 = jc1-jc5. */
#ifndef FP_JC_H
#define FP_JC_H
#include "../fp.h"

/* ---- jc_flow.c (types 1280-1309) */

/* ---- jc_enemy.c (types 1310-1369) */

/* ---- jc_gim.c (types 1370-1429) */

/* ---- jc_boss.c (types 1430-1479: Hovercop fly-by (act 1), Hovercop chase (acts 2 / 3), Neera (act 3); build/re/spec_jc_bosses.md).
 * Placed classes (tools/import_frame.py JC_BOSS, json 40-43 only) then the runtime objects. */
enum { OT_JCB_HC = 1430, OT_JCB_TB1, OT_JCB_TB2, OT_JCB_PROP1, OT_JCB_PROP2, OT_JCB_SIR1, OT_JCB_SIR2, OT_JCB_SIR3, OT_JCB_SIR4,
       OT_JCB_HOOK, OT_JCB_HOOK2, OT_JCB_LIGHT, OT_JCB_ICED, OT_JCB_NEERA, OT_JCB_NHIT, OT_JCB_ICEBAR, OT_JCB_ICEPIECE,   /* 1430-1446 placed */
       OT_JCB_GLOW = 1450, OT_JCB_CRYSTAL, OT_JCB_MISSILE, OT_JCB_STICKY, OT_JCB_METEOR, OT_JCB_RING, OT_JCB_NSHIELD, OT_JCB_BUBBLE,
       OT_JCB_END = 1479 };                          /* (OT_JCB_LAST in fp.h is the whole JC boss range, 1535) */
float jc_boss_hud_dy(void);                          /* the boss module's HUD offset (loop_gui_shiftup / shiftdown; 0: none in JC1-JC4, RE) */
int   jc_boss_ml_y(void);                            /* MasterLevel Y (1 while the JC3 Neera fight runs: ControllerMusic slot 4) */

/* ---- jc_boss5.c (types 1480-1535: act 5's bosses and scenes) */
enum { OT_JCB5_FIRST = 1480 };
void jc_boss5_spawn(void), jc_boss5_pre(player_t *p), jc_boss5_frame(player_t *p);
int  jc_boss5_obj(obj_t *o, player_t *p), jc_boss5_q20(int type), jc_boss5_draw_obj(const obj_t *o, int pass), jc_boss5_input(uint32_t *held);
int  jc_boss5_player_hidden(void);

#endif
