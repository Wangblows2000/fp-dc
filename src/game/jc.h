/* Jade Creek (session 21): shared declarations of the jc_*.c modules. Engine hooks are in src/fp.h ("Jade Creek");
 * each module adds its own type enum and helpers here, in its own block. Stage index 5, json 40-44 = jc1-jc5. */
#ifndef FP_JC_H
#define FP_JC_H
#include "../fp.h"

/* ---- jc_flow.c (types 1280-1309) */

/* ---- jc_enemy.c (types 1310-1369) */

/* ---- jc_gim.c (types 1370-1429; build/re/spec_jc_gimmicks.md). Families share a type: par = member (importer JCG_SUB). */
enum { OT_JCG_STREAM = 1370 /* JCWaterStream1-8 (par = 1..8) */, OT_JCG_LEAFBACK = 1371 /* GimmickLeafPlatformBack_2357 */,
       OT_JCG_LEAF = 1372 /* GimmickLeafPlatform1 / 2 (par 0 / 1) */, OT_JCG_FLEAF = 1373 /* GimmickFlyingLeafPlatform_2476 */,
       OT_JCG_FLEAFM = 1374 /* its mask (the Q8 Q13 platform) */, OT_JCG_SLIDE = 1375 /* JCSlidingPlatform_2377 (Q7 Q13) */,
       OT_JCG_SLIDEM = 1376 /* JCSlidingPlatformMask_2381 (drawn) */, OT_JCG_SLIDEL = 1377 /* JCSlidingPlatformLandPoint_2382 */,
       OT_JCG_COLL = 1378 /* LevelJCCollapsingPlatform 2456 / 2475 (par 0 / 1) */, OT_JCG_CPIECE = 1379 /* runtime: its pieces (2457) */,
       OT_JCG_SEEDSP = 1380 /* GimmickSeedSpawner_2418 */, OT_JCG_SEED = 1381 /* runtime: GimmickFlyingSeed_2419 */,
       OT_JCG_BUBSP = 1382 /* GimmickBubbleSpringSpawner_2449 */, OT_JCG_BUBBACK = 1383 /* runtime: GimmickBubbleSpringBack2_2459 */,
       OT_JCG_BUBFRONT = 1384 /* runtime: GimmickBubbleSpringFront2_2460 */, OT_JCG_SLIP = 1385 /* LevelSlipperySurface_2473 */,
       OT_JCG_FALLLEAF = 1386 /* EffectFallingLeaf_2396 (placed once in JC1; created by the flying leaves) */,
       OT_JCG_END = 1386 };
int  jc_gim_stream_push(void);                 /* this frame's water-stream push on the player's X (-1, 0, +1; tests / other modules) */

/* ---- jc_boss.c (types 1430-1479: Hovercop, Neera; acts 2 / 3) */

/* ---- jc_boss5.c (types 1480-1535: act 5's bosses and scenes) */
enum { OT_JCB5_FIRST = 1480 };
void jc_boss5_spawn(void), jc_boss5_pre(player_t *p), jc_boss5_frame(player_t *p);
int  jc_boss5_obj(obj_t *o, player_t *p), jc_boss5_q20(int type), jc_boss5_draw_obj(const obj_t *o, int pass), jc_boss5_input(uint32_t *held);
int  jc_boss5_player_hidden(void);

#endif
