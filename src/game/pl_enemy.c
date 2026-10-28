/* Pangu Lagoon enemies (session 23): types 2080-2143 (pl.h OT_PL_*). Spec: build/re/spec_pl_enemies.md.
 * Classes: EnemyStinger_2933, EnemyStingerNest_2937, EnemySunflora_2940 (+ EnemySleepGas_2941, EffectsConfetti_2943),
 * EnemyTerreon_3006 (+ Left / RightLeg_3004 / 3005, Left / RightShoulder_3002 / 3003, EffectTerreonWarning_3008, the runtime
 * EnemyTerreonDebris_3007 and the shared HazardShockwave_1576 of relic_boss.c), EnemyObelisk_3047 (+ EnemyObeliskLaser_3048),
 * EnemyWallStinger_3056 (+ EnemyWallStingerSpawn_3057), EnemyWallStingerBack_3058 with WallStingerLimitTop / Bottom_3060 / 3061.
 * EnemyCacteon_1013 (A2-A5) is the engine's OT_CACTEON. Active_2927 (PL1, 5) is a pause-only prop (12842 / 12843): flow scenery.
 * Labels: RE (events 12736-12961, 13024, 13057-13107 of handle_frame_51..56, on_frame_51..56_start_events 12851-12853 / 12961 /
 * 13106 / 13107, constructors, on_frame_51..56_init qualifier 20, test_collisions_50..55 + col_11_50 / col_6_51 / col_3_53),
 * INFERRED (labelled). Shared code: enemy_kit.h (ENS rows, R-hurt, burst death, limiters, pairing) and combat.c's Q20 hits /
 * defeat / fly-away (the stinger lands like a Ganseki: objdefs OD_LANDS, sound 50, RE 12810).
 * Order: pl_enemy_pre (objects.c, before combat) = the collision-flag hurts (sleep gas, laser, terreon spikes / legs);
 * pl_enemy_obj = each class's events after the animations advanced (update_objects), in event order per class.
 * Alterables: av[] = the class's letters (named per class below); flags: 0x20 the original's flag 5 (armoured), 0x40 flag 6
 * (defeated), F_NOFLY flag 16; the classes' own flags 1..3 are FL1..FL3. Per-type instance lists, never a scan of every object. */
#include "pl.h"
#include "enemy_kit.h"

#define DEFEATED(o) ((o)->fl & 0x40)
#define FL1 0x1000                                   /* the class's flag 1 */
#define FL2 0x2000                                   /* flag 2 */
#define FL3 0x4000                                   /* flag 3 */
#define FLLIM 0x8000                                 /* obelisk: its EnemyLimiter used (13067 destroys the limiter) */
#define FLS10 0x80000                                /* back swarm: on json layer 1 scrolling at 1 (objects_draw(-1) would use 0.8: drawn by pl_enemy_draw(-1)) */
#define RT0 OT_PLE_FIRST
#define NRT (OT_PLE_LAST - OT_PLE_FIRST + 1)
#define PL_LMAX 320                                  /* the stingers' list: nests (15 each) and wall stingers add them at run time */
typedef struct { short idx[PL_LMAX]; int n; } pl_list_t;
static pl_list_t LST[NRT];
#define EACH(T, OB) for (int _k = 0; _k < LST[(T) - RT0].n; _k++) if (((OB) = &G.o[LST[(T) - RT0].idx[_k]])->alive && (OB)->type == (T))
static struct { int on, swarm, nstingers; ek_limiters_t lim; short shk[32]; int nshk; } S;

static int R(int n) { return cross_rand(n); }
static int g3(void) { return (int)(G.frame % 100000); }                      /* global value 3 (the frame counter) */
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }    /* !is_near_border(-n) */
static double MVX(const player_t *p) { return p->x; }                         /* PlayerMovementValues X / Y */
static double MVY(const player_t *p) { return p->y - ORIG_REF_DY; }
static void list_add(obj_t *o) {
    pl_list_t *L = &LST[o->type - RT0]; int i = (int)(o - G.o);
    for (int k = 0; k < L->n; k++) if (L->idx[k] == i) return;
    if (L->n < PL_LMAX) { L->idx[L->n++] = (short)i; return; }
    for (int k = 0; k < PL_LMAX; k++) { const obj_t *q = &G.o[L->idx[k]]; if (!q->alive || q->type != o->type) { L->idx[k] = (short)i; return; } }
}
static int list_alive(int t) { int n = 0; obj_t *o; EACH(t, o) n++; return n; }
static obj_t *spawn(int type, const char *anim, float x, float y, int dir, int layer) {
    obj_t *o = ek_spawn(NULL, type, anim, x, y, dir);
    if (!o) return NULL;
    o->layer = layer; list_add(o);
    return o;
}
/* Fusion's "pick one of" (cross_seed): one random instance among n candidates */
static obj_t *pick(obj_t **c, int n) { return n <= 0 ? NULL : c[n == 1 ? 0 : R(n)]; }
static int lv_word(const obj_t *o) { int i = o->t / 13; return (o->t % 13 == 0 && i >= 0 && i < G.lvl.nobj && G.lvl.objs[i].type == o->type) ? G.lvl.objs[i].anim : 0x200; }

int pl_enemy_q20(int t) {                                                       /* RE on_frame_51..56_init qualifier 20 */
    return t == OT_PL_STINGER || t == OT_PL_NEST || t == OT_PL_SUNFLORA || t == OT_PL_TERREON || t == OT_PL_TLEG || t == OT_PL_TRLEG ||
           t == OT_PL_OBELISK || t == OT_PL_WSTINGER;
}

/* ================================================================ EnemyStinger_2933 (RE 12784-12810, 12736 / 12737 loop_stinger_50,
 * start 12851; A4 / A5 13085-13087). Ctor A 4, E 4. av: [0] D state (0 hover, 1 fly, 2 wind-up, 3 dive, 4 climb back, 5 defeated),
 * [1] E x speed / side, [2] F x home, [3] G y home, [4] H timer, [5] I timer; B (contact damage) in bd.
 * D0 hovers I > 50 frames, then D1 flies 4 px a frame (E reversed) for 30 frames, I = R(30). A player below it on its 45-degree
 * diagonal (150 px down, +-10 px, not hurt / dead) makes one random stinger wind up (D2: anim 11, rises G - H / 2 for 30 frames,
 * faces the player, slides onto the diagonal when E > 0), then D3: sound 10 once at H 15 (one random stinger a frame), anim 9,
 * contact damage 5, 5 px a frame down the diagonal until the background or G + 150; D4: 10 frames, then back up 3 px a frame to
 * G (anim 0). Defeated: anim 8, the shared fly-away, the Ganseki landing (12810: shade + explosion01 + sound 50, OD_LANDS). */
