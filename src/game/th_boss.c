/* Trap Hideout bosses and arena (session 21b): types 1680-1791; spawn, instance lists, the rules the encounters share, the
 * dispatch to the encounter files. Spec: build/re/spec_th_bosses.md.
 * Encounters (RE handle_frame_57..59 groups; no character test in any of them, the stage is Carol / Milla only):
 *  - TH1 (json 57): the Laser Iris (th_boss_iris.c, 13502-13531): it waits in the corridor, flies 2800 px ahead to the railing
 *    arena (x 4768-5552) and fights there with its scan laser; its defeat opens the arena (railings -> crystals).
 *  - TH3 (json 59): the Crystal Basher at the top of the start shaft, the arena outside (crowd, spotlights, goal message,
 *    enemies left, the ninja waves), then Spade (sb_spade.c).
 * Order in the frame (RE): the boss events run after the gimmicks (th_boss_frame from game.c after th_gim_frame). Nothing tests a
 * level name: every rule hangs on the placed types. Performance (SH4): instance lists are built once at spawn; a level without
 * these types returns at the first test. */
#include "th_boss.h"
#include <stdio.h>
#include <stdlib.h>

static const char *const SN[TS_N] = { "thb_iris", "thb_iris8", "thb_iris16", "thb_iris24", "thb_can", "thb_can16", "thb_wl", "thb_wl16",
    "thb_wr", "thb_wr16", "thb_las", "thb_las1", "dust",
    "thb_door", "thc_base", "thc_head", "thc_seg", "thc_spike", "thc_spike2", "thc_face" };
short thb_sa[TS_N];
thb_list_t thb_l[THB_NT];
static int on;                                       /* the level places any of these types */
#define RMAX 96
static short R[RMAX]; static int nR;                 /* runtime objects (1720-1791) */

/* ---- helpers */
obj_t *thb_slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
obj_t *thb_get(int type, int k) { const thb_list_t *l = &thb_l[type - OT_THB_FIRST]; return k >= 0 && k < l->n ? thb_slot(l->i[k]) : NULL; }
void thb_set_a(obj_t *o, int k) { if (o && k >= 0 && thb_sa[k] >= 0 && (o->an.b != &G.ens || o->an.anim != thb_sa[k])) anim_set(&o->an, &G.ens, thb_sa[k]); }
int thb_a_is(const obj_t *o, int k) { return o && k >= 0 && thb_sa[k] >= 0 && o->an.b == &G.ens && o->an.anim == thb_sa[k]; }
obj_t *thb_spawn(int type, int sk, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = x; o->y = o->oy = o->fy = y; o->layer = layer; o->born = G.frame;
    thb_set_a(o, sk);
    short i = (short)(o - G.o); int k;
    for (k = 0; k < nR && R[k] != i; k++) ;
    if (k == nR) { if (nR < RMAX) R[nR++] = i; else o->alive = 0; }
    return o;
}
static void r_compact(void) {
    int n = 0;
    for (int k = 0; k < nR; k++) { const obj_t *o = &G.o[R[k]]; if (o->alive && o->type >= OT_THB_RT && o->type <= OT_THB_END) R[n++] = R[k]; }
    nR = n;
}
int thb_g3(void) { return G.frame; }
void thb_explode_snd(void) { static const int SX[3] = { 311, 319, 320 }; snd_play_orig(SX[cross_rand(3)], 220, 128); }   /* "sound_explode" + (3 + R(3)) */
void thb_act_point(const obj_t *o, float *ax, float *ay) {
    *ax = o ? o->x : 0; *ay = o ? o->y : 0;
    if (!o || !o->an.b || !o->an.b->act) return;
    const bank_t *b = o->an.b; const frame_t *f = anim_frame(&o->an); int fi = (int)(f - b->frames);
    double dx = b->act[2 * fi] - f->hx, dy = b->act[2 * fi + 1] - f->hy, a = o->ang;
    if (o->flip) dx = -dx;
    double c = sbb_cos(a), s = sbb_sin(a);
    *ax = floorf(o->x) + (float)(int)floor(dx * c + dy * s + 0.5); *ay = floorf(o->y) + (float)(int)floor(-dx * s + dy * c + 0.5);
}

