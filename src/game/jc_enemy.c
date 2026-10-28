/* Jade Creek enemies (session 21): EnemyMeteorRoller_2416 (+ MeteorRollerFlame_2417, EffectWarning_1528, the thrown
 * EffectsExplosion06_435), EnemyPlantTurretus_2393 (+ Hat_2394, Stalk_2395, EffectFallingLeaf_2396, the DV EnemyPellet_934),
 * EnemyBuoyBuoy_2407 (+ BuoyBullet_2409 / BuoyBullet2_2415), EnemyBullFrag_2389 (+ BullFragMask_2410, BullFragBubble_2408),
 * EnemySharkTorpedo_2421 (+ SharkTorpedo2_2422), EnemyBubblOrbiter_2455 (+ BadnikGolemShot_1260 ring), HazardTorpedoLauncher_2515
 * (+ the DV MinibossTreddBomb_1060), EnemyElite_2529 (+ EliteMask_2528, JC5).
 * Source: build/re/spec_jc_enemies.md. Labels: RE (JC1 events 10230-10339 + start events, test_collisions_39 / 40 handlers,
 * JC2 10396-10399 / 10471-10475 / 10568-10569, JC3 10806-10817), RV (original traces build/orig_traces/jc_enemies), INFERRED.
 * The shared hit / defeat / fly-away rules are combat.c's (2730-2768): the Q20 classes are listed by jc_enemy_q20 (RE
 * on_frame_40..44_init qualifier 20). None of these classes sets contact damage B (RE: no event writes it), so only their
 * projectiles, blasts and flames hurt the player.
 * Order: jc_enemy_pre (objects.c, before combat) runs the collision handlers and the "pick one at random" triggers; jc_enemy_obj
 * runs each object's own events from the objects.c per-object pass (after the animations advanced).
 * Performance: per-type instance lists (EACH), nothing runs in a level without these classes (S.on).
 * Alterables: av[] = the class's letters as named per class; flags: the original's flag 5 = 0x20 (armoured, combat.c),
 * flag 6 = 0x40 (defeated), flag 16 = F_NOFLY; own flags >= 0x1000. */
#include "jc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEFEATED(o) ((o)->fl & 0x40)
#define FL_WATER 0x1000                            /* meteor flag 1: in the water (10243) */
#define FL_LANDED 0x2000                           /* "flag 11": the wreck crashed (10245 / 10275 / 10290 / 10310 / 10334) */
#define FL_FIRED 0x4000                            /* bull frag flag 2 / shark J: this cycle's shot made */
#define FL_REFL 0x8000                             /* frog bubble flag 3: reflected by Milla's cube (col_19_39) */
#define FL_POP 0x20000                             /* orbiter shot flag 6 (popped: 10472 / col_4_40 / col_15_40 / col_25_40) */
#define FL_LIM 0x40000                             /* meteor: overlapping a limiter last frame (col_3_39 edge) */
#define RAD 57.29577951308232
#define RT0 OT_JCE_FIRST
#define NRT (OT_JCE_LAST - OT_JCE_FIRST + 1)
#define LMAX 64
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
static struct { int on, hover, hover_t; short lim_x[64], lim_y[64]; int nlim; int view_alert, lim_done; } S;

static int R(int n) { return cross_rand(n); }
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static double c_sin(double deg) { double r = nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / RAD); }
static double c_cos(double deg) { double r = nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / RAD); }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }

/* ---- sprites: '<base>' direction 0, '<base>_l' direction 16 (a missing _l row falls back to the base) */
static int ens_of(const obj_t *o, const char *base) {
    char nm[24]; snprintf(nm, sizeof nm, "%s%s", base, (o->dir & 31) == 16 ? "_l" : "");
    int a = ens_anim(nm);
    return a >= 0 ? a : ens_anim(base);
}
/* a direction-16 row left out by the importer (JCE_MIRRORED: exact mirrors) is drawn as the base row flipped; flip 2 also
 * moves it 1 px right (the original's left hotspot is w - 1 - hx, frame_draw's flip uses w - hx) */
static void mirror_fix(obj_t *o, const char *base) {
    char nm[24]; snprintf(nm, sizeof nm, "%s_l", base);
    if ((o->dir & 31) == 16 && ens_anim(nm) < 0 && ens_anim(base) >= 0) o->flip = 2;
    else if (o->flip == 2) o->flip = 0;
}
static void set_a(obj_t *o, const char *base) {                                 /* force_animation: frame 0, the speed counter kept (RE_NOTES) */
    mirror_fix(o, base);
    int a = ens_of(o, base);
    if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) { int acc = o->an.b ? o->an.acc : 0; anim_set(&o->an, &G.ens, a); o->an.acc = acc; }
}
static void restart_a(obj_t *o, const char *base) { mirror_fix(o, base); int a = ens_of(o, base); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static int a_is(const obj_t *o, const char *base) { return o->an.b == &G.ens && o->an.anim == ens_of(o, base); }
static void face(obj_t *o, int dir) {                                           /* set_direction: animation frame and counter kept */
    if ((o->dir & 31) == dir) return;
    char stem[24] = "";
    if (o->an.b == &G.ens) { snprintf(stem, sizeof stem, "%s", G.ens.anims[o->an.anim].name); size_t n = strlen(stem); if (n > 2 && !strcmp(stem + n - 2, "_l")) stem[n - 2] = 0; }
    o->dir = dir;
    if (!stem[0]) return;
    mirror_fix(o, stem);
    int a = ens_of(o, stem);
    if (a < 0 || a == o->an.anim) return;
    animator_t k = o->an;
    o->an.anim = a; o->an.frame = k.frame < G.ens.anims[a].count ? k.frame : G.ens.anims[a].count - 1;
}
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static obj_t *spawn(int type, const char *anim, float x, float y, int dir) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = 2; o->born = G.frame; o->par = -1; o->dir = dir;
    if (anim) set_a(o, anim);
    lst_add(o);
    return o;
}
static void shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }                    /* CAM.Z = max(CAM.Z, z) */
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }       /* !is_near_border(-n) */
static int csens_hit(const player_t *p, const obj_t *o) {                       /* PlayerCharacterSensor (+0x4f30) */
    int X = (int)PX(p), Y = (int)PY(p), small = player_char_sensor_small(p);
    return o->an.b && obj_rect_hit(o, X - 7, small ? Y - 13 : Y - 34, X + 9, small ? Y + 15 : Y + 13);
}
/* "R-hurt n": MV.R = 1, LIFE.B += n when MV.R == 0 and OTHERS.E <= 0, unless OTHERS.B is the immune shield (0: none) */
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }
static int r_hurt(player_t *p, int n, int immune) { if (!vulnerable(p) || (immune && p->shield == immune)) return 0; player_contact(p, n); return 1; }
static float box_bottom(const obj_t *o) { if (!o->an.b) return o->y; const frame_t *f = anim_frame(&o->an); return floorf(o->y) - f->hy + f->h; }
static int solid_under(const obj_t *o) {                                         /* the background or a Q7 object (the wrecks' crash) */
    if (obj_overlaps_bg(o)) return 1;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *s = &G.o[i];
        if (!s->alive || s == o || !s->an.b || fabsf(s->x - o->x) > 160 || fabsf(s->y - o->y) > 160) continue;
        if (obj_in_group(s->type, OBJGRP_SOLID) && !(s->fl & 1) && obj_obj_hit(o, s)) return 1;
    }
    return 0;
}
static int limiter_hit(const obj_t *o) {                                        /* EnemyLimiter_383: image 16 x 32, hotspot (21, 18) */
    for (int k = 0; k < S.nlim; k++)
        if (fabsf(S.lim_x[k] - o->x) < 96 && fabsf(S.lim_y[k] - o->y) < 96 &&
            obj_rect_hit(o, S.lim_x[k] - 21, S.lim_y[k] - 18, S.lim_x[k] - 5, S.lim_y[k] + 14)) return 1;
    return 0;
}
static void debris(const obj_t *o, int n) {                                     /* loop_spawndebris_39 (10156 / 10158 / 10159): motion INFERRED as combat.c */
    int a = ens_anim("debris");
    for (int k = 0; k < n && a >= 0; k++) {
        obj_t *d = obj_alloc();
        if (!d) break;
        memset(d, 0, sizeof *d);
        d->type = OT_DEBRIS; d->alive = 1; d->layer = 2; d->x = o->x; d->y = o->y; anim_set(&d->an, &G.ens, a); d->an.frame = 10 - k;
        d->vx = (float)(k - 1.5) * 1.5f; d->vy = -3.f - k * 0.5f;
    }
}
/* a defeated wreck falls with the shared fly-away (combat.c 2765) until it meets the background or a Q7 object (flag 11):
 * sound, explosion01 at it, a keystone (dir 2) and debris (10275 / 10290 / 10310 / 10335; the meteor: no keystone, 10245) */