static void stinger_init(obj_t *o, int k) {
    o->A = 4; o->av[1] = 4; o->av[3] = floorf(o->y); o->av[2] = floorf(o->x);
    ek_set_a(o, "pls_idle"); ek_force_frame(o, (R(32768) * 3) >> 14);         /* 12851: a random frame (restored: it animates) */
    o->av[5] = fmod(k * 10.0, 50.0);                                           /* 12851: I = (instance index * 10) mod 50 */
}
static void stinger_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *Gy = &o->av[3], *H = &o->av[4], *I = &o->av[5];
    if (*D == 0) *I += 1;                                                      /* 12784 */
    if (*D == 0 && *I > 50) { *D = 1; *I = 0; *E = -*E; }                       /* 12785 */
    if (*D == 1) { *I += 1; ek_set_a(o, "pls_fly"); o->x = (float)(int)(floorf(o->x) + *E); }   /* 12786 */
    if (*D == 1 && *I > 30) { *D = 0; *I = (R(32768) * 30) >> 15; ek_set_a(o, "pls_idle"); }   /* 12787 */
    if (*D == 1) ek_face(o, *E > 0 ? 0 : 16);                                  /* 12788 / 12789 */
    if (*D == 2) {                                                             /* 12792: anim 11 (= anim 1's images) */
        ek_set_a(o, "pls_fly"); *H += 1; o->y = (float)(int)(*Gy - 0.5 * *H);
        double X = MVX(p), dy = MVY(p) - *Gy, x = floorf(o->x);                /* 12736 / 12737 (loop_stinger_50: E > 0 only, RE) */
        if (*E > 0 && x > X + 2 - dy) o->x = (float)(x - 1);
        else if (*E > 0 && x < X - 2 - dy) o->x = (float)(x + 1);
        if (floorf(o->x) < MVX(p)) { ek_face(o, 0); *E = 4; }                  /* 12794 */
        else if (floorf(o->x) > MVX(p)) { ek_face(o, 16); *E = -4; }            /* 12795 */
        if (*H > 30) { *D = 3; *H = 0; }                                       /* 12796 */
    }
    if (*D == 3) {
        if (*H < 15) *H += 3;                                                  /* 12797 */
        if (*H >= 15) {                                                        /* 12799-12801 (12798: the sound, stinger_sound) */
            ek_set_a(o, "pls_att"); o->bd = 5;
            o->x = (float)(int)(floorf(o->x) + (*E > 0 ? 5 : -5)); o->y = (float)(int)(floorf(o->y) + 5);
        }
        if (obj_overlaps_bg(o) || floorf(o->y) > *Gy + 150) { *D = 4; *H = 0; }   /* 12802 / 12803 */
    }
    if (*D != 3 && o->bd > 0) o->bd = 0;                                       /* 12804 */
    if (*D == 4) {
        if (*H < 10) *H += 1;                                                  /* 12805 */
        else { o->y = (float)(int)(floorf(o->y) - 3); ek_set_a(o, "pls_idle"); }   /* 12806 */
        if (*D == 4 && *Gy >= floorf(o->y)) { o->y = (float)(int)*Gy; *D = 0; *H = 0; }   /* 12807 */
    }
    if (DEFEATED(o) || *D == 5) { *D = 5; ek_set_a(o, "pls_dead"); o->bd = 0; }   /* 12808 / 12809 */
}
/* 12790 / 12791: the dive trigger, one random stinger a frame for each side; 12798: the dive sound, one random stinger */
static void stinger_group(player_t *p) {
    obj_t *o, *c[PL_LMAX]; int n;
    int ok = !ek_dead(p) && p->state != ST_HURT;                               /* OTHERS.Y != 10 / 11 (INFERRED: hurt / defeated) */
    double X = MVX(p), Y = MVY(p);
    for (int side = 0; side < 2 && ok; side++) {
        n = 0;
        EACH(OT_PL_STINGER, o) {
            if (DEFEATED(o) || o->av[0] > 1 || !in_view(o, 100)) continue;
            double y = floorf(o->y), x = floorf(o->x), dy = Y - o->av[3];
            if (!(y < Y && y > Y - 150)) continue;
            if (side == 0 ? (x > dy + X - 10 && x < dy + X + 10) : (x > X - dy - 10 && x < X - dy + 10)) c[n++] = o;
        }
        if ((o = pick(c, n))) { o->av[0] = 2; o->av[1] = side ? 4 : -4; o->av[4] = 0; ek_face(o, side ? 0 : 16); }
    }
    if (S.swarm) EACH(OT_PL_STINGER, o) if (!DEFEATED(o)) {                   /* A4 / A5 13086 / 13087: the swarm wraps with the view (every
                                                                                  stinger, also the ones asleep far from it) */
        double W = SCREEN_W;                                                   /* global value 16 (INFERRED: the view width, event 138) */
        if (floorf(o->x) < G.camx - 80) o->x = (float)(int)(floorf(o->x) + W + 180);
        else if (floorf(o->x) > G.camx + W + 80) o->x = (float)(int)(floorf(o->x) - W - 180);
    }
    n = 0;
    EACH(OT_PL_STINGER, o) if (o->av[0] == 3 && o->av[4] == 15) c[n++] = o;
    if ((o = pick(c, n))) { snd_play_orig(10, 220, 128); o->av[4] += 1; }
}
static obj_t *stinger_new(float x, float y, int layer) {
    obj_t *s = spawn(OT_PL_STINGER, "pls_idle", x, y, 0, layer);
    if (s) { s->A = 4; s->av[1] = 4; s->av[2] = floorf(x); s->av[3] = floorf(y); }
    return s;
}

/* ================================================================ EnemyStingerNest_2937 (RE 12811-12817, 12739, start 12852; A4 / A5
 * 13057 foreach_nest_53, 13085). Ctor A 25, G 15; start: flag 16 (no fly-away), D / E = its position. av: [0] D, [1] E home,
 * [2] F shake, [3] G stingers left, [4] Q bursts. A hit (C > 0) angers it (flag 1, F = |X| + |Y| of the knock-back): it shakes
 * around D / E by up to min(F, 5) (F - 0.5 a frame); every 8th frame an angry nest with G > 0 releases a stinger (D 1, E +-4, at
 * +-30 px, its home there; sound 323). A4 / A5: a calm nest angers when the player is within 150 x 100 px (13085, fewer than 300
 * stingers). Defeated: anim 8, the burst death (explosion06 every 7th frame, sound explode3-5; at Q 10 keystone, explosion01,
 * sound 38, debris). */
