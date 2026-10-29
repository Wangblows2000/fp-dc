/* SB2 boss for Carol / Milla: the chopper (BossChopper_2216 + BossChopperBlade_2217, four BossChopperTail_2215 links with
 * BossChopperTailBlade_2218 / TailBlade2_2214 at the end, BossChopperBars_2212 / Bars2_2213), its BossMissile_2219 and the
 * MinibossTreddBomb_1060 bombs (the Tredd's bomb rules, bosses.c OT_TD_BOMB).
 * RE handle_frame_37_events group 0x228f5, events 9553 (trigger, MV.A != 0), 9555-9597, the missile events 9728-9736 and
 * foreach_m_2219_2_36; RV traces c_idle / c_kill (build/orig_traces/sb_boss). spec_sb_bosses Part B.
 * Chopper alterables: A HP 70 (Q20), D state (-3..-1 missiles, 0 hover, 1 swoop, 2 bomb run, 6 defeated), E / F position,
 * G / H velocity, I rotor bob phase (+6 per frame), J timer, K tilt (always 0 in the traces), L bob amplitude 8, M facing
 * (1 right, -1 left), N the swoop's return height, flag 2 the next attack after a swoop (clear: missiles), flag 4 the entry. */
#include "sb_boss.h"
#include <stdio.h>
#include <stdlib.h>

enum { C_CH, C_CHL, C_CH8, C_CH8L, C_BLADE, C_BLADEL, C_TAIL, C_TAILL, C_TB, C_TBL, C_TB2, C_TB2L, C_BARS, C_BARSL, C_BARS2, C_BARS2L,
       C_MIS, C_MISL, C_MIS8, C_TDBOMB, C_N };
static const char *const CN[C_N] = { "sbc_ch", "sbc_ch_l", "sbc_ch8", "sbc_ch8_l", "sbc_blade", "sbc_blade_l", "sbc_tail", "sbc_tail_l", "sbc_tb", "sbc_tb_l",
    "sbc_tb2", "sbc_tb2_l", "sbc_bars", "sbc_bars_l", "sbc_bars2", "sbc_bars2_l", "sbc_mis", "sbc_mis_l", "sbc_mis8", "td_bomb" };
static short CA[C_N];
static void ca(obj_t *o, int k) { if (o && CA[k] >= 0 && (o->an.b != &G.ens || o->an.anim != CA[k])) anim_set(&o->an, &G.ens, CA[k]); }

#define NTAIL 4
#define NMIS 8
static struct {
    int on, once;
    double D, E, F, G, H, I, J, K, L, M, N; int fl2, fl4;
    double tB[NTAIL], tC[NTAIL]; int tfl[NTAIL];
    double tbB[2], tbC[2], brB[2], brC[2];           /* flying tail blades / bars (9594 / 9595) */
    short mis[NMIS]; int nmis; double mG[NMIS], mF[NMIS], mV[NMIS], mx[NMIS], my[NMIS], mE[NMIS]; int mfl[NMIS];
    int ended;
    int bg2;                                         /* Battle Glacier 2's variant (stage 9: handle_frame_61 14229-14239, session 24) */
} C;
static obj_t *ch(void) { return sbb_get(OT_SB_CH, 0); }
static obj_t *tail(int k) { return sbb_get(OT_SB_CH_TAIL, k); }
int sbc_active(void) { return C.on; }

