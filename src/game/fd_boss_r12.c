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
    if (pass != 2 || !P.alarm || P.tunnel) return;
    double v = sbb_sin(P.alA) * 200 + 180;                                      /* 15388: opacity (255 - the blend coefficient) */
    int a = v < 0 ? 0 : v > 255 ? 255 : (int)v;
    if (a > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)a << 24) | 0x983800);   /* img 11369: (152, 56, 0) */
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
enum { MR_TRAIN0 = MP_N, MR_TRAIN8, MR_TRAIN24, MR_EBALL, MR_LASER, MR_SHOCK, MR_SHOCK16, MR_SHOCKE, MR_GLOW, MR_HEAD1, MR_BODY1, MR_N };
static const char *const MRROW[MR_N - MP_N] = { "fdm_train0", "fdm_train8", "fdm_train24", "fdm_eball", "fdm_laser", "fdm_shock",
    "fdm_shock16", "fdm_shocke", "fdm_glow", "fdm_head1", "fdm_body1" };
static short mra[MR_N];
#define NSHOT 24
enum { SH_TRAIN = 1, SH_EBALL, SH_LASER, SH_SHOCK, SH_GLOW };
static struct {
    int on, J, R, hD, hC, def, endt, results, slen;
    double w[7], K, Lr, M, Q, P, bA, bB, bC, floorO, vx;
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

/* Part offsets from (K, L) per form weight (A B C D E F G), x then y: least-squares fit of the RV traces m1_l (A, B, C) and m2_l (D, E,
 * G) (residuals <= 5 px: the small sin(20 M) 1.1 bobs of 15412-15421 are left out, INFERRED); every weight 0: every part on (K, L)
 * (15409 / 15410: the ball between two forms). B is A turned upside down (the ceiling walker). Order: MP_* (ORB2 ORB1 WH4 WH3 WH2 WH1
 * TUBE1 TUBE2 GRILL2 GRILL1 PELVIS BODY NODE2 NODE1 HEAD). */
static const signed char MOFF[7][MP_N][2] = {
    { { 32, 8 }, { -32, 8 }, { 36, 44 }, { 12, 44 }, { -12, 44 }, { -36, 44 }, { -16, -16 }, { 16, -16 }, { 24, 32 }, { -24, 32 }, { 0, 0 }, { 0, 0 }, { 52, 0 }, { -52, 0 }, { 0, 0 } },   /* A */
    { { -32, -8 }, { 32, -8 }, { -36, -44 }, { -12, -44 }, { 12, -44 }, { 36, -44 }, { 16, 16 }, { -16, 16 }, { -24, -32 }, { 24, -32 }, { 0, 0 }, { 0, 0 }, { -52, 0 }, { 52, 0 }, { 0, 0 } },   /* B */
    { { 32, 64 }, { -24, 64 }, { 100, 84 }, { 68, 84 }, { 31, 63 }, { -25, 63 }, { 64, 32 }, { 80, 56 }, { 96, 72 }, { 72, 72 }, { -16, 32 }, { 0, 0 }, { 28, 24 }, { 3, 57 }, { 0, 0 } },   /* C */
    { { 0 } }, { { 0 } }, { { 0 } }, { { 0 } } };

static void m_layout(void) {
    double Ls = M.Lr;
    for (int k = 0; k < MP_N; k++) {
        obj_t *o = mpart(k);
        if (!o) continue;
        double x = M.K, y = Ls;
        for (int f = 0; f < 7; f++) { x += MOFF[f][k][0] * M.w[f]; y += MOFF[f][k][1] * M.w[f]; }
        o->x = (float)(int)x; o->y = (float)(int)y;
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
    if (w[3] >= 1) {
        if (M.Q == 0) M.Q = 1;
        if (M.K > G.camx + 360) M.Q = -1; else if (M.K < G.camx + 60) M.Q = 1;
        M.K += M.Q * 6;
        double s = sbb_sin(M.M), s2 = sbb_sin(M.M - 5);
        if (fabs(s) < fabs(s2) && fabs(sbb_sin(M.M + 5)) > fabs(s) && h) {    /* the bounce's lowest point (|sin M| minimum) */
            for (int d = -1; d <= 1; d += 2) { obj_t *sw = shot_new(SH_SHOCK, d < 0 ? MR_SHOCK16 : MR_SHOCK, (float)M.K, (float)sxv_y()); if (sw) sw->av[4] = 4 * d; }
            snd_play_orig(132, 220, 128); snd_play_orig(385, 220, 128); obj_effect((float)M.K, (float)sxv_y() - 16, "explosion");
        }
    } else M.Q = 0;
    /* E form: eases toward view x + 440 / 20 (15453); the body C counts: 40 and 100 elec balls from the orbs (15454-15457), past 120 0 */
    if (w[4] >= 1) {
        M.K = (19 * M.K + G.camx + 440 - 160) * 0.05;                          /* (the RE eases to (19 K + x + 440) / 20; INFERRED: 160 left of it) */
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
    M.floorO = fmod(M.floorO + 15, 128);                                        /* 15341 */
    M.M = fmod(M.M + 5, 360);                                                   /* 15407 */
    M.P += 1;                                                                   /* 15468 */
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
                    M.def = 1; M.J = 12; M.P = 0; h->fl |= F_ARMOURED;
                    music_stop();
                } else {                                                        /* 15488 */
                    h->fl &= ~F_DEFEATED; h->A = 50; M.hC = 100; M.J = 1; M.P = 0; M.R = 0; snd_play_orig(47, 220, 128);
                    strcpy(M.str, M.hD == 3 ? "5234" : M.hD == 2 ? "752" : "62754"); M.slen = (int)strlen(M.str);   /* 15465-15467 */
                }
            }
        }
    } else {                                                                    /* the defeat (group 0x22f51, 15519-15541) */
        for (int f = 0; f < 7; f++) w[f] = f == 2 ? fmin(w[f] + 0.05, 1) : fmax(w[f] - 0.05, 0);   /* 15520 */
        M.Lr = sxv_y() - 56 - 40 * w[2];
        if (h && M.P < 60) { obj_effect(h->x - 40 + cross_rand(81), h->y - 40 + cross_rand(81), "explosion02"); }   /* 15521 / 15522 */
        if (h && M.P >= 60 && M.P < 180 && g3 % 4 == 0) { obj_effect(h->x - 40 + cross_rand(81), h->y - 40 + cross_rand(81), "explosion06"); tbb_explode_snd(); }   /* 15524-15526 */
        if (M.P < 60 && M.K < G.camx + 120) M.K += 2;                           /* 15523 */
        if (M.P == 180) {                                                       /* 15527-15529: the parts fly apart, sound 38 */
            snd_play_orig(38, 220, 128);
            for (int k = 0; k < MP_N; k++) { obj_t *o = mpart(k); if (o) { o->av[5] = (k % 4) + 1; o->av[6] = -4 - (k % 3); } }
        }
        if (M.P > 180)
            for (int k = 0; k < MP_N; k++) { obj_t *o = mpart(k); if (o) { o->x += (float)o->av[5]; o->av[6] += 0.3; o->y += (float)o->av[6]; if (o->y > G.camy + SCREEN_H + 80) o->hidden = 1; } }   /* 15530 */
        if (M.P == 300) snd_play_orig(76, 220, 128);                            /* 15535 (+ the white fade) */
        if (M.P == 340) {                                                       /* 15538: the arena items removed */
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
    if (o->type == OT_FDB_SFLOOR || o->type == OT_FDB_SFLOOR2) {                 /* drawn scrolled (15341-15344) */
        obj_t t = *o; t.x = o->x - (float)M.floorO; sbb_draw(&t, 0xffffffff);
        if (M.on && o->x + 128 >= 512) { t.x += 128; sbb_draw(&t, 0xffffffff); }
        return 1;
    }
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

/* ======================================================================== hooks */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&P, sizeof P); snap_register(&M, sizeof M); } }
void fdb12_spawn(void) {
    snap_once();
    memset(L, 0, sizeof L); on = 0;
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_FDB_PB && G.lvl.objs[j].type <= OT_FDB_PLACED_LAST) on = 1;
    memset(&P, 0, sizeof P); memset(&M, 0, sizeof M);
    if (!on) return;
    for (int k = 0; k < FS_N; k++) fsa[k] = (short)ens_anim(FSN[k]);
    for (int k = 0; k < MR_N; k++) mra[k] = (short)ens_anim(k < MP_N ? MPROW[k] : MRROW[k - MP_N]);
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
    m_frame(p);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_FDB")) {
        obj_t *c = get(OT_FDB_PBC, 0);
        printf("fdb %d px %.0f py %.0f pb %d cx %.0f alA %.0f D %.0f F %.0f\n", G.frame, PX(p), PY(p), fd_boss_pb_state(), c ? c->x : -1, P.alA, G.cam_nD, G.cam_nF);
        obj_t *h = M.on ? mpart(MP_HEAD) : NULL;
        if (M.on) printf("fdm %d J %d P %.0f K %.0f L %.0f w %.2f %.2f %.2f %.2f %.2f %.2f %.2f hA %d hD %d hC %d def %d hp %d\n", G.frame, M.J, M.P, M.K, M.Lr,
            M.w[0], M.w[1], M.w[2], M.w[3], M.w[4], M.w[5], M.w[6], h ? (int)h->A : -1, M.hD, M.hC, M.def, fd_boss_morpher_hp());
    }
#endif
}
int   fdb12_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }       /* every rule runs in fdb12_frame */
int   fdb12_draw_obj(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_FDB_SXV || o->type == OT_FDB_ALARM) return 1;             /* invisible controllers (the alarm is pb_draw's overlay) */
    if (m_draw_obj(o, pass)) return 1;
    if (!o->hidden) sbb_draw(o, 0xffffffff);
    return 1;
}
int   fdb12_q20(int type) { return type == OT_FDB_MPART + MP_HEAD; }   /* the morpher head takes the shared hits (RE: flag 6 by the shared rule, 15488) */
int   fdb12_input(uint32_t *held) { (void)held; return 0; }
int   fdb12_player_hidden(void) { return 0; }
void  fdb12_draw(int pass) { if (on) { pb_draw(pass); m_draw(pass); } }
float fdb12_hud_dy(void) { return 0.f; }
