/* Fortune Night act 6 (the Panther fight): the partner assist group 0x227c8 (session 18). Events 8668-8721 of
 * handle_frame_35_events, the plane mode 8524-8532 (Milla on the plane) and the closing scene's cast 8765-8785.
 *  - the offense helper PlayerAssistOffense_2048: Carol on her bike for Lilac (direction 1, subgroup 0x227c4, 8674-8687),
 *    Lilac for Carol (direction 0, 0x227be, 8688-8701), both in turn for Milla (alterable flag 5 toggles, 8672 / 8673);
 *    she drives in along the floor, jumps at the panther's hitbox and strikes with ShieldInstaShield_407 objects that take
 *    the shared hit 2740 (combat.c: combat_shield_at), lands, drives back out; I is the 600-frame cooldown;
 *  - the plane PlayerAssist_2045 with Torque at the controls and Milla (PlayerAssistDefense_2046) in the back seat
 *    (0x227c1 / 0x227c0, 8702-8721): flies in from far left, stops, Milla holds up a MonitorRings_357 (the life-petal box)
 *    and drops it; the box breaks on the scrolling floor (6 petals); the plane flies off, crosses the far background
 *    (direction 1, small) and comes back when the player is low on petals;
 *  - Milla: the plane is her platform (0x227c2, 8524-8532); no deliveries (8528 switches 0x227c0 / c1 / c3 off).
 * Sources: build/re/spec_fn_bosses.md G.10 + RE_NOTES "Session 18: the partner assist" (the event reading), original traces
 * (tools/orig, session 18: RV). Labels RE / RV / INFERRED as everywhere. fn_panther.c calls the hooks (fn_assist_*). */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / 57.29577951308232); }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / 57.29577951308232); }

enum { S_PLANE, S_ASSIST, S_ASSIST1, S_ADEF, S_ADEF1, S_ADEF7, S_ADEF10, S_ADEF11, S_AOFF0, S_AOFF1, S_AOFF7, S_AOFF7B, S_AOFF8,
       S_AOFF8B, S_APROJ, S_APROJ7, S_APROJ8, S_DUST,
       C_L41S, C_L41G, C_L1, C_L40, C_L40L, C_L37L, C_C41S, C_C41G, C_C40L, C_C37L, C_M2, C_M40, C_M37, C_T1, C_T40, C_T37, S_N };
static const char *const SN[S_N] = { "fp_plane", "fp_assist", "fp_assist1", "fp_adef", "fp_adef1", "fp_adef7", "fp_adef10", "fp_adef11",
    "fp_aoff0", "fp_aoff1", "fp_aoff7", "fp_aoff7b", "fp_aoff8", "fp_aoff8b", "fp_aproj", "fp_aproj7", "fp_aproj8", "fndust",
    "fc_l41s", "fc_l41g", "fc_l1", "fc_l40", "fc_l40l", "fc_l37l", "fc_c41s", "fc_c41g", "fc_c40l", "fc_c37l", "fc_m2", "fc_m40", "fc_m37",
    "fc_t1", "fc_t40", "fc_t37" };
static int SA[S_N];

/* a helper's alterables A..I, its flags (bit 1 = flag 1, bit 2 = flag 2, bit 5 = flag 5) */
typedef struct { double A, B, C, D, E, F, G, H, I; unsigned fl; int dir; } alt_t;
enum { NDUST = 24 };
static struct {
    int on, grp, plane_mode;           /* 0x227c8 (assist), the delivery groups 0x227c0 / c1 (not for Milla), 0x227c2 */
    int g_carol, g_lilac;              /* 0x227c4 (Carol on the bike) / 0x227be (Lilac) */
    obj_t *plane, *def, *off, *proj, *plat, *mon;
    alt_t pl, df, of;                  /* PlayerAssist, AssistDefense, AssistOffense */
    animator_t a_off, a_proj;          /* the offense / projectile animate at a forced speed (8693): own animators */
    int proj_vis, proj_anim, off_anim, off_speed;
    int voice_l, voice_m;              /* 0x227f4 / 0x22839: the arrival voice once */
    int t, last_hit;
    double plA, plB, plC;              /* plane mode (Milla) */
    int dust[NDUST], dust_born[NDUST], dust_spd[NDUST], ndust;
    /* the closing scene's cast (8765-8785): Lilac, Carol, Milla, Torque */
    struct { float x, y; animator_t an; int k, speed; } cast[4];
    int cast_on, cast_turned, cast_win;
    int dbg, knock_by;
} AS;

