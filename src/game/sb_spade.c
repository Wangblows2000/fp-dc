/* SB2 boss for Lilac: Spade (BossSpadeMask_2248 holds the state, BossSpade_2247 is the sprite, BossSpadeHitbox_2250 takes
 * the hits, BossSpadeCard_2249 his cards, EffectsSpadeImage_456 the dash afterimages), delivered and picked up by the chopper
 * (BossChopper_2216 and its parts, the Carol / Milla boss of sb_chopper.c, here only a carrier).
 * RE handle_frame_37_events group 0x228ef: 9598 (trigger, MV.A == 0), 9599-9611 (the chopper), 9612-9711 (Spade: one
 * group per state F, 0x228ea / e5 / f2 / f4 / ee / e6 / e7 / ec), 9712-9727 (cards); RV traces p_kill / p_idle
 * (build/orig_traces/sb_boss). spec_sb_bosses Part C.
 * Mask alterables: A / B position, C / D velocity, E floor Y, F state (-1 carried in, 0 run, 2 jump / fall, 3 air dash,
 * 4 card throw, 5 spinning card rain, 6 / 7 staggered, 9 carried out), H facing, I wanted direction, J / K / L / N timers and
 * counters, M jumps left before the card rain. Spade alterables: A cards thrown x 10, B cards per throw (3), F stagger time.
 * Hitbox: A HP 60 (Q20), D stagger stages 3 (one per 20 HP). Chopper (here): A 99 every frame, E / F position, G / H
 * velocity, I rotor bob (+6), L 8, M -1. */
#include "sb_boss.h"
#include <stdio.h>
#include <stdlib.h>

enum { P_0, P_1, P_2, P_8, P_15, P_21, P_24, P_26, P_29, P_NA };              /* Spade animations 0 1 2 8 15 21 24 26 29 */
static const char *const PN[P_NA] = { "sbp_0", "sbp_1", "sbp_2", "sbp_8", "sbp_15", "sbp_21", "sbp_24", "sbp_26", "sbp_29" };
enum { Q_CARD, Q_CARD7, Q_HIT, Q_SPARK, Q_N };
static const char *const QN[Q_N] = { "sbp_card", "sbp_card7", "sbp_hit", "sbp_tspark" };
static short PA[P_NA][2], QA[Q_N];

#define NCARD 24
#define NIMG 12
static struct {
    int on, once, ended, ak, fl2, f0, sdone;
    double A, B, C, D, E, F, H, I, J, K, L, M, N;      /* BossSpadeMask */
    double sA, sB, sF;                                  /* BossSpade A, B, F */
    double hD;                                          /* hitbox D */
    double cE, cF, cG, cH, cI, cL, cM;                  /* the chopper */
    double tB[4], tC[4];                                /* its tail links */
    short card[NCARD]; int ncard;
    short img[NIMG]; int nimg;
} S;

static obj_t *mask(void) { return sbb_get(OT_SB_SP_MASK, 0); }
static obj_t *spade(void) { return sbb_get(OT_SB_SPADE, 0); }
static obj_t *hitbox(void) { return sbb_get(OT_SB_SP_HIT, 0); }

/* force_animation(k) in the facing H (direction 0 / 16); a direction change keeps the frame (Active::set_direction) */
static void sa(obj_t *o, int k) {
    if (!o) return;
    int row = PA[k][S.H < 0];
    if (row < 0 || (o->an.b == &G.ens && o->an.anim == row)) return;
    if (o->an.b == &G.ens && S.ak == k) { o->an.anim = row; return; }
    anim_set(&o->an, &G.ens, row); S.ak = k;
}
static void sframe(obj_t *o, int f) { if (o && o->an.b) { o->an.frame = f; o->an.done = 0; } }