static int wreck_crash(obj_t *o, int snd, int keystone) {
    if (!DEFEATED(o) || (o->fl & FL_LANDED) || !(o->ky > 0) || !solid_under(o)) return 0;
    o->fl |= FL_LANDED;
    snd_play_orig(snd, 220, 128); obj_effect(o->x, o->y, "explosion");
    if (keystone) keystone_spawn(o->x, o->y, 0, 0);
    debris(o, 4);
    o->alive = 0;
    return 1;
}
/* the frog's and the shark's end (10289 / 10290, 10334 / 10335): an explosion06 at +-20 every 7th frame (Q += 1; the frog: sound
 * 29 each, the shark: sound_explode3..5 each), at Q 10 explosion01, a keystone, debris, then sound_explode3..5 (frog) / 38 (shark) */
static int explode_snd(void) { static const int S3[3] = { 311, 319, 320 }; return S3[R(3)]; }   /* loop_sound_explode3..5_0 (RE) */
static void burst_death(obj_t *o, double *Q, int each, int last) {
    if (G.frame % 7 == 0 && *Q < 10) {
        obj_effect(floorf(o->x) - 20 + R(41), floorf(o->y) - 20 + R(41), "explosion06"); *Q += 1;
        snd_play_orig(each ? each : explode_snd(), 220, 128);
    }
    if (*Q == 10) {
        obj_effect(o->x, o->y, "explosion"); keystone_spawn(o->x, o->y, 0, 0); debris(o, 4);
        snd_play_orig(last ? last : explode_snd(), 220, 128); o->alive = 0;
    }
}
static obj_t *partner(const obj_t *o) { return o->par >= 0 && G.o[o->par].alive ? &G.o[o->par] : NULL; }
static void pair_nearest(int child, int owner) {                                /* Fusion's object pairing by instance order: by placement */
    obj_t *o, *q;
    EACH(child, o) { float best = 1e9f; EACH(owner, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->par = (int)(q - G.o); } } }
}
/* the heat of a flame on the player (MeteorRollerFlame 10249-10252, as SB's Magmeon): J += 5 per frame on the sensor (no fire
 * shield), the skin tinted (255 - J), J > 60 -> R-hurt 5 and J = 0; off the sensor J = 0 */
static void heat(obj_t *f, player_t *p, double *J) {
    int on = csens_hit(p, f) && !dead(p);
    if (on && vulnerable(p) && p->shield != 2) {                                  /* 10249 */
        *J += 5; int c = 255 - (int)fmin(*J, 255.);
        p->tint = 15; p->tint_argb = 0xffff0000u | ((uint32_t)c << 8) | (uint32_t)c;
    }
    if (*J > 0 && !on) *J = 0;                                                    /* 10251 */
    if (*J > 60 && on && r_hurt(p, 5, 2)) { *J = 0; p->tint = 30; p->tint_argb = 0xffffa080u; }   /* 10252: OTHERS.R 30, blend 0x80a0ff (BGR) */
}

int jc_enemy_q20(int t) {                                                       /* RE on_frame_40..44_init qualifier 20 */
    return t == OT_JC_METEOR || t == OT_JC_BUOY || t == OT_JC_PLANT || t == OT_JC_FROG || t == OT_JC_SHARK || t == OT_JC_ORBITER ||
           t == OT_JC_ELITEMASK;
}

/* ================================================================ EnemyMeteorRoller_2416 (10230-10248, 10254, col_3_39)
 * A HP 5, G 1 (roll sign; ctor), created invisible. av: [0] D state (0 new, 1 waiting 200 px up-left, 2 falling, 3 landed,
 * 4 rolling, 6 defeated), [1] E / [2] F landing point (the placement), [3] G roll sign, [4] H fall speed, [5] I blast pieces. */
static void meteor_spawn(obj_t *o) {                                             /* ctor + 10230 (run at once: the far ones sleep in the port) */
    o->A = 5; o->av[3] = 1; o->hidden = 1; o->dir = 0; set_a(o, "jem_roll");
    o->av[0] = 1; o->av[1] = floorf(o->x); o->av[2] = floorf(o->y); o->x -= 200; o->y -= 200; o->fl |= 0x40 | F_NOFLY;
}
static void meteor_trigger(const player_t *p) {                                  /* 10231: one random waiting meteor per frame */
    obj_t *o, *c[LMAX]; int n = 0;
    EACH(OT_JC_METEOR, o)
        if (o->av[0] == 1 && in_view(o, 120) && !((double)G.camy - 30 <= floor(o->y) || PX(p) <= floor(o->x))) c[n++] = o;
    if (!n) return;
    o = c[n > 1 ? R(n) : (R(1), 0)];
    o->av[0] = 2; set_a(o, "jem_fall"); o->hidden = 0;
    obj_t *w = spawn(OT_JC_WARN, "jem_warn", (float)o->av[1], (float)(o->av[2] + 20), 0);
    if (w) { w->layer = 3; w->par = (int)(o - G.o); }
    o->fl &= ~(0x40 | F_NOFLY);
}
static void meteor_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gs = &o->av[3], *H = &o->av[4], *I = &o->av[5];
    if (*D == 0) { *D = 1; o->hidden = 1; *E = floorf(o->x); *F = floorf(o->y); o->x -= 200; o->y -= 200; o->fl |= 0x40 | F_NOFLY; }   /* 10230 */
    if (*D == 2) {                                                                 /* 10232 */
        o->x = (float)fmin(floorf(o->x) + 5, *E); o->y = (float)fmin(floorf(o->y) + 5, *F); o->fl |= 0x20;
        if (o->x >= *E && o->y >= *F) {                                            /* 10233 */
            *D = 3; restart_a(o, "jem_land"); snd_play_orig(38, 220, 128); shake(5); o->fl &= ~0x20;
        }
    }
    if (*D == 3 && *I < 8) {                                                       /* 10234: one explosion06 piece a frame, 45 deg apart, speed 6 */
        obj_t *e = spawn(OT_JC_MEXPL, "explosion06", o->x, o->y, 0);
        if (e) { e->vx = (float)(6 * c_cos(*I * 45)); e->vy = (float)(6 * c_sin(*I * 45)); e->layer = 3; }
        *I += 1;
    }
    if (*D == 3) {
        obj_t *w; EACH(OT_JC_WARN, w) if (w->par == (int)(o - G.o)) w->alive = 0;   /* 10235 (overlap: its own warning) */
        if (o->an.frame < 2 && csens_hit(p, o)) r_hurt(p, 10, 2);                   /* 10236: the blast, not the fire shield */
        for (int i = 0; i < G.nobjs; i++) {                                        /* 10237: Q19 monitors in the blast break */
            obj_t *m = &G.o[i];
            if (m->alive && mon_type(m->type) && fabsf(m->x - o->x) < 96 && fabsf(m->y - o->y) < 96 && obj_obj_hit(o, m)) m->fl |= 0x200;
        }
        if (o->an.done) { o->fl &= ~0x20; *D = 4; set_a(o, "jem_roll"); }           /* 10238 (anim 0 again: INFERRED) */
    }
    if (*D == 4 && in_view(o, 80)) {                                               /* 10239 / 10240 */
        if (*Gs == -1) { face(o, 16); o->x -= 2; }
        else if (*Gs == 1) { face(o, 0); o->x += 2; }
    }
    if (*D > 2 && *D < 6 && in_view(o, 50)) {
        if (!px_solid(&G.lvl, (int)floorf(o->x), (int)box_bottom(o) - 1)) {        /* 10241 */
            o->y = (float)(int)(floorf(o->y) + *H); *H = fmin(*H + ((o->fl & FL_WATER) ? 0.05 : 0.5), 12);
        }
        if (px_solid(&G.lvl, (int)floorf(o->x), (int)box_bottom(o) - 1)) { o->y = floorf(o->y) - 1; *H = 0; }   /* 10242 foreach_mfloor_39 */
    }
    if (water_on() && floorf(o->y) > level_cur()->water && !(o->fl & FL_WATER)) {   /* 10243 */
        obj_effect(floorf(o->x), (float)(int)water_c(), "splash"); snd_play_orig(14, 220, 128); o->fl |= FL_WATER; *H = 1;
    }
    if (DEFEATED(o) && *D > 1 && *D != 6) { *D = 6; set_a(o, "jem_hurt"); }       /* 10244 */
    if (*D == 6) wreck_crash(o, 29, 0);                                            /* 10245 / 10246 */
}
static void meteor_hits(void) {                                                  /* 10254: a falling meteor hits another Q20 enemy */
    obj_t *o;
    EACH(OT_JC_METEOR, o) {
        if (o->av[0] != 2) continue;
        obj_t *c[16]; int n = 0;
        for (int i = 0; i < G.nobjs && n < 16; i++) {
            obj_t *e = &G.o[i];
            if (!e->alive || e == o || !enemy_type(e->type) || !e->an.b || e->c > 0 || (e->fl & 0x60)) continue;
            if (fabsf(e->x - o->x) < 128 && fabsf(e->y - o->y) < 128 && obj_obj_hit(o, e)) c[n++] = e;
        }
        if (!n) continue;
        obj_t *e = c[n > 1 ? R(n) : (R(1), 0)];
        e->c = 5; e->kx = -6; e->ky = 6; e->A -= 5; e->inv = 1; snd_play_orig(24, 220, 128);
    }
}
static void flame_frame(obj_t *f, player_t *p) {                                  /* 10247 / 10248 / 10249-10253 */
    obj_t *o = partner(f);
    if (!o) { f->alive = 0; return; }                                              /* 10253: a flame per roller */
    f->x = o->x; f->y = o->y; face(f, o->dir & 31); f->av[0] = o->av[0]; f->hidden = 1;
    if (f->av[0] == 4) f->hidden = 0;
    if (f->av[0] == 4) heat(f, p, &f->av[1]);
}

