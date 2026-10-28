/* Final Dreadnought enemies (session 24): types 2624-2751. Spec: build/re/spec_fd_enemies.md. */
#include "fd.h"
void fd_enemy_spawn(void) {}
void fd_enemy_pre(player_t *p) { (void)p; }
void fd_enemy_draw(int pass) { (void)pass; }
int  fd_enemy_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  fd_enemy_q20(int type) { (void)type; return 0; }
