/* Sky Battalion gimmicks (session 19): the hover platforms LevelSBPlatform1_2095 (+ LevelSBPlatformFlame1_2096) with the
 * SB2-SB4 opening hold / release, LevelSBPlatform3_2188, the act-end cannons (GimmickCannon_2142 + Base_2141 (Q20) + Base2_2176
 * + CannonBullet_2098), the fire jets HazardFireUp/Down/Left/Right, GimmickFlamethrower_2182 (MinibossFlame_1264 = relic.c's
 * OT_FLAME), the lightning HazardLightningRadial/2, the fans (GimmickFan_2223 lift + GimmickFanDebris_2230, FanSide, fan
 * blades), the breakable floors BreakableFloor1/2 (+ Piece2_2229), SB4's chain walls + masks, scrolling chains, crusher
 * platform + chain, moving block, the paternoster bars (GimmickSBBar_2314 + Mask_2313, Q23 hang), SB3's spin cups, back
 * eyes, the loop pieces (SBLoop1-7, Active..Active4: objdefs OD_LAYER0 / 1) and the decor.
 * Source: build/re/spec_sb_gimmicks.md. Labels: RE (read from the events / constructors), RV (original traces session 19),
 * INFERRED (marked).
 * Passes: sb_gim_pre (objects_platforms, before the movement: the one-way flags 4737 / 4738, 9344 / 9345, the fans' lift),
 * sb_gim_obj (the objects.c per-object pass: each object's own events), sb_gim_frame (after the objects: the path step +
 * carry 3283-3289 / 4061, the opening hold / release 9461 / 9468 / 9548 / 9549, the dip 3303-3307, the families' events).
 * Performance: every type keeps its instance list (spawn + runtime children, EACH); nothing runs without these types (S.on).
 * Object fields: par = family member (importer SBG_SUB: lvobj anim bits 0-7), layer = instance layer (bits 8-15); path
 * platforms: av[0] A, av[1] E, av[2] B, av[3] F, av[4] C, av[5] G, kx I, ky J, c bits D / H, av[6] / av[7] last M / N,
 * vx / vy the frame's O / P, fy the dip; fl bit 0 + 0x40 = one-way pass (shared meaning), own flags >= 0x1000. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define RAD 57.29577951308232
#define RT0 OT_SBG_FIRST
#define NRT (OT_SBG_LAST - OT_SBG_FIRST + 1)
#define LMAX 128
#define F_REL 0x800                                /* platform1 flag 11: released (9468), being flown in */
#define F_STOOD 0x1000                             /* dip: stood on this frame */
#define F1 0x2000                                  /* the object's flag 1 */
#define F2 0x4000                                  /* the object's flag 2 */
#define F6 0x8000                                  /* breakable floor flag 6 (break) */
#define DEFEATED(o) ((o)->fl & 0x40)
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]
static struct { int on, kills; double Q[LMAX]; short lim_x[64], lim_y[64]; int nlim; } S;   /* Q: platform1 alterable Q (its I before the hold) by list slot */

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double nrm(double d) { d = fmod(d, 360.); return d < 0 ? d + 360. : d; }
static double c_sin(double deg) { double r = nrm(deg); return (r == 0. || r == 180.) ? 0. : fd_sin(r / RAD); }
static double c_cos(double deg) { double r = nrm(deg); return (r == 90. || r == 270.) ? 0. : fd_cos(r / RAD); }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int R(int n) { return cross_rand(n); }

