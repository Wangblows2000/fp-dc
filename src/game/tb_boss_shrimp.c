/* TB1: the RoboShrimp (BossRoboShrimp_2673 + 6 x Segment_2672, Tail_2675, Lightning_2674, 18 x Blocker_2691), in the shaft before
 * the act's end (camera box x 9992-10487, y 1216-1935). RE handle_frame_45_events 11844-11879 (group 0x22ba4 the fight from 11844,
 * 0x22ba5 the swim, 0x22ba3 the body; both on at the start, 11871 switches them off), constructors (head A 1, H 90; segment A 10),
 * loop_boss_explode_44; RV build/orig_traces/tb_bosses s1_l (Lilac, TP 10120 1890 at 60: trigger 61, segments A 0 at 700-705, the
 * head A 0 at 950: explosions every 7 frames from 952, 38 at 1050, the railings' crystals and 40 at 1091). spec_tb_bosses.md Part A.
 * No character test in these events (RE): the same fight for Lilac, Carol and Milla.
 * Head alterables: F / G its position, H the heading (deg; 3 px a frame along it: 11854), J the turn (+-1: H +-4 a frame; 0 straight),
 * K the frames since the last turn change, L the next change's offset (0 / 10), I the explosion count (99: the debris phase), R the
 * debris step; flag 7 (0x80) the defeat done (11871). C (the shared hit timer) is 5 while any segment lives (11845): the head cannot
 * be hit until the six segments are down, then blinks (11870) and one hit (A 1) ends the fight.
 * The body is a trail: a BossTailTracker_2126 is made at the head every frame (11859) and the trackers older than 85 go (11851);
 * segment I sits on tracker 12 (I + 1), the tail on tracker 84 (11862-11868). RV s1_l: segment I = the head's place 14 + 12 I frames
 * earlier, the tail 86 frames earlier (both the position and the angle): the port keeps the head's trail in a ring. */
#include "tb_boss.h"
#include <stdio.h>
#include <stdlib.h>

#define NH 128
static struct {
    int on, fight, swim, body, locked, opened;       /* placed; 0x22ba4; 0x22ba5; 0x22ba3; CAM.M 1 (11848); 11878 done */
    double F, G, H, J, K, L, I, R;                   /* head */
    int def;                                         /* head flag 7 (0x80): 11871 done */
    int lt_on;                                       /* the lightning's direction 1 (11857) */
    int tail_gone;                                   /* 11874 destroyed the tail */
    double railG;                                    /* MinibossRailing G (the inline rule after 11876; every railing the same) */
    int nh; short hx[NH], hy[NH]; float ha[NH];      /* the head's trail (position, angle), newest at nh - 1 */
} S;

static obj_t *head(void) { return tbb_get(OT_TBB_SHRIMP, 0); }
static obj_t *seg(int k) { return tbb_get(OT_TBB_SH_SEG, k); }
static obj_t *tail(void) { return tbb_get(OT_TBB_SH_TAIL, 0); }
static obj_t *light(void) { return tbb_get(OT_TBB_SH_LIGHT, 0); }
static int seg_I(int k) { return tbb_count(OT_TBB_SH_SEG) - 1 - k; }          /* RV s1_l: the last placed segment is I 0 (next to the head) */
int tb_boss_shrimp_state(void) { return !S.on ? -1 : S.opened ? 4 : S.def ? 3 : !S.fight ? 0 : head() && head()->c <= 0 ? 2 : 1; }

void tbs_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    obj_t *h = head();
    if (!h) return;
    S.on = 1; S.swim = S.body = 1;                                              /* on_frame_45_init: 0x22ba4 0, 0x22ba5 1, 0x22ba3 1 */
    tbb_set_a(h, TS_SH_HEAD); h->A = 1; h->fl |= F_NOFLY; h->c = 5;           /* ctor A 1, H 90; RV s1_l f55: F / G = the placement */
    S.F = h->x; S.G = h->y; S.H = 90;
    for (int k = 0; k < tbb_count(OT_TBB_SH_SEG); k++) { obj_t *s = seg(k); if (s) { tbb_set_a(s, TS_SH_SEG); s->A = 10; s->fl |= F_NOFLY; } }   /* ctor A 10 */
    tbb_set_a(tail(), TS_SH_TAIL);
    tbb_set_a(light(), TS_SH_LT0);
}

