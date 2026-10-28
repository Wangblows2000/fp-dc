/* Battle Glacier gimmicks (session 24): types 2400-2479 (bg.h OT_BGG_*). Spec: build/re/spec_bg_gimmicks.md.
 * Labels: RE (decompiled events / constructors of frames 59-64 = handle_frame_60..65 and on_frame_6x_start_events,
 * build/re/Frames__event_func_*.a.c, create_*_int__int.a.c; qualifiers from on_frame_60..65_init), RV (original traces,
 * build/orig_traces/bg_gim), INFERRED (marked).
 * Families and their events (frames: 1-6 = bg1-bg6):
 *   tele-block masks + blocks (BG1-BG3): 14124-14141, 14175, mask angles 14125-14134 (BG1) / 14192-14199 (BG2);
 *   spiral slides (BG1-BG3): 14142-14147 (+ the SlideCheck counter, BG1's inline event); snake drills (BG1-BG2): 14148-14160, 14176;
 *   snow spires (BG1-BG2, BG3's template): 14118-14123, 14174, 14265-14270; solar panels: 14118 / 14265 / 14365 / 14632 (frame by g3);
 *   glacier platforms: the objects.c path registry (ctor F 1 G 10 / F 2 G 80, q12 q13); BG bars (BG2): 14180-14186, 14245;
 *   crank ferris (BG4-BG5): 14362-14368, 14604, visuals 14365 / 14632; BG platforms 1 (BG4-BG6): static Q8 Q9, frame by g3;
 *   antigrav spawners (BG4-BG5, FD4): 14369-14378; magnet launchers (BG4-BG5): 14379-14394 / 14633-14646 + on_frame_63/64 start;
 *   spin mines (BG4-BG5, FD2): 14395 / 14396, 14606; switch blocks + tele switches + crystals (BG5): 14647-14654;
 *   transporters (BG5): Q7 pads only (FD's transporter travel 15188-15201 is FD's); sirens (BG5-BG6, FD): 14656 / 14912.
 * Reused systems: the objects.c path-platform registry (glacier platforms; the crank ferris with objects_plat_own_steps), the
 * Q7 / Q8 / Q9 / Q23 groups (objdefs.c flags; flag 0 = ignored by the sensors), obj_hang_try + relic_hang_hold (the BG bars:
 * moving handles), combat.c's Q20 rules (spin mines: armoured, contact B), the shared EnemyRock (OT_ROCK_AT, aqua_boss.c
 * rocks_frame: the snake drills' debris), player.c's ST_RIDE hook (M 95 magnet, M 96 spiral slide).
 * Passes: bg_gim_pre (nothing yet), bg_gim_obj (the BG bar grab), bg_gim_frame (after the player block, in event order),
 * bg_gim_player (ST_RIDE). Object fields: par = family member (importer BGG_SUB), layer = instance layer; av[] = class
 * alterables A..H as named per family. */
#include "bg.h"
#include <math.h>
#include <string.h>

#define RAD 57.29577951308232
#define RT0 OT_BGG_TBMASK
#define NRT (OT_BGG_END - RT0 + 1)
#define LMAX 256                                      /* BG5: 220 switch blocks */
#define F0 0x1                                     /* the object's flag 0 (sensors ignore it) */
#define F1 0x2                                     /* flag 1 */
#define F2 0x4                                     /* flag 2 */
#define F5 0x20                                    /* flag 5: armoured (combat.c F_ARMOURED) */
static struct {
    short lst[NRT][LMAX]; int nls[NRT];
    int on;
    int ride, ride_obj;                            /* 95 magnet, 96 spiral slide (MV.M); the start / magnet slot */
    int slide_k;                                   /* SlideCheck K: frames in M 96 without an active start (BG1 inline / 14145-14147) */
    uint32_t held, pressed;                        /* ST_RIDE: this frame's input */
    int color, color_set;                          /* CurrentColor_3359 K (14650), the block flags applied for it (14651 / 14652) */
    int light_below;                               /* the magnet light pulls while its A < this (BG4 1, BG5 70) */
    int mag_snd;                                   /* sound 165's loop channel (14386 / 14387) or -1 */
    int snd_until[400];                            /* "sample not playing": frame its last play ends (INFERRED from the sample length) */
} S;
#define LST S.lst
#define NLS S.nls
#define EACH(T, OB) for (int _k = 0; _k < NLS[(T) - RT0]; _k++) if (((OB) = &G.o[LST[(T) - RT0][_k]])->alive && (OB)->type == (T))
#define N(T) NLS[(T) - RT0]

