/* Jade Creek (session 21): shared declarations of the jc_*.c modules. Engine hooks are in src/fp.h ("Jade Creek");
 * each module adds its own type enum and helpers here, in its own block. Stage index 5, json 40-44 = jc1-jc5. */
#ifndef FP_JC_H
#define FP_JC_H
#include "../fp.h"

/* ---- jc_flow.c (types 1280-1309) */

/* ---- jc_enemy.c (types 1310-1369; build/re/spec_jc_enemies.md): placed 1310-1339 (tools/import_frame.py JC_ENEMIES), runtime
 * children 1340-1369. All Q20 classes take the shared hits of combat.c through jc_enemy_q20. */
enum {
    OT_JC_METEOR = 1310,   /* EnemyMeteorRoller_2416 (q20) */
    OT_JC_MFLAME = 1311,   /* EnemyMeteorRollerFlame_2417 */
    OT_JC_BUOY = 1312,     /* EnemyBuoyBuoy_2407 (q20) */
    OT_JC_PLANT = 1314,    /* EnemyPlantTurretus_2393 (q20) */
    OT_JC_PHAT = 1315,     /* EnemyPlantTurretusHat_2394 */
    OT_JC_PSTALK = 1316,   /* EnemyPlantTurretusStalk_2395 */
    OT_JC_FROG = 1318,     /* EnemyBullFrag_2389 (q20) */
    OT_JC_FROGMASK = 1319, /* EnemyBullFragMask_2410 (invisible) */
    OT_JC_SHARK = 1321,    /* EnemySharkTorpedo_2421 (q20) */
    OT_JC_ORBITER = 1323,  /* EnemyBubblOrbiter_2455 (q20, JC2-JC4) */
    OT_JC_LAUNCHER = 1325, /* HazardTorpedoLauncher_2515 (JC3 / JC4) */
    OT_JC_ELITE = 1326,    /* EnemyElite_2529 (JC5) */
    OT_JC_ELITEMASK = 1327,/* EnemyEliteMask_2528 (q20) */
    OT_JC_MEXPL = 1340,    /* runtime: EffectsExplosion06_435 thrown out by a landing meteor (10234 / 10247) */
    OT_JC_BJET = 1341,     /* runtime: EnemyBuoyBullet_2409 / EnemyBuoyBullet2_2415 */
    OT_JC_LEAF = 1342,     /* runtime: EffectFallingLeaf_2396 */
    OT_JC_BUBBLE = 1343,   /* runtime: EnemyBullFragBubble_2408 */
    OT_JC_TORP = 1344,     /* runtime: EnemySharkTorpedo2_2422 */
    OT_JC_OSHOT = 1345,    /* runtime: BadnikGolemShot_1260 orbiting a Bubbl Orbiter */
    OT_JC_WARN = 1346,     /* runtime: EffectWarning_1528 at a meteor's landing point */
    OT_JC_LASER = 1347,    /* runtime: EnemyTrooperLaser_2530 (the Elite's shots) */
    OT_JC_GLOW = 1348      /* runtime: EnemyEliteChargeGlow_2527 */
};
void jc_enemy_hover_start(void);   /* JC3: group 0x22a5d on (10769 after Neera / 10798): the torpedo launchers' phases start */

/* ---- jc_gim.c (types 1370-1429) */

/* ---- jc_boss.c (types 1430-1479: Hovercop, Neera; acts 2 / 3) */

/* ---- jc_boss5.c (types 1480-1535: act 5's bosses and scenes) */
enum { OT_JCB5_FIRST = 1480 };
void jc_boss5_spawn(void), jc_boss5_pre(player_t *p), jc_boss5_frame(player_t *p);
int  jc_boss5_obj(obj_t *o, player_t *p), jc_boss5_q20(int type), jc_boss5_draw_obj(const obj_t *o, int pass), jc_boss5_input(uint32_t *held);
int  jc_boss5_player_hidden(void);

#endif
