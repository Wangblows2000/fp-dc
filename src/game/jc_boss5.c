/* Jade Creek act 5 (session 21): the finale. Spec: build/re/spec_jc_boss5.md (RE handle_frame_44_events / on_frame_44_start,
 * RV build/orig_traces/jc_boss5). Labels: RE read from code, RV measured in the original, INFERRED reasoned.
 *
 * What Jade Creek's last act plays in the Classic route (Lilac, Carol, Milla; RE + RV l_tp / l_kill / l_kill2):
 *   - start: Neera's ice ambush (Hovercop + NPCNeera, group 0x22afe, 11043-11072): jc_boss.c's M_AMBUSH (session 22; it opens the
 *     barrier at x 1536);
 *   - x 5800: the Tredd miniboss (engine types 73-87);
 *   - x > Syntax - 1200: loop_cutscene_start_43 -> the Brevon mech flyby scene with Syntax and Torque (0x22aff; g1 == 5 =
 *     Torque would get the Brevon mech FIGHT 0x22af4 instead: not playable, not ported);
 *   - the arena (camera trigger x 13560): Serpentine 2 = FN3's fight (fn_boss.c, hitbox 2552 variant: A 180, D 6), RV l_kill;
 *   - his defeat: petals, he is knocked off the cliff (0x22af5, fn_boss.c) and comes back in the GUNSHIP (0x22b05, this file);
 *   - the gunship's defeat: the partner finisher, explosions in slow motion, the white flash, the three heroes and the results
 *     (0x221a1 -> Classic 10941 -> the next stage; Adventure 10942 / 10943 opens a dialog instead).
 * This file: the gunship rig and attacks (11284-11343), its projectiles (11396-11417), the Elite troopers (10954-11041), Milla's
 * box delivery (11344 / 11345), the end (11346-11372: finisher hit, slow motion, crash, flash, the three heroes, the results), the
 * act's lightning (11618-11622). Order in the frame (RE): Elites (10954-11041) before the bosses, then the fall check (11283), the
 * gunship block, its projectiles. Performance (SH4): per-type instance lists built once at spawn; runtime objects (flares,
 * missiles, Elites, lasers) live in a small slot table; a level without these types returns at once. */
#include "jc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RADC 57.29577951308232
#define F_DEF 0x40
#define F_ARM 0x20
static double nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static double c_sin(double deg) { double r = nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / RADC); }   /* Chowdren degree trig */
static double c_cos(double deg) { double r = nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / RADC); }
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }

/* ---- sprites (tools/import_frame.py JC_BOSS5 rows), looked up once per level */
enum { GS_SHIP, GS_BLADEL, GS_BLADER, GS_CANL, GS_CANL7, GS_CANR, GS_CANR7, GS_HIT, GS_HIT8, GS_LIGHT, GS_LL, GS_LL1, GS_LR, GS_LR1,
       GS_RACKL, GS_RACKR, GS_PILOT, GS_PILOT5, GS_PLAT, GS_TRP, GS_TRP_L, GS_TURBL, GS_TURBL8, GS_TURBR,
       GS_FLARE, GS_FLARE10, GS_FLARE11, GS_FLARE11L, GS_MIS, GS_MISL, GS_WARN2,
       GE_WALK, GE_JUMP, GE_HURT, GE_CHARGE, GE_FIRE, GE_GLOW, GE_LASER, GS_BOLT, GS_BOLTL, GH_LILAC, GH_CAROL, GH_MILLA, GS_N };
static const char *const SN[GS_N] = { "j5g_ship", "j5g_bladel", "j5g_blader", "j5g_canl", "j5g_canl7", "j5g_canr", "j5g_canr7", "j5g_hit", "j5g_hit8",
    "j5g_light", "j5g_ll", "j5g_ll1", "j5g_lr", "j5g_lr1", "j5g_rackl", "j5g_rackr", "j5g_pilot", "j5g_pilot5", "j5g_plat", "j5g_trp", "j5g_trp_l",
    "j5g_turbl", "j5g_turbl8", "j5g_turbr", "j5g_flare", "j5g_flare10", "j5g_flare11", "j5g_flare11_l", "fb_mis", "fb_mis_l", "fb_warn2",
    "j5e_walk", "j5e_jump", "j5e_hurt", "j5e_charge", "j5e_fire", "j5e_glow", "j5e_laser", "j5_bolt", "j5_bolt_l", "fc_l37l", "fc_c37l", "fc_m37" };
static short SA[GS_N];
static void set_a(obj_t *o, int k) { if (o && k >= 0 && SA[k] >= 0 && (o->an.b != &G.ens || o->an.anim != SA[k])) anim_set(&o->an, &G.ens, SA[k]); }
static void restart_a(obj_t *o, int k) { if (o && k >= 0 && SA[k] >= 0) { anim_set(&o->an, &G.ens, SA[k]); anim_restart(&o->an); } }
static int a_is(const obj_t *o, int k) { return o && k >= 0 && SA[k] >= 0 && o->an.b == &G.ens && o->an.anim == SA[k]; }
static void force_frame(obj_t *o, int f) { if (!o || !o->an.b) return; int n = o->an.b->anims[o->an.anim].count; o->an.frame = f < 0 ? 0 : f < n ? f : n - 1; o->an.acc = 0; }

/* ---- per-type instance lists (built once at spawn, in placement order) */
#define NT (OT_J5_LAST - OT_JCB5_FIRST + 1)
#define LMAX 8
typedef struct { short n, i[LMAX]; } list_t;
static list_t L[NT];
static int on;                                       /* the level places the gunship */
static short mark_b = -1;                            /* CameraLimitBossBottom */
static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static obj_t *get(int type, int k) { const list_t *l = &L[type - OT_JCB5_FIRST]; return k >= 0 && k < l->n ? slot(l->i[k]) : NULL; }
static int count(int type) { return L[type - OT_JCB5_FIRST].n; }
/* runtime objects of this module (flares, missiles, Elites and their masks, lasers, glows, the bolt, the heroes): their slots,
 * so the per-frame rules walk this short table instead of every object */
