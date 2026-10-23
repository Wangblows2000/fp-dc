#include <stdlib.h>
/* Minibosses and bosses (RE: build/re/spec_wretchnid.md, spec_dv_enemies.md §5, spec_dv_bosses.md).
 * Each fight is an event script; its state lives in a static struct registered for snapshots. The hittable parts
 * (Wretchnid weakspot, Tredd eye, snake segments / hitbox) are ordinary qualifier-20 enemies in combat.c. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../fp.h"

static double snap_half(double v) { double h = floor(v * 2 + 0.5) / 2; return fabs(v - h) < 1e-12 ? h : v; }
/* Chowdren's degree sin/cos: exact 0 at 180 / 90 / 270; RV (tredd3.csv D = -0.0625 at H 210): exact +-0.5 too */
static double c_sin(double deg) { return deg == 180. ? 0. : snap_half(fd_sin(deg / 57.29577951)); }
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : snap_half(fd_cos(deg / 57.29577951)); }
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


/* ================================================================ Tredd (DV3 miniboss, events 5237-5299; RE spec_dv_enemies §5,
 * piece placement RV from the original trace build/orig_traces/tredd.csv):
 *   body x = (int)I, y = (int)(E + 16 D); piece k (B = -48/-48/-48/-24/-36/-16 left, mirrored right):
 *   x = (int)(X + F B), y = (int)(E + off_k + D B) with off = 0, 32, -, 8, 0, -8 and the column (piece 3) at
 *   y = (int)(E + 64 + min(0, 2 D B)). Gait: w = (1 - sin^8(H/2))^4 - 0.5, D = cos^40(H + 70) - sin^4 H,
 *   F = 1.76 w J, G = 2.2 J (0.88 - |F|); H += 1.5 (340 -> 200), I += 1.76 G while near the view. */
