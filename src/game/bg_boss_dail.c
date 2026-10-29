/* Battle Glacier 6: Prince Dail on the Shade Beast. Spec: build/re/spec_bg_bosses.md section 6.
 * RE handle_frame_65 14912-15136 (groups: 0x22ed6 the beast's assembly, 0x22ed5 the fight, 0x22ed0 the beast's rig), start
 * events 15151-15155; RV build/orig_traces/bg_bosses d1_l (Lilac waiting at the right), d2_l (Lilac jumping about: 1 hit).
 * The arena: CameraLimitBoss 608..1439 x 1440..1951, hatch doors at x 576 / 1440. The beast's anchor BossDailSpawnPoint swings
 * its D within C +- 160 (B accelerates 0.05 a frame to +-2; RV d1_l), the head follows, a neck of 72 eyes joins it to the
 * floor (D index, E phase - 30 an eye, F the amplitude e^(0.0247 k): RV d2_l), Dail rides the head.
 * Dail: D state (0 wait, 1 cards, 2 the card wall, 3 the vortex, 9 falling, 10 the end), E attacks since the last vortex, F timer,
 * G shield (2), C the Q20 hit timer, I hits taken (7 win), A 500 refilled. Cycle (RE 14976-14981, RV d1_l / d2_l): D 0 F > 60 ->
 * D 1 (E + 1) for 56 frames (three cards, 14982-14984); E 3 -> D 2 (F -120: seven cards fly to a V and each casts a vertical
 * lightning column, 20 damage, RV d2_l f1186-f1390); E 7 and F > 227 -> D 3 (sound 349, E 0): the beast comes down to the
 * floor with its mouth open (F 15 -> -80, H 0 -> 1, 14947-14952), rings of 8 vortex stars every 30 frames (3 damage + the curse
 * tint, 14969-14975), Dail within reach for ~540 frames; F > 630 the beast rises, F 720 -> D 1.
 * Hits (15033-15044): a hit on the shield (G > 0) takes one off (sound 315, C 20); G 0 -> sound 11; a hit without it: C 120,
 * sounds 25 + 355, anim 8, I + 1 (I 3 / 5: Dail's lines 357 / 356); I >= 7 (15049): music fade, "da_dail_no", D 9: the fall
 * (bossdailfall, gravity 0.3), explosions on the beast from F 90 (sound_explode, shake 5), F 400 -> D 10 (white flash, 76);
 * D 10: the parts go (F 19), the stage-clear card at F 465 -> results_begin_boss (the row's results wipe).
 * INFERRED (labelled below): the head's offset from the anchor, the eyes' neck geometry, the cards' life (3 dashes), the vortex
 * patrol, the assembly timing (one eye per 7 frames), the shield's restoration at the vortex end. */
#include "bg_boss.h"
#include <stdio.h>
#include <stdlib.h>

#define NEYE 72
#define FL_LATCH 0x8                                 /* flag 3: the hit is being handled (15039 / 15040) */
#define MAXCARD 24
enum { CK_AIM = 0, CK_WALL = 1 };

static struct {
    int on, phase, t;                                /* phase: 0 waiting, 1 assembly, 2 fight, 3 over */
    double sC, sD, sB, sA; int sfl;                  /* the anchor (spawn point): C centre, D x, B speed, A phase, flag 2 */
    double bx, by, bF, bH, bG;                       /* the head: x, y, F height term (15 / -80), H mouth (0..1), G phase */
    double D, E, F, G, I; int fl;                    /* Dail */
    int eyes, music, v2, v3, v5, res, cam, fade, dead_x;
    float fx, fy, fvx, fvy;                          /* bossdailfall */
    short card[MAXCARD]; int ncard;
} B;

static obj_t *dail(void) { return bgb_get(OT_BGB_DAIL, 0); }
static obj_t *beast(void) { return bgb_get(OT_BGB_SBEAST, 0); }
static obj_t *crown(void) { return bgb_get(OT_BGB_SBCROWN, 0); }
static obj_t *fall(void) { return bgb_get(OT_BGB_DAILFALL, 0); }
int bg_boss_dail_state(void) { return B.on ? (B.phase == 2 || B.phase == 3 ? (int)B.D : -B.phase - 1) : -99; }