static int ch_mv_a(void) { return G.character; }   /* MV.A: 0 Lilac, 2 Carol, 3 Carol on the bike, 4 Milla (1 Spade, 5 Torque: not playable) */
static void setan(obj_t *o, int k) { int a = SA[k]; if (o && a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
/* Active::force_animation (RE): a change restarts at frame 0 but keeps the animation counter (+0xc0); the same animation: nothing */
static void own_set(animator_t *an, int k) { int a = SA[k]; if (a >= 0 && (an->b != &G.ens || an->anim != a)) { int acc = an->acc; anim_set(an, &G.ens, a); an->acc = acc; } }

/* ===================================================================================================== spawn */
/* every level load (fn_panther_spawn, before its FN5 / FN6 test): AS outlives the level otherwise. Session 20: the FN6 closing
 * scene leaves cast_on = 1 (MV.M 50: the player not drawn), so the player vanished in SB1 and in every later level until FN6 was
 * replayed or the console restarted (game_draw tests fn_assist_cast_on() in every level). */
void fn_assist_reset(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&AS, sizeof AS); }
    memset(&AS, 0, sizeof AS);
}
void fn_assist_spawn(void) {
    fn_assist_reset();
    for (int k = 0; k < S_N; k++) SA[k] = ens_anim(SN[k]);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_FNP_PLANE: if (!AS.plat) AS.plat = o; setan(o, S_PLANE); o->hidden = 1; break;   /* PlanePlatform: invisible (ctor) */
        case OT_FNP_ASSIST: if (!AS.plane) AS.plane = o; break;
        case OT_FNP_ADEF: if (!AS.def) AS.def = o; break;
        case OT_FNP_AOFF: if (!AS.off) AS.off = o; break;
        case OT_FNP_APROJ: if (!AS.proj) AS.proj = o; break;
        }
    }
    if (!AS.plane || !AS.def || !AS.off) return;
    AS.on = 1;
    AS.plane->layer = 1; AS.def->layer = 1; AS.off->layer = 2; if (AS.proj) AS.proj->layer = 1;   /* json layers 2 / 2 / 3 / 2 */
    AS.off->an.b = NULL; AS.off->hidden = 1; if (AS.proj) { AS.proj->an.b = NULL; AS.proj->hidden = 1; }
    /* PlayerAssist: instance B = F = 104 (RV); the first frame's 8703 (X 888 > the frame width, G 0) sends it to view x - 2000
     * with D 3, 8704 (X > 464) D - 0.05 (RV: A -1646 at act frame 121 = -2000 + 120 x 2.95, view x 0 on that first frame) */
    AS.pl.B = AS.pl.F = 104; AS.pl.A = -2000; AS.pl.D = 2.95;
    setan(AS.plane, S_ASSIST); AS.plane->hidden = 0;
    setan(AS.def, S_ADEF); AS.def->hidden = 0;
    /* AssistOffense ctor: C 5, I 1200 (RE); instance G 267 (the floor line, RV) */
    AS.of.C = 5; AS.of.I = 1200; AS.of.G = 267;
    AS.grp = 1;
    if (chr_is_milla(ch_mv_a())) {                                               /* 8814 (g1 4): defense destroyed, I 60, plane on layer 3 */
        AS.def->alive = 0; AS.def = NULL; AS.of.I = 60; AS.plane->layer = 2;
        AS.plane_mode = 1; AS.grp = 0;                                           /* 8528: 0x227c1 / c0 / c3 off */
        AS.plB = 200;
    }
#ifdef FP_HOST_ENV
    AS.dbg = getenv("FP_DBG_FP") != NULL;
#endif
}

/* ===================================================================================================== plane mode (Milla, 8524-8532) */
static void plane_mode(player_t *p) {
    if (!AS.plat || !AS.plane) return;
    if (AS.t < 4) { p->x = 520; p->y = 170 + ORIG_REF_DY; AS.plA = 520; AS.plB = 200; }   /* 8524 (g3 < 4) */
    uint32_t h = p->held;
    if (h & BTN_UP) AS.plB = fmax(AS.plB - 2, 70);                               /* 8526 (CTRL.A) */
    if (h & BTN_DOWN) AS.plB = fmin(AS.plB + 2, 230);                            /* 8527 (CTRL.B) */
    double oldy = AS.plat->y;
    AS.plane->hidden = 0; AS.plane->x = (float)(int)p->x;                        /* 8525 */
    AS.plane->y = (float)(int)(AS.plB + 8 * c_cos(2 * AS.plC + 90));
    AS.plC = fmod(AS.plC + 0.7, 360);
    AS.plat->x = (float)(int)p->x; AS.plat->y = AS.plane->y;
    if (p->ground && fabs((p->y) - oldy) <= 3) p->y = AS.plat->y;               /* standing on the platform: carried (INFERRED) */
    G.cam.H = (float)(100 - 0.5 * floor(p->y - ORIG_REF_DY));
    if (p->x > G.lvl.w) p->x = G.lvl.w;
    setan(AS.plane, S_ASSIST);
}
void fn_assist_plane_mode(player_t *p) { if (AS.on && AS.plane_mode) { AS.t++; plane_mode(p); } }

