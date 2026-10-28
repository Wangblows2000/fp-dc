/* Thermal Base stage script (session 21c, stage index 8 only): cards, routes, act events. Types 1792-1823.
 * Spec: build/re/spec_tb_flow.md. */
#include "tb.h"
void  tb_flow_spawn(void) {}
void  tb_flow_frame(player_t *p) { (void)p; }
void  tb_flow_draw(int pass) { (void)pass; }
int   tb_flow_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   tb_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 6 ? L->act : 0; }
float tb_flow_hud_dy(void) { return 0.f; }
