/* Trap Hideout (session 21b): shared declarations of the th_*.c modules. Engine hooks are in src/fp.h ("Trap Hideout");
 * each module adds its own type enum and helpers here, in its own block. Stage index 8, json 57-59 = th1-th3; Carol / Milla. */
#ifndef FP_TH_H
#define FP_TH_H
#include "../fp.h"

/* ---- th_flow.c (types 1536-1559): stage-wide scenery driven by the frames' own events (spec_th_flow 5) */
enum { OT_THF_BACKLIGHT = OT_THF_FIRST,   /* thbacklight1_3113: frame (g3 / 10) mod 6 every frame (13532 / handle_frame_57 inline) */
       OT_THF_FLAME,                      /* thflame_3176: a torch flame, its own looping animation (speed 16; stopped while paused) */
       OT_THF_SPOT };                     /* thspotlight_3209 (TH3, json layer 0): 14099 / 14082 / 14083, drawn in the parallax order */
void th_flow_spot_draw(void);             /* the parallax hook (par_set_hook): TH3's spotlights */

/* ---- th_enemy.c (types 1560-1619) */

/* ---- th_gim.c (types 1620-1679) */

/* ---- th_boss.c (types 1680-1791) */

#endif
