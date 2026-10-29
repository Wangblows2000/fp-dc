/* Final Dreadnought bosses B (rounds 3-4: fd3b fd4b; types 2992-3071), called by fd_boss.c's dispatch. Spec:
 * build/re/spec_fd_bosses_b.md. This file: spawn, the per-type lists, the runtime objects, the shared helpers and the dispatch to
 * the encounter files fd_boss_r34_3b.c (FD3b) and fd_boss_r34_4b.c (FD4b). Encounters (RE handle_frame_70..73_events groups):
 *  - FD3a (json 70): none (BossSparky_3336 is placed at 256,1616 under the floor and no event of frame 70 references it: RE).
 *  - FD3b (json 71): the MinibossBulldozer (fn_boss.c, FN_BOSS classes mapped in json 71: the same Fusion object and events
 *    7404-7464); the Kingdom Stone room (fd_boss_r34_3b.c).
 *  - FD4a (json 72): none (the laser tunnel is the gimmick module's).
 *  - FD4b (json 73): the final arena (fd_boss_r34_4b.c).
 * Order in the frame (RE): the boss events run after the gimmicks (fd_boss_frame from game.c). Nothing tests a level name: every
 * rule hangs on the placed types. Performance (SH4): the lists are built once at spawn; a level without these types returns at
 * the first test; runtime objects live in a small slot table (no per-frame scan of the object pool). */
#include "fd_boss_r34.h"
#include <stdio.h>
#include <stdlib.h>

fdb_list_t fdb_l[FDB_NT];
static int on;
#define RMAX 96
static short R[RMAX]; static int nR;
int fdb_ctrl; uint32_t fdb_keys;

