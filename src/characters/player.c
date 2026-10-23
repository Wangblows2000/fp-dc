/* Lilac. Animations: recovered tables of playerskinsash_341 (both facings drawn
 * separately in the original). Rules marked [KB] come from the SDA knowledge
 * base (kb.speeddemosarchive.com/Freedom_Planet/Lilac, /Mechanics_and_Bugs).
 * Physics constants: fp.h (RV = runtime-verified against the original, see RUNTIME_MAP.md). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../fp.h"

enum { ST_NORMAL, ST_ATTACK, ST_CYCLONE, ST_CHARGE, ST_BOOST, ST_HURT, ST_DEAD,
       ST_UPPERCUT, ST_DKWIND, ST_DIVEKICK, ST_CROUCHATK, ST_HANG };

#define HALF_W   7.
#define BODY_H   30.
#define STEP_UP  12.
#define MAX_HP   14      /* RV: petal counter max 14 half-petals = 7 petals (GUIRings_125 Counter max) */

enum { A_IDLE, A_WALK, A_RUN, A_FASTRUN, A_JUMP, A_LOOKUP, A_CROUCH, A_SKID, A_ATTACK, A_AIRATK,
       A_UPPERCUT, A_DKWIND, A_DIVEKICK, A_CYCLONE, A_CHARGE, A_BOOST, A_HURT, A_DEFEAT, A_CROUCHATK, A_SPRING, A_HANG, A_COUNT };
/* names = tools/import_frame.py LILAC (runtime-verified playerskinsash_341 animation numbers) */
static const char *anim_names[A_COUNT] = { "idle", "walk", "run", "fastrun", "jump", "lookup", "crouch",
    "skid", "attack", "airatk", "uppercut", "dkwind", "divekick", "cyclone", "charge", "boost", "hurt", "defeat", "crouchatk", "spring", "hang" };
static int anim_r[A_COUNT], anim_l[A_COUNT];
static int cur_anim = -1;

static void set_anim(player_t *p, int a) {
    int idx = p->face < 0 ? anim_l[a] : anim_r[a];
    cur_anim = a;
    anim_set(&p->an, &G.lilac, idx);
}

void player_reset(player_t *p, double x, double y) {
    static int init;
    if (!init) {
        init = 1;
        char nm[20];
        for (int i = 0; i < A_COUNT; i++) {
            anim_r[i] = bank_anim(&G.lilac, anim_names[i]);
            snprintf(nm, sizeof nm, "%s_l", anim_names[i]);
            anim_l[i] = bank_anim(&G.lilac, nm);
            if (anim_r[i] < 0) { plat_log("lilac anim %s missing\n", anim_names[i]); anim_r[i] = 0; }
            if (anim_l[i] < 0) anim_l[i] = anim_r[i];
        }
    }
    int crystals = p->crystals, lives = p->lives;
    double energy = p->energy; int init_energy = p->energy_init;
    double sx = p->spawn_x, sy = p->spawn_y;
    *p = (player_t){0};
    p->crystals = init_energy ? crystals : CRYSTALS_PER_LIFE; p->lives = init_energy ? lives : LIVES_START;
    /* RV (GUIEnergy_127 at stage start, hud_dv1.png): the meter starts EMPTY on every stage/act start and
     * refills at 0.4/frame; the earlier "persists" reading came from a stale object */
    (void)energy; p->energy = 0.; p->energy_init = 1;
    p->spawn_x = sx ? sx : x; p->spawn_y = sy ? sy : y;
    p->x = x; p->y = y; p->face = 1; p->hp = MAX_HP;
    mv_reset();
    set_anim(p, A_IDLE);
}

int player_invincible(const player_t *p) {
    return p->invuln > 0 || p->state == ST_BOOST || p->state == ST_DEAD;
}

/* RE (Frames::event_func_2414, the damage event; collision handlers such as col_4_20 only set the
 * movement object's R = 1 and add the source's damage to GUILifeBaseMeter.B):
 *   blocked while PlayerOthers.E > 0 (60-frame invulnerability, counted down 1/frame) or on Casual;
 *   half-petals lost = mult * damage / 5, mult = 2 on Hard, 1 otherwise;
 *   B = 0.75 B - 2.2 Q, G = 0.75 G - 4.2 (Q = facing: the knock-back is AWAY FROM THE FACING, not from the
 *   source), state M8, sound 25 + a random hurt voice; PlayerOthers D = 2, E = 60. */
extern int fe_difficulty;
void player_damage(player_t *p, int amount) {
    if (player_invincible(p) || fe_difficulty >= 3) return;
    int mult = fe_difficulty == 0 ? 2 : 1;
    p->hp -= mult * amount / 5;
    snd_play_orig(25, 220, 128);    /* RE: Media::play_id 25 */
    snd_play_orig(17, 220, 128);    /* RV: id 17 logged on the same frame */
    { static const char *vh[4] = { "lilac_hurt0", "lilac_hurt1", "lilac_hurt2", "lilac_hurt3" }; voice_play(vh[rand() & 3]); }
    if (p->hp <= 0) { p->state = ST_DEAD; p->dead_t = 0; p->vy = -5; p->vx = 0; p->ground = 0; return; }
    double b = p->ground ? p->gsp : p->vx, g = p->ground ? 0. : p->vy;   /* ground: B = ground speed, G = 0 */
    p->state = ST_HURT; p->state_t = 0;
    p->vx = 0.75 * b - PHYS_HURT_VX * (float)p->face;
    p->vy = 0.75 * g - PHYS_HURT_VY;
    p->ground = 0; p->jumping = 0; p->hurt_land_t = 1000;
    p->cyclone_used = 0;            /* [KB] cyclone usable again after taking damage */
    p->invuln = PHYS_INVULN_T;      /* RE: PlayerOthers.E = 60 */
}

