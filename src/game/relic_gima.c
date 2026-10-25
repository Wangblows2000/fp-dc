/* Relic Maze gimmicks, acts 1-2 (session 14). Source: build/re/spec_rm_gimmicks_a.md (static RE; its traces 1-14 are
 * not recorded yet; section numbers below refer to it). Every rule is attached to an object type (class + handle) or to
 * level data (levels.c: gates, clean-ups, the basement break-out zone, the wall plate rule); nothing tests the level.
 *   pre pass  (objects_platforms, before the movement): rolling platform control 5650-5658, hanging switch / mover
 *             5866-5876 and the hang hold for moving handles (4062)
 *   post pass (relic_frame, after the objects): paths 3283-3289 + carry 4061, wall plate / switch column, spin ball,
 *             relic panels, flip panels, fireballs / turrets, axle, clock, gates and the basement, moving floors
 * Labels: RE (read from code), INFERRED (reasoned). */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / 57.29577951308232); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / 57.29577951308232); }
static double MVY(const player_t *p) { return p->y - ORIG_REF_DY; }
static int PXi(const player_t *p) { return (int)floor(p->x); }
static int PYi(const player_t *p) { return (int)floor(p->y - ORIG_REF_DY); }
static void set_ens(obj_t *o, const char *name) { int a = ens_anim(name); if (a >= 0) { if (o->an.b != &G.ens || o->an.anim != a) anim_set(&o->an, &G.ens, a); } }
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
static int sens_hit(int s, const obj_t *o) {          /* a player sensor (mask rows) on an object (box objects: the image rect) */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    if (objdef(o->type)->flags & OD_BOXCOL) {
        const frame_t *f = anim_frame(&o->an);
        int bx = (int)floorf(o->x) - f->hx, by = (int)floorf(o->y) - f->hy;
        for (int j = 0; j < h; j++) { int y = oy + j; if (b[j] < a[j] || y < by || y >= by + f->h) continue; if (ox + b[j] >= bx && ox + a[j] < bx + f->w) return 1; }
        return 0;
    }
    return obj_rows_hit(o, ox, oy, a, b, h);
}
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {   /* PlayerCharacterSensor_375 */
    int X = PXi(p), Y = PYi(p);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return o->an.b && obj_rect_hit(o, a, b, c, d); }
static int near_view(const obj_t *o, int n) { return obj_near_view(o, n); }
static int loop_ch[2] = { -1, -1 };                   /* sound 62 loops: 0 machines (column, flip panels, axle), 1 moving floors */
static void loop62(int k, int on) {
    if (on) { if (loop_ch[k] >= 0) snd_loop_stop(loop_ch[k]); loop_ch[k] = snd_loop_orig(62, 200, 0); }
    else if (loop_ch[k] >= 0) { snd_loop_stop(loop_ch[k]); loop_ch[k] = -1; }
}

/* ================================================================ path platforms (3283-3289, start 3565)
 * av[0] A, av[1] E, av[3] B, av[4] C, av[5] F, av[6] G; c bit 0 = D (x by sin), bit 1 = H (y by sin); fx / fy = M / N;
 * av[2] / dir = the integer deltas O / P of the last step */
static void path_init(obj_t *o, double A, double B, double C, double E, double F, double Gm, int D, int H) {
    o->ox = o->x; o->oy = o->y;
    o->av[0] = A; o->av[1] = E; o->av[3] = B; o->av[4] = C; o->av[5] = F; o->av[6] = Gm; o->c = (D ? 1 : 0) | (H ? 2 : 0);
    o->fx = o->x; o->fy = o->y; o->av[2] = 0; o->dir = 0;
}
static int is_path(int t) { return t == OT_HBAR || t == OT_HSENS || t == OT_HBAR2 || t == OT_HSENS2 || t == OT_RPLAT; }
static void path_step(obj_t *o) {
    double K = o->fx, L = o->fy;
    o->av[0] = fmod(o->av[0] + o->av[3], 360.); o->av[1] = fmod(o->av[1] + o->av[5], 360.);
    double M = ((o->c & 1) ? c_sin(o->av[0]) : c_cos(o->av[0])) * o->av[4] + o->ox;
    double N = ((o->c & 2) ? c_sin(o->av[1]) : c_cos(o->av[1])) * o->av[6] + o->oy;
    o->av[2] = floor(M) - floor(K); o->dir = (int)(floor(N) - floor(L));
    o->fx = (float)M; o->fy = (float)N; o->x = (float)floor(M); o->y = (float)floor(N);
}
static void carry(player_t *p) {                      /* 4061: G >= 0, S_BB 1 px lower on a Q13 platform with flag 6 off */
    if (!(p->vy >= 0 || p->ground)) return;
    int k = mv_sensor_obj(p->x, MVY(p) + 1, p->N, SENSOR_BOTTOMBIG, OT_RPLAT);
    if (k < 0 || (G.o[k].fl & 0x40)) return;
    p->x += G.o[k].av[2]; p->y += G.o[k].dir;
    if (!p->ground) { p->gsp = p->vx; p->ground = 1; }
    p->vy = 0; p->N = 0; p->O = 0;
    player_sync_sensors(p);
}

