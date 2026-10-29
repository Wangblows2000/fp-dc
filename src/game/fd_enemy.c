/* Final Dreadnought enemies (session 24): types 2624-2751 (fd.h). Spec: build/re/spec_fd_enemies.md.
 * New classes: EnemyPounder_3423 (FD1 / FD3), the gatling gun EnemyGatlingGun_3669 + its five parts (FD3 / FD4a), EnemyBeastIris_3735
 * (FD4a), EnemySpiderBeamer_3755 (FD4a / FD4b), EnemyEliteSpawnerSpawner_3453 (FD1a's pod rain).
 * Reused, not copied: the EliteSpawner pods are Battle Glacier's (bg_enemy.c; this module gives FD1's payload codes 1-4, RE 15162-15164
 * + foreach_spawner_65 / 67, 15246-15249, and FD1a's pod rain 15287 through bg_enemy_spawner_drop), the payloads and the other placed
 * enemies are their home modules' (tb_enemy.c tank bots, relic.c hoppers / fire spitters, sb_enemy.c HopperMkII, jc_enemy.c Elites +
 * the 14408 view alert), the pounders' HazardShockwave_1576 is Relic Maze's (relic_shock_new: movement 6.25 px, R-hurt while B <= 50),
 * the gatlings' shots are the Elite's TrooperLaser_2530 (jc_enemy_laser_create), the spiders' the seeker's EnemyHomingBullet_2648
 * (tb_enemy_bullet_create); hits / defeat / contact damage B (o->bd) / fly-away are combat.c's shared Q20 rules (Lilac, Carol, Milla).
 * Labels: RE (decompiled events of handle_frame_66..73 / on_frame_66..73_start_events / test_collisions_65: 15255-15271, 15313,
 * 16052; 16053-16088, 16181-16183, 16202, 16729, loop_gd_69, foreach_x_69; 14761-14780; 16690-16698, 16805-16808; 15287; create_* /
 * class constructors; on_frame_66..73_init qualifier 20), RV (none recorded: FD needs the save to reach), INFERRED.
 * Performance: per-type lists (enemy_kit.h); nothing runs without these types (S.on); the only scans are at spawn (the pods' codes). */
#include "fd.h"
#include "bg.h"
#include "jc.h"
#include "tb.h"
#include "enemy_kit.h"

#define DEFEATED(o) ((o)->fl & 0x40)
#define FL1 0x1000                                   /* the class's flag 1 */
#define FL2 0x2000                                   /* flag 2 */
#define F_ARMOURED 0x20                              /* flag 5: hits deflect (combat.c) */
#define FLLIM 0x4000                                 /* (port) on a limiter last frame: test_collisions' edge trigger */
#define NSLOT 11                                     /* 2624-2633 + the runtime debris */
static int slot(int t) { return t == OT_FDE_GDEB ? 10 : t - OT_FDE_POUNDER; }
static ek_list_t LST[NSLOT];
#define EACH(T, OB) EK_EACH(&LST[slot(T)], T, OB)
#define NGUN 24
typedef struct {                                     /* a gatling gun's alterables (more than av[8]) and its parts (RE 16053 pairing) */
    short g, part[5];                                /* gun, parts: gun2, barrel, left, right, table (object indices, -1 none) */
    double D, E, F, Gx, H, I, J, K, L, M, N, O, P, Q, R;
    int fl1, fl2, latch[2], dormant;
} gat_t;
enum { P_GUN2, P_BAR, P_LEFT, P_RIGHT, P_TABLE };
static struct {
    int on, stage, act, pick_frame;
    int rain, rain_hook;                             /* FD1a pod rain (the subtitle object's C, 15284) */
    int alarm_a, alarm_b;                            /* FD3a: ObjectSiren_3362's A / B (16181 / 15803 / 16183) */
    int ngun; gat_t gun[NGUN];
    short shk[32]; int nshk;                         /* the pounders' shockwaves (B + 1 a frame, 15271) */
    ek_limiters_t lim;
} S;

static obj_t *spawn(int type, const char *anim, float x, float y) { return ek_spawn(&LST[slot(type)], type, anim, x, y, 0); }
static int rnd(int n) { return cross_rand(n); }
static obj_t *obj_at(int i) { return i >= 0 && i < MAX_OBJS && G.o[i].alive ? &G.o[i] : NULL; }

int fd_enemy_q20(int t) {                                                         /* RE on_frame_66..73_init qualifier 20 */
    return t == OT_FDE_POUNDER || t == OT_FDE_GUN || t == OT_FDE_IRIS || t == OT_FDE_SPIDER;
}

