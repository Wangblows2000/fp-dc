/* Level gimmicks (RE: build/re/spec_traversal.md, spec_cards_results.md, spec_dv3_gimmicks.md).
 * Spikes (all directions), red springs, loop collision layers (MV.P), the half corkscrew ride (M45),
 * geyser plates, the collectible cards (ObjectCodec_430). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../fp.h"

static double MVY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / 57.29577951308232); }
static int box_sensor(int s, float x0, float y0, float x1, float y1) {   /* a sensor mask overlapping a box */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) {
        int y = oy + j;
        if (b[j] < a[j] || y < y0 || y >= y1) continue;
        if (ox + b[j] >= x0 && ox + a[j] < x1) return 1;
    }
    return 0;
}
static void obox(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o->an.b) { *x0 = o->x; *y0 = o->y; *x1 = o->x + 32; *y1 = o->y + 32; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
static int sensor_on(const obj_t *o, int s) { float a, b, c, d; obox(o, &a, &b, &c, &d); return box_sensor(s, a, b, c, d); }
/* Player_326: 27-px disc at (int X, int Y) against a box */
static int disc_on_box(const player_t *p, float x0, float y0, float x1, float y1) {
    int px = (int)p->x, py = (int)MVY(p);
    float cx = fmaxf(x0, fminf((float)px, x1 - 1)), cy = fmaxf(y0, fminf((float)py, y1 - 1));
    return (cx - px) * (cx - px) + (cy - py) * (cy - py) <= 13.5f * 13.5f;
}

/* ---- spikes (RE 3259-3262): solid (q7), damage 10 + |0.5 speed| when the facing sensor touches them moving
 * into them; not while already hurt (OTHERS.D) and not with the metal shield (not ported) */
void spikes_update(obj_t *o, player_t *p) {
    if (p->state == ST_DEAD || p->hurt_pending || player_invincible(p) || p->state == ST_HURT || p->shield == 3) return;   /* 3259-3262: not with the metal shield */
    double B = p->ground ? p->gsp : p->vx, Gv = p->ground ? 0 : p->vy;
    int hit = 0; double sp = 0;
    switch (o->type) {
    case OT_SPIKES: hit = Gv >= 0 && sensor_on(o, SENS_BOTTOMBIG); sp = Gv; break;     /* 3259 */
    case OT_SPIKES_D: hit = Gv <= 0 && sensor_on(o, SENS_TOPBIG); sp = Gv; break;      /* 3260 */
    case OT_SPIKES_L: hit = B >= 0 && sensor_on(o, SENS_RIGHTBIG); sp = B; break;      /* 3261 (points left) */
    case OT_SPIKES_R: hit = B <= 0 && sensor_on(o, SENS_LEFTBIG); sp = B; break;       /* 3262 */
    }
    if (hit) player_contact(p, (int)(10 + fabs(0.5 * sp)));
}

/* ---- red spring up-left (RE 3003/3004: B = -14, G = -14, O = -5; the port's yellow RV values are these plus one
 * gravity step, so red = (-14, -13.75)); same top-edge contact rule as the yellow diagonal springs */
void rspring_update(obj_t *o, player_t *p) {
    float bx0, by0, bx1, by1; obox(o, &bx0, &by0, &bx1, &by1);
    if (p->state == ST_DEAD || p->ground || p->vy < 0) return;
    float px0 = p->x - 7, px1 = p->x + 7;
    if (px1 > bx0 && px0 < bx1 && p->y >= by0 - 2 && p->y - 30 < by1) {
        player_spring(p, -14.0, 13.75, by0 - 2);
        int a = ens_anim("rspring_ul_b"); if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); }
    }
}

/* ---- loops: collision layers (RE 3025/3026, 1613-1616, 3031/3032; DV2/DV4 4914-4917).
 * Layer-0 pieces (levellayer016, levellayer0loop04) and layer-1 pieces (levellayer115, levellayer1loop03) are
 * always-solid objects; MV.P picks which set is solid (flag 0 off) and which is ignored (flag 0 on). */
static int is_loop0(int t) { return t == OT_LOOP016 || t == OT_LOOP0Q; }
static int is_loop1(int t) { return t == OT_LOOP115 || t == OT_LOOP1Q; }
void layers_refresh(const player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (is_loop0(o->type)) { if (p->layer == 0) o->fl &= ~1; else o->fl |= 1; }
        if (is_loop1(o->type)) { if (p->layer == 1) o->fl &= ~1; else o->fl |= 1; }
    }
}
void layers_update(player_t *p) {                              /* after the movement block */
    double B = p->ground ? p->gsp : p->vx;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || (o->type != OT_LAYER_0 && o->type != OT_LAYER_1 && o->type != OT_LAYER_SW)) continue;
        if (!disc_on_box(p, o->x, o->y, o->x + 16, o->y + 48)) continue;
        if (o->type == OT_LAYER_0) p->layer = 0;                 /* 1613 */
        else if (o->type == OT_LAYER_1) p->layer = 1;            /* 1614 */
        else if (B > 0) p->layer = 1;                            /* 1615 */
        else if (B < 0) p->layer = 0;                            /* 1616 */
    }
    layers_refresh(p);                                           /* 3031 / 3032 */
}

/* ---- half corkscrew (RE 4715-4724, 4756/4757): scripted state M45. HCL/HCR: invisible 64x16 boxes, hotspot
 * (32,15); B = 80 (plateau), D = 240 (ramp), F = 100 (height). Object fields: st = flag 0 (active), fx = E,
 * fy = A, vy = C (MV.Y at entry). */
#define CK_B 80.0
/* plateau length B per pair: 80 for the 528 px pairs (constructor), and the pair distance - 448 in general
 * (RV dv4_chase: the 784 px DV4 pair holds the plateau to X - x = 576, i.e. B = 336); instance values INFERRED */
