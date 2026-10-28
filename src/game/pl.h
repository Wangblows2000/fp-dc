/* Pangu Lagoon (session 23): shared declarations of the pl_*.c modules. Engine hooks are in src/fp.h ("Pangu Lagoon");
 * each module adds its own type enum and helpers here, in its own block. Stage index 7 (Lilac only), json 51-56 = pl1-pl6. */
#ifndef FP_PL_H
#define FP_PL_H
#include "../fp.h"

/* ---- pl_flow.c (types 2048-2079) */
/* PL1-PL3 loop pieces (RE 12747-12750: the shared Q10 / Q11 loop-layer rule): solid on loop layer 0 / 1 (registry OD_LAYER0 / 1) */
enum { OT_PLF_LOOP01 = 2048 /* PLLayer01_2975 (layer 0) */, OT_PLF_LOOP02 = 2049 /* PLLayer02_2976 (layer 0) */,
       OT_PLF_LOOP11 = 2050 /* PLLayer11_2973 (layer 1) */, OT_PLF_LOOP12 = 2051 /* PLLayer12_2974 (layer 1) */ };
/* the stone-door exits of PL3 (to PL4) and PL6 (into the Holo-Dragon arena), RE 13009 / 13330: pl_gim.c reports the door pair every
 * frame (StoneDoor1 / StoneDoor2 x, StoneDoor1's A: 1 closed .. 0 open); the flow walks the player in once she stands between them */
void  pl_flow_door_report(float x1, float x2, float a);
int   pl_flow_door_busy(void);        /* the door walk / fade is running (the player is scripted) */

/* ---- pl_enemy.c (types 2080-2143) */

/* ---- pl_gim.c (types 2144-2223) */

/* ---- pl_boss.c (types 2224-2303) */

#endif
