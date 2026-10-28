/* Trap Hideout stage script (session 21b, stage index 8 only): cards, routes, act events. Types 1536-1559.
 * Spec: build/re/spec_th_flow.md. */
#include "th.h"
void  th_flow_spawn(void) {}
void  th_flow_frame(player_t *p) { (void)p; }
void  th_flow_draw(int pass) { (void)pass; }
int   th_flow_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   th_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 8 ? L->act : 0; }
float th_flow_hud_dy(void) { return 0.f; }