static void nest_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gs = &o->av[3], *Q = &o->av[4];
    {                                                                          /* 12811 */
        int n = *F <= 5 ? (int)*F & 0xffff : 5; double h = fmin(0.5 * *F, 5);
        o->x = (float)(int)(((R(32768) * n) >> 15) + (*D - h));
        o->y = (float)(int)(((R(32768) * n) >> 15) + (*E - h));
        *F = fmax(*F - 0.5, 0);
    }
    if (o->c > 0 && !DEFEATED(o)) { o->fl |= FL1; *F = fabs(o->kx) + fabs(o->ky); }   /* 12812 */
    if (S.swarm && !(o->fl & FL1) && S.nstingers < 300 && !ek_dead(p) &&       /* 13085 */
        fabs(floorf(o->x) - MVX(p)) < 150 && fabs(floorf(o->y) - MVY(p)) < 100) o->fl |= FL1;
    if (g3() % 8 == 0 && (o->fl & FL1) && *Gs > 0 && !DEFEATED(o) && S.nstingers < PL_LMAX - 8) {   /* 12813 / 13057 (cap: port list) */
        obj_t *s = stinger_new(floorf(o->x), floorf(o->y), o->layer);
        if (s) {
            s->av[0] = 1; s->av[1] = (R(32768) & 0x4000) ? 4 : -4;            /* ((seed >> 27) & 8) - 4 */
            s->x = (float)(int)(floorf(s->x) - 30 + ((R(32768) * 61) >> 15)); s->y = (float)(int)(floorf(s->y) - 30 + ((R(32768) * 61) >> 15));
            s->av[2] = floorf(s->x); s->av[3] = floorf(s->y); *Gs -= 1; S.nstingers++;
            snd_play_orig(323, 220, 128);
        }
    }
    if (DEFEATED(o)) { ek_set_a(o, "pln_dead"); ek_burst_death(o, Q, 0, 38); }   /* 12814-12817 */
}

/* ================================================================ EnemySunflora_2940 (RE 12818-12828, 12834-12837, foreach_sf_50, start
 * 12853). Ctor A 12, F 1; start flag 16 (it stays and regrows). av: [0] D (0 watch, 1 close, 2 spray, 6 regrowing), [1] E timer,
 * [2] F facing -1..1 (anim 0's frame int(3F + 3)). In view (100) E counts; at E 20 it decides the side (flag 1: the player is
 * left of it) and turns F by 0.1 a frame; at E > 40 with F at +-1: anim 10, D 1; 60 frames later D 2 (sound 44): every 3rd
 * frame for 60 frames a sleep gas from its action point (angle (1 + F) 90 + 200 - R(41)); at E 80 the side flips, D 0.
 * Defeated: explosion01, sound 50, invisible, 8 confetti; at E 250 it grows back (scale 0.02 a frame), at E 300 A 12, D 0. */
static void sunflora_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2];
    if (in_view(o, 100) && *D == 0) *E += 1;                                   /* 12818 */
    if (*D == 0 && *E == 20) { if (floorf(o->x) > ek_px(p)) o->fl |= FL1; else o->fl &= ~FL1; }   /* 12819 / 12820 */
    if (*D == 0 && *E > 40 && (*F == -1 || *F == 1)) {                         /* 12821 / 12822 */
        *D += 1; *E = 0; ek_face(o, *F == -1 ? 16 : 0); ek_restart_a(o, "plf_shoot");
    }
    if (*D == 0) {                                                             /* 12823 / 12824 */
        *F = (o->fl & FL1) ? fmax(*F - 0.1, -1) : fmin(*F + 0.1, 1);
        ek_set_a(o, "plf_turn"); ek_force_frame(o, (int)(3 * *F + 3));
    }
    if (*D > 0) *E += 1;                                                       /* 12825 */
    if (*D == 1 && *E > 60 && (*F == -1 || *F == 1)) {                         /* 12826 / 12827 */
        *E = 0; *D += 1; ek_force_frame(o, 0); snd_play_orig(44, 220, 128);   /* (frame 0 held while it sprays; 12827's frame 6 is past
                                                                                  anim 10's end: INFERRED the same open frame) */
    }
    if (*D == 2 && *E <= 60) {
        if (o->an.b) o->an.acc = 0;                                            /* (the forced frame holds) */
        if (g3() % 3 == 0) {                                                   /* 12828 foreach_sf_50 */
            double ax, ay; relic_act_point(o, &ax, &ay);
            obj_t *g = spawn(OT_PL_GAS, "plz_gas", (float)(int)ax, (float)(int)ay, 0, o->layer);
            if (g) {
                g->hidden = 1; g->av[1] = 4; g->av[3] = (int)ax; g->av[4] = (int)ay;   /* B 4 (ctor), D / E */
                g->av[2] = (1 + *F) * 90 + 200 - ((R(32768) * 41) >> 15);       /* C */
                g->av[5] = (double)(((R(32768) * 5) >> 13) - 15);              /* F */
            }
        }
    } else if (*D == 2 && o->an.b) o->an.acc = 0;
    if (*D == 2 && *E > 80) { o->fl ^= FL1; *D = 0; *E = 0; ek_set_a(o, "plf_turn"); }   /* 12834 */
    if (DEFEATED(o) && *D != 6) {                                              /* 12835 */
        obj_effect(o->x, o->y, "explosion"); snd_play_orig(50, 220, 128);
        *D = 6; o->hidden = 1; *E = 0; ek_face(o, *F < 0 ? 16 : 0); ek_set_a(o, "plf_grow"); o->scale = 0;
        for (int k = 0; k < 8; k++) {
            obj_t *c = spawn(OT_PL_CONFETTI, "plf_conf", o->x, o->y, 0, 3);
            if (!c) break;
            ek_force_frame(c, (R(32768) * 3) >> 13); c->an.acc = 0;            /* (restored: it animates) */
            double a = R(32) * 11.25, v = (((R(32768) * 5) >> 13) + 20) / 8.0;  /* speed 20 + R: INFERRED a random direction */
            c->vx = (float)(v * ek_cos(a)); c->vy = (float)(-v * ek_sin(a)); c->av[0] = R(360);
        }
    }
    if (*D == 6 && *E > 250) { o->scale = (float)fmin((*E - 250) * 0.02, 1); if (o->scale <= 0) o->scale = 0.01f; o->hidden = 0; }   /* 12836 */
    if (*D == 6 && *E > 300) { ek_set_a(o, "plf_turn"); o->fl &= ~0x40; o->A = 12; *D = 0; *E = 0; o->scale = 0; o->hidden = 0; }   /* 12837 */
}
static void confetti_frame(obj_t *c) {                                         /* 12839-12841 */
    c->ang = (float)(45 * ek_sin(c->av[0])); c->av[0] += 4; if (c->av[0] >= 360) c->av[0] -= 360;
    c->x += c->vx; c->vy *= 0.95f;                                             /* (its movement: INFERRED, slowing) */
    c->y = (float)(int)(floorf(c->y) + fmax(1, (c->av[1] - 20) / 10) + c->vy); c->av[1] += 1;
    if (floorf(c->y) > G.camy + SCREEN_H + 20) c->alive = 0;
}
/* EnemySleepGas_2941 (RE 12829-12833, col_11_50, col_6_51). av: [0] A age, [1] B speed (4, -0.04 a frame), [2] C angle, [3] D /
 * [4] E float position, [5] F sparkle phase. Ring sparkles at A = F + 20 / 40 / 60; alpha 159, from A 31: 249 - 3A; gone at A 81.
 * Touching the character sensor while A < 65: R-hurt 5 unless the wood shield (OTHERS.B 1), pink tint 90 frames (col_11_50).
 * It also stings a terreon (col_6_51): A - 3, C 5, sound 24 (flags 5 / 6 off, C <= 0). */