/* ================================================================ EnemyPlantTurretus_2393 (10255-10278, start events)
 * A HP 5, D -1 (facing sign), H 80 (sunk depth; ctor), I / J the placement (start event), E state (0 hidden, 1 rising, 2 shooting,
 * 3 recoil / sinking, 6 defeated), F timer, G recoil x. av: [0] D, [1] E, [2] F, [3] G, [4] H, [5] I, [6] J.
 * Drawn behind the play layer's pieces (10255 / 10256: move_back on the player's layer): port layer 1. */
static void plant_spawn(obj_t *o) {
    o->A = 5; o->av[0] = -1; o->av[4] = 80; o->av[5] = floorf(o->x); o->av[6] = floorf(o->y); o->layer = 1; set_a(o, "jep_idle");
    o->y = (float)(o->av[6] + o->av[4]);
}
static void plant_trigger(const player_t *p) {                                   /* 10263: every 10 frames one random hidden plant */
    if (G.frame % 10) return;
    obj_t *o, *c[LMAX]; int n = 0;
    EACH(OT_JC_PLANT, o)
        if (o->av[1] == 0 && o->av[2] <= 0 && !DEFEATED(o) && fabs(PX(p) - o->av[5]) < 180 && PY(p) > o->av[6] && PY(p) - 100 < o->av[6]) c[n++] = o;
    if (!n) return;
    c[n > 1 ? R(n) : (R(1), 0)]->av[1] = 1;
}
static void plant_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gx = &o->av[3], *H = &o->av[4];
    if (!DEFEATED(o)) { o->x = (float)(int)(o->av[5] + *Gx); o->y = (float)(int)(o->av[6] + *H); }   /* 10257 */
    if (*D == -1) face(o, 16); else if (*D == 1) face(o, 0);                       /* 10260 / 10261 */
    if (*E == 0 && *F > 0) *F -= 1;                                                /* 10262 */
    if (*E == 1 && G.frame % 4 == 0) {                                             /* 10264: falling leaves */
        obj_t *l = spawn(OT_JC_LEAF, "jep_leaf", floorf(o->x) - 8 + R(17), floorf(o->y) - 6, 0);
        if (l) { int d = 21 + R(7); l->vx = (float)(2.5 * c_cos(d * 11.25)); l->vy = (float)(-2.5 * c_sin(d * 11.25)); l->ang = (float)R(360); l->layer = 3; }   /* ctor: BallMovement speed 20 (2.5 px), direction pick_random(21..27) */
    }
    if (*E == 1) { *D = PX(p) <= o->av[5] ? -1 : 1; *H = fmax(*H - 2, 0); }       /* 10265 / 10266 */
    if (*E == 1 && *H <= 0) { restart_a(o, "jep_shoot"); *E = 2; }                 /* 10267 */
    if (*E == 2 && a_is(o, "jep_shoot") && o->an.frame == 6) {                     /* 10268 */
        double ax, ay; relic_act_point(o, &ax, &ay);
        snd_play_orig(28, 220, 128); obj_spawn_pellet((float)ax, (float)ay, o->dir & 31);
        *E = 3; *Gx = *D * -4;
    }
    if (*Gx > 0) *Gx -= 1; else if (*Gx < 0) *Gx += 1;                            /* 10269 / 10270 */
    if (*E == 3) { *F += 1; if (*F > 30) *H = fmin(*H + 2, 80); }                  /* 10271 / 10272 */
    if (*E == 3 && *H >= 80) { *E = 0; *F = 60; }                                  /* 10273 */
    if (*E == 2 && a_is(o, "jep_shoot") && o->an.done) set_a(o, "jep_idle");       /* (anim 7 plays once: back to anim 0, INFERRED) */
    if (DEFEATED(o) && *E != 6) { set_a(o, "jep_idle"); *E = 6; }                  /* 10274 */
    if (*E == 6) wreck_crash(o, 29, 1);                                            /* 10275 */
}
static void plant_parts(obj_t *o) {                                              /* 10258 / 10259 / 10276: the hat sits on the plant, the stalk under it */
    obj_t *q = partner(o);
    if (o->type == OT_JC_PHAT) {
        if (!q) { o->alive = 0; return; }                                          /* 10276: hidden with its plant (A -1) */
        o->x = q->x; o->y = q->y; face(o, q->dir & 31);
    } else if (q && !DEFEATED(q)) { o->x = q->x; o->y = q->y; }                    /* 10259 (stalk A = plant A > 0) */
}
static void leaf_frame(obj_t *o) {                                                /* 10258 / 10277 / 10278: A += 1, turns, removed after 120 frames */
    o->av[0] += 1; o->ang += 1; o->fx += o->vx; o->fy += o->vy;                    /* BallMovement: floor steps, fraction kept */
    float ix = floorf(o->fx), iy = floorf(o->fy); o->x += ix; o->y += iy; o->fx -= ix; o->fy -= iy;
    if (o->av[0] > 120 || obj_overlaps_bg(o)) o->alive = 0;                        /* test_collisions_39: removed on the background */
}

/* ================================================================ EnemyBuoyBuoy_2407 (10299-10318, foreach_buoy_39)
 * A HP 10, H 32 (bob amplitude). av: [0] E state (0 bob, 1 open, 2 shot, 4 defeated), [1] F home y, [2] G bob phase, [3] I timer.
 * RE cycle: I counts to 20 in view, opens (anim 7), at its frame 10 two jets (+-10, -10; sound 60), 81 frames, I = -60, closes. */
