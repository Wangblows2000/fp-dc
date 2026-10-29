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
void tbb_explode_snd(void);
#define F_ARMOURED 0x20                                                         /* enemy flag 5: armoured (combat.c) */
#define F_DEFEATED 0x40                                                         /* enemy flag 6: defeated (combat.c) */                                                     /* tb_boss.c: sound_explode 311 / 319 / 320 */
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ======================================================================== lists, sprites, helpers */
#define NT (OT_FDB_LIST_LAST - OT_FDB_PB + 1)
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
 * 15388 sets its alpha byte from A (255 - clamp(200 cos A + 180), 75 at A 90 / 270: FD3a's 16184 rule, fd_alarm_alpha), then A + 5 mod
 * 360; drawn additively over the view (ctor shader 0x20) on json layer 5 (objects_draw pass 3); once A > 90 the beam appears and the cannon
 * becomes active (15389: flag 2, sound 169). Active: cannon x += 4 + A (A 0), the beam at the cannon's x (15390); Player_326 inside the
 * beam: X = max(X, beam x + 10), B = max(B, A, 5) (15391); the character sensor inside the beam: 10 damage, B + 5 (15392). Past X 14400
 * (15393 / 15394): a cannon behind the view's left edge (x + 4 <= view x + 7) jumps to view x + 8 (always in the tunnel: ML.Y 1), and
 * right held adds 0.1 to B (15395). 15399 / 15400: past X 12575 (once: the values' flag 8) the camera's top / bottom start at the view
 * and move 2 px a frame to the CameraLimitBossTop / Bottom markers (1904 / 2143). 15396-15398: past X 13400 Brevon's line (subtitle
 * paragraph 1, voices br/br_pa_03, channel 28) for 250 frames. 15401: past X 16380 (once) the act's end tunnel. */
static struct {
    int alarm, active, sub, camlock, tunnel, voice_t, alpha;
    double alA, D, F;
} P;
static void m_start(player_t *p);
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
    if (P.alarm) { P.alpha = fd_alarm_alpha(P.alA); P.alA = fmod(P.alA + 5, 360); }   /* 15388: the alpha byte from A, then A + 5 */
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
    if (!P.camlock && X > 12575) {                                              /* 15399: the tunnel strips shown, piece0 hidden */
        P.camlock = 1; G.cam_nD = G.camy; G.cam_nF = G.camy + SCREEN_H;
        par_set_hidden(3463, 0); par_set_hidden(3464, 0); par_set_hidden(3390, 1);
    }
    if (P.camlock && !P.tunnel) {                                               /* 15400 */
        float t = marker(OT_CAMLIM_T, 0), b = marker(OT_CAMLIM_B, 0);
        if (t > -1e8f) G.cam_nD = fminf(G.cam_nD + 2, t);
        if (b > -1e8f) G.cam_nF = fmaxf(G.cam_nF - 2, b);
    }
    if (!P.tunnel && X > 16380) m_start(p);                                     /* 15401 */
    if (!P.sub && X > 13400) { P.sub = 1; P.voice_t = 250; voice_play("br_pa_03"); }   /* 15396 (the subtitle text: not drawn, INFERRED) */
    if (P.voice_t > 0) P.voice_t--;                                             /* 15397 / 15398 */
}

static void pb_draw(int pass) {
    if (pass != 3 || !P.alarm || P.tunnel) return;
    obj_draw_view_tint(0x983800, P.alpha);                                      /* img 11369: (152, 56, 0), additive (ctor shader 0x20) */
}

/* ======================================================================== fd1b: the tunnel and the morpher (RE 15327-15347, 15401-15561) */
/* The tunnel (15401): X = 200 (the act's left end, y unchanged: both corridors stand on y 2112), the view fixed at x 0 (CAM.M 3, A 0,
 * E = window width), ML.Y 1, the cannon at the left edge (15403: x = sin(G3) 16 - 8), the player kept at X <= view x + 402, B <= 10
 * (15406). The original runs a treadmill (the floor tiles scroll 15 px a frame, B -10 in the air / +10 on landing: 15328-15345): the port
 * keeps a static arena and scrolls the floor tiles' drawing (INFERRED: the same picture, no speed hack).
 * The morpher: BossSyntaxValues_3467 holds the form weights A-G, the phase J, its timer P, the rig position K / L, the turn M; its 15
 * parts (BossMorpher*_3469..3486, off-map templates) are created here. Form order strings by the head's D (15465-15467): 4 "234",
 * 3 "5234", 2 "752", 1 "62754" (RE, the static strings). Phase J n > 1 grows one weight 0.05 a frame (J 7: G 0.025) for its time
 * (J 2 B 180, 3 A 180, 4 C 240, 5 D 200, 6 E 380, 7 G 285 frames: 15475-15487), J 1 shrinks them all 0.05 a frame for 20 frames
 * (15471, sound 382 at P 1: 15472) and takes the next digit (15473 / 15474). The head (A 80, D 4) takes the shared hits (Q20):
 * flag 6 -> A 50, C 100, D - 1, J 1 (15488, sound 47); D 0 -> the defeat (15491). */
enum { MP_ORB2, MP_ORB1, MP_WH4, MP_WH3, MP_WH2, MP_WH1, MP_TUBE1, MP_TUBE2, MP_GRILL2, MP_GRILL1, MP_PELVIS, MP_BODY, MP_NODE2, MP_NODE1, MP_HEAD, MP_N };
static const char *const MPROW[MP_N] = { "fdm_orb", "fdm_orb", "fdm_wh4", "fdm_wh3", "fdm_wh2", "fdm_wh1", "fdm_tube1", "fdm_tube2",
    "fdm_grill2", "fdm_grill1", "fdm_pelvis", "fdm_body", "fdm_node2", "fdm_node1", "fdm_head" };
enum { MR_TRAIN0 = MP_N, MR_TRAIN8, MR_TRAIN24, MR_EBALL, MR_LASER, MR_SHOCK, MR_SHOCK16, MR_SHOCKE, MR_GLOW, MR_HEAD1, MR_BODY1, MR_SPARK, MR_N };
static const char *const MRROW[MR_N - MP_N] = { "fdm_train0", "fdm_train8", "fdm_train24", "fdm_eball", "fdm_laser", "fdm_shock",
    "fdm_shock16", "fdm_shocke", "fdm_glow", "fdm_head1", "fdm_body1", "fdm_spark" };
static short mra[MR_N];
#define NSHOT 24
enum { SH_TRAIN = 1, SH_EBALL, SH_LASER, SH_SHOCK, SH_GLOW };
static struct {
    int on, J, R, hD, hC, def, endt, results, slen, gE, dsub;
    double w[7], K, Lr, M, Q, P, bA, bB, bC, vx, gF;
    char str[8];
    short part[MP_N], shot[NSHOT];
} M;
static double sxv_y(void) { obj_t *v = get(OT_FDB_SXV, 0); return v ? v->y : 2112; }
static obj_t *mpart(int k) { obj_t *o = slot(M.part[k]); return o && o->type == OT_FDB_MPART + k ? o : NULL; }
int fd_boss_morpher_state(void) { return !M.on ? -1 : M.def ? 100 + (M.results ? 1 : 0) : M.J; }
int fd_boss_morpher_hp(void) { obj_t *h = M.on ? mpart(MP_HEAD) : NULL; return h ? (int)h->A + 50 * (M.hD - 1) : -1; }

static obj_t *mspawn(int type, int row, float x, float y) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = x; o->y = o->oy = o->fy = y; o->layer = 3; o->born = G.frame;
    if (row >= 0 && mra[row] >= 0) anim_set(&o->an, &G.ens, mra[row]);
    return o;
}
static void mset(obj_t *o, int row) { if (o && row >= 0 && mra[row] >= 0 && (o->an.b != &G.ens || o->an.anim != mra[row])) anim_set(&o->an, &G.ens, mra[row]); }
static obj_t *shot_new(int kind, int row, float x, float y) {
    int k; for (k = 0; k < NSHOT && slot(M.shot[k]); k++) ;
    if (k == NSHOT) return NULL;
    obj_t *o = mspawn(OT_FDB_TRAIN + (kind == SH_EBALL ? 1 : kind == SH_LASER ? 2 : kind == SH_SHOCK ? 3 : kind == SH_GLOW ? 4 : 0), row, x, y);
    if (o) { o->par = kind; M.shot[k] = (short)(o - G.o); }
    return o;
}