/* RE (event 4065): a falling Lilac (G > 0), not in state 9, Down not held, whose top-big sensor overlaps a
 * hang sensor grabs it: M = 41, X = sensor x, Y (reference point) = sensor y + 32; events 4062/4065 hold
 * that position every frame. Release (INFERRED, events 4057-4066 not fully read): Jump jumps off, Down drops. */
int player_can_hang(const player_t *p) {
    return p->state != ST_HANG && p->state != ST_HURT && p->state != ST_DEAD && !p->ground && p->vy > 0 && !(p->held & BTN_DOWN);
}
void player_hang(player_t *p, double hx, double hy) {
    p->state = ST_HANG; p->state_t = 0; p->hang_x = hx; p->hang_y = hy;
    p->x = hx; p->y = hy + 32 + ORIG_REF_DY; p->gsp = 0; p->ground = 0;   /* RV: B, G not reset on the grab */
    p->jumping = 0; p->cyclone_used = 0; p->boost_used = 0; p->springing = 0;
}

/* sources whose damage value has not been recovered yet use the Turretus pellet's 10 (2 half-petals) */
void player_hurt(player_t *p, double from_x) { (void)from_x; player_damage(p, 10); }

static double fric_step(double g);

/* the original's trig (cos taken as 0 at exactly 90/270, sin as 0 at exactly 180; N in degrees) */
#define DEG 57.29577951
static double o_cos(double n) { return (n == 270. || n == 90.) ? 0. : cos(n / DEG); }
static double o_sin(double n) { return n == 180. ? 0. : sin(n / DEG); }

/* RE (events 1644 / 1646): on the ground, with the RAW sensed angle N between 35 and 325 degrees,
 * B -= F sin(N) when |B| >= C (5), and B -= 0.38 F sin(N) when 0 < |B| < C (no force when stopped) */
static void slope_force(player_t *p) {
    double n = p->N;
    if (!(n > 35. && n < 325.)) return;
    if (fabs(p->gsp) >= PHYS_TOP) p->gsp = p->gsp - o_sin(n) * PHYS_SLOPE;
    else if (p->gsp != 0) p->gsp = p->gsp - PHYS_SLOPE * o_sin(n) * 0.38;
}

void player_bounce(player_t *p, double vy) { if (!p->ground && p->vy > 0) p->vy = vy; }

/* RV (spring_yellow_up / _upleft): on contact the feet snap to the spring top and the speed is set
 * (vertical: vy -10.75, vx kept; diagonal: (+-9, -8.75)); the original's spring state (M40, skin anim 14)
 * lasts until the apex, then the fall animation; normal gravity and air drag apply */
void player_spring(player_t *p, double vx, double vy, double top) {
    p->ground = 0; p->N = p->O = 0; p->y = top; p->vy = -vy; if (vx) { p->vx = vx; p->face = vx > 0 ? 1 : -1; }
    p->jumping = 0; p->cyclone_used = 0; p->boost_used = 0; p->springing = 1;
    if (p->state != ST_BOOST) p->state = ST_NORMAL;
    snd_play_orig(5, 230, 128);    /* RV: id 5 at spring contact (spring_yellow_up .snd) */
}

/* RV (spring_yellow_right): a wall spring sets the ground speed to +-10, no input lock */
void player_spring_side(player_t *p, double gsp) {
    /* the trigger frame already includes that frame's (input-free) friction step: 10 -> 9.63125 */
    if (p->ground) { p->gsp = fric_step(gsp); p->ctrl_lock = 4; p->skidding = 0; } else p->vx = gsp;
    snd_play_orig(5, 230, 128);
}

/* ---- movement core bridge (src/physics/sensors.c, RE events 1584-1611) --------------------------- */
static void air_speed_update(player_t *p, const input_t *in, int control, double grav);
static int is_m5(const player_t *p) { return p->state == ST_BOOST || p->state == ST_CHARGE; }   /* state M5 */

/* bookkeeping when the core lands Lilac (events 1591/1592 already set B, G, N) */
static void on_land(player_t *p) {
    p->springing = 0;
    if (p->state == ST_BOOST) {
        /* RV (boost_air_downright/downleft): a down-diagonal boost (8, 8) that lands carries on along the
         * ground at a constant 10 for the rest of the boost */
        p->gsp = (p->boost_dx > 0 ? 1. : -1.) * PHYS_BOOST_LAND; p->boost_dy = 0;
        p->boost_dx = p->boost_dx > 0 ? PHYS_BOOST_LAND / PHYS_BOOST : -PHYS_BOOST_LAND / PHYS_BOOST;
        p->jumping = 0; p->cyclone_used = 0;
        return;
    }
    p->jumping = 0; p->cyclone_used = 0; p->boost_used = 0;
    if (p->state == ST_HURT || p->state == ST_CYCLONE) p->state = ST_NORMAL;
}