static void buoy_spawn(obj_t *o) { o->A = 10; o->av[4] = 32; set_a(o, "jeb_idle"); }
static void buoy_frame(obj_t *o, player_t *p) {
    (void)p;
    double *E = &o->av[0], *F = &o->av[1], *Gp = &o->av[2], *I = &o->av[3];
    if (*E == 0 && *F == 0) *F = floorf(o->y);                                     /* 10299 */
    else if (*E == 0) { o->y = (float)(int)(c_sin(*Gp) * o->av[4] + *F); *Gp = fmod(*Gp + 1, 360); }   /* 10300 */
    if (*E == 0 && *I < 20 && in_view(o, 50)) *I += 1;                             /* 10301 */
    if (*E == 0 && *I >= 20) *E = 1;                                               /* 10302 */
    if (*E == 1) set_a(o, "jeb_open");                                             /* 10303 */
    if (*E == 1 && a_is(o, "jeb_open") && o->an.frame == 10) {                     /* 10304 */
        *E = 2; *I = 0;
        obj_t *j = spawn(OT_JC_BJET, "jeb_jet", floorf(o->x) + 10, floorf(o->y) - 10, 0); if (j) j->layer = 3;
        j = spawn(OT_JC_BJET, "jeb_jet", floorf(o->x) - 10, floorf(o->y) - 10, 16); if (j) j->layer = 3;
        snd_play_orig(60, 220, 128);
    }
    if (*E == 2) *I += 1;                                                          /* 10305 */
    if (*E == 2 && *I > 80) { *I = -60; *E = 0; restart_a(o, "jeb_shut"); }        /* 10306 */
    if (*E == 0 && a_is(o, "jeb_shut") && o->an.done) set_a(o, "jeb_idle");        /* (anim 8 once, then anim 0: INFERRED) */
    if (DEFEATED(o) && *E != 4) *E = 4;                                            /* 10307 */
    if (*E == 4) { face(o, o->kx >= 0 ? 16 : 0); set_a(o, "jeb_dead"); wreck_crash(o, 29, 1); }   /* 10308-10310 */
}
static void bjet_frame(obj_t *o, player_t *p) {                                   /* 10312-10318: 10 damage (not the water shield) */
    o->av[0] += 1;
    if (o->av[0] > 70) { o->alive = 0; return; }                                   /* RV buoy: 71 frames (RE 10313 A > 60: A's count start not read) */
    if (csens_hit(p, o) && r_hurt(p, 10, 4)) shake(2);
    if (in_view(o, 40)) { obj_t *b; EACH(OT_JC_BUOY, b) if (DEFEATED(b) && obj_obj_hit(o, b)) { o->alive = 0; return; } }   /* foreach_buoy_39 / 10162 */
}

/* ================================================================ EnemyBullFrag_2389 (10279-10298, col_6_39 / col_17_39 / col_19_39)
 * A HP 40, D -1 (facing sign), S 20 (ctor). av: [0] E state (0 wait, 1 blowing, 4 defeated), [1] F timer / bubble count,
 * [2] D, [3] Q crash count. RE: F counts in view to 30, then 10 bubbles every 4th frame from frame 6 of anim 10 (C = (0.5 F + 3) D),
 * F = -70 (a 100-frame wait), anim 11; defeated: 10 explosions every 7 frames (sound 29), then keystone + debris + explode sound. */
static void bubble_frame(obj_t *b, player_t *p);
static void frog_spawn(obj_t *o) { o->A = 40; o->av[2] = -1; o->dir = 16; set_a(o, "jef_idle"); }
static void frog_frame(obj_t *o, player_t *p) {
    double *E = &o->av[0], *F = &o->av[1], *D = &o->av[2], *Q = &o->av[3];
    if (DEFEATED(o)) {                                                             /* (it flies away meanwhile: combat.c 2765) */
        if (*E != 4) { set_a(o, "jef_hurt"); *E = 4; }                             /* 10288 */
        burst_death(o, Q, 29, 0);                                                  /* 10289 / 10290 */
        return;
    }
    if (in_view(o, 80) && *E == 0) *F += 1;                                        /* 10280 */
    if (*E == 0 && *F > 30) { o->fl &= ~FL_FIRED; *F = 0; *E = 1; }                /* 10281 */
    if (*E == 0) { if (PX(p) < floorf(o->x)) { *D = -1; face(o, 16); } else if (floorf(o->x) < PX(p)) { *D = 1; face(o, 0); } }   /* 10282 / 10283 */
    if (*E == 1) set_a(o, "jef_blow");                                             /* 10284 */
    if (*E == 1 && a_is(o, "jef_blow") && o->an.frame >= 6 && G.frame % 4 == 0) {  /* 10285 / 10286 */
        if (!(o->fl & FL_FIRED)) { o->fl |= FL_FIRED; snd_play_orig(275, 220, 128); }
        *F += 1;
        double ax, ay; relic_act_point(o, &ax, &ay);
        obj_t *b = spawn(OT_JC_BUBBLE, "jef_bubble", (float)ax, (float)ay, 0);
        if (b) { b->layer = 3; b->av[2] = (0.5 * *F + 3) * *D; b->av[3] = 3; bubble_frame(b, p); }   /* RE: 10293-10298 follow, same frame */
    }
    if (*E == 1 && *F > 9) { *F = -70; *E = 0; restart_a(o, "jef_shut"); }        /* 10287 */
    if (*E == 0 && a_is(o, "jef_shut") && o->an.done) set_a(o, "jef_idle");        /* (anim 11 once: INFERRED back to anim 0) */
}
static void bubble_pop(obj_t *b) { obj_effect(b->x, b->y, "wburst"); shake(2); snd_play_orig(314, 220, 128); b->alive = 0; }
static void bubble_frame(obj_t *b, player_t *p) {                                 /* 10293-10298 + col_6_39 / col_17_39 / col_19_39 */
    double *A = &b->av[0], *B = &b->av[1], *C = &b->av[2], *D = &b->av[3];
    if (!(b->fl & 1)) { b->fl |= 1; *A = floorf(b->x); *B = floorf(b->y); }       /* 10293 (RE: 10294 runs in the same frame) */
    {
        *A += *C; *B += *D; b->x = (float)(int)*A; b->y = (float)(int)*B;          /* 10294 */
        *D = fmax(*D - 0.12, -15);
        if (*C > 0) *C = fmax(*C - 0.1, 0); else if (*C < 0) *C = fmin(*C + 0.1, 0);   /* 10295 / 10296 */
        if (*C == 0) { bubble_pop(b); return; }                                    /* 10297 */
    }
    if (b->fl & FL_REFL) {                                                         /* 10298: a reflected bubble hits an enemy */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *e = &G.o[i];
            if (!e->alive || !enemy_type(e->type) || !e->an.b || (e->fl & 0x40) || fabsf(e->x - b->x) > 96 || fabsf(e->y - b->y) > 96 || !obj_obj_hit(b, e)) continue;
            e->c = 5; e->kx = *C; e->ky = -2; e->A -= 5; e->inv = 1; snd_play_orig(24, 220, 128);   /* ky INFERRED */
            bubble_pop(b); return;
        }
    }
    if (!csens_hit(p, b)) return;
    if (p->shield == 4) { bubble_pop(b); return; }                                  /* col_17_39: the water shield pops it */
    if (!(b->fl & FL_REFL) && player_cube_state() >= 4 && player_cube_hits(b)) { b->fl |= FL_REFL; *C = -*C; return; }   /* col_19_39 */
    if (r_hurt(p, 10, 4)) bubble_pop(b);                                           /* col_6_39 */
}

/* ================================================================ EnemySharkTorpedo_2421 (10319-10335, col_14_39, 10337-10339)
 * A HP 15, G -2 (x speed; ctor); start: flag 16. av: [0] D timer, [1] E (6 defeated), [2] H turn frame 0..6, [3] I its target,
 * [4] J 1 = the torpedo is out, [5] G. RE: in view (60) x += G (limiters reverse G), D counts; D > 30 and J 0: one random shark
 * per event fires a torpedo up (frame 0: A -4) or down (frame 6: A 4), D = -60; J 1 at D > 0 -> I = 6 / 0, the shark turns
 * one frame every 3 frames. Defeated: it fires once more, flickers, crashes (sound 38, keystone). */
