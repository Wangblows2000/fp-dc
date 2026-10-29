/* Jade Creek (session 21): shared declarations of the jc_*.c modules. Engine hooks are in src/fp.h ("Jade Creek");
 * each module adds its own type enum and helpers here, in its own block. Stage index 5, json 40-44 = jc1-jc5. */
#ifndef FP_JC_H
#define FP_JC_H
#include "../fp.h"

/* ---- jc_flow.c (types 1280-1309) */
enum { OT_JCF_LOOP0 = 1280 /* JCLoop3 / 4_2463 / 2464 (Q7 Q10: solid on layer 0; sub 0 / 1) */,
       OT_JCF_LOOP1 = 1281 /* JCLoop2_2462 (Q7 Q11: solid on layer 1) */,
       OT_JCF_LOOPX = 1282 /* JCLoop1_2461 (Q7 only: flag 0 by the frame's own events 10462 / 10644-10646) */ };
/* the act-3 boss is beaten (10772 / 10773, called by the boss module once the subtanker reaches E 8, F > 30): checkpoint g4 10
 * at (9128, 1550) with the restart snapshot, music slot 5 (m_stage5b, loop_next_act_41), camera released, the Sign moved to
 * x 0 (normal bounds C = 0, E = width + 50 until the act restarts) */
void jc_flow_act3_cleared(void);

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
    OT_JC_GLOW = 1348,     /* runtime: EnemyEliteChargeGlow_2527 */
    OT_JC_TBOMB = 1349     /* runtime: MinibossTreddBomb_1060 fired by a torpedo launcher (its blast: the DV OT_BLAST) */
};
void jc_enemy_hover_start(void);   /* JC3: group 0x22a5d on (10769 after Neera / 10798): the torpedo launchers' phases start */
/* EnemyElite_2529 + EnemyEliteMask_2528 as a reusable enemy (Battle Glacier / Final Dreadnought): */
obj_t *jc_enemy_elite_create(float x, float y);   /* a runtime Elite, feet at (x, y) (BG5 / FD elite spawners); returns the mask */
void   jc_enemy_elite_view_alert(int on);          /* 14408 (BG4 / BG5 / FD1 / FD2): idle Elites inside the view alert (reset per level) */
obj_t *jc_enemy_laser_create(float x, float y, double speed, double angle);   /* a TrooperLaser along angle (deg CCW), drawn turned (FD gatlings) */

/* ---- jc_gim.c (types 1370-1429; build/re/spec_jc_gimmicks.md). Families share a type: par = member (importer JCG_SUB). */
enum { OT_JCG_STREAM = 1370 /* JCWaterStream1-8 (par = 1..8) */, OT_JCG_LEAFBACK = 1371 /* GimmickLeafPlatformBack_2357 */,
       OT_JCG_LEAF = 1372 /* GimmickLeafPlatform1 / 2 (par 0 / 1) */, OT_JCG_FLEAF = 1373 /* GimmickFlyingLeafPlatform_2476 */,
       OT_JCG_FLEAFM = 1374 /* its mask (the Q8 Q13 platform) */, OT_JCG_SLIDE = 1375 /* JCSlidingPlatform_2377 (Q7 Q13) */,
       OT_JCG_SLIDEM = 1376 /* JCSlidingPlatformMask_2381 (drawn) */, OT_JCG_SLIDEL = 1377 /* JCSlidingPlatformLandPoint_2382 */,
       OT_JCG_COLL = 1378 /* LevelJCCollapsingPlatform 2456 / 2475 (par 0 / 1) */, OT_JCG_CPIECE = 1379 /* runtime: its pieces (2457) */,
       OT_JCG_SEEDSP = 1380 /* GimmickSeedSpawner_2418 */, OT_JCG_SEED = 1381 /* runtime: GimmickFlyingSeed_2419 */,
       OT_JCG_BUBSP = 1382 /* GimmickBubbleSpringSpawner_2449 */, OT_JCG_BUBBACK = 1383 /* runtime: GimmickBubbleSpringBack2_2459 */,
       OT_JCG_BUBFRONT = 1384 /* runtime: GimmickBubbleSpringFront2_2460 */, OT_JCG_SLIP = 1385 /* LevelSlipperySurface_2473 */,
       OT_JCG_FALLLEAF = 1386 /* EffectFallingLeaf_2396 (placed once in JC1; created by the flying leaves) */,
       OT_JCG_END = 1386 };
int  jc_gim_stream_push(void);                 /* this frame's water-stream push on the player's X (-1, 0, +1; tests / other modules) */

/* ---- jc_boss.c (types 1430-1479: Hovercop fly-by (act 1), Hovercop chase (acts 2 / 3), Neera (act 3); build/re/spec_jc_bosses.md).
 * Placed classes (tools/import_frame.py JC_BOSS, json 40-43 only) then the runtime objects. */
enum { OT_JCB_HC = 1430, OT_JCB_TB1, OT_JCB_TB2, OT_JCB_PROP1, OT_JCB_PROP2, OT_JCB_SIR1, OT_JCB_SIR2, OT_JCB_SIR3, OT_JCB_SIR4,
       OT_JCB_HOOK, OT_JCB_HOOK2, OT_JCB_LIGHT, OT_JCB_ICED, OT_JCB_NEERA, OT_JCB_NHIT, OT_JCB_ICEBAR, OT_JCB_ICEPIECE,   /* 1430-1446 placed */
       OT_JCB_GLOW = 1450, OT_JCB_CRYSTAL, OT_JCB_MISSILE, OT_JCB_STICKY, OT_JCB_METEOR, OT_JCB_RING, OT_JCB_NSHIELD, OT_JCB_BUBBLE,
       OT_JCB_END = 1479 };                          /* (OT_JCB_LAST in fp.h is the whole JC boss range, 1535) */
float jc_boss_hud_dy(void);                          /* the boss module's HUD offset (loop_gui_shiftup / shiftdown; 0: none in JC1-JC4, RE) */
int   jc_boss_ml_y(void);                            /* MasterLevel Y (1 while the JC3 Neera fight runs: ControllerMusic slot 4) */

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