/* one pass of the original's movement + sensor collision with the current B, G */
static int core(player_t *p) {
    mv_t m = { p->x, p->y - ORIG_REF_DY, p->ground ? p->gsp : p->vx, p->ground ? 0. : p->vy, p->N, p->O, 0, p->ground, 0 };
    int r = mv_core(&m);
    p->x = m.X; p->y = m.Y + ORIG_REF_DY; p->N = m.N; p->O = m.O; p->ground = m.L;
    if (m.L) { p->gsp = m.B; p->vy = 0; }
    else { p->vx = m.B; p->vy = m.G; }
    if (r & MV_LANDED) on_land(p);
    return r;
}

/* RE (events 1635/1636): moving into a wall the big side sensor touches stops B (not in state M5) */
static void wall_stop(player_t *p) {
    if (is_m5(p)) return;
    double *b = p->ground ? &p->gsp : &p->vx;
    if (*b > 0 && mv_sensor_right_big()) *b = 0;
    if (*b < 0 && mv_sensor_left_big()) *b = 0;
}

static void detach(player_t *p) {
    p->vx = p->gsp * o_cos(p->N); p->vy = -p->gsp * o_sin(p->N);
    p->ground = 0; p->N = 0;
}

/* RV: original order per logic frame = MOVE with the current speed, THEN update the speed
 * (dX(f) = B(f-1)*cos(O); see RUNTIME_MAP "Ground movement"). */
/* friction step toward 0 (RV: 5E, or 9E when the step would leave |gsp| below 2) */
static double fric_step(double g) {
    double s = fabs(g), f = PHYS_ACC * PHYS_FRIC_MUL;
    if (s - f < PHYS_LOWSPD) f = PHYS_ACC * PHYS_ACC_LOWMUL;
    return g - fmin(s, f) * (g > 0 ? 1 : -1);
}

/* RE (events 1621-1634): horizontal acceleration and friction, E = 0.07375, C = 5 (top speed):
 *   ground, no input toward +: B >= 0 -> B = max(0, B - 5E), then if |B| < 2 a further max(0, B - 4E)
 *   (mirror for -); input toward + with B < C: B += E, and if |B| < 2 (and N not within 80..280)
 *   B = min(2, B + 8E); in the air additionally B += E and, below 2, B = min(2, B + 4E).
 *   The input rules are skipped in states M2-M6 (look up, crouch, skid, boost) on the ground. */
static void horiz_accel(player_t *p, const input_t *in, int control, int mx) {
    double *b = p->ground ? &p->gsp : &p->vx, E = PHYS_ACC, C = PHYS_TOP;
    int L = p->ground;
    int r = control && (in->held & BTN_RIGHT), l = control && (in->held & BTN_LEFT);
    int steep = L && p->N > 80 && p->N < 280;          /* 1619/1620: movement flag 0x20 */
    if (L) {
        if (*b >= 0 && !r) *b = fmax(0., *b - 5 * E);
        if (*b <= 0 && !l) *b = fmin(0., 5 * E + *b);
        if (fabs(*b) < 2 && *b >= 0 && !r) *b = fmax(0., *b - 4 * E);
        if (fabs(*b) < 2 && *b <= 0 && !l) *b = fmin(0., 4 * E + *b);
    }
    if (r && *b < C && !mx) *b = *b + E;
    if (l && -C < *b && !mx) *b = *b - E;
    if (r && *b < C && !mx && fabs(*b) < 2 && !steep) *b = fmin(2., 8 * E + *b);
    if (l && -C < *b && !mx && fabs(*b) < 2 && !steep) *b = fmax(-2., *b - 8 * E);
    if (!L) {
        if (r && *b < C) *b = *b + E;
        if (r && *b < 2) *b = fmin(2., 4 * E + *b);
        if (l && -C < *b) *b = *b - E;
        if (l && -2 < *b) *b = fmax(-2., *b - 4 * E);
    }
}

static void ground_speed_update(player_t *p, const input_t *in, int control) {
    int dir = 0;
    if (p->ctrl_lock > 0) { p->ctrl_lock--; control = 0; }   /* RV: side spring locks input 4 frames */
    if (control && (in->held & BTN_RIGHT)) dir = 1;
    else if (control && (in->held & BTN_LEFT)) dir = -1;
    if (dir && p->gsp * dir < 0 && p->skidding) {
        /* RV (skid): once the input has turned Lilac round, the skid state slows her by 0.5 per frame */
        p->face = dir;
        p->gsp += dir * PHYS_DEC;
        if (p->gsp * dir > 0) p->gsp = 0;
    } else {
        /* RV: a skid only starts when the input turns Lilac round (the next frame) */
        int turning = dir && p->gsp * dir < 0 && p->face != dir;
        if (!(dir && p->gsp * dir < 0)) p->skidding = 0;
        if (dir) p->face = dir;
        horiz_accel(p, in, control, 0);
        if (turning) p->skidding = 1;
    }
    if (p->gsp == 0) p->skidding = 0;
    wall_stop(p);                                  /* RE 1635/1636, before the slope force */
    slope_force(p);
    if (p->gsp > PHYS_MAXSPD) p->gsp = PHYS_MAXSPD;
    if (p->gsp < -PHYS_MAXSPD) p->gsp = -PHYS_MAXSPD;
}

