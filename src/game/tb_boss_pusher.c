/* TB2: the wall pusher room (MinibossTrapdoor_2747, EnemyWallPusher_2748, 3 x MinibossBarrier_969 + 3 x MinibossRailing2_1204, the
 * SwitchButtonRight_2705 at the room's right end, EnemyLimiter_383 at its left end). RE handle_frame_46_events: the inline trapdoor
 * rules before 11996 and after 12007, 11996-12011; constructors (trapdoor B 128). RV build/orig_traces/tb_bosses w1_l (Lilac, TP
 * 6300 3560 at 60, right + attack every 16 frames from 200): trigger 133, the wall pushed back by the deflected hits, the switch at
 * 344 (sound 49), the robot's explosion 347 (29, 38), the railings' crystals 386 (40). spec_tb_bosses.md Part B.
 * The robot is armoured (every hit deflects: sound 30, C 5) and kept at A 30 every frame: it cannot be beaten; each deflected hit
 * (C > 3: D = 10, 12003) pushes the wall, the barriers and the robot right by floor(D / 2) a frame while D falls by 0.5 (12005 /
 * 12006), against its own walk of 2 px a frame (1 every other frame while the player pushes the wall: M 11, trapdoor flag 1). When a
 * barrier reaches the switch (12007) the switch blows the explosive blocks (the gimmick module's chain), the robot explodes and the
 * trapdoor slides open again (B + 3 a frame to 128); then the railings turn into crystals (12009) and the camera is released.
 * Trapdoor: A its closed x (the placement: RV w1_l f60), B the slide (x = A + B), C the state (0 idle, 1 the push, 2 the switch hit,
 * 3 open), flag 1 the player pushes. No character test in these events (RE). */
#include "tb_boss.h"
#include <stdio.h>
#include <stdlib.h>

/* the gimmick module's switch / explosive blocks (tb_gim.c: its TB_BOSS hooks; weak). tb_gim_button_at(o): a
 * SwitchButtonRight_2705 overlapping o is pressed (12007: hidden, flag 1) -> 1. tb_gim_blast(x, y): an
 * ObjectExplosiveBlockDestroyer_2744 at (x, y) (12007 makes three: the switch, -64 / -64 and -128 / -128). */
extern int  tb_gim_button_at(const obj_t *o) __attribute__((weak));
extern void tb_gim_blast(float x, float y) __attribute__((weak));

static struct {
    int on, C, fl1, boom_t, boomed, opened;          /* trapdoor C, flag 1; frames since the switch; the robot gone; 12009 done */
    double A, B, D;                                  /* trapdoor A / B; the robot's D (the hit push) */
    float bar0;                                      /* the first barrier's start x (the switch fallback) */
} W;

static obj_t *trap(void) { return tbb_get(OT_TBB_TRAPDOOR, 0); }
static obj_t *robot(void) { return tbb_get(OT_TBB_PUSHER, 0); }
int tb_boss_pusher_state(void) { return W.on ? W.C : -1; }

/* the barriers and railings of the room and the robot, moved together (11997 / 11999 / 12005) */
static short wall[16]; static int nwall;             /* the barriers and railings of the room (recorded at spawn) */
static void wall_shift(int dx) {
    for (int k = 0; k < nwall; k++) { obj_t *o = tbb_slot(wall[k]); if (o && (o->type == OT_WR_BARRIER || o->type == OT_RAIL2)) o->x += (float)dx; }
    obj_t *r = robot(); if (r) r->x += (float)dx;
}
static int bar_on(int (*test)(const obj_t *b)) {
    for (int k = 0; k < nwall; k++) { obj_t *o = tbb_slot(wall[k]); if (o && o->type == OT_WR_BARRIER && test(o)) return 1; }
    return 0;
}
/* image boxes of the invisible markers (the engine keeps them without a sprite): MinibossBarrier_969 img 209 16 x 48 hot spot 0, 0;
 * EnemyLimiter_383 img 698 16 x 32 hot spot 21, 18 */
static int lim_hit(const obj_t *b, const obj_t *l) {
    float bx = floorf(b->x), by = floorf(b->y), lx = floorf(l->x) - 21, ly = floorf(l->y) - 18;
    return bx < lx + 16 && lx < bx + 16 && by < ly + 32 && ly < by + 48;
}
static short lims[8]; static int nlims;
static int on_limiter(const obj_t *b) {
    for (int k = 0; k < nlims; k++) { const obj_t *o = tbb_slot(lims[k]); if (o && o->type == OT_LIMITER && lim_hit(b, o)) return 1; }
    return 0;
}
static int right_sensor_on(const obj_t *b) {
    int ox, oy, h; const signed char *x0, *x1;
    return mv_sensor_rows(SENS_RIGHTBIG, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(b, ox, oy, x0, x1, h);
}
static int on_switch(const obj_t *b) {
    if (tb_gim_button_at) return tb_gim_button_at(b);
    return b->x > W.bar0 + 56;                                                  /* fallback: the SwitchButtonRight 16 x 32 (hot spot 0, 0) placed 72 px right of the
                                                                                   barriers (TB2 6608 / 6536): overlapped once x + 16 > 6608 (RV w1_l: 6594 -> 6596 at the press) */
}

void tbw_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&W, sizeof W); snap_register(wall, sizeof wall); snap_register(&nwall, sizeof nwall); }
    memset(&W, 0, sizeof W); nwall = 0;
    obj_t *t = trap(), *r = robot();
    if (!t || !r) return;
    W.on = 1;
    tbb_set_a(t, TS_TRAP); W.A = t->x; W.B = 128;                               /* ctor B 128; A = the closed x (RV w1_l f60) */
    tbb_set_a(r, TS_WP); r->A = 30; r->fl |= TBB_ARM | F_NOFLY; r->bd = 0;
    W.bar0 = 1e9f; nlims = 0;
    for (int i = 0; i < G.nobjs; i++) {                                         /* the room's left limiters (same row band as the robot) */
        obj_t *o = &G.o[i];
        if (o->alive && o->type == OT_LIMITER && nlims < 8 && fabsf(o->y - r->y) < 96 && o->x < r->x && r->x - o->x < 640) lims[nlims++] = (short)i;
    }
    for (int i = 0; i < G.nobjs && nwall < 16; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || (o->type != OT_WR_BARRIER && o->type != OT_RAIL2)) continue;
        if (fabsf(o->x - r->x) > 128 || fabsf(o->y - r->y) > 192) continue;   /* the room's own gate (INFERRED radius: the act has one) */
        wall[nwall++] = (short)i;
        if (o->type == OT_WR_BARRIER && o->x < W.bar0) W.bar0 = o->x;
    }
}

