/* Battle Glacier: the Brevon mech (Absolution). Spec: build/re/spec_bg_bosses.md section 3. (work in progress) */
#include "bg_boss.h"
int bg_boss_mech_state(void) { return -9; }
void bgm_spawn(void) {}
void bgm_frame(player_t *p) { (void)p; }
int bgm_draw(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
float bgm_hud_dy(void) { return 0.f; }
obj_t *bgm_bot_target(void) { return NULL; }