static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static int slot_of(int T, const obj_t *o) { for (int k = 0; k < NLS[T - RT0]; k++) if (&G.o[LST[T - RT0][k]] == o) return k; return 0; }
static obj_t *spawn(int type, const char *anim, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame; o->par = -1;
    if (anim) { set_s(o, anim); if (o->an.b) anim_restart(&o->an); }
    lst_add(o);
    return o;
}
static void shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }                    /* CAM.Z = max(CAM.Z, z) */
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }       /* !is_near_border(-n) */
static int vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }
static int r_hurt(player_t *p, int n) { if (!vulnerable(p)) return 0; player_contact(p, n); return 1; }   /* MV.R = 1, LIFE.B += n */
static void kill_player(player_t *p) {                                          /* the shared kill block: M 9 */
    if (p->state == ST_DEAD) return;
    p->hp = 0; p->state = ST_DEAD; p->dead_t = 0; p->vy = -5; p->vx = 0; p->gsp = 0; p->ground = 0; p->invinc = 0; p->invuln = 0;
}
static void act_pt(const obj_t *o, float *x, float *y) { double ax, ay; relic_act_point(o, &ax, &ay); *x = (float)ax; *y = (float)ay; }
static void img_box(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    const frame_t *f = o->an.b ? anim_frame(&o->an) : NULL;
    if (!f) { *x0 = *x1 = o->x; *y0 = *y1 = o->y; return; }
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int box_ov(float a0, float b0, float a1, float b1, float c0, float d0, float c1, float d1) { return a0 < c1 && c0 < a1 && b0 < d1 && d0 < b1; }
static int csens_hit(const player_t *p, const obj_t *o) {                       /* PlayerCharacterSensor_375 */
    int X = (int)PX(p), Y = (int)PY(p), small = player_char_sensor_small(p);
    return o->an.b && obj_rect_hit(o, X - 7, small ? Y - 13 : Y - 34, X + 9, small ? Y + 15 : Y + 13);
}
static int disc_hit(const player_t *p, const obj_t *o) {                        /* PLAYER (Player_326: the 27-px disc) */
    static signed char x0[27], x1[27]; static int init;
    if (!init) { init = 1; for (int j = 0; j < 27; j++) { double d = 13.25 * 13.25 - (j - 13) * (j - 13); int w = d < 0 ? -1 : (int)sqrt(d); x0[j] = (signed char)(13 - w); x1[j] = (signed char)(13 + w); } }
    return o->an.b && obj_rows_hit(o, (int)PX(p) - 13, (int)PY(p) - 13, x0, x1, 27);
}
static int sens_hit(int s, const obj_t *o) {                                    /* a placed player sensor overlapping the object */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int sens_on_bg(int s) {
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) if (b[j] >= a[j] && px_span_solid(&G.lvl, ox + a[j], ox + b[j], oy + j)) return 1;
    return 0;
}
static int limiter_hit(const obj_t *o) {                                        /* EnemyLimiter_383: 16 x 32, hotspot (21, 18) */
    for (int k = 0; k < S.nlim; k++)
        if (fabsf(S.lim_x[k] - o->x) < 160 && fabsf(S.lim_y[k] - o->y) < 160 &&
            obj_rect_hit(o, S.lim_x[k] - 21, S.lim_y[k] - 18, S.lim_x[k] - 5, S.lim_y[k] + 14)) return 1;
    return 0;
}
static obj_t *pair(int T, const obj_t *o, float dx, float dy, float tol) {      /* the T object placed at o + (dx, dy) */
    obj_t *q, *best = NULL; float bd = tol;
    EACH(T, q) { float d = fabsf(q->x - (o->x + dx)) + fabsf(q->y - (o->y + dy)); if (d <= bd) { bd = d; best = q; } }
    return best;
}

/* ================================================================ path platforms (3283-3289) + carry (4061) + dip (Q12) */
static void path_init(obj_t *o, double A, double E, double B, double F, double C, double Gm, int D, int H) {
    o->kx = o->x; o->ky = o->y; o->av[0] = A; o->av[1] = E; o->av[2] = B; o->av[3] = F; o->av[4] = C; o->av[5] = Gm; o->c = D | (H << 1);
    o->av[6] = o->kx + C * ((o->c & 1) ? c_sin(A) : c_cos(A)); o->av[7] = o->ky + Gm * ((o->c & 2) ? c_sin(E) : c_cos(E));
    o->x = (float)floor(o->av[6]); o->y = (float)floor(o->av[7]); o->vx = o->vy = 0; o->fy = 0;
}
static void path_step(obj_t *o) {                    /* 3283-3289: A += B, E += F (mod 360); M = I + C cos|sin A, N = J + G cos|sin E */
    o->av[0] = nrm(o->av[0] + o->av[2]); o->av[1] = nrm(o->av[1] + o->av[3]);
    double M = o->kx + o->av[4] * ((o->c & 1) ? c_sin(o->av[0]) : c_cos(o->av[0]));
    double Nn = o->ky + o->av[5] * ((o->c & 2) ? c_sin(o->av[1]) : c_cos(o->av[1]));
    o->vx = (float)(floor(M) - floor(o->av[6])); o->vy = (float)(floor(Nn) - floor(o->av[7]));
    o->av[6] = M; o->av[7] = Nn; o->x = (float)floor(M); o->y = (float)floor(Nn);
}
static void carry(player_t *p) {                     /* 4061: G >= 0, S_BB 1 px lower on a Q13 platform with flag 6 off */
    static const int Q13[] = { OT_SB_PLAT1, OT_SB_PLAT3 };
    if (!(p->vy >= 0 || p->ground) || dead(p) || p->state == ST_HANG || p->state == ST_HELD) return;
    for (unsigned q = 0; q < sizeof Q13 / sizeof Q13[0]; q++) {
        if (!N(Q13[q])) continue;
        int k = mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, Q13[q]);
        if (k < 0 || (G.o[k].fl & 0x40)) continue;
        p->x += G.o[k].vx; p->y += G.o[k].vy;
        if (!p->ground) { p->gsp = p->vx; p->ground = 1; }
        p->vy = 0; p->N = 0; p->O = 0;
        player_sync_sensors(p);
        break;
    }
}
static void platforms(player_t *p) {
    obj_t *o;
    /* the dip of the last frame comes off, then the path step (3270-3272, 3283-3289) and the carry (4061) */
    EACH(OT_SB_PLAT1, o) { o->y -= floorf(o->fy); if (!(o->fl & F_STOOD)) o->fy -= o->fy * 0.2f; o->fl &= ~F_STOOD; }
    EACH(OT_SB_PLAT1, o) path_step(o);
    EACH(OT_SB_PLAT3, o) path_step(o);
    carry(p);
    /* SB2-SB4 opening (sb_flow.c: group 0x228f3) */
    int intro = sb_flow_intro();
    EACH(OT_SB_PLAT1, o) {
        int s = slot_of(OT_SB_PLAT1, o);
        if (intro && o->kx > 0 && o->kx < 1200 && !(o->fl & F_REL)) { S.Q[s] = o->kx; o->kx = -160; o->av[4] = 0; }   /* 9461: Q = I, I -160, C 0 */
        if (intro == 2 && o->kx < 0) o->fl |= F_REL;                                   /* 9468: the planks are out (A 2) */
        if (o->fl & F_REL) { o->kx = fmin(o->kx + 4, S.Q[s]); o->av[4] = fmin(o->av[4] + 0.5, 40); }   /* 9548 */
        if ((o->fl & F_REL) && o->kx >= S.Q[s]) o->fl &= ~F_REL;                       /* 9549 */
    }
    /* the dip (3303 / 3304 / 3307, Q12): stood on -> S eases to 9, the drawn y = N + S */
    EACH(OT_SB_PLAT1, o) {
        if (p->ground && !dead(p) && mv_sensor_obj(p->x, MY(p), p->N, SENSOR_BOTTOMBIG, OT_SB_PLAT1) == (int)(o - G.o)) o->fl |= F_STOOD;
        if (o->fl & F_STOOD) o->fy += (9 - o->fy) * 0.2f;
        o->y += floorf(o->fy);
    }
    EACH(OT_SB_PLATFLAME, o) if (o->par >= 0 && G.o[o->par].alive) { o->x = G.o[o->par].x - 1; o->y = G.o[o->par].y + 18; }   /* INFERRED */
}

