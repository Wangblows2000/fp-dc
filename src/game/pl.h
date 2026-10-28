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

/* ---- pl_enemy.c (types 2080-2143; build/re/spec_pl_enemies.md): placed 2080-2111 (tools/import_frame.py PL_ENEMIES), runtime
 * 2112-2143. Q20 classes (RE on_frame_51..56_init qualifier 20: stinger, nest, sunflora, terreon + its legs, obelisk, wall stinger)
 * take the shared hits of combat.c through pl_enemy_q20. EnemyCacteon_1013 (A2-A5) is the engine's OT_CACTEON (same class). */
enum {
    OT_PL_STINGER = 2080,   /* EnemyStinger_2933 (q20, A 4, E 4) */
    OT_PL_NEST = 2081,      /* EnemyStingerNest_2937 (q20, A 25, G 15 stingers) */
    OT_PL_SUNFLORA = 2082,  /* EnemySunflora_2940 (q20, A 12, F 1; regrows 300 frames after its defeat) */
    OT_PL_GAS = 2083,       /* EnemySleepGas_2941 (B 4; created by the sunflora, the placed ones are off-map templates) */
    OT_PL_TERREON = 2084,   /* EnemyTerreon_3006 (q20, A 40, J 300) */
    OT_PL_TLEG = 2085,      /* EnemyTerreonLeftLeg_3004 (q20, A 50) */
    OT_PL_TRLEG = 2086,     /* EnemyTerreonRightLeg_3005 (q20, A 50) */
    OT_PL_TLSH = 2087,      /* EnemyTerreonLeftShoulder_3002 */
    OT_PL_TRSH = 2088,      /* EnemyTerreonRightShoulder_3003 */
    OT_PL_TWARN = 2089,     /* EffectTerreonWarning_3008 */
    OT_PL_OBELISK = 2090,   /* EnemyObelisk_3047 (q20, A 20, F -1) */
    OT_PL_WSTINGER = 2091,  /* EnemyWallStinger_3056 (q20, A 4, E 2) */
    OT_PL_WBACK = 2092,     /* EnemyWallStingerBack_3058 (A4 / A5 json layer 1: the swarm behind the wall, scenery) */
    OT_PL_WLTOP = 2093,     /* WallStingerLimitTop_3060 (the swarm band's top marker) */
    OT_PL_WLBOT = 2094,     /* WallStingerLimitBottom_3061 */
    OT_PL_TDEBRIS = 2112,   /* runtime: EnemyTerreonDebris_3007 */
    OT_PL_OLASER = 2113,    /* runtime: EnemyObeliskLaser_3048 */
    OT_PL_WSPAWN = 2114,    /* runtime: EnemyWallStingerSpawn_3057 (a wall stinger becoming a stinger) */
    OT_PL_CONFETTI = 2115   /* runtime: EffectsConfetti_2943 (a sunflora's petals) */
};

/* ---- pl_gim.c (types 2144-2223) */

/* ---- pl_boss.c (types 2224-2303) */

#endif