#define RTMAX 48
static short RT[RTMAX]; static int nrt;
static obj_t *spawn(int type, int sk, float x, float y, int layer) {
    if (nrt >= RTMAX) return NULL;
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    set_a(o, sk);
    RT[nrt++] = (short)(o - G.o);
    return o;
}
static void rt_compact(void) {                                                   /* drop the dead (and reused) slots */
    int k = 0;
    for (int i = 0; i < nrt; i++) { const obj_t *o = slot(RT[i]); if (o && o->type >= OT_JCB5_FIRST && o->type <= OT_J5_LAST) RT[k++] = RT[i]; }
    nrt = k;
}
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }   /* MV.R 0, OTHERS.E <= 0 */
static int csens_hit(const player_t *p, const obj_t *o) {                       /* PlayerCharacterSensor_375 */
    if (!o || !o->alive || !o->an.b) return 0;
    int X = (int)PX(p), Y = (int)PY(p), y0, y1;
    if (player_char_sensor_small(p)) { y0 = Y - 13; y1 = Y + 15; } else { y0 = Y - 34; y1 = Y + 13; }
    return obj_rect_hit(o, X - 7, y0, X + 9, y1);
}
static void hurt_g(player_t *p, int n) {                                         /* MV.R 1, LIFE.B += n, MV.G -8 */
    if (!vulnerable(p)) return;
    player_contact(p, n);
    p->ground = 0; p->vy = -8;
}
static int near_border(const obj_t *o, int b) {                                  /* FrameObject::is_near_border(b) (RE: Engine facts) */
    if (!o->an.b) return 1;
    const frame_t *f = anim_frame(&o->an);
    float x0 = o->x - f->hx, y0 = o->y - f->hy, x1 = x0 + f->w, y1 = y0 + f->h;
    return !(x0 >= G.camx + b && x1 <= G.camx + SCREEN_W - b && y0 >= G.camy + b && y1 <= G.camy + SCREEN_H - b);
}

/* ======================================================================== the gunship (0x22b05)
 * BossGunship alterables (RE ctor + 11651 + 11283): D / E the centre (D = the player's X at the start), F tilt (degrees), G 40 / H 20
 * the bob amplitudes, J x speed, K steering (-1 / 0 / 1), L attack state (1 flares, 2 none, 3 missiles, 4 troopers), M state timer,
 * N sub counter, O attack cycle (-1 -> 0..3), P the placed y (the hover reference: E P - 260, P - 360 in state 4). */
static struct {
    int on, intro, active, proj, ended, results, slow0;      /* groups 0x22b05 / 0x22b03 / 0x22b06 / 0x22b01 */
    double D, E, F, G, H, J, K, Lst, M, N, O, P;
    double turbD[2];                                          /* turbine D: the flare charge counter */
    double rackD[2], rackE[2], rackF[2];                      /* missile racks: D slide, E missiles left (frame), F timer */
    double platA, platB, platC, platD;                        /* platform: A cable, B lowering, C clock, D Elites dropped */
    double hitE, hitD, hitF;                                  /* hitbox: E debris burst, D damage stage, F the end clock */
    int once34;                                               /* 0x22b34 */
    double crashV;                                            /* L 9 after the slow motion: the fall speed (RV l_kill2 3427-3570) */
    int closing, heroes, fade, boxT;                          /* 11368 (0x22b35), 11369 (0x22b44), the white card's opacity, delivery clock */
    double skyC; int boltT;                                   /* LevelDarkSky C (11618-11620), the bolt's frames left */
} GN;
static obj_t *gun(void) { return get(OT_J5_GUN, 0); }
static obj_t *ghit(void) { return get(OT_J5_GUN_HIT, 0); }

/* 11283: Serpentine has fallen 800 px below the gunship: it starts (D = the player's X, L 4, the camera's bottom bound at P - 70,
 * sound 427, voice serp_kungfu; Serpentine's objects destroyed; Serpentine's projectile rules off, the gunship's on) */
static void gunship_start(player_t *p) {
    obj_t *g = gun();
    GN.on = 1; GN.intro = 1; GN.proj = 1;
    GN.D = PX(p); GN.Lst = 4;
    fn_boss_serp_remove();
    obj_t *mb = slot(mark_b);
    if (mb) { mb->y = (float)(GN.P - 70); level_lock_markers(); }
    snd_play_orig(427, 220, 128);
    voice_play("serp_kungfu");
    (void)g;
}

/* 11286: the rig. x = D + G sin(g3), y = E + H sin(2 g3) + (g3 / 2) & 1, angle F; parts at their offsets rotated by F
 * (Fusion: CCW in y-down: x' = dx cos F + dy sin F, y' = -dx sin F + dy cos F). g3 = G.frame (INFERRED phase). */
static void rot(double dx, double dy, double F, double *x, double *y) { *x = dx * c_cos(F) + dy * c_sin(F); *y = -dx * c_sin(F) + dy * c_cos(F); }
static void place(obj_t *o, double gx, double gy, double dx, double dy, double F) {
    if (!o) return;
    double x, y; rot(dx, dy, F, &x, &y);
    o->x = (float)(int)(gx + x); o->y = (float)(int)(gy + y); o->ang = (float)F;
}
static void rig(void) {
    obj_t *g = gun();
    if (!g) return;
    int g3 = G.frame;
    GN.D += GN.J;
    int gx = (int)(GN.D + GN.G * c_sin(g3)), gy = (int)(GN.E + GN.H * c_sin(2 * g3) + ((g3 / 2) & 1));
    g->x = (float)gx; g->y = (float)gy; g->ang = (float)GN.F;
    double F = GN.F;
    place(get(OT_J5_GUN_TURBL, 0), gx, gy, -148, 0, F);
    place(get(OT_J5_GUN_TURBR, 0), gx, gy, 148, 0, F);
    for (int s = 0; s < 2; s++) {                                                /* cannons: their placed offsets D / E */
        obj_t *c = get(s ? OT_J5_GUN_CANR : OT_J5_GUN_CANL, 0);
        if (c) place(c, gx, gy, c->av[0], c->av[1], F);
    }
    static const int LT[3] = { OT_J5_GUN_LIGHTL, OT_J5_GUN_LIGHT, OT_J5_GUN_LIGHTR };
    for (int t = 0; t < 3; t++) for (int k = 0; k < count(LT[t]); k++) { obj_t *l = get(LT[t], k); if (l) place(l, gx, gy, l->av[0], l->av[1], F); }
    place(get(OT_J5_GUN_PLAT, 0), gx, gy, 0, GN.platA, F);
    place(get(OT_J5_GUN_RACKL, 0), gx, gy, -GN.rackD[0], 0, F);
    place(get(OT_J5_GUN_RACKR, 0), gx, gy, GN.rackD[1], 0, F);
    force_frame(get(OT_J5_GUN_RACKL, 0), (int)GN.rackE[0]); force_frame(get(OT_J5_GUN_RACKR, 0), (int)GN.rackE[1]);
    obj_t *pl = get(OT_J5_GUN_PLAT, 0);
    for (int k = 0; k < count(OT_J5_GUN_TROOPER); k++) {                          /* troopers ride the platform (x = gun x + A) */
        obj_t *t = get(OT_J5_GUN_TROOPER, k);
        if (t) { t->x = (float)(gx + (int)t->av[0]); t->y = pl ? pl->y : (float)gy; }
    }
    for (int s = 0; s < 2; s++) {                                                /* blades at the turbines, x-scale 1 + 0.8 sin(70 g3) (flicker) */
        obj_t *b = get(s ? OT_J5_GUN_BLADER : OT_J5_GUN_BLADEL, 0), *t = get(s ? OT_J5_GUN_TURBR : OT_J5_GUN_TURBL, 0);
        if (b && t) { b->x = t->x; b->y = t->y; b->ang = (float)F; b->scale = (float)(1 + 0.8 * c_sin(70 * g3)); }
    }
    obj_t *pi = get(OT_J5_GUN_PILOT, 0); if (pi) { pi->x = (float)gx; pi->y = (float)gy; pi->ang = (float)F; }
    obj_t *h = ghit(); if (h) { h->x = (float)gx; h->y = (float)gy; h->ang = (float)F; }
}

