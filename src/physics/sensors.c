/* Lilac's movement core: the original's sensor collision (RE; docs/RE_NOTES.md "Sensor collision").
 *
 * The original moves the movement object (PlayerMovementValues_253: X, Y, B, G, N, O, L) and keeps it
 * out of the terrain with 15 invisible sensor objects. Each sensor is placed at an offset (A along the
 * surface, B along the normal) rotated by the raw angle N, tested with pixel-mask overlap against the
 * background and two object groups, and the result is stored in its alterable Z. Fast loops nudge the
 * movement object one pixel at a time until the sensors are clear. This file reproduces that per-frame
 * order (events 1584-1611) from the recovered rules; it contains no decompiled code.
 *
 * Coordinates: X, Y = the original's movement object (reference point, 14 px above the feet on flat
 * ground). The player code keeps p->y = Y + ORIG_REF_DY. */
#include <math.h>
#include <string.h>
#include "../fp.h"

/* ---- sensor shapes (RE: Image data of the sensor classes read from the running original; all are
 * discs, a 3x3 box or 3-px bars, reproduced here procedurally; tests/test_sensors.py checks them
 * against the extracted images) */
typedef struct { int w, h, hx, hy; signed char x0[27], x1[27]; } smask_t;   /* row spans, x1 < x0 = empty */
enum { M_C9, M_C11, M_C27, M_R3, M_E0, M_E1, M_E2, M_E3, M_E4, M_E5, M_E6, M_E7, M_COUNT };
static smask_t MK[M_COUNT];

static void mk_disc(smask_t *m, int n) {
    /* disc of radius n/2 - 1/4 around the centre pixel: 9x9 (r^2 <= 18), 11x11 (<= 27), 27x27 (<= 175) */
    int c = (n - 1) / 2; double t = (n / 2.0 - 0.25) * (n / 2.0 - 0.25);
    m->w = m->h = n; m->hx = m->hy = c;
    for (int j = 0; j < n; j++) {
        m->x0[j] = 1; m->x1[j] = 0;
        for (int i = 0; i < n; i++)
            if ((i - c) * (i - c) + (j - c) * (j - c) <= t) { if (m->x1[j] < m->x0[j]) m->x0[j] = (signed char)i; m->x1[j] = (signed char)i; }
    }
}
/* bar: full w x h box; diag: rows of x - y in [lo, hi] (sgn 1) or x + y in [lo, hi] (sgn -1), one pixel excluded */
static void mk_box(smask_t *m, int w, int h, int hx, int hy) {
    m->w = w; m->h = h; m->hx = hx; m->hy = hy;
    for (int j = 0; j < h; j++) { m->x0[j] = 0; m->x1[j] = (signed char)(w - 1); }
}
static void mk_diag(smask_t *m, int w, int h, int hx, int hy, int sgn, int lo, int hi, int ex, int ey) {
    m->w = w; m->h = h; m->hx = hx; m->hy = hy;
    for (int j = 0; j < h; j++) {
        m->x0[j] = 1; m->x1[j] = 0;
        for (int i = 0; i < w; i++) {
            int v = sgn > 0 ? i - j : i + j;
            if (v >= lo && v <= hi && !(i == ex && j == ey)) { if (m->x1[j] < m->x0[j]) m->x0[j] = (signed char)i; m->x1[j] = (signed char)i; }
        }
    }
}
static void masks_init(void) {
    static int done;
    if (done) return;
    done = 1;
    mk_disc(&MK[M_C9], 9); mk_disc(&MK[M_C11], 11); mk_disc(&MK[M_C27], 27);
    mk_box(&MK[M_R3], 3, 3, 1, 1);
    /* edge sensors (PlayerSensorLeftEdge_339 / RightEdge_340): 8 direction images (directions 0, 4, ..., 28) */
    mk_box(&MK[M_E0], 3, 25, 1, 1);                                 /* dir 0: bar down */
    mk_diag(&MK[M_E1], 18, 18, 1, 1, 1, -2, 1, -1, -1);             /* dir 4 */
    mk_box(&MK[M_E2], 25, 3, 2, 1);                                 /* dir 8: bar right */
    mk_diag(&MK[M_E3], 18, 17, 1, 16, -1, 16, 19, -1, -1);          /* dir 12 */
    mk_box(&MK[M_E4], 3, 25, 1, 24);                                /* dir 16: bar up */
    mk_diag(&MK[M_E5], 17, 18, 16, 17, 1, -2, 1, 0, 0);             /* dir 20 */
    mk_box(&MK[M_E6], 25, 3, 24, 2);                                /* dir 24: bar left */
    mk_diag(&MK[M_E7], 18, 18, 17, 1, -1, 16, 19, 0, 17);           /* dir 28 */
}

