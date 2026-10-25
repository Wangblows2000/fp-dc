/* Relic Maze acts 3-5 gimmicks (session 14): moving pillars + hanging switch, rising swing, pickaxe trap, spinny cup,
 * elevator, Milla's hand-car with its track switches and wall plates, rolling cave platform, drill platform, the RM4
 * gateway exit, the RM5 light stones / spike ceiling / crush boxes / chairs, static platforms, the Milla route split, the
 * RM4 loop-layer pieces. Source: build/re/spec_rm_gimmicks_b.md (static RE; section numbers below refer to it; no original
 * trace recorded yet). Labels: RE (read from code), INFERRED (reasoned, marked), UNKNOWN (spec U-number).
 * Breakable ruins use the aqua-brick rule (aqua.c bricks_frame), the door return and the jump-pad triggers are level data
 * (level_def_t.rmg, switches.c / carol_bike.inc). Everything runs after the movement block and the objects pass (spec 0:
 * in RM the q13 path platforms move after the player's movement), on index lists cached at spawn.
 * Object fields: par = family member (RMB_SUB); fl bits >= 0x1000 are this file's flags (bit 0 keeps the shared meaning
 * "ignored by the sensors", 0x40 the aqua-brick "break" flag on ruins; wall plates: bit 0 = pressed, flag 0). */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define RT0 OT_RM_PILLAR
#define NRT (OT_RM_CHAIR - OT_RM_PILLAR + 1)
#define LMAX 96
static short LST[NRT][LMAX]; static int NLS[NRT];
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define F1 0x1000
#define F2 0x2000
#define F3 0x4000
#define F4 0x8000
#define DEG 57.29577951308232

static struct {
    int cart, probe[3], cpart[2];          /* slots (-1 none) */
    int cart_ch, elev_ch, gate_ch;         /* looping samples (channels) */
    int chair;                             /* the chair M39 walks to */
    int plate_snd[8];                      /* click latches per plate (6151 / 6152 / 4506 / 5678 once per press) */
    uint32_t held0;                        /* last frame's buttons (CTRL.H edge) */
} S;

