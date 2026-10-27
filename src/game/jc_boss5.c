/* Jade Creek act 5 (session 21): its bosses and scenes, types 1480-1535 (called through jc_boss.c for those types).
 * Spec: build/re/spec_jc_boss5.md. */
#include "jc.h"
void jc_boss5_spawn(void) {}
void jc_boss5_pre(player_t *p) { (void)p; }
void jc_boss5_frame(player_t *p) { (void)p; }
int  jc_boss5_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  jc_boss5_q20(int type) { (void)type; return 0; }
int  jc_boss5_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int  jc_boss5_input(uint32_t *held) { (void)held; return 0; }
int  jc_boss5_player_hidden(void) { return 0; }
