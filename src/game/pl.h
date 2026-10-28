/* Pangu Lagoon (session 23): shared declarations of the pl_*.c modules. Engine hooks are in src/fp.h ("Pangu Lagoon");
 * each module adds its own type enum and helpers here, in its own block. Stage index 7 (Lilac only), json 51-56 = pl1-pl6. */
#ifndef FP_PL_H
#define FP_PL_H
#include "../fp.h"

/* ---- pl_flow.c (types 2048-2079) */

/* ---- pl_enemy.c (types 2080-2143) */

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