/* ===================================================================================================== the plane and Milla (8702-8721) */
static void dust_add(obj_t *o, int spd) {                                        /* 8705: EffectsDust01 behind the plane, moving left */
    if (!o) return;
    int k = AS.ndust < NDUST ? AS.ndust++ : (int)(G.frame % NDUST);
    AS.dust[k] = (int)(o - G.o); AS.dust_born[k] = o->born; AS.dust_spd[k] = spd;
}
static void dust_move(void) {   /* movement speed 50 to the left (3.125 px a frame: INFERRED from the original's trail length);
                                 * animation forced to 50..99 (the generic tick adds the table's 30) */
    for (int k = 0; k < AS.ndust; k++) {
        obj_t *o = &G.o[AS.dust[k]];
        if (!o->alive || o->type != OT_EXPLODE || o->born != AS.dust_born[k]) continue;
        if (o->born != G.frame) { o->x -= 3.125f; anim_tick(&o->an, AS.dust_spd[k] - 30); }
    }
}
static void plane_frame(const player_t *p) {
    obj_t *pl = AS.plane, *df = AS.def;
    alt_t *P = &AS.pl, *D = &AS.df;
    const double vx = G.camx, vy = G.camy, limL = 224;                          /* CameraLimitBossLeft x 224 (frame_35.json) */
    /* 8702: x = A + 32 sin C, y = B + 8 cos(2C + 90); C + 0.7; Milla at the plane; A += D, B += E */
    pl->x = (float)(int)(P->A + 32 * c_sin(P->C)); pl->y = (float)(int)(P->B + 8 * c_cos(2 * P->C + 90));
    P->C = fmod(P->C + 0.7, 360);
    if (df) { df->x = pl->x; df->y = pl->y; }
    P->A += P->D; P->B += P->E;
    if (pl->x > G.lvl.w && P->G == 0) { P->D = 3; P->A = vx - 2000; }          /* 8703 */
    if (P->G == 0 && pl->x > limL + 240) P->D = fmax(P->D - 0.05, 0);           /* 8704 */
    if (P->dir == 0 && pl->x > 0 && SA[S_DUST] >= 0) {                           /* 8705: a puff each frame (2 cross_seed steps: speed, angle) */
        int spd = 50 + cross_rand(50), ang = cross_rand(4) * 90;                /* force_speed(50 + rand 50), angle 0 / 90 / 180 / 270 (bits INFERRED) */
        obj_t *d = obj_alloc();
        if (d) {
            memset(d, 0, sizeof *d); d->type = OT_EXPLODE; d->alive = 1; d->x = pl->x - 64; d->y = pl->y + 8; d->born = G.frame;
            d->layer = pl->layer; d->ang = (float)ang; anim_set(&d->an, &G.ens, SA[S_DUST]); dust_add(d, spd);
        }
    }
    if (df && P->G == 0 && pl->x > limL + 240 && pl->x < G.lvl.w && P->D <= 0 && D->A == 0) { D->A = 1; D->B = 0; }   /* 8706 */
    if (P->G == 1) { P->D = fmax(P->D - 0.05, -3); P->E = fmax(P->E - 0.03, -4); }   /* 8707 */
    if (P->G == 1 && pl->y < vy - 80) { P->D = P->E = 0; P->G = 2; P->H = 0; P->A = vx - 80; P->B = P->F; }   /* 8708 */
    if (P->G == 2) {                                                             /* 8709: the far pass (direction 1) */
        P->dir = 1; if (df) D->dir = 1;
        P->A = fmin(P->A + 1, limL + 240); P->H += 1;
    }
    if (P->G == 2 && (p->hp <= 6 || (p->hp <= 12 && P->H > 600))) P->G = 3;     /* 8710: GUIRings (petals) <= 6, or <= 12 after 600 frames */
    if (P->G == 3) P->A -= 1;                                                    /* 8711 */
    if (P->G == 3 && pl->x < vx - 100) { P->G = 0; P->D = 3; P->A = vx - 200; P->dir = 0; if (df) D->dir = 0; }   /* 8712 */
    setan(pl, P->dir ? S_ASSIST1 : S_ASSIST);
    if (!df) return;
    /* Milla (0x227c0): 8713 / 8714 (A 2, the shield cube) are never reached: no event sets A = 2 (RE) */
    if (D->A == 3) { D->B += 1; setan(df, S_ADEF10); }                           /* 8715 */
    if (D->A == 3 && D->B > 40) { D->A = 0; D->B = 0; setan(df, S_ADEF); P->G = 1; }   /* 8716: the plane leaves */
    if (D->A == 1) { setan(df, S_ADEF7); D->B += 1; }                            /* 8717 */
    if (D->A == 1 && D->B > 60) {                                                /* 8718: MonitorRings_357 at Milla, held (flag 2), sound 241 */
        D->A = 4; D->B = 0;
        int a = bank_anim(&G.objs, "monitorrings_35");
        obj_t *m = a >= 0 ? obj_alloc() : NULL;
        if (m) {
            memset(m, 0, sizeof *m); m->type = OT_MON_RINGS; m->alive = 1; m->hp = 1; m->layer = df->layer; m->born = G.frame;
            m->x = m->ox = df->x; m->y = m->oy = df->y; anim_set(&m->an, &G.objs, a); m->av[4] = 1;   /* held */
            AS.mon = m;
        }
        snd_play_orig(241, 220, 128);
    }
    if (D->A == 4) { setan(df, S_ADEF11); D->B += 1; }                           /* 8719 */
    if (D->A == 4 && AS.mon && AS.mon->alive && AS.mon->type == OT_MON_RINGS && AS.mon->av[4]) {   /* 8720: at Milla's action point - (16, 32) */
        double ax, ay; relic_act_point(df, &ax, &ay);
        AS.mon->x = (float)(int)(ax - 16); AS.mon->y = (float)(int)(ay - 32);
    }
    if (D->A == 4 && D->B > 60) {                                                /* 8721: thrown down (A 2, Y 4), on the player's layer */
        if (AS.mon && AS.mon->alive && AS.mon->type == OT_MON_RINGS && AS.mon->av[4]) {
            obj_t *m = AS.mon; m->fl &= ~1u; m->av[4] = 0; m->av[0] = 2; m->av[2] = 0; m->av[3] = 4; m->layer = 2;
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d drop %.0f %.0f\n", G.frame, m->x, m->y);
#endif
        }
        AS.mon = NULL;
        D->A = 3; D->G = 0; D->B = 0; D->F = 0;
    }
    setan(df, D->A == 1 ? S_ADEF7 : D->A == 4 ? S_ADEF11 : D->A == 3 ? S_ADEF10 : D->dir ? S_ADEF1 : S_ADEF);
}