/* ================================================================ hang handles (OD_HANG): the held handle carries the player */
void relic_hang_hold(player_t *p) {                   /* 4062 (moving handles) / 5867, 5872, 5873 (MV.Y follows the switch / mover) */
    if (p->state != ST_HANG) { p->hang_obj = -1; return; }   /* 5683 / 4041 / 4042: Q = 0 */
    if (p->hang_obj < 0 || p->hang_obj >= G.nobjs) return;
    const obj_t *h = &G.o[p->hang_obj];
    if (!h->alive) { p->hang_obj = -1; return; }
    p->hang_x += h->x - p->hang_px; p->hang_y += h->y - p->hang_py; p->hang_px = h->x; p->hang_py = h->y;
    p->x = p->hang_x; p->y = p->hang_y + 32 + ORIG_REF_DY;
}
static int held(const player_t *p, const obj_t *o) { return p->state == ST_HANG && p->hang_obj >= 0 && &G.o[p->hang_obj] == o; }   /* handle Q > 0 */

/* ================================================================ 7. rolling platform (5650-5658, 5713); st S, fl 0x200000 flag 21 */
static void rplat_control(player_t *p) {
    int any = 0;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_RPLAT && sens_hit(SENS_BOTTOMBIG, &G.o[i])) any = 1;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_RPLAT) continue;
        double A = o->av[0];
        if (!any) o->fl &= ~0x200000;                                                   /* 5650 */
        if (sens_hit(SENS_BOTTOMBIG, o) && p->ground && o->st == 0 && A > 90 && A < 270 && !(o->fl & 0x200000)) { o->fl |= 0x200000; o->st = 1; }   /* 5651 */
        if (!p->ground && o->st == 0 && (A > 270 || A < 90)) { o->fl |= 0x200000; o->st = -1; }   /* 5652 */
        int mid = A > 90 && A < 270, out = A > 270 || A < 90;
        if (o->st == 1 && mid) o->av[3] = fmin(o->av[3] + 0.05, 6);                     /* 5653 */
        if (o->st == 1 && out) o->av[3] = fmax(o->av[3] - 0.05, 0);                     /* 5654 */
        if (o->av[0] < 0) { o->av[0] += 360; o->av[1] += 360; }                          /* 5655 */
        if (o->st == -1 && out) o->av[3] = fmax(o->av[3] - 0.05, -6);                   /* 5656 */
        if (o->st == -1 && mid) o->av[3] = fmin(o->av[3] + 0.05, 0);                    /* 5657 */
        o->av[5] = o->av[3];                                                            /* B = F */
        if (o->av[3] == 0 && o->st != 0) o->st = 0;                                     /* 5658 */
    }
}

/* ================================================================ 5 / 6. hanging switches, movers (RM2 5866-5876) */
static void hangers(player_t *p) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_HISWITCH) {                                                    /* 5866-5868 (A = av[0] rest y) */
            if (!held(p, o)) { o->y = (float)fmax(o->av[0], o->y - 2); o->fl &= ~2; }
            else if (o->y < o->av[0] + 8) o->y = (float)fmin(o->av[0] + 8, o->y + 2);
            if (held(p, o) && o->y >= o->av[0] + 8 && !(o->fl & 2)) o->fl |= 2;         /* flag 1: pulled */
        } else if (o->type == OT_HIMOVER) {                                              /* 5870-5876: fl 2 = flag 1 (down), 4 = flag 2 (grab latch) */
            int q = held(p, o);
            if (!q && (o->fl & 4)) { o->fl &= ~4; G.input_lock = 0; }                    /* 5870 */
            if (q && !(o->fl & 4)) { o->fl ^= 2; G.input_lock = 1; o->fl |= 4; }        /* 5871 */
            if (q && !(o->fl & 2) && o->y > o->av[0]) { o->y = (float)fmax(o->av[0], o->y - 3); p->layer = 1; }   /* 5872 */
            else if (q && (o->fl & 2) && o->y < o->av[0] + 280) { o->y = (float)fmin(o->av[0] + 280, o->y + 3); p->layer = 1; }   /* 5873 */
            else if (q && G.input_lock) G.input_lock = 0;                                /* 5875 / 5876: reached the stop */
        }
    }
    for (int i = 0, si = 0, mi = 0; i < G.nobjs; i++) {                                  /* 5874: the visuals at their paired handles (index pairing) */
        obj_t *v = &G.o[i];
        if (!v->alive || (v->type != OT_HSWITCHV && v->type != OT_HMOVERV)) continue;
        int want = v->type == OT_HSWITCHV ? OT_HISWITCH : OT_HIMOVER, k = v->type == OT_HSWITCHV ? si++ : mi++;
        for (int j = 0, n = 0; j < G.nobjs; j++) if (G.o[j].alive && G.o[j].type == want && n++ == k) { v->x = G.o[j].x - 1; v->y = G.o[j].y; break; }
    }
}

