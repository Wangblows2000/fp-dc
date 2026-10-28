/* Thermal Base bosses and arena (session 21c): types 1968-2047. Spec: build/re/spec_tb_bosses.md. */
#include "tb.h"
void  tb_boss_spawn(void) {}
void  tb_boss_pre(player_t *p) { (void)p; }
void  tb_boss_frame(player_t *p) { (void)p; }
int   tb_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   tb_boss_q20(int type) { (void)type; return 0; }
int   tb_boss_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int   tb_boss_input(uint32_t *held) { (void)held; return 0; }
int   tb_boss_player_hidden(void) { return 0; }
float tb_boss_hud_dy(void) { return 0.f; }
