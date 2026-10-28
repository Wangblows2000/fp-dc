/* Final Dreadnought stage script (session 24, stages 10-13 (rounds 1-4, two levels each), json 66-73 = fd1a fd1b fd2a fd2b fd3a fd3b fd4a fd4b): cards, routes, act events. Types 2560-2623.
 * Spec: build/re/spec_fd_flow.md. */
#include "fd.h"
void  fd_flow_spawn(void) {}
void  fd_flow_frame(player_t *p) { (void)p; }
void  fd_flow_draw(int pass) { (void)pass; }
int   fd_flow_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   fd_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage >= 10 && L->stage <= 13 ? (L->stage - 10) * 2 + L->act : 0; }
float fd_flow_hud_dy(void) { return 0.f; }