/* ===================================================================================================== the offense helper (8668-8701) */
static void off_box(int *x0, int *y0, int *x1, int *y1) {                       /* the current image box */
    const obj_t *o = AS.off; const frame_t *f = AS.a_off.b ? anim_frame(&AS.a_off) : NULL;
    if (!f) { *x0 = (int)o->x - 16; *y0 = (int)o->y - 32; *x1 = *x0 + 32; *y1 = *y0 + 32; return; }
    *x0 = (int)floorf(o->x) - f->hx; *y0 = (int)floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int knocked(const fn_assist_ctx_t *c) {                                   /* 8684 / 8698: plasma, laser, missile, rapid bullet */
    int x0, y0, x1, y1; off_box(&x0, &y0, &x1, &y1);
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive || (o->type != OT_FNP_PLASMA && o->type != OT_FNP_MISSILE && o->type != OT_FNP_RBULLET) || !o->an.b) continue;
        if (fabsf(o->x - AS.off->x) > 160 || fabsf(o->y - AS.off->y) > 160) continue;
        if (obj_rect_hit(o, x0, y0, x1, y1)) { AS.knock_by = o->type; return 1; }
    }
    if (c->laser_on) {                                                           /* the 800 x 24 laser segment (as fn_panther.c) */
        double ux = c_cos(c->lang), uy = -c_sin(c->lang), dx = AS.off->x - c->lx, dy = AS.off->y - 16 - c->ly;
        double along = dx * ux + dy * uy, across = fabs(-dx * uy + dy * ux);
        if (along > 0 && along < 800 && across < 12 + 16) { AS.knock_by = -1; return 1; }
    }
    return 0;
}
static void shields_follow(void) {                                               /* 8681 / 8695: flag-4 shields at the helper */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *s = &G.o[i];
        if (s->alive && s->type == OT_SHIELD && (s->fl & 0x10)) { s->x = (float)(int)AS.off->x; s->y = (float)(int)AS.off->y; }
    }
}
static void offense_frame(const fn_assist_ctx_t *c) {
    obj_t *o = AS.off; alt_t *O = &AS.of;
    const double vx = G.camx;
    const int A0 = ch_mv_a(), milla = A0 >= 4 && A0 <= 5, carolp = A0 >= 2 && A0 <= 3;
    const int nearhit = c->hit != NULL;
    if (O->I > 2) O->I -= 1;                                                     /* 8668 */
    if (O->I == 2 && !(c->fight && c->mode2) && !(c->cannon_d >= 3 && c->cannon_d <= 5)) O->I = 1;   /* 8669 */
    if (O->I == 1 && A0 == 0 && !AS.voice_l) { AS.voice_l = 1; snd_play_orig(126, 220, 128); }   /* 8670: Carol's voice (playing Lilac) */
    if (O->I == 1 && milla && (O->fl & 0x20) && !AS.voice_m) { AS.voice_m = 1; snd_play_orig(126, 220, 128); }   /* 8671: her first turn (Milla) */
    int X = (int)o->x;
    if (O->I == 1 && O->A < vx - 50 && (A0 == 0 || (milla && (O->fl & 0x20)))) {   /* 8672: Carol on the bike (direction 1) */
        AS.g_carol = 1; AS.g_lilac = 0;
        O->A = X; O->B = (int)o->y; O->C = 5; O->I = 0; O->fl = (O->fl & ~2u) ^ 0x20; O->dir = 1; O->F = 0; O->E = 0; O->D = 0;
        AS.proj_vis = 0;
#ifdef FP_HOST_ENV
        if (AS.dbg) printf("fpas %d arrive carol %d\n", G.frame, X);
#endif
    } else if (O->I == 1 && O->A < vx - 50 && (carolp || (milla && !(O->fl & 0x20)))) {   /* 8673: Lilac (direction 0) */
        AS.g_lilac = 1; AS.g_carol = 0;
        O->A = X; O->B = (int)o->y; O->C = 5; O->I = 0; O->fl = (O->fl & ~2u) ^ 0x20; O->dir = 0; O->F = 0; O->E = 0; O->D = 0;
        AS.proj_vis = 1;
#ifdef FP_HOST_ENV
        if (AS.dbg) printf("fpas %d arrive lilac %d\n", G.frame, X);
#endif
    }
    int speed = -1;
    if (AS.g_carol) {
        o->x = (float)(O->A >= -50 ? (int)O->A : -50); o->y = (float)(int)O->B;   /* 8674 */
        O->A += O->C; O->B += O->D;
        if (O->A > vx + 100 && !(O->fl & 2)) O->C = fmax(O->C - 0.1, 1);         /* 8676 (8675: exhaust dust, not ported) */
        if (O->F == 0) { own_set(&AS.a_off, S_AOFF1); O->E = fmax(O->E - 1, 0); O->D += 0.2; O->B = fmin(O->B, O->G); }   /* 8677 */
        X = (int)o->x;
        if (O->F == 0 && O->E == 0 && nearhit && X >= (int)c->hit->x - 9 && X <= (int)c->hit->x + 39 && c->head_a <= 0) {   /* 8678 */
            O->F = 1; O->D = -8; O->G = O->B;
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d jump %d %.0f\n", G.frame, X, O->B);
#endif
        }
        if (O->F == 1) { O->D += 0.3; own_set(&AS.a_off, S_AOFF7B); }          /* 8679 */
        int fr = AS.a_off.frame;
        if (O->F == 1 && (fr == 2 || fr == 5 || fr == 7) && !(O->fl & 4)) {     /* 8680: the strike */
            snd_play_orig(137, 220, 128);
            combat_shield_at(o->x, o->y, 16, 1, 1, 1);
            O->fl |= 4; O->H = fmod(O->H + 1, 3);
            AS.last_hit = G.frame;
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d strike %d %d\n", G.frame, (int)o->x, (int)o->y);
#endif
        }
        shields_follow();                                                        /* 8681 */
        if (!(O->F == 1 && (fr == 2 || fr == 5 || fr == 7))) O->fl &= ~4u;      /* 8682 */
        if (O->F == 1 && O->D > 0 && O->B >= O->G) { O->fl |= 2; O->F = 0; O->D = 0; O->E = 30; O->B = O->G; O->C = -3;   /* 8683: landed */
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d land %d\n", G.frame, (int)o->x);
#endif
        }
        if (O->F == 0 && knocked(c)) {                                           /* 8684 */
            O->F = 2; snd_play_orig(25, 220, 128); O->D = -4; own_set(&AS.a_off, S_AOFF8B); O->C = -4;
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d knocked %d by %d\n", G.frame, (int)o->x, AS.knock_by);
#endif
        }
        if (O->F == 2) { O->D += 0.4; O->B = fmin(O->B, O->G); }                 /* 8685 */
        if (O->F == 2 && AS.a_off.done) { O->fl |= 2; O->F = 0; }               /* 8686: anim 8 over */
        if (O->A < 0 && O->I == 0) O->I = 600;                                   /* 8687 */
    } else if (AS.g_lilac) {
        o->x = (float)(int)O->A; o->y = (float)(int)O->B;                        /* 8688 (no -50 clamp), the projectile at Lilac */
        O->A += O->C; O->B += O->D;
        if (O->A > vx + 100 && !(O->fl & 2)) O->C = fmax(O->C - 0.1, 1);         /* 8689 */
        if (O->F == 0) { own_set(&AS.a_off, S_AOFF0); O->E = fmax(O->E - 1, 0); own_set(&AS.a_proj, S_APROJ); O->D += 0.2; O->B = fmin(O->B, O->G); }   /* 8690 */
        if (O->F == 0 && O->B >= O->G) O->D = 0;                                 /* 8691 */
        X = (int)o->x;
        if (O->F == 0 && O->E == 0 && nearhit && X >= (int)c->hit->x - 9 && X <= (int)c->hit->x + 39 && c->head_a <= 0) {   /* 8692 */
            O->F = 1; O->D = -5; snd_play_orig(288, 220, 128);
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d jump %d %.0f\n", G.frame, X, O->B);
#endif
        }
        if (O->F == 1) {                                                         /* 8693: anim 7 at 4 |D| + 30 */
            O->D += 0.1; own_set(&AS.a_off, S_AOFF7); own_set(&AS.a_proj, S_APROJ7); speed = (int)(4 * fabs(O->D) + 30);
        }
        if (O->F == 1 && G.frame % 6 == 0) {                                     /* 8694: an invisible shield (direction 8, A 2) + a ring sparkle */
            combat_shield_at(o->x, o->y, 8, 2, 0, 0);
            int rx = cross_rand(41), ry = cross_rand(41);
            obj_effect((float)((int)o->x - 20 + rx), (float)((int)o->y - 20 + ry), "ringsparkle");
            AS.last_hit = G.frame;
        }
        shields_follow();                                                        /* 8695 */
        if (O->F == 1 && O->D > 0 && O->B >= O->G) { O->fl |= 2; O->F = 0; O->D = 0; O->E = 30; O->B = O->G; O->C = -3;   /* 8697 */
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d land %d\n", G.frame, (int)o->x);
#endif
        }
        if (O->F == 0 && knocked(c)) {                                           /* 8698 */
            O->F = 2; snd_play_orig(25, 220, 128); O->D = -4; own_set(&AS.a_off, S_AOFF8); own_set(&AS.a_proj, S_APROJ8); O->C = -4;
#ifdef FP_HOST_ENV
            if (AS.dbg) printf("fpas %d knocked %d by %d\n", G.frame, (int)o->x, AS.knock_by);
#endif
        }
        if (O->F == 2) { O->D += 0.4; O->B = fmin(O->B, O->G); }                 /* 8699 */
        if (O->F == 2 && AS.a_off.done) { O->fl |= 2; O->F = 0; }               /* 8700 */
        if (O->A < 0 && O->I == 0) O->I = 600;                                   /* 8701 */
    }
    AS.off_speed = speed;
    if (AS.proj) { AS.proj->x = o->x; AS.proj->y = o->y; }
}