void sbp_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    obj_t *m = mask();
    if (!m) return;
    for (int k = 0; k < P_NA; k++) {
        char nm[16]; PA[k][0] = (short)ens_anim(PN[k]); snprintf(nm, sizeof nm, "%s_l", PN[k]); PA[k][1] = (short)ens_anim(nm);
    }
    for (int k = 0; k < Q_N; k++) QA[k] = (short)ens_anim(QN[k]);
    S.A = m->x; S.B = m->y; S.E = m->y; S.F = -1; S.H = -1; S.M = 3; S.ak = -1;   /* RV p_kill f55: A / B = the mask, E = B, F -1, H -1, M 3 */
    S.sB = 1; S.hD = 3; S.f0 = G.frame;
    obj_t *s = spade(), *h = hitbox();
    if (s) s->hidden = 1;
    if (h) { h->A = 60; h->fl |= F_NOFLY; if (QA[Q_HIT] >= 0) anim_set(&h->an, &G.ens, QA[Q_HIT]); }   /* ctor A 60, D 3 */
    for (int k = 0; k < 4; k++) { obj_t *t = sbb_get(OT_SB_CH_TAIL, k); double A = sbb_count(OT_SB_CH_TAIL) - 1 - k; S.tB[k] = -32 - 16 * A; S.tC[k] = -24 - 16 * A; if (t) t->av[0] = A; }
    S.cL = 8; S.cM = -1;
}
void sbp_pre(player_t *p) { (void)p; }

/* ---- the cards (9712-9727; 9715 / 9717 / 9722 PlayerReflect and 9726 / 9727 player projectiles: Milla / Carol only) */
static void card_new(obj_t *s, double ax, double ay, double C, double D, int g) {
    if (S.ncard >= NCARD) return;
    obj_t *o = sbb_spawn(OT_SB_SP_CARD, -1, (float)ax, (float)ay, s->layer);
    if (!o) return;
    if (QA[Q_CARD] >= 0) anim_set(&o->an, &G.ens, QA[Q_CARD]);
    o->av[0] = ax; o->av[1] = ay; o->av[2] = C; o->av[3] = D; o->av[4] = g;     /* A / B position, C / D velocity, G (1: the rain) */
    S.card[S.ncard++] = (short)(o - G.o);
}
static void cards(player_t *p) {
    for (int k = 0; k < S.ncard; k++) {
        obj_t *o = sbb_slot(S.card[k]);
        if (!o || o->type != OT_SB_SP_CARD) { S.card[k--] = S.card[--S.ncard]; continue; }
        int g = o->av[4] == 1, kill = 0;
        o->x = (float)(int)o->av[0]; o->y = (float)(int)o->av[1];             /* 9712: set the position, then move */
        o->av[0] += o->av[2]; o->av[1] += o->av[3];
        o->av[5] += 1;                                                          /* 9713: H + 1 (F, the freeze, is never set) */
        if (!g && obj_overlaps_bg(o)) { snd_play_orig(446, 220, 128); obj_effect(o->x, o->y, "sbp_tspark"); kill = 1; }   /* 9714 / 9716 */
        if (!g && !kill && sbb_vulnerable(p) && sbb_csens_hit(p, o)) player_contact(p, 5);   /* 9718 */
        if (g) {                                                                /* 9719: anim 7 turned along its path, N + 1 */
            if (QA[Q_CARD7] >= 0) anim_set(&o->an, &G.ens, QA[Q_CARD7]);
            o->ang = (float)(-atan2(o->av[3], o->av[2]) * SB_RAD);
            o->av[6] += 1;
            if ((obj_overlaps_bg(o) && o->av[6] > 3) || px_platform(&G.lvl, (int)o->x, (int)o->y)) o->fl |= SB_DEF;   /* 9720 / 9721 (Q obstacles: platforms, INFERRED) */
            if (o->fl & SB_DEF) {                                               /* 9723 */
                snd_play_orig(240, 220, 128); G.cam.Z = fminf(G.cam.Z + 3, 3); obj_effect(o->x, o->y, "wburst"); kill = 1;
            } else if (sbb_vulnerable(p) && sbb_csens_hit(p, o)) player_contact(p, 10);   /* 9724 */
        }
        if (!kill && (o->x < G.camx - 100 || o->x > G.camx + SCREEN_W + 100 || o->y < G.camy - 100 || o->y > G.camy + SCREEN_H + 100)) kill = 1;   /* 9725 */
        if (kill) { o->alive = 0; S.card[k--] = S.card[--S.ncard]; }
    }
}
/* EffectsSpadeImage_456 (9661): Spade's frame, green, fading (the fade is not in the events: INFERRED 16 frames) */
static void img_new(const obj_t *s) {
    if (S.nimg >= NIMG || !s->an.b) return;
    obj_t *o = sbb_spawn(OT_SB_SP_IMG, -1, s->x, s->y, s->layer);
    if (!o) return;
    o->an = s->an; o->an.done = 1;                                              /* a still copy of the current frame */
    o->tm = 16; S.img[S.nimg++] = (short)(o - G.o);
}
static void imgs(void) {
    for (int k = 0; k < S.nimg; k++) {
        obj_t *o = sbb_slot(S.img[k]);
        if (!o || o->type != OT_SB_SP_IMG) { S.img[k--] = S.img[--S.nimg]; continue; }
        if (--o->tm <= 0) { o->alive = 0; S.img[k--] = S.img[--S.nimg]; }
    }
}

