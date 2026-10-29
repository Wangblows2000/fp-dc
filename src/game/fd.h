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

/* ---- fd_gim.c (types 2752-2911) */

/* ---- fd_boss.c (types 2912-3071) */

#endif