/* ---- railings / barriers (MinibossRailing_953 / MinibossBarrier_969: the engine's OT_WR_RAIL / OT_WR_BARRIER) */
static short rails[24], bars[48], marks[4]; static int nrails, nbars;
float thb_marker(int t, int x) { obj_t *o = t >= OT_CAMLIM_L && t <= OT_CAMLIM_B ? thb_slot(marks[t - OT_CAMLIM_L]) : NULL; return o ? (x ? o->x : o->y) : -1e9f; }
void thb_rails_kill(void) { for (int k = 0; k < nrails; k++) { obj_t *o = thb_slot(rails[k]); if (o && o->type == OT_WR_RAIL) o->alive = 0; } }
int thb_rails_alive(void) { int n = 0; for (int k = 0; k < nrails; k++) { obj_t *o = thb_slot(rails[k]); if (o && o->type == OT_WR_RAIL) n++; } return n; }
void thb_arena_open(void) {
    int ca = bank_anim(&G.objs, "objectcrystal_3");
    for (int k = 0; k < nrails; k++) {
        obj_t *r = thb_slot(rails[k]);
        if (!r || r->type != OT_WR_RAIL) continue;
        for (int j = 0; j < 2; j++) {
            obj_t *c = obj_alloc(); if (!c) break; memset(c, 0, sizeof *c);       /* ObjectCrystal_317 (static), move_back */
            c->type = OT_CRYSTAL; c->alive = 1; c->hp = 1; c->layer = r->layer; c->x = c->ox = r->x; c->y = c->oy = r->y + (j ? 36 : 12);
            if (ca >= 0) anim_set(&c->an, &G.objs, ca);
        }
    }
    snd_play_orig(40, 220, 128);
    for (int k = 0; k < nrails; k++) { obj_t *r = thb_slot(rails[k]); if (r && r->type == OT_WR_RAIL) r->alive = 0; }
    for (int k = 0; k < nbars; k++) { obj_t *b = thb_slot(bars[k]); if (b && b->type == OT_WR_BARRIER) b->alive = 0; }
}

/* ======================================================================== the scan laser (qualifier 0x22d98)
 * RE 13484: flag 1 cleared, then loop_l_56 sixteen times a frame. One pass (13458-13461 + the loop's own event): overlapping a
 * backdrop: A - 1; overlapping the always-solid group 0x218c0: A - 1; Milla (MV.A 4) and her PlayerReflect (the cube) on it:
 * A - 1, flag 1; free of all three: A + 1. x scale = 0.1 A on the 10-px image: the beam is A px long from its hot spot (0, 4 /
 * 0, 7) along the angle. RV i1_c: + 16 a frame while free; a beam swept into a barrier anywhere along its length shrinks 16 a
 * frame back to its first contact (f523-f531: 126 -> 7 at the barrier tops).
 * The port marches the beam (1 px steps, its cross-section every 2 px) to its first contact D with the play layer's solid
 * pixels, the solid objects listed at the fight start and the cube's box; "the beam overlaps" is then A >= D. */
