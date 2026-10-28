/* Pangu Lagoon gimmicks (session 23): types 2144-2223. Spec: build/re/spec_pl_gimmicks.md. */
#include "pl.h"
void pl_gim_spawn(void) {}
void pl_gim_pre(player_t *p) { (void)p; }
void pl_gim_frame(player_t *p) { (void)p; }
void pl_gim_draw(int pass) { (void)pass; }
int  pl_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  pl_gim_q20(int type) { (void)type; return 0; }
