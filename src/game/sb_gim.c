/* Sky Battalion gimmicks (session 19): the hover platforms LevelSBPlatform1_2095 (+ LevelSBPlatformFlame1_2096) with the
 * SB2-SB4 opening hold / release, LevelSBPlatform3_2188, the loop pieces (SBLoop1-7, Active..Active4), the decor (fan blades,
 * fans, scrolling chains, back eyes).
 * Source: build/re/spec_sb_gimmicks.md. Labels: RE (read from the events / constructors), RV (original traces session 19),
 * INFERRED (marked).
 * Passes: sb_gim_pre (objects_platforms, before the movement: the one-way flags 4737 / 4738, 9344 / 9345), sb_gim_obj (the
 * objects.c per-object pass: each object's own events), sb_gim_frame (after the objects: the path step + carry 3283-3289 /
 * 4061, the opening hold / release 9461 / 9468 / 9548 / 9549, the dip 3303-3307).
 * Performance: every type keeps its instance list (spawn + runtime children, EACH); nothing runs without these types (S.on).
 * Object fields: par = family member (importer SBG_SUB: lvobj anim bits 0-7), layer = instance layer (bits 8-15); path
 * platforms: av[0] A, av[1] E, av[2] B, av[3] F, av[4] C, av[5] G, kx I, ky J, c bits D / H, av[6] / av[7] last M / N,
 * vx / vy the frame's O / P, fy the dip; fl bit 0 + 0x40 = one-way pass (shared meaning), own flags >= 0x1000. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAD 57.29577951308232
#define RT0 OT_SBG_FIRST
#define NRT (OT_SBG_LAST - OT_SBG_FIRST + 1)
#define LMAX 128
#define F_REL 0x800                                /* platform1 flag 11: released (9468), being flown in */
#define F_STOOD 0x1000                             /* dip: stood on this frame */
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]
static struct { int on; double Q[LMAX]; } S;       /* Q: platform1 alterable Q (its I before the hold), by list slot */

static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static double c_sin(double deg) { double r = nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / RAD); }
static double c_cos(double deg) { double r = nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / RAD); }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }

static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static int slot_of(int T, const obj_t *o) { for (int k = 0; k < NLS[T - RT0]; k++) if (&G.o[LST[T - RT0][k]] == o) return k; return 0; }

/* ================================================================ path platforms (3283-3289) + carry (4061) + dip (Q12) */
static void path_init(obj_t *o, double A, double E, double B, double F, double C, double Gm, int D, int H) {
    o->kx = o->x; o->ky = o->y; o->av[0] = A; o->av[1] = E; o->av[2] = B; o->av[3] = F; o->av[4] = C; o->av[5] = Gm; o->c = D | (H << 1);
    o->av[6] = o->kx + C * ((o->c & 1) ? c_sin(A) : c_cos(A)); o->av[7] = o->ky + Gm * ((o->c & 2) ? c_sin(E) : c_cos(E));
    o->x = (float)floor(o->av[6]); o->y = (float)floor(o->av[7]); o->vx = o->vy = 0; o->fy = 0;
}
static void path_step(obj_t *o) {                    /* 3283-3289: A += B, E += F; M = I + C cos|sin A, N = J + G cos|sin E */
    o->av[0] += o->av[2]; o->av[1] += o->av[3];
    double M = o->kx + o->av[4] * ((o->c & 1) ? c_sin(o->av[0]) : c_cos(o->av[0]));
    double Nn = o->ky + o->av[5] * ((o->c & 2) ? c_sin(o->av[1]) : c_cos(o->av[1]));
    o->vx = (float)(floor(M) - floor(o->av[6])); o->vy = (float)(floor(Nn) - floor(o->av[7]));
    o->av[6] = M; o->av[7] = Nn; o->x = (float)floor(M); o->y = (float)floor(Nn);
}
static void carry(player_t *p) {                     /* 4061: G >= 0, S_BB 1 px lower on a Q13 platform with flag 6 off */
    static const int Q13[] = { OT_SB_PLAT1, OT_SB_PLAT3 };
    if (!(p->vy >= 0 || p->ground) || dead(p) || p->state == ST_HANG || p->state == ST_HELD) return;
    for (unsigned q = 0; q < sizeof Q13 / sizeof Q13[0]; q++) {
        if (!N(Q13[q])) continue;
        int k = mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, Q13[q]);
        if (k < 0 || (G.o[k].fl & 0x40)) continue;
        p->x += G.o[k].vx; p->y += G.o[k].vy;
        if (!p->ground) { p->gsp = p->vx; p->ground = 1; }
        p->vy = 0; p->N = 0; p->O = 0;
        player_sync_sensors(p);
        break;
    }
}

/* ================================================================ spawn */
static const char *const LOOPN[3][5] = { { "sbl_0_0", "sbl_0_1", "sbl_0_2", "sbl_0_3", "sbl_0_4" },
                                         { "sbl_1_0", "sbl_1_1", "sbl_1_2", "sbl_1_3", "" }, { "sbl_2_0", "sbl_2_1", "", "", "" } };
