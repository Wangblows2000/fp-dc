/* Fortune Night acts 4-5 gimmicks (session 17): escalators + wall plate, fountains / water platforms, clotheslines (1987),
 * trolleys on the ropes (hanging moving bar, sensor, wheel, stopper), scissor lifts with stilts, LevelMovingPlatform2_1976,
 * turning springs, air balloons, parasols, the keyboard and its secret melody, the A5 disco light / dance, restrooms, the
 * A5 carousel, pole pods, arrow lights, disco balls and the A4 layer-0 loop pieces. Source: build/re/spec_fn_gimmicks_b.md
 * (static RE + six original traces fn4_lift / keys / esc / bar / balloon / rope; section numbers below refer to it).
 * Labels: RE (read from code), RV (original trace), INFERRED (reasoned, marked), UNKNOWN (spec U-number).
 * Shared with acts 1-3 (src/game/fn_gima.c, not here): lantern bridges, teeter-totter, gachapons + hanging idle switches,
 * clothesline2, the chair (M39 rule 8192 for chairs), lamp posts, ferris wheels, Layer0Loop2/3 / Layer1Loop2.
 * Passes: fn_gimb_pre (objects_platforms, before the movement: balloons 8104-8113, spec 0 order), fn_gimb_frame (after the
 * objects and the Relic Maze passes: paths + carry, ropes, trolleys, then the act's events in numeric order), each family
 * gated by its presence (index lists cached at spawn). Object fields: par = family member (FNB_SUB), fl bits >= 0x1000 are
 * this file's flags (bit 0 keeps the shared meaning "ignored by the sensors"). */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FT0 OT_FNB_FIRST
#define NRT (OT_FNB_LAST - OT_FNB_FIRST + 2)          /* + the rope stop (OT_FN_ROPEEND, a type of the acts 1-3 block) */
#define LMAX 240                                      /* the A5 carousel places 228 pieces */
#define TIX(t) ((int)(t) == (int)OT_FN_ROPEEND ? NRT - 1 : (int)(t) - FT0)
static short LST[NRT][LMAX]; static short NLS[NRT];
#define N(T) NLS[TIX(T)]
#define EACH(T, OB) for (int _k = 0; _k < NLS[TIX(T)]; _k++) if (((OB) = &G.o[LST[TIX(T)][_k]])->alive && (OB)->type == (T))
#define F1 0x1000
#define F2 0x2000
#define F3 0x4000
#define DEG 57.29577951308232
#define FNB_ROLLER OT_FN_ROLLER                       /* EnemyRoller_1934 (fn_enemy.c): it plays the keys too (8208) */

static struct {
    int any, plate, plate0;                           /* a type of this block is placed; the escalator wall plate (slot), its last flag 0 */
    int rope; double ropeB;                           /* M41 on a rope (8132), the shimmy B (8130 / 8131) applied by the next frame */
    double spin; int spin_bar;                        /* OTHERS.Z while a launched trolley spins the sprite (8142) */
    int lift_latch;                                   /* 8169 condition on the previous frame (INFERRED edge trigger, U2b) */
    int dl;                                           /* the DiscoLight slot */
    int melody;                                       /* 8214 / 8215 done this act */
    int room, behind;                                 /* the restroom slot taken by M39 (-1), skins behind its door (8196-8205) */
    int pod;                                          /* the pod door carrying the player (-1) */
    int idle_t;                                       /* frames in the idle pose (8306-8308 "frame > 10", INFERRED) */
    uint32_t car_t;                                   /* carousel frames (8347: B = B0 - t) */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / DEG); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / DEG); }
static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0) { if (o->an.b != &G.ens || o->an.anim != a) anim_set(&o->an, &G.ens, a); } }
static void set_sk(obj_t *o, const char *fmt, int k) { char nm[20]; snprintf(nm, sizeof nm, fmt, k); set_s(o, nm); }
static void restart_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static int is_s(const obj_t *o, const char *nm) { return o->an.b == &G.ens && o->an.anim == ens_anim(nm); }
static void force_frame(obj_t *o, int k) { if (!o->an.b) return; const anim_t *a = &o->an.b->anims[o->an.anim]; if (k >= a->count) k = a->count - 1; if (k < 0) k = 0; o->an.frame = k; o->an.acc = 0; }
static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static int Mst(const player_t *p) { return player_orig_m(p); }
static double pB(const player_t *p) { return p->ground ? p->gsp : p->vx; }
static void setB(player_t *p, double b) { if (p->ground) p->gsp = b; else p->vx = b; }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int hurt(const player_t *p) { return p->state == ST_HURT || dead(p); }   /* 8 <= M <= 10 */
static obj_t *nth(int T, int k) { obj_t *o; int n = 0; EACH(T, o) if (n++ == k) return o; return NULL; }
/* the current frame's action point, rotated with the object (Chowdren get_action_x / y) */
static void act_pt(const obj_t *o, double *x, double *y) {
    *x = o->x; *y = o->y;
    if (!o->an.b || !o->an.b->act) return;
    const frame_t *f = anim_frame(&o->an); int fi = (int)(f - o->an.b->frames);
    double dx = o->an.b->act[2 * fi] - f->hx, dy = o->an.b->act[2 * fi + 1] - f->hy, a = o->ang;
    *x = floorf(o->x) + dx * c_cos(a) + dy * c_sin(a); *y = floorf(o->y) - dx * c_sin(a) + dy * c_cos(a);
}
/* a placed player sensor overlapping the object (box objects: the image rect, others: the rotated mask); flag 0 is ignored
 * (check_overlap) */
