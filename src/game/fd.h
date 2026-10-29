/* Final Dreadnought (session 24): shared declarations of the fd_*.c modules. Engine hooks are in src/fp.h ("Final Dreadnought");
 * each module adds its own type enum and helpers here, in its own block. stages 10-13 (rounds 1-4, two levels each), json 66-73 = fd1a fd1b fd2a fd2b fd3a fd3b fd4a fd4b. */
#ifndef FP_FD_H
#define FP_FD_H
#include "../fp.h"

/* ---- fd_flow.c (types 2560-2623) */
/* FD3's loop-layer pieces (fd3a / fd3b; RE on_frame_70_init Q10 / Q11): 01-05 solid on loop layer 0, 11-15 on loop layer 1 */
enum { OT_FDF_LOOP01 = 2560, OT_FDF_LOOP02, OT_FDF_LOOP03, OT_FDF_LOOP04, OT_FDF_LOOP05,
       OT_FDF_LOOP11, OT_FDF_LOOP12, OT_FDF_LOOP13, OT_FDF_LOOP14, OT_FDF_LOOP15 };

/* ---- fd_enemy.c (types 2624-2751; build/re/spec_fd_enemies.md): placed 2624-2699 (tools/import_frame.py FD_ENEMIES), runtime
 * 2700-2751. Final Dreadnought's other enemies are the existing modules' types: the EliteSpawner pods (bg_enemy.c 2337, payloads
 * 1-4 by this module), Elites (jc_enemy.c), tank bots / seekers (tb_enemy.c), hoppers / fire spitters (relic.c), HopperMkII /
 * drones / spawn tubes / rotor blades / magmeons / DNA cannons / axle turrets (sb_enemy.c), super turretus / coils (fn_enemy.c),
 * slimes, slugs, buoys / bubble orbiters (jc_enemy.c), sunfloras / terreons (pl_enemy.c). */
enum {
    OT_FDE_POUNDER = 2624, /* EnemyPounder_3423 (Q20, A 25): walks between limiters, slams down near the player (two shockwaves) */
    OT_FDE_GTABLE = 2625,  /* EnemyGatlingGunTable_3666 (the gun's hatch, placed by the gun's K / J) */
    OT_FDE_GRIGHT = 2626,  /* EnemyGatlingGunRight_3667 (right barrel) */
    OT_FDE_GLEFT = 2627,   /* EnemyGatlingGunLeft_3668 (left barrel) */
    OT_FDE_GUN = 2628,     /* EnemyGatlingGun_3669 (Q20, A 20): the gun ball; parts k belong to gun k (instance order, RE 16053) */
    OT_FDE_GBARREL = 2629, /* EnemyGatlingGunBarrel_3670 */
    OT_FDE_GUN2 = 2630,    /* EnemyGatlingGun2_3671 (the body) */
    OT_FDE_IRIS = 2631,    /* EnemyBeastIris_3735 (Q20, A 10): runs at the player and bites (FD4a) */
    OT_FDE_SPIDER = 2632,  /* EnemySpiderBeamer_3755 (Q20, A 15, ctor invisible): wall turret firing homing bullets (FD4a / FD4b) */
    OT_FDE_PODSPOT = 2633, /* EnemyEliteSpawnerSpawner_3453 (ctor invisible): FD1a's pod-rain drop points (15287) */
    OT_FDE_GDEB = 2700     /* runtime: EnemyGatlingGunDebris_3673 */
};
void fd_enemy_pod_rain(int on);       /* FD1a: the dialog's step (15284, the subtitle object's C = 1) starts the pod rain (15287) */
void fd_enemy_gatlings_wake(void);    /* FD3a: 16183 (the alarm's B 540): dormant gatling guns (flag 4) wake */
int  fd_enemy_gatlings_dormant(void); /* gatling guns still dormant (tests) */

