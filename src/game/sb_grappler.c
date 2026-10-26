/* SB4 boss: the grappler (BossGrappler_2331 with BossGrapplerHandLeft_2330 / HandRight_2329 on the two ropes and four
 * BossGrapplerArmLeft_2327 / ArmRight_2328 links each) and the crystal spire (BossCrystalSpire_2333, its CrystalPlatform_2334,
 * BossCrystalSpikeSpawner1_2325 / 2_2326 -> BossCrystalSpike_2324), BossHomingSpark_2332 shots, the EffectWarning_1528 marker.
 * RE handle_frame_39_events groups 0x2298b (+ 0x22989 climb, 0x2298a attacks, 0x22988 rig) events 10078-10140, the
 * collision handlers col_7_38 / col_13_38 / col_38_38 (sparks), col_12_38 / col_18_38 (spawner and EnemyLimiter), col_30_38
 * (the spire landing), the start events 10151 (link index A, flag 16) and on_frame_39_start (spawner home A / B).
 * RV trace g_idle / g_kill (build/orig_traces/sb_boss). Everyone fights it (no MV.A test). spec_sb_bosses Part D.
 * Grappler alterables: A HP (1: every hit is a stage), C the shared hit timer (60 after a stage), D phase 0-4, E stages taken
 * (5: defeated), F climb speed (+-4), G depth below the start (+ down), H direction (1 down, -1 up), I phase timer, J hop
 * offset, K / L the fall after the defeat, flag 2 the moving hand (clear: left). Spire A: 0 carried, 1 dropping, 2 landed, 3 rising. */
#include "sb_boss.h"
#include <stdio.h>
#include <stdlib.h>

enum { G_BODY, G_BODY1, G_BODY2, G_BODY7, G_BODY7_1, G_BODY7_2, G_BODY8, G_HL, G_HL7, G_HR, G_HR7, G_AL, G_AL7, G_AR, G_AR7,
       G_SPIRE, G_SPIKE, G_SPARK, G_SPAWN, G_PLAT, G_WARN, G_N };
static const char *const GN[G_N] = { "sbg_gr", "sbg_gr1", "sbg_gr2", "sbg_gr7", "sbg_gr71", "sbg_gr72", "sbg_gr8", "sbg_hl", "sbg_hl7", "sbg_hr", "sbg_hr7",
    "sbg_al", "sbg_al7", "sbg_ar", "sbg_ar7", "sbg_spire", "sbg_spike", "sbg_spark", "sbg_spawn", "sbg_plat", "sbg_warn" };
static short GA[G_N];
static void ga(obj_t *o, int k) { if (o && GA[k] >= 0 && (o->an.b != &G.ens || o->an.anim != GA[k])) anim_set(&o->an, &G.ens, GA[k]); }

#define NLINK 4
#define NSPK 64
#define NSPR 16
static struct {
    int on;                                     /* group 0x2298b */
    int sub;                                    /* subgroups 0x22989 / 0x2298a / 0x22988 (one switch: they go off together, 10103) */
    int start_once;
    double D, E, F, G, H, I, J, K, L; int fl2, dir, hurt;
    double sA;                                  /* the spire's A */
    int warn_vis;
    double spC[2], spA[2], spB[2]; int spfl[2]; /* spawners: C counter, home A / B, flags 2 / 4 / 8 */
    short spike[NSPK]; int nspike;
    short spark[NSPR]; int nspark;
    double kA[NSPR], kB[NSPR], kC[NSPR], kD[NSPR], kE[NSPR], kF[NSPR];   /* sparks' alterables, by slot of spark[] */
    int ended, parts_gone;
} R;
static short lims[8]; static int nlims;
static obj_t *body(void) { return sbb_get(OT_SB_GR, 0); }
static obj_t *hand(int r) { return sbb_get(r ? OT_SB_GR_HANDR : OT_SB_GR_HANDL, 0); }
static obj_t *arm(int r, int k) { return sbb_get(r ? OT_SB_GR_ARMR : OT_SB_GR_ARML, k); }
static obj_t *spire(void) { return sbb_get(OT_SB_SPIRE, 0); }
static obj_t *plat(void) { return sbb_get(OT_SB_CPLAT, 0); }
static obj_t *spawner(int k) { return sbb_get(k ? OT_SB_SPAWN2 : OT_SB_SPAWN1, 0); }
static obj_t *warn(void) { return sbb_get(OT_SB_WARNING, 0); }