/* upd: 1 = normal speed update, 0 = speed frozen (boost charge / boost: RV, B constant) */
static void ground_move(player_t *p, const input_t *in, int control, int upd) {
    core(p);
    if (!upd) return;
    if (p->ground) ground_speed_update(p, in, control);
    else air_speed_update(p, in, control, PHYS_GRAV);   /* left the ground: air rules apply this frame */
}

/* RV order: air accel -> gravity -> Sonic-style air drag tested on the NEW vy
 * (uppercut trace: vx 2 -> 1.9375 on the frame vy goes -4 -> -3.75; stops once vy >= 0) */
static void air_speed_update(player_t *p, const input_t *in, int control, double grav) {
    if (control) {
        if (in->held & BTN_RIGHT) p->face = 1;
        else if (in->held & BTN_LEFT) p->face = -1;
        horiz_accel(p, in, control, 0);
    }
    if (grav > 0) {
        p->vy += grav;
        if (p->vy > -PHYS_DRAG_VY && p->vy < 0 && fabs(p->vx) >= 0.125) p->vx *= PHYS_AIR_DRAG;   /* RE 1648: only |B| >= 0.125 */
    }
}

/* air frame: move + sensors (core), then speeds in the original's event order: air acceleration,
 * wall stop (1635/1636), gravity (1647), drag (1648), ceiling (1649: rising into the top-big sensor) */
static void air_move(player_t *p, const input_t *in, int control, double grav) {
    core(p);
    if (p->ground) { if (p->state != ST_BOOST) ground_speed_update(p, in, control); return; }
    if (control) {
        if (in->held & BTN_RIGHT) p->face = 1;
        else if (in->held & BTN_LEFT) p->face = -1;
        horiz_accel(p, in, control, 0);
    }
    wall_stop(p);
    air_speed_update(p, in, 0, grav);
    if (p->vy < 0 && mv_sensor_top_big() && !is_m5(p)) p->vy = 0;
}

/* RV: on the jump frame Lilac first moves along the ground with her current speed, then the
 * velocity becomes B along + 7 perpendicular to the RAW surface angle N (run_jump: N 355.914,
 * B 5.013 -> vx 5.499, vy -6.625; using the smoothed O gives 5.396 / -6.71). No gravity that frame. */
static void jump(player_t *p) {
    /* RE (event 3047): G = cos(N) A - B sin(N), B = cos(N) B + A sin(N), A = jump strength -7; N = 0 */
    double n = p->N, c = o_cos(n), sn = o_sin(n), b = p->gsp;
    p->vy = c * -PHYS_JUMP - b * sn;
    p->vx = c * b + -PHYS_JUMP * sn;
    p->ground = 0; p->jumping = 1; p->N = 0;
    snd_play(G.sfx.jump, 200, 128);
}

static void start_boost(player_t *p, const input_t *in) {
    /* [KB] six directions: left/right and the four diagonals (no straight up/down) */
    double dx = (in->held & BTN_LEFT) ? -1. : (in->held & BTN_RIGHT) ? 1. : (float)p->face;
    double dy = (in->held & BTN_UP) ? -1. : (in->held & BTN_DOWN) ? 1. : 0.;
    if (p->ground && dy > 0) dy = 0;                  /* down-diagonal on the ground = along the ground */
    /* RV (boost_* traces): straight = 12, diagonals = (+-8, +-8) (not 12/sqrt2); straight up/down input
     * gives the diagonal on the facing side, which the dx default above already does */
    if (dy != 0) { p->boost_dx = dx * PHYS_BOOST_DIAG / PHYS_BOOST; p->boost_dy = dy * PHYS_BOOST_DIAG / PHYS_BOOST; }
    else { p->boost_dx = dx; p->boost_dy = 0; }
    p->face = dx > 0 ? 1 : -1;
    p->state = ST_BOOST; p->state_t = 0; p->boost_used = 1;

    if (dy < 0 && p->ground) detach(p);
    /* RV (boost_* traces): B and G already hold the boost speed on the fire frame (before any move) */
    if (p->ground) p->gsp = p->boost_dx * PHYS_BOOST;
    else { p->vx = p->boost_dx * PHYS_BOOST; p->vy = p->boost_dy * PHYS_BOOST; }
    snd_play_orig(53, 230, 128);                  /* RV: boost fire = 53 */
}

static void set_state(player_t *p, int s) { p->state = s; p->state_t = 0; }

/* RV (cyclone, cyc_down, dk_kick_then_cyc*): aerial cyclone sets vy = -3, or 0 when Down is held;
 * one per airtime (cyc_dk_cyc refused a second one); vx is kept (kick: 5) */