/* ---- sensors (RE: alterables A, B of each sensor object at stage start) */
enum { S_MAIN, S_LEFT, S_LEFTBIG, S_RIGHT, S_RIGHTBIG, S_TOP, S_TOPBIG, S_BOTTOM, S_BOTTOMBIG,
       S_SLOPES, S_LANGLE, S_RANGLE, S_LEDGE, S_REDGE, S_COUNT };
typedef struct { int x, y, z, mask; double ya; } sens_t;   /* ya: alterable Y (angle-sensor scan depth) */
static sens_t S[S_COUNT];
static const struct { double a, b; int mask; } SDEF[S_COUNT] = {
    { 0, 0, M_C27 },    /* Player_326 (27x27 disc), placed at (int X, int Y) */
    { -12, -2, M_C9 }, { -13, -2, M_C11 }, { 12, -2, M_C9 }, { 13, -2, M_C11 },
    { 0, -12, M_C9 }, { 0, -13, M_C11 }, { 0, 12, M_C9 }, { 0, 13, M_C11 },
    { 0, 24, M_C11 },   /* PlayerSensorSlopes_335 */
    { -7, 0, M_R3 }, { 7, 0, M_R3 },     /* PlayerSensorLeftAngle_337 / RightAngle_338 (+ scan Y) */
    { -8, 12, M_E0 }, { 8, 12, M_E0 },   /* PlayerSensorLeftEdge_339 / RightEdge_340 */
};
static int edge_last_exact = -1;          /* Active +0xd1: last direction that had its own image */

int mv_z(int s) { return S[s].z; }
int mv_sensor_left_big(void) { return S[S_LEFTBIG].z; }
int mv_sensor_right_big(void) { return S[S_RIGHTBIG].z; }
int mv_sensor_top_big(void) { return S[S_TOPBIG].z; }

void mv_reset(void) {
    masks_init();
    memset(S, 0, sizeof S);
    for (int i = 0; i < S_COUNT; i++) S[i].mask = SDEF[i].mask;
    edge_last_exact = -1;
}

/* the original's trig: cos is taken as 0 at exactly 90/270 and sin as 0 at exactly 180; degrees are
 * converted with the constant 57.29577951 */
#define DEG 57.29577951
static double o_cos(double n) { return (n == 270. || n == 90.) ? 0. : cos(n / DEG); }
static double o_sin(double n) { return n == 180. ? 0. : sin(n / DEG); }

static int clampi(double v, double lo, double hi) { if (v < lo) v = lo; if (v > hi) v = hi; return (int)v; }

/* placement (events 1413, 1423, 1437, 1445, 1457, 1472, 1508): x = floor((s B + A c) + X),
 * y = floor((B c + Y) - s A), clamped to the camera bounds C..E, D..F */
static void place(const mv_t *m, int i) {
    double c = o_cos(m->N), s = o_sin(m->N), A = SDEF[i].a, B = SDEF[i].b + S[i].ya;
    S[i].x = clampi(floor(s * B + A * c + m->X), 0, G.lvl.w);
    S[i].y = clampi(floor((B * c + m->Y) - s * A), 0, G.lvl.h);
    S[i].z = 0;
}
static void place_main(const mv_t *m) {   /* event 1467: (int) of the clamped X, Y */
    S[S_MAIN].x = clampi(m->X, 0, G.lvl.w);
    S[S_MAIN].y = clampi(m->Y, 0, G.lvl.h);
    S[S_MAIN].z = 0;
}

static int mask_hits_bg(const smask_t *k, int px, int py) {
    int ox = px - k->hx, oy = py - k->hy;
    for (int j = 0; j < k->h; j++)
        if (k->x1[j] >= k->x0[j] && px_span_solid(&G.lvl, ox + k->x0[j], ox + k->x1[j], oy + j)) return 1;
    return 0;
}
static int mask_hits_objs(int grp, const smask_t *k, int px, int py) {
    return objects_sensor_hit(grp, px - k->hx, py - k->hy, k->x0, k->x1, k->h);
}

