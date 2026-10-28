/* Final Dreadnought bosses A (rounds 1-2: fd1a fd1b fd2a fd2b; types 2912-2991), called by fd_boss.c's dispatch.
 * Spec: build/re/spec_fd_bosses_a.md. Labels: RE read from the decompiled events (handle_frame_67 = fd1b, 68 = fd2a, 69 = fd2b),
 * RV measured in the original (build/orig_traces/fd_bosses_a), INFERRED reasoned.
 * Encounters (each returns at once when its classes are not placed; nothing tests a level name):
 *  - fd1a (json 66): none (RE: no boss class, no CameraLimitBoss room in handle_frame_66's events).
 *  - fd1b (json 67): the push-beam chase (15387-15395, 15399 / 15400) from the alarm room to the act's end, the tunnel (15401) and
 *    the morpher (BossSyntaxValues_3467 + BossMorpher*, 15402-15561).
 *  - fd2a (json 68): the RoboSquid = TB3's class and events (12097-12128): tb_boss_squid.c runs it (TB_BOSS types; nothing here).
 *  - fd2b (json 69): the mech Serpentine (BossMSerp*, BossEnergyPunch, 15782-15983).
 * Performance: per-type instance lists built once at spawn; a level without these types returns at the first test. */
#include "fd.h"
#include "sb_boss.h"                                                            /* the shared boss helpers (sbb_sin / cos, px / py, hurt, draw) */
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ======================================================================== lists, sprites, helpers */
#define NT (OT_FDB_PLACED_LAST - OT_FDB_PB + 1)
#define LMAX 16
static struct { short n, i[LMAX]; } L[NT];
static int on;                                                                  /* the level places any of these types */
static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static obj_t *get(int type, int k) { return k >= 0 && k < L[type - OT_FDB_PB].n ? slot(L[type - OT_FDB_PB].i[k]) : NULL; }
static int count(int type) { return L[type - OT_FDB_PB].n; }
enum { FS_PB, FS_PBC, FS_N };
static const char *const FSN[FS_N] = { "fdb_pb", "fdb_pbc" };
static short fsa[FS_N];
static void set_a(obj_t *o, int k) { if (o && k >= 0 && fsa[k] >= 0 && (o->an.b != &G.ens || o->an.anim != fsa[k])) anim_set(&o->an, &G.ens, fsa[k]); }
static double MVB(const player_t *p) { return p->ground ? p->gsp : p->vx; }
static void setB(player_t *p, double b) { if (p->ground) p->gsp = (float)b; else p->vx = (float)b; }
#define PX(p) sbb_px(p)
#define PY(p) sbb_py(p)
static float marker(int t, int x) {                                             /* the first CameraLimitBoss marker of a type (the RE's object-list head) */
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == t) return x ? G.o[i].x : G.o[i].y;
    return -1e9f;
}

/* ======================================================================== fd1b: the push-beam chase (RE 15387-15401) */
/* MiniBossPushBeamCannon_3494 (12928, 1936) carries MiniBossPushBeam_3493 (12928, 1968; ctor invisible, A 1). The alarm (AlarmLight_3495,
 * a 10 x 10 orange pixel stretched over the view) starts once X > the first CameraLimitBossLeft x + 120 (15387: flag 2, shown, sound 310);
 * its A turns 5 degrees a frame and its opacity is sin(A) 200 + 180 clamped to 0-255 (15388); once A > 90 the beam appears and the cannon
 * becomes active (15389: flag 2, sound 169). Active: cannon x += 4 + A (A 0), the beam at the cannon's x (15390); Player_326 inside the
 * beam: X = max(X, beam x + 10), B = max(B, A, 5) (15391); the character sensor inside the beam: 10 damage, B + 5 (15392). Past X 14400
 * (15393 / 15394): a cannon behind the view's left edge (x + 4 <= view x + 7) jumps to view x + 8 (always in the tunnel: ML.Y 1), and
 * right held adds 0.1 to B (15395). 15399 / 15400: past X 12575 (once: the values' flag 8) the camera's top / bottom start at the view
 * and move 2 px a frame to the CameraLimitBossTop / Bottom markers (1904 / 2143). 15396-15398: past X 13400 Brevon's line (subtitle
 * paragraph 1, voices br/br_pa_03, channel 28) for 250 frames. 15401: past X 16380 (once) the act's end tunnel. */
static struct {
    int alarm, active, sub, camlock, tunnel, voice_t;
    double alA, D, F;
} P;
int fd_boss_pb_state(void) { return !count(OT_FDB_PBC) ? -1 : P.tunnel ? 3 : P.active ? 2 : P.alarm ? 1 : 0; }

static void pb_spawn(void) {
    memset(&P, 0, sizeof P);
    for (int k = 0; k < count(OT_FDB_PB); k++) { obj_t *b = get(OT_FDB_PB, k); if (b) { set_a(b, FS_PB); b->hidden = 1; b->A = 1; } }
    for (int k = 0; k < count(OT_FDB_PBC); k++) { obj_t *c = get(OT_FDB_PBC, k); if (c) { set_a(c, FS_PBC); c->A = 0; } }
}