static int sens_hit(int s, const obj_t *o) {
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    if ((objdef(o->type)->flags & OD_BOXCOL) && o->ang == 0) {
        const frame_t *f = anim_frame(&o->an);
        int bx = (int)floorf(o->x) - f->hx, by = (int)floorf(o->y) - f->hy;
        if (ox > bx + f->w || ox + 32 < bx || oy >= by + f->h || oy + h <= by) return 0;
        for (int j = 0; j < h; j++) { int y = oy + j; if (b[j] < a[j] || y < by || y >= by + f->h) continue; if (ox + b[j] >= bx && ox + a[j] < bx + f->w) return 1; }
        return 0;
    }
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int sens_on_bg(int s) {                        /* a sensor overlapping a backdrop (solid or one-way) */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) {
        if (b[j] < a[j]) continue;
        if (px_span_solid(&G.lvl, ox + a[j], ox + b[j], oy + j)) return 1;
        for (int x = ox + a[j]; x <= ox + b[j]; x++) if (px_platform(&G.lvl, x, oy + j)) return 1;
    }
    return 0;
}
static int disc_hit_dy(const player_t *p, const obj_t *o, int dy) {   /* PLAYER (Player_326, the 27-px disc at (int X, int Y + dy)) */
    static signed char x0[27], x1[27]; static int init;
    if (!init) { init = 1; for (int j = 0; j < 27; j++) { double d = 13.25 * 13.25 - (j - 13) * (j - 13); int w = d < 0 ? -1 : (int)sqrt(d); x0[j] = (signed char)(13 - w); x1[j] = (signed char)(13 + w); } }
    if (!o->an.b) return 0;
    return obj_rows_hit(o, (int)PX(p) - 13, (int)PY(p) + dy - 13, x0, x1, 27);
}
static int disc_hit(const player_t *p, const obj_t *o) { return disc_hit_dy(p, o, 0); }
static int skin_hit(const player_t *p, const obj_t *o) {   /* the skin's box: PlayerCharacterSensor_375's rectangle (INFERRED stand-in) */
    int X = (int)PX(p), Y = (int)PY(p);
    return o->an.b && obj_rect_hit(o, X - 10, Y - 34, X + 11, Y + 20);
}
static obj_t *spawn(int type, const char *anim, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    if (anim) { int a = ens_anim(anim); if (a >= 0) anim_set(&o->an, &G.ens, a); else { a = bank_anim(&G.objs, anim); if (a >= 0) anim_set(&o->an, &G.objs, a); } }
    if (!o->an.b && type == OT_EXPLODE) { o->alive = 0; return NULL; }
    int t = TIX(type);
    if (type >= FT0 && type <= OT_FNB_LAST && NLS[t] < LMAX) LST[t][NLS[t]++] = (short)(o - G.o);
    else if (type >= FT0 && type <= OT_FNB_LAST) { for (int i = 0; i < LMAX; i++) if (!G.o[LST[t][i]].alive || G.o[LST[t][i]].type != type) { LST[t][i] = (short)(o - G.o); break; } }
    return o;
}
static void fx(float x, float y, const char *anim) { obj_t *e = spawn(OT_EXPLODE, anim, x, y, 3); if (e) anim_restart(&e->an); }   /* run-time effects: in front (spec 25) */

/* ================================================================ generic path platforms (3283-3289) + carry (4061) + dip (Q12)
 * av[0] A, av[1] E, av[2] B, av[3] F, av[4] C, av[5] G, kx I, ky J, c: bit 0 D (sin for x), bit 1 H (sin for y);
 * av[6] / av[7] last M / N, vx / vy this frame's O / P; fy = the dip S (3270-3272 / 3303-3307), fl 4 = stood on */
static void path_init(obj_t *o, double A, double E, double B, double F, double C, double Gm, int D, int H) {
    o->kx = o->x; o->ky = o->y; o->av[0] = A; o->av[1] = E; o->av[2] = B; o->av[3] = F; o->av[4] = C; o->av[5] = Gm; o->c = D | (H << 1);
    o->av[6] = o->kx + C * ((o->c & 1) ? c_sin(A) : c_cos(A)); o->av[7] = o->ky + Gm * ((o->c & 2) ? c_sin(E) : c_cos(E));
    o->x = (float)floor(o->av[6]); o->y = (float)floor(o->av[7]); o->vx = o->vy = 0; o->fy = 0;
}
static void path_step(obj_t *o) {                    /* 3283-3289, 2989 */
    o->av[0] += o->av[2]; o->av[1] += o->av[3];
    double M = o->kx + o->av[4] * ((o->c & 1) ? c_sin(o->av[0]) : c_cos(o->av[0]));
    double Nn = o->ky + o->av[5] * ((o->c & 2) ? c_sin(o->av[1]) : c_cos(o->av[1]));
    o->vx = (float)(floor(M) - floor(o->av[6])); o->vy = (float)(floor(Nn) - floor(o->av[7]));
    o->av[6] = M; o->av[7] = Nn; o->x = (float)floor(M); o->y = (float)floor(Nn);
}
static void carry(player_t *p) {                     /* 4061: G >= 0, S_BB 1 px lower on a q13 platform with flag 6 off */
    static const int Q13[] = { OT_FN_LIFT, OT_FN_MPLAT2 };
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
static void paths(player_t *p) {
    obj_t *o;
    EACH(OT_FN_MPLAT2, o) { o->y -= floorf(o->fy); if (!(o->fl & 4)) o->fy -= o->fy * 0.2f; o->fl &= ~4; }   /* 3270-3272 (the path step then sets y = N) */
    EACH(OT_FN_LIFT, o) path_step(o);
    EACH(OT_FN_MPLAT2, o) path_step(o);
    carry(p);
}
static void dip(player_t *p) {                       /* 3303 / 3304 / 3307 (Q12: LevelMovingPlatform2_1976) */
    obj_t *o;
    EACH(OT_FN_MPLAT2, o) {
        if (p->ground && !dead(p) && mv_sensor_obj(p->x, MY(p), p->N, SENSOR_BOTTOMBIG, OT_FN_MPLAT2) == (int)(o - G.o)) o->fl |= 4;
        if (o->fl & 4) o->fy += (9 - o->fy) * 0.2f;
        o->y += floorf(o->fy);                                                      /* the carry delta (P) excludes the dip */
    }
}

/* ================================================================ spawn */
static const char *const LOOPN = "fnloopb_%d", *const STILTN = "stilt%d";
void fn_gimb_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS);
    S.plate = S.dl = S.room = S.pod = S.spin_bar = -1;
    if (!objects_have_from(FT0) && !objects_have(OT_FN_ROPEEND)) return;
    /* family member index and instance layer: the k-th object of a type is the k-th placement of that type (objects_spawn
     * keeps the order); the importer writes sub | layer << 8 in the lvobj anim field */
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !((o->type >= FT0 && o->type <= OT_FNB_LAST) || o->type == OT_FN_ROPEEND)) continue;
        int t = TIX(o->type), n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = o->type == OT_FN_ROPEEND ? 0 : (a & 0xff); o->layer = o->type == OT_FN_ROPEEND ? 2 : (a >> 8) & 0xff;
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = 0; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) || o->type == OT_FN_ROPEEND;
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        if (NLS[t] < LMAX) LST[t][NLS[t]++] = (short)i;
        S.any = 1;
    }
    if (!S.any) return;
    obj_t *o, *q;
    int k;
    EACH(OT_FN_ESCFRONT, o) set_sk(o, "escf%d", o->par & 1);
    EACH(OT_FN_ESCSTEP, o) { set_sk(o, "escs%d", o->par & 1); o->av[0] = (o->par & 1) ? 1 : -1; }   /* ctor D: Right -1, Left +1 (RE) */
    EACH(OT_FN_FOUNTAIN, o) set_sk(o, "fount%d", o->par & 1);
    EACH(OT_FN_WATERPLAT, o) set_s(o, "wplat");
    EACH(OT_FN_ROPE, o) set_s(o, "rope");
    EACH(OT_FN_ROPEEND, o) set_s(o, "fnclstop");
    k = 0; EACH(OT_FN_TBAR, o) { set_s(o, "tbar"); o->av[0] = o->av[4] = o->x; o->tm = k++; }   /* start (A4 inline / A5 8319): A = F = x */
    k = 0; EACH(OT_FN_TSENS, o) { set_s(o, "tsens"); o->tm = k++; }
    k = 0; EACH(OT_FN_TWHEEL, o) { set_s(o, "twheel"); o->tm = k++; }
    EACH(OT_FN_TSTOP, o) set_s(o, "tstop");
    k = 0; EACH(OT_FN_LIFT, o) { set_s(o, "lift"); path_init(o, 0, 0, 0, 0, 0, -224, 0, 1); o->tm = k++; }   /* ctor G -224 H 1: y = J - 224 sin E */
    { int ks[4] = { 0 }; EACH(OT_FN_STILT, o) { set_sk(o, STILTN, o->par & 3); o->kx = o->x; o->tm = ks[o->par & 3]++; o->av[0] = (o->par == 0 || o->par == 3) ? 8 : -8; o->ang = (float)o->av[0]; } }   /* ctor A 8 / -8 / -8 / 8; 8321: B = x */
    EACH(OT_FN_MPLAT2, o) { set_s(o, "mplat2f"); path_init(o, 0, 0, 0, 1, 0, 8, 0, 0); }   /* ctor F 1 G 8 H 0: y = J + 8 cos E (spec 7) */
    EACH(OT_FN_TSPRING, o) set_s(o, "tspr");
    k = 0; EACH(OT_FN_AIRBALLOON, o) { set_s(o, "balloon"); o->tm = k++; }
    k = 0; EACH(OT_FN_IBALLOON, o) { set_s(o, "iballoon"); o->tm = k++; }
    { int n = N(OT_FN_KEY); k = 0; EACH(OT_FN_KEY, o) { set_s(o, "key"); o->tm = (n - 1 - k++) % 8; } }   /* start: A = spread index mod 8 (newest first; RV fn4_keys: 0..7 left to right) */
    EACH(OT_FN_DISCOLIGHT, o) { S.dl = (int)(o - G.o); o->an.b = NULL; }
    EACH(OT_FN_DISCOBALL, o) set_sk(o, "dball%d", o->par & 1);
    EACH(OT_FN_PARASOL, o) set_s(o, "parasol");
    EACH(OT_FN_RESTROOM, o) { set_sk(o, "rroom%d", o->par & 1); force_frame(o, 0); }
    EACH(OT_FN_ARROW, o) set_s(o, "arrow");
    { int n = N(OT_FN_CAROUSEL); k = 0; EACH(OT_FN_CAROUSEL, o) { set_s(o, "carousel"); o->hidden = 1; o->av[0] = o->x; o->av[1] = ((n - 1 - k++) % 6) * 60; } }   /* 8485: A = x, B = (spread mod 6) 60 */
    EACH(OT_FN_CARPLAT, o) set_s(o, "carplat");
    EACH(OT_FN_POD, o) set_s(o, "pod");
    EACH(OT_FN_PODCASE, o) set_s(o, "podcase");
    EACH(OT_FN_PODDOOR, o) {                                                        /* 8486: C = x, E = x + 32, D = F = y; the body / case it overlaps */
        set_s(o, "poddoor"); force_frame(o, 11);
        o->av[1] = o->x; o->av[3] = o->x + 32; o->av[2] = o->av[4] = o->y; o->tm = o->c = -1;
        EACH(OT_FN_POD, q) if (fabsf(q->x - o->x) < 8 && fabsf(q->y - o->y) < 8) o->tm = (int)(q - G.o);
        EACH(OT_FN_PODCASE, q) if (fabsf(q->x - o->x) < 8 && fabsf(q->y - o->y) < 8) o->c = (int)(q - G.o);
    }
    EACH(OT_FN_LOOP0B, o) set_sk(o, LOOPN, o->par % 3);
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_RM_WPLATE && G.o[i].par == 0) { S.plate = i; break; }   /* SwitchWallPlate_1149 (relic_gimb.c presses it) */
    layers_refresh(&G.p);                                                           /* the A4 loop pieces (OD_LAYER0) take MV.P now */
}