static double ck_plateau(const obj_t *o) {
    double best = 1e9;
    for (int k = 0; k < G.nobjs; k++) {
        const obj_t *q = &G.o[k];
        if (!q->alive || q->type != (o->type == OT_CORK_L ? OT_CORK_R : OT_CORK_L) || fabsf(q->y - o->y) > 1) continue;
        double d = o->type == OT_CORK_L ? q->x - o->x : o->x - q->x;
        if (d > 0 && d < best) best = d;
    }
    return best < 1e9 ? best - 448.0 : CK_B;
}
#define CK_D 240.0
#define CK_F 100.0
static int ck_overlap(const obj_t *o) { return box_sensor(SENS_BOTTOMBIG, o->x - 32, o->y - 15, o->x + 32, o->y + 1); }
void cork_start_check(player_t *p) {                           /* 4756 / 4757, end of the frame */
    if (p->state == ST_CORK || !p->ground) return;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || (o->type != OT_CORK_L && o->type != OT_CORK_R) || !ck_overlap(o)) continue;
        if (o->type == OT_CORK_L ? (p->face > 0 && p->gsp > 3 && p->x >= o->x) : (p->face < 0 && p->gsp < -3 && p->x <= o->x)) {
            p->state = ST_CORK; p->state_t = 0;
            o->st = 1; o->vy = (float)MVY(p); o->fx = 0; o->fy = 0;
            if ((p->held & BTN_RIGHT) && p->gsp >= PHYS_TOP && p->gsp < 6) p->gsp += 0.1;     /* 4758 runs after the start */
            if ((p->held & BTN_LEFT) && p->gsp <= -PHYS_TOP && p->gsp > -6) p->gsp -= 0.1;
        }
    }
}
/* one frame of M45: X still moves with B (movement block, U = 0: no vertical move), Y from the profile */
void cork_move(player_t *p) {
    /* 4758 / 4759: holding the direction at top speed accelerates to 6 */
    double *bp = p->ground ? &p->gsp : &p->vx;
    if ((p->held & BTN_RIGHT) && *bp >= PHYS_TOP && *bp < 6) *bp += 0.1;
    if ((p->held & BTN_LEFT) && *bp <= -PHYS_TOP && *bp > -6) *bp -= 0.1;
    player_core_nov(p);                                          /* movement block with U = 0 (X along N, sensors, 1610) */
    if (!p->ground) p->vy = fminf((float)p->vy + PHYS_GRAV, 15.25f);   /* G keeps its gravity (unused while U = 0) */
    double B = p->ground ? p->gsp : p->vx;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || !o->st || (o->type != OT_CORK_L && o->type != OT_CORK_R)) continue;
        double d = o->type == OT_CORK_L ? p->x - o->x : o->x - p->x, E = o->fx, A = o->fy, C = o->vy, Y = MVY(p);
        p->cork_e = E;                                           /* 4715/4716 use last frame's E */
        if (E < 1) { E = d / CK_D; Y = C + CK_F / 2 * c_cos(fmod(180 * E, 360)) - CK_F / 2; }            /* 4717 / 4721 */
        double PB = ck_plateau(o);
        if (E >= 1 && A < PB + CK_D) { Y = C - CK_F; E = d / (2 * CK_D); A = d; }                        /* 4718 / 4722 */
        if (E >= 1 && A >= PB + CK_D) { E = (d - PB) / CK_D; Y = C + CK_F / 2 * c_cos(fmod(180 * E, 360)) - CK_F / 2; }   /* 4719 / 4723 */
        o->fx = (float)E; o->fy = (float)A;
        p->y = Y + ORIG_REF_DY;
        /* 4720 / 4724: slowed down, or reached the far end */
        int end = o->type == OT_CORK_L ? B < 3 : B > -3;
        for (int k = 0; k < G.nobjs && !end; k++) {
            obj_t *q = &G.o[k];
            if (!q->alive || q->type != (o->type == OT_CORK_L ? OT_CORK_R : OT_CORK_L)) continue;
            if (ck_overlap(q) && (o->type == OT_CORK_L ? p->x > q->x : p->x < q->x)) end = 1;
        }
        if (end) {
            p->state = ST_NORMAL; if (p->ground) { p->vx = p->gsp; p->ground = 0; } p->vy = 0;   /* M = 1, U = 1, G = 0 */
            o->st = 0; o->fx = o->fy = 0;
        }
        player_sync_sensors(p);
        return;
    }
    p->state = ST_NORMAL;                                        /* no active corkscrew: back to normal */
}

/* ---- geyser plate (RE 4968/4969 + the platform engine with F = 2, G = 240, H = 1): see objects.c platforms */

/* ---- cards (ObjectCodec_430, events 2569-2578). Act k's cards are numbered from the last-created placement down
 * (DV1 1-4, DV2 5-6, DV3 7-10); o->dir = card ID, tm = E (pickup timer), vy = F (sparkle timer), inv flag 1 = already
 * in the records (drawn half transparent). The collected set goes to SV.card_bits at act clear (save.c). */
static const int CARD_ID0[4] = { 1, 5, 7, 8 };
static uint16_t cards_taken_act;                               /* bits taken this act (written to SV at act clear) */
static int act_index(void) { return G.lvdir[0] == 'd' && G.lvdir[1] == 'v' ? G.lvdir[2] - '1' : -1; }
void cards_spawn_all(void) {
    int a = act_index(), id;
    cards_taken_act = 0;
    if (a < 0 || a > 3) return;
    id = CARD_ID0[a];
    for (int i = G.nobjs - 1; i >= 0; i--) {                   /* last created first */
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_CARD) continue;
        if (o->x < 0) { o->alive = 0; id++; continue; }          /* the template (-295, 108) takes the next number */
        o->dir = id++; o->tm = 0; o->vy = 0;
        int an = ens_anim("card"); if (an >= 0) anim_set(&o->an, &G.ens, an);
    }
}
static int card_bit(int id) { return id - 1; }                 /* card IDs 1..10 -> bits 0..9 of the DV set */
int card_recorded(int id) { int b = card_bit(id); return b >= 0 && b < 16 && (SV.card_bits[0] >> b & 1); }
static void card_sparkle(const obj_t *o) {
    int r1 = cross_rand_raw(32768), r2 = cross_rand_raw(32768), r3 = cross_rand_raw(32768), r4 = cross_rand_raw(32768);
    obj_effect(o->x + ((r1 >> 11) & 15) - ((r2 >> 11) & 15), o->y + ((r3 >> 11) & 15) - ((r4 >> 11) & 15), "ringsparkle");
}
void card_update(obj_t *o, player_t *p) {
    if (o->vy > 10) { o->vy = (float)-cross_rand(60); card_sparkle(o); }                   /* 2569 */
    if (o->tm == 0 && o->an.b) {                                                             /* 2570: 12.5 images/s */
        const anim_t *an = &o->an.b->anims[o->an.anim];
        o->an.frame = (int)fmod(1000.0 * (G.frame / 60.0) / 80.0, (double)an->count); o->an.acc = 0;
    }
    if (o->tm == 0 && p->state != ST_HURT && p->state != ST_DEAD) {                          /* 2571 pickup */
        int px = (int)p->x, py = (int)MVY(p);
        if (fabsf(o->x - px) <= 19 && o->y >= py - 31 && o->y <= py + 19) {
            for (int k = 0; k < 3; k++) card_sparkle(o);
            o->tm = 1; snd_play_orig(32, 220, 128);
            int b = card_bit(o->dir); if (b >= 0 && b < 16) cards_taken_act |= (uint16_t)(1u << b);
        }
    }
    if (o->tm > 0) {                                                                         /* 2572 */
        if (o->an.b) { const anim_t *an = &o->an.b->anims[o->an.anim]; o->an.frame = o->tm % an->count; o->an.acc = 0; }
        o->tm++; o->vy += 10;
        o->x = (float)(int)p->x; o->y = (float)(int)(MVY(p) - 40 - 0.5 * o->tm);
    }
    if (o->tm == 0 && obj_near_view(o, 50) && card_recorded(o->dir)) o->inv = 0, o->fl |= 2;   /* 2573: half transparent */
    if (o->tm > 80) { for (int k = 0; k < 3; k++) card_sparkle(o); o->alive = 0; }           /* 2578 */
}
/* act clear: merge this act's cards into the save (one VMU write per act, never mid-stage) */
int cards_commit(void) {
    uint16_t before = SV.card_bits[0];
    SV.card_bits[0] |= cards_taken_act;
    cards_taken_act = 0;
    return SV.card_bits[0] != before;
}
int cards_taken_now(void) { int n = 0; for (unsigned v = cards_taken_act | SV.card_bits[0]; v; v &= v - 1) n++; return n; }