obj_t *fdb_slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
obj_t *fdb_get(int type, int k) { const fdb_list_t *l = &fdb_l[type - OT_FDB_STONE]; return k >= 0 && k < l->n ? fdb_slot(l->i[k]) : NULL; }
obj_t *fdb_get_sub(int type, int sub) {
    const fdb_list_t *l = &fdb_l[type - OT_FDB_STONE];
    for (int k = 0; k < l->n; k++) { obj_t *o = fdb_slot(l->i[k]); if (o && o->par == sub) return o; }
    return NULL;
}
int  fdb_row(const char *name) { return ens_anim(name); }
void fdb_set(obj_t *o, const char *row) { int a = o ? ens_anim(row) : -1; if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
void fdb_restart(obj_t *o, const char *row) { int a = o ? ens_anim(row) : -1; if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
int  fdb_is(const obj_t *o, const char *row) { int a = o ? ens_anim(row) : -1; return a >= 0 && o->an.b == &G.ens && o->an.anim == a; }

obj_t *fdb_spawn(int kind, const char *row, float x, float y, int layer) {
    int k; for (k = 0; k < nR && fdb_slot(R[k]) && G.o[R[k]].type == OT_FDB_RT; k++) ;
    if (k == RMAX) return NULL;
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = OT_FDB_RT; o->st = kind; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = x; o->y = o->oy = o->fy = y; o->layer = layer;
    o->born = G.frame;
    if (row) fdb_set(o, row);
    R[k] = (short)(o - G.o); if (k == nR) nR++;
    return o;
}
static void r_compact(void) {
    int n = 0;
    for (int k = 0; k < nR; k++) { const obj_t *o = fdb_slot(R[k]); if (o && o->type == OT_FDB_RT) R[n++] = R[k]; }
    nR = n;
}
int fdb_rt_n(void) { return nR; }
obj_t *fdb_rt(int k) { obj_t *o = k >= 0 && k < nR ? fdb_slot(R[k]) : NULL; return o && o->type == OT_FDB_RT ? o : NULL; }

int fdb_char(void) { return G.p.bike ? CHR_CAROL : G.character; }
int fdb_g3(void) { return G.frame; }
void fdb_explode_snd(void) { static const int SX[3] = { 311, 319, 320 }; snd_play_orig(SX[cross_rand(3)], 220, 128); }
void fdb_act_point(const obj_t *o, float *ax, float *ay) {
    *ax = o ? o->x : 0; *ay = o ? o->y : 0;
    if (!o || !o->an.b || !o->an.b->act) return;
    const bank_t *b = o->an.b; const frame_t *f = anim_frame(&o->an); int fi = (int)(f - b->frames);
    double dx = b->act[2 * fi] - f->hx, dy = b->act[2 * fi + 1] - f->hy, a = o->ang;
    if (o->flip) dx = -dx;
    double c = sbb_cos(a), s = sbb_sin(a);
    *ax = floorf(o->x) + (float)(int)floor(dx * c + dy * s + 0.5); *ay = floorf(o->y) + (float)(int)floor(-dx * s + dy * c + 0.5);
}
static short marks[4];
float fdb_marker(int t, int x) { obj_t *o = t >= OT_CAMLIM_L && t <= OT_CAMLIM_B ? fdb_slot(marks[t - OT_CAMLIM_L]) : NULL; return o ? (x ? o->x : o->y) : -1e9f; }
void fdb_cam_lock(float x0, float x1, float y0, float y1) { camera_boss_bounds(x0, y0, x1, y1); }
void fdb_cam_free(void) { camera_normal_bounds(); }

/* ======================================================================== spawn / dispatch */
static void snap_once(void) {
    static int reg; if (reg) return; reg = 1;
    snap_register(&R, sizeof R); snap_register(&nR, sizeof nR); snap_register(&fdb_ctrl, sizeof fdb_ctrl); snap_register(&fdb_keys, sizeof fdb_keys);
}
void fdb34_spawn(void) {
    snap_once();
    memset(fdb_l, 0, sizeof fdb_l); nR = 0; fdb_ctrl = 0; fdb_keys = 0; on = 0;
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_FDB_STONE && G.lvl.objs[j].type <= OT_FDB_LAST_USED) on = 1;
    if (!on) return;
    marks[0] = marks[1] = marks[2] = marks[3] = -1;
    /* lvobj anim: sub | instance layer << 8 (tools/import_frame.py FD_BOSS B); the k-th object of a type is its k-th placement */
    static uint16_t AN[FDB_NT][FDB_LMAX]; static short na[FDB_NT], seen[FDB_NT];
    memset(na, 0, sizeof na); memset(seen, 0, sizeof seen);
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - OT_FDB_STONE; if (t >= 0 && t < FDB_NT && na[t] < FDB_LMAX) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if (t >= OT_CAMLIM_L && t <= OT_CAMLIM_B && marks[t - OT_CAMLIM_L] < 0) marks[t - OT_CAMLIM_L] = (short)i;
        if (t < OT_FDB_STONE || t > OT_FDB_LAST_USED) continue;
        int ti = t - OT_FDB_STONE, an = seen[ti] < na[ti] ? AN[ti][seen[ti]] : (3 << 8); seen[ti]++;
        o->layer = (an >> 8) & 0x7f; o->par = an & 0xff;
        o->st = o->tm = o->c = o->fl = o->dir = 0; o->ang = 0; o->scale = 0; o->bd = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL;
        fdb_list_t *l = &fdb_l[ti]; if (l->n < FDB_LMAX) l->i[l->n++] = (short)i;
    }
    f3b_spawn(); f4b_spawn();
}
void fdb34_pre(player_t *p) { (void)p; if (!on) return; r_compact(); }
void fdb34_frame(player_t *p) {
    if (!on) return;
    for (int k = 0; k < nR; k++) { obj_t *o = fdb_rt(k); if (o && o->st == FDB_K_FX && (!o->an.b || o->an.done)) o->alive = 0; }   /* one-shot effects */
    f3b_frame(p);
    f4b_frame(p);
}
int fdb34_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }          /* every rule runs in fdb34_frame */
int fdb34_q20(int type) { return type == OT_FDB_MM_HEAD || type == OT_FDB_SUIT_HEAD || type == OT_FDB_BHIT || type == OT_FDB_MWEAK; }   /* RE on_frame_71 / 73_init: qualifier 20 */
int fdb34_draw_obj(const obj_t *o, int pass) {
    if (!on) return 1;
    if (f3b_draw(o, pass) || f4b_draw(o, pass)) return 1;
    if (o->hidden) return 1;
    sbb_draw(o, 0xffffffff);
    return 1;
}
int fdb34_input(uint32_t *held) { if (!on || !fdb_ctrl) return 0; *held = fdb_keys; return 1; }   /* CTRL.M 1: the script's buttons */
int fdb34_player_hidden(void) { return 0; }
float fdb34_hud_dy(void) { return on ? f3b_hud_dy() + f4b_hud_dy() : 0.f; }
#ifdef FP_HOST_ENV
int fdb34_host_state(char *buf, int n) { int k = f3b_host_state(buf, n); return k + f4b_host_state(buf + k, n - k); }
#endif
