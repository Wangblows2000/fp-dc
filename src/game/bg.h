/* Battle Glacier (session 24): shared declarations of the bg_*.c modules. Engine hooks are in src/fp.h ("Battle Glacier");
 * each module adds its own type enum and helpers here, in its own block. stage index 9, json 60-65 = bg1-bg6. */
#ifndef FP_BG_H
#define FP_BG_H
#include "../fp.h"

/* ---- bg_flow.c (types 2304-2335) */
/* the loop pieces of BG4 / BG5 (RE on_frame_63 / 64_init qualifier tables: BGLoop1_3340 in Q7 + Q11 = solid on loop layer 1,
 * BGLoop2_3343 / BGLoop3_3344 in Q7 + Q10 = solid on loop layer 0; 14345 / 14346 / 14348 / 14349 = the shared flag rule by MV.P) */
enum { OT_BGF_LOOP1 = 2304 /* BGLoop1_3340 (layer 1) */, OT_BGF_LOOP2 = 2305 /* BGLoop2_3343 (layer 0) */,
       OT_BGF_LOOP3 = 2306 /* BGLoop3_3344 (layer 0) */ };
/* BG4's opening Brevon mech beaten (the flow part of 14502): checkpoint (5, 444, 1800) + play_music(5) (m_stage9b). The boss module
 * calls it once at the end of the fight (the act starts silent while g4 < 5: levels.c .music_fade_g4) */
void  bg_flow_opening_cleared(void);

/* ---- bg_enemy.c (types 2336-2399; build/re/spec_bg_enemies.md): placed 2336-2369 (tools/import_frame.py BG_ENEMIES), runtime
 * 2370-2399. Battle Glacier's other enemies are the existing modules' types (Thermal Base saucers / squids, Jade Creek's Elite,
 * meteor rollers and bull frags, Fortune Night's super turretus / ghetto shell, Sky Battalion's spawn tubes / rotor blades /
 * pelletizer, the engine's dragonflies, turretus, Tredds, extractors). */
enum {
    OT_BGE_DRILL = 2336,   /* EnemyDrillSat_3291 (Q20, A 5, ctor invisible): bursts out of the ground, hovers, knocked into a flying drill */
    OT_BGE_SPAWNER = 2337, /* EnemyEliteSpawner_3361 (not Q20): a pod that drops on the player and releases its payload (BG5 / FD) */
    OT_BGE_WAVE = 2370,    /* runtime: EnemyEliteSpawnerWave_3363 (the landing shock ring) */
    OT_BGE_GHOST = 2371,   /* runtime: EffectsSpadeImage_456 direction 12 (the falling pod's afterimage) */
    OT_BGE_DEB = 2372      /* runtime: EffectsDebris_427 anim 5 (the drill's dirt) */
};
/* EnemyEliteSpawner (RE BG5 14656-14662 / 14724, FD 15245-15249 / 21288-21294 / 22376): reusable by any stage. The pod waits
 * 300 px above its placed y (A = that y); with the player within 200 px in x and A in (MV.Y - 50, MV.Y + 150) it shows (sound
 * 345) and drops 20 px a frame with afterimages, lands (B 2: a shock wave, sound 346), opens (anim 8, frame B / 6) and at B > 30
 * releases its payload at its feet and goes. Payload: the spawner's av[BGE_SPAWNER_C] (FD's alterable C; 0 = an Elite, BG5's
 * only payload). Another module handles other codes with bg_enemy_spawner_payload(fn): fn(spawner, code) returns non-zero when
 * it created the payload. */
#define BGE_SPAWNER_C 2      /* spawner av[]: [0] A landing y, [1] B (0 waiting, 1 falling, > 1 opening clock: another module may set
                                B = 1 to drop one itself, e.g. an arena script), [2] C payload code */
typedef int (*bge_payload_fn)(obj_t *spawner, int code);
void bg_enemy_spawner_payload(bge_payload_fn fn);
int  bg_enemy_spawners_open(void);        /* spawners still waiting / falling (tests, FD arenas) */