/* ================================================================ 9. air balloons (8104-8113: before the movement block) */
static void drop_balloon(player_t *p) {              /* 8111 / 8112: M = 40, G = 0, every IdleBalloon Q = 0 */
    p->state = ST_NORMAL; p->state_t = 0; p->ground = 0; p->vy = 0; p->vx = 0; p->springing = 1; p->cY = 40; p->m1 = 0; p->hang_obj = -1;
}
void fn_gimb_pre(player_t *p) {
    if (!S.any || !N(OT_FN_AIRBALLOON)) return;
    obj_t *b, *ib;
    EACH(OT_FN_AIRBALLOON, b) {
        double *A = &b->av[0], *B = &b->av[1], *D = &b->av[2];
        if (*B == 0) *B = b->y;                                                     /* 8104: home */
        if (!(b->fl & F1)) { b->y = (float)(*B + 8 * c_sin(*D)); *D = fmod(*D + 1.5, 360); }   /* 8105: +-8 px, 240 frames (RV fn4_balloon) */
        if (!(b->fl & F1) && p->state == ST_HANG && disc_hit_dy(p, b, -20)) { b->fl |= F1; *A = -1; }   /* 8106-8108: PLAYER 20 px up on the balloon, M = 41 */
        if (b->fl & F1) *A = fmax(*A - 0.05, -4);                                   /* 8109 */
        b->y = (float)fmax(floor(b->y + *A), -150);                                 /* 8110 */
        EACH(OT_FN_IBALLOON, ib) if (ib->tm == b->tm) { double x, y; act_pt(b, &x, &y); ib->x = (float)floor(x); ib->y = (float)floor(y); }   /* + its basket handle */
        if (!obj_near_view(b, 100) && (*B > G.camy + 340 || b->x < G.camx - 100 || b->x > G.camx + 526)) { *A = 0; b->y = (float)*B; b->fl &= ~F1;   /* 8113 */
            EACH(OT_FN_IBALLOON, ib) if (ib->tm == b->tm) { double x, y; act_pt(b, &x, &y); ib->x = (float)floor(x); ib->y = (float)floor(y); } }
    }
    if (p->state == ST_HANG) EACH(OT_FN_IBALLOON, ib) {                           /* Q = 1: she holds this basket */
        if (p->hang_obj != (int)(ib - G.o)) continue;
        if (obj_overlaps_bg(ib) || ib->y < 50) { drop_balloon(p); break; }          /* 8111 (a ceiling / wall) / 8112 (top of the act) */
    }
}