static void gas_frame(obj_t *g) {
    double *A = &g->av[0], *B = &g->av[1], *C = &g->av[2], *Dx = &g->av[3], *Ey = &g->av[4], *F = &g->av[5];
    g->x = (float)(int)*Dx; g->y = (float)(int)*Ey;                            /* 12829 */
    *Dx += ek_cos(*C) * *B; *Ey -= ek_sin(*C) * *B; *A += 1; *B -= 0.04;
    if (*A == *F + 20 || *A == *F + 40 || *A == *F + 60) obj_effect(g->x, g->y, "ringsparkle");   /* 12830-12832 */
    if (*A > 80) g->alive = 0;                                                 /* 12833 */
}
static int gas_alpha(const obj_t *g) { double v = 3 * g->av[0] - 90; return v <= 0 ? 159 : (int)fmax(0, 255 - (int)(v + 96)); }

/* ================================================================ EnemyTerreon_3006 + legs / shoulders / warning (RE 12906-12954,
 * 12857-12863 (the jump's sensors, in 12919's loop), start 12961). Ctor A 40, J 300; legs A 50 (armoured from the start). av: [0] D
 * (0 rest, 1 wind-up, 2 jump, 3 land, 6 defeated, 7 breaking), [1] E / [2] F float position, [3] G x speed, [4] H y speed, [5] I
 * blink timer, [6] J phase (deg), [7] K counter; L (warning shown) in st; flag 2 (FL2) = woken (within 200 px of the view).
 * The body sits at (E, F - 16 + 24 cos J), legs at (E -+ (32 + 16 cos J), F) tilted max(10 - 20 cos J, 0) / min(20 cos J - 10, 0),
 * shoulders at (E -+ (16 + 8 cos J), F + 8 cos J). Rest: K counts to 15 (D 1, K = -10 - R(30)); wind-up: J + 10 a frame; at
 * J 150 it jumps (H -10, G = clamp((X - E) 0.03, -15, 15), sound 147): E + G, F + H, H + 0.4 (max 15), J to 180; walls stop G,
 * a ceiling stops H, on the ground (F + 36 solid) it lands: two shockwaves (x -+ 16, y + 76, 6.25 px a frame), two explosions,
 * sound 51, camera shake 4, D 3, J 240 -> 360 -> 60, then J 300, D 0. Hurts: its spiky top (Player_326 above its centre: 10, not in
 * M 26 / 99), the legs while J is 180..240 (B 10), the shockwaves (10). Defeated: anim 8, explosions every 7th frame (K to 12),
 * then sound 38, a keystone, four debris pieces (the legs and shoulders) and it goes. The warning marks it 50..200 px off view. */
/* HazardShockwave_1576 (the Relic Maze golem's: relic_boss.c moves it 6.25 px a frame and hurts 10 while B <= 50). PL's own rules
 * (RE 12923-12926): B + 1 a frame, gone past B 50 or off a ledge (2 px below it neither the background nor a Q7 / Q8 object) */