void bgd_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&B, sizeof B); }
    memset(&B, 0, sizeof B);
    obj_t *d = dail(), *b = beast(), *s = bgb_get(OT_BGB_DAILSPAWN, 0);
    if (!d || !b) return;
    B.on = 1;
    B.sC = s ? s->x : 1024; B.sD = B.sC; B.sB = -2;                         /* RV d1_l: C 1024, B -2 at the start */
    B.bx = b->x; B.by = b->y; B.bF = 15;
    B.G = 2; B.D = 0; d->A = 500; d->fl |= F_NOFLY;                         /* 15153: Dail C = D = X, flag 16; G 2 (RV) */
    bgb_set(d, "bgd_dail"); bgb_set(b, "bgd_beast"); bgb_set(crown(), "bgd_crown");
    d->hidden = b->hidden = 1; if (crown()) crown()->hidden = 1;             /* shown as the beast assembles */
    obj_t *f = fall(); if (f) { bgb_set(f, "bgd_fall"); f->hidden = 1; }
}

/* ---- cards (BossDailCard_3371): av 0 kind, 1 state J, 2 timer H, 3 vx, 4 vy, 5 dashes, 6 target x, 7 target y */
static obj_t *card_new(float x, float y, int kind, double vx, double vy) {
    obj_t *c = bgb_spawn(OT_BGB_D_CARD, "bgd_card", x, y, 3);
    if (!c) return NULL;
    c->av[0] = kind; c->av[1] = 0; c->av[2] = 0; c->av[3] = vx; c->av[4] = vy; c->av[5] = 0;
    if (B.ncard < MAXCARD) B.card[B.ncard++] = (short)(c - G.o);
    return c;
}
static void cards_frame(player_t *p) {
    int n = 0;
    for (int k = 0; k < B.ncard; k++) {
        obj_t *c = bgb_slot(B.card[k]);
        if (!c || c->type != OT_BGB_D_CARD) continue;
        B.card[n++] = B.card[k];
        double *a = c->av;
        a[2] += 1;
        if (a[0] == CK_AIM) {
            /* RV d2_l card0.1: J0 flies 8 px a frame ~12 frames, J1 holds ~30, then dashes at 8 px a frame for 16 frames toward the
             * player (J2) and stops 18 (J3), re-aims; INFERRED: three dashes, then it goes */
            if (a[1] == 0) { c->x += (float)a[3]; c->y += (float)a[4]; if (a[2] >= 7) { a[1] = 1; a[2] = 0; } }
            else if (a[1] == 1 || a[1] == 3) {
                if (a[2] >= (a[1] == 1 ? 36 : 18)) {
                    if (a[5] >= 3) { obj_effect(c->x, c->y, "explosion02"); c->alive = 0; n--; continue; }
                    double ang = atan2(-(sbb_py(p) - c->y), sbb_px(p) - c->x);
                    a[3] = 8 * cos(ang); a[4] = -8 * sin(ang); a[1] = 2; a[2] = 0; a[5] += 1;
                }
            } else if (a[1] == 2) { c->x += (float)a[3]; c->y += (float)a[4]; if (a[2] >= 16) { a[1] = 3; a[2] = 0; } }
            if (sbb_csens_hit(p, c)) sbb_hurt(p, 10);                         /* 15006: the card's effect, 10 */
        } else {                                                              /* CK_WALL: fly to the V slot 41 frames, the column ~155 */
            if (a[1] == 0) { c->x += (float)a[3]; c->y += (float)a[4]; if (a[2] >= 41) { a[1] = 5; a[2] = 0; } }
            else if (a[1] == 5) {
                if (a[2] >= 155) { obj_effect(c->x, c->y, "explosion02"); c->alive = 0; n--; continue; }
                int X = (int)sbb_px(p);
                if (abs(X - (int)c->x) < 9 && a[2] >= 4) sbb_hurt(p, 20);          /* 15029: the lightning column, 20 */
            }
        }
    }
    B.ncard = n;
}
static void cards_throw(obj_t *d) {                  /* 14982-14984: three cards at 270 / 225 / 315 from x + 0 / -40 / +40 (RV), sound 71 */
    static const double OX[3] = { 0, -40, 40 }, VX[3] = { 0, -8, 8 };
    for (int k = 0; k < 3; k++) card_new(d->x + (float)OX[k], d->y + 15, CK_AIM, VX[k], 8);
    snd_play_orig(71, 220, 128);
}
static void cards_wall(obj_t *d) {                   /* RV d2_l f1186-1228: seven cards to a V (+-123 / 246 / 369, +300 / 259 / 177 / 95) */
    static const double DX[7] = { 0, -123, 123, -246, 246, -369, 369 }, DY[7] = { 300, 259, 259, 177, 177, 95, 95 };
    for (int k = 0; k < 7; k++) card_new(d->x, d->y, CK_WALL, DX[k] / 41., DY[k] / 41.);
    snd_play_orig(71, 220, 128);
}
/* ---- vortex stars (BossVortexHitZone_3381 + BossVortexStar_3379): rings of 8 at speed 5 from the mouth (RV d2_l), 3 damage + tint */
static void stars_ring(float x, float y, int ring) {
    for (int k = 0; k < 8; k++) {
        double a = 45. * k + 8 * ring;
        obj_t *s = bgb_spawn(OT_BGB_D_STAR, "bgd_vortex2", x, y, 3);   /* the 64 x 64 hit zone shows as BossVortex2 (INFERRED) */
        if (s) { s->vx = (float)(5 * sbb_cos(a)); s->vy = (float)(5 * sbb_sin(a)); }
    }
}
static void stars_frame(player_t *p) {
    for (int k = 0; k < bgb_runtime_n(); k++) {
        obj_t *s = bgb_runtime(k);
        if (!s || s->type != OT_BGB_D_STAR) continue;
        s->x += s->vx; s->y += s->vy;
        if (s->x < G.camx - 64 || s->x > G.camx + SCREEN_W + 64 || s->y < G.camy - 64 || s->y > G.camy + SCREEN_H + 64) { s->alive = 0; continue; }
        if (sbb_csens_hit(p, s) && sbb_hurt(p, 3)) p->tint = 100;              /* 14974: LIFE.B + 3, OTHERS.R (the curse tint) */
    }
}

