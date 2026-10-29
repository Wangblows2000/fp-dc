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
#include "tb.h"
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
    int siren_a, siren_b;                          /* fd2b's script state: the off-map ObjectSiren's A / B (15802-15811) */
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
        if ((o->par & 3) == 1 && (S.act == 3 || S.act == 7 || S.act == 8)) { S.rx = nx; S.ra = 90; continue; }   /* 15591 (fd2a, fd4a, fd4b): x only, no sound */
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
 * the beam reaches 10 x scale px from the cannon's centre along its angle (RV zap1: scale 24 from the act start). (15213 / 15214: the beam hits Q20 enemies too: the
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
        else if (S.act == 4) *sc = fmin(*sc + 1, 24);                          /* 15216 (fd2b only: fd1a / fd1b's beams do not grow back) */
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
 * into place). 15375 / 15378: C 1: B -> 0 by 16. 15376 / 15379: C 1 and B 0: C 2, the closed animation (7), CAM.Z >= 5, sound 132.
 * 15381 / 15382: a closed-in door (C > 0) on both TopBig and BottomBig while both touch something: Lilac is crushed (M 9). */
static void pdoors(player_t *p) {
    obj_t *o;
    int X = PXI(p), Y = PYI(p), crush = 0;
    EACH(OT_FDG_PDOOR, o) {
        double *A = &o->av[0], *B = &o->av[1], *C = &o->av[2];
        o->y = trunc_i(*A + *B);                                                /* 15373 */
        if (*C == 0 && X - 60 > floorf(o->x) && *A > Y - 240 && *A < Y) *C = 1;   /* 15374 / 15377 */
        if (*C == 1) *B = o->par & 1 ? fmax(*B - 16, 0) : fmin(*B + 16, 0);     /* 15375 / 15378 */
        if (*C == 1 && *B == 0) { *C = 2; shake(5); snd(132); set_s(o, o->par & 1 ? "fdg_pdoor2c" : "fdg_pdoor1c"); }                 /* 15376 / 15379 */
        if (*C > 0 && *B > -48 && *B < 48 && !dead(p) && sens_hit(SENS_TOPBIG, o) && sens_hit(SENS_BOTTOMBIG, o)) crush = 1;   /* 15381 / 15382 */
    }
    EACH(OT_FDG_PDOOR3, o) {                                                    /* 15380 */
        double *A = &o->av[0], *B = &o->av[1], *C = &o->av[2];
        if (*C == 0 && X - 60 > floorf(o->x) && *A > Y - 240 && *A < Y) { *C = 1; o->y = trunc_i(floorf(o->y) + *B); }
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

/* ================================================================ 8. FD push platforms (fd2a; RE 15622-15629, on_frame_68_start)
 * platform (Q8 Q9 Q13, invisible 128 x 24): the objects.c path rule with C = G = 0 (X = floor(I), Y = floor(J)); Q delay (start:
 * (reversed index mod 12) 32), R side (0 up, 1 right, 2 down, 3 left), S frames on the side. 15623: Q 0, R 0: J - 3; 15624: R 1:
 * I + 4 and Lilac on it X - 4 (15625); 15626: R 2: J + 3; 15627 / 15628: R 3: I - 4, Lilac X + 4; S + 1 on every side; 15629: R 0 / 2
 * at S >= 117, R 1 / 3 at S >= 80: R = (R + 1) mod 4, S 0 (a 351 x 320 loop). 15622: Q - 1 (min 0); the mask at its platform, frame
 * min((g3 / 2) mod 20, 4) (FDPlatforms too). */
static int fd_frame(void) { int f = (g3() / 2) % 20; return f > 4 ? 4 : f; }
static void push_pre(player_t *p) {
    obj_t *o;
    int on = -1;
    if (p->ground && !dead(p)) on = mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, OT_FDG_PUSH);
    EACH(OT_FDG_PUSH, o) {
        double *Q = &o->av[0], *R = &o->av[1], *Sv = &o->av[2];
        if (*Q > 0) { *Q = fmax(*Q - 1, 0); continue; }                         /* 15622 */
        int r = (int)*R, me = on == (int)(o - G.o);
        if (r == 0) o->oy -= 3; else if (r == 2) o->oy += 3;                   /* 15623 / 15626 */
        else if (r == 1) { o->ox += 4; if (me) p->x -= 4; } else { o->ox -= 4; if (me) p->x += 4; }   /* 15624 / 15625, 15627 / 15628 */
        *Sv += 1;
        if (*Sv >= ((r & 1) ? 80 : 117)) { *R = (r + 1) % 4; *Sv = 0; }        /* 15629 */
    }
}
static void push(void) {
    obj_t *o;
    EACH(OT_FDG_PUSHMASK, o) { obj_t *q = partner(o, OT_FDG_PUSH); if (q) { o->x = q->x; o->y = q->y; } set_frame(o, fd_frame()); }
    EACH(OT_FDG_FDPLAT, o) set_frame(o, S.act == 4 ? (((int)floorf(o->y) / 6) % 6 > 4 ? 4 : ((int)floorf(o->y) / 6) % 6) : fd_frame());   /* 15622 / 15758 */
}

/* ================================================================ 9. electric crushers (fd2a, fd2b; RE 15630 / 15631 / 15753, on_frame_68 / 69_start)
 * crusher (Q7 Q13, invisible 128 x 128, hot spot 64, 128): the objects.c path rule N = J + 64 cos E, E + 2 (ctor F 2, G 64);
 * start: E = 45 x (reversed index). The bolt (128 x 24, under it) and the drawn mask sit at the crusher (15630). 15631: the bolt
 * on PLAYERCHARACTERSENSOR, MV.R 0, not the metal shield (OTHERS.B 3), OTHERS.E <= 0: hurt 10, CAM.Z >= 3, sound 371, G = 10. */
static void crushers(player_t *p) {
    obj_t *o, *c;
    EACH(OT_FDG_ECBOLT, o) { if ((c = partner(o, OT_FDG_ECRUSH))) { o->x = c->x; o->y = c->y; } }
    EACH(OT_FDG_ECMASK, o) { if ((c = partner(o, OT_FDG_ECRUSH))) { o->x = c->x; o->y = c->y; } }
    if (dead(p) || p->shield == 3) return;
    EACH(OT_FDG_ECBOLT, o) {
        if (!csens_hit(p, o) || !vulnerable(p)) continue;
        player_contact(p, 10); shake(3); snd(371);
        if (p->ground) { p->vx = p->gsp; p->ground = 0; }
        p->vy = 10;
        break;
    }
}

/* ================================================================ 10. corkscrew doors + switches (fd2a, fd2b; RE 15609-15621, 15740-15752)
 * switch (64 x 8 floor button): shown every frame; pressed (hidden, flag 0) while Lilac stands on it (BottomBig, L 1), the bike or
 * an object of the qualifier 0x21968 is on it (INFERRED: Lilac / Carol's bike only); a new press plays sound 49. door (Q7 128 x 32):
 * B = its x, C opening (0 .. -128), D = its switch's flag 0 (index pairing, inline after 15614). 15618: D 1: C - 2 (min -128);
 * 15619: D 0 and the door off Lilac's LeftBig: C + 1 (max 0); x = B + C, frame (-C) mod 8 (15609); sounds 368 (opening) / 369
 * (closing) and CAM.Z >= 2 while it moves (15616-15621). */
static void corkscrews(player_t *p) {
    obj_t *o, *d;
    EACH(OT_FDG_CSWITCH, o) {
        int pr = !dead(p) && p->ground && sens_hit(SENS_BOTTOMBIG, o);          /* 15612 */
        if (pr && !(o->fl & F0)) snd(49);                                       /* 15613 */
        o->fl = pr ? (o->fl | F0) : (o->fl & ~F0); o->hidden = pr;              /* 15609-15614 */
    }
    EACH(OT_FDG_CDOOR, d) {
        obj_t *s = partner(d, OT_FDG_CSWITCH);
        double *C = &d->av[2], c0 = *C;
        d->av[3] = s && (s->fl & F0) ? 1 : 0;                                   /* (inline after 15614) */
        if (d->av[3] == 1) *C = fmax(*C - 2, -128);                             /* 15618 */
        else if (!sens_hit(SENS_LEFTBIG, d)) *C = fmin(*C + 1, 0);              /* 15619 */
        if (*C != c0) { shake(2); if (obj_near_view(d, 100)) snd_cool(1, *C < c0 ? 368 : 369); }   /* 15616 / 15617 / 15621 (INFERRED: not looped) */
        d->x = trunc_i(d->av[1] + *C); set_frame(d, ((int)(-*C)) % 8);           /* 15609 */
    }
}

/* ================================================================ 11. player shield givers (fd2a, fd2b; RE 15592-15608, 15722-15728; Milla only)
 * A 0: Milla (MV.A 4) within 150 px (x) / 100 px (y): A 1. A 1: B + 1, frame B / 3; B > 30: animation 11, A 2, B 0. A 2: B + 1
 * (her cube held at the giver, INFERRED: not drawn there); B > 30: the cube is taken (hidden), sound 241, a monitor at the giver's
 * action point (fd2a: a wood shield from the first giver (C 0), a ring monitor from the others; fd2b: a water shield), A 3, B 0.
 * A 3: B + 1; B 20: animation 10; B > 50: animation 0; B > 100: A 4 (done; INFERRED for fd2a, fd2b resets it when she leaves). */
static void give_monitor(obj_t *g, int type) {
    obj_t *m = obj_alloc();
    if (!m) return;
    memset(m, 0, sizeof *m); m->type = type; m->alive = 1; m->hp = 1; m->layer = 2; m->born = G.frame;
    m->x = m->ox = floorf(g->x) - 16; m->y = m->oy = floorf(g->y) - 21 - 16;
    if (type == OT_MON_RINGS) { int a = bank_anim(&G.objs, "monitorrings_35"); if (a >= 0) anim_set(&m->an, &G.objs, a); }
    else { const char *e = objdef(type)->ens; int a = e ? ens_anim(e) : -1; if (a >= 0) anim_set(&m->an, &G.ens, a); }
    if (!m->an.b) m->alive = 0;
}
static void givers(player_t *p) {
    obj_t *o;
    int milla = chr_is_milla(G.character);
    EACH(OT_FDG_SHGIVER, o) {
        double *A = &o->av[0], *B = &o->av[1];
        if (*A == 0 && milla && !dead(p) && fabs(floorf(o->x) - p->x) < 150 && fabs(floorf(o->y) - MY(p)) < 100) *A = 1;   /* 15592 */
        if (*A == 1) { *B += 1; set_s(o, "fdg_shg"); set_frame(o, (int)(*B / 3)); if (*B > 30) { *A = 2; *B = 0; set_s(o, "fdg_shg11"); } }   /* 15593 / 15594 */
        else if (*A == 2) {                                                     /* 15595-15597 / 15722 */
            *B += 1;
            if (*B > 30) {
                player_cube_cancel(); snd(241);
                give_monitor(o, S.act == 4 ? OT_MON_WATER : o->av[2] == 0 ? OT_MON_WOOD : OT_MON_RINGS);
                *A = 3; *B = 0;
            }
        } else if (*A == 3) {                                                   /* 15598, 15605-15608 */
            *B += 1;
            if (*B == 20) set_s(o, "fdg_shg10");
            if (*B > 50) { set_s(o, "fdg_shg"); set_frame(o, 0); }
            if (*B > 100) *A = 4;
        }
        if (*A == 4 && S.act == 4 && (fabs(floorf(o->x) - p->x) >= 150 || !milla)) { *A = 0; *B = 0; }   /* 15726-15728 (INFERRED) */
    }
}

/* ================================================================ 12. rotating barriers + switch buttons (fd2b; RE 15729-15739)
 * barrier (Q7 160 x 48, hot spot 0, 24, rotated by A): A -> B by 3 (15729), CAM.Z >= 2 and sound 370 while it turns (15731 / 15732).
 * button (SwitchButtonLeft / Right, fd2b: momentary): shown every frame, pressed (hidden, flag 0) while Lilac pushes it (M 11 / 47 on
 * S_LEFTBIG / S_RIGHTBIG: switches_wall_push, as Thermal Base's); a new press with the cool-down (the off-map siren's C) at 0:
 * the barriers' B + 90, sounds 49 + 369, cool-down 40 (15735 / 15736, 15737). INFERRED: a button turns the barriers within 400 px. */
static void barriers(player_t *p) {
    obj_t *o, *b;
    static int cool;
    if (cool > 0) cool--;                                                       /* 15737 */
    EACH(OT_FDG_BUTTON, o) {
        int pr = switches_wall_push(p, o, o->par & 1 ? SENS_RIGHTBIG : SENS_LEFTBIG);   /* 15733 / 15734 */
        if (pr && !(o->fl & F0) && cool <= 0) {                                 /* 15735 / 15736 */
            int n = 0;
            EACH(OT_FDG_RBARRIER, b) if (fabsf(b->x - o->x) < 400 && fabsf(b->y - o->y) < 400) { b->av[1] += 90; n++; }
            if (n) { snd(49); snd(369); cool = 40; }
        }
        o->fl = pr ? (o->fl | F0) : (o->fl & ~F0); o->hidden = pr;
    }
    EACH(OT_FDG_RBARRIER, b) {
        if (b->av[0] < b->av[1]) { b->av[0] = fmin(b->av[0] + 3, b->av[1]); shake(2); if (obj_near_view(b, 100)) snd_cool(2, 370); }
        b->ang = (float)fmod(b->av[0], 360.);
    }
}

/* ================================================================ 13. big lifts + their switches (fd2b; RE 15758-15777, foreach_lift_68)
 * lift (Q7 Q13 256 x 64, hot spot 128, 0): the objects.c path rule with a scripted J (C = G = 0), Q its speed, S its home J (the
 * placed y). Switches 1 / 2 ride on it (15758) and are pressed (hidden, flag 0, sound 49) while Lilac stands on one (BottomBig, L 1).
 * 15758: J += Q. foreach_lift_68 (a switch pressed): Q = max(Q - 0.1, -3) (up); 15773: none pressed: Q = min(Q + 0.05, 4) (it falls);
 * 15770 / 15771: falling onto an FDPlatform (a ledge of the shaft): J = the ledge's y, Q 0, CAM.Z >= 3, sound 51; 15760 / 15774: at
 * home (J >= S): J = S, Q 0 (the same landing). Sound 262 while it rises (15776 / 15777). INFERRED: it lands on a ledge only falling. */
static void lifts_pre(player_t *p) {
    obj_t *o, *s, *l;
    EACH(OT_FDG_BIGLIFT, o) {
        int pressed = 0;
        EACH(OT_FDG_LSWITCH, s) if (partner(s, OT_FDG_BIGLIFT) == o && (s->fl & F0)) pressed = 1;
        double *Q = &o->av[0], Sh = o->av[1], J0 = o->oy;
        if (pressed) *Q = fmax(*Q - 0.1, -3); else *Q = fmin(*Q + 0.05, 4);      /* foreach_lift_68 / 15773 */
        o->oy = (float)(o->oy + *Q);                                            /* 15758 */
        if (*Q > 0) {
            EACH(OT_FDG_FDPLAT, l) {                                            /* 15770 / 15771 */
                float ly = floorf(l->y);
                if (fabsf(l->x - o->x) < 160 && J0 <= ly && o->oy >= ly) { o->oy = ly; if (*Q > 1) { shake(3); snd(51); } *Q = 0; break; }
            }
            if (o->oy >= Sh) { o->oy = (float)Sh; if (*Q > 1) { shake(3); snd(51); } *Q = 0; }   /* 15760 / 15774 */
        }
        if (*Q < 0 && obj_near_view(o, 100)) snd_cool(3, 262);                  /* 15776 (INFERRED: re-played, not looped) */
    }
    (void)p;
}
static void lifts(player_t *p) {
    obj_t *s, *o;
    EACH(OT_FDG_LSWITCH, s) {
        if (!(o = partner(s, OT_FDG_BIGLIFT))) continue;
        s->x = trunc_i(floorf(o->x) + ((s->par & 1) ? 64 : -64)); s->y = o->y;   /* 15758 (INFERRED: the switches' offsets) */
        int pr = !dead(p) && p->ground && sens_hit(SENS_BOTTOMBIG, s);           /* 15761 / 15762 */
        if (pr && !(s->fl & F0)) snd(49);                                       /* 15763 / 15765 */
        s->fl = pr ? (s->fl | F0) : (s->fl & ~F0); s->hidden = pr;
    }
    EACH(OT_FDG_BIGLIFT, o) set_frame(o, (g3() / 4) % 6);                       /* (INFERRED: its lights cycle) */
}

/* ================================================================ 14. consoles, tube shades (fd2a, fd2b: scenery; RE 15667)
 * Tube shade: B = (B + 2) mod 360, y = A + 24 sin B (A = the placed y: INFERRED). */
static void scenery(void) {
    obj_t *o;
    EACH(OT_FDG_TUBESHADE, o) { o->av[1] = fmod(o->av[1] + 2, 360.); o->y = trunc_i(o->av[0] + 24 * c_sin(o->av[1])); }
}

/* ================================================================ 16. speed boosters (fd3a, fd3b; RE col_26_69, 16038 / 16039)
 * col_26_69 (PLAYER on a booster): sound 379, MV.B = 15 (or -15 when B < 0: her own direction), OTHERS.E >= 30, the afterimage
 * clock (A) 30, MV.Q = the direction; 16039: while A > 0 an afterimage a frame (INFERRED: left out). 16038: the front at its booster. */
static void boosters(player_t *p) {
    obj_t *o, *b;
    EACH(OT_FDG_BOOSTF, o) { if ((b = partner(o, OT_FDG_BOOST))) { o->x = b->x; o->y = b->y; } }   /* 16038 */
    if (dead(p) || hurt(p) || p->state == ST_RIDE) return;
    EACH(OT_FDG_BOOST, b) {
        float x0, y0, x1, y1; if (!b->an.b) continue; box_of(b, &x0, &y0, &x1, &y1);
        if (!rect_disc(x0, y0, x1, y1, p)) continue;
        double B = p->ground ? p->gsp : p->vx, v = B >= 0 ? 15 : -15;
        if (p->ground) p->gsp = v; else p->vx = v;
        if (p->ground) p->vx = v;
        p->face = v > 0 ? 1 : -1;
        if (p->invuln < 30) p->invuln = 30;
        snd_cool(4, 379);
        break;
    }
}

/* ================================================================ 17. orbit beams (fd3a, fd3b; RE 16040-16046, start 16201)
 * beam: D, E = its base (the centre), F its angle (start: k = the selection index, F = (k / 6) 180 + ((k mod 6) + 0.5) 24 + 18.5),
 * H the clock (+1, -100 at 100: 16046), at (D + 240 cos F, E - 240 sin F), angle F + 180 (16040); F - 1 a frame (16041). The base's
 * frame = (g3 0.3) mod 8. scan (the ray to the centre): at its beam, angle F + 180, A = the beam's H; A < 55: direction 1 (it aims,
 * harmless: 16043); A >= 55: it burns (16045: PLAYERCHARACTERSENSOR, MV.R 0, OTHERS.B != 2, OTHERS.E <= 0: hurt 5); H 55 in view:
 * sound 70, CAM.Z >= 3 (16042). INFERRED: the ray reaches the centre (x scale 240 / 64), the base is not a target. */
static int ray_hits(const obj_t *s, double len, int x0, int y0, int x1, int y1) {
    double a = s->ang / RAD, ux = cos(a), uy = -sin(a);
    for (double d = 0; d < len; d += 4)
        for (int k = -1; k <= 1; k++) {
            double x = floorf(s->x) + ux * d - uy * k * 7, y = floorf(s->y) + uy * d + ux * k * 7;
            if (x >= x0 && x <= x1 && y >= y0 && y <= y1) return 1;
        }
    return 0;
}
static void orbits(player_t *p) {
    obj_t *o, *s;
    int X0, Y0, X1, Y1, hit = 0, fire = 0; char_rect(p, &X0, &Y0, &X1, &Y1);
    EACH(OT_FDG_OBEAM, o) {
        double *F = &o->av[2], *H = &o->av[3];
        o->x = trunc_i((int)o->av[0] + 240 * c_cos((int)*F)); o->y = trunc_i((int)o->av[1] - 240 * c_sin((int)*F));   /* 16040 */
        o->ang = (float)fmod((int)*F + 180, 360.);
        *H += 1;
        if (*H == 55 && obj_near_view(o, 100)) fire = 1;                        /* 16042 */
        *F = (double)(((int)*F + 359) % 360);                                   /* 16041 */
        if (*H >= 100) *H -= 100;                                               /* 16046 */
    }
    if (fire) { snd_cool(5, 70); shake(3); }
    EACH(OT_FDG_OBASE, o) set_frame(o, (int)fmod(g3() * 0.3, 8.));
    EACH(OT_FDG_OSCAN, s) {
        if (!(o = partner(s, OT_FDG_OBEAM))) continue;
        s->x = o->x; s->y = o->y; s->ang = o->ang; s->av[0] = o->av[3];
        set_s(s, s->av[0] < 55 ? "fdg_oscan1" : "fdg_oscan0");
        if (s->av[0] >= 55 && !hit && !dead(p) && ray_hits(s, 240, X0, Y0, X1, Y1)) hit = 1;   /* 16045 */
    }
    if (hit && p->shield != 2 && vulnerable(p)) player_contact(p, 5);
}

/* ================================================================ 18. the laser tunnel (fd4a; RE 16674-16681, on_frame_72_start)
 * start: B = 160. 16674: B = (B + 1) mod 400 while MV.X lies between the act's CameraLimitBossLeft / Right. 16676: 200 < B < 340:
 * A = min(A + 0.2, 16), CAM.Z >= 0.2 A (the charge; 16677: sound 388 as it starts); 16678: B 300: shown, sound 270, CAM.Z >= 20
 * (it fires); 16681: B in [300, 365) with MV.X in [x, x + 256]: flag 1 (the hit; INFERRED: hurt 10, OTHERS.E rules); B > 340:
 * A - 0.2. INFERRED: the beam fills the 256-px column from the tunnel up to the view's top while it fires (16679 sizes its box);
 * the glass panes (16715-16717) are scenery here. */
static void tunnel(player_t *p) {
    obj_t *o, *c;
    float lx = -1e9f, rx = 1e9f;
    EACH(OT_FDG_LTUNNEL, o) {
        if (o->av[5] == 0) {                                                    /* (the camera-limit pair, found once) */
            o->av[5] = 1; o->av[6] = -1e9; o->av[7] = 1e9;
            for (int i = 0; i < G.nobjs; i++) { c = &G.o[i]; if (!c->alive) continue;
                if (c->type == OT_CAMLIM_L && fabsf(c->y - o->y) < 600) o->av[6] = c->x;
                if (c->type == OT_CAMLIM_R && fabsf(c->y - o->y) < 600) o->av[7] = c->x; }
        }
        lx = (float)o->av[6]; rx = (float)o->av[7];
        double *A = &o->av[0], *B = &o->av[1];
        if (p->x > lx && p->x < rx) *B = fmod(*B + 1, 400.);                    /* 16674 */
        if (*B > 200 && *B < 340) { *A = fmin(*A + 0.2, 16); shake((float)(0.2 * *A)); if (*B == 201) snd(388); }   /* 16676 / 16677 */
        if (*B == 300) { snd(270); shake(20); }                                 /* 16678 */
        if (*B > 340) *A = fmax(*A - 0.2, 0);                                   /* 16681 */
        o->av[4] = *B >= 300 && *B < 365; o->hidden = 1;                         /* (shown while it fires: drawn by fd_gim_draw) */
        o->fl &= ~F1;
        if (*B >= 300 && *B < 365 && p->x > floorf(o->x) && p->x < floorf(o->x) + 256 && MY(p) < o->y + 8) o->fl |= F1;
        if ((o->fl & F1) && vulnerable(p)) player_contact(p, 10);
        set_s(o, *B >= 300 ? "fdg_ltun8" : "fdg_ltun0");
    }
}

/* ================================================================ 15. fd2b's alarm (RE 15802, 15803, 15807, 15808, 15811)
 * The off-map ObjectSiren's A / B: 15802: A 0, MV.Y > 5340, global 4 (the checkpoint) < 2, not Milla (global 1 != 4): A 1 (the
 * warning subtitle: the flow's). 15803: A 1: B + 1. 15807: A 1 and B > 400, or A < 2 with the checkpoint in [2, 4), not Milla and
 * CTRL.M 0: A 2, sound 372, OTHERS.J 0 (the air). 15808: A 2: B + 1 up to 460. 15811: A 2 and Y < 288: A 3. INFERRED: global 4 =
 * G.at_g4 (the restart's checkpoint number). */
static void siren(player_t *p) {
    int cp = G.at_g4 > 1 ? G.at_g4 : 0, milla = chr_is_milla(G.character);
    if (S.siren_a == 0 && MY(p) > 5340 && cp < 2 && !milla) S.siren_a = 1;                   /* 15802 */
    if (S.siren_a == 1) S.siren_b += 1;                                                        /* 15803 */
    if ((S.siren_a == 1 && S.siren_b > 400) || (S.siren_a < 2 && cp >= 2 && cp < 4 && !milla && !G.input_lock)) {   /* 15807 */
        S.siren_a = 2; snd(372);
    }
    if (S.siren_a == 2 && S.siren_b < 460) S.siren_b += 1;                                    /* 15808 */
    if (S.siren_a == 2 && MY(p) < 288) S.siren_a = 3;                                          /* 15811 */
}
int fd_gim_siren_alarm(void) { return S.siren_a == 2; }
int fd_gim_siren_state(void) { return S.siren_a; }

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
    tb_gim_mdoor_ywin(fd_flow_act() ? 79 : 59);                                /* the lab doors (TB's metal doors): 15641 / 15646 */
    objects_plat_path(OT_FDG_DHPLAT, 0, 0, 1, 0, 0, 1, 0);                      /* C = G = 0: X = floor(I), Y = floor(J) (15231 sets them) */
    objects_plat_path(OT_FDG_PUSH, 0, 0, 1, 0, 0, 1, 0);                        /* (15623-15628 set I / J) */
    objects_plat_path(OT_FDG_BIGLIFT, 0, 0, 1, 0, 0, 1, 0);                     /* (15758 sets J) */
    objects_plat_path(OT_FDG_ECRUSH, 0, 0, 1, 2, 64, 0, 0);                     /* ctor F 2, G 64, H 0: N = J + 64 cos E */
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
    EACH(OT_FDG_ZBEAM, o) o->av[0] = 24;                                        /* y scale 24 (RV zap1: 0xcc = 24 from the start) */
    link_pairs(OT_FDG_ZBEAM, OT_FDG_ZCANNON); link_pairs(OT_FDG_ZHIT, OT_FDG_ZCANNON);
    EACH(OT_FDG_ZHIT, o) o->an.b = NULL;
    EACH(OT_FDG_DHPLAT, o) { o->av[0] = o->ox; o->av[1] = o->oy; o->fl = 0; }  /* Q, R = the start (I, J) */
    link_pairs(OT_FDG_DHWHEEL, OT_FDG_DHPLAT);
    EACH(OT_FDG_AIRBASE, o) o->an.b = NULL;
    EACH(OT_FDG_AIRLOCK_S, o) set_s(o, o->par & 1 ? "fdg_air4" : "fdg_air1");
    EACH(OT_FDG_AIRLOCK, o) set_s(o, o->par & 1 ? "fdg_air3" : "fdg_air2");
    link_pairs(OT_FDG_AIRLOCK_S, OT_FDG_AIRBASE); link_pairs(OT_FDG_AIRLOCK, OT_FDG_AIRBASE);
    EACH(OT_FDG_PDOOR, o) { set_s(o, o->par & 1 ? "fdg_pdoor2" : "fdg_pdoor1"); o->av[0] = floorf(o->y); o->av[1] = o->par & 1 ? 192 : -192; o->y = trunc_i(o->av[0] + o->av[1]); }
    EACH(OT_FDG_PDOOR3, o) { o->av[0] = floorf(o->y); o->av[1] = 192; o->y = trunc_i(o->av[0] - 192); }   /* 15566: A = y, y - B (it waits 192 px up, solid) */
    EACH(OT_FDG_SFLOOR, o) { set_s(o, o->par & 1 ? "fdg_sfloor2" : "fdg_sfloor"); o->av[1] = floorf(o->x); if (!(o->par & 1)) o->fl |= F0; }   /* Q7: ScrollingFloor2 only */
    int n = N(OT_FDG_PUSH), k = 0;
    EACH(OT_FDG_PUSH, o) { set_s(o, "fdg_push"); o->av[0] = ((n - 1 - k++) % 12) * 32.; o->fl = 0; }   /* on_frame_68_start: Q = (k mod 12) 32 */
    link_pairs(OT_FDG_PUSHMASK, OT_FDG_PUSH);
    EACH(OT_FDG_PUSHMASK, o) set_s(o, "fdg_pushmask");
    EACH(OT_FDG_FDPLAT, o) set_s(o, "fdg_pushmask");                           /* (the same images) */
    EACH(OT_FDG_CONSOLE, o) set_s(o, o->par & 1 ? "fdg_console2" : "fdg_console1");
    EACH(OT_FDG_TUBESHADE, o) { set_s(o, "fdg_tubeshade"); o->av[0] = floorf(o->y); }
    n = N(OT_FDG_ECRUSH); k = 0;
    EACH(OT_FDG_ECRUSH, o) {                                                    /* on_frame_68 / 69_start: E = 45 k */
        set_s(o, "fdg_ecmask"); o->vx = (float)(45. * (n - 1 - k++)); o->fl = 0;
        o->fx = (float)(o->oy + 64 * c_cos(o->vx)); o->y = floorf(o->fx);
    }
    EACH(OT_FDG_ECBOLT, o) set_s(o, "fdg_ecbolt");
    EACH(OT_FDG_ECMASK, o) set_s(o, "fdg_ecmask");
    link_pairs(OT_FDG_ECBOLT, OT_FDG_ECRUSH); link_pairs(OT_FDG_ECMASK, OT_FDG_ECRUSH);
    EACH(OT_FDG_CSWITCH, o) set_s(o, "fdg_cswitch");
    EACH(OT_FDG_CDOOR, o) { set_s(o, "fdg_cdoor"); o->av[1] = floorf(o->x); }
    link_pairs(OT_FDG_CDOOR, OT_FDG_CSWITCH);
    n = N(OT_FDG_SHGIVER); k = 0;
    EACH(OT_FDG_SHGIVER, o) { set_s(o, "fdg_shg"); set_frame(o, 0); o->av[2] = n - 1 - k++; }   /* on_frame_68_start: C = the index */
    EACH(OT_FDG_RBARRIER, o) set_s(o, "fdg_rbarrier");
    k = 0;
    EACH(OT_FDG_RBARRIER, o) { if (k++ & 1) o->av[0] = o->av[1] = 180; o->ang = (float)o->av[0]; }   /* (INFERRED: the pairs at one spot face apart) */
    EACH(OT_FDG_BUTTON, o) set_s(o, o->par & 1 ? "tbg_btn1" : "tbg_btn0");
    EACH(OT_FDG_BIGLIFT, o) { set_s(o, "fdg_biglift"); o->av[1] = o->oy; o->av[0] = 0; o->fl = 0; }   /* S = home J */
    EACH(OT_FDG_LSWITCH, o) set_s(o, "fdg_lswitch");
    link_pairs(OT_FDG_LSWITCH, OT_FDG_BIGLIFT);
    EACH(OT_FDG_BOOST, o) set_s(o, "fdg_boost");
    EACH(OT_FDG_BOOSTF, o) set_s(o, "fdg_boostf");
    link_pairs(OT_FDG_BOOSTF, OT_FDG_BOOST);
    EACH(OT_FDG_OBASE, o) set_s(o, "fdg_obase");
    link_pairs(OT_FDG_OBEAM, OT_FDG_OBASE); link_pairs(OT_FDG_OSCAN, OT_FDG_OBEAM);
    n = N(OT_FDG_OBEAM); k = 0;
    EACH(OT_FDG_OBEAM, o) {                                                     /* 16201: D, E = the base, F from the index, G = +-1 */
        obj_t *b = partner(o, OT_FDG_OBASE); int j = n - 1 - k++;
        set_s(o, "fdg_obeam");
        o->av[0] = b ? floorf(b->x) : floorf(o->x); o->av[1] = b ? floorf(b->y) : floorf(o->y);
        o->av[2] = (j / 6) * 180 + ((j % 6) + 0.5) * 24 + 18.5; o->av[4] = ((j / 6) % 2) * 2 - 1;
        o->av[2] = (double)((int)o->av[2] % 360);
    }
    EACH(OT_FDG_OSCAN, o) { set_s(o, "fdg_oscan1"); o->hidden = 1; }
    EACH(OT_FDG_LTPLAT, o) set_s(o, "fdg_ltplat");
    EACH(OT_FDG_LTUNNEL, o) { set_s(o, "fdg_ltun0"); o->av[1] = 160; o->hidden = 1; }   /* on_frame_72_start: B 160 */
    EACH(OT_FDG_LTGLASS, o) set_s(o, (o->par == 2) ? "fdg_ltg3" : (o->par == 1) ? "fdg_ltg2" : "fdg_ltg1");
    if (N(OT_FDG_LSWITCH) == 2 * N(OT_FDG_BIGLIFT)) {                           /* (INFERRED: each lift takes its two switches, by class) */
        int i1 = 0, i2 = 0;
        EACH(OT_FDG_LSWITCH, o) { int j = (o->par & 1) ? i2++ : i1++; if (j < N(OT_FDG_BIGLIFT)) o->tm = LST[OT_FDG_BIGLIFT - RT0][j]; }
    }
}
void fd_gim_pre(player_t *p) {
    if (!S.on) return;
    if (N(OT_FDG_DHPLAT)) downhill_pre(p);                                       /* 15229-15238 (I / J before the path rule) */
    if (N(OT_FDG_PUSH)) push_pre(p);                                             /* 15622-15629 */
    if (N(OT_FDG_BIGLIFT)) lifts_pre(p);                                         /* 15758-15777 */
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
    if (N(OT_FDG_CSWITCH) || N(OT_FDG_CDOOR)) corkscrews(p);                     /* 15609-15621 / 15740-15752 */
    if (N(OT_FDG_PUSHMASK) || N(OT_FDG_FDPLAT)) push();                          /* 15622 */
    if (N(OT_FDG_ECRUSH)) crushers(p);                                           /* 15630 / 15631 / 15753 */
    if (N(OT_FDG_SHGIVER)) givers(p);                                            /* 15592-15608 / 15722-15728 */
    if (N(OT_FDG_RBARRIER) || N(OT_FDG_BUTTON)) barriers(p);                     /* 15729-15739 */
    if (N(OT_FDG_LSWITCH)) lifts(p);                                             /* 15758-15768 */
    if (N(OT_FDG_TUBESHADE)) scenery();                                          /* 15667 */
    if (N(OT_FDG_BOOST)) boosters(p);                                            /* col_26_69, 16038 / 16039 */
    if (N(OT_FDG_LTUNNEL)) tunnel(p);                                            /* 16674-16681 */
    if (N(OT_FDG_OBEAM)) orbits(p);                                              /* 16040-16046 */
    if (S.act == 4) siren(p);                                                    /* 15802-15811 */
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
    EACH(OT_FDG_LTUNNEL, b) {                                                   /* the firing tunnel: its 2-px line up the view (INFERRED) */
        if (!b->av[4] || !b->an.b) continue;
        float sx = floorf(b->x - G.camx), sy = floorf(b->y - G.camy);
        if (sx < -260 || sx > SCREEN_W) continue;
        const frame_t *f = anim_frame(&b->an);
        for (float y = sy > SCREEN_H ? SCREEN_H : sy; y > -2; y -= 2) frame_draw(b->an.b, f, sx, y, 0, 0xffffffff);
    }
    EACH(OT_FDG_OSCAN, b) {                                                     /* the orbit rays: x scale 240 / 64 (INFERRED) */
        if (!b->an.b) continue;
        float sx = floorf(b->x - G.camx), sy = floorf(b->y - G.camy);
        if (sx < -260 || sx > SCREEN_W + 260 || sy < -260 || sy > SCREEN_H + 260) continue;
        r_set_xscale(3.75f); frame_draw_rot(b->an.b, anim_frame(&b->an), sx, sy, b->ang, 0, 0xffffffff); r_set_xscale(1.f);
    }
}
int fd_gim_obj(obj_t *o, player_t *p) {
    (void)p;
    if (o->type == OT_FDG_ZBEAM) o->hidden = 1;                                  /* (drawn by fd_gim_draw) */
    return 1;                                                                    /* (the families run in fd_gim_frame) */
}
int fd_gim_q20(int type) { (void)type; return 0; }
