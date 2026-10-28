/* Battle Glacier bosses and arena (session 24): types 2480-2559. Spec: build/re/spec_bg_bosses.md. */
#include "bg.h"
void  bg_boss_spawn(void) {}
void  bg_boss_pre(player_t *p) { (void)p; }
void  bg_boss_frame(player_t *p) { (void)p; }
int   bg_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   bg_boss_q20(int type) { (void)type; return 0; }
int   bg_boss_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int   bg_boss_input(uint32_t *held) { (void)held; return 0; }
int   bg_boss_player_hidden(void) { return 0; }
float bg_boss_hud_dy(void) { return 0.f; }
