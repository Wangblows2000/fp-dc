/* Relic Maze golem minibosses (session 14): the RM2 Eye Golem (MinibossGolem_1259 + lid 1258 + leg blocks 1257 / 1256,
 * shots 1260 / 1261, flames 1264) and the RM5 Bone Golem (core 1567, spine 1570, legs 1566 / 1568 / 1569, neck 1571-1574,
 * head 1575, shockwaves 1576). Source: build/re/spec_rm_golems.md (static RE; traces T1-T12 not recorded yet).
 * The eye and the core are ordinary Q20 enemies for combat.c (HP A, armour flag 5, fly-away); everything else is their
 * own events here, after the shared rules, in the original's order. Rules are attached to the object types; the arena
 * comes from the level's own CameraLimitBoss / CameraTriggerBoss markers. Labels RE / INFERRED. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEFEATED(o) ((o)->fl & 0x40)
#define F_ARMOURED 0x20                                                        /* enemy flag 5 (combat.c) */
static int R(int n) { return cross_rand(n); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / 57.29577951308232); }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / 57.29577951308232); }
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static void set_ens(obj_t *o, const char *name) { int a = ens_anim(name); if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void restart_ens(obj_t *o, const char *name) { int a = ens_anim(name); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static int anim_named(const obj_t *o, const char *name) { return o->an.b == &G.ens && o->an.anim == ens_anim(name); }
static obj_t *spawn(int type, const char *anim, float x, float y) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = 2; o->born = G.frame;
    if (anim) set_ens(o, anim);
    return o;
}
static obj_t *first_of(int t) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == t) return &G.o[i]; return NULL; }
static int count_of(int t) { int n = 0; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == t) n++; return n; }
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {
    int X = (int)PX(p), Y = (int)PY(p);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return o->an.b && obj_rect_hit(o, a, b, c, d); }
static int vulnerable(const player_t *p) { return p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p); }   /* MV.R = 0, OTHERS.E <= 0 */
static int overlaps_level(const obj_t *o) {                                       /* background or Q7 */
    if (obj_overlaps_bg(o)) return 1;
    for (int i = 0; i < G.nobjs; i++) { const obj_t *s = &G.o[i]; if (s->alive && s != o && s->an.b && obj_in_group(s->type, OBJGRP_SOLID) && fabsf(s->x - o->x) < 160 && fabsf(s->y - o->y) < 160 && obj_obj_hit(o, s)) return 1; }
    return 0;
}
static void shake(float z) { float n = G.cam.Z + z; G.cam.Z = n < z ? n : z; }      /* CAM.Z = min(Z + z, z) */
/* the action point of the current frame, relative to the hot spot, rotated by the object's angle (Active::update) */
static void act_rot(const obj_t *o, double *x, double *y) {
    *x = o->x; *y = o->y;
    if (!o->an.b || !o->an.b->act) return;
    const frame_t *f = anim_frame(&o->an);
    int fi = (int)(f - o->an.b->frames);
    double dx = o->an.b->act[2 * fi] - f->hx, dy = o->an.b->act[2 * fi + 1] - f->hy, a = o->ang;
    *x = o->x + dx * c_cos(a) + dy * c_sin(a); *y = o->y - dx * c_sin(a) + dy * c_cos(a);
}
void relic_act_point(const obj_t *o, double *x, double *y) { act_rot(o, x, y); }   /* shared: the Mantalith's claws (rm_mantalith.c) */
static void rock_debris(float x, float y, int speed) {                             /* EnemyRock_1194 debris: pinball, gravity 30, no collision (RE: no RM2 rock events) */
    static const int DIRS[12] = { 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14, 15 };
    obj_t *r = spawn(OT_DEBRIS, "rock", x, y);
    if (!r) return;
    double a = DIRS[R(12)] * 11.25;
    r->vx = (float)(speed * 0.1 * c_cos(a)); r->vy = (float)(-speed * 0.1 * c_sin(a));   /* PinballMovement: v / 10 px per frame */
}
static void rails_to_crystals(void) {                                            /* 5978 / 6859: each railing gives 2 crystals; railings and barriers go */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive) continue;
        if (o->type == OT_WR_RAIL) { obj_launch_crystal(o->x, o->y + 12, 7); obj_launch_crystal(o->x, o->y + 36, 9); o->alive = 0; }
        else if (o->type == OT_WR_BARRIER) o->alive = 0;
    }
    snd_play_orig(40, 220, 128);
}

/* ======================================================================= RM2 Eye Golem (spec Part A)
 * EYE: A HP, st D, av[0] E, av[1] F, av[2] G, av[3] H, av[4] I, av[5] J, av[6] K, av[7] L, tm M; c = (int) T, fx U, fy V;
 * fl 0x2000 flag 13. Pieces: av[0] A (segment), av[1] B, av[2] C, av[3] Z; fl 1 = flag 0 (not standable).
 * LID: av[0] A, av[1] B, dir 0 / 1. Group state: GE.active (0x22410), GE.phase 0 dormant 1 intro 2 walking 3 defeat. */
