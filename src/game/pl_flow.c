/* Pangu Lagoon stage script (session 23, stage index 7 only, Lilac): cards, routes, act events. Types 2048-2079.
 * Spec: build/re/spec_pl_flow.md. */
#include "pl.h"
void  pl_flow_spawn(void) {}
void  pl_flow_frame(player_t *p) { (void)p; }
void  pl_flow_draw(int pass) { (void)pass; }
int   pl_flow_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   pl_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 7 ? L->act : 0; }
float pl_flow_hud_dy(void) { return 0.f; }
