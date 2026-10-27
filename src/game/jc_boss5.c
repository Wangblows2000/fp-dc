/* Jade Creek act 5 (session 21): the finale. Spec: build/re/spec_jc_boss5.md (RE handle_frame_44_events / on_frame_44_start,
 * RV build/orig_traces/jc_boss5). Labels: RE read from code, RV measured in the original, INFERRED reasoned.
 *
 * What Jade Creek's last act plays in the Classic route (Lilac, Carol, Milla; RE + RV l_tp / l_kill / l_kill2):
 *   - start: Neera's ice ambush (Hovercop + NPCNeera, group 0x22afe; the bosses module's types: not here, see the spec);
 *   - x 5800: the Tredd miniboss (engine types 73-87);
 *   - x > Syntax - 1200: loop_cutscene_start_43 -> the Brevon mech flyby scene with Syntax and Torque (0x22aff; g1 == 5 =
 *     Torque would get the Brevon mech FIGHT 0x22af4 instead: not playable, not ported);
 *   - the arena (camera trigger x 13560): Serpentine 2 = FN3's fight (fn_boss.c, hitbox 2552 variant: A 180, D 6), RV l_kill;
 *   - his defeat: petals, he is knocked off the cliff (0x22af5, fn_boss.c) and comes back in the GUNSHIP (0x22b05, this file);
 *   - the gunship's defeat: the partner finisher, explosions in slow motion, the white flash, the three heroes and the results
 *     (0x221a1 -> Classic 10941 -> the next stage; Adventure 10942 / 10943 opens a dialog instead).
 * This file: the gunship rig and attacks (11284-11343), its projectiles (11396-11417), the Elite troopers (10954-11041), the end
 * (11350-11372). Order in the frame (RE): Elites (10954-11041) before the bosses, then the fall check (11283), the gunship block,
 * its projectiles. Performance (SH4): per-type instance lists built once at spawn; a level without these types returns at once. */
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
       GS_FLARE, GS_FLARE10, GS_FLARE11, GS_FLARE11L, GS_MIS, GS_MISL, GS_WARN2, GS_N };
static const char *const SN[GS_N] = { "j5g_ship", "j5g_bladel", "j5g_blader", "j5g_canl", "j5g_canl7", "j5g_canr", "j5g_canr7", "j5g_hit", "j5g_hit8",
    "j5g_light", "j5g_ll", "j5g_ll1", "j5g_lr", "j5g_lr1", "j5g_rackl", "j5g_rackr", "j5g_pilot", "j5g_pilot5", "j5g_plat", "j5g_trp", "j5g_trp_l",
    "j5g_turbl", "j5g_turbl8", "j5g_turbr", "j5g_flare", "j5g_flare10", "j5g_flare11", "j5g_flare11_l", "fb_mis", "fb_mis_l", "fb_warn2" };
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
static obj_t *spawn(int type, int sk, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    set_a(o, sk);
    return o;
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
        if (GN.Lst != 4 && GN.E < GN.P - 260) { GN.E += 2; GN.M = 0; }           /* 11301 */
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
        if (!def && h->A < GN.hitD * 100) { GN.hitE = 5; GN.hitD -= 1; snd_play_orig(134, 220, 128); }   /* 11338 */
        if (GN.hitE > 0) { obj_effect((float)(h->x + 6 * GN.hitE - 15), h->y, "debris"); GN.hitE -= 1; }   /* 11339 (debris: INFERRED look) */
        if (G.character != CHR_LILAC && h->A <= 220) h->A = 220;                 /* 11340 */
        if (G.character == CHR_MILLA && h->A <= 50) { h->A = 50; GN.hitD = 0; }  /* 11341 */
        if (def && !GN.once34) { GN.once34 = 1; GN.hitE = 5; GN.hitD -= 1; snd_play_orig(134, 220, 128); }   /* 11342 */
        if (def) { set_a(h, GS_HIT8); h->hidden = 0; GN.hitF += 1; }            /* 11343 */
    }
}

/* ======================================================================== the Elite troopers (10954-11041; spec E)
 * The mask (EnemyEliteMask_2528, Q20, A 20) is the body: it falls, walks toward the player and is what gets hit; the Elite
 * sprite (EnemyElite_2529) is drawn at it. INFERRED where the spec says so. */