/* Part offsets from (K, L), each form's offset times its weight (15412-15423 add them one form an event; every weight 0: every part on
 * (K, L), 15409 / 15410: the ball between two forms). A, B, C are constant (least-squares fit of the RV trace m1_l, residual <= 5 px;
 * the small sin(20 M) 1.1 bobs are left out, INFERRED); B is A upside down (the ceiling walker). D (the bouncer), E (the cannon) and
 * G (the rolling ball) move with M: see m_shape (RE constants of 15417 / 15419 / 15421, terms fitted on m2_l, residual <= 4 px).
 * Order: MP_* (ORB2 ORB1 WH4 WH3 WH2 WH1 TUBE1 TUBE2 GRILL2 GRILL1 PELVIS BODY NODE2 NODE1 HEAD). */
static const signed char MOFF[3][MP_N][2] = {
    { { 32, 8 }, { -32, 8 }, { 36, 44 }, { 12, 44 }, { -12, 44 }, { -36, 44 }, { -16, -16 }, { 16, -16 }, { 24, 32 }, { -24, 32 }, { 0, 0 }, { 0, 0 }, { 52, 0 }, { -52, 0 }, { 0, 0 } },   /* A */
    { { -32, -8 }, { 32, -8 }, { -36, -44 }, { -12, -44 }, { 12, -44 }, { 36, -44 }, { 16, 16 }, { -16, 16 }, { -24, -32 }, { 24, -32 }, { 0, 0 }, { 0, 0 }, { -52, 0 }, { 52, 0 }, { 0, 0 } },   /* B */
    { { 32, 64 }, { -24, 64 }, { 100, 84 }, { 68, 84 }, { 31, 63 }, { -25, 63 }, { 64, 32 }, { 80, 56 }, { 96, 72 }, { 72, 72 }, { -16, 32 }, { 0, 0 }, { 28, 24 }, { 3, 57 }, { 0, 0 } } };   /* C */
/* D, E, G offsets of part k at the turn M (degrees) */
static void m_shape(int k, double m, double *d, double *e, double *g) {
    double s2 = sbb_sin(2 * m), s = sbb_sin(m), c = sbb_cos(m), s4 = sbb_sin(4 * m), c4 = sbb_cos(4 * m);
    d[0] = d[1] = e[0] = e[1] = g[0] = g[1] = 0;
    switch (k) {                                                                /* D: the legs spring with sin 2M (15417) */
    case MP_WH4: d[1] = 64 + 12 * s2; break;
    case MP_WH3: d[1] = 44; break;
    case MP_GRILL2: case MP_GRILL1: d[0] = k == MP_GRILL2 ? 8 : -8; d[1] = 56 + 8 * s2; break;
    case MP_PELVIS: d[1] = 16; break;
    case MP_NODE2: case MP_NODE1: d[0] = k == MP_NODE2 ? 4 : -4; d[1] = 80 + 16 * s2; break;
    }
    switch (k) {                                                                /* E: the cannon pointing left, recoiling with sin M (15419) */
    case MP_ORB2: case MP_ORB1: e[0] = -64 - 10 * c; e[1] = k == MP_ORB2 ? 56 : -56; break;
    case MP_WH4: case MP_WH3: e[0] = -48 + 10 * sbb_sin(m - 60); e[1] = k == MP_WH4 ? 52 : -52; break;
    case MP_WH2: case MP_WH1: e[0] = -32 + 10 * sbb_sin(m - 30); e[1] = k == MP_WH2 ? 40 : -40; break;
    case MP_TUBE1: case MP_TUBE2: e[0] = 14 * s; e[1] = k == MP_TUBE1 ? 40 : -32; break;
    case MP_GRILL2: case MP_GRILL1: e[0] = -32 + 10 * s; e[1] = k == MP_GRILL2 ? 7 : -7; break;
    case MP_PELVIS: e[0] = 24 + 10 * s; break;
    case MP_BODY: case MP_HEAD: e[0] = 10 * s; break;
    case MP_NODE2: case MP_NODE1: e[0] = -32 + 10 * s; e[1] = k == MP_NODE2 ? 20 : -20; break;
    }
    switch (k) {                                                                /* G: the shell turning 4M (15421) */
    case MP_ORB2: g[0] = -16 * c4; g[1] = -16 * s4; break;
    case MP_ORB1: g[0] = 16 * c4; g[1] = 16 * s4; break;
    case MP_TUBE1: g[0] = -32 * s4; g[1] = 32 * c4; break;
    case MP_TUBE2: g[0] = 32 * s4; g[1] = -32 * c4; break;
    case MP_GRILL2: g[0] = 32 * c4; g[1] = 32 * s4; break;
    case MP_GRILL1: g[0] = -32 * c4; g[1] = -32 * s4; break;
    }
}

static void m_layout(void) {
    for (int k = 0; k < MP_N; k++) {
        obj_t *o = mpart(k);
        if (!o) continue;
        double x = M.K, y = M.Lr, d[2], e[2], g[2];
        for (int f = 0; f < 3; f++) { x += MOFF[f][k][0] * M.w[f]; y += MOFF[f][k][1] * M.w[f]; }
        m_shape(k, M.M, d, e, g);
        x += d[0] * M.w[3] + e[0] * M.w[4] + g[0] * M.w[6]; y += d[1] * M.w[3] + e[1] * M.w[4] + g[1] * M.w[6];
        o->x = (float)(int)x; o->y = (float)(int)y;
        /* 15422: the shell's pieces turn with it (angle -(4 M + a) G; the per-piece a INFERRED) */
        static const short GA[MP_N] = { 0, 0, -1, -1, -1, -1, 135, 315, 225, 45, -1, -1, -1, -1, -1 };
        if (GA[k] >= 0) o->ang = (float)fmod(360.0 - fmod((4 * M.M + GA[k]) * M.w[6], 360.0), 360.0);
        /* 15416 / 15418 / 15420 / 15426: the orbs show image 1 in the C, E and G forms */
        if ((k == MP_ORB1 || k == MP_ORB2) && o->an.b) {
            int want = (M.w[2] >= 1 || M.w[4] >= 1 || M.w[6] >= 1) ? 1 : 0, n = o->an.b->anims[o->an.anim].count;
            o->an.frame = want < n ? want : n - 1; o->an.acc = 0;
        }
    }
}

static void m_start(player_t *p) {                                              /* 15401 */
    P.tunnel = 1;
    p->x = 200; p->gsp = fminf(p->gsp, 10); player_sync_sensors(p);
    G.cam_nC = 0; G.cam_nE = SCREEN_W;
    camera_reset(); G.cam_nC = 0; G.cam_nE = SCREEN_W; G.cam_nD = marker(OT_CAMLIM_T, 0); G.cam_nF = marker(OT_CAMLIM_B, 0);
    camera_update();
    for (int k = 0; k < count(OT_FDB_PBC); k++) { obj_t *c = get(OT_FDB_PBC, k); if (c) { c->x = 0; c->A = -4; } }
    par_set_drift(3463, -8); par_set_drift(3464, -12); par_set_drift(3394, -5); par_set_drift(3391, -3); par_set_drift(3395, -1); par_set_drift(3393, -3);   /* 15401: G */
    par_set_hidden(3463, 0); par_set_hidden(3464, 0); par_set_hidden(3390, 1);
    music_play(level_music_slot(4) ? level_music_slot(4) : "bgm/m_boss5.adp");   /* stop_music + play_music(global string 4): see the spec (RV) */
    memset(&M, 0, sizeof M);
    M.on = 1; M.J = 0; M.hD = 4; strcpy(M.str, "234"); M.slen = 3; M.K = 0;
    { obj_t *v = get(OT_FDB_SXV, 0); M.vx = v ? v->x : 512; }
    for (int k = 0; k < NSHOT; k++) M.shot[k] = -1;
    for (int k = 0; k < MP_N; k++) {
        obj_t *o = mspawn(OT_FDB_MPART + k, k, 0, (float)sxv_y());
        M.part[k] = o ? (short)(o - G.o) : -1;
    }
    obj_t *h = mpart(MP_HEAD);
    if (h) { h->A = 80; h->fl |= F_NOFLY; }                                     /* ctor: A 80, D 4 (RE create_bossmorpherhead_3486) */
}

static int ov(const obj_t *a, const obj_t *b) {                                 /* a's image box on b's mask (check_overlap, INFERRED box) */
    if (!a->an.b || !b->an.b) return 0;
    const frame_t *f = anim_frame(&a->an);
    int x0 = (int)a->x - f->hx, y0 = (int)a->y - f->hy;
    return obj_rect_hit(b, x0, y0, x0 + f->w, y0 + f->h);
}
static void m_hurt_on(player_t *p, obj_t *o, int dmg, int snd) {
    if (o && !o->hidden && sbb_csens_hit(p, o) && sbb_vulnerable(p)) { if (snd) snd_play_orig(snd, 220, 128); sbb_hurt(p, dmg); }
}