static struct { int phase, cam, started, lid_snd; obj_t *eye, *lid; } GE;
static void ge_pose(obj_t *e) {                                                   /* 5937-5943 */
    obj_t *lid = GE.lid;
    double F = e->av[1];
    int Fi = (int)F, m = Fi % 180;
    double E = m < 15 ? 15 : m > 165 ? 165 : m;                                  /* 5937 (C %: negative -> 15) */
    int solid = 0;
    if (e->st > 1 && E > 45 && E < 135) { E = 45; solid = 1; }                    /* 5939: the mid-stride pose is standable */
    e->av[0] = E;
    double T = 114 * (Fi >= 0 ? Fi / 180 : 0) + e->fy;                            /* 5940 */
    e->y = (float)(e->fx - fabs(c_cos(E)) * 128);
    e->x = (float)(E < 90 ? T + 128 * c_sin(E) : T + 180 - 128 * c_sin(E));      /* 5941 / 5942 */
    for (int i = 0; i < G.nobjs; i++) {                                           /* 5943 */
        obj_t *p = &G.o[i]; if (!p->alive || (p->type != OT_GP1 && p->type != OT_GP2)) continue;
        double A = p->av[0], a = p->type == OT_GP1 ? 90 - E : 90 + E;
        p->x = (float)(e->x + 32 * A * c_cos(a)); p->y = (float)(e->y + 32 * A * fabs(c_sin(a)));
        if (solid) p->fl &= ~1; else p->fl |= 1;
    }
    if (solid) e->fl &= ~1; else e->fl |= 1;
    if (lid) { double ax, ay; act_rot(e, &ax, &ay); lid->x = (float)ax; lid->y = (float)(ay + 32 * lid->dir); }
}
static void ge_number_pieces(void) {                                              /* 6041 / 5970: A = 1..4 per leg in selection order (last created first) */
    int n1 = 0, n2 = 0;
    for (int i = G.nobjs - 1; i >= 0; i--) {
        obj_t *p = &G.o[i]; if (!p->alive) continue;
        if (p->type == OT_GP1) p->av[0] = ++n1; else if (p->type == OT_GP2) p->av[0] = ++n2;
    }
}
static void ge_spawn(void) {
    memset(&GE, 0, sizeof GE);
    GE.eye = first_of(OT_GEYE); GE.lid = first_of(OT_GLID);
    obj_t *e = GE.eye;
    if (!e) return;
    e->A = 100; e->st = 0; e->bd = 0; memset(e->av, 0, sizeof e->av); e->av[1] = 270; e->av[4] = 2;   /* ctor: A 100, F 270, I 2 */
    e->fy = e->x; e->fx = e->y + 128; e->fl |= F_ARMOURED | 1 | F_NOFLY;                        /* 6041: V = T = x, U = y + 128; flag 5, flag 0 */
    set_ens(e, "geye");
    if (GE.lid) { GE.lid->hidden = 1; GE.lid->dir = 0; GE.lid->av[0] = GE.lid->av[1] = 0; set_ens(GE.lid, "glid0"); }
    for (int i = 0; i < G.nobjs; i++) { obj_t *p = &G.o[i]; if (p->alive && (p->type == OT_GP1 || p->type == OT_GP2)) { memset(p->av, 0, sizeof p->av); p->fl |= 1; set_ens(p, p->type == OT_GP1 ? "gp1" : "gp2"); } }
    ge_number_pieces();
}
static const obj_t *marker(int t) { return first_of(t); }
static int ge_zone(const player_t *p) {                                            /* 5928: x >= CamLimitLeft - 199, y <= CamLimitBottom + 99, a railing left */
    const obj_t *l = marker(OT_CAMLIM_L), *b = marker(OT_CAMLIM_B);
    if (!l || !b || !first_of(OT_WR_RAIL)) return 0;
    return p->x >= l->x - 199 && PY(p) <= b->y + 99;
}
static void ge_camera(const player_t *p) {                                         /* 5929 / 5930 (spec_rm_flow 2.3) */
    const obj_t *l = marker(OT_CAMLIM_L), *r = marker(OT_CAMLIM_R), *t = marker(OT_CAMLIM_T), *b = marker(OT_CAMLIM_B);
    if (!l || !r || !t || !b) return;
    double X = p->x, Y = PY(p);
    if (!GE.cam && X > l->x && X < 2727 && Y < b->y) { GE.cam = 1; camera_boss_bounds((int)l->x, (int)t->y, (int)r->x, (int)b->y); }   /* 5929 (once) */
    else if (GE.cam == 1 && X >= r->x - 39) { GE.cam = 2; level_release_camera(0); }   /* 5930: past the arena's right side */
}
static void ge_drop_piece(int type) {                                              /* 5959 / 5962: a random piece of that leg starts falling */
    int cand[8], n = 0;
    for (int i = 0; i < G.nobjs && n < 8; i++) { obj_t *p = &G.o[i]; if (p->alive && p->type == type && p->av[1] == 0) cand[n++] = i; }
    if (!n) return;
    obj_t *p = &G.o[cand[n > 1 ? R(n) : (R(1), 0)]];
    GE.eye->av[3] += 1; p->av[5] = p->y; p->av[1] = p->y + 128; p->av[2] = 0;     /* A (av[5] here) = y, B = y + 128 */
}
static void ge_shot_new(obj_t *e) {
    obj_t *s = spawn(OT_GSHOT, "gshot", e->x, e->y);
    if (!s) return;
    s->dir = GE.lid ? GE.lid->dir : 1; s->av[0] = e->x; s->av[1] = e->y;
    s->av[4] = count_of(OT_GSHOT) - 3;                                            /* E = count field - 4 (count field = live + 1) */
    snd_play_orig(71, 220, 128);
}
static void ge_frame(player_t *p) {
    obj_t *e = GE.eye;
    if (!e) return;
    int active = ge_zone(p) || GE.phase == 3;
    if (!active) return;                                                          /* 5927 / 5928: the golem freezes outside the zone */
    ge_camera(p);
    if (e->alive && !DEFEATED(e) && GE.phase < 3) {
        for (int i = 0; i < G.nobjs; i++) {                                       /* 5955: the pushable block crushes the eye (records x2 y34: not ported) */
            obj_t *b = &G.o[i];
            if (b->alive && (b->type == OT_PBLOCK1 || b->type == OT_PBLOCK2) && obj_obj_hit(b, e)) { e->A -= 100; b->av[5] = 0; break; }
        }
    }
    if (GE.phase == 0) {                                                          /* dormant (0x2240e) */
        if (!GE.started) {                                                        /* first active frame: the pose once, then 5957 (the ledge) */
            GE.started = 1; ge_pose(e);
            e->y -= 128;
            for (int i = 0; i < G.nobjs; i++) { obj_t *q = &G.o[i]; if (q->alive && (q->type == OT_GP1 || q->type == OT_GP2)) q->y = e->y; }
            if (GE.lid) GE.lid->hidden = 1;
        }
        const obj_t *trg = NULL;
        for (int i = 0; i < G.nobjs; i++) { const obj_t *t = &G.o[i]; if (t->alive && t->type == OT_CAMTRIGGER && t->x >= 0 && t->y >= 0 && fabsf(t->x - e->x) < 400) { trg = t; break; } }
        if (trg) {                                                                /* 5958: Player_326 on the trigger */
            float x0, y0, x1, y1;
            if (trg->an.b) { const frame_t *f = anim_frame(&trg->an); x0 = trg->x - f->hx; y0 = trg->y - f->hy; x1 = x0 + f->w; y1 = y0 + f->h; } else { x0 = trg->x; y0 = trg->y; x1 = trg->x + 16; y1 = trg->y + 192; }   /* CameraTriggerBoss image 1317: 16x192, hot spot (0, 0) */
            double px = PX(p), py = PY(p), cx = fmax(x0, fmin(px, x1 - 1)), cy = fmax(y0, fmin(py, y1 - 1));
            if ((cx - px) * (cx - px) + (cy - py) * (cy - py) <= 13.5 * 13.5) GE.phase = 1;
        }
        return;
    }
    if (GE.phase == 1) {                                                          /* intro (0x2240d, 5959-5970) */
        if (G.frame % 12 == 0) ge_drop_piece(OT_GP1);
        if (G.frame % 12 == 6) ge_drop_piece(OT_GP2);
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *q = &G.o[i]; if (!q->alive || (q->type != OT_GP1 && q->type != OT_GP2) || q->av[1] <= 0) continue;
            if (q->y < q->av[1]) { q->y = (float)(int)q->av[5]; q->av[5] += q->av[2]; q->av[2] += 0.2; }   /* 5960 / 5963 */
            if (q->av[2] > 0 && q->y >= q->av[1]) { q->av[2] = 0; q->y = (float)q->av[1]; shake(3); snd_play_orig(51, 220, 128); }   /* 5961 / 5964 */
            if (q->av[2] > 0 && csens_hit(p, q) && vulnerable(p)) player_contact(p, 5);   /* col_2_25: a falling block */
        }
        if (e->av[3] >= 8 && e->av[6] == 0) { e->av[5] = e->y; e->av[6] = e->y + 128; }   /* 5965 */
        if (e->av[6] > 0 && e->y < e->av[6]) { e->y = (float)(int)e->av[5]; e->av[5] += e->av[7]; e->av[7] += 0.2; }   /* 5966 */
        if (e->av[6] > 0 && e->av[7] > 0 && e->y >= e->av[6]) { e->av[7] = 0; e->y = (float)e->av[6]; shake(3); snd_play_orig(51, 220, 128); }   /* 5967 */
        if (e->av[7] > 0 && csens_hit(p, e) && vulnerable(p)) player_contact(p, 5);   /* col_18_25 */
        if (e->av[6] > 0 && e->y >= e->av[6]) {
            e->tm++;                                                              /* 5968 */
            if (e->tm == 10) restart_ens(e, "geye_opening");                     /* 5969: anim 7, direction 8 */
            if (anim_named(e, "geye_opening") && e->an.done) set_ens(e, "geye_open");
            if (e->tm >= 40) { GE.phase = 2; e->st = 1; ge_number_pieces(); e->fl &= ~F_ARMOURED; set_ens(e, "geye_open"); }   /* 5970 */
        }
        return;
    }
    if (GE.phase == 2) {                                                          /* walking pose / attack (0x2240c, 5931-5956) */
        if (DEFEATED(e)) {                                                        /* 5956 */
            GE.phase = 3; e->fl &= ~F_NOFLY;
            if (GE.lid) { GE.lid->alive = 0; GE.lid = NULL; }
            for (int i = 0; i < G.nobjs; i++) { obj_t *q = &G.o[i]; if (q->alive && (q->type == OT_GP1 || q->type == OT_GP2)) q->av[3] = q->type == OT_GP1 ? 30 * q->av[0] - 15 : 30 * q->av[0]; }
            return;
        }
        if (e->st == 1) { e->av[1] += 2; if (e->av[1] >= 305) { e->st = 2; e->av[2] = 1; e->av[1] = 305; } }   /* 5931 / 5932 */
        if (e->st == 2) { e->av[1] += 3 * e->av[2]; if (e->av[1] < 0) e->av[2] = 1; if (e->av[1] > 720) e->av[2] = -1; }   /* 5933-5936 */
        double Eraw = (int)e->av[1] % 180;
        int at_side = (e->av[2] > 0 && Eraw > 135) || (e->av[2] < 0 && Eraw <= 45);
        if (at_side && !GE.lid_snd) snd_play_orig(68, 220, 128);                  /* 5938 once */
        GE.lid_snd = at_side;
        ge_pose(e);
        int m = (int)e->av[1] % 180;
        if (e->st == 2 && m >= 80 && m <= 100 && !(e->fl & 0x2000)) { e->fl |= 0x2000; e->av[4] += 1; }   /* 5944 */
        if ((e->fl & 0x2000) && (m <= 79 || m > 100)) e->fl &= ~0x2000;          /* 5945 */
        if (e->st == 2 && e->av[4] >= 3) { e->st = 3; if (GE.lid) GE.lid->hidden = 0; }   /* 5946 */
        obj_t *lid = GE.lid;
        if (e->st == 3 && lid) {
            if (lid->av[0] < 100) { lid->av[0] += 1; lid->av[1] = fmin(lid->av[1] + 8, 150); }   /* 5947 */
            else lid->av[1] = fmax(lid->av[1] - 8, 0);                            /* 5948 */
            lid->ang = (float)(lid->dir == 0 ? -lid->av[1] : lid->av[1]);         /* 5949 / 5950 */
            set_ens(lid, lid->dir ? "glid1" : "glid0");
            double A = lid->av[0];
            if (lid->dir == 0 && A > 20 && A < 80) {
                if (!(lid->fl & 2)) { lid->fl |= 2; snd_play_orig(70, 220, 128); }   /* 5951 once */
                if (G.frame % 3 == 0) {                                           /* 5952: rising, swaying flames */
                    obj_t *f = relic_flame_new(e->x, e->y, 2.4 * c_sin(9 * A), -4 - fabs(1.6 * c_cos(9 * A)));
                    if (f) f->ang = 0;
                    lid->av[0] -= 0.5;
                }
            }
            if (lid->dir != 0 && A > 20 && A < 40 && G.frame % 5 == 0) ge_shot_new(e);   /* 5953 */
            if (lid->av[0] >= 100 && lid->av[1] <= 0) {                           /* 5954: the attacks alternate */
                lid->hidden = 1; lid->ang = 0; lid->av[0] = 0; lid->fl &= ~2; e->st = 2; e->av[4] = 0; lid->dir = (lid->dir + 1) % 2;
            }
        }
        for (int i = 0; i < G.nobjs; i++) {                                       /* 5979-5981: one-way, only below the player's feet */
            obj_t *q = &G.o[i];
            if (q->alive && (q->type == OT_GP1 || q->type == OT_GP2 || q->type == OT_GEYE) && q->y <= PY(p) + 15) q->fl |= 1;
        }
        return;
    }
    /* defeat (0x2240f, 5971-5978) */
    if (e->alive) {
        e->fl |= 1;                                                               /* 5971 */
        if (DEFEATED(e) && e->ky > 0 && overlaps_level(e)) {                      /* 5972: the eye crashes into 5 rocks */
            static const int OX[5] = { -12, 12, 0, 0, 0 }, OY[5] = { 0, 0, 0, 12, -12 };
            for (int k = 0; k < 5; k++) rock_debris(e->x + OX[k], e->y + OY[k], 80);
            obj_effect(e->x, e->y, "explosion"); snd_play_orig(29, 220, 128); e->alive = 0;
        }
        return;
    }
    int left = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *q = &G.o[i]; if (!q->alive || (q->type != OT_GP1 && q->type != OT_GP2)) continue;
        left = 1;
        q->av[3] -= 1;                                                            /* 5973 */
        if (q->av[3] <= 0) {                                                      /* 5974 / 5975: straight drop, spinning */
            q->y = (float)(int)(q->y + q->av[4]); q->av[4] = fmin(q->av[4] + 0.25, 15); q->ang += 8; q->fl |= 1;
            if (q->av[4] > 0 && overlaps_level(q)) {                              /* 5976 / 5977 */
                snd_play_orig(29, 220, 128);
                for (int k = 0; k < 4; k++) rock_debris(q->x + ((k & 1) ? 8 : -8), q->y + ((k & 2) ? 8 : -8), 60);
                obj_effect(q->x, q->y, "explosion"); q->alive = 0;
            }
        }
    }
    if (!left && GE.phase == 3) {                                                 /* 5978 */
        rails_to_crystals(); G.stage_flags |= 0x2; GE.phase = 4;
        if (GE.cam == 1) { GE.cam = 2; level_release_camera(0); }                /* 5982 intent (T3: the original may keep the lock; INFERRED release) */
    }
}
static void ge_shots(player_t *p) {                                                /* 5985-5995 + col_11_25 / col_4_25 / col_22_25 */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *s = &G.o[i]; if (!s->alive || s->type != OT_GSHOT) continue;
        if (!(s->fl & 0x40)) { s->x = (float)(int)s->av[0]; s->y = (float)(int)s->av[1]; s->av[0] += s->av[2]; s->av[1] += s->av[3]; }   /* 5985 */
        if (s->dir != 0) s->av[3] = fmin(s->av[3] + 0.2, 12);                    /* 5986 */
        int bg = obj_overlaps_bg(s);
        if (s->dir != 0 && bg && s->av[3] > 0) { s->av[3] = -5; snd_play_orig(72, 220, 128); }   /* 5987 */
        if (s->av[4] == 0) s->av[4] = 2;                                          /* 5988 */
        if (s->dir != 0 && s->av[3] < 0 && s->av[2] == 0) s->av[2] = s->av[4];   /* 5989 */
        if (bg && s->dir != 0 && (px_solid(&G.lvl, (int)s->x - 10, (int)s->y) || px_solid(&G.lvl, (int)s->x + 10, (int)s->y))) s->fl |= 0x40;   /* 5990 / 5991 */
        if (!obj_near_view(s, 100)) s->fl |= 0x40;                                /* 5992 */
        if (!(s->fl & 0x80) && csens_hit(p, s) && p->shield != 4 && vulnerable(p)) {   /* col_11_25: water shield immune */
            player_contact(p, 5); s->fl |= 0x40; p->tint = 60; p->tint_argb = 0xffff80ff;   /* pink tint 0xff80ff */
        }
        if (p->shield == 4 && csens_hit(p, s)) s->fl |= 0x40;                     /* col_22_25: the water shield pops it */
        if (player_cube_state() >= 4 && s->an.b && player_cube_hits(s)) s->fl |= 0x40;   /* col_4_25 */
        if ((s->fl & 0x40) && !(s->fl & 0x80)) {                                  /* 5993: pops into 4 harmless pieces */
            s->fl |= 0x80; s->alive = 0;
            for (int k = 0; k < 4; k++) {
                obj_t *d = spawn(OT_DEBRIS, "gshot2", s->x + ((k & 1) ? 6 : -6), s->y + ((k & 2) ? 6 : -6));
                if (d) { static const int D[12] = { 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14 }; double a = D[R(12)] * 11.25; d->vx = (float)(5 * c_cos(a)); d->vy = (float)(-5 * c_sin(a)); }
            }
            snd_play_orig(69, 220, 128); obj_effect(s->x, s->y, "explosion02");
        }
    }
}

