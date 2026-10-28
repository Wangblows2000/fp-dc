/* Enemy kit (session 21b): the helpers every stage enemy module re-wrote as private statics (jc_enemy.c, sb_enemy.c,
 * fn_enemy.c, relic.c ...), written once so later stages include this header instead of copying them. All functions are
 * static inline (each module gets its own copy of nothing it does not use). First user: th_enemy.c (Trap Hideout).
 *   - per-type instance lists (ek_list_t + EK_EACH): the original's per-object-type ObjectLists, so no event scans every
 *     object slot each frame (Dreamcast budget);
 *   - ENS sprite rows: '<base>' = direction 0, '<base>_l' = direction 16 with a fallback to the base row drawn mirrored
 *     (importer: exact-mirror left rows are not exported, VRAM), Fusion's force_animation (frame 0, speed counter kept),
 *     set_direction (frame kept);
 *   - the player's PlayerCharacterSensor box and the "R-hurt n" rule (MV.R = 1, LIFE.B += n when R = 0, E <= 0);
 *   - EnemyLimiter_383 overlap (image 16 x 32, hotspot (21, 18)), collected once per level;
 *   - the "burst" death of the big enemies (JC frog / shark, TH Rocket Puncher: explosion06 every 7th frame, then the
 *     explosion01, the keystone, the debris);
 *   - Fusion object pairing by placement (a child takes the nearest owner).
 * Labels as in the modules: RE (decompiled events), RV (original traces), INFERRED. */
#ifndef FP_ENEMY_KIT_H
#define FP_ENEMY_KIT_H
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define EK_RAD 57.29577951308232
#define EK_LMAX 96
typedef struct { short idx[EK_LMAX]; int n; } ek_list_t;
/* every live instance of type T in list L (a slot whose object died or was retyped is skipped) */
#define EK_EACH(L, T, OB) for (int _k = 0; _k < (L)->n; _k++) if (((OB) = &G.o[(L)->idx[_k]])->alive && (OB)->type == (T))
static inline void ek_list_add(ek_list_t *L, const obj_t *q, int type) {
    int i = (int)(q - G.o);
    for (int k = 0; k < L->n; k++) if (L->idx[k] == i) return;
    if (L->n < EK_LMAX) { L->idx[L->n++] = (short)i; return; }
    for (int k = 0; k < EK_LMAX; k++) { const obj_t *o = &G.o[L->idx[k]]; if (!o->alive || o->type != type) { L->idx[k] = (short)i; return; } }
}

/* ---- math as the original evaluates it (Chowdren's sin / cos of degrees, exact zeros at the axes) */
static inline double ek_nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static inline double ek_sin(double deg) { double r = ek_nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / EK_RAD); }
static inline double ek_cos(double deg) { double r = ek_nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / EK_RAD); }
static inline double ek_px(const player_t *p) { return floor(p->x); }                   /* Player_326 X */
static inline double ek_py(const player_t *p) { return floor(p->y - ORIG_REF_DY); }     /* Player_326 Y */
static inline int ek_dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }

/* ---- ENS sprite rows */
static inline int ek_row(const obj_t *o, const char *base) {
    char nm[24]; snprintf(nm, sizeof nm, "%s%s", base, (o->dir & 31) == 16 ? "_l" : "");
    int a = ens_anim(nm);
    return a >= 0 ? a : ens_anim(base);
}
/* a direction-16 row the importer left out (an exact mirror) is the base row drawn flipped; flip 2 moves it 1 px right
 * (the original's left hotspot is w - 1 - hx, frame_draw's flip uses w - hx; objects.c draws flip 2) */