/* ================================================================ EliteSpawner pods (bg_enemy.c): FD1's payloads (RE)
 * Start (foreach_spawner_65 in FD1, 15312 after 14724; foreach_spawner_67 in FD2, 15682): every placed TankBot (code 1), Hopper (2),
 * FireSpitter (3), HopperMkII (4) with |x - pod x| <= 7 and A - 50 < y < A (A: the pod's landing y) is destroyed and the pod's C =
 * the code. FD1 (15246-15249): a landed pod (B > 1) with C 1-4 releases that enemy at (x, y - 38 / 31 / 15 / 31) and goes; C 0 an
 * Elite at B > 30 (15245). FD2 runs only BG5's 14661 (an Elite whatever C): its tank bots under pods just go (C left 0 here). */
static int payload(obj_t *s, int code) {
    if (S.stage != 10) return 0;
    float x = floorf(s->x), y = floorf(s->y);
    obj_t *o = NULL;
    switch (code) {
    case 1: o = tb_enemy_create(OT_TBE_TANK, x, y - 38); if (o) o->fl |= F_NOFLY; break;           /* 15246: flag 16 */
    case 2: o = relic_enemy_create(OT_HOPPER, x, y - 31); break;                                    /* 15247: H, I = its x, y */
    case 3: o = relic_enemy_create(OT_FSPITTER, x, y - 15); if (o) { o->av[0] = -1; o->dir = 16; } break;   /* 15248: D -1 */
    case 4: o = sb_enemy_hopper_create(x, y - 31); break;                                           /* 15249: D = E = y */
    default: return 0;
    }
    if (o) o->layer = s->layer;
    return o != NULL;
}
static void pod_codes(void) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *s = &G.o[i];
        if (!s->alive || s->type != OT_BGE_SPAWNER || s->x < 0 || s->y < 0) continue;
        double A = floorf(s->y);                                                   /* (14724 has not run yet: y is the landing y) */
        for (int j = 0; j < G.nobjs; j++) {
            obj_t *e = &G.o[j];
            int code = e->type == OT_TBE_TANK ? 1 : e->type == OT_HOPPER ? 2 : e->type == OT_FSPITTER ? 3 : e->type == OT_SB_HOPPER ? 4 : 0;
            if (!e->alive || !code || (S.stage == 11 && code != 1)) continue;
            if (fabsf(floorf(e->x) - floorf(s->x)) > 7 || !(floorf(e->y) > A - 50 && floorf(e->y) < A)) continue;
            e->alive = 0;
            if (S.stage == 10) s->av[BGE_SPAWNER_C] = code;
        }
    }
}
/* 15287 (FD1a): while the subtitle object's C > 0, on g3 % 10 == 5, one random drop point within 199 px of the player in x goes
 * (sound 345) and a pod appears there, shown and already landing (B 1, A = its y). The dialog script is flow's (15273-15288: C = 1
 * at its step 3, B 160, long before the drop points at x 11615-12511); a run that starts from the act start (g4 < 2) reaching x 800
 * starts the dialog: the rain is on from there (INFERRED; fd_enemy_pod_rain is the exact hook). */
void fd_enemy_pod_rain(int on) { S.rain_hook = on; S.rain = on; }
static void pod_rain(player_t *p) {
    if (!S.rain && !S.rain_hook && S.stage == 10 && S.act == 1 && G.at_g4 < 2 && ek_px(p) > 800) S.rain = 1;
    if (!S.rain || G.frame % 10 != 5) return;
    obj_t *o, *c[EK_LMAX]; int n = 0; double px = ek_px(p);
    EACH(OT_FDE_PODSPOT, o) if (floorf(o->x) >= px - 199 && floorf(o->x) <= px + 199) c[n++] = o;
    if (!n) return;
    o = c[rnd(n)];
    o->alive = 0; snd_play_orig(345, 220, 128);
    obj_t *s = bg_enemy_spawner_drop(floorf(o->x), floorf(o->y), 0);
    if (s) s->layer = o->layer;
}

/* ================================================================ EnemyPounder_3423 (RE 15255-15271, 15313, 16052, foreach_pound_65,
 * col_18_65). Ctor A 25, direction 8; start E = y (15313). av: [0] D (0 walking, 1 slamming, 2 rising, 4 beaten), [1] E home y,
 * [2] F timer, [3] Q explosions. D 0: walks 2 px a frame (left, flag 1 right), F counts up to 0; an EnemyLimiter turns it (flag 1
 * toggles as it touches one); with F >= 0 and the player within 99 px in x and 0..199 px below it (y within -199 / +63) it points
 * down (direction 24, D 1). D 1: F + 1; past 20 it falls 16 px a frame; on the background: two shockwaves (directions 0 / 16, B 40)
 * at its action point, sound 132, CAM.Z 5, D 2, F 0. D 2: out of the background 8 px a frame, F + 1, past 50 back up 2 px a frame
 * to its home y: D 0, F -30. Beaten: anim 8 (D 4), explosion06 every 6th frame (Q + 1), at Q 8 a keystone, explosion01, sound 38,
 * debris, gone. Its own contact does not hurt (no B, RE); the shockwaves R-hurt by their A (10) while B <= 50. */