/* ---- the chopper as the carrier (9600-9611) */
static void chopper(void) {
    obj_t *c = sbb_get(OT_SB_CH, 0);
    if (!c) return;
    c->A = 99;                                                                  /* 9600 */
    S.cE += S.cG; S.cF += S.cH;
    c->x = (float)(int)S.cE; c->y = (float)(int)(sbb_sin(S.cI) * S.cL + S.cF); /* 9601 */
    obj_t *bl = sbb_get(OT_SB_CH_BLADE, 0); if (bl) { bl->x = c->x; bl->y = c->y; }
    int n = sbb_count(OT_SB_CH_TAIL); if (n > 4) n = 4;
    for (int k = 0; k < n; k++) {                                               /* 9602 */
        obj_t *t = sbb_get(OT_SB_CH_TAIL, k); if (!t) continue;
        double A = t->av[0];
        t->x = (float)(int)(c->x + S.tB[k]);
        t->y = (float)(int)(c->y + S.tC[k] - sbb_sin(S.cI) * S.cL + sbb_sin(S.cI - 20 - 20 * A) * S.cL);
        if (A == 3) {                                                           /* 9604: the tail blades at the last link */
            obj_t *b1 = sbb_get(OT_SB_CH_TBLADE, 0), *b2 = sbb_get(OT_SB_CH_TBLADE2, 0);
            if (b1) { b1->x = t->x; b1->y = t->y; }
            if (b2) { b2->x = t->x; b2->y = t->y; }
        }
    }
    sbc_pose_parts(S.cM > 0);                                                   /* 9605 / 9606: direction 0 / 16 */
    S.cI = sbb_fmod360(S.cI + 6);                                               /* 9607 (D -3, never 6 here) */
    obj_t *b; if ((b = sbb_get(OT_SB_CH_BARS, 0))) { b->x = c->x; b->y = c->y; } if ((b = sbb_get(OT_SB_CH_BARS2, 0))) { b->x = c->x; b->y = c->y; }
    for (int k = 0; k < n; k++) {                                               /* 9608 / 9609: the links trail behind */
        obj_t *t = sbb_get(OT_SB_CH_TAIL, k); if (!t) continue;
        double A = t->av[0];
        if (S.cM == -1 && S.tB[k] < A * 16 + 32) S.tB[k] += 8;
        if (S.cM == 1 && S.tB[k] > -32 - A * 16) S.tB[k] -= 8;
    }
    if (S.F != -1 && S.F != 9) {                                                /* 9610 / 9611: after the drop it flies off up-left */
        if (S.cF > 0) { S.cG = fmax(S.cG - 0.5, -15); S.cH = fmax(S.cH - 0.2, -15); }
        else S.cG = S.cH = 0;
    }
}
static void carried(obj_t *s) {                                                /* 9614 / 9693: hanging under the chopper */
    obj_t *c = sbb_get(OT_SB_CH, 0);
    sa(s, P_15);
    S.A = (c ? c->x : 0) + 10 * S.H; S.B = (c ? c->y : 0) + 68;
}
static void chopper_rise(double PX) {                                          /* 9616 / 9694: up at 8, E = min(X + 150, the right limit - 150) */
    S.cH = -8;
    float r = sbb_marker(OT_CAMLIM_R, 1);
    S.cE = fmin((int)PX + 150, r > -1e8f ? (int)r - 150 : 1e9);
}