/* the box's petals on the treadmill: thrown items land on the scrolling floor (y 280, ScrollingFloor_2037's top) and ride it to
 * the left at the treadmill's 10 px a frame (8518) until collected or gone past the view (INFERRED: the port's thrown-item
 * rules only land on tiles; RV: Lilac idle 110 px left of the box collects all 6 petals 33-40 frames after the break) */
static void items_on_floor(void) {
    const float top = 280;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || (o->type != OT_ITEM_FLY && o->type != OT_PETAL) || o->y < top - 40) continue;
        if (o->type == OT_ITEM_FLY && o->vy > 0 && o->y + 4 >= top) { o->y = top - 4; o->vy = 0; o->vx = 0; }
        if (o->y >= top - 4 && (o->type == OT_PETAL || (o->vx == 0 && o->vy == 0))) { o->y = top - 4; o->x -= 10; }
        if (o->x < G.camx - 64) o->alive = 0;
    }
}

/* ===================================================================================================== hooks */
void fn_assist_frame(player_t *p, const fn_assist_ctx_t *c) {
    if (!AS.on) return;
    if (!AS.plane_mode) AS.t++;
    if (AS.grp) { plane_frame(p); items_on_floor(); }
    dust_move();
    if (AS.off && AS.off->alive) offense_frame(c);
    /* the helpers' own animators (8693's forced speed) */
    if (AS.g_carol || AS.g_lilac) {
        anim_tick(&AS.a_off, AS.off_speed);
        if (AS.g_lilac && AS.proj_vis) anim_tick(&AS.a_proj, AS.off_speed);
    }
#ifdef FP_HOST_ENV
    if (AS.dbg) printf("fpa %d pl %.0f %.0f G %.0f dir %d dA %.0f dB %.0f off %d %.0f %.0f F %.0f I %.0f C %.2f D %.2f E %.0f grp %d%d mon %d\n", G.frame,
                       AS.plane ? AS.plane->x : 0, AS.plane ? AS.plane->y : 0, AS.pl.G, AS.pl.dir, AS.df.A, AS.df.B, AS.off ? (int)AS.of.dir : -1,
                       AS.off ? AS.off->x : 0, AS.off ? AS.off->y : 0, AS.of.F, AS.of.I, AS.of.C, AS.of.D, AS.of.E, AS.g_carol, AS.g_lilac, AS.mon != NULL);
#endif
}
void fn_assist_end(void) {                                                       /* 8762 (the white flash): plane, Milla, the helper destroyed */
    if (!AS.on) return;
    if (AS.plane) AS.plane->alive = 0;
    if (AS.def) AS.def->alive = 0;
    if (AS.off) AS.off->alive = 0;
    if (AS.proj) AS.proj->hidden = 1;
    AS.plane = AS.def = AS.off = NULL; AS.proj_vis = 0; AS.g_carol = AS.g_lilac = 0; AS.grp = 0;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SHIELD && (G.o[i].fl & 0x10)) G.o[i].alive = 0;
}
/* achievement 21 "CPU Finish" (RE 8790 / 8792): a flag-4 ShieldInstaShield overlapping the hitbox when the head's A is 1 */
int fn_assist_cpu_finish(const obj_t *hit) {
    if (!hit) return 0;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *s = &G.o[i];
        if (s->alive && s->type == OT_SHIELD && (s->fl & 0x10) && combat_shield_overlaps(s, hit)) return 1;
    }
    return 0;
}
int fn_assist_last_hit(void) { return AS.last_hit; }