static void start_air_cyclone(player_t *p, const input_t *in) {
    set_state(p, ST_CYCLONE); p->cyclone_used = 1; p->jumping = 0;
    p->vy = (in->held & BTN_DOWN) ? 0. : -PHYS_CYCLONE_VY;
    snd_play_orig(288, 220, 128); snd_play_orig(174, 220, 128);       /* RV: cyclone start = 288 + 174 */
    voice_play(rand() & 1 ? "lilac_cyclone1" : "lilac_cyclone2");     /* voice choice/frequency INFERRED */
}

/* grounded ST_NORMAL: action inputs (RV unless noted) */
static int ground_actions(player_t *p, const input_t *in) {
    if ((in->pressed & BTN_ATTACK) && (in->held & BTN_DOWN) && fabs(p->gsp) >= 0.01) {
        /* RV (gc_run_down_attack): Down + attack while MOVING on the ground = grounded Dragon Cyclone
         * (M26, anim 18). From a standstill the same input is the crouch kick (anim 30). */
        set_state(p, ST_CYCLONE); p->cyclone_ground_start = 1;
        snd_play_orig(288, 220, 128); snd_play_orig(174, 220, 128);
        return 1;
    }
    if ((in->pressed & BTN_ATTACK) && fabs(p->gsp) < 0.01 && (in->held & BTN_DOWN)) {
        set_state(p, ST_CROUCHATK); snd_play(G.sfx.attack, 200, 128); return 1;   /* RV: anim 30, M stays 3 */
    }
    if (in->pressed & BTN_ATTACK) {
        if (in->held & BTN_UP) {                                             /* RV: M21 anim 28 */
            set_state(p, ST_UPPERCUT); p->gsp = 0;
            if (in->pressed & BTN_JUMP) { jump(p); p->jumping = 0; }        /* RV: high uppercut */
        }
        else set_state(p, ST_ATTACK);                                        /* RV: M20 anim 26 */
        return 1;                                                            /* sound: see ST_ATTACK */
    }
    if ((in->pressed & BTN_SPECIAL) || ((in->held & BTN_SPECIAL) && p->energy >= PHYS_ENERGY_MAX)) {
        /* RV: charging needs a FULL energy meter; holding special waits for it (boost_hold_long) */
        if (p->energy >= PHYS_ENERGY_MAX) { set_state(p, ST_CHARGE); snd_play_orig(9, 230, 128); return 1; }   /* RV: 9 */
    }
    if (in->pressed & BTN_JUMP) {
        /* RE order: move + sensors (1584-1611), speed rules (1621-1646), then the jump (3047) */
        ground_move(p, in, 1, 1);
        if (p->ground) jump(p);
        return 1;
    }
    return 0;
}

/* RV: on the frame an air action starts, the original still moves with the old state's physics
 * (gravity included) and only then switches state / sets the new speed (down_kick: G -3.25 -> -3.0 on
 * the press frame, then +0.125; cyclone: moves with the old vy, then vy = -3). So: decide, move, apply. */
enum { ACT_NONE, ACT_DKWIND, ACT_AIRATK, ACT_CHARGE, ACT_CYCLONE };
static int air_action_wanted(const player_t *p, const input_t *in) {
    if (in->pressed & BTN_ATTACK) return (in->held & BTN_DOWN) ? ACT_DKWIND : ACT_AIRATK;
    if ((in->held & BTN_SPECIAL) && p->energy >= PHYS_ENERGY_MAX) return ACT_CHARGE;
    if ((in->pressed & BTN_JUMP) && !p->cyclone_used && !p->boost_used) return ACT_CYCLONE;
    return ACT_NONE;
}
static void air_action_apply(player_t *p, const input_t *in, int act) {
    if (p->ground) return;                 /* landed this frame: the action is dropped */
    switch (act) {
    case ACT_DKWIND: set_state(p, ST_DKWIND); snd_play_orig(405, 220, 128); break;   /* RV: M19 anim 7, sound 405 */
    case ACT_AIRATK: set_state(p, ST_ATTACK); break;                                  /* RV: M20 anim 25 */
    case ACT_CHARGE: set_state(p, ST_CHARGE); snd_play_orig(9, 230, 128); break;
    case ACT_CYCLONE: start_air_cyclone(p, in); break;
    }
}

/* RV: the original keeps Lilac inside X >= 16 (and <= width-16) once she moves (speed -> 0) */
#define EDGE_X 16.
static void clamp_edges(player_t *p, double x0) {
    /* only stops crossing the line; standing beyond it (spawn at X 0) is left alone, as in the original */
    if (x0 >= EDGE_X && p->x < EDGE_X) { p->x = EDGE_X; if (p->ground) p->gsp = 0; else p->vx = 0; }
    double r = G.lvl.w - EDGE_X;
    if (x0 <= r && p->x > r && !G.clear_t) {   /* RV: during the act-end run-out she leaves the frame */ p->x = r; if (p->ground) p->gsp = 0; else p->vx = 0; }
}