static void shark_spawn(obj_t *o) { o->A = 15; o->av[5] = -2; o->fl |= F_NOFLY; set_a(o, "jes_l0"); }
static void shark_fire(obj_t *o) {
    obj_t *t = spawn(OT_JC_TORP, "jes_torp", floorf(o->x), floorf(o->y), 0);
    if (t) { t->av[0] = o->av[2] == 0 ? -4 : 4; t->layer = 2; }
    o->av[4] = 1; o->av[0] = -60;
}
static void shark_trigger(void) {                                               /* 10322-10325: one random shark per event */
    for (int ev = 0; ev < 4; ev++) {
        obj_t *o, *c[LMAX]; int n = 0;
        EACH(OT_JC_SHARK, o) {
            int six = o->av[1] == 6, h0 = o->av[2] == 0;
            if (ev < 2 ? (in_view(o, 60) && !six && o->av[0] > 30 && o->av[4] == 0 && h0 == (ev == 0)) : (six && o->av[4] == 0 && h0 == (ev == 2))) c[n++] = o;
        }
        if (n) shark_fire(c[n > 1 ? R(n) : (R(1), 0)]);
    }
}
static void shark_frame(obj_t *o, player_t *p) {
    (void)p;
    double *D = &o->av[0], *E = &o->av[1], *H = &o->av[2], *I = &o->av[3], *J = &o->av[4], *Gs = &o->av[5];
    if (in_view(o, 60)) o->x = (float)(int)(floorf(o->x) + *Gs);                   /* 10319 */
    if (in_view(o, 60) && *E != 6) *D += 1;                                        /* 10321 */
    if (in_view(o, 60) && *E != 6 && *D > 0 && *J == 1) { *I = *H == 0 ? 6 : 0; *J = 0; *D = -60; }   /* 10326 / 10327 */
    if (G.frame % 3 == 0) { if (*H > *I) *H -= 1; else if (*H < *I) *H += 1; }     /* 10330 / 10331 */
    if (DEFEATED(o) && *E != 6) *E = 6;                                            /* 10332 */
    const char *nm = *E == 6 ? (*Gs > 0 ? "jes_hr" : "jes_hl") : (*Gs > 0 ? "jes_r" : "jes_l");   /* 10328 / 10329 / 10333 */
    char row[12]; snprintf(row, sizeof row, "%s%d", nm, *E == 6 ? (int)((G.frame / 2) & 1) : (int)*J);
    int a = ens_anim(row);
    if (a >= 0) { if (o->an.b != &G.ens || o->an.anim != a) anim_set(&o->an, &G.ens, a); o->an.frame = (int)*H < G.ens.anims[a].count ? (int)*H : 0; o->an.acc = 0; }
    o->dir = *Gs > 0 ? 0 : 16;
    if (*E == 6) burst_death(o, &o->av[6], 0, 38);                                 /* 10334 / 10335 (flag 16: it stays) */
}
static void torp_frame(obj_t *o, player_t *p) {                                   /* 10337-10339 + col_14_39 */
    o->y = (float)(int)(floorf(o->y) + o->av[0]); o->av[1] += 1;
    if (o->av[1] > 150) {
        if (in_view(o, 50)) { snd_play_orig(50, 220, 128); obj_effect(o->x, o->y, "wburst"); }
        o->alive = 0; return;
    }
    if (csens_hit(p, o) && r_hurt(p, 10, 5)) { snd_play_orig(50, 220, 128); shake(2); obj_effect(o->x, o->y, "wburst"); o->alive = 0; }   /* not the earth shield */
}

/* ================================================================ EnemyBubblOrbiter_2455 (JC2-4: 10396, 10399, 10471-10475, 10568, 10569)
 * A HP 5. av: [0] D bob phase, [1] E base y, [2] G shots made. 15 BadnikGolemShots ring it (G 360 - 24 k): x = X + (60 + 30 sin D)
 * sin G, y = E + 14, G += 3, scale 0.8 + 0.2 cos G, behind it while 90 < G < 270. Shot: R-hurt 5 (not the water shield), pops on
 * Milla's cube (col_4_40) and the water shield (col_25_40, sound 101). The orbiter faces the player (32 directions). */
static void orbiter_spawn(obj_t *o) { o->A = 5; o->av[1] = floorf(o->y); o->hidden = 1; set_a(o, "jeo_body"); }   /* 10568; drawn with its ring by jc_enemy_draw */
static void orbiter_ring(obj_t *o) {
    for (int k = 0; k < 15; k++) {                                                /* 10569 loop_bbls_40 / 10396 */
        obj_t *s = spawn(OT_JC_OSHOT, "jeo_shot", o->x, o->y, 0);
        if (!s) break;
        s->par = (int)(o - G.o); s->av[0] = 360 - 24 * o->av[2]; o->av[2] += 1; s->hidden = 1;
    }
}
static void orbiter_frame(obj_t *o, player_t *p) {
    if (in_view(o, 100) && !DEFEATED(o)) {                                         /* 10471 */
        o->av[0] = fmod(o->av[0] + 3, 360);
        o->y = (float)(int)(o->av[1] - 16 + 16 * c_sin(o->av[0]));
        double a = atan2(floorf(o->y) - PY(p), PX(p) - floorf(o->x)) * RAD; if (a < 0) a += 360;
        int d = (int)(a / 11.25) & 31; face(o, (d > 8 && d < 24) ? 16 : 0);
    }
    if (DEFEATED(o)) { o->hidden = 0; if (wreck_crash(o, 29, 1)) return; }        /* 10475 */
}
static void oshot_frame(obj_t *s, player_t *p) {
    obj_t *o = partner(s);
    if (!o || DEFEATED(o)) s->fl |= FL_POP;                                        /* J = the orbiter's flag 6 */
    else {                                                                         /* 10472 foreach_bbl_40 (RV: every ring turns) */
        s->x = (float)(int)((60 + 30 * c_sin(o->av[0])) * c_sin(s->av[0]) + floorf(o->x));
        s->y = (float)(int)(14 + o->av[1]);
        s->av[0] = fmod(s->av[0] + 3, 360);
        s->scale = (float)(c_cos(s->av[0]) * 0.2 + 0.8);
    }
    if (!(s->fl & FL_POP) && csens_hit(p, s)) {
        if (p->shield == 4) { snd_play_orig(101, 220, 128); s->fl |= FL_POP; }     /* col_25_40 */
        else if (r_hurt(p, 5, 4)) { s->fl |= FL_POP; p->tint = 60; p->tint_argb = 0xffff80ff; }   /* col_15_40 (OTHERS.R 60) */
    }
    if (!(s->fl & FL_POP) && player_cube_state() >= 4 && player_cube_hits(s)) s->fl |= FL_POP;   /* col_4_40 */
    if (s->fl & FL_POP) { obj_effect(s->x, s->y, "jeo_pop"); s->alive = 0; }       /* (the pop: anim 4, INFERRED) */
}

/* ================================================================ HazardTorpedoLauncher_2515 (JC3 10806-10817 / JC4 10850-10859)
 * av: [0] A cycle (start: (index * 7) mod 150), [1] B hatch 0..40 (frame B / 10). Cycle 180: open at 60, fire at 90 when in view
 * (a Tredd bomb, B -10; explosion01; sound), close at 120, A -= 180 past 150. JC3: only once group 0x22a5d is on (the hovercraft
 * section after Neera, 10769 / 10798: jc_enemy_hover_start); JC4: from the start (10850). */
static void launcher_start(void) {
    obj_t *o; int k = 0;
    EACH(OT_JC_LAUNCHER, o) { o->av[0] = fmod(k * 7.0, 150); k++; }               /* 10806 / 10850 */
}
void jc_enemy_hover_start(void) { if (!S.hover) { S.hover = 1; launcher_start(); } }
static void launcher_frame(obj_t *o, player_t *p) {
    (void)p;
    if (!S.hover) return;
    double *A = &o->av[0], *B = &o->av[1];
    *A += 1;                                                                       /* 10807 */
    if (*A == 60) o->fl |= 1;                                                      /* 10808 */
    if (*A == 90 && in_view(o, 120)) {                                             /* 10809: A += 1 too (RV launcher: a 179-frame cycle when firing) */
        *A += 1;
        obj_t *b = spawn(OT_JC_TBOMB, "td_bomb", o->x, o->y, 0);
        if (b) { b->fy = b->y; b->vy = -10; }
        obj_effect(o->x, o->y, "explosion"); snd_play_orig(99, 220, 128);         /* RV launcher: sound 99 (the Media::play sample) */
    }
    if (*A == 120) o->fl &= ~1;                                                    /* 10810 */
    if (*A > 150) *A -= 180;                                                       /* 10811 */
    *B = (o->fl & 1) ? fmin(*B + 2, 40) : fmax(*B - 2, 0);                         /* 10812 / 10813 */
    int f = (int)(*B / 10); if (o->an.b && f < G.ens.anims[o->an.anim].count) { o->an.frame = f; o->an.acc = 0; }   /* 10807 force_frame */
}

