/* Aqua Tunnel gimmicks B (session 12): platforms, switches, crushers, pipes, beams, axels, swings, slime floor, rafts.
 * Source: build/re/spec_at_gimmicks_b.md (static RE of the Linux build; section numbers below refer to it) and the
 * generic platform events of build/re/spec_platforms.md (path 3283-3289, carry 4061, dip 3270-3272 / 3303-3307).
 * Everything here runs in objects_platforms(), before Lilac's movement, in the act's event order (spec 0.2).
 * Labels: RE (read from code), RV (original trace), INFERRED (reasoned). */
#include "../fp.h"
#include <math.h>
#include <string.h>

extern int aqua_gt_off;
static int GT(void) { return G.frame + aqua_gt_off; }
static const input_t *cur_in;
void aqua_set_input(const input_t *in) { cur_in = in; }

static int wplate_latch, pipe_ch = -1;
static double pgaF;
static void set_ens(obj_t *o, const char *name) {
    int a = ens_anim(name);
    if (a >= 0) anim_set(&o->an, &G.ens, a); else o->an.b = NULL;
}
/* Chowdren's degree trig: cos is exactly 0 at 90 / 270, sin exactly 0 at 180 (spec 8.1) */
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / 57.29577951308232); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / 57.29577951308232); }

static int PY_(const player_t *p) { return (int)floor(p->y - ORIG_REF_DY); }   /* PLAYER.y (Player_326 at int Y) */
static int is_box(int t) { return (objdef(t)->flags & OD_BOXCOL) != 0; }   /* registry (objdefs.c) */
/* a placed player sensor (mask rows) overlapping the object: box objects use the image rect, others the (rotated) mask */
static int sens_hit(int s, const obj_t *o) {
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    if (is_box(o->type)) {
        const frame_t *f = anim_frame(&o->an);
        int bx = (int)floorf(o->x) - f->hx, by = (int)floorf(o->y) - f->hy;
        for (int j = 0; j < h; j++) { int y = oy + j; if (b[j] < a[j] || y < by || y >= by + f->h) continue; if (ox + b[j] >= bx && ox + a[j] < bx + f->w) return 1; }
        return 0;
    }
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int sens_bg(int s) {                         /* the sensor mask on solid background (Frame overlaps_background) */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) for (int x = ox + a[j]; x <= ox + b[j]; x++) if (px_solid(&G.lvl, x, oy + j)) return 1;
    return 0;
}
static void resense(player_t *p) { player_sync_sensors(p); }   /* loop_player01allsensors */
static int orig_M(const player_t *p) { return player_orig_m(p); }
static double pB(const player_t *p) { return p->ground ? p->gsp : p->vx; }