static void shock_add(obj_t *k) {
    int i = (int)(k - G.o);
    for (int j = 0; j < S.nshk; j++) if (!G.o[S.shk[j]].alive || G.o[S.shk[j]].type != OT_SHOCK) { S.shk[j] = (short)i; return; }
    if (S.nshk < 32) S.shk[S.nshk++] = (short)i;
}
static void pounder_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Q = &o->av[3];
    if (DEFEATED(o)) {
        if (*D != 4) { ek_set_ad(o, "fdp_dead"); *D = 4; }                          /* 15267 */
        else ek_set_ad(o, "fdp_dead");
        if (G.frame % 6 == 0 && *Q < 8) {                                          /* 15268 */
            obj_effect(floorf(o->x) - 20 + rnd(41), floorf(o->y) - 20 + rnd(41), "explosion06"); *Q += 1;
            snd_play_orig(ek_explode_snd(), 220, 128);
        }
        if (*Q >= 8) {                                                             /* 15269 / 16052 */
            keystone_spawn(o->x, o->y, 0, 0); obj_effect(o->x, o->y, "explosion"); ek_debris(o, 4);
            snd_play_orig(38, 220, 128); o->alive = 0;
        }
        return;
    }
    int lim = ek_limiter_hit(&S.lim, o);                                           /* col_18_65 (edge: while D 0) */
    if (*D == 0 && lim && !(o->fl & FLLIM)) o->fl ^= FL1;
    o->fl = lim ? (o->fl | FLLIM) : (o->fl & ~FLLIM);
    double X = floorf(o->x), Y = floorf(o->y), px = ek_px(p), py = ek_py(p);
    if (*D == 0 && !(o->fl & FL1)) { ek_face_d(o, 16); o->x = (float)(X - 2); }    /* 15255 */
    if (*D == 0 && (o->fl & FL1)) { ek_face_d(o, 0); o->x = (float)(X + 2); }      /* 15256 */
    if (*D == 0 && *F < 0) *F += 1;                                                /* 15257 */
    else if (*D == 0 && !ek_dead(p) && X <= px + 99 && X >= px - 99 && Y >= py - 199 && Y <= py + 63) { ek_face_d(o, 24); *D = 1; }   /* 15258 */
    if (*D == 1) *F += 1;                                                          /* 15259 */
    if (*D == 1 && *F > 20) o->y = (float)(floorf(o->y) + 16);                     /* 15260 */
    if (*D == 1 && obj_overlaps_bg(o)) {                                           /* 15261 / foreach_pound_65 */
        double ax, ay; relic_act_point(o, &ax, &ay);
        for (int k = 0; k < 2; k++) {
            obj_t *w = relic_shock_new(ax, ay, k ? 16 : 0, 0);
            if (w) { w->av[1] = 40; w->layer = o->layer; shock_add(w); }            /* B 40 */
        }
        snd_play_orig(132, 220, 128); ek_shake(5); *D = 2; *F = 0;
    }
    if (*D == 2 && obj_overlaps_bg(o)) o->y = (float)(floorf(o->y) - 8);          /* 15262 */
    if (*D == 2) *F += 1;                                                          /* 15263 */
    if (*D == 2 && *F > 50) { ek_face_d(o, (o->fl & FL1) ? 0 : 16); o->y = (float)(floorf(o->y) - 2); }   /* 15264 / 15265 */
    if (*D == 2 && *E >= floorf(o->y)) { *D = 0; *F = -30; o->y = (float)(int)*E; }   /* 15266 */
    ek_set_ad(o, "fdp");
}
static void shocks_frame(void) {                                                  /* 15271: B + 1 (12924 removes past 50: relic_boss.c) */
    for (int j = 0; j < S.nshk; j++) { obj_t *k = &G.o[S.shk[j]]; if (k->alive && k->type == OT_SHOCK && k->born != G.frame) k->av[1] += 1; }
}