/* ================================================================ 4. clotheslines (8128-8133) */
static int rope_end(int s) { obj_t *e; EACH(OT_FN_ROPEEND, e) if (sens_hit(s, e)) return 1; return 0; }
static int under_rope(const player_t *p) {           /* SKIN overlaps a rope piece */
    obj_t *o; int X = (int)PX(p);
    EACH(OT_FN_ROPE, o) if (fabsf(o->y - (float)p->hang_y) < 2 && obj_rect_hit(o, X - 8, (int)o->y - 2, X + 8, (int)o->y + 12)) return 1;
    return 0;
}
static void ropes(player_t *p) {
    obj_t *o, *g = NULL;
    if (S.rope && p->state == ST_HANG && S.ropeB != 0) { p->hang_x += S.ropeB; p->x = p->hang_x; player_sync_sensors(p); }   /* the movement block's X += B */
    S.ropeB = 0;
    int M = Mst(p);
    if (p->vy > 0 && M != 41 && !hurt(p) && !(p->held & BTN_DOWN))                 /* 8128: S_TB into a rope while falling */
        EACH(OT_FN_ROPE, o) if (sens_hit(SENS_TOPBIG, o)) o->fl |= F1;
    EACH(OT_FN_ROPE, o) if (o->fl & F1) { g = o; break; }
    if (g && p->vy > 0 && M != 41 && !hurt(p)) {                                   /* 8132: M 41, MV.Y = rope y + 32 (sound 33: player_hang) */
        player_hang(p, p->x, g->y); p->hang_obj = -1; p->rot = 0; S.rope = 1;
    }
    if (p->state != ST_HANG) { EACH(OT_FN_ROPE, o) o->fl &= ~F1; S.rope = 0; }      /* 8133 */
    if (S.rope && p->state == ST_HANG && p->vy >= 0 && under_rope(p)) {            /* 8130 / 8131: shimmy 3 px a frame, stopped by the end sensors */
        if ((p->held & BTN_RIGHT) && !rope_end(SENS_RIGHTBIG)) S.ropeB = 3;
        else if ((p->held & BTN_LEFT) && !rope_end(SENS_LEFTBIG)) S.ropeB = -3;
    }
}

/* ================================================================ 5. trolleys (8134-8150) */
/* bar: av[0] A x, av[1] B speed, st C state, av[2] D angle, av[3] E spin, av[4] F home x, c G launch timer, F1 = flag 1,
 * F2 = the 8139 whoosh latch; sensor st = its D (the bar's C) */
static void trolleys(player_t *p) {
    obj_t *b, *s, *w, *t;
    EACH(OT_FN_TSENS, s) if (!dead(p)) obj_hang_try(s, p);                         /* 4065 / 4066: the generic Q23 grab */
    EACH(OT_FN_TBAR, b) {
        double *A = &b->av[0], *B = &b->av[1], *D = &b->av[2], *E = &b->av[3], F = b->av[4];
        w = nth(OT_FN_TWHEEL, b->tm); s = nth(OT_FN_TSENS, b->tm);
        if (b->st == 0) b->fl &= ~F1;                                               /* 8134 */
        if (b->st == 0 && p->state == ST_HANG && skin_hit(p, b)) { b->st = 1; G.input_lock = 1; p->face = 1; }   /* 8135: CTRL.M = 1, MV.Q = 1 */
        if (b->st == 1) { *A += *B; b->x = (float)(int)*A; *B = fmin(*B + 0.2, 15); *D = -2 * *B; if (w) w->ang = (float)fmod(w->ang - 2 * *B, 360); }   /* 8136 */
        if (b->st == 1) EACH(OT_FN_TSTOP, t) if (t->x <= *A && obj_obj_hit(b, t)) { *A = t->x; b->x = t->x; *E = 12; b->st = 2; G.input_lock = 0; break; }   /* 8137 */
        if (b->st == 2) {                                                           /* 8138: the swing, 12 degrees a frame */
            *D += *E; p->face = 1; set_s(b, "tbar_sw");
            if (p->state == ST_HANG) player_pose(p, 40, 7, -1);
            double x, y; act_pt(b, &x, &y); fx((float)x, (float)y, "ringsparkle");
            double m = fmod(*D, 360); if (m < 0) m += 360;
            if (m < 30 && !(b->fl & F2)) { b->fl |= F2; snd_play_orig(216, 220, 128); }   /* 8139: one whoosh per turn */
            if (m >= 30) b->fl &= ~F2;
            if (hurt(p)) { b->fl |= F1; p->hang_obj = -1; }                         /* 8140 */
            if (!(b->fl & F1) && p->state != ST_HANG) {                             /* 8141: launched along the swing, B = 1.5 E cos D */
                double v = 1.5 * *E; setB(p, v * c_cos(*D)); p->vy = -v * c_sin(*D); p->ground = 0; p->vx = v * c_cos(*D);
                b->fl |= F1; b->c = 30; S.spin = p->rot; S.spin_bar = (int)(b - G.o);
            }
            if (p->state != ST_HANG) { b->st = 3; *D = fmod(*D, 360); }             /* 8143 */
        }
        if ((b->fl & F1) && b->c > 0) { b->c--; if (S.spin_bar == (int)(b - G.o) && !dead(p)) { S.spin += 12; p->rot = (float)fmod(S.spin, 360); } }   /* 8142 */
        if (b->st == 3) { *D += *E; if (*D >= 360) { *B = 0; b->st = 4; } }        /* 8144 / 8145 */
        if (b->st == 4) {                                                           /* 8146 / 8147: back home at up to 12 px a frame */
            *A += *B; b->x = (float)(int)fmax(*A, F); *B = fmax(*B - 0.2, -12); *D = -2 * *B; if (w) w->ang = (float)fmod(w->ang - 2 * *B, 360);
            if (*A <= F) { b->st = 0; *D = 0; *B = 0; set_s(b, "tbar"); b->fl &= ~F1; }
        }
        b->ang = (float)*D;                                                         /* 8148 */
        if (w) { w->x = b->x; w->y = b->y; }
        if (s) { double x, y; act_pt(b, &x, &y); s->x = (float)x; s->y = (float)y; s->ang = b->ang; s->st = b->st;
                 if (p->state != ST_HANG && s->st > 2) { s->x = -100; s->y = 0; } }  /* 8149 */
    }
    EACH(OT_FN_TSENS, s) if (s->st > 0 && s->st < 3 && !dead(p)) {                 /* 8150: she orbits 32 px from the grip */
        double a = s->ang;
        p->state = ST_HANG; p->hang_obj = (int)(s - G.o); p->hang_px = s->x; p->hang_py = s->y;
        p->hang_x = s->x + 32 * c_sin(a); p->hang_y = s->y + 32 * c_cos(a) - 32;
        p->x = p->hang_x; p->y = p->hang_y + 32 + ORIG_REF_DY; p->rot = (float)fmod(a + 720, 360); p->gsp = 0; p->ground = 0;
        player_sync_sensors(p);
    }
}

/* ================================================================ 2. escalators (8151-8157) */
static void escalators(player_t *p) {
    obj_t *o, *hit = NULL;
    int fr = (int)((unsigned)G.frame & 7);                                         /* GV[3] mod 8 (the port's frame counter, INFERRED phase) */
    EACH(OT_FN_ESCFRONT, o) force_frame(o, fr);                                     /* 8151 */
    EACH(OT_FN_ESCSTEP, o) o->fl &= ~1;
    if (p->ground && sens_on_bg(SENS_BOTTOMBIG)) EACH(OT_FN_ESCSTEP, o) o->fl |= 1;   /* 8152: on backdrop ground the steps let her through */
    if (p->ground && !dead(p)) {
        double x0 = p->x, B = pB(p);
        for (int side = 0; side < 2; side++) {                                     /* 8153 / 8154 (Right), 8155 / 8156 (Left) */
            hit = NULL; obj_t *dir1 = NULL;
            EACH(OT_FN_ESCSTEP, o) if ((o->par & 1) == side && sens_hit(SENS_BOTTOMBIG, o)) { if (!hit) hit = o; if (o->av[0] == (side ? -1 : 1) && !dir1) dir1 = o; }
            if (hit) p->x += hit->av[0];
            if (dir1 && (side ? B < 0 : B > 0)) p->x += B;                           /* walking with the belt doubles the walk */
        }
        if (p->x != x0) player_sync_sensors(p);
    }
    obj_t *pl = slot(S.plate);
    int on = pl && pl->type == OT_RM_WPLATE && (pl->fl & 1);
    if (on && !S.plate0) {                                                          /* 8157: every belt reverses (the click 49: relic_gimb.c) */
        EACH(OT_FN_ESCFRONT, o) { o->dir ^= 16; set_sk(o, o->dir ? "escf%dr" : "escf%d", o->par & 1); force_frame(o, fr); }
        EACH(OT_FN_ESCSTEP, o) o->av[0] = -o->av[0];
    }
    S.plate0 = on;
}

