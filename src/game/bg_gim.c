/* Battle Glacier gimmicks (session 24): types 2400-2479. Spec: build/re/spec_bg_gimmicks.md. */
#include "bg.h"
void bg_gim_spawn(void) {}
void bg_gim_pre(player_t *p) { (void)p; }
void bg_gim_frame(player_t *p) { (void)p; }
void bg_gim_draw(int pass) { (void)pass; }
int  bg_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  bg_gim_q20(int type) { (void)type; return 0; }