/* ======================================================================= RM5 Bone Golem (spec Part B)
 * P5 (spine): av[0] A facing, av[1] B walk phase, av[2] C attack timer, av[3] D intro step, av[4] E, av[5] F turn limits,
 * av[6] G walk mode, av[7] H attack interval; fl 1 flag 0 (not standable), 2 flag 1 (player on the back), 8 flag 3, 0x80 flag 7.
 * Parts: av[0] A joint angle, av[1] B side (P3 / PC / P6) or HEAD timer, av[2] R rattle, kx / ky X / Y flight, c Z bounces;
 * fl 4 flag 2 (rattle phase), 8 flag 3 (attached), 0x40 flag 6 (flying), HEAD fl 1 = flag 0 (struck). */
static struct { int phase, voice, pillarD, front, back; obj_t *core, *spine, *n[4], *head, *p3[2], *pc[2], *p6[2]; } BG;
int relic_is_gate_pillar(const obj_t *o) { (void)o; return 0; }   /* GimmickMovingPillar1 / 3 (acts 3-5 gimmick types: wired at the merge) */
static int bg_part(int t) { return t >= OT_BGSPINE && t <= OT_BGHEAD; }
static void bg_pillars(int dy) {                                                   /* GimmickMovingPillar1 / 3 at the room's left exit (relic_gimb.c types) */
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && relic_is_gate_pillar(o) && fabsf(o->x - BG.core->x) < 600 && fabsf(o->y - BG.core->y) < 200) o->y += (float)dy; }
}
static void bg_spawn(void) {
    memset(&BG, 0, sizeof BG);
    BG.core = first_of(OT_BGCORE); BG.spine = first_of(OT_BGSPINE); BG.head = first_of(OT_BGHEAD);
    if (!BG.core || !BG.spine) { BG.core = NULL; return; }
    static const int NT[4] = { OT_BGN1, OT_BGN2, OT_BGN3, OT_BGN4 };
    for (int k = 0; k < 4; k++) BG.n[k] = first_of(NT[k]);
    for (int i = G.nobjs - 1, a = 0, b = 0, c = 0; i >= 0; i--) {                  /* 7143: B = 0, 1 in selection order (last created -> 0) */
        obj_t *o = &G.o[i]; if (!o->alive) continue;
        if (o->type == OT_BGP3 && a < 2) { o->av[1] = a; BG.p3[a++] = o; }
        if (o->type == OT_BGPC && b < 2) { o->av[1] = b; BG.pc[b++] = o; }
        if (o->type == OT_BGP6 && c < 2) { o->av[1] = c; BG.p6[c++] = o; }
    }
    obj_t *s = BG.spine, *g = BG.core;
    g->A = 70; g->bd = 0; g->st = 0; g->fl |= F_ARMOURED | F_NOFLY; set_ens(g, "bgcore");   /* ctor A 70; 7143 flag 5 */
    memset(s->av, 0, sizeof s->av); s->av[0] = 1; s->av[4] = 256; s->av[5] = 800; s->av[6] = 1;   /* ctor: A 1, E 256, F 800, G 1 */
    s->y += 48; g->x = s->x; g->y = s->y - 48 + 12; s->fl |= 1;                   /* 7143: BG = (P5.x, P5.y + 12), P5 y += 48, flag 0 */
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && bg_part(o->type) && o != s) { o->av[0] = 0; o->av[2] = 0; o->fl &= ~0x4c; } }
}
static void bg_dirsprite(obj_t *o, const char *base) {
    char nm[16]; snprintf(nm, sizeof nm, "%s%s", base, o->dir == 16 ? "_l" : o->dir == 24 ? "_24" : "");
    int a = ens_anim(nm); if (a < 0) { snprintf(nm, sizeof nm, "%s", base); a = ens_anim(nm); }
    if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a);
}
static int bg_on_back(const player_t *p) { int k = mv_sensor_obj(p->x, PY(p), p->N, SENSOR_BOTTOMBIG, OT_BGSPINE); return k >= 0 && p->ground; }
/* HazardShockwave_1576: A = 10 damage, B age; av[2] its BallMovement speed in px / frame (0: the golem's 50 = 6.25;
 * the Mantalith sets 20 / 40 / 60 = 2.5 / 5 / 7.5, rm_mantalith.c, which also ages its own waves) */
