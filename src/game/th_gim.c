/* Trap Hideout gimmicks / traps (session 21b): types 1620-1679. Spec: build/re/spec_th_gimmicks.md. */
#include "th.h"
void th_gim_spawn(void) {}
void th_gim_pre(player_t *p) { (void)p; }
void th_gim_frame(player_t *p) { (void)p; }
void th_gim_draw(int pass) { (void)pass; }
int  th_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  th_gim_q20(int type) { (void)type; return 0; }