/* ---- bg_gim.c (types 2400-2479; build/re/spec_bg_gimmicks.md). Families share a type: par = member (importer BGG_SUB, lvobj anim
 * bits 0-7), layer = instance layer (bits 8-14). Final Dreadnought maps its HazardSpinMine / GimmickAntigravSpawner / ObjectSiren /
 * ObjectTransporter instances to these types (the same events run there: 14395 / 14396, 14369-14378, 14656). */
enum { OT_BGG_TBMASK = 2400,   /* ObjectTeleBlockMask_3255 (Q7, invisible, rotated by A) */
       OT_BGG_TBLOCK = 2401,   /* runtime: ObjectTeleBlock_3254 (14175) */
       OT_BGG_SLIDE = 2402,    /* GimmickSpiralSlideStart_3268 (par 0) / End_3269 (par 1) */
       OT_BGG_SNAKE = 2403,    /* GimmickSnakeDrill_3273 (8 segments per placement) */
       OT_BGG_SPIRE = 2404,    /* GimmickSnowSpire_3249 */
       OT_BGG_SPMASK = 2405,   /* GimmickSnowSpireMask_3251 (Q7) */
       OT_BGG_SOLAR = 2406,    /* BGSolarPanel_3252 */
       OT_BGG_GPLAT1 = 2407,   /* LevelGlacierPlatform1_3275 (objects.c path registry F 1 G 10) */
       OT_BGG_GPLAT2 = 2408,   /* LevelGlacierPlatform2_3293 (F 2 G 80) */
       OT_BGG_BAR = 2409,      /* GimmickBGBar_3283 */
       OT_BGG_BARMASK = 2410,  /* GimmickBGBarMask_3284 (Q23) */
       OT_BGG_BGPLAT = 2411,   /* BGPlatform1_3313 (Q8 Q9) */
       OT_BGG_FERRIS = 2412,   /* LevelCrankFerris_3317 (Q8 Q9 Q13 path platform, own steps) */
       OT_BGG_FERRISV = 2413,  /* LevelCrankFerrisMask / 2 / 3 / 4 (par 0-3) */
       OT_BGG_FSTOP = 2414,    /* LevelCrankFerrisCollisionStopper_3324 */
       OT_BGG_AGSPAWN = 2415,  /* GimmickAntigravSpawner_3323 */
       OT_BGG_AG = 2416,       /* runtime: GimmickAntigrav_3322 (14376) */
       OT_BGG_MAGNET = 2417,   /* GimmickMagnetLauncher_3326 */
       OT_BGG_MSTOP = 2418,    /* GimmickMagnetLauncherLeft_3328 (par 0) / Right_3329 (par 1) */
       OT_BGG_MLIGHT = 2419,   /* GimmickMagnetLauncherLight_3330 */
       OT_BGG_SPINMINE = 2420, /* HazardSpinMine_3331 (Q20, armoured) */
       OT_BGG_SWBLOCK = 2421,  /* ObjectSwitchBlock1_3354 (par 0) / 2_3355 (par 1) (Q7) */
       OT_BGG_TSWITCH = 2422,  /* GimmickTeleSwitch_3356 */
       OT_BGG_TSCRYSTAL = 2423,/* GimmickTeleSwitchCrystal1_3357 (par 0) / 2_3358 (par 1) */
       OT_BGG_TRANSP = 2424,   /* ObjectTransporter_3360 (Q7 pad) */
       OT_BGG_SIREN = 2425,    /* ObjectSiren_3362 */
       OT_BGG_END = 2425 };
/* (bg_gim_player / bg_gim_ride_m: fp.h, the M 95 / M 96 rides of player.c's ST_RIDE) */
/* reusable rules (Final Dreadnought): the magnet light pulls while the light's A (its magnet's cooldown D) is below this
 * (BG4 14379: A == 0 -> 1; BG5 14633: A < 70); the switch-block colour (CurrentColor_3359 K) */
void bg_gim_set_light_rule(int a_below);
int  bg_gim_switch_color(void);

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
