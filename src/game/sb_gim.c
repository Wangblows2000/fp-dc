/* Sky Battalion gimmicks (session 19): see spec_sb_gimmicks.md. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void sb_gim_spawn(void) {}
void sb_gim_pre(player_t *p) { (void)p; }
void sb_gim_frame(player_t *p) { (void)p; }
void sb_gim_draw(int pass) { (void)pass; }
int  sb_gim_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 0; }
int  sb_gim_q20(int t) { (void)t; return 0; }