/* ================================================================ 3. fountains, arrow lights, water platforms (8163-8166) */
static void waters(player_t *p) {
    obj_t *o;
    EACH(OT_FN_FOUNTAIN, o) force_frame(o, (int)(((unsigned)G.frame / 2) % 16));   /* 8163 */
    EACH(OT_FN_ARROW, o) force_frame(o, (int)(((unsigned)G.frame / 4) % 12));
    if (!N(OT_FN_WATERPLAT)) return;
    if (!p->ground) EACH(OT_FN_WATERPLAT, o) o->fl &= ~F1;                         /* 8164 */
    EACH(OT_FN_WATERPLAT, o) {
        if (dead(p) || !sens_hit(SENS_BOTTOMBIG, o)) continue;
        if (p->ground && !(o->fl & F1)) {                                           /* 8165: the landing splash */
            o->fl |= F1; fx((float)(PX(p) - 6), (float)(PY(p) + 12), "waterhit"); fx((float)(PX(p) + 6), (float)(PY(p) + 12), "waterhit"); snd_play_orig(208, 220, 128);
        }
        if ((o->fl & F1) && G.frame % 5 == 0) {                                    /* 8166: wading splashes */
            int sx, sy; mv_sensor_pos(SENS_BOTTOMBIG, &sx, &sy); fx((float)(sx - 7 + cross_rand_raw(15)), (float)sy, "waterhit");
        }
    }
}

/* ================================================================ 6. scissor lifts (8169-8177; RV fn4_lift) */
static void lifts(player_t *p) {
    obj_t *o, *s;
    if (!N(OT_FN_LIFT)) return;
    /* 8169 (OR): standing on a lift at rest (S_BB and both edge sensors on it, above it, L = 1), or pinned under a sinking deck.
     * RV f57: every lift F = 1.5, V += 1, the stood-on one V = 0. The original did not fire again on f58-f91 while she stood
     * still on it, so the port fires on the condition's rising edge (INFERRED); its second firing at f92 is U2b. */
    int cond = 0, mtch[LMAX] = { 0 }, n = 0;
    if (!dead(p)) EACH(OT_FN_LIFT, o) {
        int m = 0;
        if (o->st == 0 && o->y > MY(p) && p->ground && sens_hit(SENS_BOTTOMBIG, o) && sens_hit(12, o) && sens_hit(13, o)) m = 1;   /* S_LEDGE / S_REDGE */
        if (o->st > 1 && mv_z(SENSOR_BOTTOMBIG) && sens_hit(SENS_TOPBIG, o)) m = 1;
        mtch[n++] = m; cond |= m;
    }
    if (cond && !S.lift_latch) {
        EACH(OT_FN_LIFT, o) { o->av[3] = 1.5; o->st += 1; }                         /* loop rise */
        n = 0; EACH(OT_FN_LIFT, o) if (mtch[n++]) o->st = 0;
    }
    S.lift_latch = cond;
    EACH(OT_FN_LIFT, o) if (o->st == 1 && o->av[1] >= 90) { o->av[3] = 0; o->st += 1; }   /* 8170: at the top */
    int some2 = 0; EACH(OT_FN_LIFT, o) if (o->st == 2) some2 = 1;
    if (some2) EACH(OT_FN_LIFT, o) { o->av[3] = 0.5; o->st += 1; }                  /* 8171: loop sink */
    EACH(OT_FN_LIFT, o) {
        if (o->st > 2 && o->av[1] >= 180) { o->av[3] = 0; o->st = 0; o->av[1] = 0; }   /* 8172: back down */
        if (o->av[1] > 180) o->av[1] -= 180;                                        /* 8173 */
        o->fl &= ~0x41; if (o->y < PY(p)) o->fl |= 0x41;                            /* 8174 / 8175: passable from below, no carry */
    }
    /* 8176: the scissor arms; Stilt1 / 2 slide (x = B + C), 3 / 4 hang from 1 / 2's action points; the index-paired lift's E */
    EACH(OT_FN_STILT, s) s->ang = (float)s->av[0];
    EACH(OT_FN_STILT, s) if (s->par < 2) s->x = (float)(s->kx + s->av[1]);
    EACH(OT_FN_STILT, s) if (s->par >= 2) {
        obj_t *a = NULL, *q; int k = 0; EACH(OT_FN_STILT, q) if (q->par == s->par - 2 && k++ == s->tm) { a = q; break; }
        if (a) { double x, y; act_pt(a, &x, &y); s->x = (float)x; s->y = (float)y; }
    }
    EACH(OT_FN_STILT, s) {
        obj_t *l = nth(OT_FN_LIFT, s->tm); if (!l) continue;
        double sn = c_sin(l->av[1]);
        if (s->par == 0) { s->av[0] = 70 * sn; s->av[1] = 40 * sn; }
        else if (s->par == 1) { s->av[0] = -70 * sn; s->av[1] = -40 * sn; }
        else s->av[0] = (s->par == 2 ? -70 : 70) * sn;
    }
}

