/* Thermal Base gimmicks (session 21c): types 1888-1967. Spec: build/re/spec_tb_gimmicks.md. */
#include "tb.h"
void tb_gim_spawn(void) {}
void tb_gim_pre(player_t *p) { (void)p; }
void tb_gim_frame(player_t *p) { (void)p; }
void tb_gim_draw(int pass) { (void)pass; }
int  tb_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  tb_gim_q20(int type) { (void)type; return 0; }
