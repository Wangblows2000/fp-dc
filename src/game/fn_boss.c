/* Fortune Night bosses and minibosses (session 17): the UFO (FN1 floor breaker, FN3 beam chase, Serpentine's ship), the
 * Bulldozer (FN1 / FN2), Serpentine (FN3, Lilac only) and the samurai MinibossSamu (FN3 Carol / Milla, FN5 everyone). The
 * Panther (FN5 / FN6) is fn_panther.c's.
 * Source: build/re/spec_fn_bosses.md Parts A-E (static RE of handle_frame_30..35_events; no original trace of these fights
 * yet: Part Z). Labels: RE read from code, INFERRED reasoned (marked), UNKNOWN (spec U-number).
 * Order in the frame (RE): the test_collisions handlers (fn_boss_pre, before the shared combat events 2730-2767), then the
 * encounters' own event blocks (fn_boss_frame, after the gimmicks). Every rule is attached to the object types placed by the
 * level; per-act differences are level data (level_def_t.fnb), per-character ones are the flow's class sets (levels.c
 * CK_FN3: the Serpentine set exists only for Lilac, the FN3 samurai set only for Carol / Milla). Nothing tests a level name.
 * Boss state lives in the module (one encounter of each kind per act); it is mirrored into the objects' av[] for FP_TRACK.
 * Performance (SH4): every instance list is built once at spawn; a level without these objects returns at the first test. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern int fe_difficulty;                                                      /* G[12]: 0 Hard (frontend.c) */

