/* Final Dreadnought bosses B (rounds 3-4: fd3a fd3b fd4a fd4b; types 2992-3071), called by fd_boss.c's dispatch.
 * Spec: build/re/spec_fd_bosses_b.md. Stubs (session 24, created by bosses A): bosses B fills them. */
#include "fd.h"
void  fdb34_spawn(void) {}
void  fdb34_pre(player_t *p) { (void)p; }
void  fdb34_frame(player_t *p) { (void)p; }
int   fdb34_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   fdb34_draw_obj(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int   fdb34_q20(int type) { (void)type; return 0; }
int   fdb34_input(uint32_t *held) { (void)held; return 0; }
int   fdb34_player_hidden(void) { return 0; }
float fdb34_hud_dy(void) { return 0.f; }
