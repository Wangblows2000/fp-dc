/* Fortune Night gimmicks, acts 1-3 (session 17): dragon tunnels (segments, heads, masks, hides), ferris wheels (cars + spokes),
 * gold platforms, rolling arches, boost blocks (+ flames / turn markers / dust), breakable walls and floors (bomb blast, boost
 * blocks, per-character breakers), lantern bridges, clotheslines (hang M41 + shimmy), vertical half-pipes (player state M38),
 * teeter-totters, the FN2 trolley car (+ platforms / lights / loop sound), gachapon machines (the RM hanging switch drives them),
 * fireworks, decor with logic (spinning signs, breakable lampposts, paper / floating lanterns, strobe lights, stars; the pigeons are
 * fn_enemy.c's).
 * Reused elsewhere: the chair (relic_gimb.c, type 336), the hanging switch + rope (relic_gimb.c 301 / relic_gima.c 264), the
 * loop-layer pieces (registry OD_LAYER0 / 1, gimmicks.c layers_refresh), monitors / springs / spikes / pads (shared code, the
 * pads' values in levels.c).
 * Source: build/re/spec_fn_gimmicks_a.md (static RE; no original trace of Fortune Night yet). Section numbers below refer to it.
 * Labels: RE (read from code), INFERRED (reasoned, marked), UNKNOWN (spec U-number).
 * Everything runs after the movement block and the objects pass (spec 0.4: "every gimmick moves after Lilac's movement block"),
 * in one pass per family on index lists cached at spawn; nothing runs in a level without these objects (S.on).
 * Object fields: par = family member (lvobj anim bits 0-5), c = the act's event-copy flags (bits 6-7), layer = the instance
 * layer (bits 8-15). fl bit 0 keeps the shared meaning (flag 0: ignored by the sensors); 0x20 / 0x40 are combat.c's armoured /
 * defeated on the Q20 members (lampposts, boost blocks); this file's own flags are >= 0x1000. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RT0 OT_FNA_FIRST
#define NRT (OT_FNA_LAST - OT_FNA_FIRST + 1)
#define LMAX 192
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define HAS(T) (NLS[(T) - RT0] > 0)
#define F1 0x1000
#define F2 0x2000
#define F3 0x4000
#define F6 0x8000                                  /* "flag 6" of the breakables: break this frame */
#define DEG 57.29577951308232
#define GT (G.frame)                               /* GV[3]: the frame counter */

static struct {
    int on;                                        /* any object of this set in the level */
    int trolley_ch, trolley_snd;                   /* channel 27 (sound 260 loop), loop requested */
    int line;                                      /* clothesline held (slot, -1 none) */
    int flant_on; double flant_x;                  /* floating lanterns: group +0x2262c, its start X (parameter key 1) */
    int arch_pair[8][2];                           /* rolling arch -> base, wheel slots */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / DEG); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / DEG); }