/* ================================================================ spawn */
static void path_init(obj_t *o, double B, double C, double F, double Gm, double E0) {
    o->ox = o->x; o->oy = o->y;                     /* 3565: I = X, J = Y */
    o->av[0] = 0; o->av[1] = E0; o->av[3] = B; o->av[4] = C; o->av[5] = F; o->av[6] = Gm;
    o->fx = 0; o->fy = 0; o->vy = 0; o->dir = 0; o->av[2] = 0; o->fl = 0;   /* N, M = 0 before the first update (K = L = 0) */
}
static void swing_spawn(void);
void aqua_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&wplate_latch, sizeof wplate_latch); snap_register(&pgaF, sizeof pgaF); }
    wplate_latch = 0; pgaF = 0;
    if (pipe_ch >= 0) { snd_loop_stop(pipe_ch); pipe_ch = -1; }
    int npipe = 0, ncog = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_ABPLAT: set_ens(o, "abplat"); break;                                      /* 731 (AT1) */
        case OT_ABPLAT2: set_ens(o, "abplat2"); break;                                    /* 803 (AT2, AT4) */
        case OT_COLLAPSE: set_ens(o, "collapse"); o->av[0] = o->av[1] = 0; o->st = 0; o->av[3] = o->av[4] = 0; o->ang = 0; break;
        case OT_WPLATE_AT: set_ens(o, "wallplate"); o->fl = 0; o->hidden = 0; break;
        case OT_CRUSH_H:                                                                     /* LevelCrusher1_847: horizontal slab */
            o->layer = 2; set_ens(o, "crushh");
            o->av[0] = o->x; o->av[1] = -128; o->av[2] = 128; o->av[3] = 40; o->av[4] = 1; break;
        case OT_CRUSH_V:
            set_ens(o, "crushv");                                                            /* 4374 / 4375: layer-2 copy drawn, layer-3 copy collides */
            if (o->layer < 2) o->type = OT_CRUSH_VIS; else o->hidden = 1;
            path_init(o, 0, 0, 1, 96, 0); break;
        case OT_CRUSH_H2: o->layer = 2; set_ens(o, "crushh2"); o->av[0] = o->x; o->av[1] = -32; o->av[2] = 128; o->av[3] = 40; o->av[4] = 1; break;
        case OT_PIPE: set_ens(o, "pipe"); o->av[0] = 0; o->av[1] = 0; o->st = 0; o->dir = 1; o->ang = 0; o->par = npipe++; break;
        case OT_PIPEHUB: set_ens(o, "pipehub"); break;
        case OT_PIPECOG: set_ens(o, "pipecog"); o->par = ncog++; o->ang = 0; break;      /* paired with the pipe of the same index (INFERRED, U2) */
        case OT_BEAM: set_ens(o, "beam"); o->ang = 0; break;
        case OT_AXEL1: case OT_AXEL2: o->alive = 0; break;                                  /* 680 / 737 come from the makers only */
        case OT_AXEL1P: case OT_AXEL2P:                                                      /* AT3 845 / 846: bobbing (F 2, G 80; 846 E 180) */
            set_ens(o, o->type == OT_AXEL1P ? "axel1b" : "axel2b"); o->c = 10;
            path_init(o, 0, 0, 2, 80, o->type == OT_AXEL2P ? 180 : 0); break;
        case OT_AXMAKER1: case OT_AXMAKER2: o->an.b = NULL; break;                          /* invisible */
        case OT_SCROLLBRICK: set_ens(o, "scrollbrick"); o->av[0] = o->x; o->av[1] = 0; break;   /* 4690: A = x (scrolls in the boss phase only) */
        case OT_SLIMEFLOOR:
            set_ens(o, "slimefloor"); o->av[0] = o->x; o->av[2] = 0;                       /* A = x, B = layer + 1 */
            if (o->layer < 2) o->type = OT_SLIMEF_VIS; else o->hidden = 1;                  /* 4553 */
            break;
        case OT_RAFT: set_ens(o, "raft"); o->st = 0; o->av[1] = 264; break;                  /* ctor B = 264 */
        case OT_TREEOVL: set_ens(o, "treeovl"); break;
        case OT_FISHSTATUE: set_ens(o, "fishstatue"); o->layer = 3; break;                   /* 4361: re-linked in front (INFERRED: over Lilac) */
        }
    }
    swing_spawn();
}