/* ---- fd_gim.c (types 2752-2911; build/re/spec_fd_gimmicks.md). Families share a type: par = member (importer FDG_SUB, lvobj
 * anim bits 0-7), layer = instance layer (bits 8-14). Placed classes 2752-2879, runtime objects 2880-2911. Final Dreadnought's
 * ObjectTransporter / ObjectSiren / HazardSpinMine / GimmickAntigravSpawner are Battle Glacier's types (bg.h OT_BGG_*), its key
 * cards / locked-door terminals / seals / switch buttons Thermal Base's (tb.h OT_TBG_*). */
enum { OT_FDG_TSTART = 2752,  /* ObjectTransporterStart_3416 (ctor A 90): a pipe entrance */
       OT_FDG_TSTART2 = 2753, /* ObjectTransporterStart2_3417: the light orbiting a start (15188) */
       OT_FDG_TNODE = 2754,   /* ObjectTransporterNode0 / 90 / 180 / 270 (par 0-3: the new direction / 90; invisible markers) */
       OT_FDG_ZCANNON = 2755, /* HazardZapBeamCannon_3427 (ctor A 270, E 240, F 300) */
       OT_FDG_ZBEAM = 2756,   /* HazardZapBeam_3428 (drawn y-scaled) */
       OT_FDG_ZHIT = 2757,    /* HazardZapBeamHitbox_3429 (invisible) */
       OT_FDG_SMAKER = 2758,  /* HazardSpiralSparkMaker_3430 */
       OT_FDG_SLIGHT = 2759,  /* FDSpiralSparkTowerLight_3432 */
       OT_FDG_DHPLAT = 2760,  /* GimmickDownhillPlatform_3441 (Q8 Q9 Q13: objects.c path platform with a scripted I / J) */
       OT_FDG_DHWHEEL = 2761, /* GimmickDownhillPlatformWheel_3442 */
       OT_FDG_LOWSPRING = 2762, /* ObjectLowSpring_3448 (no player event in FD: scenery) */
       OT_FDG_HANGAR = 2763,  /* BreakableFDHangarWall_3407 (fd1a's opening: fd_gim_hangar_break) */
       OT_FDG_AIRBASE = 2764, /* AirlockBase_3459 (invisible) */
       OT_FDG_AIRLOCK_S = 2765, /* Airlock1_3457 (par 0) / Airlock4_3458 (par 1): Q7 */
       OT_FDG_AIRLOCK = 2766, /* Airlock2_3455 (par 0) / Airlock3_3456 (par 1) */
       OT_FDG_PDOOR = 2767,   /* PressureDoor1_3489 (par 0, ctor B -192) / PressureDoor2_3490 (par 1, B 192) */
       OT_FDG_PDOOR3 = 2768,  /* PressureDoor3_3492 (Q7, ctor B 192) */
       OT_FDG_SFLOOR = 2769,  /* ScrollingFloor_3466 (par 0) / ScrollingFloor2_3465 (par 1, Q7): fd_gim_scroll_floor */
       OT_FDG_PUSH = 2770,    /* LevelFDPushPlatform_3540 (Q8 Q9 Q13, invisible: path platform with a scripted I / J) */
       OT_FDG_PUSHMASK = 2771,/* LevelFDPushPlatformMask_3544 (drawn at its platform) */
       OT_FDG_FDPLAT = 2772,  /* FDPlatform_3557 (Q8 Q9) */
       OT_FDG_CONSOLE = 2773, /* FDConsole1_3552 (par 0) / FDConsole2_3553 (par 1): scenery */
       OT_FDG_TUBESHADE = 2774, /* EffectTubeShade_3521 (bobs: 15667) */
       OT_FDG_ECRUSH = 2775,  /* LevelElectricCrusher_3541 (Q7 Q13, invisible: path platform F 2 G 64) */
       OT_FDG_ECBOLT = 2776,  /* LevelElectricCrusherBolt_3542 (hurts) */
       OT_FDG_ECMASK = 2777,  /* LevelElectricCrusherMask_3543 (drawn) */
       OT_FDG_CDOOR = 2778,   /* CorkscrewDoor_3537 (Q7) */
       OT_FDG_CSWITCH = 2779, /* SwitchCorkscrewDoor_3536 */
       OT_FDG_SHGIVER = 2780, /* PlayerShieldGiver_3528 (Milla only) */
       OT_FDG_RBARRIER = 2781,/* FDRotatingBarrier_3567 (Q7, rotated) */
       OT_FDG_BUTTON = 2782,  /* SwitchButtonLeft_2707 (par 0) / Right_2705 (par 1) in fd2b (momentary: FRAME_TYPE_REMAP from TB's) */
       OT_FDG_BIGLIFT = 2783, /* ObjectBigLift_3584 (Q7 Q13: path platform with a scripted J) */
       OT_FDG_LSWITCH = 2784, /* SwitchBigLift1_3586 (par 0) / 2_3587 (par 1) */
       OT_FDG_BOOST = 2785,   /* GimmickSpeedBooster_3640 (PLAYER on it: col_26_69) */
       OT_FDG_BOOSTF = 2786,  /* GimmickSpeedBoosterFront_3639 (drawn at its booster) */
       OT_FDG_OBASE = 2787,   /* HazardOrbitBeamBase_3652 (the centre; frame = its beam's I) */
       OT_FDG_OBEAM = 2788,   /* HazardOrbitBeam_3653 (240 px around its base) */
       OT_FDG_OSCAN = 2789,   /* HazardOrbitBeamScan_3654 (the ray to the centre: aims, then burns) */
       OT_FDG_LTPLAT = 2790,  /* FDLaserTunnelPlatform_3743 (Q8 Q9) */
       OT_FDG_LTUNNEL = 2791, /* HazardLaserTunnel_3742 */
       OT_FDG_LTGLASS = 2792, /* HazardLaserTunnelGlass1 / 2 / 3 (par 0-2) */
       OT_FDG_PLACED_LAST = 2879,
       OT_FDG_FX = 2880,      /* runtime: a one-shot moving effect (the pipe exit's shock ring): av[0] age, av[1] life */
       OT_FDG_SPARK = 2881,   /* runtime: HazardSpiralSpark_3435 (foreach_st_65) */
       OT_FDG_WIND = 2882,    /* runtime: EffectAirlockWind_3460 */
       OT_FDG_END = 2882 };
