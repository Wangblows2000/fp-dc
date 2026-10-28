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

/* ======================================================================== the boss laser's dust (13486 / 13489)
 * A burning beam (direction 1) whose end point D / E lies in the view +- 20: an EffectsDust_437 at its action point (the tip), turned
 * to a random angle while it touches a laser (it does at birth). */
void thb_beam_dust(const obj_t *b) {
    if (!b || !b->alive || b->dir != 1) return;
    float tx = (float)b->av[3], ty = (float)b->av[4];                            /* D / E (th_gim_beam) */
    if (tx <= G.camx - 20 || tx >= G.camx + SCREEN_W + 20 || ty <= G.camy - 20 || ty >= G.camy + SCREEN_H + 20) return;
    obj_t *d = thb_spawn(OT_THB_DUST, TS_DUST, tx, ty, b->layer);
    if (d) d->ang = (float)cross_rand(360);
}

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&R, sizeof R); snap_register(&nR, sizeof nR); } }
void th_boss_spawn(void) {
    snap_once();
    memset(thb_l, 0, sizeof thb_l); nR = 0; nrails = nbars = 0; marks[0] = marks[1] = marks[2] = marks[3] = -1;
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
