/* Aqua Tunnel bosses (session 12): the Iron Worm (AT1 miniboss, build/re/spec_ironworm.md), the Chomper (AT2 miniboss,
 * build/re/spec_chomper.md) and the Chomper Snake (AT4 boss, build/re/spec_chompersnake.md), with their shared acid
 * (MinibossAcid_774) and the worm's rocks (EnemyRock_776). Static RE specs; traces: tests/orig_at (iw_*, ch_*, sn_*).
 * Event numbers are Frames::event_func_N. Labels: RE (read from code), RV (original trace), INFERRED (reasoned).
 * Everything runs in aqua_update() after the shared enemy hits (combat_frame), as in the original's event order. */
#include "../fp.h"
#include <math.h>
#include <string.h>

extern int aqua_gt_off;
static int GT(void) { return G.frame + aqua_gt_off; }
static int act(void) { return aqua_level(); }
static double c_sin(double d) { return d == 180. ? 0. : fd_sin(d / 57.29577951308232); }
static double c_cos(double d) { return (d == 90. || d == 270.) ? 0. : fd_cos(d / 57.29577951308232); }
static int R(int n) { return cross_rand(n); }                       /* Random(n): one LCG step */
static void shake_min(float z) { if (G.cam.Z < z) G.cam.Z = z; }   /* CAM.Z = max(CAM.Z, z) */
static int PX(const player_t *p) { return (int)floor(p->x); }
static int PY(const player_t *p) { return (int)floor(p->y - ORIG_REF_DY); }

static int set_anim_ens(obj_t *o, const char *name) {              /* force_animation: no-op when already playing, counter kept */
    int a = ens_anim(name);
    if (a < 0) return 0;
    if (o->an.b != &G.ens || o->an.anim != a) { int acc = o->an.acc; anim_set(&o->an, &G.ens, a); o->an.acc = acc; }
    return 1;
}
static void last_frame(obj_t *o) {                                   /* force_frame(9) + restore_frame: clamps to the last image */
    if (!o->an.b) return;
    const anim_t *a = &o->an.b->anims[o->an.anim];
    o->an.frame = a->count - 1; o->an.done = 0;
}
static obj_t *spawn(int type, const char *anim, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    if (anim) set_anim_ens(o, anim);
    return o;
}
static void box_of(const obj_t *o, float *x0, float *y0, float *x1, float *y1) {   /* unrotated image box (box collision) */
    if (!o->an.b) { *x0 = *x1 = o->x; *y0 = *y1 = o->y; return; }
    const frame_t *f = anim_frame(&o->an);
    *x0 = floorf(o->x) - f->hx; *y0 = floorf(o->y) - f->hy; *x1 = *x0 + f->w; *y1 = *y0 + f->h;
}
/* PlayerCharacterSensor_375 (CSENS / SENSOR): x -7..8, y -34..12 (anim 1: -13..14) around (int X, int Y) */
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {
    int X = PX(p), Y = PY(p);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return obj_rect_hit(o, a, b, c, d); }
/* Player_326: 27-px disc at (int X, int Y) against a box */
static int p326_box(const player_t *p, float x0, float y0, float x1, float y1) {
    int px = PX(p), py = PY(p);
    float cx = fmaxf(x0, fminf((float)px, x1 - 1)), cy = fmaxf(y0, fminf((float)py, y1 - 1));
    return (cx - px) * (cx - px) + (cy - py) * (cy - py) <= 13.5f * 13.5f;
}
static int vulnerable(const player_t *p) { return p->state != ST_DEAD && !p->hurt_pending && p->invuln <= 0; }   /* MV.R = 0, OTHERS.E <= 0 */
static void cube_rect_of(int *ok, float *x, float *y) { *ok = player_cube_xy(x, y); }

/* ================================================================ acid (MinibossAcid_774) */
/* fields: vx = A (x speed), vy = B (y speed), st = C (0 flying, 1 splash, 2 stuck), fx / fy = E / F (offset from CSENS),
 * par = owner (1 worm, 2 Chomper, 3 snake) */
static obj_t *acid_new(float x, float y, int owner, int layer) {
    (void)R(1);                                                     /* ctor: direction (seed >> 26) & 31 (one LCG step) */
    obj_t *a = spawn(OT_ACID, "acid", x, y, layer);
    if (a) { a->par = owner; a->vx = 3; a->vy = 0; a->st = 0; }
    return a;
}
static void acid_splash(obj_t *a, int st) { a->st = st; set_anim_ens(a, "acid_splat"); a->an.frame = 0; a->an.done = 0; a->an.acc = 0; }
static int on_slime(const obj_t *o), on_slime_col(const obj_t *o);
static int on_raft(const obj_t *o);
static void acids_frame(player_t *p) {
    int sticky = 0, snd = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *a = &G.o[i];
        if (!a->alive || a->type != OT_ACID) continue;
        if (a->st != 0 && a->an.done) { a->alive = 0; continue; }                         /* 4237 / 4462: splash over */
        if (a->born == G.frame) continue;
        /* collisions first in the frame: background (worm; snake while falling), slime / raft (snake), Milla's cube A >= 4 */
        if (a->st == 0) {
            int ov = 0;
            if (a->par == 1) ov = obj_overlaps_bg(a);
            if (a->par == 3 && a->layer == 2) ov = (a->vy > 0 && obj_overlaps_bg(a)) || on_slime_col(a) || on_raft(a);   /* col_1_19 / col_12_19 / col_19_17: same layer only (4620 moves it up) */
            if (ov && !(a->fl & 1)) { acid_splash(a, 1); snd_play_orig(69, 220, 128); }   /* edge-triggered (INFERRED latch) */
            a->fl = (a->fl & ~1) | (ov ? 1 : 0);
            float cx, cy; int cA; cube_rect_of(&cA, &cx, &cy);
            if (a->st == 0 && cA >= 4 && player_cube_hits(a)) { acid_splash(a, 1); snd_play_orig(69, 220, 128); }   /* col_0_16 / col_0_17 */
        }
        if (a->st == 0) { a->x = (float)(int)(a->x + a->vx); a->y = (float)(int)(a->y + a->vy); a->vy = fminf(a->vy + 0.2f, 12); }   /* 4233 / 4458 */
    }
    for (int i = 0; i < G.nobjs; i++) {                                                  /* 4234 / 4459: damage 5 */
        obj_t *a = &G.o[i];
        if (a->alive && a->type == OT_ACID && a->st == 0 && vulnerable(p) && csens_hit(p, a)) { player_contact(p, 5); p->tint = 100; break; }
    }
    for (int i = 0; i < G.nobjs; i++) {                                                  /* 4235 / 4460: stick + splash */
        obj_t *a = &G.o[i];
        if (!a->alive || a->type != OT_ACID || a->st != 0 || p->state == ST_DEAD || !csens_hit(p, a)) continue;
        acid_splash(a, 2); a->fx = a->x - PX(p); a->fy = a->y - PY(p); snd = 1; p->tint = 100;
    }
    if (snd) snd_play_orig(69, 220, 128);
    for (int i = 0; i < G.nobjs; i++) {                                                  /* 4236 / 4461 */
        obj_t *a = &G.o[i];
        if (!a->alive || a->type != OT_ACID || a->st != 2) continue;
        a->x = PX(p) + a->fx; a->y = PY(p) + a->fy;
        if (a->par != 1) sticky = 1;
    }
    if (sticky) { if (p->ground) p->gsp *= 0.9; else p->vx *= 0.9; }                   /* 4461: MV.B x 0.9 while stuck */
}