static void cast_draw(void);
void fn_assist_draw(int pass) {
    if (!AS.on) return;
    if (pass == 1 && AS.cast_on) cast_draw();
    if (pass == 0 && AS.proj && AS.proj_vis && AS.g_lilac && AS.a_proj.b && AS.off && AS.off->alive)   /* PlayerAssistProjectile (layer 2) */
        frame_draw(AS.a_proj.b, anim_frame(&AS.a_proj), floorf(AS.proj->x - G.camx), floorf(AS.proj->y - G.camy), 0, 0xffffffff);
    if (pass == 1 && AS.off && AS.off->alive && (AS.g_carol || AS.g_lilac) && AS.a_off.b)              /* AssistOffense (layer 3) */
        frame_draw(AS.a_off.b, anim_frame(&AS.a_off), floorf(AS.off->x - G.camx), floorf(AS.off->y - G.camy), 0, 0xffffffff);
}

/* ===================================================================================================== the closing scene (8764-8785) */
/* At C 2 the four heroines stand in for the player (MV.M 50: the player is not drawn): Lilac (PlayerSkinSash_341) sits at 345,
 * Carol lies at 530 (8769, every frame while D < 80), Torque and Milla wait off-screen at 120 (they came down from the plane).
 * D 80 / 110: Lilac / Carol get up (direction 30); D 130: Lilac walks +3 (anim 1, speed 30) until x >= Carol - 49 (8776 / 8777:
 * D 131, both stand, anim 40); D 131: Milla runs +5 (anim 2) to Lilac - 50, Torque walks +4 (anim 1) to Lilac - 90, Lilac turns
 * to them when Milla passes Lilac - 180 (8778-8782); both there: D 132 (8783), D + 1 (8784); D > 160: the victory poses (anim 37,
 * 8785, with Sign.B 179 = the results in fn_panther.c). RE + RV (session-18 traces: stops at 483 / 396 / 435, results at defeat +
 * 883). Returns the head's new D (the D 130 / 131 waits replace fn_panther.c's former INFERRED 113-frame hold). */
