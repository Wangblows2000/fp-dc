/* Thermal Base (session 21c): shared declarations of the tb_*.c modules. Engine hooks are in src/fp.h ("Thermal Base");
 * each module adds its own type enum and helpers here, in its own block. Stage index 6, json 45-50 = tb1-tb6. */
#ifndef FP_TB_H
#define FP_TB_H
#include "../fp.h"

/* ---- tb_flow.c (types 1792-1823) */

/* ---- tb_enemy.c (types 1824-1887; build/re/spec_tb_enemies.md): placed 1824-1855 (tools/import_frame.py TB_ENEMIES), runtime
 * children 1856-1887. Q20 (RE on_frame_45..49_init qualifier 20): seeker, push-up hitbox, lava squid (+ its debris), tank bot,
 * saucer (+ its debris); they take the shared hits of combat.c through tb_enemy_q20. */
enum {
    OT_TBE_SEEKER = 1824,   /* EnemySeeker_2603 (q20, A 5) */
    OT_TBE_PUSHUP = 1825,   /* EnemyPushUp_2669 (the body; direction 0 rises / 1 crouches) */
    OT_TBE_PUHIT = 1826,    /* EnemyPushUpHitbox_2663 (q20, A 16, ctor invisible: the head) */
    OT_TBE_SQUID = 1827,    /* EnemyLavaSquid_2740 (q20, A 15) */
    OT_TBE_TANK = 1828,     /* EnemyTankbot_2838 (q20, A 15) */
    OT_TBE_SAUCER = 1829,   /* EnemySaucer_2828 (q20, A 22) */
    OT_TBE_LEG1 = 1830,     /* EnemySaucerLeg1_2827 .. Leg4_2826: 1830-1833 (member 0-3) */
    OT_TBE_LEG4 = 1833,
    OT_TBE_SLIM = 1834,     /* EnemySaucerLimiter_2832 (ctor invisible) */
    OT_TBE_BULLET = 1856,   /* runtime: EnemyHomingBullet_2648 (a seeker's shot) */
    OT_TBE_PULAS = 1857,    /* runtime: EnemyPushUpLaser_2670 */
    OT_TBE_PULAS2 = 1858,   /* runtime: EnemyPushUpLaser2_2695 (the invisible ground wave) */
    OT_TBE_PULAS3 = 1859,   /* runtime: EnemyPushUpLaser3_2696 (the wave's diamonds) */
    OT_TBE_PUDEB = 1860,    /* runtime: EnemyPushUpDebris_2664 */
    OT_TBE_SQLAS = 1861,    /* runtime: EnemyLavaSquidLaser_2741 */
    OT_TBE_DEBRIS = 1862,   /* runtime: q20 flying debris: EnemyLavaSquidDebris_2742 (av[7] 0), EnemySaucerDebris_2831 (1) */
    OT_TBE_TDEB = 1863      /* runtime: EnemyTankbotDebris_2839 (the tracks) */
};   /* the saucer's GimmickFireball_1192 / EffectEmber_1025 are Relic Maze's OT_FBALL / OT_EMBER (relic_gima.c fireball_fly / _rules) */

/* ---- tb_gim.c (types 1888-1967) */

/* ---- tb_boss.c (types 1968-2047) */

#endif
