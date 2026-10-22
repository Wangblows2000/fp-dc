/* Lilac: ground/air movement on heightmap terrain plus character abilities.
 * Physics constants are UNVERIFIED placeholders (see DECISIONS.md D-005). */
#include <math.h>
#include "../fp.h"

enum { ST_NORMAL, ST_ATTACK, ST_CYCLONE, ST_CHARGE, ST_BOOST, ST_HURT, ST_DEAD };

#define HALF_W   7.f
#define BODY_H   30.f
#define STEP_UP  12.f
#define MAX_HP   6

static int A_IDLE, A_RUN, A_SPIN, A_CYC, A_ATK, A_CHG, A_BST, A_HURT, A_FALL;

static void set_anim(player_t *p, int a) {
    if (p->anim != a) { p->anim = a; p->anim_tick = 0; }
}

void player_reset(player_t *p, float x, float y) {
    static int init;
    if (!init) {
        init = 1;
        A_IDLE = bank_anim(&G.lilac, "idle"); A_RUN = bank_anim(&G.lilac, "run");
        A_SPIN = bank_anim(&G.lilac, "spin"); A_CYC = bank_anim(&G.lilac, "cyclone");
        A_ATK = bank_anim(&G.lilac, "attack"); A_CHG = bank_anim(&G.lilac, "charge");
        A_BST = bank_anim(&G.lilac, "boost"); A_HURT = bank_anim(&G.lilac, "hurt");
        A_FALL = bank_anim(&G.lilac, "fall");
    }
    int crystals = p->crystals;
    float sx = p->spawn_x, sy = p->spawn_y;
    *(p) = (player_t){0};
    p->crystals = crystals;
    p->spawn_x = sx ? sx : x; p->spawn_y = sy ? sy : y;
    p->x = x; p->y = y; p->face = 1; p->hp = MAX_HP; p->ground = 0;
    p->anim = A_IDLE;
}

int player_attacking(const player_t *p) {
    return p->state == ST_ATTACK || p->state == ST_CYCLONE || p->state == ST_BOOST ||
           (p->state == ST_NORMAL && !p->ground && p->jumping);
}

void player_hurt(player_t *p, float from_x) {
    if (p->invuln || p->state == ST_DEAD || p->state == ST_BOOST) return;
    p->hp--;
    snd_play(G.sfx.hurt, 220, 128);
    if (p->hp <= 0) { p->state = ST_DEAD; p->dead_t = 0; p->vy = -5; p->vx = 0; p->ground = 0; return; }
    p->state = ST_HURT; p->state_t = 0;
    float dir = (p->x < from_x) ? -1.f : 1.f;
    p->vx = 2.5f * dir; p->vy = -3.5f; p->ground = 0; p->jumping = 0;
    p->invuln = 120;
}

/* horizontal wall test at body height; returns 1 if blocked */
static int wall(const player_t *p, float x, float dir) {
    float wx = x + dir * HALF_W;
    float top = p->y - BODY_H + 4, mid = p->y - BODY_H * 0.5f;
    float low = p->y - (STEP_UP + 2);
    return solid_at(&G.lvl, wx, top) || solid_at(&G.lvl, wx, mid) || solid_at(&G.lvl, wx, low);
}

static int probe2(const player_t *p, float x, float y0, float dist, int oneway, float *sy, float *ang) {
    float s1, s2, a1 = 0, a2 = 0;
    int f1 = floor_probe(&G.lvl, x - 5, y0, dist, oneway, &s1, &a1);
    int f2 = floor_probe(&G.lvl, x + 5, y0, dist, oneway, &s2, &a2);
    (void)p;
    if (!f1 && !f2) return 0;
    if (f1 && (!f2 || s1 <= s2)) { *sy = s1; *ang = a1; }
    else { *sy = s2; *ang = a2; }
    /* prefer centre angle when both feet are on the same surface */
    float sc, ac;
    if (floor_probe(&G.lvl, x, *sy - 4, 8, oneway, &sc, &ac)) *ang = ac;
    return 1;
}

static void land(player_t *p, float sy, float ang) {
    p->y = sy; p->ground = 1; p->angle = ang;
    /* Sonic-style landing: mostly-horizontal motion keeps vx, otherwise
     * project velocity onto the surface tangent. */
    if (fabsf(p->vx) >= fabsf(p->vy) || fabsf(ang) < 0.2f) p->gsp = p->vx;
    else p->gsp = p->vx * cosf(ang) - p->vy * sinf(ang);
    p->jumping = 0; p->cyclone_used = 0;
    if (p->state == ST_HURT || p->state == ST_CYCLONE) p->state = ST_NORMAL;
}

