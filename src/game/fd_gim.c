/* Final Dreadnought gimmicks (session 24): types 2752-2911 (fd.h OT_FDG_*). Spec: build/re/spec_fd_gimmicks.md.
 * Labels: RE (decompiled events / constructors of handle_frame_66..73 = fd1a..fd4b and their start events,
 * build/re/Frames__event_func_*.a.c, create_*_int__int.a.c; qualifiers from on_frame_66..73_init), RV (original traces,
 * build/orig_traces/fd_gim), INFERRED (marked).
 * Families and their events (acts: 1 fd1a .. 8 fd4b):
 *   transporter pipes (all acts): 15188-15201 (starts, start lights, nodes, the M 95 ride, the pads' exit);
 *   zap beam cannons (fd1a, fd1b, fd2b): 15203-15220; spiral spark makers / tower lights (fd1a, fd1b, fd2a): 15221-15228, foreach_st_65;
 *   downhill platforms (fd1a, fd1b): 15229-15238; airlocks (fd1b): 15357-15372, foreach_air_66 / foreach_airlock_66;
 *   pressure doors (fd1b): 15373-15382; the Syntax treadmill floors (fd1b): 15341-15344 (fd_gim_scroll_floor);
 *   hangar walls (fd1a's opening): 15299 / 15300 (fd_gim_hangar_break).
 * Reused systems: Battle Glacier's transporter pads / sirens / spin mines / antigrav spawners (bg_gim.c run them by type), the
 * objects.c path-platform registry (downhill platforms: scripted I / J), player.c's ST_RIDE hook (the pipe M 95), the Q7 / Q8 / Q9
 * groups (objdefs.c flags; flag 0 = ignored by the sensors), Milla's cube rectangle (player_cube_rect: the zap beam's PlayerReflect).
 * Passes: fd_gim_pre (downhill platforms' I / J before the platform pass), fd_gim_frame (after the player block, in event order),
 * fd_gim_player (ST_RIDE), fd_gim_draw (the y-scaled zap beams). Object fields: par = family member (importer FDG_SUB),
 * layer = instance layer; av[] = class alterables A..H as named per family. */
#include "fd.h"
#include "bg.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define RAD 57.29577951308232
#define RT0 OT_FDG_TSTART
#define NRT (OT_FDG_END - RT0 + 1)
#define LMAX 160                                   /* fd4a: 103 laser tunnel platforms */
#define F0 0x1                                     /* the object's flag 0 (sensors ignore it) */
#define F1 0x2                                     /* flag 1 */
#define F2 0x4                                     /* flag 2 */
#define F3 0x8                                     /* flag 3 */
#define NPAD 48
static struct {
    short lst[NRT][LMAX]; short nls[NRT];
    short pad[NPAD]; int npad;                     /* ObjectTransporter_3360 pads (bg_gim.c's OT_BGG_TRANSP) */
    int on, act;
    int ride;                                      /* 95 while travelling a pipe (MV.M 95) */
    double rx, ry, ra;                             /* MV.X / MV.Y during the ride, the wave's A (direction) */
    int wave;                                      /* ObjectTransporterWave flag 1: the exit effect is due (15201) */
    int snd_until[400];                            /* "sample not playing" (INFERRED from the sample length) */
    int snd_cool[8];                               /* the events' 2-frame sound cool-downs (Frames + 0x22f4c ...) */
    int floor_on; float floor_sx, floor_sy; double floor_o;   /* the Syntax treadmill (fd_gim_scroll_floor) */
    int wind_ch;                                   /* sound 381's loop channel or -1 */
} S;
#define LST S.lst
#define NLS S.nls
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]