void relic_gima_pre(player_t *p) {
    if (!objects_have(OT_RPLAT) && !objects_have(OT_HISWITCH) && !objects_have(OT_HIMOVER)) { relic_hang_hold(p); return; }
    rplat_control(p);
    hangers(p);
    relic_hang_hold(p);
}

/* ================================================================ 12. wall plate 1149 -> switch column / flip panels */
static int wp_latch, col_C_sound;
static void wallplates(player_t *p) {
    int pressed = 0, M = player_orig_m(p), cyc = level_cur()->wplate_cyclone;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_WPLATE_RM) continue;
        o->hidden = 0; o->fl &= ~1;                                                      /* 4505 / 4365: momentary */
        if (sens_hit(SENS_RIGHTBIG, o) && (M == 11 || M == 47 || (cyc && M == 26))) { o->hidden = 1; o->fl |= 1; }   /* 5677 / 5900 */
        if (o->fl & 1) pressed = 1;
    }
    int edge = pressed && !wp_latch; wp_latch = pressed;
    /* switch column (5678-5682): plate av[0] C, av[1] D, fl 2 = flag 1 (stood on); pieces av[0] = their rest y */
    obj_t *front = first_of(OT_COLFRONT);
    if (front) {
        static double C, D; static int stood;
        if (G.frame <= 1) { C = 0; D = 0; stood = 0; }
        if (edge) { snd_play_orig(49, 220, 128); stood = 0; D = 0; loop62(0, 1); col_C_sound = 1; }   /* 5678 */
        if (pressed && C < 160) C += 4;                                                  /* 5680 */
        if (sens_hit(SENS_BOTTOMBIG, front) && C > 0) stood = 1;                         /* 5681 */
        if (!pressed && stood && C > 0) { C = fmax(0, C - D); D = fmin(D + 0.01, 2); }   /* 5682 */
        if ((!pressed || C >= 160) && col_C_sound) { loop62(0, 0); col_C_sound = 0; }    /* 5679 */
        for (int i = 0; i < G.nobjs; i++) { obj_t *c = &G.o[i]; if (c->alive && (c->type == OT_COLFRONT || c->type == OT_COLBACK1 || c->type == OT_COLBACK2)) c->y = (float)(c->av[0] - C); }
    }
    if (edge && first_of(OT_FLIPPANEL)) {                                                /* 5904: every flip panel turns a quarter */
        for (int i = 0; i < G.nobjs; i++) { obj_t *f = &G.o[i]; if (f->alive && f->type == OT_FLIPPANEL) f->av[0] = -1; }
        snd_play_orig(49, 220, 128); loop62(0, 1);
    }
}
/* ================================================================ 2. flip panels (5905-5910): av[0] A, av[1] B, av[2] C rest angle */
static void flippanels(void) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_FLIPPANEL) continue;
        if (o->av[0] != 0 && o->av[0] != 99) { o->ang += (float)(0.1 * o->av[1]); o->av[1] += o->av[0]; }   /* 5905 */
        if (o->av[0] == -1) {                                                            /* 5906: past C - 90 */
            double tgt = o->av[2] - 90, a = o->ang;
            if (a <= tgt) { o->av[2] = fmod(o->av[2] - 90 + 360, 360); o->ang = (float)o->av[2]; o->av[0] = 0; o->av[1] = 0; snd_play_orig(51, 220, 128); loop62(0, 0); }
        }
    }
}

/* ================================================================ 9. pressure plates (switches.c rule 2) -> axle (5898 / 5899) */
void relic_axle_turn(void) {                          /* 5896 / 5897: axle A = fmod(A + 90, 360), flag 1 */
    for (int i = 0; i < G.nobjs; i++) { obj_t *a = &G.o[i]; if (a->alive && a->type == OT_AXLE) { a->av[0] = fmod(a->av[0] + 90, 360); a->fl |= 2; } }
    loop62(0, 1);
}
static int axle_turning(void) { obj_t *a = first_of(OT_AXLE); return a && fmod(a->ang + 360, 360) != a->av[0]; }
static void axles(void) {
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *a = &G.o[i]; if (!a->alive || a->type != OT_AXLE) continue;
        double ang = fmod(a->ang + 360, 360);
        if (ang != a->av[0]) a->ang = (float)fmod(ang + 5, 360);                         /* 5898: 90 deg in 18 frames */
        else if (a->fl & 2) { loop62(0, 0); snd_play_orig(51, 220, 128); a->fl &= ~2; } /* 5899 */
    }
}