static int set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a < 0) return 0; if (o->an.b != &G.ens || o->an.anim != a) anim_set(&o->an, &G.ens, a); return 1; }
static int set_sk(obj_t *o, const char *fmt, int k) { char nm[20]; snprintf(nm, sizeof nm, fmt, k); return set_s(o, nm); }
static void restart_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }
static int anim_is(const obj_t *o, const char *nm) { return o->an.b == &G.ens && o->an.anim == ens_anim(nm); }
static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static int Mst(const player_t *p) { return player_orig_m(p); }
static double pB(const player_t *p) { return p->ground ? p->gsp : p->vx; }
static double *pBp(player_t *p) { return p->ground ? &p->gsp : &p->vx; }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
static int near_view(const obj_t *o, int n) { return obj_near_view(o, n); }
/* the current frame's action point, rotated with the object (Chowdren get_action_x / y) */
static void act_pt(const obj_t *o, double *x, double *y) {
    *x = o->x; *y = o->y;
    if (!o->an.b || !o->an.b->act) return;
    const frame_t *f = anim_frame(&o->an); int fi = (int)(f - o->an.b->frames);
    double dx = o->an.b->act[2 * fi] - f->hx, dy = o->an.b->act[2 * fi + 1] - f->hy, a = o->ang;
    *x = floorf(o->x) + dx * c_cos(a) + dy * c_sin(a); *y = floorf(o->y) - dx * c_sin(a) + dy * c_cos(a);
}
static void img_box(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = o->x; *y0 = o->y; *x1 = o->x + 1; *y1 = o->y + 1; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int box_ov(float a0, float b0, float a1, float b1, float c0, float d0, float c1, float d1) { return a0 < c1 && c0 < a1 && b0 < d1 && d0 < b1; }
/* a placed player sensor overlapping the object (box objects unrotated: image rect, others: the (rotated) mask) */
static int sens_hit(int s, const obj_t *o) {
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    if ((objdef(o->type)->flags & OD_BOXCOL) && o->ang == 0) {
        float bx, by, bx1, by1; img_box(o, &bx, &by, &bx1, &by1);
        for (int j = 0; j < h; j++) { int y = oy + j; if (b[j] < a[j] || y < by || y >= by1) continue; if (ox + b[j] >= bx && ox + a[j] < bx1) return 1; }
        return 0;
    }
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int near_p(const player_t *p, const obj_t *o, float r) { return fabsf(o->x - (float)p->x) < r && fabsf(o->y - (float)MY(p)) < r; }
static int disc_hit(const player_t *p, const obj_t *o) {   /* PLAYER (Player_326, the 27-px disc at (int X, int Y)) */
    static signed char x0[27], x1[27]; static int init;
    if (!init) { init = 1; for (int j = 0; j < 27; j++) { double d = 13.25 * 13.25 - (j - 13) * (j - 13); int w = d < 0 ? -1 : (int)sqrt(d); x0[j] = (signed char)(13 - w); x1[j] = (signed char)(13 + w); } }
    if (!o->an.b) return 0;
    if ((objdef(o->type)->flags & OD_BOXCOL) && o->ang == 0) {
        float bx, by, bx1, by1; img_box(o, &bx, &by, &bx1, &by1); int X = (int)PX(p), Y = (int)PY(p);
        float cx = fmaxf(bx, fminf((float)X, bx1 - 1)), cy = fmaxf(by, fminf((float)Y, by1 - 1));
        return (cx - X) * (cx - X) + (cy - Y) * (cy - Y) <= 13.25f * 13.25f;
    }
    return obj_rows_hit(o, (int)PX(p) - 13, (int)PY(p) - 13, x0, x1, 27);
}
static int csens_hit(const player_t *p, const obj_t *o) {   /* PlayerCharacterSensor_375 */
    int X = (int)PX(p), Y = (int)PY(p), small = player_char_sensor_small(p);
    return o->an.b && obj_rect_hit(o, X - 7, small ? Y - 13 : Y - 34, X + 9, small ? Y + 15 : Y + 13);
}
static int r_hurt(player_t *p, int n) {          /* MV.R = 1, LIFE.B += n (2414 applies it next frame) */
    if (dead(p) || p->hurt_pending || player_invincible(p)) return 0;
    player_contact(p, n); return 1;
}
static void lst_add(obj_t *q) {                  /* a runtime object joins its type's list (a dead entry is reused when full) */
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static obj_t *spawn(int type, const char *anim, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    if (anim) set_s(o, anim);
    lst_add(o);
    return o;
}
static void launch_item(float x, float y, int dir, int st) {   /* a thrown petal / crystal (Pinball-style flight: obj_launch_crystal's) */
    if (st == OT_CRYSTAL) { obj_launch_crystal(x, y, dir); return; }
    obj_t *e = obj_alloc(); int a = bank_anim(&G.objs, "objectpetal_355");
    if (!e) return;
    double r = (dir & 31) * 11.25 / DEG;
    e->type = OT_ITEM_FLY; e->st = st; e->alive = 1; e->layer = 2; e->x = x; e->y = y;
    e->vx = 4.f * (float)fd_cos(r); e->vy = -4.f * (float)fd_sin(r);   /* speed INFERRED (as the crystals) */
    if (a >= 0) anim_set(&e->an, &G.objs, a);
}

/* ================================================================ path platforms (3283-3289) + carry (4061)
 * av[0] A, av[1] E, kx I, ky J; av[6] / av[7] the last M / N; vx / vy this frame's O / P */
static void path_init(obj_t *o, double A, double E, double C, double Gm) {
    o->kx = o->x; o->ky = o->y; o->av[0] = A; o->av[1] = E;
    o->av[6] = o->kx + C * c_cos(A); o->av[7] = o->ky + Gm * c_sin(E); o->vx = o->vy = 0;
}
static void path_step(obj_t *o, double dA, double dE, double C, double Gm) {   /* D = 0 (x by cos A), H = 1 (y by sin E) */
    double K = o->av[6], L = o->av[7];
    o->av[0] = fmod(o->av[0] + dA, 360.); o->av[1] = fmod(o->av[1] + dE, 360.);
    double M = o->kx + C * c_cos(o->av[0]), N = o->ky + Gm * c_sin(o->av[1]);
    o->vx = (float)(floor(M) - floor(K)); o->vy = (float)(floor(N) - floor(L));
    o->av[6] = M; o->av[7] = N; o->x = (float)floor(M); o->y = (float)floor(N);
}
static void carry(player_t *p) {   /* 4061 (spec_platforms 5.1): G >= 0, S_BB 1 px lower on a Q13 object: MV += O, P, L = 1, N = O = 0 */
    static const int Q13[] = { OT_FN_DSEG, OT_FN_DSEGP, OT_FN_FCAR };
    if (!(p->vy >= 0 || p->ground) || dead(p) || p->state == ST_HANG || p->state == ST_HELD || p->state == ST_CART) return;
    for (unsigned q = 0; q < sizeof Q13 / sizeof Q13[0]; q++) {
        if (!HAS(Q13[q])) continue;
        { int nearby = 0; obj_t *o; EACH(Q13[q], o) if (near_p(p, o, 96)) { nearby = 1; break; } if (!nearby) continue; }   /* (DSEG is in no sensor group: its type query scans every slot) */
        int k = mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, Q13[q]);
        if (k < 0) continue;
        p->x += G.o[k].vx; p->y += G.o[k].vy;
        if (!p->ground) { p->gsp = p->vx; p->ground = 1; }
        p->vy = 0; p->N = 0; p->O = 0;
        player_sync_sensors(p);
        break;
    }
}
static void paths(player_t *p) {
    obj_t *o;
    EACH(OT_FN_DSEG, o) path_step(o, 0, 4, 0, 32);        /* dragons: F 4, G 32, H 1 (x overridden by 7198) */
    EACH(OT_FN_DSEGP, o) path_step(o, 0, 4, 0, 32);
    EACH(OT_FN_FCAR, o) path_step(o, 1, 1, 128, 128);     /* ferris cars: B = F = 1, C = G = 128 */
    carry(p);
}

/* ================================================================ 1. dragon tunnel (7198-7204; start 7474 / 8081)
 * segments: av[1] E, av[2] Q, tm R (segment number), dir +1 (Tunnel1) / -1 (Tunnel2); heads: tm = the R they follow, dir;
 * masks: dir (0 Mask1, 1 Mask2), fl bit 0 = flag 0 (disabled) */
static int seg_t1(const obj_t *o) { return o->type == OT_FN_DSEGP || o->par == 0; }
static void dragons(player_t *p) {
    obj_t *o, *m, *h;
    if (!HAS(OT_FN_DSEG) && !HAS(OT_FN_DSEGP)) return;
    for (int t = OT_FN_DSEG; t <= OT_FN_DSEGP; t++) EACH(t, o) {
        int s = seg_t1(o) ? 1 : -1;
        o->x = (float)(o->kx + s * o->av[2]);                                  /* 7198: x = I + Q / I - Q */
        o->av[2] += 4;                                                         /* 7199 */
        o->ang = (float)(s * 30 * c_sin(o->av[1] - 90));
    }
    EACH(OT_FN_DMASK, m) m->fl |= 1;                                           /* 7199: every mask disabled ... */
    for (int t = OT_FN_DSEG; t <= OT_FN_DSEGP; t++) EACH(t, o) {               /* ... except next to a segment of its dragon (+-39 px) */
        int want = seg_t1(o) ? 0 : 1;
        EACH(OT_FN_DMASK, m) if (m->par == want && fabsf(m->x - o->x) <= 39 && fabsf(m->y - o->y) <= 39) m->fl &= ~1;
    }
    for (int t = OT_FN_DSEG; t <= OT_FN_DSEGP; t++) EACH(t, o) if (o->av[2] >= 752) {   /* 7200 / 7201: the segment re-enters its window */
        o->av[1] = seg_t1(o) ? 0 : 180; o->av[2] -= 752;
    }
    if (!dead(p) && p->ground) {                                               /* 7202 / 7203: conveyor on the back */
        int r = 0, l = 0;
        EACH(OT_FN_DMASK, m) if (near_p(p, m, 80) && sens_hit(SENS_BOTTOMBIG, m)) { if (m->par == 0) r = 1; else l = 1; }
        if (r && !mv_z(SENS_RIGHTBIG)) { p->x += 4; player_sync_sensors(p); }
        if (l && !mv_z(SENS_LEFTBIG)) { p->x -= 4; player_sync_sensors(p); }
    }
    EACH(OT_FN_DHEAD, h) {                                                     /* 7204 (7149 / 7150): heads on their segments */
        for (int t = OT_FN_DSEG; t <= OT_FN_DSEGP; t++) EACH(t, o) if ((seg_t1(o) ? 0 : 1) == h->par && o->tm == h->tm) {
            h->x = o->x; h->y = o->y; h->ang = (float)((h->par ? -30 : 30) * c_sin(o->av[1] - 90));
        }
    }
}

/* ================================================================ 2. ferris spokes (7197): C index mod 20, A / B the hub, D phase */
static void spokes(void) {
    obj_t *o;
    EACH(OT_FN_FSPOKE, o) {
        int C = o->tm, j = C % 5;
        double th = 18.0 * C + o->av[2] - 10.0 * j, r = (1 + j) * 22.0;
        o->x = (float)(int)(o->av[0] + r * c_cos(th)); o->y = (float)(int)(o->av[1] + r * c_sin(th));
        o->av[2] = fmod(o->av[2] + 1, 360.);
    }
}

/* ================================================================ 4. rolling arch (7224-7234 / 7519-7528, 7668 / 7669)
 * arch: av[0] A (angle 0..180), av[1] B (angular speed), av[2] / av[3] base / wheel slots; base: fl bit 0 = flag 0 (not solid) */
static void arches(player_t *p) {
    obj_t *a;
    EACH(OT_FN_ARCH, a) {
        obj_t *b = slot((int)a->av[2]), *w = slot((int)a->av[3]);
        double *A = &a->av[0], *B = &a->av[1];
        a->ang = (float)*A; *A = clampd(*A, 0, 180);                           /* 7224 */
        if (w) { double x, y; act_pt(a, &x, &y); w->x = (float)floor(x); w->y = (float)floor(y); w->ang = (float)fmod(3 * *A, 360); }
        if (!b) continue;
        float bx0, by0, bx1, by1; img_box(b, &bx0, &by0, &bx1, &by1);           /* the base's 256 x 128 box (hot spot 0, 0) */
        int close = !dead(p) && (float)p->x > bx0 - 48 && (float)p->x < bx1 + 48 && (float)MY(p) > by0 - 48 && (float)MY(p) < by1 + 48;   /* (a 200-px
             * radius around the base's top-left corner left the ring's right half out: the base went non-solid under her, 7231) */
        if (close) {
            double Bp = pB(p);
            int rb_a = sens_hit(SENS_RIGHTBIG, a), lb_a = sens_hit(SENS_LEFTBIG, a), bb_a = sens_hit(SENS_BOTTOMBIG, a);
            int side = 0;
            /* 7225 / 7226 keep the arches with A >= 175 / A <= 5 (RE session 18: the selection loops remove A < 175 / A > 5; the
             * session-17 port read them the other way round, so the arch resting at 180 / 0 never caught the player). RV arch_lf:
             * sprung up at the tower's left side holding right, Lilac is caught at the ring's left foot (A 180, B 4.8, N ~86) and
             * runs round the upper half while the arch follows her (A 178 -> 103 in 38 frames). */
            if (rb_a && sens_hit(SENS_RIGHTBIG, b) && !lb_a && !bb_a && Bp >= 0 && *A >= 175) side = 1;         /* 7225 */
            else if (lb_a && sens_hit(SENS_LEFTBIG, b) && !rb_a && !bb_a && Bp <= 0 && *A <= 5) side = -1; /* 7226 */
            if (side) {
                p->N = p->O = side > 0 ? 90 : 270; b->fl &= ~1;
                p->ground = 1; p->vy = 0; p->gsp = 5.0 * side; p->vx = 0;
                player_sync_sensors(p);                                        /* all-sensors loop, L = 1 */
            }
        }
        int solid = !(b->fl & 1);
        if (solid && close && p->state == ST_FLOAT) { p->state = ST_NORMAL; p->state_t = 0; p->nol = 0; p->c_nospd = 0; }   /* 7227 (M22; 7228 M36 not ported) */
        int bb_b = close && sens_hit(SENS_BOTTOMBIG, b);
        if (solid && bb_b && obj_obj_hit(b, a)) {                              /* 7229 / 7230: the arch points at her */
            int sx, sy; mv_sensor_pos(SENS_BOTTOMBIG, &sx, &sy);
            *A = 180 - fd_atan2(a->y - sy, a->x - sx) * DEG; *B = 0.001;
            if (*A > 190) *A -= 360;
        }
        if (solid && !bb_b) b->fl |= 1;                                        /* 7231 */
        if ((b->fl & 1) && (*A > 0 && *A < 180) && obj_obj_hit(a, b)) {        /* 7232 / 7233: it falls to the nearer rest */
            if (*A >= 90) { *A += *B; *B += 0.2; } else { *A += *B; *B -= 0.2; }
        }
        if ((*A == 0 || *A == 180) && *B != 0) *B = 0;                         /* 7234 */
    }
}

/* ================================================================ 5. boost blocks (7242-7256, start 7476, inline crash, col_5_29)
 * block: st D (0 idle, 1 flying, 2..80 crashed), av[0] E, av[1] F, av[2] flame slot, av[3] last background overlap; A 99 HP;
 * fl 0x20 = flag 5 (armoured, combat.c). flame: st = its block's D */
static void dust(const obj_t *fl) {
    double x, y; act_pt(fl, &x, &y);
    obj_t *d = spawn(OT_FN_DUST, "fndust", (float)x, (float)y, fl->layer);
    if (!d) return;
    int dir = (fl->dir + 16) & 31; double r = dir * 11.25 / DEG;            /* direction + 16, speed 50 (/ 8 px a frame, INFERRED) */
    d->vx = (float)(6.25 * fd_cos(r)); d->vy = (float)(-6.25 * fd_sin(r)); d->ang = (float)(90 * cross_rand(4));
}
static void boosts(player_t *p) {
    obj_t *o, *f, *d;
    EACH(OT_FN_BOOST, o) {
        f = slot((int)o->av[2]); if (f && f->type != OT_FN_BFLAME) f = NULL;
        if (o->fl & 0x40) { if (f) f->hidden = 1; continue; }                  /* defeated: combat.c's fly-away */
        if (o->st == 0 && near_view(o, 120)) { o->fl |= 0x20; o->st = 1; }    /* 7242 */
        if (o->st == 1) { o->x = (float)(int)(o->x + o->av[0]); o->y = (float)(int)(o->y + o->av[1]); }   /* 7243 */
        if (o->st == 1) {                                                      /* inline test_collisions: rising edge -> anim 7, D = 2 (the edge latch only */
            int bg = obj_overlaps_bg(o);                                       /* changes in flight: a block resting on the floor keeps it set) */
            if (bg && !o->av[3]) { char nm[16]; snprintf(nm, sizeof nm, "fnboost7_%d", o->dir & 24); restart_s(o, nm); o->st = 2; }
            o->av[3] = bg;
        }
        if (f) {                                                               /* 7244 / 7245 */
            f->hidden = 1; f->x = o->x; f->y = o->y; f->dir = o->dir; f->st = o->st; set_sk(f, "fnbflame%d", o->dir & 24);
            if (f->st == 1) f->hidden = 0;
            if (f->st == 1 && near_view(f, 30) && (GT & 1)) dust(f);           /* 7246 */
        }
        if (!dead(p) && near_p(p, o, 120)) {
            if (o->st == 1 && disc_hit(p, o)) { p->x += o->av[0]; p->y += o->av[1]; player_sync_sensors(p); }   /* 7247 */
            if (o->st == 1) {
                double *Bp = pBp(p);
                if (o->av[0] > 0 && sens_hit(SENS_LEFTBIG, o)) *Bp = fmax(*Bp, 0);    /* 7248 */
                if (o->av[0] < 0 && sens_hit(SENS_RIGHTBIG, o)) *Bp = fmin(*Bp, 0);   /* 7249 */
                if (!p->ground && o->av[1] > 0 && sens_hit(SENS_TOPBIG, o)) p->vy = fmax(p->vy, 0);      /* 7250 */
                if (!p->ground && o->av[1] < 0 && sens_hit(SENS_BOTTOMBIG, o)) p->vy = fmin(p->vy, 0);   /* 7251 */
            }
            if (disc_hit(p, o)) r_hurt(p, 10);                                 /* 7252: contact (any D, no shield test) */
            if (f && !f->hidden && p->shield != 2 && csens_hit(p, f)) r_hurt(p, 5);   /* 7253: the flame (not the fire shield) */
        }
        if (o->st == 2 && near_view(o, 80)) { o->st += 1; snd_play_orig(163, 220, 128); }   /* 7254 */
        if (o->st > 1 && o->st < 80) o->st += 1;                               /* 7255 */
        if (o->st == 80) { o->st = 0; o->av[0] = -o->av[0]; o->av[1] = -o->av[1]; o->dir = (o->dir + 16) & 31; }   /* 7256 */
    }
    EACH(OT_FN_DUST, d) {
        if (d->born != G.frame) { d->x += d->vx; d->y += d->vy; }
        if (!d->an.b || d->an.done) d->alive = 0;
    }
}

/* ================================================================ 6. breakable walls / floors (7211-7223, col_0/16_29, col_5_29)
 * fl F6 = flag 6 (break), av[0] / av[1] = X / Y (piece speeds) */
static void blast_box(const obj_t *e, float *x0, float *y0, float *x1, float *y1) { *x0 = e->x - 8; *y0 = e->y - 4; *x1 = e->x + 40; *y1 = e->y + 36; }   /* MonitorIcon frame 9 (aqua.c) */
static short BL[16]; static int NBL;            /* this frame's bomb blasts (OT_BOMBX, aqua.c), collected once */
static void blasts_collect(void) {
    NBL = 0;
    if (!objects_alive_type(OT_BOMBX)) return;
    for (int i = 0; i < G.nobjs && NBL < 16; i++) if (G.o[i].alive && G.o[i].type == OT_BOMBX) BL[NBL++] = (short)i;
}
static int blasted(const obj_t *o) {             /* 7218 / 7219: a bomb blast overlaps it */
    float a0, b0, a1, b1, c0, d0, c1, d1; img_box(o, &a0, &b0, &a1, &b1);
    for (int k = 0; k < NBL; k++) {
        const obj_t *e = &G.o[BL[k]];
        if (!e->alive || e->type != OT_BOMBX) continue;
        blast_box(e, &c0, &d0, &c1, &d1);
        if (box_ov(a0, b0, a1, b1, c0, d0, c1, d1)) return 1;
    }
    return 0;
}
void fn_gima_floor_hit(obj_t *f, double X, double Y) { if (f && f->alive && f->type == OT_FN_BFLOOR) { f->fl |= F6; f->av[0] = X; f->av[1] = Y; } }
static void pieces_new(const obj_t *o, int type, const char *anim, const int (*pos)[2], int n, double X, double Y, int xr, int rot) {
    for (int k = 0; k < n; k++) {
        obj_t *q = spawn(type, anim, o->x + pos[k][0], o->y + pos[k][1], o->layer);
        if (!q) return;
        q->av[0] = X - xr / 2 + cross_rand(xr); q->av[1] = Y - 4 + cross_rand(4); q->av[2] = 0.3; q->av[3] = q->x; q->av[4] = q->y;
        q->ang = rot ? (float)(90 * cross_rand(4)) : 0;
    }
}
static void pieces_frame(int type) {             /* 7216 / 7217, 7222 / 7223 */
    obj_t *q;
    EACH(type, q) {
        if (q->born == G.frame) continue;
        q->av[3] += q->av[0]; q->av[1] += q->av[2]; q->av[4] += q->av[1];
        q->x = (float)q->av[3]; q->y = (float)q->av[4]; q->ang = (float)fmod(q->ang + q->av[0] + 360, 360);
        if (q->y > G.camy + 320) q->alive = 0;
    }
}
static void breakables(player_t *p) {
    obj_t *o, *b;
    if (!HAS(OT_FN_BWALL) && !HAS(OT_FN_BFLOOR)) return;
    blasts_collect();
    int M = Mst(p), Q = p->face > 0 ? 1 : -1, Z = player_body_z(p), K = p->hitstop;
    double B = pB(p), Gv = p->ground ? 0 : p->vy;
    int bike_fast = p->bike && fabs(B) > 5;
    EACH(OT_FN_BWALL, o) o->fl &= ~1;                                          /* 7211 */
    EACH(OT_FN_BFLOOR, o) o->fl &= ~1;
    if (M == 5 || M == 6 || M == 26 || bike_fast) { EACH(OT_FN_BWALL, o) o->fl |= 1; }   /* 7212: pass through (boost, roll, cyclone, fast bike) */
    EACH(OT_FN_BWALL, o) {
        if (o->fl & F6) continue;
        double sX = clampd(fmax(3, fabs(1.5 * B)) * Q, -10, 10), sY = 0.5 * Gv;
        int brk = 0;
        if (!dead(p) && near_p(p, o, 160)) {
            int lb = sens_hit(SENS_LEFTBIG, o), rb = sens_hit(SENS_RIGHTBIG, o);
            if (p->bike && ((B > 5 && rb) || (B < -5 && lb))) brk = 1;          /* col_0_29 / col_16_29: the bike rams it */
            else if (Z && K <= 0 && (lb || rb)) brk = 1;                        /* 7213: Z = 1 side hit */
            else if (M == 26 && (lb || rb || disc_hit(p, o))) brk = 1;          /* 7213: the cyclone's InstaShield (INFERRED: her disc) */
            else if (player_cube_breaks_rock(o)) brk = 1;                       /* 7213: the cube (A 3) / beam (anim 5) */
            else if (Z && K <= 0 && (sens_hit(SENS_BOTTOMBIG, o) || sens_hit(SENS_TOPBIG, o))) { brk = 2; sX = clampd(0.5 * B, -10, 10); sY = Gv; }   /* 7214 */
        }
        if (brk) { player_hitstop(p, 5); o->fl |= F6; o->av[0] = sX; o->av[1] = sY; }
        if (!(o->fl & F6) && blasted(o)) o->fl |= F6;                           /* 7218 */
        EACH(OT_FN_BOOST, b) if (!(o->fl & F6) && !(b->fl & 0x40) && fabsf(b->x - o->x) < 96 && fabsf(b->y - o->y) < 128 && obj_obj_hit(o, b)) {   /* col_5_29 */
            o->fl |= F6; o->av[0] = b->av[0]; o->av[1] = b->av[1];
        }
    }
    EACH(OT_FN_BFLOOR, o) if (o->par == 1 && !(o->fl & F6) && blasted(o)) o->fl |= F6;   /* 7219: the bomb opens Floor2 only */
    static const int WP[8][2] = { { 10, 10 }, { 30, 10 }, { 10, 30 }, { 30, 30 }, { 10, 42 }, { 30, 42 }, { 10, 62 }, { 30, 62 } };
    static const int FP[8][2] = { { 8, 12 }, { 24, 12 }, { 40, 12 }, { 56, 12 }, { 8, 36 }, { 24, 36 }, { 40, 36 }, { 56, 36 } };
    EACH(OT_FN_BWALL, o) if (o->fl & F6) {                                     /* 7215 */
        obj_t v = *o; o->alive = 0; snd_play_orig(13, 220, 128);
        pieces_new(&v, OT_FN_BWPIECE, "fnbwpiece", WP, 8, v.av[0], v.av[1], 4, 1);
    }
    EACH(OT_FN_BFLOOR, o) if (o->fl & F6) {                                    /* 7220 / 7221 */
        obj_t v = *o; o->alive = 0; snd_play_orig(13, 220, 128); camera_shake(3);
        pieces_new(&v, OT_FN_BFPIECE, "fnbfpiece", FP, 8, v.av[0], v.av[1], 8, 0);
    }
}

/* ================================================================ 7. lantern bridge (7235-7241; 7347 / 7348 at the start)
 * link1 (par 0): av[0] A curl, av[1] B phase, av[2] C sign, st D, tm F, dir; every link: av[5] = its bridge (link1 slot) */
static obj_t *link_of(int bridge, int k) {
    obj_t *o; EACH(OT_FN_LBRIDGE, o) if (o->par == k && (int)o->av[5] == bridge) return o;
    return NULL;
}
static void lbridges(player_t *p) {
    obj_t *l1;
    if (!HAS(OT_FN_LBRIDGE)) return;
    EACH(OT_FN_LBRIDGE, l1) {
        if (l1->par != 0) continue;
        int br = (int)(l1 - G.o);
        obj_t *prev = l1;
        for (int k = 1; k < 8; k++) {                                          /* 7235: link k+1 at link k's action point, angle k A */
            obj_t *l = link_of(br, k); if (!l) break;
            double x, y; act_pt(prev, &x, &y); l->x = (float)floor(x); l->y = (float)floor(y); l->ang = (float)(k * l1->av[0]);
            prev = l;
        }
        double *A = &l1->av[0], *B = &l1->av[1], C = l1->av[2];
        if (l1->st == 1 && *B < 180) { *A = 6.5 * C * (1 + c_cos(180 + *B)); *B += 8; }   /* 7236 */
        else if (l1->st == 1 && l1->tm < 30) l1->tm += 1;                       /* 7237 */
        if (l1->st == 1 && l1->tm >= 30 && !p->ground) { l1->tm = 0; l1->st = -1; }   /* 7238 */
        if (l1->st == -1 && *B > 0) { *A = 6.5 * C * (1 + c_cos(180 + *B)); *B -= 4; }   /* 7239 */
        int X = (int)PX(p), Y = (int)PY(p);
        if (!dead(p) && l1->st <= 0 && Y >= l1->y - 199 && Y < l1->y &&
            (l1->dir == 0 ? (X >= l1->x - 87 && X < l1->x) : (X > l1->x && X <= l1->x + 87))) l1->st = 1;   /* 7240 / 7241 */
    }
    obj_t *o;
    EACH(OT_FN_LBRIDGE, o) if (o->par <= 1) { if (p->layer == 0) o->fl &= ~1; else o->fl |= 1; }   /* Q10: links 1-2 solid on layer 0 only */
}

/* ================================================================ 8. clotheslines (7205-7210): hang M41 + shimmy */
static void clotheslines(player_t *p) {
    obj_t *o, *s;
    if (!HAS(OT_FN_CLOTHES)) return;
    int M = Mst(p);
    if (p->state == ST_HANG && S.line >= 0) {                                  /* 7206-7208 */
        int dirh = (p->held & BTN_RIGHT) ? 1 : (p->held & BTN_LEFT) ? -1 : 0, stop = 0;
        if (dirh) EACH(OT_FN_CLSTOP, s) if (near_p(p, s, 64) && sens_hit(dirh > 0 ? SENS_RIGHTBIG : SENS_LEFTBIG, s)) stop = 1;
        int moved = 0;
        if (dirh && !stop && p->vy >= 0) {
            double nx = p->hang_x + 3 * dirh; int on = 0;                     /* B = +-3; SKIN still on a line (INFERRED: the hang point) */
            EACH(OT_FN_CLOTHES, o) { float a0, b0, a1, b1; img_box(o, &a0, &b0, &a1, &b1); if (nx >= a0 && nx < a1 && fabs(p->hang_y - o->y) < 12) on = 1; }
            if (on) { p->hang_x = nx; p->x = nx; p->face = dirh; moved = 1; player_sync_sensors(p); }
        }
        p->cY = moved ? 42 : 41;                                               /* OTHERS.Y 42 hang move / 41 hang idle */
    }
    if (M != 41) {                                                             /* 7210 */
        if (S.line >= 0 && p->cY == 42) p->cY = 40;
        S.line = -1; EACH(OT_FN_CLOTHES, o) o->fl &= ~F1;
    }
    if (player_can_hang(p) && !dead(p) && M != 41 && M != 8 && M != 9) {       /* 7205 / 7209: grab (sound 33 in player_hang) */
        EACH(OT_FN_CLOTHES, o) {
            if (!near_p(p, o, 96) || !sens_hit(SENS_TOPBIG, o)) continue;
            o->fl |= F1;
            player_hang(p, p->x, o->y); p->hang_obj = -1; S.line = (int)(o - G.o); p->cY = 41;   /* MV.Y = line y + 32, X kept */
            break;
        }
    }
}

/* ================================================================ 9. vertical half-pipes and M38 (7535-7542; A3 7703-7705)
 * par = hp1..hp6 - 1; c bit 0x40: this act toggles its solidity with M38, 0x80: entering at hp1 needs B < 0 */
static void halfpipes(player_t *p) {
    obj_t *o;
    if (!HAS(OT_FN_HPIPE)) return;
    int m38 = p->state == ST_HPIPE;
    EACH(OT_FN_HPIPE, o) if (o->c & 0x40) { if (m38) o->fl &= ~1; else o->fl |= 1; }   /* 7535 / 7536 */
    if (m38 && chr_is_milla(G.character) && p->mF > 0) p->mF = p->mG = p->mH = 0;     /* 7537: MILLA F / G / H (hover data) */
    if (dead(p)) return;
    int on[6] = { 0 }, any = 0, sbx = 0, sby = 0; double B = pB(p);
    mv_sensor_pos(SENS_BOTTOMBIG, &sbx, &sby);
    EACH(OT_FN_HPIPE, o) if (near_p(p, o, 160) && sens_hit(SENS_BOTTOMBIG, o)) { on[o->par % 6] = (int)(o - G.o) + 1; any = 1; }
    int Q = p->face > 0 ? 1 : -1;
    int busy = p->state == ST_HURT || p->state == ST_HANG || p->state == ST_PAD || p->state == ST_LADDER || p->state == ST_CORK || p->state == ST_CART || p->state == ST_SIT;
    if (p->ground && !busy) {                                                  /* 7538 / 7705 (no M test in the original; scripted states kept) */
        int go = 0; obj_t *h;
        if (on[0] && (h = &G.o[on[0] - 1]) && sbx < h->x + 48 && Q == -1 && (!(h->c & 0x80) || B < 0)) go = 1;
        if (on[3] && (h = &G.o[on[3] - 1]) && sbx >= h->x + 48 && Q == 1) go = 1;
        if ((on[2] && Q == -1) || (on[5] && Q == 1)) go = 1;
        if (go) {
            if (p->state != ST_HPIPE) { p->state = ST_HPIPE; p->state_t = 0; p->skidding = 0; p->look = 0; p->jumping = 0; p->springing = 0; }
            p->cY = 45; m38 = 1;
        }
    }
    if (!m38) return;
    if (on[1] || on[4]) p->cY = 46;                                            /* 7539 */
    double *Bp = pBp(p);
    if (on[1] && Q == -1 && *Bp > -6) *Bp = fmax(*Bp - 0.2, -8);               /* 7540 */
    if (on[4] && Q == 1 && *Bp < 6) *Bp = fmin(*Bp + 0.2, 8);                  /* 7541 */
    if (!any || (*Bp > -1 && *Bp < 1)) { p->state = ST_NORMAL; p->state_t = 0; p->cY = 40; p->m1 = 0; }   /* 7542 */
}

/* ================================================================ 10. teeter-totter (7543-7548): av[0] A, av[1] B, F1 = flag 1 (armed) */
static void teeters(player_t *p) {
    obj_t *o;
    EACH(OT_FN_TEETER, o) {
        double *A = &o->av[0], *B = &o->av[1];
        o->ang = (float)*A; *A = clampd(*A + *B, -30, 30);                     /* 7543 */
        if (!dead(p) && near_p(p, o, 200) && sens_hit(SENS_BOTTOMBIG, o)) *B += (o->x - p->x) * 0.0008;   /* 7544 */
        if (*B < 0 && *A <= -30) { *B = 0; set_s(o, "fnteeter16"); }          /* 7545 */
        if (*B > 0 && *A >= 30) { *B = 0; set_s(o, "fnteeter0"); }            /* 7546 */
        if ((*A <= -30 || *A >= 30) && (o->fl & F1)) { snd_play_orig(143, 220, 128); o->fl &= ~F1; }   /* 7547 */
        if (!(o->fl & F1) && *A > -25 && *A < 25) o->fl |= F1;                 /* 7548 */
    }
}

/* ================================================================ 11. trolley car (7529-7534, start inline + 7610)
 * car: av[0] D (end x), av[1] A, av[2] B (speed), av[3] C (carrying); platforms / lights follow it */
static void trolley(player_t *p) {
    obj_t *c, *o;
    if (!HAS(OT_FN_TROLLEY)) return;
    double vl = G.camx, vb = G.camy + SCREEN_H;
    EACH(OT_FN_TROLLEY, c) {
        double *A = &c->av[1], *B = &c->av[2];
        if (*A < vl - 430 && *B == 0) { *B = 6; c->hidden = 0; }              /* 7529: starts once the camera has left it behind */
        int vol = 0;
        if (*B != 0) {                                                         /* 7530 */
            c->x += (float)*B; *A = c->x;
            EACH(OT_FN_TPLAT, o) o->x = c->x + (o->par ? 80 : 16);
            EACH(OT_FN_TLIGHT, o) o->x = c->x + (o->par ? 23 : 407);
            vol = (int)clampd(fmin(100, 120 - 0.2 * fabs(c->x - vl)) - clampd(0.2 * (c->y - vb), 0, 100), 0, 100);
            if (S.trolley_ch >= 0) snd_loop_update(S.trolley_ch, vol * 255 / 100, 22050);
        }
        if (!dead(p) && p->ground && !mv_z(SENS_RIGHTBIG)) {                   /* 7531: carried */
            int on = 0; EACH(OT_FN_TPLAT, o) if (sens_hit(SENS_BOTTOMBIG, o)) on = 1;
            if (on) { p->x += *B; c->av[3] = 1; player_sync_sensors(p); }
        }
        if (c->av[3] == 1) {                                                   /* 7532: leaving keeps the car's speed */
            int on = 0; EACH(OT_FN_TPLAT, o) if (sens_hit(SENS_BOTTOMBIG, o)) on = 1;
            if (!p->ground || !on) { *pBp(p) += *B; c->av[3] = 0; }
        }
        if (*B != 0 && *A > fmin(c->av[0], vl + 1626)) {                       /* 7533: loops back behind the camera */
            c->x = (float)(vl - 800);
            if (S.trolley_ch >= 0) snd_loop_stop(S.trolley_ch);
            S.trolley_ch = -1; S.trolley_snd = 0;
        }
        if (!S.trolley_snd && *B != 0 && *A > vl - 430) { S.trolley_ch = snd_loop_orig(260, vol * 255 / 100, 22050); S.trolley_snd = 1; }   /* 7534: channel 27 */
    }
}

/* ================================================================ 12. gachapon machines (7681-7702; switches 5866 / 7664 / 5868 in relic_gimb.c)
 * wheel: st A (0 idle, 1 spin-up, 2 slow-down, 3 prizes, 4 spent), av[0] B, av[1] C, av[2] D, av[3] E, F1 flag 0, F2 flag 1,
 * tm = its index (the switch / door / front with the same index) */
static obj_t *nth(int T, int k) { obj_t *o; int n = 0; for (int i = 0; i < G.nobjs; i++) { o = &G.o[i]; if (o->alive && o->type == T && n++ == k) return o; } return NULL; }
static void gachapons(void) {
    obj_t *w, *o;
    if (!HAS(OT_FN_GWHEEL)) return;
    EACH(OT_FN_GWHEEL, w) {
        int k = w->tm;
        EACH(OT_FN_GFRONT, o) if (o->tm == k) { o->x = w->x + 39; o->y = w->y - 39; o->st = w->st; set_s(o, o->st ? "fngfront1" : "fngfront0"); }   /* 7683 / 7684 */
        EACH(OT_FN_GDOOR, o) if (o->tm == k) { o->x = w->x; o->y = w->y; o->st = w->st; }
        obj_t *sw = slot((int)w->av[4]); if (sw && sw->type != OT_RM_IDLESW) sw = NULL;
        if (sw && (sw->fl & 0x1000)) w->fl |= F1;                              /* 7685: its switch pulled (relic_gimb.c F1 = flag 1) */
        double *B = &w->av[0], *C = &w->av[1], *D = &w->av[2], *E = &w->av[3];
        if ((w->fl & F1) && w->st == 0) { w->st = 1; snd_play_orig(187, 220, 128); *D = 20 + cross_rand(20); set_s(w, "fngwheel1"); }   /* 7686 */
        if (w->st == 1) {                                                      /* 7687-7689 */
            *B = fmin(*B + 0.4, 20); w->ang = (float)fmod((int)*C, 360); *C = (int)*C + *B;
            if (*B >= 20) *D -= 1;
            if (*D <= 0) w->st = 2;
        } else if (w->st == 2) {                                               /* 7690 / 7691 */
            *B = fmax(*B - 0.4, 3); *C = fmod((int)*C + *B, 360); w->ang = (float)*C;
            if (*B <= 3 && fmod((int)*C, 45) < 3) { w->st = 3; *E = (int)*C / 45; set_s(w, "fngwheel2"); }
        }
        if (w->st == 3) {
            if (!(w->fl & F2)) { snd_play_orig(188, 220, 128); w->fl |= F2; }  /* 7692 */
            int e = (int)*E, Di = (int)*D;
            if (e == 0 && GT % 10 == 0) { launch_item(w->x, w->y, Di / 10 + 20, OT_PETAL); snd_play_orig(15, 220, 128); }   /* 7693 */
            else if ((e == 2 || e == 6) && GT % 7 == 0) { launch_item(w->x, w->y, Di / 7 + 18, OT_CRYSTAL); snd_play_orig(15, 220, 128); }   /* 7694 / 7698 */
            else if (e != 0 && e != 2 && e != 6 && Di % 10 == 0) {             /* 7695-7700: E 7 / 5 / 1 / 4 / 3 -> keystone direction 0..4 */
                static const int KD[8] = { 0, 2, 0, 4, 3, 1, 0, 0 };
                int kd = KD[e & 7];
                keystone_spawn(w->x, w->y, 2, 0);                              /* keystone B = 2 (it settles 2555, INFERRED pick-up as any keystone, U7) */
                for (int i = G.nobjs - 1; i >= 0 && kd; i--) if (G.o[i].alive && G.o[i].type == OT_KEYSTONE && G.o[i].born == G.frame) { set_sk(&G.o[i], "fnks%d", kd); break; }
                snd_play_orig(15, 220, 128);
            }
            *D += 1;                                                           /* 7701: lit sector, flashing every 5 GT */
            if ((GT / 5) & 1) set_s(w, "fngwheel1"); else { set_s(w, "fngwheel2"); w->an.frame = (8 - e) & 7; w->an.acc = 0; }
            if (*D >= 80) { w->st = 4; set_s(w, "fngwheel4"); }                /* 7702: spent */
        }
    }
}

/* ================================================================ 13. fireworks (7359-7365): launcher st A (frames), hidden until it rises */
static void fireworks(player_t *p) {
    obj_t *o, *q;
    if (!HAS(OT_FN_FWLAUNCH)) return;
    EACH(OT_FN_FWLAUNCH, o) {
        if (o->hidden && fabs(o->x - p->x) <= 200 && o->y > G.camy + 250) { o->hidden = 0; o->st = 0; }   /* 7359 */
        if (o->hidden) continue;
        o->y -= 6; o->st += 1;                                                 /* 7360 */
        if (o->st > 40) {                                                      /* 7361 / 7362: 2 x 32 particles, sound 430, the launcher goes */
            for (int k = 0; k < 64; k++) {
                int dx = -16 + cross_rand(33), dy = -16 + cross_rand(33);
                obj_t *e = spawn(OT_FN_FWPART, "fnfwp", o->x + dx, o->y + dy, o->layer);
                if (!e) break;
                e->an.frame = k < 32 ? cross_rand(4) : 1 + cross_rand(5);
                double sp = (30 + cross_rand(10)) * 0.125, a = fd_atan2(-(double)dy, (double)dx);   /* outward, BallMovement speed / 8 */
                int d = (int)floor((a < 0 ? a + 6.283185307 : a) * DEG / 11.25) & 31; double r = d * 11.25 / DEG;
                e->vx = (float)(sp * fd_cos(r)); e->vy = (float)(-sp * fd_sin(r));
            }
            snd_play_orig(430, 220, 128); o->alive = 0;
        }
    }
    EACH(OT_FN_FWPART, q) {
        if (q->born == G.frame) continue;
        q->x += q->vx; q->y += q->vy;
        if (GT & 1) { q->x += 1; q->y += 1; }                                  /* 7363 */
        if (!anim_is(q, "fnfwp7") && q->an.frame >= 9) restart_s(q, "fnfwp7"); /* 7364 */
        else if (anim_is(q, "fnfwp7") && q->an.done) q->alive = 0;             /* 7365 */
    }
}

/* ================================================================ 14. decor with logic */
static void signs(player_t *p) {                 /* 7258-7267 / 7671-7680: av[0] A (spin speed) */
    obj_t *o;
    EACH(OT_FN_SIGN, o) {
        if (!near_view(o, 30)) { o->an.frame = 0; o->an.acc = 0; continue; }
        if (!dead(p) && fabs(pB(p)) >= 5 && near_p(p, o, 80) && csens_hit(p, o)) o->av[0] = floor(10 * fabs(pB(p)));
        if (o->av[0] > 20) o->av[0] -= 1;
        if (o->av[0] == 20 && o->an.frame == 0) o->av[0] = 0;
        if (o->av[0] > 0) anim_tick(&o->an, (int)o->av[0]);                    /* force_speed(A) on a speed-0 animation */
    }
}
static void lamps(player_t *p) {                 /* 7268-7271 (+ the shared Q20 hit / defeat 2764 / 2765): av[0] D (landings) */
    obj_t *o; (void)p;
    EACH(OT_FN_LAMP, o) {
        if (!(o->fl & 0x40)) continue;
        if (!(o->fl & F2)) {                                                   /* 7268: first frame defeated */
            o->fl |= F2; snd_play_orig(134, 220, 128); set_s(o, "fnlamp8");     /* (sample 23 is not stopped: no per-sample stop) */
            for (int k = 0; k < 5; k++) {
                obj_t *d = obj_alloc(); int a = ens_anim("fnlampdeb"); if (!d || a < 0) break;
                d->type = OT_DEBRIS; d->alive = 1; d->hp = 1; d->layer = o->layer; d->x = o->x; d->y = o->y; anim_set(&d->an, &G.ens, a); d->an.frame = k;
                double sp = (40 + cross_rand(30)) * 0.125, r = (3 + cross_rand(11)) * 11.25 / DEG;
                d->vx = (float)(sp * fd_cos(r)); d->vy = (float)(-sp * fd_sin(r));
            }
        }
        float x0, y0, x1, y1; img_box(o, &x0, &y0, &x1, &y1);
        static const signed char r0[1] = { 0 }; signed char r1[1] = { (signed char)(x1 - x0 > 26 ? 26 : x1 - x0) };
        if (obj_overlaps_bg(o) || objects_sensor_hit(OBJGRP_SOLID, (int)x0, (int)y1 - 2, r0, r1, 1)) {   /* 7270: lands (INFERRED: D counts the frames on the ground) */
            if (!(o->fl & F1)) {
                o->fl |= F1;
                for (int k = 0; k < 2; k++) { obj_t *d = obj_alloc(); int a = ens_anim("fnlampdeb"); if (!d || a < 0) break;
                    d->type = OT_DEBRIS; d->alive = 1; d->hp = 1; d->layer = o->layer; d->x = o->x; d->y = o->y; anim_set(&d->an, &G.ens, a); d->an.frame = 3 + k;
                    d->vx = (float)(k ? 1.5 : -1.5); d->vy = -3.f; }
            }
            o->av[0] += 1;
        }
        if (o->av[0] >= 2) o->alive = 0;                                       /* 7271 */
    }
}
static void paper_lanterns(void) {               /* 7257 / 7670: frame = fmod(1000 T / 150, 4), all in sync (T: the stage time in s) */
    obj_t *o; int f = (int)((G.frame * 1000.0 / FPS) / 150) % 4;
    EACH(OT_FN_PLANTERN, o) { o->an.frame = f; o->an.acc = 0; }
}
static void floating_lanterns(player_t *p) {     /* 7589-7604: av[0] A (depth / scale), av[1] C, av[2] D */
    obj_t *o;
    if (!HAS(OT_FN_FLANTERN) || !S.flant_x) return;
    if (!S.flant_on) {
        if (!(p->x > S.flant_x)) return;
        S.flant_on = 1;                                                        /* 7589 (once): depth from the spread, C, D; two swap places */
        int n = NLS[OT_FN_FLANTERN - RT0], k = 0;
        EACH(OT_FN_FLANTERN, o) {
            int Bs = n - 1 - k++;
            o->av[0] = 0.5 + 0.05 * (Bs % 10); o->av[1] = o->x * o->av[0] + 2000; o->av[2] = o->y; o->layer = 1; o->hidden = 1;   /* 7590: layer 3 -> 2 */
        }
        if (n > 1) { obj_t *a = &G.o[LST[OT_FN_FLANTERN - RT0][cross_rand(n)]], *b = &G.o[LST[OT_FN_FLANTERN - RT0][cross_rand(n)]];
                     double t1 = a->av[1], t2 = a->av[2]; a->av[1] = b->av[1]; a->av[2] = b->av[2]; b->av[1] = t1; b->av[2] = t2; }
    }
    EACH(OT_FN_FLANTERN, o) {                                                  /* 7602-7604 */
        o->scale = (float)o->av[0]; o->x = (float)(G.camx * (0.8 - o->av[0]) + o->av[1]); o->y = (float)o->av[2];
        o->av[2] -= 2 * o->av[0];
        if (o->y < G.camy - 60) o->av[2] += 350;
        if (o->y > G.camy + 290) o->av[2] -= 350;
    }
}
static void strobes(void) {                      /* 7670 (start 8080: A = 130 x spread) */
    obj_t *o;
    EACH(OT_FN_STROBE, o) { o->ang = (float)(30 * c_sin(o->av[0])); o->av[0] = fmod(o->av[0] + 1, 360); }
}
static void stars(void) {                        /* 7466: piece0a + (A, B); piece0a J = view x 0.99, K = view y 0.94 (spec_fn_flow 4), wrapped by 320 (3543, INFERRED) */
    obj_t *o;
    if (!HAS(OT_FN_STAR)) return;
    double J = G.camx * 0.99, K = G.camy * 0.94;
    J += 320 * floor((G.camx - J) / 320);
    EACH(OT_FN_STAR, o) { o->x = (float)(int)(J + o->av[0]); o->y = (float)(int)(K + o->av[1]); }
}

/* ================================================================ spawn */
static int mine(int t) { return t >= RT0 && t <= OT_FNA_LAST; }
void fn_gima_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    else if (S.trolley_ch >= 0) snd_loop_stop(S.trolley_ch);
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS); S.trolley_ch = -1; S.line = -1;
    for (int t = RT0; t <= OT_FNA_LAST && !S.on; t++) if (objects_have(t)) S.on = 1;
    if (!S.on) return;
    /* lvobj anim per object: the k-th object of a type is the k-th placement of that type (objects_spawn keeps the order) */
    static uint16_t AN[NRT][LMAX]; int na[NRT] = { 0 }, seen[NRT] = { 0 };
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - RT0; if (t >= 0 && t < NRT && na[t] < LMAX) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !mine(o->type)) continue;
        int t = o->type - RT0, an = seen[t] < na[t] ? AN[t][seen[t]] : 0; seen[t]++;
        o->par = an & 0x3f; o->c = an & 0xc0; o->layer = an >> 8; o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = 0; o->dir = 0; o->ang = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->type == OT_FN_PARAM) { if (o->y == 1) S.flant_x = o->x; o->alive = 0; continue; }   /* importer parameters (FNA_PARAMS) */
        if ((o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) && o->type != OT_FN_TPLAT && o->type != OT_FN_TLIGHT) { o->alive = 0; continue; }   /* off-map templates (the trolley's parts are templates moved by 7530) */
        if (NLS[t] < LMAX) LST[t][NLS[t]++] = (short)i;
    }
    obj_t *o, *q;
    /* 1. dragons (7474 / 8081): E = 35 k, Q = E, R = k (k = spread per class, newest first); heads follow segment 13 (s + 1) - 1 */
    { int n1 = 0, n2 = 0, k1 = 0, k2 = 0;
      for (int t = OT_FN_DSEG; t <= OT_FN_DSEGP; t++) EACH(t, o) { if (seg_t1(o)) n1++; else n2++; }
      for (int t = OT_FN_DSEG; t <= OT_FN_DSEGP; t++) EACH(t, o) {
          int k = seg_t1(o) ? n1 - 1 - k1++ : n2 - 1 - k2++;
          set_s(o, o->type == OT_FN_DSEGP ? "fndsegp" : o->par ? "fndseg1" : "fndseg0");
          o->tm = k; path_init(o, 0, 35.0 * k, 0, 32); o->av[2] = 35.0 * k;
      }
      int nh[2] = { 0, 0 }, kh[2] = { 0, 0 };
      EACH(OT_FN_DHEAD, o) nh[o->par & 1]++;
      EACH(OT_FN_DHEAD, o) { int s = nh[o->par & 1] - 1 - kh[o->par & 1]++; o->tm = 13 * (s + 1) - 1; set_sk(o, "fndhead%d", o->par & 1); }
      int nm[2] = { 0, 0 }, km[2] = { 0, 0 };
      EACH(OT_FN_DMASK, o) nm[o->par & 1]++;
      EACH(OT_FN_DMASK, o) { int A = nm[o->par & 1] - 1 - km[o->par & 1]++; set_sk(o, "fndmask%d", o->par & 1); o->an.frame = A % 9; o->fl |= 1; }
      EACH(OT_FN_DHIDE, o) set_sk(o, "fndhide%d", o->par); }
    /* 2. ferris wheels: cars A = E = 90 (k - 1) for wheel car k (ctor), spokes C = spread mod 20, A = x, B = y, D = 320 (7473) */
    EACH(OT_FN_FCAR, o) { set_sk(o, "fnfcar%d", o->par & 3); path_init(o, 90.0 * (o->par & 3), 90.0 * (o->par & 3), 128, 128); }
    { int n = NLS[OT_FN_FSPOKE - RT0], k = 0; EACH(OT_FN_FSPOKE, o) { set_s(o, "fnfspoke"); o->tm = (n - 1 - k++) % 20; o->av[0] = o->x; o->av[1] = o->y; o->av[2] = 320; } }
    EACH(OT_FN_GOLD, o) set_sk(o, "fngold%d", o->par);
    /* 4. rolling arches: A 180 (ctor), base flag 0 on (inline / 7475); pairs by position (the base's box holds the pivot) */
    EACH(OT_FN_ARCHBASE, o) { set_s(o, "fnarchbase"); o->fl |= 1; }
    EACH(OT_FN_ARCHWHEEL, o) set_s(o, "fnarchwheel");
    EACH(OT_FN_ARCH, o) {
        set_s(o, "fnarch"); o->av[0] = 180; o->ang = 180; o->av[2] = o->av[3] = -1;
        EACH(OT_FN_ARCHBASE, q) if (fabsf(q->x + 128 - o->x) <= 8 && fabsf(q->y + 128 - o->y) <= 8) o->av[2] = (double)(q - G.o);
        float bd = 1e9f; EACH(OT_FN_ARCHWHEEL, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < bd && d < 160) { bd = d; o->av[3] = (double)(q - G.o); } }
    }
    /* 5. boost blocks: direction 16, E -5, F 0, A 99 HP; 7476: a turn marker on a block turns it down (F 5, E 0); flames by index */
    { int k = 0;
      EACH(OT_FN_BOOST, o) {
          set_s(o, "fnboost"); o->dir = 16; o->av[0] = -5; o->av[1] = 0; o->A = 99; o->bd = 0; o->av[2] = -1;
          float a0, b0, a1, b1, c0, d0, c1, d1; img_box(o, &a0, &b0, &a1, &b1);
          EACH(OT_FN_BTURN, q) { set_s(q, "fnbturn"); img_box(q, &c0, &d0, &c1, &d1); if (box_ov(a0, b0, a1, b1, c0, d0, c1, d1)) { o->dir = 24; o->av[1] = 5; o->av[0] = 0; q->fl |= F1; } }
          int j = 0; EACH(OT_FN_BFLAME, q) if (j++ == k) { o->av[2] = (double)(q - G.o); set_sk(q, "fnbflame%d", o->dir); }
          k++;
      }
      EACH(OT_FN_BTURN, q) if (q->fl & F1) q->alive = 0; }
    EACH(OT_FN_BWALL, o) set_s(o, "fnbwall");
    EACH(OT_FN_BFLOOR, o) set_sk(o, "fnbfloor%d", o->par & 1);
    /* 7. lantern bridges: link1 E = spread from 1 (newest first); 7347: E 1 -> direction 0, C 1, D 1; 7348: E 2 -> direction 1, C -1
     * (D stays -1). The links of a bridge pair by index; links 2-8 take link1's direction (INFERRED, U3) */
    { int n = 0, k = 0; EACH(OT_FN_LBRIDGE, o) if (o->par == 0) n++;
      EACH(OT_FN_LBRIDGE, o) if (o->par == 0) {
          int E = n - k++; o->st = -1; o->av[2] = 0;
          if (E == 1) { o->dir = 0; o->av[2] = 1; o->st = 1; } else if (E == 2) { o->dir = 1; o->av[2] = -1; }
          o->av[5] = (double)(o - G.o);
      }
      for (int kk = 1; kk < 8; kk++) { int j = 0; EACH(OT_FN_LBRIDGE, o) if (o->par == kk) {
          int jj = 0; EACH(OT_FN_LBRIDGE, q) if (q->par == 0 && jj++ == j) { o->av[5] = (double)(q - G.o); o->dir = q->dir; }
          j++; } }
      EACH(OT_FN_LBRIDGE, o) { char nm[16]; snprintf(nm, sizeof nm, "fnlb%d_%d", o->par & 7, o->dir & 1); set_s(o, nm); } }
    EACH(OT_FN_CLOTHES, o) set_s(o, "fnclothes");
    EACH(OT_FN_CLSTOP, o) set_s(o, "fnclstop");
    EACH(OT_FN_HPIPE, o) { set_sk(o, "fnhpipe%d", o->par % 6); if (o->c & 0x40) o->fl |= 1; }   /* 7535: the act's toggled walls start off */
    EACH(OT_FN_TEETER, o) { set_s(o, "fnteeter0"); o->av[0] = 30; o->ang = 30; }   /* ctor A 30 */
    EACH(OT_FN_FWLAUNCH, o) { set_s(o, "fnfwl"); o->hidden = 1; }
    EACH(OT_FN_FLAG, o) set_s(o, "fnflag");
    EACH(OT_FN_PLANTERN, o) set_s(o, "fnplantern");
    EACH(OT_FN_FLANTERN, o) set_s(o, "fnflantern");
    { int k = 0; EACH(OT_FN_GWHEEL, o) { set_s(o, "fngwheel0"); o->tm = k++; }
      k = 0; EACH(OT_FN_GDOOR, o) { set_s(o, "fngdoor"); o->tm = k++; }
      k = 0; EACH(OT_FN_GFRONT, o) { set_s(o, "fngfront0"); o->tm = k++; }
      EACH(OT_FN_GWHEEL, o) { obj_t *sw = nth(OT_RM_IDLESW, o->tm); o->av[4] = sw ? (double)(sw - G.o) : -1; if (sw) sw->x = o->x + 80; } }   /* 7681: the switch at its wheel + (80, -80) (y = its rest A) */
    EACH(OT_FN_SIGN, o) set_sk(o, "fnsign%d", o->par & 1);
    EACH(OT_FN_LAMP, o) { set_s(o, "fnlamp"); o->A = 1; o->bd = 0; }           /* ctor A 1 (Q20 HP); contact never hurts */
    { int n = NLS[OT_FN_STROBE - RT0], k = 0; EACH(OT_FN_STROBE, o) { set_s(o, "fnstrobe"); o->av[0] = fmod(130.0 * (n - 1 - k++), 360); o->hidden = 1; } }   /* additive: fn_gima_draw */
    EACH(OT_FN_LSIGN, o) set_sk(o, "fnlsign%d", o->par & 3);
    EACH(OT_FN_COLUMN, o) set_sk(o, "fncolumn%d", o->par & 1);
    EACH(OT_FN_RAMP, o) set_sk(o, "fnramp%d", o->par & 1);
    /* 11. trolley: car D = 6420, invisible, A = x (start + 7610); platforms / lights follow it */
    EACH(OT_FN_TROLLEY, o) { set_s(o, "fntrolley"); o->av[0] = 6420; o->av[1] = o->x; o->hidden = 1; }
    EACH(OT_FN_TPLAT, o) set_sk(o, "fntplat%d", o->par & 1);
    EACH(OT_FN_TLIGHT, o) { set_sk(o, "fntlight%d", o->par & 1); o->hidden = 1; }
    /* 15. loop pieces: the layer-4 copy of a piece at the same spot is its drawn front half (one solid object, drawn in front) */
    for (int t = OT_FN_LOOP0; t <= OT_FN_LOOP1; t++) {
        EACH(t, o) set_sk(o, t == OT_FN_LOOP0 ? "fnloop0_%d" : "fnloop1_%d", o->par & 3);
        EACH(t, o) EACH(t, q) if (q != o && q->alive && o->alive && q->par == o->par && q->x == o->x && q->y == o->y && q->layer > o->layer) { o->layer = q->layer; q->alive = 0; }
    }
    { int k = 0; EACH(OT_FN_STAR, o) { set_s(o, "fnstar"); o->av[0] = o->x; o->av[1] = o->y; o->an.frame = (k++) % 16; } }   /* 7471: A = x, B = y, frame C */
    layers_refresh(&G.p);
}