static obj_t *flare_new(const obj_t *t) {                                        /* 11305 / 11309: a flare from a turbine */
    obj_t *f = spawn(OT_J5_GUN_FLARE, GS_FLARE, t->x, t->y, 2);
    if (f) { f->av[0] = 0; f->av[1] = 0; }
    return f;
}
static void missile_new(const obj_t *rack, int right, double E) {                 /* 11316 / 11320 */
    obj_t *m = spawn(OT_J5_GUN_MISSILE, GS_MIS, (float)(rack->x + (right ? -14 : 14) * E), rack->y, 2);
    if (!m) return;
    m->av[0] = m->x; m->av[1] = m->y; m->av[2] = 10; m->av[5] = 90; m->av[6] = E;   /* A, B position, C speed 10, F angle 90 (up), G = E */
    if (right) m->fl |= 2;                                                       /* flag 1: the right rack's (turns the other way) */
    snd_play_orig(195, 220, 128);
}
/* 11330 / 11331: a trooper jumps off the platform as an Elite (its mask: G = y, D -1, O 0, I -5, Q 1; flag 16), white burst,
 * platform D + 1, sound 233 (RV) */
static void elite_drop(obj_t *t);

static void gunship_frame(player_t *p) {
    obj_t *g = gun(), *h = ghit();
    if (!g) return;
    {   obj_t *mb = slot(mark_b); if (mb) G.cam.bF = mb->y; }                    /* 11284: CAM.F = the bottom marker (RV view top 410) */
    if (GN.D > G.lvl.w - 200) { GN.D = G.lvl.w - 200; if (GN.J > 0) GN.J = 0; }   /* 11285 */
    rig();                                                                       /* 11286 / 11287 */
    if (GN.F > 0) GN.F -= 0.25;                                                  /* 11288 / 11289 */
    if (GN.F < 0) GN.F += 0.25;
    if (GN.intro) {                                                              /* 0x22b03: 11290 / 11291 rise to P - 260 */
        GN.E -= 6;
        if (GN.E <= GN.P - 260) { GN.intro = 0; GN.active = 1; }
    }
    int def = h && (h->fl & F_DEF);
    if (GN.active) {                                                             /* 0x22b06 */
        GN.K = 0;                                                                /* 11292-11298: steer toward the player (+-50 dead zone) */
        if (PX(p) + 50 < GN.D) GN.K = -1;
        if (GN.D < PX(p) - 50) GN.K = 1;
        if (GN.K == -1) { GN.J = fmax(GN.J - 0.1, -12); GN.F = fmin(GN.F + 0.4, 10); }
        if (GN.K == 1) { GN.J = fmin(GN.J + 0.1, 12); GN.F = fmax(GN.F - 0.4, -10); }
        if (GN.K >= 0 && GN.J < 0) GN.J = fmin(GN.J + 0.2, 0);
        if (GN.K <= 0 && GN.J > 0) GN.J = fmax(GN.J - 0.2, 0);
        if (GN.Lst == 0 && !def) { GN.O = fmod(GN.O + 1, 4); GN.M = 0; GN.N = 0; GN.Lst = 1 + GN.O; }   /* 11299 */
        if (GN.Lst == 4 && GN.E > GN.P - 360) { GN.E -= 2; GN.M = 0; }           /* 11300 */
        if (GN.Lst != 4 && GN.Lst != 9 && GN.E < GN.P - 260) { GN.E += 2; GN.M = 0; }   /* 11301 */
    }
    if (GN.Lst == 9 && GN.hitF >= 1060 && GN.hitF < 1200 && GN.E < GN.P + 713) {   /* L 9 after the slow motion: it falls (RV l_kill2: */
        GN.crashV += 0.1; GN.E = fmin(GN.E + GN.crashV, GN.P + 713);           /* +0.1 a frame, 468 -> 1441 in 143 frames) */
    }
    GN.M += 1;                                                                   /* (the state clock) */
    /* L 1: the turbines charge (lights by D / 10) and drop a flare at D > 40, recoil F -/+ 6, sound 67; twice, then rest */
    if (GN.Lst == 1 && GN.M == 1) GN.turbD[0] = 1;                               /* 11302 */
    if (GN.Lst == 1 && GN.M == 100) GN.turbD[1] = 1;                             /* 11306 */
    for (int s = 0; s < 2; s++) {
        if (GN.turbD[s] > 0) GN.turbD[s] += 1;                                   /* 11303 / 11307 */
        obj_t *t = get(s ? OT_J5_GUN_TURBR : OT_J5_GUN_TURBL, 0);
        int lt = s ? OT_J5_GUN_LIGHTR : OT_J5_GUN_LIGHTL;
        for (int k = 0; k < count(lt); k++) { obj_t *l = get(lt, k); if (l) set_a(l, (GN.turbD[s] > 0 && (int)(GN.turbD[s] / 10) >= (int)l->av[2]) ? (s ? GS_LR1 : GS_LL1) : (s ? GS_LR : GS_LL)); }   /* 11304 / 11308 */
        if (GN.turbD[s] > 40 && t) { flare_new(t); GN.turbD[s] = 0; GN.F += s ? 6 : -6; snd_play_orig(67, 220, 128); }   /* 11305 / 11309 */
    }
    if (GN.Lst == 1 && GN.M >= 200 && GN.N < 2) { GN.N += 1; GN.M = 0; }        /* 11310 */
    if (GN.Lst == 1 && GN.N >= 2 && GN.M > 320) GN.Lst = 0;                      /* 11311 */
    if (GN.Lst == 2) GN.Lst = 0;                                                 /* 11312 */
    /* L 3: the racks slide out and fire five missiles each, every 20 frames (left from F > 50, right from F > 160) */
    if (GN.Lst == 3 && GN.N == 0) { GN.rackE[0] = GN.rackE[1] = 5; GN.rackF[0] = GN.rackF[1] = 1; GN.N = 1; }   /* 11313 */
    for (int s = 0; s < 2; s++) {
        obj_t *r = get(s ? OT_J5_GUN_RACKR : OT_J5_GUN_RACKL, 0);
        if (GN.rackF[s] > 0 && GN.rackF[s] <= 300) { GN.rackF[s] += 1; GN.rackD[s] = fmin(GN.rackD[s] + 4, 65); }   /* 11314 / 11319 */
        if (r && GN.rackF[s] > (s ? 160 : 50) && GN.rackE[s] > 0 && G.frame % 20 == 0) { missile_new(r, s, GN.rackE[s]); GN.rackE[s] -= 1; }   /* 11316 / 11320 */
        if (GN.rackF[s] > 300) { GN.rackD[s] = fmax(GN.rackD[s] - 4, -8); if (GN.rackD[s] <= -8) GN.rackF[s] = 0; }   /* 11317 / 11318 / 11321 / 11322 */
    }
    if (GN.Lst == 3 && GN.M > 300) GN.Lst = 0;                                   /* 11323 */
    /* L 4: up at P - 360, the troopers show and the platform lowers 96 px on its cables, a trooper jumps off at C 40 and 140 */
    obj_t *pl = get(OT_J5_GUN_PLAT, 0);
    if (GN.Lst == 4 && GN.N == 0 && GN.E <= GN.P - 360 && GN.active) {          /* 11325 */
        GN.platB = 1;
        for (int k = 0; k < count(OT_J5_GUN_TROOPER); k++) { obj_t *t = get(OT_J5_GUN_TROOPER, k); if (t && t->av[3] == 0) t->hidden = 0; }
        GN.N = 1;
    }
    if (GN.platB == 1) {
        if (GN.platA == 0) snd_play_orig(165, 220, 128);                         /* 11326: the cable sound (RV 1609) */
        GN.platA = fmin(GN.platA + 2, 96);                                       /* 11327 */
        if (GN.platA == 94) snd_play_orig(166, 220, 128);                        /* 11328 (stop 165, RV 1655) */
        if (GN.platA >= 96) GN.platC += 1;                                       /* 11329 */
        for (int k = 0; k < count(OT_J5_GUN_TROOPER); k++) {                     /* 11330 / 11331: the left one at C 40, the right one at C 140 */
            obj_t *t = get(OT_J5_GUN_TROOPER, k);
            if (t && !t->hidden && t->av[3] == 0 && ((GN.platC == 40 && t->av[0] < 0) || (GN.platC == 140 && t->av[0] > 0))) elite_drop(t);
        }
        if (GN.platC == 40) GN.platC = 41;
        if (GN.platC == 140) GN.platC = 141;
        if (GN.platC > 180) { GN.platC = 0; GN.platB = 0; }                      /* 11332 */
    } else GN.platA = fmax(GN.platA - 2, 0);                                     /* 11333 */
    if (GN.Lst == 4 && GN.N >= 1 && GN.platB == 0 && GN.platA == 0 && GN.M > 30) {   /* 11334: back to the cycle, the troopers re-armed (INFERRED terms) */
        GN.Lst = 0;
        for (int k = 0; k < count(OT_J5_GUN_TROOPER); k++) { obj_t *t = get(OT_J5_GUN_TROOPER, k); if (t) { t->hidden = 1; t->av[3] = 0; } }
    }
    (void)pl;
    /* the hitbox (Q20, A 300, D 2): the cockpit overlay, frame 1 / 0 while hit / 3 - D when damaged; a stage per 100 HP (debris,
     * sound 134); not below 220 HP for Carol / Milla (11340), the partner finishes it (11346-11352) */
    if (h) {
        force_frame(h, 1); h->av[7] = 1;                                         /* 11335 (shader 0x20: additive) */
        if (h->c > 0 && !def) force_frame(h, 0);                                 /* 11336 */
        if (h->c == 0 && GN.hitD < 2) { force_frame(h, (int)(3 - GN.hitD)); h->hidden = 0; }   /* 11337 */
        if (!def && h->A > 0 && h->A < GN.hitD * 100) { GN.hitE = 5; GN.hitD -= 1; snd_play_orig(134, 220, 128); }   /* 11338 (the
                                                                                    * killing hit: the defeat flag comes with it in the original, RV D -1 after) */
        if (GN.hitE > 0) { obj_effect((float)(h->x + 6 * GN.hitE - 15), h->y, "debris"); GN.hitE -= 1; }   /* 11339 (debris: INFERRED look) */
        if (G.character != CHR_LILAC && h->A >= 220) h->A = 220;                 /* 11340: a cap for Carol / Milla (RV c_gun 1624: 300 -> 220) */
        if (G.character == CHR_MILLA && h->A >= 50) { h->A = 50; GN.hitD = 0; }  /* 11341: Milla's gunship has 50 (D 0: the last stage) */
        if (def && !GN.once34) { GN.once34 = 1; GN.hitE = 5; GN.hitD -= 1; snd_play_orig(134, 220, 128); snd_play_orig(38, 220, 128); }   /* 11342 (RV 2996: 38, 23, 134) */
        if (def) { set_a(h, GS_HIT8); h->hidden = 0; GN.hitF += 1; }            /* 11343 */
    }
}

