/* Jade Creek bosses (session 21): types 1430-1479 here (Hovercop, Neera: acts 2 / 3); 1480-1535 are act 5's bosses and
 * scenes in jc_boss5.c, reached through these hooks. Spec: build/re/spec_jc_bosses.md. */
#include "jc.h"
void jc_boss_spawn(void) { jc_boss5_spawn(); }
void jc_boss_pre(player_t *p) { jc_boss5_pre(p); }
void jc_boss_frame(player_t *p) { jc_boss5_frame(p); }
int  jc_boss_obj(obj_t *o, player_t *p) { if (o->type >= OT_JCB5_FIRST) return jc_boss5_obj(o, p); return 0; }
int  jc_boss_q20(int type) { return jc_boss5_q20(type); }
int  jc_boss_draw_obj(const obj_t *o, int pass) { if (o->type >= OT_JCB5_FIRST) return jc_boss5_draw_obj(o, pass); return 0; }
int  jc_boss_input(uint32_t *held) { return jc_boss5_input(held); }
int  jc_boss_player_hidden(void) { return jc_boss5_player_hidden(); }
