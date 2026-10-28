/* Battle Glacier: the Sparky (BossSparky_3336 + LeftArm_3337 / RightArm_3338 + four BossSparkyTail_3335 links, its
 * BossSparkyPlasmaShot_2587). Spec: build/re/spec_bg_bosses.md section 2.
 *  - BG4 (json 63): a miniboss in the CameraLimitBoss room x 8000-8575 (RE 14513-14549, group 0x22e8b; start events 14611 / 14612).
 *  - BG5 (json 64): Milla's ride (RE 14681-14716, 15139 / 15140; start events 14726 / 14727: destroyed unless MV.A == 4).
 * Sparky alterables: A HP (100, Q20), D / E its centre, F the bob phase, G the tail spacing (30; -30 upside down), H state (0 hover and
 * shoot, 1 dash, 2 spin, 6 exploding, 7 gone), I the home x, J the x speed (+-5), K timer, L dashes since the last spin, M explosions.
 * Flags: 1 upside down, 2 hold fire, 6 defeated (combat.c), 16 no fly-away. Tail link: A its index 1..4 (scale 1.1 - 0.1 A), B its
 * swing phase. Arms: A the recoil angle (-20 / 20 at a shot, back by 2 a frame). */
#include "bg_boss.h"
#include <stdio.h>
#include <stdlib.h>

#define FL_FLIP 0x2                                 /* flag 1: upside down (the tail above, pellets upwards) */
#define FL_HOLD 0x4                                 /* flag 2: no shots (set at spawn and after a spin) */
#define NTAIL 4
#define FIRE_DMG 10                                 /* INFERRED: the plasma shot's damage (the pellet's 10, col_4_20) */

static struct {
    int on, act, group, cam, done, trig;             /* act: 4 (BG4 boss) / 5 (BG5 ride); cam: 0 free, 1 locked (14513), 2 released (14514); trig: the room's CameraTriggerBoss */
    double D, E, F, G, H, I, J, K, L, M;
    double tA[NTAIL], tB[NTAIL];
    double aA[2];                                    /* the arms' A */
} S;
static obj_t *body(void) { return bgb_get(OT_BGB_SPARKY, 0); }
static obj_t *arm(int r) { return bgb_get(r ? OT_BGB_SP_RARM : OT_BGB_SP_LARM, 0); }
static obj_t *tail(int k) { return bgb_get(OT_BGB_SP_TAIL, k); }
int bg_boss_sparky_state(void) { return S.on && S.act == 4 ? (int)S.H : -1; }

static void kill_all(void) {
    obj_t *o;
    if ((o = body())) o->alive = 0;
    if ((o = arm(0))) o->alive = 0;
    if ((o = arm(1))) o->alive = 0;
    for (int k = 0; k < bgb_count(OT_BGB_SP_TAIL); k++) if ((o = tail(k))) o->alive = 0;
}
void bgs_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    obj_t *b = body();
    if (!b) return;
    S.act = level_cur()->act == 5 ? 5 : 4;                                   /* BG4: the boss room; BG5: Milla's ride */
    if (S.act == 4 && G.at_g4 >= 8) {                                        /* 14612: a restart past the fight: the room is open */
        kill_all();
        bgb_arena_open(-1e9f, 1e9f);
        return;
    }
    if (S.act == 5 && !bgb_milla()) { kill_all(); return; }                  /* 14726: MV.A != 4 */
    S.on = 1;
    b->A = 100; S.G = 30; S.J = 5; S.K = 60;                                 /* RE create_bosssparky_3336: A 100, G 30, J 5, K 60 */
    S.D = b->x; S.E = b->y; S.I = b->x; b->fl |= FL_HOLD | F_NOFLY;          /* 14611 / 14727 */
    bgb_set(b, "bgs_body");
    for (int r = 0; r < 2; r++) { obj_t *a = arm(r); if (a) bgb_set(a, r ? "bgs_rarm" : "bgs_larm"); }
    for (int k = 0; k < bgb_count(OT_BGB_SP_TAIL) && k < NTAIL; k++) {
        obj_t *t = tail(k); if (!t) continue;
        S.tA[k] = k + 1; t->scale = (float)(1.1 - 0.1 * S.tA[k]); S.tB[k] = 30 * S.tA[k];   /* 14611: A = index, scale 1.1 - 0.1 A, B 30 A */
        bgb_set(t, "bgs_tail");
    }
    S.trig = -1;                                                             /* the CameraTriggerBoss of the room (one scan at spawn) */
    float L = bgb_marker(OT_CAMLIM_L, 1), Rm = bgb_marker(OT_CAMLIM_R, 1);
    for (int i = 0; i < G.nobjs && S.trig < 0; i++) if (G.o[i].alive && G.o[i].type == OT_CAMTRIGGER && G.o[i].x > L - 64 && G.o[i].x < Rm + 64) S.trig = i;
}
#ifdef FP_HOST_ENV
static void dbg(const player_t *p) {
    static int on_ = -1; if (on_ < 0) on_ = getenv("FP_DBG_BGS") != NULL;
    obj_t *b = body();
    if (on_ && b) printf("bgs %d H %.0f K %.0f L %.0f D %.0f E %.0f G %.0f J %.0f A %.0f fl %x xy %.0f %.0f grp %d cam %d px %.0f py %.0f\n", G.frame, S.H, S.K, S.L, S.D, S.E, S.G, S.J, b->A, b->fl, b->x, b->y, S.group, S.cam, sbb_px(p), sbb_py(p));
}
#else
static void dbg(const player_t *p) { (void)p; }
#endif