/* ================================================================ 8 / 12. turning springs, parasols (8178-8183, 8099-8101) */
static void spring_launch(player_t *p, double G_, int snd) {   /* M 40, L 0, G set (the B kept) */
    if (p->ground) p->vx = p->gsp;
    player_launch(p, 0, -G_, snd);
}
static void springs(player_t *p) {
    obj_t *o;
    unsigned g = (unsigned)G.frame % 120;
    EACH(OT_FN_TSPRING, o) {
        if ((o->st == 0 || o->st == 2) && is_s(o, "tspr") && g == 20) set_s(o, "tspr_w");   /* 8178: warning blink */
        if ((o->st == 0 || o->st == 2) && g == 50) o->st += 1;                      /* 8179 / 8180 */
        if (o->st == 1 || o->st == 3) {                                             /* 8181 / 8182: 180 degrees in 30 frames, not solid */
            o->av[0] += 3; o->ang = (float)(o->st == 1 ? 180 + 180 * c_cos(o->av[0]) : 180 * c_cos(o->av[0]));
            o->fl |= 1; set_s(o, "tspr_t"); o->dir = o->st == 1 ? 8 : 24;
        }
        if (o->av[0] >= 90) { o->av[0] = 0; o->st = (o->st + 1) % 4; o->fl &= ~1; restart_s(o, "tspr_s"); }   /* 8183 */
        if (is_s(o, "tspr_s") && o->an.done) set_s(o, "tspr");                      /* the settle ends on anim 0 (INFERRED: the stopped animation) */
        if (dead(p) || (o->fl & 1)) continue;
        int top = o->st == 0 && p->vy <= 0 && sens_hit(SENS_TOPBIG, o), bot = o->st == 2 && p->vy >= 0 && sens_hit(SENS_BOTTOMBIG, o);
        if (top || bot) {                                                           /* 8100 (face down: G 11) / 8101 (face up: G -11) */
            spring_launch(p, top ? 11 : -11, 5);
            char nm[12]; snprintf(nm, sizeof nm, "sbeam%d", o->dir);
            float by = o->y + (top ? 8 : -24);
            fx(o->x - 16, by, nm); fx(o->x - 48, by, nm); fx(o->x + 16, by, nm);
            set_s(o, "tspr"); anim_restart(&o->an);
        }
    }
    EACH(OT_FN_PARASOL, o) {                                                        /* 8099: a soft spring, G -7 */
        if (is_s(o, "parasol_b") && o->an.done) set_s(o, "parasol");
        if (dead(p) || p->vy < 0 || !sens_hit(SENS_BOTTOMBIG, o)) continue;
        spring_launch(p, -7, 212); restart_s(o, "parasol_b");
    }
}

/* ================================================================ 10. keyboard and the secret melody (8206-8215) */
static void reward_icon(player_t *p, int frame) {    /* MonitorIcon_363 at PLAYER (x - 16, y - 16): rises, then the reward (as 2630-2658) */
    obj_t *o = spawn(OT_FN_MICON, "monicon", (float)(PX(p) - 16), (float)(PY(p) - 16), 3);
    if (o) { force_frame(o, frame); o->st = frame; }
}
static void keys(player_t *p) {
    obj_t *o, *r, *dl = slot(S.dl);
    if (!N(OT_FN_KEY)) return;
    int on[LMAX], non = 0, roll = 0, nr = 0;
    obj_t *rl[8];                                                                   /* the rollers near the keys (objects far off-screen sleep anyway) */
    if (objects_alive_type(FNB_ROLLER)) { EACH(OT_FN_KEY, o) if (fabsf(o->x - (G.camx + SCREEN_W / 2)) < SCREEN_W * 1.5f) { roll = 1; break; } }
    if (roll) for (int i = 0; i < G.nobjs && nr < 8; i++) { r = &G.o[i]; if (r->alive && r->type == FNB_ROLLER && fabsf(r->x - (G.camx + SCREEN_W / 2)) < SCREEN_W * 2) rl[nr++] = r; }
    EACH(OT_FN_KEY, o) o->fl &= ~F1;                                                /* 8206 */
    if (!dead(p)) EACH(OT_FN_KEY, o) if (sens_hit(SENS_BOTTOMBIG, o)) on[non++] = (int)(o - G.o);   /* 8207: one random key under her */
    if (non) { o = &G.o[on[cross_rand(non)]]; o->fl |= F1; o->av[1] = 0; }
    for (int i = 0; i < nr; i++) {                                                  /* 8208: a roller on the keys */
        r = rl[i];
        non = 0; EACH(OT_FN_KEY, o) if (obj_obj_hit(r, o)) on[non++] = (int)(o - G.o);
        if (non) { o = &G.o[on[cross_rand(non)]]; o->fl |= F1; o->av[1] = 0; }
    }
    EACH(OT_FN_KEY, o) {
        if ((o->fl & F1) && !(o->fl & F2)) {                                        /* 8209: pressed: its note, sound 223 + A, DiscoLight C = A + 1 */
            restart_s(o, "key_p"); o->fl |= F2;
            obj_t *nt = spawn(OT_FN_NOTE, "note", o->x + 24, o->y, 3);
            if (nt) { force_frame(nt, cross_rand(3)); nt->av[0] = nt->x; }
            snd_play_orig(223 + o->tm, 220, 128);
            if (dl) dl->av[2] = o->tm + 1;
        }
        if (!(o->fl & F1)) {                                                        /* 8210 / 8211: released after 4 free frames */
            int touch = !dead(p) && sens_hit(SENS_BOTTOMBIG, o);
            for (int i = 0; i < nr && !touch; i++) if (obj_obj_hit(rl[i], o)) touch = 1;
            if (!touch) o->av[1] += 1;
            if (o->av[1] > 3) { set_s(o, "key"); o->fl &= ~F2; o->av[1] = 0; }
        }
    }
    if (!dl) return;
    static const signed char TUNE[7] = { 1, 3, 4, 5, 3, 1, 8 };                     /* 8212: notes 1 3 4 5 3 1 8 (keys A 0 2 3 4 2 0 7) */
    int B = (int)dl->av[1], C = (int)dl->av[2];
    if (B < 7 && C == TUNE[B]) { dl->av[2] = 0; dl->av[1] += 1; }
    if (dl->av[2] > 0) { dl->av[2] = 0; dl->av[1] = 0; }                           /* 8213: a wrong note */
    if (dl->av[1] == 7 && !S.melody) {                                              /* 8214 / 8215: once per act */
        S.melody = 1; reward_icon(p, 8); snd_play_orig(428, 220, 128); reward_icon(p, 1);
        ach_earn(41);                                                               /* records x2 y41: "Secret Melody" (spec_achievements) */
    }
}
static void runtime_objs(player_t *p) {
    obj_t *o;
    EACH(OT_FN_NOTE, o) {
        o->tm++;
        if (o->st == 0) { o->y -= 1; o->x = (float)(o->av[0] + 4 * c_sin(o->tm * 6.)); if (o->tm > 48) o->alive = 0; }   /* the note rises (INFERRED motion) */
        else { o->x -= (float)(0.5 * o->av[0]); if (!o->an.b || o->an.done) o->alive = 0; }                              /* 8305: disco sparkles drift */
    }
    EACH(OT_FN_MICON, o) {                                                          /* rises 32 frames, the reward at 64 (as 2630-2658; U8) */
        force_frame(o, o->st);
        if (++o->tm < 32) o->y -= 1;
        if (o->tm >= 64) {
            if (o->st == 8) { p->lives++; music_play("bgm/m_1up.adp"); G.mus_resume_t = 240; }   /* 1-up (as OT_MON_1UP) */
            else { p->hp = 14; snd_play_orig(4, 220, 128); }                          /* petals refilled (INFERRED reading of the frame-1 reward) */
            o->alive = 0;
        }
    }
}

