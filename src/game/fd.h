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

/* ---- fd_boss.c (types 2912-3071): fd_boss.c is the dispatch (spawn / pre / frame / obj / draw / q20 / input / hidden / hud);
 * bosses A (rounds 1-2, fd1a-fd2b, types 2912-2991) live in fd_boss_r12.c, bosses B (rounds 3-4, fd3a-fd4b, types 2992-3071)
 * in fd_boss_r34.c. fd_boss.c calls both modules' hooks every frame (each returns at once when its classes are not placed)
 * and routes per-object calls by type range. */
enum { OT_FDB_A_FIRST = 2912, OT_FDB_A_LAST = 2991, OT_FDB_B_FIRST = 2992, OT_FDB_B_LAST = 3071 };

/* ---- fd_boss.c / fd_boss_r12.c, bosses A (types 2912-2991) */
void  fdb12_spawn(void), fdb12_pre(player_t *p), fdb12_frame(player_t *p);
int   fdb12_obj(obj_t *o, player_t *p), fdb12_draw_obj(const obj_t *o, int pass), fdb12_q20(int type), fdb12_input(uint32_t *held);
int   fdb12_player_hidden(void);
float fdb12_hud_dy(void);

/* ---- fd_boss_r34.c, bosses B (types 2992-3071): the same hooks, called by fd_boss.c */
void  fdb34_spawn(void), fdb34_pre(player_t *p), fdb34_frame(player_t *p);
int   fdb34_obj(obj_t *o, player_t *p), fdb34_draw_obj(const obj_t *o, int pass), fdb34_q20(int type), fdb34_input(uint32_t *held);
int   fdb34_player_hidden(void);
float fdb34_hud_dy(void);
/* bosses B types (tools/import_frame.py FD_BOSS B; the lvobj anim field carries sub id | instance layer << 8). Placed classes of
 * json 71 (fd3b) / 73 (fd4b); OT_FDB_RT is every runtime object of the module (o->st = its kind, FDB_K_*). */
enum { OT_FDB_STONE = 2992 /* KingdomStone_3680 (the scripts' state holder: D phase, E timer) */, OT_FDB_BREVON = 2993 /* Brevon_3701 */,
       OT_FDB_KNIFE = 2994 /* BrevonKnifeArm_3702 */, OT_FDB_TERM = 2995 /* ComputerTerminal_3682 / 2_3684 (sub 1) / 3_3703 (sub 2) */,
       OT_FDB_DEVICE = 2996 /* Device_3388 */, OT_FDB_MBODY = 2997 /* NPCMillaBody_3704 */,
       OT_FDB_MM_HEAD = 2998 /* BossMMillaHead_3694 (Q20) */, OT_FDB_MM_PART = 2999 /* the other BossMMilla* parts (sub = FDB_MM_*) */,
       OT_FDB_MM_TAIL = 3000 /* BossMMillaTail_3689 (6 segments) */, OT_FDB_HATCH = 3001 /* BossHatchDoor_3312 (Q7, fd3b) */,
       OT_FDB_SUIT_HEAD = 3002 /* BrevonSuitHead_3777 (Q20, A 30) */, OT_FDB_SUIT_PART = 3003 /* BrevonSuit* parts (sub = FDB_SP_*) */,
       OT_FDB_SUIT_WALL = 3004 /* BrevonSuitWallLeft_3797 / Right_3798 (sub 1) */, OT_FDB_HBAR = 3005 /* BossHealthBar1_3800 / 2_3801 (sub 1) */,
       OT_FDB_XHAIR = 3006 /* BrevonCrosshair6_3790 / 7_3791 (sub 1) */, OT_FDB_CAPE = 3007 /* BrevonDummyCape_3792 */,
       OT_FDB_BHIT = 3008 /* BrevonHitbox_3809 (Q20, A 160; runtime) */, OT_FDB_RT = 3009 /* runtime objects (st = FDB_K_*) */,
       OT_FDB_LAST_USED = 3009 };
#ifdef FP_HOST_ENV
int   fdb34_host_state(char *buf, int n);                                       /* one-line state for host tests */
#endif

#endif