static short solids[48]; static int nsolids;
static void solids_collect(float x0, float x1) {
    nsolids = 0;
    for (int i = 0; i < G.nobjs && nsolids < 48; i++) {
        const obj_t *o = &G.o[i];
        if (o->alive && obj_in_group(o->type, OBJGRP_SOLID) && o->x >= x0 && o->x <= x1) solids[nsolids++] = (short)i;
    }
}
static int solid_obj_at(float x, float y) {
    for (int k = 0; k < nsolids; k++) {
        const obj_t *o = thb_slot(solids[k]);
        if (!o || !o->an.b) continue;
        const frame_t *f = anim_frame(&o->an);
        float x0 = floorf(o->x) - f->hx, y0 = floorf(o->y) - f->hy;
        if (x >= x0 && x < x0 + f->w && y >= y0 && y < y0 + f->h) return 1;
    }
    return 0;
}
static int section_hits(const thb_laser_t *l, double d, double c, double s, double hw, int *cube) {   /* the beam's cross-section at d */
    double tx = l->x + d * c, ty = l->y - d * s;
    for (double w = -hw; w <= hw; w += 2) {
        int px = (int)floor(tx + w * s), py = (int)floor(ty + w * c);
        if (px_solid(&G.lvl, px, py) || solid_obj_at((float)px, (float)py)) return 1;
    }
    if (G.character == CHR_MILLA) {
        int x0 = (int)floor(tx - hw), y0 = (int)floor(ty - hw);
        if (player_cube_rect(x0, y0, x0 + (int)(2 * hw) + 1, y0 + (int)(2 * hw) + 1)) { *cube = 1; return 1; }
    }
    return 0;
}
void thb_laser_cast(thb_laser_t *l, int layer) {
    (void)layer;
    double c = sbb_cos(l->ang), s = sbb_sin(l->ang), hw = l->dir ? 7 : 4, D = 1e9;
    int cube = 0;
    for (double d = 0; d <= l->A + 16; d += 1) if (section_hits(l, d, c, s, hw, &cube)) { D = d; break; }
    l->flag1 = 0;
    for (int it = 0; it < 16; it++) {
        if (l->A >= D) { l->A -= 1; if (cube) l->flag1 = 1; }                   /* 13458 / 13459 / 13460 (13461) */
        if (!(l->A >= D) && !l->flag1) l->A += 1;                               /* the loop's grow event */
    }
    if (l->A < 0) l->A = 0;
}
void thb_laser_dust(const thb_laser_t *l, int layer) {
    if (l->dir != 1) return;                                                    /* 13486: firing beams (direction 1) */
    float tx = (float)(l->x + l->A * sbb_cos(l->ang)), ty = (float)(l->y - l->A * sbb_sin(l->ang));
    if (tx <= G.camx - 20 || tx >= G.camx + SCREEN_W + 20 || ty <= G.camy - 20 || ty >= G.camy + SCREEN_H + 20) return;   /* D / E in the view +- 20 (INFERRED: the tip) */
    obj_t *d = thb_spawn(OT_THB_DUST, TS_DUST, (float)(int)tx, (float)(int)ty, layer);   /* EffectsDust_437 at the action point (the tip) */
    if (d) d->ang = (float)cross_rand(360);                                     /* 13489: on a laser: a random angle */
}
int thb_laser_hits(const thb_laser_t *l, const player_t *p) {
    if (l->dir != 1 || l->A <= 0) return 0;
    int X = (int)sbb_px(p), Y = (int)sbb_py(p), y0, y1;                         /* PlayerCharacterSensor (combat.c char_rect) */
    if (player_char_sensor_small(p)) { y0 = Y - 13; y1 = Y + 14; } else { y0 = Y - 34; y1 = Y + 12; }
    int x0 = X - 7, x1 = X + 8;
    double c = sbb_cos(l->ang), s = sbb_sin(l->ang), hw = 7;
    for (double d = 0; d <= l->A; d += 3) {                                     /* the beam (14 px wide) sampled every 3 px */
        double bx = l->x + d * c, by = l->y - d * s;
        if (bx + hw >= x0 && bx - hw <= x1 && by + hw >= y0 && by - hw <= y1) return 1;
    }
    return 0;
}
void thb_laser_rule(const thb_laser_t *l, player_t *p) {
    if (!thb_laser_hits(l, p) || p->state == ST_DEAD) return;
    if (!p->hurt_pending && p->invuln <= 0) player_contact(p, 5);              /* 13490: MV.R 0, OTHERS.E <= 0: R 1, LIFE.B + 5 */
    if (p->state == ST_HURT) { p->vx = p->vy = 0; p->gsp = 0; }                 /* 13492: MV.M 8: B 0, G 0 */
    if (p->invuln > 6) p->invuln = 6;                                           /* 13493: OTHERS.E = min(E, 6) */
}

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&R, sizeof R); snap_register(&nR, sizeof nR); } }
void th_boss_spawn(void) {
    snap_once();
    memset(thb_l, 0, sizeof thb_l); nR = 0; nrails = nbars = nsolids = 0; marks[0] = marks[1] = marks[2] = marks[3] = -1;
    on = 0;
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_THB_FIRST && G.lvl.objs[j].type <= OT_THB_PLACED_LAST) on = 1;
    if (!on) return;
    for (int k = 0; k < TS_N; k++) thb_sa[k] = (short)ens_anim(SN[k]);
    /* lvobj anim: instance layer << 8 (tools/import_frame.py TH_BOSS); the k-th object of a type is its k-th placement */
    static uint16_t AN[THB_NT][THB_LMAX]; static short na[THB_NT], seen[THB_NT];
    memset(na, 0, sizeof na); memset(seen, 0, sizeof seen);
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - OT_THB_FIRST; if (t >= 0 && t < THB_NT && na[t] < THB_LMAX) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if (t == OT_WR_RAIL && nrails < 24) rails[nrails++] = (short)i;
        if (t == OT_WR_BARRIER && nbars < 48) bars[nbars++] = (short)i;
        if (t >= OT_CAMLIM_L && t <= OT_CAMLIM_B && marks[t - OT_CAMLIM_L] < 0) marks[t - OT_CAMLIM_L] = (short)i;
        if (t < OT_THB_FIRST || t > OT_THB_PLACED_LAST) continue;
        int ti = t - OT_THB_FIRST, an = seen[ti] < na[ti] ? AN[ti][seen[ti]] : (3 << 8); seen[ti]++;
        o->layer = (an >> 8) & 0xff; o->st = o->tm = o->c = o->fl = o->dir = 0; o->ang = 0; o->scale = 0; o->bd = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL;
        thb_list_t *l = &thb_l[ti]; if (l->n < THB_LMAX) l->i[l->n++] = (short)i;
    }
    thi_spawn(); thc_spawn();
    if (nrails) { float x0 = 1e9f, x1 = -1e9f; for (int k = 0; k < nrails; k++) { obj_t *r = thb_slot(rails[k]); if (r) { x0 = fminf(x0, r->x); x1 = fmaxf(x1, r->x); } } solids_collect(x0 - 64, x1 + 64); }
}
void th_boss_pre(player_t *p) { (void)p; if (!on) return; r_compact(); }
void th_boss_frame(player_t *p) {
    if (!on) return;
    for (int k = 0; k < nR; k++) { obj_t *o = &G.o[R[k]]; if (o->alive && o->type == OT_THB_DUST && (!o->an.b || o->an.done)) o->alive = 0; }   /* a puff plays once */
    thi_frame(p);
    thc_frame(p);
}
int  th_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }        /* every rule runs in th_boss_frame */
int  th_boss_q20(int type) { return type == OT_THB_IRIS || type == OT_THB_CB_FACE; }   /* RE qualifier 20 (RV XLIST q20: i2_m the iris, c1_c the face) */
int  th_boss_draw_obj(const obj_t *o, int pass) {
    if (!on) return 1;
    if (thi_draw(o, pass) || thc_draw(o, pass)) return 1;
    if (o->hidden) return 1;
    sbb_draw(o, 0xffffffff);
    return 1;
}
int   th_boss_input(uint32_t *held) { (void)held; return 0; }
int   th_boss_player_hidden(void) { return 0; }
float th_boss_hud_dy(void) { return 0.f; }
