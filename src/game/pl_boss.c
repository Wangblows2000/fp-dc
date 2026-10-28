/* Pangu Lagoon bosses and arena (session 23): types 2224-2303. Spec: build/re/spec_pl_bosses.md. */
#include "pl.h"
void  pl_boss_spawn(void) {}
void  pl_boss_pre(player_t *p) { (void)p; }
void  pl_boss_frame(player_t *p) { (void)p; }
int   pl_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   pl_boss_q20(int type) { (void)type; return 0; }
int   pl_boss_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int   pl_boss_input(uint32_t *held) { (void)held; return 0; }
int   pl_boss_player_hidden(void) { return 0; }
float pl_boss_hud_dy(void) { return 0.f; }
