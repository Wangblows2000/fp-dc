/* Jade Creek (session 21): shared declarations of the jc_*.c modules. Engine hooks are in src/fp.h ("Jade Creek");
 * each module adds its own type enum and helpers here, in its own block. Stage index 5, json 40-44 = jc1-jc5. */
#ifndef FP_JC_H
#define FP_JC_H
#include "../fp.h"

/* ---- jc_flow.c (types 1280-1309) */

/* ---- jc_enemy.c (types 1310-1369) */

/* ---- jc_gim.c (types 1370-1429) */

/* ---- jc_boss.c (types 1430-1479: Hovercop, Neera; acts 2 / 3) */

/* ---- jc_boss5.c (types 1480-1535: act 5's bosses and scenes) */
enum { OT_JCB5_FIRST = 1480 };
void jc_boss5_spawn(void), jc_boss5_pre(player_t *p), jc_boss5_frame(player_t *p);
int  jc_boss5_obj(obj_t *o, player_t *p), jc_boss5_q20(int type), jc_boss5_draw_obj(const obj_t *o, int pass), jc_boss5_input(uint32_t *held);
int  jc_boss5_player_hidden(void);

#endif