/* ================================================================ 11. disco light, A5 dance (8298-8313) */
static void disco(player_t *p) {
    obj_t *dl = slot(S.dl), *o;
    if (!dl) return;
    double *A = &dl->av[0];
    if (level_cur()->fnb_disco) {                                                   /* 8298-8303 (A5 only) */
        int vis = 0; EACH(OT_FN_DISCOBALL, o) if (o->par == 0 && obj_near_view(o, 60)) vis = 1;
        if (!vis && *A > 1) *A -= 1;
        else if (!vis && *A == 1) *A = 0;
        else if (vis && *A == 0) *A = 1;
        else if (vis && *A > 0 && *A < 64) *A += 1;
    }
    if (*A > 0) {                                                                   /* 8304 / 8305: sparkles at random points of the view */
        obj_t *s = spawn(OT_FN_NOTE, "ringsparkle", (float)(G.camx + cross_rand_raw(SCREEN_W)), (float)(G.camy + cross_rand_raw(SCREEN_H)), 3);
        if (s) { s->st = 1; s->av[0] = level_cur()->fnb_disco ? cross_rand_raw(5) : 0; anim_restart(&s->an); }
    }
    int still = p->state == ST_NORMAL && p->ground && fabs(p->gsp) < 0.01 && !(p->held & (BTN_LEFT | BTN_RIGHT | BTN_JUMP | BTN_UP | BTN_DOWN)) && Mst(p) == 0;
    S.idle_t = still ? S.idle_t + 1 : 0;
    if (*A >= 64 && still && !p->bike && S.idle_t > 10 && !dead(p)) { p->state = ST_DANCE; p->state_t = 0; }   /* 8306-8308: M 50 (Lilac, Carol on foot, Milla) */
    if (*A >= 64 && p->state == ST_DANCE) ach_earn(20);                             /* 8313: "Disco Fever" */
}

/* ================================================================ 13. restrooms (8192-8205, 8102 / 8103) */
static int door_frame(double A) { int f = (int)(11 * c_sin(A)); return f < 0 ? 0 : f; }
static void restrooms(player_t *p) {
    obj_t *o, *in = slot(S.room);
    if (!N(OT_FN_RESTROOM)) return;
    int M = Mst(p);
    uint32_t held = p->held;
    static uint32_t held0; int up = (held & BTN_UP) && !(held0 & BTN_UP); held0 = held;
    if (M < 4 && p->state == ST_NORMAL && p->ground && !dead(p) && !in) {          /* 8192: idle in front of a door, or Up pressed there */
        int idle = fabs(p->gsp) < 0.01 && !(held & (BTN_LEFT | BTN_RIGHT | BTN_JUMP));
        EACH(OT_FN_RESTROOM, o) if ((idle || (up && fabs(pB(p)) < 2)) && disc_hit(p, o)) { p->state = ST_SIT; p->state_t = 0; S.room = (int)(o - G.o); in = o; break; }
    }
    if (p->state != ST_SIT) { S.room = -1; in = NULL; }
    EACH(OT_FN_RESTROOM, o) {
        double *A = &o->av[0], *B = &o->av[1];
        if (o == in) {
            if (*A <= 0 && disc_hit(p, o)) *A = 10;                                 /* 8193 / 8194: the door opens 10 degrees a frame */
            else if (*A > 0 && *A < 180) *A = fmin(*A + 10, 180);
            if (*A > 90) p->invuln = p->invuln > 10 ? p->invuln : 10;              /* 8195 / 8197: OTHERS.D 2, E 10 (inside, INFERRED reading) */
            if (*A <= 90) S.behind = 1;                                             /* 8196 / 8198: behind the door */
            if (*A > 60 && *A < 90) S.behind = 0;                                   /* 8204 / 8205 */
            if (*A >= 180 && (o->par & 1) == 0) *B += 1;                            /* 8199: the heroines (MV.A 0 / 2 / 3 / 4) in the men's room */
            if (*B == 10) { *B += 1; snd_play_orig((o->par & 1) ? 218 : 217, 220, 128); }   /* 8201 / 8202 */
            if (*B > 30) {                                                          /* 8102 / 8103: thrown out, B = 10 */
                p->state = ST_NORMAL; p->cY = 0; setB(p, 10); p->face = 1; snd_play_orig(5, 220, 128); fx(o->x - 16, o->y, "sbeam0");
                S.room = -1;
            }
        } else { *A = fmax(*A - 10, 0); *B = 0; }                                   /* 8203 */
        force_frame(o, door_frame(*A));
    }
    if (!slot(S.room)) { int near = 0; EACH(OT_FN_RESTROOM, o) if (disc_hit(p, o)) near = 1; if (!near) S.behind = 0; }   /* out of the doorway: drawn normally again */
}

/* ================================================================ 15. carousel (8347 / 8348): drawn by fn_gimb_draw_back */
/* ================================================================ 16. pole pods (8350-8367) */
/* door: st A, av[0] B, av[1] C, av[2] D, av[3] E, av[4] F, av[5] G, av[6] J, av[7] L; H 2, I 3, K 0.15; tm body, c case */
static void pods(player_t *p) {
    obj_t *d, *q;
    if (!N(OT_FN_PODDOOR)) return;
    EACH(OT_FN_PODDOOR, d) {
        double *Bv = &d->av[0], *C = &d->av[1], *D = &d->av[2], *Gv = &d->av[5], *J = &d->av[6], *L = &d->av[7];
        const double E = d->av[3], F = d->av[4], H = 2, K = 0.15;
        if (d->st == 0) *L = -0.7 * pB(p) / PHYS_TOP;                               /* 8350 (MV.C = the top speed 5, INFERRED: U4) */
        if (d->st == 0 && S.pod < 0 && !dead(p) && pB(p) > 0 && p->x >= d->x && disc_hit(p, d)) {   /* 8351: in from the left */
            d->st = 1; *Bv = fabs(pB(p)) * *L; setB(p, 0); *L = 1; *Gv = 0;
            p->state = ST_HELD; p->state_t = 0; S.pod = (int)(d - G.o); snd_play_orig(249, 220, 128);
        }
        if (d->st == 1) *Bv -= K;                                                   /* 8352 / 8353 */
        else if (d->st == 2) *Bv += K;
        if (d->st >= 1) { *Gv += *Bv; *J = fmod(*Gv, 360); if (*J < 0) *J += 360; *C = E - 32 * c_sin(90 + *Gv); *D = F - *Gv / H; }   /* 8354: 32-px spiral, 0.5 px per degree */
        if (d->st == 1 && S.pod == (int)(d - G.o)) {                               /* 8360: she rides inside */
            p->x = *C; p->y = *D + 10 + ORIG_REF_DY; p->ground = 1; p->gsp = p->vx = p->vy = 0; player_sync_sensors(p);
            if (*Gv <= -900) {                                                      /* 8355: 2.5 turns, 450 px down: released with half the pod's speed */
                p->state = ST_NORMAL; p->state_t = 0; setB(p, -0.5 * *Bv); *Bv = 0; d->st = 2; S.pod = -1; player_sync_sensors(p);
            }
        } else if (d->st == 1 && S.pod < 0) { *Bv = 0; d->st = 2; }               /* (she left the pod some other way: it returns, INFERRED) */
        int fr = 11;                                                                /* 8356-8359: the shell's rotation frame */
        if (d->st == 1) fr = (int)(12 - 12 * fmod(*J + 15, 360) / 360);
        else if (d->st == 2) fr = (int)(12 * fmod(*J + 15, 360) / 360);
        if (d->st == 2 && *Gv >= 0) { d->st = 0; fr = 11; }
        force_frame(d, fr);
        d->x = (float)*C; d->y = (float)*D;                                         /* 8363 */
        if ((q = slot(d->tm)) && q->type == OT_FN_POD) {                            /* 8361, 8364 / 8365: the body, not solid while it climbs back */
            q->x = d->x; q->y = d->y;
            if (d->st == 2 && !sens_hit(SENS_BOTTOMBIG, q)) q->fl |= 1; else if (d->st <= 1) q->fl &= ~1;
        }
        if ((q = slot(d->c)) && q->type == OT_FN_PODCASE) { q->x = d->x; q->y = d->y; }   /* 8362 */
    }
    if (p->state != ST_HELD) S.pod = -1;
}