/* ================================================================ Iron Worm (AT1) */
/* head: A HP, av[0] D phase speed, av[1] E length, av[2] F phase, av[3] G sway, st H, tm I, av[4] J facing, av[5] X,
 * av[6] Y flight, av[7] flag 1 (thrash done); c / fl / inv shared enemy fields. Segment: av[0] D index, av[1] E phase. */
static obj_t *iw_head, *iw_base;
static void iw_chain(void);
static int iw_group, iw_bg_latch, iw_j_latch, iw_inv_latch, iw_anim;   /* iw_anim: 0 idle, 7 open, 8 close, 10 blink */
static void iw_set_anim(obj_t *o, int k, int seg) {
    static const char *H[4][2] = { { "iw_r", "iw_l" }, { "iw_open_r", "iw_open_l" }, { "iw_close_r", "iw_close_l" }, { "iw_blink_r", "iw_blink_l" } };
    static const char *S[2][2] = { { "iws_r", "iws_l" }, { "iws_blink_r", "iws_blink_l" } };
    int j = iw_head && iw_head->av[4] < 0 ? 1 : 0;
    if (seg) { set_anim_ens(o, S[k == 10 ? 1 : 0][j]); return; }
    int r = k == 7 ? 1 : k == 8 ? 2 : k == 10 ? 3 : 0;
    int was = o->an.anim; set_anim_ens(o, H[r][j]);
    if (o->an.anim != was && (k == 7 || k == 8)) { o->an.frame = 0; o->an.done = 0; }
    iw_anim = k;
}
static void iw_face(void) {                                         /* set_direction 0 / 16 on head and segments (4207 / 4208) */
    int k = iw_anim;
    obj_t *h = iw_head; const char *cur = h->an.b ? h->an.b->anims[h->an.anim].name : "";
    int fr = h->an.frame, acc = h->an.acc;
    iw_set_anim(h, k, 0); h->an.frame = fr; h->an.acc = acc; (void)cur;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_IW_SEG) iw_set_anim(&G.o[i], k == 10 ? 10 : 0, 1);
}
static void iw_spawn(void) {
    iw_head = iw_base = NULL; iw_group = 0; iw_bg_latch = 0; iw_j_latch = 0; iw_inv_latch = 0; iw_anim = 0;
    int nseg = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_IW_HEAD) { iw_head = o; }
        if (o->type == OT_IW_BASE) { iw_base = o; set_anim_ens(o, "iwbase"); o->layer = 2; o->hidden = 1; }   /* invisible in effect (behind the floor) */
        if (o->type == OT_IW_SEG) nseg++;
    }
    if (!iw_head || !iw_base) return;
    obj_t *h = iw_head;                                              /* ctor + 4320 */
    h->A = 50; h->av[0] = 6; h->av[1] = -10; h->av[3] = 10; h->st = -3; h->tm = 0; h->av[4] = -1; h->av[7] = 0;
    h->av[2] = -20.0 * (nseg + 1); h->fl = F_NOFLY; h->layer = 1; h->bd = 0;   /* RV a1_worm: F0 = -140 (-20 (n + 1)), not the RE reading -120 */
    iw_set_anim(h, 0, 0);
    /* segments: D = 1 for the bottom one (INFERRED selection order, U2), E = -20 D */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_IW_SEG) continue;
        int k = 1; for (int j = 0; j < G.nobjs; j++) if (G.o[j].alive && G.o[j].type == OT_IW_SEG && G.o[j].y > o->y) k++;
        o->av[0] = k; o->av[1] = -20.0 * k; o->layer = 1; iw_set_anim(o, 0, 1);
    }
    for (int i = 0; i < 62; i++) iw_chain();                         /* RV a1_worm: the chain has run 62 frames when play starts (stage card) */
}
static void iw_chain(void) {                                        /* 4203 */
    obj_t *h = iw_head;
    if (!h || h->st >= 8) return;
    double Bx = iw_base->x, By = iw_base->y, E = h->av[1], Gs = h->av[3];
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_IW_SEG) continue;
        double k = o->av[0], th = 180 + Gs * c_sin(o->av[1]);
        o->x = (float)(int)(Bx + fmax(0.5, k - 1) * E * c_sin(th));
        o->y = (float)(int)(By + k * E * c_cos(th));
    }
    double th = 180 + Gs * c_sin(h->av[2]);
    h->x = (float)(int)(Bx + 6 * E * c_sin(th)); h->y = (float)(int)(By + 7 * E * c_cos(th) - 14);   /* RV a1_worm: the head is link n + 1 = 7 of the chain (x 6 E, y 7 E), not the RE reading 5 / 6 */
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && o->type == OT_IW_SEG) o->av[1] = fmod(o->av[1] + h->av[0], 360.); }
    h->av[2] = fmod(h->av[2] + h->av[0], 360.);
}
/* EnemyRock_776: vx / vy pinball speed (1/10 px), fx / fy sub-pixel, tm = A age, fl bit 0 broken, bit 1 sound done,
 * bit 2 / 3: background / solid overlap latches */
