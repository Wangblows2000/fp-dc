/* Serpentine (RE: build/re/spec_dv_bosses.md; RV: build/orig_traces/snake_t1.csv)
 *   DV2: the geyser encounter (events 5049-5093): the snake arcs between 5 geyser holes, rests and spits a
 *        fireball, its 7 segments (HP 8) are the targets, then the head hitbox (HP 1) once; it flees right through
 *        the arena wall and the act continues.
 *   DV4: the boss fight (5461-5591, spec §5) -- see serp4_* below.
 * Head / body / hitbox are level objects (OT_SNAKE_HEAD / BODY / HIT); segments and hitbox are qualifier-20
 * enemies, so hits, flash and HP come from combat.c. Flag 16 (F_NOFLY) keeps dead segments in the chain. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fp.h"

#define F_ARMOURED 0x20
#define F_DEFEATED 0x40
#define NSEG 8

/* Chowdren degree trig on the raw angle (x / 57.29577951308232, no reduction: RV snake_t1 8 cos(8t) with t ~ 3e5);
 * exact 0 at 180 (sin) and 90 / 270 (cos), cos 180 = -1 (spec §3.4) */
static double c_sin(double deg) { return deg == 180. || deg == 0. ? 0. : fd_sin(deg / 57.29577951308232); }
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : deg == 180. ? -1. : fd_cos(deg / 57.29577951308232); }
static void set_ens(obj_t *o, const char *name) { int a = ens_anim(name); if (a >= 0) anim_set(&o->an, &G.ens, a); }
static void force_frame(obj_t *o, int f) { if (o->an.b) { const anim_t *a = &o->an.b->anims[o->an.anim]; o->an.frame = f < 0 ? 0 : f >= a->count ? a->count - 1 : f; o->an.acc = 0; } }

static struct {
    int level;                       /* 2 / 4, 0 = no snake in this act */
    float climL, climR, climT, climB, trigX;
    int cam, active, done, setup, fled, flee_music;
    /* head alterables */
    double A, C, D, F, H; int B, E, Gt, fire_latch;
    int nseg, ngey;
    float gx[5], gy[5]; int gA[5], gkind[5];
    double segG[NSEG]; int segH[NSEG], segF[NSEG]; double segE[NSEG];
    double hitG;                     /* hitbox G (flee frames) */
    double hV, hW;                   /* head V, W (chain leader position) */
    int gc0;                         /* global counter offset (GLOBALS+0x18) */
} SN;

static obj_t *head(void) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SNAKE_HEAD) return &G.o[i]; return NULL; }
static obj_t *hitbox(void) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SNAKE_HIT) return &G.o[i]; return NULL; }
static obj_t *seg(int h) {   /* body with H == h */
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SNAKE_BODY && (int)G.o[i].av[0] == h) return &G.o[i];
    return NULL;
}
static int geyser_by_A(int a) { for (int k = 0; k < SN.ngey; k++) if (SN.gA[k] == a) return k; return -1; }
static int gcount(void) { return SN.gc0 + G.frame; }