enum { CL, CC, CM, CT };
static void cast_set(int i, int k, int speed) {                                  /* force_animation (+ force_speed; -1: the table's) */
    int a = SA[k];
    AS.cast[i].speed = speed;
    if (a < 0 || (AS.cast[i].k == k && AS.cast[i].an.b)) return;
    AS.cast[i].k = k; anim_set(&AS.cast[i].an, &G.ens, a); anim_restart(&AS.cast[i].an);
}
int fn_assist_closing(int D) {
    if (!AS.cast_on) {                                                           /* 8764: monitors gone; 8765-8768: the skins created */
        AS.cast_on = 1;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_MON_RINGS) G.o[i].alive = 0;
        for (int i = 0; i < 4; i++) { AS.cast[i].y = 266; AS.cast[i].k = -1; AS.cast[i].an.b = NULL; }
        AS.cast[CL].x = 345; AS.cast[CC].x = 530; AS.cast[CM].x = AS.cast[CT].x = 120;
        cast_set(CL, C_L41S, -1); cast_set(CC, C_C41S, -1); cast_set(CM, C_M40, 0); cast_set(CT, C_T40, 0);
    }
    if (D < 80) { AS.cast[CL].x = 345; AS.cast[CC].x = 530; AS.cast[CM].x = AS.cast[CT].x = 120; }   /* 8769 */
    if (D < 130) D += 1;                                                         /* 8772 */
    if (D == 80) cast_set(CL, C_L41G, -1);                                       /* 8774: direction 30 (gets up) */
    if (D == 110) cast_set(CC, C_C41G, -1);                                      /* 8775 */
    if (D == 130) { cast_set(CL, C_L1, 30); AS.cast[CL].x += 3; }                /* 8776 */
    if (D == 130 && AS.cast[CL].x >= AS.cast[CC].x - 49) { D = 131; cast_set(CL, C_L40, 0); cast_set(CC, C_C40L, 0); }   /* 8777 */
    if (D == 131) {
        float lx = AS.cast[CL].x;
        if (AS.cast[CM].x < lx - 50) { cast_set(CM, C_M2, 30); AS.cast[CM].x += 5; }   /* 8778 */
        if (AS.cast[CM].x >= lx - 180 && !AS.cast_turned) { AS.cast_turned = 1; cast_set(CL, C_L40L, 0); }   /* 8779: Lilac turns */
        if (AS.cast[CM].x >= lx - 50) cast_set(CM, C_M40, 0);                    /* 8780 */
        if (AS.cast[CT].x < lx - 90) { cast_set(CT, C_T1, 30); AS.cast[CT].x += 4; }   /* 8781 */
        if (AS.cast[CT].x >= lx - 90) cast_set(CT, C_T40, 0);                    /* 8782 */
        if (AS.cast[CT].x >= lx - 90 && AS.cast[CM].x >= lx - 100) D = 132;      /* 8783 */
    }
    if (D > 131) D += 1;                                                         /* 8784 */
    if (D > 160 && !AS.cast_win) {                                               /* 8785: anim 37, restore_speed */
        AS.cast_win = 1;
        cast_set(CL, C_L37L, -1); cast_set(CC, C_C37L, -1); cast_set(CM, C_M37, -1); cast_set(CT, C_T37, -1);
    }
    for (int i = 0; i < 4; i++) if (AS.cast[i].an.b) anim_tick(&AS.cast[i].an, AS.cast[i].speed);
#ifdef FP_HOST_ENV
    if (AS.dbg) printf("fpcast %d D %d L %.0f %d C %.0f %d M %.0f %d T %.0f %d\n", G.frame, D, AS.cast[CL].x, AS.cast[CL].k, AS.cast[CC].x, AS.cast[CC].k,
                       AS.cast[CM].x, AS.cast[CM].k, AS.cast[CT].x, AS.cast[CT].k);
#endif
    return D;
}
int fn_assist_cast_on(void) { return AS.cast_on; }                              /* MV.M 50: the player itself is not drawn */
static void cast_draw(void) {
    for (int i = 0; i < 4; i++)
        if (AS.cast[i].an.b) frame_draw(AS.cast[i].an.b, anim_frame(&AS.cast[i].an), floorf(AS.cast[i].x - G.camx), floorf(AS.cast[i].y - G.camy), 0, 0xffffffff);
}