/* ---- shots: a plasma shot (BallMovement speed 80, direction 24) or a pellet (direction 8, speed 80) at an arm's action point */
static void shoot(int r, double recoil, int spin) {
    obj_t *a = arm(r), *b = body();
    if (!a || !b) return;
    S.aA[r] = recoil;
    bgb_set(a, r ? "bgs_rarm7" : "bgs_larm7"); if (a->an.b) anim_restart(&a->an);
    float x, y; bgb_act_point(a, &x, &y);
    if (spin || S.G > 0) {                                                   /* 14517 / 14518 (G > 0), 14531 / 14532 (the spin) */
        obj_t *s = bgb_spawn(OT_BGB_PLASMA, "bgs_plasma", x, y, a->layer);
        if (s) { s->vx = 0; s->vy = 10.f; s->dir = 24; }                     /* direction 24, speed 80 / 8 px (INFERRED: BallMovement px = speed / 8) */
        snd_play_orig(spin ? 192 : 436, 220, 128);
        if (spin) { bgb_set(b, "bgs_body7"); for (int k = 0; k < bgb_count(OT_BGB_SP_TAIL); k++) bgb_set(tail(k), "bgs_tail7"); }
    } else {                                                                 /* 14519 / 14520: G <= 0: EnemyPellet_934, direction 8, speed 80 */
        obj_spawn_pellet(x, y, 8);
        snd_play_orig(28, 220, 128);
    }
}

/* ---- the rig (14546, every frame): body on its bob, arms on the body, tail links swinging below (or above) it */
static void rig(void) {
    obj_t *b = body();
    if (!b) return;
    S.F = sbb_fmod360(S.F + 4); if (S.F < 0) S.F += 360;
    b->x = (float)(int)(S.D + 20 * sbb_sin(S.F)); b->y = (float)(int)(S.E - 8 * sbb_sin(2 * S.F));
    for (int r = 0; r < 2; r++) {
        obj_t *a = arm(r); if (!a) continue;
        a->x = b->x; a->y = (float)(int)(S.E - 10 * sbb_sin(2 * S.F));
        a->ang = (float)(r ? 3 * S.G - 90 + S.aA[r] : -3 * S.G + 90 + S.aA[r]);
    }
    for (int k = 0; k < bgb_count(OT_BGB_SP_TAIL) && k < NTAIL; k++) {
        obj_t *t = tail(k); if (!t) continue;
        S.tB[k] = fmod(S.tB[k] + 356, 360);
        double sw = 30 * sbb_sin(S.tB[k]), A = S.tA[k];
        t->x = (float)(int)(sbb_sin(sw) * A * (30 - 2 * A) + floorf(b->x));
        t->y = (float)(int)((S.G - 0.08 * S.G) * sbb_cos(sw) * A + floorf(b->y));
    }
}