static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / DEG); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / DEG); }
static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0) { if (o->an.b != &G.ens || o->an.anim != a) anim_set(&o->an, &G.ens, a); } }
static void set_sk(obj_t *o, const char *fmt, int k) { char nm[20]; snprintf(nm, sizeof nm, fmt, k); set_s(o, nm); }
static obj_t *slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static int Mst(const player_t *p) { return player_orig_m(p); }
static double pB(const player_t *p) { return p->ground ? p->gsp : p->vx; }
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
/* the current frame's action point, rotated with the object (Chowdren get_action_x / y) */
static void act_pt(const obj_t *o, double *x, double *y) {
    *x = o->x; *y = o->y;
    if (!o->an.b || !o->an.b->act) return;
    const frame_t *f = anim_frame(&o->an); int fi = (int)(f - o->an.b->frames);
    double dx = o->an.b->act[2 * fi] - f->hx, dy = o->an.b->act[2 * fi + 1] - f->hy, a = o->ang;
    *x = floorf(o->x) + dx * c_cos(a) + dy * c_sin(a); *y = floorf(o->y) - dx * c_sin(a) + dy * c_cos(a);
}
/* a placed player sensor overlapping the object (box objects: image rect, others: rotated mask), dy: sensor moved down */
static int sens_hit(int s, const obj_t *o, int dy) {
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    oy += dy;
    if ((objdef(o->type)->flags & OD_BOXCOL) && o->ang == 0) {
        const frame_t *f = anim_frame(&o->an);
        int bx = (int)floorf(o->x) - f->hx, by = (int)floorf(o->y) - f->hy;
        for (int j = 0; j < h; j++) { int y = oy + j; if (b[j] < a[j] || y < by || y >= by + f->h) continue; if (ox + b[j] >= bx && ox + a[j] < bx + f->w) return 1; }
        return 0;
    }
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int disc_hit(const player_t *p, const obj_t *o) {   /* PLAYER (Player_326, the 27-px disc at (int X, int Y)) */
    static signed char x0[27], x1[27]; static int init;
    if (!init) { init = 1; for (int j = 0; j < 27; j++) { double d = 13.25 * 13.25 - (j - 13) * (j - 13); int w = d < 0 ? -1 : (int)sqrt(d); x0[j] = (signed char)(13 - w); x1[j] = (signed char)(13 + w); } }
    if (!o->an.b) return 0;
    return obj_rows_hit(o, (int)PX(p) - 13, (int)PY(p) - 13, x0, x1, 27);
}
static int csens_hit(const player_t *p, const obj_t *o) {   /* PlayerCharacterSensor_375 */
    int X = (int)PX(p), Y = (int)PY(p), small = player_char_sensor_small(p);
    return o->an.b && obj_rect_hit(o, X - 7, small ? Y - 13 : Y - 34, X + 9, small ? Y + 15 : Y + 13);
}
static int box_solid(int x0, int y0, int w, int h) {    /* a probe box on the background or a Q7 object */
    static const signed char r0[16] = { 0 }, r1[16] = { 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };
    for (int y = 0; y < h; y++) if (px_span_solid(&G.lvl, x0, x0 + w - 1, y0 + y)) return 1;
    return objects_sensor_hit(OBJGRP_SOLID, x0, y0, r0, r1, h);
}
/* the shared kill block (6361 / 6766 / 6767): GUIRings = 0, MV.R = 1, LIFE.C = 1, OTHERS.D = E = 0, MV.M = 9 */
static void kill_player(player_t *p) {
    if (p->state == ST_DEAD) return;
    p->hp = 0; p->state = ST_DEAD; p->dead_t = 0; p->vy = -5; p->vx = 0; p->gsp = 0; p->ground = 0; p->invinc = 0; p->invuln = 0; p->in_lock = 0;
}
static int r_hurt(player_t *p, int n) {          /* MV.R = 1, LIFE.B += n (2414 applies it next frame) */
    if (dead(p) || p->hurt_pending || player_invincible(p)) return 0;
    player_contact(p, n); return 1;
}
static void shake(int z) { camera_shake(z); }

/* ================================================================ generic path platforms (3283-3289) + carry (4061)
 * av[0] A, av[1] E, av[2] B, av[3] F, av[4] C, av[5] G, kx I, ky J, c: bit 0 D (sin for x), bit 1 H (sin for y);
 * av[6] / av[7] last M / N, vx / vy this frame's O / P */
static void path_init(obj_t *o, double A, double E, double B, double F, double C, double Gm, int D, int H) {
    o->kx = o->x; o->ky = o->y; o->av[0] = A; o->av[1] = E; o->av[2] = B; o->av[3] = F; o->av[4] = C; o->av[5] = Gm; o->c = D | (H << 1);
    o->av[6] = o->kx + C * ((o->c & 1) ? c_sin(A) : c_cos(A)); o->av[7] = o->ky + Gm * ((o->c & 2) ? c_sin(E) : c_cos(E));
    o->vx = o->vy = 0;
}
static void path_place(obj_t *o) {               /* 3284-3289 after A += B, E += F */
    double M = o->kx + o->av[4] * ((o->c & 1) ? c_sin(o->av[0]) : c_cos(o->av[0]));
    double N = o->ky + o->av[5] * ((o->c & 2) ? c_sin(o->av[1]) : c_cos(o->av[1]));
    o->vx = (float)(floor(M) - floor(o->av[6])); o->vy = (float)(floor(N) - floor(o->av[7]));
    o->av[6] = M; o->av[7] = N; o->x = (float)floor(M); o->y = (float)floor(N);
}
static void carry(player_t *p) {                 /* 4061: G >= 0, S_BB 1 px lower on a q13 platform */
    static const int Q13[] = { OT_RM_RSWING, OT_RM_MOVPLAT, OT_RM_ELEV2, OT_RM_RCAVE };
    if (!(p->vy >= 0 || p->ground) || dead(p) || p->state == ST_HANG || p->state == ST_CART || p->state == ST_HELD) return;
    for (unsigned q = 0; q < sizeof Q13 / sizeof Q13[0]; q++) {
        if (!NLS[Q13[q] - RT0]) continue;
        int k = mv_sensor_obj(p->x, MY(p) + 1, p->N, SENSOR_BOTTOMBIG, Q13[q]);
        if (k < 0) continue;
        p->x += G.o[k].vx; p->y += G.o[k].vy;
        if (!p->ground) { p->gsp = p->vx; p->ground = 1; }
        p->vy = 0; p->N = 0; p->O = 0;
        player_sync_sensors(p);
        break;
    }
}

/* ================================================================ spawn */
static int milla(void) { return chr_is_milla(G.character); }   /* MV.A = 4 */
static obj_t *nearest(int T, float x, float y, float r) {
    obj_t *o, *best = NULL; float bd = r;
    EACH(T, o) { float d = fabsf(o->x - x) + fabsf(o->y - y); if (d <= bd) { bd = d; best = o; } }
    return best;
}
void relic_gimb_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    else { if (S.cart_ch >= 0) snd_loop_stop(S.cart_ch); if (S.elev_ch >= 0) snd_loop_stop(S.elev_ch); if (S.gate_ch >= 0) snd_loop_stop(S.gate_ch); }
    memset(&S, 0, sizeof S); memset(NLS, 0, sizeof NLS);
    S.cart = S.probe[0] = S.probe[1] = S.probe[2] = S.cpart[0] = S.cpart[1] = -1; S.cart_ch = S.elev_ch = S.gate_ch = -1; S.chair = -1;
    if (!objects_have_from(RT0)) return;
    /* family member index: the k-th object of a type is the k-th placement of that type (objects_spawn keeps the order) */
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < RT0 || o->type > OT_RM_CHAIR) continue;
        int t = o->type - RT0, n = 0, sub = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { sub = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = sub < 16 ? sub : 0;
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = 0; o->c = 0; o->dir = 0; o->ang = 0; o->layer = 2; memset(o->av, 0, sizeof o->av);
        if (o->x < 0 || o->y < 0 || o->x >= G.lvl.w || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        if (NLS[t] < LMAX) LST[t][NLS[t]++] = (short)i;
    }
    const rmgim_t *R = level_cur()->rmg;
    obj_t *o;
    int npx = 0, nsw = 0, nsw_v = 0, nrs = 0, nrp = 0;
    EACH(OT_RM_PILLAR, o) {                                                    /* 6378-6385 / 6527-6531: B = y, A = 0, C by zone */
        set_sk(o, "pil%d", o->par);
        for (int z = 0; R && z < R->npillars; z++) {
            const pillarzone_t *Z = &R->pillars[z];
            if (o->x >= Z->x0 && o->x < Z->x1 && o->y >= Z->y0 && o->y < Z->y1) o->av[2] = Z->C;
        }
    }
    EACH(OT_RM_IDLESW, o) { o->av[0] = o->y; o->hidden = 1; o->tm = nsw++; }   /* invisible; start: A = y */
    EACH(OT_RM_HSWITCH, o) { set_s(o, "hswitch"); o->tm = nsw_v++; }         /* 6081: paired by index */
    EACH(OT_RM_RSWING, o) nrs++;
    { int k = 0; EACH(OT_RM_RSWING, o) { set_s(o, "rswing"); path_init(o, 0, 0, 0, 0, -96, 96, 1, 0); o->tm = nrs - 1 - k++; } }   /* ctor C -96 D 1 G 96; R = spread index (newest first) */
    EACH(OT_RM_RSPIECE, o) nrp++;
    { int k = 0; EACH(OT_RM_RSPIECE, o) { set_s(o, "rspiece"); o->tm = nrp - 1 - k++; } }   /* 6372: A = spread index, I = A mod 4, B = A div 4 */
    EACH(OT_RM_PICKAXE, o) { set_s(o, "pickaxe"); o->av[3] = 15 * c_sin(45. * npx++) + 15; }   /* 6371: D = 15 sin(45 k) + 15 (k order INFERRED, U9) */
    EACH(OT_RM_PXPART, o) set_sk(o, "pxpart%d", o->par);
    EACH(OT_RM_MARKER, o) { set_s(o, "marker2"); o->hidden = 1; }
    EACH(OT_RM_CUP, o) set_s(o, "cup");
    EACH(OT_RM_ELEV, o) set_s(o, "elev");
    EACH(OT_RM_ELEV2, o) { set_s(o, "elev2"); path_init(o, 0, 0, 0, 0, 0, 0, 0, 0); o->A = 1; o->fx = 2300; }   /* ctor Q 1, R 2300; G = 0: Y = J */
    /* 7. the hand-car exists for Milla only (6374: MV.A != 4 destroys the cart, its parts and the track switches) */
    EACH(OT_RM_CART, o) { if (!milla()) { o->alive = 0; continue; } set_s(o, "cart"); o->kx = o->x; S.cart = (int)(o - G.o); }   /* 6373: C = x */
    EACH(OT_RM_CARTPART, o) { if (!milla()) { o->alive = 0; continue; } set_sk(o, "cartpart%d", o->par); S.cpart[o->par & 1] = (int)(o - G.o); }
    EACH(OT_RM_CARTPROBE, o) { if (!milla()) { o->alive = 0; continue; } set_sk(o, "cartprobe%d", o->par); o->hidden = 1; if (o->par < 3) S.probe[o->par] = (int)(o - G.o); }
    EACH(OT_RM_TSWITCH, o) { if (!milla()) { o->alive = 0; continue; } set_s(o, "tswitch"); o->av[1] = -1; }   /* ctor B -1 */
    EACH(OT_RM_WPLATE, o) set_sk(o, "rmplate%d", o->par);
    EACH(OT_RM_FLOATBOX, o) { set_sk(o, "floatbox%d", o->par); o->hidden = 1; }   /* invisible obstacle floors (11) */
    EACH(OT_RM_MOVPLAT, o) { set_s(o, "movplat"); path_init(o, 0, 0, 2, 2, -40, 40, 0, 0); }   /* ctor B 2 C -40 F 2 G 40 */
    EACH(OT_RM_FLOORBOX, o) { set_s(o, "floorbox"); o->hidden = 1; if (G.at_g4 >= 10) o->alive = 0; }   /* 6196: removed on a restart past the boss */
    EACH(OT_RM_ALTPATH, o) {                                                   /* 6203 / 6204: the Milla route split (RE) */
        if ((o->par == 0) == milla()) { o->alive = 0; continue; }
        set_sk(o, "altpath%d", o->par);
    }
    EACH(OT_RM_RCAVE, o) { set_s(o, "rcave"); path_init(o, 180, 180, 0, 0, 192, -192, 0, 1); }   /* ctor A 180 C 192 E 180 G -192 H 1 */
    EACH(OT_RM_RCAVE2, o) { set_s(o, "rcave2"); obj_t *pl = nearest(OT_RM_RCAVE, o->x, o->y, 4); o->tm = pl ? (int)(pl - G.o) : -1; }
    EACH(OT_RM_DRILL, o) { set_s(o, "drill"); o->fl |= 1; }                     /* 6524: flag 0 on (not solid while it digs) */
    EACH(OT_RM_RUINS, o) set_s(o, "ruins");
    EACH(OT_RM_RMPLAT, o) set_s(o, "rmplat");
    EACH(OT_RM_GATE, o) set_s(o, "gate");
    EACH(OT_RM_GATE2, o) set_s(o, "gate2");
    EACH(OT_RM_CRATER, o) set_sk(o, "crater%d", o->par);
    EACH(OT_RM_LOOP0, o) set_sk(o, "rmloop0_%d", o->par);
    EACH(OT_RM_LOOP1, o) set_sk(o, "rmloop1_%d", o->par);
    EACH(OT_RM_LSBASE, o) set_sk(o, "lsbase%d", o->par);
    EACH(OT_RM_LSTOP, o) set_sk(o, "lstop%d", o->par);
    EACH(OT_RM_LSDOOR, o) set_sk(o, "lsdoor%d", o->par);
    EACH(OT_RM_SCEIL, o) set_s(o, "sceil0");
    EACH(OT_RM_CRUSHBOX, o) { set_s(o, "crushbox"); o->hidden = 1; }
    EACH(OT_RM_CHAIR, o) set_s(o, "chair");
}
/* the Bone Golem's gate pillars (RM5 GimmickMovingPillar1 / 3 without a zone): no travel of their own (relic_boss.c moves them) */
int relic_gimb_is_gate_pillar(const obj_t *o) { return o->type == OT_RM_PILLAR && o->par != 1 && o->av[2] == 0 && o->av[1] == 0; }

