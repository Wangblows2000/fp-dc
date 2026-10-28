/* Pangu Lagoon enemies (session 23): types 2080-2143. Spec: build/re/spec_pl_enemies.md. */
#include "pl.h"
void pl_enemy_spawn(void) {}
void pl_enemy_pre(player_t *p) { (void)p; }
void pl_enemy_draw(int pass) { (void)pass; }
int  pl_enemy_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  pl_enemy_q20(int type) { (void)type; return 0; }