/* ================================================================ generic path / dip / carry (Q12, Q13) */
static int q13(int t) { return t == OT_CRUSH_V || t == OT_CRUSH_VIS || t == OT_AXEL1 || t == OT_AXEL2 || t == OT_AXEL1P || t == OT_AXEL2P; }
static void path_step(void) {                       /* 3270-3272, 3283-3289, 2989 */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !q13(o->type)) continue;
        o->y -= floorf(o->vy);                                            /* 3270 */
        if (!(o->fl & 4)) o->vy -= o->vy * 0.2f;                          /* 3271 */
        o->fl &= ~4;                                                      /* 3272 */
        double K = o->fy, L = o->fx;                                      /* 3283 */
        o->av[0] = fmod(o->av[0] + o->av[3], 360.); o->av[1] = fmod(o->av[1] + o->av[5], 360.);
        double M = c_cos(o->av[0]) * o->av[4] + o->ox;                    /* 3284 (D = 0) */
        double N = c_cos(o->av[1]) * o->av[6] + o->oy;                    /* 3286 (H = 0) */
        o->av[2] = floor(M) - floor(K); o->dir = (int)(floor(N) - floor(L));   /* 3288: O, P */
        o->fy = (float)M; o->fx = (float)N;
        o->x = (float)floor(M); o->y = (float)floor(N);                  /* 2989 */
    }
}
static void carry(player_t *p) {                    /* 4061: G >= 0, S_BB 1 px lower on a q13 platform with flag 6 off */
    if (!(p->vy >= 0 || p->ground)) return;
    double MY = p->y - ORIG_REF_DY;
    static const int Q13[] = { OT_CRUSH_V, OT_AXEL1, OT_AXEL1P, OT_AXEL2, OT_AXEL2P };   /* q13 order: Crusher1, Axel1, Axel2 */
    for (unsigned q = 0; q < sizeof Q13 / sizeof Q13[0]; q++) {
        int t = Q13[q];
        int k = mv_sensor_obj(p->x, MY + 1, p->N, SENSOR_BOTTOMBIG, t);
        if (k < 0 || (G.o[k].fl & 0x40)) continue;
        p->x += G.o[k].av[2]; p->y += G.o[k].dir;
        if (!p->ground) { p->gsp = p->vx; p->ground = 1; }               /* L = 1 (B kept) */
        p->vy = 0; p->N = 0; p->O = 0;
        resense(p);
        break;
    }
}
static void dip(player_t *p) {                      /* 3303-3307 */
    double MY = p->y - ORIG_REF_DY;
    int first = -1;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !q13(o->type) || o->type == OT_CRUSH_VIS) continue;
        if (p->ground && mv_sensor_obj(p->x, MY, p->N, SENSOR_BOTTOMBIG, o->type) == i) { o->fl |= 4; if (first < 0) first = i; }
    }
    if (first >= 0 && G.o[first].fx > 0)                                 /* 3306: linked platforms (same L) share the dip */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (o->alive && q13(o->type) && i != first && o->fx == G.o[first].fx) { o->fl |= 4; o->vy = G.o[first].vy; }
        }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !q13(o->type)) continue;
        if (o->fl & 4) o->vy += (9 - o->vy) * 0.2f;                       /* 3304 */
        o->y += floorf(o->vy);                                            /* 3307 */
    }
}

/* ================================================================ 2. collapsing platform (4078-4083) */
static void collapse_frame(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_COLLAPSE || !o->an.b) continue;
        int on = sens_hit(SENS_BOTTOMBIG, o), sx, sy; mv_sensor_pos(SENS_BOTTOMBIG, &sx, &sy);
        if (on && o->st == 0 && sx > o->x + 50) { o->st = 1; obj_effect(o->x, o->y - 48, "explosion01"); }   /* 4078 (L not tested) */
        if (o->av[0] < -2 && !o->av[3]) { o->av[3] = 1; obj_effect(o->x, o->y - 64, "explosion01"); }        /* 4079 once */
        if (o->av[0] < -5 && !o->av[4]) { o->av[4] = 1; obj_effect(o->x, o->y - 16, "explosion01"); }        /* 4080 once */
        if (on && o->st == 1) resense(p);                                                                   /* 4081 */
        if (o->st == 1) {                                                                                  /* 4082 */
            o->av[1] = fmin(o->av[1] + 0.02, 12); o->av[0] -= o->av[1]; if (o->av[0] < -40) o->av[0] = -40;
            o->ang = (float)o->av[0];
        }
        if (o->st == 1 && o->av[0] <= -40) { o->st = 2; if (G.cam.Z < 5) G.cam.Z = 5; snd_play_orig(51, 220, 128); }   /* 4083 */
    }
}

/* 3.1 / 3.2 plates, the Milla block and the doors: src/game/switches.c (session 13) */