/* ================================================================ 2. hanging switch -> moving pillars */
static void hang_switches(player_t *p) {
    obj_t *o, *v;
    EACH(OT_RM_IDLESW, o) {
        int held = p->state == ST_HANG && p->hang_x == o->x && p->hang_y == o->y;   /* Q > 0: a player hangs on it */
        double tx = PX(p), ty = PY(p) - 13;                                    /* Q23 grab (4065, as OT_HANG) */
        if (!held && player_can_hang(p) && tx + 6 > o->x - 25 && tx - 5 < o->x + 25 && ty + 6 > o->y && ty - 5 < o->y + 15) { player_hang(p, o->x, o->y); held = 1; }
        double A = o->av[0];
        if (!held) { o->y = (float)fmax(A, o->y - 2); o->fl &= ~F1; }        /* 5866 */
        else if (o->y < A + 8) { o->y = (float)fmin(A + 8, o->y + 2); p->hang_y = o->y; p->y += 2; }   /* 5867: MV.Y += 2 */
        else if (!(o->fl & F1)) o->fl |= F1;                                   /* 5868: pulled */
        EACH(OT_RM_HSWITCH, v) if (v->tm == o->tm) v->y = o->y;                /* 6081 */
    }
}
static void pillars(player_t *p) {
    obj_t *o, *s;
    if (!NLS[OT_RM_PILLAR - RT0]) return;
    EACH(OT_RM_IDLESW, s) s->fl &= ~F2;                                        /* 5998 */
    int moving = 0, last = 0;
    EACH(OT_RM_PILLAR, o) {                                                    /* 6353-6358: D > 0 up 4, D < 0 down 4 */
        double D = o->av[1];
        if (o->par == 0 && D >= -2 && D <= 2 && D != 0) last = 1;              /* 6359 */
        if (D > 0) { o->av[1] -= 2; o->y -= 4; moving = 1; }
        else if (D < 0) { o->av[1] += 2; o->y += 4; moving = 1; }
    }
    if (moving) { EACH(OT_RM_IDLESW, s) s->fl |= F2; }
    if (last) { snd_play_orig(51, 220, 128); shake(2); }
    if (!dead(p) && moving) {                                                  /* 6360-6362: S_BB 3 px down on a Pillar1 with S_TB blocked, or S_TB on a Pillar3 with S_BB blocked */
        int tz = mv_z(SENSOR_TOPBIG), bz = mv_z(SENSOR_BOTTOMBIG), crush = 0;
        EACH(OT_RM_PILLAR, o) {
            if (fabsf(o->x - (float)p->x) > 80 || fabsf(o->y - (float)MY(p)) > 120) continue;
            if ((o->par == 0 && tz && sens_hit(SENS_BOTTOMBIG, o, 3)) || (o->par == 2 && bz && sens_hit(SENS_TOPBIG, o, 0))) crush = 1;
        }
        if (crush) kill_player(p);
    }
    int go = 0;
    EACH(OT_RM_IDLESW, s) if ((s->fl & F1) && !(s->fl & F2) && !(s->fl & F3)) { go = 1; s->fl |= F3; }   /* 6363 */
    if (go) { EACH(OT_RM_PILLAR, o) { o->av[1] = o->av[2]; o->av[2] = -o->av[2]; } snd_play_orig(62, 220, 128); }
    EACH(OT_RM_IDLESW, s) if (!(s->fl & (F1 | F2)) && (s->fl & F3)) s->fl &= ~F3;   /* 6007: re-armed */
}