/* ================================================================ per frame (spec 0.4 order) */
void fn_gima_frame(player_t *p) {
    if (!S.on) return;
    paths(p);                                    /* 3283-3289, 4061 */
    spokes();                                    /* 7197 */
    dragons(p);                                  /* 7198-7204 */
    clotheslines(p);                             /* 7205-7210 */
    breakables(p);                               /* 7211-7223, col_0/16_29, col_5_29 */
    pieces_frame(OT_FN_BWPIECE); pieces_frame(OT_FN_BFPIECE);
    arches(p);                                   /* 7224-7234 */
    lbridges(p);                                 /* 7235-7241 */
    boosts(p);                                   /* 7242-7256 */
    paper_lanterns();                            /* 7257 */
    signs(p);                                    /* 7258-7267 */
    lamps(p);                                    /* 7268-7271 */
    fireworks(p);                                /* 7359-7365 */
    trolley(p);                                  /* 7529-7534 */
    halfpipes(p);                                /* 7535-7542 */
    teeters(p);                                  /* 7543-7548 */
    gachapons();                                 /* 7681-7702 */
    floating_lanterns(p);                        /* 7589-7604 */
    strobes();                                   /* 7670 */
    stars();                                     /* 7466 */
}
int fn_gima_enemy(int t) { return t == OT_FN_LAMP || t == OT_FN_BOOST; }

