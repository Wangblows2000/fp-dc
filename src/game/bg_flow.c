/* Battle Glacier stage script (session 24, stage index 9, json 60-65 = bg1-bg6): cards, routes, act events. Types 2304-2335.
 * Spec: build/re/spec_bg_flow.md. */
#include "bg.h"
void  bg_flow_spawn(void) {}
void  bg_flow_frame(player_t *p) { (void)p; }
void  bg_flow_draw(int pass) { (void)pass; }
int   bg_flow_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int   bg_flow_act(void) { const level_def_t *L = level_cur(); return L && L->stage == 9 ? L->act : 0; }
float bg_flow_hud_dy(void) { return 0.f; }