/* ================================================================ EnemyElite_2529 + EnemyEliteMask_2528 (JC5 10949-11033, col_21_43; lasers
 * EnemyTrooperLaser_2530 11034-11039). The invisible mask (40 x 73, hotspot at the feet) is the Q20 body and runs the AI; the
 * Elite is its sprite (11027: A = mask D, B = mask E, C = mask H, drawn at the mask). Mask av: [0] D state (0 walk, 1 attack,
 * 2 recover, -1 air, -2 knocked, 3 down, 4 getting up, 6 dying), [1] E facing, [2] N walk sign, [3] Q alert, [4] H x speed,
 * [5] I y speed, [6] T attack timer, [7] V pacing centre; tm = K, c-like counters packed in the sprite's av: [0] R, [1] O,
 * [2] P, [3] L air frames, [4] M, [5] U knock-downs, [6] F charge frames, [7] its sprite frame accumulator.
 * RE: patrols at 2 px (limiters / walls turn it), alerts when the player is ahead within 219 px (-39..+99 in y), then paces
 * around her x (every 20 frames: +-60 / 80 px) at up to 5 px, jumps (I -10, sound 233) after a wall blocks it 21 frames;
 * T counts while she is within 119 px: T > 30 -> attack (T -70, P 1): faces her at T <= -60, charges (anim 10, a glow at its
 * frame F 1), from F > 25 shoots lasers (anim 12, at its frame 1: speed 12, sound 192) until T > 0, then recovers 6 frames.
 * Hits: the shared Q20 rules; at A <= 0 it is knocked (D -2: x = E + knock x, y = knock y - 1), bounces once (I -3), lies 20
 * frames, gets up 22 frames with A 20 again; knocked again after that, it dies on landing (sound 29, explosion, debris;
 * Carol: ObjectFuel 10870). The floor / wall / ceiling sensors are the mask's directions 1-4 (11016-11026). */
#define ELM_GND 0x10                                                              /* mask flag 4: on the ground */
static int elite_sensor(obj_t *m, int d) {                                       /* force_direction(d) + overlap bg / Q7 / Q8 (from above) */
    char nm[12]; snprintf(nm, sizeof nm, "jee_m%d", d);
    animator_t keep = m->an; int a = ens_anim(nm), hit = 0;
    if (a < 0) return 0;
    anim_set(&m->an, &G.ens, a);
    hit = obj_overlaps_bg(m) || objects_mask_hit_groups(m, d == 1);   /* (the group candidate lists, as enemy_kit.h ek_mask_sensor:
                                                                         no scan of every object: BG5 / FD run up to ten Elites) */
    m->an = keep;
    return hit;
}
static void elite_spawn(obj_t *m) {
    m->A = 20; m->av[1] = 1; m->hidden = 1; m->fx = m->x; m->fy = m->y; set_a(m, "jee_m0");
    m->fl |= F_NOFLY;          /* flag 16 (RV bg_enemies/elite4: placed masks carry 0x10000; the spawners set it, 14661): no shared
                                  fly-away (combat.c 2765 turned the mask and its sensors while it lay knocked down) */
}
static void elite_laser(obj_t *e, int dir) {                                      /* 10949: TrooperLaser A 12, B (1 + B) 90 - 180 */
    double ax, ay; relic_act_point(e, &ax, &ay);
    obj_t *l = spawn(OT_JC_LASER, "jee_laser", (float)ax, (float)ay, 0);
    if (l) { l->av[0] = 12; l->av[1] = dir > 0 ? 0 : 180; l->av[2] = floorf(l->x); l->av[3] = floorf(l->y); l->flip = dir < 0; l->layer = 3; }
    snd_play_orig(192, 220, 128);
}
static void elite_frame(obj_t *m, player_t *p) {
    obj_t *e = partner(m);
    if (!e) { m->alive = 0; return; }
    double *D = &m->av[0], *E = &m->av[1], *N = &m->av[2], *Q = &m->av[3], *H = &m->av[4], *I = &m->av[5], *T = &m->av[6], *V = &m->av[7];
    double *R = &e->av[0], *O = &e->av[1], *P = &e->av[2], *L = &e->av[3], *M = &e->av[4], *U = &e->av[5], *F = &e->av[6];
    double X = floorf(m->x), Y = floorf(m->y), px = PX(p), py = PY(p);
    int alive_p = !dead(p) && player_orig_m(p) != 9;
    if (*D == 0 && *Q == 0 && m->tm < 30) m->tm = 30;                              /* 10950 / 10951: the first walk is to the left */
    if (*D == 0 && *Q == 0 && m->tm == 30) { *N = -1; m->tm++; }
    if (*D == 0 && *Q == 0 && *N == 1 && (m->fl & 2)) *N = -1;                     /* 10952 / 10953: walls */
    else if (*D == 0 && *Q == 0 && *N == -1 && (m->fl & 4)) *N = 1;
    if (*D == 0 && *Q == 0 && alive_p && py - 39 <= Y && Y <= py + 99 &&          /* 10954 / 10955: she is ahead */
        ((*E == 1 && X < px && px <= X + 219) || (*E == -1 && px < X && X <= px + 219))) *Q = 1;
    if (*D == 0 && *Q == 0 && m->c > 0 && alive_p) *Q = 1;                        /* 10956: hit */
    if (*Q == 1 && !alive_p) *Q = 0;                                               /* 10957 */
    if (*D == 0 && *Q == 1 && G.frame % 20 == (int)(m - G.o) % 20) {               /* 10958-10962 (frame % 20 = its fixed value % 20) */
        if (*V == 0) *V = px;
        if (X >= *V && X < *V + 60) *N = 1; else if (X > *V + 80) *N = -1;
        else if (X < *V && X > *V - 60) *N = -1; else if (X < *V - 80) *N = 1;
    }
    for (int s = -1; s <= 1; s += 2) {                                             /* 10963-10967: blocked while alert */
        int wall = s == 1 ? (m->fl & 2) : (m->fl & 4);
        if (*D == 0 && *Q == 1 && *N == s && wall && *R < 21 && *O == 0) { *O = 1; *R += 10; }
    }
    if (*D == 0 && *R > 0) *R -= 1;
    for (int s = -1; s <= 1; s += 2) {
        int wall = s == 1 ? (m->fl & 2) : (m->fl & 4);
        if (*D == 0 && *Q == 1 && *N == s && wall && *R >= 21) { *R = 0; *O = 0; *Q = 0; *N = -*N; }
    }
    if (*D == 0 && *Q == 1 && fabs(px - X) <= 119 && py >= Y - 119 && py <= Y + 59) *T += 1;   /* 10968 */
    if (*D == 0 && *Q == 1 && *T > 30) { *T = -70; *P = 1; }                       /* 10969 */
    if (*D == 1 && *T <= -60) *E = px < X ? -1 : 1;                                /* 10970 / 10971 */
    if (*D == 1 && *P == 1) *T += 1;                                               /* 10972 */
    if (*D == 1 && *P == 1 && *T > 0) *P = 0;                                      /* 10973 */
    if (*D <= 0 && *D > -2) {                                                      /* 10974-10979: walk / run speeds */
        double top = *Q == 1 ? 5 : 2;
        if (*N == 1) { *E = 1; *H = fmin(*H + 0.4, top); }
        else if (*N == -1) { *E = -1; *H = fmax(*H - 0.4, -top); }
        if (*Q == 1 && *N != 1 && *H > 0) *H = fmax(*H - 0.4, 0);
        if (*Q == 1 && *N != -1 && *H < 0) *H = fmin(*H + 0.4, 0);
    }
    if (*D == 0 && *P == 1) { *D = 1; *H = 0; }                                    /* 10980 */
    if (*D == 1 && *P == 0 && a_is(e, "jee_shoot") && e->tm >= 4) { *D = 2; m->tm = 0; }   /* 10981 */
    if (*D == 2 && ++m->tm > 6) { *D = 0; m->tm = 0; *V = 0; }                      /* 10982 / 10983 */
    if (*O == 1 && *D == 0 && (m->fl & ELM_GND)) {                                 /* 11000 / 11001: jump over the wall */
        if (in_view(m, 50)) snd_play_orig(233, 220, 128);
        *D = -1; *I = -10; obj_effect(m->x, m->y, "wburst"); *O = 0;
    }
    if (*D == 0 && *L > 16) *D = -1;                                               /* 11002 */
    if (*D >= -2 && *D <= -1) *I = fmin(*I + 0.5, 15);                             /* 11003 */
    if (*D >= -2 && *D <= -1 && (m->fl & 8)) *I = fmax(*I, 0);                     /* 11004 */
    if (*D == -1 && *I > 0 && (m->fl & ELM_GND)) { *I = 0; *D = 0; }               /* 11005 */
    if (DEFEATED(m) && *D != -2 && *D < 3) {                                       /* 11006: knocked */
        m->c = 0; *D = -2; *H = *E + m->kx; *I = m->ky - 1; m->fy -= 12;
    }
    if (*D == -2 && *I > 0 && (m->fl & ELM_GND)) {                                 /* 11007-11009 */
        if (*M < 1 && *U >= 1) *D = 6;
        else if (*M < 1) { *I = -3; *M += 1; *U += 1; }
        else { *I = 0; *D = 3; *H = 0; }
    }
    if (*D == 3 && ++*M > 20) { *D = 4; *M = 0; }                                  /* 11010 / 11011 */
    if (*D == 4 && ++*M > 22) { m->fl &= ~0x40; *D = 0; m->A = 20; }              /* 11012 / 11013 */
    if (*M > 0 && *D != -2 && *D < 3) *M = 0;                                      /* 11014 */
    /* 11016-11026: the sensors, then 11027 the motion (x += H, y += I) */
    m->fl &= ~(ELM_GND | 2 | 4 | 8);
    m->y = floorf(m->fy) + 1;
    if (elite_sensor(m, 1)) { m->fy -= 1; m->fl |= ELM_GND; *L = 0; m->y = floorf(m->fy) + 1;
        if (elite_sensor(m, 1)) m->fy -= 1; }                                      /* 11017 / 11018 */
    else if (*D >= 0) { m->fy += 1; *L += 1; }                                     /* 11019 */
    m->y = floorf(m->fy);
    if (elite_sensor(m, 2)) { m->fl |= 2; *H = fmin(*H, 0); }                      /* 11020 / 11021 */
    if (elite_sensor(m, 3)) { m->fl |= 4; *H = fmax(*H, 0); }                      /* 11022 / 11023 */
    if (elite_sensor(m, 4)) m->fl |= 8;                                            /* 11024 / 11025 */
    if (limiter_hit(m) && *D == 0 && *Q == 0) *N = -*N;                            /* col_21_43 */
    m->fx += *H; m->fy += *I; m->x = (float)(int)m->fx; m->y = (float)(int)m->fy;  /* 11027 */
    e->x = m->x; e->y = m->y; e->inv = m->inv;
    if (m->y > G.lvl.h + 400) { m->alive = 0; e->alive = 0; return; }             /* fell out of the level (JC5's is placed over a void: INFERRED cleanup) */
    face(e, *E > 0 ? 0 : 16);
    /* the sprite (10984-10997; its frame advanced here: speed |C| * 8 on the walk / run, 10949) */
    const char *an = "jee_idle"; int sp = -1;
    if (*D == 0 && *H != 0) { an = fabs(*H) < 5 ? "jee_walk" : "jee_run"; if (fabs(*H) * 8 >= 20 || fabs(*H) >= 5) sp = (int)(fabs(*H) * 8); }
    else if (*D == -1) an = "jee_jump";
    else if (*D == 1) {                                                            /* 10990-10993: charge, glow, shoot */
        if (!a_is(e, "jee_shoot") && !a_is(e, "jee_charge")) { restart_a(e, "jee_charge"); *F = 0; }
        if (!a_is(e, "jee_shoot") && e->tm > 1) { *F += 1;
            if (*F == 1) { double ax, ay; relic_act_point(e, &ax, &ay); obj_t *g = spawn(OT_JC_GLOW, "jee_glow", (float)ax, (float)ay, 0); if (g) g->layer = 3; } }
        if (!a_is(e, "jee_shoot") && *F > 25) { restart_a(e, "jee_shoot"); *F = 0; e->tm = 0; }
        an = NULL;
    }
    else if (*D == 2) an = "jee_rec"; else if (*D == -2) an = "jee_knock"; else if (*D == 3) an = "jee_down"; else if (*D == 4) an = "jee_up";
    if (an && !a_is(e, an)) { set_a(e, an); e->tm = 0; e->av[7] = 0; }
    if (e->an.b) {                                                                 /* frame advance (Active::update: > 100) */
        const anim_t *ad = &G.ens.anims[e->an.anim];
        e->av[7] += sp >= 0 ? sp : ad->speed;
        while (e->av[7] > 100) { e->av[7] -= 100; if (e->tm + 1 < ad->count) e->tm++; else if (ad->loop != 1) e->tm = ad->back_to; }
        int was = e->an.frame; e->an.frame = e->tm; e->an.acc = 0; e->an.done = 0;
        if (a_is(e, "jee_shoot")) {                                                /* 10949 / 11015: a laser at frame 1 */
            if (e->tm == 1 && was != 1) elite_laser(e, *E > 0 ? 1 : -1);
        }
    }
    if (*D == 6) {                                                                 /* 11029 / 10870 */
        snd_play_orig(29, 220, 128); obj_effect(m->x, m->y - 20, "explosion"); debris(m, 4);
        if (chr_is_carol(G.character)) { obj_t *f = obj_alloc(); if (f) { memset(f, 0, sizeof *f); f->type = OT_FUEL; f->alive = 1; f->layer = 2; f->x = m->x; f->y = m->y - 20; int a = bank_anim(&G.objs, "objectfuel_533"); if (a >= 0) anim_set(&f->an, &G.objs, a); } }
        m->alive = 0; e->alive = 0; return;
    }
    /* 14408 (BG4 / BG5 / FD1 / FD2 only, jc_enemy_elite_view_alert): an idle, calm Elite inside the view (+50) alerts while the
     * player is not in MV.M 9 */
    if (S.view_alert && *D == 0 && *Q == 0 && in_view(m, 50) && player_orig_m(p) != 9) *Q = 1;
}
/* a runtime Elite (mask + sprite) with its feet at (x, y): the mask's ctor values (A 20, E 1, J 1) and flag 16 (BG5 14661 / FD
 * 15245: the spawners set F = x, G = y and flag 16 on the new mask). Usable from any stage: the module starts if it was idle. */