obj_t *relic_shock_new(double x, double y, int dir, double speed) {
    obj_t *s = spawn(OT_SHOCK, (dir & 31) == 16 ? "shock_l" : "shock", (float)x, (float)y);
    if (s) { s->dir = dir & 31; s->av[0] = 10; s->av[1] = 0; s->av[2] = speed; }
    return s;
}
static void shock_new(double x, double y, int dir) { relic_shock_new(x, y, dir, 0); }
static void bg_frame(player_t *p) {
    obj_t *g = BG.core, *s = BG.spine;
    if (!g || !s) return;
    double PXv = PX(p), PYv = PY(p);
    if (BG.phase == 0) {                                                          /* 6777: wake (in the room past the gem, or the gem hit) */
        int room = PYv >= 1097 && PYv <= 1607 && PXv < 2000 && PXv > g->x && first_of(OT_WR_RAIL);
        if (room || g->c > 0) {
            BG.phase = 1;
            for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && bg_part(o->type)) o->av[2] = R(5); }
            bg_pillars(-88); snd_play_orig(57, 220, 128);
        }
        return;
    }
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && bg_part(o->type)) o->inv = g->c > 0 && !(g->fl & F_ARMOURED) && BG.phase < 3; }   /* 6778 / 6779 */
    if (BG.phase == 1) {                                                          /* assembly (0x22513, 6780-6807) */
        for (int i = 0; i < G.nobjs; i++) {                                       /* 6780 / 6781: loose bones rattle */
            obj_t *o = &G.o[i]; if (!o->alive || !bg_part(o->type) || (o->fl & 8)) continue;
            o->av[2] += 1 + R(3);
            if (o->av[2] > 5) { o->y += (o->fl & 4) ? 1 : -1; o->fl ^= 4; o->av[2] = 0; }
        }
        double D = s->av[3];
        if (D < 30) s->av[3] = ++D;                                               /* 6782 */
        if (D > 0 && D <= 30) { s->y = (float)fmax(g->y - 12, s->y - 2); s->fl |= 8; }   /* 6783 */
        if (D == 30 && s->y < g->y - 11) { s->fl &= ~1; s->av[3] = D = 31; }    /* 6784 */
        double ax, ay;
        obj_t *o = NULL; double tx = 0, ty = 0, sx = 0, sy = 0; int maxx = 0;      /* step D: part o moves toward (tx, ty) by (sx, sy) */
        switch ((int)D) {
        case 31: o = BG.p3[0]; tx = s->x; ty = s->y + 3; sx = 10; sy = 6; break;
        case 32: o = BG.pc[0]; if (BG.p3[0]) { act_rot(BG.p3[0], &ax, &ay); tx = ax; ty = ay; } sx = 10; sy = 4; break;
        case 33: o = BG.p6[0]; if (BG.pc[0]) { act_rot(BG.pc[0], &ax, &ay); tx = ax; ty = ay; } sx = 10; sy = 2; break;
        case 34: o = BG.p3[1]; tx = s->x + 24; ty = s->y + 3; sx = -8; sy = 5; break;
        case 35: o = BG.pc[1]; if (BG.p3[1]) { act_rot(BG.p3[1], &ax, &ay); tx = ax; ty = ay; } sx = 10; sy = 4; break;
        case 36: o = BG.p6[1]; if (BG.pc[1]) { act_rot(BG.pc[1], &ax, &ay); tx = ax; ty = ay; } sx = -10; sy = 2; break;
        case 37: o = BG.n[0]; act_rot(s, &ax, &ay); tx = ax; ty = ay; sx = -8; sy = 8; break;
        case 38: o = BG.n[1]; if (BG.n[0]) { act_rot(BG.n[0], &ax, &ay); tx = ax; ty = ay; } sx = -8; sy = 8; break;
        case 39: o = BG.n[2]; if (BG.n[1]) { act_rot(BG.n[1], &ax, &ay); tx = ax; ty = ay; } sx = -9; sy = 9; break;
        case 40: o = BG.n[3]; if (BG.n[2]) { act_rot(BG.n[2], &ax, &ay); tx = ax; ty = ay; } sx = -10; sy = 10; break;
        case 41: o = BG.head; if (BG.n[3]) { act_rot(BG.n[3], &ax, &ay); tx = ax; ty = ay; } sx = -10; sy = 10; break;
        case 42: BG.phase = 2; g->fl &= ~F_ARMOURED; break;                       /* 6807 */
        }
        (void)maxx;
        if (o && D >= 31 && D <= 41) {
            o->x = (float)(sx > 0 ? fmin(tx, o->x + sx) : fmax(tx, o->x + sx));
            o->y = (float)(o->y > ty ? fmax(ty, o->y - sy) : fmin(ty, o->y + sy));
            int there = (sx > 0 ? o->x >= (float)tx : o->x <= (float)tx) && fabsf(o->y - (float)ty) < 1;   /* compared at float precision */
#ifdef FP_HOST_ENV
            if (getenv("FP_DBG_BG")) printf("bg D %d x %.2f tx %.2f y %.2f ty %.2f there %d\n", (int)D, o->x, tx, o->y, ty, there);
#endif
            if (there) {
                o->fl |= 8; snd_play_orig(30, 220, 128); s->av[3] += 1;
                int side = (int)(BG.p3[0] ? BG.p3[0]->av[1] : 0);
                double f = 2 * side - 1;
                if (o->type == OT_BGP3) { o->av[0] = (2 * o->av[1] - 1) * 20 * c_sin(s->av[1]) - 45; o->ang = (float)(o->av[0] + 45); }
                else if (o->type == OT_BGPC) { o->av[0] = (2 * o->av[1] - 1) * 30 * c_cos(s->av[1]) + 25; o->ang = (float)(o->av[0] - 45); }
                else if (o == BG.n[0]) { o->av[0] = f * 10 * c_sin(s->av[1]) - 60; o->ang = (float)(o->av[0] + 90); }
                else if (o == BG.n[1]) { o->av[0] = f * 10 * c_sin(s->av[1] - 20) - 30; o->ang = (float)(o->av[0] + 90); }
                else if (o == BG.n[2]) { o->av[0] = f * 10 * c_sin(s->av[1] - 40) + 10; o->ang = (float)(o->av[0] + 90); }
                else if (o == BG.n[3]) { o->av[0] = f * 10 * c_sin(s->av[1] - 60) - 10; o->ang = (float)(o->av[0] + 90); }
            }
        } else if (D >= 31 && D <= 41 && !o) s->av[3] += 1;                        /* a missing part: skip its step */
        return;
    }
    if (BG.phase == 2) {                                                          /* fight (0x22518, 6808-6849) */
        double *A = &s->av[0], *B = &s->av[1], *C = &s->av[2], *Gm = &s->av[6], *H = &s->av[7];
        if (p->ground) s->fl &= ~1;                                               /* 6808 */
        if (*C == 0 && g->c <= 0) {                                               /* 6809-6817: walk, carry, turn */
            int on = bg_on_back(p);
            if (*Gm == 1) { *A = 1; *B += 8; s->x += 3; if (on) p->x += 3; }
            if (*Gm == -1) { *A = -1; *B -= 8; s->x -= 3; if (on) p->x -= 3; }
            if (*Gm == 1 && s->x > s->av[5]) *Gm = -2;
            if (*Gm == -1 && s->x < s->av[4]) *Gm = 2;
            if (*Gm == -2) *A = fmax(*A - 0.1, -1);
            if (*Gm == 2) *A = fmin(*A + 0.1, 1);
            if ((*Gm == -2 && *A <= -1) || (*Gm == 2 && *A >= 1)) { *A = *Gm > 0 ? 1 : -1; *Gm = *A; }
        }
        if (*C == 0 && fabs(*Gm) < 2) *H += 1;                                    /* 6818 */
        if (*H > 60 && fabs(*Gm) < 2) { s->fl |= 0x80; *H = -60; }                /* 6819 */
        double ax, ay;                                                            /* 6820: the chain, from last frame's angles */
        act_rot(s, &ax, &ay); if (BG.n[0]) { BG.n[0]->x = (float)ax; BG.n[0]->y = (float)ay; }
        for (int k = 1; k < 4; k++) if (BG.n[k] && BG.n[k - 1]) { act_rot(BG.n[k - 1], &ax, &ay); BG.n[k]->x = (float)ax; BG.n[k]->y = (float)ay; }
        if (BG.head && BG.n[3]) { act_rot(BG.n[3], &ax, &ay); BG.head->x = (float)ax; BG.head->y = (float)ay; }
        for (int k = 0; k < 2; k++) {
            obj_t *p3 = BG.p3[k], *pc = BG.pc[k], *p6 = BG.p6[k];
            if (p3) { p3->y = s->y + 3; p3->x = (float)(s->x + 24 * p3->av[1] * *A); }
            if (pc && p3) { act_rot(p3, &ax, &ay); pc->x = (float)ax; pc->y = (float)ay; }
            if (p6 && pc) { act_rot(pc, &ax, &ay); p6->x = (float)ax; p6->y = (float)fmin(ay, s->y + 50); }
        }
        g->x = s->x; g->y = s->y + 12;
        double f = *A >= 0 ? 1 : -1;                                              /* 6823 / 6824 */
        static const double NOFF[4] = { 0, 20, 40, 60 }, NBASE[4] = { -60, -30, 10, -10 };
        for (int k = 0; k < 4; k++) if (BG.n[k]) { double v = -10 * c_sin(*B - NOFF[k]) + NBASE[k]; BG.n[k]->av[0] = f > 0 ? v : -v; BG.n[k]->dir = 0; }
        for (int k = 0; k < 2; k++) {
            double side = 2 * (BG.p3[k] ? BG.p3[k]->av[1] : k) - 1;
            if (BG.p3[k]) BG.p3[k]->av[0] = side * 20 * c_sin(*B) + (f > 0 ? -45 : 45);
            if (BG.pc[k]) { BG.pc[k]->av[0] = f > 0 ? side * 30 * c_cos(*B) + 25 : -35 - side * 30 * c_cos(*B); BG.pc[k]->dir = f > 0 ? 0 : 16; }
            if (BG.p6[k]) BG.p6[k]->dir = f > 0 ? 0 : 16;
        }
        s->dir = f > 0 ? 0 : 16;
        if (BG.head) BG.head->dir = f > 0 ? 16 * ((s->fl & 2) ? 1 : 0) : (16 * (((s->fl & 2) ? 1 : 0) + 1)) & 31;
        if (*A > -1 && *A < 1) {                                                  /* 6825: the 20-frame turn blend */
            double a = fabs(*A);
            for (int k = 0; k < 4; k++) if (BG.n[k]) { BG.n[k]->x = (float)(((1 - a) * s->x + (1 + a) * BG.n[k]->x) / 2); BG.n[k]->av[0] *= a; }
            if (BG.head) BG.head->x = (float)(((1 - a) * s->x + (1 + a) * BG.head->x) / 2);
            for (int k = 0; k < 2; k++) {
                obj_t *L[3] = { BG.pc[k], BG.p3[k], BG.p6[k] };
                for (int j = 0; j < 3; j++) if (L[j]) { L[j]->x = (float)(((1 - a) * (s->x - 16 + 32 * L[j]->av[1]) + (1 + a) * L[j]->x) / 2); if (j < 2) L[j]->av[0] *= a; }
            }
        }
        if (*C == 0) { s->fl &= ~2; if (bg_on_back(p)) s->fl |= 2; }              /* 6826 / 6827 */
        if ((s->fl & 0x80) && *C == 0) { *C = 1; if (s->fl & 2) BG.back = 1; else BG.front = 1; s->fl &= ~0x80; }   /* 6828 / 6829 */
        if (*C > 0) *C = fmod(*C + 1, 80);                                        /* 6830 */
        obj_t *hd = BG.head;
        double Cv = *C, sgn = *A >= 0 ? 1 : -1;
        if (BG.front && hd) {                                                     /* 6831-6838 */
            if (Cv > 0 && Cv < 30) { for (int k = 0; k < 4; k++) if (BG.n[k]) BG.n[k]->av[0] += fmin(1.5 * Cv, 30) * sgn; hd->av[0] = fmax(-20, fmin(20, Cv * sgn)); }
            else if (Cv >= 30 && Cv < 50) {
                for (int k = 0; k < 4; k++) if (BG.n[k]) BG.n[k]->av[0] = sgn > 0 ? fmax(BG.n[k]->av[0] + 330 - 10 * Cv, -125) : fmin(BG.n[k]->av[0] - 330 + 10 * Cv, 125);
                hd->av[0] = sgn > 0 ? fmax(hd->av[0] + 170 - 5 * Cv, -30) : fmin(hd->av[0] - 170 + 5 * Cv, 30);
                if (BG.n[3] && ((sgn > 0 && BG.n[3]->av[0] <= -125) || (sgn < 0 && BG.n[3]->av[0] >= 125)) && !(hd->fl & 1)) {   /* 6834: one shockwave per strike */
                    hd->fl |= 1; shock_new(hd->x, s->y + 56, hd->dir + 16); shake(6); snd_play_orig(17, 220, 128);
                }
            } else if (Cv >= 50) {
                for (int k = 0; k < 4; k++) if (BG.n[k]) BG.n[k]->av[0] = sgn > 0 ? fmin(BG.n[k]->av[0], fmax(5 * Cv - 405, -125)) : fmax(BG.n[k]->av[0], fmin(405 - 5 * Cv, 125));
                hd->av[0] = sgn > 0 ? fmin(hd->av[0] + 1, 0) : fmax(hd->av[0] - 1, 0);
            }
            if (Cv == 0) { hd->av[0] = 0; BG.front = 0; hd->fl &= ~1; }
        }
        if (BG.back && hd) {                                                      /* 6839-6847 */
            static const double LIM[4] = { 20, 45, 60, 120 };
            if (Cv > 0 && Cv < 30) { for (int k = 0; k < 4; k++) if (BG.n[k]) BG.n[k]->av[0] -= fmin(1.5 * Cv, 30) * sgn; hd->av[0] = fmax(-20, fmin(20, -Cv * sgn)); }
            else if (Cv >= 30 && Cv < 50) {
                hd->av[0] = sgn > 0 ? fmax(170 - 5 * Cv, -40) : fmin(5 * Cv - 170, 40);
                for (int k = 0; k < 4; k++) if (BG.n[k]) {
                    double base = k == 3 ? 390 - 12 * Cv : 330 - 10 * Cv, a = BG.n[k]->av[0];
                    BG.n[k]->av[0] = sgn > 0 ? fmax(a + base, a - LIM[k]) : fmin(a - base, a + LIM[k]);
                }
            } else if (Cv >= 50) {                                                /* 6845 / 6846 (recover; constants partly read, INFERRED) */
                for (int k = 0; k < 4; k++) if (BG.n[k]) BG.n[k]->av[0] = sgn > 0 ? fmin(BG.n[k]->av[0], fmax(5 * Cv - 405, -125)) : fmax(BG.n[k]->av[0], fmin(405 - 5 * Cv, 125));
                hd->av[0] = sgn > 0 ? fmin(hd->av[0] + 1, 0) : fmax(hd->av[0] - 1, 0);
            }
            if (Cv == 40 && !(hd->fl & 1)) {                                       /* 6842: it strikes its own back */
                hd->fl |= 1; shock_new(hd->x, s->y - 4, hd->dir + 16); snd_play_orig(17, 220, 128); shake(6);
                g->kx = 0; g->c = 10; g->ky = 10; g->A -= 25; g->inv = 1; g->st += 1; s->fl |= 1; hd->av[1] = 10;
                if (g->st == 2 && G.character == CHR_LILAC && !BG.voice) { BG.voice = 1; snd_play_orig(131, 220, 128); }   /* 6844 (Lilac) */
            }
            if (Cv == 0) { hd->av[0] = 0; BG.back = 0; hd->fl &= ~1; }
        }
        for (int k = 0; k < 4; k++) if (BG.n[k]) { BG.n[k]->ang = (float)(BG.n[k]->av[0] + 90); bg_dirsprite(BG.n[k], k == 3 ? "bgn4" : k == 2 ? "bgn3" : k == 1 ? "bgn2" : "bgn1"); }   /* 6848 */
        for (int k = 0; k < 2; k++) {
            if (BG.p3[k]) { BG.p3[k]->ang = (float)(BG.p3[k]->av[0] + 45); bg_dirsprite(BG.p3[k], "bgp3"); }
            if (BG.pc[k]) { BG.pc[k]->ang = (float)(BG.pc[k]->av[0] - 45); bg_dirsprite(BG.pc[k], "bgpc"); }
            if (BG.p6[k]) bg_dirsprite(BG.p6[k], "bgp6");
        }
        if (hd) { hd->ang = (float)hd->av[0]; bg_dirsprite(hd, "bghead"); if (hd->av[1] > 0) hd->av[1]--; }   /* 7116 */
        bg_dirsprite(s, "bgp5");
        if (DEFEATED(g)) {                                                        /* 6849: the skeleton flies apart */
            s->fl |= 1; s->kx = g->kx / 2; g->fl &= ~F_NOFLY;                  /* the gem flies (2765) */
            double X = g->kx;
            for (int k = 0; k < 4; k++) if (BG.n[k]) { X = (k + 2) * sgn + X; BG.n[k]->kx = X; BG.n[k]->ky = -(k + 2); }
            if (hd) { hd->kx = g->kx; hd->ky = 0; }
            for (int k = 0; k < 2; k++) {
                if (BG.p6[k]) { BG.p6[k]->ky = -2; BG.p6[k]->kx = g->kx / 2; }
                if (BG.pc[k]) { BG.pc[k]->ky = -4; BG.pc[k]->kx = g->kx - (2 * BG.pc[k]->av[1] + 3) * sgn; }
            }
            snd_play_orig(132, 220, 128);
            for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && bg_part(o->type)) { o->fl |= 0x40; o->c = 0; o->inv = 0; } }
            g->c = 0; g->st = 0; BG.phase = 3; BG.pillarD = 0;
        }
        return;
    }
    if (BG.phase == 3) {                                                          /* beaten (0x22516, 6850-6859) */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i]; if (!o->alive || !bg_part(o->type) || !(o->fl & 0x40)) continue;
            o->x = (float)(int)(o->x + o->kx); o->y = (float)(int)(o->y + o->ky); o->ky = fmin(o->ky + 0.25, 15); o->ang += (float)fmax(8, 2 * o->kx);   /* 6850 */
            if (o->y >= 1449 && o->ky > 0) { o->ky *= -0.7; o->c++; }             /* 6852: floor bounce (RM5 room floor) */
            if (o->x <= 183 && o->kx < 0) o->kx = -o->kx;                         /* 6853 */
            if (o->x >= 873 && o->kx > 0) o->kx = -o->kx;                         /* 6854 */
            if (o->kx > 0.05) o->kx -= 0.02; else if (o->kx < -0.05) o->kx += 0.02;   /* 6855 / 6856 */
            if (o->c >= 2) o->fl &= ~0x40;                                        /* 6857: rests as scenery */
        }
        if (BG.pillarD < 88) { bg_pillars(2); BG.pillarD += 2; }                  /* 6858 */
        else if (BG.pillarD == 88) { BG.pillarD = 90; rails_to_crystals(); BG.phase = 4; }   /* 6859 (records x2 y35: not ported) */
    }
}
static void shocks(player_t *p) {                                                  /* 7113 / 7114: BallMovement 50 (6.25 px / frame) */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *s = &G.o[i]; if (!s->alive || s->type != OT_SHOCK) continue;
        { float v = s->av[2] > 0 ? (float)s->av[2] : 6.25f; if (s->born != G.frame) s->x += s->dir == 16 ? -v : v; }
        if (s->av[1] <= 50 && csens_hit(p, s) && vulnerable(p)) { player_contact(p, (int)s->av[0]); if (BG.core) BG.core->st = 0; }   /* 7114 */
        if (s->av[1] > 50 || s->x < -64 || s->x > G.lvl.w + 64) s->alive = 0;     /* 7113 (the off-level removal: port safety) */
    }
}