/* ================================================================ 3.3 wall plates + 6 pipes (4365-4371, 4507-4510) */
static void wallplates_pipes(player_t *p) {
    int pressed = 0, M = orig_M(p), auto_y = level_cur()->pipe_auto_y;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_WPLATE_AT) continue;
        o->hidden = 0; o->fl &= ~1;                                                                  /* 4365 */
        if (sens_hit(SENS_RIGHTBIG, o) && (M == 11 || M == 47)) { o->hidden = 1; o->fl |= 1; }      /* 4366 */
        if (o->fl & 1) pressed = 1;
    }
    if (pressed && !wplate_latch) snd_play_orig(49, 220, 128);                                    /* 4367 once */
    wplate_latch = pressed;
    int started = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_PIPE) continue;
        int go = o->st == 0 && (pressed || (auto_y && p->y - ORIG_REF_DY > auto_y && o->av[0] <= -90));   /* 4368 / 4507 (AT3: level data pipe_auto_y 2900) */
        if (go) { o->st = 1; o->dir = -o->dir; started = 1; }
    }
    if (started) { if (pipe_ch >= 0) snd_loop_stop(pipe_ch); pipe_ch = snd_loop_orig(62, 200, 0); }   /* sound 62 (looping, INFERRED) */
    int stopped = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_PIPE) continue;
        if (o->st == 1) { o->av[1] = fmin(o->av[1] + 0.03, 5); o->av[0] += o->av[1] * o->dir; o->ang = (float)o->av[0]; }   /* 4369 */
        if (o->st == 1 && ((o->dir == -1 && o->av[0] <= -90) || (o->dir == 1 && o->av[0] >= 0))) {                       /* 4370 / 4371 */
            o->av[0] = o->dir == -1 ? -90 : 0; o->ang = (float)o->av[0]; o->st = 0; o->av[1] = 0; stopped = 1;
        }
    }
    if (stopped) { snd_play_orig(51, 220, 128); if (pipe_ch >= 0) { snd_loop_stop(pipe_ch); pipe_ch = -1; } }
    for (int i = 0; i < G.nobjs; i++) {                                                             /* cogs follow their pipe */
        obj_t *c = &G.o[i]; if (!c->alive || c->type != OT_PIPECOG) continue;
        for (int k = 0; k < G.nobjs; k++) if (G.o[k].alive && G.o[k].type == OT_PIPE && G.o[k].par == c->par) { c->ang = G.o[k].ang; break; }
    }
}

/* ================================================================ 7. spinning beam (4372, 4373) */
static void beams(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_BEAM) continue;
        o->ang = (float)fmod(o->ang + 1.0, 360.0);                                                    /* 4372 */
        if (p->state != ST_DEAD && sens_hit(SENS_BOTTOMBIG, o) && mv_z(SENSOR_TOPBIG)) {             /* 4373: crushed against a ceiling */
            p->hp = 0; p->state = ST_DEAD; p->dead_t = 0; p->vy = -5; p->vx = 0; p->ground = 0; p->invinc = 0; p->invuln = 0;   /* M9; E = 0 (GUIRings = 0 not modelled) */
        }
    }
}

/* ================================================================ 5.2 AT3 horizontal crushers (4511-4518) */
static void crushers_h(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || (o->type != OT_CRUSH_H && o->type != OT_CRUSH_H2)) continue;
        double *A = &o->av[0], *B = &o->av[1], C = o->av[2], *D = &o->av[3], *E = &o->av[4];
        if (*D < 40) *D += 1;                                                                        /* 4511 */
        if (*D == 40) { *B += 3 * *E; o->x = (float)(int)(*A + *B); }                               /* 4512 */
        if (*D == 40 && p->state != ST_DEAD) {                                                        /* 4513 / 4517 */
            int bb = sens_hit(SENS_BOTTOMBIG, o) && p->ground;
            if ((*E == -1 && sens_hit(SENS_RIGHTBIG, o)) || (*E == 1 && sens_hit(SENS_LEFTBIG, o)) ||
                (*E == 1 && bb && !sens_bg(SENS_RIGHTBIG)) || (*E == -1 && bb && !sens_bg(SENS_LEFTBIG))) { p->x += 3 * *E; resense(p); }
        }
        if (*D == 40 && ((*E == 1 && *B >= C) || (*E == -1 && *B <= -C))) { *D = 0; *E = -*E; }     /* 4514 / 4518 */
    }
}