void tbw_frame(player_t *p) {
    if (!W.on) return;
    obj_t *t = trap(), *r = robot();
    double PX = sbb_px(p), PY = sbb_py(p);
    if (t) t->x = (float)(int)(W.A + W.B);                                      /* inline: x = A + B */
    if (r) r->A = 30;                                                           /* inline: every EnemyWallPusher A 30 */
    if (t && W.C == 0 && PX >= t->x - 199 && PY > t->y + 48) { W.C = 1; level_lock_markers(); }   /* inline: C 1, CAM.M 1 */
    if (W.C == 1) { if (W.B > 0) W.B -= 8; W.fl1 = 0; }                         /* inline: the door slides shut; flag 1 off */
    if (W.C == 1) {
        int dead = p->hp <= 0 || p->state == ST_DEAD;
        if (!dead && bar_on(right_sensor_on) && player_orig_m(p) == 11) W.fl1 = 1;   /* 11996 */
        int lim = bar_on(on_limiter);
        if (!lim && !W.fl1) {                                                   /* 11997 / 11998 */
            if (r) tbb_set_a(r, TS_WP);
            wall_shift(-2);
            if (!dead && bar_on(right_sensor_on)) p->x -= 2;
        }
        if (!lim && W.fl1 && tbb_g3() % 2 == 0) {                               /* 11999 / 12000 */
            if (r) tbb_set_a(r, TS_WP);
            wall_shift(-1);
            if (!dead && bar_on(right_sensor_on)) p->x -= 1;
        }
        if (lim && r) tbb_set_a(r, W.fl1 ? TS_WP : TS_WP16);                 /* 12001 / 12002 (direction 16 / 0) */
        if (r && r->c > 3) W.D = 10;                                            /* 12003: a deflected hit */
        if (W.D > 0) wall_shift((int)floor(0.5 * W.D));                         /* 12004 / 12005 (RV w1_l 210-241: floor) */
        if (W.D > 0) W.D -= 0.5;                                                /* 12006 */
        if (bar_on(on_switch)) {                                                /* 12007 */
            snd_play_orig(49, 220, 128);
            float sx = W.bar0 + 72 + 8, sy = r ? r->y - 31 + 16 : 0;                    /* (the blasts are the gimmick module's: at the switch and up-left) */
            if (tb_gim_blast) { tb_gim_blast(sx, sy); tb_gim_blast(sx - 64, sy - 64); tb_gim_blast(sx - 128, sy - 128); }
            W.C += 1;
        }
    }
    if (W.C == 2) {
        W.B = fmin(W.B + 3, 128);                                               /* inline: the door opens */
        W.boom_t++;
        if (W.boom_t == 4 && r && !W.boomed) {                                  /* 12010: sound 29, explosions, debris, the robot gone (RV w1_l: 3 frames after 49) */
            W.boomed = 1; snd_play_orig(29, 220, 128); snd_play_orig(38, 220, 128);
            obj_effect(r->x, r->y - 20, "explosion"); obj_effect(r->x, r->y - 20, "explosion06");
            for (int k = 0; k < 4; k++) {
                obj_t *d = obj_alloc(); int a = ens_anim("debris");
                if (!d || a < 0) break;
                d->type = OT_DEBRIS; d->alive = 1; d->x = r->x; d->y = r->y - 20; anim_set(&d->an, &G.ens, a); d->an.frame = 10 - k;
                d->vx = (float)(k - 1.5) * 1.5f; d->vy = -3.f - k * 0.5f;      /* loop_spawndebris_45 (INFERRED motion, as combat.c's) */
            }
            r->alive = 0; r = NULL;
        }
        if (W.B >= 128 && !W.opened) {                                          /* 12009: crystals, the railings gone, C 3, CAM.M 2 */
            W.opened = 1; tbb_gates_open(OT_RAIL2); W.C = 3; level_release_camera(0);
        }
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBW")) {
        const obj_t *b = NULL; for (int k = 0; k < nwall && !b; k++) { const obj_t *o = tbb_slot(wall[k]); if (o && o->type == OT_WR_BARRIER) b = o; }
        printf("tbw %d px %.0f py %.0f C %d B %.0f tx %.0f fl1 %d D %.1f bx %.0f rx %.0f rc %d boom %d presses %d\n", G.frame, PX, PY, W.C, W.B, t ? t->x : -1, W.fl1, W.D,
            b ? b->x : -1, r ? r->x : -1, r ? r->c : -1, W.boomed, tb_gim_button_presses());
    }
#endif
}

int tbw_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_TBB_TRAPDOOR || o->type == OT_TBB_PUSHER) { sbb_draw(o, 0xffffffff); return 1; }
    return 0;
}
