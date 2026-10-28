/* Thermal Base (session 21c): shared declarations of the tb_*.c modules. Engine hooks are in src/fp.h ("Thermal Base");
 * each module adds its own type enum and helpers here, in its own block. Stage index 6, json 45-50 = tb1-tb6. */
#ifndef FP_TB_H
#define FP_TB_H
#include "../fp.h"

/* ---- tb_flow.c (types 1792-1823) */

/* ---- tb_enemy.c (types 1824-1887) */

/* ---- tb_gim.c (types 1888-1967; build/re/spec_tb_gimmicks.md). Families share a type: par = member (importer TBG_SUB,
 * lvobj anim bits 0-7), layer = instance layer (bits 8-14). Placed classes 1888-1949, runtime objects 1950-1967. */
enum { OT_TBG_LIFT = 1888 /* LevelLift_2649 (Q7 Q13) */, OT_TBG_LIFTEND = 1889 /* LevelLiftTop / Bottom (par 0 / 1, invisible) */,
       OT_TBG_KEY = 1890 /* ObjectKeyCard_2685 */, OT_TBG_TERM = 1891 /* LockedDoorTerminal_2689 */,
       OT_TBG_LDOOR = 1892 /* LockedDoorBack / Front / Seal (par 0 / 1 / 2) */, OT_TBG_BARRIER = 1893 /* LockedDoorBarrier_2688 (Q7, invisible) */,
       OT_TBG_MDOOR = 1894 /* MetalDoorBack / Front (par 0 / 1): scenery that opens */, OT_TBG_WLIGHT = 1895 /* TBWallLightAnim_2644 */,
       OT_TBG_FPLAT = 1896 /* LevelFirePlatform1_2708 (Q8 Q9 Q12 Q13: objects.c path platform, G 160) */,
       OT_TBG_FFLAME = 1897 /* LevelFirePlatformFlame1 / 2 (par 0 / 1) */,
       OT_TBG_SPLAT = 1898 /* LevelScrollingPlatform1 / 2 (par 0 / 1; Q7 Q13, invisible) */, OT_TBG_SMASK = 1899 /* ...Mask1 / 2 (drawn) */,
       OT_TBG_SFLAME = 1900 /* ...Flame1 / 2 */, OT_TBG_SMARK = 1901 /* ...Start / End (par 0 / 1, invisible) */,
       OT_TBG_PUSH = 1902 /* LevelScrollingPlatformPlayerPush_2697 (invisible) */,
       OT_TBG_PISTON = 1903 /* LevelPistonPlatform1_2713 (Q8 Q9 Q13, invisible) */, OT_TBG_PMASK = 1904 /* LevelPistonPlatformMask_2712 */,
       OT_TBG_SPIN = 1905 /* LevelSpinPlatform_2731 (Q8 Q9 Q13, invisible) */, OT_TBG_SPINMASK = 1906, OT_TBG_SPINSUP = 1907,
       OT_TBG_VENT = 1908 /* AirVentPlatform_2694 (Q8 Q9) */, OT_TBG_LAVA = 1909 /* HazardLavaPit_2736 */,
       OT_TBG_LFALL = 1910 /* HazardLavaFall_2737 (par 0) / HazardLavaFall3_2777 (par 1) */, OT_TBG_LWARN = 1911 /* HazardLavaFallWarning_2739 */,
       OT_TBG_MAGMA = 1912 /* MagmaFlow_2676 */, OT_TBG_XBLOCK = 1913 /* ObjectExplosiveBlock_2738 (Q7) */,
       OT_TBG_BUTTON = 1914 /* SwitchButtonLeft / Right (par 0 / 1) */, OT_TBG_FTRAP = 1915 /* HazardFloorTrap_2761 (Q7) */,
       OT_TBG_BLOCKER = 1916 /* PlayerBlocker_2780 (Q7, invisible) */,
       OT_TBG_CONV = 1917 /* GimmickConveyorBeltRight / 2 / 3, Left / 2 / 3 (par 0-5; Q7 Q31) */,
       OT_TBG_LOOP0 = 1918 /* TBPipeLoop2 / 3 / 6 / 7 (Q7 Q10, pixel) */, OT_TBG_LOOP0B = 1919 /* TBPipeLoop5 / 9 / 11 (Q7 Q10, box) */,
       OT_TBG_LOOP1 = 1920 /* TBPipeLoop1 / 4 / 8 (Q7 Q11, pixel) */, OT_TBG_LOOP1B = 1921 /* TBPipeLoop10 (Q7 Q11, box) */,
       OT_TBG_CTOP = 1922 /* GimmickCrankPlatformTop1 / 2 (Q8 Q9) */, OT_TBG_CPART = 1923 /* ...Mid1 / 2, Base1 / 2, Arm1 / 2 (par 0-5) */,
       OT_TBG_PRESS = 1924 /* GimmickGiantPresserTube (par 0) / GimmckGiantPresser (par 1) */, OT_TBG_PHIT = 1925 /* GimmckGiantPresserHitbox_2879 */,
       OT_TBG_TURB = 1926 /* GimmickWaterTurbine_2863 (Q7) */, OT_TBG_TBUBSP = 1927 /* TubeBubbleSpawner_2848 */,
       OT_TBG_BGTURB = 1929 /* UnderwaterBGTurbine_2880 */, OT_TBG_WTLIGHT = 1930 /* TBWaterLight_2866 */,
       OT_TBG_LEVER = 1931 /* GimmickWaterLever1 / 2 */, OT_TBG_WMASK = 1932 /* GimmickWaterLevelMask1 / 2 (Q23 hang handles) */,
       OT_TBG_BWAVE = 1933 /* GimmickBubbleWaveSpawner / 2 */, OT_TBG_FPLAT80 = 1934 /* LevelFirePlatform1_2665 (TB1: ctor G 80) */,
       OT_TBG_TBUB = 1950 /* runtime: TubeBubble_2849 / GimmickWaterTurbineBubble_2865 */,
       OT_TBG_DESTROYER = 1951 /* runtime: ObjectExplosiveBlockDestroyer_2744 */,
       OT_TBG_FX = 1952 /* runtime: one-shot effects (explosions, sparkles) */,
       OT_TBG_END = 1952 };
int  tb_gim_keycards(void);                     /* GUIKeyCards K: key cards held (HUD, tests) */
int  tb_gim_elevator(void);                     /* TB5's elevator (12524-12532): 0 waiting, 1 rising, 2 up (the encounter: tb_boss.c) */
int  tb_gim_button_presses(void);               /* switch buttons pressed so far this act (the bosses' trapdoor, tests) */

/* ---- tb_boss.c (types 1968-2047) */

#endif