/* ================================================================ 8.1 axel platforms (4084-4092, 4519, 4551, 4552) */
static obj_t *first_of(int t) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == t) return &G.o[i]; return NULL; }
static void axel_new(int t, const obj_t *mk) {
    obj_t *o = obj_alloc(); if (!o) return;
    o->type = t; o->alive = 1; o->hp = 1; o->x = mk->x; o->y = mk->y; o->layer = 2; o->born = G.frame;
    set_ens(o, t == OT_AXEL1 ? "axel1" : "axel2"); o->c = 10;
    path_init(o, 3, 64, 0, 0, 0);
}
static void axel_speed(obj_t *o) { o->c = o->av[0] == 180 ? 10 : (int)(10 + 15 * fabs(c_sin(o->av[0]))); }
static void axels(player_t *p) {
    int PY = PY_(p), M = orig_M(p);
    for (int i = 0; i < G.nobjs; i++) {                                                               /* placed (AT3): 4519, 4090, inline 846 */
        obj_t *o = &G.o[i]; if (!o->alive || (o->type != OT_AXEL1P && o->type != OT_AXEL2P)) continue;
        o->fl &= ~0x41; axel_speed(o);
        if (o->y < PY) o->fl |= 0x41;
    }
    obj_t *m1 = first_of(OT_AXMAKER1), *m2 = first_of(OT_AXMAKER2);
    if (!m1 && !m2 && !first_of(OT_AXEL1) && !first_of(OT_AXEL2)) return;
    if (GT() % 60 == 0) { if (m1) axel_new(OT_AXEL1, m1); if (m2) axel_new(OT_AXEL2, m2); }       /* 4084 / 4551 (U5: GT phase) */
    for (int i = 0; i < G.nobjs; i++) {                                                               /* 4085 */
        obj_t *o = &G.o[i]; if (!o->alive || (o->type != OT_AXEL1 && o->type != OT_AXEL2)) continue;
        o->fl &= ~0x41; o->oy += o->type == OT_AXEL1 ? 2 : -2; axel_speed(o);
        if (o->type == OT_AXEL1 && o->y > G.lvl.h) o->alive = 0;                                      /* 4086 */
        if (o->type == OT_AXEL2 && o->y < level_cur()->axel2_top) o->alive = 0;                     /* 4087 / 4552: level data (AT1 320, AT4 0) */
    }
    int bg = M != 10 && p->ground && sens_bg(SENS_BOTTOMBIG);
    int tx, ty; mv_sensor_pos(SENS_TOPBIG, &tx, &ty);
    int roof = px_solid(&G.lvl, tx, ty - 32);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive) continue;
        if (o->type == OT_AXEL1) {
            if (bg) o->fl |= 0x41;                                                                     /* 4088 */
            if (m1 && o->y <= m1->y + 159) o->fl |= 0x41;                                             /* 4089 */
            if (o->y < PY) o->fl |= 0x41;                                                              /* 4090 */
        } else if (o->type == OT_AXEL2) {
            if (M != 10 && roof && sens_hit(SENS_BOTTOMBIG, o)) o->fl |= 0x41;                        /* 4091: anti-crush */
            if (o->y < PY) o->fl |= 0x41;                                                              /* 4092 */
        }
    }
}