/* ================================================================ EnemyGatlingGun_3669 + parts (RE 16053-16088, 16202 / 16729,
 * loop_gd_69). Ctor A 20, D 270, F G -24, J -64, K 90; start: flag 16, P Q = x y, S 10, FD3 (16202) D = 90 + 180 k (k: instance
 * index), K = D - 180, FD4a (16729) D -90, K 90; R = D. FD3a (start, g4 < 3): flag 4 (dormant) until the alarm (16183).
 * H 0: hidden in its hatch (C 5: no hits); the player within 199 px in x, the gun 159 px above to 119 px below him (not dormant): H 1
 * (J + 4 a frame: the table slides out), J 64: sound 254, H 2 (C 0): 16 frames 8 px a frame back along D, H 3: turns 180 deg (D + 10,
 * 18 frames), H 4: the barrels come out (E + 2, F G + 4, 6 frames), H 5: aims at the player (D turns at most 2 deg a frame toward
 * his last position), the barrels spin (L + 20), I = (I + 1) mod 10, O = (O + 1) mod 200: while O < 120, at I 3 the left barrel
 * recoils (F -24, E 6) and fires, at I 8 the right one (G -24): a TrooperLaser from the barrel's tip (speed 12, sound 376, CAM.Z 3).
 * More than 400 px off the view (H < 10) it resets. Beaten (H 10): parts flash (frame (I / 2) mod 2), every I % 7 == 1 an explosion,
 * at I 60: sound 38, a keystone, four debris pieces, the parts go (H 11; smoke every 10th frame in view).
 * Parts (16053, each frame first): gun2 at the gun, barrel at E along D, left / right at F / G along D and -/+ 16 cos L across it
 * (the barrels' spin), table at J along K; all turned by D (table K). */
