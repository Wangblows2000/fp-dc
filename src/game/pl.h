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

/* ---- pl_gim.c (types 2144-2223; build/re/spec_pl_gimmicks.md). Families share a type: par = member (importer PLG_SUB, lvobj
 * anim bits 0-7), layer = instance layer (bits 8-14). BreakablePLRock_3012 is Fortune Night's breakable wall (fn_gima.c OT_FN_BWALL
 * member 1); GimmickHangingIdleSwitch_1268 is relic_gimb.c's OT_RM_IDLESW. */
enum { OT_PLG_VBLOCK = 2144 /* ObjectVanishingBlock_3035 (Q7, box) */, OT_PLG_TGATE = 2145 /* ObjectTimeGate_2915 (invisible, box) */,
       OT_PLG_TPIECE = 2146 /* ObjectTimeGatePiece_2916 */, OT_PLG_STRING = 2147 /* GimmickBowString_2997 */,
       OT_PLG_ARROW = 2148 /* GimmickBowArrow_2998 */, OT_PLG_ARROWPLAT = 2149 /* runtime: GimmickBowArrowPlatform_2999 (Q8 Q9, invisible) */,
       OT_PLG_PINWHEEL = 2150 /* ObjectPinwheel_2992 */, OT_PLG_CTOP = 2151 /* ObjectCycloneTop_2969 */,
       OT_PLG_BWALL = 2152 /* GimmickBreakoutWallLeft / Right (par 0 / 1; invisible, box) */, OT_PLG_BBALL = 2153 /* GimmickBreakoutBall_3052 */,
       OT_PLG_BPLAT = 2154 /* GimmickBreakoutPlatform_3050 (Q8 Q9) */, OT_PLG_BMASK = 2155 /* GimmickBreakoutPlatformMask_3051 */,
       OT_PLG_LPLAT = 2156 /* LevelLagoonPlatform1_2994 (Q8 Q9 Q12 Q13: objects.c path platform) */, OT_PLG_LPLAT2 = 2157 /* ...2_3000 */,
       OT_PLG_SWORD = 2158 /* HazardGiantSword_3067 (Q20, invisible) */, OT_PLG_SWMASK = 2159 /* HazardGiantSwordMask_3068 */,
       OT_PLG_SPINBALL = 2160 /* GimmickSpinball_1173 in Pangu Lagoon (its own rule: 13040 / 13118 / 13225) */,
       OT_PLG_CHAIN = 2161 /* GimmickSpinballChain / 2 / 3 / 4 (par 0-3) */, OT_PLG_PLSWITCH = 2162 /* GimmickHangingPLSwitch_3023 */,
       OT_PLG_SDOOR = 2163 /* ObjectStoneDoor1 / 2 / 3 / 4 (par 0-3) */, OT_PLG_ACTIVE = 2164 /* Active_2927 (Q8, invisible) */,
       OT_PLG_REBOUND = 2165 /* runtime: EffectRebound / 2 (par 0 / 1) */, OT_PLG_BURST = 2166 /* runtime: EffectsWhiteBurst_31 (cyclone top) */,
       OT_PLG_END = 2166 };
int    pl_gim_vblock_solid(int k);                 /* tests: vanishing block k (instance order) solid this frame */

/* ---- pl_boss.c (types 2224-2303) */

#endif
