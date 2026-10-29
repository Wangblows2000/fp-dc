/* stub (filled below) */
#include "fd_boss_r34.h"
void f3b_spawn(void) {}
void f3b_frame(player_t *p) { (void)p; }
int f3b_draw(const obj_t *o, int pass) { (void)o; (void)pass; return 0; }
float f3b_hud_dy(void) { return 0.f; }
#ifdef FP_HOST_ENV
int f3b_host_state(char *buf, int n) { (void)buf; (void)n; return 0; }
#endif