static void pb_frame(player_t *p) {
    if (!count(OT_FDB_PBC)) return;
    double X = PX(p);
    float lx = marker(OT_CAMLIM_L, 1);
    if (!P.alarm && lx > -1e8f && X > lx + 120) { P.alarm = 1; P.alA = 0; snd_play_orig(310, 220, 128); }   /* 15387 */
    if (P.alarm) P.alA = fmod(P.alA + 5, 360);                                  /* 15388 (the opacity is drawn from it) */
    if (P.alarm && P.alA > 90 && !P.active) {                                   /* 15389 */
        P.active = 1;
        for (int k = 0; k < count(OT_FDB_PB); k++) { obj_t *b = get(OT_FDB_PB, k); if (b) b->hidden = 0; }
        snd_play_orig(169, 220, 128);
    }
    int nc = count(OT_FDB_PBC);
    if (P.active) {                                                             /* 15390 */
        for (int k = 0; k < nc; k++) { obj_t *c = get(OT_FDB_PBC, k); if (c) c->x = (float)(int)(floorf(c->x) + 4 + c->A); }
        for (int k = 0; k < count(OT_FDB_PB); k++) { obj_t *b = get(OT_FDB_PB, k), *c = get(OT_FDB_PBC, k % nc); if (b && c) b->x = floorf(c->x); }
    }
    if (P.active)
        for (int k = 0; k < count(OT_FDB_PB); k++) {
            obj_t *b = get(OT_FDB_PB, k), *c = get(OT_FDB_PBC, k % nc);
            if (!b || b->hidden) continue;
            if (sbb_p326_hit(b)) {                                              /* 15391 */
                if (p->x < b->x + 10) { p->x = floorf(b->x) + 10; player_sync_sensors(p); }
                setB(p, fmax(MVB(p), fmax(c ? c->A : 0, 5)));
            }
            if (sbb_csens_hit(p, b) && sbb_vulnerable(p)) { setB(p, MVB(p) + 5); sbb_hurt(p, 10); }   /* 15392 */
        }
    if (X > 14400) {                                                            /* 15393 / 15394 */
        for (int k = 0; k < nc; k++) {
            obj_t *c = get(OT_FDB_PBC, k);
            if (c && (P.tunnel || floorf(c->x) + 4 <= G.camx + 7)) c->x = floorf(G.camx) + 8;
        }
        if (p->held & BTN_RIGHT) setB(p, MVB(p) + 0.1);                          /* 15395 (CTRL.D: right held) */
    }
    if (!P.camlock && X > 12575) { P.camlock = 1; G.cam_nD = G.camy; G.cam_nF = G.camy + SCREEN_H; }   /* 15399 */
    if (P.camlock && !P.tunnel) {                                               /* 15400 */
        float t = marker(OT_CAMLIM_T, 0), b = marker(OT_CAMLIM_B, 0);
        if (t > -1e8f) G.cam_nD = fminf(G.cam_nD + 2, t);
        if (b > -1e8f) G.cam_nF = fmaxf(G.cam_nF - 2, b);
    }
    if (!P.sub && X > 13400) { P.sub = 1; P.voice_t = 250; voice_play("br_pa_03"); }   /* 15396 (the subtitle text: not drawn, INFERRED) */
    if (P.voice_t > 0) P.voice_t--;                                             /* 15397 / 15398 */
}

static void pb_draw(int pass) {
    if (pass != 2 || !P.alarm || P.tunnel) return;
    double v = sbb_sin(P.alA) * 200 + 180;                                      /* 15388: opacity (255 - the blend coefficient) */
    int a = v < 0 ? 0 : v > 255 ? 255 : (int)v;
    if (a > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)a << 24) | 0x983800);   /* img 11369: (152, 56, 0) */
}

/* ======================================================================== hooks */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&P, sizeof P); } }
void fdb12_spawn(void) {
    snap_once();
    memset(L, 0, sizeof L); on = 0;
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_FDB_PB && G.lvl.objs[j].type <= OT_FDB_PLACED_LAST) on = 1;
    memset(&P, 0, sizeof P);
    if (!on) return;
    for (int k = 0; k < FS_N; k++) fsa[k] = (short)ens_anim(FSN[k]);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_FDB_PB || o->type > OT_FDB_PLACED_LAST) continue;
        int t = o->type - OT_FDB_PB; if (L[t].n < LMAX) L[t].i[L[t].n++] = (short)i;
    }
    pb_spawn();
}
void  fdb12_pre(player_t *p) { (void)p; }
void  fdb12_frame(player_t *p) {
    if (!on) return;
    pb_frame(p);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_FDB")) {
        obj_t *c = get(OT_FDB_PBC, 0);
        printf("fdb %d px %.0f py %.0f pb %d cx %.0f alA %.0f D %.0f F %.0f\n", G.frame, PX(p), PY(p), fd_boss_pb_state(), c ? c->x : -1, P.alA, G.cam_nD, G.cam_nF);
    }
#endif
}
int   fdb12_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }       /* every rule runs in fdb12_frame */
int   fdb12_draw_obj(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_FDB_SXV || o->type == OT_FDB_ALARM) return 1;             /* invisible controllers (the alarm is pb_draw's overlay) */
    if (!o->hidden) sbb_draw(o, 0xffffffff);
    return 1;
}
int   fdb12_q20(int type) { (void)type; return 0; }
int   fdb12_input(uint32_t *held) { (void)held; return 0; }
int   fdb12_player_hidden(void) { return 0; }
void  fdb12_draw(int pass) { if (on) pb_draw(pass); }
float fdb12_hud_dy(void) { return 0.f; }