static void elite_drop(obj_t *t) {
    t->hidden = 1; t->av[3] = 1;
    obj_t *m = spawn(OT_J5_ELITEMASK, -1, t->x, t->y, 2), *e = spawn(OT_J5_ELITE, -1, t->x, t->y, 2);
    if (m) { m->A = 20; m->hidden = 1; m->fl |= F_NOFLY; m->av[3] = -1; m->av[4] = -5; m->av[6] = 1; }   /* D -1, I -5 (the hop), Q 1 */
    (void)e;
    obj_effect(t->x, t->y, "wburst");
    GN.platD += 1;
    snd_play_orig(233, 220, 128);
}

/* ======================================================================== projectiles (0x22b01: 11396-11417) */
static void projectiles(player_t *p) {
    if (!GN.proj) return;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->born == G.frame) continue;
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
static void ending(player_t *p) {
    obj_t *h = ghit();
    if (!h || !(h->fl & F_DEF) || GN.results) return;
    double F = GN.hitF;
    if (F == 50) { GN.hitF = 1000; GN.slow0 = G.frame; music_stop(); }          /* 11346-11352 (the partner's finisher: INFERRED skipped to its hit) */
    if (GN.hitF >= 1000 && GN.hitF < 1060 && (G.frame - GN.slow0) % 6 == 0) {   /* 11358-11360 (slow motion: GUIPause B 6, RV l_kill2 3095-3455) */
        static const int SX[3] = { 98, 99, 311 };
        obj_t *pi = get(OT_J5_GUN_PILOT, 0);
        if (pi) obj_effect((float)(pi->x - 60 + cross_rand(120)), (float)(pi->y - 40 + cross_rand(80)), "explosion06");
        snd_play_orig(SX[cross_rand(3)], 220, 128);
    }
    if (GN.hitF == 1200) { snd_play_orig(76, 220, 128); if (G.cam.Z < 10) G.cam.Z = 10; }   /* 11365: the flash */
    if (GN.hitF >= 1380 && !GN.results) { GN.results = 1; results_begin_boss(game_timer()); }   /* 11372: the sign -> results */
    (void)p;
}

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&GN, sizeof GN); } }
void jc_boss5_spawn(void) {
    snap_once();
    memset(L, 0, sizeof L); memset(&GN, 0, sizeof GN); mark_b = -1;
    on = 0;
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
}
void jc_boss5_pre(player_t *p) { (void)p; }
void jc_boss5_frame(player_t *p) {
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_J5") && G.frame % 30 == 0) {
        obj_t *g = gun(), *h = ghit();
        const obj_t *sp = NULL, *sh = NULL;
        for (int i = 0; i < G.nobjs; i++) { if (G.o[i].alive && G.o[i].type == OT_FN_SERP) sp = &G.o[i]; if (G.o[i].alive && G.o[i].type == OT_FN_SERP_HIT) sh = &G.o[i]; }
        printf("j5 %d on %d serp %d (%.0f,%.0f h%d st%.0f B%.0f) shA %.1f c%d fl%x px %.0f gun %d (%.0f,%.0f) L %.0f M %.0f N %.0f E %.0f J %.2f F %.2f hitA %.0f D %.0f F %.0f\n",
               G.frame, on, fn_boss_serp_jc5(), sp ? sp->x : 0, sp ? sp->y : 0, sp ? sp->hidden : -1, sp ? sp->av[0] : -1, sp ? sp->av[1] : -1, sh ? sh->A : -1, sh ? sh->c : -1, sh ? sh->fl : 0,
               p->x, GN.on, g ? g->x : 0, g ? g->y : 0, GN.Lst, GN.M, GN.N, GN.E, GN.J, GN.F, h ? h->A : -1, GN.hitD, GN.hitF);
    }
#endif
    if (!on) return;
    float y;
    if (!GN.on && fn_boss_serp_falling(&y) && y > GN.P + 800) gunship_start(p);   /* 11283 (gunship y = E: still the placed one) */
    if (GN.on) { gunship_frame(p); projectiles(p); ending(p); }
}
int  jc_boss5_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }        /* every rule runs in jc_boss5_frame */
int  jc_boss5_q20(int t) { return t == OT_J5_GUN_HIT || t == OT_J5_ELITEMASK; }
int  jc_boss5_input(uint32_t *held) { (void)held; return 0; }
int  jc_boss5_player_hidden(void) { return 0; }
float jc_boss5_hud_dy(void) { return 0.f; }
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