void player_update(player_t *p, const input_t *in) {
    double x_start = p->x;
    p->state_t++;
    if (p->invuln) p->invuln--;
    /* RV energy meter (GUIEnergy_127 alt A): -3/frame while boosting, else +0.4/frame up to 100 */
    if (p->state == ST_BOOST) p->energy = fmax(0., p->energy - PHYS_ENERGY_DRAIN);
    else p->energy = fmin(PHYS_ENERGY_MAX, p->energy + PHYS_ENERGY_REFILL);
    /* RE (event 1574, start of the movement block): B, G and N are floored to 4 decimals every frame */
    {
        #define R4(v) (floor((double)(v) * 10000.0) / 10000.0)
        if (p->ground) p->gsp = R4(p->gsp); else p->vx = R4(p->vx);
        p->vy = R4(p->vy);
        p->N = R4(p->N);
        #undef R4
        /* RE 1575/1576: B within +-D (15); 1577: G = min(G, H) (15) before this frame's move and gravity */
        double *b = p->ground ? &p->gsp : &p->vx;
        if (*b > PHYS_MAXSPD) *b = PHYS_MAXSPD;
        if (*b < -PHYS_MAXSPD) *b = -PHYS_MAXSPD;
        if (!p->ground && p->vy > PHYS_MAXFALL) p->vy = PHYS_MAXFALL;
    }

    if (p->state == ST_DEAD) {
        p->vy += PHYS_GRAV; p->y += p->vy;
        set_anim(p, A_DEFEAT); anim_tick(&p->an, -1);
        if (++p->dead_t > 120) player_reset(p, p->spawn_x, p->spawn_y);
        return;
    }

    int crouch_look = 0;
    p->look = 0;
    p->held = in->held;
    if (p->state == ST_HANG) {
        /* RE 4062/4065 hold the position; RV: B is 0 from the next frame, G keeps gaining gravity (unused) */
        p->x = p->hang_x; p->y = p->hang_y + 32 + ORIG_REF_DY; p->vx = 0; if (p->vy > PHYS_MAXFALL) p->vy = PHYS_MAXFALL; p->vy += PHYS_GRAV;
        if (in->pressed & BTN_JUMP) { p->state = ST_NORMAL; p->vx = 0; p->vy = -PHYS_JUMP; p->jumping = 1; snd_play(G.sfx.jump, 200, 128); }   /* INFERRED */
        else if (in->pressed & BTN_DOWN) { p->state = ST_NORMAL; p->vy = 0; p->y += 1; }                                                     /* INFERRED */
        set_anim(p, A_HANG); anim_tick(&p->an, -1);
        return;
    }
    switch (p->state) {
    case ST_NORMAL:
        if (p->ground) {
            if (ground_actions(p, in)) break;
            /* RV: Down / Up while stopped -> crouch (M3) / look up (M2); no walking from them */
            /* RV (gcyc_jump): Down + direction from a standstill walks; crouch/look-up need no direction */
            if (fabs(p->gsp) < 0.01 && (in->held & (BTN_DOWN | BTN_UP)) && !(in->held & (BTN_LEFT | BTN_RIGHT))) crouch_look = 1;
            if (crouch_look) p->look = (in->held & BTN_UP) && !(in->held & BTN_DOWN) ? 2 : (in->held & BTN_DOWN) && !(in->held & BTN_UP) ? 3 : 0;   /* M2 / M3 */
            ground_move(p, in, !crouch_look, 1);
            /* RV: with Down held (no direction) Lilac drops into a crouch once speed falls below 2
             * (down_kick: B 2.05 -> 0, M3 on the frame friction would take it under 2) */
            if (p->ground && (in->held & BTN_DOWN) && !(in->held & (BTN_LEFT | BTN_RIGHT)) && fabs(p->gsp) < PHYS_LOWSPD) p->gsp = 0;
        } else {
            int act = air_action_wanted(p, in);
            air_move(p, in, 1, PHYS_GRAV);
            /* RV: order move -> gravity -> cut (tap: -6.25 -> -3 -> -2.75) */
            if (p->jumping && !p->ground && !(in->held & BTN_JUMP) && p->vy < -PHYS_JUMP_CUT) p->vy = -PHYS_JUMP_CUT;
            air_action_apply(p, in, act);
        }
        break;
    case ST_ATTACK:   /* RV: 22 frames, physics unchanged (ground: no input; air: normal) */
        if (p->state_t == 7) {                     /* RV: swing sound 21, 7 frames into the attack */
            snd_play_orig(21, 220, 128);
            static const char *vo[4] = { "lilac_attack0", "lilac_attack1", "lilac_attack2", "lilac_attack3" };
            voice_play(vo[rand() & 3]);           /* voice choice/frequency INFERRED (sound_attack0-3 loops exist) */
        }
        if (p->ground) ground_move(p, in, 0, 1); else air_move(p, in, 1, PHYS_GRAV);
        if (p->state == ST_ATTACK && p->state_t >= PHYS_ATTACK_T) p->state = ST_NORMAL;
        break;
    case ST_UPPERCUT: /* RV: stand 10 f (jump allowed = high uppercut), then vx 2*face, vy -7; ~39 f total */
        if (p->state_t == 1) {
            static const char *vu[4] = { "lilac_uppercut0", "lilac_uppercut1", "lilac_uppercut2", "lilac_uppercut3" };
            voice_play(vu[rand() & 3]);           /* INFERRED (sound_uppercut0-3 loops exist) */
        }
        if (p->state_t <= PHYS_UPPER_WIND) {
            if (p->ground) {
                if (in->pressed & BTN_JUMP) { jump(p); p->jumping = 0; }
                else ground_move(p, in, 0, 1);
            } else air_move(p, in, 0, PHYS_GRAV);
            if (p->state_t == PHYS_UPPER_WIND) {
                if (p->ground) detach(p);
                p->ground = 0; p->vx = PHYS_UPPER_VX * p->face; p->vy = -PHYS_UPPER_VY;
            }
        } else {
            air_move(p, in, 0, PHYS_GRAV);
            if (p->state == ST_UPPERCUT && p->state_t >= PHYS_UPPER_T) p->state = ST_NORMAL;
        }
        if (p->ground && p->state_t > PHYS_UPPER_WIND) p->state = ST_NORMAL;
        break;
    case ST_DKWIND:   /* RV: 15 f windup at gravity 0.125 (jump ignored), then vx = 5*face, vy += 5 */
        air_move(p, in, 0, PHYS_DK_WIND_GRAV);
        if (p->ground) { p->state = ST_NORMAL; break; }
        /* RV: kick vy = windup vy + 5 (down_kick -1.25 -> 3.875, cyc_dk_early -0.075 -> 4.925, dk_wind 1.375 -> 6.375) */
        if (p->state_t >= PHYS_DK_WIND_T) {
            set_state(p, ST_DIVEKICK); p->vx = PHYS_DK_VX * p->face; p->vy += PHYS_DK_VY_ADD;
            snd_play_orig(176, 220, 128); snd_play_orig(396, 220, 128);   /* RV: kick launch = 176 + 396 */
        }
        break;
    case ST_DIVEKICK: /* RV: normal air physics (M0) with the kick animation until landing; jump = cyclone if unused */
        {
            int cyc = (in->pressed & BTN_JUMP) && !p->cyclone_used && !p->boost_used;
            air_move(p, in, 1, PHYS_GRAV);
            if (p->ground) { p->state = ST_NORMAL; break; }
            if (cyc) start_air_cyclone(p, in);
        }
        break;
    case ST_CYCLONE:  /* RV: one 79-frame state (M26) on the ground or in the air; landing keeps it.
                       * Ground: normal ground physics + control; jump -> vy -4.9. Air: gravity 0.1, or
                       * 0.25 while Down is held; Down + attack = down-kick windup. Attack/special ignored. */
        if (p->ground) {
            if (in->pressed & BTN_JUMP) {
                ground_move(p, in, 0, 0);
                if (p->ground) { jump(p); p->vy = -PHYS_GCYC_JUMP; p->jumping = 0; p->cyclone_used = 1; }
            } else ground_move(p, in, 1, 1);
        } else {
            int dk = (in->pressed & BTN_ATTACK) && (in->held & BTN_DOWN);
            air_move(p, in, 1, (in->held & BTN_DOWN) ? PHYS_GRAV : PHYS_CYCLONE_GRAV);
            if (dk && !p->ground) { set_state(p, ST_DKWIND); snd_play_orig(405, 220, 128); break; }
        }
        if (p->state == ST_CYCLONE && p->state_t >= PHYS_CYCLONE_T) p->state = ST_NORMAL;
        break;
    case ST_CROUCHATK: /* RV (gc_down_attack): crouch kick, anim 30 for ~18 frames, no movement */
        ground_move(p, in, 0, 1);
        if (p->state_t >= PHYS_CROUCHATK_T) p->state = ST_NORMAL;
        break;
    case ST_CHARGE:   /* RV (ground): speed frozen for 30 f, then auto-fire. Release first = cancel */
        if (!(in->held & BTN_SPECIAL)) { p->state = ST_NORMAL; break; }
        if (p->ground) ground_move(p, in, 0, 0);
        else {                                         /* RV (boost_air_*): air charge hovers, B = G = 0, */
            if (p->state_t == 1) air_move(p, in, 0, 0); /* after one last move with the old speed */
            p->vx = 0; p->vy = 0;
        }
        if (p->state_t >= PHYS_CHARGE_T) start_boost(p, in);
        break;
    case ST_BOOST: {  /* RV: speed 12 held constant for 33 f, then normal physics carry the speed */
        double ox = p->x, oy = p->y;
        if (p->ground && p->boost_dy >= 0) {
            /* RV: B set to 12 when fired; afterwards only the slope force changes it (12 -> 12.82 down a 45 deg slope) */
            if (p->state_t == 1) p->gsp = p->boost_dx * PHYS_BOOST;   /* (landed boosts keep their 10) */
            ground_move(p, in, 0, 0);
            if (p->ground) slope_force(p);
        }
        else { p->vx = p->boost_dx * PHYS_BOOST; p->vy = p->boost_dy * PHYS_BOOST; air_move(p, in, 0, 0); }
        if (p->state == ST_BOOST && (p->state_t >= PHYS_BOOST_T || (p->x == ox && p->y == oy))) {
            p->state = ST_NORMAL;
            p->invuln = 30;   /* [KB] brief invincibility after the boost (duration UNVERIFIED) */
        }
        break;
    }
    case ST_HURT:     /* RV (hurt_turretus): knockback (-2.2*away, -4.2), gravity 0.5 + air drag; after
                       * landing M8 stays ~5 frames with ground friction */
        if (p->ground) { ground_move(p, in, 0, 1); if (p->state_t >= p->hurt_land_t + 5) p->state = ST_NORMAL; }
        else { air_move(p, in, 0, PHYS_HURT_GRAV); if (p->ground) p->hurt_land_t = p->state_t; }
        break;
    }

    clamp_edges(p, x_start);
    if (p->state != ST_DEAD && p->y > G.lvl.h + 64) {
        p->hp = 0; p->state = ST_DEAD; p->dead_t = 40; p->vy = 0;
        snd_play(G.sfx.hurt, 220, 128);
    }

    /* sprite angle (RV, rot_run_x and boost_x traces, original Active angle):
     *  ground: smoothed angle O; boost: direction of motion (ground: O, +180 facing left);
     *  air: eases back to 0 by 4 degrees per frame; landing snaps to the surface */
    {
        double deg_s = p->O;
        if (p->state == ST_BOOST) {
            if (p->ground) p->rot = deg_s + (p->face < 0 ? 180. : 0.);
            else p->rot = atan2(-p->boost_dy, p->boost_dx) * 57.29578;
        } else if (p->ground) p->rot = deg_s;
        else {
            double r = fmod(p->rot, 360.); if (r < 0) r += 360.;
            if (r <= 4. || r >= 356.) r = 0; else if (r < 180.) r -= 4.; else r += 4.;
            p->rot = r;
        }
        p->rot = fmod(p->rot + 720., 360.);
    }

    /* animation selection (RV: recovered indices, speed bands from traces) */
    int a, speed = -1, restart = 0;
    switch (p->state) {
    case ST_ATTACK: a = p->ground ? A_ATTACK : A_AIRATK; restart = 1; break;
    case ST_UPPERCUT: a = A_UPPERCUT; restart = 1; break;
    case ST_DKWIND: a = A_DKWIND; restart = 1; break;
    case ST_DIVEKICK: a = A_DIVEKICK; break;
    case ST_CYCLONE: a = A_CYCLONE; break;
    case ST_CROUCHATK: a = A_CROUCHATK; restart = 1; break;
    case ST_CHARGE: a = A_CHARGE; break;
    case ST_BOOST: a = A_BOOST; break;
    case ST_HURT: a = A_HURT; break;
    default:
        if (p->ground) {
            double s = fabs(p->gsp);
            int pushing_back = (p->gsp > 0.5 && (in->held & BTN_LEFT)) || (p->gsp < -0.5 && (in->held & BTN_RIGHT));
            (void)pushing_back;
            if (p->skidding) a = A_SKID;                                  /* RV: anim 17 while skidding */
            else if (s < 0.01) a = crouch_look ? ((in->held & BTN_DOWN) ? A_CROUCH : A_LOOKUP) : A_IDLE;
            else if (s < PHYS_TOP) { a = A_WALK; speed = 30; }          /* RV band; speed fitted */
            else if (s < 9.5) { a = A_RUN; speed = 35 + (int)s; }      /* RV band; speed fitted */
            else { a = A_FASTRUN; speed = 35 + (int)s; }
        } else a = (p->springing && p->vy < 0) ? A_SPRING : A_JUMP;
    }
    if (restart) { if (cur_anim != a || p->state_t == 1) { set_anim(p, a); anim_restart(&p->an); } }
    else set_anim(p, a);
    anim_tick(&p->an, speed);
}