static void rock_new(float x, float y) {
    static const int DIRS[8] = { 3, 4, 5, 6, 10, 11, 12, 13 };
    int d = DIRS[R(8)]; d = DIRS[R(8)];                              /* pick_random twice (the second wins) */
    obj_t *r = spawn(OT_ROCK_AT, "rock", x, y, 2);
    if (!r) return;
    r->scale = (float)(1 - 0.05 * R(10)); r->ang = (float)R(360);
    double sp = 50 - 10 + R(20), a = d * 11.25 / 57.29577951308232;
    r->vx = (float)(sp * cos(a)); r->vy = (float)(-sp * sin(a)); r->fx = r->fy = 0; r->tm = 0;
}
void rock_at_new(float x, float y) { rock_new(x, y); }   /* the Relic Maze extractors throw the same object (spec_rm_enemies 4.5) */
static void rock_break(obj_t *r) { r->fl |= 1; set_anim_ens(r, "rock_break"); r->an.frame = 0; r->an.done = 0; r->scale = 1; r->vx = r->vy = 0; }
static void rocks_frame(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *r = &G.o[i];
        if (!r->alive || r->type != OT_ROCK_AT) continue;
        if ((r->fl & 1) && r->an.done) { r->alive = 0; continue; }                         /* 4232 */
        if (r->born == G.frame) continue;
        if (!(r->fl & 1)) {                                                                /* PinballMovement: gravity 30 */
            r->vy += 3.0f;
            float sp = sqrtf(r->vx * r->vx + r->vy * r->vy); if (sp > 100.f) { r->vx *= 100.f / sp; r->vy *= 100.f / sp; }
            float dx = r->vx / 10.f + r->fx, dy = r->vy / 10.f + r->fy, ix = floorf(dx), iy = floorf(dy);
            r->fx = dx - ix; r->fy = dy - iy; r->x += ix; r->y += iy;
            int bg = obj_overlaps_bg(r), so = 0;
            for (int k = 0; k < G.nobjs && !so; k++) { const obj_t *q = &G.o[k]; if (q->alive && q->an.b && obj_in_group(q->type, OBJGRP_SOLID) && fabsf(q->x - r->x) < 300 && fabsf(q->y - r->y) < 300 && obj_obj_hit(r, q)) so = 1; }
            if ((bg && !(r->fl & 4) && r->tm > 10) || (so && !(r->fl & 8) && r->tm > 10)) rock_break(r);   /* edge-triggered, A > 10 */
            r->fl = (r->fl & ~12) | (bg ? 4 : 0) | (so ? 8 : 0);
            if (!(r->fl & 1) && csens_hit(p, r) && !(r->fl & 16)) { if (vulnerable(p)) { player_contact(p, 5); rock_break(r); } }   /* col_4_16 */
            r->fl = (r->fl & ~16) | (csens_hit(p, r) ? 16 : 0);
            if (r->y > G.lvl.h + 64) r->alive = 0;                                           /* (no destroy rule: U5; port cleanup) */
        }
        r->tm++;                                                                            /* 4230 */
        if ((r->fl & 1) && !(r->fl & 2)) { r->fl |= 2; snd_play_orig(55, 220, 128); }      /* 4231 */
    }
}
void rock_at_break(obj_t *r) { rock_break(r); }
void rocks_at_frame(player_t *p) { if (act() != 1) rocks_frame(p); }   /* other levels (AT1 runs it with the worm) */
int aqua_worm_on_plate(const obj_t *pl) { return iw_head && iw_head->alive && iw_head->an.b && pl->an.b && obj_obj_hit(iw_head, pl); }   /* 4095 */
static void iw_frame(player_t *p) {
    obj_t *h = iw_head;
    if (!h || !h->alive) return;
    int H = h->st;
    /* test_collisions_16: head starts overlapping the background while H = 8 and falling -> H = 9, camera released */
    { int bg = iw_group && obj_overlaps_bg(h);
      if (bg && !iw_bg_latch && h->st == 8 && h->av[6] > 0) { h->st = 9; level_release_camera(1); }
      iw_bg_latch = bg; }
    if (h->st == -3 && PX(p) > 11704 + 100) h->st = -2;                                     /* 4200 */
    if (h->st == -2) { h->tm++; shake_min(3); }                                             /* 4201 */
    if (h->st == -2 && h->tm > 60) { h->tm = 0; h->st = -1; snd_play_orig(242, 230, 128); shake_min(5); }   /* 4202 */
    iw_chain();                                                                             /* 4203 */
    if (GT() % 3 == 0 && h->st == -1 && h->tm < 8) { h->tm++; rock_new(iw_base->x, iw_base->y - 24); }   /* 4204 */
    if (h->st == -1) h->av[1] = fmin(24, h->av[1] + 0.05 * (26 - h->av[1]));               /* 4205 */
    if (h->st == -1 && h->av[1] >= 24) {                                                    /* 4206: on the play layer, hittable */
        h->st = 0; iw_group = 1; h->layer = 2; h->av[1] = 24; h->tm = 0; h->type = OT_IW_HEADF;
    }
    (void)H;
    if (iw_group) {
        int j = (int)h->av[4];
        if (j != iw_j_latch) { iw_j_latch = j; iw_face(); }                                 /* 4207 / 4208 */
        int inv = h->c > 0;
        if (inv != iw_inv_latch) { iw_inv_latch = inv; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_IW_SEG) G.o[i].inv = inv; }   /* 4209 / 4210 */
        int dead = (h->fl & 0x40 /* flag 6 */) != 0;
        if (h->A < 25 && !h->av[7] && !dead) {                                              /* 4211 thrash */
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_IW_SEG) G.o[i].av[1] = -20 * G.o[i].av[0] - 30;
            h->av[3] = 50; h->st = 6; h->av[2] = -150; iw_set_anim(h, 0, 0); h->av[7] = 1;
        }
        if (h->st == 0 && level_cam_locked()) h->tm++;                                       /* 4212 */
        if (h->st == 0 && iw_anim != 8 && PX(p) + 20 < h->x) h->av[4] = -1;                 /* 4213 */
        if (h->st == 0 && iw_anim != 8 && h->x < PX(p) - 20) h->av[4] = 1;                  /* 4214 */
        if (h->st == 0 && h->tm > 100) { h->st = 1; h->tm = 0; }                            /* 4215 */
        if (h->st == 1 && h->tm < 150) { h->av[3] = fmin(h->av[3] + 1, 30); h->tm++; iw_set_anim(h, 7, 0); }   /* 4216 */
        if (GT() % 4 == 0 && h->st == 1 && h->tm > 60 && h->tm < 150) {                     /* 4217: acid from the mouth */
            float mx = h->x + (h->av[4] > 0 ? 11 : -11), my = h->y + 4;
            obj_t *a = acid_new(mx, my, 1, h->layer);
            if (a) { a->x += -8 + R(17); a->y += -8 + R(17); a->vx = (float)(3 * h->av[4]); }
        }
        if (h->st == 1 && h->tm >= 150) h->av[3] = fmax(10, h->av[3] - 1);                  /* 4218 */
        if (h->st == 1 && h->av[3] <= 10) { h->st = 0; iw_set_anim(h, 8, 0); h->tm = -60; } /* 4219 */
        if (h->st == 6 && h->av[3] > 10) h->av[3] = fmax(10, h->av[3] - 1);                 /* 4220 */
        if (h->st == 6 && h->av[3] <= 10) h->st = 0;                                         /* 4221 */
        if (dead && h->st < 7) {                                                             /* 4222 */
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_IW_SEG) G.o[i].av[1] = -20 * G.o[i].av[0] - 30;
            h->av[2] = -150; h->tm = 0; h->av[3] = 30; h->st = 7;
        }
        if (GT() % 7 == 0 && h->st == 7) { static const int S[3] = { 311, 319, 320 }; snd_play_orig(S[R(3)], 220, 128); }   /* 4223 */
        if (GT() % 4 == 0 && h->st == 7) { float ex = h->x - 20 + R(41), ey = h->y - 30 + R(41); obj_effect(ex, ey, "explosion06"); }   /* 4224 */
        if (h->st == 7 && h->tm < 140) {                                                     /* 4225 */
            h->tm++; h->av[0] = 16; iw_set_anim(h, 10, 0);
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_IW_SEG) { iw_set_anim(&G.o[i], 10, 1); G.o[i].inv = 0; }
        }
        if (h->st == 7 && h->tm >= 140) { h->av[3] = fmin(h->av[3] + 3, 90); h->av[0] = fmax(4, h->av[0] - 2); }   /* 4226 */
        if (h->st == 7 && h->av[0] < 12 && h->av[2] >= 60 && obj_overlaps_bg(h)) {         /* 4227 slam */
            h->st = 8; h->av[0] = 0; iw_set_anim(h, 0, 0);
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_IW_SEG) iw_set_anim(&G.o[i], 0, 1);
            h->av[5] = -3; h->av[6] = -5; snd_play_orig(17, 220, 128); shake_min(5);
        }
        if (h->st >= 8) {                                                                    /* 4228 */
            h->x = (float)(int)(h->x + h->av[5]); h->y = (float)(int)(h->y + h->av[6]);
            h->ang = (float)(h->ang - h->av[5]); h->av[6] = fmin(h->av[6] + 0.25, 12);
        }
        if (h->st == 9) { h->av[6] = 0; h->av[5] = fmin(0, h->av[5] + 0.09); }              /* 4229 */
    }
}

