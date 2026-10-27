/* Jade Creek gimmicks (session 21): types 1370-1429. Spec: build/re/spec_jc_gimmicks.md. */
#include "jc.h"
void jc_gim_spawn(void) {}
void jc_gim_pre(player_t *p) { (void)p; }
void jc_gim_frame(player_t *p) { (void)p; }
void jc_gim_draw(int pass) { (void)pass; }
int  jc_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  jc_gim_q20(int type) { (void)type; return 0; }