static double MY(const player_t *p) { return p->y - ORIG_REF_DY; }
static int PXI(const player_t *p) { return (int)floor(p->x); }    /* PLAYER (Player_326) sits at (int X, int Y) */
static int PYI(const player_t *p) { return (int)floor(MY(p)); }
static double c_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / RAD); }                   /* Chowdren sin: 0 at exactly 180 */
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / RAD); }    /* cos: 0 at exactly 90 / 270 */
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
static int hurt(const player_t *p) { return p->state == ST_HURT || dead(p); }                       /* M 8 / 9 / 10 */
static void snd(int id) { snd_play_orig(id, 220, 128); }
static void snd_once(int id, int frames) {                                       /* "if sample id is not playing: play it" */
    if (id < 0 || id >= 400 || G.frame < S.snd_until[id]) return;
    S.snd_until[id] = G.frame + frames; snd(id);
}
static void shake(float z) { if (G.cam.Z < z) G.cam.Z = z; }                     /* CAM.Z = max(CAM.Z, z) */
static int g3(void) { return G.frame; }                                         /* global value 3 (+1 a frame; INFERRED phase: the stage frame) */
static float trunc_i(double v) { return (float)(int)v; }                        /* FrameObject::set_x / set_y take an int */
static void set_s(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void set_frame(obj_t *o, int f) {                                        /* Active::force_frame (the frame held: speed-0 rows) */
    if (!o->an.b) return;
    int n = o->an.b->anims[o->an.anim].count;
    o->an.frame = n > 0 ? ((f % n) + n) % n : 0; o->an.acc = 0;
}
static void lst_add(obj_t *q) {
    int t = q->type - RT0, i = (int)(q - G.o);
    if (t < 0 || t >= NRT) return;
    for (int k = 0; k < NLS[t]; k++) if (LST[t][k] == i) return;
    if (NLS[t] < LMAX) { LST[t][NLS[t]++] = (short)i; return; }
    for (int k = 0; k < LMAX; k++) { const obj_t *o = &G.o[LST[t][k]]; if (!o->alive || o->type != q->type) { LST[t][k] = (short)i; return; } }
}
static obj_t *spawn(int type, const char *anim, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame; o->par = -1;
    if (anim) { set_s(o, anim); if (o->an.b) anim_restart(&o->an); }
    lst_add(o);
    return o;
}
static void box_of(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {   /* the current frame's image box */
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - (o->flip ? f->w - 1 - f->hx : f->hx); *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int rect_disc(float x0, float y0, float x1, float y1, const player_t *p) {   /* a box (box collision) vs PLAYER's 27-px disc */
    double X = PXI(p), Y = PYI(p), cx = fmax(x0, fmin(X, x1 - 1)), cy = fmax(y0, fmin(Y, y1 - 1));
    return (cx - X) * (cx - X) + (cy - Y) * (cy - Y) <= 13.5 * 13.5;
}
static int box_disc(const obj_t *o, const player_t *p) { float x0, y0, x1, y1; if (!o->an.b) return 0; box_of(o, &x0, &y0, &x1, &y1); return rect_disc(x0, y0, x1, y1, p); }
static int sens_hit(int s, const obj_t *o) {                                    /* a placed player sensor overlapping the object */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int boxes_overlap(const obj_t *a, float bx0, float by0, float bx1, float by1) {
    float x0, y0, x1, y1; box_of(a, &x0, &y0, &x1, &y1);
    return x0 < bx1 && bx0 < x1 && y0 < by1 && by0 < y1;
}
/* Chowdren's index pairing ("the k-th of A acts on the matching instance of B"): A's i-th instance from the end takes B's
 * (i mod nB)-th from the end; equal counts pair forward index with forward index. Stored as the partner's slot in o->tm. */
static void link_pairs(int ta, int tb) {
    int na = N(ta), nb = N(tb);
    for (int a = 0; a < na; a++) {
        obj_t *o = &G.o[LST[ta - RT0][a]];
        int i = na - 1 - a;
        o->tm = nb ? LST[tb - RT0][nb - 1 - (i % nb)] : -1;
    }
}
static void pair_members(int ta, int tb, int nm) {                              /* link_pairs per member class (par) of ta */
    int nb = N(tb);
    for (int m = 0; m < nm; m++) {
        int na = 0;
        for (int a = 0; a < N(ta); a++) if ((G.o[LST[ta - RT0][a]].par & (nm - 1)) == m) na++;
        for (int a = 0, j = 0; a < N(ta); a++) {
            obj_t *o = &G.o[LST[ta - RT0][a]];
            if ((o->par & (nm - 1)) != m) continue;
            int i = na - 1 - j++;
            o->tm = nb ? LST[tb - RT0][nb - 1 - (i % nb)] : -1;
        }
    }
}
static obj_t *partner(const obj_t *o, int type) {
    if (o->tm < 0 || o->tm >= G.nobjs) return NULL;
    obj_t *q = &G.o[o->tm];
    return q->alive && q->type == type ? q : NULL;
}
static void leave_ground(player_t *p) { if (p->ground) { p->vx = p->gsp; p->ground = 0; } }   /* MV.L = 0 (B kept as the air speed) */

/* ================================================================ ST_RIDE (M 95 magnet, M 96 spiral slide)
 * The skins show OTHERS.Y 8 (skin animation 7, the ball: 'charge'; Milla's MILLA_IDX[8] = 7) at a forced speed. */
static void ride_anim(player_t *p, int speed) {
    if (chr_is_milla(G.character)) player_pose(p, 7, p->face > 0 ? 0 : 16, speed);
    else player_ride_anim(p, "charge", speed);
}
static void ride_start(player_t *p, int m, int obj) {
    S.ride = m; S.ride_obj = obj; S.slide_k = 0;
    p->state = ST_RIDE; p->state_t = 0; p->springing = 0; p->jumping = 0; p->m1 = 0;
}
static void ride_end(player_t *p) {
    S.ride = 0; S.ride_obj = -1;
    if (p->state == ST_RIDE) { p->state = ST_NORMAL; p->state_t = 0; }
}
int bg_gim_player(player_t *p, const input_t *in) {
    if (!S.ride || p->state != ST_RIDE) return 0;
    if (p->hurt_pending) { ride_end(p); return 0; }                              /* 2414 still runs: the hit ends the ride (INFERRED) */
    S.held = in->held; S.pressed = in->pressed;
    const obj_t *o = S.ride_obj >= 0 ? &G.o[S.ride_obj] : NULL;
    ride_anim(p, S.ride == 96 && o ? (int)(5 * o->av[3]) : 50);                 /* 14143: speed 5 D; 14388: 50 */
    return 1;
}
int bg_gim_ride_m(void) { return S.ride ? S.ride : -1; }

/* ================================================================ 1. tele-block masks + blocks (BG1-BG3; RE 14124-14141, 14175)
 * mask (Q7, invisible; its collision is the 64x64 square of the block's image, rotated by A): A angle, B player near, C distance
 * (ctor 320), D spin angle, E index (instance order reversed: 0 = the last instance; 14175), flag 1 = the arrival latch.
 * start 14175: E = k, D = (110 E) mod 360, one ObjectTeleBlock per mask at its position (layer: mask layer - 1, 14124).
 * every frame: B = 0 (BG1 inline / 14135); 14136: mask within X - 150 < x < X + 150, Y - 120 < y < Y + 150 -> B = 1;
 * 14137: B 1, C > 0: C -= 20, D = (D + 5) mod 360; 14138: B 0, C < 320: D = (D + 5) mod 360, flag 1 off, C += 20;
 * 14139: C 0, flag 1 off: flag 1, four white bursts at (x +- 24, y +- 24), shake 3, sound 77; 14140: block k at
 * (mask.x + cos(D) C, mask.y + sin(D) C), angle 0.025 C D + A, block A = mask C, invisible, frame (g3 / 4) mod 12;
 * 14141: block A < 320 -> visible. The masks' angles: one-shot events on E (BG1 14125-14134, BG2 14192-14199). */
static const signed char TB_ANG1[38] = { 0, -15, -30, -45, -45, -45, -45, -30, -15, [30] = 15, 30, 45, 45, 45, 45, 30, 15 };
static const signed char TB_ANG2[83] = { 45, 45, 45, [75] = 15, 30, 45, 45, 45, 45, 30, 15 };
static void tb_spawn(void) {
    obj_t *m; int n = N(OT_BGG_TBMASK), i = 0, act = bg_flow_act();
    EACH(OT_BGG_TBMASK, m) {
        int E = n - 1 - i++;
        m->av[4] = E; m->av[3] = fmod(110. * E, 360.); m->av[2] = 320; m->av[1] = 0;
        m->av[0] = act == 1 && E < 38 ? TB_ANG1[E] : act == 2 && E < 83 ? TB_ANG2[E] : 0;
        set_s(m, "bgg_tblock"); set_frame(m, 0); m->ang = (float)m->av[0]; m->hidden = 1;
    }
    for (int k = 0; k < n; k++) {
        m = &G.o[LST[OT_BGG_TBMASK - RT0][k]];
        obj_t *b = spawn(OT_BGG_TBLOCK, "bgg_tblock", m->x, m->y, m->layer > 2 ? m->layer - 1 : m->layer);
        if (b) { b->par = k; b->hidden = 1; b->av[0] = 320; }
    }
    link_pairs(OT_BGG_TBLOCK, OT_BGG_TBMASK);
}
static void teleblocks(player_t *p) {
    obj_t *m, *b; int arrive = 0;
    double X = p->x, Y = MY(p);
    EACH(OT_BGG_TBMASK, m) {
        double *A = &m->av[0], *B = &m->av[1], *C = &m->av[2], *D = &m->av[3];
        *B = 0;                                                                   /* inline / 14135 */
        if (X - 150 < m->x && m->x < X + 150 && Y - 120 < m->y && m->y < Y + 150) *B = 1;   /* 14136 */
        if (*B == 1 && *C > 0) { *C -= 20; *D = fmod(*D + 5, 360.); }             /* 14137 */
        else if (*B == 0 && *C < 320) { *D = fmod(*D + 5, 360.); m->fl &= ~F1; *C += 20; }   /* 14138 */
        if (*C == 0 && !(m->fl & F1)) {                                          /* 14139 */
            m->fl |= F1; arrive = 1;
            obj_effect(m->x - 24, m->y - 24, "wburst"); obj_effect(m->x + 24, m->y - 24, "wburst");
            obj_effect(m->x + 24, m->y + 24, "wburst"); obj_effect(m->x - 24, m->y + 24, "wburst");
        }
        m->ang = (float)*A; set_frame(m, 0);                                     /* 14140: set_angle(A) */
        (void)A;
    }
    if (arrive) { shake(3); snd(77); }
    EACH(OT_BGG_TBLOCK, b) {                                                    /* 14140 / 14141 */
        if (!(m = partner(b, OT_BGG_TBMASK))) { b->alive = 0; continue; }
        double C = m->av[2], D = m->av[3];
        b->ang = (float)fmod(0.0025 * C * 10 * D + m->av[0], 360.);
        b->x = trunc_i(c_cos(D) * C + floorf(m->x)); b->y = trunc_i(c_sin(D) * C + floorf(m->y));
        b->av[0] = C; b->hidden = !(C < 320);
        set_frame(b, (g3() / 4) % 12);
    }
}

/* ================================================================ 2. spiral slides (BG1-BG3; RE 14142-14147, BG1 inline)
 * start (invisible 32x32, hot spot 16, 16): A active, B progress, D speed. 14142: PLAYER on a start not above her (start.y <= Y),
 * start A 0, M not 8 / 9, G >= 0 and B >= -3: A = 1, M = 96, sound 7, D = max(0.7 G, 0.7 B), start B = 0.
 * 14143 (M 96): X = start.x + 24 sin(2.5 B), Y = start.y + B, B += D, MV.B = 1.2 D, OTHERS.Y 8 at speed 5 D, D = min(D + 0.1, 10),
 * MV.S = 0. 14144 (M 96): PLAYER on an end not above her: every start A = 0, MV.S = 1, M = 0, Y = end.y.
 * SlideCheck (BG1 inline, 14145-14147): K counts the M 96 frames, reset while a start is active; K > 3 -> M 0, S = 1 (the release
 * when the start is lost). The start / end masks are approximated by the 27-px disc on a 32-px disc (INFERRED; images 3202 / 4666
 * are 840-px discs). */
static int on_marker(const obj_t *o, const player_t *p) {
    double dx = PXI(p) - floorf(o->x), dy = PYI(p) - floorf(o->y);
    return dx * dx + dy * dy <= (13.5 + 16) * (13.5 + 16);
}
static void slides(player_t *p) {
    obj_t *o;
    double B = p->ground ? p->gsp : p->vx, Gv = p->ground ? 0 : p->vy;
    if (S.ride != 96 && !hurt(p) && p->state != ST_RIDE && Gv >= 0 && B >= -3) {   /* 14142 */
        EACH(OT_BGG_SLIDE, o) {
            if (o->par != 0 || o->av[0] != 0 || !on_marker(o, p) || floorf(o->y) > PYI(p)) continue;
            o->av[0] = 1; o->av[1] = 0; o->av[3] = fmax(0.7 * Gv, 0.7 * B);
            snd(7); ride_start(p, 96, (int)(o - G.o)); p->ground = 0;
            break;
        }
    }
    if (S.ride != 96) return;
    if (p->state != ST_RIDE) {                                                  /* hurt / killed during the ride */
        EACH(OT_BGG_SLIDE, o) if (o->par == 0) o->av[0] = 0;
        S.ride = 0; S.ride_obj = -1; return;
    }
    o = S.ride_obj >= 0 ? &G.o[S.ride_obj] : NULL;
    if (o && o->alive && o->av[0] == 1) {                                       /* 14143 */
        S.slide_k = 0;
        p->x = floorf(o->x) + 24 * c_sin(2.5 * o->av[1]);
        p->y = floorf(o->y) + o->av[1] + ORIG_REF_DY;
        o->av[1] += o->av[3];
        p->gsp = p->vx = 1.2 * o->av[3];
        if (p->vy > PHYS_MAXFALL) p->vy = PHYS_MAXFALL;                          /* RV sp1: G keeps gaining gravity in M 96 (0.25 -> 13.25), */
        p->vy += PHYS_GRAV;                                                      /* so she drops onto the floor right after the end */
        p->cY = 8;
        o->av[3] = fmin(o->av[3] + 0.1, 10);
        player_sync_sensors(p);
    } else if (++S.slide_k > 3) { ride_end(p); return; }                         /* SlideCheck 14145-14147 */
    EACH(OT_BGG_SLIDE, o) {                                                     /* 14144 */
        if (o->par != 1 || !on_marker(o, p) || floorf(o->y) > PYI(p)) continue;
        obj_t *s; EACH(OT_BGG_SLIDE, s) if (s->par == 0) s->av[0] = 0;
        p->y = floorf(o->y) + ORIG_REF_DY; ride_end(p); p->ground = 0; p->vy = Gv > 0 ? Gv : 0;
        player_sync_sensors(p);
        break;
    }
}

/* ================================================================ 3. snake drills (BG1-BG2; RE 14148-14160, 14176)
 * Eight segments per placement (one spot). start 14176: G = (instance index) mod 8 (7 = the head, direction 1: 14149), F = 540 - 20 G,
 * D, E = the spot, H = (reversed index) / 8 (14150's phase swap reads H mod 2 == 1 on the first H > 0 instance: 0.125, never true:
 * no swap, RE; RV sn1: H 5 on the 8th instance, untouched). Every frame: 14148 x = D + 128 sin F, y = E - 128 cos F (F before this
 * frame's step; RV sn1 exact); flag 1 off: F -= 3 while F >= 280, else -1,
 * angle 180 - F (14151 / 14152); F < 180 -> flag 1 on, flag 2 off (14155); flag 1 on: F += 3 while F <= 440, else +1, angle -F
 * (14156 / 14157); F > 540 -> flags 1, 2 off (14160). The head in view (grown 150) on the way down (flag 1 off, F < 280; flag 1 on,
 * F > 440): sound 149 once (flag 2; 14153 / 14158) and on even g3 frames an EnemyRock_1194 at (D -+ 128 - 32 + rand 65, E):
 * scale 1 - 0.05 rand 10, angle rand 360, speed 50 - 10 + rand 20, direction 5..10 (14154 / 14159). No qualifier: the segments
 * do not touch the player (RE: no collision event); the rocks do (rocks_frame). */
static void snake_rock(obj_t *h, int side) {
    double sc = 1 - cross_rand(10) * 0.05, ang = cross_rand(360), sp = 50 - 10 + cross_rand(20);
    int x = (int)(h->av[3] + side * 128 - 32 + cross_rand(65));
    static const int DIRS[6] = { 5, 6, 7, 8, 9, 10 };
    int d = DIRS[cross_rand(6)];
    obj_t *r = obj_alloc();
    if (!r) return;
    memset(r, 0, sizeof *r);
    r->type = OT_ROCK_AT; r->alive = 1; r->hp = 1; r->x = r->ox = (float)x; r->y = r->oy = (float)(int)h->av[4]; r->layer = 2; r->born = G.frame;
    { int a = ens_anim("rock"); if (a >= 0) anim_set(&r->an, &G.ens, a); }
    r->scale = (float)sc; r->ang = (float)ang;
    double a = d * 11.25 / RAD; r->vx = (float)(sp * cos(a)); r->vy = (float)(-sp * sin(a)); r->fx = r->fy = 0; r->tm = 0;
}
static void snakes(void) {
    obj_t *o; int snd149 = 0, even = (g3() & 1) == 0;
    EACH(OT_BGG_SNAKE, o) {
        double *D = &o->av[3], *E = &o->av[4], *F = &o->av[5], *Gs = &o->av[6];
        o->x = trunc_i(*D + 128 * c_sin(*F)); o->y = trunc_i(*E - 128 * c_cos(*F));   /* 14148 (RV sn1: sin for x, cos for y) */
        if (*Gs == 7) set_s(o, "bgg_snakeh");                                    /* 14149 */
        int head = *Gs == 7 && obj_near_view(o, 150);
        if (!(o->fl & F1)) { *F -= *F >= 280 ? 3 : 1; o->ang = (float)(180 - *F); }   /* 14151 / 14152 */
        if (head && !(o->fl & (F1 | F2)) && *F < 280) { o->fl |= F2; snd149 = 1; }   /* 14153 */
        if (head && even && !(o->fl & F1) && *F < 280) snake_rock(o, -1);        /* 14154 */
        if (!(o->fl & F1) && *F < 180) { o->fl = (o->fl & ~F2) | F1; }           /* 14155 */
        if (o->fl & F1) { *F += *F <= 440 ? 3 : 1; o->ang = (float)-*F; }        /* 14156 / 14157 */
        if (head && (o->fl & F1) && !(o->fl & F2) && *F > 440) { o->fl |= F2; snd149 = 1; }   /* 14158 */
        if (head && even && (o->fl & F1) && *F > 440) snake_rock(o, 1);          /* 14159 */
        if ((o->fl & F1) && *F > 540) o->fl &= ~(F1 | F2);                       /* 14160 */
    }
    if (snd149) snd(149);
}

/* ================================================================ 4. snow spires (BG1-BG2; RE 14118-14123, 14174, BG3 14265-14270)
 * mask (Q7, invisible; collision = the spire's image, INFERRED: the mask image 13546 is the spire 13593 + 495 px): A, B = the
 * start spot (14174), C 90 (ctor), D rise (0), F 200 (ctor). 14118: mask at (A + D cos C, B - D sin C), angle C - 90; 14119: the
 * player within 200 (X - 200 < A < X + 200, Y - 200 < B < Y + 200) -> flag 1; while flag 1 and D < F: shake 3 when in view grown 200
 * (14120), sound 110 when in view grown 300 and not playing (14121), D += 1.5 (14122). 14123: spire k at mask k, same angle. */
static void spires(player_t *p) {
    obj_t *o; double X = p->x, Y = MY(p);
    EACH(OT_BGG_SPMASK, o) {
        double A = o->av[0], B = o->av[1], C = o->av[2], *D = &o->av[3], F = o->av[5];
        o->x = trunc_i(A + *D * c_cos(C)); o->y = trunc_i(B - *D * c_sin(C)); o->ang = (float)(C - 90);   /* 14118 */
        if (!(o->fl & F1) && A < X + 200 && X - 200 < A && B < Y + 200 && Y - 200 < B) o->fl |= F1;      /* 14119 */
        if ((o->fl & F1) && *D < F) {
            if (obj_near_view(o, 200)) shake(3);                                  /* 14120 */
            if (obj_near_view(o, 300)) snd_once(110, 17);                         /* 14121 (sample 0.26 s) */
            *D += 1.5;                                                           /* 14122 */
        }
    }
    EACH(OT_BGG_SPIRE, o) {                                                     /* 14123 */
        obj_t *m = partner(o, OT_BGG_SPMASK);
        if (m) { o->x = m->x; o->y = m->y; o->ang = m->ang; }
    }
}

/* ================================================================ 5. BG bars (BG2; RE 14180-14186, 14245)
 * mask (Q23 hang handle, invisible, 50 x 11, hot spot 25, -4): A = start y, B offset, C speed (+-6: reversed index parity, 14245),
 * D pause, E 128 (ctor). 14180 / 14181: B = clamp(B + C, -E, E); 14182 / 14183: at the end D += 1; 14184: D > 40 -> D 0, C = -C;
 * 14185: mask y = A + B, bar k at mask k; 14186: hanging on a mask (M 41, its Q) -> MV.Y = mask.y. The grab is the shared rule
 * (obj_hang_try, 4064-4066), the carry relic_hang_hold (4062). */
static void bars(player_t *p) {
    obj_t *o;
    EACH(OT_BGG_BARMASK, o) {
        double *B = &o->av[1], *C = &o->av[2], *D = &o->av[3], E = o->av[4];
        if (*C < 0) { *B = fmax(*B + *C, -E); if (*B <= -E) *D += 1; }
        else if (*C > 0) { *B = fmin(*B + *C, E); if (*B >= E) *D += 1; }
        if (*D > 40) { *D = 0; *C = -*C; }
        o->y = trunc_i(o->av[0] + *B);
        if (p->state == ST_HANG && p->hang_obj == (int)(o - G.o)) {             /* 14186 (relic_hang_hold keeps the offset) */
            p->hang_y = o->y - 32; p->hang_py = o->y; p->y = o->y + ORIG_REF_DY;
        }
    }
    EACH(OT_BGG_BAR, o) { obj_t *m = partner(o, OT_BGG_BARMASK); if (m) { o->x = m->x; o->y = m->y; } }
}

/* ================================================================ 6. crank ferris (BG4-BG5; RE 14362-14368, 14604, 14365 / 14632)
 * platform (Q8 Q9 Q13, invisible 128 x 32 box): the q13 path rule (objects.c registry: X = I + 128 sin A, Y = J + 128 cos E, own
 * steps B / F in av[6] / av[7]); start 14604: A = E = (reversed index mod 4) 90. 14362: R = (R + 1) mod 50, B = F = 0; 14363: R 1 ->
 * Q 9; 14364: Q > 0 -> B = F = -5, Q -= 1 (a 45-degree ratchet every 50 frames). 14366-14368: flags 0 and 6 (0x41: ignored, no carry)
 * on every platform not below the player (plat.y <= Y + 11) and on those touching a collision stopper. Visuals (14365): car k on
 * platform k, frame (g3 / 5) mod 6; spokes (masks 2 / 3 / 4) at 1/4, 1/2, 3/4 of the way from the hub (I, J). */
static void ferris(player_t *p) {
    obj_t *o, *s;
    EACH(OT_BGG_FERRIS, o) {
        double *R = &o->av[0], *Q = &o->av[1];
        *R = fmod(*R + 1, 50.); o->av[6] = o->av[7] = 0;                          /* 14362 */
        if (*R == 1) *Q = 9;                                                     /* 14363 */
        if (*Q > 0) { o->av[6] = o->av[7] = -5; *Q -= 1; }                       /* 14364 */
        o->fl &= ~0x41;                                                          /* 14366 */
        if (floorf(o->y) <= PYI(p) + 11) o->fl |= 0x41;                          /* 14367 */
        else EACH(OT_BGG_FSTOP, s) {                                             /* 14368 */
            float x0, y0, x1, y1; box_of(s, &x0, &y0, &x1, &y1);
            if (boxes_overlap(o, x0, y0, x1, y1)) { o->fl |= 0x41; break; }
        }
    }
    EACH(OT_BGG_FERRISV, o) {                                                   /* 14365 / 14632 */
        int m = o->par & 3; obj_t *pl = partner(o, OT_BGG_FERRIS);
        if (!pl) continue;
        double x = floorf(pl->x), y = floorf(pl->y), I = pl->ox, J = pl->oy;
        if (m == 0) { o->x = pl->x; o->y = pl->y; set_frame(o, (g3() / 5) % 6); }
        else if (m == 1) { o->x = trunc_i((x + 3 * I) * 0.25); o->y = trunc_i((y + 3 * J) * 0.25); }
        else if (m == 2) { o->x = trunc_i((x + I) * 0.5); o->y = trunc_i((y + J) * 0.5); }
        else { o->x = trunc_i((3 * x + I) * 0.25); o->y = trunc_i((3 * y + J) * 0.25); }
    }
}

/* ================================================================ 7. antigrav spawners (BG4-BG5, FD4; RE 14369-14378)
 * spawner (box 64 x 16): flag 1 near, flag 2 the sound latch. 14369: flag 1 off, frame (g3 / 2) mod 2; 14370: |x - X| <= 95 and
 * Y < y <= Y + 239 (PLAYER's int position) -> flag 1. 14371: PLAYER on an antigrav -> G = min(G, -4), L = 0; 14373: there with
 * M 0 / 1 -> M 40 (the launched code); 14374: flag 1 without flag 2 -> flag 2, sound 344; 14375: flag 2 without flag 1 -> off;
 * 14376: every near spawner makes a GimmickAntigrav at its position; 14377: antigrav y -= 20, frame (g3 / 2) mod 8, A += 1; 14378:
 * destroyed at y < -32 or A > 16. (14372: MV.A 5 only: not a playable character here.) */
static void antigrav(player_t *p) {
    obj_t *o; int X = PXI(p), Y = PYI(p), snd344 = 0;
    EACH(OT_BGG_AGSPAWN, o) {
        o->fl &= ~F1; set_frame(o, (g3() / 2) & 1);                              /* 14369 */
        int x = (int)floorf(o->x), y = (int)floorf(o->y);
        if (!(x < X - 95 || X + 95 < x) && !(y <= Y || Y + 239 < y)) o->fl |= F1;   /* 14370 */
    }
    int on = 0;
    if (!dead(p)) EACH(OT_BGG_AG, o) if (box_disc(o, p)) { on = 1; break; }
    if (on) {                                                                   /* 14371 / 14373 */
        if (p->state == ST_NORMAL && !p->m1) p->springing = 1;                   /* M 0 / 1 -> 40 */
        leave_ground(p); p->vy = fmin(p->vy, -4);
    }
    EACH(OT_BGG_AGSPAWN, o) {
        if ((o->fl & F1) && !(o->fl & F2)) { o->fl |= F2; snd344 = 1; }          /* 14374 */
        if ((o->fl & F2) && !(o->fl & F1)) o->fl &= ~F2;                         /* 14375 */
    }
    if (snd344) snd(344);
    int n = N(OT_BGG_AGSPAWN);
    for (int k = 0; k < n; k++) {                                               /* 14376 */
        o = &G.o[LST[OT_BGG_AGSPAWN - RT0][k]];
        if (o->alive && o->type == OT_BGG_AGSPAWN && (o->fl & F1)) { obj_t *a = spawn(OT_BGG_AG, "bgg_ag", o->x, o->y, o->layer); if (a) a->av[0] = 0; }
    }
    EACH(OT_BGG_AG, o) {
        if (o->born == G.frame) continue;                                        /* created objects move from the next frame */
        o->y = trunc_i(floorf(o->y) - 20); set_frame(o, (g3() / 2) & 7); o->av[0] += 1;   /* 14377 */
        if (o->y < -32 || o->av[0] > 16) o->alive = 0;                           /* 14378 */
    }
}

/* ================================================================ 8. magnet launchers (BG4-BG5; RE 14379-14394, 14633-14646)
 * magnet (box 64 x 56, hot spot 32, 32; action point 44 below): A home x (start), B the player's offset, C held, D cooldown.
 * 14379: PLAYER in a light whose A (= its magnet's D, 14389) is below the act's limit (BG4 A == 0, BG5 A < 70) -> G = min(G, -4),
 * L = 0. 14380: M not 8 / 9 / 10 / 95, TopBig on a magnet with C 0 and Y > magnet.y: M 95, C 1, B = X - x, sound 7, move_front.
 * 14381 / 14382: B -> 0 by 2. 14384 / 14385: left / right held and the magnet off its Left / Right stopper: x -+ 6. 14386 / 14387:
 * sound 165 while left != right (port: a looping channel, INFERRED equivalent). 14388 (M 95): Y = the action point, X = x + B,
 * T = U = 0, G = 15, MV.B = 0, direction right - left (1 / 0 / 31), frame (g3 / 10) mod 2, OTHERS.Y 8 at speed 50, D = 90.
 * 14390: jump or attack pressed in M 95: M 1, T = U = 1, every magnet C 0, direction 0, sound 5, an EffectsSpringBeam at
 * (PLAYER.x + 32, PLAYER.y - 8) angle 270 scale 2, sound 165 stopped, S = 1 (she drops at G 15). 14391: M not 95 -> C 0.
 * 14392: C 0, D > 0 -> D -= 1; 14393 / 14394: C 0, D 0: back to A by 6. */
static void stop_box(const obj_t *s, float *x0, float *y0, float *x1, float *y1) {   /* 16 x 32 image, hot spot 32, 32 (13738 / 13676) */
    *x0 = floorf(s->x) - 32; *y0 = floorf(s->y) - 32; *x1 = *x0 + 16; *y1 = *y0 + 32;
}
static int mag_blocked(const obj_t *m, int side) {
    const obj_t *s;
    EACH(OT_BGG_MSTOP, s) { if (s->par != side) continue; float x0, y0, x1, y1; stop_box(s, &x0, &y0, &x1, &y1); if (boxes_overlap(m, x0, y0, x1, y1)) return 1; }
    return 0;
}
static void mag_dir(obj_t *m, int d) {                                          /* direction 0 / 1 / 31 (31: the mirror of 1) */
    if (d == 0) { set_s(m, "bgg_magnet"); m->flip = 0; }
    else { set_s(m, "bgg_magnet1"); m->flip = d < 0 ? 2 : 0; }
}
static void magnets(player_t *p) {
    obj_t *o, *l; int M95 = S.ride == 95 && p->state == ST_RIDE;
    EACH(OT_BGG_MLIGHT, l) { obj_t *m = partner(l, OT_BGG_MAGNET); if (m) { l->x = m->x; l->y = m->y; l->av[0] = m->av[3]; } }   /* 14389 (last frame's D) */
    if (!dead(p)) EACH(OT_BGG_MLIGHT, l) if (l->av[0] < S.light_below && box_disc(l, p)) { leave_ground(p); p->vy = fmin(p->vy, -4); break; }   /* 14379 */
    if (!M95 && !hurt(p) && p->state != ST_RIDE) {                              /* 14380 */
        EACH(OT_BGG_MAGNET, o) {
            if (o->av[2] != 0 || !(MY(p) > floorf(o->y)) || !sens_hit(SENSOR_TOPBIG, o)) continue;
            o->av[2] = 1; o->av[1] = p->x - floorf(o->x); snd(7);
            ride_start(p, 95, (int)(o - G.o)); M95 = 1; leave_ground(p);
            break;
        }
    }
    int lr = ((S.held & BTN_RIGHT) ? 1 : 0) - ((S.held & BTN_LEFT) ? 1 : 0);
    EACH(OT_BGG_MAGNET, o) {
        if (o->av[2] != 1) continue;
        if (o->av[1] < 0) o->av[1] = fmin(o->av[1] + 2, 0);                       /* 14381 */
        else if (o->av[1] > 0) o->av[1] = fmax(o->av[1] - 2, 0);                  /* 14382 */
        if (M95 && (S.held & BTN_LEFT) && !mag_blocked(o, 0)) o->x = floorf(o->x) - 6;    /* 14384 */
        if (M95 && (S.held & BTN_RIGHT) && !mag_blocked(o, 1)) o->x = floorf(o->x) + 6;   /* 14385 */
        if (M95 && lr != 0) { if (S.mag_snd == -1) { S.mag_snd = snd_loop_orig(165, 220, 0); if (S.mag_snd < 0) S.mag_snd = -2; } }   /* 14386 (-2: no channel) */
        else if (S.mag_snd != -1) { if (S.mag_snd >= 0) snd_loop_stop(S.mag_snd); S.mag_snd = -1; }   /* 14387 */
    }
    if (M95) {
        o = &G.o[S.ride_obj];
        if (p->state != ST_RIDE || !o->alive) { M95 = 0; S.ride = 0; S.ride_obj = -1; }
        else {                                                                  /* 14388 */
            p->y = floorf(o->y) + 44 + ORIG_REF_DY; p->x = floorf(o->x) + o->av[1];
            p->vy = 15; p->vx = p->gsp = 0; p->ground = 0; p->cY = 8;
            mag_dir(o, lr); if (lr == 0) set_frame(o, (g3() / 10) % 2);
            player_sync_sensors(p);
            o->av[3] = 90;
            if (S.pressed & (BTN_JUMP | BTN_ATTACK)) {                           /* 14390 */
                EACH(OT_BGG_MAGNET, o) { o->av[2] = 0; mag_dir(o, 0); set_frame(o, 0); }
                snd(5);
                obj_t *e = spawn(OT_EXPLODE, "bgg_sbeam", (float)(PXI(p) + 32), (float)(PYI(p) - 8), 3);
                if (e) { e->ang = 270; e->scale = 2; }
                if (S.mag_snd >= 0) snd_loop_stop(S.mag_snd);
                S.mag_snd = -1;
                ride_end(p); p->m1 = 1; p->vy = 15; p->ground = 0; p->vx = p->gsp = 0;
                M95 = 0;
            }
        }
    }
    if (!M95) EACH(OT_BGG_MAGNET, o) if (o->av[2] != 0 || o->dir) { o->av[2] = 0; mag_dir(o, 0); set_frame(o, 0); }   /* 14391 */
    EACH(OT_BGG_MAGNET, o) {
        if (o->av[2] != 0) continue;
        if (o->av[3] > 0) o->av[3] -= 1;                                        /* 14392 */
        else if (floorf(o->x) > o->av[0]) o->x = (float)fmax(o->av[0], floorf(o->x) - 6);   /* 14393 */
        else if (floorf(o->x) < o->av[0]) o->x = (float)fmin(o->av[0], floorf(o->x) + 6);   /* 14394 */
    }
}

/* ================================================================ 9. spin mines (BG4-BG5, FD2; RE 14395 / 14396, 14606)
 * Q20, flag 5 (armoured: attacks deflect), ctor A 30, F 64. start 14606: D = y, E = 30 (reversed index). 14395: y = D + F sin E,
 * E = (E + 2) mod 360, frame (g3 / 5) mod 8, contact damage B 5; 14396: the metal shield (OTHERS.B 3) -> B 0. */
static void mines(player_t *p) {
    obj_t *o;
    EACH(OT_BGG_SPINMINE, o) {
        o->y = trunc_i(o->av[3] + o->av[5] * c_sin(o->av[4]));
        o->av[4] = fmod(o->av[4] + 2, 360.);
        set_frame(o, (g3() / 5) % 8);
        o->bd = p->shield == 3 ? 0 : 5;
    }
}

/* ================================================================ 10. switch blocks, tele switches, crystals (BG5; RE 14647-14654)
 * CurrentColor K (0 / 1). 14647: blocks shown as direction (flag 0) (0: the solid 12-frame loop at (g3 / 3) mod 12, 1: the outline),
 * switches and crystals direction K, crystal k at (switch k + 20 cos A, - 20 sin A), switches flag 1 off; 14648: PLAYER within 31
 * of a switch (both axes) -> flag 1; 14649: flag 2 without flag 1 -> off; 14650: a new touch (flag 1, not 2): flag 2, K = (K + 1)
 * mod 2, sound 285, a white burst at the switch, every crystal B += 270; 14651 / 14652 (once per K): K 0 -> blocks 1 solid, blocks 2
 * flag 0 (ignored); K 1 -> the reverse; 14653 / 14654: crystals with B > 0: A = (A + 350) mod 360, B -= 10. Crystal 1 ctor A 180. */
static void color_apply(void) {
    obj_t *o;
    EACH(OT_BGG_SWBLOCK, o) { int solid = (o->par & 1) == S.color; o->fl = solid ? (o->fl & ~F0) : (o->fl | F0); }
    S.color_set = S.color;
}
static void switches(player_t *p) {
    obj_t *o; int touched = 0;
    if (S.color_set != S.color) color_apply();
    EACH(OT_BGG_SWBLOCK, o) {                                                   /* 14647 */
        int off = o->fl & F0;
        set_s(o, (o->par & 1) ? (off ? "bgg_swb2o" : "bgg_swb2") : (off ? "bgg_swb1o" : "bgg_swb1"));
        if (!off) set_frame(o, (g3() / 3) % 12);
    }
    EACH(OT_BGG_TSWITCH, o) {
        set_s(o, S.color ? "bgg_tsw1" : "bgg_tsw0"); o->fl &= ~F1;
        int dx = (int)floorf(o->x) - PXI(p), dy = (int)floorf(o->y) - PYI(p);
        if (!dead(p) && dx >= -31 && dx <= 31 && dy >= -31 && dy <= 31) o->fl |= F1;   /* 14648 */
        if (!(o->fl & F1) && (o->fl & F2)) o->fl &= ~F2;                         /* 14649 */
        if ((o->fl & F1) && !(o->fl & F2)) { o->fl |= F2; touched = 1; obj_effect(o->x, o->y, "wburst"); }   /* 14650 */
    }
    if (touched) {
        S.color = (S.color + 1) % 2; snd(285);
        EACH(OT_BGG_TSCRYSTAL, o) o->av[1] += 270;
        color_apply();                                                           /* 14651 / 14652 */
    }
    EACH(OT_BGG_TSCRYSTAL, o) {
        set_s(o, S.color ? "bgg_tsc1" : "bgg_tsc0");
        obj_t *s = partner(o, OT_BGG_TSWITCH);                                   /* RV sw5: placed (14647) before the turn */
        if (s) { o->x = trunc_i(floorf(s->x) + 20 * c_cos(o->av[0])); o->y = trunc_i(floorf(s->y) - 20 * c_sin(o->av[0])); }
        if (o->av[1] > 0) { o->av[0] = fmod(o->av[0] + 350, 360.); o->av[1] -= 10; }   /* 14653 / 14654 */
    }
}
int bg_gim_switch_color(void) { return S.color; }
void bg_gim_set_light_rule(int a_below) { S.light_below = a_below; }

/* ================================================================ spawn / frame / draw */
void bg_gim_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S); S.ride_obj = -1; S.mag_snd = -1; S.color_set = -1;
    S.light_below = bg_flow_act() == 5 ? 70 : 1;                                 /* 14633 (BG5) / 14379 (BG4) */
    /* objects.c path platforms (RE ctors): glacier platforms (q12 q13: the dip and one-way flags), the crank ferris (q13 only:
     * X = I + C sin A, Y = J + G cos E, C = G = 128, its own ratchet steps) */
    objects_plat_type(OT_BGG_GPLAT1, 1, 10);
    objects_plat_type(OT_BGG_GPLAT2, 2, 80);
    objects_plat_path(OT_BGG_FERRIS, 0, 128, 1, 0, 128, 0, 0); objects_plat_own_steps(OT_BGG_FERRIS);
    if (!objects_have_from(RT0)) return;
    int seen[NRT] = { 0 };
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < RT0 || o->type > OT_BGG_END) continue;
        int t = o->type - RT0, n = 0, a = 0;
        for (int j = 0; j < G.lvl.nobj; j++) if (G.lvl.objs[j].type == o->type) { if (n++ == seen[t]) { a = G.lvl.objs[j].anim; break; } }
        seen[t]++;
        o->par = a & 0xff; o->layer = (a >> 8) & 0x7f;
        o->an.b = NULL; o->fl = 0; o->st = 0; o->tm = -1; o->c = 0; o->dir = 0; o->ang = 0; memset(o->av, 0, sizeof o->av);
        o->hidden = (objdef(o->type)->flags & OD_HIDDEN) != 0;
        if (o->x < 0 || o->y < 0 || o->x > G.lvl.w || o->y > G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        lst_add(o); S.on = 1;
    }
    if (!S.on) return;
    obj_t *o; int n, k;
    tb_spawn();
    EACH(OT_BGG_SLIDE, o) o->an.b = NULL;
    n = N(OT_BGG_SNAKE); k = 0;
    EACH(OT_BGG_SNAKE, o) {                                                     /* 14176 */
        set_s(o, "bgg_snake"); o->av[3] = floorf(o->x); o->av[4] = floorf(o->y);
        o->av[6] = k % 8; o->av[7] = (n - 1 - k) * 0.125; o->av[5] = 540 - 20 * o->av[6]; k++;
    }
    k = 0;
    EACH(OT_BGG_SPMASK, o) {                                                    /* ctor C 90 F 200; 14174 */
        set_s(o, "bgg_spire"); o->av[0] = floorf(o->x); o->av[1] = floorf(o->y); o->av[2] = 90; o->av[5] = 200; o->av[4] = k++;
    }
    EACH(OT_BGG_SPIRE, o) set_s(o, "bgg_spire");
    EACH(OT_BGG_SOLAR, o) set_s(o, "bgg_solar");
    EACH(OT_BGG_GPLAT1, o) { set_s(o, "bgg_gplat"); o->vx = 0; o->vy = 0; o->fx = o->oy; o->ky = o->ox; o->fl = 0; }
    EACH(OT_BGG_GPLAT2, o) { set_s(o, "bgg_gplat"); o->vx = 0; o->vy = 0; o->fx = o->oy; o->ky = o->ox; o->fl = 0; }
    EACH(OT_BGG_BAR, o) set_s(o, "bgg_bar");
    n = N(OT_BGG_BARMASK); k = 0;
    EACH(OT_BGG_BARMASK, o) {                                                   /* ctor E 128; 14245: A = y, C = (reversed index mod 2) 12 - 6 */
        set_s(o, "bgg_barmask"); o->av[0] = floorf(o->y); o->av[4] = 128; o->av[2] = ((n - 1 - k) % 2) * 12 - 6; k++;
    }
    EACH(OT_BGG_BGPLAT, o) set_s(o, "bgg_bgplat");
    n = N(OT_BGG_FERRIS); k = 0;
    EACH(OT_BGG_FERRIS, o) {                                                    /* ctor C 128 D 1 G 128; 14604: A = E = (k mod 4) 90 */
        set_s(o, "bgg_ferris"); set_frame(o, 0);
        int r = (n - 1 - k++) % 4;
        o->kx = 90. * r; o->vx = (float)(90. * r); o->vy = 0; o->fx = (float)(o->oy + 128 * c_cos(o->vx)); o->ky = o->ox + 128 * c_sin(o->kx); o->fl = 0;
        o->x = (float)floor(o->ky); o->y = (float)floor(o->fx);
    }
    EACH(OT_BGG_FERRISV, o) set_s(o, (o->par & 3) ? "bgg_ferris2" : "bgg_ferris");
    EACH(OT_BGG_FSTOP, o) { set_s(o, "bgg_ferris"); set_frame(o, 0); }
    EACH(OT_BGG_AGSPAWN, o) set_s(o, "bgg_agspawn");
    EACH(OT_BGG_MAGNET, o) { set_s(o, "bgg_magnet"); o->av[0] = floorf(o->x); }   /* on_frame_63 / 64 start: A = x */
    EACH(OT_BGG_MSTOP, o) o->an.b = NULL;
    EACH(OT_BGG_MLIGHT, o) set_s(o, "bgg_mlight");
    n = N(OT_BGG_SPINMINE); k = 0;
    EACH(OT_BGG_SPINMINE, o) {                                                  /* ctor A 30 F 64; 14606: D = y, E = 30 k, flag 5 */
        set_s(o, "bgg_mine"); o->A = 30; o->hp = 30; o->fl |= F5; o->bd = 5;
        o->av[3] = floorf(o->y); o->av[4] = 30. * (n - 1 - k++); o->av[5] = 64;
    }
    EACH(OT_BGG_SWBLOCK, o) set_s(o, (o->par & 1) ? "bgg_swb2" : "bgg_swb1");
    S.color = 0; color_apply();                                                  /* 14651 at the first frame (K 0) */
    EACH(OT_BGG_TSWITCH, o) set_s(o, "bgg_tsw0");
    EACH(OT_BGG_TSCRYSTAL, o) { set_s(o, "bgg_tsc0"); o->av[0] = (o->par & 1) ? 0 : 180; }   /* crystal 1 ctor A 180 */
    EACH(OT_BGG_TRANSP, o) set_s(o, "bgg_transp");
    EACH(OT_BGG_SIREN, o) set_s(o, "bgg_siren");
    link_pairs(OT_BGG_SPIRE, OT_BGG_SPMASK);                                    /* 14123 */
    link_pairs(OT_BGG_BAR, OT_BGG_BARMASK);                                     /* 14185 */
    link_pairs(OT_BGG_MLIGHT, OT_BGG_MAGNET);                                   /* 14389 */
    pair_members(OT_BGG_FERRISV, OT_BGG_FERRIS, 4);                             /* 14365: each mask class with the platforms */
    pair_members(OT_BGG_TSCRYSTAL, OT_BGG_TSWITCH, 2);                          /* 14647: each crystal class with the switches */
}
void bg_gim_pre(player_t *p) { (void)p; }
void bg_gim_frame(player_t *p) {
    if (!S.on) return;
    obj_t *o;
    if (N(OT_BGG_SPMASK)) spires(p);                                             /* 14118-14123 */
    EACH(OT_BGG_SOLAR, o) set_frame(o, (g3() / 4) % 20);                         /* 14118 / 14265 / 14365 / 14632 */
    if (N(OT_BGG_TBMASK)) teleblocks(p);                                         /* 14124-14141 */
    if (N(OT_BGG_SLIDE) || S.ride == 96) slides(p);                              /* 14142-14147 */
    if (N(OT_BGG_SNAKE)) snakes();                                               /* 14148-14160 */
    if (N(OT_BGG_BARMASK)) bars(p);                                              /* 14180-14186 */
    if (N(OT_BGG_FERRIS)) ferris(p);                                             /* 14362-14368, 14365 */
    EACH(OT_BGG_BGPLAT, o) set_frame(o, (g3() / 5) % 8);                         /* 14365 / 14632 / 14912 */
    if (N(OT_BGG_AGSPAWN) || N(OT_BGG_AG)) antigrav(p);                          /* 14369-14378 */
    if (N(OT_BGG_MAGNET)) magnets(p);                                            /* 14379-14394 */
    if (N(OT_BGG_SPINMINE)) mines(p);                                            /* 14395 / 14396 */
    if (N(OT_BGG_SWBLOCK) || N(OT_BGG_TSWITCH)) switches(p);                     /* 14647-14654 */
    EACH(OT_BGG_SIREN, o) set_frame(o, (g3() / 4) % 4);                         /* 14656 / 14912 */
}
void bg_gim_draw(int pass) { (void)pass; }
int bg_gim_obj(obj_t *o, player_t *p) {
    if (o->type == OT_BGG_BARMASK) {                                            /* 4064-4066: the shared grab (Q23) */
        if (!dead(p) && !(p->state == ST_HANG && p->hang_obj == (int)(o - G.o))) {
            int was = p->state == ST_HANG;
            obj_hang_try(o, p);
            if (!was && p->state == ST_HANG && p->hang_obj == (int)(o - G.o)) { p->hang_y = o->y - 32; p->y = o->y + ORIG_REF_DY; }   /* 14186 */
        }
    }
    return 1;                                                                    /* (the families run in bg_gim_frame) */
}
int bg_gim_q20(int type) { return type == OT_BGG_SPINMINE; }