/* one sensor placed at (X, Y, N) tested against the objects of one type (e.g. S_BOTTOMBIG vs LadderTop,
 * events 4044 / 4058): index of the first overlapping object, or -1 */
int mv_sensor_obj(double X, double Y, double N, int sensor, int objtype) {
    mv_t m; memset(&m, 0, sizeof m); m.X = X; m.Y = Y; m.N = N;
    sens_t keep = S[sensor];
    place(&m, sensor);
    const smask_t *k = &MK[S[sensor].mask];
    int r = objects_sensor_hit_type(objtype, S[sensor].x - k->hx, S[sensor].y - k->hy, k->x0, k->x1, k->h);
    S[sensor] = keep;
    return r;
}

/* overlap test of one sensor (e.g. events 1425-1431): background, then the always-solid group
 * (QualifierList +0x218c0: monitors, wall spring, spikes, breakable rock, ...), then the obstacle group
 * (+0x218d8: bridge logs, springs, ladder tops, platforms; platforms flagged 0 (4738: Lilac's reference
 * point below their top) are skipped) while grounded (L = 1). In the air (L = 0) only the bottom sensors
 * count obstacles, when G >= 0 and the platform sensor touches no obstacle at all (1427/1431). */
/* PlayerSensorPlatform_336 (RE 2954, 1423): 11-px disc at A = 0, B = -5 */
static int platform_sensor_clear(const mv_t *m) {
    double c = o_cos(m->N), s = o_sin(m->N), A = 0, B = -5;
    int x = clampi(floor(s * B + A * c + m->X), 0, G.lvl.w), y = clampi(floor((B * c + m->Y) - s * A), 0, G.lvl.h);
    const smask_t *k = &MK[M_C11];
    return !mask_hits_objs(OBJGRP_OBSTACLE_ALL, k, x, y);
}
static void test(const mv_t *m, int i) {
    const smask_t *k = &MK[S[i].mask];
    if (mask_hits_bg(k, S[i].x, S[i].y)) S[i].z = 1;
    if (!S[i].z && mask_hits_objs(OBJGRP_SOLID, k, S[i].x, S[i].y)) S[i].z = 1;
    if (S[i].z) return;
    if (m->L == 1) { if (mask_hits_objs(OBJGRP_OBSTACLE, k, S[i].x, S[i].y)) S[i].z = 1; }   /* 1426/1430 & co. */
    else if ((i == S_BOTTOM || i == S_BOTTOMBIG) && m->G >= 0 &&                            /* 1427/1431 */
             mask_hits_objs(OBJGRP_OBSTACLE, k, S[i].x, S[i].y) && platform_sensor_clear(m)) S[i].z = 1;
}