static void m_shots(player_t *p) {
    obj_t *h = mpart(MP_HEAD);
    for (int k = 0; k < NSHOT; k++) {
        obj_t *o = slot(M.shot[k]);
        if (!o || o->type < OT_FDB_TRAIN || o->type > OT_FDB_FX) { M.shot[k] = -1; continue; }
        o->av[1] += 1; o->av[2] += 1;                                           /* B / C counters (15497) */
        switch (o->par) {
        case SH_TRAIN:   /* 15498-15509 (RV m1_l 1221-1300): A 0 right 12 a frame at head height to the values' x - 111, A 1 a 10-frame stop,
                          * A 2 down 8 a frame, A 3 a 10-frame stop on the floor, A 5 back left 12 a frame (A 4 / A 7: reflected right / up) */
            if (o->av[0] == 0) { o->x += 12; if (o->x >= M.vx - 111) { mset(o, MR_TRAIN8); o->av[0] = 1; o->av[1] = 0; } }   /* 15499 / 15504 */
            else if (o->av[0] == 1 && o->av[1] > 10) { mset(o, MR_TRAIN24); o->av[0] = 2; o->av[1] = 0; }   /* 15500 */
            else if (o->av[0] == 2) { o->y += 8; if (o->av[1] > 8) { mset(o, MR_TRAIN8); o->av[0] = 3; o->av[1] = 0; } }   /* 15501 / 15502 */
            else if (o->av[0] == 3 && o->av[1] > 10) { mset(o, MR_TRAIN0); o->av[0] = 5; o->av[1] = 0; }   /* 15503 */
            else if (o->av[0] == 4) o->x += 12;
            else if (o->av[0] == 5) o->x -= 12;                                 /* 15505 */
            else if (o->av[0] == 7) o->y -= 8;                                  /* 15506 */
            if (o->av[2] > 3) m_hurt_on(p, o, 10, 0);                           /* 15509: C > 3 */
            if (o->av[0] > 3 && o->av[1] > 80) o->alive = 0;                    /* 15508 */
            if (o->alive && o->av[0] == 5 && h && !h->hidden && M.hC <= 0 && !(h->fl & (F_ARMOURED | F_DEFEATED)) && ov(o, h)) {
                h->A -= 5; M.hC = 5; h->c = 5; snd_play_orig(24, 220, 128);     /* 15507: A - 5, C 5, flash */
            }
            break;
        case SH_EBALL:                                                          /* 15510-15516 */
            if (o->av[0] < 10) { o->av[0] += 0.5; o->scale = (float)(o->av[0] * 0.1); }
            else {
                o->x += (o->fl & 2) ? 4 : -4;
                if (h) o->y = (float)(int)(h->y + (2 * o->av[3] - 1) * sbb_sin(5 * o->av[1]) * 60);
                if (!(o->fl & 2)) m_hurt_on(p, o, 10, 371);
                if ((o->fl & 2) && h && M.hC <= 0 && !(h->fl & (F_ARMOURED | F_DEFEATED)) && ov(o, h)) {
                    h->A -= 5; M.hC = 5; h->c = 5; snd_play_orig(24, 220, 128); o->alive = 0;   /* 15516 */
                }
                if (o->av[1] > 300) o->alive = 0;                               /* 15514 */
            }
            break;
        case SH_LASER: o->y += (float)o->av[4]; m_hurt_on(p, o, 10, 0); if (!obj_near_view(o, 32) || obj_overlaps_bg(o)) o->alive = 0; break;   /* EnemyTrooperLaser_2530: 15 a frame (RV) */
        case SH_SHOCK: o->x += (float)o->av[4]; m_hurt_on(p, o, 10, 0); if (!obj_near_view(o, 64)) o->alive = 0; break;
        case SH_GLOW: if (o->av[1] > 30) o->alive = 0; break;
        }
    }
}

/* Milla's cube (PlayerReflect_463) reflects a ball (flag 1, sound 219: col_9_66) or the train beam (A 0 -> 5, A 2 -> 7, sound 219:
 * col_23_66); no event lets Lilac's or Carol's attacks reflect them (RE): they hit the head directly (Q20). */
static void m_reflect(player_t *p) {
    for (int k = 0; k < NSHOT; k++) {
        obj_t *o = slot(M.shot[k]);
        if (!o || (o->par != SH_EBALL && o->par != SH_TRAIN)) continue;
        if (!player_cube_hits(o)) continue;                                     /* PlayerReflect_463: Milla's cube (RE) */
        if (o->par == SH_EBALL && !(o->fl & 2) && o->av[0] >= 10) { o->fl |= 2; snd_play_orig(219, 220, 128); }
        if (o->par == SH_TRAIN && (o->av[0] == 0 || o->av[0] == 2) && !(o->fl & 2)) { o->fl |= 2; o->av[0] = o->av[0] == 0 ? 5 : 7; snd_play_orig(219, 220, 128); }   /* col_23_66 */
    }
}

static void m_attacks(player_t *p) {
    obj_t *h = mpart(MP_HEAD), *n1 = mpart(MP_NODE1), *n2 = mpart(MP_NODE2), *o1 = mpart(MP_ORB1), *o2 = mpart(MP_ORB2);
    double *w = M.w;
    /* A / B forms (15427-15437; RV m1_l 788-902): A walks left 3 a frame to the view x + 80, B (the ceiling) right 3 a frame to 55 px
     * before the player (RV: stops at X - 55, INFERRED rule); once the weight is 1 the body's B counts: 10 the charge glows at the nodes
     * (sound 191), from 39 every 4 frames a laser from node 1 / node 2 in turn (20 lasers, sound 192), 15 a frame up from the node - 48
     * (A) / down from the node + 48 (B); 120 restarts. */
    if (w[0] >= 1 && M.K > G.camx + 80) M.K -= 3;
    if (w[1] >= 1 && M.K < sbb_px(p) - 55) M.K += 3;
    if (w[0] < 1 && w[1] < 1) M.bB = 0;
    else {
        int up = w[0] >= 1;
        M.bB += 1;
        if (M.bB == 10) {
            if (n1) shot_new(SH_GLOW, MR_GLOW, n1->x + (up ? -4 : 4), n1->y + (up ? -16 : 16));
            if (n2) shot_new(SH_GLOW, MR_GLOW, n2->x + (up ? 4 : -4), n2->y + (up ? -16 : 16));
            snd_play_orig(191, 220, 128);
        }
        if (M.bB >= 39 && M.bB < 119 && (int)(M.bB - 39) % 4 == 0) {
            obj_t *n = ((int)(M.bB - 39) / 4) % 2 ? n2 : n1;
            if (n) {
                obj_t *l = shot_new(SH_LASER, MR_LASER, n->x + (up ? -4 : 4), n->y + (up ? -48 : 48));
                if (l) { l->av[4] = up ? -15 : 15; l->ang = 90; }
            }
            snd_play_orig(192, 220, 128);
        }
        if (M.bB >= 120) M.bB = 0;
    }
    /* C form: the train beam every 80 frames from the head (15441 / 15443 / 15444: body A 30, reset past 80) */
    if (w[2] < 1) M.bA = 0;
    else { M.bA += 1; if (M.bA == 30 && h) { shot_new(SH_TRAIN, MR_TRAIN24, h->x + 16, h->y); snd_play_orig(384, 220, 128); } if (M.bA > 80) M.bA = 0; }
    /* D form: bounce toward the player 6 px a frame (15436 / 15437 / 15449 / 15450), shockwaves on landing (15445-15447) */
    if (w[3] >= 1) {                                                            /* (RV m2_l: turns past view x + 395 / under + 30) */
        if (M.Q == 0) M.Q = 1;
        if (M.K > G.camx + 395) M.Q = -1; else if (M.K < G.camx + 30) M.Q = 1;
        M.K += M.Q * 6;
        double s = sbb_sin(M.M), s2 = sbb_sin(M.M - 5);
        if (fabs(s) < fabs(s2) && fabs(sbb_sin(M.M + 5)) > fabs(s) && h) {    /* the bounce's lowest point (|sin M| minimum) */
            for (int d = -1; d <= 1; d += 2) { obj_t *sw = shot_new(SH_SHOCK, d < 0 ? MR_SHOCK16 : MR_SHOCK, (float)M.K, (float)sxv_y()); if (sw) sw->av[4] = 4 * d; }
            snd_play_orig(132, 220, 128); snd_play_orig(385, 220, 128); obj_effect((float)M.K, (float)sxv_y() - 16, "explosion");
        }
    } else M.Q = 0;
    /* E form: eases to view x + 440 (15453; RV m2_l: K 440, the cannon on the right edge pointing left); the body C counts: 40 and
     * 100 elec balls from the orbs (15454-15457, sound 60 every 60 frames in m2_l), past 120 0 */
    if (w[4] >= 1) {
        M.K = (19 * M.K + G.camx + 440) * 0.05;
        M.bC += 1;
        if (M.bC == 40 || M.bC == 100) {
            for (int s = 0; s < 2; s++) {
                obj_t *ob = s ? o1 : o2; if (!ob) continue;
                obj_t *b = shot_new(SH_EBALL, MR_EBALL, ob->x, ob->y);
                if (b) { b->scale = 0.05f; b->av[3] = s; }
            }
            snd_play_orig(60, 220, 128);
        }
        if (M.bC > 120) M.bC = 0;
    } else M.bC = 0;
    /* G form (15460-15464; RV m2_l 2465-2700): the ball rolls round the room's corners, the body's E the corner (0 bottom right, 1 top
     * right, 2 top left, 3 bottom left), its F counting: past 50 the next corner (sound 383); K eases 1/20 to view x + 396 / 376 / 40 /
     * 60, L 1/10 to the values' y - 54 (floor) / - 120 (ceiling); every 4 frames on the floor a ground spark (15424 / 15425). */
    if (w[6] >= 1) {
        static const short GX[4] = { 396, 376, 40, 60 }, GY[4] = { -54, -120, -120, -54 };
        M.gF += 1;
        if (M.gF > 50) { M.gE = (M.gE + 1) & 3; M.gF = 0; snd_play_orig(383, 220, 128); }
        M.K = (19 * M.K + G.camx + GX[M.gE]) * 0.05;
        M.Lr = (9 * M.Lr + sxv_y() + GY[M.gE]) * 0.1;
        if ((G.frame & 3) == 0 && M.Lr >= sxv_y() - 57) {
            obj_t *sp = shot_new(SH_GLOW, MR_SPARK, (float)(int)M.K, (float)sxv_y());
            if (sp) sp->ang = (float)(170 - cross_rand(11));
        }
    } else { M.gE = 0; M.gF = 0; }
    /* 15517 / 15518: the rolling ball's orbs (G 1, J < 12) hurt on contact (10) */
    if (w[6] >= 1 && M.J < 12) { m_hurt_on(p, o1, 10, 0); m_hurt_on(p, o2, 10, 0); }
}

