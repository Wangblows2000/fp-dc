/* Final Dreadnought gimmicks (session 24): types 2752-2911. Spec: build/re/spec_fd_gimmicks.md. */
#include "fd.h"
void fd_gim_spawn(void) {}
void fd_gim_pre(player_t *p) { (void)p; }
void fd_gim_frame(player_t *p) { (void)p; }
void fd_gim_draw(int pass) { (void)pass; }
int  fd_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  fd_gim_q20(int type) { (void)type; return 0; }
