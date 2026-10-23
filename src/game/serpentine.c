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

/* VectorMovement (spec_dv4_boss_events 0.2): look_at(tx, ty), speed s: x += s cos(a) 0.01, y -= s sin(a) 0.01,
 * floor steps with the fraction carried (av[6], av[7]) */
static void vec_move(obj_t *b, double tx, double ty, int speed) {
    double a = atan2(b->y - ty, tx - b->x) * 57.29577951308232; if (a < 0) a += 360;
    double dx = speed * c_cos(a) * 0.01 + b->av[6], dy = -speed * c_sin(a) * 0.01 + b->av[7];
    double sx = floor(dx), sy = floor(dy);
    b->av[6] = dx - sx; b->av[7] = dy - sy; b->x += (float)sx; b->y += (float)sy;
}
/* loop_movesnake (5082 / 5325): each body H = 1.. aims at the previous link (head V, W) at int(dist * 20) */
static void chain_follow(obj_t *hd_unused, double V, double W) {
    (void)hd_unused;
    int sp[NSEG + 1]; double tx[NSEG + 1], ty[NSEG + 1];
    for (int h = 1; h <= SN.nseg; h++) {
        obj_t *b = seg(h); sp[h] = -1;
        if (!b) continue;
        double d = hypot(b->x - V, b->y - W);
        sp[h] = (int)(d * 20); tx[h] = (int)V; ty[h] = (int)W;
        V = b->x; W = b->y;
    }
    for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b && sp[h] >= 0) vec_move(b, tx[h], ty[h], sp[h]); }
}

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
            if (p->x <= SN.C) { if (SN.H > -1) SN.H -= 0.1; } else if (SN.H < 1) SN.H += 0.1;   /* 5061 / 5062 (no clamp: RE) */
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
        chain_follow(hd, SN.hV, SN.hW);                                              /* 5082 + loop_movesnake_21 */
        for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) set_ens(b, (hd->an.frame & 1) ? "sb_fly" : "sb_fly8"); }   /* 5083 / 5084 */
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

/* ================================================================ DV4 boss (RE: build/re/spec_dv4_boss_events.md) */
static struct {
    int setup, P1, P2, P3, hbr, stun, exit, K, hud;
    double A, B, C, D, E, F, G, H, J, Kr, L, M, R, V, W; int flag3, flag4, fire_latch;
    /* hitbox */ double hG, hI, hF;
    /* bounce ball (BossBounceMode_1089): BallMovement speed 40 */
    double bx, by, bfx, bfy, bA, bB; int bdir, bon, bstarted, bC, bwas;
    int flash, cine, armour_once, exit_x;
    double cx, cy;            /* cinematic camera */
} S4;

static void ball_step(void) {
    if (!S4.bstarted) return;
    double a = S4.bdir * 11.25 / 57.29577951308232, v = 40 * 0.125;
    double dx = (S4.bdir == 0 ? 1 : S4.bdir == 16 ? -1 : (S4.bdir == 8 || S4.bdir == 24) ? 0 : c_cos(a * 57.29577951308232)) * v + S4.bfx;
    double dy = (S4.bdir == 8 ? -1 : S4.bdir == 24 ? 1 : (S4.bdir == 0 || S4.bdir == 16) ? 0 : -c_sin(a * 57.29577951308232)) * v + S4.bfy;
    double sx = floor(dx), sy = floor(dy);
    S4.bfx = dx - sx; S4.bfy = dy - sy;
    double nx = S4.bx + sx, ny = S4.by + sy;
    int hit = solid_at(&G.lvl, (float)nx, (float)ny + 22) || solid_at(&G.lvl, (float)nx, (float)ny - 22) ||
              solid_at(&G.lvl, (float)nx + 22, (float)ny) || solid_at(&G.lvl, (float)nx - 22, (float)ny) ||
              nx < SN.climL + 22 || nx > SN.climR - 22 || ny < SN.climT + 22;
    if (hit && !S4.bwas) {           /* BallMovement::bounce (reflection; the 20 % randomizer INFERRED away) */
        int hx = solid_at(&G.lvl, (float)nx + 22, (float)S4.by) || solid_at(&G.lvl, (float)nx - 22, (float)S4.by) || nx < SN.climL + 22 || nx > SN.climR - 22;
        int hy = solid_at(&G.lvl, (float)S4.bx, (float)ny + 22) || solid_at(&G.lvl, (float)S4.bx, (float)ny - 22) || ny < SN.climT + 22;
        if (!hx && !hy) hx = hy = 1;
        if (hx) S4.bdir = (16 - S4.bdir + 32) % 32;
        if (hy) S4.bdir = (32 - S4.bdir) % 32;
        S4.bC++; camera_shake(4); snd_play_orig(51, 220, 128);
        S4.bwas = 1;
        return;
    }
    S4.bwas = hit;
    S4.bx = nx; S4.by = ny;
}
static int dir32(double dyup, double dx) { double d = atan2(dyup, dx) * 57.29577951308232; if (d < 0) d += 360; return (int)(d / 11.25); }