void serp_spawn(void) {
    memset(&SN, 0, sizeof SN);
    snap_register(&SN, sizeof SN);
    if (!head()) return;
    const char *d = G.lvdir;
    if (strstr(d, "dv2")) { SN.level = 2; SN.climL = 8240; SN.climR = 9039; SN.climT = 632; SN.climB = 919; SN.trigX = 8568; }
    else if (strstr(d, "dv4")) { SN.level = 4; SN.climL = 12360; SN.climR = 13191; SN.climT = 1832; SN.climB = 2303; SN.trigX = 12608; }
    else return;
#ifdef FP_HOST_ENV
    if (getenv("FP_GCOUNT0")) SN.gc0 = atoi(getenv("FP_GCOUNT0"));
#endif
    /* geyser markers: selection order is last-created first (spec §1 notes; RV snake_t1: 8768 = 0, 8512 = 1,
     * 8896 = 2, 8640 = 3, 8384 = 4) -> indices in reverse level order, geyseroverlap1 first */
    int n1 = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = G.nobjs - 1; i >= 0; i--) {
            obj_t *o = &G.o[i];
            if (!o->alive || o->type != (pass ? OT_GEYSER_HOLE2 : OT_GEYSER_HOLE1) || SN.ngey >= 5) continue;
            SN.gx[SN.ngey] = o->x; SN.gy[SN.ngey] = o->y; SN.gA[SN.ngey] = SN.ngey; SN.gkind[SN.ngey] = pass + 1; SN.ngey++;
            if (!pass) n1++;
            o->an.b = NULL;
        }
    (void)n1;
    /* bodies: H = 1 for the last created (RV: the first body in the frame has H 7) */
    int h = 0;
    for (int i = G.nobjs - 1; i >= 0; i--) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_SNAKE_BODY) continue;
        o->av[0] = ++h; o->A = 8; o->bd = 0; set_ens(o, "sb_idle"); o->hidden = 1;
    }
    SN.nseg = h;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_SNAKE_HEAD) { set_ens(o, "sh_rest"); o->hidden = 1; SN.A = -30; SN.H = -1; }
        if (o->type == OT_SNAKE_HIT) { set_ens(o, "sx_rest"); o->hidden = 1; o->A = 1; o->bd = 0; }
        if (o->type == OT_CAMTRIGGER) o->an.b = NULL;
    }
}

int serp_hidden(const obj_t *o) {
    if (o->type == OT_SNAKE_HIT || o->type == OT_CAMTRIGGER || o->type == OT_GEYSER_HOLE1 || o->type == OT_GEYSER_HOLE2) return 1;
    return 0;
}

/* arc position (5057): a = min(A, 180), x = int(C - F/2 + F/2 cos a), y = int(D - 180 |sin a|) */
static void arc_pos(double A, float *x, float *y) {
    double a = A < 180 ? A : 180;
    *x = (float)(int)(SN.C - SN.F / 2 + SN.F / 2 * c_cos(a));
    *y = (float)(int)(SN.D - 180 * fabs(c_sin(a)));
}

static void fireball_spawn(float x, float y, int dir) {
    obj_t *f = obj_alloc();
    if (!f) return;
    f->type = OT_FIREBALL; f->alive = 1; f->x = x; f->y = y; f->dir = dir; f->fx = 0; f->fy = 0;
    set_ens(f, "fireball");
}

/* BallMovement (RE BallMovement::update): speed * 0.125 px per frame along the direction, floor steps with the
 * fraction carried; fireball speed 30. Damage 10 to Lilac's sensor (5071). Removal: INFERRED (background or
 * 64 px outside the view). */
void serp_fireball_update(obj_t *f, player_t *p) {
    double a = f->dir * 11.25 / 57.29577951308232, v = 30 * 0.125;
    double dx = (f->dir == 0 ? 1 : f->dir == 16 ? -1 : f->dir == 8 || f->dir == 24 ? 0 : fd_cos(a)) * v + f->fx;
    double dy = (f->dir == 8 ? -1 : f->dir == 24 ? 1 : f->dir == 0 || f->dir == 16 ? 0 : -fd_sin(a)) * v + f->fy;
    double sx = floor(dx), sy = floor(dy);
    f->fx = (float)(dx - sx); f->fy = (float)(dy - sy);
    f->x += (float)sx; f->y += (float)sy;
    if (p->state != ST_HURT && p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p)) {
        int px = (int)floor(p->x), py = (int)floor(p->y - ORIG_REF_DY);
        if (obj_rect_hit(f, px - 7, py - 34, px + 8, py + 12)) player_contact(p, 10);
    }
    if (obj_overlaps_bg(f) || f->x < G.camx - 64 || f->x > G.camx + SCREEN_W + 64 || f->y < G.camy - 64 || f->y > G.camy + SCREEN_H + 64) f->alive = 0;
}