static void lim_collect(void) {
    if (S.lim_done) return;
    S.lim_done = 1; S.nlim = 0;
    for (int i = 0; i < G.nobjs && S.nlim < 64; i++) { const obj_t *o = &G.o[i]; if (o->alive && o->type == OT_LIMITER && o->x >= 0) { S.lim_x[S.nlim] = (short)o->x; S.lim_y[S.nlim++] = (short)o->y; } }
}
obj_t *jc_enemy_elite_create(float x, float y) {
    obj_t *m = spawn(OT_JC_ELITEMASK, NULL, x, y, 0);
    if (!m) return NULL;
    obj_t *e = spawn(OT_JC_ELITE, NULL, x, y, 0);
    if (!e) { m->alive = 0; return NULL; }
    S.on = 1; lim_collect();
    elite_spawn(m);
    set_a(e, "jee_idle"); e->layer = 2;
    e->par = (int)(m - G.o); m->par = (int)(e - G.o);
    return m;
}
void jc_enemy_elite_view_alert(int on) { S.view_alert = on; }
static void laser_frame(obj_t *l, player_t *p) {                                  /* 11034-11039 */
    double *A = &l->av[0], *B = &l->av[1], *C = &l->av[2], *D = &l->av[3], *E = &l->av[4];
    *C += c_cos(*B) * *A; *D += -c_sin(*B) * *A; *E += 1;
    l->x = (float)(int)*C; l->y = (float)(int)*D;
    if ((*E > 3 && obj_overlaps_bg(l)) || *E > 300) { obj_effect(l->x, l->y, "wburst"); l->alive = 0; return; }
    if (l->fl & FL_REFL) {                                                         /* 11038: a reflected laser hits an enemy */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *e = &G.o[i];
            if (!e->alive || !enemy_type(e->type) || !e->an.b || e->c > 0 || (e->fl & 0x60) || fabsf(e->x - l->x) > 96 || fabsf(e->y - l->y) > 96 || !obj_obj_hit(l, e)) continue;
            snd_play_orig(24, 220, 128); e->kx = e->ky = 0; e->c = 5; e->A -= 10; e->inv = 1;
            obj_effect(l->x, l->y, "wburst"); l->alive = 0; return;
        }
        return;
    }
    if (player_cube_state() >= 4 && player_cube_hits(l)) { l->fl |= FL_REFL; *B = fmod(*B + 180, 360); l->flip = !l->flip; return; }   /* (Milla's cube reflects it: INFERRED handler) */
    if (csens_hit(p, l) && player_orig_m(p) != 9 && r_hurt(p, 10, 2)) { obj_effect(l->x, l->y, "wburst"); l->alive = 0; }   /* 11039 */
}
static void glow_frame(obj_t *g) {                                                /* 11031-11033: A += 1, fades, removed at 25 */
    g->av[0] += 1; g->scale = (float)fmax(0.1, 1 - g->av[0] * 0.04);
    if (g->av[0] >= 25) g->alive = 0;
}