static void place(obj_t *d, obj_t *b) {
    /* the anchor (14933-14936, RV d1_l): B +- 0.05 toward +-2, the flag flips beyond C +- 160 */
    if (!(B.sfl & 2)) B.sB = fmax(B.sB - 0.05, -2); else B.sB = fmin(B.sB + 0.05, 2);
    B.sD += B.sB;
    if (B.sD < B.sC - 160) B.sfl |= 2;
    if (B.sD > B.sC + 160) B.sfl &= ~2;
    B.sA = fmod(B.sA + 3, 360); B.bG = fmod(B.bG + 3, 360);
    /* the head (INFERRED offsets, RV d2_l: x within the anchor +- 60, y 1714..1727 while F 15; down to ~1900 at F -80) */
    double base = 1720 + 6 * sbb_sin(B.bG + 60);
    B.bx = B.sD + 50 * sbb_cos(B.bG);
    B.by = base - (B.bF - 15) * (180 / 95.);
    if (b) { b->x = (float)(int)B.bx; b->y = (float)(int)B.by; }
    obj_t *c = crown(); if (c && b) { c->x = b->x; c->y = b->y; }
    if (d && b && B.phase < 3) { d->x = b->x - 2; d->y = b->y - 58; }         /* RV: Dail rides the head (x - 2, y - 58) */
}

static void hits(obj_t *d) {
    if (!d) return;
    d->A = 500;                                                               /* 15033 */
    if (d->c > 0 && !(B.fl & FL_LATCH) && B.D < 9) {
        B.fl |= FL_LATCH;
        if (B.G > 0) {                                                        /* 15038 / 15039 */
            B.G -= 1; d->c = 20; snd_play_orig(315, 220, 128);
            if (B.G <= 0) snd_play_orig(11, 220, 128);                        /* 15036 */
        } else {                                                              /* 15041 */
            d->c = 120; B.I += 1; snd_play_orig(25, 220, 128); snd_play_orig(355, 220, 128); bgb_set(d, "bgd_dail8");
            if (B.I >= 3 && !B.v3) { B.v3 = 1; snd_play_orig(357, 220, 128); }   /* 15043 */
            if (B.I >= 5 && !B.v5) { B.v5 = 1; snd_play_orig(356, 220, 128); }   /* 15044 */
        }
    }
    if (d->c <= 0) B.fl &= ~FL_LATCH;                                          /* 15040 */
    if (d->c == 10 && B.G <= 0) { bgb_set(d, "bgd_dail"); snd_play_orig(1, 220, 128); }   /* 15042 */
    d->inv = d->c > 0;
}