/* ================================================================ per frame (spec 0 order) */
void fn_gimb_frame(player_t *p) {
    if (!S.any) return;
    if (N(OT_FN_LIFT) || N(OT_FN_MPLAT2)) paths(p);   /* 3283-3289, 4061 */
    if (N(OT_FN_IBALLOON)) { obj_t *ib; EACH(OT_FN_IBALLOON, ib) if (!dead(p)) obj_hang_try(ib, p); }   /* 4065 / 4066 on the basket handles (Q23) */
    if (N(OT_FN_ROPE)) ropes(p);                     /* 8128-8133 */
    if (N(OT_FN_TBAR)) trolleys(p);                  /* 8134-8150 */
    if (N(OT_FN_ESCSTEP) || N(OT_FN_ESCFRONT)) escalators(p);   /* 8151-8157 */
    if (N(OT_FN_FOUNTAIN) || N(OT_FN_ARROW) || N(OT_FN_WATERPLAT)) waters(p);   /* 8163-8166 */
    lifts(p);                                        /* 8169-8177 */
    if (N(OT_FN_TSPRING) || N(OT_FN_PARASOL)) springs(p);   /* 8178-8183, spring loop 8099-8101 */
    restrooms(p);                                    /* 8192-8205, 8102 / 8103 */
    keys(p);                                         /* 8206-8215 */
    disco(p);                                        /* 8298-8313 */
    if (N(OT_FN_CAROUSEL)) S.car_t++;                /* 8347 */
    pods(p);                                         /* 8350-8367 */
    if (N(OT_FN_NOTE) || N(OT_FN_MICON)) runtime_objs(p);
    if (N(OT_FN_MPLAT2)) dip(p);                     /* 3303-3307 */
}

/* player.c hook: M39 in a restroom / M50 dance replace the movement block (1 = handled; 0 = M0 this frame) */
int fn_gimb_player(player_t *p, const input_t *in) {
    p->held = in->held;
    if (p->state == ST_DANCE) {
        if ((in->held & (BTN_LEFT | BTN_RIGHT)) || (in->pressed & BTN_JUMP) || !level_cur()->fnb_disco) { p->state = ST_NORMAL; p->cY = 0; return 0; }   /* 8309 / 8310 (B != 0 / jump) */
        p->ground = 1; p->gsp = p->vx = p->vy = 0; player_pose(p, 40, 8, -1); player_sync_sensors(p);
        return 1;
    }
    obj_t *r = slot(S.room);
    if (!r || r->type != OT_FN_RESTROOM) return 0;                                  /* a chair: relic_gimb.c */
    if ((in->held & (BTN_LEFT | BTN_RIGHT)) || (in->pressed & BTN_JUMP)) { p->state = ST_NORMAL; p->cY = 0; S.room = -1; return 0; }   /* 3072: back to M0 */
    p->ground = 1; p->gsp = p->vx = p->vy = 0;
    if (p->x < r->x) { p->x = fmin(p->x + 2, r->x); p->face = 1; player_pose(p, 1, 0, -1); }        /* 3069 / 3070: to the door (Q22, INFERRED as the chair) */
    else if (p->x > r->x) { p->x = fmax(p->x - 2, r->x); p->face = -1; player_pose(p, 1, 16, -1); }
    else { p->cY = 50; player_pose(p, 40, 24, -1); }                                /* 8193 / 8194: facing in (skin direction 24) */
    player_sync_sensors(p);
    return 1;
}

/* ================================================================ drawing */
static void draw_obj(const obj_t *o) {
    if (!o->an.b) return;
    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
    if (sx < -200 || sx > SCREEN_W + 200 || sy < -200 || sy > SCREEN_H + 200) return;
    if (o->ang != 0) frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, 0, 0xffffffff);
    else frame_draw(o->an.b, anim_frame(&o->an), sx, sy, 0, 0xffffffff);
}
/* back of the play layer: the carousel rings (8347 / 8348 move_back: the far half behind the near half; layer-2 pieces
 * first) and the A5 disco light (8327 move_back, Chowdren shader 0x14 = subtract: drawn as a darkening tint, INFERRED) */
void fn_gimb_draw_back(void) {
    if (!S.any) return;
    if (N(OT_FN_CAROUSEL)) {
        for (int pass = 0; pass < 4; pass++) {
            obj_t *o;
            EACH(OT_FN_CAROUSEL, o) {
                if ((o->layer >= 2) != (pass >= 2) || o->av[0] < G.camx - 200 || o->av[0] > G.camx + SCREEN_W + 200 || o->y > G.camy + SCREEN_H + 8 || o->y + 100 < G.camy) continue;
                double B = fmod(o->av[1] - (double)S.car_t, 360); if (B < 0) B += 360;
                int far = B > 90 && B < 270; if (far != !(pass & 1)) continue;
                int a = o->an.anim; const anim_t *an = &o->an.b->anims[a];
                int fi = (int)(7 + 7 * c_sin(B + 5)); if (fi < 0) fi = 0; if (fi >= an->count) fi = an->count - 1;
                const frame_t *f = &o->an.b->frames[o->an.b->seq[an->first + fi]];
                double xs = 0.5 + 0.5 * c_cos(2 * B), x = o->av[0] + 112 * c_sin(B);
                float w = (float)(f->w * xs);
                if (w < 1) continue;
                frame_draw_scaled(o->an.b, f, (float)floor(x - G.camx - f->hx * xs), floorf(o->y - G.camy) - f->hy, w, f->h, 0xffffffff);
            }
        }
    }
    obj_t *dl = slot(S.dl);
    if (dl && dl->av[0] > 0) {
        int a = (int)fmin(fmax(4 * dl->av[0], 0), 255);
        r_tint(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)(88 * a / 255) << 16) | ((uint32_t)(152 * a / 255) << 8));   /* image 7349: (88, 152, 0) */
    }
}
/* over the player: the restroom door she walks in behind (8196-8205) and the pod shell carrying her (8351 move_front) */
void fn_gimb_draw_front(void) {
    if (!S.any) return;
    obj_t *o;
    if (S.behind) EACH(OT_FN_RESTROOM, o) if (disc_hit(&G.p, o) || (int)(o - G.o) == S.room) draw_obj(o);
    if (S.pod >= 0 && slot(S.pod)) draw_obj(&G.o[S.pod]);
}