/* ======================================================================== DV3 (spec_dv3_gimmicks.md) */
static obj_t *first_type(int t) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == t) return &G.o[i]; return NULL; }
static void ens(obj_t *o, const char *nm) { int a = ens_anim(nm); if (a >= 0) anim_set(&o->an, &G.ens, a); else o->an.b = NULL; }
#define WATER_Y 2700.0                     /* RE 5314: ML.B = 2700 at DV3 start */

/* ---- waterfall logs (5131-5140, 5215, 5216). Log: fy = A (double Y; 0 = unset), vy = B, st = C (0 emerging behind
 * the water / 1 falling on the play layer), tm = E x 10, dir = direction 0..31, fl bit 0 = ignored, bit 1 = stopped.
 * Generator: tm = A (starts 96). Kill zones (WaterfallFront3): boxes x-32..x+31, y+17..y+48. */
static void wlog_anim(obj_t *o) { char nm[8]; snprintf(nm, sizeof nm, "wlog%d", (o->dir & 31) / 4); ens(o, nm); }
static obj_t *wlog_new(float x, float y) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    o->type = OT_WLOG; o->alive = 1; o->x = x; o->y = y; o->fy = 0; o->vy = 0; o->st = 0; o->tm = 0; o->dir = 0;
    wlog_anim(o);
    return o;
}
void wlogs_spawn(void) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_WLOG) { o->fy = 0; o->st = 0; o->tm = 0; o->dir = 0; wlog_anim(o); }
        if (o->type == OT_WLOG_GEN) { o->tm = 96; o->an.b = NULL; }
        if (o->type == OT_WF_KILL) o->an.b = NULL;
        if (o->type == OT_FLOATPLAT) ens(o, "floatplat");
    }
}
void wlogs_update(const player_t *p) {
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_WLOG && o->fy == 0) { o->fy = o->y; o->vy = 1.5f; } }   /* 5215 */
    int run = 0;                                                                              /* 5216 */
    for (int i = 0; i < G.nobjs; i++) { obj_t *g = &G.o[i]; if (g->alive && g->type == OT_WLOG_GEN && g->x >= G.camx - 159 && g->x <= G.camx + 585) run = 1; }
    if (!run) return;
    for (int i = 0; i < G.nobjs; i++) {                                                       /* 5131 */
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_WLOG) continue;
        o->y = (float)(int)o->fy; o->fy += o->vy;
        o->dir = (o->dir + (int)fmax(0.0, 1.5 - o->vy)) & 31; wlog_anim(o);
        o->fl &= ~1;
    }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *g = &G.o[i];
        if (!g->alive || g->type != OT_WLOG_GEN) continue;
        g->tm++;
        if (g->tm == 100) { wlog_new(g->x - 64, g->y); g->tm++; }                             /* 5132 */
        if (g->tm == 200) { wlog_new(g->x + 64, g->y); g->tm++; }                             /* 5133 */
        if (g->tm >= 300) { wlog_new(g->x, g->y); g->tm = 0; }                                /* 5134 */
    }
    int touch_bg = 0;
    { int ox, oy, h; const signed char *a, *b;                                               /* 5140: BottomBig on level ground */
      if (mv_sensor_rows(SENS_BOTTOMBIG, &ox, &oy, &a, &b, &h)) for (int j = 0; j < h && !touch_bg; j++) if (b[j] >= a[j] && px_span_solid(&G.lvl, ox + a[j], ox + b[j], oy + j)) touch_bg = 1; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_WLOG) continue;
        if (o->st == 0) { o->vy = -0.4f; o->tm += 4; }                                         /* 5135 (E += 0.4) */
        if (o->st == 0 && o->tm >= 240) o->st = 1;                                             /* 5136 */
        if (o->st == 1) o->vy = fminf(o->vy + 0.05f, 1.2f);                                    /* 5137 */
        for (int k = 0; k < G.nobjs; k++) {                                                    /* 5138 */
            obj_t *z = &G.o[k];
            if (z->alive && z->type == OT_WF_KILL && o->x - 64 < z->x + 32 && o->x + 64 > z->x - 32 && o->y - 16 < z->y + 49 && o->y + 16 > z->y + 17) { o->alive = 0; break; }
        }
        if (!o->alive) continue;
        if (o->y > WATER_Y && !(o->fl & 2)) { o->fl |= 2; o->vy = 0; }                         /* 5139 */
        if (o->st == 0 || touch_bg) o->fl |= 1;                                                /* emerging: other layer; 5140 */
    }
    (void)p;
}

/* ---- boulder switch room (5187-5210). Plate: st = pressed. Trapdoor: c = C (0..90), ang. Boulder: fx, fy = A, B;
 * vx = C, vy = D, ang = E (display), c = H, fl bit 1 active, bit 2 water, bit 4 on ground. */
