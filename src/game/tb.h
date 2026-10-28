/* Thermal Base (session 21c): shared declarations of the tb_*.c modules. Engine hooks are in src/fp.h ("Thermal Base");
 * each module adds its own type enum and helpers here, in its own block. Stage index 6, json 45-50 = tb1-tb6. */
#ifndef FP_TB_H
#define FP_TB_H
#include "../fp.h"

/* ---- tb_flow.c (types 1792-1823) */
enum { OT_TBF_LIFT = 1792,          /* LevelLift_2649: the elevator box (Q7 Q13; tb_flow.c lifts) */
       OT_TBF_LIFTTOP = 1793,       /* LevelLiftTop_2650: the upper stop marker (invisible) */
       OT_TBF_LIFTBOT = 1794,       /* LevelLiftBottom_2651: the lower stop marker (invisible) */
       OT_TBF_WALLLIGHT = 1795 };   /* TBWallLightAnim_2644: wall light, frame (g3 / 4) mod 12 */
int  tb_flow_lift_state(int k);     /* host tests / other modules: the k-th lift's Q (0 bottom, 1 rising, 2 top, 3 sinking), -1 none */

/* ---- tb_enemy.c (types 1824-1887) */

/* ---- tb_gim.c (types 1888-1967) */

/* ---- tb_boss.c (types 1968-2047) */

#endif