/* ================================================================ 3. rising swing (6123-6130, chain 6065) */
static void swings(player_t *p) {
    obj_t *o, *k;
    EACH(OT_RM_RSWING, o) {
        int on = !dead(p) && sens_hit(SENS_BOTTOMBIG, o, 0);
        double A = o->av[0], *B = &o->av[2];
        if (on && p->ground && !(o->fl & (F1 | F2))) o->fl |= F1;              /* 6124 */
        if (o->fl & F1) {
            if (A < 90) *B = fmin(*B + 0.08, 8); else *B = fmax(*B - 0.08, 0);  /* 6125 / 6126 */
            if (A >= 90 && *B <= 0 && !on) { o->fl &= ~F1; o->fl |= F2; }       /* 6127 */
        } else if (o->fl & F2) {
            if (A >= 90) *B = fmax(*B - 0.08, -8); else *B = fmin(*B + 0.08, 0);   /* 6128 / 6129 */
            if (A < 90 && *B >= 0) o->fl &= ~F2;                               /* 6130 */
        }
        o->av[3] = *B;
    }
    EACH(OT_RM_RSPIECE, k) {                                                   /* 6065: four links evenly between the pivot and the platform */
        int R = k->tm / 4, I = k->tm % 4;
        EACH(OT_RM_RSWING, o) if (o->tm == R) {
            const double W = 20, Sx = (o->x - o->kx - 5 * W) / 5, Sy = (o->y - o->ky - 5 * W) / 5;
            k->x = (float)floor(o->kx + Sx + W + I * (Sx + W)); k->y = (float)floor(o->ky + Sy + W + I * (Sy + W));
        }
    }
}

/* ================================================================ 4. pickaxe (6102-6119) */
static void pickaxes(player_t *p) {
    obj_t *o, *m, *h;
    EACH(OT_RM_PICKAXE, o) {
        EACH(OT_RM_MARKER, m) if (obj_obj_hit(o, m)) { o->av[4] += 90; m->alive = 0; }   /* 6102 */
        double *B = &o->av[1], *D = &o->av[3]; int *C = &o->tm;
        o->ang = (float)(*B + o->av[4]);
        if (*C == 0) {                                                         /* 6106 / 6107 */
            double d0 = *D; *D += 1; *B += 0.25 * d0; o->ang = (float)(*B + o->av[4]);
            if (*B > -45 && obj_overlaps_bg(o)) { *C = 1; *D = 0; o->dir = 0; }
        }
        if ((*C > 0 && *C < 80) || *C > 80) *C += 1;                           /* 6108 */
        if (*C > 60 && *C < 80) o->dir = 1;                                    /* 6109 */
        if (*C == 80) {                                                        /* 6110 / 6111 */
            double d0 = *D; *D -= 1; *B += 0.25 * d0; o->ang = (float)(*B + o->av[4]);
            if (*B < 45 && obj_overlaps_bg(o)) { *C = 81; *D = 0; o->dir = 0; }
        }
        if (*C > 140) o->dir = 1;                                              /* 6112 */
        if (*C >= 160) *C = 0;                                                 /* 6113 */
        int bg = ((*C > 0 && *C < 80) || *C > 80) && obj_near_view(o, 180) && obj_overlaps_bg(o);
        if (bg && !(o->fl & F1)) { snd_play_orig(75, 220, 128); shake(3); o->fl |= F1; }   /* 6114-6117 */
        if (!bg) o->fl &= ~F1;                                                 /* 6118 (INFERRED: the latch follows the same test) */
        set_s(o, o->dir ? "pickaxe_b" : "pickaxe");
        if (!dead(p) && !player_invincible(p) && csens_hit(p, o)) r_hurt(p, 10);   /* 6119: 2 petals on Normal */
        EACH(OT_RM_PXPART, h) {                                                /* 6103-6105 */
            if (h->par == 0 && fabsf(h->ox - o->ox) < 8 && fabsf(h->oy - o->oy) < 8) { h->x = o->x; h->y = o->y; h->ang = o->ang; h->dir = o->dir; }
        }
    }
    EACH(OT_RM_PXPART, h) if (h->par > 0) {
        obj_t *hd; int d = 0;
        EACH(OT_RM_PXPART, hd) if (hd->par == 0 && obj_obj_hit(h, hd)) d = hd->dir;
        set_s(h, h->par == 1 ? (d ? "pxpart1_b" : "pxpart1") : (d ? "pxpart2_b" : "pxpart2"));
    }
}

/* ================================================================ 5. spinny cup (6120-6122) */
static void cups(player_t *p) {
    obj_t *o;
    EACH(OT_RM_CUP, o) {
        double ax, ay; act_pt(o, &ax, &ay);
        if (!(o->fl & F1) && o->av[0] == 0 && !dead(p) && p->state != ST_HELD && pB(p) >= 0 && o->x < PX(p) && PY(p) <= ay + 15 && disc_hit(p, o)) {   /* 6120 */
            o->av[1] = -0.5 * pB(p); o->fl |= F1;
            p->state = ST_HELD; p->state_t = 0; p->gsp = p->vx = 0; p->vy = 0; snd_play_orig(7, 220, 128);   /* M = 43 */
        }
        if (o->fl & F1) {                                                      /* 6121 */
            o->av[1] = fmax(o->av[1] - 0.05, -12); o->av[0] += o->av[1]; o->ang = (float)o->av[0];
            act_pt(o, &ax, &ay); p->x = ax; p->y = ay + ORIG_REF_DY; player_sync_sensors(p);
            if (o->av[0] <= -90) {                                             /* 6122: one use per act */
                o->av[0] = -90; o->ang = -90; o->fl &= ~F1; o->av[1] = 0; snd_play_orig(51, 220, 128);
                act_pt(o, &ax, &ay);
                if (p->state == ST_HELD) { p->state = ST_NORMAL; p->state_t = 0; p->ground = 0; p->gsp = p->vx = 0; p->vy = 1; p->m1 = 0; }
                obj_t *h = obj_alloc();                                        /* a GimmickHangingIdleSensor_674 at the action point */
                if (h) { h->type = OT_HANG; h->alive = 1; h->hp = 1; h->layer = o->layer; h->x = h->ox = (float)floor(ax); h->y = h->oy = (float)floor(ay); }
            }
        }
    }
}

