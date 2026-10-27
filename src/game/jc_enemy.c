/* Jade Creek enemies (session 21): types 1310-1369. Spec: build/re/spec_jc_enemies.md. */
#include "jc.h"
void jc_enemy_spawn(void) {}
void jc_enemy_pre(player_t *p) { (void)p; }
void jc_enemy_draw(int pass) { (void)pass; }
int  jc_enemy_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  jc_enemy_q20(int type) { (void)type; return 0; }