/* ======================================================================== the Elite troopers (10954-11041; spec E)
 * The mask (EnemyEliteMask_2528, Q20, A 20) is the body: it falls, walks toward the player and is what gets hit; the Elite
 * sprite (EnemyElite_2529) is drawn at it. INFERRED where the spec says so. */
/* mask alterables here: av[0] H x speed, av[1] I y speed, av[2] N direction, av[3] D state (0 ground, -1 air, -2 knocked,
 * 2 attack), av[4] T attack clock, av[5] Q aggro, av[6] K attack timer, av[7] R patrol clock; par = the Elite sprite's slot */
enum { E_H, E_I, E_N, E_D, E_T, E_Q, E_K, E_R };
static void elite_drop(obj_t *t) {
    t->hidden = 1; t->av[3] = 1;
    obj_t *e = spawn(OT_J5_ELITE, GE_JUMP, t->x, t->y, 2), *m = e ? spawn(OT_J5_ELITEMASK, GE_WALK, t->x, t->y, 2) : NULL;
    if (m) {                                                                     /* the mask: A 20 (ctor), hidden, flag 16; D -1, I -5 (the hop) */
        m->A = 20; m->hidden = 1; m->fl |= F_NOFLY; m->av[E_D] = -1; m->av[E_I] = -5; m->av[E_N] = t->av[0] < 0 ? -1 : 1;
        m->par = (int)(e - G.o);
    } else if (e) e->alive = 0;
    obj_effect(t->x, t->y, "wburst");
    GN.platD += 1;
    snd_play_orig(233, 220, 128);
}
static void elite_kill(obj_t *m, obj_t *e) {                                     /* 11029: sound 29, explosion and debris at Y - 20 */
    snd_play_orig(29, 220, 128);
    obj_effect(m->x, m->y - 20, "explosion01");
    for (int k = 0; k < 3; k++) obj_effect(m->x - 12 + 12 * k, m->y - 20, "debris");
    m->alive = 0; if (e) e->alive = 0;
}
/* the Elite's rules (INFERRED where the spec says so; the RE skeleton: 10958-10962 the aggro check every 20 frames within 200 px,
 * 10974-10979 the walk (H +- 0.4 up to 2, 5 when aggressive), 10968-10971 the attack clock T (cooldown -70), 10990-10993 charge
 * then fire after 25, 11003 / 11005 the fall and the landing, 11006 the knock-back, 11029 the end) */