/* ================================================================ cannons (9311-9324; SB3 9819-9822, SB4 9993: the same)
 * cannon: av[0] A angle, av[1] B target, av[2] C counter, av[3] D (its base's D), par = its base; base: av[3] D, av[4] E;
 * bullet: av[0] A x, av[1] B y, av[2] C speed 12 (ctor), av[3] D angle */
static void cannon_fire(obj_t *c) {                  /* 9315-9317: C + 1, anim 7, a bullet at the action point, sound 99, CAM.Z 4 */
    c->av[2] += 1; set_s(c, "sbc_fire"); anim_restart(&c->an);
    float x, y; act_pt(c, &x, &y);
    obj_t *b = spawn(OT_SB_CANBULLET, "sbc_ball", x, y, c->layer);
    if (b) { b->av[0] = x; b->av[1] = y; b->av[2] = 12; b->av[3] = c->av[0]; b->ang = (float)c->av[0]; }
    snd_play_orig(99, 220, 128); shake(4);
}
static void cannons(void) {
    obj_t *c, *b;
    EACH(OT_SB_CANNON, c) {
        obj_t *base = c->par >= 0 ? &G.o[c->par] : NULL;
        if (base && base->alive) { c->x = base->x; c->y = base->y; c->av[3] = base->av[3]; }   /* 9324: on its base */
        if (c->av[3] >= 30) { c->hidden = 1; continue; }                                         /* 9321 */
        if (in_view(c, 100) && c->av[0] > c->av[1]) c->av[0] = fmax(c->av[0] - 4, c->av[1]);  /* 9311 */
        if (in_view(c, 100) && c->av[0] < c->av[1]) c->av[0] = fmin(c->av[0] + 4, c->av[1]);  /* 9312 */
        if (c->av[0] == c->av[1]) c->fl |= F1;                                                  /* 9313 */
        double C = c->av[2];
        if ((c->fl & F1) && C != 10 && C != 30 && C != 50 && c->av[3] < 1) c->av[2] += 1;      /* 9314 */
    }
    for (int w = 10; w <= 50; w += 20) {             /* 9315-9317: one random cannon of those at C = w fires */
        int n = 0; EACH(OT_SB_CANNON, c) if (c->av[2] == w && !c->hidden) n++;
        if (!n) continue;
        int pick = R(n), k = 0;
        EACH(OT_SB_CANNON, c) if (c->av[2] == w && !c->hidden && k++ == pick) { cannon_fire(c); break; }
    }
    EACH(OT_SB_CANNON, c) {
        if (c->av[2] > 120) { c->av[1] = fmod(c->av[1] + 20, 50); c->fl &= ~F1; c->av[2] = (R(32768) * 5) >> 12; }   /* 9318 */
        if (c->an.done && c->an.b && c->an.anim == ens_anim("sbc_fire")) set_s(c, "sbc_cannon");
        c->ang = (float)c->av[0];                                                               /* 9324: angle A */
    }
    EACH(OT_SB_CANBULLET, b) {                       /* 9324: x = A, y = B; A += C cos D, B -= C sin D */
        b->x = (float)(int)b->av[0]; b->y = (float)(int)b->av[1];
        b->av[0] += c_cos(b->av[3]) * b->av[2]; b->av[1] -= c_sin(b->av[3]) * b->av[2]; b->ang = (float)b->av[3];
        if (!in_view(b, 200)) b->alive = 0;          /* INFERRED (no destroy event: the original keeps them) */
    }
    EACH(OT_SB_CANBASE, b) {                         /* the base: Q20 (combat.c), A = 15 */
        if (!DEFEATED(b)) continue;
        double *D = &b->av[3], *E = &b->av[4];
        if (*D == 0) { set_s(b, "sbc_bdead"); *D = 1; S.kills++; }                              /* 9319: anim 8, PlayerAssist.I + 1 */
        else if (*D < 30) { *D += 1; if (G.frame % 4 == 0) obj_effect(b->x - 40 + R(81), b->y - 40 + R(81), "explosion01"); }   /* 9320 */
        else if (*D == 30) {                                                                    /* 9322 */
            snd_play_orig(38, 220, 128); b->hidden = 1; *D += 1; obj_effect(b->x, b->y, "explosion01");
            obj_t *b2 = pair(OT_SB_CANBASE2, b, -8, 0, 24); if (b2) b2->hidden = 1;            /* INFERRED: the back plate goes with it */
        } else if (*E < 45) { *E += 1; if ((int)*E % 5 == 0) obj_effect(b->x - 32 + R(65), b->y - 32 + R(65), "explosion06"); }   /* 9323 */
    }
}
int sb_gim_cannon_kills(void) { return S.kills; }

/* ================================================================ SwitchDoor_2102 (SB2 9341 / 9342, SB4 9832 / 9833; RE)
 * The pad events: while the player stands on a SwitchPlatform (L 1, O < 30 or > 330) whose B is still below 24 after this frame's
 * sink (+0.5), the door with A = the pad's C rises 2 px (y - 2, C + 2): 47 frames, 94 px for the 24-px sink. No other door event
 * (the door is a plain Q7 solid; it never comes back down). av: [0] A index, [2] C risen. */
