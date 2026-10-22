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
/* level collision API returns floats */
static int fprobe(double x, double y0, double dist, int oneway, double *sy, double *ang) {
    float fs, fa; int r = floor_probe(&G.lvl, (float)x, (float)y0, (float)dist, oneway, &fs, &fa);
    if (r) { *sy = fs; *ang = fa; }
    return r;
}
#define LAND_REACH 4.
/* RE (events 1644 / 1646): on the ground, with the RAW sensed angle N between 35 and 325 degrees,
 * B -= F sin(N) when |B| >= C (5), and B -= 0.38 F sin(N) when 0 < |B| < C (no force when stopped) */
static void slope_force(player_t *p) {
    double a = p->angle, deg = a * 57.29578;
    while (deg < 0) deg += 360.;
    while (deg >= 360.) deg -= 360.;
    if (!(deg > 35. && deg < 325.)) return;
    double s = fabs(p->gsp);
    if (s >= PHYS_TOP) p->gsp -= PHYS_SLOPE * sin(a);
    else if (p->gsp != 0) p->gsp -= PHYS_SLOPE * sin(a) * 0.38;
}

void player_bounce(player_t *p, double vy) { if (!p->ground && p->vy > 0) p->vy = vy; }

/* RV (spring_yellow_up / _upleft): on contact the feet snap to the spring top and the speed is set
 * (vertical: vy -10.75, vx kept; diagonal: (+-9, -8.75)); the original's spring state (M40, skin anim 14)
 * lasts until the apex, then the fall animation; normal gravity and air drag apply */
void player_spring(player_t *p, double vx, double vy, double top) {
    p->ground = 0; p->y = top; p->vy = -vy; if (vx) { p->vx = vx; p->face = vx > 0 ? 1 : -1; }
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

/* ---- 360-degree ground collision (Sonic-Worlds-style modes) ----------------
 * angle a: 0 = flat floor, +pi/2 = right wall (running up), +-pi = ceiling.
 * tangent t = (cos a, -sin a), surface normal n = (-sin a, -cos a) (screen coords).
 * Mode picks the axis we probe along ("down" towards the surface). */
static const int MODE_DOWN[4][2] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };   /* floor, rwall, ceiling, lwall */
static const int MODE_FWD[4][2]  = { {1, 0}, {0, -1}, {-1, 0}, {0, 1} };
#define SLIP_SPEED 2.5

/* Floor mode extends to ~50 deg (not 45): DV1 has long exact-45-degree slopes (orig N = 315),
 * and switching to wall probing right at 45 broke the concave slope->flat transition. */
#define MODE_EDGE 0.82
static int mode_of(double a) {
    if (a > -MODE_EDGE && a <= MODE_EDGE) return 0;
    if (a > MODE_EDGE && a <= 3.14159 - MODE_EDGE) return 1;
    if (a < -MODE_EDGE && a >= -(3.14159 - MODE_EDGE)) return 3;
    return 2;
}

/* scan from (x,y) along (dx,dy) for the first solid pixel; platforms only floor-down */
static int scan(int x, int y, int dx, int dy, int maxd, int oneway, int *cx, int *cy) {
    /* oneway=1 (floor mode): platform pixels are top-only. oneway=0 (wall/ceiling
     * modes): platform pieces are ridden like solid (curves/quarter-pipes are type 8). */
    if (px_solid(&G.lvl, x, y) || (!oneway && px_platform(&G.lvl, x, y))) return -1;
    for (int k = 1; k <= maxd; k++) {
        int qx = x + dx * k, qy = y + dy * k;
        if (px_solid(&G.lvl, qx, qy) ||
            (!oneway && px_platform(&G.lvl, qx, qy)) ||
            (oneway && dy == 1 && px_platform(&G.lvl, qx, qy) && !px_platform(&G.lvl, qx, qy - 1))) {
            *cx = qx; *cy = qy; return k;
        }
    }
    return -1;
}