/* ================================================================ 6. elevator (6167-6172) */
static void elevators(player_t *p) {
    obj_t *o, *m;
    EACH(OT_RM_ELEV2, o) {
        if (!(o->fl & F1) && !dead(p) && p->ground && sens_hit(SENS_BOTTOMBIG, o, 1)) {   /* 6167 */
            o->fl |= F1; EACH(OT_RM_ELEV, m) set_s(m, "elev_on");
            if (S.elev_ch < 0) S.elev_ch = snd_loop_orig(262, 220, 22050);
        }
        if (!(o->fl & F1)) continue;
        o->ky += o->A;                                                         /* 6169: J += Q */
        if (o->ky < o->fx) o->A = fmin(o->A + 0.02, 12); else o->A = fmax(o->A - 0.06, 0);   /* 6170 / 6171 */
        if (o->ky >= o->fx && o->A == 0 && !(o->fl & F2)) {                    /* 6172 */
            o->fl |= F2; EACH(OT_RM_ELEV, m) set_s(m, "elev");
            if (S.elev_ch >= 0) { snd_loop_stop(S.elev_ch); S.elev_ch = -1; }
        }
    }
}
static void elev_follow(void) { obj_t *o, *m; EACH(OT_RM_ELEV2, o) { EACH(OT_RM_ELEV, m) if (fabsf(m->x - o->x) < 8) m->y = o->y - 2; } }   /* 6169: machine y = floor y (placements 966 / 968) */

/* ================================================================ wall plates (5900 / 6142-6152 / 6432 / 6580-6583) */
/* sub 0 SwitchWallPlate_1149 (RM3): S_RB, M 11 / 47, shown every frame; 1 WallPlate2_1429 (RM3): S_LB, M 11 / 47; both also
 * pressed by the hand-car's bumper (6145 / 6149). 2 WallPlate_1526 (RM4 gateway): S_RB, M 11 / 47, permanent (6432).
 * 3 WallPlate_1543 (RM5 stone 1): S_LB, M 11 / 47 / 26; 4 WallPlate2_1578 (RM5 stone 2): S_RB, M 11 / 47 / 26 (6580-6583) */
static const struct { signed char sens, cyc, perm, cart; } PLATE[5] = { { SENS_RIGHTBIG, 0, 0, 1 }, { SENS_LEFTBIG, 0, 0, 1 }, { SENS_RIGHTBIG, 0, 1, 0 },
                                                                       { SENS_LEFTBIG, 1, 0, 0 }, { SENS_RIGHTBIG, 1, 0, 0 } };
static obj_t *plate_of(int sub) { obj_t *o; EACH(OT_RM_WPLATE, o) if (o->par == sub) return o; return NULL; }
static int plate_on(int sub) { obj_t *o = plate_of(sub); return o && (o->fl & 1); }
static void plates(player_t *p) {
    obj_t *o, *f3 = slot(S.probe[2]);
    int M = Mst(p);
    EACH(OT_RM_WPLATE, o) {
        int k = o->par < 5 ? o->par : 0;
        if (!PLATE[k].perm) { o->hidden = 0; o->fl &= ~1; }                    /* shown, flag 0 off every frame */
        int push = !dead(p) && (M == 11 || M == 47 || (PLATE[k].cyc && M == 26)) && sens_hit(PLATE[k].sens, o, 0);
        int cart = PLATE[k].cart && f3 && obj_obj_hit(f3, o);
        if (push || cart) { o->hidden = 1; o->fl |= 1; }
        int on = (o->fl & 1) != 0, j = (int)(o - G.o) & 7;
        if (on && !S.plate_snd[j]) snd_play_orig(49, 220, 128);               /* click once per press */
        S.plate_snd[j] = on;
    }
}

