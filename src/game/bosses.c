/* Minibosses and bosses (RE: build/re/spec_wretchnid.md, spec_dv_enemies.md §5, spec_dv_bosses.md).
 * Each fight is an event script; its state lives in a static struct registered for snapshots. The hittable parts
 * (Wretchnid weakspot, Tredd eye, snake segments / hitbox) are ordinary qualifier-20 enemies in combat.c. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../fp.h"

static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / 57.29577951); }
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / 57.29577951); }
static obj_t *first_of(int type) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == type) return &G.o[i]; return NULL; }
static void set_ens(obj_t *o, const char *name) { int a = ens_anim(name); if (a >= 0) anim_set(&o->an, &G.ens, a); }
static void box_of(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = *x1 = o->x; *y0 = *y1 = o->y; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int inside_view(const obj_t *o, int n) {
    float x0, y0, x1, y1; box_of(o, &x0, &y0, &x1, &y1);
    return x0 >= G.camx - n && x1 <= G.camx + SCREEN_W + n && y0 >= G.camy - n && y1 <= G.camy + SCREEN_H + n;
}

/* ================================================================ Wretchnid (DV1, events 4844-4871) */
static struct {
    int active, defeat, burst, unlocked;   /* G_WRETCH, G_DEFEAT (G_FIGHT = active && !defeat), once latches */
    double A, B, C; int E;                 /* Base phase, y (double), amplitude, direction */
    int turretA, baseG, railG, weakF, weakBlink;
    double legA[12], legB[12], legC[12];
} WR;
static const double LEG0[12] = { 210, 270, 0, 150, 90, 0, 330, 270, 180, 30, 90, 180 };   /* 4853 base angles: L1-6, R1-6 */
static const int LEGSGN[12] = { 1, -1, -1, 1, -1, -1, 1, -1, -1, 1, -1, -1 };
static const double BURSTV[12][2] = { {-3,-5}, {-2,-4}, {-1,-2}, {-6,-5}, {-5,-4}, {-4,-2}, {3,-5}, {2,-4}, {1,-2}, {6,-5}, {5,-4}, {4,-2} };
static obj_t *leg(int k) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_WR_LEG + k) return &G.o[i]; return NULL; }
/* rotated action point of a 32 px segment: joint + (int)(32 cos A, -32 sin A) (INFERRED rounding, spec §4) */
static void seg_tip(const obj_t *o, float *x, float *y) {
    double a = o->ang;
    *x = o->x + (float)(int)(32.0 * c_cos(fmod(a + 360.0, 360.0)));
    *y = o->y + (float)(int)(-32.0 * c_sin(fmod(a + 360.0, 360.0)));
}
static void wr_spawn(void) {
    memset(&WR, 0, sizeof WR); WR.E = 1;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_WR_BASE: set_ens(o, "wr_base"); break;
        case OT_WR_TURRET: set_ens(o, "wr_turret"); o->dir = 0; break;
        case OT_WR_WEAK: set_ens(o, "wr_weak"); o->A = 50; o->bd = 0; break;
        case OT_WR_RAIL: set_ens(o, "wr_rail"); break;
        case OT_WR_BARRIER: set_ens(o, "wr_barrier"); break;
        default:
            if (o->type >= OT_WR_LEG && o->type < OT_WR_LEG + 12) {
                int k = o->type - OT_WR_LEG, seg = k % 3;
                set_ens(o, seg == 0 ? "wr_seg1" : seg == 1 ? "wr_seg2" : "wr_seg3");
                static const double A0[12] = { 225, 270, 0, 135, 90, 0, 315, 270, 180, 45, 90, 180 };
                o->ang = (float)A0[k];
            }
        }
    }
}
int boss_invert_base(void) { return 0; }
static void wr_update(player_t *p) {
    obj_t *base = first_of(OT_WR_BASE), *tur = first_of(OT_WR_TURRET), *weak = first_of(OT_WR_WEAK);
    if (!WR.active) {                                                                        /* 4844 */
        if (base && inside_view(base, 150)) WR.active = 1; else return;
    }
    if (!WR.defeat && base) {
        if (WR.B == 0) { WR.B = base->y; WR.C = 2.2; }                                        /* 4845 */
        base->y = (float)(int)WR.B;                                                          /* 4846 */
        if (WR.E == 1) { WR.A += 5; WR.B += fabs(c_cos(fmod(WR.A + 3600, 360))) * WR.C; }   /* 4847 */
        else { WR.A -= 5; WR.B -= fabs(c_cos(fmod(WR.A + 3600, 360))) * WR.C; }             /* 4848 */
        if (WR.E == 1 && WR.A > 270) WR.E = -1;                                              /* 4849 */
        else if (WR.E == -1 && WR.A < -270) WR.E = 1;                                        /* 4850 */
        /* 4851: rig positions from last frame's angles */
        for (int side = 0; side < 2; side++)
            for (int chain = 0; chain < 2; chain++) {
                int k0 = side * 6 + chain * 3;
                obj_t *a = leg(k0), *b = leg(k0 + 1), *c = leg(k0 + 2);
                if (a) { a->x = base->x + (side ? 11 : -11); a->y = base->y; }
                if (a && b) seg_tip(a, &b->x, &b->y);
                if (b && c) seg_tip(b, &c->x, &c->y);
            }
        if (tur) { tur->x = base->x; tur->y = base->y - 20; }
        if (weak) { weak->x = base->x; weak->y = base->y; }
        /* 4852: segments take the angles of 4853 (last frame), turret aims at Player_326 (truncated direction) */
        for (int k = 0; k < 12; k++) { obj_t *o = leg(k); if (o) o->ang = (float)WR.legA[k]; }
        if (tur) {
            double a = fd_atan2(tur->y - (int)(p->y - ORIG_REF_DY), (int)p->x - tur->x) * 57.29577951; if (a < 0) a += 360;
            tur->dir = (int)(a / 11.25) & 31; tur->ang = (float)(tur->dir * 11.25);              /* auto-rotate */
        }
        double s = c_sin(fmod(WR.A + 3600, 360));                                            /* 4853 */
        for (int k = 0; k < 12; k++) WR.legA[k] = LEG0[k] + LEGSGN[k] * s * 25;
        /* 4854-4860: turret clock and 5 shots */
        if (tur) {
            if (inside_view(base, 120) || WR.turretA > 100) WR.turretA++;
            if (WR.turretA == 100) { set_ens(tur, "wr_turret_fire"); WR.turretA++; snd_play_orig(74, 220, 128); }
            if (WR.turretA == 140 || WR.turretA == 155 || WR.turretA == 170 || WR.turretA == 185 || WR.turretA == 200) {
                double a = tur->ang / 57.29577951;
                obj_spawn_pellet(tur->x + (float)(int)(27 * fd_cos(a)), tur->y + (float)(int)(-27 * fd_sin(a)), tur->dir);
                snd_play_orig(28, 220, 128);
                if (WR.turretA == 200) { WR.turretA = -80; set_ens(tur, "wr_turret"); } else WR.turretA++;
            }
        }
        if (weak && weak->A <= 0) { WR.defeat = 1; set_ens(base, "wr_base_die"); }          /* 4863 */
    }
    if (WR.defeat) {
        if (WR.weakF > 1) { WR.weakBlink = !WR.weakBlink; WR.weakF = 0; }                    /* 4864-4866 */
        if (base && G.frame % 7 == 0) {                                                      /* 4867 */
            int rx = cross_rand(41), ry = cross_rand(61);
            obj_effect(base->x - 20 + rx, base->y - 30 + ry, "explosion06");
            WR.baseG++;
            static const int ex[3] = { 311, 319, 320 };
            snd_play_orig(ex[cross_rand(3)], 220, 128);
        }
        if (base && WR.baseG > 20 && !WR.burst) {                                            /* 4868 */
            WR.burst = 1; snd_play_orig(38, 220, 128);
            for (int k = 0; k < 12; k++) { WR.legB[k] = BURSTV[k][0]; WR.legC[k] = BURSTV[k][1]; }
            obj_effect(base->x, base->y + 21, "explosion"); obj_effect(base->x, base->y, "explosion");
            if (tur) tur->alive = 0;
            base->alive = 0; base = NULL;
        }
        if (!base && WR.burst) {                                                             /* 4869 / 4870 */
            for (int k = 0; k < 12; k++) { obj_t *o = leg(k); if (o) { o->x = (float)(int)(o->x + WR.legB[k]); o->y = (float)(int)(o->y + WR.legC[k]); WR.legC[k] += 0.4; } }
            WR.weakF++; WR.railG++;
        }
        if (WR.railG > 40 && !WR.unlocked) {                                                 /* 4871: unlock */
            WR.unlocked = 1;
            if (weak) weak->alive = 0;
            for (int i = 0; i < G.nobjs; i++) {
                obj_t *o = &G.o[i];
                if (!o->alive) continue;
                if (o->type == OT_WR_RAIL) {
                    for (int k = 0; k < 2; k++) {
                        obj_t *c = obj_alloc();
                        if (!c) break;
                        c->type = OT_CRYSTAL; c->alive = 1; c->x = o->x; c->y = o->y + (k ? 36 : 12);
                        int a = bank_anim(&G.objs, "objectcrystal_3"); if (a >= 0) anim_set(&c->an, &G.objs, a);
                    }
                    o->alive = 0;
                }
                if (o->type == OT_WR_BARRIER || (o->type >= OT_WR_LEG && o->type < OT_WR_LEG + 12)) o->alive = 0;
            }
            snd_play_orig(40, 220, 128);
        }
    }
}

/* ================================================================ dispatch */
void bosses_spawn(void) {
    wr_spawn();
    snap_register(&WR, sizeof WR);
}
void bosses_update(player_t *p) {
    if (first_of(OT_WR_BASE) || WR.active) wr_update(p);
}
/* legs / turret visibility and the Base flash (4861/4862: inverted while the weakspot's C > 0 and A > 0) */
int boss_hidden(const obj_t *o) {
    if (o->type == OT_WR_BARRIER || o->type == OT_WR_WEAK) return 1;
    if (WR.burst && !WR.weakBlink && (o->type == OT_WR_TURRET || (o->type >= OT_WR_LEG && o->type < OT_WR_LEG + 12))) return 1;
    return 0;
}
int boss_flash(const obj_t *o) {
    if (o->type == OT_WR_BASE) { obj_t *w = first_of(OT_WR_WEAK); return w && w->c > 0 && w->A > 0; }
    return o->inv;
}
void bosses_draw(void) {}
