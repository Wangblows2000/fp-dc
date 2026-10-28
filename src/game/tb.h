/* Thermal Base (session 21c): shared declarations of the tb_*.c modules. Engine hooks are in src/fp.h ("Thermal Base");
 * each module adds its own type enum and helpers here, in its own block. Stage index 6, json 45-50 = tb1-tb6. */
#ifndef FP_TB_H
#define FP_TB_H
#include "../fp.h"

/* ---- tb_flow.c (types 1792-1823) */

/* ---- tb_enemy.c (types 1824-1887) */

/* ---- tb_gim.c (types 1888-1967) */

/* ---- tb_boss.c (types 1968-2047; build/re/spec_tb_bosses.md): tb_boss.c (spawn, lists, shared helpers, dispatch) and the
 * encounter files tb_boss_shrimp.c (TB1 RoboShrimp), tb_boss_pusher.c (TB2 wall pusher), tb_boss_spider.c (TB3 Syntax Spider),
 * tb_boss_squid.c (TB3 RoboSquid), tb_boss_syntax.c (TB6 arena: the Syntax squid and its platforms). Placed classes 1968-2015
 * (tools/import_frame.py TB_BOSS; lvobj anim = sub | instance layer << 8), runtime objects 2016-2047. */
enum {
    OT_TBB_SHRIMP = 1968,   /* BossRoboShrimp_2673 (Q20, A 1, H 90; C 5 while a segment lives: 11845) */
    OT_TBB_SH_SEG = 1969,   /* BossRoboShrimpSegment_2672 (Q20, A 10; six off-map templates) */
    OT_TBB_SH_TAIL = 1970,  /* BossRoboShrimpTail_2675 */
    OT_TBB_SH_LIGHT = 1971, /* BossRoboShrimpLightning_2674 (direction 1: sparks, hurts 10: 11879) */
    OT_TBB_SH_BLOCK = 1972, /* BossRoboShrimpBlocker_2691 (invisible 64 x 64 steering boxes: 11856) */
    OT_TBB_TRAPDOOR = 1973, /* MinibossTrapdoor_2747 (TB2: the wall pusher's ceiling door; TB3: the spider arena's) */
    OT_TBB_PUSHER = 1974,   /* EnemyWallPusher_2748 (Q20 armoured, A 30 every frame) */
    OT_TBB_SQUID = 1975,    /* BossRoboSquid_2765 */
    OT_TBB_SQ_GLASS = 1976, /* BossRoboSquidGlass_2764 */
    OT_TBB_SQ_JET = 1977,   /* BossRoboSquidJet_2766 */
    OT_TBB_SQ_ARML = 1978,  /* BossRoboSquidLeftArm_2762 (7) */
    OT_TBB_SQ_ARMR = 1979,  /* BossRoboSquidRightArm_2763 (7) */
    OT_TBB_SPIDER = 1980,   /* BossSyntaxSpider_2755 (the mask: A 200) */
    OT_TBB_SP_BODY = 1981,  /* BossSyntaxSpiderBody_2753 */
    OT_TBB_SP_CANNON = 1982,/* BossSyntaxSpiderLavaCannon_2752 */
    OT_TBB_SP_ARM = 1983,   /* BossSyntaxSpiderArm_2754 (2) */
    OT_TBB_SP_LEGF = 1984,  /* BossSyntaxSpiderLegFront_2756 (2) */
    OT_TBB_SP_LEGB = 1985,  /* BossSyntaxSpiderLegBack_2751 (2) */
    OT_TBB_SP_WEB = 1986,   /* BossSyntaxSpiderWeb_2760 */
    OT_TBB_SP_GRAPPLE = 1987,/* BossSyntaxGrappleLine_2770 */
    OT_TBB_SPLASH = 1988,   /* BossLavaSplash1_2758 (par 0) / 2_2759 (par 1) */
    OT_TBB_PBLOCK = 1989,   /* PlayerBlocker_2780 (TB3 exit row) */
    OT_TBB_SX_PART = 1990,  /* the TB6 squid's rig, par = member: 0 Back_2887, 1 ShoulderRight_2888, 2 ShoulderLeft_2889, 3 Head_2890,
                               4 Body_2893, 5 ClawRight_2894, 6 ClawLeft_2895 */
    OT_TBB_SX_ARML = 1991,  /* BossSyntaxSquidArmLeft_2891 (7) */
    OT_TBB_SX_ARMR = 1992,  /* BossSyntaxSquidArmRight_2892 (7) */
    OT_TBB_SX_HIT = 1993,   /* BossSyntaxSquidHitbox_2902 (Q20, A 50) */
    OT_TBB_SX_LASER = 1994, /* BossLavaLaser_2896 */
    OT_TBB_SX_SPHERE = 1995,/* BossSpikeSphere_2897 (par 0) / BossSpikeSphere2_2898 (par 1) */
    OT_TBB_SX_SPLASH = 1996,/* BossLavaSplashLeft1_2899 (par 0), Right1_2901 (1), Left2_2903 (2), Right2_2904 (3) */
    OT_TBB_PLAT = 1997,     /* BossTBPlatform_2886 (the arena's floor / ledges; solid) */
    OT_TBB_PLAT2 = 1998,    /* BossTBPlatform2_2908 (the centre floor) */
    OT_TBB_WAYPOINT = 1999, /* BossWaypoint_2905 (invisible) */
    OT_TBB_EYE = 2000,      /* NPCSyntaxEye_2682 (Syntax's eye: shows near the player, follows; 11843 / 12014 / 12019) */
    OT_TBB_PLACED_LAST = 2015,
    OT_TBB_RT = 2016,       /* runtime objects from here */
    OT_TBB_FX = 2016,       /* a one-shot effect played by the boss rules (explosions, splashes) */
    OT_TBB_SHOT = 2017,     /* a boss projectile (par: kind) */
    OT_TBB_END = 2047 };
int   tb_boss_shrimp_state(void);   /* tests: 0 waiting, 1 fight, 2 segments down, 3 defeated, 4 arena open; -1 not placed */
int   tb_boss_pusher_state(void);   /* tests: the trapdoor's C (0 idle, 1 fight, 2 switch hit, 3 open); -1 not placed */

#endif