static void shock_new(double x, double y, int dir) {
    obj_t *k = relic_shock_new(x, y, dir, 0);
    if (!k) return;
    int i = (int)(k - G.o);
    for (int j = 0; j < S.nshk; j++) if (!G.o[S.shk[j]].alive || G.o[S.shk[j]].type != OT_SHOCK) { S.shk[j] = (short)i; return; }
    if (S.nshk < 32) S.shk[S.nshk++] = (short)i;
}
static void shocks_frame(void) {
    for (int j = 0; j < S.nshk; j++) {
        obj_t *k = &G.o[S.shk[j]];
        if (!k->alive || k->type != OT_SHOCK) continue;
        k->av[1] += 1; k->y += 2;                                              /* 12923 */
        if (k->av[1] > 50 || (!obj_overlaps_bg(k) && !objects_mask_hit_groups(k, 1))) k->alive = 0;   /* 12924 / 12925 */
        else k->y -= 2;                                                        /* 12926 */
    }
}
static void terreon_init(obj_t *o) {
    o->A = 40; o->av[6] = 300; o->av[1] = floorf(o->x); o->av[2] = floorf(o->y); o->av[5] = (R(32768) * 60) >> 15;   /* ctor + 12961 */
    o->fl |= F_NOFLY; o->hidden = 1; ek_set_a(o, "plt_body");
}
static obj_t *part_of(const obj_t *o, int type) { obj_t *q; EACH(type, q) if (q->par == (int)(o - G.o)) return q; return NULL; }
static void terreon_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gx = &o->av[3], *H = &o->av[4], *I = &o->av[5], *J = &o->av[6], *K = &o->av[7];
    obj_t *ll = part_of(o, OT_PL_TLEG), *rl = part_of(o, OT_PL_TRLEG), *ls = part_of(o, OT_PL_TLSH), *rs = part_of(o, OT_PL_TRSH), *w = part_of(o, OT_PL_TWARN);
    double cj = ek_cos(*J);
    {                                                                          /* 12906 */
        o->x = (float)(int)*E; o->y = (float)(int)(*F - 16 + cj * 24); *I += 1;
        if (ll) { ll->av[0] = fmax(10 - 20 * cj, 0); ll->ang = (float)ll->av[0]; ll->bd = 0; ll->av[1] = *J; ll->av[2] = *D; }
        if (rl) { rl->av[0] = fmin(20 * cj - 10, 0); rl->ang = (float)ek_nrm(rl->av[0]); rl->bd = 0; rl->av[1] = *J; rl->av[2] = *D; }
        if (ls) ls->av[0] = *D;
        if (rs) rs->av[0] = *D;
        if (w) {
            w->av[0] = fmax(fmin(floorf(o->x), G.camx + SCREEN_W - 20), G.camx + 20);
            w->av[1] = fmax(fmin(floorf(o->y), G.camy + SCREEN_H - 20), G.camy + 20);
            w->hidden = 1; w->av[2] = o->st;
        }
        o->st = 0;
    }
    if (*D != 7) {                                                             /* 12907-12910 */
        if (ll) { ll->x = (float)(int)(*E - 32 - cj * 16); ll->y = (float)(int)*F; }
        if (rl) { rl->x = (float)(int)(cj * 16 + *E + 32); rl->y = (float)(int)*F; }
        if (ls) { ls->x = (float)(int)(*E - 16 - cj * 8); ls->y = (float)(int)(cj * 8 + *F); }
        if (rs) { rs->x = (float)(int)(cj * 8 + *E + 16); rs->y = (float)(int)(cj * 8 + *F); }
    }
    if (in_view(o, 200) && !in_view(o, 50)) o->st = 1;                         /* 12911 */
    if (w && w->av[2] == 1) { w->x = (float)(int)w->av[0]; w->y = (float)(int)w->av[1]; w->hidden = 0; }   /* 12912 */
    if (*D < 6 && *I > 120) { *I = (R(32768) * 50) >> 15; ek_restart_a(o, "plt_blink"); }   /* 12913 */
    if (ek_a_is(o, "plt_blink") && o->an.done) ek_set_a(o, "plt_body");        /* (anim 5 plays twice: INFERRED back to 0) */
    if (!(o->fl & FL2) && in_view(o, 200)) o->fl |= FL2;                       /* 12914 */
    if ((o->fl & FL2) && in_view(o, 1200) && *D == 0) *K += 1;                 /* 12915 */
    if (*D == 0 && *K > 15) { *D = 1; *K = (double)(int)(-10 - ((R(32768) * 30) >> 15)); }   /* 12916 */
    if (*D == 1) *J = fmod(*J + 10, 360);                                      /* 12917 */
    if (*D == 1 && *J >= 150 && *J < 240) {                                    /* 12918 */
        *H = -10; *D += 1; *Gx = fmax(fmin((MVX(p) - *E) * 0.03, 15), -15); snd_play_orig(147, 220, 128);
    }
    if (*D == 2) {                                                             /* 12919: 12857-12863 */
        if (*Gx > 0 && px_solid(&G.lvl, (int)(30 + *E), (int)*F)) *Gx = 0;    /* 12857 */
        if (*Gx < 0 && px_solid(&G.lvl, (int)(*E - 30), (int)*F)) *Gx = 0;    /* 12858 */
        if (*Gx == 0 && *E < MVX(p) && !px_solid(&G.lvl, (int)(*E + 30), (int)*F)) *Gx = 3;   /* 12859 */
        if (*Gx == 0 && *E > MVX(p) && !px_solid(&G.lvl, (int)(*E - 30), (int)*F)) *Gx = -3;  /* 12860 */
        if (*H < 0 && px_solid(&G.lvl, (int)*E, (int)(*F - 16))) *H = 0;       /* 12861 */
        if (*H >= 0 && px_solid(&G.lvl, (int)*E, (int)(36 + *F))) {           /* 12862 / 12863 */
            shock_new(floorf(o->x) - 16, floorf(o->y) + 76, 16);
            *D = 3; *H = 0; *J = 240;
            obj_effect(floorf(o->x) - 24, floorf(o->y) + 72, "explosion"); obj_effect(floorf(o->x) + 24, floorf(o->y) + 72, "explosion");
            snd_play_orig(51, 220, 128); ek_shake(4);
            shock_new(floorf(o->x) + 16, floorf(o->y) + 76, 0);
        }
    }
    if (*D == 2) { *J = fmin(*J + 10, 180); *E += *Gx; *F += *H; *H = fmin(*H + 0.4, 15); }   /* 12920 */
    if (*D == 3) *J = fmod(*J + 10, 360);                                      /* 12921 */
    if (*D == 3 && *J >= 60 && *J < 80) { *J = 300; *D = 0; }                   /* 12922 */
    if (DEFEATED(o) && *D < 6) { *D = 6; *K = 0; }                             /* 12931 */
    if (*D == 6) ek_set_a(o, "plt_dead");                                      /* 12932 */
    if (*D == 6 && g3() % 7 == 0) {                                            /* 12941 */
        obj_effect(floorf(o->x) - 20 + ((R(32768) * 41) >> 15), floorf(o->y) - 20 + ((R(32768) * 41) >> 15), "explosion06");
        *K += 1; snd_play_orig(ek_explode_snd(), 220, 128);
    }
    if (*D == 6 && *K > 12) {                                                  /* 12942 */
        *D = 7; o->hidden = 2; *K = 0; snd_play_orig(38, 220, 128);
        keystone_spawn(o->x, o->y, 0, 0); obj_effect(o->x, o->y, "explosion");
    }
    if (*D == 7) {                                                             /* 12943-12947: the four debris pieces */
        static const float DX[4] = { -40, -40, 40, 40 }, VX[4] = { -8, -5, 8, 5 }, VY[4] = { -6, 0, -6, 0 };
        int k = (int)*K;
        if (k < 4) {
            obj_t *d = spawn(OT_PL_TDEBRIS, "plt_deb", floorf(o->x) + DX[k], floorf(o->y), 0, o->layer);
            if (d) { ek_force_frame(d, k); d->av[0] = VX[k]; d->av[1] = VY[k]; }
            *K += 1;
        } else {
            o->alive = 0;
            if (ll) ll->alive = 0;
            if (rl) rl->alive = 0;
            if (ls) ls->alive = 0;
            if (rs) rs->alive = 0;
            if (w) w->alive = 0;
        }
    }
}
static void tpart_frame(obj_t *q) {                                            /* 12929 / 12930, 12933-12940 */
    obj_t *o = ek_partner(q);
    if (!o || o->type != OT_PL_TERREON) { q->alive = 0; return; }             /* 12948-12952: more parts than terreons */
    q->hidden = 1;                                                             /* (drawn with its terreon: pl_enemy_draw) */
    if (q->type == OT_PL_TLEG || q->type == OT_PL_TRLEG) {
        int lft = q->type == OT_PL_TLEG;
        if (q->av[1] > 180 && q->av[1] < 240) q->bd = 10;                     /* 12929 / 12930: E = the terreon's J */
        ek_set_a(q, q->av[2] == 6 ? (lft ? "plt_ll8" : "plt_rl8") : (lft ? "plt_ll" : "plt_rl"));
    } else {
        int lft = q->type == OT_PL_TLSH;
        ek_set_a(q, q->av[0] == 6 ? (lft ? "plt_ls8" : "plt_rs8") : (lft ? "plt_ls" : "plt_rs"));
    }
}
static void tdebris_frame(obj_t *d) {                                          /* 12953 / 12954 */
    d->x = (float)(int)(floorf(d->x) + d->av[0]); d->y = (float)(int)(floorf(d->y) + d->av[1]); d->av[1] = fmin(d->av[1] + 0.3, 15);
    if (d->an.b) d->an.acc = 0;
    if (!in_view(d, 100)) d->alive = 0;
}