static struct { int active, latch, stomp, M, K, dead_t, bombs, claw8, free, turn, pd; double colE; double D, E, F, Gw, H, I; int J, L; double pieceB[12]; } TD;
static const double TD_B[12] = { -48, -48, -48, -24, -36, -16, 48, 48, 48, 24, 36, 16 };
static const double TD_OFF[12] = { 0, 32, 64, 8, 0, -8, 0, 32, 64, 8, 0, -8 };
static obj_t *td_piece(int k) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_TD_PIECE + k) return &G.o[i]; return NULL; }
static double powi(double v, int n) { double r = 1; while (n-- > 0) r *= v; return r; }
static void td_spawn(void) {
    memset(&TD, 0, sizeof TD); TD.J = 1;
    for (int k = 0; k < 12; k++) TD.pieceB[k] = TD_B[k];
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_TD_BODY) { set_ens(o, "td_body"); o->A = 50; o->bd = 0; TD.H = 200; }
        if (o->type == OT_TD_PUSH || o->type == OT_TD_PUSH2) o->an.b = NULL;
        if (o->type >= OT_TD_PIECE && o->type < OT_TD_PIECE + 12) {
            static const char *pn[6] = { "td_claw", "td_shaft", "td_column", "td_joint", "td_p5", "td_p6" };
            set_ens(o, pn[(o->type - OT_TD_PIECE) % 6]);
        }
    }
}
static void td_update(player_t *p) {
    obj_t *body = first_of(OT_TD_BODY);
    if (!body) return;
    if (!TD.active) { if (inside_view(body, 200)) TD.active = 1; return; }    /* 5244: groups on, first update next frame (RV) */
#ifndef DREAMCAST
    if (getenv("FP_TD_KILL") && G.frame == atoi(getenv("FP_TD_KILL"))) body->fl |= 0x40;   /* test hook */
#endif
    int dead = (body->fl & 0x40) != 0;
    /* 5237-5242 run before the body's events: previous frame's leg positions, E and D */
    /* 5241 / 5242: a stomping column (same window as its damage) destroys a BreakableRock it overlaps */
    if (TD.colE > 274 && TD.colE < 285) for (int k = 2; k < 12; k += 6) {
        obj_t *c = td_piece(k);
        if (!c || !((k < 6) ? TD.pd < 0 : TD.pd > 0)) continue;
        for (int i = 0; i < G.nobjs; i++) { obj_t *r = &G.o[i];
            if (r->alive && r->type == OT_ROCK && obj_obj_hit(r, c)) { r->alive = 0; obj_explode(r->x, r->y - 8); } }
    }
    /* leg damage (5237-5240): claws 5, stomping columns 10 */
    if (!dead && p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p) && !player_body_z(p) &&
        p->state != ST_ATTACK && p->state != ST_UPPERCUT) {
        int sx0 = (int)floor(p->x) - 7, sy0 = (int)floor(p->y - ORIG_REF_DY) - 34;
        for (int k = 0; k < 12; k++) {
            obj_t *o = td_piece(k);
            if (!o || !o->an.b) continue;
            int kind = k % 6;
            if (kind == 0 && !TD.claw8 && obj_rect_hit(o, sx0, sy0, sx0 + 15, sy0 + 46) && floor(p->y - ORIG_REF_DY) <= o->y + 7) { player_contact(p, 5); break; }
            if (kind == 2 && TD.colE > 274 && TD.colE < 285 && ((k < 6) ? TD.pd < 0 : TD.pd > 0) && obj_rect_hit(o, sx0, sy0, sx0 + 15, sy0 + 46)) { player_contact(p, 10); break; }
        }
    }
    /* 5245 (H = 230 once, condition "Y <= 79" of unknown meaning) does not fire here: RV tredd3.csv starts at H 200 */
    if (TD.I == 0) { TD.I = body->x; TD.E = body->y; }                                         /* 5246 */
    body->x = (float)(int)TD.I;                                                               /* 5247 */
    double h2 = fd_sin(TD.H / 2 / 57.29577951), w = powi(1 - powi(h2, 8), 4) - 0.5;          /* 5248 */
    double ch = c_cos(fmod(TD.H + 70, 360)), sh = c_sin(fmod(TD.H, 360));
    TD.D = powi(ch, 40) - powi(sh, 4);
    TD.F = 1.76 * w * TD.J; TD.Gw = 2.2 * TD.J * (0.88 - fabs(TD.F));
    body->y = (float)(int)(TD.E + 16 * TD.D + 1e-7);   /* +1e-7: the original's exact degree trig lands on whole values (RV tredd3.csv) */
    TD.pd = TD.J; TD.colE = TD.H;     /* pieces copy the body's J into their D and H into their E (RV tredd3.csv) */
    for (int k = 0; k < 12; k++) {                                                            /* 5249 */
        obj_t *o = td_piece(k);
        if (!o) continue;
        double B = TD.pieceB[k];
        o->x = (float)(int)(body->x + TD.F * B + 1e-7);
        o->y = (float)(int)(((k % 6 == 2) ? TD.E + 64 + fmin(0.0, 2 * TD.D * B) : TD.E + TD_OFF[k] + TD.D * B) + 1e-7);
    }
    if (TD.H > 284 && !TD.stomp && !dead) {                                                   /* 5250 / 5251 */
        TD.stomp = 1; camera_shake(3);
        if (inside_view(body, 200)) snd_play_orig(51, 220, 128);   /* stomp sound id not read: 51 (heavy impact) INFERRED */
    }
    if (TD.H >= 340) { TD.stomp = 0; TD.H -= 140; }                                           /* 5252 */
    /* 5253-5255 turning (RE + RV tredd3.csv): 5253 the body overlaps neither marker -> free to turn; 5254 free and
     * overlapping PushLeft with J = 1 (PushLeft2 with J = -1) -> turn pending; 5255 pending and H <= 202 (the next
     * step boundary, after the 5252 wrap) -> negate every piece's B and J. Markers and body: 32x32, hotspot (16,16). */
    {
        int ov = 0;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *m = &G.o[i];
            if (!m->alive || (m->type != OT_TD_PUSH && m->type != OT_TD_PUSH2)) continue;
            if (fabsf(body->x - m->x) < 32 && fabsf(body->y - m->y) < 32) ov |= m->type == OT_TD_PUSH ? 1 : 2;
        }
        if (!ov) TD.free = 1;
        if (TD.free && (((ov & 1) && TD.J == 1) || ((ov & 2) && TD.J == -1))) { TD.turn = 1; TD.free = 0; }
        if (TD.turn && TD.H <= 202) { TD.turn = 0; TD.J = -TD.J; for (int k = 0; k < 12; k++) TD.pieceB[k] = -TD.pieceB[k]; }
    }
    if (!dead && inside_view(body, 200)) { TD.H += 1.5; TD.I += 1.76 * TD.Gw; }               /* 5258 */
#ifndef DREAMCAST
    if (getenv("FP_DBG_TD")) { obj_t *a = td_piece(0), *b2 = td_piece(1), *c = td_piece(2), *d = td_piece(3), *e = td_piece(4), *f = td_piece(5), *r1 = td_piece(6);
        printf("td f%d %d %d D%.4f E%.0f F%.4f H%.1f I%.2f J%d L%d | %g,%g %g,%g %g,%g %g,%g %g,%g %g,%g r1 %g,%g\n", G.frame, (int)body->x, (int)body->y, TD.D, TD.E, TD.F, TD.H, TD.I, TD.J, TD.L,
            a?a->x:0, a?a->y:0, b2?b2->x:0, b2?b2->y:0, c?c->x:0, c?c->y:0, d?d->x:0, d?d->y:0, e?e->x:0, e?e->y:0, f?f->x:0, f?f->y:0, r1?r1->x:0, r1?r1->y:0); }