static void head_rest_frame(obj_t *hd) { force_frame(hd, (int)(1.5 + SN.H)); }

/* ================================================================ per frame */
static void dv2_update(player_t *p) {
    obj_t *hd = head(), *hb = hitbox();
    if (!hd) return;
    /* 5049: Lilac inside the arena's x range before the fight -> boss camera (bounds ease to the limits) */
    if (!SN.cam && !SN.done && p->x > SN.climL && p->x < SN.climR) { SN.cam = 1; camera_boss_bounds(SN.climL, SN.climT, SN.climR, SN.climB); }
    /* 5050: the character sensor reaches the trigger column (16x192 at trigX) -> boss active (one-shot) */
    if (!SN.active && !SN.done && floor(p->x) + 8 > SN.trigX && p->y > SN.climT && p->y < SN.climB + 64) SN.active = 1;
    /* 5051: invisible left wall at climL + 16 (the camera bound gives the same clamp in the port) */
    if (!SN.active) return;
    if (!SN.setup) {                                                                   /* 5052 */
        SN.setup = 1; SN.E = 1; SN.Gt = 2; SN.B = 0;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || o->type != OT_SNAKE_BODY) continue;
            o->scale = (float)fmin(1.0, 1.2 - 0.05 * o->av[0]); o->fl |= F_NOFLY; o->hidden = 0;
        }
        hd->hidden = 0;
        if (hb) { hb->fl |= F_ARMOURED; }
    }
    int t = gcount();
#ifdef FP_HOST_ENV
    if (getenv("FP_SN_KILL") && G.frame == atoi(getenv("FP_SN_KILL"))) for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->A = 0; }
    if (getenv("FP_SN_KILLH") && G.frame == atoi(getenv("FP_SN_KILLH")) && hb) hb->A = 0;
