/* Final Dreadnought bosses and arena (session 24): types 2912-3071. Dispatch only: bosses A (rounds 1-2, types 2912-2991,
 * fd_boss_r12.c, spec build/re/spec_fd_bosses_a.md) and bosses B (rounds 3-4, types 2992-3071, fd_boss_r34.c,
 * spec build/re/spec_fd_bosses_b.md). Every hook calls both halves; each half returns at once when its classes are not placed. */
#include "fd.h"
void  fd_boss_spawn(void) { fdb12_spawn(); fdb34_spawn(); }
void  fd_boss_pre(player_t *p) { fdb12_pre(p); fdb34_pre(p); }
void  fd_boss_frame(player_t *p) { fdb12_frame(p); fdb34_frame(p); }
int   fd_boss_obj(obj_t *o, player_t *p) { return o->type <= OT_FDB_A_LAST ? fdb12_obj(o, p) : fdb34_obj(o, p); }
int   fd_boss_q20(int type)
{
    if (type < OT_FDB_FIRST || type > OT_FDB_LAST) return 0;
    return type <= OT_FDB_A_LAST ? fdb12_q20(type) : fdb34_q20(type);
}
int   fd_boss_draw_obj(const obj_t *o, int pass) { return o->type <= OT_FDB_A_LAST ? fdb12_draw_obj(o, pass) : fdb34_draw_obj(o, pass); }
int   fd_boss_input(uint32_t *held) { return fdb12_input(held) || fdb34_input(held); }
int   fd_boss_player_hidden(void) { return fdb12_player_hidden() || fdb34_player_hidden(); }
float fd_boss_hud_dy(void) { return fdb12_hud_dy() + fdb34_hud_dy(); }