/* probe the surface below foot point (fx,fy) in mode m; returns contact + angle */
static int ground_probe(int m, double fx, double fy, double reach_up, double reach_down, double *ox, double *oy, double *oa) {
    const int *dn = MODE_DOWN[m], *fw = MODE_FWD[m];
    int best = -1, bx = 0, by = 0;
    for (int s = -1; s <= 1; s += 2) {
        int sx = (int)floor(fx + fw[0] * 5 * s - dn[0] * reach_up);
        int sy = (int)floor(fy + fw[1] * 5 * s - dn[1] * reach_up);
        int cx, cy, k = scan(sx, sy, dn[0], dn[1], (int)(reach_up + reach_down), m == 0, &cx, &cy);
        if (k >= 0 && (best < 0 || k < best)) { best = k; bx = cx; by = cy; }
    }
    if (best < 0) return 0;
    /* contact replaces the foot's coordinate along the down axis */
    *ox = dn[0] ? (float)bx - (dn[0] > 0 ? 0 : -1) : fx;
    *oy = dn[1] ? (float)by - (dn[1] > 0 ? 0 : -1) : fy;
    /* angle from surface points behind/ahead along the forward axis */
    double pts[2][2]; int ok[2];
    /* note: the original's raw angles are atan(n/14) (14 px sensor spread) but a plain +-7 spread here
     * broke vertical jumps at the old spawn (X 68.7), so +-6 stays until the sensor layout is traced */
    for (int s = 0; s < 2; s++) {
        double d = s ? 6. : -6.;
        int sx = (int)floor(*ox + fw[0] * d - dn[0] * 12), sy = (int)floor(*oy + fw[1] * d - dn[1] * 12);
        int cx, cy;
        ok[s] = scan(sx, sy, dn[0], dn[1], 24, m == 0, &cx, &cy) >= 0;
        pts[s][0] = ok[s] ? cx : *ox + fw[0] * d; pts[s][1] = ok[s] ? cy : *oy + fw[1] * d;
    }
    if (!ok[0] && !ok[1]) { *oa = m == 0 ? 0. : m == 1 ? 1.5708 : m == 2 ? 3.14159 : -1.5708; return 1; }
    double dxv = pts[1][0] - pts[0][0], dyv = pts[1][1] - pts[0][1];
    *oa = atan2(-dyv, dxv);
    return 1;
}

/* point in body space: along the normal (up from the feet) */
static void body_pt(const player_t *p, double up, double fwd, double *x, double *y) {
    double sa = sin(p->angle), ca = cos(p->angle);
    *x = p->x - sa * up + ca * fwd;
    *y = p->y - ca * up - sa * fwd;
}

static int wall_ahead(const player_t *p, double dir) {
    double x, y;
    for (int i = 0; i < 2; i++) {
        body_pt(p, i ? BODY_H - 6 : BODY_H * 0.5, dir * HALF_W, &x, &y);
        if (solid_at(&G.lvl, x, y)) return 1;
    }
    return 0;
}

/* axis-aligned wall test used in the air */
static int wall(const player_t *p, double x, double dir) {
    double wx = x + dir * HALF_W;
    double top = p->y - BODY_H + 4, mid = p->y - BODY_H * 0.5, low = p->y - (STEP_UP + 2);
    return solid_at(&G.lvl, wx, top) || solid_at(&G.lvl, wx, mid) || solid_at(&G.lvl, wx, low);
}

static int probe2(double x, double y0, double dist, int oneway, double *sy, double *ang) {
    double s1, s2, a1 = 0, a2 = 0;
    int f1 = fprobe(x - 5, y0, dist, oneway, &s1, &a1);
    int f2 = fprobe(x + 5, y0, dist, oneway, &s2, &a2);
    if (!f1 && !f2) return 0;
    if (f1 && (!f2 || s1 <= s2)) { *sy = s1; *ang = a1; } else { *sy = s2; *ang = a2; }
    double sc, ac;
    if (fprobe(x, *sy - 6, 12, oneway, &sc, &ac)) *ang = ac;
    return 1;
}