#endif
    /* attacks (group 0x222ee): bomb timer L, bombs at L == 150 / L > 300 while H > 300 */
    if (!dead) {
        if (TD.L != 150) TD.L++;                                                              /* 5259 */
        /* 5260-5265: claws read their E, a copy of the body's L from the previous frame (RV: tredd.csv tl1_E) */
        int cl = TD.L - 1, ca = cl == 140 ? 1 : cl == 200 ? 2 : cl == 280 ? 3 : 0;
        if (ca) for (int k = 0; k < 12; k += 6) { obj_t *c = td_piece(k); if (c) set_ens(c, ca == 1 ? "td_claw_open" : ca == 2 ? "td_claw" : "td_claw_snap"); }
        if (ca == 2) TD.claw8 = 0; else if (ca == 3) TD.claw8 = 1;
        if ((TD.L == 150 && TD.H > 300) || (TD.L > 300 && TD.H > 300)) {                      /* 5266 / 5267 */
            obj_t *b = obj_alloc();
            if (b) { b->type = OT_TD_BOMB; b->alive = 1; b->x = body->x; b->y = body->y; b->fy = b->y; b->vy = 0; set_ens(b, "td_bomb"); }
            TD.L = TD.L == 150 ? 151 : 0;                                                     /* reset after the second bomb: INFERRED */
        }
    }
    /* death (5282-5298): no fly-away, explosions every 7 frames, 10 crystals, debris, final blast */
    if (dead) {
        body->kx = body->ky = 0; body->ang = 0;
        set_ens(body, "td_die");
        if (G.frame % 7 == 0 && TD.K < 2) {
            obj_effect(body->x + cross_rand(41) - 20, body->y + cross_rand(41) - 20, "explosion06"); TD.M++;
            static const int ex[3] = { 311, 319, 320 }; snd_play_orig(ex[cross_rand(3)], 220, 128);
        }
        if (TD.M == 15) { body->absorbed = 10; TD.M++; }
        if (TD.M > 16) TD.K++;
        if (TD.K > 0) for (int k = 0; k < 12; k++) { obj_t *o = td_piece(k); if (o) { obj_t *d = obj_alloc(); if (d) { d->type = OT_DEBRIS; d->alive = 1; d->x = o->x; d->y = o->y; d->vx = (float)(cross_rand(15) - 10) * 0.3f; d->vy = -3; set_ens(d, "td_debris"); } o->alive = 0; } }
        if (TD.K >= 2) { snd_play_orig(38, 220, 128); obj_effect(body->x, body->y, "explosion"); camera_shake(3); body->alive = 0; }
    }
}
/* falling bomb -> blast (MonitorIcon frame 9) on solid ground: 20 damage to Player_326 while not hurt */
void tredd_bomb_update(obj_t *o, player_t *p) {
    if (o->type == OT_TD_BOMB) {
        o->y = (float)(int)o->fy; o->fy += o->vy; o->vy = fminf(o->vy + 0.3f, 12.f);
        if (obj_overlaps_bg(o)) {
            obj_t *b = obj_alloc();
            if (b) { b->type = OT_BLAST; b->alive = 1; b->x = o->x - 12; b->y = o->y; b->tm = 0; set_ens(b, "blast"); }
            camera_shake(3); o->alive = 0;
        }
    } else if (o->type == OT_BLAST) {
        if (++o->tm > 20) { o->alive = 0; return; }                    /* blast lifetime INFERRED */
        int px = (int)p->x, py = (int)(p->y - ORIG_REF_DY);
        if (p->state != ST_HURT && p->state != ST_DEAD && !p->hurt_pending && obj_rect_hit(o, px - 13, py - 13, px + 14, py + 14)) player_contact(p, 20);
    } else if (o->type == OT_DEBRIS) {
        o->x += o->vx; o->y += o->vy; o->vy += 0.4f;
        if (!obj_near_view(o, 100)) o->alive = 0;
    }
}

/* ================================================================ dispatch */
void bosses_spawn(void) {
    wr_spawn(); td_spawn();
    snap_register(&WR, sizeof WR); snap_register(&TD, sizeof TD);
}
void bosses_update(player_t *p) {
    if (first_of(OT_WR_BASE) || WR.active) wr_update(p);
    td_update(p);
}
/* legs / turret visibility and the Base flash (4861/4862: inverted while the weakspot's C > 0 and A > 0) */
int boss_hidden(const obj_t *o) {
    if (o->type == OT_WR_BARRIER || o->type == OT_WR_WEAK || o->type == OT_TD_PUSH || o->type == OT_TD_PUSH2) return 1;
    if (WR.burst && !WR.weakBlink && (o->type == OT_WR_TURRET || (o->type >= OT_WR_LEG && o->type < OT_WR_LEG + 12))) return 1;
    return 0;
}
int boss_flash(const obj_t *o) {
    if (o->type == OT_WR_BASE) { obj_t *w = first_of(OT_WR_WEAK); return w && w->c > 0 && w->A > 0; }
    return o->inv;
}
void bosses_draw(void) {}