/* ================================================================ EnemyObelisk_3047 + EnemyObeliskLaser_3048 (RE 13067-13082, start
 * 13107, col_3_53 -> 13024). Ctor A 20, F -1; start flag 16 (its wreck stays). av: [0] D (0 asleep, 1 wait, 2 opening, 3 aiming,
 * 4 fired, 6 defeated), [1] E timer, [2] F facing (-1 left). Waking in view (60) on an EnemyLimiter turns it round (F = -F; the
 * limiter goes). D1 waits 20 frames (anim 11 opens), D2 20 (anim 7), D3 30: a laser from its action point (sound 327), D4 20:
 * anim 10 closes, D1 with E -80 (100 frames to the next shot). The laser runs 8 px a frame from its frame 6 and hurts 5 (R-hurt,
 * no shield exemption); off view (200) it goes. Defeated: sound 29, a keystone (y + 14, B -3), explosion06, anim 8, then five
 * explosions every 2nd frame. */
static void obelisk_frame(obj_t *o) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2];
    int v = in_view(o, 60);
    if (*D == 0 && v && !(o->fl & FLLIM) && ek_limiter_hit(&S.lim, o)) { *F = -*F; o->fl |= FLLIM; }   /* 13067 */
    if (*D == 0 && v) *D = 1;                                                  /* 13068 */
    if (*D == 1 && v) *E += 1;                                                 /* 13069 */
    if (*D > 1 && *D < 6) *E += 1;                                             /* 13070 */
    ek_face(o, *F == -1 ? 16 : 0);                                             /* 13071 / 13072 */
    if (*D == 1 && *E > 20) { *E = 0; *D += 1; ek_restart_a(o, "plo_open"); }  /* 13073 */
    if (*D == 2 && *E > 20) { *D += 1; *E = 0; ek_restart_a(o, "plo_fire"); }  /* 13075 */
    if (*D == 3 && *E > 30) {                                                  /* 13076 */
        double ax, ay; relic_act_point(o, &ax, &ay);
        obj_t *l = spawn(OT_PL_OLASER, "plo_laser", (float)(int)ax, (float)(int)ay, o->dir & 31, o->layer);
        if (l) { ek_face(l, o->dir & 31); ek_restart_a(l, "plo_laser"); }
        *E = 0; *D += 1; snd_play_orig(327, 220, 128);
    }
    if (*D == 4 && *E > 20) { ek_restart_a(o, "plo_close"); *D = 1; *E = -80; }   /* 13077 */
    if (DEFEATED(o) && *D != 6) {                                              /* 13081 */
        *D = 6; snd_play_orig(29, 220, 128); *E = 0;
        keystone_spawn(o->x, o->y + 14, -3, 0); obj_effect(o->x, o->y + 14, "explosion06"); ek_set_a(o, "plo_dead");
    }
    if (*D == 6 && *E <= 4 && g3() % 2 == 0) {                                 /* 13082 */
        *E += 1; obj_effect(floorf(o->x) - 5 + ((R(32768) * 11) >> 15), floorf(o->y) + 14 - 8 + ((R(32768) * 17) >> 15), "explosion06");
    }
}
static void olaser_frame(obj_t *l) {                                           /* 13078-13080 */
    if (l->an.frame > 5) l->x = (float)(int)(floorf(l->x) + ((l->dir & 31) == 16 ? -8 : 8));
    if (!in_view(l, 200)) l->alive = 0;
}

/* ================================================================ EnemyWallStinger_3056 + Spawn_3057 (RE 13058-13066, start 13106).
 * Ctor A 4, E 2. av: [0] D heading (deg), [1] E speed, [2] F / [3] G float position, [4] H turn timer, [5] I / [6] J home; flag 1
 * woken (in view 60: F, G = position, D = R(360), H = R(50)), flag 2 the turn side. It burrows through the rock at 2 px a frame,
 * turning 4 deg a frame (the side flips every 50 + R(50) frames), drawn rotated. Out of the rock in view (60) one random wall
 * stinger a frame becomes a stinger (heading up, D <= 180: climbing back to y - 100 (D 4, H 10); down: home there) and leaves a
 * spawn marker at its home; defeated it becomes a defeated stinger with its knock-back. A marker 200 px off view makes a new
 * wall stinger there. Harmless itself (no contact damage). */
static void wstinger_init(obj_t *o) { o->A = 4; o->av[1] = 2; o->av[5] = floorf(o->x); o->av[6] = floorf(o->y); ek_set_a(o, "plw_fly"); }
static void wstinger_frame(obj_t *o) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gy = &o->av[3], *H = &o->av[4];
    if (!(o->fl & FL1) && in_view(o, 60)) {                                    /* 13058 */
        o->fl |= FL1; *F = floorf(o->x); *Gy = floorf(o->y); *D = (R(32768) * 360) >> 15; *H = (R(32768) * 50) >> 15;
    }
    if (!(o->fl & FL1) || DEFEATED(o)) return;
    o->x = (float)(int)*F; o->y = (float)(int)*Gy;                             /* 13059 */
    *F += ek_cos(*D) * *E; *Gy -= ek_sin(*D) * *E; *H += 1; o->ang = (float)ek_nrm(*D);
    *D += (o->fl & FL2) ? -4 : 4;                                              /* 13060 / 13061 */
    if (*H > 50) { o->fl ^= FL2; *H = (double)(int)-((R(32768) * 50) >> 15); }   /* 13062 */
}
static void wspawn_new(const obj_t *w) {
    obj_t *m = spawn(OT_PL_WSPAWN, "plw_spawn", (float)(int)w->av[5], (float)(int)w->av[6], 0, w->layer);
    if (m) m->hidden = 1;                                                      /* (16 x 16 marker: INFERRED not drawn) */
}
static void wstinger_group(void) {                                             /* 13063-13066 */
    obj_t *o, *c[PL_LMAX]; int n;
    for (int up = 1; up >= 0; up--) {
        n = 0;
        EACH(OT_PL_WSTINGER, o) if ((o->fl & FL1) && !DEFEATED(o) && in_view(o, 60) && !obj_overlaps_bg(o) && (up ? ek_nrm(o->av[0]) <= 180 : ek_nrm(o->av[0]) > 180)) c[n++] = o;
        if (!(o = pick(c, n)) || S.nstingers >= PL_LMAX - 8) continue;
        obj_t *s = stinger_new(floorf(o->x), floorf(o->y), o->layer);
        if (s) { S.nstingers++; if (up) { s->av[0] = 4; s->av[4] = 10; s->av[3] = floorf(o->y) - 100; } }
        wspawn_new(o); o->alive = 0;
    }
    n = 0;
    EACH(OT_PL_WSTINGER, o) if (DEFEATED(o)) c[n++] = o;                        /* 13065: one random defeated wall stinger a frame */
    if ((o = pick(c, n))) {
        obj_t *s = stinger_new(floorf(o->x), floorf(o->y), o->layer);
        if (s) { s->fl |= 0x40; s->kx = o->kx; s->ky = o->ky; s->A = 0; s->av[0] = 5; ek_set_a(s, "pls_dead"); S.nstingers++; }
        wspawn_new(o); o->alive = 0;
    }
    EACH(OT_PL_WSPAWN, o) if (!in_view(o, 200)) {                              /* 13066 */
        obj_t *w = spawn(OT_PL_WSTINGER, "plw_fly", o->x, o->y, 0, o->layer);
        if (w) wstinger_init(w);                                               /* (its home: the marker, INFERRED; 13106 only runs at start) */
        o->alive = 0;
    }
}
/* EnemyWallStingerBack_3058 + WallStingerLimitTop / Bottom (RE 13088-13099): the swarm drawn behind the cave wall (json layer 1),
 * scenery (no collision). Positions relative to the top limit: woken at once (F, G = offset, D = R(360), H = R(50)), the same
 * burrowing flight; above the top limit heading up (or below the bottom heading down) it skips the side flip (flag 3) and H - 1;
 * 50 px past a limit it is pushed back 2 px a frame; it wraps with the view (x < view - 50: F + 80 + W; x > view + 476: F - 80 - W). */
