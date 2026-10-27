/* Jade Creek (session 21): shared declarations of the jc_*.c modules. Engine hooks are in src/fp.h ("Jade Creek");
 * each module adds its own type enum and helpers here, in its own block. Stage index 5, json 40-44 = jc1-jc5. */
#ifndef FP_JC_H
#define FP_JC_H
#include "../fp.h"

/* ---- jc_flow.c (types 1280-1309) */
enum { OT_JCF_LOOP0 = 1280 /* JCLoop3 / 4_2463 / 2464 (Q7 Q10: solid on layer 0; sub 0 / 1) */,
       OT_JCF_LOOP1 = 1281 /* JCLoop2_2462 (Q7 Q11: solid on layer 1) */,
       OT_JCF_LOOPX = 1282 /* JCLoop1_2461 (Q7 only: flag 0 by the frame's own events 10462 / 10644-10646) */ };
/* the act-3 boss is beaten (10772 / 10773, called by the boss module once the subtanker reaches E 8, F > 30): checkpoint g4 10
 * at (9128, 1550) with the restart snapshot, music slot 5 (m_stage5b, loop_next_act_41), camera released, the Sign moved to
 * x 0 (normal bounds C = 0, E = width + 50 until the act restarts) */
void jc_flow_act3_cleared(void);

/* ---- jc_enemy.c (types 1310-1369) */

/* ---- jc_gim.c (types 1370-1429) */

/* ---- jc_boss.c (types 1430-1479: Hovercop, Neera; acts 2 / 3) */

/* ---- jc_boss5.c (types 1480-1535: act 5's bosses and scenes) */
enum { OT_JCB5_FIRST = 1480 };
void jc_boss5_spawn(void), jc_boss5_pre(player_t *p), jc_boss5_frame(player_t *p);
int  jc_boss5_obj(obj_t *o, player_t *p), jc_boss5_q20(int type), jc_boss5_draw_obj(const obj_t *o, int pass), jc_boss5_input(uint32_t *held);
int  jc_boss5_player_hidden(void);

#endif