static void trail_at(int age, float *x, float *y, float *a) {               /* the head's place `age` frames ago (clamped to the oldest) */
    int n = S.nh < NH ? S.nh : NH; if (age > n - 1) age = n - 1; if (age < 0) age = 0;
    int i = (S.nh - 1 - age) % NH; if (i < 0) i += NH;
    *x = S.hx[i]; *y = S.hy[i]; *a = S.ha[i];
}
static int segs_alive(void) {
    for (int k = 0; k < tbb_count(OT_TBB_SH_SEG); k++) { const obj_t *s = seg(k); if (s && !(s->fl & TBB_DEF)) return 1; }
    return 0;
}
/* 11856: a blocker within 39 px of the head (hot spots, inclusive box) */
static int near_blocker(const obj_t *h) {
    int hx = (int)h->x, hy = (int)h->y;
    for (int k = 0; k < tbb_count(OT_TBB_SH_BLOCK); k++) {
        const obj_t *b = tbb_get(OT_TBB_SH_BLOCK, k);
        if (b && abs((int)b->x - hx) <= 39 && abs((int)b->y - hy) <= 39) return 1;
    }
    return 0;
}

void tbs_frame(player_t *p) {
    if (!S.on) return;
    obj_t *h = head();
    double PX = sbb_px(p), PY = sbb_py(p);
    float L = tbb_marker(OT_CAMLIM_L, 1), Rm = tbb_marker(OT_CAMLIM_R, 1), T = tbb_marker(OT_CAMLIM_T, 0), B = tbb_marker(OT_CAMLIM_B, 0);
    if (h && segs_alive()) h->c = 5;                                            /* 11845 (the shared 2762 counts it down once they are gone) */
    /* 11844: the fight (once): L < X < R, B - 100 <= Y < B */
    if (!S.fight && PX > L && PX < Rm && PY >= B - 100 && PY < B) S.fight = 1;
    /* 11848: the camera box (CAM.M 1) once the player is inside: X > L, T + 240 < Y < B (INFERRED: CAM.U, the 2-frame repeat guard) */
    if (!S.locked && PX > L && PY > T + 240 && PY < B) { S.locked = 1; level_lock_markers(); }
    if (!S.fight) return;
    obj_t *t = tail(), *lt = light();
    if (h && S.swim) {
        if (S.J == 1) S.H += 4;                                                 /* 11852 */
        if (S.J == -1) S.H -= 4;                                                /* 11853 */
        S.F += 3 * sbb_cos(S.H); S.G += -3 * sbb_sin(S.H);                      /* 11854 */
        S.K += 1;                                                               /* inline: flag 4 off, K + 1 */
        int busy = near_blocker(h) || obj_overlaps_bg(h);                       /* 11856 (flag 4) / overlaps_background */
        if (S.K > 20 && S.J == 0 && !busy) {                                    /* 11857: start turning, the lightning sparks */
            S.J = 1; S.K = 0; S.lt_on = 1;
        }
        if (S.K > 40 && S.J != 0 && !busy) {                                    /* 11858: the other way, K = L, L = (L + 10) mod 20 */
            S.J = -S.J; S.K = S.L; S.L = fmod(S.L + 10, 20);
        }
    }
    if (h && S.body) {
        h->x = (float)(int)S.F; h->y = (float)(int)S.G; h->ang = (float)sbb_nrm(S.H);   /* 11860 */
        int i = S.nh % NH; S.hx[i] = (short)h->x; S.hy[i] = (short)h->y; S.ha[i] = h->ang; S.nh++;   /* 11859 (the tracker) */
        for (int k = 0; k < tbb_count(OT_TBB_SH_SEG); k++) {                    /* 11861 / 11862-11867 */
            obj_t *s = seg(k); if (!s) continue;
            int I = seg_I(k); float x, y, a; trail_at(14 + 12 * I, &x, &y, &a);
            s->x = x; s->y = y; s->ang = a;
            if (!(s->fl & TBB_DEF) && s->an.b) s->an.frame = ((tbb_g3() / 20) + I) % 5;   /* force_frame((g3 / 20 + I) mod 5) */
        }
        if (t) { float x, y, a; trail_at(86, &x, &y, &a); t->x = x; t->y = y; t->ang = a; }   /* 11868 (tracker 84) */
        if (lt && t) { float ax, ay; tbb_act_point(t, &ax, &ay); lt->x = ax; lt->y = ay; lt->ang = t->ang; }   /* 11861: at the tail's action point */
        if (lt) tbb_set_a(lt, S.lt_on ? TS_SH_LT1 : TS_SH_LT0);
    }
    /* 11869: a segment down: its broken frame, sound 45 (the shared 23 is stopped: INFERRED kept), explosion02 */
    for (int k = 0; k < tbb_count(OT_TBB_SH_SEG); k++) {
        obj_t *s = seg(k);
        if (!s || !(s->fl & TBB_DEF) || (s->st & 2)) continue;
        s->st |= 2; tbb_set_a(s, TS_SH_SEG8); snd_play_orig(45, 220, 128); obj_effect(s->x, s->y, "explosion02");
    }
    if (h && !S.def && h->c <= 0 && h->an.b) h->an.frame = (tbb_g3() / 15) % 2;   /* 11870: the head blinks once it can be hit */
    if (h && (h->fl & TBB_DEF) && !S.def) {                                    /* 11871: the head down */
        S.def = 1; tbb_set_a(h, TS_SH_HEAD8); S.swim = 0;
        for (int k = 0; k < tbb_count(OT_TBB_SH_SEG); k++) tbb_set_a(seg(k), TS_SH_SEG9);
        tbb_set_a(t, TS_SH_TAIL9);
        if (lt) { lt->alive = 0; lt = NULL; }
        S.body = 0;
    }
    if (h && S.def && S.I < 15 && tbb_g3() % 7 == 0) {                         /* 11872: an explosion every 7 frames, 15 of them */
        obj_effect(h->x - 20 + cross_rand(41), h->y - 30 + cross_rand(61), "explosion06");
        S.I += 1; tbb_explode_snd();
    }
    if (h && S.def && S.I == 15) {                                              /* 11874: sound 38, shade, loop_boss_explode_44, the tail gone */
        snd_play_orig(38, 220, 128); obj_effect(h->x, h->y, "shade_at"); obj_effect(h->x, h->y, "explosion");
        S.I = 99;
        if (t) { t->alive = 0; t = NULL; } S.tail_gone = 1;
    }
    if (h && S.I == 99) {
        if (S.R < 45) {                                                         /* 11875: an explosion06 along the debris curve */
            double r = S.R, w = 8 + 40 * sbb_sin(2 * r);
            obj_effect((float)(int)(w * sbb_cos(132 * r) + h->x), (float)(int)(h->y - 80 * sbb_sin(5 * r)), "explosion06");
        }
        S.R += 1;                                                               /* 11876: the head and the segments hidden */
        h->hidden = 1;
        for (int k = 0; k < tbb_count(OT_TBB_SH_SEG); k++) { obj_t *s = seg(k); if (s) s->hidden = 1; }
    }
    if (S.tail_gone && tbb_rails_alive(OT_WR_RAIL)) S.railG += 1;               /* inline: the tail count 0 -> every railing G + 1 */
    if (S.railG > 40 && !S.opened) { S.opened = 1; tbb_gates_open(OT_WR_RAIL); }   /* 11878 */
    /* 11879: the lightning (direction 1) on the player: R 1, LIFE.B + 10 (not with the metal shield OTHERS.B 3, E <= 0) */
    if (lt && S.lt_on && p->shield != 3 && sbb_p326_hit(lt)) sbb_hurt(p, 10);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_TBS") && h) {
        printf("tbs %d px %.0f py %.0f x %.0f y %.0f ang %.0f H %.0f J %.0f K %.0f L %.0f C %d A %.0f I %.0f R %.0f def %d segs %d st %d lt %d rail %.0f\n",
            G.frame, PX, PY, h->x, h->y, h->ang, S.H, S.J, S.K, S.L, h->c, h->A, S.I, S.R, S.def, segs_alive(), tb_boss_shrimp_state(), lt ? S.lt_on : -1, S.railG);
    }
#endif
}

int tbs_draw(const obj_t *o, int pass) {
    (void)pass;
    switch (o->type) {
    case OT_TBB_SHRIMP: case OT_TBB_SH_SEG: case OT_TBB_SH_TAIL: case OT_TBB_SH_LIGHT:
        sbb_draw(o, 0xffffffff); return 1;
    case OT_TBB_SH_BLOCK: return 1;
    }
    return 0;
}