static void piece_at(float x, float y, double vx, double vy) {
    obj_t *d = obj_alloc(); int a = ens_anim("rockpiece");
    if (!d || a < 0) return;
    d->type = OT_CHUNK; d->alive = 1; d->x = x; d->y = y; d->fx = x; d->fy = y;
    d->av[0] = vx; d->av[1] = vy; d->av[2] = 0.3; anim_set(&d->an, &G.ens, a); d->ang = (float)(cross_rand(4) * 90);
}

static void s4_spawn_snake(void) {                                 /* 5463 / 5464 */
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && (o->type == OT_SNAKE_HEAD || o->type == OT_SNAKE_BODY)) o->alive = 0; }
    obj_t *hd = obj_alloc(); if (!hd) return;
    hd->type = OT_SNAKE_HEAD; hd->alive = 1; hd->x = 8968; hd->y = 1797; hd->layer = 2; set_ens(hd, "sh_rest");
    S4.A = -30; S4.H = -1; S4.J = 12; S4.Kr = -1;
    obj_t *bs[8];
    for (int k = 0; k < 8; k++) { obj_t *b = obj_alloc(); bs[k] = b; if (!b) return; b->type = OT_SNAKE_BODY; b->alive = 1; b->x = 8922; b->y = 1790; b->layer = 2; set_ens(b, "sb_idle"); }
    for (int k = 0; k < 8; k++) {                                   /* last created first: H = 1 */
        obj_t *b = bs[7 - k]; b->av[0] = k + 1; b->scale = (float)fmin(1.0, 1.2 - 0.05 * (k + 1)); b->fl |= F_NOFLY; b->A = 4; b->bd = 0;
    }
    SN.nseg = 8;
}

static void s4_hit_cycle_hitbox(obj_t *hd, obj_t *hb) {             /* 5524 */
    hb->x = hd->x; hb->y = hd->y; hb->ang = hd->ang;
    const char *nm = hd->an.b ? hd->an.b->anims[hd->an.anim].name : "sh_rest";
    char hn[16]; snprintf(hn, sizeof hn, "sx%s", nm + 2); set_ens(hb, hn); force_frame(hb, hd->an.frame);
}

void serp4_update(player_t *p) {
    obj_t *hd = head(), *hb = hitbox();
    int t = gcount();
    int plx = (int)floor(p->x), ply = (int)floor(p->y - ORIG_REF_DY);
#ifdef FP_HOST_ENV
    if (getenv("FP_SN_KILL") && G.frame == atoi(getenv("FP_SN_KILL"))) for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->A = 0; }
    if (getenv("FP_SN_KILLH") && hb && SN.active && !S4.stun && hb->c <= 0 && atoi(getenv("FP_SN_KILLH")) <= G.frame) { hb->A = 0; }
    if (getenv("FP_SN_AUTOKILL") && hb && (S4.P2 || S4.P3) && G.frame % 97 == 0) {     /* test hook: segments then the head */
        int alive = 0; for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b && !(b->fl & F_DEFEATED)) { b->A = 0; alive = 1; } }
        if (!alive && hb->c <= 0) { hb->A = 0; hb->kx = 3; hb->ky = -4; }
    }