/* ================================================================ 8.3 swings (4480-4482 spawn; 4357-4360) */
static void swing_part(int t, const obj_t *sp, double C, double D, double F, int links) {
    obj_t *o = obj_alloc(); if (!o) return;
    o->type = t; o->alive = 1; o->hp = 1; o->x = sp->x; o->y = sp->y; o->layer = 2;
    set_ens(o, t == OT_SWBASE ? "swbase" : t == OT_SWCHAIN ? "swchain" : "swplat");
    o->av[0] = sp->x + 0.5; o->av[1] = sp->y; o->av[2] = C; o->av[3] = D; o->av[4] = 0; o->av[5] = F;
    o->av[6] = 2 * 3.14159265358979323846 * C; o->ox = o->x; o->oy = o->y; o->st = 0; (void)links;
}
static void swing_spawn(void) {
    pgaF = 0;
    double wl = level_cur()->water ? level_cur()->water : 1e9;                                        /* ML.B (AT3: no water) */
    int n = G.nobjs;
    for (int i = 0; i < n; i++) {
        obj_t *s = &G.o[i]; if (!s->alive || (s->type != OT_SWSPAWN1 && s->type != OT_SWSPAWN1B && s->type != OT_SWSPAWN2)) continue;
        int links = s->type == OT_SWSPAWN1 ? 8 : 6; double D = s->type == OT_SWSPAWN2 ? 220 : 180;   /* ctor A, B: 824 8 links, 848 / 2 6 */
        if (s->type == OT_SWSPAWN2 && level_cur()->stage == 4) { links = 8; D = 0; }                 /* SB4's GimmickSwingSpawner2_2322: ctor A 8, B unset (0) (RE, session 19b) */
        double d = s->y + 72 - wl, F = d < 0 ? 0.04 : d <= 1 ? 0.04 - 0.015 * d : 0.025;
        swing_part(OT_SWBASE, s, 0, D, F, links);                                                     /* 4336 */
        for (int k = 0; k < links; k++) swing_part(OT_SWCHAIN, s, 16 * (k + 1), D, F, links);        /* 4334 */
        swing_part(OT_SWPLAT, s, 16 * (links + 1), D, F, links);                                     /* 4335 */
        s->alive = 0;                                                                                 /* 4482 */
    }
}
static void swings(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {                                                               /* 4357: launch */
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_SWPLAT) continue;
        if (o->st == -1 && !p->ground && (pgaF > 1 || (cur_in && (cur_in->pressed & BTN_JUMP)))) {
            double v = o->av[4] / 360 * o->av[6];
            p->vx -= v * c_sin(o->av[3]); p->vy -= v * c_cos(o->av[3]); o->st = 5;
        }
    }
    for (int i = 0; i < G.nobjs; i++) {                                                               /* 4358 */
        obj_t *o = &G.o[i]; if (!o->alive || (o->type != OT_SWCHAIN && o->type != OT_SWPLAT)) continue;
        o->av[4] -= o->av[5] * c_cos(o->av[3]); o->av[3] += o->av[4];
        o->x = (float)(int)(o->av[0] + o->av[2] * c_cos(o->av[3])); o->y = (float)(int)(o->av[1] - o->av[2] * c_sin(o->av[3]));
    }
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_SWPLAT && o->st > 0) o->st--; }
    pgaF = 0;
    if (p->ground) {                                                                                  /* 4359: carry */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_SWPLAT || !sens_hit(SENS_BOTTOMBIG, o)) continue;
            p->x += o->x - o->ox; p->y += o->y - o->oy;
            for (int k = i; k < G.nobjs; k++) if (G.o[k].alive && G.o[k].type == OT_SWPLAT && sens_hit(SENS_BOTTOMBIG, &G.o[k])) G.o[k].st = -1;
            pgaF = fabs(pB(p)) + fabs(p->ground ? 0 : p->vy);
            resense(p);
            break;
        }
    }
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_SWPLAT) { o->ox = o->x; o->oy = o->y; } }   /* 4360 */
}

