/* Battle Glacier (session 24): shared declarations of the bg_*.c modules. Engine hooks are in src/fp.h ("Battle Glacier");
 * each module adds its own type enum and helpers here, in its own block. stage index 9, json 60-65 = bg1-bg6. */
#ifndef FP_BG_H
#define FP_BG_H
#include "../fp.h"

/* ---- bg_flow.c (types 2304-2335) */

/* ---- bg_enemy.c (types 2336-2399) */

/* ---- bg_gim.c (types 2400-2479) */

/* ---- bg_boss.c (types 2480-2559): bosses, minibosses and the boss-class set pieces of BG1-BG6 (spec build/re/spec_bg_bosses.md).
 * Placed classes 2480-2509 (importer BG_BOSS: lvobj anim = instance layer << 8); runtime objects 2510-2559 (created by the encounters). */
enum { OT_BGB_SPARKY = 2480 /* BossSparky_3336 (Q20, A 100; BG4 boss, BG5 Milla's ride) */, OT_BGB_SP_LARM = 2481 /* BossSparkyLeftArm_3337 */,
       OT_BGB_SP_RARM = 2482 /* BossSparkyRightArm_3338 */, OT_BGB_SP_TAIL = 2483 /* BossSparkyTail_3335 (4 links) */,
       OT_BGB_HATCH = 2484 /* BossHatchDoor_3312 (BG4 / BG6 arena walls) */, OT_BGB_MECHBG = 2485 /* BrevonMechBackground_2444 */,
       OT_BGB_MWARN2 = 2486 /* EffectBrevonMechWarning2_2586 (BG4: the claw-swipe warnings) */,
       OT_BGB_DAIL = 2490 /* BossDail_3370 */, OT_BGB_DAILFALL = 2491 /* BossDailFall_3383 (BG6 scene controller) */,
       OT_BGB_DAILSPAWN = 2492 /* BossDailSpawnPoint_3366 */, OT_BGB_SBEAST = 2493 /* BossShadeBeast_3368 */,
       OT_BGB_SBCROWN = 2494 /* BossShadeBeastCrown_3369 */, OT_BGB_SHIPCL = 2495 /* LevelBrevonShipClose_3353 */,
       OT_BGB_SHIPCL2 = 2496 /* LevelBrevonShipClose2_3364 */, OT_BGB_JET1 = 2497 /* LiftOffJet1_3386 */, OT_BGB_JET2 = 2498 /* LiftOffJet2_3387 */,
       OT_BGB_DEVICE = 2499 /* Device_3388 */,
       OT_BGB_PLACED_LAST = 2509,
       OT_BGB_RT = 2510 /* runtime objects from here */,
       OT_BGB_PLASMA = 2510 /* BossSparkyPlasmaShot_2587 (BallMovement 80) */, OT_BGB_FX = 2511 /* a one-shot effect of this module */,
       OT_BGB_M_FIRST = 2512 /* the Brevon mech (Absolution) parts, created at the BG4 fight start (bg_boss_mech.c) */,
       OT_BGB_M_TORSO = 2512, OT_BGB_M_HEAD = 2513, OT_BGB_M_WEAK = 2514 /* Q20 */, OT_BGB_M_TURRET = 2515, OT_BGB_M_CHARGE = 2516,
       OT_BGB_M_LASER = 2517, OT_BGB_M_WARN = 2518, OT_BGB_M_ABDOMEN = 2519, OT_BGB_M_AXLE = 2520, OT_BGB_M_BARM = 2521,
       OT_BGB_M_BCLAW = 2522, OT_BGB_M_BSHOULDER = 2523, OT_BGB_M_BWING = 2524, OT_BGB_M_FLEG = 2525, OT_BGB_M_FLEG2 = 2526,
       OT_BGB_M_FKNEE = 2527, OT_BGB_M_FFOOT = 2528, OT_BGB_M_FWING = 2529, OT_BGB_M_FSHOULDER = 2530, OT_BGB_M_FARM = 2531,
       OT_BGB_M_FCLAW = 2532, OT_BGB_M_SHOCK = 2533 /* HazardShockwave_1576 */, OT_BGB_M_LAST = 2533,
       OT_BGB_D_FIRST = 2536 /* the Shade Beast / Dail runtime objects (bg_boss_dail.c) */,
       OT_BGB_D_TTOOTH = 2536, OT_BGB_D_BTOOTH = 2537, OT_BGB_D_MOUTH = 2538, OT_BGB_D_EYE = 2539, OT_BGB_D_VORTEX1 = 2540,
       OT_BGB_D_VORTEX2 = 2541, OT_BGB_D_STAR = 2542, OT_BGB_D_HITZONE = 2543, OT_BGB_D_CARD = 2544, OT_BGB_D_TRAIL = 2545,
       OT_BGB_D_CARDFX = 2546, OT_BGB_D_BOLT = 2547, OT_BGB_D_SHADE = 2548, OT_BGB_D_BURST = 2549, OT_BGB_D_LAST = 2549,
       OT_BGB_END = 2559 };
int  bg_boss_sparky_state(void);            /* the BG4 Sparky's H (-1: none), for tests and the flow */
int  bg_boss_mech_state(void);              /* the Absolution torso's F (-9: none) */
int  bg_boss_dail_state(void);              /* the BG6 fight phase (-1: none) */

#endif
