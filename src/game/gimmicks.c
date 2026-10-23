/* Level gimmicks (RE: build/re/spec_traversal.md, spec_cards_results.md, spec_dv3_gimmicks.md).
 * Spikes (all directions), red springs, loop collision layers (MV.P), the half corkscrew ride (M45),
 * geyser plates, the collectible cards (ObjectCodec_430). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../fp.h"

static double MVY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double c_cos(double deg) { return (deg == 90. || deg == 270.) ? 0. : fd_cos(deg / 57.29577951); }
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
    if (p->state == ST_DEAD || p->hurt_pending || player_invincible(p) || p->state == ST_HURT) return;
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
        if (E >= 1 && A < CK_B + CK_D) { Y = C - CK_F; E = d / (2 * CK_D); A = d; }                      /* 4718 / 4722 */
        if (E >= 1 && A >= CK_B + CK_D) { E = (d - CK_B) / CK_D; Y = C + CK_F / 2 * c_cos(fmod(180 * E, 360)) - CK_F / 2; }   /* 4719 / 4723 */
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