#endif
    if (!SN.fled) {
        if (SN.B == 0) {                                                               /* 5053 / 5054 */
            int g = geyser_by_A(SN.E);
            int g1 = 0; for (int k = 0; k < SN.ngey; k++) if (SN.gkind[k] == 1) { g1 = k; break; }
            if (g >= 0) { SN.A = 0; SN.C = SN.gx[g]; SN.D = SN.gy[g1] + 40; SN.B = 1; }   /* RE quirk: geyser1's y */
        }
        if (SN.B == 1) {                                                               /* 5055 / 5056 */
            int g = geyser_by_A(SN.Gt);
            if (g >= 0) { SN.F = SN.C - SN.gx[g]; SN.B = 2; }
        }
        if (SN.B == 2) {                                                               /* 5057 arc */
            float x, y;
            set_ens(hd, SN.F >= 1 ? "sh_arc_l" : "sh_arc");
            arc_pos(SN.A, &x, &y); hd->x = x; hd->y = y;
            double cf = SN.F < -1 ? -1 : SN.F > 1 ? 1 : SN.F;
            double ang = cf * SN.A; ang = ang < -180 ? -180 : ang > 180 ? 180 : ang;
            hd->ang = (float)fmod(ang + 360.0, 360.0);
            for (int h = 1; h <= SN.nseg; h++) {
                obj_t *b = seg(h);
                if (!b) continue;
                double g = fmax(0.0, SN.A - 12.0 * h);
                SN.segG[h - 1] = g;
                arc_pos(g, &b->x, &b->y);
                if (!(b->fl & F_DEFEATED)) set_ens(b, "sb_idle");
            }
            if (t % 10 == 5) snd_play_orig(35, 200, 128);     /* 5058: positional sample, id UNKNOWN U3 (slot 35 INFERRED) */
            SN.A += 1.5;
            /* 5059 / 5060 landing: the last segment has reached 180 and the head overlaps the target marker */
            if (SN.A >= 12.0 * SN.nseg + 180) {
                for (int k = 0; k < SN.ngey; k++) {
                    if (SN.gA[k] != SN.Gt) continue;
                    if (hd->x >= SN.gx[k] - 40 && hd->x < SN.gx[k] + 40 && hd->y >= SN.gy[k] && hd->y < SN.gy[k] + 96) {
                        SN.B = 3; SN.C = SN.gx[k]; SN.D = SN.gy[k] + 40; SN.A = 0; SN.F = 0; SN.E = SN.gA[k];
                        set_ens(hd, "sh_rest"); hd->ang = 0;
                    }
                }
            }
        }
        if (SN.B == 3) {                                       /* rest at a geyser (also on the landing frame: 5061+ follow 5059/5060) */
            if (p->x <= SN.C) SN.H = fmax(-1.0, SN.H - 0.1); else SN.H = fmin(1.0, SN.H + 0.1);   /* 5061 / 5062 */
            hd->x = (float)(int)(SN.C + 20 * SN.H - 20 * SN.H * c_cos(SN.A) + 8 * c_sin(4.0 * t));    /* 5063 */
            hd->y = (float)(int)(SN.D - SN.A + 8 * c_cos(8.0 * t));
            for (int i = 1; i <= SN.nseg; i++) {
                obj_t *b = seg(i);
                if (!b) continue;
                double g = fmax(0.0, 24 + SN.A - 24.0 * i);
                b->x = (float)(int)(SN.C - 20 * SN.H + 20 * SN.H * c_cos(g) + 8 * c_sin(4.0 * t + 15 * i));
                b->y = (float)(int)(SN.D - g + 8 * c_cos(8.0 * t + 15 * i));
            }
            head_rest_frame(hd);
            if (SN.A >= 80) SN.F += 1;                                                 /* 5064 */
            if (SN.F < 80) SN.A = fmin(SN.A + 4, 80);                                  /* 5065 */
            if (SN.F >= 80) SN.A = fmax(SN.A - 4, 0);                                  /* 5066 */
            if (SN.F == 5) { set_ens(hd, "sh_fire"); head_rest_frame(hd); SN.fire_latch = 0; }   /* 5067 */
            if (SN.F == 40) { set_ens(hd, "sh_rest"); head_rest_frame(hd); }           /* 5068 */
            if (hd->an.b && hd->an.anim == ens_anim("sh_fire") && hd->an.frame != 1 && !SN.fire_latch) {   /* 5069 */
                fireball_spawn(hd->x, hd->y, hd->an.frame * 8 + 16 >= 32 ? 0 : hd->an.frame * 8 + 16);
                snd_play_orig(46, 220, 128); SN.fire_latch = 1; SN.F = 6;
            }
            if (SN.F >= 60 && SN.A <= 0) {                                             /* 5072 / 5073 */
                SN.Gt = cross_rand_raw(SN.ngey);          /* RE: (rand15 * (count1 + count2 - 2)) >> 15 over list counts n + 1 */
                if (SN.Gt != SN.E) SN.B = 0;
            }
        }
    }
    /* hitbox (5074 / 5078 / 5079): on the head with its animation / frame / angle; immune while a segment lives */
    if (hb && !SN.fled) {
        hb->x = hd->x; hb->y = hd->y; hb->ang = hd->ang;
        const char *nm = hd->an.b ? hd->an.b->anims[hd->an.anim].name : "sh_rest";
        char hn[16]; snprintf(hn, sizeof hn, "sx%s", nm + 2); set_ens(hb, hn); force_frame(hb, hd->an.frame);
        int alive = 0;
        for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b && !(b->fl & F_DEFEATED)) alive = 1; }
        if (alive) hb->c = 3;
        else if (hb->c <= 0 && (hb->fl & F_ARMOURED)) hb->fl &= ~F_ARMOURED;
    }
    /* segment death (5075-5077): 60 frames of spinning skull frames, explosion, sound 45 */
    for (int h = 1; h <= SN.nseg; h++) {
        obj_t *b = seg(h);
        if (!b || !(b->fl & F_DEFEATED)) continue;
        if (!SN.segH[h - 1]) {
            SN.segH[h - 1] = 1; SN.segF[h - 1] = 60; SN.segE[h - 1] = 0;
            set_ens(b, "sb_skull"); obj_effect(b->x, b->y, "explosion06"); snd_play_orig(45, 220, 128);
        }
        if (SN.segF[h - 1] > 0) { SN.segF[h - 1]--; SN.segE[h - 1] += fmin(1.0, 0.01 * SN.segF[h - 1]); }
        if (SN.segE[h - 1] > 4) SN.segE[h - 1] -= 4;
        force_frame(b, (int)SN.segE[h - 1]);
    }
    /* defeat and flee (5080-5093; polarity of 5085-5091 UNKNOWN U7: the hitbox charges right at 8 px/frame) */
    if (hb && (hb->fl & F_DEFEATED) && !SN.fled) {
        SN.fled = 1; SN.hitG = 0;
        hd->ang = 0; set_ens(hd, "sh_stun"); hb->hidden = 1;
        for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) set_ens(b, "sb_fly8"); }
        snd_play_orig(47, 220, 128);
    }
    if (SN.fled && hb) {
        SN.hitG += 1;
        hb->kx = 8; hb->ky = 0;                                                        /* 5090 / 5091 */
        SN.hV = hb->x; SN.hW = hb->y;
        hd->x = hb->x; hd->y = hb->y;                                                  /* 5081 */
        for (int h = 1; h <= SN.nseg; h++) {                                           /* 5082 chain (rate INFERRED) */
            obj_t *b = seg(h);
            if (!b) continue;
            double nx = b->x + (SN.hV - b->x) * 0.5, ny = b->y + (SN.hW - b->y) * 0.5;
            SN.hV = b->x; SN.hW = b->y; b->x = (float)nx; b->y = (float)ny;
            set_ens(b, (hd->an.frame & 1) ? "sb_fly" : "sb_fly8");                       /* 5083 / 5084 */
        }
        if (t % 6 == 5) snd_play_orig(35, 200, 128);
        if (hb->x >= 8994 - (SN.level == 2 ? 0 : 99999) && !SN.flee_music) { SN.flee_music = 1; music_stop(); }   /* 5087 / 5088 */
        for (int i = 0; i < G.nobjs; i++) {                                            /* 5092: the arena wall breaks */
            obj_t *r = &G.o[i];
            if (r->alive && r->type == OT_BROCK2 && r->an.b && obj_rect_hit(r, (int)hb->x - 16, (int)hb->y - 16, (int)hb->x + 16, (int)hb->y + 16)) {
                r->alive = 0; obj_explode(r->x + 16, r->y + 16); snd_play_orig(13, 220, 128);
            }
        }
        if (hb->x >= SN.climR) {                                                       /* 5093 */
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->alive = 0; }
            hd->alive = 0; hb->alive = 0;
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_FIREBALL) G.o[i].alive = 0;
            SN.done = 1; SN.active = 0;
            camera_normal_bounds();
            music_play(level_music(G.lvdir));
        }
    }
}

void serp_update(player_t *p) {
    if (SN.level == 2) dv2_update(p);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_SN") && SN.active) { obj_t *hd = head(), *b7 = seg(7), *hb = hitbox();
        printf("sn f%d B%d A%g C%g D%g E%d F%g G%d H%.2f xy %d,%d b7 %d,%d hb %d,%d c%d fl%x\n", G.frame, SN.B, SN.A, SN.C, SN.D, SN.E, SN.F, SN.Gt, SN.H,
               hd ? (int)hd->x : 0, hd ? (int)hd->y : 0, b7 ? (int)b7->x : 0, b7 ? (int)b7->y : 0, hb ? (int)hb->x : 0, hb ? (int)hb->y : 0, hb ? hb->c : 0, hb ? hb->fl : 0); }
#endif
    else if (SN.level == 4) serp4_update(p);
}
int serp_active(void) { return SN.active && !SN.done; }

/* ================================================================ DV4 boss (spec §5) */
void serp4_update(player_t *p) { (void)p; }