void sb_gim_door_rise(int c) {
    if (!S.on || !N(OT_SB_DOOR)) return;
    obj_t *o;
    EACH(OT_SB_DOOR, o) if ((int)o->av[0] == c) { o->y -= 2; o->av[2] += 2; }
}

/* ================================================================ fire jets (9332-9340; 9765: A = index 20 at the start) */
static void fires(player_t *p) {
    obj_t *o;
    static const int SN[4] = { SENS_TOPBIG, -1, SENS_LEFTBIG, SENS_RIGHTBIG };   /* up, down (PLAYER only), left (9340: LeftBig), right (9339: RightBig) */
    EACH(OT_SB_FIRE, o) {
        o->av[0] = fmod(o->av[0] + 1, 180);                                       /* 9332: A = (A + 1) mod 180, direction 0 */
        int on = o->av[0] > 90;                                                   /* 9333-9336: A > 90 -> direction 1 (the low pilot flame) */
        static const char *const NM[4][2] = { { "sbf_up", "sbf_up1" }, { "sbf_down", "sbf_down1" }, { "sbf_left", "sbf_left1" }, { "sbf_right", "sbf_right1" } };
        set_s(o, NM[o->par & 3][on]);
        if (on || !in_view(o, 50) || dead(p) || p->shield == 2) continue;          /* 9337-9340: near, test_directions(1) = mask bit 0 = direction 0 (the tall jet), not the fire shield */
        if (!disc_hit(p, o) || (SN[o->par & 3] >= 0 && !sens_hit(SN[o->par & 3], o))) continue;
        if (r_hurt(p, 5)) { p->tint = 30; p->tint_argb = 0xffffa080; }             /* LIFE.B + 5, OTHERS.R 30, blend 0x80a0ff */
    }
}

/* ================================================================ flamethrowers (9325-9330, 9292); ctor C 90 D 60 E 2
 * av[0] A angle, av[1] B phase, av[2] C centre, av[3] D sweep, av[4] E speed */
static void flamers(void) {
    obj_t *o;
    EACH(OT_SB_FLAMER, o) {
        o->av[0] = o->av[2] - c_sin(o->av[1]) * o->av[3]; o->av[1] = nrm(o->av[1] + o->av[4]); o->ang = (float)o->av[0];   /* 9326 */
        o->fl &= ~F1;
        double B = o->av[1];
        if ((B > 150 && B < 210) || B > 330 || B <= 30) o->fl |= F1;                 /* 9327: aimed near C */
        if (in_view(o, 100) && (o->fl & F1) && !(o->fl & F2)) { o->fl |= F2; snd_play_orig(263, 220, 128); }   /* 9328 */
        if (!(o->fl & F1) && (o->fl & F2)) o->fl &= ~F2;                             /* 9329 */
        if (in_view(o, 100) && (o->fl & F1) && G.frame % 3 == 0) {                  /* 9330 / 9292: MinibossFlame at the action point */
            float x, y; act_pt(o, &x, &y);
            obj_t *f = relic_flame_new(x, y, 6 * c_cos(o->av[0]), -6 * c_sin(o->av[0]));
            if (f) f->layer = o->layer;
        }
    }
}

/* ================================================================ lightning (9823-9826; start: A = index 120, ctor B 3)
 * radial (invisible, rotating): av[0] A, av[1] B, av[2] C turn cooldown; radial2 (drawn) sits on its radial */
static void lightning(player_t *p) {
    obj_t *o, *q;
    EACH(OT_SB_LIGHTNING, o) {
        o->av[2] = fmax(o->av[2] - 1, 0);                                          /* 9823 */
        if (o->av[2] <= 0 && limiter_hit(o)) { o->av[1] = -o->av[1]; o->av[2] = 10; }   /* 9824 (cooldown 10: INFERRED) */
        o->av[0] = nrm(o->av[0] + o->av[1]); o->ang = (float)o->av[0];                 /* 9825 */
        if (in_view(o, 32) && !dead(p) && p->shield != 3 && csens_hit(p, o) && r_hurt(p, 10)) { shake(3); snd_play_orig(371, 220, 128); }   /* 9826: not the metal shield */
    }
    EACH(OT_SB_LIGHTNING2, q) if (q->par >= 0 && G.o[q->par].alive) { q->x = G.o[q->par].x; q->y = G.o[q->par].y; q->ang = G.o[q->par].ang; }   /* 9823 */
}

/* ================================================================ fans (9347-9352), fan debris (9348 / 9349), blades (9346) */
static void fans(player_t *p) {
    obj_t *o;
    int lift = 0;
    EACH(OT_SB_FAN, o) {
        int fx = (int)o->x, fy = (int)o->y, X = (int)PX(p), Y = (int)PY(p);
        if (fx >= X - 63 && fx <= X + 63 && fy > Y && fy <= Y + 399) lift = 1;     /* 9350-9352: the column above the fan */
        if (fx < G.camx - 63 || fx > G.camx + 489 || fy <= G.camy || G.frame % 3) continue;   /* 9347 */
        obj_t *d = spawn(OT_SB_FANDEBRIS, "sbv_debris", o->x, o->y, o->layer);
        if (d) {                                                                    /* 9348: x - 60 + R(121), A = y, x scale 0.5-1 */
            d->x += (float)(-60 + R(121)); d->av[0] = d->y; d->scale = (float)((50 + R(51)) * 0.01);
            d->vx = (float)(0.5 * c_cos(90 + R(40))); d->vy = -6;                   /* BallMovement direction 90 + R(40) (INFERRED speed 6) */
        }
    }
    if (lift && !dead(p) && !p->ground) {
        if (p->state == ST_NORMAL && p->cY != 24) p->springing = 1;                  /* 9350: L 0, M <= 1, OTHERS.Y != 24 -> M 40 */
        p->vy -= 0.2;                                                               /* 9351 */
        if (chr_is_milla(G.character)) p->vy -= 0.1;                                /* 9352: MV.A >= 4 */
    }
    EACH(OT_SB_FANDEBRIS, o) { o->x += o->vx; o->y += o->vy; if (o->y < o->av[0] - 400) o->alive = 0; }   /* 9349 */
}