static void ground_move(player_t *p, const input_t *in, int control) {
    float a = p->angle;
    if (control) {
        if (in->held & BTN_RIGHT) {
            if (p->gsp < 0) p->gsp += PHYS_DEC;
            else if (p->gsp < PHYS_TOP) p->gsp = fminf(p->gsp + PHYS_ACC, PHYS_TOP);
            p->face = 1;
        } else if (in->held & BTN_LEFT) {
            if (p->gsp > 0) p->gsp -= PHYS_DEC;
            else if (p->gsp > -PHYS_TOP) p->gsp = fmaxf(p->gsp - PHYS_ACC, -PHYS_TOP);
            p->face = -1;
        } else {
            p->gsp -= fminf(fabsf(p->gsp), PHYS_FRIC) * (p->gsp > 0 ? 1 : -1);
        }
    } else {
        p->gsp -= fminf(fabsf(p->gsp), PHYS_FRIC) * (p->gsp > 0 ? 1 : -1);
    }
    if (fabsf(a) > 0.1f) p->gsp -= PHYS_SLOPE * sinf(a);
    if (p->gsp > PHYS_MAXSPD) p->gsp = PHYS_MAXSPD;
    if (p->gsp < -PHYS_MAXSPD) p->gsp = -PHYS_MAXSPD;

    /* advance in sub-steps of <= 4px so we never tunnel through thin walls */
    float dx = p->gsp * cosf(a);
    int n = (int)(fabsf(dx) / 4.f) + 1;
    for (int i = 0; i < n; i++) {
        float step = dx / n;
        float nx = p->x + step;
        if (step != 0 && wall(p, nx, step > 0 ? 1.f : -1.f)) { p->gsp = 0; break; }
        float sy, ang;
        float down = 6.f + fabsf(p->gsp) * 1.5f;
        if (probe2(p, nx, p->y - STEP_UP, STEP_UP + down, 1, &sy, &ang)) {
            p->x = nx; p->y = sy; p->angle = ang;
        } else {
            p->x = nx;
            p->ground = 0;
            p->vx = p->gsp * cosf(a);
            p->vy = -p->gsp * sinf(a);
            return;
        }
    }
}