static int moved_hits_bg(obj_t *o, float dx, float dy) { o->x += dx; o->y += dy; int h = obj_overlaps_bg(o); o->x -= dx; o->y -= dy; return h; }
static void elite_frame(obj_t *m, player_t *p) {
    obj_t *e = slot(m->par);
    if (!e || e->type != OT_J5_ELITE) e = NULL;
    if (m->fl & F_DEF) { elite_kill(m, e); return; }
    double *a = m->av;
    if (m->c > 0 && a[E_D] != -2) {                                              /* 11006: hit: knocked back, up 3 */
        a[E_D] = -2; a[E_I] = -3; a[E_H] = m->x > PX(p) ? 3 : -3; a[E_K] = 0;
    }
    if (a[E_D] < 0) {                                                            /* in the air (-1) or knocked (-2): I + 0.5 (11003) */
        a[E_I] = fmin(a[E_I] + 0.5, 10);
        if (moved_hits_bg(m, (float)a[E_H], 0)) a[E_H] = 0;
        m->x += (float)a[E_H];
        if (a[E_I] > 0 && moved_hits_bg(m, 0, (float)a[E_I])) {                 /* 11005: landed: on the floor, D 0 */
            for (int k = 0; k < 16 && !moved_hits_bg(m, 0, 1); k++) m->y += 1;
            a[E_I] = 0; a[E_D] = 0; a[E_H] = 0;
        } else m->y += (float)a[E_I];
    } else {
        if (!moved_hits_bg(m, 0, 2)) { a[E_D] = -1; a[E_I] = 0; }             /* off a ledge */
        if ((G.frame % 20) == 0) {                                               /* 10958-10962 */
            a[E_Q] = fabs(PX(p) - m->x) <= 200;
            if (a[E_Q]) a[E_N] = PX(p) < m->x ? -1 : 1;
        }
        if (a[E_D] == 0) {
            double mx = a[E_Q] ? 5 : 2;                                          /* 10974-10979 */
            a[E_H] = a[E_N] > 0 ? fmin(a[E_H] + 0.4, mx) : fmax(a[E_H] - 0.4, -mx);
            if (moved_hits_bg(m, (float)a[E_H], 0)) { a[E_H] = 0; a[E_N] = -a[E_N]; }   /* a wall: turn */
            m->x += (float)a[E_H];
            if (!a[E_Q] && ++a[E_R] > 120) { a[E_R] = 0; a[E_N] = -a[E_N]; }     /* 10963-10967: the patrol turns (INFERRED period) */
            if (a[E_Q]) a[E_T] += 1;                                             /* 10968 */
            if (a[E_T] > 60 && a[E_Q]) { a[E_D] = 2; a[E_K] = 0; a[E_H] = 0; a[E_N] = PX(p) < m->x ? -1 : 1; }   /* 10969 / 10981 */
        } else if (a[E_D] == 2) {                                                /* 10990-10993: charge 25 frames, the glow, fire */
            a[E_K] += 1;
            float gx = m->x + (a[E_N] < 0 ? -24.f : 24.f), gy = m->y - 44;
            if (a[E_K] == 2) { obj_t *g = spawn(OT_J5_GLOW, GE_GLOW, gx, gy, 2); if (g) g->av[0] = 0; }   /* 10992 */
            if (a[E_K] == 26) {                                                  /* 11034 / 11035: the laser, aimed at the player */
                obj_t *l = spawn(OT_J5_ELITELASER, GE_LASER, gx, gy, 2);
                if (l) {
                    double dx = PX(p) - gx, dy = PY(p) - 20 - gy, d = sqrt(dx * dx + dy * dy); if (d < 1) d = 1;
                    l->vx = (float)(8 * dx / d); l->vy = (float)(8 * dy / d); l->ang = (float)(atan2(-dy, dx) * RADC);
                }
            }
            if (a[E_K] > 50) { a[E_D] = 0; a[E_T] = -70; }                       /* 10983 / 10969: back, the cooldown */
        }
    }
    if (a[E_D] != -2 && csens_hit(p, m)) hurt_g(p, 5);                           /* contact (INFERRED damage) */
    if (e) {                                                                     /* 11027: the sprite at the mask; 10986-10997 its animation */
        e->x = m->x; e->y = m->y; e->flip = a[E_N] < 0;
        int k = a[E_D] == -2 ? GE_HURT : a[E_D] == -1 ? GE_JUMP : a[E_D] == 2 ? (a[E_K] <= 25 ? GE_CHARGE : GE_FIRE) : GE_WALK;
        if (k == GE_FIRE && !a_is(e, GE_FIRE)) restart_a(e, GE_FIRE); else set_a(e, k);   /* (objects.c ticks it) */
    }
}