/* ================================================================ 10. gates (level data: levels.c gates / flag clean-ups / break-out) */
static int gates_created;
static void gate_create(int rail_type, float x, float y) {
    obj_t *r = spawn(rail_type, rail_type == OT_RAIL2 ? "rail2" : "wr_rail", x + 8, y);
    obj_t *b = spawn(OT_WR_BARRIER, "wr_barrier", x, y);
    (void)r; (void)b;
    obj_effect(x, y + 24, "wburst"); snd_play_orig(51, 220, 128);
}
static void gates(player_t *p) {
    const level_def_t *L = level_cur();
    for (int k = 0; k < L->ngates; k++) {
        const gate_t *g = &L->gates[k];
        if (g->create) {                                                                  /* RM2 5922 / 5923: closed behind the player after the walk-in */
            if (gates_created || G.act_in || G.card_t < 192) continue;
            double Y = MVY(p);
            if (!(G.stage_flags & g->flag) && Y > g->ymin && Y <= g->ymax) gate_create(g->rail_type, g->x, g->y);
        } else if (G.stage_flags & g->flag) {                                            /* RM1 5832 / 5833, RM2 5926: the flag opens its gate */
            for (int i = 0; i < G.nobjs; i++) {
                obj_t *r = &G.o[i]; if (!r->alive || r->type != g->rail_type) continue;
                for (int j = 0; j < G.nobjs; j++) { obj_t *b = &G.o[j]; if (b->alive && b->type == OT_WR_BARRIER && fabsf(b->x - r->x) < 32 && fabsf(b->y - r->y) < 48 && obj_obj_hit(b, r)) b->alive = 0; }
                r->alive = 0;
            }
        }
    }
    if (!G.act_in && G.card_t >= 192) gates_created = 1;
    /* 5925: the basement break-out (zone, the axle turning, flag off) */
    const zone_t *z = L->breakout;
    if (z && !(G.stage_flags & L->breakout_flag) && axle_turning()) {
        int X = PXi(p), Y = PYi(p);
        if (X >= z->x0 && X <= z->x1 && Y >= z->y0 && Y <= z->y1) {
            for (int i = 0; i < G.nobjs; i++) {
                obj_t *o = &G.o[i]; if (!o->alive) continue;
                if (o->type == OT_RAIL2) { obj_launch_crystal(o->x, o->y + 12, 7); obj_launch_crystal(o->x, o->y + 12, 9); o->alive = 0; }
                else if (o->type == OT_WR_BARRIER || o->type == OT_BLIGHT) o->alive = 0;
            }
            snd_play_orig(40, 220, 128); G.stage_flags |= L->breakout_flag;
        }
    }
}
static void flag_cleanups(void) {                     /* RM2 6039 / 6040 (act start): with the flag already set, the area's contents go */
    const level_def_t *L = level_cur();
    for (int k = 0; k < L->nflagclears; k++) {
        const flagclear_t *c = &L->flagclears[k];
        if (!(G.stage_flags & c->flag)) continue;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || o->x < c->x0 || o->x > c->x1 || o->y < c->y0 || o->y > c->y1) continue;
            int t = o->type;
            if (t == OT_CRYSTAL || t == OT_PETAL || t == OT_WR_RAIL || t == OT_RAIL2 || t == OT_WR_BARRIER || t == OT_RPANEL || t == OT_POT || t == OT_BLIGHT ||
                t == OT_MONITOR || t == OT_MON_RINGS || t == OT_MON_1UP || t == OT_MON_INVINC || t == OT_MON_KEYSTONE || (t >= OT_MON_WOOD && t <= OT_MON_WATER) ||
                relic_boss_clear_type(t)) o->alive = 0;
        }
    }
}

