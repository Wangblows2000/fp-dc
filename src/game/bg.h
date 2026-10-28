/* Battle Glacier (session 24): shared declarations of the bg_*.c modules. Engine hooks are in src/fp.h ("Battle Glacier");
 * each module adds its own type enum and helpers here, in its own block. stage index 9, json 60-65 = bg1-bg6. */
#ifndef FP_BG_H
#define FP_BG_H
#include "../fp.h"

/* ---- bg_flow.c (types 2304-2335) */

/* ---- bg_enemy.c (types 2336-2399) */

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

/* ---- bg_boss.c (types 2480-2559) */

#endif
