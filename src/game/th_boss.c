/* Trap Hideout bosses and arena (session 21b): types 1680-1791. Spec: build/re/spec_th_bosses.md. */
#include "th.h"
void  th_boss_spawn(void) {}
void  th_boss_pre(player_t *p) { (void)p; }
void  th_boss_frame(player_t *p) { (void)p; }
int   th_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   th_boss_q20(int type) { (void)type; return 0; }
int   th_boss_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int   th_boss_input(uint32_t *held) { (void)held; return 0; }
int   th_boss_player_hidden(void) { return 0; }
float th_boss_hud_dy(void) { return 0.f; }