static void wback_frame(obj_t *o, const obj_t *top, const obj_t *bot) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gy = &o->av[3], *H = &o->av[4], *LX = &o->av[5], *LY = &o->av[6];
    double tx = floorf(top->x), ty = floorf(top->y), by = bot ? floorf(bot->y) : ty + 281;
    o->fl &= ~FL3;                                                             /* 13088 */
    if (!(o->fl & FL1)) {                                                      /* 13095 */
        o->fl |= FL1; *F = *LX - tx; *Gy = *LY - ty; *D = (R(32768) * 360) >> 15; *H = (R(32768) * 50) >> 15;
    }
    double y = *LY, x = *LX, d = ek_nrm(*D);
    if (y < ty && d < 180) { o->fl |= FL3; *H -= 1; }                          /* 13089 */
    if (y > by && d > 180) { o->fl |= FL3; *H -= 1; }                          /* 13090 */
    if (y < ty - 50) *Gy += 2;                                                 /* 13091 */
    if (y > by + 50) *Gy -= 2;                                                 /* 13092 */
    if (x < G.camx - 50) *F += 80 + SCREEN_W;                                  /* 13093 (global 16 = the view width, INFERRED) */
    if (x > G.camx + 476) *F -= 80 + SCREEN_W;                                 /* 13094 */
    *LX = (int)(tx + *F); *LY = (int)(ty + *Gy);                               /* 13096 */
    *F += ek_cos(*D) * *E; *Gy -= ek_sin(*D) * *E; *H += 1; o->ang = (float)ek_nrm(*D);
    *D += (o->fl & FL2) ? -4 : 4;                                              /* 13097 / 13098 */
    if (*H > 50 && !(o->fl & FL3)) { o->fl ^= FL2; *H = (double)(int)-((R(32768) * 50) >> 15); }   /* 13099 */
    o->x = (float)*LX; o->y = (float)*LY;
}

/* ================================================================ hurts (collision flags of test_collisions_50..55) */
void pl_enemy_pre(player_t *p) {
    if (!S.on) return;
    obj_t *o, *t;
    S.nstingers = list_alive(OT_PL_STINGER);                                   /* the group events (pick one of, the wrap) once a frame */
    if (S.nshk) shocks_frame();
    stinger_group(p);
    if (LST[OT_PL_WSTINGER - RT0].n || LST[OT_PL_WSPAWN - RT0].n) wstinger_group();
    if (LST[OT_PL_WBACK - RT0].n) {                                            /* the back swarm wraps with the view: run every instance */
        obj_t *t = NULL, *b = NULL;
        EACH(OT_PL_WLTOP, o) { t = o; break; }
        EACH(OT_PL_WLBOT, o) { b = o; break; }
        if (t) EACH(OT_PL_WBACK, o) wback_frame(o, t, b);
    }
    EACH(OT_PL_GAS, o) {
        if (o->av[0] >= 65) continue;
        if (ek_csens_hit(p, o) && ek_r_hurt(p, 5, 1)) { p->tint = 90; p->tint_argb = 0xffff80ff; }   /* col_11_50 */
        EACH(OT_PL_TERREON, t)                                                 /* col_6_51 */
            if (t->c <= 0 && !(t->fl & 0x60) && fabsf(t->x - o->x) < 96 && fabsf(t->y - o->y) < 96 && obj_obj_hit(o, t)) {
                snd_play_orig(24, 220, 128); t->c = 5; t->A -= 3; t->inv = 1;
            }
    }
    EACH(OT_PL_OLASER, o) if (ek_csens_hit(p, o)) ek_r_hurt(p, 5, 0);         /* col_3_53 -> 13024 */
    EACH(OT_PL_TERREON, t) {                                                   /* 12928: the spiky top */
        if (t->av[0] >= 7 || !ek_p326_hit(t) || ek_py(p) >= floorf(t->y)) continue;   /* (the selection keeps Player_326 Y < its Y) */
        int M = player_orig_m(p);
        if (M != 26 && M != 99) ek_r_hurt(p, 10, 0);
    }
}