/* ---- loops (loop_player01*) */
static int wall_iter(mv_t *m) {             /* loop_player01wallcollision */
    place(m, S_LEFT); place(m, S_LEFTBIG); test(m, S_LEFT); test(m, S_LEFTBIG);
    place(m, S_RIGHT); place(m, S_RIGHTBIG); test(m, S_RIGHT); test(m, S_RIGHTBIG);
    if (S[S_RIGHT].z) { m->X -= o_cos(m->N); m->Y += o_sin(m->N); }
    if (S[S_LEFT].z) { m->X += o_cos(m->N); m->Y -= o_sin(m->N); }
    return S[S_LEFT].z || S[S_RIGHT].z;     /* the loop stops once neither side touches */
}
static int up_iter(mv_t *m) {               /* loop_player01up: one-pixel push out of a ceiling */
    place(m, S_TOP); place(m, S_TOPBIG); test(m, S_TOP); test(m, S_TOPBIG);
    if (m->G > 0) return 1;
    if (S[S_TOP].z) { m->Y += 1; return 1; }
    return 0;
}
static int down_iter(mv_t *m) {             /* loop_player01down: one-pixel push out of the floor */
    place(m, S_BOTTOM); place(m, S_BOTTOMBIG); test(m, S_BOTTOM); test(m, S_BOTTOMBIG);
    if (m->G < 0) return 1;
    if (S[S_BOTTOM].z) { m->Y -= 1; return 1; }
    return 0;
}
static int edge_dir(double n) {             /* Active::set_direction((int)(N / 11.25)) with 8 images */
    int d = (int)(n / 11.25) & 31;
    switch (d & 3) {
    case 0: edge_last_exact = d; return d;
    case 1: return d - 1;
    case 3: return (d + 1) & 31;
    default:                                /* halfway: keep turning the way the last exact one lies */
        if (edge_last_exact < 0 || ((d - edge_last_exact) & 31) > 15) return (d + 2) & 31;
        return (d - 2) & 31;
    }
}
static void edges_iter(mv_t *m) {           /* loop_player01detectsensoredges (events 1508-1517) */
    int d = edge_dir(m->N);
    place(m, S_LEDGE); place(m, S_REDGE);
    S[S_LEDGE].mask = S[S_REDGE].mask = M_E0 + d / 4;
    for (int i = S_LEDGE; i <= S_REDGE; i++) {
        test(m, i);
        /* the object group +0x218f0 (monitors, springs, bridges, spikes, platforms, ...) clears it */
        const smask_t *k = &MK[S[i].mask];
        if (S[i].z && mask_hits_objs(OBJGRP_NOEDGE, k, S[i].x, S[i].y)) S[i].z = 0;
    }
}
static void angles_place_test(mv_t *m) {
    place(m, S_LANGLE); place(m, S_RANGLE); test(m, S_LANGLE); test(m, S_RANGLE);
}
/* loop_toolsanglecalculator: A = Lx - Rx, B = Ly - Ry of the two angle sensors */
static double angle_calc(double A, double B) {
    double v = 0, C;
    if (A == 0) C = 0;
    else { v = 0.0 - atan(B / A) * DEG; C = v; if (v < 0) { v += 180; C = v; } }
    if (B < 0) { v += 180; C = v; }
    if (v == 0) {
        if (0 < A) { C = 180; if (B < 0) C = 270; return C; }
        if (0 < B) return 90;
    }
    if (v != 180) return C;
    if (B < 0) C = 270;
    return C;
}
static double angle_detect(mv_t *m) {       /* events 1590 / 1596 / 1603: 20 scan steps */
    S[S_LANGLE].ya = S[S_RANGLE].ya = 0;
    for (int i = 0; i < 20; i++) {
        angles_place_test(m);
        if (!S[S_LANGLE].z) S[S_LANGLE].ya += 1;
        if (!S[S_RANGLE].z) S[S_RANGLE].ya += 1;
    }
    return angle_calc(S[S_LANGLE].x - S[S_RANGLE].x, S[S_LANGLE].y - S[S_RANGLE].y);
}
static int slopesdown_iter(mv_t *m) {       /* loop_player01slopesdown: follow the ground down */
    place_main(m); test(m, S_MAIN);
    place(m, S_SLOPES); test(m, S_SLOPES);
    if (!S[S_MAIN].z && S[S_SLOPES].z) { m->X += o_sin(m->N); m->Y += o_cos(m->N); }
    if (S[S_MAIN].z) { m->X += o_sin(m->N); m->Y += o_cos(m->N); return 0; }
    return 1;
}
static int slopesup_iter(mv_t *m) {         /* loop_player01slopesup: out of the ground along the normal */
    place_main(m); test(m, S_MAIN);
    if (!S[S_MAIN].z) return 0;
    m->X -= o_sin(m->N); m->Y -= o_cos(m->N);
    return 1;
}

void mv_allsensors(mv_t *m) {               /* loop_player01allsensors (event 1611 and others) */
    place_main(m); test(m, S_MAIN);
    place(m, S_LEFT); place(m, S_LEFTBIG); test(m, S_LEFT); test(m, S_LEFTBIG);
    place(m, S_RIGHT); place(m, S_RIGHTBIG); test(m, S_RIGHT); test(m, S_RIGHTBIG);
    place(m, S_TOP); place(m, S_TOPBIG); test(m, S_TOP); test(m, S_TOPBIG);
    place(m, S_BOTTOM); place(m, S_BOTTOMBIG); test(m, S_BOTTOM); test(m, S_BOTTOMBIG);
    place(m, S_SLOPES); test(m, S_SLOPES);
    angles_place_test(m);
    edges_iter(m);
}

/* leaving the ground (events 1609 / 1610): N is floored first; G = -B sin N, B = B cos N */
static void leave_ground(mv_t *m) {
    double n = floor(m->N);
    m->G = -(m->B * o_sin(n));
    m->B = o_cos(n) * m->B;
    m->N = m->O = 0; m->L = 0;
}