void sbc_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&C, sizeof C); }
    memset(&C, 0, sizeof C);
    obj_t *c = ch();
    if (!c) return;
    for (int k = 0; k < C_N; k++) CA[k] = (short)ens_anim(CN[k]);
    C.bg2 = level_cur() && level_cur()->stage == 9;                           /* the stage index, not a level name */
    c->A = 70; c->fl |= F_NOFLY; c->hidden = 1;                                 /* ctor A 70, D -3, L 8, M -1; shown by 9555 */
    C.D = -3; C.L = 8; C.M = -1; C.E = c->x; C.F = c->y + 100; C.N = c->y;     /* RV c_idle f0: E = X, F = Y + 100, N = Y */
    int n = sbb_count(OT_SB_CH_TAIL);
    for (int k = 0; k < n && k < NTAIL; k++) { obj_t *t = tail(k); if (t) { t->av[0] = n - 1 - k; C.tB[k] = -32 - 16 * t->av[0]; C.tC[k] = -24 - 16 * t->av[0]; t->hidden = 1; } }   /* A from the last created; B / C (RV c_idle / p_kill f0) */
    obj_t *o;
    if ((o = sbb_get(OT_SB_CH_BLADE, 0))) o->hidden = 1;
    if ((o = sbb_get(OT_SB_CH_TBLADE, 0))) o->hidden = 1;
    if ((o = sbb_get(OT_SB_CH_TBLADE2, 0))) o->hidden = 1;
    if ((o = sbb_get(OT_SB_CH_BARS, 0))) o->hidden = 1;
    if ((o = sbb_get(OT_SB_CH_BARS2, 0))) o->hidden = 1;
}
void sbc_pre(player_t *p) { (void)p; }
/* the parts' directions 0 / 16 (9561 / 9562; Spade's carrier 9602-9607 in sb_spade.c) */
void sbc_pose_parts(int right) {
    obj_t *c = ch(); if (!c) return;
    ca(c, right ? C_CH : C_CHL);
    ca(sbb_get(OT_SB_CH_BLADE, 0), right ? C_BLADE : C_BLADEL); ca(sbb_get(OT_SB_CH_BARS, 0), right ? C_BARS : C_BARSL);
    ca(sbb_get(OT_SB_CH_BARS2, 0), right ? C_BARS2 : C_BARS2L); ca(sbb_get(OT_SB_CH_TBLADE, 0), right ? C_TB : C_TBL);
    ca(sbb_get(OT_SB_CH_TBLADE2, 0), right ? C_TB2 : C_TB2L);
    for (int k = 0; k < sbb_count(OT_SB_CH_TAIL) && k < NTAIL; k++) ca(tail(k), right ? C_TAIL : C_TAILL);
}

static void missile_new(obj_t *c, int m) {                                      /* 9569 / 9570: direction 28 / 20, F = M, speed 50 */
    if (C.nmis >= NMIS) return;
    obj_t *o = sbb_spawn(OT_SB_MISSILE, m > 0 ? C_MIS : C_MISL, c->x, c->y + 38, c->layer);   /* + 38: RV c_idle 2065 / 2066 */
    if (!o) return;
    ca(o, m > 0 ? C_MIS : C_MISL);
    int k = C.nmis++; C.mis[k] = (short)(o - G.o);
    C.mG[k] = m > 0 ? 28 : 20; C.mF[k] = m; C.mV[k] = 6.25; C.mx[k] = o->x; C.my[k] = o->y; C.mE[k] = 0; C.mfl[k] = 0;
}
static int probe_ground(const obj_t *o) {                                       /* 9729-9732: y + 30 on the backdrop or a Q8 obstacle */
    obj_t t = *o; t.y += 30;
    if (obj_overlaps_bg(&t)) return 1;
    int ox = (int)t.x, oy = (int)t.y;
    return px_platform(&G.lvl, ox, oy) || px_platform(&G.lvl, ox, oy + 4);
}
static void missiles(player_t *p) {
    for (int k = 0; k < C.nmis; k++) {
        obj_t *o = sbb_slot(C.mis[k]);
        if (!o || o->type != OT_SB_MISSILE) {
            C.nmis--; C.mis[k] = C.mis[C.nmis]; C.mG[k] = C.mG[C.nmis]; C.mF[k] = C.mF[C.nmis]; C.mV[k] = C.mV[C.nmis]; C.mx[k] = C.mx[C.nmis];
            C.my[k] = C.my[C.nmis]; C.mE[k] = C.mE[C.nmis]; C.mfl[k] = C.mfl[C.nmis]; k--; continue;
        }
        if (!(C.mfl[k] & SB_DEF)) {
            int dir = ((int)(C.mG[k] + 0.5)) & 31;
            if (dir != 0 && dir != 16 && probe_ground(o)) { C.mG[k] = fmod(0.4 * C.mF[k] + C.mG[k] + 32, 32); dir = ((int)(C.mG[k] + 0.5)) & 31; }   /* 9730 / 9731 */
            if (dir == 0 || dir == 16) C.mV[k] += 0.5;                          /* 9733: levelled off, faster (RV: +0.5 px a frame) */
            double a = dir * 11.25;
            C.mx[k] += C.mV[k] * sbb_cos(a); C.my[k] -= C.mV[k] * sbb_sin(a);
            o->x = (float)(int)C.mx[k]; o->y = (float)(int)C.my[k];
            ca(o, dir > 8 && dir < 24 ? C_MISL : C_MIS);
            if (sbb_csens_hit(p, o) || obj_overlaps_bg(o)) { C.mfl[k] |= SB_DEF; snd_play_orig(38, 220, 128); ca(o, C_MIS8); o->hidden = 1; }   /* (INFERRED trigger; RV sound 38) */
            else if (o->x < G.camx - 300 || o->x > G.camx + SCREEN_W + 300 || o->y > G.camy + SCREEN_H + 300 || o->y < G.camy - 300) { o->alive = 0; continue; }   /* 9736 */
        }
        if (C.mfl[k] & SB_DEF) {                                                /* 9734 + foreach_m_2219: 45 explosions on a spiral */
            if (C.mE[k] < 45) {
                double e = C.mE[k], ex = 2 * e == 180 ? 8 : sbb_sin(2 * e) * 40 + 8;
                obj_effect((float)(int)(ex * sbb_sin(132 * e) + o->x), (float)(int)(o->y - sbb_sin(5 * e) * 80), "explosion06");
                C.mE[k] += 1;
            } else { o->alive = 0; continue; }
            if (C.mE[k] < 30 && sbb_csens_hit(p, o) && sbb_vulnerable(p)) {     /* 9735: anim 8 and E < 30: 20 damage, knocked up */
                player_contact(p, 20); p->ground = 0; p->vy = -8; p->y -= 4;
            }
        }
    }
}
static void tails_frame(obj_t *c) {
    int n = sbb_count(OT_SB_CH_TAIL); if (n > NTAIL) n = NTAIL;
    for (int k = 0; k < n; k++) {
        obj_t *t = tail(k); if (!t) continue;
        double A = t->av[0];
        if (C.M == -1 && C.tB[k] < A * 16 + 32) C.tB[k] += 8;                   /* 9564 / 9565 (RV: the tail trails behind) */
        if (C.M == 1 && C.tB[k] > -32 - A * 16) C.tB[k] -= 8;
        if (C.tfl[k] & SB_DEF) continue;
        t->x = (float)(int)(c->x + C.tB[k]);                                    /* 9558 */
        t->y = (float)(int)(c->y + C.tC[k] - sbb_sin(C.I) * C.L + sbb_sin(C.I - 20 - 20 * A) * C.L);
        ca(t, C.M > 0 ? C_TAIL : C_TAILL);
    }
}
static obj_t *tail_a(int A) { for (int k = 0; k < sbb_count(OT_SB_CH_TAIL) && k < NTAIL; k++) { obj_t *t = tail(k); if (t && (int)t->av[0] == A) return t; } return NULL; }
static int tail_k(int A) { for (int k = 0; k < sbb_count(OT_SB_CH_TAIL) && k < NTAIL; k++) { obj_t *t = tail(k); if (t && (int)t->av[0] == A) return k; } return -1; }