static void land(player_t *p, double sy, double ang) {
    p->y = sy; p->ground = 1; p->angle = ang; p->angle_s = ang; p->springing = 0;
    if (p->state == ST_BOOST) {
        /* RV (boost_air_downright/downleft): a down-diagonal boost (8, 8) that lands carries on along the
         * ground at a constant 10 for the rest of the boost */
        p->gsp = (p->boost_dx > 0 ? 1. : -1.) * PHYS_BOOST_LAND; p->boost_dy = 0;
        p->boost_dx = p->boost_dx > 0 ? PHYS_BOOST_LAND / PHYS_BOOST : -PHYS_BOOST_LAND / PHYS_BOOST;
        p->jumping = 0; p->cyclone_used = 0;
        return;
    }
    /* RE (event 1591): landing with G >= 0 on a sensed angle N between 22 and 338 degrees:
     * B = B - G * J * sin(N), J = 0.84; otherwise B (the horizontal speed) is kept */
    {
        double deg = ang * 57.29578;
        while (deg < 0) deg += 360.;
        while (deg >= 360.) deg -= 360.;
        p->gsp = p->vx;
        if (p->vy >= 0 && deg > 22. && deg < 338.) p->gsp = p->vx - p->vy * 0.84 * sin(ang);
    }
    p->jumping = 0; p->cyclone_used = 0; p->boost_used = 0;
    if (p->state == ST_HURT || p->state == ST_CYCLONE) p->state = ST_NORMAL;
}

static void detach(player_t *p) {
    p->vx = p->gsp * cos(p->angle); p->vy = -p->gsp * sin(p->angle);
    p->ground = 0; p->angle = 0;
}

/* RV: original order per logic frame = MOVE with the current speed, THEN update the speed
 * (dX(f) = B(f-1)*cos(O); see RUNTIME_MAP "Ground movement"). */
/* friction step toward 0 (RV: 5E, or 9E when the step would leave |gsp| below 2) */
static double fric_step(double g) {
    double s = fabs(g), f = PHYS_ACC * PHYS_FRIC_MUL;
    if (s - f < PHYS_LOWSPD) f = PHYS_ACC * PHYS_ACC_LOWMUL;
    return g - fmin(s, f) * (g > 0 ? 1 : -1);
}

static void ground_speed_update(player_t *p, const input_t *in, int control) {
    double a = p->angle_s;
    int dir = 0;
    if (p->ctrl_lock > 0) { p->ctrl_lock--; control = 0; }   /* RV: side spring locks input 4 frames */
    if (control && (in->held & BTN_RIGHT)) dir = 1;
    else if (control && (in->held & BTN_LEFT)) dir = -1;
    double s = fabs(p->gsp);
    /* RV: the x9 rate is used when the NORMAL step would leave |gsp| below 2
     * (accel: 0->0.66->1.33->1.99 then +E; friction: 2.43->2.06 (5E) then 2.06->1.40 (9E)) */
    if (dir && p->gsp * dir < 0) {
        /* RV (skid, spring_yellow_right): input against the motion. A skid (-0.5/frame, anim 17) only
         * starts when the input turns Lilac round; that frame and any frame where she already faces the
         * input (e.g. thrown backwards by a spring) use friction then acceleration toward the input */
        int turning = p->face != dir;
        p->face = dir;
        if (p->skidding) {
            p->gsp += dir * PHYS_DEC;
            if (p->gsp * dir > 0) p->gsp = 0;
        } else {
            p->gsp = fric_step(p->gsp);
            double g = fabs(p->gsp) * -1., n = g + PHYS_ACC;     /* signed along dir */
            if (fabs(n) < PHYS_LOWSPD) n = g + PHYS_ACC * PHYS_ACC_LOWMUL;
            p->gsp = dir * n;
            if (turning) p->skidding = 1;
        }
    } else if (dir) {
        p->face = dir; p->skidding = 0;
        if (s < PHYS_TOP) {
            double n = s + PHYS_ACC;
            if (n < PHYS_LOWSPD) n = s + PHYS_ACC * PHYS_ACC_LOWMUL;
            p->gsp = dir * fmin(n, PHYS_TOP);
        }                                              /* RV: >= top while holding: speed kept */
    } else {
        p->skidding = 0;
        p->gsp = fric_step(p->gsp);
    }
    if (p->gsp == 0) p->skidding = 0;
    (void)a; slope_force(p);
    if (p->gsp > PHYS_MAXSPD) p->gsp = PHYS_MAXSPD;
    if (p->gsp < -PHYS_MAXSPD) p->gsp = -PHYS_MAXSPD;
}