/* ================================================================ breakable floors (9353-9358, 7219, 10015 / 10016, 10024 / 10025) */
static void floors(void) {
    obj_t *o, *e;
    EACH(OT_SB_BFLOOR, o) {
        if (o->fl & F6) continue;
        float a0, b0, a1, b1; img_box(o, &a0, &b0, &a1, &b1);
        for (int i = 0; i < G.nobjs; i++) {                                         /* 9354 / 7219: the bomb's blast (MonitorIcon frame 9) */
            e = &G.o[i];
            if (!e->alive || e->type != OT_BOMBX || fabsf(e->x - o->x) > 128 || fabsf(e->y - o->y) > 128) continue;
            if (box_ov(a0, b0, a1, b1, e->x - 8, e->y - 4, e->x + 40, e->y + 36)) { o->fl |= F6; break; }
        }
    }
    static const int FP[8][2] = { { 8, 12 }, { 24, 12 }, { 40, 12 }, { 56, 12 }, { 8, 36 }, { 24, 36 }, { 40, 36 }, { 56, 36 } };
    EACH(OT_SB_BFLOOR, o) if (o->fl & F6) {                                         /* 9355 / 9356: sound 13, 8 pieces, CAM.Z 3 */
        obj_t v = *o; o->alive = 0; snd_play_orig(13, 220, 128); shake(3);
        for (int k = 0; k < 8; k++) {
            obj_t *q = spawn(OT_SB_FLOORPIECE, "sbq_fpiece", v.x + FP[k][0], v.y + FP[k][1], v.layer);
            if (!q) break;
            q->av[0] = v.av[0] - 4 + R(8); q->av[1] = v.av[1] - 4 + R(4); q->av[2] = 0.3; q->av[3] = q->x; q->av[4] = q->y;
        }
    }
    EACH(OT_SB_FLOORPIECE, o) {                                                     /* 9357 / 9358 */
        if (o->born == G.frame) continue;
        o->av[3] += o->av[0]; o->av[1] += o->av[2]; o->av[4] += o->av[1];
        o->x = (float)(int)o->av[3]; o->y = (float)(int)o->av[4]; o->ang = (float)nrm(o->ang + o->av[0]);
        if (o->av[4] > G.camy + 320) o->alive = 0;
    }
}
static void floors_hit_by(const obj_t *s) {          /* 10015 / 10016, 10024 / 10025: flag 6 on the floors it overlaps */
    obj_t *o;
    EACH(OT_SB_BFLOOR, o) if (fabsf(o->x - s->x) < 192 && fabsf(o->y - s->y) < 192 && obj_obj_hit(o, s)) o->fl |= F6;
}

/* ================================================================ SB4: chain walls (9996-10001, 10146), scrolling chains,
 * crusher (10009-10021), moving block (10022-10026), bars (10002-10008, 10147) */