static inline void ek_mirror_fix(obj_t *o, const char *base) {
    char nm[24]; snprintf(nm, sizeof nm, "%s_l", base);
    if ((o->dir & 31) == 16 && ens_anim(nm) < 0 && ens_anim(base) >= 0) o->flip = 2;
    else if (o->flip == 2) o->flip = 0;
}
/* force_animation: frame 0, the speed counter kept. A finished (non-looping) animation holds the counter it had before the
 * tick that tried to pass its last frame (RV th_enemies/puppet: anim 7 ends with the counter frozen at 80, anim 8 then
 * advances on its first tick); anim_tick leaves the remainder, so the kit restores the held value. */
static inline int ek_held_acc(const animator_t *an) {
    if (!an->b) return 0;
    return an->done ? an->acc + 100 - an->b->anims[an->anim].speed : an->acc;
}
static inline void ek_set_a(obj_t *o, const char *base) {
    ek_mirror_fix(o, base);
    int a = ek_row(o, base);
    if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) { int acc = ek_held_acc(&o->an); anim_set(&o->an, &G.ens, a); o->an.acc = acc; }
}
static inline void ek_restart_a(obj_t *o, const char *base) { ek_mirror_fix(o, base); int a = ek_row(o, base); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static inline int ek_a_is(const obj_t *o, const char *base) { return o->an.b == &G.ens && o->an.anim == ek_row(o, base); }
static inline void ek_face(obj_t *o, int dir) {                    /* set_direction: animation frame and counter kept */
    if ((o->dir & 31) == dir) return;
    char stem[24] = "";
    if (o->an.b == &G.ens) { snprintf(stem, sizeof stem, "%s", G.ens.anims[o->an.anim].name); size_t n = strlen(stem); if (n > 2 && !strcmp(stem + n - 2, "_l")) stem[n - 2] = 0; }
    o->dir = dir;
    if (!stem[0]) return;
    ek_mirror_fix(o, stem);
    int a = ek_row(o, stem);
    if (a < 0 || a == o->an.anim) return;
    animator_t k = o->an;
    o->an.anim = a; o->an.frame = k.frame < G.ens.anims[a].count ? k.frame : G.ens.anims[a].count - 1;
}
/* rows of any direction d (session 21c, TB): '<base>' (0), '<base>_l' (16), '<base>_<d>'; a missing row is the row of the mirror
 * direction (16 - d) & 31 drawn flipped (importer _tbe_mirror_rows), else the nearer of 0 / 16. *flip = 1 when drawn mirrored. */
static inline void ek_dname(char *nm, size_t n, const char *base, int d) {
    d &= 31;
    if (d == 0) snprintf(nm, n, "%s", base); else if (d == 16) snprintf(nm, n, "%s_l", base); else snprintf(nm, n, "%s_%d", base, d);
}
static inline int ek_row_d(const char *base, int d, int *flip) {
    char nm[24]; int a;
    *flip = 0; ek_dname(nm, sizeof nm, base, d); if ((a = ens_anim(nm)) >= 0) return a;
    *flip = 1; ek_dname(nm, sizeof nm, base, (16 - d) & 31); if ((a = ens_anim(nm)) >= 0) return a;
    d &= 31; int side = (d > 8 && d < 24) ? 16 : 0;
    *flip = 0; ek_dname(nm, sizeof nm, base, side); if ((a = ens_anim(nm)) >= 0) return a;
    *flip = 1; ek_dname(nm, sizeof nm, base, 16 - side); return ens_anim(nm);
}
static inline int ek_same_family(int a, const char *base) {   /* row a is '<base>' or '<base>_<suffix>' */
    if (a < 0 || a >= G.ens.nanims) return 0;
    const char *n = G.ens.anims[a].name; size_t k = strlen(base);
    return !strncmp(n, base, k) && (n[k] == 0 || n[k] == '_');
}
/* force_animation(base) in the object's direction: a new animation starts at frame 0 (speed counter kept, ek_held_acc); a
 * direction change within the same animation keeps the frame (set_direction) */
static inline void ek_set_ad(obj_t *o, const char *base) {
    int fl, a = ek_row_d(base, o->dir, &fl);
    if (a < 0) return;
    o->flip = fl ? 2 : 0;
    if (o->an.b == &G.ens && o->an.anim == a) return;
    if (o->an.b == &G.ens && ek_same_family(o->an.anim, base)) {
        animator_t k = o->an; o->an.anim = a; o->an.frame = k.frame < G.ens.anims[a].count ? k.frame : G.ens.anims[a].count - 1;
        return;
    }
    int acc = ek_held_acc(&o->an); anim_set(&o->an, &G.ens, a); o->an.acc = acc;
}
static inline void ek_face_d(obj_t *o, int d) {                   /* set_direction for ek_set_ad rows (frame and counter kept) */
    d &= 31;
    if ((o->dir & 31) == d) return;
    o->dir = d;
    if (o->an.b != &G.ens) return;
    char stem[24]; snprintf(stem, sizeof stem, "%s", G.ens.anims[o->an.anim].name);
    char *u = strrchr(stem, '_');
    if (u && u != stem && strchr(stem, '_') != u && (!strcmp(u + 1, "l") || (u[1] && strspn(u + 1, "0123456789") == strlen(u + 1)))) *u = 0;
    ek_set_ad(o, stem);
}
static inline void ek_restart_ad(obj_t *o, const char *base) { int fl, a = ek_row_d(base, o->dir, &fl); if (a >= 0) { o->flip = fl ? 2 : 0; anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static inline int ek_ad_is(const obj_t *o, const char *base) { return o->an.b == &G.ens && ek_same_family(o->an.anim, base); }
static inline void ek_force_frame(obj_t *o, int f) {               /* Active::force_frame (held: no advance, INFERRED as SB's) */
    if (!o->an.b) return;
    int n = o->an.b->anims[o->an.anim].count;
    o->an.frame = f < 0 ? 0 : f >= n ? n - 1 : f; o->an.acc = 0;
}

/* ---- the player: PlayerCharacterSensor (+0x4f30) box, "R-hurt n" */
static inline int ek_csens_hit(const player_t *p, const obj_t *o) {
    int X = (int)ek_px(p), Y = (int)ek_py(p), small = player_char_sensor_small(p);
    return o->an.b && obj_rect_hit(o, X - 7, small ? Y - 13 : Y - 34, X + 9, small ? Y + 15 : Y + 13);
}
static inline int ek_vulnerable(const player_t *p) { return !ek_dead(p) && !p->hurt_pending && !player_invincible(p); }
/* MV.R = 1, LIFE.B += n when MV.R == 0 and OTHERS.E <= 0, unless OTHERS.B is the immune shield (0: none) */
static inline int ek_r_hurt(player_t *p, int n, int immune) { if (!ek_vulnerable(p) || (immune && p->shield == immune)) return 0; player_contact(p, n); return 1; }
static inline void ek_shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }   /* CAM.Z = max(CAM.Z, z) */
static inline void ek_add_mvB(player_t *p, double d) { if (p->ground) p->gsp += d; else p->vx += d; }   /* MV.B += d */
/* the player's main object Player_326 (+0x43f0) against an object (check_overlap(obj, Player_326)) */
static inline int ek_p326_hit(const obj_t *o) {
    int ox, oy, h; const signed char *a, *b;
    return o->an.b && mv_sensor_rows(SENS_MAIN, &ox, &oy, &a, &b, &h) && obj_rows_hit(o, ox, oy, a, b, h);
}
/* a mask object's sensor (the Elite / bike ninja masks: force_direction(d) then overlaps the background, a Q7 object, or a
 * Q8 object from above (d 1)): row '<base><d>' */
static inline int ek_mask_sensor(obj_t *m, const char *base, int d) {
    char nm[16]; snprintf(nm, sizeof nm, "%s%d", base, d);
    animator_t keep = m->an; int a = ens_anim(nm), hit = 0;
    if (a < 0) return 0;
    anim_set(&m->an, &G.ens, a);
    hit = obj_overlaps_bg(m) || objects_mask_hit_groups(m, d == 1);   /* (group candidate lists, not a scan of every object) */
    m->an = keep;
    return hit;
}

/* ---- EnemyLimiter_383 */
typedef struct { short x[96], y[96]; int n; } ek_limiters_t;
static inline void ek_limiters_collect(ek_limiters_t *L) {
    L->n = 0;
    for (int i = 0; i < G.nobjs && L->n < 96; i++) { const obj_t *o = &G.o[i]; if (o->alive && o->type == OT_LIMITER && o->x >= 0) { L->x[L->n] = (short)o->x; L->y[L->n++] = (short)o->y; } }
}
static inline int ek_limiter_hit(const ek_limiters_t *L, const obj_t *o) {
    for (int k = 0; k < L->n; k++)
        if (fabsf(L->x[k] - o->x) < 96 && fabsf(L->y[k] - o->y) < 96 &&
            obj_rect_hit(o, L->x[k] - 21, L->y[k] - 18, L->x[k] - 5, L->y[k] + 14)) return 1;
    return 0;
}

/* ---- effects */
static inline void ek_debris(const obj_t *o, int n) {             /* loop_spawndebris (motion INFERRED as combat.c) */
    int a = ens_anim("debris");
    for (int k = 0; k < n && a >= 0; k++) {
        obj_t *d = obj_alloc();
        if (!d) break;
        memset(d, 0, sizeof *d);
        d->type = OT_DEBRIS; d->alive = 1; d->layer = 2; d->x = o->x; d->y = o->y; anim_set(&d->an, &G.ens, a); d->an.frame = 10 - k;
        d->vx = (float)(k - 1.5) * 1.5f; d->vy = -3.f - k * 0.5f;
    }
}
static inline int ek_explode_snd(void) { static const int S3[3] = { 311, 319, 320 }; return S3[cross_rand(3)]; }   /* loop_sound_explode3..5 (RE) */
/* the big enemies' end (JC 10289 / 10334, TH 13681-13683): every 7th frame (g3 % 7) an explosion06 at +-20 and a sound
 * (each: an id, 0 = sound_explode3..5), Q += 1; at Q 10 explosion01, a keystone, debris and the last sound; returns 1 when gone */
static inline int ek_burst_death(obj_t *o, double *Q, int each, int last) {
    if (G.frame % 7 == 0 && *Q < 10) {
        obj_effect(floorf(o->x) - 20 + cross_rand(41), floorf(o->y) - 20 + cross_rand(41), "explosion06"); *Q += 1;
        snd_play_orig(each ? each : ek_explode_snd(), 220, 128);
    }
    if (*Q == 10) {
        obj_effect(o->x, o->y, "explosion"); keystone_spawn(o->x, o->y, 0, 0); ek_debris(o, 4);
        snd_play_orig(last ? last : ek_explode_snd(), 220, 128); o->alive = 0;
        return 1;
    }
    return 0;
}

/* ---- Fusion object pairing by instance order: here, each child takes the nearest owner (by placement) */
static inline void ek_pair_nearest(const ek_list_t *L, int child, int owner) {
    obj_t *o, *q;
    EK_EACH(L, child, o) { float best = 1e9f; EK_EACH(L, owner, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->par = (int)(q - G.o); } } }
}
static inline obj_t *ek_partner(const obj_t *o) { return o->par >= 0 && o->par < MAX_OBJS && G.o[o->par].alive ? &G.o[o->par] : NULL; }

/* ---- a runtime object on the ENS bank */
static inline obj_t *ek_spawn(ek_list_t *L, int type, const char *anim, float x, float y, int dir) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = 2; o->born = G.frame; o->par = -1; o->dir = dir;
    if (anim) ek_set_a(o, anim);
    if (L) ek_list_add(L, o, type);
    return o;
}
#endif