/* ================================================================ 8.5 / 8.6 slime floor, rafts (4553-4559) */
static void slime_raft(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || (o->type != OT_SLIMEFLOOR && o->type != OT_SLIMEF_VIS)) continue;
        o->x = (float)(int)(o->av[0] - o->av[2]); o->av[2] = fmod(o->av[2] + 4, 128);                 /* 4554 */
    }
    if (p->ground && !p->hurt_pending && !player_invincible(p) && p->state != ST_HURT && p->state != ST_DEAD)   /* 4555 */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (o->alive && o->type == OT_SLIMEFLOOR && sens_hit(SENS_BOTTOMBIG, o)) { player_contact(p, 5); p->tint = 100; break; }   /* OTHERS.R = 100: green tint */
        }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_RAFT) continue;
        if (o->st == 0 && p->ground && sens_hit(SENS_BOTTOMBIG, o)) o->st = 1;                        /* 4556 */
        if (o->st == 1) { o->x += 4; o->av[1] -= 4; }                                                 /* 4557 */
        if (o->st == 1 && p->ground && sens_hit(SENS_BOTTOMBIG, o)) p->x += 4;                        /* 4558 */
        if (o->st == 1 && o->av[1] <= 0) o->st = 2;                                                   /* 4559 */
    }
}

/* ================================================================ the AT platform block (spec 0.2 order) */
/* Placement (RV session 12): the act's gimmick block runs in aqua_update(), after Lilac's movement and the objects
 * pass, not in objects_platforms(): a4_raft, a1_plates, a3_crusher, a2_beam and a2_crusher only match that way (the raft
 * starts, the door moves, the pillar carries on the frame the sensors first reach them). The RE call order puts these
 * events before the movement call; the port's frame boundary differs from that (see RE_NOTES session 12). */
void aqua_gim_post(player_t *p) {
    if (!objects_have_from(OT_DIGSPOT)) return;   /* nothing of this family placed (Dragon Valley) */
#define A_(t) objects_alive_type(t)
    /* each pass scans every slot: skipped when none of its types is alive (RM places dig spots, so the block runs there) */
    int q13 = A_(OT_CRUSH_V) || A_(OT_CRUSH_VIS) || A_(OT_AXEL1) || A_(OT_AXEL2) || A_(OT_AXEL1P) || A_(OT_AXEL2P);
    if (q13) path_step();                    /* 3270-3289 */
    if (q13) carry(p);                       /* 4061 */
    if (A_(OT_SWPLAT) || A_(OT_SWCHAIN) || A_(OT_SWSPAWN1) || A_(OT_SWSPAWN1B) || A_(OT_SWSPAWN2)) swings(p);   /* 4357-4360 (AT2, AT3) */
    if (A_(OT_COLLAPSE)) collapse_frame(p);  /* 4078-4083 */
    switches_update(p, 1);                   /* 4093-4100, 4362-4364 (src/game/switches.c, levels with switch_late) */
    if (A_(OT_WPLATE_AT) || A_(OT_PIPE) || A_(OT_PIPECOG)) wallplates_pipes(p);   /* 4365-4371, 4507-4510 */
    if (A_(OT_BEAM)) beams(p);               /* 4372 / 4373 */
    if (A_(OT_CRUSH_H) || A_(OT_CRUSH_H2)) crushers_h(p);   /* 4511-4518 */
    if (A_(OT_AXEL1P) || A_(OT_AXEL2P) || A_(OT_AXMAKER1) || A_(OT_AXMAKER2) || A_(OT_AXEL1) || A_(OT_AXEL2)) axels(p);   /* 4084-4092 / 4519 / 4551-4552 */
    if (A_(OT_SLIMEFLOOR) || A_(OT_SLIMEF_VIS) || A_(OT_RAFT)) slime_raft(p);   /* 4553-4559 */
    if (q13) dip(p);                         /* 3303-3307 */
#undef A_
}
int aqua_gim_obj(obj_t *o) { return (o->type >= OT_ABPLAT && o->type <= OT_EXPL01) || (o->type >= OT_ABPLAT2 && o->type <= OT_SWSPAWN1B); }