/* ================================================================ drawing: additive / scaled / depth-sorted members (objects_draw pass) */
static int on_screen(float sx, float sy, float m) { return sx > -m && sx < SCREEN_W + m && sy > -m && sy < SCREEN_H + m; }
void fn_gima_draw(int pass) {
    obj_t *o;
    if (!S.on) return;
    int want = pass == 0 ? 1 : pass == 1 ? 2 : 3;
    EACH(OT_FN_STROBE, o) {                      /* StrobeLight: set_shader(0x20) additive */
        if ((pass == 0 ? o->layer > 1 : pass == 1 ? o->layer != 2 : o->layer < 3) || !o->an.b) continue;
        float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy); if (!on_screen(sx, sy, 64)) continue;
        r_set_additive(1); frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, 0, 0xffffffff); r_set_additive(0);
    }
    if (want == 1) {
        EACH(OT_FN_TLIGHT, o) {                  /* trolley headlights: additive, x-scale 6.0 (RE ctor) */
            obj_t *c = NULL, *t; EACH(OT_FN_TROLLEY, t) c = t;
            if (!c || c->hidden || !o->an.b) continue;
            const frame_t *f = anim_frame(&o->an);
            float sx = floorf(o->x - G.camx) - f->hx * 6.f, sy = floorf(o->y - G.camy) - f->hy;
            if (sx > SCREEN_W || sx + f->w * 6.f < 0 || sy > SCREEN_H || sy + f->h < 0) continue;
            r_set_additive(1); frame_draw_scaled(o->an.b, f, sx, sy, f->w * 6.f, (float)f->h, 0xffffffff); r_set_additive(0);
        }
        if (S.flant_on) {                        /* floating lanterns: depth sorted by A (7591-7601 move_front), scale A */
            for (int step = 0; step <= 10; step++) {
                double A = 0.5 + 0.05 * step;
                EACH(OT_FN_FLANTERN, o) {
                    if (fabs(o->av[0] - A) > 0.001 || !o->an.b) continue;
                    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy); if (!on_screen(sx, sy, 64)) continue;
                    r_set_scale((float)A); frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, 0, 0, 0xffffffff); r_set_scale(1.f);
                }
            }
        }
    }
}