/* smoothed angle O eases toward the sensed angle N (RE events 1605-1607; the earlier fit
 * k 0.175 @ |B| 1.3, 0.33 @ 5, 0.40 @ 6.1 is reproduced exactly by max(0.175, |B|/15)) */
static void smooth_angle(player_t *p) {
    double d = p->angle - p->angle_s;
    while (d > 3.14159) d -= 6.28318;
    while (d < -3.14159) d += 6.28318;
    /* RE (events 1606/1607): within 45 degrees O moves toward N by k = max(0.175, |B| / D), D = 15;
     * at 45 degrees or more O snaps to N */
    double k = fabs(d) < 0.785398 ? fmax(0.175, fabs(p->gsp) / 15.) : 1.;
    p->angle_s += d * k;
}

/* upd: 1 = normal speed update, 0 = speed frozen (boost charge / boost: RV, B constant) */
static void ground_move(player_t *p, const input_t *in, int control, int upd) {
    /* too slow on a wall or ceiling: fall off */
    if (mode_of(p->angle) != 0 && fabs(p->gsp) < SLIP_SPEED) { detach(p); return; }

    double dist = fabs(p->gsp);
    int n = (int)(dist / 4.) + 1;
    for (int i = 0; i < n && p->gsp != 0; i++) {
        double dir = p->gsp >= 0 ? 1. : -1.;
        if (wall_ahead(p, dir)) { p->gsp = 0; break; }
        double step = p->gsp / n;
        double ma = mode_of(p->angle) == 0 ? p->angle_s : p->angle;
        double nx = p->x + cos(ma) * step, ny = p->y - sin(ma) * step;
        int m = mode_of(p->angle);
        double ox, oy, oa;
        double down = 6. + fabs(p->gsp) * 1.5;
        if (ground_probe(m, nx, ny, STEP_UP, down, &ox, &oy, &oa)) {
            p->x = ox; p->y = oy; p->angle = oa;
        } else {
            p->x = nx; p->y = ny;
            detach(p);
            return;
        }
    }
    smooth_angle(p);
    if (upd) ground_speed_update(p, in, control);
    if (mode_of(p->angle) == 2 && fabs(p->gsp) < SLIP_SPEED) detach(p);
}

/* RV order: air accel -> gravity -> Sonic-style air drag tested on the NEW vy
 * (uppercut trace: vx 2 -> 1.9375 on the frame vy goes -4 -> -3.75; stops once vy >= 0) */
static void air_speed_update(player_t *p, const input_t *in, int control, double grav) {
    if (control) {
        if ((in->held & BTN_RIGHT) && p->vx < PHYS_TOP) { p->vx = fmin(p->vx + PHYS_AIR_ACC, PHYS_TOP); p->face = 1; }
        if ((in->held & BTN_LEFT) && p->vx > -PHYS_TOP) { p->vx = fmax(p->vx - PHYS_AIR_ACC, -PHYS_TOP); p->face = -1; }
    }
    if (grav > 0) {
        if (p->vy > PHYS_MAXFALL) p->vy = PHYS_MAXFALL;   /* RE 1577: G clamped to H BEFORE gravity (max 15.25) */
        p->vy += grav;
        if (p->vy > -PHYS_DRAG_VY && p->vy < 0 && fabs(p->vx) >= 0.125) p->vx *= PHYS_AIR_DRAG;   /* RE 1648: only |B| >= 0.125 */
    }
}

/* RV: in the air the original also moves first, then adds acceleration and gravity
 * (jump: y moves -7 on the first air frame, then G = -6.75) */