static void m_frame(player_t *p) {
    if (!M.on) return;
    obj_t *h = mpart(MP_HEAD), *c = get(OT_FDB_PBC, 0), *b = get(OT_FDB_PB, 0);
    int g3 = G.frame;
    G.cam_nC = 0; G.cam_nE = SCREEN_W;
    if (c) { c->x = (float)(int)(sbb_sin(g3) * 16 - 8); if (b) b->x = c->x; }   /* 15403 */
    if (!M.def) {                                                               /* 15406 (group 0x22f56) */
        if (p->x > G.camx + 402) { p->x = G.camx + 402; player_sync_sensors(p); }
        if (MVB(p) > 10) setB(p, 10);
    } else if (p->x > G.camx + 426) { p->x = G.camx + 426; player_sync_sensors(p); }
    if (!M.results) fd_gim_scroll_floor(1, 15, 0); else fd_gim_scroll_floor(0, 0, 0);   /* 15341-15344: the treadmill (FD_GIM) while the fight runs */
    M.M = fmod(M.M + 5, 360);                                                   /* 15407 */
    if (!M.def) M.P += 1;                                                       /* 15468 (the defeat counts its own: 15519 / 15522) */
    double *w = M.w;
    if (!M.def) {
        if (M.J == 0) { if (M.P > 60) M.K = (9 * M.K + G.camx + 80) * 0.1; if (M.P > 90) { M.P = 0; M.J = 1; } }   /* 15469 / 15470 */
        else if (M.J == 1) {
            for (int f = 0; f < 7; f++) w[f] = fmax(w[f] - 0.05, 0);            /* 15471 */
            if (M.P == 1) snd_play_orig(382, 220, 128);                         /* 15472 */
            if (M.P > 20) { if (M.R >= M.slen) M.R = 0; M.J = M.str[M.R] - '0'; M.R++; M.P = 0; }   /* 15473 / 15474 */
        } else {
            static const signed char WI[8] = { -1, -1, 1, 0, 2, 3, 4, 6 };      /* J -> weight (2 B, 3 A, 4 C, 5 D, 6 E, 7 G) */
            static const short DUR[8] = { 0, 0, 180, 180, 240, 200, 380, 285 };
            int f = M.J >= 2 && M.J <= 7 ? WI[M.J] : -1;
            if (f >= 0) { w[f] = fmin(w[f] + (M.J == 7 ? 0.025 : 0.05), 1); if (M.P > DUR[M.J]) { M.P = 0; M.J = 1; } }
        }
        int allz = 1; for (int f = 0; f < 7; f++) if (w[f] != 0) allz = 0;
        if (allz) M.M = 0;                                                      /* 15411 */
        if (w[5] < 1 && w[6] < 1)                                               /* 15408 */
            M.Lr = sxv_y() - 56 - 64 * w[1] - 40 * w[2] - (fabs(sbb_sin(M.M)) * 64 + 32) * w[3] - 32 * w[4];
        if (w[2] >= 1 && M.K > G.camx + 50) M.K -= 3;                           /* 15439 */
        m_attacks(p);
        if (h) {                                                                /* the head: 15488-15491 */
            if (M.hC > 0) M.hC--;
            if (M.hC > 0 || M.J == 0) h->fl |= F_ARMOURED; else h->fl &= ~F_ARMOURED;
            if (h->fl & F_DEFEATED) {
                M.hD -= 1;
                if (M.hD <= 0) {                                                /* 15491 */
                    M.def = 1; M.J = 12; M.P = 0; M.dsub = 0; h->fl |= F_ARMOURED;
                    music_stop(); snd_play_orig(47, 220, 128);                  /* (RV m2_l 4400: 47 here too) */
                } else {                                                        /* 15488 */
                    h->fl &= ~F_DEFEATED; h->A = 50; M.hC = 100; M.J = 1; M.P = 0; M.R = 0; snd_play_orig(47, 220, 128);
                    strcpy(M.str, M.hD == 3 ? "5234" : M.hD == 2 ? "752" : "62754"); M.slen = (int)strlen(M.str);   /* 15465-15467 */
                }
            }
        }
    } else {                                                                    /* the defeat (group 0x22f51, 15519-15541) */
        /* RV m2_l 4400-4900: P counts every 3 frames to 60 (15519's 2-frame step + 15522), then every frame. Every 6 frames under 60
         * the weights step 0.05 (A B E F down, C D G up to 1: 15520) and an explosion06 with its sound (98 / 99 / 311) at the head,
         * explosion02 at every step; 60-180 an explosion every 4 frames (15524-15526). 180: sound 38 and every part slides left,
         * 0.2 px a frame faster each frame, into the beam at the left edge (RV; INFERRED: the parts stop on the cannon x + 16).
         * 300: sound 76 and the white fade (15535); 340: the cannon, beam and shots removed (15538); past 500 the results (15541). */
        int step = 1;
        if (M.P < 60) { step = (++M.dsub % 3) == 0; }
        if (M.P < 60 && (M.dsub % 6) == 0) {
            for (int f = 0; f < 7; f++) w[f] = (f == 2 || f == 3 || f == 6) ? fmin(w[f] + 0.05, 1) : fmax(w[f] - 0.05, 0);
            if (h) { obj_effect(h->x - 40 + cross_rand(81), h->y - 40 + cross_rand(81), "explosion06"); tbb_explode_snd(); }
        }
        if (step) M.P += 1;
        if (h && M.P < 60 && step) obj_effect(h->x - 40 + cross_rand(81), h->y - 40 + cross_rand(81), "explosion02");   /* 15521 / 15522 */
        if (h && M.P >= 60 && M.P < 180 && g3 % 4 == 0) { obj_effect(h->x - 40 + cross_rand(81), h->y - 40 + cross_rand(81), "explosion06"); tbb_explode_snd(); }
        if (M.P < 60 && M.K < G.camx + 120) M.K += 2;                           /* 15523 */
        if (w[5] < 1 && w[6] < 1) M.Lr = sxv_y() - 56 - 64 * w[1] - 40 * w[2] - (fabs(sbb_sin(M.M)) * 64 + 32) * w[3] - 32 * w[4];
        if (M.P == 180 && step) {                                               /* 15527-15529 */
            snd_play_orig(38, 220, 128);
            for (int k = 0; k < MP_N; k++) { obj_t *o = mpart(k); if (o) o->av[5] = 0; }
        }
        if (M.P > 180) {
            float cx = c ? c->x + 16 : (float)G.camx + 16;
            for (int k = 0; k < MP_N; k++) {
                obj_t *o = mpart(k); if (!o) continue;
                o->av[5] += 0.2; o->x -= (float)o->av[5];
                if (o->x < cx) { o->x = cx; o->av[5] = 0; }
            }
        }
        if (M.P == 300 && step) snd_play_orig(76, 220, 128);                    /* 15535 (+ the white fade) */
        if (M.P == 340 && step) {                                               /* 15538: the arena items removed */
            if (c) c->alive = 0;
            if (b) b->alive = 0;
            for (int k = 0; k < NSHOT; k++) { obj_t *o = slot(M.shot[k]); if (o) o->alive = 0; }
        }
        if (M.P > 500 && !M.results) { M.results = 1; results_begin_boss(game_timer()); }   /* 15541: Sign.B 179 -> the results */
    }
    if (M.P <= 180 || !M.def) m_layout();
    m_shots(p);
    if (!M.def) m_reflect(p);
}