static void chains(player_t *p) {
    obj_t *o, *w;
    int g3 = (int)(G.frame % 100000);
    EACH(OT_SB_SCHAIN, o) o->y = (float)(int)(o->par ? o->av[0] + (g3 * 3) % 64 : o->av[0] + 64 - (g3 * 3) % 64);   /* 9996 */
    EACH(OT_SB_CHAINMASK, o) {                                                      /* 9996: y = A + B, B -/+ 3; 9964 / 9966 reset B (span INFERRED) */
        o->av[1] += o->par ? 3 : -3;
        if (fabs(o->av[1]) >= 600) o->av[1] = 0;
        o->y = (float)(int)(o->av[0] + o->av[1]);
    }
    EACH(OT_SB_CHAINWALL, w) if (w->par >= 0 && G.o[w->par].alive) { w->x = G.o[w->par].x; w->y = G.o[w->par].y; }   /* 9996: on its mask */
    if (dead(p)) return;
    EACH(OT_SB_CHAINMASK, o) {
        if (p->ground && sens_hit(SENS_BOTTOMBIG, o)) {                             /* 9997-10000: no standing on top: pushed off */
            p->ground = 0; p->vx = p->gsp + (PX(p) < o->x ? -2 : 2);
        }
        if (!o->par && sens_hit(SENS_TOPBIG, o) && p->ground) kill_player(p);      /* 10001: pinned under a rising wall (Z = 1: INFERRED = grounded) */
    }
}
static void crushers(player_t *p) {
    obj_t *o, *c;
    EACH(OT_SB_CRUSHER, o) {
        double *A = &o->av[0], *B = &o->av[1], *C = &o->av[2];
        int on = !dead(p) && p->ground && sens_hit(SENS_BOTTOMBIG, o);
        if (*C == 0) { o->y -= 2; if (on) { p->y -= 2; player_sync_sensors(p); } if (o->y <= *A - 128) *C = 1; }   /* 10010 / 10011 (top: INFERRED) */
        else if (*C == 1) { if (fabs(PX(p) - o->x) < 48 && PY(p) > o->y) { *C = 2; o->fl |= F1; } }   /* 10013 (trigger box INFERRED) */
        else if (*C == 2) {                                                         /* 10014 / 10017 */
            *B += 0.25; o->y += (float)*B;
            if (o->y >= *A) { o->y = (float)*A; shake(4); *C += 1; snd_play_orig(75, 220, 128); *B = 0; }
            floors_hit_by(o);
        } else { *C += 1; if (*C > 60) *C = 0; }                                    /* 10018 / 10019 */
        if (*C > 2 && (o->fl & F1) && !disc_hit(p, o)) o->fl &= ~F1;                 /* 10020 */
        if (!dead(p) && p->invuln <= 0 && sens_hit(SENS_TOPBIG, o) && *C == 2) r_hurt(p, 15);   /* 10021 */
        if (!dead(p) && sens_hit(SENS_BOTTOMBIG, o) && sens_on_bg(SENS_TOPBIG)) kill_player(p);   /* 10012: crushed */
        if (o->an.b) { int k = (int)((G.frame / 5) % 8); if (k < o->an.b->anims[o->an.anim].count) o->an.frame = k; }   /* 10009 */
    }
    EACH(OT_SB_CRUSHCHAIN, c) if (c->par >= 0 && G.o[c->par].alive) { c->x = G.o[c->par].x; c->y = G.o[c->par].y; }   /* 10009 */
    EACH(OT_SB_MBLOCK, o) {
        double *A = &o->av[0], *B = &o->av[1];
        int on = !dead(p) && p->ground && sens_hit(SENS_BOTTOMBIG, o);
        if (*B == 0 && on) *B = 1;                                                  /* INFERRED trigger: stood on */
        if (*B == 1) {
            *A = fmin(*A + 0.25, 15); o->y += (float)*A;                             /* 10022 */
            if (on) { p->y += (float)*A; player_sync_sensors(p); }                   /* 10023 */
            floors_hit_by(o);                                                        /* 10024 / 10025 */
            if (obj_overlaps_bg(o)) { *B = 2; snd_play_orig(17, 220, 128); shake(4); }   /* 10026 */
        }
    }
}
static void bars(player_t *p) {
    obj_t *o;
    EACH(OT_SB_BAR, o) { path_step(o); o->layer = o->av[1] <= 180 ? 2 : 3; }        /* 10002 / 10003 */
    EACH(OT_SB_BARMASK, o) {
        path_step(o); o->layer = o->av[1] <= 180 ? 2 : 3;                            /* 10004 / 10005 */
        int held = p->state == ST_HANG && p->hang_obj == (int)(o - G.o);
        if (held && o->av[1] > 180) {                                               /* 10008: M 40, G 0, Q 0 */
            p->state = ST_NORMAL; p->state_t = 0; p->ground = 0; p->vy = 0; p->springing = 1; p->hang_obj = -1;
        } else if (!held && o->av[1] <= 180 && !dead(p)) obj_hang_try(o, p);         /* 10006 / 10007, 4065 (Q23) */
    }
}

/* ================================================================ SB3: spin cups (9836-9838, 9938), back eyes (9835) */
static void cups(player_t *p) {
    obj_t *o;
    EACH(OT_SB_SPINCUP, o) {
        o->av[0] = nrm(o->av[0] + 357); o->ang = (float)o->av[0];                   /* 9836: A = (A - 3) mod 360 */
        o->av[1] = fmax(o->av[1] - 1, 0);
        obj_t *s = o->par >= 0 ? &G.o[o->par] : NULL;                               /* the start point rides the action point */
        if (s && s->alive) { float x, y; act_pt(o, &x, &y); s->x = x; s->y = y; }
        if (!(o->fl & F1) && o->av[1] <= 0 && s && !dead(p) && disc_hit(p, s)) o->fl |= F1;   /* the grab (INFERRED: the handler is not in the event list) */
        if (o->fl & F1) {                                                           /* 9837: M 43 on the cup's action point, OTHERS.Y 8 */
            float x, y; act_pt(o, &x, &y);
            p->x = x; p->y = y + ORIG_REF_DY; p->ground = 0; p->cY = 8;
            p->vx = 6 * c_cos(o->av[0]); p->vy = -6 * c_sin(o->av[0]);
            player_sync_sensors(p);
            obj_t *l;
            EACH(OT_SB_CUPLAND, l) if (obj_obj_hit(o, l)) { o->fl &= ~F1; o->av[1] = 60; }   /* 9838: let go, B 60 */
        }
    }
    EACH(OT_SB_BACKEYE, o) {                                                        /* 9835: alpha (128 - (64 sin A + 64)) 2, A + 4 */
        if (!in_view(o, 100)) continue;
        int a = (128 - (int)(c_sin(o->av[0]) * 64 + 64)) * 2;
        o->av[2] = a < 0 ? 0 : a > 255 ? 255 : a; o->av[0] = nrm(o->av[0] + 4);
        o->hidden = o->av[2] <= 0;
    }
}

/* ================================================================ spawn */
static const char *const LOOPN[3][5] = { { "sbl_0_0", "sbl_0_1", "sbl_0_2", "sbl_0_3", "sbl_0_4" },
                                         { "sbl_1_0", "sbl_1_1", "sbl_1_2", "sbl_1_3", "" }, { "sbl_2_0", "sbl_2_1", "", "", "" } };