static int boulder_latch, boulder_fired;
void dv3_switch_spawn(void) {
    boulder_latch = boulder_fired = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_WALLPLATE) { ens(o, "wallplate"); o->st = 0; }
        if (o->type == OT_TRAPDOOR) { ens(o, "trapdoor"); o->c = 0; o->ang = 0; }
        if (o->type == OT_BOULDER) { ens(o, "boulder"); o->fx = o->x; o->fy = o->y; o->vx = 0; o->vy = 0; o->fl = 0; o->c = 0; }
        if (o->type == OT_PPLATE) { ens(o, "pplate"); o->st = 0; }
        if (o->type == OT_DOOR) { ens(o, "door"); o->c = 0; o->layer = 1; }   /* RV plate_f90 (DV2): drawn behind the layer-2 ground, so the sunk door shows only its top */
        if (o->type == OT_TRUNK) { ens(o, "trunk"); o->dir = 0; o->fx = 0; o->vx = 0; }
        if (o->type >= OT_BAM1 && o->type <= OT_BAM4) { static const char *bn[4] = { "bam1", "bam2", "bam3", "bam4" }; ens(o, bn[o->type - OT_BAM1]); o->st = 0; o->tm = 0; o->fx = 0; o->dir = 1; }
    }
    snap_register(&boulder_latch, sizeof boulder_latch); snap_register(&boulder_fired, sizeof boulder_fired);
}
static int probe(const obj_t *b, int x0, int x1, int y0, int y1) {     /* probe rect relative to (x, y) on the background */
    for (int y = y0; y <= y1; y++) if (px_span_solid(&G.lvl, (int)b->x + x0, (int)b->x + x1, (int)b->y + y)) return 1;
    return 0;
}
void dv3_switch_update(player_t *p) {
    obj_t *plate = first_type(OT_WALLPLATE), *door = first_type(OT_TRAPDOOR), *bo = first_type(OT_BOULDER);
    if (plate && !plate->st && (p->state == ST_PUSH || p->state == ST_CYCLONE) && sensor_on(plate, SENS_LEFTBIG)) {   /* 5187 */
        plate->st = 1; plate->an.b = NULL;
    }
    if (plate && plate->st && !boulder_latch) { boulder_latch = 1; snd_play_orig(49, 220, 128); }   /* 5188 */
    if (plate && plate->st && door && door->c < 90) { door->c += 5; door->ang -= 5; }          /* 5189 */
    if (door && door->c >= 90 && bo && !(bo->fl & 4) && !boulder_fired) { boulder_fired = 1; bo->fl |= 2; bo->fx = bo->x; bo->fy = bo->y; }   /* 5191 */
    if (!bo) return;
    int ground = probe(bo, -6, 6, 21, 33);                                                     /* 5194 / 5195: bottom-centre probe */
    if (bo->fl & 2) {                                                                          /* 5198 */
        bo->x = (float)(int)bo->fx; bo->y = (float)(int)bo->fy; bo->fx += bo->vx; bo->fy += bo->vy;
        if (bo->c > 0) bo->c--;
        bo->ang -= bo->vx;                                                                     /* E -= C */
    }
    if (!ground) bo->vy = fminf(12.f, bo->vy + 0.3f - 0.2f * ((bo->fl & 4) ? 1 : 0));          /* 5199 (also while waiting) */
    if ((bo->fl & 2) && ground && bo->vy > 2) { bo->vx = 2; bo->fy -= bo->vy; bo->vy = -0.25f * bo->vy; snd_play_orig(51, 220, 128); }   /* 5200 */
    if ((bo->fl & 2) && ground && bo->vy >= 0 && bo->vy <= 2) { bo->vy = 0; bo->fy -= 1; }    /* 5201 */
    if ((bo->fl & 2) && !ground && bo->c <= 0) {                                               /* 5202: hits enemies while airborne */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *e = &G.o[i];
            if (!e->alive || !enemy_type(e->type) || e->c > 0 || (e->fl & 0x40) || !e->an.b) continue;
            float x0 = bo->x - 31, y0 = bo->y - 31;
            if (!obj_rect_hit(e, (int)x0, (int)y0, (int)x0 + 62, (int)y0 + 62)) continue;
            e->c = 5; e->ky = bo->vy + 4; e->kx = cross_rand(5) - 2; e->A -= fabs(bo->vy) / 5 + 6; e->inv = 1;
            snd_play_orig(24, 220, 128); bo->c = 4;
        }
    }
    if (bo->fl & 2) {                                                                          /* 5203-5208: bridges */
        for (int b = 0; b < G.nobjs; b++) {
            obj_t *br = &G.o[b];
            if (!br->alive || br->type != OT_BRIDGE) continue;
            float bx0 = br->x, by0 = br->y, bx1 = br->x + 192, by1 = br->y + 128;
            int over = 0;
            for (int i = 0; i < G.nobjs && !over; i++) {
                obj_t *lg = &G.o[i];
                if (lg->alive && lg->type == OT_LOG && lg->par == b && fabsf(lg->x + 8 - bo->x) < 39 && fabsf(lg->y + 8 - bo->y) < 39) over = 1;
            }
            if (!over || !(bo->x + 31 > bx0 && bo->x - 31 < bx1 && bo->y + 31 > by0 && bo->y - 31 < by1)) continue;
            snd_play_orig(13, 220, 128);
            for (int i = 0; i < G.nobjs; i++) {                                                /* 5205 */
                obj_t *lg = &G.o[i];
                if (lg->alive && lg->type == OT_LOG && lg->x >= bx0 - 3 && lg->x <= bx1 + 3 && lg->y > by0 && lg->y < by1) { lg->fx = -(float)lg->dir; lg->fy = lg->y; lg->st = 9; lg->par = -1; }
            }
            br->alive = 0;                                                                     /* 5206 */
        }
    }
    for (int i = 0; i < G.nobjs; i++) {                                                        /* 5207 / 5208: collapsing logs */
        obj_t *lg = &G.o[i];
        if (!lg->alive || lg->type != OT_LOG || lg->st != 9) continue;
        lg->y = (float)(int)lg->fy; lg->fy += fmaxf(lg->fx, 0.f); lg->fx += 0.3f;
        if (lg->fx > 0) lg->fl |= 1;
        if (lg->y > G.lvl.h + 64) lg->alive = 0;
    }
    if ((bo->fl & 2) && bo->y > WATER_Y && !(bo->fl & 4)) {                                    /* 5209 */
        bo->fl |= 4; obj_effect(bo->x, (float)WATER_Y, "explosion02"); snd_play_orig(14, 220, 128); bo->vy = 0; bo->vx *= 0.5f;
    }
    if ((bo->fl & 4) && ground) { bo->vx = bo->vy = 0; bo->fl &= ~2; }                        /* 5210 */
}