static int m_draw_obj(const obj_t *o, int pass) {
    (void)pass;
    if (o->type >= OT_FDB_MPART && o->type <= OT_FDB_FX) {
        if (o->hidden) return 1;
        if (o->type == OT_FDB_MPART + MP_HEAD && M.hC > 5 && (M.hC / 2) % 2) return 1;   /* 15489: the blink */
        sbb_draw(o, 0xffffffff);
        return 1;
    }
    return 0;
}

static void m_draw(int pass) {
    if (!M.on || pass != 2 || !M.def || M.P < 300) return;
    int a = M.P < 340 ? (int)((M.P - 300) * 255 / 40) : M.P < 420 ? 255 : 255 - (int)((M.P - 420) * 4);   /* 15535-15539: the white flash (INFERRED ramps) */
    if (a > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)(a > 255 ? 255 : a) << 24) | 0xffffff);
}

/* ======================================================================== fd2b: the mech Serpentine (RE 15782-15983, RV s2_l) */
/* The transformation (NPCSerpentine A stages, its G counting: 15812-15830). 15812: X > the NPC's x - 200 and Y < its y + 50: A 1, the
 * stage music fades out (fadeout / unqueue / lock_music); 15814: A 1: the player walks right 2 px a frame to the NPC's x - 80 and the
 * GUI slides up (loop_gui_shiftup_44); 15815: A 1, G > 100: the player stops (loop_stop_player), A 2; 15817: A 2, G > 30: A 3; 15818:
 * A 3, G = 60: the NPC's animation 17, sound 373; 15819: G > 64: A 4; 15820-15824: A 4: from G 70 sound 374 every 20 frames and the
 * transform bubble grows on the NPC (RV s2_l: 17 x 374, 326-646); 15830 (RV 657): sound 67, play_music slot 4 (m_boss6, RE 15688), the
 * flare (BossMSerpFlare_3606) rises 16 px a frame from the NPC, then falls down the shaft at the NPC's x - 150 (RV: 7622, 16 px a
 * frame); 15831 (RV 720): the flare breaks the FDPlatforms under the NPC (sound 354, explosions) and the camera's bottom markers
 * there go: the player (at the NPC's x - 6, RV 7766) falls down the shaft (RV: 20 px a frame) to the arena (y 4992, x 7008-8160).
 * The fight (head group 0x22fc1, 15849-15936): BossMSerpHead A 40 HP, H 5 lives (15913: A <= 0: H - 1, A 40, C 120 (the blink, no
 * hits), sounds 47 + 378; 15917: H 1 left: A + 20 and m_boss6b, RE 15689); the head's D is the state, E its timer (15870):
 *   D 0 idle (pose J) -> D 1 (39 frames, pose K, facing the player) -> D 2 the dash (30 frames, sound 375, the target H + 15 F a frame,
 *   the energy punch's burst in front: 15878) -> D 3 (19 frames) -> D 0;  D 4 (80 frames, pose M: every 6 frames two pairs of burst
 *   shots from the right claw at +-11.25 and +-45 degrees, 20 px a frame, sound 376 (15891-15894) / every 8 frames a fireball (sound
 *   377, 15895 / 15896: y = A - D sin B, B + 6, D + 5 to 100, x + 5 F) - the two alternate (15873's flag 1) -> D 5 (19 frames) -> D 0;
 *   D 6 (19 frames, pose N) -> D 7 the jump (46 frames, sound 147: 10 frames crouched, then 36 frames x + 10 F on a 171 px arc) ->
 *   D 8 (19 frames, pose O; the landing flare runs along the floor 15 px a frame the other way) -> D 0.
 *   The idle waits (RV): 41 frames before a dash (31 after a jump), 51 before a jump, 101 before the shots; the order (RV s2_l): a jump
 *   first, then two dashes between the specials, jump and shots alternating (RE: a random pick, GLOBALS.C: INFERRED order).
 * Rig (RV fit on s2_l, residual median <= 2 px, the tail's swing left out): every part at the torso + F * x / + y of the pose weights
 * J-O (MSOFF); the torso bobs y = I + 20 sin 2D (D + 4 a frame, + 12 dashing), its x follows H by half the gap.
 * Contact (15941 / 15942 / 15946 / 15966): the torso, head, left arm, the burst, shots, fireballs and the floor flare hurt (10).
 * Milla's cube (PlayerReflect) sends the shots and fireballs back (15947 / 15948, sound 219).
 * The defeat (H 0: 15918-15936): D 60 (music stopped, poses cleared), explosions on the body for 230 frames (sound 309), the
 * transform bubble at E 98, the white fade from E 128; D 61: E 120 the parts go and the NPC stands at the torso; E > 220: the results
 * (CTRL.M 1, Sign.B 179: results_begin_boss). */
enum { MS_TORSO, MS_HEAD, MS_NECK, MS_TAIL, MS_RARM, MS_RCLAW, MS_BSH, MS_FSH, MS_LARM, MS_LCLAW, MS_SIGHT, MS_NP };
static const char *const MSROW[MS_NP] = { "fdms_torso", "fdms_head", "fdms_neck", "fdms_tail", "fdms_rarm", "fdms_rclaw", "fdms_bsh", "fdms_fsh",
    "fdms_larm", "fdms_lclaw", "fdms_sight" };
enum { MSR_PUNCH, MSR_BURST, MSR_FBALL, MSR_FLARE, MSR_FLARE10, MSR_WARN, MSR_BUBBLE, MSR_NPC, MSR_NPCT, MSR_N };
static const char *const MSRROW[MSR_N] = { "fdms_punch", "fdms_burst", "fdms_fball", "fdms_flare", "fdms_flare10", "fdms_warn", "fdms_bubble",
    "fdms_npc", "fdms_npct" };
static short msa[MS_NP][2], msr[MSR_N][2];                                       /* rows: [dir 0, dir 16] */
/* pose offsets (J idle, K crouch, L dash, M shoot, N jump crouch, O jump): x (times F) then y; tail x 7 instances */
static const signed char MSO[MS_NP][2][6] = {
    { { 0 } },
    { { 16, 3, 33, 4, 34, -2 }, { -41, -53, -23, -40, -25, -40 } },                   /* head */
    { { 8, 1, 17, 8, 17, -1 }, { -24, -32, -4, -24, -4, -24 } },                      /* neck */
    { { 0 } },                                                                        /* tail: MSTAIL */
    { { 54, 76, -5, 76, 22, 30 }, { 16, 21, 16, -4, 11, 24 } },                       /* right arm */
    { { 62, 88, -47, 74, 39, 38 }, { 16, 21, 16, -4, 10, 24 } },                      /* right claw */
    { { 0 }, { -1, 0, 0, 0, 0, 0 } }, { { 0 }, { -1, 0, 0, 0, 0, 0 } },              /* shoulders */
    { { -39, -89, 60, -40, -60, -40 }, { 15, 21, 16, 16, 22, 8 } },                   /* left arm */
    { { -46, -103, 72, -48, -39, -48 }, { 15, 21, 16, 16, 22, 8 } },                  /* left claw */
    { { 0 } } };
static const signed char MSTAIL[7][2][6] = {
    { { 6, 31, -91, 4, -18, 14 }, { 59, 68, 42, 57, 17, 106 } }, { { 5, 27, -80, 4, -30, 12 }, { 59, 68, 42, 57, 19, 95 } },
    { { 4, 23, -68, 3, -41, 12 }, { 59, 68, 41, 57, 21, 83 } }, { { 2, 18, -55, 4, -46, 11 }, { 58, 68, 40, 57, 23, 71 } },
    { { 0, 14, -43, 3, -42, 10 }, { 51, 68, 38, 56, 28, 57 } }, { { -1, 9, -30, 3, -30, 7 }, { 42, 55, 29, 41, 30, 41 } },
    { { -1, 6, -18, 1, -13, 4 }, { 25, 33, 17, 25, 22, 25 } } };