static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static int PXI(const player_t *p) { return (int)floor(p->x); }    /* PLAYER (Player_326) sits at (int X, int Y) */
static int PYI(const player_t *p) { return (int)floor(MY(p)); }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / RAD); }                   /* Chowdren sin: 0 at exactly 180 */
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / RAD); }    /* cos: 0 at exactly 90 / 270 */
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int hurt(const player_t *p) { return p->state == ST_HURT || dead(p); }                       /* M 8 / 9 / 10 */
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }   /* MV.R 0, OTHERS.E <= 0 */
static void snd(int id) { snd_play_orig(id, 220, 128); }
static void snd_cool(int k, int id) { if (G.frame >= S.snd_cool[k]) { S.snd_cool[k] = G.frame + 2; snd(id); } }   /* every 2nd frame at most */
static void shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }                     /* CAM.Z = max(CAM.Z, z) */
static int g3(void) { return G.frame; }                                         /* global value 3 (+1 a frame; INFERRED phase: the stage frame) */
static float trunc_i(double v) { return (float)(int)v; }                        /* FrameObject::set_x / set_y take an int */
static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void set_frame(obj_t *o, int f) {                                        /* Active::force_frame (held: speed-0 rows) */
    if (!o->an.b) return;
    int n = o->an.b->anims[o->an.anim].count;
    o->an.frame = n > 0 ? ((f % n) + n) % n : 0; o->an.acc = 0;
}
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static obj_t *spawn(int type, const char *anim, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame; o->par = -1; o->tm = -1;
    if (anim) { set_s(o, anim); if (o->an.b) anim_restart(&o->an); }
    lst_add(o);
    return o;
}
static void box_of(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {   /* the current frame's image box */
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - (o->flip ? f->w - 1 - f->hx : f->hx); *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int rect_disc(float x0, float y0, float x1, float y1, const player_t *p) {   /* a box vs PLAYER's 27-px disc */
    double X = PXI(p), Y = PYI(p), cx = fmax(x0, fmin(X, x1 - 1)), cy = fmax(y0, fmin(Y, y1 - 1));
    return (cx - X) * (cx - X) + (cy - Y) * (cy - Y) <= 13.5 * 13.5;
}
static void char_rect(const player_t *p, int *x0, int *y0, int *x1, int *y1) {      /* PlayerCharacterSensor (inclusive) */
    int X = PXI(p), Y = PYI(p), small = player_char_sensor_small(p);
    *x0 = X - 7; *x1 = X + 8; *y0 = small ? Y - 13 : Y - 34; *y1 = small ? Y + 14 : Y + 12;
}
static int csens_hit(const player_t *p, const obj_t *o) {                       /* PLAYERCHARACTERSENSOR on the object's mask */
    int x0, y0, x1, y1; char_rect(p, &x0, &y0, &x1, &y1);
    return o->an.b && obj_rect_hit(o, x0, y0, x1 + 1, y1 + 1);
}
static int sens_hit(int s, const obj_t *o) {                                    /* a placed player sensor overlapping the object */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int on_bg(const obj_t *o) {                     /* Active::overlaps_background (INFERRED: the image box's edges, every 4 px) */
    float x0, y0, x1, y1; if (!o->an.b) return 0; box_of(o, &x0, &y0, &x1, &y1);
    for (int x = (int)x0; x < (int)x1; x += 4) if (px_solid(&G.lvl, x, (int)y1 - 1) || px_solid(&G.lvl, x, (int)y0)) return 1;
    for (int y = (int)y0; y < (int)y1; y += 4) if (px_solid(&G.lvl, (int)x1 - 1, y) || px_solid(&G.lvl, (int)x0, y)) return 1;
    return 0;
}
/* Chowdren's index pairing (spec_bg_gimmicks "Method notes"): A's i-th instance from the end takes B's (i mod nB)-th from the end */
static void link_pairs(int ta, int tb) {
    int na = N(ta), nb = N(tb);
    for (int a = 0; a < na; a++) {
        obj_t *o = &G.o[LST[ta - RT0][a]];
        int i = na - 1 - a;
        o->tm = nb ? LST[tb - RT0][nb - 1 - (i % nb)] : -1;
    }
}
static obj_t *partner(const obj_t *o, int type) {
    if (o->tm < 0 || o->tm >= G.nobjs) return NULL;
    obj_t *q = &G.o[o->tm];
    return q->alive && q->type == type ? q : NULL;
}
static obj_t *fx(const char *anim, float x, float y, float vx, float vy, int life) {   /* a one-shot moving effect (OT_FDG_FX) */
    obj_t *e = spawn(OT_FDG_FX, anim, x, y, 3);
    if (!e) return NULL;
    if (!e->an.b) { e->alive = 0; return NULL; }
    e->vx = vx; e->vy = vy; e->av[1] = life;
    return e;
}

/* ================================================================ 1. transporter pipes (all acts; RE 15188-15201)
 * Start (64 x 64, hot spot 32, 32; ctor A 90 = up), Start2 (the light orbiting its start), nodes 0 / 90 / 180 / 270 (64 x 64 markers),
 * the pads (ObjectTransporter_3360, bg_gim.c's Q7 OT_BGG_TRANSP) and the wave (ObjectTransporterWave_3415: A = the direction).
 * 15189-15191: a node 0 / 180 / 270 on a start sets the start's A and is destroyed (direction markers: done once at the spawn).
 * 15192: PLAYER on a start, M not 95 / 8 / 9 / 10: M 95, wave A = start A, X, Y = the start, sound 345, wave flag 1.
 * 15193 / 15194 (M 95): OTHERS.D >= 2, T = U = V = 0 (no sensors), OTHERS.Y 8 (the ball) at speed 100, X += 20 cos A,
 * Y -= 20 sin A, OTHERS.E >= 15 (no hurt), the wave at PLAYER with a spade afterimage (INFERRED: left out, the ball is drawn).
 * 15195-15198: a node with x in (X - 12, X + 12), y in (Y - 32, Y + 32) and another direction: X, Y = the node, sound 345, A = the
 * node's. 15199: a pad with x in (X - 12, X + 12), y in (Y - 28, Y - 4): M 0, T = U = V = 1, X = pad.x, Y = pad.y - 16 (she stands
 * on it). 15200: M 95 outside the frame (X <= 0, Y <= 0, X > W, Y > H): M 0. 15201: the ride over with the wave's flag 1: an
 * EnemyEliteSpawnerWave at (X, Y + 16), sound 346. */
static int ride_anim(player_t *p) {
    if (chr_is_milla(G.character)) player_pose(p, 7, p->face > 0 ? 0 : 16, 100);
    else player_ride_anim(p, "charge", 100);
    return 1;
}
static void ride_end(player_t *p) {
    S.ride = 0;
    if (p->state == ST_RIDE) { p->state = ST_NORMAL; p->state_t = 0; }
}
int fd_gim_player(player_t *p, const input_t *in) {
    (void)in;
    if (!S.ride || p->state != ST_RIDE) return 0;
    return ride_anim(p);
}
int fd_gim_ride_m(void) { return S.ride ? S.ride : -1; }
int fd_gim_ride(void) { return S.ride; }
static void ride_place(player_t *p) {
    p->x = S.rx; p->y = S.ry + ORIG_REF_DY;
    p->vx = p->gsp = 0; p->vy = 0; p->ground = 0; p->cY = 8;
    if (p->invuln < 15) p->invuln = 15;                                         /* OTHERS.E = max(E, 15) */
    player_sync_sensors(p);
}
static void transporters(player_t *p) {
    obj_t *o;
    EACH(OT_FDG_TSTART2, o) {                                                   /* 15188: 24 px around its start, A += 10 */
        obj_t *s = partner(o, OT_FDG_TSTART);
        if (!s) continue;
        o->x = trunc_i(24 * c_cos(o->av[0]) + floorf(s->x)); o->y = trunc_i(24 * c_sin(o->av[0]) + floorf(s->y));
        o->av[0] = fmod(o->av[0] + 10, 360.);
    }
    if (S.ride && p->state != ST_RIDE) S.ride = 0;                              /* killed / hurt out of it (INFERRED) */
    if (!S.ride && !hurt(p) && p->state != ST_RIDE) {                           /* 15192 */
        EACH(OT_FDG_TSTART, o) {
            if (!o->an.b) continue;
            float x0, y0, x1, y1; box_of(o, &x0, &y0, &x1, &y1);
            if (!rect_disc(x0, y0, x1, y1, p)) continue;
            S.ride = 95; S.ra = o->av[0]; S.rx = floorf(o->x); S.ry = floorf(o->y); S.wave = 1;
            snd(345);
            p->state = ST_RIDE; p->state_t = 0; p->springing = 0; p->jumping = 0; p->m1 = 0;
            ride_place(p);
            return;                                                             /* (the movement starts next frame: 15194 ran before) */
        }
    }
    if (!S.ride) {
        if (S.wave) {                                                           /* 15201 */
            S.wave = 0;
            fx("fdg_wave", (float)PXI(p), (float)(PYI(p) + 16), 0, -4, 15); snd(346);
        }
        return;
    }
    S.rx += 20 * c_cos(S.ra); S.ry += -20 * c_sin(S.ra);                         /* 15194 */
    EACH(OT_FDG_TNODE, o) {                                                     /* 15195-15198 */
        double a = 90. * (o->par & 3), nx = floorf(o->x), ny = floorf(o->y);
        if (S.ra == a || !(S.rx - 12 < nx && nx < S.rx + 12) || !(S.ry - 32 < ny && ny < S.ry + 32)) continue;
        S.rx = nx; S.ry = ny; S.ra = a; snd(345);
    }
    for (int k = 0; k < S.npad; k++) {                                          /* 15199 */
        o = &G.o[S.pad[k]];
        if (!o->alive || o->type != OT_BGG_TRANSP) continue;
        double nx = floorf(o->x), ny = floorf(o->y);
        if (!(S.rx - 12 < nx && nx < S.rx + 12) || !(S.ry - 28 < ny && ny < S.ry - 4)) continue;
        S.rx = nx; S.ry = ny - 16;
        ride_place(p); ride_end(p); p->cY = 0;
        return;
    }
    if (S.rx <= 0 || S.ry <= 0 || S.rx > G.lvl.w || S.ry > G.lvl.h) { ride_end(p); return; }   /* 15200 */
    ride_place(p);
}

/* ================================================================ 2. zap beam cannons (fd1a, fd1b, fd2b; RE 15203-15220)
 * cannon: A angle (ctor 270), B target, C beam on (frames), D clock, E / F the two targets (240 / 300), flag 1 = which one.
 * 15203: cannon angle A + 90; beam and hitbox at their cannon (index pairing), angle A + 90; beam direction min(C, 1); hitbox A = C;
 * D + 1. 15204 / 15205: A -> B by 10. 15206 / 15207: D == 40: flag 1 toggles, B = E / F (D + 1 there, as the events do).
 * 15208: D == 60 in view (200): sound 358, CAM.Z >= 3. 15209: D == 60: C = 30. 15210: D > 80: D = 0. 15211: C > 0: C - 1.
 * 15212: the hitbox (A > 0) on PLAYERCHARACTERSENSOR and off PlayerReflect (Milla's cube), MV.R 0, OTHERS.E <= 0: hurt 10.
 * 15216-15219: beam / hitbox y scale + 1 (max 24) off the reflect, - 1 on it. Beam / hitbox image 18 x 10, hot spot (9, 0):
 * the beam reaches 10 x scale px from the cannon's centre along its angle. (15213 / 15214: the beam hits Q20 enemies too: the
 * enemies module's; INFERRED left out.) */
#define ZB_W 9.0
static void zb_axis(const obj_t *b, double *ux, double *uy) {                   /* the image's +y axis in the world (angle b->ang) */
    double a = b->ang / RAD; *ux = sin(a); *uy = cos(a);
}
static int zb_cube(const obj_t *b, double len) {                                /* the beam (len px) on Milla's cube rectangle */
    double ux, uy; zb_axis(b, &ux, &uy);
    float cx, cy;
    if (!player_cube_xy(&cx, &cy)) return 0;
    for (double d = 0; d <= len; d += 6) {
        int x = (int)(floorf(b->x) + ux * d), y = (int)(floorf(b->y) + uy * d);
        if (player_cube_rect(x - 4, y - 4, x + 5, y + 5)) return 1;
    }
    return 0;
}
static int zb_hits_rect(const obj_t *b, double len, int x0, int y0, int x1, int y1) {   /* the rotated 18 x len box on a rectangle */
    double ux, uy; zb_axis(b, &ux, &uy);
    for (double d = 0; d < len; d += 4)
        for (int s = -1; s <= 1; s++) {
            double x = floorf(b->x) + ux * d + uy * s * (ZB_W - 1), y = floorf(b->y) + uy * d - ux * s * (ZB_W - 1);
            if (x >= x0 && x <= x1 && y >= y0 && y <= y1) return 1;
        }
    return 0;
}
static void zaps(player_t *p) {
    obj_t *c, *b;
    EACH(OT_FDG_ZCANNON, c) {
        double *A = &c->av[0], *B = &c->av[1], *C = &c->av[2], *D = &c->av[3];
        c->ang = (float)fmod(*A + 90, 360.);                                    /* 15203 */
        *D += 1;
        if (*A > *B) *A -= 10; else if (*A < *B) *A += 10;                      /* 15204 / 15205 */
        if (*D == 40) { c->fl ^= F1; *B = (c->fl & F1) ? c->av[4] : c->av[5]; *D += 1; }   /* 15206 / 15207 */
        if (*D == 60) { if (obj_near_view(c, 200)) { snd(358); shake(3); } *C = 30; }   /* 15208 / 15209 */
        if (*D > 80) *D = 0;                                                    /* 15210 */
        if (*C > 0) *C -= 1;                                                    /* 15211 */
    }
    int hurtme = 0, X0, Y0, X1, Y1; char_rect(p, &X0, &Y0, &X1, &Y1);
    EACH(OT_FDG_ZBEAM, b) {
        if (!(c = partner(b, OT_FDG_ZCANNON))) continue;
        b->x = c->x; b->y = c->y; b->ang = c->ang;
        double C = c->av[2];
        b->dir = C > 1 ? 1 : (int)C;
        set_s(b, b->dir ? "fdg_zbeam1" : "fdg_zbeam0");
        double *sc = &b->av[0];                                                 /* the y scale */
        if (zb_cube(b, 10 * *sc)) *sc -= 1;                                     /* 15218 */
        else if (S.act == 4 || S.act == 1 || S.act == 2) *sc = fmin(*sc + 1, 24);   /* 15216 (INFERRED in fd1a / fd1b: see the spec) */
        if (*sc < 0) *sc = 0;
        if (C > 0 && !dead(p) && zb_hits_rect(b, 10 * *sc, X0, Y0, X1, Y1)) hurtme = 1;   /* 15212 (the hitbox follows the beam) */
    }
    if (hurtme && vulnerable(p)) player_contact(p, 10);
}

/* ================================================================ 3. spiral spark makers / tower lights (fd1a, fd1b, fd2a; RE 15221-15228)
 * maker: A clock (+1 a frame), frame (g3 / 3) mod 6 (lights too). 15222: A <= 30 -> direction 1 (charging). 15223: A > 60 in view
 * (100): sound 274 (every 2nd frame at most). 15224 / foreach_st_65: A > 60 -> direction 0, A = -90, two HazardSpiralSparks at the
 * maker (A 0 and 180, C = x, ctor B -4) and a white burst. spark: 15221 x = C + 40 sin A, y += B, B = min(B + 0.4, 3),
 * A = (A + 4) mod 360; 15225: B >= 3 -> E + 1; 15226: on a tower light -> E = 20; 15227: E > 35 -> gone (white burst);
 * 15228: on PLAYERCHARACTERSENSOR, MV.R 0, OTHERS.E <= 0: hurt 10, the spark bursts. */
static void sparks(player_t *p) {
    obj_t *o, *l;
    int fr = (g3() / 3) % 6;
    EACH(OT_FDG_SLIGHT, o) set_frame(o, fr);
    EACH(OT_FDG_SMAKER, o) {
        o->av[0] += 1;                                                          /* 15221 */
        set_s(o, o->av[0] <= 30 ? "fdg_smaker1" : "fdg_smaker"); set_frame(o, fr);   /* 15222 */
        if (o->av[0] > 60) {
            if (obj_near_view(o, 100)) snd_cool(0, 274);                        /* 15223 */
            o->av[0] = -90; set_s(o, "fdg_smaker"); set_frame(o, fr);           /* 15224 / foreach_st_65 */
            for (int k = 0; k < 2; k++) {
                obj_t *s = spawn(OT_FDG_SPARK, "fdg_spark", o->x, o->y, o->layer ? o->layer : 2);
                if (s) { s->av[0] = k ? 180 : 0; s->av[1] = -4; s->av[2] = floorf(o->x); s->av[3] = floorf(o->y); s->av[4] = 0; }
            }
            obj_effect(o->x, o->y, "wburst");
        }
    }
    int hit = 0;
    EACH(OT_FDG_SPARK, o) {
        if (o->born == G.frame) continue;
        double *A = &o->av[0], *B = &o->av[1], *E = &o->av[4];
        o->x = trunc_i(o->av[2] + 40 * c_sin(*A)); o->y = trunc_i(floorf(o->y) + *B);   /* 15221 */
        *B = fmin(*B + 0.4, 3); *A = fmod(*A + 4, 360.);
        if (*B >= 3) *E += 1;                                                   /* 15225 */
        EACH(OT_FDG_SLIGHT, l) { float x0, y0, x1, y1; box_of(l, &x0, &y0, &x1, &y1); if (obj_rect_hit(o, (int)x0, (int)y0, (int)x1, (int)y1)) { *E = 20; break; } }   /* 15226 */
        if (*E > 35) { obj_effect(o->x, o->y, "wburst"); o->alive = 0; continue; }   /* 15227 */
        if (!hit && !dead(p) && csens_hit(p, o) && vulnerable(p)) { hit = 1; obj_effect(o->x, o->y, "wburst"); o->alive = 0; }   /* 15228 */
    }
    if (hit) player_contact(p, 10);
}

/* ================================================================ 4. downhill platforms (fd1a, fd1b; RE 15229-15238)
 * platform (Q8 Q9 Q13, 160 x 32, hot spot 80, 0): the objects.c path rule with C = G = 0 (X = floor(I), Y = floor(J)); Q, R = the
 * start (av[0] / av[1]), S speed (av[2]), T distance (av[3]), U state (av[4]: 0 home, 1 rolling, 2 back). 15229 / 15230: flag 1 =
 * Lilac stands on it (L 1, BottomBig). 15231: I = Q + T, J = R + 0.5 T, T += S; the wheel at (x, y + 48), angle - 1.5 S.
 * 15232: U 0 and flag 1: U 1, direction 1, sound 359. 15233: U 1 without flag 3: S = min(S + 0.2, 15). 15234: U 1, S > 2, on the
 * background: flag 2, S = -0.5 S, sound 360. 15235: U 1, 0 < S <= 2, on the background: flags 2 and 3, S 0. 15236: U 1, flag 3,
 * not stood on: U 2, sound 359. 15237: U 2: S = max(S - 0.2, -15). 15238: U 2 and T <= 0: U = T = S = 0 (home). */
static void downhill_pre(player_t *p) {
    obj_t *o;
    EACH(OT_FDG_DHPLAT, o) {
        o->fl &= ~F1;                                                           /* 15229 */
        if (p->ground && !dead(p) && mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, o->type) == (int)(o - G.o)) o->fl |= F1;   /* 15230 */
        double *Q = &o->av[0], *R = &o->av[1], *Sp = &o->av[2], *T = &o->av[3], *U = &o->av[4];
        double t = *T;
        o->ox = (float)(*Q + t); o->oy = (float)(*R + 0.5 * t); *T = t + *Sp;   /* 15231 */
        if (*U == 0 && (o->fl & F1)) { *U = 1; o->fl &= ~(F2 | F3); set_s(o, "fdg_dh1"); snd(359); }   /* 15232 */
        if (*U == 1 && !(o->fl & F3)) *Sp = fmin(*Sp + 0.2, 15);                /* 15233 */
        if (*U == 1 && *Sp > 2 && on_bg(o)) { o->fl |= F2; *Sp *= -0.5; snd(360); }   /* 15234 */
        else if (*U == 1 && *Sp > 0 && *Sp <= 2 && on_bg(o)) { o->fl |= F2 | F3; *Sp = 0; }   /* 15235 */
        if (*U == 1 && !(o->fl & F1) && (o->fl & F3)) { *U = 2; snd(359); }      /* 15236 */
        if (*U == 2) *Sp = fmax(*Sp - 0.2, -15);                                /* 15237 */
        if (*U == 2 && *T <= 0) { *U = *T = *Sp = 0; o->fl &= ~(F2 | F3); set_s(o, "fdg_dh0"); }   /* 15238 */
    }
}
static void downhill(void) {
    obj_t *w;
    EACH(OT_FDG_DHWHEEL, w) {                                                   /* 15231: the wheel under its platform */
        obj_t *o = partner(w, OT_FDG_DHPLAT);
        if (!o) continue;
        w->x = o->x; w->y = floorf(o->y) + 48;
        w->ang = (float)fmod(w->ang - 1.5 * o->av[2], 360.);
        set_s(w, o->av[4] ? "fdg_dhw1" : "fdg_dhw0");
    }
}

/* ================================================================ 5. airlocks (fd1b; RE 15357-15372, foreach_air_66 / foreach_airlock_66)
 * base (invisible 32 x 32): A / B / C the openings of the lower door (Airlock4), the middle pair (Airlock2 / 3) and the top door
 * (Airlock1), D 1 once Lilac comes within 159 px (x; 15361), E the suction. 15362-15364 (D 1): A + 16 up to 192, then B, then C.
 * 15359: Airlock1 x = base - C, Airlock4 x = base + A, Airlock2 x = base - B, Airlock3 x = base + B (index pairing). 15360: the doors'
 * sound 279; 15365 / 15366 (D 1, C > 0, flag 1 off): the wind loop (sound 381), flag 1; 15367 / 15368: wind streaks (5 x 32) fall
 * through the view (B 20) while flag 1 is on. 15370 / 15372: Lilac over an open airlock (within 112 px, x) is pulled down
 * (G + 0.3; at G >= 15, E = min(E + 0.3, 30) is added to Y) unless she holds a clothesline. INFERRED: flag 2 (the pull) = C at 192
 * and Lilac below the doors' top; the subtitle (15357 / 15358) is the flow's. */
static void airlocks(player_t *p) {
    obj_t *b, *o;
    int pull = 0;
    EACH(OT_FDG_AIRBASE, b) {
        double *A = &b->av[0], *Bv = &b->av[1], *C = &b->av[2], *D = &b->av[3];
        if (*D == 0 && abs((int)floorf(b->x) - PXI(p)) <= 159) { *D = 1; snd(279); }   /* 15361 / 15360 */
        if (*D == 1) {                                                          /* 15362-15364 */
            if (*A < 192) *A = fmin(*A + 16, 192);
            else if (*Bv < 192) *Bv = fmin(*Bv + 16, 192);
            else if (*C < 192) *C = fmin(*C + 16, 192);
        }
        if (*D == 1 && *C > 0 && !(b->fl & F1)) { b->fl |= F1; if (S.wind_ch < 0 && obj_near_view(b, 100)) S.wind_ch = snd_loop_orig(381, 160, 0); }   /* 15365 / 15366 */
        if (*C >= 192 && abs((int)floorf(b->x) - PXI(p)) < 112 && PYI(p) >= floorf(b->y) - 48) { b->fl |= F2; pull = 1; } else b->fl &= ~F2;
        if ((b->fl & F1) && obj_near_view(b, 0) && (g3() % 3) == 0)            /* 15367: wind streaks (INFERRED rate) */
            { obj_t *w = spawn(OT_FDG_WIND, "fdg_wind", floorf(b->x) - 90 + cross_rand(180), G.camy - 16, 3); if (w) w->av[1] = 20; }
    }
    EACH(OT_FDG_AIRLOCK_S, o) {                                                 /* 15359 */
        if (!(b = partner(o, OT_FDG_AIRBASE))) continue;
        o->x = trunc_i(floorf(b->x) + (o->par & 1 ? b->av[0] : -b->av[2]));
        if (o->par & 1 ? b->av[0] >= 192 : b->av[2] >= 192) o->fl |= F0; else o->fl &= ~F0;   /* (INFERRED) a fully open door no longer blocks */
    }
    EACH(OT_FDG_AIRLOCK, o) { if ((b = partner(o, OT_FDG_AIRBASE))) o->x = trunc_i(floorf(b->x) + (o->par & 1 ? b->av[1] : -b->av[1])); }
    EACH(OT_FDG_WIND, o) {                                                      /* 15368 / 15369 */
        if (o->born == G.frame) continue;
        o->av[0] += o->av[1]; o->y = trunc_i(G.camy - 16 + o->av[0]);
        if (o->y > G.camy + 270) o->alive = 0;
    }
    int any = 0; EACH(OT_FDG_AIRBASE, b) if (b->fl & F1 && obj_near_view(b, 100)) any = 1;
    if (!any && S.wind_ch >= 0) { snd_loop_stop(S.wind_ch); S.wind_ch = -1; }
    static double E;
    if (pull && !dead(p) && p->state != ST_HANG) {                              /* 15370 / 15372 */
        if (p->ground) { p->vx = p->gsp; p->ground = 0; }
        p->vy += 0.3;
        if (p->vy >= 15) { E = fmin(E + 0.3, 30); p->y += E; player_sync_sensors(p); }
    } else E = 0;                                                               /* 15371 */
}

/* ================================================================ 6. pressure doors (fd1b; RE 15373-15382)
 * door 1 (from above, ctor B -192) / door 2 (from below, B 192): y = A + B (15373; A = the placed y: RV pd2), C state. 15374 / 15377 /
 * 15380: C 0, Lilac more than 60 px right of the door and the door's A in (Y - 240, Y): C 1 (door 3, the Q7 wall: y + B, it drops
 * into place). 15375 / 15378: C 1: B -> 0 by 16. 15376 / 15379: C 1 and B 0: C 2, the closed animation, CAM.Z >= 5, sound 361.
 * 15381 / 15382: a closed-in door (C > 0) on both TopBig and BottomBig while both touch something: Lilac is crushed (M 9). */
static void pdoors(player_t *p) {
    obj_t *o;
    int X = PXI(p), Y = PYI(p), crush = 0;
    EACH(OT_FDG_PDOOR, o) {
        double *A = &o->av[0], *B = &o->av[1], *C = &o->av[2];
        o->y = trunc_i(*A + *B);                                                /* 15373 */
        if (*C == 0 && X - 60 > floorf(o->x) && *A > Y - 240 && *A < Y) *C = 1;   /* 15374 / 15377 */
        if (*C == 1) *B = o->par & 1 ? fmax(*B - 16, 0) : fmin(*B + 16, 0);     /* 15375 / 15378 */
        if (*C == 1 && *B == 0) { *C = 2; shake(5); snd(361); }                 /* 15376 / 15379 */
        if (*C > 0 && *B > -48 && *B < 48 && !dead(p) && sens_hit(SENS_TOPBIG, o) && sens_hit(SENS_BOTTOMBIG, o)) crush = 1;   /* 15381 / 15382 */
    }
    EACH(OT_FDG_PDOOR3, o) {                                                    /* 15380 */
        double *A = &o->av[0], *B = &o->av[1], *C = &o->av[2];
        if (*C == 0 && X - 60 > floorf(o->x) && *A > Y - 240 && *A < Y) { *C = 1; o->y = trunc_i(floorf(o->y) + *B); o->hidden = 0; o->fl &= ~F0; }
    }
    if (crush) player_damage(p, 999);                                           /* MV.M 9, R 1, rings 0: the crush kills */
}

/* ================================================================ 7. hangar walls (fd1a; RE 15299 / 15300), treadmill floors (fd1b; 15341-15344) */
void fd_gim_hangar_break(void) {
    obj_t *o; int n = 0;
    if (!S.on) return;
    EACH(OT_FDG_HANGAR, o) {
        if (o->hidden) continue;
        o->hidden = 1; n++;
        for (int k = 0; k < 3; k++) fx("fdg_hpiece", o->x + 32 + cross_rand(41) - 20, o->y + 32 + cross_rand(41) - 20, (float)(cross_rand(9) - 2), (float)(-4 - cross_rand(5)), 40);
    }
    if (n) { snd(354); shake(5); }
}
void fd_gim_scroll_floor(int on, float sx, float sy) { S.floor_on = on; S.floor_sx = sx; S.floor_sy = sy; }
static void floors(void) {                                                      /* 15341-15344: 15 px a frame to the left, 128-px pieces */
    obj_t *o;
    if (!S.floor_on) return;
    EACH(OT_FDG_SFLOOR, o) {
        o->av[0] += 15;
        if (o->av[0] > 800) o->av[0] -= 640;                                    /* 15342 / 15343 (INFERRED: pieces recycled, not recreated) */
        o->x = trunc_i(S.floor_sx + o->av[1] - o->av[0]);
        o->y = trunc_i(S.floor_sy + (o->par & 1 ? -208 : 0));
    }
}

/* ================================================================ effects */
static void effects(void) {
    obj_t *o;
    EACH(OT_FDG_FX, o) {
        if (o->born == G.frame) continue;
        o->x += o->vx; o->y += o->vy; o->av[0] += 1;
        if (o->av[0] > o->av[1]) o->alive = 0;
    }
}

/* ================================================================ spawn / frame / draw */
static const char *const AN0[] = {                                              /* the spawn rows by type */
    [OT_FDG_TSTART - RT0] = "fdg_tstart", [OT_FDG_TSTART2 - RT0] = "fdg_tstart2",
    [OT_FDG_ZCANNON - RT0] = "fdg_zcannon", [OT_FDG_ZBEAM - RT0] = "fdg_zbeam0",
    [OT_FDG_SMAKER - RT0] = "fdg_smaker", [OT_FDG_SLIGHT - RT0] = "fdg_slight",
    [OT_FDG_DHPLAT - RT0] = "fdg_dh0", [OT_FDG_DHWHEEL - RT0] = "fdg_dhw0", [OT_FDG_LOWSPRING - RT0] = "fdg_lowspring",
    [OT_FDG_HANGAR - RT0] = "fdg_hangar", [OT_FDG_PDOOR3 - RT0] = "fdg_pdoor2",
};
void fd_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S); S.wind_ch = -1;
    objects_plat_path(OT_FDG_DHPLAT, 0, 0, 1, 0, 0, 1, 0);                      /* C = G = 0: X = floor(I), Y = floor(J) (15231 sets them) */
    if (!objects_have_from(RT0) && !objects_have(OT_BGG_TRANSP)) return;
    S.act = fd_flow_act();
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_BGG_TRANSP && S.npad < NPAD && o->x >= 0) { S.pad[S.npad++] = (short)i; continue; }
        if (o->type < RT0 || o->type > OT_FDG_END) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0x7f;
        float E = o->vx;                                                        /* the path platforms keep their registry init */
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = -1; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->vx = E;
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x > G.lvl.w || o->y > G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        if ((unsigned)t < sizeof AN0 / sizeof AN0[0] && AN0[t]) set_s(o, AN0[t]);
        lst_add(o); S.on = 1;
    }
    if (!S.on && !S.npad) return;
    S.on = 1;
    obj_t *o, *q;
    EACH(OT_FDG_TSTART, o) o->av[0] = 90;                                       /* ctor A 90 */
    EACH(OT_FDG_TNODE, o) {                                                     /* 15189-15191: a node on a start turns it and goes */
        o->an.b = NULL;
        if ((o->par & 3) == 1) continue;
        EACH(OT_FDG_TSTART, q) if (fabsf(q->x - o->x) < 64 && fabsf(q->y - o->y) < 64) { q->av[0] = 90. * (o->par & 3); o->alive = 0; }
    }
    link_pairs(OT_FDG_TSTART2, OT_FDG_TSTART);
    EACH(OT_FDG_ZCANNON, o) { o->av[0] = 270; o->av[1] = 270; o->av[4] = 240; o->av[5] = 300; }   /* ctor A 270, E 240, F 300 */
    EACH(OT_FDG_ZBEAM, o) o->av[0] = 1;                                         /* y scale 1 */
    link_pairs(OT_FDG_ZBEAM, OT_FDG_ZCANNON); link_pairs(OT_FDG_ZHIT, OT_FDG_ZCANNON);
    EACH(OT_FDG_ZHIT, o) o->an.b = NULL;
    EACH(OT_FDG_DHPLAT, o) { o->av[0] = o->ox; o->av[1] = o->oy; o->fl = 0; }  /* Q, R = the start (I, J) */
    link_pairs(OT_FDG_DHWHEEL, OT_FDG_DHPLAT);
    EACH(OT_FDG_AIRBASE, o) o->an.b = NULL;
    EACH(OT_FDG_AIRLOCK_S, o) set_s(o, o->par & 1 ? "fdg_air4" : "fdg_air1");
    EACH(OT_FDG_AIRLOCK, o) set_s(o, o->par & 1 ? "fdg_air3" : "fdg_air2");
    link_pairs(OT_FDG_AIRLOCK_S, OT_FDG_AIRBASE); link_pairs(OT_FDG_AIRLOCK, OT_FDG_AIRBASE);
    EACH(OT_FDG_PDOOR, o) { set_s(o, o->par & 1 ? "fdg_pdoor2" : "fdg_pdoor1"); o->av[0] = floorf(o->y); o->av[1] = o->par & 1 ? 192 : -192; o->y = trunc_i(o->av[0] + o->av[1]); }
    EACH(OT_FDG_PDOOR3, o) { o->av[0] = floorf(o->y); o->av[1] = 192; o->y = trunc_i(o->av[0] - 192); o->hidden = 1; o->fl |= F0; }   /* (INFERRED: the wall waits above, out of the way) */
    EACH(OT_FDG_SFLOOR, o) { set_s(o, o->par & 1 ? "fdg_sfloor2" : "fdg_sfloor"); o->av[1] = floorf(o->x); if (!(o->par & 1)) o->fl |= F0; }   /* Q7: ScrollingFloor2 only */
}
void fd_gim_pre(player_t *p) {
    if (!S.on) return;
    if (N(OT_FDG_DHPLAT)) downhill_pre(p);                                       /* 15229-15238 (I / J before the path rule) */
}
void fd_gim_frame(player_t *p) {
    if (!S.on) return;
    if (N(OT_FDG_TSTART) || S.ride || S.wave) transporters(p);                   /* 15188-15201 */
    if (N(OT_FDG_ZCANNON)) zaps(p);                                              /* 15203-15220 */
    if (N(OT_FDG_SMAKER) || N(OT_FDG_SPARK)) sparks(p);                          /* 15221-15228 */
    if (N(OT_FDG_DHWHEEL)) downhill();
    if (N(OT_FDG_AIRBASE)) airlocks(p);                                          /* 15357-15372 */
    if (N(OT_FDG_PDOOR) || N(OT_FDG_PDOOR3)) pdoors(p);                          /* 15373-15382 */
    if (N(OT_FDG_SFLOOR)) floors();                                              /* 15341-15344 */
    effects();
#ifdef FP_HOST_ENV
    { static int dbg = -1; if (dbg < 0) dbg = getenv("FP_DBG_FDG") != NULL;
      if (dbg) printf("fdg %d ride %d X %.1f Y %.1f A %.0f p %.1f %.1f st %d\n", G.frame, S.ride, S.rx, S.ry, S.ra, p->x, MY(p), p->state); }
#endif
}
void fd_gim_draw(int pass) {
    if (!S.on || pass != 1) return;
    obj_t *b;
    EACH(OT_FDG_ZBEAM, b) {                                                     /* the beam: 10-px tiles along its y-scaled length */
        if (!b->an.b) continue;
        float sx = floorf(b->x - G.camx), sy = floorf(b->y - G.camy);
        if (sx < -260 || sx > SCREEN_W + 260 || sy < -260 || sy > SCREEN_H + 260) continue;
        double ux, uy; zb_axis(b, &ux, &uy);
        int n = (int)ceil(b->av[0]);
        const frame_t *f = anim_frame(&b->an);
        for (int k = 0; k < n; k++) frame_draw_rot(b->an.b, f, sx + (float)(ux * 10 * k), sy + (float)(uy * 10 * k), b->ang, 0, 0xffffffff);
    }
}
int fd_gim_obj(obj_t *o, player_t *p) {
    (void)p;
    if (o->type == OT_FDG_ZBEAM) o->hidden = 1;                                  /* (drawn by fd_gim_draw) */
    return 1;                                                                    /* (the families run in fd_gim_frame) */
}
int fd_gim_q20(int type) { (void)type; return 0; }
