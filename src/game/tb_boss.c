/* Thermal Base bosses and arena (session 21c): types 1968-2047; spawn, instance lists, the rules the encounters share, the dispatch
 * to the encounter files. Spec: build/re/spec_tb_bosses.md. Encounters (RE handle_frame_45..50 groups; no character test in any of
 * their events: Lilac, Carol and Milla fight the same bosses):
 *  - TB1 (json 45): the RoboShrimp in the shaft before the act's end (tb_boss_shrimp.c, 11844-11879).
 *  - TB2 (json 46): the wall pusher room (tb_boss_pusher.c, 11996-12011 + the handler's inline trapdoor rules).
 * Order in the frame (RE): the boss events run after the gimmicks (tb_boss_frame from game.c after tb_gim_frame). Nothing tests a
 * level name: every rule hangs on the placed types. Performance (SH4): instance lists are built once at spawn; a level without
 * these types returns at the first test. */
#include "tb_boss.h"
#include <stdio.h>
#include <stdlib.h>

static const char *const SN[TS_N] = { "tbs_head", "tbs_head8", "tbs_seg", "tbs_seg8", "tbs_seg9", "tbs_tail", "tbs_tail9", "tbs_lt0", "tbs_lt1",
    "tbw_trap", "tbw_wp", "tbw_wp16",
    "explosion06", "shade_at",
    "tbx_back", "tbx_shr", "tbx_shr7", "tbx_shl", "tbx_shl7", "tbx_head", "tbx_body", "tbx_body8", "tbx_clawr", "tbx_clawl",
    "tbx_arml", "tbx_armr", "tbx_hit", "tbx_hit8", "tbx_laser", "tbx_sph", "tbx_sph2", "tbx_spl1", "tbx_spl2", "tbx_plat" };
short tbb_sa[TS_N];
tbb_list_t tbb_l[TBB_NT];
static int on;                                       /* the level places any of these types */
#define RMAX 96
static short R[RMAX]; static int nR;                 /* runtime objects (2016-2047) */

/* ---- helpers */
obj_t *tbb_slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
obj_t *tbb_get(int type, int k) { const tbb_list_t *l = &tbb_l[type - OT_TBB_SHRIMP]; return k >= 0 && k < l->n ? tbb_slot(l->i[k]) : NULL; }
void tbb_set_a(obj_t *o, int k) { if (o && k >= 0 && tbb_sa[k] >= 0 && (o->an.b != &G.ens || o->an.anim != tbb_sa[k])) anim_set(&o->an, &G.ens, tbb_sa[k]); }
int tbb_a_is(const obj_t *o, int k) { return o && k >= 0 && tbb_sa[k] >= 0 && o->an.b == &G.ens && o->an.anim == tbb_sa[k]; }
obj_t *tbb_spawn(int type, int sk, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = x; o->y = o->oy = o->fy = y; o->layer = layer; o->born = G.frame;
    tbb_set_a(o, sk);
    short i = (short)(o - G.o); int k;
    for (k = 0; k < nR && R[k] != i; k++) ;
    if (k == nR) { if (nR < RMAX) R[nR++] = i; else o->alive = 0; }
    return o;
}
static void r_compact(void) {
    int n = 0;
    for (int k = 0; k < nR; k++) { const obj_t *o = &G.o[R[k]]; if (o->alive && o->type >= OT_TBB_RT && o->type <= OT_TBB_END) R[n++] = R[k]; }
    nR = n;
}
int tbb_g3(void) { return G.frame; }
void tbb_explode_snd(void) { static const int SX[3] = { 311, 319, 320 }; snd_play_orig(SX[cross_rand(3)], 220, 128); }   /* "sound_explode" + (3 + R(3)) */
void tbb_act_point(const obj_t *o, float *ax, float *ay) {
    *ax = o ? o->x : 0; *ay = o ? o->y : 0;
    if (!o || !o->an.b || !o->an.b->act) return;
    const bank_t *b = o->an.b; const frame_t *f = anim_frame(&o->an); int fi = (int)(f - b->frames);
    double dx = b->act[2 * fi] - f->hx, dy = b->act[2 * fi + 1] - f->hy, a = o->ang;
    if (o->flip) dx = -dx;
    double c = sbb_cos(a), s = sbb_sin(a);
    *ax = floorf(o->x) + (float)(int)floor(dx * c + dy * s + 0.5); *ay = floorf(o->y) + (float)(int)floor(-dx * s + dy * c + 0.5);
}

/* ---- railings / barriers (MinibossRailing_953 = OT_WR_RAIL, MinibossRailing2_1204 = OT_RAIL2, MinibossBarrier_969 = OT_WR_BARRIER)
 * and the CameraLimitBoss markers, recorded once at spawn */
