/* Final Dreadnought bosses A (rounds 1-2: fd1a fd1b fd2a fd2b; types 2912-2991), called by fd_boss.c's dispatch.
 * Spec: build/re/spec_fd_bosses_a.md. */
#include "fd.h"
void  fdb12_spawn(void) {}
void  fdb12_pre(player_t *p) { (void)p; }
void  fdb12_frame(player_t *p) { (void)p; }
int   fdb12_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   fdb12_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int   fdb12_q20(int type) { (void)type; return 0; }
int   fdb12_input(uint32_t *held) { (void)held; return 0; }
int   fdb12_player_hidden(void) { return 0; }
float fdb12_hud_dy(void) { return 0.f; }