void sbp_frame(player_t *p) {
    obj_t *m = mask(), *s = spade(), *h = hitbox();
    if (!m || !s || !h || G.character != CHR_LILAC) return;
    double PX = sbb_px(p), PY = sbb_py(p);
    if (s->an.b && s->an.done && !S.sdone) s->an.acc += 100 - (s->c > 0 ? s->c : s->an.b->anims[s->an.anim].speed);   /* Active::update keeps the counter when an animation ends (RE) */
    S.sdone = s->an.b && s->an.done;
    if (h->fl & SB_DEF) h->ky = fmin(h->ky + 0.25, 15.0);                       /* 2765: the beaten hitbox's fall speed (its position is 9701's) */
    int pdead = player_orig_m(p) == 9;
    /* 9598: Lilac (MV.A 0) inside the arena (the flow's lock 9552), barriers up */
    if (!S.on && sbb_lock_edge(&S.once) > 0 && sbb_bars() > 0) {
        S.on = 1; S.cM = -1; S.cF = 600 + S.B; S.cE = PX + 200;
    }
    if (!S.on) { cards(p); imgs(); return; }
    if (S.on == 1) {                                                            /* 9599 (once): shown, the stage music fades */
        S.on = 2; sbb_music_fadeout();
        int ts[7] = { OT_SB_CH, OT_SB_CH_BLADE, OT_SB_CH_TBLADE, OT_SB_CH_TBLADE2, OT_SB_CH_BARS, OT_SB_CH_BARS2, OT_SB_CH_TAIL };
        for (int j = 0; j < 7; j++) for (int k = 0; k < sbb_count(ts[j]); k++) { obj_t *o = sbb_get(ts[j], k); if (o) o->hidden = 0; }
        s->hidden = 0;
        /* BossSpade has played its anim 0 (speed 14) since the frame began; its counter carries into anim 15 (Active::
         * force_animation keeps it). RV p_kill: counter 20 / 34 / 48 / 62 at frames 58-61 = 88 + 14 n at the port's frame n */
        sa(s, P_15);
        int v = 88; for (int n = 0; n < G.frame - S.f0; n++) { v += 14; while (v > 100) v -= 100; }
        s->an.acc = v;
    }
    chopper();
    /* F -1 (0x228ea): carried in */
    if (S.F == -1) {
        carried(s);
        if (S.cF > S.E - 160) chopper_rise(PX);                                   /* 9616 */
        else { S.cH = 0; S.J += 1; sbb_fight_start(); }                         /* 9617 / 9618: ML.Y 1 (the boss track) */
        if (S.J > 80) { S.J = 0; S.F = 2; S.K = 9; S.I = -1; }                  /* 9620: dropped */
    }
    /* F 0 (0x228e5): running at the player */
    if (S.F == 0) {
        if (PX + 100 < S.A) S.I = -1;                                           /* 9622 / 9623 (BossSpade Q, the dialogue: always 0) */
        if (S.A < PX - 100) S.I = 1;
        if (pdead && fabs(S.A - PX) <= 99) { S.I = 0; S.N += 1;                 /* 9624: taunts the beaten player */
            if (S.N == 30) voice_play(pdead ? "spade_goodenough" : "spade_comeon");   /* 9625 / 9626 (channel 27) */
            if (S.N == 90) voice_play("lilac_hitmeifyoucan");                   /* 9627 */
            if (S.N == 400) voice_play("spade_comeon2");                        /* 9628 */
        }
        if (S.N == 400) S.N = -300;                                             /* 9628 */
        float cl = sbb_marker(OT_CAMLIM_L, 1);
        if (S.N != 0 && cl + 200 < PX) { S.N = 0; S.D = 0; S.J = 0; S.F = 3; S.L = 0; S.H = 1; S.C = 5; S.K += 1; }   /* 9629 */
        if (S.F == 0) {
            if (S.I == -1 && S.C <= 0 && S.C > -2) S.C = -2;                     /* 9630-9637: accelerate, turn, stop */
            if (S.I == -1 && S.C > 0) { S.H = -1; S.C = fmax(S.C - 0.2, -5); }
            if (S.I == -1) { S.C = fmax(S.C - 0.1, -5); S.H = -1; }
            if (S.I == 1 && S.C >= 0 && S.C < 2) S.C = 2;
            if (S.I == 1 && S.C < 0) { S.C = fmin(S.C + 0.2, 5); S.H = 1; }
            if (S.I == 1) { S.C = fmin(S.C + 0.1, 5); S.H = 1; }
            if (S.I == 0 && S.C < 0) S.C = fmin(S.C + 0.2, 0);
            if (S.I == 0 && S.C > 0) S.C = fmax(S.C - 0.2, 0);
            if (S.L == 0) S.L += 90;                                            /* 9638 / 9639 */
            if (S.L > 0) S.L += 1;
            if (S.L > 0 && (S.A < PX - 300 || S.A > PX + 300)) S.L += 4;       /* 9640: far away: sooner (RV p_kill 501 / 759) */
            if (S.L > 140 && !pdead && cl + 200 < PX) {                        /* 9641 / 9642: the jump */
                S.L = 0; S.F = 2; S.D = -8; snd_play_orig(8, 220, 128); S.H = PX <= S.A ? -1 : 1;
            }
        }
        if (S.B >= S.E && S.I == 0 && S.C == 0) { sa(s, P_0); }                /* 9643-9645 (anim 40, the dialogue, never) */
        else if (S.B >= S.E && fabs(S.C) > 0 && fabs(S.C) < 5) { sa(s, P_1); s->c = (int)(5 * fabs(S.C) + 14); }   /* force_speed (kept until 9648) */
        else if (S.B >= S.E && fabs(S.C) >= 5) { sa(s, P_2); s->c = (int)(2 * fabs(S.C) + 27); }
    }
    /* F 2 (0x228f2): in the air */
    if (S.F == 2) {
        sa(s, P_24); s->c = 0; S.D = fmin(S.D + 0.3, 15);                       /* 9648 (restore_speed) */
        if (S.I == 0) S.I = (sbb_rand(2) ? 1 : -1);                             /* 9649: RND (cross_seed bit 30) */
        if (S.I == -1) { S.C = fmax(S.C - 0.1, -5); S.H = -1; }                 /* 9650 / 9651 */
        if (S.I == 1) { S.C = fmin(S.C + 0.1, 5); S.H = 1; }
        if (S.M <= 0 && S.D >= -2 && !pdead) {                                  /* 9652 / 9653: the card rain */
            double c3 = 3 * S.H;
            S.J = 0; S.M = 1; S.F = 5; S.D = fmin(S.D, -5); S.H = PX <= S.A ? -1 : 1; S.C = c3;
        }
        if (S.F == 2 && S.D >= 0 && S.E <= S.B) {                               /* 9654 / 9655: landed: cards (near) or run */
            if (PX - 300 < S.A && S.A < PX + 300 && !pdead) {
                S.D = 0; S.B = S.E; S.F = 4; sa(s, P_0); sframe(s, 0); S.L = 0; S.K = 0; S.sB = 3; S.sA = 0; S.M -= 1;
            } else { S.F = 0; S.D = 0; S.B = S.E; sa(s, P_0); S.L = 0; S.K = 0; }
        }
        if (S.F == 2 && S.K < 2 && ((S.D >= -6 && PY > S.B && S.M > 0) || S.D >= -2)) {   /* 9656: the air dash (twice a jump) */
            S.F = 3; S.D = 0; sframe(s, 0); S.K += 1;
        }
    }
    /* F 3 (0x228f4): the air dash */
    if (S.F == 3) {
        S.J += 1;                                                               /* 9658 */
        if (S.J == 5) { obj_effect(s->x, s->y - 12, "explosion01"); snd_play_orig(155, 220, 128); }   /* 9659 */
        if (S.J > 5) { sa(s, P_29); S.C = 15 * S.H; }                           /* 9660 */
        if (S.J > 5 && fmod(S.J, 2) == 1) img_new(s);                           /* 9661 */
        if (S.J > 13) { sa(s, P_24); sframe(s, 3); S.F = 2; S.C = fmax(fmin(S.C, 5), -5); S.J = 0; }   /* 9662 */
    }
    /* F 4 (0x228ee): three cards */
    if (S.F == 4) {
        int thr = PA[P_26][S.H < 0] >= 0 && s->an.anim == PA[P_26][S.H < 0];
        if (!thr && S.sA <= 0) { S.H = PX <= S.A ? -1 : 1; sa(s, P_26); thr = 1; }   /* 9664 / 9665 */
        if (S.C < 0) S.C = fmin(S.C + 0.2, 0);                                  /* 9666 / 9667 */
        if (S.C > 0) S.C = fmax(S.C - 0.2, 0);
        int fr = s->an.frame;
        if (fr < 2) S.sA = 0;                                                   /* 9668 */
        double per = 3 - floor(0.25 * S.sB);
        if (thr && per != 0 && G.frame % (int)per == 0 && fr >= 2 && 10 * S.sB > S.sA) {   /* 9669 / 9670 (frame counter phase INFERRED) */
            snd_play_orig(71, 220, 128);
            double ang = S.H == 1 ? 2 * S.sB - S.sA : S.sA - 2 * S.sB, v = S.H == 1 ? 0.5 * S.C + 8 : -0.5 * S.C + 8;
            card_new(s, s->x + 19 * S.H, s->y - 6, v * sbb_cos(ang) * S.H, v * sbb_sin(ang) * S.H, 0);   /* the action point (anim 26: +-19, -6) */
            S.sA += 10;
        }
        if (thr && s->an.done) { sa(s, P_24); thr = 0; }                        /* 9671-9673: the throw is over */
        if (!thr) { sa(s, P_24); sframe(s, 6); S.F = 0; }                       /* 9674 */
    }
    /* F 5 (0x228e6): hovering 80 px beside the player, cards straight down at 45 degrees */
    if (S.F == 5) {
        sa(s, P_21); S.D = fmin(S.D + 0.3, 2); S.J += 1;                        /* 9676 */
        if (S.C >= 0 && PX - 80 * S.H < S.A) S.C = -S.C;                        /* 9677 / 9678 */
        else if (S.C < 0 && S.A < PX - 80 * S.H) S.C = -S.C;
        int fr = s->an.frame;
        if (fr != 0 && fr != 3) S.fl2 = 0;                                      /* 9679 */
        if ((fr == 0 || fr == 3) && !S.fl2) {                                   /* 9680: a card on frames 0 and 3 (RV p_kill 649-709) */
            S.fl2 = 1; card_new(s, s->x + 5 * S.H, s->y - 5, 8 * S.H, 8, 1); snd_play_orig(445, 220, 128);
        }
        if (S.J > 60 && fr > 6) { S.F = 2; S.J = 0; }                            /* 9681 */
    }
    /* F 6 / 7 (0x228e7): staggered (or beaten) */
    if (S.F >= 6 && S.F <= 7) {
        if (S.F == 6) {
            S.H = PX <= s->x ? -1 : 1;                                          /* 9683 / 9684 */
            sa(s, P_8); sframe(s, 0); S.sF = 0; S.F += 1; S.C = h->kx; S.D = h->ky;   /* 9685: the hit's knock-back */
        }
        if (S.F == 7) {
            S.sF += 1; S.D = fmin(S.D + 0.3, 15); h->c = 20;                    /* 9686 */
            int beaten = (h->fl & SB_DEF) != 0;
            if (!beaten) S.B = fmin(S.B, S.E);                                  /* 9687 */
            if (s->an.frame >= 7) sframe(s, 7);                                 /* 9688 */
            if (S.sF > 18 && S.B >= S.E && !beaten) {                           /* 9689: back to the cards */
                S.sF = 0; S.C = 0; S.D = 0; S.F = 4; S.L = 0; S.B = S.E; S.K = 0; S.M -= 1; sa(s, P_24);
            }
            if (S.sF > 18 && S.B >= S.E + 600 && beaten) {                     /* 9691: fallen off the ship: the chopper comes */
                sbb_music_fadeout();
                S.C = 0; S.D = 0; S.F = 9; S.cF = S.B; S.cE = PX + 200; S.H = -1; S.J = 0;
            }
        }
    }
    /* F 9 (0x228ec): carried away; the arena end */
    if (S.F == 9) {
        carried(s);
        if (S.J == 0) chopper_rise(PX);                                           /* 9694 */
        if (S.J == 0 && S.cF <= S.E - 160) { S.cH = 0; S.J += 1; }               /* 9695 */
        if (S.J > 0) S.J += 1;                                                  /* 9696 (the same frame too: RV p_kill 1079) */
        if (S.J > 60) S.cH = -8;                                                /* 9697 */
        if (S.J > 90 && !S.ended) { S.ended = 1; sbb_arena_end(1, 0x10); }      /* 9698 */
    }
    float cl = sbb_marker(OT_CAMLIM_L, 1), cr = sbb_marker(OT_CAMLIM_R, 1);
    if (cl > -1e8f && S.A < cl + 200) { S.I = 1; S.A = (int)cl + 200; }         /* 9699 / 9700 */
    if (cr > -1e8f && S.A > cr) { S.I = -1; S.A = (int)cr; }
    if (S.F > -1) { h->x = m->x; h->y = m->y; }                                 /* 9701 (before the move: one frame behind, RV) */
    s->inv = h->c > 1;                                                          /* 9703: the hit flash */
    if (h->alive && !(h->fl & SB_DEF) && h->A < S.hD * 20) {                                /* 9705 / 9706: a stagger per 20 HP */
        S.hD -= 1; S.F = 6; snd_play_orig(25, 220, 128); snd_play_orig(103, 220, 128);
    }
    if ((h->fl & SB_DEF) && S.F < 6) { S.hD -= 1; S.F = 6; snd_play_orig(156, 220, 128); sbb_music_fadeout(); }   /* 9707 */
    S.A += S.C; S.B += S.D;                                                     /* 9708 */
    m->x = (float)(int)S.A; m->y = (float)(int)S.B;
    if (S.H == 1 || S.H == -1) sa(s, S.ak >= 0 ? S.ak : P_15);                  /* 9709 / 9710: direction 0 / 16 */
    s->x = m->x; s->y = m->y;                                                   /* 9711 */
    cards(p); imgs();
    m->av[0] = S.F; m->av[1] = S.A; m->av[2] = S.B; m->av[3] = S.C; m->av[4] = S.D; m->av[5] = S.J; m->av[6] = S.L; m->av[7] = S.M;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_SBP")) printf("sbp %d F %.0f A %.1f B %.1f C %.2f D %.2f H %.0f I %.0f J %.0f K %.0f L %.0f M %.0f N %.0f sA %.0f an %d fr %d acc %d hp %.0f hD %.0f cE %.1f cF %.1f nc %d\n",
        G.frame, S.F, S.A, S.B, S.C, S.D, S.H, S.I, S.J, S.K, S.L, S.M, S.N, S.sA, S.ak, s->an.frame, s->an.acc, h->A, S.hD, S.cE, S.cF, S.ncard);
#endif
}
int sbp_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_SB_SPADE || o->type == OT_SB_SP_CARD) { sbb_draw(o, 0xffffffff); return 1; }
    if (o->type == OT_SB_SP_IMG) { if (o->an.b) sbb_draw(o, ((uint32_t)(o->tm * 10) << 24) | 0x60ff60); return 1; }
    return o->type == OT_SB_SP_MASK || o->type == OT_SB_SP_HIT;
}