static void air_move(player_t *p, const input_t *in, int control, double grav) {
    int n = (int)(fabs(p->vx) / 4.) + 1;
    for (int i = 0; i < n; i++) {
        double nx = p->x + p->vx / n, sy, ang;
        if (p->vx != 0 && wall(p, nx, p->vx > 0 ? 1. : -1.)) {
            p->vx = 0;
            if (p->state == ST_BOOST) p->boost_used = 0;   /* [KB] wall ram restores cyclone */
            break;
        }
        p->x = nx;
        if (p->vy >= 0 && probe2(p->x, p->y - STEP_UP, STEP_UP, 0, &sy, &ang) && sy <= p->y) { land(p, sy, ang); if (p->state != ST_BOOST) ground_speed_update(p, in, control); return; }
    }
    n = (int)(fabs(p->vy) / 4.) + 1;
    for (int i = 0; i < n; i++) {
        double ny = p->y + p->vy / n, sy, ang;
        if (p->vy < 0) {
            if (solid_at(&G.lvl, p->x - 4, ny - BODY_H) || solid_at(&G.lvl, p->x + 4, ny - BODY_H)) { p->vy = 0; break; }
            p->y = ny;
            /* RV (spring_yellow_upleft f52): the frame the head reaches the ceiling, G becomes 0 at once
             * and no gravity is added that frame */
            if (i == n - 1 && (solid_at(&G.lvl, p->x - 4, p->y - BODY_H - 1) || solid_at(&G.lvl, p->x + 4, p->y - BODY_H - 1))) {
                if (p->vy > -PHYS_DRAG_VY && fabs(p->vx) >= 0.125) p->vx *= PHYS_AIR_DRAG;   /* drag still applies that frame */
                p->vy = 0; air_speed_update(p, in, control, 0); return;
            }
        } else {
            /* RV (landing snaps in 20+ traces: +2..+3.99 px): on the last step the original lands when the
             * ground is up to LAND_REACH px below the feet and snaps down to it */
            double reach = (i == n - 1) ? LAND_REACH : 1.;
            if (probe2(p->x, p->y - 2, (ny - p->y) + 2 + reach, 1, &sy, &ang) && sy <= ny + reach) { land(p, sy, ang); if (p->state != ST_BOOST) ground_speed_update(p, in, control); return; }
            p->y = ny;
        }
    }
    air_speed_update(p, in, control, grav);
}

/* RV: on the jump frame Lilac first moves along the ground with her current speed, then the
 * velocity becomes B along + 7 perpendicular to the RAW surface angle N (run_jump: N 355.914,
 * B 5.013 -> vx 5.499, vy -6.625; using the smoothed O gives 5.396 / -6.71). No gravity that frame. */
static void jump(player_t *p) {
    double a = p->angle;
    p->vx = p->gsp * cos(a) - PHYS_JUMP * sin(a);
    p->vy = -p->gsp * sin(a) - PHYS_JUMP * cos(a);
    p->ground = 0; p->jumping = 1;
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
        ground_move(p, in, 0, 0);
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
    if (x0 <= r && p->x > r) { p->x = r; if (p->ground) p->gsp = 0; else p->vx = 0; }
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
        double deg = p->angle * 57.29577951308232;
        p->angle = floor(deg * 10000.0) / 10000.0 / 57.29577951308232;
        #undef R4
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
        double deg_s = p->angle_s * 57.29578;
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
    /* RV: the original's object position (= sprite hotspot, = rotation centre) is the reference point
     * ~14 px above the feet (ORIG_REF_DY). Our p->x/p->y is the foot contact, so the hotspot goes 14 px
     * up along the surface normal on the ground (normal = (-sin a, -cos a)) and straight up in the air.
     * Drawing at the feet sank Lilac 14 px into the floor. */
    const frame_t *f = anim_frame(&p->an);
    double hxp = p->x, hyp = p->y - ORIG_REF_DY;
    if (p->ground) { hxp = p->x - sin(p->angle_s) * ORIG_REF_DY; hyp = p->y - cos(p->angle_s) * ORIG_REF_DY; }
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