#endif
    /* 5461 trigger: the character sensor overlaps a cameratriggerboss (x 12608, 16 px wide) */
    if (!SN.active && !SN.done && plx + 8 > SN.trigX && ply > SN.climT - 300 && ply < SN.climB + 64) {
        SN.active = 1; S4.P1 = 1; S4.hbr = 1; S4.K = 3;
        p->layer = 1; chase_end(); camera_boss_bounds(SN.climL, SN.climT, SN.climR, SN.climB);
    }
    if (!SN.active) return;
    if (!S4.setup) { S4.setup = 1; s4_spawn_snake(); hd = head(); if (hb) { set_ens(hb, "sx_rest"); hb->hidden = 1; hb->A = 1; hb->fl &= ~F_ARMOURED; } }
    if (!hd) return;
    /* 5467-5475 thrown boulders (OT_BBOULDER: E angle, H y, D fall speed, F bounces) */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_BBOULDER) continue;
        if (b->fl & F_DEFEATED) {                                                     /* 5474 / 5475 */
            if (b->scale > 0.85f) { obj_t *pt = obj_alloc(); if (pt) { pt->type = OT_ITEM_FLY; pt->st = OT_PETAL; pt->alive = 1; pt->x = b->x; pt->y = b->y; pt->vx = 0; pt->vy = -3; anim_set(&pt->an, &G.objs, bank_anim(&G.objs, "objectpetal_355")); } }
            static const int ox[4] = { -10, -10, 10, 10 }, oy[4] = { -10, 10, 10, -10 };
            for (int k = 0; k < 4; k++) { double vx = b->kx - 2 + cross_rand(4), vy = b->ky - 4 + cross_rand(4); piece_at(b->x + ox[k], b->y + oy[k], vx, vy); }
            snd_play_orig(13, 220, 128); b->alive = 0; continue;
        }
        b->ang = (float)fmod(b->av[0], 360.0); b->av[0] += 2;                       /* 5467 */
        b->y = (float)(int)b->av[1]; b->av[1] += b->av[2]; b->av[2] = fmin(b->av[2] + 0.2, 8);
        if (b->y > SN.climT + 100) b->bd = 5;                                        /* 5468 */
        if (b->av[2] > 7.5) b->av[3] = 1;                                            /* 5470 flag 0 */
        if (b->av[3] && b->av[4] < 3 && obj_overlaps_bg(b)) { b->av[3] = 0; b->av[2] = -3; b->av[4] += 1; camera_shake(3); snd_play_orig(51, 220, 128); }   /* 5471 */
        if (b->y > SN.climB + 40) b->alive = 0;                                      /* 5472 */
    }
    /* P1 boulder rain (5476-5482) */
    if (S4.P1) {
        S4.A += 1; camera_shake(2);                                                  /* 5476 */
        if (S4.A >= 20 && S4.B < 12) {                                               /* 5477 */
            S4.A = 0; S4.B += 1;
            obj_t *b = obj_alloc();
            if (b) {
                b->type = OT_BBOULDER; b->alive = 1; b->layer = 2; set_ens(b, "bboulder"); b->A = 6; b->bd = 0;
                b->x = (float)(int)(280 * c_sin(30 * S4.B) + 12784 - 40 + cross_rand(81)); b->y = 1810;
                b->ang = (float)cross_rand(360); b->av[1] = 1810; b->av[0] = 0; b->av[2] = 0;
                b->scale = (float)(1 - cross_rand(30) * 0.01);
                obj_t *w = obj_alloc(); int wa = ens_anim("warning");
                if (w && wa >= 0) { w->type = OT_EXPLODE; w->alive = 1; w->x = b->x; w->y = SN.gy[0] + 24; w->layer = 3; anim_set(&w->an, &G.ens, wa); }
            }
        }
        if (S4.B == 12 && S4.A > 65) S4.hud = 1;                                     /* 5478 (GUIB.A condition U-G1 dropped) */
        if (S4.B == 12 && S4.A > 150) {
            if (S4.K > 1) {                                                          /* 5479 / 5480: odd segments revive */
                if (hb) hb->hidden = 1;
                for (int h = 1; h <= SN.nseg; h += 2) { obj_t *b = seg(h); if (b) { b->fl &= ~F_DEFEATED; b->A = 4; set_ens(b, "sb_idle"); SN.segH[h - 1] = 0; SN.segE[h - 1] = 0; } }
            }
            S4.A = 0; S4.B = 0; S4.P1 = 0;
            if (S4.K == 3) { S4.P2 = 1; S4.E = 1; S4.G = 0; }                        /* 5481 */
            else { S4.P3 = 1; S4.E = 0; S4.G = 1; }                                  /* 5482 */
        }
    }
    /* P2 geyser arcs (5483-5500): DV2 rules with A += 1.2, 8-degree body lag, landing at A >= 276, fixed G toggle */
    if (S4.P2) {
        if (S4.B == 0) { int g = geyser_by_A((int)S4.E); if (g >= 0) { S4.A = 0; S4.C = SN.gx[g]; S4.D = SN.gy[g] + 40; S4.B = 1; } }
        if (S4.B == 1) { int g = geyser_by_A((int)S4.G); if (g >= 0) { S4.F = S4.C - SN.gx[g]; S4.B = 2; } }
        SN.C = S4.C; SN.D = S4.D; SN.F = S4.F;
        if (S4.B == 2) {                                                             /* 5485 */
            float x, y;
            set_ens(hd, S4.F >= 1 ? "sh_arc_l" : "sh_arc");
            arc_pos(S4.A, &x, &y); hd->x = x; hd->y = y;
            double cf = S4.F < -1 ? -1 : S4.F > 1 ? 1 : S4.F, ang = cf * S4.A; ang = ang < -180 ? -180 : ang > 180 ? 180 : ang;
            hd->ang = (float)fmod(ang + 360.0, 360.0);
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) arc_pos(fmax(0.0, S4.A - 8.0 * h), &b->x, &b->y); }
            if (t % 10 == 5) snd_play_orig(35, 200, 128);                            /* 5486 */
            S4.A += 1.2;
            int g = geyser_by_A((int)S4.G);
            if (S4.A >= 276 && g >= 0) {                                             /* 5487 */
                S4.B = 3; S4.C = hd->x; S4.A = 0; S4.D = SN.gy[g] + 40; S4.F = 0; S4.E = SN.gA[g];
                set_ens(hd, "sh_rest"); hd->ang = 0;
                if (hb) hb->av[1] = 0;
            }
        }
        if (S4.B == 3) {
            if (plx <= S4.C) { if (S4.H > -1) S4.H -= 0.1; } else if (S4.H < 1) S4.H += 0.1;   /* 5488 / 5489 */
            hd->x = (float)(int)(S4.C + 20 * S4.H - 20 * S4.H * c_cos(S4.A) + 8 * c_sin(4.0 * t));   /* 5490 */
            hd->y = (float)(int)(S4.D - S4.A + 8 * c_cos(8.0 * t));
            for (int i = 1; i <= SN.nseg; i++) {
                obj_t *b = seg(i);
                if (!b) continue;
                double g = fmax(0.0, 24 + S4.A - 24.0 * i);
                b->x = (float)(int)(S4.C - 20 * S4.H + 20 * S4.H * c_cos(g) + 8 * c_sin(4.0 * t + 15 * i));
                b->y = (float)(int)(S4.D - g + 8 * c_cos(8.0 * t + 15 * i));
            }
            force_frame(hd, (int)(1.5 + S4.H));
            if (S4.A >= 80) S4.F += 1;                                                 /* 5491 */
            if (S4.F < 80) S4.A = fmin(S4.A + 4, 80);                                  /* 5492 */
            if (S4.F >= 80) S4.A = fmax(S4.A - 4, 0);                                  /* 5493 */
            if (S4.F == 5) { set_ens(hd, "sh_fire"); force_frame(hd, (int)(1.5 + S4.H)); S4.fire_latch = 0; }   /* 5494 */
            if (S4.F == 40) { set_ens(hd, "sh_rest"); force_frame(hd, (int)(1.5 + S4.H)); }                    /* 5495 */
            if (hd->an.b && hd->an.anim == ens_anim("sh_fire") && hd->an.frame != 1 && !S4.fire_latch) {      /* 5496 */
                fireball_spawn(hd->x, hd->y, hd->an.frame * 8 + 16 >= 32 ? 0 : hd->an.frame * 8 + 16);
                snd_play_orig(46, 220, 128); S4.fire_latch = 1; S4.F = 6;
            }
            if (S4.F >= 60 && S4.A <= 0) {                                             /* 5498-5500 */
                S4.G = fmod(S4.G + 1, 2);
                if (S4.G == 1) { S4.P2 = 0; S4.P3 = 1; S4.B = 0; }
                else if (S4.G != S4.E) S4.B = 0;
            }
        }
    }
    /* P3 wheel and bounce (5501-5523) */
    if (S4.P3) {
        ball_step();
        if (S4.B == 0) {                                                               /* 5501 */
            int g = geyser_by_A(0);
            if (g >= 0) {
                S4.A = 0; S4.C = SN.gx[g]; S4.D = SN.gy[g] + 40; S4.B = 1; S4.bon = 0; S4.bC = 0;
                for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) { b->av[1] = 0; b->av[4] = 0; } }
                S4.M = 0; S4.L = 0;
            }
        }
        if (S4.B == 1) {                                                               /* 5502 */
            int g = geyser_by_A(1);
            if (g >= 0) {
                S4.F = S4.C - SN.gx[g]; S4.Kr = S4.F * 0.5; S4.A = 90; S4.J = 9; S4.C = (SN.gx[g] + S4.C) * 0.5; S4.B = 2;
                for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) { b->av[1] = S4.Kr; b->av[2] = S4.D; b->av[3] = fmax(0.0, S4.A - h * S4.J); } }
            }
        }
        if (S4.B == 2 && S4.L >= 12 && S4.bC >= 5) S4.bdir = dir32(S4.by - S4.bB, S4.bA - S4.bx);   /* 5503 (steer home) */
        if (S4.B >= 2) {                                                               /* 5504 */
            set_ens(hd, S4.F >= 1 ? "sh_arc_l" : "sh_arc");
            hd->x = (float)(int)(S4.C + S4.Kr * c_sin(S4.A)); hd->y = (float)(int)(S4.D - S4.Kr * c_cos(S4.A));
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) { b->x = (float)(int)(S4.C + b->av[1] * c_sin(b->av[3])); b->y = (float)(int)(b->av[2] - b->av[1] * c_cos(b->av[3])); } }
            double cf = S4.F < -1 ? -1 : S4.F > 1 ? 1 : S4.F;
            hd->ang = (float)fmod((S4.A - 90) * cf + 720.0, 360.0);
            S4.A += 1.5 + S4.L;
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->av[3] += 1.5 + b->av[4]; }
            S4.flag3 = 1;
        }
        if (t % 10 == 5 && ((S4.B == 2 && S4.L < 12) || (S4.B == 3 && S4.L == 0))) snd_play_orig(35, 200, 128);   /* 5505 */
        if (S4.B == 2 && S4.A > 180 && S4.Kr < -44) { S4.L = 6; S4.D = S4.D + S4.Kr + 44; S4.Kr = -44; }           /* 5506 */
        if (S4.B == 2) {
            int any = 0;
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (!b) continue;
                if (b->av[3] > 180 && b->av[1] < -44) { b->av[4] = 6; b->av[2] = b->av[2] + b->av[1] + 44; b->av[1] = -44; }   /* 5507 */
                if (b->av[1] < -44) any = 1; }
            if (any) S4.flag3 = 0;                                                     /* 5508 */
        }
        if (S4.B == 2 && S4.flag3 && S4.L < 12) {                                      /* 5509 */
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->av[4] = fmin(b->av[4] + 0.2, 12); }
            S4.L = fmin(S4.L + 0.2, 12);
            S4.bx = (int)S4.C; S4.by = (int)S4.D; S4.bA = S4.C; S4.bB = S4.D;
        }
        if (S4.B == 2 && S4.L >= 12 && !S4.bon) {                                       /* 5510 */
            S4.bstarted = 1; S4.bon = 1; S4.bfx = S4.bfy = 0; S4.bdir = dir32(S4.by - ply, plx - S4.bx); snd_play_orig(7, 220, 128);
        }
        if (S4.B == 2 && S4.L >= 12) { S4.C = S4.bx; S4.D = S4.by; for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->av[2] = S4.by; } }   /* 5511 */
        if (S4.bdir == 8) S4.bdir = cross_rand(2) ? 10 : 6;                           /* 5512 */
        if (S4.L >= 12 && p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p) && !player_body_z(p) && p->state != ST_HURT) {   /* 5513 */
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b && obj_rect_hit(b, plx - 7, ply - 34, plx + 8, ply + 12)) { player_contact(p, 10); break; } }
        }
        if (S4.B == 2 && S4.L >= 12 && S4.bC >= 5 && fabs(S4.bx - S4.bA) < 3 && fabs(S4.by - S4.bB) < 3) {   /* 5514 */
            S4.B = 3; S4.C = S4.bA; S4.D = S4.bB; S4.bstarted = 0;
        }
        if (S4.B == 3 && S4.L > 6) { S4.L = fmax(S4.L - 0.2, 6); for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->av[4] = fmax(b->av[4] - 0.2, 6); } }   /* 5515 */
        int g0 = geyser_by_A(0);
        if (S4.B == 3 && S4.L > 0 && S4.L <= 6 && S4.A >= 180 && S4.A <= 188) { S4.L = 0; S4.Kr = S4.F * 0.5; S4.D = SN.gy[g0 >= 0 ? g0 : 0] + 40; S4.M += 1; }   /* 5516 */
        if (S4.B == 3 && S4.L <= 6) {                                                  /* 5517 */
            obj_t *b = seg((int)S4.M);
            if (b && b->av[4] > 0 && b->av[3] >= 180 && b->av[3] <= 188) { b->av[4] = 0; b->av[1] = S4.F * 0.5; b->av[2] = SN.gy[g0 >= 0 ? g0 : 0] + 40; S4.M += 1; }
        }
        if (S4.B == 3 && S4.L == 0 && S4.A >= 270) S4.A = 270;                         /* 5518 */
        if (S4.B == 3 && S4.L <= 6) for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b && b->av[4] == 0 && b->av[3] >= 270) b->av[3] = 270; }   /* 5519 */
        if (S4.B == 3 && S4.L <= 6 && S4.A >= 270) {                                   /* 5520 / 5521 */
            S4.flag4 = 1;
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b && b->av[3] < 270) S4.flag4 = 0; }
        }
        if (S4.B == 3 && S4.flag4) {                                                   /* 5522: overlaps GEY 1 -> P2 rest */
            for (int k = 0; k < SN.ngey; k++)
                if (hd->x >= SN.gx[k] - 40 && hd->x < SN.gx[k] + 40 && hd->y >= SN.gy[k] && hd->y < SN.gy[k] + 96) {
                    S4.P3 = 0; S4.P2 = 1; S4.C = hd->x; S4.D = SN.gy[k] + 40; S4.A = 0; set_ens(hd, "sh_rest"); hd->ang = 0; S4.F = 0; S4.flag4 = 0; S4.E = SN.gA[k]; S4.B = 3;
                    break;
                }
        }
        S4.A = fmod(S4.A, 360); for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->av[3] = fmod(b->av[3], 360); }   /* 5523 */
    }
    /* hitbox rules (0x2227c, 5524-5530) */
    if (S4.hbr && hb) {
        s4_hit_cycle_hitbox(hd, hb);
        for (int h = 1; h <= SN.nseg; h++) {                                           /* 5525-5527 */
            obj_t *b = seg(h);
            if (!b || !(b->fl & F_DEFEATED)) continue;
            if (!SN.segH[h - 1]) { SN.segH[h - 1] = 1; SN.segF[h - 1] = 60; SN.segE[h - 1] = 0; set_ens(b, "sb_skull"); obj_effect(b->x, b->y, "explosion06"); snd_play_orig(45, 220, 128); }
            if (SN.segF[h - 1] > 0) { SN.segF[h - 1]--; SN.segE[h - 1] += fmin(SN.segF[h - 1] * 0.01, 1.0); }
            if (SN.segE[h - 1] > 4) SN.segE[h - 1] -= 4;
            force_frame(b, (int)SN.segE[h - 1]);
        }
        int alive = 0;
        for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b && !(b->fl & F_DEFEATED)) alive = 1; }
        if (alive) { hb->c = 3; hb->fl &= ~F_DEFEATED; }                               /* 5528 */
        if (!S4.P1) hb->hidden = 1;
        if ((hb->fl & F_DEFEATED) && hb->A <= 0) {                                     /* 5530 */
            hd->ang = 0; set_ens(hd, "sh_stun");
            for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) { set_ens(b, "sb_fly8"); b->av[6] = b->av[7] = 0; } }
            snd_play_orig(47, 220, 128);
            S4.P2 = S4.P3 = 0; S4.hbr = 0; S4.stun = 1; S4.K -= 1; S4.bstarted = 0; S4.hG = 0; S4.hI = 0; S4.hF = 0;
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_FIREBALL && S4.K == 0) G.o[i].alive = 0;
        }
    }
    /* stunned / retreat / final defeat (0x22349, 5531-5566) */
    if (S4.stun && hb) {
        S4.V = hd->x; S4.W = hd->y; hd->x = hb->x; hd->y = hb->y; S4.hG += 1;          /* 5531 */
        if (!(S4.hI == 1 && hb->ky < -8)) chain_follow(hd, S4.V, S4.W);                /* 5532 (bodies freeze in the shaft flight) */
        if (hd->an.anim == ens_anim("sh_stun")) for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) set_ens(b, hd->an.frame == 0 ? "sb_fly8" : "sb_fly"); }   /* 5533 / 5534 */
        if (S4.hG <= 60) {
            hb->x = fminf(fmaxf(hb->x, SN.climL), SN.climR);                          /* 5535 */
            if (t % 2 == 0) force_frame(hd, abs(hd->an.frame - 1));                    /* 5536 */
            if (obj_overlaps_bg(hb) && hb->ky >= 0) { hb->kx = 0; hb->ky = 0; }        /* 5537 */
        }
        if (S4.K > 0) {                                                                /* retreat 5538-5541 */
            if (S4.hG > 60 && obj_overlaps_bg(hb) && hd->an.anim != ens_anim("sh_arc")) { set_ens(hd, "sh_arc"); for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) set_ens(b, "sb_fly8"); } }
            if (S4.hG > 60 && hd->an.anim == ens_anim("sh_arc")) { if (t % 6 == 5) snd_play_orig(35, 200, 128); hb->kx = 0; hb->ky = -8; }
            if (S4.hG > 60 && hb->y < SN.climT - 100) {                                /* 5541 */
                S4.stun = 0; S4.hbr = 1; S4.P1 = 1; S4.hG = 0; S4.B = 0; S4.E = 0; S4.A = 0; S4.F = -340; S4.Kr = -1;
                hb->hidden = 1; hb->fl &= ~F_DEFEATED; hb->A = 1; hb->kx = hb->ky = 0; hb->ang = 0;
                for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) { b->y = hd->y; set_ens(b, "sb_idle"); } }
            }
        } else {                                                                       /* final defeat 5542-5566 */
            if (!S4.armour_once) { S4.armour_once = 1; music_stop(); }               /* 5542 */
            if (S4.hG < 60 && S4.hI == 0) {                                            /* 5543-5545 explosions */
                int k = cross_rand(8);
                obj_t *src = k == 0 ? hd : seg(k);
                if (src) obj_effect(src->x - 20 + cross_rand(41), src->y - 20 + cross_rand(41), "explosion06");
                static const int ex[3] = { 311, 319, 320 }; snd_play_orig(ex[cross_rand(3)], 220, 128);
            }
            if (S4.hI == 0 && S4.hG > 40 && S4.hG < 60) {                              /* 5547 */
                snd_play_orig(76, 220, 128); S4.hG = 61; S4.cine = 1; S4.hI = 1; S4.flash = 255;
                for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) set_ens(b, "sb_skull"); }
                set_ens(hd, "sh_die"); camera_shake(10);
                for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_BTUNNEL) o->x = hd->x; if (o->alive && o->type == OT_FIREBALL) o->alive = 0; }
                S4.cx = G.camx + 213; S4.cy = G.camy + 120;
            }
            if (S4.hG > 60 && S4.flash > 0) S4.flash = S4.flash >= 255 ? 252 : (S4.flash >> 1) >= 2 ? 2 * (S4.flash >> 1) - 4 : 0;   /* 5548 */
            if (S4.hI >= 1 && S4.hI <= 2) { hd->ang = (float)fmod(hd->ang - 12 + 360, 360); }   /* 5551 */
            if (S4.hI == 1) {                                                          /* 5552 / 5554-5557 */
                hb->kx = 0; hb->ky = -12;
                if (t % 6 == 1 && obj_overlaps_bg(hb)) { snd_play_orig(13, 220, 128); camera_shake(3); }
                for (int i = 0; i < G.nobjs; i++) {
                    obj_t *o = &G.o[i];
                    if (!o->alive || o->type != OT_BTUNNEL || !o->hidden || o->y <= hb->y) continue;
                    o->hidden = 0;
                    static const int px6[6] = { -20, 0, 20, -20, 0, 20 }, py6[6] = { -8, -8, -8, 8, 8, 8 };
                    for (int k = 0; k < 6; k++) piece_at(o->x + px6[k], o->y + py6[k], cross_rand(19) - 9, cross_rand(5) - 9);
                }
                if (hb->y <= 1249) S4.hI = 2;
            }
            if (S4.hI == 2) {                                                          /* 5559-5563 */
                hb->kx = fmax(hb->kx - 0.07, 0);
                if (obj_overlaps_bg(hb) && hb->ky > 0) { hb->ky = -0.5 * hb->ky; S4.hF += 1; snd_play_orig(51, 220, 128); }
                if (S4.hF >= 1) { hb->ky = 0; S4.hI = 3; }
            }
            if (S4.hI == 3) {                                                          /* 5564 */
                snd_play_orig(38, 220, 128); S4.hI = 4; obj_effect(hd->x, hd->y, "explosion");
                for (int h = 1; h <= 7; h++) { obj_t *b = seg(h); if (!b) continue; b->x = hd->x; b->y = hd->y;
                    for (int k = 0; k < 4; k++) { obj_t *d = obj_alloc(); int a = ens_anim("debris"); if (!d || a < 0) break;
                        double sp = (70 + cross_rand(30)) * 0.125, dir = (1 + cross_rand(15)) * 11.25;
                        d->type = OT_CHUNK; d->alive = 1; d->x = d->fx = hd->x; d->y = d->fy = hd->y; anim_set(&d->an, &G.ens, a); d->an.frame = 7 + k;
                        d->av[0] = sp * c_cos(dir); d->av[1] = -sp * c_sin(dir); d->av[2] = 0.3; } }
            }
            if (S4.hI == 4) {                                                          /* 5565 / 5566 */
                if (S4.R < 45) obj_effect((float)(int)((40 * c_sin(2 * S4.R) + 8) * c_sin(132 * S4.R) + hd->x), (float)(int)(hd->y - 80 * c_sin(5 * S4.R)), "explosion06");
                hb->ky = 0; hb->kx = 0; S4.R += 1; hd->hidden = 1;
                for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->hidden = 1; }
                if (S4.R > 70) {                                                       /* 5567: scripted exit */
                    for (int h = 1; h <= SN.nseg; h++) { obj_t *b = seg(h); if (b) b->alive = 0; }
                    S4.stun = 0; S4.exit = 1; S4.B = 0; hb->alive = 0;
                    float tx = hd->x; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_BTUNNEL) { tx = G.o[i].x; break; }
                    p->x = tx; p->y = G.camy + 290 + ORIG_REF_DY; p->state = ST_NORMAL; p->vx = p->vy = 0; p->gsp = 0; p->ground = 0;
                    p->scripted = 1; snd_play_orig(53, 220, 128);
                }
            }
        }
    }
    /* cinematic camera (5590): eases 10 % per frame toward the head */
    if (S4.cine) {
        if (S4.hI >= 1 && S4.hI <= 2) { G.cam.A = hd->x; G.cam.B = hd->y; }
        S4.cx = (int)((G.cam.A - S4.cx) * 0.1 + S4.cx); S4.cy = (int)((G.cam.B - S4.cy) * 0.1 + S4.cy);
        G.cam.cine = 1; G.cam.cineX = (float)S4.cx; G.cam.cineY = (float)S4.cy;
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_S4")) printf("s4 f%d P%d%d%d st%d ex%d K%d A%.1f B%g L%.1f hd %d,%d hb %d,%d c%d fl%x hG%g hI%g bm %d,%d C%d p %d,%d\n", G.frame, S4.P1, S4.P2, S4.P3, S4.stun, S4.exit, S4.K, S4.A, S4.B, S4.L,
        (int)hd->x, (int)hd->y, hb ? (int)hb->x : 0, hb ? (int)hb->y : 0, hb ? hb->c : 0, hb ? hb->fl : 0, S4.hG, S4.hI, (int)S4.bx, (int)S4.by, S4.bC, plx, ply);
