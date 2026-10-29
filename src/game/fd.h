/* Final Dreadnought (session 24): shared declarations of the fd_*.c modules. Engine hooks are in src/fp.h ("Final Dreadnought");
 * each module adds its own type enum and helpers here, in its own block. stages 10-13 (rounds 1-4, two levels each), json 66-73 = fd1a fd1b fd2a fd2b fd3a fd3b fd4a fd4b. */
#ifndef FP_FD_H
#define FP_FD_H
#include "../fp.h"

/* ---- fd_flow.c (types 2560-2623) */

/* ---- fd_enemy.c (types 2624-2751) */

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
