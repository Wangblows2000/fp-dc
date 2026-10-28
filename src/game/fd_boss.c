/* Final Dreadnought bosses and arena (session 24): types 2912-3071. Spec: build/re/spec_fd_bosses.md. */
#include "fd.h"
void  fd_boss_spawn(void) {}
void  fd_boss_pre(player_t *p) { (void)p; }
void  fd_boss_frame(player_t *p) { (void)p; }
int   fd_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   fd_boss_q20(int type) { (void)type; return 0; }
int   fd_boss_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int   fd_boss_input(uint32_t *held) { (void)held; return 0; }
int   fd_boss_player_hidden(void) { return 0; }
float fd_boss_hud_dy(void) { return 0.f; }