void sb_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); snap_register(NLS, sizeof NLS); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS);
    if (!objects_have_from(RT0)) return;
    /* family member / layer: the k-th object of a type is the k-th placement of that type (objects_spawn keeps the order) */
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_LIMITER && S.nlim < 64) { S.lim_x[S.nlim] = (short)o->x; S.lim_y[S.nlim++] = (short)o->y; }
        if (o->type < RT0 || o->type > OT_SBG_LAST) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0xff;
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = 0; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        lst_add(o); S.on = 1;
    }
    if (!S.on) return;
    obj_t *o, *q;
    int k, n;
    /* 9766 (start of the act): platform3 A = index 20; platform1 E = index 180, A = E + 90 (index = selection order, newest
     * first: the last placement is 0). Constructors (RE): platform1 A 90 B 1 C 40 F 2 G 10 H 1; platform3 B 2 C 128 */
    n = N(OT_SB_PLAT1); k = 0;
    EACH(OT_SB_PLAT1, o) { double E = (n - 1 - k++) * 180.; set_s(o, "sbp_plat1"); path_init(o, E + 90, E, 1, 2, 40, 10, 0, 1); }
    n = N(OT_SB_PLAT3); k = 0;
    EACH(OT_SB_PLAT3, o) { set_s(o, "sbp_plat3"); path_init(o, (n - 1 - k++) * 20., 0, 2, 0, 128, 0, 0, 0); }
    EACH(OT_SB_PLATFLAME, o) {                       /* each flame rides the platform placed at (+1, -18) from it (INFERRED pairing) */
        set_s(o, "sbp_flame"); o->par = -1; float best = 1e9f;
        EACH(OT_SB_PLAT1, q) { float d = (float)(fabs(q->kx - (o->x + 1)) + fabs(q->ky - (o->y - 18))); if (d < best) { best = d; o->par = (int)(q - G.o); } }
        if (best > 4) o->par = -1;
    }
    EACH(OT_SB_LOOP0, o) set_s(o, LOOPN[0][o->par < 5 ? o->par : 0]);
    EACH(OT_SB_LOOP1, o) set_s(o, LOOPN[1][o->par < 4 ? o->par : 0]);
    EACH(OT_SB_LOOP1B, o) set_s(o, LOOPN[2][o->par < 2 ? o->par : 0]);
    { int ks[3] = { 0 }; EACH(OT_SB_FANBLADE, o) { static const char *const B[3] = { "sbv_blade0", "sbv_blade1", "sbv_blade2" }; int s = o->par < 3 ? o->par : 0; set_s(o, B[s]); o->av[0] = (ks[s]++) * 90.; o->ang = (float)nrm(o->av[0]); } }   /* 9767: A = index 90 */
    EACH(OT_SB_FAN, o) set_s(o, "sbv_fan");
    EACH(OT_SB_FANSIDE, o) set_s(o, "sbv_fanside");
    { int ks[2] = { 0 }; EACH(OT_SB_SCHAIN, o) { set_s(o, "sbv_chain"); o->av[0] = o->y; (void)ks; } }   /* 10146: A = y */
    EACH(OT_SB_BACKEYE, o) set_s(o, o->par ? "sbv_eye1" : "sbv_eye0");
    /* cannons: ctor A 30 B 30 C 20; base A 15 (Q20); each cannon sits on the base placed at its position */
    EACH(OT_SB_CANBASE, o) { set_s(o, "sbc_base"); o->A = 15; o->fl |= F_NOFLY; }
    EACH(OT_SB_CANBASE2, o) set_s(o, "sbc_base2");
    EACH(OT_SB_CANNON, o) { set_s(o, "sbc_cannon"); o->av[0] = 30; o->av[1] = 30; o->av[2] = 20; q = pair(OT_SB_CANBASE, o, 0, 0, 8); o->par = q ? (int)(q - G.o) : -1; }
    /* fires: 9765 A = index 20 (newest first) per direction family */
    { static const char *const NM[4] = { "sbf_up", "sbf_down", "sbf_left", "sbf_right" }; int nn[4] = { 0 }, kk[4] = { 0 };
      EACH(OT_SB_FIRE, o) nn[o->par & 3]++;
      EACH(OT_SB_FIRE, o) { int s = o->par & 3; set_s(o, NM[s]); o->av[0] = fmod((nn[s] - 1 - kk[s]++) * 20., 180); } }
    /* flamethrowers: ctor C 90 D 60 E 2; 9325: each limiter on one turns it by 90 (C + 90, B - 90) and is removed */
    EACH(OT_SB_FLAMER, o) {
        set_s(o, "sbf_thrower"); o->av[2] = 90; o->av[3] = 60; o->av[4] = 2;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *l = &G.o[i];
            if (!l->alive || l->type != OT_LIMITER || fabsf(l->x - o->x) > 96 || fabsf(l->y - o->y) > 96) continue;
            if (obj_rect_hit(o, (int)l->x - 21, (int)l->y - 18, (int)l->x - 5, (int)l->y + 14)) { o->av[2] += 90; o->av[1] -= 90; l->alive = 0; }
        }
        o->av[1] = nrm(o->av[1]);
    }
    /* lightning: on_frame_38 start A = index 120 (newest first), ctor B 3; radial2 rides the radial at its position */
    n = N(OT_SB_LIGHTNING); k = 0;
    EACH(OT_SB_LIGHTNING, o) { set_s(o, "sbz_ray"); o->av[0] = (n - 1 - k++) * 120.; o->av[1] = 3; }
    EACH(OT_SB_LIGHTNING2, o) {
        set_s(o, "sbz_ray2"); o->par = -1;
        obj_t *best = NULL; float bd = 1e9f;
        EACH(OT_SB_LIGHTNING, q) { int used = 0; obj_t *r; EACH(OT_SB_LIGHTNING2, r) if (r != o && r->par == (int)(q - G.o)) used = 1;
                                   float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (!used && d < bd) { bd = d; best = q; } }
        if (best) o->par = (int)(best - G.o);
    }
    /* floors: sub 0-3 = handles 2196 / 2197 / 2298 / 2299 */
    EACH(OT_SB_BFLOOR, o) { static const char *const F[4] = { "sbq_f0", "sbq_f1", "sbq_f2", "sbq_f3" }; set_s(o, F[o->par & 3]); }
    /* SB4: chain masks A = y, B = (index mod 2) -300 (10146); each wall rides the mask at its index */
    { int km[2] = { 0 }; EACH(OT_SB_CHAINMASK, o) { set_s(o, o->par ? "sbw_mdown" : "sbw_mup"); o->av[0] = o->y; o->av[1] = (km[o->par & 1]++ % 2) * -300.; } }
    { int kw[2] = { 0 };
      EACH(OT_SB_CHAINWALL, o) {
          set_s(o, o->par ? "sbw_down" : "sbw_up"); o->par = o->par & 1; int want = kw[o->par]++, m = 0; int sub = o->par; o->par = -1;
          EACH(OT_SB_CHAINMASK, q) if ((q->par & 1) == sub && m++ == want) o->par = (int)(q - G.o);
      } }
    EACH(OT_SB_CRUSHER, o) { set_s(o, "sbk_plat"); o->av[0] = o->y; }                 /* A = the bottom (INFERRED: the placement) */
    EACH(OT_SB_CRUSHCHAIN, o) { set_s(o, "sbk_chain"); q = pair(OT_SB_CRUSHER, o, 0, 0, 64); o->par = q ? (int)(q - G.o) : -1; }
    EACH(OT_SB_MBLOCK, o) set_s(o, "sbk_block");
    /* bars: ctor F 1 G 168 (y = J + 168 cos E); 10147: E = index 36, masks flags 0x41, bars 0x40 */
    n = N(OT_SB_BAR); k = 0;
    EACH(OT_SB_BAR, o) { set_s(o, "sbb_bar"); path_init(o, 0, (n - 1 - k++) * 36., 0, 1, 0, 168, 0, 0); o->fl |= 0x40; }
    n = N(OT_SB_BARMASK); k = 0;
    EACH(OT_SB_BARMASK, o) { set_s(o, "sbb_mask"); path_init(o, 0, (n - 1 - k++) * 36., 0, 1, 0, 168, 0, 0); o->fl |= 0x41; }
    /* SB3 cups (9938: A = index 180), their start points ride them */
    n = N(OT_SB_SPINCUP); k = 0;
    EACH(OT_SB_SPINCUP, o) { set_s(o, "sbs_cup"); o->av[0] = ((n - 1 - k++) % 2) * 180.; o->par = -1; }
    EACH(OT_SB_CUPSTART, o) {
        set_s(o, "sbs_start"); obj_t *best = NULL; float bd = 1e9f;
        EACH(OT_SB_SPINCUP, q) if (q->par < 0) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < bd) { bd = d; best = q; } }
        if (best) best->par = (int)(o - G.o);
    }
    EACH(OT_SB_CUPLAND, o) set_s(o, "sbs_land");
    EACH(OT_SB_PBLOCK4, o) set_s(o, "sbq_block");
    n = N(OT_SB_DOOR); k = 0;                        /* 9216 (start): door A = index (newest first), the pads' C likewise (sb_flow.c) */
    EACH(OT_SB_DOOR, o) { set_s(o, "door");   /* the shared 'door' row: the placed handle's image (1098) */ o->av[0] = n - 1 - k++; o->av[2] = 0; }
    EACH(OT_SB_BTUNNEL, o) set_s(o, o->par ? "sbt_tun1" : "sbt_tun0");
}