/* ======================================================================== projectiles (0x22b01: 11396-11417) */
static void projectiles(player_t *p) {
    for (int i = 0; i < nrt; i++) {
        obj_t *o = slot(RT[i]);
        if (!o || o->born == G.frame) continue;
        if (o->type == OT_J5_ELITEMASK) { if (!GN.closing) elite_frame(o, p); continue; }
        if (o->type == OT_J5_GLOW) {                                             /* 11032 / 11033: shrinks 0.04 a frame, gone after 25 */
            o->av[0] += 1; o->scale = (float)fmax(0.1, 1 - 0.04 * o->av[0]);
            if (o->av[0] > 25) o->alive = 0;
            continue;
        }
        if (o->type == OT_J5_ELITELASER) {                                       /* 11035-11039 */
            o->x += o->vx; o->y += o->vy; o->av[4] += 1;
            if ((o->av[4] > 3 && obj_overlaps_bg(o)) || o->av[4] > 120) { obj_effect(o->x, o->y, "wburst"); o->alive = 0; continue; }
            if (csens_hit(p, o) && vulnerable(p)) { hurt_g(p, 5); obj_effect(o->x, o->y, "wburst"); o->alive = 0; }
            continue;
        }
        if (!GN.proj) continue;
        if (o->type == OT_J5_GUN_FLARE) {
            double *A = &o->av[0], *B = &o->av[1];
            if (*A == 0) { o->y += 12; *B += 1; if (obj_overlaps_bg(o)) { set_a(o, GS_FLARE10); *A = 1; } }   /* 11404 / 11405: falls, burns on the ground */
            else if (*A == 1) {
                *B += 1;                                                         /* 11410 */
                if (*B > 30) { int left = o->x >= PX(p); o->flip = 0; set_a(o, left ? GS_FLARE11L : GS_FLARE11); o->dir = left ? 16 : 0; *A = 2; }   /* 11411 / 11412 */
            } else if (*A == 2) {
                o->x += o->dir == 0 ? 15 : -15; *B += 1;                         /* 11413 / 11414: rolls at 15 px a frame, the camera shakes */
                if (G.cam.Z < 3) G.cam.Z = 3;
                if (*B > 180 && near_border(o, -400)) { o->alive = 0; continue; }   /* 11415 */
            }
            if (csens_hit(p, o)) hurt_g(p, 5);                                   /* 11417 */
        } else if (o->type == OT_J5_GUN_MISSILE) {
            if (!(o->fl & F_DEF)) {
                o->x = (float)(int)o->av[0]; o->y = (float)(int)o->av[1];        /* 11398: A += cos F C, B -= sin F C */
                o->av[0] += c_cos(o->av[5]) * o->av[2]; o->av[1] -= c_sin(o->av[5]) * o->av[2];
                if (!(o->fl & 2)) o->av[5] = fmax(o->av[5] - 10, o->av[6] * 15 - 170);   /* 11399: the left rack's turn down-left */
                else o->av[5] = fmin(o->av[5] + 10, 350 - o->av[6] * 15);               /* 11400: the right rack's */
                o->ang = (float)o->av[5];
                if (csens_hit(p, o) || (o->av[1] > G.camy + 40 && obj_overlaps_bg(o)) || near_border(o, -300)) {   /* the collision handlers (INFERRED) */
                    o->fl |= F_DEF; o->tm = 0; o->hidden = 1; snd_play_orig(38, 220, 128); if (G.cam.Z < 6) G.cam.Z = 6;
                }
            } else {
                double E = o->tm;                                                /* 11401 (foreach_m_43 = FN 8068) / 11402 / 11403 */
                if (E < 45) obj_effect((float)(o->x + (8 + 40 * c_sin(2 * E)) * c_sin(132 * E)), (float)(o->y - 80 * c_sin(5 * E)), "explosion06");
                if (E < 30 && csens_hit(p, o) && p->shield != 2) hurt_g(p, 10);
                if (++o->tm >= 45) o->alive = 0;
            }
        }
    }
}

/* ======================================================================== the end (11343-11372): the hitbox F clock */
static void boom(void) {                                                         /* 11358 / 11360: an explosion on the hull, 98 / 99 / 311 */
    static const int SX[3] = { 98, 99, 311 };
    obj_t *g = gun();
    if (g) obj_effect((float)(g->x - 120 + cross_rand(240)), (float)(g->y - 60 + cross_rand(120)), "explosion06");
    snd_play_orig(SX[cross_rand(3)], 220, 128);
}
static obj_t *hero(int k, float x) { obj_t *o = spawn(OT_J5_HERO, k, x, 610, 2); if (o) force_frame(o, 0); return o; }
static void ending(player_t *p) {
    obj_t *h = ghit();
    if (!h || !(h->fl & F_DEF) || GN.results) return;
    double F = GN.hitF;
    /* 11346-11352: the partner's finisher flies at the pilot; when it reaches him (RV l_kill2: F 70, frame 3066) the pilot is
     * hit (animation 5), the gunship goes to L 9, the music stops, GUIPause B = 6 (slow motion) and F jumps to 1000. The finisher
     * itself (PlayerAssistFinisher, 600-800 K pixels per partner) is not drawn: VRAM (INFERRED timing for Carol / Milla) */
    if (F == 49) snd_play_orig(126, 220, 128);                                 /* RV l_kill2 3045: the finisher's call */
    if (F == 70) {
        set_a(get(OT_J5_GUN_PILOT, 0), GS_PILOT5);
        GN.Lst = 9; GN.K = 0; GN.hitF = 1000; G.slowmo = 6; G.slowmo_t = 0;
        music_stop();
    }
    if (GN.hitF >= 1000 && GN.hitF < 1060) boom();                              /* 11358: every (slowed) frame, RV every 6 */
    if (GN.hitF == 1060) G.slowmo = 0;                                           /* 11359: GUIPause B 0 */
    if (GN.hitF > 1060 && GN.hitF < 1200 && (int)GN.hitF % 5 == 0) boom();      /* 11360: RV 3503-3563 every 5 frames */
    if (GN.hitF == 1200) {                                                       /* 11365: sound 76, shake 10, the white card (opacity 255) */
        snd_play_orig(76, 220, 128); if (G.cam.Z < 10) G.cam.Z = 10;
        GN.fade = 255;
    }
    if (GN.hitF > 1300 && GN.fade > 0) GN.fade = GN.fade - 4 < 0 ? 0 : GN.fade - 4;   /* 11366: fades out (RV l_kill2 3600: still white) */
    if (GN.hitF >= 1219 && !GN.closing) {                                        /* 11368: the player stops and is hidden (its skins */
        GN.closing = 1; GN.proj = 0;                                              /* destroyed), the HUD slides up, the Elites go */
        p->x = 13300; p->vx = p->vy = 0; p->gsp = 0;                              /* (RV: the view centred on the heroes) */
        for (int i = 0; i < nrt; i++) { obj_t *o = slot(RT[i]); if (o && (o->type == OT_J5_ELITE || o->type == OT_J5_ELITEMASK || o->type == OT_J5_ELITELASER || o->type == OT_J5_GLOW || o->type == OT_J5_GUN_FLARE || o->type == OT_J5_GUN_MISSILE)) o->alive = 0; }
    }
    if (GN.hitF >= 1220 && !GN.heroes) {                                         /* 11369: Lilac 13340, Milla 13290, Carol 13240 at y 610 */
        GN.heroes = 1;
        hero(GH_LILAC, 13340); hero(GH_MILLA, 13290); hero(GH_CAROL, 13240);
    }
    /* 11372 (F 1380): the heroes' animation 37 is forced again; they play it from their creation here (under the white card, INFERRED) */
    if (GN.hitF >= 1382 && !GN.results) { GN.results = 1; results_begin_boss(game_timer()); }   /* RV l_kill2 3748: 0x2219e (the results) */
}