static gat_t *gat_of(const obj_t *g) { int i = (int)(g - G.o); for (int k = 0; k < S.ngun; k++) if (S.gun[k].g == i) return &S.gun[k]; return NULL; }
static void gat_reset(gat_t *t, obj_t *g) {                                       /* 16056 */
    t->H = 0; g->x = (float)(int)t->P; g->y = (float)(int)t->Q; t->E = 0; t->F = -24; t->I = 0; t->O = 0; t->D = t->R; t->L = 0; t->Gx = -24; t->J = -64;
}
static void gat_parts(gat_t *t, obj_t *g) {                                       /* 16053 */
    double gx = floorf(g->x), gy = floorf(g->y), c = ek_cos(t->D), s = ek_sin(t->D), cl = ek_cos(t->L);
    for (int k = 0; k < 5; k++) {
        obj_t *q = obj_at(t->part[k]);
        if (!q) continue;
        double x = gx, y = gy, a = t->D;
        switch (k) {
        case P_GUN2: break;
        case P_BAR: x = gx + c * t->E; y = gy - s * t->E; break;
        case P_LEFT: x = gx + c * t->F - 16 * cl * s; y = gy - s * t->F - 16 * cl * c; break;    /* (y: INFERRED as x, the decompiler lost it) */
        case P_RIGHT: x = gx + c * t->Gx + 16 * cl * s; y = gy - s * t->Gx + 16 * cl * c; break;
        case P_TABLE: a = t->K; x = gx + ek_cos(t->K) * t->J; y = gy - ek_sin(t->K) * t->J; break;
        }
        q->x = (float)(int)x; q->y = (float)(int)y; q->ang = (float)ek_nrm(a); q->hidden = t->H >= 11;
        q->av[0] = k == P_LEFT ? t->fl1 : k == P_RIGHT ? t->fl2 : t->H;            /* parts' A: the gun's flag 1 / 2 or H; B: H or C */
        q->av[1] = (k == P_LEFT || k == P_RIGHT) ? t->H : g->c;
    }
}
static void gat_fire(gat_t *t, int side) {                                        /* 16069-16072 */
    obj_t *q = obj_at(t->part[side ? P_RIGHT : P_LEFT]);
    int A = side ? t->fl2 : t->fl1;
    if (!q) return;
    if (A == 1 && !t->latch[side]) {
        double ax, ay; relic_act_point(q, &ax, &ay);
        obj_t *l = jc_enemy_laser_create((float)ax, (float)ay, 12, t->D);           /* (angle: along the barrel, INFERRED; RE B = -angle) */
        if (l) l->layer = q->layer;
        t->latch[side] = 1; snd_play_orig(376, 220, 128); ek_shake(3);
    } else if (t->latch[side] && A == 0) t->latch[side] = 0;
}
static void gun_frame(obj_t *g, player_t *p) {
    gat_t *t = gat_of(g);
    if (!t) return;
    gat_parts(t, g);
    double gx = floorf(g->x), gy = floorf(g->y), px = ek_px(p), py = ek_py(p);
    if (t->H == 0) g->c = 5;                                                       /* 16054 */
    if (t->H == 0 && !t->dormant && gx >= px - 199 && gx <= px + 199 && gy >= py - 159 && gy <= py + 119) t->H = 1;   /* 16055 */
    if (t->H < 10 && !obj_near_view(g, 400)) gat_reset(t, g);                      /* 16056 */
    if (t->H == 1) t->J += 4;                                                      /* 16057 */
    if (t->H == 1 && t->J >= 64) { g->c = 0; t->J = 64; t->H = 2; snd_play_orig(254, 220, 128); g->inv = 0; }   /* 16058 */
    if (t->H == 2) {                                                               /* 16059 */
        g->x = (float)(int)(floorf(g->x) - ek_cos(t->D) * 8); g->y = (float)(int)(floorf(g->y) + ek_sin(t->D) * 8); t->I += 1; t->J -= 8;
    }
    if (t->H == 2 && t->I >= 16) { t->I = 0; t->H = 3; }                           /* 16060 */
    if (t->H == 3) { t->D += 10; t->I += 1; }                                      /* 16061 */
    if (t->H == 3 && t->I >= 18) { t->I = 0; t->H = 4; }                           /* 16062 */
    if (t->H == 4) { t->E += 2; t->F += 4; t->Gx += 4; t->I += 1; }                /* 16063 */
    if (t->H == 4 && t->I >= 6) { t->I = 0; t->H = 5; }                            /* 16064 */
    if (t->H == 5) {                                                               /* 16065 */
        gx = floorf(g->x); gy = floorf(g->y);
        t->I = fmod(t->I + 1, 10); t->F = fmin(t->F + 6, 0); t->Gx = fmin(t->Gx + 6, 0); t->L = fmod(t->L + 20, 360); t->E = fmin(t->E + 3, 12);
        double a = atan2(-(gy - t->N), gx - t->M) * EK_RAD; if (a < 0) a += 360;
        double d = fmod(((int)a - 180) - t->D + 540, 360); if (d < 0) d += 360;
        d -= 180; if (d > 2) d = 2; if (d < -2) d = -2;
        t->D += d; t->M = px; t->N = py; t->O = fmod(t->O + 1, 200);
    }
    if (t->H == 5 && t->D > 360) t->D -= 360;                                      /* 16066 */
    if (t->H == 5 && t->I == 3 && t->O < 120) { t->F = -24; t->E = 6; t->fl1 = 1; t->fl2 = 0; }   /* 16067 */
    if (t->H == 5 && t->I == 8 && t->O < 120) { t->Gx = -24; t->E = 6; t->fl2 = 1; t->fl1 = 0; }  /* 16068 */
    gat_fire(t, 0); gat_fire(t, 1);
    for (int k = P_GUN2; k <= P_BAR; k++) {                                        /* 16073 / 16074: hit frame while 1 < H < 10 */
        obj_t *q = obj_at(t->part[k]);
        if (q && t->H > 1 && t->H < 10) ek_force_frame(q, g->c > 1 ? 1 : 0);
    }
    if (DEFEATED(g) && t->H < 10) { t->H = 10; t->I = 0; }                         /* 16076 */
    if (t->H == 10) {                                                              /* 16077-16082 */
        t->I += 1;
        for (int k = 0; k < 4; k++) { obj_t *q = obj_at(t->part[k]); if (q) ek_force_frame(q, (int)(0.5 * t->I) % 2); }
        if ((int)t->I % 7 == 1) {
            snd_play_orig(ek_explode_snd(), 220, 128);
            obj_effect(floorf(g->x) - 20 + rnd(41), floorf(g->y) - 20 + rnd(41), "explosion06");
        }
    }
    if (t->H == 10 && t->I > 60) {                                                 /* 16083 / loop_gd_69 */
        t->H = 11; snd_play_orig(38, 220, 128); keystone_spawn(g->x, g->y, 0, 0);
        for (int k = 0; k < 4; k++) {
            obj_t *d = spawn(OT_FDE_GDEB, "fdg_deb", g->x, g->y);
            if (!d) break;
            d->layer = g->layer; ek_force_frame(d, k); d->ang = (float)ek_nrm(t->D);
            obj_pinball_start(d, rnd(32), 50);                                     /* (ctor: random direction, PinballMovement 50) */
        }
        for (int k = 0; k < 5; k++) { obj_t *q = obj_at(t->part[k]); if (q) q->hidden = 1; }   /* 16085-16088 */
    }
    if (t->H == 11 && obj_near_view(g, 40) && G.frame % 10 == 0) obj_effect(g->x, g->y, "dust");   /* 16084 (direction 5..10: smoke) */
    if (t->H == 11) g->hidden = 1;                                                 /* (INFERRED: the ball goes with its parts) */
    g->av[0] = t->H; g->av[1] = t->D; g->av[2] = t->I; g->av[3] = t->dormant; g->av[4] = t->O;   /* (mirrors for the host tracer / tests) */
}
static void gdeb_frame(obj_t *d) {
    if (d->born != G.frame) obj_pinball_step(d, 25);                               /* (gravity INFERRED as the drill's dirt) */
    if (d->y > G.camy + SCREEN_H + 64 || !obj_near_view(d, 200)) d->alive = 0;
}
void fd_enemy_gatlings_wake(void) { for (int k = 0; k < S.ngun; k++) S.gun[k].dormant = 0; }
int fd_enemy_gatlings_dormant(void) { int n = 0; for (int k = 0; k < S.ngun; k++) n += S.gun[k].dormant != 0; return n; }
/* FD3a's alarm (ObjectSiren_3362 as the script object, RE 16181 / 15803 / 16183): A 0, the player past x 5000 and below y 800, g4 < 3:
 * A 1 (Brevon's voice br_br_pa_05 + subtitles: flow); A 1: B + 1 a frame; B 540: B 541, the gatling guns wake (flag 4 off), sound 310 */