static void fight(player_t *p) {
    obj_t *d = dail(), *b = beast();
    if (!d || !b) return;
    if (B.D == 0 || B.D == 3) B.F += 1;                                        /* 14976 / 14946: F counts in D 0 and D 3 only */
    else B.t += 1;                                                            /* D 1 / 2: Dail's throw (anim 7, RV 56 frames) */
    if (B.D == 0 && B.F > 60 && B.E < 7 && B.E != 3) { B.D = 1; B.E += 1; B.F = 0; B.t = 0; bgb_set(d, "bgd_dail7"); if (d->an.b) anim_restart(&d->an); }   /* 14977 / 14980 */
    else if (B.D == 0 && B.F > 60 && B.E == 3) { B.D = 2; B.E += 1; B.F = -120; B.t = 0; bgb_set(d, "bgd_dail7"); if (d->an.b) anim_restart(&d->an); }   /* 14979 */
    else if (B.D == 0 && B.E >= 7 && B.F > 227) { B.D = 3; B.E = 0; B.F = 0; snd_play_orig(349, 220, 128); }   /* 14981 */
    if (B.D == 1 && B.t == 45) cards_throw(d);                                /* RV: the three cards 45 frames into D 1 */
    if (B.D == 2 && B.t == 35) cards_wall(d);                                  /* RV f1186: 35 frames into D 2 */
    if ((B.D == 1 || B.D == 2) && B.t >= 56) { B.D = 0; bgb_set(d, "bgd_dail"); }   /* 14985: the throw over (F kept: 0 / -120) */
    if (B.D == 3) {                                                            /* 14947-14958: down, the mouth, the stars */
        if (B.F < 630) { B.bF = fmax(B.bF - 1, -80); if (B.bF <= -80) B.bH = fmin(B.bH + 0.06, 1); }
        else { B.bH = fmax(B.bH - 0.06, 0); B.bF = fmin(B.bF + 1, 15); }
        if (B.F >= 430 && B.F < 430 + 120 && (int)(B.F - 430) % 30 == 0) stars_ring(b->x, b->y + 30, (int)(B.F - 430) / 30);   /* RV f2335.. */
        if (B.F >= 720) { B.D = 1; B.E = 1; B.F = 0; B.t = 0; B.G = 2; B.bF = 15; bgb_set(d, "bgd_dail7"); }   /* RV f2627 (INFERRED: the shield back) */
    }
    hits(d);
    if (B.I >= 7 && B.D < 9) {                                                /* 15049 / 15051 */
        B.D = 9; B.F = 0; music_fade(120, 0); voice_play("dail_no"); snd_play_orig(47, 220, 128);
        bgb_set(b, "bgd_beast8"); bgb_set(crown(), "bgd_crown8");
        d->hidden = 1; obj_t *f = fall();
        B.fx = d->x; B.fy = d->y; B.fvx = (float)(d->x < 1024 ? 2 : -2); B.fvy = -6;
        if (f) { f->hidden = 0; f->x = B.fx; f->y = B.fy; }
        B.phase = 3; B.ncard = 0;
        for (int k = 0; k < bgb_runtime_n(); k++) { obj_t *o = bgb_runtime(k); if (o && (o->type == OT_BGB_D_CARD || o->type == OT_BGB_D_STAR)) o->alive = 0; }
    }
    cards_frame(p);
    stars_frame(p);
}