/* ================================================================ 1. relic panel (5691-5701): st A, c B pieces, an frame (speed 0, manual) */
static void relic_panels(void) {
    int on_any = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_RPANEL) continue;
        if (sens_hit(SENS_BOTTOMBIG, o)) { on_any = 1; if (o->st == 0) o->st = 1; }   /* 5691 (MV.L not tested) */
    }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_RPANEL) continue;
        if (!on_any && o->st == 1) { o->st = 2; o->tm = 0; }                            /* 5692: force_speed(50) */
        if (o->st > 1) { o->st++; o->tm++; }                                            /* 5693 */
        int frame = o->st > 1 ? (o->tm / 2) % 5 : o->an.frame;                          /* speed 50: an image every 2 frames */
        int gems = o->dir;
        if (o->st > 30 && frame == 0 && gems == 3) {                                    /* 5694: 2 crystals */
            obj_launch_crystal(o->x - 16, o->y, 8); obj_launch_crystal(o->x + 16, o->y, 8); gems = gems > 0 ? gems - 1 : 0;
        }
        if (o->st > 30 && frame == 0 && gems == 1) obj_launch_crystal(o->x, o->y, 8);  /* 5695 */
        if (o->st > 30 && frame == 0 && gems != 0) { o->st = 0; gems--; frame = 0; }    /* 5696 */
        o->dir = gems;
        { char nm[12]; snprintf(nm, sizeof nm, "rpanel%d", gems); set_ens(o, nm); o->an.frame = frame; }
        if (gems == 0 && o->st > 1) {                                                   /* 5698: shatter (12 slices, sound 426) */
            for (int k = 0; k < 12; k++) {
                obj_t *pc = spawn(OT_RPIECE, "rpiece", o->x, o->y);
                if (pc) { pc->an.frame = k; pc->av[0] = o->y; pc->av[1] = 0; pc->av[2] = 0; pc->tm = 4 * (k + 1); }
            }
            snd_play_orig(426, 220, 128); o->alive = 0;
        }
    }
    for (int i = 0; i < G.nobjs; i++) {                                                  /* 5699-5701 */
        obj_t *pc = &G.o[i]; if (!pc->alive || pc->type != OT_RPIECE) continue;
        if (pc->tm > 0) pc->tm--;
        else { pc->y = (float)(pc->av[0] + pc->av[1]); pc->av[1] += pc->av[2]; pc->av[2] = fmin(pc->av[2] + 0.25, 15); }
        if (pc->y > G.camy + 280) pc->alive = 0;
    }
}

/* ================================================================ 8. spin ball (5684-5690): av[0] A, av[1] B, av[2] C, av[3] D, fl 1 = flag 0 */
static void spinball(player_t *p) {
    obj_t *o = first_of(OT_SPINBALL);                                                    /* only the first instance moves */
    if (!o) return;
    o->x = (float)(int)(o->av[0] + 100 * c_cos(o->av[3]));                              /* 5684 (E = 100) */
    o->y = (float)(int)(o->av[1] + 100 * c_sin(o->av[2]));                              /* 5685 */
    o->av[2] += 3;                                                                       /* 5686 */
    double k = fmax(0, 8 * c_cos(0.5 * o->av[3] + 45));
    if (o->av[3] < 270 && !(o->fl & 1)) { o->av[3] += 3; o->ang += (float)k; }          /* 5687 */
    if ((o->av[3] >= 270 && !(o->fl & 1)) || (o->av[3] <= -90 && (o->fl & 1))) o->fl ^= 1;   /* 5688 */
    if (o->av[3] > -90 && (o->fl & 1)) { o->av[3] -= 3; o->ang -= (float)fmax(0, 8 * c_cos(0.5 * o->av[3] + 45)); }   /* 5689 */
    if (csens_hit(p, o) && p->state != ST_HURT && p->state != ST_DEAD && !p->hurt_pending && p->shield != 3)   /* 5690: metal shield immune */
        player_contact(p, (int)(5 + fmax(0, 10 * c_cos(0.5 * o->av[3] + 45))));
}

/* ================================================================ 3. fireballs (5613-5616, 5702-5710): spawner st A, fl 2 = flag 1;
 * fireball av[4] speed (BallMovement), av[3] direction, av[5..6] fraction, fl 2 = flag 1 (in a cup) */