static void boss_frame(player_t *p) {
    obj_t *b = body();
    double X = sbb_px(p), Y = sbb_py(p);
    float L = bgb_marker(OT_CAMLIM_L, 1), Rm = bgb_marker(OT_CAMLIM_R, 1), T = bgb_marker(OT_CAMLIM_T, 0), B = bgb_marker(OT_CAMLIM_B, 0);
    /* 14513: the room lock (CAM.M 0 -> 1 inside L < X < R - 50, T < Y < B; the camera events turn M 1 into U 1: the markers' bounds);
     * 14514: R - 30 < X < R + 60 -> CAM.M 2, U 0 (released; M back to 0). RV s1_l: M 2 at f75 entering from the right, U 1 at f108 (X 8524) */
    if (X > L && X < Rm - 50 && Y > T && Y < B && !level_cam_locked()) { S.cam = 1; level_lock_markers(); }
    if (X > Rm - 30 && X < Rm + 60 && S.cam == 1) { S.cam = 2; level_release_camera(0); }
    /* 14515: the character sensor on CameraTriggerBoss (while a Sparky exists): the trigger off, the group on */
    if (!S.group && b && !S.done) {
        obj_t *o = bgb_slot(S.trig);                                                         /* its image: 16 x 192, hot spot (0, 0) */
        int x0 = (int)X - 7, x1 = (int)X + 9, y0 = (int)Y - (player_char_sensor_small(p) ? 13 : 34), y1 = (int)Y + 13;
        if (o && o->type == OT_CAMTRIGGER && !(o->fl & 1) && x1 > o->x && x0 < o->x + 16 && y1 > o->y && y0 < o->y + 192) { o->fl |= 1; S.group = 1; }
    }
    if (S.group && b) {
        int fl = b->fl;
        if (S.H == 0) S.K += 1;                                                              /* 14516 */
        if (S.H == 0 && !(fl & FL_HOLD) && fmod(S.K, 16) == 0) shoot(0, -20, 0);            /* 14517 / 14519 */
        if (S.H == 0 && !(fl & FL_HOLD) && fmod(S.K, 16) == 8) shoot(1, 20, 0);             /* 14518 / 14520 */
        if (S.H == 0 && S.K > 60 && S.D <= S.I - 180) S.J = 5;                              /* 14521 */
        if (S.H == 0 && S.K > 60 && S.D >= S.I + 180) S.J = -5;                             /* 14522 */
        if (S.H == 0 && S.K > 60 && S.L < 6) { b->fl &= ~FL_HOLD; S.K = 0; S.H = 1; S.L += 1; }   /* 14523 */
        if (S.H == 1) { S.K += 1; S.D = fmax(fmin(S.D + S.J, S.I + 180), S.I - 180); }       /* 14524 */
        if (S.H == 1 && S.K >= 36) { S.H = 0; S.K = 0; }                                   /* 14525 */
        if (S.L == 4) { b->fl ^= FL_FLIP; S.L += 1; }                                       /* 14526 */
        if (S.H == 0 && S.K > 60 && S.L >= 6) { b->fl &= ~FL_FLIP; S.K = 0; S.H = 2; }        /* 14527 */
        if (S.H == 2) {                                                                     /* 14528 */
            S.F = fmod(S.F + 4, 360); S.D += S.J; S.K += 1;
            for (int k = 0; k < NTAIL; k++) S.tB[k] = fmod(S.tB[k] + 356, 360);
        }
        if (S.H == 2 && S.D <= S.I - 180) S.J = 5;                                          /* 14529 */
        if (S.H == 2 && S.D >= S.I + 180) S.J = -5;                                         /* 14530 */
        if (S.H == 2 && S.K > 60 && fmod(S.K, 8) == 0) shoot(0, -10, 1);                    /* 14531 */
        if (S.H == 2 && S.K > 60 && fmod(S.K, 8) == 4) shoot(1, 10, 1);                     /* 14532 */
        if (S.H == 2 && S.K >= 220) {                                                       /* 14533 */
            b->fl |= FL_HOLD; S.H = 0; S.K = 0; S.L = 0; bgb_set(b, "bgs_body");
            for (int k = 0; k < bgb_count(OT_BGB_SP_TAIL); k++) bgb_set(tail(k), "bgs_tail");
        }
        if ((b->fl & BGB_DEF) && S.H < 6) S.H = 6;                                         /* 14534 */
        if (S.H == 6) {                                                                     /* 14535 */
            S.F = fmod(S.F + 4, 360); bgb_set(b, "bgs_body8");
            for (int k = 0; k < bgb_count(OT_BGB_SP_TAIL) && k < NTAIL; k++) { bgb_set(tail(k), "bgs_tail8"); S.tB[k] = fmod(S.tB[k] + 356, 360); }
        }
        if (S.H == 6 && bgb_g3() % 7 == 0) {                                                /* 14536 (INFERRED phase: the port's frame) */
            bgb_explode_snd();
            obj_effect(b->x - 30 + cross_rand(61), b->y - 30 + cross_rand(61), "explosion06");
            S.M += 1;
        }
        if (S.H == 6 && S.M > 15) {                                                         /* 14537: loop_sparky_62 x5 (debris), 38, shade + explosion01 */
            for (int k = 0; k < 5; k++) {
                obj_t *d = obj_alloc(); int a = ens_anim("debris");
                if (!d || a < 0) break;
                memset(d, 0, sizeof *d);
                double ang = cross_rand(360), sp = (50 + cross_rand(30)) / 8.0;      /* speed 50 + R(30) (RE), BallMovement px INFERRED */
                d->type = OT_DEBRIS; d->alive = 1; d->x = b->x; d->y = b->y; d->layer = 3; anim_set(&d->an, &G.ens, a); d->an.frame = k;
                d->vx = (float)(sp * sbb_cos(ang)); d->vy = (float)(-sp * sbb_sin(ang));
            }
            snd_play_orig(38, 220, 128);
            obj_effect(b->x, b->y, "shade_at"); obj_effect(b->x, b->y, "explosion");
            b->hidden = 1; for (int r = 0; r < 2; r++) if (arm(r)) arm(r)->hidden = 1;
            for (int k = 0; k < bgb_count(OT_BGB_SP_TAIL); k++) if (tail(k)) tail(k)->hidden = 1;
            S.H = 7; S.K = 0;
        }
        if (S.H == 7) S.K += 1;                                                             /* 14538 */
        if (S.H == 7 && S.K > 30) {                                                         /* 14539: everything gone, the room opens */
            kill_all();
            bgb_arena_open(-1e9f, 1e9f);
            S.group = 0; S.done = 1; b = NULL;
        }
    }
    if (!(b = body())) return;
    /* 14540 / 14541: upside down -> G down to -30 (E + 2 a frame), else back to 30 (E - 2) */
    if ((b->fl & FL_FLIP) && S.G > -30) { S.G = fmax(S.G - 2, -30); S.E += 2; }
    if (!(b->fl & FL_FLIP) && S.G < 30) { S.G = fmin(S.G + 2, 30); S.E -= 2; }
    for (int r = 0; r < 2; r++) { if (S.aA[r] < 0) S.aA[r] += 2; else if (S.aA[r] > 0) S.aA[r] -= 2; }   /* 14542-14545 */
    rig();                                                                                  /* 14546 */
}