void sbc_frame(player_t *p) {
    obj_t *c = ch();
    /* 9553: MV.A != 0 (Lilac meets Spade: sb_spade.c); BG2 (14229): MV.A != 4 (Lilac / Carol; Milla rides another route) */
    if (!c || (C.bg2 ? chr_is_milla(G.character) : G.character == CHR_LILAC)) { if (c) missiles(p); return; }
    double PXv = sbb_px(p), PYv = sbb_py(p);
    /* 9553: the trigger (Carol / Milla; the arena test of the flow's lock 9552); BG2 14229: MV.X >= 2201, g4 < 2, once */
    if (!C.on && !C.bg2 && sbb_lock_edge(&C.once) > 0 && sbb_bars() > 0) C.on = 1;
    if (!C.on && C.bg2 && !C.once && PXv >= 2201 && G.at_g4 < 2) { C.once = 1; C.on = 1; }
    obj_t *blade = sbb_get(OT_SB_CH_BLADE, 0), *tb = sbb_get(OT_SB_CH_TBLADE, 0), *tb2 = sbb_get(OT_SB_CH_TBLADE2, 0), *bars = sbb_get(OT_SB_CH_BARS, 0), *bars2 = sbb_get(OT_SB_CH_BARS2, 0);
    if (C.on) {
        if (C.on == 1) {                                                        /* 9555 (once): D 1, G 6, M 1, E = VIEWX - 200, shown, flag 4; ML.Y 1 */
            C.on = 2; C.D = 1; C.G = 6; C.M = 1; C.E = (int)G.camx - 200; C.fl4 = 1; c->hidden = 0;
            for (int k = 0; k < NTAIL; k++) { obj_t *t = tail(k); if (t) t->hidden = 0; }
            obj_t *parts[5] = { blade, tb, tb2, bars, bars2 };
            for (int k = 0; k < 5; k++) if (parts[k]) parts[k]->hidden = 0;
            if (!C.bg2) sbb_fight_start();                                     /* BG2 14230: no ML.Y (the chase keeps the stage track) */
        }
        C.E += C.G; C.F += C.H;                                                 /* 9556 */
        c->x = (float)(int)C.E; c->y = (float)(int)(sbb_sin(C.I) * C.L + C.F);   /* 9557 */
        if (blade) { blade->x = c->x; blade->y = c->y; }
        tails_frame(c);                                                         /* 9558 / 9564 / 9565 */
        obj_t *te = tail_a(3);                                                  /* 9560: the tail blades at the last link */
        if (te && !(C.tfl[tail_k(3)] & SB_DEF)) {
            if (tb) { tb->x = te->x; tb->y = te->y; }
            if (tb2) { tb2->x = te->x; tb2->y = te->y; }
        }
        if (C.D != 6) {                                                         /* 9563: rotor phase, the bars at the chopper */
            C.I = sbb_fmod360(C.I + 6);
            if (bars) { bars->x = c->x; bars->y = c->y; }
            if (bars2) { bars2->x = c->x; bars2->y = c->y; }
        }
        int right = C.M > 0;                                                    /* 9561 / 9562: direction 0 / 16 */
        if (C.D == 6) ca(c, right ? C_CH8 : C_CH8L); else ca(c, right ? C_CH : C_CHL);
        ca(blade, right ? C_BLADE : C_BLADEL); ca(bars, right ? C_BARS : C_BARSL); ca(bars2, right ? C_BARS2 : C_BARS2L);
        if (!(C.D == 6 && C.J >= 3)) { ca(tb, right ? C_TB : C_TBL); ca(tb2, right ? C_TB2 : C_TB2L); }
        if (C.D <= 0) {                                                         /* 9566 / 9567: hold 150 px off the player's side */
            double tx = C.M == 1 ? PXv - 150 : PXv + 150;
            if (C.E < tx) C.G = fmin(C.G + 0.2, 3); else C.G = fmax(C.G - 0.2, -3);
            C.J += 1;                                                           /* 9568 */
        }
        if (C.D < 0 && C.J > 20) { missile_new(c, (int)C.M); C.J = 10; C.D += 1; snd_play_orig(195, 220, 128); }   /* 9569 / 9570 */
        if (C.D == 0 && C.J > 80) {                                             /* 9571 / 9572: the swoop */
            int r = PXv > C.E;
            C.J = 0; C.M = r ? 1 : -1; C.H = 7; C.D += 1; C.N = C.F;
            C.G = r ? fmin((PXv + 100 - C.E) * 0.03, 5) : fmax((PXv - 100 - C.E) * 0.03, -5);
        }
        if (C.D == 1 && C.fl4 && C.E < PXv - 50) C.J = 2;                      /* 9573: the entry flies level up to the player */
        if (C.D == 1 && C.J > 0) C.J -= 1;                                      /* 9574 */
        else if (C.D == 1) { C.fl4 = 0; C.H = fmax(C.H - 0.2, -4); }            /* 9575 */
        if (C.D == 1 && C.H > -1 && C.H <= 0 && C.F < PYv - 100) C.N += 300;    /* 9576 */
        if (C.D == 1 && C.H < 0 && C.N >= C.F && C.F > PYv + 30) C.N -= 200;    /* 9577 */
        if (C.D == 1 && C.H < 0 && C.F <= C.N) {                                /* 9578-9581: back up: missiles (flag 2 clear) or a bomb run */
            int left = C.E >= PXv;
            if (!C.fl2) { C.G = left ? 3 : -3; C.D = -3; C.M = left ? -1 : 1; }
            else { C.G = left ? -4 : 4; C.D = 2; C.M = left ? -1 : 1; }
            C.fl2 ^= 1; C.H = 0;
        }
        if (C.D == 2) C.J += 1;                                                 /* 9582 */
        if (C.bg2 && C.D == 2 && (obj_overlaps_bg(c) || px_solid(&G.lvl, (int)c->x, (int)c->y + 32))) C.F -= 3;   /* BG2 14231: climbs over walls */
        if (C.D == 2 && (C.J == 20 || C.J == 40 || C.J == 60 || C.J == 80 || C.J == 100) && !(C.bg2 && obj_overlaps_bg(c))) {   /* 9583 (BG2 14232: not in a wall) */
            obj_t *b = obj_alloc();
            if (b) { memset(b, 0, sizeof *b); b->type = OT_TD_BOMB; b->alive = 1; b->hp = 1; b->x = c->x; b->y = c->y + 24; b->fy = b->y; b->layer = c->layer; ca(b, C_TDBOMB); }
            C.J += 1;
        }
        if (C.D == 2 && C.J == 120) { C.G = 0; C.M = -C.M; }                    /* 9584 */
        if (C.D == 2 && C.J > 160) C.D = 0;                                     /* 9585 */
        if ((c->fl & SB_DEF) && C.D != 6) {                                     /* 9586: the defeat (BG2 14233: no music change) */
            C.D = 6; C.G = 0; C.H = 1; C.J = 0; if (!C.bg2) sbb_music_fadeout();
        }
        if (C.D == 6 && C.J < 15 && G.frame % 7 == 0) {                         /* 9588 (g3 % 7) */
            C.J += 1;
            obj_effect((float)((int)c->x - 40 + sbb_rand(81)), (float)((int)c->y - 40 + sbb_rand(81)), "explosion06");
            static const int SX[3] = { 311, 319, 320 }; snd_play_orig(SX[sbb_rand(3)], 220, 128);   /* RV c_kill */
        }
        if (C.D == 6) for (int A = 3; A >= 0; A--) {                            /* 9589-9592: the links blow up at J 3 / 6 / 9 / 12 */
            int k = tail_k(A); obj_t *t = tail_a(A);
            if (!t || k < 0 || (C.tfl[k] & SB_DEF) || C.J != 3 * (4 - A)) continue;
            C.tfl[k] |= SB_DEF; t->hidden = 1; obj_effect(t->x, t->y, "explosion01"); snd_play_orig(38, 220, 128);
            if (A == 3) { C.tbB[0] = -6 * C.M; C.tbB[1] = 6 * C.M; }           /* 9589: the tail blades fly off */
        }
        if (C.D == 6 && C.J == 15 && C.brB[0] == 0) { C.brB[0] = -6 * C.M; C.brB[1] = 6 * C.M; snd_play_orig(38, 220, 128); }   /* 9593 */
        if (C.tbB[0] != 0) {                                                    /* 9594 */
            if (tb) { tb->x = (float)(int)(tb->x + C.tbB[0]); tb->y = (float)(int)(tb->y + C.tbC[0]); } C.tbC[0] += 0.4;
            if (tb2) { tb2->x = (float)(int)(tb2->x + C.tbB[1]); tb2->y = (float)(int)(tb2->y + C.tbC[1]); } C.tbC[1] += 0.4;
        }
        if (C.brB[0] != 0) {                                                    /* 9595 */
            if (bars) { bars->x = (float)(int)(bars->x + C.brB[0]); bars->y = (float)(int)(bars->y + C.brC[0]); } C.brC[0] += 0.4;
            if (bars2) { bars2->x = (float)(int)(bars2->x + C.brB[1]); bars2->y = (float)(int)(bars2->y + C.brC[1]); } C.brC[1] += 0.4;
        }
        if (C.D == 6 && C.J >= 15) {                                            /* 9596: flies away */
            C.G = fmax(fmin(C.M * 0.3 + C.G, 12), -7); C.H = fmax(C.H - 0.1, -7); C.J += 1;
        }
        if (C.D == 6 && C.J == 50 && !C.ended && !C.bg2) { C.ended = 1; sbb_arena_end(1, 0x10); }   /* 9597 (BG2: no arena) */
        if (C.bg2 && C.D == 6 && C.J >= 3 && G.character == CHR_LILAC && !C.ended) { C.ended = 1; voice_play("lilac_awesome"); }   /* 14238 (INFERRED: the flag-9 timer of 14236 / 14237 not ported) */
        if (C.bg2) C.E = fmin(fmax(C.E, G.camx - 300), G.camx + 726);                     /* 14239: kept near the view (the chase) */
    }
    missiles(p);
    c->av[0] = C.D; c->av[1] = C.E; c->av[2] = C.F; c->av[3] = C.G; c->av[4] = C.H; c->av[5] = C.J; c->av[6] = C.M; c->av[7] = C.on;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_SBC")) printf("sbc %d on %d D %.0f E %.1f F %.1f G %.2f H %.2f I %.0f J %.0f M %.0f N %.0f x %.0f y %.0f A %.0f nm %d\n", G.frame, C.on, C.D, C.E, C.F, C.G, C.H, C.I, C.J, C.M, C.N, c->x, c->y, c->A, C.nmis);
#endif
}
int sbc_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type >= OT_SB_CH && o->type <= OT_SB_MISSILE) { sbb_draw(o, 0xffffffff); return 1; }
    return 0;
}