/* events 1584-1611 in order. Returns MV_LANDED / MV_LEFT_GROUND / MV_CEILING flags. */
int mv_core(mv_t *m) {
    int r = 0, n;
    /* 1584: move along N by B, then push out of walls (int)(|B| + 1) times */
    m->X = o_cos(m->N) * m->B + m->X;
    m->Y = m->Y - m->B * o_sin(m->N);
    n = (int)(fabs(m->B) + 1);
    for (int i = 0; i < n; i++) if (!wall_iter(m)) break;
    if (m->L == 0) {
        m->Y += m->G;                                       /* 1585: fall/rise, ceiling push-out */
        for (int i = 0; i < 10; i++) if (!up_iter(m)) break;
        for (int i = 0; i < 20; i++) if (!wall_iter(m)) break;   /* 1586 */
        for (int i = 0; i < 10; i++) if (!down_iter(m)) break;   /* 1587 */
    }
    /* 1588-1592: landing */
    if (m->L == 0 && m->G >= 0 && S[S_BOTTOMBIG].z) {
        m->N = m->O = 0;
        edges_iter(m);
        if (S[S_LEDGE].z && S[S_REDGE].z) m->N = m->O = angle_detect(m);
        if (m->N > 22 && m->N < 338) m->B = m->B - m->G * PHYS_LAND_J * o_sin(m->N);
        m->W = m->G; m->G = 0; m->L = 1;
        r |= MV_LANDED;
    }
    /* 1593-1600: hitting a ceiling hard enough attaches to it (running on the ceiling) */
    m->P = 0;
    if (m->L == 0 && m->G < -1.75 && S[S_TOPBIG].z) {
        m->P = 1; m->N = m->O = 180;
        mv_allsensors(m);
    }
    if (m->P == 1 && S[S_BOTTOMBIG].z) { m->P = 2; edges_iter(m); }
    if (m->P == 2 && S[S_LEDGE].z && S[S_REDGE].z) m->N = m->O = angle_detect(m);
    if (m->P == 2 && (m->N < 170 || m->N > 190)) { m->P = 3; m->B = m->B - m->G * PHYS_LAND_J * (o_sin(m->N) / 0.35); }
    if (m->P == 3) { m->P = 0; m->W = m->G; m->G = 0; m->L = 1; r |= MV_LANDED | MV_CEILING; }
    if (m->P > 0 && m->L == 0) m->N = m->O = 0;
    if (m->L == 1) {
        /* 1601: follow the ground down (up to 15 px), then out of it */
        for (int i = 0; i < 15; i++) if (!slopesdown_iter(m)) break;
        for (int i = 0; i < 15; i++) if (!slopesup_iter(m)) break;
        edges_iter(m);                                                /* 1602 */
        int both = S[S_LEDGE].z && S[S_REDGE].z;
        if (both) m->N = angle_detect(m);                             /* 1603 */
        if (both) {                                                   /* 1605-1608: smoothed angle O */
            if (fabs(360 + m->N - m->O) < fabs(m->N - m->O)) m->N = 360 + m->N;
            double d = m->N - m->O;
            if (fabs(d) < 45) {
                double k = 0.175;
                if (PHYS_ANGLE_D != 0 && (k = fabs(m->B) / PHYS_ANGLE_D) <= 0.175) k = 0.175;
                m->O = fmod(d * k + m->O, 360);
            }
            if (fabs(m->N - m->O) >= 45) m->O = fmod(m->N, 360);
            m->N = fmod(m->N, 360);
        }
        /* 1609: too slow on a wall or ceiling (75 <= N <= 285): fall off */
        if (m->N >= 75 && m->N <= 285) {
            double t = 0.4, c = o_cos(m->N);
            if (m->N != 90 && m->N != 270 && c <= 0) t = 2 * fabs(c) + 0.4;
            if (fabs(m->B) < t) { leave_ground(m); r |= MV_LEFT_GROUND; }
        }
        /* 1610: the bottom-big sensor (as last tested) is clear: leave the ground */
        if (m->L == 1 && !S[S_BOTTOMBIG].z) { leave_ground(m); r |= MV_LEFT_GROUND; }
    }
    mv_allsensors(m);                                                 /* 1611 */
    return r;
}
