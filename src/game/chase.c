/* DV4 boulder chase (RE + RV: build/re/spec_dv4_chase_events.md, events 5397-5460; trace dv4_chase.csv).
 * BossGiantBoulder_1091 rolls behind Lilac from (784,1952) along stage thresholds, breaks the tunnel walls
 * (BossTunnel2, solid until broken), BreakableRock2 and Breakable1-3, kills enemies, hurts Lilac (5) and crushes her
 * against walls; it stops at x 12320 left of the Serpentine arena. The camera follows the midpoint of boulder and
 * Lilac (CAM.M = 3, 5402-5409). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../fp.h"

#define F_DEFEATED 0x40
static struct {
    int on, started, flag3, flag4, voice;
    double A, B, C, D, F, G, H, O, P; int E;
    int ended;
} CH;

static obj_t *boulder(void) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_GBOULDER) return &G.o[i]; return NULL; }
static void set_ens(obj_t *o, const char *name) { int a = ens_anim(name); if (a >= 0) anim_set(&o->an, &G.ens, a); }

void chase_spawn(void) {
    memset(&CH, 0, sizeof CH);
    snap_register(&CH, sizeof CH);
    obj_t *b = boulder();
    if (!b) return;
    CH.on = 1;
    set_ens(b, "gboulder");
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_BTUNNEL3) o->alive = 0;                    /* 5398: GV[1] < 2 (Lilac) removes the Milla exit */
        if (o->type == OT_BTUNNEL2) { set_ens(o, "btunnel2"); o->hidden = 1; }   /* invisible solid wall until broken */
        if (o->type == OT_BTUNNEL) { set_ens(o, "btunnel"); o->hidden = 1; }
        if (o->type == OT_BRK1) set_ens(o, "brk1");
        if (o->type == OT_BRK2) set_ens(o, "brk2");
        if (o->type == OT_BRK3) set_ens(o, "brk3");
    }
}

static int test_obstacle(int x, int y) { return solid_at(&G.lvl, (float)x, (float)y); }

/* debris / rock piece: A x speed, B y speed, C gravity (5441: D += A; B += C; E += B), removed below the view (5442) */
static void piece(float x, float y, const char *anim) {
    obj_t *d = obj_alloc();
    if (!d) return;
    d->type = OT_CHUNK; d->alive = 1; d->x = x; d->y = y; d->fx = x; d->fy = y;
    d->av[0] = 8 + cross_rand(4); d->av[1] = cross_rand(4) - 4; d->av[2] = 0.3;
    set_ens(d, anim);
    d->ang = (float)(cross_rand(4) * 90);
}
void chase_chunk_update(obj_t *d) {
    d->fx += (float)d->av[0]; d->av[1] += d->av[2]; d->fy += (float)d->av[1];
    d->x = (float)(int)d->fx; d->y = (float)(int)d->fy;
    if (d->y > G.camy + 320) d->alive = 0;
}

static void player_kill(player_t *p) {          /* 5450 / 5452: R = 1, petals = 0, LIFE.C = 1 (death path) */
    if (p->state == ST_DEAD) return;
    p->hp = 0; p->state = ST_DEAD; p->dead_t = 0; p->vy = -5; p->vx = 0; p->ground = 0;
}

