/* Loose monitors (session 12, build/re/spec_milla_monitor_carry.md, static RE): the head bump / hop (A = 1, 2605-2606
 * and the unstuck loops 4730-4735 = AT 4049-4055 ...), the thrown / dropped flight (A = 2, 2613, 2614, 4736, 2751 / 2752)
 * and the flag-0 ghost rules 2615 / 2616. Milla's lift / carry / throw / drop live in src/characters/milla.inc.
 * Monitor fields: av[0] A (0 rest / held, 1 hop, 2 thrown), av[1] B (fall speed), av[2] / av[3] X / Y (throw velocity),
 * av[4] held (flag 2); fl bit 0 = flag 0 (ignored by the player's sensors and the landing tests), fl 0x200 = broken.
 * Labels: RE (spec), INFERRED (marked). */
#include "../fp.h"
#include <math.h>
#include <string.h>

int mon_type(int t) {
    return t == OT_MONITOR || t == OT_MON_RINGS || t == OT_MON_1UP || t == OT_MON_INVINC || t == OT_MON_KEYSTONE ||
           (t >= OT_MON_WOOD && t <= OT_MON_WATER) || t == OT_MON_BOMB || t == OT_MON_CRATE;
}
static int sensor_on_obj(int s, const obj_t *o) {
    int ox, oy, h; const signed char *a, *b;
    return o->an.b && mv_sensor_rows(s, &ox, &oy, &a, &b, &h) && obj_rows_hit(o, ox, oy, a, b, h);
}
/* one random monitor the sensor overlaps (cross_seed step only when there is a choice: as pick() in combat.c) */
obj_t *mon_pick(int sensor, int need_b_pos, int need_rest) {
    int idx[16], n = 0;
    for (int i = 0; i < G.nobjs && n < 16; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !mon_type(o->type) || o->av[4] || (o->fl & 0x200)) continue;
        if (need_b_pos && !(o->av[1] > 0)) continue;
        if (need_rest && o->av[0] != 0) continue;
        if (sensor_on_obj(sensor, o)) idx[n++] = i;
    }
    if (!n) return NULL;
    return &G.o[idx[n > 1 ? cross_rand(n) : (cross_rand(1), 0)]];
}
static int on_solid(const obj_t *m, int grp) {                 /* overlapping a q7 / q8 instance with flag 0 off (not itself) */
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *q = &G.o[i];
        if (q == m || !q->alive || !q->an.b || (q->fl & 1) || !obj_in_group(q->type, grp)) continue;
        if (fabsf(q->x - m->x) > 160 || fabsf(q->y - m->y) > 160) continue;
        if (obj_obj_hit(m, q)) return 1;
    }
    return 0;
}
static int on_player(const obj_t *m, const player_t *p) {     /* Player_326: 27-px disc */
    int px = (int)floor(p->x), py = (int)floor(p->y - ORIG_REF_DY);
    float cx = fmaxf(m->x, fminf((float)px, m->x + 31)), cy = fmaxf(m->y, fminf((float)py, m->y + 31));
    return (cx - px) * (cx - px) + (cy - py) * (cy - py) <= 13.5f * 13.5f;
}
void monitors_frame(player_t *p) {
    int M = player_orig_m(p);
    /* 2605: the head bump (any character), A < 2, not held -> A = 1, B = -2, flag 0 (AT only in the port: DV's monitors
     * keep their session-8 behaviour, D-051) */
    if (aqua_level() && M != 46 && p->state != ST_DEAD)
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || !mon_type(o->type) || o->av[4] || o->av[0] >= 2 || (o->fl & 0x200)) continue;
            if (sensor_on_obj(SENS_TOPBIG, o)) { o->fl |= 1; o->av[0] = 1; o->av[1] = -2; }
        }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !mon_type(o->type) || (o->fl & 0x200)) continue;
        if (o->av[0] == 1) {
            o->y = (float)(int)(o->y + o->av[1]); o->av[1] = fmin(o->av[1] + 0.2, 10);                  /* 2606 */
            if (o->av[1] < 0 && (obj_overlaps_bg(o) || on_solid(o, OBJGRP_SOLID))) {                       /* 4730 / 4731 ceiling */
                o->av[1] = 0;
                for (int k = 0; k < 10 && (obj_overlaps_bg(o) || on_solid(o, OBJGRP_SOLID)); k++) o->y += 1;
            } else if (o->av[1] >= 0 && (obj_overlaps_bg(o) || on_solid(o, OBJGRP_SOLID) || on_solid(o, OBJGRP_OBSTACLE_ALL))) {   /* 4732-4734 */
                for (int k = 0; k < 10 && (obj_overlaps_bg(o) || on_solid(o, OBJGRP_SOLID) || on_solid(o, OBJGRP_OBSTACLE_ALL) || on_player(o, p)); k++) o->y -= 1;
                o->fl &= ~1; o->av[0] = 0;                                                                 /* B kept (RE) */
            } else if (on_player(o, p)) {                                                                  /* 4735: rides on the head */
                for (int k = 0; k < 10 && on_player(o, p); k++) o->y -= 1;
            }
        }
        if (o->av[0] == 2) {
            o->x = (float)(int)(o->x + o->av[2]); o->y = (float)(int)(o->y + o->av[3]); o->av[3] = fmin(o->av[3] + 0.25, 15);   /* 2613 */
            if (o->x < -64 || o->x > G.lvl.w + 64 || o->y < -64 || o->y > G.lvl.h + 64) { o->alive = 0; continue; }   /* 2614 */
            if (aqua_level() && o->av[2] > 0)                                                              /* 4143: water jets turn it back */
                for (int k = 0; k < G.nobjs; k++) { obj_t *j = &G.o[k]; if (j->alive && (j->type == OT_WJET || j->type == OT_WJET_R) && j->an.b && obj_obj_hit(o, j)) { o->av[2] = -o->av[2]; snd_play_orig(14, 220, 128); break; } }
            if (obj_overlaps_bg(o) || on_solid(o, OBJGRP_SOLID) || on_solid(o, OBJGRP_OBSTACLE_ALL)) o->fl |= 0x200;   /* 4736 */
            for (int k = 0; k < G.nobjs && !(o->fl & 0x200); k++) {                                        /* 2751 / 2752 */
                obj_t *e = &G.o[k];
                if (!e->alive || !enemy_type(e->type) || !e->an.b || (e->fl & 0x40) || !obj_obj_hit(o, e)) continue;
                o->fl |= 0x200;
                if (e->fl & 0x20) snd_play_orig(30, 220, 128);
                else { snd_play_orig(24, 220, 128); e->c = 3; e->kx = 0.5 * o->av[2]; e->ky = 0.5 * o->av[3]; e->inv = 1; e->A -= 15; }
            }
        }
        /* 2615 / 2616: flag 0 (ghost) */
        int ghost_m = M == 5 || M == 6 || M == 30 || M == 46;
        if (!ghost_m && !(o->av[0] == 1)) o->fl &= ~1;
        if (o->av[0] == 2 || (o->av[4] && ghost_m)) o->fl |= 1;
    }
}
/* a thrown monitor that breaks leaves EffectsExplosion01 at (x + 16, y + 16) instead of the broken shell (2622) */
int mon_thrown(const obj_t *m) { return m->av[0] == 2; }