/* ================================================================ 7. Milla's hand-car (6131-6166, 6052-6059) */
static void wheel_settle(obj_t *w) {             /* loop_cart_floorcheck_26 (6162: up to 12 passes): fall onto, then out of, backdrop / Q7 */
    int falling = 1;
    for (int k = 0; k < 12; k++) {
        int hit = box_solid((int)w->x - 4, (int)w->y - 4, 8, 8);
        if (falling) { if (!hit) { w->y += 1; continue; } falling = 0; }
        if (hit) w->y -= 1; else break;
    }
}
static void spike_bits(const obj_t *s, double X, double Y) {   /* 6158: 4 BreakableSpike_1436 pieces */
    for (int k = 0; k < 4; k++) {
        obj_t *q = obj_alloc(); if (!q) return;
        q->type = OT_RM_SPIKEBIT; q->alive = 1; q->hp = 1; q->layer = 3; set_s(q, "spikebit");
        q->x = s->x + 4 + 8 * k; q->y = s->y + 16; q->fx = q->x; q->fy = q->y;
        q->av[0] = X - 2 + (cross_rand_raw(0x10000) >> 13 & 3); q->av[1] = Y - 4 + (cross_rand_raw(0x10000) >> 13 & 3); q->av[2] = 0.3;
        int t = OT_RM_SPIKEBIT - RT0; if (NLS[t] < LMAX) LST[t][NLS[t]++] = (short)(q - G.o); else { int i; for (i = 0; i < LMAX; i++) if (!G.o[LST[t][i]].alive || G.o[LST[t][i]].type != OT_RM_SPIKEBIT) { LST[t][i] = (short)(q - G.o); break; } }
    }
}
static void cart_point(const obj_t *c, double ox, double oy, double *x, double *y) {   /* action point + (ox cos A + oy sin A, oy cos A - ox sin A) */
    double ax, ay, A = c->av[2]; act_pt(c, &ax, &ay);
    *x = ax + ox * c_cos(A) + oy * c_sin(A); *y = ay + oy * c_cos(A) - ox * c_sin(A);
}
static void cart_frame(player_t *p) {
    obj_t *c = slot(S.cart), *w0 = slot(S.probe[0]), *w1 = slot(S.probe[1]), *f3 = slot(S.probe[2]), *hd = slot(S.cpart[0]), *tp = slot(S.cpart[1]), *o;
    if (!c || c->type != OT_RM_CART) return;
    int M90 = p->state == ST_CART;
    double *B = &c->av[1], A = c->av[2];
    if (!M90 && !dead(p) && p->ground && Mst(p) != 90 && hd && disc_hit(p, hd) && PX(p) < hd->x - 20) {   /* 6132: mount */
        p->state = ST_CART; p->state_t = 0; M90 = 1;
        p->mF = p->mG = p->mH = 0;                                             /* 6131: float state cleared */
    }
    if (M90) {
        if (p->held & BTN_RIGHT) *B += 0.15;                                   /* 6133 */
        if (p->held & BTN_LEFT) *B -= 0.15;                                    /* 6134 */
        c->fl |= F2;                                                           /* 6135 */
    } else c->fl &= ~F2;                                                       /* 6137 */
    *B = (*B - 0.003 * A) * 0.98;                                              /* 6138: friction / slope */
    if (M90) {                                                                 /* 6139-6141: sample 260, volume and pitch from B */
        int vol = (int)(fmin(100 * fabs(*B), 100) * 255 / 100), fr = (int)(22050 + 5000 * fabs(*B));
        if (S.cart_ch < 0) S.cart_ch = snd_loop_orig(260, vol, fr); else snd_loop_update(S.cart_ch, vol, fr);
    } else if (S.cart_ch >= 0) { snd_loop_stop(S.cart_ch); S.cart_ch = -1; }
    if (f3) {                                                                  /* 6142 / 6146: the front bumper */
        double x, y; cart_point(c, 45, 4, &x, &y); f3->x = (float)floor(x); f3->y = (float)floor(y);
        if (px_span_solid(&G.lvl, (int)f3->x - 4, (int)f3->x + 3, (int)f3->y) || px_span_solid(&G.lvl, (int)f3->x - 4, (int)f3->x + 3, (int)f3->y - 3) ||
            px_span_solid(&G.lvl, (int)f3->x - 4, (int)f3->x + 3, (int)f3->y + 3)) { *B *= -0.5; shake(2); }   /* 6143 / 6147: bounces off walls */
    }
    for (int i = 0; i < G.nobjs; i++) {                                        /* 6144 / 6148, 6156-6158: floor spikes */
        obj_t *s = &G.o[i];
        if (!s->alive || s->type != OT_SPIKES || !s->an.b || fabsf(s->x - c->x) > 96 || fabsf(s->y - c->y) > 64) continue;
        double b = *B;
        if (f3 && obj_obj_hit(f3, s) && fabs(b) <= 3 && b != 0) *B *= -0.5;     /* slow carts bounce off spikes */
        else if (fabs(b) > 3 && obj_obj_hit(c, s)) {                           /* a fast cart smashes them */
            obj_t sv = *s; s->alive = 0; snd_play_orig(13, 220, 128); shake(2); spike_bits(&sv, b, -2);
        }
    }
    if (w0 && w1) {                                                            /* 6161 / 6162: wheels at action x -+ 28 cos A, settled */
        double ax, ay; act_pt(c, &ax, &ay);
        w0->x = (float)floor(ax - 28 * c_cos(A)); w1->x = (float)floor(ax + 28 * c_cos(A));
        wheel_settle(w0); wheel_settle(w1);
        c->kx += *B;                                                           /* 6163 */
        double dx = w1->x - w0->x, dy = w1->y - w0->y;
        c->av[2] = A = dx != 0 ? -fd_atan(dy / dx) * DEG : 0; c->ang = (float)A;
        c->x = (float)c->kx; c->y = (float)floor((w0->y + w1->y) / 2);
    }
    if (tp) { double x, y; act_pt(c, &x, &y); tp->x = (float)floor(x); tp->y = (float)floor(y); tp->ang = (float)A; }   /* Top / Handle follow the action points */
    if (hd && tp) { double x, y; act_pt(tp, &x, &y); hd->x = (float)floor(x); hd->y = (float)floor(y); hd->ang = (float)A; }
    if (M90) {                                                                 /* 6164: Milla at action + (-35 cos A - 14 sin A, -14 cos A + 35 sin A) */
        double x, y; cart_point(c, -35, -14, &x, &y);
        p->x = x; p->y = y + ORIG_REF_DY; p->vy = 0; p->ground = 1; p->N = p->O = 0; p->rot = (float)fmod(A + 360, 360); player_sync_sensors(p);
    }
    /* track switches (6153-6155): a pressed cart plate toggles both rails between 0 and 25 degrees (1 per frame) */
    int any_idle = 0;
    EACH(OT_RM_TSWITCH, o) if (o->st == 0) any_idle = 1;
    if ((plate_on(0) || plate_on(1)) && any_idle) { EACH(OT_RM_TSWITCH, o) { o->st = 1; o->av[1] = -o->av[1]; } }
    EACH(OT_RM_TSWITCH, o) if (o->st == 1) {
        o->av[0] += o->av[1]; o->ang = (float)o->av[0];
        if ((o->av[1] <= 0 && o->av[0] <= 0) || (o->av[1] > 0 && o->av[0] >= 25)) o->st = 0;
    }
    EACH(OT_RM_SPIKEBIT, o) {                                                  /* 6159 / 6160 / test_collisions_26 */
        o->fx += (float)o->av[0]; o->av[1] += o->av[2]; o->fy += (float)o->av[1]; o->x = (float)(int)o->fx; o->y = (float)(int)o->fy;
        if (o->y > G.camy + SCREEN_H + 64 || (o->av[1] > 0 && obj_overlaps_bg(o))) o->alive = 0;
    }
}

/* ================================================================ 8. rolling cave platform (6406-6414, 6448); 11. moving platform 01 */
static void path_platforms(player_t *p) {
    obj_t *o, *w;
    EACH(OT_RM_RSWING, o) {                                                    /* 3283 + 6123: A, E within [0, 180] */
        o->av[0] = fmin(fmax(o->av[0] + o->av[2], 0), 180); o->av[1] = fmin(fmax(o->av[1] + o->av[3], 0), 180); path_place(o);
    }
    EACH(OT_RM_MOVPLAT, o) { o->av[0] = fmod(o->av[0] + o->av[2], 360); o->av[1] = fmod(o->av[1] + o->av[3], 360); path_place(o); }
    EACH(OT_RM_RCAVE, o) {
        /* states (U6: the half-arc limits are INFERRED): S = 1 when she lands on it at rest on the left rim (A 180), it rolls
         * through the trough (accelerating 0.05 to 270, braking after) to the right rim (A 360); S = -1 back once she is off it */
        int on = !dead(p) && p->ground && sens_hit(SENS_BOTTOMBIG, o, 1);
        double A = o->av[0], *B = &o->av[2];
        if (!on) o->fl &= ~F1;                                                 /* 6406 */
        if (o->st == 0 && on && A > 90 && A < 270 && !(o->fl & F1)) { o->fl |= F1; o->st = 1; }   /* 6407 */
        if (o->st == 0 && !on && A > 270) { o->fl |= F1; o->st = -1; }         /* 6408 */
        if (o->st == 1) *B = A < 270 ? fmin(*B + 0.05, 6) : fmax(*B - 0.05, 0);    /* 6409 / 6410 */
        else if (o->st == -1) *B = A > 270 ? fmax(*B - 0.05, -6) : fmin(*B + 0.05, 0);   /* 6412 / 6413 */
        if (*B == 0 && o->st != 0 && (o->st == 1 ? A >= 270 : A <= 270)) o->st = 0;   /* 6414 */
        o->av[3] = *B; o->av[0] += *B; o->av[1] = o->av[0]; path_place(o);
    }
    EACH(OT_RM_ELEV2, o) {                                                     /* G = 0: Y = J */
        double N = o->ky; o->vx = 0; o->vy = (float)(floor(N) - floor(o->av[7])); o->av[7] = N; o->y = (float)floor(N);
    }
    carry(p);                                                                  /* 4061 */
    EACH(OT_RM_RCAVE2, w) {                                                    /* 6448: the wheel at the platform, rolling */
        obj_t *pl = slot(w->tm); if (!pl || pl->type != OT_RM_RCAVE) continue;
        w->x = pl->x; w->y = pl->y; w->ang = (float)fmod(w->ang - 4 * pl->av[2] + 720, 360);
    }
}

