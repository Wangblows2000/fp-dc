/* Trap Hideout (session 21b): shared declarations of the th_*.c modules. Engine hooks are in src/fp.h ("Trap Hideout");
 * each module adds its own type enum and helpers here, in its own block. Stage index 8, json 57-59 = th1-th3; Carol / Milla. */
#ifndef FP_TH_H
#define FP_TH_H
#include "../fp.h"

/* ---- th_flow.c (types 1536-1559): stage-wide scenery driven by the frames' own events (spec_th_flow 5) */
enum { OT_THF_BACKLIGHT = OT_THF_FIRST,   /* thbacklight1_3113: frame (g3 / 10) mod 6 every frame (13532 / handle_frame_57 inline) */
       OT_THF_FLAME,                      /* thflame_3176: a torch flame, its own looping animation (speed 16; stopped while paused) */
       OT_THF_SPOT };                     /* thspotlight_3209 (TH3, json layer 0): 14099 / 14082 / 14083, drawn in the parallax order */
void th_flow_spot_draw(void);             /* the parallax hook (par_set_hook): TH3's spotlights */

/* ---- th_enemy.c (types 1560-1619; build/re/spec_th_enemies.md): placed 1560-1589 (tools/import_frame.py TH_ENEMIES), runtime
 * children 1590-1619. Q20 classes (RE on_frame_57..59_init qualifier 20) take the shared hits of combat.c through th_enemy_q20.
 * EnemyNinja_3193 (th3) is not an enemy: an invisible prop of the act-3 finale (13850 / 13854, GUIGoalMessage group): th_boss.c. */
enum {
    OT_TH_TOTEM = 1560,    /* EnemyTotematic_3144 (q7 solid, q20, A 1) */
    OT_TH_PUP = 1561,      /* EnemyMetalPup_3148 (q20) */
    OT_TH_PUPPET = 1562,   /* EnemyShockPuppet_3149 (q20) */
    OT_TH_ORB = 1563,      /* EnemyBladeOrbiter_3165 (q20) */
    OT_TH_BLADEL = 1564,   /* EnemyBladeOrbiterLeft_3163 */
    OT_TH_BLADER = 1565,   /* EnemyBladeOrbiterRight_3164 */
    OT_TH_PUNCH = 1566,    /* EnemyRocketPuncher_3182 (q20) */
    OT_TH_PHAND = 1567,    /* EnemyRocketPuncherHand_3183 */
    OT_TH_PSHIELD = 1568,  /* EnemyRocketPuncherShield_3184 (q7 solid, q20, A 99) */
    OT_TH_PWARN = 1569,    /* EffectRocketPuncherWarning_3185 (ctor invisible) */
    OT_TH_BIKE = 1570,     /* EnemyBikeNinja_3203 (the sprite) */
    OT_TH_BMASK = 1571,    /* EnemyBikeNinjaMask_3204 (q20, invisible: the body and its sensors) */
    OT_TH_BWARN = 1572,    /* EffectBikeNinjaWarning_3206 (ctor invisible) */
    OT_TH_STAR = 1590,     /* runtime: EnemyNinjaStar_3221 (thrown by a bike ninja, 13765) */
    OT_TH_BDEAD = 1591     /* runtime: EnemyBikeNinjaDefeated_3205 (13787) */
};
/* TH1 13488: a totem inside the view (80), not defeated, overlapping an object of the qualifier +0x22d98 gets A = 0 (the shared
 * 2764 defeats it). RV th_enemies/th1_totem (XLIST q:): that qualifier = the scan lasers (HazardScanLaserSweep x5, ...Right,
 * ...Left, ...Down x8, ...Up x8, ...Boss). The gimmick / boss module calls this with each beam object; returns the count. */
int th_enemy_totem_zap(const obj_t *beam);

/* ---- th_gim.c (types 1620-1679) */

/* ---- th_boss.c (types 1680-1791) */

#endif