/* the shots: 14547 the plasma shot turns (INFERRED -10 degrees a frame: the constant is a float Ghidra misreads); 14344 one on a
 * background obstacle is destroyed (explosion02); off the view: destroyed (INFERRED); the player: FIRE_DMG */
static void shots(player_t *p) {
    for (int k = 0; k < bgb_runtime_n(); k++) {
        obj_t *s = bgb_runtime(k);
        if (!s || s->type != OT_BGB_PLASMA) continue;
        s->ang = (float)fmod(s->ang - 10 + 360, 360);
        s->x += s->vx; s->y += s->vy;
        if (s->x < G.camx - 64 || s->x > G.camx + SCREEN_W + 64 || s->y < G.camy - 64 || s->y > G.camy + SCREEN_H + 64) { s->alive = 0; continue; }
        if (px_solid(&G.lvl, (int)s->x, (int)s->y)) { obj_effect(s->x, s->y, "explosion02"); s->alive = 0; continue; }
        if (!(s->fl & FL_HOLD) && sbb_csens_hit(p, s) && sbb_hurt(p, FIRE_DMG)) { obj_effect(s->x, s->y, "explosion02"); s->alive = 0; }
    }
}

void bgs_frame(player_t *p) {
    if (!S.on) { shots(p); return; }
    if (S.act == 4) { boss_frame(p); dbg(p); }
    shots(p);
}
int bgs_draw(const obj_t *o, int pass) {
    (void)pass;
    if (o->type == OT_BGB_SP_TAIL || o->type == OT_BGB_SPARKY || o->type == OT_BGB_SP_LARM || o->type == OT_BGB_SP_RARM || o->type == OT_BGB_PLASMA) {
        if (!o->hidden) sbb_draw(o, 0xffffffff);
        return 1;
    }
    return 0;
}
int bgs_input(uint32_t *held) { (void)held; return 0; }
int bgs_player_hidden(void) { return 0; }
