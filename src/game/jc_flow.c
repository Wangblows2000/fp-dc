/* Jade Creek stage script (session 21, stage index 5 only): water, loops, cards, routes, act events.
 * Types 1280-1309. Spec: build/re/spec_jc_flow.md. */
#include "jc.h"
void  jc_flow_spawn(void) {}
void  jc_flow_frame(player_t *p) { (void)p; }
void  jc_flow_draw(int pass) { (void)pass; }
int   jc_flow_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   jc_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 5 ? L->act : 0; }
float jc_flow_hud_dy(void) { return 0.f; }