void player_draw(const player_t *p) {
    if (p->invuln && (p->invuln & 4) && p->state != ST_HURT) return;
    /* the sprite hotspot (= rotation centre) is the movement object's position: p->y - ORIG_REF_DY */
    const frame_t *f = anim_frame(&p->an);
    double hxp = p->x, hyp = p->y - ORIG_REF_DY;   /* RE 1517: the skin sits at (int X, int Y) */
    frame_draw_rot(&G.lilac, f, floor(hxp - G.camx), floor(hyp - G.camy), p->rot, 0xffffffff);
}

int player_attack_box(const player_t *p, float *x0, float *y0, float *x1, float *y1) {
    switch (p->state) {
    case ST_ATTACK: {
        double fx = p->x + p->face * 4;
        *x0 = p->face > 0 ? fx : fx - 34; *x1 = *x0 + 34;
        *y0 = p->y - 34; *y1 = p->y - 2;
        return 1;
    }
    case ST_UPPERCUT: case ST_DIVEKICK: {
        double fx = p->x + p->face * 2;
        *x0 = fx - 16; *x1 = fx + 16; *y0 = p->y - 40; *y1 = p->y + 2;
        return 1;
    }
    case ST_CYCLONE: case ST_BOOST:
        *x0 = p->x - 20; *x1 = p->x + 20; *y0 = p->y - 38; *y1 = p->y + 2;
        return 1;
    case ST_NORMAL:
        if (!p->ground && p->jumping) { *x0 = p->x - 14; *x1 = p->x + 14; *y0 = p->y - 30; *y1 = p->y; return 1; }
    }
    return 0;
}