/* 11344 / 11345: Milla brings a life-petal box (MonitorRings_357 thrown from the left edge) when four Elites have dropped or the
 * gunship is low, not for Milla herself. Milla (PlayerAssistMilla, about 700 K pixels) is not drawn: VRAM; the box flies in from
 * the view's left edge (RE x: view left - 40; y: Milla's y - 100). The re-delivery period is INFERRED (one box at a time). */
static obj_t *box;
static void milla_box(const player_t *p) {
    obj_t *h = ghit();
    if (G.character == CHR_MILLA || !h || (h->fl & F_DEF) || GN.closing) return;
    if (box && (!box->alive || box->type != OT_MON_RINGS)) box = NULL;
    if (GN.boxT > 0) GN.boxT--;
    if (box || GN.boxT > 0 || !(GN.platD >= 4 || h->A <= 100)) return;
    int a = bank_anim(&G.objs, "monitorrings_35");
    obj_t *mo = a >= 0 ? obj_alloc() : NULL;
    if (!mo) return;
    memset(mo, 0, sizeof *mo); mo->type = OT_MON_RINGS; mo->alive = 1; mo->hp = 1; mo->layer = 2; mo->born = G.frame;
    mo->x = mo->ox = G.camx - 40; mo->y = mo->oy = (float)(PY(p) - 100);
    anim_set(&mo->an, &G.objs, a);
    mo->av[0] = 2; mo->av[2] = 5; mo->av[3] = -6;                               /* A 2: thrown (monitors.c; INFERRED velocity) */
    box = mo; GN.boxT = 600;
}

/* 11618-11622: the storm. LevelDarkSky C counts up while the rain is on (> 40 drops: INFERRED always, the rain is not ported);
 * at C 10 the bolt shows (sound 292) at a random x in the view and up to 100 px above it, facing 0 / 16 at random; at C > 14 it
 * hides and C restarts at -100 - rand(300). (LevelDarkSky's own flash is a 10 x 10 image at world (0, 0): never seen.) */
static obj_t *bolt, *fadeo;
static void storm(void) {
    if (!bolt || !bolt->alive || bolt->type != OT_J5_BOLT) return;
    GN.skyC += 1;
    if (GN.skyC == 10) {
        GN.skyC = 11;
        snd_play_orig(292, 220, 128);
        bolt->y = G.camy - cross_rand(100);
        bolt->x = G.camx + 50 + cross_rand(SCREEN_W - 100);
        set_a(bolt, cross_rand(2) ? GS_BOLTL : GS_BOLT); force_frame(bolt, 0);
        bolt->hidden = 0;
    }
    if (GN.skyC > 14) { GN.skyC = -100 - cross_rand(300); bolt->hidden = 1; }
}

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&GN, sizeof GN); snap_register(RT, sizeof RT); snap_register(&nrt, sizeof nrt);
                                                  snap_register(&bolt, sizeof bolt); snap_register(&box, sizeof box); snap_register(&fadeo, sizeof fadeo); } }
void jc_boss5_spawn(void) {
    snap_once();
    memset(L, 0, sizeof L); memset(&GN, 0, sizeof GN); mark_b = -1;
    on = 0; nrt = 0; bolt = box = fadeo = NULL; G.slowmo = 0;
    for (int t = OT_JCB5_FIRST; t <= OT_J5_LAST && !on; t++) if (objects_have(t)) on = 1;
    if (!on) return;
    for (int k = 0; k < GS_N; k++) SA[k] = (short)ens_anim(SN[k]);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if (t == OT_CAMLIM_B && mark_b < 0) mark_b = (short)i;
        if (t < OT_JCB5_FIRST || t > OT_J5_LAST) continue;
        list_t *l = &L[t - OT_JCB5_FIRST]; if (l->n < LMAX) l->i[l->n++] = (short)i;
        o->st = 0; o->tm = 0; o->c = 0; o->fl = 0; o->dir = 0; o->ang = 0; o->scale = 0; memset(o->av, 0, sizeof o->av);
        o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL; o->bd = 0;
        o->layer = 1;                                                            /* json layer 2: behind the play layer */
        switch (t) {
        case OT_J5_GUN: set_a(o, GS_SHIP); break;
        case OT_J5_GUN_BLADEL: set_a(o, GS_BLADEL); break;
        case OT_J5_GUN_BLADER: set_a(o, GS_BLADER); break;
        case OT_J5_GUN_CANL: set_a(o, GS_CANL); break;
        case OT_J5_GUN_CANR: set_a(o, GS_CANR); break;
        case OT_J5_GUN_HIT: set_a(o, GS_HIT); o->A = 300; o->fl |= F_NOFLY; break;   /* ctor A 300, D 2; 11651 flag 16 */
        case OT_J5_GUN_LIGHT: set_a(o, GS_LIGHT); break;
        case OT_J5_GUN_LIGHTL: set_a(o, GS_LL); break;
        case OT_J5_GUN_LIGHTR: set_a(o, GS_LR); break;
        case OT_J5_GUN_RACKL: set_a(o, GS_RACKL); break;
        case OT_J5_GUN_RACKR: set_a(o, GS_RACKR); break;
        case OT_J5_GUN_PILOT: set_a(o, GS_PILOT); break;
        case OT_J5_GUN_PLAT: set_a(o, GS_PLAT); break;
        case OT_J5_GUN_TROOPER: set_a(o, GS_TRP); break;
        case OT_J5_GUN_TURBL: set_a(o, GS_TURBL); break;
        case OT_J5_GUN_TURBR: set_a(o, GS_TURBR); break;
        case OT_J5_ELITE: case OT_J5_ELITEMASK: o->alive = 0; break;             /* 11651: the placed template destroyed */
        }
    }
    obj_t *g = gun();
    if (!g) { on = 0; return; }
    GN.D = g->x; GN.E = g->y; GN.P = g->y; GN.G = 40; GN.H = 20; GN.O = -1;      /* ctor G 40, H 20, O -1; 11651 D, E, P */
    GN.rackD[0] = GN.rackD[1] = -8; GN.hitD = 2;                                 /* racks' ctor D -8; hitbox D 2 */
    static const int REL[5] = { OT_J5_GUN_LIGHTL, OT_J5_GUN_LIGHTR, OT_J5_GUN_LIGHT, OT_J5_GUN_CANL, OT_J5_GUN_CANR };
    for (int t = 0; t < 5; t++) for (int k = 0; k < count(REL[t]); k++) {        /* 11651: their offsets from the gunship (D, E); lights A = index */
        obj_t *o = get(REL[t], k); if (!o) continue;
        o->av[0] = o->x - g->x; o->av[1] = o->y - g->y; o->av[2] = k;
    }
    for (int k = 0; k < count(OT_J5_GUN_TROOPER); k++) { obj_t *o = get(OT_J5_GUN_TROOPER, k); if (o) o->av[0] = k * 40 - 20; }   /* 11651: A = 40 k - 20 */
    bolt = spawn(OT_J5_BOLT, GS_BOLT, 0, 0, 0);                                  /* LevelLightningBolt (json layer 1): hidden (ctor) */
    if (bolt) bolt->hidden = 1;
    GN.skyC = -98;                                                               /* LevelDarkSky C (ctor 0) counts once the rain has > 40 drops:
                                                                                    * the rain is not ported, its fill time is (RV: the first bolt at 108 in every trace) */
}
void jc_boss5_pre(player_t *p) { (void)p; }
void jc_boss5_frame(player_t *p) {
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_J5")) {                                                   /* host tests (tests/test_jc_act5.py): one line a gameplay frame */
        const obj_t *h = ghit(); int ne = 0, nb = 0;
        for (int i = 0; i < nrt; i++) { const obj_t *o = slot(RT[i]); if (o && o->type == OT_J5_ELITEMASK) ne++; }
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_MON_RINGS && &G.o[i] == box) nb = 1;
        printf("j5 s %d L %.0f E %.0f D %.0f F %.0f A %.0f hd %.0f sm %d pd %.0f cl %d fd %d res %d el %d box %d px %.0f\n", fn_boss_serp_jc5(), GN.Lst, GN.E, GN.D,
               GN.hitF, h ? h->A : -1, GN.hitD, G.slowmo, GN.platD, GN.closing, GN.fade, GN.results, ne, nb, p->x);
    }