static void alarm_frame(player_t *p) {
    if (S.alarm_a == 0 && ek_px(p) > 5000 && ek_py(p) > 800 && G.at_g4 < 3) S.alarm_a = 1;
    else if (S.alarm_a == 1) S.alarm_b += 1;
    if (S.alarm_a == 1 && S.alarm_b == 540) { S.alarm_b = 541; fd_enemy_gatlings_wake(); snd_play_orig(310, 220, 128); }
}

/* ================================================================ EnemyBeastIris_3735 (RE 14761-14780). Ctor A 10, D 1925, F -6, G 1.
 * av: [0] D landing y, [1] E speed, [2] F fall speed, [3] G state (0 dropping, 1 running, 2 biting, 6 beaten), [4] H bite clock,
 * [5] I turn clock, [6] J fall y. G 0: falls (anim 7, J + F, F + 0.3 up to 15) to y D, G 1. G 1: runs at the player (E -/+ 4, set
 * when 0), x + E, H + 2, I - 1; an EnemyLimiter (I <= 0) turns it (E = -E, I 10); facing the player, H > 20 and on Player_326: G 2,
 * anim 10 (the bite, 19 frames): frames 14-16 hits deflect (flag 5) and B 10 (contact damage) while it faces him; the bite over:
 * G 1, H -80. Beaten: anim 8, the shared fly-away. */
static void iris_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gs = &o->av[3], *H = &o->av[4], *I = &o->av[5], *J = &o->av[6];
    if (DEFEATED(o) && *Gs != 6) *Gs = 6;                                          /* 14779 */
    if (*Gs == 6) { o->bd = 0; o->fl &= ~F_ARMOURED; ek_set_ad(o, "fdi_dead"); return; }   /* 14780 */
    double px = ek_px(p);
    if (*Gs == 0 && *J == 0) *J = floorf(o->y);                                    /* 14761 */
    else if (*Gs == 0) { ek_set_ad(o, "fdi_fall"); o->y = (float)(int)*J; *J += *F; *F = fmin(*F + 0.3, 15); }   /* 14762 */
    if (*Gs == 0 && *F > 0 && !(o->fl & FL1)) { o->fl |= FL1; o->layer = 2; }      /* 14763: the player's layer */
    if (*Gs == 0 && *F > 0 && *J >= *D) { o->y = (float)(int)*D; *Gs = 1; *E = 0; ek_set_ad(o, "fdi"); }   /* 14764 */
    if (*Gs == 1 && *E == 0) *E = floorf(o->x) >= px ? -4 : 4;                     /* 14765 / 14766 */
    if (*Gs == 1) { o->x = (float)(int)(floorf(o->x) + *E); *I = fmax(*I - 1, 0); *H += 2; }   /* 14767 */
    if (*Gs == 1) ek_face_d(o, *E > 0 ? 0 : 16);                                   /* 14768 / 14769 */
    if (*Gs == 1 && *I <= 0 && ek_limiter_hit(&S.lim, o)) { *I = 10; *E = -*E; }   /* 14770 */
    double X = floorf(o->x);
    if (*Gs == 1 && *H > 20 && (((o->dir & 31) == 0 && X < px) || ((o->dir & 31) == 16 && px < X)) && ek_p326_hit(o)) {   /* 14771 / 14772 */
        *Gs = 2; ek_restart_ad(o, "fdi_bite"); o->fl &= ~FL2;
    }
    if (*Gs == 2) {
        int fr = o->an.frame, facing = ((o->dir & 31) == 0 && px > X) || ((o->dir & 31) == 16 && X > px);
        o->fl &= ~F_ARMOURED; o->bd = 0;                                           /* 14773 */
        if (fr == 14 && !(o->fl & FL2)) o->fl |= FL2;                              /* 14774 */
        if (fr > 13 && fr < 17) o->fl |= F_ARMOURED;                               /* 14775 */
        if (fr >= 14 && fr <= 16 && facing) o->bd = 10;                            /* 14776 / 14777 */
        if (o->an.done) { o->fl &= ~F_ARMOURED; *Gs = 1; *H = -80; o->bd = 0; ek_set_ad(o, "fdi"); }   /* 14778 */
    } else if (*Gs == 1) ek_set_ad(o, "fdi");
}