void sb_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS);
    if (!objects_have_from(RT0)) return;
    /* family member / layer: the k-th object of a type is the k-th placement of that type (objects_spawn keeps the order) */
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < RT0 || o->type > OT_SBG_LAST) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0xff;
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = 0; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        lst_add(o); S.on = 1;
    }
    if (!S.on) return;
    obj_t *o, *q;
    int k, n;
    /* 9766 (start of the act): platform3 A = index 20; platform1 E = index 180, A = E + 90 (index = selection order, newest
     * first: the last placement is 0). Constructors (RE): platform1 A 90 B 1 C 40 F 2 G 10 H 1; platform3 B 2 C 128 */
    n = N(OT_SB_PLAT1); k = 0;
    EACH(OT_SB_PLAT1, o) { double E = (n - 1 - k++) * 180.; set_s(o, "sbp_plat1"); path_init(o, E + 90, E, 1, 2, 40, 10, 0, 1); }
    n = N(OT_SB_PLAT3); k = 0;
    EACH(OT_SB_PLAT3, o) { set_s(o, "sbp_plat3"); path_init(o, (n - 1 - k++) * 20., 0, 2, 0, 128, 0, 0, 0); }
    EACH(OT_SB_PLATFLAME, o) {                       /* each flame rides the platform placed at (+1, -18) from it (INFERRED pairing) */
        set_s(o, "sbp_flame"); o->par = -1; float best = 1e9f;
        EACH(OT_SB_PLAT1, q) { float d = (float)(fabs(q->kx - (o->x + 1)) + fabs(q->ky - (o->y - 18))); if (d < best) { best = d; o->par = (int)(q - G.o); } }
        if (best > 4) o->par = -1;
    }
    EACH(OT_SB_LOOP0, o) set_s(o, LOOPN[0][o->par < 5 ? o->par : 0]);
    EACH(OT_SB_LOOP1, o) set_s(o, LOOPN[1][o->par < 4 ? o->par : 0]);
    EACH(OT_SB_LOOP1B, o) set_s(o, LOOPN[2][o->par < 2 ? o->par : 0]);
    EACH(OT_SB_FANBLADE, o) { static const char *const B[3] = { "sbv_blade0", "sbv_blade1", "sbv_blade2" }; set_s(o, B[o->par < 3 ? o->par : 0]); }
    EACH(OT_SB_FAN, o) set_s(o, "sbv_fan");
    EACH(OT_SB_FANSIDE, o) set_s(o, "sbv_fanside");
    EACH(OT_SB_SCHAIN, o) set_s(o, "sbv_chain");
    EACH(OT_SB_BACKEYE, o) set_s(o, o->par ? "sbv_eye1" : "sbv_eye0");
}

/* ================================================================ passes */
void sb_gim_pre(player_t *p) {
    if (!S.on) return;
    obj_t *o; double Y = PY(p);
    EACH(OT_SB_PLAT1, o) { o->fl &= ~0x41; if (o->y < Y) o->fl |= 0x41; }            /* 4737 / 4738 (Q13) */
    EACH(OT_SB_PLAT3, o) { o->fl |= 0x41; if (o->y > Y + 8) o->fl &= ~0x41; }        /* 9344 / 9345: solid only under her */
}

int sb_gim_obj(obj_t *o, player_t *p) {
    (void)p;
    switch (o->type) {
    case OT_SB_FANBLADE: o->ang = (float)nrm(o->ang + 3); return 1;                  /* 9346: A = (A + 3) mod 360, angle A */
    default: return 0;
    }
}

void sb_gim_frame(player_t *p) {
    if (!S.on) return;
    obj_t *o;
    /* the dip of the last frame comes off, then the path step (3270-3272, 3283-3289) and the carry (4061) */
    EACH(OT_SB_PLAT1, o) { o->y -= floorf(o->fy); if (!(o->fl & F_STOOD)) o->fy -= o->fy * 0.2f; o->fl &= ~F_STOOD; }
    EACH(OT_SB_PLAT1, o) path_step(o);
    EACH(OT_SB_PLAT3, o) path_step(o);
    carry(p);
    /* SB2-SB4 opening (sb_flow.c: group 0x228f3) */
    int intro = sb_flow_intro();
    EACH(OT_SB_PLAT1, o) {
        int s = slot_of(OT_SB_PLAT1, o);
        if (intro && o->kx > 0 && o->kx < 1200 && !(o->fl & F_REL)) { S.Q[s] = o->kx; o->kx = -160; o->av[4] = 0; }   /* 9461: Q = I, I -160, C 0 */
        if (intro == 2 && o->kx < 0) o->fl |= F_REL;                                   /* 9468: the planks are out (A 2) */
        if (o->fl & F_REL) { o->kx = fmin(o->kx + 4, S.Q[s]); o->av[4] = fmin(o->av[4] + 0.5, 40); }   /* 9548 */
        if ((o->fl & F_REL) && o->kx >= S.Q[s]) o->fl &= ~F_REL;                       /* 9549 */
    }
    /* the dip (3303 / 3304 / 3307, Q12): stood on -> S eases to 9, the drawn y = N + S */
    EACH(OT_SB_PLAT1, o) {
        if (p->ground && !dead(p) && mv_sensor_obj(p->x, MY(p), p->N, SENSOR_BOTTOMBIG, OT_SB_PLAT1) == (int)(o - G.o)) o->fl |= F_STOOD;
        if (o->fl & F_STOOD) o->fy += (9 - o->fy) * 0.2f;
        o->y += floorf(o->fy);
    }
    EACH(OT_SB_PLATFLAME, o) if (o->par >= 0 && G.o[o->par].alive) { o->x = G.o[o->par].x - 1; o->y = G.o[o->par].y + 18; }
}

void sb_gim_draw(int pass) { (void)pass; }
int  sb_gim_q20(int t) { (void)t; return 0; }