#define DEFEATED(o) ((o)->fl & 0x40)
#define F_ARM 0x20                                                              /* enemy flag 5: armoured (combat.c deflects) */
#define F_DEF 0x40
#define RAD 57.29577951308232
static int R(int n) { return cross_rand(n); }
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MVB(const player_t *p) { return p->ground ? p->gsp : p->vx; }
static void setB(player_t *p, double b) { if (p->ground) p->gsp = b; else p->vx = b; }
static void setG(player_t *p, double g) { if (p->ground) { p->ground = 0; } p->vy = g; }   /* MV.G (a grounded player leaves the floor, INFERRED) */
static double nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static double c_sin(double deg) { double r = nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / RAD); }
static double c_cos(double deg) { double r = nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / RAD); }
static double sgn(double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
static double approach(double v, double t, double d) { return v < t ? fmin(v + d, t) : fmax(v - d, t); }

/* ---- sprites (tools/import_frame.py FNBOSS_ENS), looked up once per level */
enum { S_UFO, S_UFO7, S_UFO8, S_UFOP, S_HATCH0, S_HATCH1, S_BEAM, S_BORDER, S_UFODEB,
       S_DZ, S_DZ8, S_DZ10, S_DZARM, S_DZCLAW, S_DOOR, S_DOOR7, S_DOOR8, S_DOOR10, S_DZHIT, S_DZHIT8, S_DZDEB, S_WHEEL0, S_WHEEL1, S_LIM, S_BOOM,
       S_FS0, S_FS0L, S_FS6, S_FS6L, S_FS7, S_FS7L, S_FS8, S_FS8L, S_FS9, S_FS9L, S_FS15, S_FS15L, S_FS16, S_FS16L, S_FS12, S_FS13, S_FS9U, S_FS14, S_FS14L,
       S_SHIT, S_SPROBE, S_SSHIELD, S_GLOW, S_WARN, S_WARN2, S_RB8, S_RB24, S_MIS, S_MISL, S_LASER3, S_LASER0,
       S_SARM1, S_SARM2, S_SARM3, S_SARM17, S_SARM18, S_SARM19,
       S_SAMU0, S_SAMU16, S_SAMU24, S_SAMU8, S_SAMU9, S_HAND0, S_HAND0_8, S_HAND1, S_HAND1_8, S_SWHEEL, S_SPROBE2, S_SDEB, S_GATE0, S_GATE1, S_N };
static const char *const SN[S_N] = { "fb_ufo", "fb_ufo7", "fb_ufo8", "fb_ufop", "fb_hatch0", "fb_hatch1", "fb_beam", "fb_border", "fb_ufodeb",
    "fb_dz", "fb_dz8", "fb_dz10", "fb_dzarm", "fb_dzclaw", "fb_door", "fb_door7", "fb_door8", "fb_door10", "fb_dzhit", "fb_dzhit8", "fb_dzdeb", "fb_wheel0", "fb_wheel1", "fb_lim", "fb_boom",
    "fs_0", "fs_0_l", "fs_6", "fs_6_l", "fs_7", "fs_7_l", "fs_8", "fs_8_l", "fs_9", "fs_9_l", "fs_15", "fs_15_l", "fs_16", "fs_16_l", "fs_12", "fs_13", "fs_9u", "fs_14", "fs_14_l",
    "fb_shit", "fb_sprobe", "fb_sshield", "fb_glow", "fb_warn2", "fb_warn2", "fb_rb8", "fb_rb24", "fb_mis", "fb_mis_l", "fb_laser3", "fb_laser0",
    "fb_sarm1", "fb_sarm2", "fb_sarm3", "fb_sarm17", "fb_sarm18", "fb_sarm19",
    "fb_samu0", "fb_samu16", "fb_samu24", "fb_samu8", "fb_samu9", "fb_hand0", "fb_hand0_8", "fb_hand1", "fb_hand1_8", "fb_swheel", "fb_sprobe2", "fb_sdeb", "fb_gate0", "fb_gate1" };
static short SA[S_N];
static void set_a(obj_t *o, int k) { if (k >= 0 && SA[k] >= 0 && (o->an.b != &G.ens || o->an.anim != SA[k])) anim_set(&o->an, &G.ens, SA[k]); }   /* force_animation */
static void restart_a(obj_t *o, int k) { if (k >= 0 && SA[k] >= 0) { anim_set(&o->an, &G.ens, SA[k]); anim_restart(&o->an); } }
static int a_is(const obj_t *o, int k) { return k >= 0 && SA[k] >= 0 && o->an.b == &G.ens && o->an.anim == SA[k]; }
static int a_done(const obj_t *o, int k) { return a_is(o, k) && o->an.done; }
static void force_frame(obj_t *o, int f) { if (!o->an.b) return; int n = o->an.b->anims[o->an.anim].count; o->an.frame = f < 0 ? 0 : f < n ? f : n - 1; o->an.acc = 0; }

/* ---- shared helpers */
static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static obj_t *spawn(int type, int sk, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    set_a(o, sk);
    return o;
}
static void act_pt(const obj_t *o, double *x, double *y) { relic_act_point(o, x, y); }   /* rotated with the object */
static void ibox(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = *x1 = o->x; *y0 = *y1 = o->y; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int obstacle(float x, float y) { return px_solid(&G.lvl, (int)floorf(x), (int)floorf(y)); }
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {      /* PlayerCharacterSensor_375 */
    int X = (int)PX(p), Y = (int)PY(p);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return o && o->alive && o->an.b && obj_rect_hit(o, a, b, c, d); }
static int csens_rect(const player_t *p, float x0, float y0, float x1, float y1) { int a, b, c, d; csens(p, &a, &b, &c, &d); return a < x1 && x0 < c && b < y1 && y0 < d; }
static int p326_hit(const obj_t *o) {                                           /* Player_326 (the movement's main sensor rows) */
    int ox, oy, h; const signed char *x0, *x1;
    return o && o->alive && o->an.b && mv_sensor_rows(SENS_MAIN, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(o, ox, oy, x0, x1, h);
}
static int p326_rect(const player_t *p, float x0, float y0, float x1, float y1) {   /* the 27-px disc at (int X, int Y) against a box */
    double px = PX(p), py = PY(p), cx = fmax(x0, fmin(px, x1 - 1)), cy = fmax(y0, fmin(py, y1 - 1));
    return (cx - px) * (cx - px) + (cy - py) * (cy - py) <= 13.5 * 13.5;
}
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }   /* MV.R = 0, OTHERS.E <= 0 */
static int r_hurt(player_t *p, int n) { if (!vulnerable(p)) return 0; player_contact(p, n); return 1; }   /* MV.R = 1, LIFE.B += n */
static void zadd(float z) { G.cam.Z += z; }                                     /* CAM.Z += z */
static void explode_snd(void) { static const int SX[3] = { 311, 319, 320 }; snd_play_orig(SX[R(3)], 220, 128); }   /* named sample sound_explode<3..5> (fn_enemy.c) */
static int bg_overlap(const obj_t *o) { return o->an.b && obj_overlaps_bg(o); }

/* ---- instance lists (spawn) */
#define NL 32
static short floors[96]; static int nfloors;                                    /* BreakableFloor1/2 (fn_gima.c types) */
static short lamps[NL]; static int nlamps;                                      /* LampPostFront_1791 */
static short trig[8]; static int ntrig;                                         /* CameraTriggerBoss_389 */
static short sol[NL]; static int nsol;                                          /* Q7 instances near the samurai / dozer (gates, columns) */
static short cols[8]; static int ncols;                                         /* LargeColumn / LargeColumn2 (fn_gima.c 596) */
static short bars[8]; static int nbars;                                         /* MinibossBarrier_969 */
static int on;                                                                  /* the level places any of these bosses */
static const fnboss_t *fnb(void) { return level_cur()->fnb; }
static short marks[4];                                                         /* CameraLimitBoss L / R / T / B slots (cached at spawn) */
static float marker(int t, int x) { obj_t *o = t >= OT_CAMLIM_L && t <= OT_CAMLIM_B ? slot(marks[t - OT_CAMLIM_L]) : NULL; return o ? (x ? o->x : o->y) : -1e9f; }
static int floor_overlap_rect(float x0, float y0, float x1, float y1, int hit, double X, double Y, int need_a) {   /* floors overlapping a box: A += 1 (hit 1), break at A >= need_a */
    int any = 0;
    for (int k = 0; k < nfloors; k++) {
        obj_t *f = slot(floors[k]);
        if (!f || f->type != OT_FN_BFLOOR || !f->an.b) continue;
        if (fabsf(f->x - x0) > 600 || f->y + 128 < y0 || f->y - 128 > y1) continue;
        if (!obj_rect_hit(f, (int)x0, (int)y0, (int)x1, (int)y1)) continue;
        any = 1;
        if (hit == 1) f->A += 1;
        if (hit == 2 || (need_a > 0 && f->A >= need_a)) fn_gima_floor_hit(f, X, Y);
    }
    return any;
}

/* ======================================================================== PART A / C: MinibossUFO (FN1, FN3)
 * U.G state 0-5 (A1) / 0 (FN3 chase), H x speed, D angle, F its velocity, E light phase, I base y, J bob phase, K hatch angle,
 * L state timer, M climb speed (FN3 7795), N light spin (Serpentine's escape); beam: A y-scale, B x (FN3), fl1 / dir.
 * UFO flag 1 (fl1): camera released. The UFO set is also Serpentine's ship (PART D, ufo_escape). */
static struct {
    int ufo, piece[4], hatch[2], beam, deb, nborder; short border[4];
    int act;                                    /* group 0x225d4 on */
    double G, H, D, F, E, I, J, K, L, M, N;
    int fl1, fl13;
    double bA, bB; int bvis, bfl1, bdir;
    int music4;                                 /* 7726 played m_midboss */
} U;
static obj_t *ufo(void) { obj_t *o = slot(U.ufo); return o && o->type == OT_FN_UFO ? o : NULL; }
static obj_t *beam(void) { obj_t *o = slot(U.beam); return o && o->type == OT_FN_UFO_BEAM ? o : NULL; }
static void ufo_show(int vis) {                                                 /* the UFO, its lights and hatches */
    obj_t *u = ufo(); if (u) u->hidden = !vis;
    for (int k = 0; k < 4; k++) { obj_t *q = slot(U.piece[k]); if (q) q->hidden = !vis; }
    for (int k = 0; k < 2; k++) { obj_t *q = slot(U.hatch[k]); if (q) q->hidden = !vis; }
}
static void ufo_layer(int l) {
    obj_t *u = ufo(); if (u) u->layer = l;
    for (int k = 0; k < 4; k++) { obj_t *q = slot(U.piece[k]); if (q) q->layer = l; }
    for (int k = 0; k < 2; k++) { obj_t *q = slot(U.hatch[k]); if (q) q->layer = l; }
    obj_t *b = beam(); if (b) b->layer = l;
}
static void ufo_start(void) {                                                   /* 7368 / 7794 / 7819: hatches, I = Y, visible, pieces A = 0..3 */
    obj_t *u = ufo(); if (!u) return;
    U.I = u->y; ufo_show(1);
    for (int k = 0; k < 4; k++) { obj_t *q = slot(U.piece[k]); if (q) q->A = 3 - k; }   /* spread, selection order last created first (INFERRED) */
}
static void ufo_rig(void) {                                                     /* 7370-7376 (A3: 7921-7927) */
    obj_t *u = ufo(); if (!u) return;
    u->ang = (float)U.D;
    for (int k = 0; k < 4; k++) { obj_t *q = slot(U.piece[k]); if (q) q->ang = (float)U.D; }
    obj_t *h1 = slot(U.hatch[0]), *h2 = slot(U.hatch[1]);
    if (h1) h1->ang = (float)(U.D - U.K);
    if (h2) h2->ang = (float)(U.D + U.K);
    U.D += U.F;
    for (int k = 0; k < 4; k++) {                                               /* 7371 */
        obj_t *q = slot(U.piece[k]); if (!q) continue;
        double B = 90 * q->A + U.E; q->av[1] = B;
        q->x = (float)(int)(u->x + c_cos(B) * c_cos(U.D) * 80); q->y = (float)(int)(u->y - c_cos(B) * c_sin(U.D) * 80);
        q->fl = (B > 180 && B < 360) || B > 540 ? (q->fl | 0x1000) : (q->fl & ~0x1000);   /* 7373-7376: behind the hull while sin B < 0 */
    }
    if (h1) { h1->x = u->x; h1->y = u->y; }
    if (h2) { h2->x = u->x; h2->y = u->y; }
    u->x = (float)(int)(u->x + U.H);                                            /* set_x truncates */
}
static void beam_cut(player_t *p, int need_a, double X1, int x2_mvb) {          /* 7389-7393 / 7800-7801: beam damage and floor cutting */
    obj_t *b = beam(), *u = ufo();
    if (!b || !U.bvis) return;
    float x0 = b->x - 24, x1 = b->x + 24, y0 = b->y, y1 = b->y + (float)(48 * U.bA);
    if (csens_rect(p, x0, y0, x1, y1)) r_hurt(p, 10);                           /* 7389: 10 */
    for (int fl = 0; fl < 2; fl++) {                                            /* 7390 (Floor1) / 7391 (Floor2) */
        int any = 0;
        for (int k = 0; k < nfloors; k++) {
            obj_t *f = slot(floors[k]);
            if (!f || f->type != OT_FN_BFLOOR || (f->par & 1) != fl || !f->an.b) continue;
            if (f->x + 96 < x0 || f->x - 96 > x1 || f->y + 96 < y0 || f->y - 96 > y1) continue;
            if (!obj_rect_hit(f, (int)x0, (int)y0, (int)x1, (int)y1)) continue;
            f->A += 1; any = 1;
        }
        if (!any) continue;
        if (u) u->c = 5;
        U.bA -= 0.5;
        float by = y1;                                                          /* the action point: bottom of the column */
        for (int s = -1; s <= 1; s += 2) obj_effect((float)(b->x + 16 * s - 10 + R(21)), (float)(by - 8 + R(17)), "wburst");
    }
    for (int k = 0; k < nfloors; k++) {                                         /* 7392 / 7393 (A1: A >= 5), 7800 / 7801 (FN3: 4) */
        obj_t *f = slot(floors[k]);
        if (f && f->type == OT_FN_BFLOOR && f->A >= need_a) { fn_gima_floor_hit(f, (f->par & 1) && x2_mvb ? MVB(p) : X1, -4); f->A = -1000; }
    }
}
static void ufo_a1(player_t *p) {
    obj_t *u = ufo(), *b = beam();
    if (!u) return;
    if (U.G == 0) { u->fl |= F_ARM; u->c = 5; } else u->fl &= ~F_ARM;       /* 7367: armoured before the encounter (cleared after: spec A.5, INFERRED) */
    if (!U.act) {                                                               /* 7368: Player_326 on a CameraTriggerBoss */
        if (U.G != 0) return;
        int hit = 0;
        for (int k = 0; k < ntrig && !hit; k++) { obj_t *t = slot(trig[k]); if (t && fabsf(t->x - u->x) < 1200) hit = p326_rect(p, t->x, t->y, t->x + 16, t->y + 192); }   /* image 1317: 16 x 192, hot spot (0, 0) */
        if (!hit) return;
        U.act = 1; ufo_start(); level_lock_markers();                           /* CAM.M = 1 (x 6984..7559, y 824..1831) */
    }
    if (!U.fl1 && u->y + 300 < PY(p)) { U.fl1 = 1; level_release_camera(0); }   /* 7369: the player fell through the floor */
    ufo_rig();
    U.E = fmod(U.E + 4, 360);                                                   /* 7372 */
    obj_t *last = slot(U.nborder ? U.border[U.nborder - 1] : -1);
    if (U.G == 0) { U.H = 12; if (last && u->x >= last->x - 449) U.G = 1; }     /* 7377 / 7378 */
    if (U.G == 1) U.H = fmax(U.H - 0.1, 3);                                     /* 7379 */
    if (U.G >= 1 && U.D > 0 && U.F > -2) U.F -= 0.075;                          /* 7380 */
    if (U.G == 1 && U.D <= 0) U.F += 0.075;                                     /* 7381 */
    if (U.G == 1 && U.H <= 3) U.G = 2;                                          /* 7382 */
    if (U.G == 2 && U.D <= 0) { U.D = fmin(U.D + 1, 0); U.F = fmin(U.F + 0.1, 0); }   /* 7383 */
    if (U.G >= 2) { u->y = (float)(int)(U.I + 10 * c_sin(U.J)); U.J += 2; }     /* 7384: the bob */
    if (U.G == 2 && U.D == 0) U.K = fmin(U.K + 3, 45);                          /* 7385 */
    if (U.G == 2 && U.K >= 45) { set_a(u, S_UFO7); U.G = 3; snd_play_orig(168, 220, 128); }   /* 7386 */
    if (U.G == 3 && u->c <= 0) U.L += 1;                                        /* 7387 */
    if (U.G == 3 && U.L == 60) { U.L = 61; snd_play_orig(169, 220, 128); }      /* 7388 */
    if (b) { b->x = u->x; b->y = u->y; }                                        /* 7394 (inline): the beam hangs from the UFO */
    beam_cut(p, 5, 0, 0);                                                       /* 7389-7393 */
    if (U.G == 3 && U.L > 60 && u->c <= 0) { U.bA = fmin(U.bA + 0.5, 12); U.bvis = 1; }   /* 7395 */
    if (U.G == 3 && U.L > 220) { U.G = 4; U.L = 80; }                           /* 7396 */
    if (U.G == 4 && U.L > 60) { U.L -= 1; U.bA = fmax(U.bA - 0.5, 0); }         /* 7397 */
    if (U.G == 4 && U.L <= 60 && !U.fl1) level_release_camera(0);               /* 7398: CAM.M 2, U 0 */
    if (U.G == 4 && U.L <= 60) { U.G = 5; U.L = 0; set_a(u, S_UFO); U.bvis = 0; U.fl1 = 1; }   /* 7399 */
    if (U.G == 5) {                                                             /* 7400-7403 */
        U.K = fmax(U.K - 3, 0); U.H = fmin(U.H + 0.1, 16);
        if (U.D > -90) U.D -= 1;
        if (u->x > G.camx + 826) ufo_show(0);                                   /* it never returns */
    }
}
static int in_zone(const player_t *p, const short *z) { double x = PX(p), y = PY(p); return x >= z[0] && x <= z[1] && y >= z[2] && y <= z[3]; }
static void ufo_chase(player_t *p) {                                            /* PART C: FN3 7794-7810 (every character) */
    const fnboss_t *F = fnb();
    obj_t *u = ufo(), *b = beam();
    if (!u) return;
    if (U.G == 0) { u->fl |= F_ARM; u->c = 5; }                                 /* 7367 */
    if (!U.act) {                                                               /* 7794 */
        if (!(PX(p) > F->chase_x0 && PX(p) < F->chase_x1) || U.G != 0) return;
        U.act = 1; ufo_start();
    }
    ufo_rig(); U.E = fmod(U.E + 4, 360);                                        /* 7370-7376 (+ 7372) */
    if (U.G == 0 && u->y > -100) { U.H = fmax(MVB(p) + 5, 12); U.M -= 0.05; u->y = (float)(int)(u->y + U.M); U.D += 1; }   /* 7795: races ahead and climbs */
    if (u->y <= -100) { U.H = 0; U.M = 0; }                                     /* 7796: parked above the level */
    if (b && !U.bfl1) {
        if (in_zone(p, F->zone[0])) { u->av[7] += 1; snd_play_orig(169, 220, 128); U.bfl1 = 1; U.bB = PX(p) + 80; U.bdir = 16; }   /* 7797 */
        else if (in_zone(p, F->zone[1])) { u->av[7] += 1; snd_play_orig(169, 220, 128); U.bfl1 = 1; U.bB = PX(p) - 80; U.bdir = 0; }   /* 7798 */
    }
    if (b) { b->y = (float)(G.camy - 32); b->x = (float)(int)U.bB; }            /* 7802: a column from the top of the view */
    beam_cut(p, 1 << 30, 0, 0);                                                 /* 7389-7391 (the breaks are 7800 / 7801 below) */
    if (b && U.bvis) for (int k = 0; k < nlamps; k++) {                         /* 7799: a lamppost under the beam breaks */
        obj_t *l = slot(lamps[k]);
        if (l && l->type == OT_FN_LAMP && !(l->fl & F_DEF) && obj_rect_hit(l, (int)b->x - 24, (int)b->y, (int)b->x + 24, (int)(b->y + 48 * U.bA))) l->fl |= F_DEF;
    }
    for (int k = 0; k < nfloors; k++) {                                         /* 7800 / 7801: A >= 4, X = 0 / MV.B, Y = -4 */
        obj_t *f = slot(floors[k]);
        if (f && f->type == OT_FN_BFLOOR && f->A >= F->cut_a) { fn_gima_floor_hit(f, (f->par & 1) && F->floor2_mvb ? MVB(p) : 0, -4); f->A = -1000; }
    }
    if (b && U.bfl1 && U.bA < 12) {                                             /* 7803 */
        float x0 = b->x - 24, x1 = b->x + 24, y0 = b->y, y1 = b->y + (float)(48 * U.bA);
        if (!floor_overlap_rect(x0, y0, x1, y1, 0, 0, 0, 0)) { U.bA += 0.5; U.bvis = 1; }
    }
    if (U.bfl1 && U.bdir == 16 && p->hitstop <= 0) U.bB += fmin(MVB(p), -3);    /* 7804 */
    if (U.bfl1 && U.bdir == 0) U.bB += fmax(MVB(p), 3);                         /* 7805 */
    if (!U.bfl1) U.bA = fmax(U.bA - 0.5, 0);                                    /* 7806 */
    if (!U.bfl1 && U.bA <= 0) U.bvis = 0;                                       /* 7807 */
    if (U.bfl1 && U.bdir == 16 && !in_zone(p, F->zone[0])) U.bfl1 = 0;          /* 7808 */
    if (U.bfl1 && U.bdir == 0 && !in_zone(p, F->zone[1])) U.bfl1 = 0;           /* 7809 */
    if (in_zone(p, F->zone[2]) && U.bA <= 0) U.bvis = 0;                        /* 7810 */
}

/* ======================================================================== PART B: MinibossBulldozer (FN1, FN2)
 * Z.J state (0 drive, 1 raise, 2 stop, 3 slam, 6 exploding, 7 wreck falling, 8 rests), L timer, G x speed, M drive direction,
 * D arm angle, E facing (+1 in the port, see the spawn), F wheel angle, H tread phase, I x (double), K door open, N claw armed, O / P wreck y / speed,
 * fl1 limiter latch, fl4 landed. Door: av[0] E open counter. Boomerang: av[0] A x, av[1] B speed, av[2] C range, av[3] D
 * target speed, fl 0x1000 flag 1 (returning), 0x2000 backdrop overlap (rising edge). */
static struct {
    int dz, arm, claw, door, wheel[2], hit, lim[4], nlim;
    int act;
    double J, L, G, M, D, E, F, H, I, K, N, O, P, Q;
    int fl1, fl4;
} Z;
static obj_t *dozer(void) { obj_t *o = slot(Z.dz); return o && o->type == OT_FN_DOZER ? o : NULL; }
static void boomerang_new(obj_t *d) {                                           /* 7423 */
    double ax, ay; act_pt(d, &ax, &ay);
    obj_t *b = spawn(OT_FN_BOOMERANG, S_BOOM, (float)ax, (float)(ay - 6), d->layer);
    if (!b) return;
    b->av[0] = b->x; b->av[2] = 100; b->av[1] = 6 * Z.E; b->av[3] = 6 * Z.E;
    snd_play_orig(28, 220, 128);
    b->x = (float)(int)(d->x + 30 * Z.E); b->av[0] = b->x;
}
static void dozer_frame(player_t *p) {
    obj_t *d = dozer();
    if (!d) return;
    obj_t *arm = slot(Z.arm), *claw = slot(Z.claw), *door = slot(Z.door), *hb = slot(Z.hit);
    if (!Z.act) { if (!obj_near_view(d, 300)) return; Z.act = 1; }             /* 7404: group 0x225d5 on (no lock, no music) */
    int near3 = obj_near_view(d, 300), near2 = obj_near_view(d, 200), def = DEFEATED(d) != 0;
    if (claw && Z.G < 0 && p326_hit(claw)) { p->x += Z.G; player_sync_sensors(p); }   /* 7405: pushed while it drives left */
    if (hb && hb->an.b) {                                                       /* 7406: the body shoves the player */
        int ox, oy, h; const signed char *x0, *x1, B = 0; (void)B;
        int l = mv_sensor_rows(SENS_LEFTBIG, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(hb, ox, oy, x0, x1, h);
        int r = mv_sensor_rows(SENS_RIGHTBIG, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(hb, ox, oy, x0, x1, h);
        if ((l && Z.G > 0 && MVB(p) <= 0) || (r && Z.G < 0 && MVB(p) >= 0)) { p->x += Z.G; player_sync_sensors(p); }
    }
    if (claw && Z.N == 1 && p326_hit(claw)) {                                   /* 7407 / 7408 */
        if (vulnerable(p)) { player_contact(p, 10); setB(p, -8); }
        else if (p->state == ST_HURT) setB(p, -8);
    }
    if (Z.J < 6) {                                                              /* 7409-7412 */
        if (Z.M == -1) Z.G = fmax(Z.G - 0.1, -3);
        if (Z.M == 1) Z.G = fmin(Z.G + 0.1, 3);
        if (Z.G < 0 && Z.M != -1) Z.G = fmin(Z.G + 0.1, 0);
        if (Z.G > 0 && Z.M != 1) Z.G = fmax(Z.G - 0.1, 0);
        int lim = 0;                                                            /* 7413 / 7414 */
        for (int k = 0; k < Z.nlim && !lim; k++) { obj_t *m = slot(Z.lim[k]); if (m && m->an.b && obj_obj_hit(d, m)) lim = 1; }
        if (!lim) Z.fl1 = 0;
        else if (!Z.fl1) { Z.fl1 = 1; Z.M = -Z.M; }
    }
    if (Z.H < 0) Z.H += 12;                                                     /* 7415 */
    if (Z.J == 0 && d->c <= 0 && near2) Z.L += 1;                               /* 7416 */
    if (Z.J == 0 && Z.L >= 150 && near2) { snd_play_orig(165, 220, 128); Z.J = 1; Z.L = 0; }   /* 7417 / 7418: rev */
    if (Z.J == 1 && d->c <= 0) Z.D = fmin(Z.D + 2, 90);                         /* 7419 */
    if (Z.J == 1 && Z.D == 90 && near2) { snd_play_orig(166, 220, 128); Z.K = 1; Z.J = 2; }   /* 7420 / 7421 (stop 165: one-shot here) */
    if (Z.J == 2) { Z.G = 0; Z.L += 1; }                                        /* 7422 */
    if (Z.J == 2 && Z.L == 50) { Z.L = 51; boomerang_new(d); }                  /* 7423 */
    if (Z.J == 2 && Z.L >= 140) { Z.K = 0; Z.G = 0; }                           /* 7424 */
    if (Z.J == 2 && Z.L >= 160) { Z.J = 3; Z.L = 0; }                           /* 7425 */
    if (Z.J == 3) { Z.G = 0; Z.N = 1; Z.D = fmax(Z.D - 8, 0); }                 /* 7426 */
    if (Z.J == 3 && Z.D == 0 && Z.L == 0) { Z.L = 1; snd_play_orig(51, 220, 128); zadd(5); }   /* 7427 / 7606 */
    else if (Z.J == 3 && Z.D == 0) Z.L += 1;                                    /* 7428 */
    if (Z.J == 3 && Z.L >= 30) { Z.J = 0; Z.L = 0; Z.N = 0; }                   /* 7429 */
    if (near3 && ((d->c <= 0 && !def) || def)) { d->x = (float)(int)Z.I; Z.I += Z.G; }   /* 7430 */
    if (near3 && Z.J < 6) {                                                     /* 7431: treads, wheels */
        Z.H += Z.G; if (a_is(d, S_DZ)) force_frame(d, (int)floor(fmod(Z.H, 12) / 3)); Z.F -= 2 * Z.G;
    }
    if (near2 && !def && G.frame % 10 == 1) {                                   /* 7432: exhaust */
        obj_t *e = spawn(OT_EXPLODE, -1, (float)(d->x - 40 * Z.E), (float)(d->y - 78), d->layer);
        if (e) { int a = ens_anim("fndust"); if (a >= 0) anim_set(&e->an, &G.ens, a); else e->alive = 0; e->vy = -1; }
    }
    /* 7433-7435: rig */
    double ax, ay; act_pt(d, &ax, &ay);
    if (arm) { arm->x = (float)ax; arm->y = (float)ay; arm->ang = (float)(Z.D * Z.E); arm->dir = d->dir; arm->hidden = Z.J >= 7; }
    if (claw) { claw->x = (float)ax; claw->y = (float)ay; claw->ang = (float)(Z.D * Z.E); claw->dir = d->dir; claw->hidden = Z.J >= 7; }
    for (int k = 0; k < 2; k++) {
        obj_t *w = slot(Z.wheel[k]); if (!w) continue;
        w->av[0] = k ? d->x + 43 * Z.E : d->x - 38 * Z.E; w->av[1] = k ? d->y + 4 : d->y; w->ang = (float)Z.F;
        if (Z.J < 6) { w->x = (float)w->av[0]; w->y = (float)w->av[1]; }
    }
    if (door) { door->x = d->x; door->y = d->y; door->dir = d->dir; }
    if (hb) { hb->x = d->x; hb->y = d->y; hb->dir = d->dir; }
    /* 7439-7446: the door (A = 1 while closed and armoured; one hit while open breaks it) */
    if (door) {
        int K = (int)Z.K;
        if (K == 0 && a_is(door, S_DOOR8)) { restart_a(door, S_DOOR); door->A = 1; door->av[0] = 0; door->fl &= ~F_DEF; door->fl |= F_ARM; }   /* 7439 */
        if (K == 0 && !a_is(door, S_DOOR8)) { set_a(door, S_DOOR); door->A = 1; door->av[0] = 0; door->fl &= ~F_DEF; door->fl |= F_ARM; }      /* 7440 */
        if (K == 1 && !a_is(door, S_DOOR7) && !a_is(door, S_DOOR10) && door->av[0] < 140 && !DEFEATED(door)) { restart_a(door, S_DOOR7); door->fl &= ~F_ARM; }   /* 7441 */
        if (K == 1 && door->av[0] >= 140 && !DEFEATED(door) && !a_is(door, S_DOOR)) { restart_a(door, S_DOOR); door->fl |= F_ARM; }   /* 7442 */
        if (a_done(door, S_DOOR7)) set_a(door, S_DOOR10);                       /* 7443 */
        if (K == 1 && !DEFEATED(door)) door->av[0] += 1;                        /* 7444 */
        if (DEFEATED(door) && !a_is(door, S_DOOR8)) restart_a(door, S_DOOR8);   /* 7445: broken */
        door->hidden = K == 6;                                                  /* 7446 */
        door->av[1] = a_is(door, S_DOOR8) ? 8 : a_is(door, S_DOOR7) ? 7 : a_is(door, S_DOOR10) ? 10 : 0;
    }
    /* 7447-7453: boomerangs */
    int ndb = objects_alive_type(OT_FN_BOOMERANG) ? 0 : 8;                         /* (no scan without a boomerang) */
    for (int i = 0; i < G.nobjs && ndb < 8; i++) {
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_FN_BOOMERANG || b->born == G.frame) continue;
        ndb++;
        b->x = (float)(int)b->av[0]; b->av[0] += b->av[1]; b->av[2] -= fabs(b->av[1]); b->ang = (float)fmod(b->ang + 30, 360);   /* 7447 */
        if (b->av[2] <= 0 && !(b->fl & 0x1000)) { b->fl |= 0x1000; b->av[3] = -2 * b->av[3]; }   /* 7448: back twice as fast */
        b->av[1] = b->av[1] < b->av[3] ? fmin(b->av[1] + 0.2, b->av[3]) : fmax(b->av[1] - 0.2, b->av[3]);   /* 7449 / 7450 */
        int on_dz = b->an.b && d->an.b && obj_obj_hit(b, d);
        if ((b->fl & 0x1000) && on_dz && fabsf(b->x - d->x) <= 29) { b->alive = 0; continue; }   /* 7451: caught */
        if (on_dz && door && a_is(door, S_DOOR8) && door->an.frame >= 4 && obj_obj_hit(b, door)) {   /* 7452 / 7453: through the broken door */
            obj_effect(b->x, b->y, "explosion02"); snd_play_orig(17, 220, 128);
            d->c = 5; d->A -= (b->fl & 0x1000) ? 200 : 100; d->inv = 1; Z.L = fmax(Z.L, 120);
            b->alive = 0;
        }
    }
    /* 7454-7464: defeat */
    if (def && Z.J < 6) { Z.J = 6; Z.L = 0; Z.G = 0; restart_a(d, S_DZ8); Z.N = 0; Z.M = 0; }   /* 7454 */
    if (Z.J == 6 && G.frame % 7 == 0) {                                         /* 7455 */
        snd_play_orig(29, 220, 128);
        obj_effect((float)(d->x + 40 * Z.E - 20 + R(41)), (float)(d->y - 20 - 20 + R(41)), "explosion06");
        Z.L += 1;
    }
    if (Z.J == 6 && Z.L >= 15) {                                                /* 7456 */
        Z.J = 7; Z.Q = -10; Z.G = -6 * Z.E; Z.O = d->y;
        spawn(OT_FN_DOZER_DEBRIS, S_DZDEB, d->x, d->y, d->layer);
        aqua_blast((float)(d->x + 20 * Z.E), (float)(d->y - 24));               /* MonitorIcon frame 9: the shared bomb blast */
        Z.K = 6; Z.P = -3;
        static const int CD[10][2] = { { 30, -50 }, { -30, -50 }, { 20, -40 }, { -20, -40 }, { 10, -50 }, { -3, -44 }, { 30, -20 }, { -30, -20 }, { 10, -20 }, { -10, -20 } };
        int ca = bank_anim(&G.objs, "objectcrystal_3");
        for (int k = 0; k < 10; k++) { obj_t *c = obj_alloc(); if (!c) break; memset(c, 0, sizeof *c);   /* ObjectCrystal (static) */
            c->type = OT_CRYSTAL; c->alive = 1; c->hp = 1; c->layer = 2; c->x = c->ox = d->x + CD[k][0]; c->y = c->oy = d->y + CD[k][1]; if (ca >= 0) anim_set(&c->an, &G.objs, ca); }
    }
    if (Z.J == 7) {
        float x0, y0, x1, y1;
        set_a(d, S_DZ10); d->y = (float)(int)Z.O; Z.O += Z.P;                   /* 7457 */
        ibox(d, &x0, &y0, &x1, &y1);
        int obs = obstacle(d->x, y1);
        if (!obs) Z.P = fmin(Z.P + 0.2, 15);                                    /* 7458 */
        if (obs && !Z.fl4) { snd_play_orig(51, 220, 128); Z.fl4 = 1; zadd(3); } /* 7459 */
        if (Z.fl4 && Z.G != 0) Z.G = Z.G > 0 ? fmax(Z.G - 0.12, 0) : fmin(Z.G + 0.12, 0);   /* 7460 / 7461 */
        if (obs) { Z.P = 0; for (int n = 0; n < 64 && obstacle(d->x, y1 - 1); n++) { d->y -= 1; Z.O -= 1; y1 -= 1; } }   /* 7463: loop_bdfloor (INFERRED: out of the floor) */
        if (Z.G > -1 && Z.G < 1) { Z.J = 8; Z.G = 0; Z.P = 0; }                 /* 7464 */
    }
    if (hb) set_a(hb, Z.J >= 7 ? S_DZHIT8 : S_DZHIT);
    if (d->x > G.lvl.w) d->x = (float)G.lvl.w;                                  /* 7462 */
    d->av[0] = Z.J; d->av[1] = Z.L; d->av[2] = Z.G; d->av[3] = Z.D; d->av[4] = Z.K; d->av[5] = Z.M; d->av[6] = Z.I; d->av[7] = Z.N;
}

/* ======================================================================== PART E: MinibossSamu (FN3 Carol / Milla, FN5)
 * M.st: 0 dormant, 1 intro (0x22668), 2 hover (0x22669), 3 hand smash (0x22658), 4 lasers (0x22667), 5 body charge (0x22656),
 * 6 defeated (0x22665). D / E rig x / y, F / G their speeds, I bob phase, J angle, K state timer, H charge step, N attack
 * rotation, O facing, T defeat counter, Mf the probe's floor reference. Hands: av[0] A x offset, av[1] B y offset, av[2] C angle. */
static struct {
    int body, hand[2], wheel, probe, gate[4], ngate;
    int st, fight, lock, turret_once, fightmus;
    double D, E, F, G, I, J, K, H, N, O, Q, T, Mf, homey;
    int fstep;
} M;
static obj_t *samu(void) { obj_t *o = slot(M.body); return o && o->type == OT_FN_SAMU ? o : NULL; }
static void samu_turret(int x, int y) {                                         /* 8047 / 8048: EnemySuperTurretus, I = -1, H = its y (fn_enemy.c rules) */
    obj_t *t = spawn(OT_FN_STURRET, -1, (float)x, (float)y, 2);
    if (!t) return;
    int a = ens_anim("st_fly_l"); if (a < 0) a = ens_anim("st_fly");
    if (a >= 0) anim_set(&t->an, &G.ens, a);
    t->A = 5; t->av[4] = -1; t->av[3] = y; t->hidden = 1; t->dir = 16;
}
static int samu_wall(const obj_t *b) {                                          /* background or a Q7 object (the gates), or the arena edge (INFERRED) */
    if (bg_overlap(b)) return 1;
    for (int k = 0; k < nsol; k++) { obj_t *s = slot(sol[k]); if (s && s->an.b && !(s->fl & 1) && fabsf(s->x - b->x) < 200 && fabsf(s->y - b->y) < 300 && obj_obj_hit(b, s)) return 1; }
    float x0, y0, x1, y1; ibox(b, &x0, &y0, &x1, &y1);
    if (G.cam.U == 1 && (x0 < G.cam.lC - 8 || x1 > G.cam.lE + 8)) return 1;
    return 0;
}
static void samu_state(int st) { M.st = st; M.K = 0; M.H = 0; if (st == 2) { M.F = 0; M.J = 0; } }
static void samu_frame(player_t *p) {
    obj_t *b = samu();
    if (!b) return;
    const fnboss_t *F = fnb();
    obj_t *h[2] = { slot(M.hand[0]), slot(M.hand[1]) }, *w = slot(M.wheel), *pr = slot(M.probe);
    int lockmode = F && F->samu_lock;
    if (lockmode && !M.lock && !M.T) {                                          /* 7815: between the moved limit markers, once */
        float l = marker(OT_CAMLIM_L, 1), r = marker(OT_CAMLIM_R, 1);
        if (PX(p) > l && PX(p) < r) { M.lock = 1; level_lock_markers(); b->fl &= ~F_ARM; }
    }
    if (M.lock == 1 && PX(p) < marker(OT_CAMLIM_L, 1) + 16 && MVB(p) < 0) setB(p, 0);   /* 7817: the left wall */
    if (!M.fight) {                                                             /* 7816 / 8422: the trigger */
        int g4ok = !F || !F->samu_g4_max || G.at_g4 < F->samu_g4_max;
        if (g4ok && b->x - 239 <= PX(p) && (lockmode || PX(p) <= b->x + 239) && b->y < PY(p) && p->ground && player_orig_m(p) != 48) {
            M.fight = 1; samu_state(1); b->fl &= ~F_ARM;                        /* ML.Y = 1, 0x2265c + intro 0x22668 */
            M.D = b->x; M.E = b->y; M.Q = b->x;                                 /* 7962 (once) */
        }
        if (!M.fight) return;
    }
    if (F && F->samu_cam_d && M.st != 6) {                                      /* 8424 / 8425 (FN5): CAM.D = 336, MV.Y >= CAM.D */
        G.cam_nD = F->samu_cam_d;
        if (PY(p) < F->samu_cam_d) { p->y = F->samu_cam_d + ORIG_REF_DY; if (p->vy < 0) p->vy = 0; }
    }
    if (pr && M.st < 6) {                                                       /* 8051-8053: the probe falls 2 px / frame to the floor */
        if (!bg_overlap(pr) && !samu_wall(pr)) pr->y += 2;
        M.Mf = pr->y - 140;
    }
    double px = PX(p), py = PY(p);
    double O = px >= M.D ? 1 : -1;
    switch (M.st) {
    case 1:                                                                     /* 7965 / 7966: intro (descends into the arena) */
        M.I += 8; M.G -= 0.1;
        if (M.G <= 0) { M.G = 0; samu_state(2); }
        break;
    case 2: {                                                                   /* 7967-7977: hover / choose */
        M.I += 4;                                                               /* (bob speed outside the intro: INFERRED) */
        double ty0 = py - 120, ty1 = py - 80;
        if (M.Mf > 0) { ty0 = fmin(ty0, M.Mf); ty1 = fmin(ty1, M.Mf); }         /* the probe's floor reference (INFERRED use of M) */
        M.G = M.E < ty0 ? 0.5 : M.E > ty1 ? -0.5 : 0;                           /* 7968-7971 */
        M.F = px > M.D ? fmin(M.F + 0.25, 7) : fmax(M.F - 0.25, -7);
        double oF = M.F;
        M.D += M.F; b->x = (float)M.D;
        if (samu_wall(b)) { M.D -= oF; M.F = -oF * 0.5; }                       /* 7972-7974: a wall reverses F */
        M.D -= M.F;                                                             /* (the rig below adds F again) */
        M.K += 1;
        if (M.K >= 90) {                                                        /* 7975-7977: the selector (UNKNOWN U-E1: a rotation, INFERRED) */
            int n = (int)M.N % 3; M.N += 1;
            samu_state(n == 0 ? 5 : n == 1 ? 3 : 4); M.F = 0; M.G = 0; M.O = O;
        }
        break; }
    case 3: {                                                                   /* 7978-8011: hand smash (K timings INFERRED, U-E1) */
        M.K += 1; M.G = 0;
        if (M.K <= 30) { for (int k = 0; k < 2; k++) if (h[k]) h[k]->av[1] = approach(h[k]->av[1], -40, 80.0 / 30); M.F = 0; }
        else if (M.K <= 60) M.F = approach(M.F, sgn(px - M.D) * fmin(fabs(px - M.D) / 8, 4), 0.5);   /* aims over the player */
        else if (M.H == 0) {                                                    /* the slam */
            M.F = 0;
            int down = 0;
            for (int k = 0; k < 2; k++) {
                obj_t *q = h[k]; if (!q) continue;
                if (q->av[1] < 72) { q->av[1] = fmin(q->av[1] + 12, 72); down = 1;
                    if (csens_hit(p, q) && q->y < py && r_hurt(p, 10)) setG(p, -8); }   /* 7993 / 8002: a falling hand above the player */
            }
            if (!down) {
                M.H = 1; G.cam.Z = 6;
                for (int k = 0; k < 2; k++) if (h[k]) { float x0, y0, x1, y1; ibox(h[k], &x0, &y0, &x1, &y1); floor_overlap_rect(x0, y1 - 8, x1, y1 + 24, 2, 0, -6, 0); }   /* floors under the hands: flag 6, Y = -6 */
            }
        } else {
            M.H += 1;
            if (M.H > 30) for (int k = 0; k < 2; k++) if (h[k]) h[k]->av[1] = approach(h[k]->av[1], 40, 2);
            if (M.H > 70) samu_state(2);
        }
        break; }
    case 4:                                                                     /* 8012-8017: lasers from the opened hands (timings INFERRED) */
        M.K += 1; M.F = 0; M.G = 0;
        for (int k = 0; k < 2; k++) if (h[k]) set_a(h[k], k ? S_HAND1_8 : S_HAND0_8);
        if (M.K == 20 || M.K == 45) for (int k = 0; k < 2; k++) {
            if (!h[k]) continue;
            double ax, ay; act_pt(h[k], &ax, &ay);
            obj_t *l = spawn(OT_FN_LASER, S_LASER0, (float)ax, (float)ay, 2);
            if (l) { l->av[0] = 8; l->av[1] = atan2(py - 14 - ay, px - ax) * RAD; }   /* EnemyLaser_1781 (fn_enemy.c: 10 damage) */
            snd_play_orig(192, 220, 128);
        }
        if (M.K >= 70) { for (int k = 0; k < 2; k++) if (h[k]) set_a(h[k], k ? S_HAND1 : S_HAND0); samu_state(2); }
        break;
    case 5:                                                                     /* 8023-8039: body charge */
        M.K += 1;
        if (M.H == 0) {                                                         /* 8023-8027: hands swing back, E tracks the player's y */
            for (int k = 0; k < 2; k++) if (h[k]) h[k]->av[2] = approach(h[k]->av[2], k ? -45 * M.O : 45 * M.O, 3);
            M.G = M.E < py - 22 ? 3 : M.E > py - 6 ? -3 : 0;                   /* (the body's centre at the player's height, INFERRED offset) */
            if (M.K >= 30) { M.H = 1; M.K = 0; M.G = 0; }                       /* 8028 / 8029 (anim 7 is not in the bank: kept) */
        } else if (M.H == 1) { M.H = 2; M.K = 0; }
        else if (M.H == 2) {
            M.G = 0;
            if (M.K > 50 && M.F == 0) { M.F = 15 * M.O; snd_play_orig(61, 220, 128); }   /* 8030: dash at 15 px / frame (sound INFERRED) */
            if (M.F != 0) {
                float x0, y0, x1, y1; ibox(b, &x0, &y0, &x1, &y1);
                floor_overlap_rect(x0, y0, x1, y1, 2, 0, -6, 0);                /* 8031 / 8032 */
                if (csens_hit(p, b) && r_hurt(p, 10)) setG(p, -8);              /* 8033 */
                double oD = M.D; M.D += M.F; b->x = (float)M.D; int wall = samu_wall(b); M.D = oD; b->x = (float)oD;
                if (wall) { M.F = 0; M.H = 3; M.K = 0; snd_play_orig(17, 220, 128); G.cam.Z = 8; }   /* 8034 */
            }
        } else if (M.H == 3) {
            if (M.K > 50) { M.K = 0; M.H = 4; M.F = -3 * M.O; }                 /* 8035 / 8036 */
        } else if (M.H == 4) {
            for (int k = 0; k < 2; k++) if (h[k]) h[k]->av[2] = approach(h[k]->av[2], 0, 3);   /* 8037 / 8038 */
            if (M.K > 30) { M.F = 0; samu_state(2); }                           /* 8039 */
        }
        break;
    case 6:                                                                     /* 8041-8050: the wreck */
        if (M.T < 25) { obj_effect((float)(M.D - 30 + R(61)), (float)(M.E - 30 + R(61)), "explosion06"); explode_snd(); M.T += 1; }   /* 8041 */
        else if (M.T == 25) {                                                   /* 8042 */
            M.T = 26; set_a(b, S_SAMU9); snd_play_orig(38, 220, 128);
            for (int k = 0; k < 5; k++) {                                       /* loop_samu_31: the debris (5 frames, pinball G = 3; directions INFERRED) */
                obj_t *dd = spawn(OT_DEBRIS, S_SDEB, b->x, b->y, b->layer); if (!dd) break;
                force_frame(dd, k); double r = (3 + 2.5 * k) * 11.25;
                dd->vx = (float)(3 * c_cos(r)); dd->vy = (float)(-3 * c_sin(r) - 2);
            }
        } else if (M.T == 26) {
            M.G = fmax(M.G - 0.2, -12);                                         /* 8043: the wreck rises */
            if (b->y < (F ? F->samu_wreck_y : 512)) { M.G = 0; M.T = 27; snd_play_orig(62, 220, 128); }   /* 8045 */
        } else if (M.T == 27) {
            int A = 0;
            for (int k = 0; k < M.ngate; k++) { obj_t *g = slot(M.gate[k]); if (g) { g->A += 1; g->y -= 4; A = (int)g->A; } }   /* 8046 */
            if (F) for (int t = 0; t < 2; t++) {
                const short *tu = F->turret[t];
                if (!tu[2] || (M.turret_once >> t & 1) || A < tu[2]) continue;
                M.turret_once |= 1 << t; samu_turret(tu[0], tu[1]);             /* 8047 / 8048 */
            }
            if (A >= 48 || !M.ngate) {                                          /* 8048 */
                M.T = 28; level_release_camera(0); snd_play_orig(51, 220, 128);
                if (F && F->samu_music) { const char *m = level_music_slot(6); if (m) music_play(m); }
                if (F && F->samu_cam_d) G.cam_nD = 0;                           /* (the arena's top bound released, INFERRED) */
            }
        } else if (M.T > 27 && py < 208) {                                      /* 8049 / 8050 (FN5 8475 / 8476) */
            if (M.T == 28) { G.cam_nF = G.camy + 240; M.T = 29; }
            G.cam_nF = fmaxf(G.cam_nF - 2, 240);
        }
        break;
    }
    if (M.st != 6 && DEFEATED(b)) {                                             /* 8040 / 8039: defeated (shared 2764) */
        samu_state(6); M.F = 0; M.G = 0; M.T = 0; M.J = 0;
        set_a(b, S_SAMU8);
        for (int k = 0; k < 2; k++) if (h[k]) { set_a(h[k], k ? S_HAND1_8 : S_HAND0_8); h[k]->av[0] = k ? 56 : -56; h[k]->av[2] = k ? 45 : -45; }
        if (F && F->samu_music) music_fade(90, 0);                              /* fadeout_music(1500) */
    }
    if (U.ufo >= 0 && !U.music4 && ufo() && U.G < 3 && M.fight) {               /* 7726: ML.Y = 1 with a UFO of G < 3 (FN3; UNKNOWN in the original, trace 10) */
        const char *m = level_music_slot(4); U.music4 = 1; if (m) music_play(m);
    }
    /* 7963 / 7964: rig */
    M.D += M.F; M.E += M.G;
    b->x = (float)(int)M.D; b->y = (float)(int)(M.E + 8 * c_sin(M.I)); b->ang = (float)M.J;
    if (w) { w->x = b->x; w->y = b->y; w->ang = (float)fmod(w->ang + 12, 360); }
    for (int k = 0; k < 2; k++) {
        obj_t *q = h[k]; if (!q) continue;
        q->x = (float)(int)(M.D + q->av[0]); q->y = (float)(int)(M.E + q->av[1] + 8 * c_sin(M.I - 60)); q->ang = (float)q->av[2];
    }
    if (M.st != 6 && G.frame % 10 == 0) {                                       /* 7964: 32-direction facing (the bank has 0, 16, 24) */
        double a = nrm(atan2(b->y - py, px - b->x) * RAD); int d = (int)(a / 11.25) & 31;
        int dd0 = abs(d - 0) < 16 ? abs(d) : 32 - d, dd16 = abs(d - 16), dd24 = abs(d - 24) < 16 ? abs(d - 24) : 32 - abs(d - 24);
        set_a(b, dd0 <= dd16 && dd0 <= dd24 ? S_SAMU0 : dd16 <= dd24 ? S_SAMU16 : S_SAMU24);
    }
    b->av[0] = M.st; b->av[1] = M.T; b->av[2] = M.K; b->av[3] = M.H; b->av[4] = M.F; b->av[5] = M.E; b->av[6] = M.lock; b->av[7] = M.fight;
}

/* ======================================================================== PART D: Serpentine (FN3, Lilac only)
 * S.st: the state group (0 none, 1 dash 0x2266c, 2 shoot 0x22666, 3 flame jump 0x22660, 4 missile 0x22663, 5 shield 0x22659,
 * 6 stagger 0x2266b); B sub-step, C / D speeds, E shot counter, F facing, G timer, H arm mode, I attack selector, J ground
 * y, L re-engage count; fl7 leaving in the UFO; dfl defeated (0x22661); esc the UFO escape (0x22655, UFO G). */
enum { SS_NONE, SS_DASH, SS_SHOOT, SS_FLAME, SS_MISSILE, SS_SHIELD, SS_STAGGER };
static struct {
    int serp, arm, hit, probe, shield, glow;
    int armed, fight, lock, music, st, fl7, dfl, esc, fl1, fl3, sh_phase, snd197, ctrl;
    double B, C, D, E, F, G, H, I, J, L, armA, armB;
    double camA, camB;
} S;
static obj_t *serp(void) { obj_t *o = slot(S.serp); return o && o->type == OT_FN_SERP ? o : NULL; }
static obj_t *shit(void) { obj_t *o = slot(S.hit); return o && o->type == OT_FN_SERP_HIT ? o : NULL; }
static void serp_anim(obj_t *s, int k) {                                         /* the facing set (direction H / 16 + H: the bank keeps 0 / 16) */
    static const int PAIR[][2] = { { S_FS0, S_FS0L }, { S_FS6, S_FS6L }, { S_FS7, S_FS7L }, { S_FS8, S_FS8L }, { S_FS9, S_FS9L }, { S_FS15, S_FS15L }, { S_FS16, S_FS16L }, { S_FS14, S_FS14L } };
    for (unsigned j = 0; j < sizeof PAIR / sizeof PAIR[0]; j++) if (PAIR[j][0] == k) { k = S.F < 0 && SA[PAIR[j][1]] >= 0 ? PAIR[j][1] : k; break; }
    if (SA[k] < 0) return;
    if (s->an.b == &G.ens && s->an.anim != SA[k]) {                             /* a facing change keeps the frame (set_direction) */
        for (unsigned j = 0; j < sizeof PAIR / sizeof PAIR[0]; j++)
            if ((s->an.anim == SA[PAIR[j][0]] || s->an.anim == SA[PAIR[j][1]]) && (k == PAIR[j][0] || k == PAIR[j][1])) {
                int fr = s->an.frame, acc = s->an.acc; anim_set(&s->an, &G.ens, SA[k]); s->an.frame = fr; s->an.acc = acc; return; }
    }
    set_a(s, k);
}
static int serp_in(const obj_t *s, int k) { return a_is(s, k) || (k == S_FS0 && a_is(s, S_FS0L)) || (k == S_FS6 && a_is(s, S_FS6L)) || (k == S_FS7 && a_is(s, S_FS7L)) ||
                                                   (k == S_FS8 && a_is(s, S_FS8L)) || (k == S_FS9 && a_is(s, S_FS9L)) || (k == S_FS15 && a_is(s, S_FS15L)) || (k == S_FS16 && a_is(s, S_FS16L)) || (k == S_FS14 && a_is(s, S_FS14L)); }
static void serp_restart(obj_t *s, int k) { serp_anim(s, k); anim_restart(&s->an); }
static void boss_action_reset(void) {                                            /* loop_boss_action_31 */
    S.st = SS_NONE; S.B = 0; S.G = 0;
    obj_t *g = slot(S.glow); if (g) { g->x = -128; g->y = 0; g->hidden = 1; }
}
static void serp_state(int st) { boss_action_reset(); S.st = st; }
static void serp_stagger(void) {                                                 /* 7917-7920: boss_action reset, 0x2266b, B 0, sounds 25 / 196 */
    serp_state(SS_STAGGER); snd_play_orig(25, 220, 128); snd_play_orig(196, 220, 128);
}
static void shield_park(void) { obj_t *sh = slot(S.shield); if (sh) { sh->hidden = 1; sh->x = -80; sh->y = 0; } }
static void face_player(const obj_t *s, const player_t *p) { if (PX(p) > s->x + 20) S.F = 1; else if (PX(p) < s->x - 20) S.F = -1; }
static void glow_at(const obj_t *s, double scale, int alpha) {
    obj_t *g = slot(S.glow); if (!g) return;
    double ax, ay; act_pt(s, &ax, &ay); g->x = (float)ax; g->y = (float)ay; g->hidden = 0; g->scale = (float)scale; g->av[0] = alpha;
}
static void kill_projectiles(void) {                                            /* 7866: warnings, rapid bullets, boss projectiles */
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i];
        if (o->alive && (o->type == OT_FN_WARNING || o->type == OT_FN_WARNING2 || o->type == OT_FN_RAPIDBULLET || o->type == OT_FN_MISSILE || o->type == OT_FLAME ||
                         (o->type == OT_FN_LASER && o->dir == 3))) o->alive = 0; }
}
static void serp_probe(const obj_t *s) {                                         /* 7901-7903: the floor check falls 4 px / frame onto a backdrop / Floor2 */
    obj_t *pr = slot(S.probe);
    if (!pr || !s) return;
    pr->x = s->x;
    int over = bg_overlap(pr) || floor_overlap_rect(pr->x - 8, pr->y - 28, pr->x + 8, pr->y + 28, 0, 0, 0, 0);
    if (!over) pr->y += 4;
    else { pr->y -= 4; if (!(bg_overlap(pr) || floor_overlap_rect(pr->x - 8, pr->y - 28, pr->x + 8, pr->y + 28, 0, 0, 0, 0))) pr->y += 4; }   /* (rises out of a higher floor, INFERRED) */
    S.J = pr->y;                                                                 /* Serpentine J = the probe's y */
}
static float beam_x(void) { obj_t *b = beam(); return b ? b->x : -1e9f; }
static void serp_escape(player_t *p, obj_t *s) {                                 /* 7930-7960: the UFO escape (0x22655, UFO G) */
    obj_t *u = ufo(), *b = beam(), *deb = slot(U.deb);
    if (!u) return;
    if (U.G >= 3) serp_probe(s);                                                 /* (0x2265d's floor check keeps following him: J under the crash) */
    if (U.G == 0) {                                                              /* 7930 */
        U.G = 1; U.fl1 = 1; ufo_layer(2);
        G.cam.U = 0; G.cam_nC = 0; G.cam_nD = 0; G.cam_nE = (float)G.lvl.w; G.cam_nF = (float)G.lvl.h;
    }
    if (U.G < 4) U.E = fmod(U.E + 4 + U.N, 360);                                 /* 7923 + 7934 */
    ufo_rig();
    if (U.G == 1) {                                                              /* 7931 / 7932 */
        U.D = fmax(U.D - 2, -90);
        double ty = S.J - 64; if (u->y < ty) u->y = (float)fmin(u->y + 2, ty); else if (u->y > ty) u->y = (float)fmax(u->y - 2, ty);
        S.camA += (u->x + 100 - S.camA) * 0.1; S.camB += (u->y - S.camB) * 0.1;
        if (U.D == -90 && u->y == ty) { U.G = 2; set_a(u, S_UFO7); snd_play_orig(200, 220, 128); U.fl13 = G.frame; }
    } else if (U.G == 2) {                                                       /* 7933-7935 */
        if (U.fl13 > 0 && G.frame - U.fl13 == 60) snd_play_orig(199, 220, 128);  /* once 198 has ended (its length: INFERRED 60 frames) */
        U.N += 0.05;
        if (U.N >= 10) { U.G = 3; S.ctrl = 1; snd_play_orig(169, 220, 128); U.bvis = 1; U.bA = 6; G.cam.Z = 8; }
    }
    if (U.G < 3 && p->x > marker(OT_CAMLIM_R, 1)) p->x = marker(OT_CAMLIM_R, 1);   /* 7936 */
    if (U.G == 3) {                                                              /* 7937-7940 */
        u->x += 15; if (s) s->x = u->x;
        if (p->x < u->x + 100) { p->x = u->x + 100; player_sync_sensors(p); }
        if (MVB(p) < 10) setB(p, fmin(fmax(MVB(p), 0) + 0.5, 15));               /* the auto-run (CTRL.D 1; the view-relative bands INFERRED as a floor of 10) */
        S.camA += (u->x + 200 - S.camA) * 0.1; S.camB += (u->y + 40 - S.camB) * 0.1;
        for (int k = 0; k < ncols; k++) {
            obj_t *c = slot(cols[k]); if (!c) continue;
            c->fl |= 1;                                                          /* flag 0: not solid */
            if ((c->par & 1) && !(c->fl & 0x1000) && c->an.b && u->an.b && obj_obj_hit(u, c)) { c->fl |= 0x1000; G.cam.Z = 8; snd_play_orig(317, 220, 128); }   /* col_24_31 */
        }
        if (u->x > 13904) {                                                      /* 7943: the crash */
            set_a(u, S_UFO8); U.G = 4;
            if (deb) { deb->x = u->x; deb->y = u->y; deb->A = 0; deb->hidden = 0; }
            for (int k = 0; k < 4; k++) { obj_t *q = slot(U.piece[k]); if (q) q->av[2] = -3 - q->A; }
            if (s) { s->hidden = 0; s->x = u->x; s->y = u->y; S.F = 1; serp_restart(s, S_FS8); S.D = 3; s->layer = 2; }
            U.L = 0; music_fade(36, 0); snd_play_orig(51, 220, 128);             /* fadeout_music(600) */
        }
    }
    for (int k = 0; k < ncols; k++) {                                            /* 7941 / 7942: a struck LargeColumn2 topples */
        obj_t *c = slot(cols[k]);
        if (!c || !(c->fl & 0x1000) || (c->fl & 0x2000)) continue;
        c->av[0] -= 3; c->ang = (float)fmax(c->av[0], -105);
        if (c->av[0] <= -105) { c->fl |= 0x2000; snd_play_orig(75, 220, 128); G.cam.Z = 8; }
    }
    if (U.G >= 4 && U.G <= 5) {                                                  /* 7944 / 7945 */
        u->x += 15;
        for (int k = 0; k < 4; k++) { obj_t *q = slot(U.piece[k]); if (q) { q->y += (float)q->av[2]; q->x -= 4; q->av[2] += 0.3; } }
        obj_t *h1 = slot(U.hatch[0]), *h2 = slot(U.hatch[1]);
        if (h2) { h2->x = u->x; h2->y = u->y; }
        if (h1 && deb) { h1->x = deb->x; h1->y = deb->y; }
        U.bA -= 1; if (U.bA <= 1) U.bvis = 0;
        if (s && s->y < S.J) { S.D = fmin(S.D + 0.25, 12); s->y = (float)fmin(s->y + S.D, S.J); }
        if (deb) { deb->A += 0.2; deb->y += (float)deb->A; }
        S.camB -= 1;
    }
    if (U.G == 4 && deb) {                                                       /* 7946 / 7947 */
        obj_effect((float)(deb->x - 40 + R(81)), deb->y, "explosion06"); explode_snd();
        U.L += 1; if (U.L >= 32) U.G = 5;
    }
    if (U.G >= 4 && U.G <= 5 && s && s->y >= S.J) serp_anim(s, S_FS16);         /* 7948 */
    if (U.G < 6 && p->x > 14230) p->x = 14230;                                   /* 7949 */
    if (U.G == 5 && deb && deb->y >= S.J + 24) {                                 /* 7950 */
        for (int k = 0; k < 10; k++) obj_effect((float)(deb->x - 40 + R(81)), (float)(deb->y - 30 + R(61)), "explosion01");
        snd_play_orig(51, 220, 128); U.G = 6; S.ctrl = 2; deb->A = 0;
    }
    if (U.G == 6 && s) {                                                         /* 7951-7953: the player walks back toward him */
        if (p->x >= s->x + 100) setB(p, -3);
        else { S.ctrl = 3; if (MVB(p) == 0 || p->ground) { setB(p, 0); U.G = 7; } }
    }
    if (U.G == 7) {                                                              /* 7954 (7955: Adventure voice, not ported) */
        S.camA += 3;
        if (p->x <= S.camA) {                                                    /* 7958 */
            U.G = 8; S.ctrl = 0;
            for (int k = 0; k < nbars; k++) { obj_t *br = slot(bars[k]); if (br) br->x = 13948; }
            G.cam.Z = 0;
            for (int k = 0; k < 4; k++) { obj_t *q = slot(U.piece[k]); if (q) q->hidden = 1; }
            { obj_t *h2 = slot(U.hatch[1]); if (h2) h2->hidden = 1; }
            G.cam.M3 = 0; G.cam_m2 = 1;
            { const char *m = level_music_slot(6); if (m) music_play(m); }       /* play_music(6) = m_stage3c */
            u->y = -100; S.fight = 2;
        }
    }
    if (U.G == 8) { p->invuln = 0; G.cam_nF = 248; }                             /* 7956-7960 */
    if (U.G >= 1 && U.G < 8) { G.cam.M3 = 1; G.cam.A = (float)S.camA; G.cam.B = (float)S.camB; }
    if (b && U.G >= 3) { b->x = u->x; b->y = u->y; }
}
static void rapid_new(const obj_t *s) {                                          /* 7877 / 7878 */
    double ax, ay; act_pt(s, &ax, &ay);
    obj_t *r = spawn(OT_FN_RAPIDBULLET, S_RB8, (float)ax, (float)ay, 2);
    if (!r) return;
    int e = (int)S.E % 2; r->av[2] = e + 2; r->av[3] = 2 * e - 1; S.E += 1;     /* C = E mod 2 + 2, D = 2 (E mod 2) - 1 */
    snd_play_orig(194, 220, 128);
}
static void serp_frame(player_t *p) {
    obj_t *s = serp(), *hb = shit(), *arm = slot(S.arm), *pr = slot(S.probe), *sh = slot(S.shield), *u = ufo(), *b = beam();
    if (!s || !hb) return;                                                       /* Lilac only: the flow removes the hitbox set for the others (8083) */
    float LL = marker(OT_CAMLIM_L, 1), LR = marker(OT_CAMLIM_R, 1);
    if (!S.lock && PX(p) > LL && PX(p) < LR && !S.fight) { S.lock = 1; level_lock_markers(); }   /* 7811 */
    if (S.lock == 1 && S.fight < 2 && PX(p) < LL + 16 && MVB(p) < 0) setB(p, 0); /* 7813 */
    if (!S.fight) {                                                              /* 7812 (Classic: no dialog) */
        int hit = 0;
        for (int k = 0; k < ntrig && !hit; k++) { obj_t *t = slot(trig[k]); if (t && fabsf(t->x - s->x) < 600) hit = csens_rect(p, t->x, t->y, t->x + 16, t->y + 288); }   /* 8082: y-scale 1.5 */
        if (!hit) return;
        S.fight = 1;
    }
    if (!S.music && u && U.G < 3) { S.music = 1; U.music4 = 1; const char *m = level_music_slot(4); if (m) music_play(m); }   /* 7726: m_midboss */
    if (!S.armed) {                                                              /* 7819 / 7820 (once) */
        S.armed = 1; S.armA = -90; S.J = s->y; s->hidden = 0;
        U.act = 0;                                                               /* group 0x225d4 off (the chase) */
        if (u) {
            u->x = LL + 95; u->y = marker(OT_CAMLIM_L, 0) - 152;                 /* LimitLeft + (95, -152) = (9967, 8) */
            ufo_start(); U.D = 0; U.G = 0; U.K = 45; U.H = 0; U.F = 0;
        }
        if (b && u) { U.bvis = 1; U.bA = 12; b->layer = 1; b->x = u->x; b->y = u->y; }   /* the beam: a wall column under the UFO (y-scale INFERRED 12) */
        for (int k = 0; k < nbars; k++) { obj_t *br = slot(bars[k]); if (br && u) br->x = u->x; }   /* the arena's left wall */
        hb->c = 0; hb->A = 135; hb->av[0] = 4; hb->fl |= F_NOFLY;
        S.B = 0; S.F = -1; boss_action_reset(); S.st = SS_DASH;                  /* 7820 */
    }
    if (u && U.G == 0) { u->fl |= F_ARM; u->c = 5; }                             /* 7367 (G stays 0 until the escape) */
    if (S.fight == 2) return;                                                    /* after 7958 he stays where he fell */
    /* the UFO rig (7921-7929) until the escape takes over */
    if (!S.esc && u) {
        ufo_rig(); if (U.G < 4) U.E = fmod(U.E + 4, 360);
        if (U.G == 0) { u->y = (float)(int)(U.I + 10 * c_sin(U.J)); U.J += 2; }
        if (b) { b->x = u->x; b->y = u->y; }
        if (U.bvis && b && fabs(PX(p) - b->x) <= 31 && r_hurt(p, 10)) setB(p, 8);   /* 7929: the beam wall hurts and pushes right */
        if (S.dfl) { U.K = fmin(U.K + 2, 45); U.bvis = 0; }                      /* 7867 */
    }
    if (S.esc) { serp_escape(p, s); return; }
    serp_probe(s);                                                               /* 7901-7903 */
    /* 7904-7910: arm and hitbox */
    if (S.F == 1) S.armA = fmax(S.armA, serp_in(s, S_FS9) ? -150 : -90); else S.armA = fmin(S.armA, serp_in(s, S_FS9) ? -30 : -90);
    double ax, ay; act_pt(s, &ax, &ay);
    if (arm) {
        int H = (int)S.H; static const int AS[2][3] = { { S_SARM1, S_SARM2, S_SARM3 }, { S_SARM17, S_SARM18, S_SARM19 } };
        if (H >= 1 && H <= 3) set_a(arm, AS[S.F < 0][H - 1]);
        arm->x = (float)ax; arm->y = (float)ay; arm->ang = (float)(floor(S.armA / 10) * 10);
        arm->hidden = S.H <= 0 || a_is(s, S_FS12) || a_is(s, S_FS13) || s->hidden;
        arm->layer = s->layer;
    }
    hb->x = s->x; hb->y = s->y;
    if (u && U.G <= 0 && s->x > LR) s->x = LR;                                   /* 7909 */
    if (!S.dfl && S.B == 0 && !S.fl7 && S.st != SS_DASH && S.st != SS_FLAME && fabs(s->x - PX(p)) > 400) {   /* 7911 / 7912 */
        S.F = PX(p) > s->x ? 1 : -1; S.L += 1; serp_state(SS_DASH);
    }
    if (S.L == 3 && hb->A > 0 && !S.snd197) { S.L = 4; S.snd197 = 1; snd_play_orig(197, 220, 128); }   /* 7913 */
    hb->fl |= F_ARM;                                                             /* 7914 */
    if (!sh || sh->hidden) hb->fl &= ~F_ARM;                                     /* 7915: vulnerable while the shield is down */
    s->inv = hb->c > 1 && !(hb->fl & (F_ARM | F_DEF));                          /* 7916 */
    if (arm) arm->inv = s->inv;
    int down = !(hb->fl & F_ARM), def = DEFEATED(hb) != 0;
    if (!def && S.st != SS_STAGGER) {
        if (player_orig_m(p) == 5 && hb->c > 0 && down && csens_hit(p, hb)) { hb->A -= 10; serp_stagger(); }   /* 7917: Lilac's Dragon Boost */
        else if (down && b && U.bvis && s->x <= beam_x() + 31) serp_stagger();   /* 7918: the beam wall */
    }
    if (!def && hb->A < 30 * hb->av[0]) {                                        /* 7919 / 7920 */
        S.I += 1; serp_stagger(); hb->av[0] -= 1; shield_park();
    }
    /* 7866: defeated */
    if (S.B == 0 && def && !S.dfl) {
        S.dfl = 1; snd_play_orig(198, 220, 128); p->invuln = 9999; kill_projectiles();
        if (S.st != SS_DASH) serp_state(SS_DASH);
        S.camA = G.cam.A; S.camB = G.cam.B;
    }
    if (S.dfl) {                                                                 /* 7867 / 7869-7872 */
        G.cam.M3 = 1; S.camA += (s->x - S.camA) * 0.1; S.camB += (s->y - S.camB) * 0.1; G.cam.A = (float)S.camA; G.cam.B = (float)S.camB;
        if (u && !S.fl7) {
            S.F = u->x > s->x ? 1 : -1;                                          /* (dashing to his ship: INFERRED from 7869 / 7870) */
            serp_anim(s, S_FS9);
            if (fabs(s->x - u->x) <= 8) { S.B = 0; force_frame(s, 0); S.fl7 = 1; S.G = 0; s->layer = 1; if (arm) arm->layer = 1; }
            else s->x = (float)(int)(s->x + approach(0, (u->x - s->x), 10));
            s->y = (float)(int)S.J;
        }
        if (S.fl7 && u) {
            S.C = 0; s->y -= 4; set_a(s, S_FS9U);                               /* 7871: rises (direction 8; speed INFERRED) */
            if (s->y < u->y) { s->hidden = 1; if (arm) arm->hidden = 1; S.st = SS_NONE; S.esc = 1; U.G = 0; }   /* 7872 */
        }
        goto mirror;
    }
    switch (S.st) {
    case SS_DASH:                                                                /* 7853-7865 */
        if (S.B == 0) {
            serp_anim(s, S_FS9); face_player(s, p); serp_anim(s, S_FS9);
            if (s->an.frame > 3) { snd_play_orig(61, 220, 128); obj_effect((float)(s->x - 20 * S.F), s->y, "wburst"); S.C = 10 * S.F; S.B = 1; S.G = 0; }
        } else if (S.B == 1) {
            s->x = (float)(int)(s->x + S.C); s->y = (float)(int)S.J; S.G += 1;
            int past = (S.F > 0 && s->x > PX(p) + 80) || (S.F < 0 && s->x < PX(p) - 80);
            int edge = (S.F < 0 && s->x <= beam_x() + 48) || (S.F > 0 && s->x >= LR - 24) || bg_overlap(s);
            if ((S.G > 15 && (past || edge)) || S.G > 90) { S.B = 2; serp_restart(s, S_FS15); }   /* 7859 / 7860 (the OR's exact terms: INFERRED) */
        } else {
            S.C = approach(S.C, 0, 0.5); s->x = (float)(int)(s->x + S.C); s->y = (float)(int)S.J;
            if (S.C == 0) {
                if (S.I == 2) { serp_state(SS_FLAME); S.F = -S.F; S.I = 3 + R(2); S.G = R(2); }   /* 7863 */
                else if (S.I >= 5) { serp_state(SS_SHIELD); S.H = 0; serp_restart(s, S_FS12); S.I = R(5); S.F = -S.F; }   /* 7864 */
                else serp_state(SS_SHOOT);                                       /* 7865 */
            }
        }
        break;
    case SS_SHOOT:                                                               /* 7822-7835 */
        s->y = (float)(int)S.J; face_player(s, p);
        if (S.B == 0) {
            int ahead = obstacle((float)(s->x + 24 * S.F), (float)(s->y - 16));
            int ramp = 0; for (int i = 0; i < G.nobjs && !ramp; i++) { obj_t *r = &G.o[i]; if (r->alive && r->type == OT_FN_RAMP && pr && r->an.b && fabsf(r->x - pr->x) < 160 && obj_obj_hit(pr, r)) ramp = 1; }
            if (ahead || ramp) { serp_state(SS_FLAME); S.G = 2; break; }         /* 7825-7827 */
            if (fe_difficulty == 0 && fabs(PX(p) - s->x) > 300) { serp_state(SS_MISSILE); break; }   /* 7828: Hard only */
            glow_at(s, 1, 0); S.B = 1; S.G = 0; serp_anim(s, S_FS0); S.H = -1; snd_play_orig(191, 220, 128);   /* 7829 */
        } else if (S.B == 1) {
            S.G += 1; glow_at(s, fmax(1 - 0.04 * S.G, 0.1), (int)fmin(255, 10 * S.G));   /* 7830 */
            if (S.G >= 25) { S.B = 2; S.E = 0; serp_restart(s, S_FS6); obj_t *g = slot(S.glow); if (g) g->hidden = 1; }   /* 7831 / 7832 */
        } else {
            serp_anim(s, S_FS6);
            if (s->an.frame == 1 && !S.fl1) {                                    /* 7833 / 7834: a 3-shot fan */
                S.fl1 = 1;
                obj_t *l = spawn(OT_FN_LASER, S_LASER3, (float)ax, (float)ay, 2);
                if (l) { l->dir = 3; l->av[0] = 16; l->av[1] = (S.F - 1) * -90 - 3 * S.E * S.F; }
                snd_play_orig(192, 220, 128); S.E += 1;
            }
            if (s->an.frame != 1) S.fl1 = 0;
            if (S.E >= 3 && s->an.frame == 4) { serp_state(SS_DASH); serp_anim(s, S_FS0); S.H = 0; S.I += 1; }   /* 7835 */
        }
        break;
    case SS_FLAME:                                                               /* 7836-7852 */
        if (S.B == 0) {
            if (S.H != 1) { snd_play_orig(70, 220, 128); S.armA = -90; }        /* 7836 */
            serp_restart(s, S_FS7); S.H = 1; S.B = 0.5;                          /* 7837 */
        } else if (S.B == 0.5) {
            if (s->an.frame >= 2) { S.B = 1; S.D = -5; S.C = 2 * S.F; S.J = s->y; snd_play_orig(8, 220, 128); }   /* 7840 */
        } else if (S.B == 1) {
            s->x = (float)(int)(s->x + S.C); s->y = (float)(s->y + S.D); S.D = fmin(S.D + 0.2, 8);   /* 7841-7843 */
            S.C = approach(S.C, 2 * S.F, 0.1);
            if (S.H == 1 && (G.frame & 1) == 0 && arm) {                         /* 7845: a flame per arm every 2nd frame */
                double fx, fy; act_pt(arm, &fx, &fy);
                double a = S.armA + 10 * c_sin(S.armB);
                relic_flame_new((float)fx, (float)fy, 6 * c_cos(a), -6 * c_sin(a));
                S.armB = fmod(S.armB + 30, 360);
            }
            double lo = S.F > 0 ? -50 : -230, hi = S.F > 0 ? 50 : -130;          /* 7846-7848: the sweep */
            S.armA += S.fl3 ? -5 : 5;
            if (S.armA >= hi) { S.armA = hi; S.fl3 = 1; } else if (S.armA <= lo) { S.armA = lo; S.fl3 = 0; }
            if (S.D > 0 && s->y >= S.J) {                                        /* 7849-7852: landing */
                s->y = (float)S.J;
                if (S.H == 1 && S.G < 2) { S.G += 1; S.B = 3; }
                else { serp_state(SS_DASH); S.H = 0; }
            }
        } else {                                                                 /* B 3..20: the pause before the second hop */
            s->y = (float)S.J; S.B += 1;
            if (S.B > 20) S.B = 0;
        }
        break;
    case SS_MISSILE:                                                             /* 7884-7888 (Hard) */
        s->y = (float)(int)S.J;
        if (S.B == 0) { S.B = 1; S.H = 2; S.armA = -90; serp_anim(s, S_FS0); }
        else if (S.B == 1) {
            double t = S.F > 0 ? 0 : -180; S.armA = approach(S.armA, t, 5);
            if (S.armA == t) {
                serp_restart(s, S_FS14); S.H = 3; S.B = 2;
                double mx, my; if (arm) act_pt(arm, &mx, &my); else { mx = ax; my = ay; }
                obj_t *m = spawn(OT_FN_MISSILE, S.F > 0 ? S_MIS : S_MISL, (float)mx, (float)my, 2);
                if (m) { m->av[4] = S.F; m->av[0] = mx; m->av[1] = my; }
                snd_play_orig(195, 220, 128);
            }
        } else if (s->an.done || !serp_in(s, S_FS14)) { serp_state(SS_DASH); S.H = 0; }
        break;
    case SS_SHIELD:                                                              /* 7873-7882 */
        s->y = (float)(int)S.J;
        if (S.B == 0) {
            snd_play_orig(193, 220, 128); S.B = 1; S.sh_phase = 0; S.E = 0;      /* 7874 / 7875 */
            set_a(s, S_FS12); if (arm) arm->hidden = 1;
            if (sh) sh->hidden = 0;
        }
        if (sh) { sh->x = s->x; sh->y = s->y - 28; }
        if (S.sh_phase == 0 && a_done(s, S_FS12)) { restart_a(s, S_FS13); S.sh_phase = 1; }   /* 7876 */
        if (S.sh_phase == 1 && a_is(s, S_FS13)) {
            int f = s->an.frame;
            if ((f == 5 || f == 8) && !S.fl1) { S.fl1 = 1; rapid_new(s); }       /* 7877 / 7878 */
            if (f != 5 && f != 8) S.fl1 = 0;
            if (s->an.done) { restart_a(s, S_FS12); S.sh_phase = 2; }
        }
        if (p->state == ST_HURT && fabs(PX(p) - s->x) < 30 && sgn(MVB(p)) == sgn(s->x - PX(p))) setB(p, -MVB(p));   /* 7880 */
        if (S.sh_phase == 2 && a_done(s, S_FS12)) {                              /* 7881 */
            serp_anim(s, S_FS0); shield_park(); serp_state(SS_SHOOT);
        }
        break;
    case SS_STAGGER:                                                             /* 7892-7900 */
        if (S.B == 0) { face_player(s, p); serp_restart(s, S_FS8); S.D = -4; S.C = -4 * S.F; S.B = 1; }
        else {
            s->x = (float)(s->x + S.C); s->y = (float)(s->y + S.D); S.D += 0.25;
            if (S.C < 0 && s->x <= beam_x() + 31) { S.C = -S.C; S.F = -S.F; }   /* bouncing off the beam wall */
            if (S.D > 0 && s->y >= S.J) { s->y = (float)S.J; serp_state(SS_DASH); S.G = 0; }
        }
        break;
    default: break;
    }
mirror:
    if (sh && !sh->hidden && csens_hit(p, sh) && r_hurt(p, 5)) setG(p, -8);      /* 8062: the shield */
    s->av[0] = S.st; s->av[1] = S.B; s->av[2] = S.I; s->av[3] = S.G; s->av[4] = S.F; s->av[5] = S.dfl; s->av[6] = S.esc ? U.G : -1; s->av[7] = S.J;
}
static void serp_projectiles(player_t *p) {                                      /* 8054-8072 (outside the groups) */
    if (!objects_alive_type(OT_FN_RAPIDBULLET) && !objects_alive_type(OT_FN_MISSILE) && !objects_alive_type(OT_FN_WARNING)) return;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->born == G.frame) continue;
        if (o->type == OT_FN_RAPIDBULLET) {
            if (o->av[0] == 0) {                                                 /* 8054 / 8055 */
                o->y -= 16; o->av[1] += 1;
                if (o->av[1] > 50) {
                    o->av[0] = 1; o->av[1] = 0; o->x += (float)(16 * o->av[2] * o->av[3]); set_a(o, S_RB24);
                    obj_t *w = spawn(OT_FN_WARNING, S_WARN, o->x, (float)(S.J + 50), 2);
                    if (w) w->tm = 0;
                }
            } else {
                o->y += 16;                                                      /* 8058 */
                if (o->y > 0 && bg_overlap(o)) { obj_effect(o->x, o->y, "explosion02"); snd_play_orig(45, 220, 128); o->alive = 0; }   /* 8059 */
                else if (o->y > G.lvl.h + 64) o->alive = 0;
            }
        } else if (o->type == OT_FN_WARNING) {                                   /* 8056 / 8057 / 8060 / 8061 */
            if (bg_overlap(o)) { o->y -= 32; o->hidden = 1; } else if (o->tm < 3) { o->y += 32; o->hidden = 0; }
            if (++o->tm >= 30) o->alive = 0;
        } else if (o->type == OT_FN_MISSILE) {                                   /* 8063-8072 */
            if (!(o->fl & F_DEF)) {
                double F = o->av[4];
                o->av[2] = F > 0 ? fmin(o->av[2] + 0.5, 12) : fmax(o->av[2] - 0.5, -12);
                o->av[3] = 0; if (o->y < PY(p) + 4) o->av[3] = 1; if (o->y > PY(p) - 4) o->av[3] = o->y < PY(p) + 4 ? o->av[3] : -1;   /* homes vertically */
                o->x = (float)(int)o->av[0]; o->y = (float)(int)o->av[1]; o->av[0] += o->av[2]; o->av[1] += o->av[3];
                float LL = marker(OT_CAMLIM_L, 1), LR = marker(OT_CAMLIM_R, 1);
                if (o->x < LL - 32 || o->x > LR + 32) o->alive = 0;
            } else {
                if (o->tm < 45) {
                    double E = o->tm;
                    obj_effect((float)(o->x + (8 + 40 * c_sin(2 * E)) * c_sin(132 * E)), (float)(o->y - 80 * c_sin(5 * E)), "explosion06");
                    if (o->tm < 30 && csens_rect(p, o->x - 48, o->y - 90, o->x + 48, o->y + 16) && p->shield != 2 && r_hurt(p, 10)) setG(p, -8);   /* 8069 (blast box INFERRED) */
                    o->tm++;
                } else o->alive = 0;
            }
        }
    }
}

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&U, sizeof U); snap_register(&Z, sizeof Z); snap_register(&M, sizeof M); snap_register(&S, sizeof S); } }
void fn_boss_spawn(void) {
    snap_once();
    memset(&U, 0, sizeof U); memset(&Z, 0, sizeof Z); memset(&M, 0, sizeof M); memset(&S, 0, sizeof S);
    U.ufo = U.beam = U.deb = U.hatch[0] = U.hatch[1] = -1; for (int k = 0; k < 4; k++) U.piece[k] = -1;
    Z.dz = Z.arm = Z.claw = Z.door = Z.hit = Z.wheel[0] = Z.wheel[1] = -1;
    M.body = M.hand[0] = M.hand[1] = M.wheel = M.probe = -1;
    S.serp = S.arm = S.hit = S.probe = S.shield = S.glow = -1;
    marks[0] = marks[1] = marks[2] = marks[3] = -1;
    on = 0; nfloors = nlamps = ntrig = nsol = ncols = nbars = 0;
    for (int t = OT_FNBOSS_FIRST; t <= OT_FNBOSS_LAST && !on; t++) if (objects_have(t)) on = 1;
    if (!on) return;
    for (int k = 0; k < S_N; k++) SA[k] = (short)ens_anim(SN[k]);
    /* lvobj anim per object: the k-th object of a type is the k-th placement of that type (objects_spawn keeps the order) */
    static uint16_t AN[OT_FNBOSS_LAST - OT_FNBOSS_FIRST + 1][8]; int na[OT_FNBOSS_LAST - OT_FNBOSS_FIRST + 1] = { 0 }, seen[OT_FNBOSS_LAST - OT_FNBOSS_FIRST + 1] = { 0 };
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - OT_FNBOSS_FIRST; if (t >= 0 && t <= OT_FNBOSS_LAST - OT_FNBOSS_FIRST && na[t] < 8) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    int np = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if (t == OT_FN_BFLOOR && nfloors < 96) floors[nfloors++] = (short)i;
        if (t == OT_FN_LAMP && nlamps < NL) lamps[nlamps++] = (short)i;
        if (t == OT_CAMTRIGGER && ntrig < 8) trig[ntrig++] = (short)i;
        if (t >= OT_CAMLIM_L && t <= OT_CAMLIM_B && marks[t - OT_CAMLIM_L] < 0) marks[t - OT_CAMLIM_L] = (short)i;
        if (t == OT_FN_COLUMN && ncols < 8) cols[ncols++] = (short)i;
        if (t == OT_WR_BARRIER && nbars < 8) bars[nbars++] = (short)i;
        if (t < OT_FNBOSS_FIRST || t > OT_FNBOSS_LAST) continue;
        int ti = t - OT_FNBOSS_FIRST, an = seen[ti] < na[ti] ? AN[ti][seen[ti]] : 0; seen[ti]++;
        o->par = an & 0xff; o->layer = (an >> 8) & 0xff; o->st = 0; o->tm = 0; o->c = 0; o->fl = 0; o->dir = 0; o->ang = 0; o->scale = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL; o->bd = 0;
        switch (t) {
        case OT_FN_UFO: U.ufo = i; set_a(o, S_UFO); o->A = 200; U.D = -90; break;   /* ctor A 200, D -90 */
        case OT_FN_UFO_PIECE: if (np < 4) U.piece[np++] = i; set_a(o, S_UFOP); break;
        case OT_FN_UFO_HATCH: U.hatch[o->par & 1] = i; set_a(o, (o->par & 1) ? S_HATCH1 : S_HATCH0); break;
        case OT_FN_UFO_BEAM: U.beam = i; set_a(o, S_BEAM); o->A = 1; break;
        case OT_FN_UFO_BORDER: if (U.nborder < 4) U.border[U.nborder++] = (short)i; set_a(o, S_BORDER); break;
        case OT_FN_UFO_DEBRIS: U.deb = i; set_a(o, S_UFODEB); o->hidden = 1; break;
        case OT_FN_DOZER: Z.dz = i; set_a(o, S_DZ); o->A = 200; o->fl |= F_NOFLY; Z.E = 1; Z.M = -1; Z.I = o->x; break;   /* ctor + 7479. E: the spec
             * reads the ctor as -1, but every part's placement (wheel 1 at X - 36, wheel 2 at X + 41, the exhaust pipe on the left,
             * the arm's action point on the right of the direction-0 image) matches the 7433 formulas with E = +1: the arm then rises
             * when D goes to 90 and the boomerang leaves forward (INFERRED; UNKNOWN until trace T-B1) */
        case OT_FN_DOZER_ARM: Z.arm = i; set_a(o, S_DZARM); break;
        case OT_FN_DOZER_CLAW: Z.claw = i; set_a(o, S_DZCLAW); break;
        case OT_FN_DOZER_DOOR: Z.door = i; set_a(o, S_DOOR); force_frame(o, 4); o->A = 15; o->fl |= F_NOFLY; break;   /* 7479: frame 5 forced */
        case OT_FN_DOZER_WHEEL: Z.wheel[o->par & 1] = i; set_a(o, (o->par & 1) ? S_WHEEL1 : S_WHEEL0); break;
        case OT_FN_DOZER_HITBOX: Z.hit = i; set_a(o, S_DZHIT); break;
        case OT_FN_LIMITER: if (Z.nlim < 4) Z.lim[Z.nlim++] = i; set_a(o, S_LIM); break;
        case OT_FN_SERP: S.serp = i; set_a(o, S_FS0L); break;
        case OT_FN_SERP_ARM: S.arm = i; set_a(o, S_SARM1); break;
        case OT_FN_SERP_HIT: S.hit = i; set_a(o, S_SHIT); o->A = 135; o->av[0] = 4; o->fl |= F_NOFLY; break;   /* ctor A 135, D 4 */
        case OT_FN_SERP_FLOOR: S.probe = i; set_a(o, S_SPROBE); break;
        case OT_FN_SERP_SHIELD: S.shield = i; set_a(o, S_SSHIELD); break;
        case OT_FN_CHARGE_GLOW: S.glow = i; set_a(o, S_GLOW); break;
        case OT_FN_SAMU: M.body = i; set_a(o, S_SAMU0); o->A = 90; o->fl |= F_NOFLY; M.G = 8; M.O = 1; M.homey = o->y; break;   /* ctor A 90, G 8, O 1 */
        case OT_FN_SAMU_HAND: M.hand[o->par & 1] = i; set_a(o, (o->par & 1) ? S_HAND1 : S_HAND0); o->av[0] = (o->par & 1) ? 56 : -56; o->av[1] = 40; break;
        case OT_FN_SAMU_WHEEL: M.wheel = i; set_a(o, S_SWHEEL); break;
        case OT_FN_SAMU_PROBE: if (o->y >= 0) { M.probe = i; set_a(o, S_SPROBE2); } else o->alive = 0; break;   /* FN5's is an off-map template */
        case OT_FN_BOSSGATE: if (M.ngate < 4) M.gate[M.ngate++] = i; set_a(o, (o->par & 1) ? S_GATE1 : S_GATE0); if (nsol < NL) sol[nsol++] = (short)i; break;
        }
    }
    obj_t *sm = samu();
    if (sm) { M.D = sm->x; M.E = sm->y; if (level_cur()->fnb && level_cur()->fnb->samu_lock) sm->fl |= F_ARM; }   /* 8083: armoured until the lock */
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && obj_in_group(G.o[i].type, OBJGRP_SOLID) && (G.o[i].type == OT_FN_COLUMN) && nsol < NL) sol[nsol++] = (short)i;
}
void fn_boss_pre(player_t *p) {                                                  /* test_collisions handlers */
    if (!on) return;
    obj_t *u = ufo();
    if (u && U.act && U.G >= 2 && U.G < 5 && u->an.b) {                         /* col_11_29: the UFO bounces off a border */
        for (int k = 0; k < U.nborder; k++) { obj_t *bd = slot(U.border[k]); if (bd && bd->an.b && fabsf(bd->x - u->x) < 160 && obj_obj_hit(u, bd)) { U.H = -U.H; break; } }
    }
    if (Z.act && objects_alive_type(OT_FN_BOOMERANG)) for (int i = 0; i < G.nobjs; i++) {   /* col_7_29 + the backdrop handler */
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_FN_BOOMERANG || b->born == G.frame) continue;
        int bg = bg_overlap(b), was = (b->fl & 0x2000) != 0;
        if (bg) b->fl |= 0x2000; else b->fl &= ~0x2000;
        if ((csens_hit(p, b) && r_hurt(p, 10)) || (bg && !was)) { obj_effect(b->x, b->y, "explosion02"); snd_play_orig(17, 220, 128); b->alive = 0; }
    }
    if (objects_alive_type(OT_FN_RAPIDBULLET) || objects_alive_type(OT_FN_MISSILE)) for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->born == G.frame) continue;
        if (o->type == OT_FN_RAPIDBULLET && !dead(p) && p326_hit(o)) {           /* col_15_31: 1 damage, no invulnerability test (RE) */
            snd_play_orig(45, 220, 128); obj_effect(o->x, o->y, "explosion02"); o->alive = 0;
            if (!p->hurt_pending) player_contact(p, 1);
        } else if (o->type == OT_FN_MISSILE && !(o->fl & F_DEF) && p326_hit(o)) {   /* col_11_31 */
            o->fl |= F_DEF; snd_play_orig(38, 220, 128); zadd(6); o->hidden = 1; o->tm = 0;
        }
    }
}
void fn_boss_frame(player_t *p) {
    if (!on) return;
    const fnboss_t *F = fnb();
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_FNB")) printf("fnb %d U %d bC %.0f lC %.0f bE %.0f lE %.0f camx %.0f\n", G.frame, G.cam.U, G.cam.bC, G.cam.lC, G.cam.bE, G.cam.lE, G.camx);
#endif
    if (shit()) serp_frame(p);                                                  /* 7811-7960 (its ship is the UFO) */
    if (!S.armed && ufo()) { if (F && F->chase_x1) ufo_chase(p); else ufo_a1(p); }
    if (dozer()) dozer_frame(p);
    if (samu()) samu_frame(p);
    serp_projectiles(p);
    obj_t *u = ufo();
    if (u) { u->av[0] = U.G; u->av[1] = U.H; u->av[2] = U.D; u->av[3] = U.K; u->av[4] = U.L; u->av[5] = U.bA; u->av[6] = U.act; }
    obj_t *b = beam(); if (b) { b->hidden = !U.bvis; b->av[0] = U.bA; b->av[1] = U.bfl1; b->av[2] = U.bdir; }
}
int fn_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }           /* every rule runs in fn_boss_frame */
int fn_boss_q20(int t) { return t == OT_FN_UFO || t == OT_FN_DOZER || t == OT_FN_DOZER_DOOR || t == OT_FN_SERP_HIT || t == OT_FN_SAMU; }
int fn_boss_input(uint32_t *held) {                                              /* CTRL.M = 1 during Serpentine's escape (7934-7958) */
    if (!on || !S.ctrl) return 0;
    *held = S.ctrl == 1 ? BTN_RIGHT : 0;                                         /* CTRL.D: auto-run right; CTRL.C: the walk back is scripted (7951-7953) */
    return 1;
}