static void air_move(player_t *p, const input_t *in, int control, int gravity) {
    if (control) {
        if ((in->held & BTN_RIGHT) && p->vx < PHYS_TOP) { p->vx = fminf(p->vx + PHYS_AIR_ACC, PHYS_TOP); p->face = 1; }
        if ((in->held & BTN_LEFT) && p->vx > -PHYS_TOP) { p->vx = fmaxf(p->vx - PHYS_AIR_ACC, -PHYS_TOP); p->face = -1; }
    }
    if (gravity) {
        p->vy += PHYS_GRAV;
        if (p->vy > PHYS_MAXFALL) p->vy = PHYS_MAXFALL;
    }
    /* horizontal */
    int n = (int)(fabsf(p->vx) / 4.f) + 1;
    for (int i = 0; i < n; i++) {
        float nx = p->x + p->vx / n;
        if (p->vx != 0 && wall(p, nx, p->vx > 0 ? 1.f : -1.f)) { p->vx = 0; break; }
        p->x = nx;
        /* moving into rising ground while descending: land on it */
        float sy, ang;
        if (p->vy >= 0 && probe2(p, p->x, p->y - STEP_UP, STEP_UP, 0, &sy, &ang) && sy <= p->y) {
            land(p, sy, ang);
            return;
        }
    }
    /* vertical */
    n = (int)(fabsf(p->vy) / 4.f) + 1;
    for (int i = 0; i < n; i++) {
        float ny = p->y + p->vy / n;
        if (p->vy < 0) {
            if (solid_at(&G.lvl, p->x - 4, ny - BODY_H) || solid_at(&G.lvl, p->x + 4, ny - BODY_H)) {
                p->vy = 0; break;
            }
            p->y = ny;
        } else {
            float sy, ang;
            if (probe2(p, p->x, p->y - 2, (ny - p->y) + 3, 1, &sy, &ang) && sy <= ny + 1) {
                land(p, sy, ang);
                return;
            }
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

void player_update(player_t *p, const input_t *in) {
    p->anim_tick++;
    p->state_t++;
    if (p->invuln) p->invuln--;

    if (p->state == ST_DEAD) {
        p->vy += PHYS_GRAV; p->y += p->vy;
        if (++p->dead_t > 90) {
            player_reset(p, p->spawn_x, p->spawn_y);
        }
        set_anim(p, A_HURT);
        return;
    }

    switch (p->state) {
    case ST_NORMAL:
        if (in->pressed & BTN_ATTACK) { p->state = ST_ATTACK; p->state_t = 0; set_anim(p, A_ATK); snd_play(G.sfx.attack, 200, 128); break; }
        if (in->pressed & BTN_SPECIAL) { p->state = ST_CHARGE; p->state_t = 0; break; }
        if (p->ground) {
            if (in->pressed & BTN_JUMP) { jump(p); break; }
            ground_move(p, in, 1);
        } else {
            if ((in->pressed & BTN_JUMP) && !p->cyclone_used) {
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
        if (p->state_t >= 15) p->state = ST_NORMAL;
        break;
    case ST_CYCLONE:
        if (p->ground) { p->state = ST_NORMAL; break; }
        p->vy = fminf(p->vy, 1.0f);          /* slow descent while spinning */
        air_move(p, in, 1, 1);
        if (p->state == ST_CYCLONE && p->state_t >= 30) p->state = ST_NORMAL;
        break;
    case ST_CHARGE:
        if (p->ground) { p->gsp *= 0.85f; ground_move(p, in, 0); }
        else { p->vy *= 0.8f; p->vx *= 0.9f; air_move(p, in, 0, 1); }
        if (!(in->held & BTN_SPECIAL)) {
            if (p->state_t >= 20) {
                float dx = 0, dy = 0;
                if (in->held & BTN_LEFT) dx = -1;
                if (in->held & BTN_RIGHT) dx = 1;
                if (in->held & BTN_UP) dy = -1;
                if (in->held & BTN_DOWN) dy = 1;
                if (!dx && !dy) dx = (float)p->face;
                float len = sqrtf(dx * dx + dy * dy);
                p->boost_dx = dx / len; p->boost_dy = dy / len;
                if (dx) p->face = dx > 0 ? 1 : -1;
                p->state = ST_BOOST; p->state_t = 0;
                if (p->boost_dy < 0 || !p->boost_dx) p->ground = 0;
                snd_play(G.sfx.boost, 230, 128);
            } else p->state = ST_NORMAL;
        }
        break;
    case ST_BOOST: {
        float ox = p->x, oy = p->y;
        if (p->ground) {            /* along the ground: forced speed, slopes followed */
            p->gsp = p->boost_dx * PHYS_BOOST;
            ground_move(p, in, 0);
        } else {
            p->vx = p->boost_dx * PHYS_BOOST; p->vy = p->boost_dy * PHYS_BOOST;
            air_move(p, in, 0, 0);
        }
        if (p->state_t >= 24 || (p->x == ox && p->y == oy)) {
            p->state = ST_NORMAL;
            if (p->ground) p->gsp = p->boost_dx * PHYS_TOP;
            else { p->vx *= 0.6f; p->vy *= 0.6f; }
        }
        break;
    }
    case ST_HURT:
        air_move(p, in, 0, 1);
        if (p->ground) p->state = ST_NORMAL;
        break;
    }

    /* hazards */
    if (p->state != ST_DEAD) {
        if (spike_at(&G.lvl, p->x, p->y + 1) || spike_at(&G.lvl, p->x, p->y - 4)) {
            if (!p->invuln) { player_hurt(p, p->x - p->face); if (p->state == ST_HURT) p->vy = -5.f; }
        }
        if (p->y > G.lvl.h * TILE + 64) {
            p->hp = 0; p->state = ST_DEAD; p->dead_t = 30; p->vy = 0;
            snd_play(G.sfx.hurt, 220, 128);
        }
    }

    /* animation selection */
    switch (p->state) {
    case ST_ATTACK: set_anim(p, A_ATK); break;
    case ST_CYCLONE: set_anim(p, A_CYC); break;
    case ST_CHARGE: set_anim(p, A_CHG); break;
    case ST_BOOST: set_anim(p, A_BST); break;
    case ST_HURT: set_anim(p, A_HURT); break;
    default:
        if (p->ground) {
            if (fabsf(p->gsp) < 0.2f) set_anim(p, A_IDLE);
            else {
                set_anim(p, A_RUN);
                p->anim_tick += (int)(fabsf(p->gsp) / 3.f); /* faster cycle at speed */
            }
        } else set_anim(p, p->jumping ? A_SPIN : A_FALL);
    }
}

void player_draw(const player_t *p) {
    if (p->invuln && (p->invuln & 4) && p->state != ST_HURT) return;
    uint32_t col = 0xffffffff;
    bank_draw(&G.lilac, p->anim, p->anim_tick, p->x - G.camx, p->y - G.camy + 1, p->face < 0, col);
}

/* damage box the player currently deals with; returns 0 if none */
int player_attack_box(const player_t *p, float *x0, float *y0, float *x1, float *y1) {
    if (!player_attacking(p)) return 0;
    if (p->state == ST_ATTACK) {
        if (p->state_t > 12) return 0;
        float fx = p->x + p->face * 6;
        *x0 = p->face > 0 ? fx : fx - 30; *x1 = *x0 + 30;
        *y0 = p->y - 30; *y1 = p->y - 4;
        return 1;
    }
    float r = p->state == ST_CYCLONE ? 20.f : 14.f;
    *x0 = p->x - r; *x1 = p->x + r; *y0 = p->y - 16 - r; *y1 = p->y - 16 + r;
    return 1;
}
