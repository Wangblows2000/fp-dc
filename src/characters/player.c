/* Lilac. Animations: recovered tables of playerskinsash_341 (both facings drawn
 * separately in the original). Rules marked [KB] come from the SDA knowledge
 * base (kb.speeddemosarchive.com/Freedom_Planet/Lilac, /Mechanics_and_Bugs).
 * Physics constants are UNVERIFIED placeholders (fp.h). */
#include <math.h>
#include <stdio.h>
#include "../fp.h"

enum { ST_NORMAL, ST_ATTACK, ST_CYCLONE, ST_CHARGE, ST_BOOST, ST_HURT, ST_DEAD };

#define HALF_W   7.f
#define BODY_H   30.f
#define STEP_UP  12.f
#define MAX_HP   6       /* "life=6" in the game's own room files */

enum { A_IDLE, A_WALK, A_RUN, A_LOOKUP, A_SPIN, A_CHARGE, A_CROUCH, A_BOOST, A_HURT,
       A_CYCLONE, A_SKID, A_AIRATK, A_ATTACK, A_UPPERCUT, A_DIVEKICK, A_DEFEAT, A_COUNT };
static const char *anim_names[A_COUNT] = { "idle", "walk", "run", "lookup", "spin", "charge", "crouch",
    "boost", "hurt", "cyclone", "skid", "airatk", "attack", "uppercut", "divekick", "defeat" };
static int anim_r[A_COUNT], anim_l[A_COUNT];
static int cur_anim = -1;

static void set_anim(player_t *p, int a) {
    int idx = p->face < 0 ? anim_l[a] : anim_r[a];
    cur_anim = a;
    anim_set(&p->an, &G.lilac, idx);
}

void player_reset(player_t *p, float x, float y) {
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
    int crystals = p->crystals;
    float sx = p->spawn_x, sy = p->spawn_y;
    *p = (player_t){0};
    p->crystals = crystals;
    p->spawn_x = sx ? sx : x; p->spawn_y = sy ? sy : y;
    p->x = x; p->y = y; p->face = 1; p->hp = MAX_HP;
    set_anim(p, A_IDLE);
}

int player_invincible(const player_t *p) {
    return p->invuln > 0 || p->state == ST_BOOST || p->state == ST_DEAD;
}

void player_hurt(player_t *p, float from_x) {
    if (player_invincible(p)) return;
    p->hp--;
    snd_play(G.sfx.hurt, 220, 128);
    if (p->hp <= 0) { p->state = ST_DEAD; p->dead_t = 0; p->vy = -5; p->vx = 0; p->ground = 0; return; }
    p->state = ST_HURT; p->state_t = 0;
    float dir = (p->x < from_x) ? -1.f : 1.f;
    p->vx = 2.5f * dir; p->vy = -3.5f; p->ground = 0; p->jumping = 0;
    p->cyclone_used = 0;            /* [KB] cyclone usable again after taking damage */
    p->invuln = 120;
}

void player_bounce(player_t *p, float vy) { if (!p->ground && p->vy > 0) p->vy = vy; }

void player_spring(player_t *p, float vy) {
    p->ground = 0; p->vy = -vy; p->jumping = 0; p->cyclone_used = 0; p->boost_used = 0;
    if (p->state != ST_BOOST) p->state = ST_NORMAL;
}

/* ---- 360-degree ground collision (Sonic-Worlds-style modes) ----------------
 * angle a: 0 = flat floor, +pi/2 = right wall (running up), +-pi = ceiling.
 * tangent t = (cos a, -sin a), surface normal n = (-sin a, -cos a) (screen coords).
 * Mode picks the axis we probe along ("down" towards the surface). */
static const int MODE_DOWN[4][2] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };   /* floor, rwall, ceiling, lwall */
static const int MODE_FWD[4][2]  = { {1, 0}, {0, -1}, {-1, 0}, {0, 1} };
#define SLIP_SPEED 2.5f

