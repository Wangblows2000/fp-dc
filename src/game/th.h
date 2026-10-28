/* Trap Hideout (session 21b): shared declarations of the th_*.c modules. Engine hooks are in src/fp.h ("Trap Hideout");
 * each module adds its own type enum and helpers here, in its own block. Stage index 8, json 57-59 = th1-th3; Carol / Milla. */
#ifndef FP_TH_H
#define FP_TH_H
#include "../fp.h"

/* ---- th_flow.c (types 1536-1559) */

/* ---- th_enemy.c (types 1560-1619) */

/* ---- th_gim.c (types 1620-1679) */

/* ---- th_boss.c (types 1680-1791; build/re/spec_th_bosses.md): th_boss.c (spawn, lists, shared helpers, dispatch),
 * th_boss_iris.c (TH1 Laser Iris), th_boss_basher.c (TH3 Crystal Basher), th_boss_arena.c (TH3 arena: crowd, spotlights, goal /
 * enemies-left GUI, the waves, dialogue skip). Spade (TH3's end) runs in sb_spade.c (the SB2 classes, generalised).
 * Placed classes 1680-1719 (tools/import_frame.py TH_BOSS), runtime objects 1720-1791. */
enum {
       OT_THB_IRIS = 1680 /* BossLaserIris_3141 (Q20, A 60) */, OT_THB_CANNON = 1681 /* BossLaserIrisCannon_3140 */,
       OT_THB_WINGL = 1682 /* BossLaserIrisWingLeft_3138 */, OT_THB_WINGR = 1683 /* BossLaserIrisWingRight_3139 */,
       OT_THB_LASER = 1684 /* HazardScanLaserBoss_3142 (laser qualifier 0x22d98, flag 2) */,
       OT_THB_CB_BASE = 1685 /* BossCrystalBasherBase_3199 */, OT_THB_CB_FACE = 1686 /* BossCrystalBasherFace_3198 */,
       OT_THB_CB_HEAD = 1687 /* BossCrystalBasherHead_3197 */, OT_THB_CB_SEG = 1688 /* BossCrystalBasherSegment_3194 */,
       OT_THB_CB_SPIKE = 1689 /* BossCrystalSpike_3195 */, OT_THB_CB_SPIKE2 = 1690 /* BossCrystalSpike2_3196 */,
       OT_THB_DOOR = 1691 /* BossDoor_3200 */, OT_THB_CROWD = 1692 /* THCrowd_3213 */, OT_THB_SPOT = 1693 /* THSpotlight_3209 */,
       OT_THB_GOAL = 1694 /* GUIGoalMessage_3217 */, OT_THB_ENLEFT = 1695 /* GUIEnemiesLeft_3218 */,
       OT_THB_PLACED_LAST = 1719,
       OT_THB_RT = 1720 /* runtime objects from here */, OT_THB_DUST = 1720 /* EffectsDust_437 at a firing laser's tip (13486 / 13489) */,
       OT_THB_END = 1791 };
int   th_boss_iris_state(void);                       /* tests: BossLaserIris I (-1 none) */
/* the boss laser: the scan-laser cast and hit rule (RE loop_l_56 = 13458-13461 + its inline grow event, 13484, 13490-13493),
 * written for the boss laser and reusable by any laser of qualifier 0x22d98 */
typedef struct { double A; float x, y, ang; int dir, flag1; } thb_laser_t;
void  thb_laser_cast(thb_laser_t *l, int layer);       /* 16 grow / shrink steps of 1 px (A x 0.1 x the 10 px image) */
void  thb_laser_dust(const thb_laser_t *l, int layer); /* 13486 / 13489: a dust puff at a firing beam's tip in view, random angle */
int   thb_laser_hits(const thb_laser_t *l, const player_t *p);   /* the firing beam (direction 1) on PlayerCharacterSensor */
void  thb_laser_rule(const thb_laser_t *l, player_t *p);         /* 13490 (5 damage), 13492, 13493 (OTHERS.E <= 6) */
void  th_gim_scanlasers_off(void) __attribute__((weak));        /* th_gim.c: 13513 destroys every HazardScanLaser* at the fight start */

#endif