void chase_update(player_t *p) {
    if (!CH.on) return;
    obj_t *b = boulder();
    if (!b) return;
    double mvB = p->ground ? p->gsp : p->vx;
    int plx = (int)floor(p->x), ply = (int)floor(p->y - ORIG_REF_DY);
    /* 5401 start: Lilac passes the right edge of the boulder's box (x + 64, INFERRED within RV [52.3, 64.6)) */
    if (!CH.started && p->x > b->x + 64) {
        CH.started = 1; CH.A = b->x; CH.B = b->y; CH.D = 8; CH.C = fmin(10, mvB);
        music_play("bgm/m_boss.adp");                                  /* RV: stop_music, play m_boss (loop) */
        G.cam.M3 = 1;
    }
    if (!CH.started || CH.ended) return;
    p->layer = 0;                                                     /* 5401 / 5402: MV.P = 0 */
    int viewL = (int)G.camx, viewT = (int)G.camy;
    /* 5402-5409 camera */
    CH.P = fmin(ply + 100, (b->y + ply) * 0.5);
    CH.O = fmin(fmax((b->x + plx) * 0.5, plx - 120.0), plx + 120.0);
    G.cam.minC = 640;
    if (viewL > 10599) G.cam.minC = 10600;
    if (!CH.flag4) { G.cam.B = (float)CH.P; G.cam.A = (float)fmax(G.cam.minC + 213, CH.O); }
    else {
        if (G.cam.A > CH.O) G.cam.A = (float)fmax(G.cam.minC + 213, G.cam.A - 8);
        if (G.cam.A < CH.O) G.cam.A += 8;
        if (CH.P < G.cam.B) G.cam.B = (float)fmax(CH.P, G.cam.B - 3);
        if (G.cam.B < CH.P) G.cam.B = (float)fmin(CH.P, G.cam.B + 3);
        if (b->x < viewL + 426 && G.cam.A <= CH.O && G.cam.B == (float)CH.P) CH.flag4 = 0;
    }
    /* 5410-5427 motion */
    if (CH.G <= 0) { b->x = (float)(int)CH.A; b->y = (float)(int)CH.B; CH.A += CH.C; CH.B += CH.D; }
    if (CH.G > 0) CH.G -= 1;
    if (CH.E == 0) b->ang = (float)(b->ang - CH.C);
    if (CH.E == 0 && CH.B > 2100 && !CH.voice) { CH.voice = 1; snd_play_orig(95, 220, 128); }
    if (CH.E == 0 && CH.B > 2100 && test_obstacle((int)b->x, (int)b->y + 64)) { snd_play_orig(51, 220, 128); camera_shake(10); CH.D = 0; CH.E++; }
    if (CH.E > 0) { b->ang = (float)(b->ang - CH.C); CH.F += (0.1 * CH.C <= 1) ? 2 + 0.1 * CH.C : 3; }
    b->ang = fmodf(b->ang, 360.f); if (b->ang < 0) b->ang += 360.f;
    if (CH.E > 0 && CH.E < 11 && CH.F >= 20 && p->invuln < 90) { snd_play_orig(78, 200, 128); CH.F = 0; }
    static const double TH[8] = { 0, 2592, 4320, 5472, 5920, 7072, 8480, 9888 };
    if (CH.E >= 1 && CH.E <= 7 && CH.A >= TH[CH.E]) CH.E++;
    if ((CH.E > 0 && CH.E < 11 && CH.E != 8 && plx > b->x) || CH.E == 9) CH.C = fmin(CH.C + 0.05, fmax(8, mvB - 1));
    if (CH.E == 1 || CH.E == 3 || CH.E == 5 || CH.E == 10) CH.D = 0.0625 * CH.C;
    if (CH.E == 2 || CH.E == 4 || CH.E == 6) CH.D = 0;
    if (CH.E == 7) CH.D = -0.0625 * CH.C;
    if (CH.E == 8) { CH.F = 0; CH.C = fmax(0, 8 - CH.H); CH.D = -1.5 * CH.H; CH.H += 0.2; }
    if (CH.E == 8 && CH.H >= 8) CH.E = 9;
    if (CH.E == 9) { CH.F = 0; CH.D = fmin(CH.D + 0.4, 12); }
    if (CH.E == 9 && CH.D > 5 && test_obstacle((int)b->x, (int)b->y + 64)) { CH.E = 10; CH.D = 0; camera_shake(10); snd_play_orig(51, 220, 128); }
    if (CH.E == 10 && CH.A >= 12320 && p->state != ST_DEAD) { G.cam.M3 = 0; G.cam.minC = (float)(CH.A - 200); }
    if (CH.E == 10 && CH.A >= 12320) { CH.C = 0; CH.A = 12320; CH.D = 0; CH.E = 11; camera_shake(5); snd_play_orig(51, 220, 128); }
    /* 5428-5442 destruction */
    int hit2 = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !o->an.b) continue;
        if (o->type == OT_BTUNNEL2 && !(o->fl & F_DEFEATED) && obj_obj_hit(o, b)) {
            o->fl |= F_DEFEATED;
            for (int k = 1; k <= 10; k++) piece(o->x, o->y + 12 * k, "rockpiece");
            o->alive = 0; hit2 = 1;
        } else if (o->type == OT_BROCK2 && obj_obj_hit(o, b)) {
            snd_play_orig(13, 220, 128); obj_explode(o->x + 20, o->y + 36); o->alive = 0;      /* flung (its own events: INFERRED) */
        } else if ((o->type == OT_BRK1 || o->type == OT_BRK2 || o->type == OT_BRK3) && obj_obj_hit(o, b) &&
                   o->x <= b->x + (o->type == OT_BRK2 ? 99 : 29)) {
            static const int dx1[3] = { 40, 56, 72 }, dx2[3] = { 0, 16, 32 };
            const int *dx = o->type == OT_BRK1 ? dx1 : dx2;
            for (int r = 0; r < 2; r++) for (int k = 0; k < 3; k++) piece(o->x + dx[k], o->y, "brkdebris");
            if (o->type == OT_BRK1) obj_effect(o->x + 50, o->y + 31, "explosion");
            else if (o->type == OT_BRK2) obj_effect(o->x + 23, o->y + 35, "explosion");
            else obj_effect(o->x + 31, o->y + 36, "explosion");
            snd_play_orig(o->type == OT_BRK2 ? 98 : 38, 220, 128); o->alive = 0;
        } else if ((o->type == OT_DRAGONFLY || o->type == OT_TURRETUS) && !(o->fl & F_DEFEATED) && obj_obj_hit(o, b)) {
            o->A = 0; o->kx = CH.C + 3; o->ky = cross_rand(12) - 8;
        }
    }
    if (hit2) { snd_play_orig(13, 220, 128); CH.C = 6; CH.G = 3; }
    /* 5443-5451 Lilac vs boulder (group on until death) */
    if (p->state != ST_DEAD) {
        if (p->x < b->x + 200 && p->y - ORIG_REF_DY > b->y - 100 && (p->held & BTN_RIGHT) && p->state != ST_HURT &&
            mvB < fmax(CH.C + 1, 7) && b->x < plx) {                                            /* 5443 */
            double add = 0.02 + 0.15 * (1 - p->ground);
            if (p->ground) p->gsp += add; else p->vx += add;
        }
        int sx = plx - 7, sy = ply - 34;
        if (!p->hurt_pending && !player_invincible(p) && p->state != ST_HURT && obj_rect_hit(b, sx, sy, sx + 15, sy + 46) &&
            ((CH.E != 8 && CH.E != 9 && CH.E < 11) || ((CH.E == 8 || CH.E == 9) && fabs(mvB) < 3))) player_contact(p, 5);   /* 5444 */
        if (p->state == ST_HURT && CH.E < 11) { if (p->ground) p->gsp = 4 + CH.C; else p->vx = 4 + CH.C; CH.flag3 = 1; }   /* 5445 */
        if (CH.flag3 && p->state != ST_HURT) { CH.flag3 = 0; if (p->ground) p->gsp = fmax(p->gsp, CH.C); else p->vx = fmax(p->vx, CH.C); }   /* 5446 */
        if (b->x < plx - 320) CH.C = fmax(CH.C, mvB);                                            /* 5447 */
        if (p->y - ORIG_REF_DY > b->y - 64 && p->x < b->x + 50 && b->x < plx && p->state != ST_HURT) {   /* 5449 push out */
            p->x = b->x + 50; if (p->ground) p->gsp = fmax(p->gsp, 4); else p->vx = fmax(p->vx, 4);
        }
        int over = obj_rect_hit(b, plx - 7, ply - 30, plx + 8, ply + 14);
        if (over && p->state != ST_HURT && CH.E != 8 && test_obstacle(plx + 12, ply)) player_kill(p);   /* 5450 crush */
    }
    /* 5453-5460 catch-up / respawn: the boulder ran past the view */
    if (CH.A > viewL + 626 && CH.E > 0 && CH.E < 11) {
        CH.flag4 = 1; CH.C = 14; ach_earn(33);                                                   /* records x2 y33 "Rolling Stones" (RE) */
        static const double RA[11] = { 0, 672, 2592, 2592, 4320, 5472, 7072, 8480, 8480, 8480, 8480 }, RB[11] = { 0, 2112, 2240, 2240, 2240, 2312, 2384, 2384, 2384, 2384, 2384 };
        static const int RE[11] = { 0, 1, 1, 2, 3, 4, 5, 6, 6, 6, 6 };
        int e = CH.E; CH.A = RA[e]; CH.B = RB[e]; CH.E = RE[e]; if (e >= 7) CH.H = 0;
    }
    (void)viewT;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_CH")) printf("ch f%d %d %d A%g B%g C%g D%g E%d G%g p %d %d camA %g\n", G.frame, (int)b->x, (int)b->y, CH.A, CH.B, CH.C, CH.D, CH.E, CH.G, plx, ply, G.cam.A);
#endif
}
int chase_running(void) { return CH.started && CH.E < 11; }
void chase_end(void) { if (CH.started) { CH.ended = 1; G.cam.M3 = 0; } }   /* 5461: the boss trigger turns the chase off */