/* ================================================================ passes */
void sb_gim_pre(player_t *p) {
    if (!S.on) return;
    obj_t *o; double Y = PY(p);
    EACH(OT_SB_PLAT1, o) { o->fl &= ~0x41; if (o->y < Y) o->fl |= 0x41; }            /* 4737 / 4738 (Q13) */
    EACH(OT_SB_PLAT3, o) { o->fl |= 0x41; if (o->y > Y + 8) o->fl &= ~0x41; }        /* 9344 / 9345: solid only under her */
}

int sb_gim_obj(obj_t *o, player_t *p) {
    (void)p;
    switch (o->type) {
    case OT_SB_FANBLADE: o->av[0] = nrm(o->av[0] + 3); o->ang = (float)o->av[0]; return 1;   /* 9346: A = (A + 3) mod 360, angle A */
    case OT_SB_CANBASE: return 0;                                                   /* Q20: combat.c's hits; the defeat runs in cannons() */
    default: return o->type >= RT0 && o->type <= OT_SBG_LAST;                       /* the families run in sb_gim_frame */
    }
}

void sb_gim_frame(player_t *p) {
    if (!S.on) return;
    platforms(p);
    if (N(OT_SB_CANNON) || N(OT_SB_CANBASE) || N(OT_SB_CANBULLET)) cannons();
    if (N(OT_SB_FIRE)) fires(p);
    if (N(OT_SB_FLAMER)) flamers();
    if (N(OT_SB_LIGHTNING)) lightning(p);
    if (N(OT_SB_FAN) || N(OT_SB_FANDEBRIS)) fans(p);
    if (N(OT_SB_SCHAIN) || N(OT_SB_CHAINMASK)) chains(p);
    if (N(OT_SB_CRUSHER) || N(OT_SB_MBLOCK)) crushers(p);
    if (N(OT_SB_BFLOOR) || N(OT_SB_FLOORPIECE)) floors();
    if (N(OT_SB_BAR) || N(OT_SB_BARMASK)) bars(p);
    if (N(OT_SB_SPINCUP) || N(OT_SB_BACKEYE)) cups(p);
}

void sb_gim_draw(int pass) { (void)pass; }
int  sb_gim_q20(int t) { return t == OT_SB_CANBASE; }
