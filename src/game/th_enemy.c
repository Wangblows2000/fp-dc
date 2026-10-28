/* Trap Hideout enemies (session 21b): types 1560-1619. Spec: build/re/spec_th_enemies.md. */
#include "th.h"
void th_enemy_spawn(void) {}
void th_enemy_pre(player_t *p) { (void)p; }
void th_enemy_draw(int pass) { (void)pass; }
int  th_enemy_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  th_enemy_q20(int type) { (void)type; return 0; }
