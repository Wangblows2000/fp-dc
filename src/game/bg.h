/* Battle Glacier (session 24): shared declarations of the bg_*.c modules. Engine hooks are in src/fp.h ("Battle Glacier");
 * each module adds its own type enum and helpers here, in its own block. stage index 9, json 60-65 = bg1-bg6. */
#ifndef FP_BG_H
#define FP_BG_H
#include "../fp.h"

/* ---- bg_flow.c (types 2304-2335) */
/* the loop pieces of BG4 / BG5 (RE on_frame_63 / 64_init qualifier tables: BGLoop1_3340 in Q7 + Q11 = solid on loop layer 1,
 * BGLoop2_3343 / BGLoop3_3344 in Q7 + Q10 = solid on loop layer 0; 14345 / 14346 / 14348 / 14349 = the shared flag rule by MV.P) */
enum { OT_BGF_LOOP1 = 2304 /* BGLoop1_3340 (layer 1) */, OT_BGF_LOOP2 = 2305 /* BGLoop2_3343 (layer 0) */,
       OT_BGF_LOOP3 = 2306 /* BGLoop3_3344 (layer 0) */ };
/* BG4's opening Brevon mech beaten (the flow part of 14502): checkpoint (5, 444, 1800) + play_music(5) (m_stage9b). The boss module
 * calls it once at the end of the fight (the act starts silent while g4 < 5: levels.c .music_fade_g4) */
void  bg_flow_opening_cleared(void);

/* ---- bg_enemy.c (types 2336-2399) */

/* ---- bg_gim.c (types 2400-2479) */

/* ---- bg_boss.c (types 2480-2559) */

#endif