#endif
    /* scripted exit (0x2234a, 5569-5578) */
    if (S4.exit) {
        if (S4.B == 0) {                                                               /* 5569 / 5570 */
            p->y -= 10; p->vx = p->vy = 0;
            if (p->y - ORIG_REF_DY < 1230) { S4.B = 1; p->scripted = 0; p->ground = 0; p->vy = -4; p->vx = 0; p->face = 1; }
        } else {
            if (S4.B >= 1 && S4.B < 40 && p->ground) S4.B += 1;                        /* 5573 */
            if (S4.B == 40) { results_begin(0); S4.B = 41; }                           /* 5576 */
            if (S4.B >= 41 && !S4.exit_x && p->x > G.camx + 458) { S4.exit_x = 1; p->x = G.lvl.w - 50; }   /* 5578 */
        }
    }
}
/* forced input of the scripted exit: Down for grounded counts 2..24, Right from 41 (5573-5577) */
int serp4_input(uint32_t *held) {
    if (SN.level != 4 || !((S4.stun && S4.K == 0 && S4.hI >= 1) || S4.exit)) return 0;   /* CTRL.M = 1 from 5547 */
    *held = 0;
    if (S4.exit && S4.B >= 1 && S4.B < 25 && G.p.ground) *held = BTN_DOWN;
    if (S4.exit && S4.B >= 41) *held = BTN_RIGHT;
    return 1;
}
int serp4_flash(void) { return SN.level == 4 ? S4.flash : 0; }
int serp4_hud(int *k) { *k = S4.K; return SN.level == 4 && S4.hud && SN.active && !S4.exit; }
int serp_progress(void) { return (SN.done ? 200 : 0) + (SN.level == 4 ? (3 - S4.K) * 100 + (S4.exit ? 100 : 0) : 0); }

void serp_draw_hud(void) {
    int k;
    if (serp4_hud(&k)) {                                     /* guiboss (264,208) + guibosshealth counter (277,222), layer 6 */
        int a = ens_anim("guiboss");
        if (a >= 0) { const anim_t *an = &G.ens.anims[a]; frame_draw(&G.ens, &G.ens.frames[G.ens.seq[an->first]], 264, 208, 0, 0xffffffff); }
        int d = bank_anim(&G.objs, "hud_digits");
        if (d >= 0 && k >= 0 && k <= 9) { const anim_t *an = &G.objs.anims[d]; frame_draw(&G.objs, &G.objs.frames[G.objs.seq[an->first + k]], 277, 222, 0, 0xffffffff); }
    }
    int f = serp4_flash();
    if (f > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)f << 24) | 0xffffff);   /* 5547 white levelcardfade */
}