static short rails[24], bars[24], marks[4]; static int nrails, nbars;
float tbb_marker(int t, int x) { obj_t *o = t >= OT_CAMLIM_L && t <= OT_CAMLIM_B ? tbb_slot(marks[t - OT_CAMLIM_L]) : NULL; return o ? (x ? o->x : o->y) : -1e9f; }
int tbb_rails_alive(int rt) { int n = 0; for (int k = 0; k < nrails; k++) { obj_t *o = tbb_slot(rails[k]); if (o && o->type == rt) n++; } return n; }
void tbb_gates_open(int rt) {
    int ca = bank_anim(&G.objs, "objectcrystal_3");
    for (int k = 0; k < nrails; k++) {
        obj_t *r = tbb_slot(rails[k]);
        if (!r || r->type != rt) continue;
        for (int j = 0; j < 2; j++) {
            obj_t *c = obj_alloc(); if (!c) break; memset(c, 0, sizeof *c);       /* ObjectCrystal_317 (static), move_back */
            c->type = OT_CRYSTAL; c->alive = 1; c->hp = 1; c->layer = r->layer; c->x = c->ox = r->x; c->y = c->oy = r->y + (j ? 36 : 12);
            if (ca >= 0) anim_set(&c->an, &G.objs, ca);
        }
    }
    snd_play_orig(40, 220, 128);
    for (int k = 0; k < nrails; k++) { obj_t *r = tbb_slot(rails[k]); if (r && r->type == rt) r->alive = 0; }
    for (int k = 0; k < nbars; k++) { obj_t *b = tbb_slot(bars[k]); if (b && b->type == OT_WR_BARRIER) b->alive = 0; }
}

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&R, sizeof R); snap_register(&nR, sizeof nR); } }
void tb_boss_spawn(void) {
    snap_once();
    memset(tbb_l, 0, sizeof tbb_l); nR = 0; nrails = nbars = 0; marks[0] = marks[1] = marks[2] = marks[3] = -1;
    on = 0;
    for (int i = 0; i < G.nobjs; i++) { const obj_t *o = &G.o[i]; if (o->alive && o->type >= OT_CAMLIM_L && o->type <= OT_CAMLIM_B && marks[o->type - OT_CAMLIM_L] < 0) marks[o->type - OT_CAMLIM_L] = (short)i; }
    tbl_spawn();                                                                /* TB5's elevator (flow types only: before the placement test) */
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_TBB_SHRIMP && G.lvl.objs[j].type <= OT_TBB_PLACED_LAST) on = 1;
    if (!on) return;
    for (int k = 0; k < TS_N; k++) tbb_sa[k] = (short)ens_anim(SN[k]);
    /* lvobj anim: sub | instance layer << 8 (tools/import_frame.py TB_BOSS); the k-th object of a type is its k-th placement */
    static uint16_t AN[TBB_NT][TBB_LMAX]; static short na[TBB_NT], seen[TBB_NT];
    memset(na, 0, sizeof na); memset(seen, 0, sizeof seen);
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - OT_TBB_SHRIMP; if (t >= 0 && t < TBB_NT && na[t] < TBB_LMAX) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if ((t == OT_WR_RAIL || t == OT_RAIL2) && nrails < 24) rails[nrails++] = (short)i;
        if (t == OT_WR_BARRIER && nbars < 24) bars[nbars++] = (short)i;
        if (t < OT_TBB_SHRIMP || t > OT_TBB_PLACED_LAST) continue;
        int ti = t - OT_TBB_SHRIMP, an = seen[ti] < na[ti] ? AN[ti][seen[ti]] : (3 << 8); seen[ti]++;
        o->layer = (an & 0x8000) ? -1 : (an >> 8) & 0x7f; o->par = an & 0xff;
        o->st = o->tm = o->c = o->fl = o->dir = 0; o->ang = 0; o->scale = 0; o->bd = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL;
        tbb_list_t *l = &tbb_l[ti]; if (l->n < TBB_LMAX) l->i[l->n++] = (short)i;
    }
    tbs_spawn(); tbw_spawn(); tbx_spawn();
}
void tb_boss_pre(player_t *p) { (void)p; if (!on) return; r_compact(); }
void tb_boss_frame(player_t *p) {
    tbl_frame(p);
    if (!on) return;
    for (int k = 0; k < nR; k++) { obj_t *o = &G.o[R[k]]; if (o->alive && o->type == OT_TBB_FX && (!o->an.b || o->an.done)) o->alive = 0; }   /* a one-shot effect */
    tbs_frame(p);
    tbw_frame(p);
    tbx_frame(p);
}
int  tb_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }        /* every rule runs in tb_boss_frame */
int  tb_boss_q20(int type) { return type == OT_TBB_SHRIMP || type == OT_TBB_SH_SEG || type == OT_TBB_PUSHER || type == OT_TBB_SX_HIT; }   /* RE qualifier 20 */
int  tb_boss_draw_obj(const obj_t *o, int pass) {
    if (!on) return 1;
    if (tbs_draw(o, pass) || tbw_draw(o, pass) || tbx_draw(o, pass)) return 1;
    if (o->hidden) return 1;
    sbb_draw(o, 0xffffffff);
    return 1;
}
int   tb_boss_input(uint32_t *held) { (void)held; return 0; }
int   tb_boss_player_hidden(void) { return 0; }
float tb_boss_hud_dy(void) { return on ? tbx_hud_dy() : 0.f; }   /* TB6 12575 / 12581 / 12713: the HUD slides up */
