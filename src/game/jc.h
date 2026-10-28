/* Jade Creek (session 21): shared declarations of the jc_*.c modules. Engine hooks are in src/fp.h ("Jade Creek");
 * each module adds its own type enum and helpers here, in its own block. Stage index 5, json 40-44 = jc1-jc5. */
#ifndef FP_JC_H
#define FP_JC_H
#include "../fp.h"

/* ---- jc_flow.c (types 1280-1309) */

/* ---- jc_enemy.c (types 1310-1369) */

/* ---- jc_gim.c (types 1370-1429) */

/* ---- jc_boss.c (types 1430-1479: Hovercop, Neera; acts 2 / 3) */

/* ---- jc_boss5.c (types 1480-1535: act 5's bosses and scenes; spec build/re/spec_jc_boss5.md). Serpentine 2 runs in fn_boss.c
 * (FN3's fight, hitbox 2552 = its JC5 variant); these are the gunship, its Elite troopers, the Brevon mech flyby scene (Syntax,
 * Torque), the partner assist and the closing scene. Runtime types (rt) are created by the module. */
enum { OT_JCB5_FIRST = 1480,
       OT_J5_GUN = 1480 /* BossGunship_2559 */, OT_J5_GUN_BLADEL = 1481 /* BossGunshipBladeLeft_2557 */, OT_J5_GUN_BLADER = 1482 /* _2558 */,
       OT_J5_GUN_CANL = 1483 /* BossGunshipCannonLeft_2566 */, OT_J5_GUN_CANR = 1484 /* _2567 */, OT_J5_GUN_HIT = 1485 /* BossGunshipHitbox_2577 (Q20) */,
       OT_J5_GUN_LIGHT = 1486 /* BossGunshipLight_2562 */, OT_J5_GUN_LIGHTL = 1487 /* _LightLeft_2563 */, OT_J5_GUN_LIGHTR = 1488 /* _LightRight_2564 */,
       OT_J5_GUN_RACKL = 1489 /* BossGunshipMissileRackLeft_2555 */, OT_J5_GUN_RACKR = 1490 /* _2556 */, OT_J5_GUN_PILOT = 1491 /* BossGunshipPilot_2565 */,
       OT_J5_GUN_PLAT = 1492 /* BossGunshipPlatform_2553 */, OT_J5_GUN_TROOPER = 1493 /* BossGunshipTrooper_2554 */,
       OT_J5_GUN_TURBL = 1494 /* BossGunshipTurbineLeft_2560 */, OT_J5_GUN_TURBR = 1495 /* _2561 */,
       OT_J5_GUN_FLARE = 1496 /* rt BossGunshipFlare_2568 */, OT_J5_GUN_MISSILE = 1497 /* rt BossMissile_1867 (the racks') */,
       OT_J5_ELITE = 1498 /* EnemyElite_2529 (the trooper's robot) */, OT_J5_ELITEMASK = 1499 /* EnemyEliteMask_2528 (Q20 body) */,
       OT_J5_WARN2 = 1500 /* rt EffectWarning2_1895 (an off-screen flare) */, OT_J5_ELITELASER = 1501 /* rt EnemyTrooperLaser */,
       OT_J5_BOLT = 1502 /* rt LevelLightningBolt_2525 (11619) */, OT_J5_GLOW = 1503 /* rt EnemyEliteChargeGlow_2527 */,
       OT_J5_HERO = 1504 /* rt the closing's heroes (PlayerSkinSash / Carol / Milla, 11369) */,
       OT_J5_FADE = 1505 /* rt LevelCardFade_423 (11365: the white card) */,
       OT_J5_LAST = 1535 };
void jc_boss5_spawn(void), jc_boss5_pre(player_t *p), jc_boss5_frame(player_t *p);
int  jc_boss5_obj(obj_t *o, player_t *p), jc_boss5_q20(int type), jc_boss5_draw_obj(const obj_t *o, int pass), jc_boss5_input(uint32_t *held);
int  jc_boss5_player_hidden(void);
float jc_boss5_hud_dy(void);                                  /* the act's own HUD slide (loop_gui_shiftup / down_14): game.c adds it */
int  jc_boss5_state(void);                                    /* tests / flow: 0 idle, 1 Serpentine, 2 gunship, 3 gunship beaten, 4 results */

#endif