static void tbomb_frame(obj_t *o) {                                               /* 10814-10817 (the DV bomb's fall: +0.3, max 12) */
    o->y = (float)(int)o->fy; o->fy += o->vy; o->vy = fminf(o->vy + 0.3f, 12.f);
    o->ang = o->vy > -2 ? (float)fmax(o->vy - 5, 0) : 0;                          /* 10816: angle B - 5 once falling */
    if (o->vy > 0 && obj_overlaps_bg(o)) {                                         /* RV launcher: bursts ~63 frames later with sound 38 */
        obj_t *b = obj_alloc();
        if (b) { memset(b, 0, sizeof *b); b->type = OT_BLAST; b->alive = 1; b->layer = 2; b->x = o->x - 12; b->y = o->y; b->par = -1; int a = ens_anim("blast"); if (a >= 0) anim_set(&b->an, &G.ens, a); }
        snd_play_orig(38, 220, 128); shake(3); o->alive = 0;
    }
}

/* ================================================================ runtime effects */
static void mexpl_frame(obj_t *o) {                                               /* 10247: explosion06 X += A, Y += B; force_speed 50 */
    o->x = (float)(int)(floorf(o->x) + o->vx); o->y = (float)(int)(floorf(o->y) + o->vy);   /* set_x(int(X + A)) (RV meteor: +4 px a frame at 45 deg) */
    anim_tick(&o->an, 30);                                                         /* (+ the row's 20 in update_objects = 50) */
    if (o->an.done) o->alive = 0;
}

/* ================================================================ spawn / passes */
void jc_enemy_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); }   /* host search snapshots */
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS);
    for (int t = OT_JCE_FIRST; t <= OT_JCE_LAST && !S.on; t++) S.on = objects_have(t);
    if (!S.on) return;
    S.lim_done = 1;                                                                /* (the limiters are collected by this loop) */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_LIMITER && S.nlim < 64 && o->x >= 0) { S.lim_x[S.nlim] = (short)o->x; S.lim_y[S.nlim++] = (short)o->y; continue; }
        if (o->type < OT_JCE_FIRST || o->type > OT_JCE_LAST) continue;
        if (o->x < 0 || o->y < -400) { o->alive = 0; continue; }                 /* off-map templates */
        memset(o->av, 0, sizeof o->av); o->st = o->tm = 0; o->fl = 0; o->dir = 0; o->an.b = NULL; o->par = -1; o->layer = 2; o->hidden = 0;
        lst_add(o);
        switch (o->type) {
        case OT_JC_METEOR: meteor_spawn(o); break;
        case OT_JC_MFLAME: set_a(o, "jem_flame"); o->hidden = 1; break;
        case OT_JC_BUOY: buoy_spawn(o); break;
        case OT_JC_PLANT: plant_spawn(o); break;
        case OT_JC_PHAT: set_a(o, "jep_hat"); o->layer = 1; break;
        case OT_JC_PSTALK: set_a(o, "jep_stalk"); o->layer = 1; break;
        case OT_JC_FROG: frog_spawn(o); break;
        case OT_JC_FROGMASK: o->hidden = 1; break;
        case OT_JC_SHARK: shark_spawn(o); break;
        case OT_JC_ORBITER: orbiter_spawn(o); break;
        case OT_JC_LAUNCHER: set_a(o, "jet_hatch"); o->an.frame = 0; break;
        case OT_JC_ELITEMASK: case OT_JC_ELITE:                                    /* RV elite (JC5): no instance exists at the act start */
            if (level_cur()->stage == 5) { o->alive = 0; break; }                  /* (not created at start; nothing in JC5 creates one) */
            if (o->type == OT_JC_ELITEMASK) elite_spawn(o); else { set_a(o, "jee_idle"); o->layer = 2; }
            break;
        default: o->alive = 0; break;
        }
    }
    { obj_t *o; EACH(OT_JC_ORBITER, o) orbiter_ring(o); }                          /* (after the placed objects: the loop above drops runtime types) */
    pair_nearest(OT_JC_ELITE, OT_JC_ELITEMASK); { obj_t *m, *e; EACH(OT_JC_ELITEMASK, m) EACH(OT_JC_ELITE, e) if (e->par == (int)(m - G.o)) m->par = (int)(e - G.o); }
    pair_nearest(OT_JC_MFLAME, OT_JC_METEOR); pair_nearest(OT_JC_PHAT, OT_JC_PLANT); pair_nearest(OT_JC_PSTALK, OT_JC_PLANT);
    if (jc_flow_act() != 3) jc_enemy_hover_start();                                /* JC4 10850: the launchers run from the start */
}
void jc_enemy_pre(player_t *p) {                                                 /* test_collisions + the pick-one triggers */
    if (!S.on) return;
    obj_t *o;
    EACH(OT_JC_METEOR, o) {                                                        /* col_3_39: limiter -> G = -G (on contact) */
        int h = o->av[0] == 4 && limiter_hit(o);
        if (h && !(o->fl & FL_LIM)) o->av[3] = -o->av[3];
        o->fl = h ? (o->fl | FL_LIM) : (o->fl & ~FL_LIM);
    }
    EACH(OT_JC_SHARK, o) if (limiter_hit(o)) o->av[5] = -o->av[5];                 /* 10320 */
    if (dead(p)) return;
    meteor_trigger(p); plant_trigger(p); shark_trigger(); meteor_hits();
}
void jc_enemy_draw(int pass) {                                                   /* the orbiters with their rings (10473 / 10474 order) */
    if (!S.on || pass != 1 || !NLS[OT_JC_ORBITER - RT0]) return;
    obj_t *o, *s;
    EACH(OT_JC_ORBITER, o) {
        float ox = floorf(o->x - G.camx), oy = floorf(o->y - G.camy);
        if (ox < -160 || ox > SCREEN_W + 160 || oy < -120 || oy > SCREEN_H + 120) continue;
        for (int front = 0; front < 2; front++) {
            EACH(OT_JC_OSHOT, s) {
                if (s->par != (int)(o - G.o) || !s->an.b) continue;
                int back = s->av[0] > 90 && s->av[0] < 270;
                if (back == front) continue;
                r_set_scale(s->scale);
                frame_draw_rot(s->an.b, anim_frame(&s->an), floorf(s->x - G.camx), floorf(s->y - G.camy), 0, 0, 0xffffffff);
                r_set_scale(1.f);
            }
            if (!front && o->an.b && !DEFEATED(o)) {
                if (o->inv) r_set_invert(1);
                frame_draw(o->an.b, anim_frame(&o->an), ox, oy, 0, 0xffffffff);
                if (o->inv) r_set_invert(0);
            }
        }
    }
}
int jc_enemy_obj(obj_t *o, player_t *p) {
    if (!S.on) return 0;
    switch (o->type) {
    case OT_JC_METEOR: meteor_frame(o, p); return 1;
    case OT_JC_MFLAME: flame_frame(o, p); return 1;
    case OT_JC_BUOY: buoy_frame(o, p); return 1;
    case OT_JC_BJET: bjet_frame(o, p); return 1;
    case OT_JC_PLANT: plant_frame(o, p); return 1;
    case OT_JC_PHAT: case OT_JC_PSTALK: plant_parts(o); return 1;
    case OT_JC_LEAF: leaf_frame(o); return 1;
    case OT_JC_FROG: frog_frame(o, p); return 1;
    case OT_JC_BUBBLE: bubble_frame(o, p); return 1;
    case OT_JC_SHARK: shark_frame(o, p); return 1;
    case OT_JC_TORP: torp_frame(o, p); return 1;
    case OT_JC_ORBITER: orbiter_frame(o, p); return 1;
    case OT_JC_OSHOT: oshot_frame(o, p); return 1;
    case OT_JC_LAUNCHER: launcher_frame(o, p); return 1;
    case OT_JC_MEXPL: mexpl_frame(o); return 1;
    case OT_JC_ELITEMASK: elite_frame(o, p); return 1;
    case OT_JC_LASER: laser_frame(o, p); return 1;
    case OT_JC_GLOW: glow_frame(o); return 1;
    case OT_JC_TBOMB: tbomb_frame(o); return 1;
    case OT_JC_WARN: return 1;
    default: return 1;
    }
}