static void fb_move(obj_t *o) {
    if (o->av[4] <= 0) return;
    int d = (int)o->av[3] & 31; double v = o->av[4] * 0.125;
    double dx = (d == 0 ? 1 : d == 16 ? -1 : d == 8 || d == 24 ? 0 : fd_cos(d * 11.25 / 57.29577951308232)) * v + o->av[5];
    double dy = (d == 8 ? -1 : d == 24 ? 1 : d == 0 || d == 16 ? 0 : -fd_sin(d * 11.25 / 57.29577951308232)) * v + o->av[6];
    double sx = floor(dx), sy = floor(dy); o->av[5] = dx - sx; o->av[6] = dy - sy; o->x += (float)sx; o->y += (float)sy;
}
static void fb_burst(obj_t *f) { if (!anim_named(f, "fball_burst")) { f->av[4] = 0; restart_ens(f, "fball_burst"); } }
static void fireballs(player_t *p) {
    int run = 0;
    for (int i = 0; i < G.nobjs && !run; i++) { const obj_t *o = &G.o[i]; if (o->alive && ((o->type == OT_FBSPAWN && near_view(o, 150)) || (o->type == OT_FBBACK && near_view(o, 100)))) run = 1; }
    if (run) for (int i = 0; i < G.nobjs; i++) {                                         /* 5702 -> loop_fireball_launcher */
        obj_t *o = &G.o[i]; if (!o->alive) continue;
        if (o->type == OT_FBSPAWN) {
            if (near_view(o, 150) && !anim_named(o, "fbspawn_open")) o->st++;            /* 5613 */
            if (o->st > 10) { o->st = -90; restart_ens(o, "fbspawn_open"); o->fl |= 2; } /* 5615 */
            if (anim_named(o, "fbspawn_open") && o->an.done) set_ens(o, "fbspawn");
            if (anim_named(o, "fbspawn_open") && o->an.frame == 5 && (o->fl & 2)) {     /* 5616 */
                o->fl &= ~2;
                obj_t *f = spawn(OT_FBALL, "fball", o->x, o->y);
                if (f) { f->av[3] = 24; f->av[4] = 40; }
                snd_play_orig(46, 220, 128);
            }
        } else if (o->type == OT_FBBACK && o->dir != 0) {                                /* 5614 / 5857: a back on a Turret2 fires right */
            for (int j = 0; j < G.nobjs; j++) if (G.o[j].alive && G.o[j].type == OT_FBTURRET2 && obj_obj_hit(o, &G.o[j])) { o->dir = 0; break; }
        }
    }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *f = &G.o[i]; if (!f->alive || f->type != OT_FBALL) continue;
        if (f->born != G.frame) fb_move(f);
        if ((G.frame & 1) == 0) { obj_t *e = spawn(OT_EMBER, "ember", f->x, f->y); if (e) e->layer = 2; }   /* 5703 */
        int in_back = 0;
        for (int j = 0; j < G.nobjs; j++) {
            obj_t *b = &G.o[j]; if (!b->alive || b->type != OT_FBBACK || !obj_obj_hit(f, b)) continue;
            in_back = 1;
            if (f->y >= b->y && !(f->fl & 2)) { f->av[4] = 0; f->y = b->y; restart_ens(b, b->dir == 16 ? "fbback_load_l" : "fbback_load"); f->fl |= 2; }   /* 5705 */
            int loading = anim_named(b, "fbback_load") || anim_named(b, "fbback_load_l"), firing = anim_named(b, "fbback_fire") || anim_named(b, "fbback_fire_l");
            if (loading && b->an.done) { f->av[4] = 40; f->av[3] = b->dir; restart_ens(b, b->dir == 16 ? "fbback_fire_l" : "fbback_fire"); f->x = b->x; f->y = b->y; snd_play_orig(46, 220, 128); }   /* 5706 */
            else if (firing && !b->an.done) { f->av[4] = 40; f->av[3] = b->dir; }        /* 5707 */
        }
        if ((f->fl & 2) && !in_back) f->fl &= ~2;                                         /* 5709 */
        if (!in_back && f->av[4] > 0 && obj_overlaps_bg(f)) fb_burst(f);                  /* 5649 / col_33_24 */
        if (anim_named(f, "fball_burst") && f->an.done) { f->alive = 0; continue; }       /* 5710 */
        if (!anim_named(f, "fball_burst") && csens_hit(p, f)) {
            if (p->shield == 2) { snd_play_orig(101, 220, 128); fb_burst(f); }             /* col_23_24: the fire shield deflects */
            else if (p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p)) player_contact(p, 10);   /* col_18_24 */
        }
        if (!anim_named(f, "fball_burst") && player_cube_state() >= 4 && player_cube_hits(f)) { snd_play_orig(219, 220, 128); fb_burst(f); }   /* col_21_24 (RM1; RM2 U2) */
        if (f->y > G.lvl.h + 32) f->alive = 0;
    }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i]; if (!b->alive || b->type != OT_FBBACK) continue;
        if ((anim_named(b, "fbback_fire") || anim_named(b, "fbback_fire_l")) && b->an.done) restart_ens(b, "fbback_end");   /* 5708 */
    }
    for (int i = 0; i < G.nobjs; i++) { obj_t *e = &G.o[i]; if (e->alive && e->type == OT_EMBER && e->an.done) e->alive = 0; }
}

