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
