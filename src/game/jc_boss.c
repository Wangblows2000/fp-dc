/* Jade Creek bosses (session 21): types 1430-1535. Spec: build/re/spec_jc_bosses.md. */
#include "jc.h"
void jc_boss_spawn(void) {}
void jc_boss_pre(player_t *p) { (void)p; }
void jc_boss_frame(player_t *p) { (void)p; }
int  jc_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  jc_boss_q20(int type) { (void)type; return 0; }
int  jc_boss_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int  jc_boss_input(uint32_t *held) { (void)held; return 0; }
int  jc_boss_player_hidden(void) { return 0; }