/* ================================================================ 5.2 moving floors (5998-6008): av[0] B base y, av[1] C, av[2] D */
static void floors(player_t *p) {
    (void)p;
    int moving = 0, land = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive || o->type < OT_MFLOOR1 || o->type > OT_MFLOOR3) continue;
        if (o->av[2] > 0) { o->av[2] -= 2; o->y -= 4; moving = 1; }                       /* 5999 / 6001 / 6003 */
        else if (o->av[2] < 0) { o->av[2] += 2; o->y += 4; moving = 1; }                  /* 6000 / 6002 / 6004 */
        if (o->type == OT_MFLOOR1 && o->av[2] != 0 && o->av[2] >= -2 && o->av[2] <= 2) land = 1;
    }
    if (land) { snd_play_orig(51, 220, 128); if (G.cam.Z < 2) G.cam.Z = 2; }              /* 6005 */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *s = &G.o[i]; if (!s->alive || s->type != OT_HISWITCH) continue;
        if ((s->fl & 2) && !moving && !(s->fl & 8)) {                                     /* 6006: a pull toggles every floor */
            for (int j = 0; j < G.nobjs; j++) { obj_t *o = &G.o[j]; if (o->alive && o->type >= OT_MFLOOR1 && o->type <= OT_MFLOOR3) { o->av[2] = o->av[1]; o->av[1] = -o->av[1]; } }
            snd_play_orig(62, 220, 128); loop62(1, 1); s->fl |= 8; moving = 1;
        }
        if (!(s->fl & 2) && !moving && (s->fl & 8)) s->fl &= ~8;                          /* 6007 */
    }
    if (!moving) loop62(1, 0);                                                           /* 6008 */
}

/* ================================================================ 9.3 / 9.4 basement light, clock */
static int blight_alpha = -1;
static void basement_light(player_t *p) {
    obj_t *o = first_of(OT_BLIGHT);
    blight_alpha = -1;
    if (!o) return;
    double Y = MVY(p);
    o->hidden = 1;
    if (Y > 1540) {                                                                      /* 5997 */
        o->x = (float)fmin(fmax(floor(p->x), G.camx), G.camx + SCREEN_W); o->y = (float)fmin(fmax(floor(Y), G.camy), G.camy + SCREEN_H);
        blight_alpha = (int)fmin(255, fmax(0, (Y - 1540) / 2));
    }
}
void relic_gima_draw_overlay(void) {                  /* the basement darkness (shader 20 UNKNOWN U4: a dark frame around a hole at the player, INFERRED) */
    if (blight_alpha <= 0) return;
    obj_t *o = first_of(OT_BLIGHT);
    if (!o || !o->an.b) return;
    const frame_t *f = anim_frame(&o->an);
    float s = 4.f, w = f->w * s, h = f->h * s, x = floorf(o->x - G.camx) - f->hx * s, y = floorf(o->y - G.camy) - f->hy * s;
    uint32_t a = (uint32_t)blight_alpha << 24;
    frame_draw_scaled(o->an.b, f, x, y, w, h, a | 0xffffff);
    if (y > 0) r_rect(0, 0, SCREEN_W, y, a);
    if (y + h < SCREEN_H) r_rect(0, y + h, SCREEN_W, SCREEN_H - (y + h), a);
    if (x > 0) r_rect(0, y, x, h, a);
    if (x + w < SCREEN_W) r_rect(x + w, y, SCREEN_W - (x + w), h, a);
}
static void clocks(void) {                            /* 5920: ML.Z = the stage time in ms */
    double Z = (double)G.frame * 1000.0 / FPS;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i]; if (!o->alive) continue;
        if (o->type == OT_CLOCK2) o->ang = (float)-fmod(Z / 222.222, 360);
        else if (o->type == OT_CLOCK1) o->ang = (float)-fmod(Z / 27.778, 360);
    }
}

int relic_boss_clear_type(int t) { (void)t; return 0; }   /* golem parts (6039): the boss module adds its types */