/* ================================================================ 9. drill platform (6446, 6447, 6387-6391) */
static void drills(player_t *p) {
    obj_t *o, *r;
    EACH(OT_RM_DRILL, o) {
        if (!(o->fl & F1) && fabs(o->x - PX(p)) <= 239 && o->y < G.camy - 80) o->fl |= F1;   /* 6446 */
        if (!(o->fl & F1) || (o->fl & F2)) continue;
        if (o->tm <= 0) { o->y += 10; o->av[0] += 10; }                        /* 6387: 10 px a frame */
        float x0 = o->x - 48, x1 = o->x + 48, y0 = o->y - 16, y1 = o->y + 216;
        EACH(OT_RM_RUINS, r) {                                                 /* 6388 / 6389: ruins broken (aqua.c 4075 path), 3-frame pause */
            if ((r->fl & 0x40) || r->x + 64 <= x0 || r->x >= x1 || r->y + 64 <= y0 || r->y >= y1) continue;
            r->fl |= 0x40; r->av[0] = 0; r->av[1] = -4 - (cross_rand_raw(0x10000) >> 13 & 3); o->tm = 3; o->fl |= F3;
        }
        if (o->tm > 0) o->tm -= 1;                                             /* 6390 */
        if (o->av[0] >= 1440) {                                                /* 6391: done, solid, two red springs on top */
            o->fl |= F2; o->fl &= ~1; shake(6); snd_play_orig(51, 220, 128); set_s(o, "drill_done");
            for (int k = 0; k < 2; k++) {
                obj_t *s = obj_alloc(); if (!s) break;
                s->type = OT_RSPRING_U; s->alive = 1; s->hp = 1; s->layer = 3; s->x = s->ox = o->x + (k ? 8 : -40); s->y = s->oy = o->y - 16;
                int a = ens_anim("rs_up"); if (a >= 0) anim_set(&s->an, &G.ens, a);
            }
        }
    }
}

/* ================================================================ 14. RM4 gateway (6432-6434, 6466-6468) */
static void gateway(player_t *p) {
    obj_t *pl = plate_of(2), *g, *f = NULL, *o;
    if (!pl) return;
    EACH(OT_RM_GATE, o) { f = o; break; }
    EACH(OT_RM_GATE2, g) {
        int pressed = (pl->fl & 1) != 0;
        if (pressed && !(pl->fl & F1)) { pl->fl |= F1; if (S.gate_ch < 0) S.gate_ch = snd_loop_orig(62, 200, 22050); }   /* motor (5678 analog, INFERRED) */
        if (pressed && pl->c < 80 && !(g->fl & F1)) {                          /* 6434: the leaf sinks 80 px in 40 frames */
            pl->c += 2; g->y += 2;
            if (pl->c >= 80) { if (S.gate_ch >= 0) { snd_loop_stop(S.gate_ch); S.gate_ch = -1; } snd_play_orig(17, 220, 128); shake(3); }   /* 6433 */
        }
        if (!dead(p) && p->x <= g->x && pressed && !(g->fl & F1)) {           /* 6466: enter */
            g->fl |= F1 | F2; p->in_lock = 1; p->gsp = p->vx = 0; p->x = g->x; if (p->state != ST_HANG) p->state = ST_NORMAL;
            if (f) f->layer = 3;                                               /* GatewayFront move_front: in front of the player */
            if (S.gate_ch >= 0) snd_loop_stop(S.gate_ch);
            S.gate_ch = snd_loop_orig(62, 200, 22050);
        }
        if (g->fl & F1) {
            if (f) g->y = fmaxf(g->y - 2, f->y);                               /* 6467: closes behind her */
            p->gsp = p->vx = 0; p->x = g->x;
        }
        if ((g->fl & F2) && f && g->y <= f->y) {                               /* 6468: closed -> the fade and the next act (GV[4] = -1) */
            g->fl &= ~F2; shake(3); snd_play_orig(51, 220, 128);
            if (S.gate_ch >= 0) { snd_loop_stop(S.gate_ch); S.gate_ch = -1; }
            if (!G.clear_t) { G.clear_t = 1; G.exit_to = level_next(G.lvdir); G.exit_b = 0; }   /* as the level-data exits (game.c) */
        }
    }
}