/* ================================================================ spawn / passes */
static void pair(int child, int owner) {
    obj_t *o, *q;
    EACH(child, o) { float best = 1e9f; EACH(owner, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->par = (int)(q - G.o); } } }
}
void pl_enemy_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); }   /* host search snapshots */
    memset(&S, 0, sizeof S); memset(LST, 0, sizeof LST);
    for (int t = OT_PLE_FIRST; t <= OT_PLE_LAST && !S.on; t++) S.on = objects_have(t);
    if (!S.on) return;
    S.swarm = objects_have(OT_PL_WSTINGER) || objects_have(OT_PL_WBACK);      /* A4 / A5 (events 13057, 13085-13087: frames 54 / 55) */
    ek_limiters_collect(&S.lim);
    int kst = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_PLE_FIRST || o->type > OT_PLE_LAST) continue;
        int a = lv_word(o);
        int top = o->type == OT_PL_WLTOP || o->type == OT_PL_WLBOT;             /* (markers at x -116: kept) */
        if (!top && (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h)) { o->alive = 0; continue; }   /* off-map templates */
        memset(o->av, 0, sizeof o->av); o->st = o->tm = 0; o->fl = 0; o->dir = 0; o->an.b = NULL; o->par = -1; o->hidden = 0;
        o->A = 0; o->c = 0; o->bd = 0; o->absorbed = 0; o->ang = 0; o->scale = 0;
        o->layer = (a & 0x8000) ? -1 : (a >> 8) & 0x3f;
        if ((a & 0x8000) && !(a & 0x4000)) o->fl |= FLS10;                    /* json layer 1 scrolling at 1 (drawn by pl_enemy_draw(-1)) */
        list_add(o);
        switch (o->type) {
        case OT_PL_STINGER: stinger_init(o, kst++); S.nstingers++; break;
        case OT_PL_NEST: o->A = 25; o->av[3] = 15; o->fl |= F_NOFLY; o->av[0] = floorf(o->x); o->av[1] = floorf(o->y); ek_set_a(o, "pln_nest"); break;   /* ctor + 12852 */
        case OT_PL_SUNFLORA: o->A = 12; o->av[2] = 1; o->fl |= F_NOFLY; ek_set_a(o, "plf_turn"); ek_force_frame(o, 6); break;   /* ctor + 12853 */
        case OT_PL_TERREON: terreon_init(o); break;
        case OT_PL_TLEG: o->A = 50; o->fl |= 0x20 | F_NOFLY; o->hidden = 1; ek_set_a(o, "plt_ll"); break;   /* ctor A 50; 12961 flag 5 */
        case OT_PL_TRLEG: o->A = 50; o->fl |= 0x20 | F_NOFLY; o->hidden = 1; ek_set_a(o, "plt_rl"); break;
        case OT_PL_TLSH: o->hidden = 1; ek_set_a(o, "plt_ls"); break;
        case OT_PL_TRSH: o->hidden = 1; ek_set_a(o, "plt_rs"); break;
        case OT_PL_TWARN: o->hidden = 1; ek_set_a(o, "plt_warn"); break;
        case OT_PL_OBELISK: o->A = 20; o->av[2] = -1; o->fl |= F_NOFLY; o->dir = 16; ek_set_a(o, "plo_idle"); break;   /* ctor + 13107 */
        case OT_PL_WSTINGER: wstinger_init(o); break;
        case OT_PL_WBACK: o->av[1] = 2; o->A = 4; o->hidden = (o->fl & FLS10) ? 1 : 0; o->av[5] = floorf(o->x); o->av[6] = floorf(o->y); ek_set_a(o, "plw_back"); break;  /* ctor A 4, E 2 */
        case OT_PL_WLTOP: case OT_PL_WLBOT: o->hidden = 1; break;                /* invisible markers (no row) */
        default: o->alive = 0; break;                                            /* EnemySleepGas templates: the sunflora creates them */
        }
    }
    pair(OT_PL_TLEG, OT_PL_TERREON); pair(OT_PL_TRLEG, OT_PL_TERREON); pair(OT_PL_TLSH, OT_PL_TERREON); pair(OT_PL_TRSH, OT_PL_TERREON);
    pair(OT_PL_TWARN, OT_PL_TERREON);
}
/* the terreon rigs in their draw order (12906: legs and shoulders move_back) and the sleep gas at its blend alpha */
static void draw_q(const obj_t *q) {
    if (!q || !q->an.b || !q->alive) return;
    float sx = floorf(q->x - G.camx) + (q->flip == 2 ? 1 : 0), sy = floorf(q->y - G.camy);
    if (q->inv) r_set_invert(1);
    if (q->ang != 0) frame_draw_rot(q->an.b, anim_frame(&q->an), sx, sy, q->ang, q->flip, 0xffffffff);
    else frame_draw(q->an.b, anim_frame(&q->an), sx, sy, q->flip, 0xffffffff);
    if (q->inv) r_set_invert(0);
}
void pl_enemy_draw(int pass) {
    if (!S.on) return;
    obj_t *o;
    if (pass == -1) {                                                          /* game.c, the back pass: json layer 1 behind the json-2 pieces */
        EACH(OT_PL_WBACK, o) if (o->fl & FLS10) {                              /* (PL's layers all scroll at 1: RE Layer::init, on_frame_52..56_init) */
            float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
            if (sx > -48 && sx < SCREEN_W + 48 && sy > -48 && sy < SCREEN_H + 48) draw_q(o);
        }
        return;
    }
    if (pass != 1) return;
    EACH(OT_PL_TERREON, o) {
        float ox = floorf(o->x - G.camx), oy = floorf(o->y - G.camy);
        if (ox < -160 || ox > SCREEN_W + 160 || oy < -160 || oy > SCREEN_H + 160 || o->hidden == 2) continue;
        draw_q(part_of(o, OT_PL_TLEG)); draw_q(part_of(o, OT_PL_TRLEG)); draw_q(part_of(o, OT_PL_TLSH)); draw_q(part_of(o, OT_PL_TRSH));
        draw_q(o);
    }
    EACH(OT_PL_GAS, o) {
        float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
        if (sx < -40 || sx > SCREEN_W + 40 || sy < -40 || sy > SCREEN_H + 40 || !o->an.b) continue;
        frame_draw(o->an.b, anim_frame(&o->an), sx, sy, 0, ((uint32_t)gas_alpha(o) << 24) | 0xffffff);
    }
}
int pl_enemy_obj(obj_t *o, player_t *p) {
    if (!S.on) return 0;
    switch (o->type) {
    case OT_PL_STINGER: stinger_frame(o, p); return 1;
    case OT_PL_NEST: nest_frame(o, p); return 1;
    case OT_PL_SUNFLORA: sunflora_frame(o, p); return 1;
    case OT_PL_GAS: gas_frame(o); return 1;
    case OT_PL_CONFETTI: confetti_frame(o); return 1;
    case OT_PL_TERREON: terreon_frame(o, p); return 1;
    case OT_PL_TLEG: case OT_PL_TRLEG: case OT_PL_TLSH: case OT_PL_TRSH: tpart_frame(o); return 1;
    case OT_PL_TWARN: if (!ek_partner(o)) o->alive = 0; return 1;
    case OT_PL_TDEBRIS: tdebris_frame(o); return 1;
    case OT_PL_OBELISK: obelisk_frame(o); return 1;
    case OT_PL_OLASER: olaser_frame(o); return 1;
    case OT_PL_WSTINGER: wstinger_frame(o); return 1;
    case OT_PL_WSPAWN: return 1;
    case OT_PL_WBACK: return 1;                                            /* (pl_enemy_pre: every instance, asleep or not) */
    default: return 1;
    }
}