/* ---- drawing: the UFO rig in its draw order (lights behind the hull while sin B < 0), the y-scaled beam, the charge glow */
static void draw_one(const obj_t *o, uint32_t col) {
    if (!o || !o->alive || !o->an.b || o->hidden) return;
    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
    if (sx < -400 || sx > SCREEN_W + 400 || sy < -400 || sy > SCREEN_H + 400) return;
    if (o->inv) r_set_invert(1);
    if (o->scale != 0 && o->scale != 1.f) r_set_scale(o->scale);
    frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, 0, col);
    r_set_scale(1.f);
    if (o->inv) r_set_invert(0);
}
int fn_boss_draw_obj(const obj_t *o, int pass) {
    (void)pass;
    switch (o->type) {
    case OT_FN_UFO_PIECE: case OT_FN_UFO_HATCH: return 1;                        /* drawn with the hull */
    case OT_FN_UFO: {
        for (int k = 0; k < 4; k++) { const obj_t *q = slot(U.piece[k]); if (q && (q->fl & 0x1000) && q->layer == o->layer) draw_one(q, 0xffffffff); }
        draw_one(o, 0xffffffff);
        for (int k = 0; k < 2; k++) { const obj_t *q = slot(U.hatch[k]); if (q && q->layer == o->layer) draw_one(q, 0xffffffff); }
        for (int k = 0; k < 4; k++) { const obj_t *q = slot(U.piece[k]); if (q && !(q->fl & 0x1000) && q->layer == o->layer) draw_one(q, 0xffffffff); }
        return 1; }
    case OT_FN_UFO_BEAM: {
        if (o->hidden || !o->an.b || U.bA <= 0) return 1;
        const frame_t *f = anim_frame(&o->an);
        float h = (float)(f->h * U.bA), sx = floorf(o->x - G.camx) - f->hx, sy = floorf(o->y - G.camy) - f->hy;
        if (sx > SCREEN_W || sx + f->w < 0 || sy > SCREEN_H || sy + h < 0) return 1;
        frame_draw_scaled(o->an.b, f, sx, sy, (float)f->w, h, 0xffffffff);
        return 1; }
    case OT_FN_CHARGE_GLOW: {
        if (o->hidden) return 1;
        int a = (int)o->av[0]; if (a <= 0) return 1;
        r_set_additive(1); draw_one(o, ((uint32_t)(a > 255 ? 255 : a) << 24) | 0xffffff); r_set_additive(0);
        return 1; }
    }
    return 0;
}
