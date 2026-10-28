/* Battle Glacier: Prince Dail on the Shade Beast (BG6). Spec: build/re/spec_bg_bosses.md section 4. (work in progress) */
#include "bg_boss.h"
int bg_boss_dail_state(void) { return -1; }
void bgd_spawn(void) {}
void bgd_frame(player_t *p) { (void)p; }
int bgd_draw(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
int bgd_input(uint32_t *held) { (void)held; return 0; }
float bgd_hud_dy(void) { return 0.f; }
int bgd_player_hidden(void) { return 0; }
obj_t *bgd_bot_target(void) { return NULL; }
