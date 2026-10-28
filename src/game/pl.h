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

/* ---- pl_boss.c (types 2224-2303; build/re/spec_pl_bosses.md): the Holo-Dragon of PL6 (no other boss or miniboss in PL1-PL5: RE).
 * Placed 2224-2259 (tools/import_frame.py PL_BOSS, json order = draw order), runtime 2260-2303 (the off-map templates and effects). */
enum { OT_PLB_NECK = 2224 /* BossHoloDragonNeck_3086: the controller (ctor invisible; F 20 G 2 J -1 K -2 L 255 M -360 N 1) */,
       OT_PLB_BREATH = 2225 /* ...Breath_3078 (ctor invisible) */, OT_PLB_TTOOTH = 2226 /* ...TopTooth_3080 */,
       OT_PLB_BTOOTH = 2227 /* ...BottomTooth_3081 */, OT_PLB_HSCALE = 2228 /* ...Scale_3082 (the head's) */,
       OT_PLB_SCALE2 = 2229 /* ...Scale2_3083 (body) */, OT_PLB_SCALE3 = 2230 /* ...Scale3_3084 */, OT_PLB_SCALE4 = 2231 /* ...Scale4_3085 */,
       OT_PLB_EAR = 2232 /* ...Ear_3087 */, OT_PLB_EYE = 2233 /* ...Eye_3088 */, OT_PLB_PUPIL = 2234 /* ...Pupil_3089 */,
       OT_PLB_BROW = 2235 /* ...Brow_3090 */,
       OT_PLB_HIT1 = 2236 /* ...HitSpot1_3094 (Q20, A 3; rides Scale2) */, OT_PLB_HIT2 = 2237 /* ...HitSpot2_3095 (Scale3) */,
       OT_PLB_HIT3 = 2238 /* ...HitSpot3_3096 (Scale4) */,
       OT_PLB_PLACED_LAST = 2259,
       OT_PLB_RT = 2260, OT_PLB_CLAW = 2260 /* ...Claw_3093 x3 (A 999, invisible) */, OT_PLB_EYE2 = 2261 /* ...Eye2_3097 (Q20, A 20) */,
       OT_PLB_LASER = 2262 /* ...Laser_3079 (A 10) */, OT_PLB_FLAME = 2263 /* ...Flame_3092 (13385) */,
       OT_PLB_PANGU = 2264 /* NPCPangu_3098 (the ending) */, OT_PLB_ARM = 2265 /* NPCPanguLeftArm / RightArm_3101 / 3102 (9 each) */,
       OT_PLB_FX = 2266 /* the white flash / the ending's black backdrop (LevelCardFade_423) */,
       OT_PLB_END = 2303 };
int pl_boss_state(void);              /* tests: the neck's K (-3: no dragon), see spec_pl_bosses */

#endif