/* ---- tree trunk -> PushableBlock2 (5142-5153, 5217-5224). Trunk: fx = A (tilt), vx = B (angular speed), dir 0/16. */
void trunks_update(player_t *p) {
    double B = p->ground ? p->gsp : p->vx;
    int Z = player_body_z(p);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_TRUNK) continue;
        if (Z && B != 0) o->fl |= 1;                                                           /* 5218 */
        int hit = 0;
        if (Z && B < 0 && sensor_on(o, SENS_LEFTBIG)) { o->vx -= 3; p->vx = 3; hit = 1; }       /* 5219 */
        else if (Z && B > 0 && sensor_on(o, SENS_RIGHTBIG)) { o->vx += 3; p->vx = -3; hit = 1; }   /* 5220 */
        else if (p->state == ST_CYCLONE && sensor_on(o, SENS_LEFTBIG)) { o->vx -= 3; p->vx = 3; hit = 2; }   /* 5221 */
        else if (p->state == ST_CYCLONE && sensor_on(o, SENS_RIGHTBIG)) { o->vx += 3; p->vx = -3; hit = 2; } /* 5222 */
        if (hit) {
            snd_play_orig(23, 220, 128);
            double n = p->N, g = hit == 2 ? -3.0 : (-PHYS_JUMP) * (n == 90 || n == 270 ? 0 : fd_cos(n / 57.29577951308232)) - (n == 180 ? 0 : fd_sin(n / 57.29577951308232)) * B;
            p->ground = 0; p->N = 0; p->vy = g; if (hit == 1) p->state = ST_NORMAL;
        }
        if (!obj_near_view(o, 200)) continue;                                                  /* 5217: loop_trunk */
        o->ang = -o->fx; o->fx = fmaxf(-90.f, fminf(90.f, o->fx + o->vx)); o->fl |= 1;          /* 5142 */
        if (fabsf(o->fx) < 1 && fabsf(o->vx) < 0.2f) { o->fl &= ~1; o->fx = 0; o->vx = 0; }     /* 5144 */
        if (o->fx > 0 && o->fx <= 15) o->vx = (o->vx - 0.2f) * 0.95f;                          /* 5145 */
        if (o->fx < 0 && o->fx >= -15) o->vx = (o->vx + 0.2f) * 0.95f;                         /* 5146 */
        if (o->fx > 15) o->vx += 0.1f;                                                         /* 5147 */
        if (o->fx < -15) o->vx -= 0.1f;                                                        /* 5148 */
        if (o->fx < 0 && o->dir == 0) { o->dir = 16; o->x -= 32; ens(o, "trunk_l"); }          /* 5149 */
        if (o->fx > 0 && o->dir == 16) { o->dir = 0; o->x += 32; ens(o, "trunk"); }            /* 5150 */
        if (o->fx <= -90 || o->fx >= 90) {                                                     /* 5151: falls into a block */
            obj_t *b = obj_alloc();
            if (b) { b->type = OT_PBLOCK2; b->alive = 1; b->x = o->x; b->y = o->y; b->dir = o->dir; ens(b, o->dir == 16 ? "pblock2_l" : "pblock2"); }
            camera_shake(3); snd_play_orig(51, 220, 128); o->alive = 0;
        }
    }
}

/* ---- bamboo spike traps (5227-5234): bam1 pivot (st = A, tm = B, fx = C swing, dir = D side), bam2 joint (hurts),
 * bam3 pole, bam4 head; paired by x. Pole image: hotspot (8,52), action point (8,-5): 57 px up the pole. */
static obj_t *bam_of(int type, float x) { for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == type && fabsf(o->x - x) < 4) return o; } return NULL; }
static void pole_tip(const obj_t *o, float *x, float *y) { double a = o->ang / 57.29577951308232; *x = o->x + (float)(int)(-57 * fd_sin(a)); *y = o->y + (float)(int)(-57 * fd_cos(a)); }
void bamboo_update(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b1 = &G.o[i];
        if (!b1->alive || b1->type != OT_BAM1) continue;
        obj_t *b2 = bam_of(OT_BAM2, b1->x), *b3 = bam_of(OT_BAM3, b1->x), *b4 = bam_of(OT_BAM4, b1->x);
        float tx, ty; pole_tip(b1, &tx, &ty);                                                  /* 5227 */
        if (b2) { b2->x = tx; b2->y = ty; }
        if (b3) { b3->x = tx; b3->y = ty; b3->ang = -b1->ang; }
        b1->ang = b1->fx * b1->dir;
        if (b3 && b4) { pole_tip(b3, &tx, &ty); b4->x = tx; b4->y = ty; }
        if (b1->st == 0 || b1->st == 2) { b1->tm++; if (b1->tm > 50) { b1->tm = 0; b1->st++; } }   /* 5229 / 5230 */
        if (b1->st == 1) { b1->fx += 5; if (b1->fx >= 70) b1->st = 2; }                        /* 5231 / 5232 */
        if (b1->st == 3) { b1->fx -= 5; if (b1->fx <= 0) { b1->st = 0; b1->dir = -b1->dir; } }  /* 5233 / 5234 */
        if (b2 && p->shield != 3 && p->state != ST_HURT && p->state != ST_DEAD && !player_invincible(p) && !p->hurt_pending) {   /* 5228 (metal shield immune) */
            int sx0 = (int)floor(p->x) - 7, sy0 = (int)floor(MVY(p)) - 34;
            if (obj_rect_hit(b2, sx0, sy0, sx0 + 15, sy0 + 46)) player_contact(p, 5);
        }
    }
}

static void pb_box(const obj_t *b, int *x1, int *y1, int *x2, int *y2);
/* ---- pressure plate + door (5182-5186): pressed while Lilac stands on it or a resting pushable block overlaps it */
static int pplate_latch;
void pplate_update(player_t *p) {
    obj_t *pl = first_type(OT_PPLATE), *d = first_type(OT_DOOR);
    if (!pl) return;
    /* 4971 (RE): a q14 block (pushable blocks) that is resting (flag 4; port: fl 0x10) and overlaps the plate presses
     * it, exactly like Lilac on it. PushableBlock2 never gets flag 4 (spec_dv3 §4), so only PushableBlock1 counts.
     * Port deviation (D-036, by request): in DV2 the 48 px block cannot enter the plate's pit (RV o2 sweep: it always
     * rests on the ledges at Y 1137, 28 px above the plate, and never presses it), so a resting block counts when it
     * covers the plate from up to PB_PLATE_REACH px above, as if its weight bore down on the switch. */
    enum { PB_PLATE_REACH = 32 };
    int pressed = 0;
    for (int i = 0; i < G.nobjs && !pressed; i++) {
        const obj_t *b = &G.o[i];
        if (!b->alive || (b->type != OT_PBLOCK1 && b->type != OT_PBLOCK2) || !(b->fl & 0x10)) continue;
        int x1, y1, x2, y2; pb_box(b, &x1, &y1, &x2, &y2);
        pressed = obj_rect_hit(pl, x1, y1, x2, y2 + PB_PLATE_REACH);
    }
    if (p->ground && sensor_on(pl, SENS_BOTTOMBIG)) pressed = 1;                                /* 4973 */
    for (int i = 0; i < G.nobjs && !pressed; i++)                                               /* 4972: a parked bike holds it */
        if (G.o[i].alive && G.o[i].type == OT_PBIKE && obj_obj_hit(&G.o[i], pl)) pressed = 1;
    pl->hidden = pressed;                                   /* 5182 shows the plate each frame; 4971/4973 hide it while pressed */
    if (pressed && !pplate_latch) { pplate_latch = 1; snd_play_orig(49, 220, 128); }          /* 5183 */
    if (!d) return;
    if (pressed && d->c < 128) { d->y += 8; d->c += 8; }   /* 5184: RV plate3 (DV2): the door sinks 8 px/frame (top stays in view) */
    int under = p->x >= d->x - 15 && p->x <= d->x + 47 && MVY(p) > d->y && MVY(p) <= d->y + 128 + 31;   /* 5185 */
    if (!pressed && d->c > 0 && !under) { d->y -= 2; d->c -= 2; }                              /* 5186: rises back 2 px/frame */
}