/* ================================================================ Chomper (AT2) */
/* CH: A HP, st D, av[0] E base y, av[1] F bob, av[2] G mouth, tm H, av[3] I phase timer, vx J, av[4] K, av[5] L, av[6] N,
 * av[7] flag 1. BALL: A 99, av[0] I, av[1] J, av[2] D, av[3] E, av[4] F, av[5] M, av[6] O; kx / ky = X / Y.
 * TAIL: av[0] A index, av[1] B, av[2] C. MOUTH: av[0] A, av[1] B. */
static obj_t *ch, *ch_ball, *ch_mouth;
static int ch_fight, ch_lure, ch_swallow, ch_defeat, ch_g_latch = -1, ch_trigger_latch, ch_fade, ch_next_done;
static int ch_mouth_frame(void) { return ch_mouth && ch_mouth->an.b ? ch_mouth->an.frame : 0; }
static void ch_mouth_anim(int open) {
    int alt = ch && (ch->dir & 1);
    set_anim_ens(ch_mouth, open ? (alt ? "chm_open1" : "chm_open") : (alt ? "chm_close1" : "chm_close"));
}
static void ch_spawn(void) {
    ch = ch_ball = ch_mouth = NULL; ch_fight = 0; ch_lure = 1; ch_swallow = 1; ch_defeat = 1; ch_g_latch = -1; ch_trigger_latch = 0; ch_fade = 0; ch_next_done = 0;
    int nt = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_CH_HEAD) ch = o;
        if (o->type == OT_CH_BALL) ch_ball = o;
        if (o->type == OT_CH_MOUTH) ch_mouth = o;
        if (o->type == OT_CH_TAIL) nt++;
    }
    if (!ch || !ch_ball || !ch_mouth) { ch = NULL; return; }
    if (G.at_g4 >= 10) {                                             /* after the Chomper: group 0x22136 off, nothing runs (parked below the view) */
        ch->hidden = ch_ball->hidden = ch_mouth->hidden = 1; ch_ball->type = OT_CH_TAIL;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_CH_TAIL) G.o[i].hidden = 1;
        ch = NULL; return;
    }
    set_anim_ens(ch, "ch"); set_anim_ens(ch_ball, "chb"); set_anim_ens(ch_mouth, "chm_close"); last_frame(ch_mouth);
    ch->A = 30; ch->av[3] = -60; ch->av[5] = -1; ch->st = 0; ch->tm = 0; ch->av[2] = 0; ch->av[1] = 0; ch->vx = 0; ch->av[7] = 0;
    ch->av[0] = ch->y; ch->av[4] = ch->x; ch->x += 500;                                    /* 4486 */
    ch_ball->A = 99; ch_ball->av[0] = 8; ch_ball->av[1] = 0; ch_ball->bd = 0; ch_ball->fl = 0;
    for (int i = 0, k = 0; i < G.nobjs; i++) {                       /* TAIL A = 0..12, last created first (x 6600 gets 0) */
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_CH_TAIL) continue;
        int idx = 0; for (int j = 0; j < G.nobjs; j++) if (G.o[j].alive && G.o[j].type == OT_CH_TAIL && G.o[j].x > o->x) idx++;
        o->av[0] = idx; set_anim_ens(o, "cht"); k++;
    }
    (void)nt;
}
static void ch_action(float *ax, float *ay) {                        /* action point (32, 4) = hotspot + (-32, -67), rotated (INFERRED) */
    double a = ch->ang / 57.29577951308232, c = cos(a), s = sin(a);
    *ax = (float)(ch->x + (-32) * c + (-67) * s); *ay = (float)(ch->y - (-32) * s + (-67) * c);
}
static void ch_bite_head(void) {                                     /* 4440 / 4442 shared actions on the head */
    ch->av[2] = 0; ch_mouth_anim(0); last_frame(ch_mouth); ch_lure = 0;
    ch->tm = 50; ch->A -= 10; ch->st = 3; snd_play_orig(47, 220, 128);
}
static void ch_frame(player_t *p) {
    if (!ch) return;
    /* 4462 acid destroy runs in acids_frame */
    int px = PX(p), py = PY(p);
    if (!ch_fight && !ch_trigger_latch && G.at_g4 < 10 && p326_box(p, 6376, 1000, 6392, 1192)) {   /* 4410 */
        ch_trigger_latch = 1; ch_fight = 1; music_play("bgm/m_midboss.adp");
    }
    ch_ball->A = 99;                                                                         /* 4411 */
    float cx0, cy0, cx1, cy1;
    if (ch_fight) {
        if (ch->av[5] == -1) ch->vx = fmaxf(ch->vx - 0.1f, -4);                              /* 4412 */
        if (ch->av[5] == 1) ch->vx = fminf(ch->vx + 0.1f, 4);                                /* 4413 */
        if ((ch->av[5] == -1 && ch->x < ch->av[4] - 120) || (ch->av[5] == 1 && ch->x > ch->av[4] + 120)) ch->av[5] = -ch->av[5];   /* 4414 */
        if (ch->st == 0) ch->av[3] += 1;                                                     /* 4415 */
        if ((ch->st == 0 && ch->av[2] == 0 && ch->av[3] > 120) || (ch->st == 0 && ch->av[2] == 1 && ch->av[3] > 180)) { ch->av[3] = 0; ch->av[2] = fmod(ch->av[2] + 1, 2); }   /* 4416 */
        if (GT() % 5 == 0 && ch->st == 0 && ch->A < 30 && ch->av[2] == 1 && ch->av[3] > 30 && ch->av[3] < 90) {   /* 4417 */
            obj_t *a = acid_new(ch->x - 42, ch->y + 3, 2, ch->layer);
            if (a) { a->x += R(17) - 8; a->y += R(17) - 8; a->vx = (float)(ch->vx + 0.1 * (-30 - R(30))); a->vy = -2; }
        }
        obj_t *b = ch_ball;
        if (ch_lure) {                                                                       /* 4421-4428 (the vertical spring is inert) */
            double I = b->av[0], J = b->av[1];
            if (I < -0.6) J = fmin(J + 0.2 + 0.01 * fabs(I), 12);
            if (I > 0.6) J = fmax(J - 0.2 - 0.01 * fabs(I), -12);
            if (I < 0 && J < 0) J *= 0.9;
            if (fabs(J) < 0.1) J = 0;
            I += J;
            if (b->kx != 0) { J += 4 * b->kx; b->kx = 0; }                                  /* 4427 */
            if (b->ky != 0) { J += 2 * b->ky; b->ky = 0; }                                  /* 4428 */
            b->av[0] = I; b->av[1] = J;
        }
        /* 4429 kinematics */
        ch->x = (float)(int)(ch->x + ch->vx);
        ch->y = (float)(int)(ch->av[0] + 8 * c_sin(ch->av[1]));
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *t = &G.o[i];
            if (!t->alive || t->type != OT_CH_TAIL) continue;
            double th = 12 * t->av[0];
            t->x = (float)(int)(0.5 * b->av[2] * c_cos(th) + b->av[3]); t->y = (float)(int)(b->y - 16 - 70 * c_sin(th));
        }
        ch->av[1] = fmod(ch->av[1] + 3, 360.); ch->ang = (float)(5 * c_cos(ch->av[1]));
        ch_mouth->x = ch->x - 6; ch_mouth->y = ch->y + 25; ch_mouth->ang = ch->ang;
        float ax, ay; ch_action(&ax, &ay);
        b->av[2] = b->x - ax; b->av[3] = (b->x + ax) * 0.5; b->av[4] = (b->y + ay) * 0.5;
        b->y = (float)(int)(ch->y + 0); b->x = (float)(int)(ch->x - 150 + b->av[0]);
        if (b->av[5] > 0) { b->av[1] = 24; b->av[5] -= 1; }                                  /* 4431 */
        int g = (int)ch->av[2];
        if (g != ch_g_latch) {                                                               /* 4432 / 4433 */
            if (g == 0) { ch_mouth_anim(0); ch->av[7] = 0; } else { ch_mouth_anim(1); ch_lure = 1; }
            ch_g_latch = g;
        }
        box_of(ch, &cx0, &cy0, &cx1, &cy1);
        if (ch_swallow) {                                                                    /* 4434-4437 */
            float mx0, my0, mx1, my1; box_of(ch_mouth, &mx0, &my0, &mx1, &my1);
            if (ch->st < 2 && p326_box(p, cx0, cy0, cx1, cy1) && p326_box(p, mx0, my0, mx1, my1) && ch->av[2] == 1 && !ch->av[7] &&
                ch_mouth_frame() >= 2 && ch->x > px && p->invuln <= 0 && p->state != ST_DEAD) {
                ch->tm = 50; ch->av[2] = 0; ch->st = 2; ch_mouth_anim(0); last_frame(ch_mouth); ch_g_latch = 0;
            }
            if (ch->st == 2) {
                ch->tm -= 1; p->state = ST_HELD; p->x = ch->x - 44; p->y = ch->y + ORIG_REF_DY; ch_mouth->layer = 3;
                if (ch->tm == 1) {                                                           /* 4436: spat out */
                    ch->av[2] = 1; ch->tm = 0; ch->st = 0; ch->av[3] = 150; ch_mouth_anim(1); last_frame(ch_mouth); ch_g_latch = 1;
                    player_contact(p, 10); p->tint = 100; ch_mouth->layer = 2;
                    p->state = ST_NORMAL; p->ground = 0; p->vx = -8; p->vy = -2;
                }
            }
            if (p->state == ST_HELD && ch->st != 2) { p->state = ST_NORMAL; }               /* 4437 */
        }
        /* 4440 lure bite / 4441 bounce / 4442 cube bite */
        int bov = b->an.b && obj_rect_hit(b, (int)cx0, (int)cy0, (int)cx1, (int)cy1) && b->x > cx0;
        if (bov && b->av[1] > 0 && ch->st != 2 && ch->st != 3 && ch->av[2] == 1 && ch_mouth_frame() >= 2) {
            ch_bite_head(); ch_g_latch = 0; b->av[0] = 100; b->av[1] = -12; b->av[5] = 0;
        }
        if (bov && b->av[1] > 0) { b->av[1] = -12; b->av[5] = 0; }
        { float qx, qy; int qa = player_cube_xy(&qx, &qy);
          if (qa == 2 && player_cube_rect((int)cx0, (int)cy0, (int)cx1, (int)cy1) && qx > cx0 && qx <= cx0 + 39 && ch->st != 2 && ch->st != 3 && ch->av[2] == 1 && ch_mouth_frame() >= 3) {
              ch_bite_head(); ch_g_latch = 0; player_cube_cancel(); } }
        if (ch->st == 3) { ch->tm -= 1; set_anim_ens(ch, (ch->dir & 1) ? "ch_daze1" : "ch_daze0"); }   /* 4444 */
        if (GT() % 2 == 0 && (ch->st == 3 || ch->st == 4)) {                                 /* 4445: dazed images toggle */
            ch->dir = (ch->dir + 1) % 2; set_anim_ens(ch, (ch->dir & 1) ? "ch_daze1" : "ch_daze0");
            int fr = ch_mouth->an.frame, acc = ch_mouth->an.acc; ch_mouth_anim(ch_mouth->an.anim == ens_anim("chm_open") || ch_mouth->an.anim == ens_anim("chm_open1"));
            ch_mouth->an.frame = fr; ch_mouth->an.acc = acc;
        }
        if (ch->st == 3 && ch->tm == 1) {                                                    /* 4446 */
            ch->tm = 0; set_anim_ens(ch, "ch"); ch->av[2] = 1; ch->dir = 0; ch_mouth_anim(1); last_frame(ch_mouth); ch_g_latch = 1;
            ch->av[3] = 150; ch->st = 0; ch->av[7] = 1;
        }
    }
    if (ch_defeat) {                                                                         /* 4447-4452 */
        if (ch->st == 3 && ch->A <= 0) { ch->tm = 0; ch->st = 4; ch_fade = 90; }             /* 4447: fadeout_music(1500), lock_music */
        if (ch->st == 4) { ch->vx = 0; ch->tm += 1; }                                        /* 4448 */
        if (ch->st == 4 && ch->tm > 120) {                                                   /* 4449 */
            ch->vx = 4; ch->st = 5; ch_mouth->av[0] = -4; ch_mouth->av[1] = 0; ch->av[6] = 0;
            for (int i = 0; i < G.nobjs; i++) { obj_t *t = &G.o[i]; if (t->alive && t->type == OT_CH_TAIL) { t->av[2] = -0.5 * t->av[0]; t->av[1] = 2 - 0.5 * (13 - t->av[0]); } }
            snd_play_orig(98, 220, 128); G.cam.Z = 8; ch_ball->fl |= 0x20 /* flag 5 */; ch_fight = 0; ch_swallow = 0;
        }
        if (ch->st == 5) {                                                                   /* 4450 */
            ch_mouth->ang += 6; ch->ang -= 3; ch->x += ch->vx; ch_mouth->x += (float)ch_mouth->av[0]; ch_mouth->y += (float)ch_mouth->av[1];
            ch->y += (float)ch->av[6]; ch->av[6] += 0.1; ch_mouth->av[1] += 0.1; ch_ball->av[6] += 1;
        }
        if (ch->st == 5 && ch_ball->alive && ch_ball->av[6] > 50) {                          /* 4451 */
            ch->hidden = 1; ch_mouth->hidden = 1; ch->st = 6;
            obj_effect(ch_ball->x, ch_ball->y, "shade_at"); obj_effect(ch_ball->x, ch_ball->y, "explosion01");
            ch_ball->alive = 0; snd_play_orig(50, 220, 128); level_release_camera(0);
        }
        if (ch->st == 6 && p->state != ST_DEAD && !ch_next_done) {                           /* 4452: Brick2 -> Brick, loop_next_act_17 */
            ch_next_done = 1; ch_defeat = 0;
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_ABRICK2) { G.o[i].type = OT_ABRICK; int a = ens_anim("abrick"); if (a >= 0) anim_set(&G.o[i].an, &G.ens, a); G.o[i].fl = 0; }
            G.at_g4 = 10; G.at_g5 = 6144; G.at_g6 = 1101;
            G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
            ch_fade = 0; music_volume(100); music_play("bgm/m_stage0b.adp");
            aqua_maker_start(); level_camera_bounds();
        }
    }
    if (ch_fade > 0) { ch_fade--; music_volume(ch_fade * 100 / 90); if (ch_fade == 0) music_stop(); }   /* 1.5 s fade (INFERRED linear) */
    if (ch->st >= 5)                                                                         /* 4456 / 4457 */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *t = &G.o[i]; if (!t->alive || t->type != OT_CH_TAIL) continue;
            t->x = (float)(int)(t->x + t->av[1]); t->y = (float)(int)(t->y + t->av[2]); t->av[2] += 0.2;
            if (t->y > G.camy + 340) t->alive = 0;
        }
    if (G.at_g4 == 10 && p->x > 7800) {                                                      /* 4463 */
        G.at_g4 = 11; G.at_g5 = 7648; G.at_g6 = 1124;
        G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
    }
    (void)py;
}