/* ================================================================ EnemySpiderBeamer_3755 (RE 16690-16698, FD4b 16805-16808). Ctor
 * invisible, direction 16, A 15; start flag 16. FD4a: left of the laser tunnel (HazardLaserTunnel_3742 x 2512) it faces right
 * (16698); FD4b: left of x 1600 it points down (direction 24, 16807). av: [0] D (0 hidden, 1 out, 6 beaten), [1] E clock. D 0: hidden,
 * C 5; inside the view in x and within 79 px of the player in y: anim 11 (comes out), D 1, shown, hits deflect (flag 5) until its frame
 * 14 in the view (60). D 1: E + 1; E > 60 (one a frame): E -60, sound 171, a homing bullet from its action point toward the player
 * (direction = angle / 11.25), anim 0, a white burst; E > 20 and not in anim 11: anim 11 at its last frames. Beaten: D 6, sound 29,
 * explosion01, debris, anim 10 (the wreck stays). */
static void spider_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1];
    if (S.stage == 13 && S.act == 1 && (o->dir & 31) == 16 && floorf(o->x) < 2512) ek_face_d(o, 0);     /* 16698 */
    if (S.stage == 13 && S.act == 2 && (o->dir & 31) == 16 && floorf(o->x) < 1600) ek_face_d(o, 24);    /* 16807 */
    if (DEFEATED(o) && *D != 6) {                                                  /* 16696 / 16805 */
        *D = 6; snd_play_orig(29, 220, 128);
        double ax, ay; relic_act_point(o, &ax, &ay); obj_effect((float)ax, (float)ay, "explosion"); ek_debris(o, 4);
        o->fl &= ~F_ARMOURED;
    }
    if (*D == 6) { ek_set_ad(o, "fds_dead"); return; }                             /* 16697 */
    double X = floorf(o->x), Y = floorf(o->y), py = ek_py(p);
    if (*D == 0) { o->hidden = 1; o->c = 5; }                                      /* 16690 */
    if (*D == 0 && X > G.camx && X < G.camx + SCREEN_W && Y >= py - 79 && Y <= py + 79 && !ek_dead(p)) {   /* 16691 / 16808 */
        ek_restart_ad(o, "fds_open"); *D = 1; o->hidden = 0; o->fl |= F_ARMOURED;
    }
    if (*D != 1) return;
    int open = ek_ad_is(o, "fds_open");
    if (!open || o->an.frame <= 14) *E += 1;                                       /* 16692 */
    else if (obj_near_view(o, 60)) { o->fl &= ~F_ARMOURED; *E += 1; }              /* 16693 */
    if (*E > 60 && S.pick_frame != G.frame) {                                      /* 16694 (one random a frame: the first) */
        S.pick_frame = G.frame; *E = -60; snd_play_orig(171, 220, 128);
        double ax, ay; relic_act_point(o, &ax, &ay);
        double a = atan2(ay - py, ek_px(p) - ax) * EK_RAD; if (a < 0) a += 360;
        obj_t *b = tb_enemy_bullet_create((float)ax, (float)ay, (int)(a / 11.25));
        if (b) b->layer = o->layer;
        ek_set_ad(o, "fds"); obj_effect((float)ax, (float)ay, "wburst");
    }
    if (*E > 20 && !ek_ad_is(o, "fds_open")) {                                     /* 16695 */
        ek_set_ad(o, "fds_open"); ek_force_frame(o, 99); o->an.acc = 0;
    }
}