/* ---- keystones (ObjectKeystone_429; RE build/re/spec_keystones.md). av: 0 F (base y, 0 = not initialised),
 * 1 H (x), 2 B (rise speed), 3 I (x step), 4 A (homing percent); st = G (homing), tm = E (taken / spin) */
void keystone_spawn(float x, float y, double B, double I) {
    obj_t *k = obj_alloc(); int a = ens_anim("keystone");
    if (!k || a < 0) return;
    k->type = OT_KEYSTONE; k->alive = 1; k->x = x; k->y = y; k->layer = 2; k->born = G.frame;
    k->av[0] = 0; k->av[2] = B; k->av[3] = I; k->av[4] = 0; k->st = 0; k->tm = 0;
    anim_set(&k->an, &G.ens, a);
}
static int keystones_taken_frame = -1;
static double c_sin_deg(double d) { return d == 180. ? 0. : fd_sin(d / 57.29577951308232); }
static void force_frame_ens(obj_t *o, int f) { if (o->an.b) { const anim_t *a = &o->an.b->anims[o->an.anim]; o->an.frame = f < a->count ? f : a->count - 1; o->an.acc = 0; } }
void keystone_update(obj_t *k, player_t *p) {
    if (k->born == G.frame) return;                                   /* created objects run from the next frame */
    if (k->tm == 0) force_frame_ens(k, (G.frame / 6) % 4);            /* 2523: 10 images per second */
    if (k->av[0] == 0) {
        if (obj_overlaps_bg(k) || k->x < G.camx - 50 || k->x > G.camx + SCREEN_W + 50 || k->y < G.camy - 50 || k->y > G.camy + SCREEN_H + 50) k->st = 1;   /* 2550 / 2551 */
        k->av[0] = k->y; k->av[1] = k->x;                             /* 2552 */
    } else if (!k->st && k->tm == 0) {                                /* 2553 bob */
        double a = (int)G.frame * 1.5, sn = a == 180 ? 0 : 4 * c_sin_deg(a);
        k->y = (float)(int)(k->av[0] + sn); k->x = (float)(int)k->av[1];
    }
    if (k->av[2] < 0 && !k->st && k->tm == 0) { k->av[0] += k->av[2]; k->av[2] = fmin(k->av[2] + 0.1, 0); k->av[1] += k->av[3]; }   /* 2554 */
    int px = (int)floor(p->x), py = (int)floor(p->y - ORIG_REF_DY);
    if (!k->st && k->tm == 0 && k->av[2] == 0 && fabsf(k->y - py) <= 119 && fabsf(k->x - px) <= 119) k->st = 1;          /* 2556 magnet */
    if (k->st) {                                                      /* 2557 homing */
        double A = k->av[4];
        k->x = (float)(int)(((int)k->x * (100 - A) + A * p->x) * 0.01);
        k->y = (float)(int)(((int)k->y * (100 - A) + A * (p->y - ORIG_REF_DY)) * 0.01);
        k->av[2] = 0; k->av[4] = fmin(A + 1, 100);
    }
    if (k->tm == 0 && p->state != ST_HURT && p->state != ST_DEAD && k->av[2] == 0 &&
        fabsf(k->x - px) <= 19 && k->y >= py - 31 && k->y <= py + 19) {                                                    /* 2558 / 1291 */
        k->tm = 1; cross_rand(1); obj_effect(k->x, k->y, "ringsparkle");
    }
    if (k->tm == 1) {                                                 /* 2564: effects once per frame */
        if (keystones_taken_frame != G.frame) {
            keystones_taken_frame = G.frame;
            p->energy = fminf(PHYS_ENERGY_MAX, p->energy + 10); G.keystones++;
            if (--p->crystals <= 0) { p->crystals = CRYSTALS_PER_LIFE; p->lives++; }
            snd_play_orig(39, 220, 128);
        }
        k->tm = 2; k->st = 0; int a = ens_anim("keystone_t"); if (a >= 0) anim_set(&k->an, &G.ens, a);
    }
    if (k->tm > 1) { k->ang = (float)fmod(720 - 2.0 * k->tm, 360); k->tm += 15; }                                       /* 2567 */
    if (k->tm > 360) {                                                                                                     /* 2568 */
        for (int s2 = 0; s2 < 3; s2++) obj_effect(k->x + cross_rand(12) - cross_rand(25), k->y + cross_rand(12) - cross_rand(25), "ringsparkle");
        snd_play_orig(48, 220, 128); k->alive = 0;
    }
}

/* ==== roll tunnels, spiral helix, spinning logs, pushable block (build/re/spec_roll_gimmicks.md) ==== */
static double r_sin(double deg) { return deg == 180. ? 0. : fd_sin(deg / 57.29577951308232); }
static int sbb_rows(int s, int *ox, int *oy, const signed char **a, const signed char **b, int *h) { return mv_sensor_rows(s, ox, oy, a, b, h); }
static int sens_on_obj(int s, const obj_t *o) {
    int ox, oy, h; const signed char *a, *b;
    if (!sbb_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static int sbb_on_type(int type) {                       /* S_BB overlaps any instance (flag 0 not tested) */
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == type && sens_on_obj(SENS_BOTTOMBIG, &G.o[i])) return i;
    return -1;
}
static int q27_type(void) {                              /* QualifierList +0x22210: spiral mask (DV1/DV2), ball tunnel (DV3) */
    if (!strcmp(G.lvdir, "dv1") || !strcmp(G.lvdir, "dv2")) return OT_SPIRAL_MASK;
    if (!strcmp(G.lvdir, "dv3")) return OT_BALLTUNNEL;
    return -1;
}
static struct { int g40, g41, g42, g43, g44, push_prev11, skinL; } TN;
void tunnels_reset(void) { memset(&TN, 0, sizeof TN); snap_register(&TN, sizeof TN); }

static void roll_enter(player_t *p) { p->state = ST_ROLL; p->state_t = 0; p->tun_lock = 1; snd_play_orig(7, 220, 128); }
static void q27_flag0(int on) {
    int t = q27_type();
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == t) { if (on) G.o[i].fl |= 1; else G.o[i].fl &= ~1; }
}
static void set_B(player_t *p, double B) { if (p->ground) p->gsp = B; else p->vx = B; }
static double get_B(const player_t *p) { return p->ground ? p->gsp : p->vx; }