/* fd1a's opening (RE 15300; the flow's intro counter reaching 160): the hangar walls burst into pieces, sound 354 */
void fd_gim_hangar_break(void);
/* the Syntax arena's treadmill (RE fd1b 15341-15344, while its group is on): the boss module calls it every frame with
 * BossSyntaxValues' position while the floor runs (on 0: the floor stays where it is) */
void fd_gim_scroll_floor(int on, float sx, float sy);
int  fd_gim_ride(void);                         /* tests: 95 while the player travels a transporter pipe, else 0 */
/* fd2b's alarm (RE 15802 / 15807 / 15811: the off-map ObjectSiren's A as the act's script state): 0 quiet, 1 warned (Y > 5340, not
 * Milla, checkpoint < 2; B counts), 2 the alarm (B > 400, or a restart from checkpoint 2 / 3: sound 372), 3 escaped (Y < 288).
 * fd_gim_siren_alarm() is 1 while A == 2 (the flow's vacuum air rule 15791-15795 reads it); fd_gim_siren_state() returns A. */
int  fd_gim_siren_alarm(void), fd_gim_siren_state(void);

/* ---- fd_boss.c (types 2912-3071): fd_boss.c is the dispatch (spawn / pre / frame / obj / draw / q20 / input / hidden / hud);
 * bosses A (rounds 1-2, fd1a-fd2b, types 2912-2991) live in fd_boss_r12.c, bosses B (rounds 3-4, fd3a-fd4b, types 2992-3071)
 * in fd_boss_r34.c. fd_boss.c calls both modules' hooks every frame (each returns at once when its classes are not placed)
 * and routes per-object calls by type range. */