/* ================================================================ Chomper Snake (AT4) */
/* SNAKE: A HP, st D, av[0] E, av[1] F, av[2] G, tm H, av[3] I, av[4] K, av[5] M, av[6] N, av[7] O, fl 0x1000 flag 2.
 * SEG: av[0] A index, av[1] B amplitude, av[2] C angle, fl 0x1000 flag 2. BUBBLE: vx / vy = A / B, fx / fy = C / D, tm E,
 * st 1 = carries the bomb. Snake bombs: OT_MON_BOMB with av[6] = 1 falling (2606), av[5] fall speed. */
static obj_t *sn, *sn_mouth;
static int sn_fight, sn_g_latch = -1, sn_killed, sn_fade_a, sn_results;
int aqua_snake_kill_frame = -1;
static void sn_mouth_anim(int open) {
    int alt = sn && (sn->dir & 1);
    set_anim_ens(sn_mouth, open ? (alt ? "snm_open1" : "snm_open") : (alt ? "snm_close1" : "snm_close"));
}
static void sn_spawn(void) {
    sn = sn_mouth = NULL; sn_fight = 0; sn_g_latch = -1; sn_killed = 0; sn_fade_a = 0; sn_results = 0; aqua_snake_kill_frame = -1;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_SN_HEAD) sn = o;
        if (o->type == OT_SN_MOUTH) sn_mouth = o;
    }
    if (!sn || !sn_mouth) { sn = NULL; return; }
    set_anim_ens(sn, "sn"); set_anim_ens(sn_mouth, "snm_close"); last_frame(sn_mouth);
    sn->A = 40; sn->av[3] = -60; sn->av[5] = 20; sn->hidden = 1; sn->layer = 1; sn_mouth->layer = 1;
    sn->av[0] = sn->y; sn->av[4] = sn->x; sn->st = 2; sn->av[1] = 120; sn->av[5] = 100; sn->av[2] = 0; sn->tm = 0;   /* 4690 + inline */
    for (int i = 0; i < G.nobjs; i++) {                              /* SEG A = 1..5 newest first (INFERRED: json-last = 1) */
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_SN_SEG) continue;
        int k = 1; for (int j = 0; j < G.nobjs; j++) if (G.o[j].alive && G.o[j].type == OT_SN_SEG && G.o[j].x > o->x) k++;
        o->av[0] = k; o->av[2] = -40.0 * k + 120; o->av[1] = 100; o->hidden = 1; o->layer = 1; set_anim_ens(o, "sns");
    }
}
static void sn_set_dir(int d) {                                      /* 4639 / 4640: normal / pale images */
    sn->dir = d; set_anim_ens(sn, d ? "sn_pale" : "sn");
    int fr = sn_mouth->an.frame, acc = sn_mouth->an.acc;
    int open = sn_mouth->an.anim == ens_anim("snm_open") || sn_mouth->an.anim == ens_anim("snm_open1");
    sn_mouth_anim(open); sn_mouth->an.frame = fr; sn_mouth->an.acc = acc;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) set_anim_ens(&G.o[i], d ? "sns_pale" : "sns");
}
static int on_slime(const obj_t *o) {
    for (int i = 0; i < G.nobjs; i++) { const obj_t *s = &G.o[i]; if (s->alive && (s->type == OT_SLIMEFLOOR || s->type == OT_SLIMEF_VIS) && fabsf(s->x - o->x) < 200 && obj_obj_hit(o, s)) return 1; }
    return 0;
}
static int on_slime_col(const obj_t *o) {                           /* the layer-3 (collision) copies only */
    for (int i = 0; i < G.nobjs; i++) { const obj_t *s = &G.o[i]; if (s->alive && s->type == OT_SLIMEFLOOR && fabsf(s->x - o->x) < 200 && obj_obj_hit(o, s)) return 1; }
    return 0;
}
static int on_raft(const obj_t *o) {
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *r = &G.o[i]; if (!r->alive || r->type != OT_RAFT || !r->an.b) continue;
        float a0, b0, a1, b1; box_of(r, &a0, &b0, &a1, &b1);
        if (obj_rect_hit(o, (int)a0, (int)b0, (int)a1, (int)b1)) return 1;
    }
    return 0;
}
static obj_t *boss_raft(void) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_RAFT && G.o[i].y < 360) return &G.o[i]; return NULL; }
static void sn_bubble(int bomb) {
    obj_t *b = spawn(OT_BUBBLE, "bub", sn->x + 34, sn->y + 15, 2);
    if (!b) return;
    b->vx = (float)((bomb ? R(3) : R(4)) + 4); b->vy = 0; b->fx = b->x; b->fy = b->y; b->tm = 0; b->st = bomb;
    b->an.frame = 0;
}
static void sn_hit(int dmg, int H) {                                 /* 4635-4637 common */
    sn->tm = H; sn->A -= dmg; set_anim_ens(sn, "sn"); snd_play_orig(47, 220, 128);
}
void aqua_blast(float x, float y);
static void sn_frame(player_t *p) {
    if (!sn) return;
    obj_t *raft = boss_raft();
    /* 4600: the boss raft gets its 1784-px ride once Milla stands on it */
    if (raft && raft->st == 0 && raft->av[1] < 320) raft->av[1] = 1784;
    if (raft && !sn_fight && raft->st == 2) {                                                /* 4601 */
        sn_fight = 1; raft->st = 3; sn->hidden = 0;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) G.o[i].hidden = 0;
        music_play("bgm/m_boss2.adp");
    }
    if (sn_fight) {
        if (sn->st == 0 || sn->st == 1 || sn->st == 3) { sn->av[1] = fmod(sn->av[1] + 5, 360.); for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) G.o[i].av[2] = fmod(G.o[i].av[2] + 5, 360.); }   /* 4602 */
        if (sn->st == 2) { sn->av[1] = fmod(sn->av[1] + 2.5, 360.); for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) G.o[i].av[2] = fmod(G.o[i].av[2] + 2.5, 360.); }   /* 4603 */
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_RAFT && G.o[i].y >= 401) G.o[i].alive = 0;   /* 4604 */
        for (int i = 0; i < G.nobjs; i++) {                                                  /* 4653: arena floor scroll (after the movement) */
            obj_t *o = &G.o[i]; if (!o->alive) continue;
            if (o->type == OT_SCROLLBRICK) { o->x = (float)(int)(o->av[0] - o->av[1]); o->av[1] = fmod(o->av[1] + 4, 128); }
            if (o->type == OT_SLIMEFLOOR || o->type == OT_SLIMEF_VIS) o->av[2] = fmod(o->av[2] + 4, 128);   /* with 4554: 8 px / frame (INFERRED) */
        }
        if (!sn_results) {                                                                   /* group 0x2219c */
            double *F = &sn->av[1], *Gm = &sn->av[2], *I = &sn->av[3], *M = &sn->av[5], *N = &sn->av[6];
            if (sn->st == 0 && sn->x > sn->av[4]) sn->x -= 3;                                 /* 4606 */
            if (sn->st == 0 && sn->x <= sn->av[4]) *I += 1;                                  /* 4607 */
            if (sn->st == 0 && *I > 3 && *I < 160) *Gm = 1;                                  /* 4608 */
            if (sn->st == 0 && *I == 30 && sn->A > 0) sn_bubble(0);                          /* 4609 */
            if (sn->st == 0 && *I == 60 && sn->A > 0) sn_bubble(0);                          /* 4610 */
            if (sn->st == 0 && *I == 90 && sn->A > 0) sn_bubble(1);                          /* 4611 */
            if (sn->st == 0 && *I == 160) *Gm = 0;                                           /* 4612 */
            if (sn->st == 0 && *I > 260) { *I = 0; sn->st = 1; }                             /* 4613 */
            if (sn->st == 1 && *F > 330) { sn->fl &= ~0x1000; *Gm = 1; *N = 0; sn->st = 2; } /* 4614 */
            if (sn->st == 2) { *M = fmin(*M + 4, 100); for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) G.o[i].av[1] = fmin(G.o[i].av[1] + 4, 100); }   /* 4615 */
            if (sn->st == 2 && *M >= 100 && !(sn->fl & 0x1000)) { *N = 0; sn->fl |= 0x1000; } /* 4616 */
            if (sn->st == 2 && *F > 150 && *N == 0) snd_play_orig(252, 220, 128);            /* 4617 */
            if ((sn->st == 2 && *F > 150 && *N < 12) || (sn->st == 3 && *N < 12)) {          /* 4618 / 4619 acid burst */
                for (int s = -1; s <= 1; s += 2) {
                    obj_t *a = acid_new(sn->x + 24, sn->y, 3, 1);
                    if (a) { a->y = (float)(sn->av[0] + 32); a->x -= R(50); a->vx = (float)(s * (10 + R(20)) * 0.1); a->vy = (float)((-50 - R(20)) * 0.1); }
                }
                *N += 1;
            }
            for (int i = 0; i < G.nobjs; i++) { obj_t *a = &G.o[i]; if (a->alive && a->type == OT_ACID && a->par == 3 && a->layer == 1 && !on_slime(a)) a->layer = 2; }   /* 4620 */
            if (sn->st == 2 && (sn->fl & 0x1000) && *F > 150) sn->x += 5;                      /* 4621 */
            if (sn->st == 2 && *F <= 4 && (sn->fl & 0x1000)) {                                  /* 4622 */
                sn->fl &= ~0x1000; *M = 20; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) G.o[i].fl |= 0x1000;
                *Gm = 0; *N = 0; sn->st = 3; snd_play_orig(251, 220, 128);
            }
            if (sn->st == 3) sn->x += 1;                                                      /* 4623 */
            if (sn->st == 3 && *F > 150) sn->st = 0;                                          /* 4624 */
            for (int i = 0; i < G.nobjs; i++) { obj_t *s = &G.o[i]; if (s->alive && s->type == OT_SN_SEG && (s->fl & 0x1000) && s->av[2] <= 5) { s->fl &= ~0x1000; s->av[1] = 20; } }   /* 4625 */
            for (int i = 0; i < G.nobjs; i++) {                                               /* 4626 bubbles */
                obj_t *b = &G.o[i]; if (!b->alive || b->type != OT_BUBBLE) continue;
                b->fx += b->vx; b->vx = fmaxf(b->vx - 0.1f, 1); b->fy += b->vy; b->vy = fmaxf(b->vy - 0.05f, -8); b->tm += 1;
                b->x = (float)(int)b->fx; b->y = (float)(int)b->fy;
            }
            { int n = 0, idx[16]; for (int i = 0; i < G.nobjs && n < 16; i++) { obj_t *b = &G.o[i]; if (b->alive && b->type == OT_BUBBLE && !b->st && b->tm > 70) idx[n++] = i; }   /* 4628 */
              if (n) { obj_t *b = &G.o[idx[R(n)]]; b->alive = 0; aqua_blast(b->x - 16, b->y - 16); } }
            { int n = 0, idx[16]; for (int i = 0; i < G.nobjs && n < 16; i++) { obj_t *b = &G.o[i]; if (b->alive && b->type == OT_BUBBLE && b->st && b->tm > 80) idx[n++] = i; }    /* 4629 */
              if (n) { obj_t *b = &G.o[idx[R(n)]]; b->alive = 0;
                       obj_t *m = spawn(OT_MON_BOMB, "mon_bomb", b->x - 16, b->y - 16, 2); if (m) { m->av[0] = 1; m->av[1] = 0; m->fl = 1; m->tm = 0; } } }   /* BOMB.A = 1: falls (shared 2606) */
            { float qx, qy; int qa = player_cube_xy(&qx, &qy);                                 /* 4630 */
              if (qa == 2) { int pop = 0; for (int i = 0; i < G.nobjs; i++) { obj_t *b = &G.o[i]; if (b->alive && b->type == OT_BUBBLE && player_cube_hits(b)) { b->alive = 0; aqua_blast(b->x - 16, b->y - 16); pop = 1; } }
                             if (pop) player_cube_burst(); } }
            for (int i = 0; i < G.nobjs; i++) {                                               /* 4631-4634 bombs */
                obj_t *m = &G.o[i]; if (!m->alive || m->type != OT_MON_BOMB) continue;
                for (int k = 0; k < G.nobjs && !(m->fl & 0x100); k++) { obj_t *a = &G.o[k]; if (a->alive && a->type == OT_ACID && obj_obj_hit(a, m)) { m->fl |= 0x100; m->tm = 0; set_anim_ens(m, "mon_bomb_arm"); snd_play_orig(172, 220, 128); } }   /* 4631 */
                if (on_slime(m)) { m->x -= 8; m->layer = 1; }                                  /* 4632 */
                if (m->x < G.camx - 200) { m->alive = 0; continue; }                          /* 4633 */
                if (on_raft(m)) { m->av[0] = 0; m->av[1] = 0; }                               /* 4634 */
            }
            float x1, y1, x2, y2; box_of(sn, &x1, &y1, &x2, &y2);
            if (sn->tm <= 0) {                                                                 /* 4635: a thrown bomb (A = 2) in the head */
                for (int i = 0; i < G.nobjs; i++) { obj_t *m = &G.o[i];
                    if (m->alive && m->type == OT_MON_BOMB && m->av[0] == 2 && m->x > x1 && m->x < x2 - 16 && m->y > y1 && m->y < y2 - 16) { m->fl |= 0x200; sn_hit(10, 150); } }
            }
            { float qx, qy; int qa = player_cube_xy(&qx, &qy);                                 /* 4636: cube into the open mouth */
              if (sn->tm <= 0 && *Gm == 1 && sn_mouth->an.frame > 2 && qa == 2 && qx >= x2 - 39 && qx < x2 && qy < y2 && qy > y1 + 32) {
                  for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_MON_BOMB) G.o[i].fl |= 0x200;
                  *Gm = 0; sn_hit(5, 150); sn_mouth_anim(0); last_frame(sn_mouth); sn_g_latch = 0; player_cube_cancel();
              } }
            if (sn->tm > 1) sn->tm -= 1;                                                       /* 4638 */
            if (GT() % 2 == 0 && sn->tm > 1) sn_set_dir((sn->dir + 1) % 2);                    /* 4639 */
            if (sn->tm == 1) { sn->tm = 0; sn_set_dir(0); }                                    /* 4640 */
        }
        if (sn->A <= 0 && !sn_killed) { sn_killed = 1; music_stop(); aqua_snake_kill_frame = G.frame; }   /* 4641 */
        if (sn_killed && !sn_results) {
            double *O = &sn->av[7]; *O += 1; sn->av[3] = 0;                                    /* 4642 */
            if (*O == 1) ach_boss_finish();                                                    /* 4663-4671 (h20 inline): O == 1 (RE) */
            if (*O <= 40 || (*O > 40 && *O < 120 && GT() % 6 == 0)) {                          /* 4643 / 4644 */
                obj_effect(sn->x - 40 + R(81), sn->y - 40 + R(81), "explosion06");
                static const int S[3] = { 311, 98, 99 }; snd_play_orig(S[R(3)], 220, 128);
            }
            if (*O > 40 && *O < 160) { sn->av[0] += 1; sn->av[5] = fmax(sn->av[5] - 1, 5); for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) G.o[i].av[1] = fmax(G.o[i].av[1] - 1, 5); }   /* 4646 */
            if (*O == 160) {                                                                   /* 4647 */
                snd_play_orig(76, 230, 128); sn_fade_a = 255; sn->hidden = 1;
                for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SN_SEG) G.o[i].hidden = 1;
            }
            if (*O > 160 && sn_fade_a > 0) sn_fade_a = sn_fade_a == 255 ? 252 : sn_fade_a - 4; /* 4648 / 4649 */
            if (sn_fade_a < 0) sn_fade_a = 0;
            if (*O == 210) snd_play_orig(252, 220, 128);                                        /* 4650 */
            if (*O > 210 && *O < 230) { (void)R(4); (void)R(9); obj_effect(sn->x, sn->y, "shade_at"); }   /* 4651 */
            if (*O > 340 && p->state != ST_DEAD && p->ground) {                                /* 4652: results (not on the slime) */
                int slime = 0; for (int i = 0; i < G.nobjs && !slime; i++) { obj_t *s = &G.o[i]; if (s->alive && s->type == OT_SLIMEFLOOR) { int ox, oy, h; const signed char *a, *b; if (mv_sensor_rows(SENS_BOTTOMBIG, &ox, &oy, &a, &b, &h)) slime = obj_rows_hit(s, ox, oy, a, b, h); } }
                if (!slime) { sn_results = 1; p->state = ST_NORMAL; player_cube_cancel(); results_begin(2); }
            }
        }
    }
    /* 4655 positioning (always) */
    sn->y = (float)(int)(c_sin(sn->av[1]) * sn->av[5] + sn->av[0]);
    for (int i = 0; i < G.nobjs; i++) { obj_t *s = &G.o[i]; if (s->alive && s->type == OT_SN_SEG) s->y = (float)(int)(c_sin(s->av[2]) * s->av[1] + sn->av[0]); }
    sn->ang = (float)(c_sin(sn->av[1] - 45) * sn->av[5] * 0.25);
    { double a = sn->ang / 57.29577951308232, c = cos(a), s = sin(a), lx = -6, ly = 37;   /* action point (46, 89) - hotspot (52, 52), rotated (U2) */
      sn_mouth->x = (float)(sn->x + lx * c + ly * s); sn_mouth->y = (float)(sn->y - lx * s + ly * c); sn_mouth->ang = sn->ang; }
    for (int i = 0; i < G.nobjs; i++) { obj_t *s = &G.o[i]; if (s->alive && s->type == OT_SN_SEG) s->x = (float)(int)(sn->x - 56 * s->av[0]); }
    int g = (int)sn->av[2];
    if (g != sn_g_latch) { sn_mouth_anim(g == 1); sn_g_latch = g; }                       /* 4659 / 4660 */
}

/* ================================================================ hooks */
void aqua_boss_spawn(void) {
    iw_head = NULL; ch = NULL; sn = NULL;
    switch (act()) { case 1: iw_spawn(); break; case 2: ch_spawn(); break; case 4: sn_spawn(); break; }
}
void aqua_boss_update(player_t *p) {
    switch (act()) {
    case 1: iw_frame(p); rocks_frame(p); break;
    case 2: ch_frame(p); break;
    case 4: sn_frame(p); break;
    default: return;
    }
    acids_frame(p);
}
int aqua_boss_obj(const obj_t *o) { return o->type >= OT_IW_HEAD && o->type <= OT_IW_HEADF; }
void aqua_boss_draw_hud(void) {
    if (act() == 4 && sn_fade_a > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)sn_fade_a << 24) | 0xffffff);   /* 4647 white LevelCardFade */
}