/* end of the frame: 4739-4755 (DV1-DV3) */
void tunnel_events(player_t *p) {
    int t = q27_type();
    if (t < 0) return;
    int on = sbb_on_type(t) >= 0, L = p->ground;
    int r = (p->held & BTN_RIGHT) != 0, l = (p->held & BTN_LEFT) != 0;
    if (p->state != ST_DEAD) {
        if (on && L && p->state != ST_ROLL) roll_enter(p);                                       /* 4739 */
        int c;
        c = on && L && get_B(p) == 0;                                                            /* 4740 */
        if (c && !TN.g40) { roll_enter(p); set_B(p, -10 * r_sin(p->N)); } TN.g40 = c;
        c = on && L && get_B(p) == 0 && r;                                                       /* 4741 (T, U untouched) */
        if (c && !TN.g41) { p->state = ST_ROLL; p->tun_lock = 1; set_B(p, -10 * r_sin(p->N)); snd_play_orig(7, 220, 128); } TN.g41 = c;
        c = on && L && get_B(p) == 0 && l;                                                       /* 4742 */
        if (c && !TN.g42) { p->state = ST_ROLL; p->tun_lock = 1; set_B(p, -10 * r_sin(p->N)); snd_play_orig(7, 220, 128); } TN.g42 = c;
        c = on && L && get_B(p) == 0 && p->face > 0;                                            /* 4743 */
        if (c && !TN.g43) { roll_enter(p); set_B(p, 3.5); } TN.g43 = c;
        c = on && L && get_B(p) == 0 && p->face < 0;                                            /* 4744 */
        if (c && !TN.g44) { roll_enter(p); set_B(p, -3.5); } TN.g44 = c;
    }
    if (p->tun_lock == 1 && !on) p->tun_lock = 0;                                                /* 4745 */
    int mi = sbb_on_type(OT_SPIRAL_MASK);                                                        /* 4746 / 4747: flag 0 off, no M test */
    if (mi >= 0 && !(G.o[mi].fl & 1)) { double B = get_B(p); if (B > 0) set_B(p, fmax(B, 3.)); else if (B < 0) set_B(p, fmin(B, -3.)); }
    /* 5181 (DV3) sets N = 10 on list +0x60b0 (another character's part, empty for Lilac), not MV.N: no effect */
    if (t != OT_SPIRAL_MASK) return;

    obj_t *top = first_type(OT_SPIRAL_TOP), *bot = first_type(OT_SPIRAL_BOTTOM), *fr = first_type(OT_SPIRAL_FRONT);
    double X = p->x, B = get_B(p);
    if (top) {                                                                                   /* 4748-4751 */
        double *A = &top->av[0], *TB = &top->av[1], *C = &top->av[2];
        if (*A == 0 && B >= 0 && p->state != ST_DEAD && X >= top->x && disc_on_box(p, top->x - 16, top->y - 46, top->x + 16, top->y + 18)) {
            *A = 1; *TB = B; p->state = ST_ROLL;
        }
        if (*A == 1) {
            *C += *TB; *TB = fmin(*TB + 0.1, 12.);
            if (fr) fr->hidden = 1;
            p->helix = 1;
            p->x = top->x + 72 * r_sin(*C); p->y = top->y + 0.3 * *C + ORIG_REF_DY;
            q27_flag0(1); p->in_lock = 1; p->state = ST_ROLL; set_B(p, *TB);
            player_sync_sensors(p);
            double m = fmod(*C, 360.);
            if (m > 90 && m < 270 && fr) fr->hidden = 0;                                         /* 4750 */
            if (*C >= 720 || *C < 0) { *A = 0; p->helix = 0; q27_flag0(0); p->in_lock = 0; }    /* 4751 */
        }
    }
    if (bot) {                                                                                   /* 4752-4755 */
        double *A = &bot->av[0], *BB = &bot->av[1], *C = &bot->av[2];
        B = get_B(p); X = p->x;
        if (*A == 0 && B <= 0 && p->state != ST_DEAD && X <= bot->x && !(top && top->av[0] == 1) &&
            disc_on_box(p, bot->x - 16, bot->y - 16, bot->x + 16, bot->y + 16)) {
            *A = 1; *BB = B; p->state = ST_ROLL;
        }
        if (*A == 1) {
            if (fr) fr->hidden = 1;
            p->helix = 1;
            p->x = bot->x + 72 * r_sin(*C); p->y = bot->y + 0.3 * *C + ORIG_REF_DY;            /* old C */
            q27_flag0(1);
            *C += *BB; *BB = fmin(*BB + 0.05, 12.);
            p->in_lock = 1; set_B(p, *BB); p->state = ST_ROLL;
            player_sync_sensors(p);
            double m = fmod(fabs(*C), 360.);
            if (m > 90 && m < 270 && fr) fr->hidden = 0;                                         /* 4754 */
            if (*C <= -720 || *C > 0) { *A = 0; p->helix = 0; q27_flag0(0); p->in_lock = 0; }   /* 4755 */
        }
    }
}
/* SpiralTunnelFront: drawn in front of Lilac while visible (4750 / 4754 move it to the front of its layer) */
void spiral_front_draw(void) {
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_SPIRAL_FRONT || o->hidden || !o->an.b) continue;
        frame_draw(o->an.b, anim_frame(&o->an), floorf(o->x - G.camx), floorf(o->y - G.camy), 0, 0xffffffff);
    }
}

/* ---- spinning logs (DV2, 4980-4986). Log: av[0] = A (spin), dir = direction 0..31; mask: av[0] = A (sink).
 * ML.C = ML.B + 3 cos(1.5 t), ML.B = 2700 (5109 / 4270; t = the global counter, offset INFERRED 0) */
void spinlogs_update(player_t *p) {
    if (strcmp(G.lvdir, "dv2")) return;
    double MLC = 2700 + 3 * c_cos(fmod(1.5 * G.frame, 360.));
    int L = p->ground, on_mask = 0;
    for (int i = 0; i < G.nobjs; i++) {                                                  /* 4980 */
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_SPINLOG || !(o->av[0] > 0)) continue;
        o->dir = ((int)(o->dir + o->av[0] / 40) % 32 + 32) % 32;                           /* wrap mod 32 (INFERRED, U5) */
        o->av[0] -= 1;
    }
    for (int i = 0; i < G.nobjs; i++) {                                                  /* 4981 */
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_SPINLOG) o->y = (float)(int)MLC;
        if (o->type == OT_SPINLOG_MASK) { o->y = (float)(int)(MLC + o->av[0]); o->fl &= ~1; }
    }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_SPINLOG && L && sens_on_obj(SENS_BOTTOMBIG, o)) o->av[0] = fmin(fmax(o->av[0] + 4, 30.), 320.);   /* 4983 */
        if (o->type == OT_SPINLOG_MASK && sens_on_obj(SENS_BOTTOMBIG, o)) { on_mask = 1; if (L) o->av[0] = fmin(o->av[0] + 0.5, 20.); }   /* 4984 */
    }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_SPINLOG_MASK) {
            if (!on_mask) o->av[0] = fmax(o->av[0] - 1, 0.);                                /* 4985 */
            if (o->av[0] >= 15) o->fl |= 1;                                                 /* 4986: ignored by the sensors */
        }
        if (o->type == OT_SPINLOG) {                                                        /* direction image: nearest of the 8 (INFERRED) */
            static const char *nm[8] = { "slog0", "slog1", "slog2", "slog3", "slog4", "slog5", "slog6", "slog7" };
            int k = ((o->dir + 2) / 4) % 8, a = ens_anim(nm[k]);
            if (a >= 0 && o->an.anim != a) anim_set(&o->an, &G.ens, a);
        }
    }
}

