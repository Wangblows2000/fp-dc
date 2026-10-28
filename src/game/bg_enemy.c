/* Battle Glacier enemies (session 24): types 2336-2399. Spec: build/re/spec_bg_enemies.md. */
#include "bg.h"
void bg_enemy_spawn(void) {}
void bg_enemy_pre(player_t *p) { (void)p; }
void bg_enemy_draw(int pass) { (void)pass; }
int  bg_enemy_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  bg_enemy_q20(int type) { (void)type; return 0; }