#endif
    if (!on) return;
    float y;
    if (!GN.on && fn_boss_serp_falling(&y) && y > GN.P + 800) gunship_start(p);   /* 11283 (gunship y = E: still the placed one) */
    storm();                                                                     /* 11618-11622 */
    if (GN.on) { gunship_frame(p); projectiles(p); milla_box(p); ending(p); }
    if (GN.fade > 0 && !fadeo) fadeo = spawn(OT_J5_FADE, -1, 0, 0, 7);
    if (fadeo && GN.fade <= 0) { fadeo->alive = 0; fadeo = NULL; }
    if (G.frame % 64 == 0) rt_compact();
}
int  jc_boss5_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }        /* every rule runs in jc_boss5_frame */
int  jc_boss5_q20(int t) { return t == OT_J5_GUN_HIT || t == OT_J5_ELITEMASK; }
int  jc_boss5_input(uint32_t *held) { if (!on || !GN.closing) return 0; *held = 0; return 1; }   /* 11368: CTRL.M 1 (no control) */
int  jc_boss5_player_hidden(void) { return on && GN.closing; }                 /* 11368: the player's skins destroyed */
float jc_boss5_hud_dy(void) { return on && GN.closing ? -80.f : 0.f; }          /* 11368: loop_gui_shiftup_14 (80 px at once, as FN6 8764) */
int  jc_boss5_state(void) { return !on ? 0 : GN.results ? 4 : (ghit() && (ghit()->fl & F_DEF)) ? 3 : GN.on ? 2 : 1; }

/* ---- drawing: the rig in its RE order (11286 / 11287 move_back / move_front): platform, troopers, racks, turbines with their
 * blades, the hull, the cockpit overlay (additive), the pilot, the lights */
static void draw1(const obj_t *o, uint32_t col) {
    if (!o || !o->alive || !o->an.b || o->hidden) return;
    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
    if (sx < -400 || sx > SCREEN_W + 400 || sy < -400 || sy > SCREEN_H + 400) return;
    int inv = boss_flash(o);
    if (inv) r_set_invert(1);
    if (o->scale != 0 && o->scale != 1.f) r_set_scale(o->scale);
    frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, o->flip, col);
    r_set_scale(1.f);
    if (inv) r_set_invert(0);
}
int jc_boss5_draw_obj(const obj_t *o, int pass) {
    (void)pass;
    if (o->type < OT_JCB5_FIRST || o->type > OT_J5_LAST) return 0;
    if (o->type != OT_J5_GUN) {
        if (o->type >= OT_J5_GUN_BLADEL && o->type <= OT_J5_GUN_TURBR) return 1;  /* drawn with the hull */
        if (o->type == OT_J5_ELITEMASK) return 1;
        if (o->type == OT_J5_FADE) {                                             /* 11365 / 11366: LevelCardFade (direction 8: white) */
            if (GN.fade > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)GN.fade << 24) | 0xf8fcf8);
            return 1;
        }
        if (o->type == OT_J5_GLOW) { r_set_additive(1); draw1(o, 0xffffffff); r_set_additive(0); return 1; }   /* shader (INFERRED add) */
        draw1(o, 0xffffffff);
        return 1;
    }
    draw1(get(OT_J5_GUN_PLAT, 0), 0xffffffff);
    for (int k = 0; k < count(OT_J5_GUN_TROOPER); k++) draw1(get(OT_J5_GUN_TROOPER, k), 0xffffffff);
    draw1(get(OT_J5_GUN_RACKL, 0), 0xffffffff); draw1(get(OT_J5_GUN_RACKR, 0), 0xffffffff);
    int flick = (G.frame % 4) < 2;                                               /* 11287: the blades in front of the turbines on alternate frames */
    for (int s = 0; s < 2; s++) {
        const obj_t *t = get(s ? OT_J5_GUN_TURBR : OT_J5_GUN_TURBL, 0), *b = get(s ? OT_J5_GUN_BLADER : OT_J5_GUN_BLADEL, 0);
        if (flick) { draw1(t, 0xffffffff); draw1(b, 0xffffffff); } else { draw1(b, 0xffffffff); draw1(t, 0xffffffff); }
    }
    draw1(o, 0xffffffff);
    draw1(get(OT_J5_GUN_CANL, 0), 0xffffffff); draw1(get(OT_J5_GUN_CANR, 0), 0xffffffff);
    const obj_t *h = ghit();
    if (h && !h->hidden) { r_set_additive(1); draw1(h, 0xffffffff); r_set_additive(0); }
    draw1(get(OT_J5_GUN_PILOT, 0), 0xffffffff);
    static const int LT[3] = { OT_J5_GUN_LIGHTL, OT_J5_GUN_LIGHT, OT_J5_GUN_LIGHTR };
    for (int t = 0; t < 3; t++) for (int k = 0; k < count(LT[t]); k++) draw1(get(LT[t], k), 0xffffffff);
    return 1;
}