/* ======================================================================= hooks */
int relic_boss_keeps(int t) { return t == OT_GEYE || t == OT_BGCORE; }
int relic_boss_clear_type(int t) { return t == OT_GEYE || t == OT_GLID || t == OT_GP1 || t == OT_GP2; }   /* 6039 */
void rmboss_spawn(void) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_GEYE || o->type > OT_CAMLIM_B) continue;
        if (o->x < 0 || o->x >= G.lvl.w || o->y < 0 || o->y >= G.lvl.h) { if (o->type < OT_CAMLIM_L) o->alive = 0; continue; }
        if (o->type >= OT_CAMLIM_L) { o->an.b = NULL; continue; }                  /* invisible markers */
        if (o->type == OT_BGCORE) continue;
        const char *nm = o->type == OT_BGSPINE ? "bgp5" : o->type == OT_BGP3 ? "bgp3" : o->type == OT_BGPC ? "bgpc" : o->type == OT_BGP6 ? "bgp6" :
                         o->type == OT_BGN1 ? "bgn1" : o->type == OT_BGN2 ? "bgn2" : o->type == OT_BGN3 ? "bgn3" : o->type == OT_BGN4 ? "bgn4" : o->type == OT_BGHEAD ? "bghead" : NULL;
        if (nm) set_ens(o, nm);
    }
    ge_spawn(); bg_spawn();
}
void rmboss_frame(player_t *p) {
    if (!objects_have(OT_GEYE) && !objects_have(OT_BGCORE) && !first_of(OT_GSHOT) && !first_of(OT_SHOCK)) return;
    ge_frame(p);
    ge_shots(p);
    bg_frame(p);
    shocks(p);
}
int rmboss_obj(obj_t *o, player_t *p) { (void)p; return o->type >= OT_GEYE && o->type <= OT_CAMLIM_B; }