/* ================================================================ 16. RM5 light stones, spike ceiling, crush boxes */
static void light_stones(player_t *p) {
    obj_t *b, *t, *d;
    EACH(OT_RM_LSBASE, b) {
        int k = b->par & 1, pressed = plate_on(3 + k);
        if (pressed && !(b->fl & F1)) {                                        /* 6756 / 6769: charging, drawn in front (layer 4) */
            b->fl |= F1; snd_play_orig(111, 220, 128);
            b->layer = 4; EACH(OT_RM_LSTOP, t) if ((t->par & 1) == k) t->layer = 4;
        }
        if (pressed && (b->fl & F1)) {                                          /* 6757 / 6770: A += 0.07, the blink frame per step */
            b->av[0] += 0.07; set_sk(b, k ? "lsbase1_c%d" : "lsbase0_c%d", (int)fmin(b->av[0], 5));
        }
        if (!pressed && (b->fl & F1)) {                                        /* 6758 / 6771: released: reset, behind again */
            b->av[0] = 0; b->fl &= ~F1; set_sk(b, "lsbase%d", k);
            b->layer = 2; EACH(OT_RM_LSTOP, t) if ((t->par & 1) == k) t->layer = 2;
        }
        if (b->av[0] >= 8) {                                                   /* 6759 / 6772: lit (~115 frames of pushing) */
            set_sk(b, "lsbase%d_lit", k); b->layer = 2;
            EACH(OT_RM_LSTOP, t) if ((t->par & 1) == k) { set_sk(t, "lstop%d_lit", k); t->layer = 2; }
            if (!(b->fl & F2)) { b->fl |= F2; snd_play_orig(40, 220, 128); }
        }
        if (b->fl & F2) { EACH(OT_RM_LSDOOR, d) if ((d->par & 1) == k) d->y = d->oy + 64; }   /* 6774 / 6775: its door lowered (open) */
    }
}
static obj_t *stone(int k) { obj_t *b; EACH(OT_RM_LSBASE, b) if ((b->par & 1) == k) return b; return NULL; }
static void spike_ceiling(player_t *p) {
    const rmgim_t *R = level_cur()->rmg;
    int basement = R && R->basement_y && MY(p) > R->basement_y;               /* group 0x2250d (6750) */
    obj_t *o, *b0 = stone(0), *k;
    EACH(OT_RM_SCEIL, o) {
        int *A = &o->st;
        if (basement) {
            o->x = (float)((int)G.camx - (int)G.camx % 32);                    /* 6752: follows the view, 32-px steps */
            if (*A == 0) { o->y += 1; shake(2); }                              /* 6753: slides down */
            if (*A == 0 && o->y > 2751) {                                      /* 6755: arrived below the stones */
                *A = 1; shake(4); o->av[2] = o->y; snd_play_orig(51, 220, 128);
                if (b0) b0->fl &= ~F2;                                         /* (RE: the base's lit flag is cleared here) */
            }
        }
        if (*A == 1 && b0 && (b0->fl & F2)) *A = 2;                            /* 6761 */
        if (*A == 2) { o->av[1] += 0.1; set_sk(o, "sceil%d", (int)fmin(o->av[1], 4)); }   /* 6762 */
        if (*A == 2 && o->av[1] >= 5) { *A = 3; o->av[3] = 0.25; }             /* 6763 */
        if (*A == 3) {                                                         /* 6764: rises, accelerating to 1 px/frame */
            shake(2); o->y = (float)(int)o->av[2]; o->av[3] = fmin(o->av[3] + 0.004, 1); o->av[2] -= o->av[3];
            if (o->y < 1873) { *A = 4; shake(4); snd_play_orig(51, 220, 128); }   /* 6768 (end condition U7: y < 1873 read from the code) */
        }
        if (dead(p) || *A < 3) continue;
        int on = sens_hit(SENS_BOTTOMBIG, o, 0);
        if (*A == 3 && on && mv_z(SENSOR_TOPBIG)) { kill_player(p); continue; }   /* 6766: pinned */
        if (on && (p->ground || p->vy >= 0)) r_hurt(p, (int)(fabs(0.5 * (p->ground ? 0 : p->vy)) + 10));   /* 6765 */
        if (*A == 3) { EACH(OT_RM_CRUSHBOX, k) if (fabsf(k->x - (float)p->x) < 128 && disc_hit(p, k)) { kill_player(p); break; } }   /* 6767 */
    }
}

/* ================================================================ 16. chair (6585) and the M39 group (3068-3072) */
static void chairs(player_t *p) {
    obj_t *o;
    uint32_t held = p->held, down_edge = (held & BTN_DOWN) && !(S.held0 & BTN_DOWN);
    S.held0 = held;
    if (p->state != ST_NORMAL || !p->ground || Mst(p) >= 4 || dead(p)) return;
    int idle = fabs(p->gsp) < 0.01 && !(held & (BTN_LEFT | BTN_RIGHT | BTN_JUMP));   /* OTHERS.Y = 0 */
    EACH(OT_RM_CHAIR, o) if ((idle || down_edge) && disc_hit(p, o)) { p->state = ST_SIT; p->state_t = 0; S.chair = (int)(o - G.o); break; }
}
/* player.c hook: M90 / M39 replace the movement block (1 = handled; 0 = back to M0 this frame, the normal update runs) */
int relic_gimb_player(player_t *p, const input_t *in) {
    p->held = in->held;
    if (p->state == ST_CART) {
        obj_t *c = slot(S.cart);
        if (!c || c->type != OT_RM_CART) { p->state = ST_NORMAL; return 0; }
        if (in->pressed & BTN_JUMP) {                                          /* 6136: the normal jump, M = 1; the cart's flag 2 off (6137) */
            p->state = ST_NORMAL; p->ground = 1; p->N = p->O = 0; p->rot = 0; p->cY = 0; c->fl &= ~F2;
            return 0;
        }
        p->cY = 51;                                                            /* 6135: OTHERS.Y 51, MILLA anim 41 direction 7, speed min(|15 B|, 50) */
        player_pose(p, 41, 7, (int)fmin(fabs(15 * c->av[1]), 50));
        player_sync_sensors(p);
        return 1;
    }
    obj_t *ch = slot(S.chair);
    if (!ch || ch->type != OT_RM_CHAIR || (in->held & (BTN_LEFT | BTN_RIGHT)) || (in->pressed & BTN_JUMP) || !disc_hit(p, ch)) {   /* 3072: back to M0 */
        p->state = ST_NORMAL; p->cY = 0; return 0;
    }
    p->ground = 1; p->gsp = p->vx = p->vy = 0;
    if (p->x < ch->x) { p->x = fmin(p->x + 2, ch->x); p->face = 1; player_pose(p, 1, 0, -1); }        /* 3069: walk to the chair */
    else if (p->x > ch->x) { p->x = fmax(p->x - 2, ch->x); p->face = -1; player_pose(p, 1, 16, -1); }   /* 3070 */
    else { p->face = 1; p->cY = 50; player_pose(p, 10, 0, -1); }               /* 3071: sit (OTHERS.Y 50 -> skin 40; pose INFERRED: the crouch) */
    player_sync_sensors(p);
    return 1;
}

/* ================================================================ 15. RM4 loop-layer pieces (6415-6418, as 3025 / 3026) */
static void loop_layers(const player_t *p) {
    obj_t *o;
    EACH(OT_RM_LOOP0, o) { if (p->layer == 0) o->fl &= ~1; else o->fl |= 1; }
    EACH(OT_RM_LOOP1, o) { if (p->layer == 1) o->fl &= ~1; else o->fl |= 1; }
}

/* ================================================================ per frame (spec 0 order: path, carry, hang, the act's gimmicks, late pillars) */
void relic_gimb_frame(player_t *p) {
    if (!objects_have_from(RT0)) return;
    loop_layers(p);
    path_platforms(p);                       /* 3283-3289, 4061, 6406-6414, 6448 */
    hang_switches(p);                        /* 4065, 5866-5868, 6081 */
    pickaxes(p);                             /* 6102-6119 */
    cups(p);                                 /* 6120-6122 */
    swings(p);                               /* 6123-6130, 6065 */
    plates(p);                               /* 5900, 6142-6152, 6432, 6580-6583 */
    cart_frame(p);                           /* 6131-6166 */
    elevators(p);                            /* 6167-6172 */
    elev_follow();
    drills(p);                               /* 6446, 6387-6391 */
    gateway(p);                              /* 6432-6434, 6466-6468 */
    light_stones(p);                         /* 6756-6759, 6769-6775 */
    spike_ceiling(p);                        /* 6751-6768 */
    chairs(p);                               /* 6585 */
    pillars(p);                              /* 5998, 6353-6363, 6007 (late) */
}