enum { OT_FDB_A_FIRST = 2912, OT_FDB_A_LAST = 2991, OT_FDB_B_FIRST = 2992, OT_FDB_B_LAST = 3071 };

/* ---- fd_boss.c / fd_boss_r12.c, bosses A (types 2912-2991; build/re/spec_fd_bosses_a.md). FD2a's RoboSquid is TB3's class
 * (tb_boss_squid.c through TB_BOSS's OT_TBB_SQUID..: no FD type). Placed 2912-2919, runtime 2920-2991. */
enum {
    OT_FDB_PB = 2912,       /* MiniBossPushBeam_3493 (fd1b; ctor invisible, A 1): the beam (48 x 144, pushes / hurts) */
    OT_FDB_PBC = 2913,      /* MiniBossPushBeamCannon_3494 (fd1b): the beam's ceiling cannon (A: extra speed) */
    OT_FDB_SXV = 2914,      /* BossSyntaxValues_3467 (fd1b; invisible): the morpher's controller (alterables A-Z, flags) */
    OT_FDB_ALARM = 2915,    /* AlarmLight_3495 (fd1b, 0,0): the red screen pulse of the push-beam alarm (an overlay) */
    OT_FDB_SFLOOR = 2916,   /* ScrollingFloor_3466 (fd1b tunnel floor, 128 x 32) */
    OT_FDB_SFLOOR2 = 2917,  /* ScrollingFloor2_3465 (fd1b tunnel ceiling, 128 x 32) */
    OT_FDB_PLACED_LAST = 2919,
    OT_FDB_MPART = 2920,    /* runtime: the morpher's parts 2920-2934 (BossMorpher*_3469..3486; par = part index) */
    OT_FDB_MPART_LAST = 2934,
    OT_FDB_TRAIN = 2935,    /* runtime: BossTrainBream_3485 (the morpher's beam) */
    OT_FDB_EBALL = 2936,    /* runtime: BossSyntaxElecBall_3484 */
    OT_FDB_TLASER = 2937,   /* runtime: EnemyTrooperLaser_2530 (the morpher's lasers) */
    OT_FDB_SHOCK = 2938,    /* runtime: HazardShockwave_1576 */
    OT_FDB_FX = 2939,       /* runtime: a one-shot effect (explosions, sparks) */
    OT_FDB_SERP = 2940,     /* runtime: the mech Serpentine's parts 2940-2955 (BossMSerp*_3593..3607; par = part index) */
    OT_FDB_SERP_LAST = 2955,
    OT_FDB_PUNCH = 2956,    /* BossEnergyPunch_3603 (fd2b) */
    OT_FDB_SHOT = 2957,     /* runtime: projectiles of the fd2b fight (BossBurstShot, EffectFireball) */
    OT_FDB_A_RT_LAST = 2991
};
void  fdb12_spawn(void), fdb12_pre(player_t *p), fdb12_frame(player_t *p);
int   fdb12_obj(obj_t *o, player_t *p), fdb12_draw_obj(const obj_t *o, int pass), fdb12_q20(int type), fdb12_input(uint32_t *held);
int   fdb12_player_hidden(void);
void  fdb12_draw(int pass);
float fdb12_hud_dy(void);

/* ---- fd_boss_r34.c, bosses B (types 2992-3071): the same hooks, called by fd_boss.c */
void  fdb34_spawn(void), fdb34_pre(player_t *p), fdb34_frame(player_t *p);
int   fdb34_obj(obj_t *o, player_t *p), fdb34_draw_obj(const obj_t *o, int pass), fdb34_q20(int type), fdb34_input(uint32_t *held);
int   fdb34_player_hidden(void);
void  fdb34_draw(int pass);                                                     /* objects_draw passes 0-2 (fd_boss_draw) */
float fdb34_hud_dy(void);

#endif