void sbg_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&R, sizeof R); }
    memset(&R, 0, sizeof R); nlims = 0;
    obj_t *b = body();
    if (!b) return;
    for (int k = 0; k < G_N; k++) GA[k] = (short)ens_anim(GN[k]);
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_LIMITER && nlims < 8) lims[nlims++] = (short)i;
    ga(b, G_BODY); b->A = 1; b->fl |= F_NOFLY; R.H = 1;                         /* ctor A 1, H 1; 10151 flag 16 */
    ga(hand(0), G_HL); ga(hand(1), G_HR);
    for (int r = 0; r < 2; r++) for (int k = 0; k < NLINK; k++) { obj_t *a = arm(r, k); if (a) { ga(a, r ? G_AR : G_AL); a->av[0] = sbb_count(r ? OT_SB_GR_ARMR : OT_SB_GR_ARML) - 1 - k; } }   /* 10151: A = index from the last created */
    ga(spire(), G_SPIRE); ga(plat(), G_PLAT); ga(warn(), G_WARN);
    for (int k = 0; k < 2; k++) { obj_t *s = spawner(k); if (s) { ga(s, G_SPAWN); R.spA[k] = s->x; R.spB[k] = s->y; } }   /* on_frame_39_start: A = X, B = Y */
    obj_t *w = warn(); if (w) w->hidden = 1;
    obj_t *pl = plat(); if (pl) pl->fl |= 1;                                     /* 10122 sets flag 0 every frame while the spire is carried */
}
static void spire_landing(void) {                                               /* col_30_38: the spire overlaps the backdrop (test_collisions,
                                                                                 * the frame after the move). The decks are Q8 objects in the original
                                                                                 * (baked into the port's map): its backdrop starts 16 px under the deck
                                                                                 * (RV g_idle: lands at y 972 (tip 1001, not 991) and at 980 (tip 1009,
                                                                                 * not 999): its top is y 1000) */
    obj_t *s = spire(), *pl = plat(); int landed = 0;
    if (!s || R.sA != 1 || !s->an.b) return;
    const frame_t *sf = anim_frame(&s->an); int bx = (int)s->x - sf->hx, by = (int)s->y - sf->hy;
    for (int yy = sf->h - 1; yy >= 0 && !landed; yy--) for (int xx = 0; xx < sf->w; xx++) if (frame_mask_px(s->an.b, sf, xx, yy) && px_solid(&G.lvl, bx + xx, by + yy - 16)) { landed = 1; break; }
    if (!landed) return;
    R.sA = 2; R.spfl[0] |= 2; R.spfl[1] |= 2; snd_play_orig(13, 220, 128);
    if (G.cam.Z < 5) G.cam.Z = 5;
    R.warn_vis = 0; if (pl) pl->fl &= ~1;
}
void sbg_pre(player_t *p) {                                                     /* test_collisions: sparks (col_7_38 / col_13_38 / col_38_38) */
    if (!body()) return;
    spire_landing();
    for (int k = 0; k < R.nspark; k++) {
        obj_t *s = sbb_slot(R.spark[k]);
        if (!s || s->type != OT_SB_SPARK) continue;
        int kill = 0;
        if (player_cube_state() >= 4 && player_cube_hits(s)) kill = 1;         /* col_7_38: PlayerReflect_463 (Milla's cube) with A >= 4 */
        else if (sbb_p326_hit(s)) {                                            /* col_13_38: Player (5 damage unless OTHERS.B == 3, not modelled) */
            if (sbb_vulnerable(p)) player_contact(p, 5);
            kill = 1;
        } else if (obj_overlaps_bg(s)) kill = 1;                               /* col_38_38 */
        if (kill) { obj_effect(s->x, s->y, "wburst"); snd_play_orig(45, 220, 128); s->alive = 0; }
    }
}
static void link_rig(void) {                                                    /* 10100: links between the body's side and the hand */
    obj_t *b = body(); if (!b || !b->an.b) return;
    const frame_t *f = anim_frame(&b->an);
    int bl = (int)b->x - f->hx, br = bl + f->w;
    for (int r = 0; r < 2; r++) {
        obj_t *h = hand(r); int n = sbb_count(r ? OT_SB_GR_ARMR : OT_SB_GR_ARML);
        if (!h || n < 1) continue;
        for (int k = 0; k < n && k < NLINK; k++) {
            obj_t *a = arm(r, k); if (!a) continue;
            double A = a->av[0];
            a->x = (float)(int)(((int)h->x * A + (r ? br : bl) * (n - A)) / n);
            a->y = (float)(int)(((int)h->y * A + (int)b->y * (n - A)) / n);
        }
    }
    obj_t *s = spire(), *pl = plat();
    if (s && pl && s->an.b) { const frame_t *sf = anim_frame(&s->an); pl->x = (float)((int)s->x - sf->hx); pl->y = (float)((int)s->y - sf->hy); }
}
static void parts_anim(int a7) {
    ga(hand(0), a7 ? G_HL7 : G_HL); ga(hand(1), a7 ? G_HR7 : G_HR);
    for (int r = 0; r < 2; r++) for (int k = 0; k < NLINK; k++) ga(arm(r, k), a7 ? (r ? G_AR7 : G_AL7) : (r ? G_AR : G_AL));
}
static void body_anim(obj_t *b) {
    static const int D0[3] = { G_BODY, G_BODY1, G_BODY2 }, D7[3] = { G_BODY7, G_BODY7_1, G_BODY7_2 };
    if (R.E >= 5) ga(b, G_BODY8); else ga(b, R.hurt ? D7[R.dir] : D0[R.dir]);
}
static void spark_new(obj_t *b, int s) {                                        /* 10087 / 10088 */
    if (R.nspark >= NSPR) return;
    float x = (float)((int)b->x + 20 * s), y = (float)((int)b->y - 20);
    obj_t *o = sbb_spawn(OT_SB_SPARK, -1, x, y, b->layer); if (!o) return;
    ga(o, G_SPARK);
    int k = R.nspark++; R.spark[k] = (short)(o - G.o);
    R.kA[k] = x; R.kB[k] = y; R.kC[k] = 6 * s; R.kD[k] = -6; R.kE[k] = 0; R.kF[k] = 0;
}
static void spike_new(obj_t *sp, int k, int up) {                               /* 10130 / 10131 / 10134 / 10135 */
    obj_t *o = R.nspike < NSPK ? sbb_spawn(OT_SB_SPIKE, -1, sp->x, sp->y, sp->layer) : NULL;
    int ang;
    if (!up) { ang = sbb_rand(61) - 30; }
    else ang = k ? sbb_rand(61) + 60 : sbb_rand(61) + 240;
    int r = sbb_rand(10);
    if (!up) { if (k) sp->x += 24; else sp->x -= 24; if (o) o->y += r; }
    else { sp->y -= 24; if (o) o->x += k ? r : -r; }
    if (!o) return;
    ga(o, G_SPIKE); o->ang = (float)ang; R.spike[R.nspike++] = (short)(o - G.o);
}
static int lim_hit(const obj_t *s) {
    for (int k = 0; k < nlims; k++) { obj_t *l = sbb_slot(lims[k]); if (l && fabsf(l->x - s->x) < 96 && fabsf(l->y - s->y) < 96 && obj_rect_hit(s, (int)l->x - 21, (int)l->y - 18, (int)l->x - 5, (int)l->y + 14)) return 1; }
    return 0;
}
static void explode_named(void) { static const int SX[3] = { 311, 319, 320 }; snd_play_orig(SX[sbb_rand(3)], 220, 128); }   /* a named sound_explode<3..5> sample (RV g_kill: 311 / 319 / 320) */
static void parts_fall_explode(obj_t *o, int hand) {                            /* 10106-10110: a part reaching y 981 explodes (hands: sound 38) */
    if (!o || o->hidden || o->y < 981) return;
    obj_effect(o->x, o->y, "explosion01"); o->hidden = 1; if (G.cam.Z < 3) G.cam.Z = 3;
    if (hand) snd_play_orig(38, 220, 128); else explode_named();
}