/* ================================================================ spawn / passes */
void fd_enemy_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); bg_enemy_spawner_payload(payload); }
    memset(&S, 0, sizeof S); memset(LST, 0, sizeof LST);
    S.pick_frame = -1;
    const level_def_t *lv = level_cur();
    S.stage = lv ? lv->stage : 0; S.act = lv ? lv->act : 0;
    if (S.stage < 10 || S.stage > 13) return;
    if (S.stage == 10) jc_enemy_elite_view_alert(1);                               /* 14408 (FD1's frames only) */
    if (S.stage == 10 || S.stage == 11) pod_codes();                               /* foreach_spawner_65 / 67 */
    for (int t = OT_FDE_POUNDER; t <= OT_FDE_PODSPOT && !S.on; t++) S.on = objects_have(t);
    if (!S.on) return;
    ek_limiters_collect(&S.lim);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_FDE_POUNDER || o->type > OT_FDE_PODSPOT) continue;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        memset(o->av, 0, sizeof o->av); o->st = o->tm = 0; o->fl = 0; o->dir = 0; o->an.b = NULL; o->par = -1; o->layer = 2; o->hidden = 0;
        o->A = 0; o->c = 0; o->bd = 0; o->absorbed = 0; o->fx = o->fy = 0; o->ang = 0;
        ek_list_add(&LST[slot(o->type)], o, o->type);
        switch (o->type) {
        case OT_FDE_POUNDER:                                                       /* ctor A 25, direction 8; 15313 E = y */
            o->A = 25; o->dir = 8; o->av[1] = floorf(o->y); o->fl = F_NOFLY; ek_set_ad(o, "fdp"); break;   /* (flag 16: the burst in place, INFERRED) */
        case OT_FDE_GUN: {                                                         /* ctor + 16202 / 16729 */
            o->A = 20; o->fl = F_NOFLY; ek_set_a(o, "fdg_gun");
            if (S.ngun >= NGUN) { o->alive = 0; break; }
            gat_t *t = &S.gun[S.ngun++];
            memset(t, 0, sizeof *t); t->g = (short)i; for (int k = 0; k < 5; k++) t->part[k] = -1;
            t->F = t->Gx = -24; t->J = -64;
            t->P = floorf(o->x); t->Q = floorf(o->y);
            t->dormant = S.stage == 12 && S.act == 1 && G.at_g4 < 3;               /* FD3a start: flag 4 */
            break;
        }
        case OT_FDE_GUN2: ek_set_a(o, "fdg_gun2"); break;
        case OT_FDE_GBARREL: ek_set_a(o, "fdg_bar"); break;
        case OT_FDE_GLEFT: case OT_FDE_GRIGHT: ek_set_a(o, "fdg_arm"); break;
        case OT_FDE_GTABLE: ek_set_a(o, "fdg_table"); break;
        case OT_FDE_IRIS: o->A = 10; o->av[0] = 1925; o->av[2] = -6; o->av[3] = 1; ek_set_ad(o, "fdi"); break;
        case OT_FDE_SPIDER: o->A = 15; o->dir = 16; o->hidden = 1; o->c = 5; o->fl = F_NOFLY; ek_set_ad(o, "fds"); break;
        case OT_FDE_PODSPOT: o->hidden = 1; break;
        default: o->alive = 0; break;
        }
    }
    /* parts k <-> gun k (16053: the spread selection pairs instance k of each part with instance k of the gun) */
    static const int PT[5] = { OT_FDE_GUN2, OT_FDE_GBARREL, OT_FDE_GLEFT, OT_FDE_GRIGHT, OT_FDE_GTABLE };
    for (int k = 0; k < 5; k++) {
        obj_t *q; int n = 0;
        EACH(PT[k], q) { if (n < S.ngun) S.gun[n].part[k] = (short)(q - G.o); else q->alive = 0; n++; }
    }
    /* 16202 (FD3): D = 90 + 180 k, k the instance's index in the object list, which Chowdren walks newest first (the last placed gun
     * is k 0): the guns in floors start pointing down (270) and rise, the ceiling ones up (90) and come down (INFERRED order: the
     * json order gives the opposite, guns sinking into the floor); 16729 (FD4a): D -90, K 90 */
    for (int k = 0; k < S.ngun; k++) {
        gat_t *t = &S.gun[k];
        if (S.stage == 13) { t->D = -90; t->K = 90; } else { t->D = 90 + 180.0 * (S.ngun - 1 - k); t->K = t->D - 180; }
        t->R = t->D;
        obj_t *g = obj_at(t->g); if (g) { gat_reset(t, g); gat_parts(t, g); }
    }
}
void fd_enemy_pre(player_t *p) {
    if (S.stage < 10 || S.stage > 13) return;
    if (S.stage == 10 && S.act == 1) pod_rain(p);
    if (!S.on) return;
    if (S.stage == 12 && S.act == 1 && S.ngun) alarm_frame(p);
    if (S.nshk) shocks_frame();
}
void fd_enemy_draw(int pass) { (void)pass; }
int fd_enemy_obj(obj_t *o, player_t *p) {
    if (!S.on) return 0;
    switch (o->type) {
    case OT_FDE_POUNDER: pounder_frame(o, p); return 1;
    case OT_FDE_GUN: gun_frame(o, p); return 1;
    case OT_FDE_IRIS: iris_frame(o, p); return 1;
    case OT_FDE_SPIDER: spider_frame(o, p); return 1;
    case OT_FDE_GDEB: gdeb_frame(o); return 1;
    default: return 1;                                                             /* the gun's parts (placed by gun_frame), drop points */
    }
}