static void ending(player_t *p) {
    (void)p;
    obj_t *b = beast(), *f = fall();
    B.F += 1;
    if (B.D == 9) {
        B.fvy = fminf(B.fvy + 0.3f, 15); B.fx += B.fvx; B.fy += B.fvy;           /* 15054: B + 0.3 */
        if (B.fy > 1951) { B.fy = 1951; B.fvy = 0; B.fvx = 0; }                 /* 15059: the floor (CameraLimitBossBottom) */
        if (f) { f->x = B.fx; f->y = B.fy; }
        if (B.F == 60) voice_play("da_05_03");                                 /* 15060 */
        if (B.F > 90 && G.frame % 6 == 0 && b) {                                /* 15061-15064 */
            obj_effect(b->x - 60 + cross_rand(121), b->y - 60 + cross_rand(121), "explosion06"); bgb_explode_snd();
            if (G.cam.Z < 5) G.cam.Z = 5;
        }
        if (B.F > 400) { B.D = 10; B.F = 0; snd_play_orig(76, 220, 128); B.fade = 16; }   /* 15065 */
    } else if (B.D == 10) {
        if (B.F == 19) {                                                         /* 15068: the parts go */
            if (b) b->hidden = 1;
            if (crown()) crown()->hidden = 1;
            for (int k = 0; k < bgb_runtime_n(); k++) { obj_t *o = bgb_runtime(k); if (o && o->type >= OT_BGB_D_FIRST && o->type <= OT_BGB_D_LAST) o->alive = 0; }
            B.eyes = 0;
        }
        if (B.F == 465 && !B.res) { B.res = 1; results_begin_boss(game_timer()); }   /* 15085: the stage-clear card (the row's wipe follows) */
    }
    if (B.fade > 0) B.fade--;
}

static void assembly(player_t *p) {
    obj_t *d = dail(), *b = beast();
    double X = sbb_px(p);
    if (B.phase == 0) {
        if (b && X > b->x - 200 && !B.music) { B.music = 1; music_fade(120, 0); }   /* 14914: fadeout 2000 + lock */
        if (d && X > d->x - 100) {                                                 /* 14915 */
            B.phase = 1; B.t = 0;
            for (int k = 0; k < bgb_count(OT_BGB_HATCH); k++) { obj_t *h = bgb_get(OT_BGB_HATCH, k); if (h) { bgb_set(h, "bgb_hatch"); h->y = 1824; } }
            snd_play_orig(351, 220, 128);
            level_lock_markers(); B.cam = 1;                                       /* the boss bounds (14915 CAM, 14931 C 576) */
        }
        return;
    }
    B.t++;
    if (B.t % 7 == 5 && B.eyes < NEYE) {                                           /* 14916 / 14917: a shade gathers, an eye forms */
        B.eyes++;
        if (b) {                                                                  /* EffectBossShade: flies into the head (INFERRED 40 frames) */
            float sx = b->x - 200 + cross_rand(401), sy = b->y - 120 + cross_rand(160);
            obj_t *e = bgb_spawn(OT_BGB_FX, "bgd_shade", sx, sy, 3);
            if (e) { e->vx = (b->x - sx) / 40.f; e->vy = (b->y - sy) / 40.f; e->av[6] = 40; }
        }
    }
    if (B.eyes >= 8 && !B.v2) { B.v2 = 1; voice_play("da_05_02"); }              /* 14924 */
    if (B.eyes >= 24) { if (b) b->hidden = 0; if (crown()) crown()->hidden = 0; if (d) d->hidden = 0; }
    if (B.eyes >= 64 && B.fade == 0 && B.phase == 1 && B.t > 0) { B.fade = 32; }   /* 14925: the white flash */
    if (B.fade > 0 && --B.fade == 0) {                                             /* 14929: the fight */
        B.eyes = NEYE; B.phase = 2; B.D = 0; B.F = 0; B.E = 0;
        const char *m = level_music_slot(4); if (m) music_play(m);                  /* ML.Y 1 -> slot 4 (m_boss3) */
    }
}

#ifdef FP_HOST_ENV
static void dbg(const player_t *p) {
    static int on_ = -1; if (on_ < 0) on_ = getenv("FP_DBG_BGD") != NULL;
    obj_t *d = dail();
    if (on_) printf("bgd %d ph %d D %.0f E %.0f F %.0f G %.0f I %.0f c %d xy %.0f %.0f eyes %d px %.0f py %.0f\n", G.frame, B.phase, B.D, B.E, B.F, B.G, B.I,
                    d ? d->c : -1, d ? d->x : 0, d ? d->y : 0, B.eyes, sbb_px(p), sbb_py(p));
}
#else
static void dbg(const player_t *p) { (void)p; }
#endif

void bgd_frame(player_t *p) {
    if (!B.on) return;
    obj_t *d = dail(), *b = beast();
    if (B.phase < 3) place(d, b);
    if (B.phase < 2) assembly(p);
    else if (B.phase == 2) fight(p);
    else ending(p);
    dbg(p);
}