/* ================================================================ hooks */
void relic_gima_spawn(void) {
    memset(loop_ch, -1, sizeof loop_ch); wp_latch = 0; col_C_sound = 0; gates_created = 0; blight_alpha = -1;
    G.input_lock = 0; G.p.hang_obj = -1;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_RAIL2 || o->type > OT_LL1L02) continue;
        if (o->x < 0 || o->x >= G.lvl.w || o->y < 0 || o->y >= G.lvl.h) { o->alive = 0; continue; }   /* off-map templates */
        o->st = 0; o->tm = 0; o->fl = 0; o->c = 0; memset(o->av, 0, sizeof o->av);
        switch (o->type) {
        case OT_RPANEL: o->dir = 3; set_ens(o, "rpanel3"); break;                            /* ctor direction 3 (gems shown) */
        case OT_FBBACK: o->dir = 16; break;                                                  /* ctor direction 16 */
        case OT_HBAR: case OT_HSENS: path_init(o, 0, 1, 100, 0, 1, 150, 0, 1); o->fl |= 0x41; break;   /* 5844: flag 0 + flag 6 (never carries) */
        case OT_HBAR2: case OT_HSENS2: path_init(o, 0, 0, 0, 0, 2, 88, 0, 1); o->fl |= 0x41; break;    /* 6033 */
        case OT_RPLAT: path_init(o, 180, 0, 192, 180, 0, -192, 0, 1); o->fx = o->x; o->fy = o->y; break;   /* A 180, C 192, E 180, G -192, H 1 */
        case OT_SPINBALL: o->av[0] = o->x; o->av[1] = o->y; break;                          /* start: A = x, B = y */
        case OT_HISWITCH: case OT_HIMOVER: o->av[0] = o->y; break;                           /* 6028: A = y */
        case OT_COLFRONT: case OT_COLBACK1: case OT_COLBACK2: o->av[0] = o->y; break;        /* 5843 */
        case OT_MFLOOR1: case OT_MFLOOR2: case OT_MFLOOR3: {                                 /* 6043-6049: C by column */
            int x = (int)o->x; o->av[0] = o->y;
            o->av[1] = x >= 591 && x <= 644 ? 96 : x >= 641 && x <= 694 ? 64 : x >= 689 && x <= 742 ? 32 : x >= 786 && x <= 839 ? -32 : x >= 834 && x <= 887 ? -64 : x >= 882 && x <= 935 ? -96 : 0;
            break; }
        case OT_AXLE: o->ang = 0; break;
        case OT_BLIGHT: o->hidden = 1; o->layer = 5; break;
        case OT_LAYER_SW2: o->an.b = NULL; break;
        }
    }
    /* 5901-5903 / 5869: markers set the flip panels' start angles and the movers' start (bottom); then they go */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *m = &G.o[i];
        if (!m->alive || m->type < OT_FLIPMARK2 || m->type > OT_FLIPMARK4) continue;
        for (int j = 0; j < G.nobjs; j++) {
            obj_t *o = &G.o[j];
            if (!o->alive || fabsf(o->x - m->x) > 48 || fabsf(o->y - m->y) > 48) continue;
            if (o->type == OT_FLIPPANEL) { o->av[2] = m->type == OT_FLIPMARK2 ? 90 : m->type == OT_FLIPMARK3 ? 180 : 270; o->ang = (float)o->av[2]; }
            else if (o->type == OT_HIMOVER && m->type == OT_FLIPMARK2) { o->y = (float)(o->av[0] + 280); o->fl |= 2; }
        }
        m->alive = 0;
    }
    flag_cleanups();
}
void relic_gima_post(player_t *p) {
    if (!objects_have_from(OT_RAIL2) && !objects_have(OT_WR_BARRIER)) return;
    for (int i = 0; i < G.nobjs; i++) { obj_t *o = &G.o[i]; if (o->alive && is_path(o->type)) path_step(o); }   /* 3283-3289 */
    carry(p);                                                                            /* 4061 */
    for (int i = 0, k = 0; i < G.nobjs; i++) {                                           /* 5713: the wheel at its platform, rolling */
        obj_t *w = &G.o[i]; if (!w->alive || w->type != OT_RWHEEL) continue;
        for (int j = 0, n = 0; j < G.nobjs; j++) if (G.o[j].alive && G.o[j].type == OT_RPLAT && n++ == k) { w->x = G.o[j].x; w->y = G.o[j].y; w->ang -= (float)(4 * G.o[j].av[3]); break; }
        k++;
    }
    wallplates(p);                                                                       /* 4505, 5677-5683 / 4365, 5900, 5904 */
    spinball(p);                                                                         /* 5684-5690 */
    relic_panels();                                                                      /* 5691-5701 */
    flippanels();                                                                        /* 5905-5910 */
    fireballs(p);                                                                        /* 5702-5710 */
    axles();                                                                             /* 5898 / 5899 */
    clocks();                                                                            /* 5920 */
    gates(p);                                                                            /* 5832 / 5833, 5922-5926 */
    basement_light(p);                                                                   /* 5996 / 5997 */
    floors(p);                                                                           /* 5998-6008 */
}
int relic_gima_obj(obj_t *o, player_t *p) {          /* per-object: the hang grab (4064-4066) on the moving handles; the rest runs in the passes */
    if (objdef(o->type)->flags & OD_HANG) { obj_hang_try(o, p); return 1; }
    if (o->type == OT_RUINS) return 0;                                                   /* objects.c: the rock break rule (5669-5674) */
    return o->type >= OT_RAIL2 && o->type <= OT_LL1L02;
}