static int mode_of(float a) {
    if (a > -0.785f && a <= 0.785f) return 0;
    if (a > 0.785f && a <= 2.356f) return 1;
    if (a < -0.785f && a >= -2.356f) return 3;
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
static int ground_probe(int m, float fx, float fy, float reach_up, float reach_down, float *ox, float *oy, float *oa) {
    const int *dn = MODE_DOWN[m], *fw = MODE_FWD[m];
    int best = -1, bx = 0, by = 0;
    for (int s = -1; s <= 1; s += 2) {
        int sx = (int)floorf(fx + fw[0] * 5 * s - dn[0] * reach_up);
        int sy = (int)floorf(fy + fw[1] * 5 * s - dn[1] * reach_up);
        int cx, cy, k = scan(sx, sy, dn[0], dn[1], (int)(reach_up + reach_down), m == 0, &cx, &cy);
        if (k >= 0 && (best < 0 || k < best)) { best = k; bx = cx; by = cy; }
    }
    if (best < 0) return 0;
    /* contact replaces the foot's coordinate along the down axis */
    *ox = dn[0] ? (float)bx - (dn[0] > 0 ? 0 : -1) : fx;
    *oy = dn[1] ? (float)by - (dn[1] > 0 ? 0 : -1) : fy;
    /* angle from surface points behind/ahead along the forward axis */
    float pts[2][2]; int ok[2];
    for (int s = 0; s < 2; s++) {
        float d = s ? 6.f : -6.f;
        int sx = (int)floorf(*ox + fw[0] * d - dn[0] * 12), sy = (int)floorf(*oy + fw[1] * d - dn[1] * 12);
        int cx, cy;
        ok[s] = scan(sx, sy, dn[0], dn[1], 24, m == 0, &cx, &cy) >= 0;
        pts[s][0] = ok[s] ? cx : *ox + fw[0] * d; pts[s][1] = ok[s] ? cy : *oy + fw[1] * d;
    }
    if (!ok[0] && !ok[1]) { *oa = m == 0 ? 0.f : m == 1 ? 1.5708f : m == 2 ? 3.14159f : -1.5708f; return 1; }
    float dxv = pts[1][0] - pts[0][0], dyv = pts[1][1] - pts[0][1];
    *oa = atan2f(-dyv, dxv);
    return 1;
}

/* point in body space: along the normal (up from the feet) */
static void body_pt(const player_t *p, float up, float fwd, float *x, float *y) {
    float sa = sinf(p->angle), ca = cosf(p->angle);
    *x = p->x - sa * up + ca * fwd;
    *y = p->y - ca * up - sa * fwd;
}

static int wall_ahead(const player_t *p, float dir) {
    float x, y;
    for (int i = 0; i < 2; i++) {
        body_pt(p, i ? BODY_H - 6 : BODY_H * 0.5f, dir * HALF_W, &x, &y);
        if (solid_at(&G.lvl, x, y)) return 1;
    }
    return 0;
}

/* axis-aligned wall test used in the air */
static int wall(const player_t *p, float x, float dir) {
    float wx = x + dir * HALF_W;
    float top = p->y - BODY_H + 4, mid = p->y - BODY_H * 0.5f, low = p->y - (STEP_UP + 2);
    return solid_at(&G.lvl, wx, top) || solid_at(&G.lvl, wx, mid) || solid_at(&G.lvl, wx, low);
}

static int probe2(float x, float y0, float dist, int oneway, float *sy, float *ang) {
    float s1, s2, a1 = 0, a2 = 0;
    int f1 = floor_probe(&G.lvl, x - 5, y0, dist, oneway, &s1, &a1);
    int f2 = floor_probe(&G.lvl, x + 5, y0, dist, oneway, &s2, &a2);
    if (!f1 && !f2) return 0;
    if (f1 && (!f2 || s1 <= s2)) { *sy = s1; *ang = a1; } else { *sy = s2; *ang = a2; }
    float sc, ac;
    if (floor_probe(&G.lvl, x, *sy - 6, 12, oneway, &sc, &ac)) *ang = ac;
    return 1;
}

static void land(player_t *p, float sy, float ang) {
    p->y = sy; p->ground = 1; p->angle = ang;
    /* [KB] downhill landing converts fall speed to ground speed (angle-dependent) */
    if (fabsf(p->vx) >= fabsf(p->vy) || fabsf(ang) < 0.2f) p->gsp = p->vx;
    else p->gsp = p->vx * cosf(ang) - p->vy * sinf(ang);
    p->jumping = 0; p->cyclone_used = 0; p->boost_used = 0;
    if (p->state == ST_HURT || p->state == ST_CYCLONE) p->state = ST_NORMAL;
}

static void detach(player_t *p) {
    p->vx = p->gsp * cosf(p->angle); p->vy = -p->gsp * sinf(p->angle);
    p->ground = 0; p->angle = 0;
}

static void ground_move(player_t *p, const input_t *in, int control) {
    float a = p->angle;
    if (control && (in->held & BTN_RIGHT)) {
        if (p->gsp < 0) p->gsp += PHYS_DEC;
        else if (p->gsp < PHYS_TOP) p->gsp = fminf(p->gsp + PHYS_ACC, PHYS_TOP);
        p->face = 1;
    } else if (control && (in->held & BTN_LEFT)) {
        if (p->gsp > 0) p->gsp -= PHYS_DEC;
        else if (p->gsp > -PHYS_TOP) p->gsp = fmaxf(p->gsp - PHYS_ACC, -PHYS_TOP);
        p->face = -1;
    } else {
        p->gsp -= fminf(fabsf(p->gsp), PHYS_FRIC) * (p->gsp > 0 ? 1 : -1);
    }
    if (fabsf(a) > 0.1f) p->gsp -= PHYS_SLOPE * sinf(a);
    if (p->gsp > PHYS_MAXSPD) p->gsp = PHYS_MAXSPD;
    if (p->gsp < -PHYS_MAXSPD) p->gsp = -PHYS_MAXSPD;

    /* too slow on a wall or ceiling: fall off */
    if (mode_of(p->angle) != 0 && fabsf(p->gsp) < SLIP_SPEED) { detach(p); return; }

    float dist = fabsf(p->gsp);
    int n = (int)(dist / 4.f) + 1;
    for (int i = 0; i < n; i++) {
        float dir = p->gsp >= 0 ? 1.f : -1.f;
        if (p->gsp != 0 && wall_ahead(p, dir)) { p->gsp = 0; break; }
        float step = p->gsp / n;
        float nx = p->x + cosf(p->angle) * step, ny = p->y - sinf(p->angle) * step;
        int m = mode_of(p->angle);
        float ox, oy, oa;
        float down = 6.f + fabsf(p->gsp) * 1.5f;
        if (ground_probe(m, nx, ny, STEP_UP, down, &ox, &oy, &oa)) {
            p->x = ox; p->y = oy; p->angle = oa;
        } else {
            p->x = nx; p->y = ny;
            detach(p);
            return;
        }
    }
    if (mode_of(p->angle) == 2 && fabsf(p->gsp) < SLIP_SPEED) detach(p);
}

static void air_move(player_t *p, const input_t *in, int control, int gravity) {
    if (control) {
        if ((in->held & BTN_RIGHT) && p->vx < PHYS_TOP) { p->vx = fminf(p->vx + PHYS_AIR_ACC, PHYS_TOP); p->face = 1; }
        if ((in->held & BTN_LEFT) && p->vx > -PHYS_TOP) { p->vx = fmaxf(p->vx - PHYS_AIR_ACC, -PHYS_TOP); p->face = -1; }
    }
    /* [KB] moving upward bleeds horizontal speed down to normal run speed */
    if (gravity && p->vy < 0 && fabsf(p->vx) > PHYS_TOP)
        p->vx -= (p->vx > 0 ? 1 : -1) * fminf(PHYS_AIR_DRAG, fabsf(p->vx) - PHYS_TOP);
    if (gravity) { p->vy += PHYS_GRAV; if (p->vy > PHYS_MAXFALL) p->vy = PHYS_MAXFALL; }

    int n = (int)(fabsf(p->vx) / 4.f) + 1;
    for (int i = 0; i < n; i++) {
        float nx = p->x + p->vx / n, sy, ang;
        if (p->vx != 0 && wall(p, nx, p->vx > 0 ? 1.f : -1.f)) {
            p->vx = 0;
            if (p->state == ST_BOOST) p->boost_used = 0;   /* [KB] wall ram restores cyclone */
            break;
        }
        p->x = nx;
        if (p->vy >= 0 && probe2(p->x, p->y - STEP_UP, STEP_UP, 0, &sy, &ang) && sy <= p->y) { land(p, sy, ang); return; }
    }
    n = (int)(fabsf(p->vy) / 4.f) + 1;
    for (int i = 0; i < n; i++) {
        float ny = p->y + p->vy / n, sy, ang;
        if (p->vy < 0) {
            if (solid_at(&G.lvl, p->x - 4, ny - BODY_H) || solid_at(&G.lvl, p->x + 4, ny - BODY_H)) { p->vy = 0; break; }
            p->y = ny;
        } else {
            if (probe2(p->x, p->y - 2, (ny - p->y) + 3, 1, &sy, &ang) && sy <= ny + 1) { land(p, sy, ang); return; }
            p->y = ny;
        }
    }
}

static void jump(player_t *p) {
    float a = p->angle;
    p->vx = p->gsp * cosf(a) - PHYS_JUMP * sinf(a);
    p->vy = -p->gsp * sinf(a) - PHYS_JUMP * cosf(a);
    p->ground = 0; p->jumping = 1;
    snd_play(G.sfx.jump, 200, 128);
}

static void start_boost(player_t *p, const input_t *in) {
    /* [KB] six directions: left/right and the four diagonals (no straight up/down) */
    float dx = (in->held & BTN_LEFT) ? -1.f : (in->held & BTN_RIGHT) ? 1.f : (float)p->face;
    float dy = (in->held & BTN_UP) ? -1.f : (in->held & BTN_DOWN) ? 1.f : 0.f;
    float len = sqrtf(dx * dx + dy * dy);
    p->boost_dx = dx / len; p->boost_dy = dy / len;
    p->face = dx > 0 ? 1 : -1;
    p->state = ST_BOOST; p->state_t = 0; p->boost_used = 1;
    if (dy < 0 || !p->ground) p->ground = 0;
    snd_play(G.sfx.boost, 230, 128);
}

void player_update(player_t *p, const input_t *in) {
    p->state_t++;
    if (p->invuln) p->invuln--;

    if (p->state == ST_DEAD) {
        p->vy += PHYS_GRAV; p->y += p->vy;
        set_anim(p, A_DEFEAT); anim_tick(&p->an, -1);
        if (++p->dead_t > 120) player_reset(p, p->spawn_x, p->spawn_y);
        return;
    }

    switch (p->state) {
    case ST_NORMAL:
        if (in->pressed & BTN_ATTACK) { p->state = ST_ATTACK; p->state_t = 0; snd_play(G.sfx.attack, 200, 128); break; }
        if (in->pressed & BTN_SPECIAL) { p->state = ST_CHARGE; p->state_t = 0; break; }
        if (p->ground) {
            if (in->pressed & BTN_JUMP) { jump(p); break; }
            ground_move(p, in, 1);
        } else {
            if ((in->pressed & BTN_JUMP) && !p->cyclone_used && !p->boost_used) {
                p->state = ST_CYCLONE; p->state_t = 0; p->cyclone_used = 1;
                if (p->vy > -2.f) p->vy = -2.f;
                snd_play(G.sfx.attack, 200, 128);
                break;
            }
            if (p->jumping && !(in->held & BTN_JUMP) && p->vy < -PHYS_JUMP_CUT) p->vy = -PHYS_JUMP_CUT;
            air_move(p, in, 1, 1);
        }
        break;
    case ST_ATTACK:
        if (p->ground) ground_move(p, in, 0); else air_move(p, in, 1, 1);
        if (p->an.done || p->state_t > 40) p->state = ST_NORMAL;
        break;
    case ST_CYCLONE:
        if (p->ground) { p->state = ST_NORMAL; break; }
        p->vy = fminf(p->vy, 1.0f);
        air_move(p, in, 1, 1);
        if (p->state == ST_CYCLONE && (p->state_t >= 30 || !(in->held & BTN_JUMP))) p->state = ST_NORMAL;
        break;
    case ST_CHARGE:
        /* [KB] charging in the air stops Lilac */
        if (p->ground) { p->gsp *= 0.85f; ground_move(p, in, 0); }
        else { p->vx *= 0.8f; p->vy *= 0.8f; air_move(p, in, 0, 0); }
        if (!(in->held & BTN_SPECIAL)) {
            if (p->state_t >= 20) start_boost(p, in); else p->state = ST_NORMAL;
        }
        break;
    case ST_BOOST: {
        float ox = p->x, oy = p->y;
        if (p->ground && p->boost_dy >= 0) { p->gsp = p->boost_dx * PHYS_BOOST; ground_move(p, in, 0); }
        else { p->vx = p->boost_dx * PHYS_BOOST; p->vy = p->boost_dy * PHYS_BOOST; air_move(p, in, 0, 0); }
        if (p->state == ST_BOOST && (p->state_t >= 24 || (p->x == ox && p->y == oy))) {
            p->state = ST_NORMAL;
            p->invuln = 30;   /* [KB] brief invincibility after the boost (duration UNVERIFIED) */
            if (p->ground) p->gsp = p->boost_dx * PHYS_TOP; else { p->vx *= 0.6f; p->vy *= 0.6f; }
        }
        break;
    }
    case ST_HURT:
        air_move(p, in, 0, 1);
        if (p->ground) p->state = ST_NORMAL;
        break;
    }

    if (p->state != ST_DEAD && p->y > G.lvl.h + 64) {
        p->hp = 0; p->state = ST_DEAD; p->dead_t = 40; p->vy = 0;
        snd_play(G.sfx.hurt, 220, 128);
    }

    /* animation state -> recovered animation */
    int a, speed = -1;
    switch (p->state) {
    case ST_ATTACK: a = p->ground ? A_ATTACK : A_AIRATK; break;
    case ST_CYCLONE: a = A_CYCLONE; break;
    case ST_CHARGE: a = A_CHARGE; break;
    case ST_BOOST: a = A_BOOST; break;
    case ST_HURT: a = A_HURT; break;
    default:
        if (p->ground) {
            float s = fabsf(p->gsp);
            int pushing_back = (p->gsp > 0.5f && (in->held & BTN_LEFT)) || (p->gsp < -0.5f && (in->held & BTN_RIGHT));
            if (pushing_back && s > 3.f) a = A_SKID;
            else if (s < 0.2f) a = (in->held & BTN_DOWN) ? A_CROUCH : (in->held & BTN_UP) ? A_LOOKUP : A_IDLE;
            else if (s < 4.0f) { a = A_WALK; speed = 10 + (int)(s * 6); }   /* speed scaling UNVERIFIED */
            else { a = A_RUN; speed = 10 + (int)(s * 4); }
        } else a = A_SPIN;
    }
    if (a == A_ATTACK || a == A_AIRATK) { if (cur_anim != a || p->state_t == 1) { set_anim(p, a); anim_restart(&p->an); } }
    else set_anim(p, a);
    anim_tick(&p->an, speed);
}

void player_draw(const player_t *p) {
    if (p->invuln && (p->invuln & 4) && p->state != ST_HURT) return;
    frame_draw(&G.lilac, anim_frame(&p->an), floorf(p->x - G.camx), floorf(p->y - G.camy), 0, 0xffffffff);
}

int player_attack_box(const player_t *p, float *x0, float *y0, float *x1, float *y1) {
    switch (p->state) {
    case ST_ATTACK: {
        float fx = p->x + p->face * 4;
        *x0 = p->face > 0 ? fx : fx - 34; *x1 = *x0 + 34;
        *y0 = p->y - 34; *y1 = p->y - 2;
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