void sbg_frame(player_t *p) {
    obj_t *b = body();
    if (!b) return;
    double PXv = sbb_px(p), PYv = sbb_py(p);
    int rails = sbb_rails();
    (void)rails;                                                                /* 10078 / 10079: the flow's camera lock (sb_flow.c) */
    if (level_cam_locked() && !R.start_once && sbb_mly == 0) { R.start_once = 1; sbb_fight_start(); R.on = 1; R.sub = 1; }   /* 10080: CAM.M 1, ML.Y 0 */
    obj_t *s = spire();
    if (R.on) {
        G.cam.H = fmaxf(G.cam.H - 3, -50);                                     /* 10081 */
        if (R.sub) {                                                            /* 0x22989: the climb */
            double lim = 352 - R.E * 88;
            R.F = 0;                                                            /* 10082 */
            if (R.H == 1 && R.G < lim) R.F = 4;                                 /* 10083 */
            if (R.H == -1 && R.G > lim - 88) R.F = -4;                          /* 10084 */
            if (R.G > lim) R.F = -4;                                            /* 10085 */
            /* 0x2298a: the attacks */
            if (R.F == 0 && R.E < 5) R.I += 1;                                  /* 10086 */
            if (R.F == 0 && R.D < 3 && R.I == 20) spark_new(b, -1);              /* 10087 */
            if (R.F == 0 && R.D < 3 && R.I == 20) { R.J += 10; b->y = (float)(int)(b->y + R.J); spark_new(b, 1); snd_play_orig(171, 220, 128); }   /* 10088 */
            if (R.J > 0) { R.J -= 1; b->y -= 1; }                               /* 10089 */
            if ((R.F == 0 && R.D < 3 && R.I > 80) || (R.F == 0 && R.D == 3 && R.I > 120 && R.J == 0)) { R.fl2 ^= 1; R.I = 0; R.H = -R.H; R.D += 1; }   /* 10090 */
            if (R.F == 0 && R.D == 3 && R.I == 10) R.dir = 1;                   /* 10091 */
            if (R.F == 0 && R.D == 3 && R.I == 40) { R.dir = 2; R.sA = 1; R.J = -10; b->y -= 10; snd_play_orig(77, 220, 128); }   /* 10092 */
            if (R.J < 0) { R.J += 1; b->y += 1; }                               /* 10093 */
            if (R.F == 0 && R.D == 4 && R.I == 20) R.sA = 3;                     /* 10094 */
            if (R.F == 0 && R.D == 4 && R.I > 20 && R.J == 0 && R.sA == 0) { R.fl2 ^= 1; R.I = 0; R.D = 0; R.H = -R.H; }   /* 10095 */
            /* 0x22988: the rig (the moving hand at 2F, the body at F) */
            obj_t *h = hand(R.fl2);
            if (h) h->y = (float)(int)(h->y + 2 * R.F);
            b->y = (float)(int)(b->y + R.F); R.G += R.F;                        /* 10096 / 10097 */
            if (h && ((R.F > 0 && h->y > b->y + 40) || (R.F < 0 && h->y < b->y - 40))) { R.fl2 ^= 1; snd_play_orig(266, 220, 128); }   /* 10098 / 10099 */
            link_rig();                                                         /* 10100 */
        }
        if ((b->fl & SB_DEF) && R.E < 4) {                                      /* 10101: a stage */
            R.E += 1; b->c = 60; R.hurt = 1; parts_anim(1); b->fl &= ~SB_DEF; R.H = -1; b->A = 1;
            snd_play_orig(47, 220, 128);                                        /* (stop_sample(23): the shared defeat sound) */
        }
        if (b->c == 1) { R.hurt = 0; parts_anim(0); }                           /* 10102: restore_animation */
        if ((b->fl & SB_DEF) && R.E == 4) {                                     /* 10103: the defeat */
            R.E = 5; parts_anim(1); R.H = -1; b->A = 30; R.sub = 0; sbb_music_fadeout();
        }
        if (R.E == 5 && !R.parts_gone && G.frame % 7 == 0) {                    /* 10104 (g3 % 7: the game's frame counter; its phase differs) */
            obj_effect((float)((int)b->x - 40 + sbb_rand(81)), (float)((int)b->y - 40 + sbb_rand(81)), "explosion06");
            explode_named(); R.L += 1;
        }
        if (R.E == 5 && R.L > 7) {                                              /* 10105: the fall */
            R.K += 0.2; b->y = (float)(int)(b->y + R.K);
            obj_t *hl = hand(0), *hr = hand(1);
            if (hl) hl->y = (float)(int)(0.7 * R.K + hl->y);
            if (hr) hr->y = (float)(int)(1.3 * R.K + hr->y);
            link_rig();
        }
        if (R.E == 5) {                                                         /* 10106-10110 */
            parts_fall_explode(hand(0), 1);
            for (int r = 0; r < 2; r++) for (int k = 0; k < NLINK; k++) parts_fall_explode(arm(r, k), 0);
            parts_fall_explode(b, 0); parts_fall_explode(hand(1), 1);
            obj_t *hl = hand(0);
            if (!R.parts_gone && hl && hl->hidden) {                            /* 10111: everything destroyed (INFERRED: once the left hand is down) */
                R.parts_gone = 1; snd_play_orig(38, 220, 128);
                hl->alive = 0; obj_t *hr = hand(1); if (hr) hr->alive = 0; b->hidden = 1;
                for (int r = 0; r < 2; r++) for (int k = 0; k < NLINK; k++) { obj_t *a = arm(r, k); if (a) a->alive = 0; }
            }
        }
        if (R.parts_gone > 1 && !R.ended && sbb_bars() > 0) {                  /* (the next frame: the destroyed hand leaves the list then, RV 2050 / 2051) */                       /* 10112: the arena end (+ CrystalPlatform destroyed) */
            R.ended = 1; sbb_arena_end(1, 0x40); obj_t *pl = plat(); if (pl) pl->alive = 0;
        }
        if (R.parts_gone == 1) R.parts_gone = 2;
    }
    if (R.ended && sbb_bars() == 0) G.cam.H = fminf(G.cam.H + 3, 0);           /* 10113 */
    /* sparks 10114-10121 */
    for (int k = 0; k < R.nspark; k++) {
        obj_t *o = sbb_slot(R.spark[k]);
        if (!o || o->type != OT_SB_SPARK) { R.spark[k] = R.spark[--R.nspark]; R.kA[k] = R.kA[R.nspark]; R.kB[k] = R.kB[R.nspark]; R.kC[k] = R.kC[R.nspark];
                                             R.kD[k] = R.kD[R.nspark]; R.kE[k] = R.kE[R.nspark]; R.kF[k] = R.kF[R.nspark]; k--; continue; }
        R.kA[k] += R.kC[k]; R.kB[k] += R.kD[k]; o->x = (float)(int)R.kA[k]; o->y = (float)(int)R.kB[k]; R.kE[k] += 1;   /* 10114 */
        if (PXv < (int)o->x && R.kC[k] > -4) R.kC[k] -= 0.25;                   /* 10115 */
        if ((int)o->x < PXv && R.kC[k] < 4) R.kC[k] += 0.25;                    /* 10116 */
        if (R.kE[k] > 150) { obj_effect(o->x, o->y, "wburst"); snd_play_orig(45, 220, 128); o->alive = 0; continue; }   /* 10117 */
        if (R.kF[k] == 0) R.kF[k] = (int)o->y < PYv ? 1 : -1;                   /* 10118 / 10119 */
        if (R.kF[k] == 1) R.kD[k] = fmin(R.kD[k] + 0.5, 6);                     /* 10120 */
        if (R.kF[k] == -1) R.kD[k] = fmax(R.kD[k] - 0.5, -6);                   /* 10121 */
    }
    /* the spire 10122-10127 and col_30_38 */
    obj_t *pl = plat(), *w = warn();
    if (s) {
        if (R.sA == 0) { s->y = (float)(int)b->y; if (pl) pl->fl |= 1; }        /* 10122 */
        if (R.sA == 1) {
            s->y += 10; R.warn_vis = 1;                                         /* 10123 */
        }
        if (R.sA == 3) {                                                        /* 10125 */
            s->y -= 10;
            if (pl && s->an.b) pl->y = (float)((int)s->y - anim_frame(&s->an)->hy - 10);
            if (!p->ground && pl && pl->an.b) {                                 /* 10126: the player on the platform rides up (MV.L 0) */
                int ox, oy, h; const signed char *x0, *x1;
                if (mv_sensor_rows(SENS_BOTTOMBIG, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(pl, ox, oy, x0, x1, h)) { p->y -= 10; player_sync_sensors(p); }
            }
            if ((int)s->y <= (int)b->y) { R.sA = 0; R.dir = 0; R.J = -10; b->y -= 10; }   /* 10127 */
        }
    }
    if (w) w->hidden = !R.warn_vis;
    /* spike spawners 10128-10136, col_12_38 / col_18_38 */
    for (int k = 0; k < 2; k++) {
        obj_t *sp = spawner(k); if (!sp) continue;
        if (!k && (R.spfl[0] & 2) && G.frame % 15 == 0) { snd_play_orig(77, 220, 128); if (G.cam.Z < 3) G.cam.Z = 3; }   /* 10128 */
        if (R.spfl[k] & 2) R.spC[k] += 1;                                       /* 10129 / 10133 */
        if (R.spC[k] > 0) { R.spC[k] -= 5; spike_new(sp, k, (R.spfl[k] & 4) != 0); }   /* 10130 / 10131 / 10134 / 10135 */
        int lh = lim_hit(sp);
        if ((R.spfl[k] & 4) && !(R.spfl[k] & 8) && !lh) R.spfl[k] |= 8;          /* 10132 / 10136 */
        if (lh) {                                                               /* col_12_38 / col_18_38 */
            if (!(R.spfl[k] & 4)) { R.spfl[k] |= 4; sp->x += k ? 16 : -12; }
            if (R.spfl[k] & 8) { R.spfl[k] &= ~(2 | 4 | 8); sp->x = (float)R.spA[k]; sp->y = (float)R.spB[k]; R.spC[k] = 0; }
        }
    }
    /* spikes 10138 / 10140 */
    for (int k = 0; k < R.nspike; k++) {
        obj_t *o = sbb_slot(R.spike[k]);
        if (!o || o->type != OT_SB_SPIKE) { R.spike[k--] = R.spike[--R.nspike]; continue; }
        if (o->an.done) { o->alive = 0; R.spike[k--] = R.spike[--R.nspike]; continue; }   /* 10140 (+ EffectsDebris, not ported) */
        if (sbb_vulnerable(p) && sbb_p326_hit(o)) player_contact(p, 5);         /* 10138 (OTHERS.B 5 exempt: not modelled) */
    }
    /* sprites + FP_TRACK mirror */
    body_anim(b);
    b->av[0] = R.D; b->av[1] = R.E; b->av[2] = R.F; b->av[3] = R.G; b->av[4] = R.H; b->av[5] = R.I; b->av[6] = R.J; b->av[7] = R.on | (R.fl2 << 1);
    if (s) { s->av[0] = R.sA; }
    for (int k = 0; k < 2; k++) { obj_t *sp = spawner(k); if (sp) { sp->av[0] = R.spC[k]; sp->av[1] = R.spfl[k]; } }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_SBG")) printf("sbg %d on %d D %.0f E %.0f F %.0f G %.0f H %.0f I %.0f J %.0f sA %.0f y %.0f hl %.0f hr %.0f fl2 %d nsk %d nsp %d\n", G.frame, R.on, R.D, R.E, R.F, R.G, R.H, R.I, R.J, R.sA, b->y,
                                      hand(0) ? hand(0)->y : 0, hand(1) ? hand(1)->y : 0, R.fl2, R.nspike, R.nspark);
#endif
}
int sbg_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_SB_SPAWN1 || o->type == OT_SB_SPAWN2 || o->type == OT_SB_CPLAT) return 1;   /* invisible */
    if (o->type >= OT_SB_GR && o->type <= OT_SB_LSPIKE) { sbb_draw(o, 0xffffffff); return 1; }
    return 0;
}
