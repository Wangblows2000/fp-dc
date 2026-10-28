/* Thermal Base enemies (session 21c): types 1824-1887. Spec: build/re/spec_tb_enemies.md. */
#include "tb.h"
void tb_enemy_spawn(void) {}
void tb_enemy_pre(player_t *p) { (void)p; }
void tb_enemy_draw(int pass) { (void)pass; }
int  tb_enemy_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  tb_enemy_q20(int type) { (void)type; return 0; }