/* the neck: 72 eyes from the anchor on the floor up to the head (RV d2_l: E = phase - 30 k, F = 1.45 e^(0.0247 k); INFERRED
 * the wiggle x 2.5 F sin E, y F cos E), drawn behind the head; the mouth (y scale 0.7 H + 0.2, 14938) and the teeth rows */
static void draw_neck(void) {
    int a = ens_anim("bgd_eye"); if (a < 0 || !B.eyes) return;
    const anim_t *an = &G.ens.anims[a];
    const frame_t *f = &G.ens.frames[G.ens.seq[an->first + ((G.frame / 6) % (an->count ? an->count : 1))]];
    double x0 = B.sD, y0 = 1950, x1 = B.bx, y1 = B.by + 60;
    for (int k = 0; k < B.eyes && k < NEYE; k++) {
        double t = k / (double)(NEYE - 1), E = fmod(B.bG * 2 - 30 * k + 3600, 360), Fm = 1.455 * exp(0.0247 * k);
        double x = x0 + (x1 - x0) * t + 2.5 * Fm * sbb_sin(E), y = y0 + (y1 - y0) * t + Fm * sbb_cos(E);
        frame_draw(&G.ens, f, floorf((float)x - G.camx), floorf((float)y - G.camy), 0, 0xffffffff);
    }
}
int bgd_draw(const obj_t *o, int pass) {
    (void)pass;
    if (!B.on) return 0;
    if (o->type == OT_BGB_SBEAST) {
        draw_neck();
        if (!o->hidden) {
            int m = ens_anim("bgd_mouth");
            if (m >= 0 && B.bH > 0) {                                                  /* the open mouth below the head */
                const frame_t *f = &G.ens.frames[G.ens.seq[G.ens.anims[m].first]];
                frame_draw_scaled(&G.ens, f, floorf(o->x - G.camx) - f->hx, floorf(o->y + 24 - G.camy) - f->hy, (float)f->w, (float)(f->h * (0.7 * B.bH + 0.2)), 0xffffffff);
            }
            sbb_draw(o, 0xffffffff);
        }
        return 1;
    }
    if (o->type == OT_BGB_D_CARD && o->av[0] == CK_WALL && o->av[1] == 5) {          /* the lightning column through the view */
        int a = ens_anim("bgd_bolt");
        if (a >= 0) {
            const frame_t *f = &G.ens.frames[G.ens.seq[G.ens.anims[a].first + (G.frame / 3) % (G.ens.anims[a].count ? G.ens.anims[a].count : 1)]];
            for (float y = floorf(G.camy / 64) * 64; y < G.camy + SCREEN_H + 64; y += 64)
                frame_draw_rot(&G.ens, f, floorf(o->x - G.camx), floorf(y - G.camy), 90, 0, 0xffffffff);
        }
        sbb_draw(o, 0xffffffff);
        return 1;
    }
    if (o->type == OT_BGB_D_CARD && o->av[0] == CK_AIM) {                          /* the card's green spark (BossDailCardEffect) */
        int a = ens_anim("bgd_cardx");
        if (a >= 0) frame_draw(&G.ens, &G.ens.frames[G.ens.seq[G.ens.anims[a].first]], floorf(o->x - G.camx), floorf(o->y - G.camy), 0, 0xffffffff);
        sbb_draw(o, 0xffffffff);
        return 1;
    }
    if (o->type == OT_BGB_DAILFALL || o->type == OT_BGB_DAILSPAWN) { if (!o->hidden) sbb_draw(o, 0xffffffff); return 1; }
    return 0;
}
int bgd_input(uint32_t *held) {                                                     /* D 10: the cutscene (CTRL.M 1, 15068) */
    if (!B.on || !(B.phase == 3 && B.D == 10 && B.F >= 19)) return 0;
    *held = 0; return 1;
}
float bgd_hud_dy(void) { return 0.f; }
int bgd_player_hidden(void) { return 0; }
obj_t *bgd_bot_target(void) {                                                       /* host bot: Dail within reach (the vortex) */
    obj_t *d = dail();
    return B.on && B.phase == 2 && d && B.D == 3 && d->y > 1780 && d->c <= 0 ? d : NULL;
}