enum { MSS_BURST = 1, MSS_FBALL, MSS_FLARE, MSS_BUBBLE, MSS_FX };
#define MS_NSHOT 48
static struct {
    int on, npcA, F, D, def, results, variant, nspec, dashes, mlast, landed, gone, cam, par, hud;
    double G, E, H, I, Dang, w[6], tx, ty;
    short shot[MS_NSHOT];
} S;
static obj_t *ms(int k, int i) { return get(OT_FDB_MS + k, i); }
static obj_t *npc(void) { return get(OT_FDB_NPCS, 0); }
int fd_boss_mserp_state(void) { return !S.on ? -1 : S.def ? (S.results ? 162 : 59 + S.def) : S.npcA < 7 ? 100 + S.npcA : S.D; }   /* D, 100 + the intro's A, 60 / 61 the defeat, 162 the results */
int fd_boss_mserp_hp(void) { obj_t *h = ms(MS_HEAD, 0); return S.on && h ? (int)h->A + 40 * ((int)h->av[7] - 1) : -1; }
static void ms_row(obj_t *o, short *r) { int a = r[S.F < 0 && r[1] >= 0 ? 1 : 0]; if (o && a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static obj_t *ms_shot(int kind, int row, float x, float y) {
    int k; for (k = 0; k < MS_NSHOT && slot(S.shot[k]); k++) ;
    if (k == MS_NSHOT) return NULL;
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = OT_FDB_SHOT; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = x; o->y = o->oy = o->fy = y; o->layer = 3; o->born = G.frame; o->par = kind;
    if (row >= 0) ms_row(o, msr[row]);
    S.shot[k] = (short)(o - G.o);
    return o;
}

static void ms_spawn(void) {
    memset(&S, 0, sizeof S);
    for (int k = 0; k < MS_NSHOT; k++) S.shot[k] = -1;
    if (!npc()) return;
    for (int k = 0; k < MS_NP; k++) { msa[k][0] = (short)ens_anim(MSROW[k]); char nm[24]; snprintf(nm, sizeof nm, "%s16", MSROW[k]); msa[k][1] = (short)ens_anim(nm); }
    for (int k = 0; k < MSR_N; k++) { msr[k][0] = (short)ens_anim(MSRROW[k]); char nm[24]; snprintf(nm, sizeof nm, "%s16", MSRROW[k]); msr[k][1] = (short)ens_anim(nm); }
    if (msa[MS_TORSO][0] < 0) return;
    for (int k = 0; k < MS_NP; k++) {                                           /* the exported templates (x > 8384) go: created below */
        int t = OT_FDB_MS + k - OT_FDB_PB;
        for (int i = 0; i < L[t].n; i++) { obj_t *o = slot(L[t].i[i]); if (o) o->alive = 0; }
        L[t].n = 0;
    }
    /* the parts: off-map templates in the json (8433-8615, 4880-5071; the importer drops them): created here at the torso's place,
     * in a drawing order behind to front (INFERRED): the coils, back shoulder, left arm + claw, torso, neck, head, front shoulder,
     * right arm + claw */
    static const signed char ORDER[] = { MS_TAIL, MS_TAIL, MS_TAIL, MS_TAIL, MS_TAIL, MS_TAIL, MS_TAIL, MS_BSH, MS_LARM, MS_LCLAW, MS_TORSO,
                                         MS_NECK, MS_HEAD, MS_FSH, MS_RARM, MS_RCLAW };
    for (unsigned j = 0; j < sizeof ORDER; j++) {
        int k = ORDER[j], t = OT_FDB_MS + k - OT_FDB_PB;
        obj_t *o = obj_alloc();
        if (!o || L[t].n >= LMAX) break;
        memset(o, 0, sizeof *o);
        o->type = OT_FDB_MS + k; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = 8544; o->y = o->oy = o->fy = 4920; o->layer = 3; o->born = G.frame;
        L[t].i[L[t].n++] = (short)(o - G.o);
    }
    obj_t *t = ms(MS_TORSO, 0);
    if (!t) return;
    S.on = 1; S.F = -1; S.w[0] = 1; S.I = 4920; S.H = t->x;
    obj_t *h = ms(MS_HEAD, 0);
    if (h) { h->A = 40; h->av[7] = 5; h->fl |= F_NOFLY | F_ARMOURED; }              /* ctor: A 40, H 5 (RV s2_l) */
    obj_t *n = npc(); S.F = -1; ms_row(n, msr[MSR_NPC]);
}

static void ms_layout(void) {
    obj_t *t = ms(MS_TORSO, 0);
    if (!t) return;
    for (int k = 0; k < MS_NP; k++)
        for (int i = 0; i < count(OT_FDB_MS + k); i++) {
            obj_t *o = ms(k, i); if (!o) continue;
            if (S.gone) { o->hidden = 1; continue; }
            ms_row(o, msa[k]);
            if (k == MS_TORSO || k == MS_SIGHT) continue;
            const signed char (*m)[6] = k == MS_TAIL ? MSTAIL[i < 7 ? i : 6] : MSO[k];
            double dx = 0, dy = 0;
            for (int j = 0; j < 6; j++) { dx += m[0][j] * S.w[j]; dy += m[1][j] * S.w[j]; }
            o->x = (float)(int)(t->x + S.F * dx); o->y = (float)(int)(t->y + dy);
        }
}

static void ms_pose(int tgt, double r) {                                         /* the pose weights move to one pose (0.05 a frame, RV) */
    for (int j = 0; j < 6; j++) S.w[j] = j == tgt ? fmin(S.w[j] + r, 1) : fmax(S.w[j] - 0.05, 0);
}
static void ms_face(player_t *p) { obj_t *t = ms(MS_TORSO, 0); if (t) S.F = PX(p) >= t->x ? 1 : -1; }
static void ms_setD(int d) { S.D = d; S.E = 0; }

static void ms_hurt(player_t *p, obj_t *o) { if (o && !o->hidden && sbb_csens_hit(p, o) && sbb_vulnerable(p)) sbb_hurt(p, 10); }

static void ms_intro(player_t *p) {
    obj_t *n = npc(), *t = ms(MS_TORSO, 0);
    if (!n || !t) return;
    double X = PX(p), Y = PY(p);
    S.G += 1;
    switch (S.npcA) {
    case 0:
        if (X > n->x - 200 && Y < n->y + 50) { S.npcA = 1; S.G = 0; music_stop(); }   /* 15812 */
        break;
    case 1: if (S.G > 100) { S.npcA = 2; S.G = 0; } break;                        /* 15814 / 15815 */
    case 2: if (S.G > 30) { S.npcA = 3; S.G = 0; } break;                         /* 15817 */
    case 3:
        if (S.G == 60) { ms_row(n, msr[MSR_NPCT]); snd_play_orig(373, 220, 128); }   /* 15818 */
        if (S.G > 64) { S.npcA = 4; S.G = 0; }                                  /* 15819 */
        break;
    case 4:
        if (S.G >= 70 && (int)(S.G - 70) % 20 == 0 && S.G <= 390) snd_play_orig(374, 220, 128);   /* 15823 (RV: 17 times) */
        if (S.G == 70) { obj_t *b = ms_shot(MSS_BUBBLE, MSR_BUBBLE, n->x, n->y - 16); if (b) b->scale = 0.1f; }   /* 15824 */
        if (S.G > 400) {                                                        /* 15830 (RV 657) */
            S.npcA = 5; S.G = 0;
            snd_play_orig(67, 220, 128);
            const char *m = level_music_slot(4); music_play(m ? m : "bgm/m_boss6.adp");   /* RE 15688: m_boss6 */
            for (int k = 0; k < MS_NSHOT; k++) { obj_t *o = slot(S.shot[k]); if (o && o->par == MSS_BUBBLE) o->alive = 0; }
            n->hidden = 1;
            obj_t *f = ms_shot(MSS_FLARE, MSR_FLARE, n->x, n->y - 16); if (f) { f->vy = -16; f->av[0] = 0; }
        }
        break;
    case 5:                                                                     /* the flare (RV 658-720) */
        if (S.G > 60) {                                                         /* 15831: the platforms under the NPC break */
            S.npcA = 6; S.G = 0;
            snd_play_orig(354, 220, 128);
            for (int i = 0; i < G.nobjs; i++) {
                obj_t *o = &G.o[i];
                if (!o->alive) continue;
                if (o->type == OT_FDG_FDPLAT && fabsf(o->x - n->x) < 450 && o->y < n->y + 100) { obj_effect(o->x, o->y, "explosion01"); o->alive = 0; }
                if (o->type == OT_CAMLIM_B && o->y < n->y + 200) o->alive = 0;
            }
            p->x = n->x - 6; p->vx = p->gsp = 0; player_sync_sensors(p);      /* (RV 750: X 7766) */
        }
        break;
    case 6:                                                                     /* the fall (RV 20 px a frame) to the arena */
        if (Y < 4600) { if (p->vy < 20) p->vy = 20; }
        if (Y > 4500 && X > 7000 && !S.cam) { S.cam = 1; camera_boss_bounds(6944, 4764, 8224, 5004); }   /* (INFERRED: the arena's view) */
        if (p->ground && Y > 4800 && !S.landed) { S.landed = 1; S.G = 0; S.H = 7327; }   /* the mech walks in to x 7327 (RV) */
        if (S.landed) {
            t->x = (float)fmax(S.H, t->x - 15);
            if (t->x <= S.H + 1) { S.npcA = 7; S.G = 0; ms_setD(0); S.dashes = 1; obj_t *h = ms(MS_HEAD, 0); if (h) h->fl &= ~F_ARMOURED; }
        }
        break;
    }
}

static void ms_fire(obj_t *c) {
    if (!c) return;
    if (S.variant == 0) {                                                       /* burst shots (15891-15894, sound 376) */
        if (S.E >= 20 && (int)(S.E - 20) % 6 == 0) {
            static const double ang[4] = { 11.25, -11.25, 45, -45 };
            for (int k = 0; k < 4; k++) {
                obj_t *b = ms_shot(MSS_BURST, MSR_BURST, c->x + 30 * S.F, c->y + (k & 1 ? 8 : -8));
                if (b) { double a = ang[k]; b->vx = (float)(20 * sbb_cos(a) * S.F); b->vy = (float)(-20 * sbb_sin(a)); b->av[0] = 5; }
            }
            snd_play_orig(376, 220, 128);
        }
    } else if (S.E >= 14 && (int)(S.E - 14) % 8 == 0) {                        /* fireballs (15895 / 15896, sound 377) */
        obj_t *f = ms_shot(MSS_FBALL, MSR_FBALL, c->x, c->y + 16);
        if (f) { f->av[0] = f->y; f->av[1] = 0; f->av[2] = 5 * S.F; f->av[3] = 0; }
        snd_play_orig(377, 220, 128);
    }
}

static void ms_fight(player_t *p) {
    obj_t *t = ms(MS_TORSO, 0), *h = ms(MS_HEAD, 0), *pu = get(OT_FDB_PUNCH, 0);
    if (!t || !h) return;
    S.E += 1;
    double rate = 0.05;
    switch (S.D) {
    case 0: {
        ms_pose(0, 0.05);
        int next = (S.dashes >= 2 || S.mlast == 1) ? (S.nspec % 2 == 0 ? 6 : 4) : 1;   /* (INFERRED order, RV s2_l) */
        int wait = next == 1 ? (S.mlast == 8 ? 31 : 41) : next == 6 ? 51 : 101;
        if (S.E > wait) {
            if (next == 1) { S.dashes++; ms_face(p); }
            else { S.nspec++; S.dashes = 0; ms_face(p); }
            S.mlast = 0; ms_setD(next);
        }
        break; }
    case 1: ms_pose(1, 0.025); if (S.E > 39) { ms_setD(2); snd_play_orig(375, 220, 128); } break;
    case 2:
        ms_pose(2, 0.1); rate = 0.1;
        if (S.E > 6) S.H = fmin(fmax(S.H + 15 * S.F, 7070), 8150);
        if (pu) { pu->hidden = 0; ms_row(pu, msr[MSR_PUNCH]); pu->x = (float)(int)(t->x + S.F * fmin(112, -47 + 16 * S.E)); pu->y = 4946; }
        if (S.E > 30) { ms_setD(3); if (pu) pu->hidden = 1; }
        break;
    case 3: ms_pose(0, 0.05); if (S.E > 19) { ms_setD(0); S.mlast = 3; } break;
    case 4: ms_pose(3, 0.05); ms_fire(ms(MS_RCLAW, 0)); if (S.E > 80) ms_setD(5); break;
    case 5: ms_pose(3, 0.05); if (S.E > 19) { ms_setD(0); S.mlast = 5; S.variant ^= 1; } break;
    case 6: ms_pose(4, 0.05); if (S.E > 19) { ms_setD(7); snd_play_orig(147, 220, 128); S.tx = t->x; } break;
    case 7:
        ms_pose(S.E > 10 ? 5 : 4, 0.05);
        if (S.E > 10 && S.E <= 46) { double u = (S.E - 10) / 36.0; S.H = S.tx + 360 * u * S.F; S.H = fmin(fmax(S.H, 7070), 8150); }
        if (S.E > 46) {
            ms_setD(8);
            obj_t *f = ms_shot(MSS_FLARE, MSR_FLARE10, t->x, 4966); if (f) { f->vx = (float)(-15 * S.F); f->vy = 0; f->av[0] = 1; }   /* 15951-15966 (RV) */
        }
        break;
    case 8: ms_pose(0, 0.05); if (S.E > 19) { ms_setD(0); S.mlast = 8; } break;
    }
    (void)rate;
    /* the torso: x halfway to H, y bobbing (RV); the jump's arc */
    S.Dang = fmod(S.Dang + (S.D == 2 ? 12 : 4), 360);
    t->x = (float)(int)(t->x + (S.H - t->x) * (S.D == 7 ? 1 : 0.5));
    double y = S.I + 20 * sbb_sin(2 * S.Dang);
    if (S.D == 7 && S.E > 10) y = S.I - 171 * sbb_sin(180 * (S.E - 10) / 36.0);
    t->y = (float)(int)y;
    /* the head: hits (Q20), the blink after a lost life (C 120), the lives */
    if (h->av[6] > 0) h->av[6] -= 1;
    if (h->av[6] > 0) h->fl |= F_ARMOURED; else h->fl &= ~F_ARMOURED;
    if (h->fl & F_DEFEATED) {                                                   /* 15913 */
        h->fl &= ~F_DEFEATED; h->av[7] -= 1; h->A = 40; h->av[6] = 120;
        snd_play_orig(47, 220, 128); snd_play_orig(378, 220, 128);
        if (h->av[7] == 1) { h->A += 20; music_play("bgm/m_boss6b.adp"); }       /* 15917: the last life (RE 15689) */
        if (h->av[7] <= 0) {                                                    /* 15918 */
            S.def = 1; S.E = 0; h->fl |= F_ARMOURED; music_stop(); snd_play_orig(309, 220, 128);
            if (pu) pu->hidden = 1;
        }
    }
    /* contact (15941 / 15942) */
    if (!S.def) { ms_hurt(p, t); ms_hurt(p, h); ms_hurt(p, ms(MS_LARM, 0)); if (pu && !pu->hidden) ms_hurt(p, pu); }
    /* the arena (the boss camera keeps the player in the view: INFERRED walls) */
    if (p->x < 6944 + 16) { p->x = 6944 + 16; player_sync_sensors(p); }
    if (p->x > 8224 - 16) { p->x = 8224 - 16; player_sync_sensors(p); }
}

static void ms_defeat(player_t *p) {
    obj_t *t = ms(MS_TORSO, 0), *n = npc();
    (void)p;
    S.E += 1;
    if (S.def == 1) {                                                           /* D 60 (15919-15930) */
        for (int j = 0; j < 6; j++) S.w[j] = j == 0 ? fmin(S.w[j] + 0.05, 1) : fmax(S.w[j] - 0.05, 0);
        if (t && (int)S.E % 5 == 0) {
            int k = cross_rand(MS_SIGHT); obj_t *o = ms(k, 0);
            if (o) { obj_effect(o->x - 20 + cross_rand(41), o->y - 20 + cross_rand(41), "explosion02"); tbb_explode_snd(); }
        }
        if (S.E == 98 && t) { obj_t *b = ms_shot(MSS_BUBBLE, MSR_BUBBLE, t->x, t->y); if (b) b->scale = 0.1f; }
        if (S.E > 230) { S.def = 2; S.E = 0; }
    } else {                                                                    /* D 61 (15931-15936) */
        if (S.E == 120) {
            S.gone = 1;
            for (int k = 0; k < MS_NSHOT; k++) { obj_t *o = slot(S.shot[k]); if (o && o->par == MSS_BUBBLE) o->alive = 0; }
            if (n && t) { n->x = t->x; n->y = 4992 - 32; n->hidden = 0; ms_row(n, msr[MSR_NPC]); }   /* (INFERRED: her feet on the floor) */
        }
        if (S.E > 220 && !S.results) { S.results = 1; results_begin_boss(game_timer()); }   /* 15936: Sign.B 179 -> the results */
    }
}

static void ms_shots(player_t *p) {
    obj_t *h = ms(MS_HEAD, 0);
    for (int k = 0; k < MS_NSHOT; k++) {
        obj_t *o = slot(S.shot[k]);
        if (!o || o->type != OT_FDB_SHOT) { S.shot[k] = -1; continue; }
        switch (o->par) {
        case MSS_BURST:
            if (o->av[0] > 0) { o->av[0] -= 1; break; }                          /* (RV: 5 frames at the claw) */
            o->x += o->vx; o->y += o->vy;
            if (!(o->fl & 2)) ms_hurt(p, o);
            if (obj_overlaps_bg(o)) { obj_effect(o->x, o->y, "explosion02"); o->alive = 0; }   /* 15949 */
            break;
        case MSS_FBALL:                                                         /* 15950: y = A - D sin B */
            o->av[1] += 6; o->av[3] = fmin(o->av[3] + 5, 100);
            o->x += (float)((o->fl & 2) ? -o->av[2] : o->av[2]);
            o->y = (float)(int)(o->av[0] - o->av[3] * sbb_sin(o->av[1]));
            if (!(o->fl & 2)) ms_hurt(p, o);
            if (o->av[1] > 360) o->alive = 0;
            break;
        case MSS_FLARE:
            o->x += o->vx; o->y += o->vy;
            if (o->av[0] == 0) {                                                /* the intro flare: up 50 frames, then down the shaft */
                obj_t *n = npc();
                if (o->vy < 0 && G.frame - o->born > 50 && n) { o->x = n->x - 150; o->y = 100; o->vy = 16; }
                if (o->y > 1400) o->alive = 0;
            } else { ms_hurt(p, o); if (!obj_near_view(o, 64)) o->alive = 0; }   /* 15966 */
            break;
        case MSS_BUBBLE: if (o->scale < 2) o->scale += 0.02f; break;             /* 15825 (INFERRED growth) */
        }
        if (o->alive && h && (o->fl & 2) && !(h->fl & (F_ARMOURED | F_DEFEATED)) && ov(o, h)) {   /* a reflected shot on the head */
            h->A -= 5; h->c = 5; snd_play_orig(24, 220, 128); o->alive = 0;
            if (h->A <= 0) h->fl |= F_DEFEATED;
        }
        if (o->alive && (o->par == MSS_BURST || o->par == MSS_FBALL) && !(o->fl & 2) && player_cube_hits(o)) {   /* 15947 / 15948 */
            o->fl |= 2; o->vx = -o->vx; o->vy = -o->vy; snd_play_orig(219, 220, 128);
        }
        if (o->alive && (o->par == MSS_BURST || o->par == MSS_FBALL) && !obj_near_view(o, 200)) o->alive = 0;
    }
}

static void ms_frame(player_t *p) {
    if (!S.on) return;
    if (PX(p) > 7000 && PY(p) > 3500 && !S.par) {                               /* 15836: the shaft's bottom shows the arena's sky */
        S.par = 1;
        static const short HID[] = { 3224, 3566, 3569, 3565, 3571, 3570 }, SHOW[] = { 3588, 3591, 3589, 3590 };   /* pieces 1 8 10 3 2 / 7 9 5 0 */
        for (unsigned k = 0; k < sizeof HID / sizeof *HID; k++) par_set_hidden(HID[k], 1);
        for (unsigned k = 0; k < sizeof SHOW / sizeof *SHOW; k++) par_set_hidden(SHOW[k], 0);
    }
    if (S.npcA >= 1 && !S.landed) S.hud = S.hud < 80 ? S.hud + 1 : 80;         /* 15814: loop_gui_shiftup_44 (1 px a frame, cap INFERRED) */
    else if (S.hud > 0) S.hud = S.hud > 2 ? S.hud - 2 : 0;                     /* (INFERRED: back down once in the arena) */
    if (S.npcA < 7) ms_intro(p);
    else if (!S.def) ms_fight(p);
    else ms_defeat(p);
    ms_shots(p);
    if (S.npcA >= 6) ms_layout();
}

static int ms_input(uint32_t *held) {
    if (!S.on || S.npcA < 1 || S.landed) return 0;
    obj_t *n = npc();
    *held = (S.npcA == 1 && n && G.p.x < n->x - 80) ? BTN_RIGHT : 0;            /* 15814 (walk), 15815 (stopped) */
    return 1;
}

/* ======================================================================== hooks */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&P, sizeof P); snap_register(&M, sizeof M); snap_register(&S, sizeof S); } }
void fdb12_spawn(void) {
    snap_once();
    memset(L, 0, sizeof L); on = 0;
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_FDB_PB && G.lvl.objs[j].type <= OT_FDB_LIST_LAST) on = 1;
    memset(&P, 0, sizeof P); memset(&M, 0, sizeof M); memset(&S, 0, sizeof S);
    if (!on) return;
    for (int k = 0; k < FS_N; k++) fsa[k] = (short)ens_anim(FSN[k]);
    for (int k = 0; k < MR_N; k++) mra[k] = (short)ens_anim(k < MP_N ? MPROW[k] : MRROW[k - MP_N]);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_FDB_PB || o->type > OT_FDB_LIST_LAST) continue;
        int t = o->type - OT_FDB_PB; if (L[t].n < LMAX) L[t].i[L[t].n++] = (short)i;
    }
    pb_spawn();
    ms_spawn();
}
void  fdb12_pre(player_t *p) { (void)p; }
void  fdb12_frame(player_t *p) {
    if (!on) return;
    pb_frame(p);
    m_frame(p);
    ms_frame(p);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_FDB")) {
        obj_t *c = get(OT_FDB_PBC, 0);
        printf("fdb %d px %.0f py %.0f pb %d cx %.0f alA %.0f D %.0f F %.0f al %d\n", G.frame, PX(p), PY(p), fd_boss_pb_state(), c ? c->x : -1, P.alA, G.cam_nD, G.cam_nF, P.alpha);
        obj_t *h = M.on ? mpart(MP_HEAD) : NULL;
        if (M.on) printf("fdm %d J %d P %.0f K %.0f L %.0f w %.2f %.2f %.2f %.2f %.2f %.2f %.2f hA %d hD %d hC %d def %d hp %d\n", G.frame, M.J, M.P, M.K, M.Lr,
            M.w[0], M.w[1], M.w[2], M.w[3], M.w[4], M.w[5], M.w[6], h ? (int)h->A : -1, M.hD, M.hC, M.def, fd_boss_morpher_hp());
        obj_t *t = S.on ? ms(MS_TORSO, 0) : NULL, *sh = S.on ? ms(MS_HEAD, 0) : NULL;
        if (t && sh) printf("fds %d st %d A %d G %.0f D %d E %.0f tx %.0f ty %.0f F %d hA %.1f H %.0f def %d hp %d px %.0f py %.0f\n", G.frame, fd_boss_mserp_state(), S.npcA, S.G,
            S.D, S.E, t->x, t->y, S.F, sh->A, sh->av[7], S.def, fd_boss_mserp_hp(), PX(p), PY(p));
    }
#endif
}
int   fdb12_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }       /* every rule runs in fdb12_frame */
int   fdb12_draw_obj(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_FDB_SXV || o->type == OT_FDB_ALARM) return 1;             /* invisible controllers (the alarm is pb_draw's overlay) */
    if (o->type == OT_FDB_MS + MS_HEAD && o->av[6] > 0 && ((int)o->av[6] / 2) % 2) return 1;   /* 15913: the blink after a lost life */
    if (m_draw_obj(o, pass)) return 1;
    if (!o->hidden) sbb_draw(o, 0xffffffff);
    return 1;
}
int   fdb12_q20(int type) { return type == OT_FDB_MPART + MP_HEAD || type == OT_FDB_MS + MS_HEAD; }   /* the morpher head takes the shared hits (RE: flag 6 by the shared rule, 15488) */
int   fdb12_input(uint32_t *held) { return on && ms_input(held); }
int   fdb12_player_hidden(void) { return 0; }
void  fdb12_draw(int pass) { if (on) { pb_draw(pass); m_draw(pass); } }
float fdb12_hud_dy(void) { return on && S.on ? -(float)S.hud : 0.f; }