/* ---- pushable block (PushableBlock1_493, 2711-2735). D = o->dir, flags o->fl: 0x08 active, 0x10 resting;
 * av: 1 = B (push-sound timer), 4 = E (dust), 5 = F (fall speed), 7 = H (crush cooldown). Box X-21..X+27, Y-41..Y+7. */
static int obst(int x, int y) { return solid_at(&G.lvl, (float)x, (float)y); }
static void pb_box(const obj_t *b, int *x1, int *y1, int *x2, int *y2) { *x1 = (int)b->x - 21; *y1 = (int)b->y - 41; *x2 = (int)b->x + 27; *y2 = (int)b->y + 7; }
void pblocks_update(player_t *p) {
    int any = 0;
    p->push_s = p->push_s > 0 ? p->push_s - 1 : 0;                                          /* 2711 */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_PBLOCK1) continue;
        any = 1;
        if (b->dir == 0) b->fl &= ~0x10;                                                     /* 2712 */
        if (b->fl & 0x08) {                                                                  /* 2713: 1269-1271 */
            int x1, y1, x2, y2; pb_box(b, &x1, &y1, &x2, &y2);
            if (obst(x2 - 1, y2 - 2) || obst(x2 - 1, y1)) { b->x -= 1; pb_box(b, &x1, &y1, &x2, &y2); }
            if (obst(x1, y2 - 2) || obst(x1, y1)) { b->x += 1; pb_box(b, &x1, &y1, &x2, &y2); }
            int mx = (x1 + x2) / 2;
            /* 1271 (partly read, U7/U11): RV pblock_dv2 shows a resting block keeps its Y (no bob): lifted only when
             * its bottom row is inside the ground, resting (flag 4) when the row below is solid */
            if (obst(x1, y2 - 1) || obst(x2 - 1, y2 - 1) || obst(mx - 1, y2 - 1) || obst(mx, y2 - 1)) { b->y -= 1; b->fl |= 0x10; }
            else if (obst(x1, y2) || obst(x2 - 1, y2) || obst(mx - 1, y2) || obst(mx, y2)) b->fl |= 0x10;
        }
    }
    if (!any) return;
    int M11 = p->state == ST_PUSH && p->state_t >= 1;      /* the push events run from the frame after the M11 entry (RV pblock_dv2) */
    if (M11) TN.skinL++;                                                                        /* 2714 */
    if (!M11 && TN.push_prev11) { TN.skinL = 0; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_PBLOCK1) G.o[i].dir = 0; }   /* 2715 */
    TN.push_prev11 = M11;
    for (int side = 1; side >= -1; side -= 2) {                                             /* 2716/2717, 2718/2719 */
        if (M11 && p->face == side)
            for (int i = 0; i < G.nobjs; i++) {
                obj_t *b = &G.o[i];
                if (!b->alive || b->type != OT_PBLOCK1 || !(b->fl & 0x10)) continue;
                int x1, y1, x2, y2; pb_box(b, &x1, &y1, &x2, &y2);
                if (!box_sensor(side > 0 ? SENS_RIGHTBIG : SENS_LEFTBIG, (float)x1, (float)y1, (float)x2, (float)y2)) continue;
                int ex = side > 0 ? x2 : x1 - 1;
                if (!obst(ex, y2 - 2) && !obst(ex, y1)) b->dir = side;
            }
        int moved = 0;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *b = &G.o[i];
            if (!b->alive || b->type != OT_PBLOCK1 || b->dir != side || !(b->fl & 0x10)) continue;
            b->x += side; b->av[1] = 4; moved = 1;
        }
        if (moved) { p->x += side; TN.skinL = 0; player_sync_sensors(p); set_B(p, 0); p->push_s = 4; }
    }
    if (TN.skinL == 40) { TN.skinL = 41; voice_play("lilac_cantmoveit"); }                        /* 2720 (voice choice INFERRED) */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i];
        if (!b->alive || b->type != OT_PBLOCK1) continue;
        int x1, y1, x2, y2; pb_box(b, &x1, &y1, &x2, &y2);
        if (b->dir && (b->fl & 0x10)) b->av[4] += 1;                                         /* 2721 */
        if (b->av[4] > 2 && (b->fl & 0x10)) { obj_effect((float)(x1 + rand() % (x2 - x1)), (float)y2, "dust"); b->av[4] = 0; }   /* 2722 */
        if (b->dir && (b->fl & 0x10) && b->av[1] < 2) snd_play_orig(43, 160, 128);           /* 2723 (play while not playing: INFERRED from B) */
        if (b->av[1] > 1) b->av[1] -= 1;                                                     /* 2724 */
        else if (b->av[1] == 1) b->av[1] = 0;                                                /* 2725 */
        if ((b->fl & 0x08) && !(b->fl & 0x10)) { b->y = (float)(int)(b->y + b->av[5]); b->av[5] += 0.25; }   /* 2726 */
        if ((b->fl & 0x08) && (b->fl & 0x10)) b->av[5] = 0;                                  /* 2727 */
        if (!(b->fl & 0x08) && b->x > G.camx && b->x < G.camx + 426 && b->y > G.camy && b->y < G.camy + 240) b->fl |= 0x08;   /* 2728 */
        else if ((b->fl & 0x08) && (b->x <= G.camx || b->x >= G.camx + 426 || b->y <= G.camy)) b->fl &= ~0x08;   /* 2729 */
        if ((b->fl & 0x08) && b->y > G.lvl.h + 32) { b->alive = 0; continue; }               /* 2730 */
        if (b->av[7] <= 0 && !(b->fl & 0x10))                                                /* 2734: crush */
            for (int k = 0; k < G.nobjs; k++) {
                obj_t *e = &G.o[k];
                if (!e->alive || !enemy_type(e->type) || e->c > 0 || !obj_obj_hit(b, e)) continue;
                e->c = 5; e->kx = rand() % 5 - 2; e->ky = 4 + b->av[5]; e->A -= fabs(b->av[5] / 5) + 4;
                snd_play_orig(24, 220, 128); b->av[7] = 4;
                break;
            }
        if (b->av[7] > 0) b->av[7] -= 1;                                                     /* 2735 */
    }
}
