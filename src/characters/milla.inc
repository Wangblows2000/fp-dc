/* Milla (MV.A = 4), included by player.c after carol_bike.inc.
 * Like Carol she runs on Lilac's shared movement block, speed rules and shared state groups (Milla uses Lilac's movement
 * table 1543: C 5, D 15, E 0.07375, I 0.25); only her own events are here, in the original's in-frame order.
 * Sources: build/re/spec_milla_moves.md (float M22, dig M27, animation), build/re/spec_milla_combat.md (the cube /
 * shield object PlayerReflect_463 and states M20 / M30 / M31); RV = original traces build/orig_traces/milla
 * (tests/orig_milla, compared by tools/orig/cmp_milla.py). Event numbers are the original's (Frames::event_func_N).
 *
 * Per frame (port order = original order):
 *   test_collisions_20 (the cube against obstacles / background: milla_cube_collide) -> movement block and speed rules
 *   (switch in player_update) -> 1664 energy / 1665 float meter -> state groups M0 / M1 / M2 / M3 / M40 (milla_normal)
 *   -> Milla group 0x21a67 (2197-2240) -> M20 0x21a45 -> M22 0x21a37 -> M27 0x21a4f -> M30 0x21a35 -> M31 0x21a34
 *   (milla_groups) -> 2414 damage -> facing / animation (milla_anim).  A state set during frame f runs its group from
 *   frame f + 1 (groups are rebuilt at 1680-1721 before the state groups). */

static int is_milla(void) { return chr_is_milla(G.character); }

/* ---- the cube: PlayerReflect_463, one reused instance (spec_milla_combat 1, 1.1). A: 0 parked, 1 held, 2 flying,
 * 3 burst, 4 shield, 5 shield burst / beam, 6 cool-down. C: aim angle, G: facing at the last shield frame, H: damage
 * term, X / Y velocity. Its animations are in the millafx bank (G.bike) as a<index>. */
typedef struct { int A, anim, vis, dir, once416, playing; double B, C, D, E, F, H, X, Y; int Gq; float x, y, ang; animator_t an; int flip; } mcube_t;
static mcube_t cube;
static int m_snd422 = -1;        /* the charge sound is playing (2237 / 2238 / 2209 stop it) */
static int m_hud;                 /* GUIScoreTimeRings frame (2197-2201) */
static int m_m0g;                 /* the grounded M0 group ran this frame */

static int mfx_anim(int idx, int left, int *flip) {
    char nm[16]; int a;
    *flip = 0;
    if (!G.bike.nanims) return -1;
    if (left) { snprintf(nm, sizeof nm, "a%d_l", idx); if ((a = bank_anim(&G.bike, nm)) >= 0) return a; *flip = 1; }
    snprintf(nm, sizeof nm, "a%d", idx); return bank_anim(&G.bike, nm);
}
static void cube_set_anim(int idx, int restart) {     /* Chowdren set_animation: a new index restarts, the same one keeps playing */
    int fl, a = mfx_anim(idx, cube.dir >= 8 && cube.dir < 24, &fl);
    if (a < 0) return;
    if (cube.anim != idx || restart || cube.an.anim != a) { int acc = cube.an.acc; anim_set(&cube.an, &G.bike, a); anim_restart(&cube.an); cube.an.acc = acc; }   /* RV m_super_ground: the frame counter carries over */
    cube.anim = idx; cube.flip = fl; cube.playing = 1;
}
/* "animation N playing" (Chowdren): N is the current animation and it has not finished */
static int cube_playing(int idx) { return cube.anim == idx && cube.playing; }
static void cube_tick(void) {               /* update_objects: the cube's animation advances before the events */
    if (!cube.an.b) return;
    int acc0 = cube.an.acc, done0 = cube.an.done;
    anim_tick(&cube.an, -1);
    if (cube.an.done && !done0) cube.an.acc = acc0;   /* RV m_super_ground: the finishing update leaves the counter as it was */
    if (cube.an.done) cube.playing = 0;          /* RV m_shield_tap: a finished one-shot keeps its id and last frame, "playing" is false */
}
static void milla_cube_reset(void) {
    memset(&cube, 0, sizeof cube); cube.anim = -1; cube.H = 5; m_snd422 = -1; m_hud = 0;
    cube_set_anim(0, 1);
}
void player_cube_reset(void) { milla_cube_reset(); }
static int m_cur_idx = -1, m_cur_dir = -1;
static void milla_reset_state(void) { milla_cube_reset(); m_cur_idx = -1; m_cur_dir = -1; }

/* reference point Player_326: (int X, int Y) */
static void m_pxy(const player_t *p, double *x, double *y) { *x = floor(p->x); *y = floor(p->y - ORIG_REF_DY); }
/* MILLA's current image action point in the level (bank ACT1; flip-aware) */
static void m_action_point(const player_t *p, double *ax, double *ay) {
    const frame_t *f = anim_frame(&p->an);
    double x = floor(p->x), y = floor(p->y - ORIG_REF_DY);
    if (!f || !p->an.b->act) { *ax = x; *ay = y; return; }
    int i = (int)(f - p->an.b->frames);
    int axl = p->an.b->act[2 * i], ayl = p->an.b->act[2 * i + 1];
    *ax = draw_flip ? x + f->hx - axl : x - f->hx + axl;   /* mirrored art: hotspot w - 1 - hx, action point w - 1 - ax */
    *ay = y - f->hy + ayl;
}

/* a whitebursts (EffectsWhiteBurst_31) */
static void m_wburst(float x, float y) { c_wburst(x, y); }

/* ---- state entries ---- */
static void m_start_attack(player_t *p) { set_state(p, ST_MATK); p->springing = 0; }                     /* 1780 / 1803 / 1819 / 1900: M = 20 */
static void m_start_shield(player_t *p) { set_state(p, ST_MSHIELD); p->springing = 0; }                  /* 1781 / 1810 / 1821 / 1838 / 1884 / 1914 */
static void m_start_float(player_t *p) { set_state(p, ST_FLOAT); p->mN = 0; p->springing = 0; }        /* 1782 / 1811 / 1915: M = 22, OTHERS.N = 0 */
static int m_can_shield(const player_t *p, const input_t *in) { return (in->held & BTN_SPECIAL) && p->mF <= 0 && cube.A == 0; }
static int m_can_float(const player_t *p, const input_t *in) { return (in->held & BTN_JUMP) && p->vy >= 0 && p->mE < 100; }
static void m_jump(player_t *p) { jump(p); p->m1 = 1; }                               /* 1750 / 1817 / 2277: M = 1, PAV_A.C = 1 */

/* ---- ST_NORMAL: M0 (ground / air), M1, M2 look up, M3 crouch, M40 springs ---- */
static void milla_normal(player_t *p, const input_t *in, int *crouch_look) {
    int jp = (in->pressed & BTN_JUMP) != 0, ap = (in->pressed & BTN_ATTACK) != 0;
    int was_ground = p->ground;
    int cm = p->m1 ? 1 : !p->ground ? 0 : p->look_prev == 2 ? 2 : p->look_prev == 3 ? 3 : 0;
    int lr = (in->held & (BTN_LEFT | BTN_RIGHT)) != 0;
    int up = (in->held & BTN_UP) && !(in->held & BTN_DOWN), dn = (in->held & BTN_DOWN) && !(in->held & BTN_UP);
    if (p->ground) ground_move(p, in, cm != 2 && cm != 3, 1);
    else air_move(p, in, 1, PHYS_GRAV);
    int landed = !was_ground && p->ground;
    if (p->ground && p->mE > 0) p->mE = 0;                                          /* 1665 */

    if (cm == 0 && p->ground) {                                                     /* ---- M0 group, grounded ---- */
        double B = fabs(p->gsp);
        m_m0g = 1;                                                                  /* 1723-1725 run after the edge clamp (milla_groups) */
        if (dn && !lr && B < PHYS_LOWSPD) { p->gsp = 0; p->look = 3; *crouch_look = 1; return; }   /* 1743 */
        if (up && !lr && B < 0.01) { p->look = 2; *crouch_look = 1; return; }                       /* 1742 */
        if (jp) { set_state(p, ST_NORMAL); m_jump(p); return; }                                    /* 1750 (group off) */
        if (p->state == ST_NORMAL && push_wants(p, in)) { set_state(p, ST_PUSH); p->face = (in->held & BTN_RIGHT) ? 1 : -1; }   /* 1752 / 1753 */
        if (p->water && (in->held & BTN_JUMP) && p->vy > -3) { set_state(p, ST_NORMAL); m_jump(p); return; }   /* 1758 */
        if (ap && p->state != ST_MATK) m_start_attack(p);                           /* 1780 */
        if (m_can_shield(p, in)) m_start_shield(p);                                 /* 1781 */
        return;
    }
    if (cm == 0) {                                                                  /* ---- M0 airborne / M40 ---- */
        if (p->springing) {                                                         /* M40 group (spring launch) */
            if (p->cY != 24) p->cY = 40;
            p->mE = 0;                                                              /* 1898 */
            if (ap && p->state != ST_MATK) m_start_attack(p);                       /* 1900 (INFERRED as 1780) */
            if (m_can_shield(p, in)) m_start_shield(p);                             /* 1914 */
            else if (m_can_float(p, in) && !(in->held & BTN_DOWN) && p->mF <= 0) m_start_float(p);   /* 1915 */
            return;
        }
        if (ap && p->state != ST_MATK) m_start_attack(p);                           /* 1780 (airborne M0) */
        if (m_can_shield(p, in)) m_start_shield(p);                                 /* 1781 */
        else if (!p->water && m_can_float(p, in)) m_start_float(p);                 /* 1782 */
        return;
    }
    if (cm == 1) {                                                                  /* ---- M1 group ---- */
        if (p->cY <= 2 || (p->cY >= 4 && p->cY <= 7) || p->cY == 40) p->cY = 3;   /* M1: Y 0-2 / 4-7 / 40 -> 3 */
        if (p->jumping && !p->ground && !(in->held & BTN_JUMP) && p->vy < -PHYS_JUMP_CUT) p->vy = -PHYS_JUMP_CUT;   /* 1801 */
        if (p->water && !p->ground && (in->held & (BTN_UP | BTN_JUMP)) && p->vy > -3) {                            /* 1802 swim */
            p->vy = fmin(fmax(p->vy - 0.2, -3.), -1.); if (p->cY != 26) p->mE = 0; p->cY = 26;
        }
        if (landed) { p->m1 = 0; p->jumping = 0; return; }                         /* 1807 landing (M1 off; no sound for Milla) */
        if (ap && p->energy > 0) { set_state(p, ST_MATK); p->m1 = 0; return; }     /* 1803 (M1 off) */
        if (m_can_shield(p, in)) { m_start_shield(p); p->m1 = 0; }                  /* 1810 */
        else if (m_can_float(p, in)) { m_start_float(p); p->m1 = 0; }               /* 1811 */
        return;
    }
    if (cm == 2) {                                                                  /* ---- M2 look up ---- */
        p->cY = 4;
        if (jp && p->ground) { m_jump(p); return; }                                 /* 1817 */
        if (ap && p->energy > 0) { m_start_attack(p); return; }                     /* 1819 */
        if (m_can_shield(p, in)) { m_start_shield(p); return; }                     /* 1821 */
        if (up && !lr && p->ground) { p->look = 2; *crouch_look = 1; }
        return;
    }
    /* ---- M3 crouch ---- */
    p->cY = 5;
    if (m_can_shield(p, in)) { m_start_shield(p); return; }                         /* 1838 */
    if (ap && p->mD == 1) {                                                         /* 1839: dig (needs a dig zone: MILLA.D = 1) */
        set_state(p, ST_DIG); p->mfl1 = 0; p->mA = 20; p->mB = 0; return;
    }
    if (dn && !lr && p->ground) { p->look = 3; *crouch_look = 1; }
}

/* ---- movement of Milla's own states (the shared movement block + speed rules) ---- */
static void milla_move(player_t *p, const input_t *in) {
    switch (p->state) {
    case ST_FLOAT:                         /* S = 1 (2250), air control, gravity then 2251; V = 0 from the 2nd float frame */
        if (p->ground) { ground_move(p, in, 1, 1); break; }
        air_move(p, in, 1, PHYS_GRAV);
        break;
    case ST_MSHIELD:                       /* RV m_shield_walk / m_shield_decay: no D-pad acceleration, shared friction applies */
        if (p->ground) ground_move(p, in, 0, 1); else air_move(p, in, 0, PHYS_GRAV);
        break;
    case ST_DIG: case ST_MATK: case ST_MCARRY: default:
        if (p->ground) ground_move(p, in, 1, 1);
        else {
            air_move(p, in, 1, PHYS_GRAV);
            if (p->state == ST_MCARRY && p->jumping && !p->ground && !(in->held & BTN_JUMP) && p->vy < -PHYS_JUMP_CUT) p->vy = -PHYS_JUMP_CUT;
        }
        break;
    }
    if (p->ground && p->mE > 0) p->mE = 0;                                          /* 1665 */
}

/* ---- the cube's collision handlers (test_collisions_20, at the start of the frame): col_4_13 (obstacle / solid
 * objects) and col_14_13 (background): a flying cube bursts ---- */
static obj_t cube_obj(void) {
    obj_t o; memset(&o, 0, sizeof o);
    o.alive = 1; o.x = cube.x; o.y = cube.y; o.an = cube.an; o.flip = cube.flip; o.ang = cube.ang;
    return o;
}
static void milla_cube_collide(void) {
    if (cube.A != 2 || !cube.an.b) return;
    obj_t c = cube_obj();
    int hit = obj_overlaps_bg(&c);
    for (int i = 0; i < G.nobjs && !hit; i++) {
        const obj_t *s = &G.o[i];
        if (!s->alive || !s->an.b) continue;
        if ((obj_in_group(s->type, OBJGRP_SOLID) || obj_in_group(s->type, OBJGRP_OBSTACLE)) && obj_obj_hit(&c, s)) hit = 1;
    }
    if (hit) { cube.A = 3; m_wburst(cube.x, cube.y); }
}
/* exported for objects.c / combat.c: the cube's current hitbox against an object (rotated images: per pixel) */
static int cube_hits_obj(const obj_t *e) {
    if (!cube.vis && cube.A != 3) return 0;
    if (!cube.an.b || !e->an.b) return 0;
    obj_t c = cube_obj();
    if (c.ang == 0) return obj_obj_hit(&c, e);
    const frame_t *f = anim_frame(&c.an);
    double a = fmod((double)c.ang, 360.0) / 57.29577951308232, co = fd_cos(a), sn = fd_sin(a);
    int hx = c.flip ? f->w - f->hx : f->hx, ox = (int)floorf(c.x), oy = (int)floorf(c.y);
    for (int y = 0; y < f->h; y += 1)
        for (int x = 0; x < f->w; x += 1) {
            if (!frame_mask_px(c.an.b, f, c.flip ? f->w - 1 - x : x, y)) continue;
            double lx = x + 0.5 - hx, ly = y + 0.5 - f->hy;
            int wx = ox + (int)floor(lx * co + ly * sn), wy = oy + (int)floor(-lx * sn + ly * co);
            if (obj_rect_hit(e, wx, wy, wx + 1, wy + 1)) return 1;
        }
    return 0;
}
int player_cube_state(void) { return is_milla() ? cube.A : -1; }
int player_cube_hits(const obj_t *e) { return is_milla() ? cube_hits_obj(e) : 0; }
/* rocks (4761 / 4762 / 5369): a bursting cube (A = 3) or the beam (anim 5) breaks them */
int player_cube_breaks_rock(const obj_t *o) {
    if (!is_milla()) return 0;
    if (!((cube.A == 3) || (cube.A == 5 && cube_playing(5)))) return 0;
    return cube_hits_obj(o);
}
/* monitors: a flying cube (2618: A = 3) or the shield burst / beam (2619) opens them */
int player_cube_opens_monitor(const obj_t *o) {
    if (!is_milla()) return 0;
    if (cube.A == 2 && cube_hits_obj(o)) { cube.A = 3; return 1; }
    if (cube.A == 5 && cube_hits_obj(o)) return 1;
    return 0;
}
/* hits on enemies (2742-2745), called from combat_frame after 2741: returns the damage term and fills the knock-back */
double player_cube_damage(double *kx, double *ky, int *armour_only, int Q) {
    extern int fe_difficulty;
    *armour_only = 0;
    int d = fe_difficulty < 1 ? 1 : fe_difficulty;     /* DIFF: 0 Hard, 1 Normal, 2 Easy, 3 Casual; max(DIFF, 1) */
    if (cube.A == 3 && cube.F < 15) { *kx = 0.75 * cube.X; *ky = 0.75 * cube.Y; return (cube.H - 1) + d; }
    if (cube.A == 5 && cube.F < 3) {
        double c = cube.C / 57.29577951308232;
        *kx = 5 * fd_cos(c) * Q; *ky = -5 * fd_sin(c) * Q; return (cube.H - 1) + d;
    }
    if (cube.A == 5 && cube.F < 15) *armour_only = 1;  /* 2745 needs F < 15 */
    return 0;
}

/* ---- Milla group 0x21a67 (2197-2240), every frame ---- */
static void milla_group_main(player_t *p, const input_t *in) {
    int ah = (in->held & BTN_ATTACK) != 0, ap = (in->pressed & BTN_ATTACK) != 0, sh = (in->held & BTN_SPECIAL) != 0;
    double px, py; m_pxy(p, &px, &py);
    int Q = p->face > 0 ? 1 : -1;
    double B = p->ground ? p->gsp : p->vx, O = p->O;
    int M = player_orig_m(p);
    /* 2197-2201: HUD panel frame (GT / 4) mod 2 + 0 / 2 / 4 / 8 / 6 */
    m_hud = (G.frame / 4) % 2;
    if ((p->mF == 1) || (p->mF == 2 && cube.A == 1)) m_hud = (G.frame / 4) % 2 + 4;         /* 2199 */
    if (p->mF == 2 && cube.A == 0) m_hud = (G.frame / 4) % 2 + 8;                            /* 2200 */
    if (M == 3 || M == 27) m_hud = (G.frame / 4) % 2 + 6;                                    /* 2201 */
    if (p->state == ST_NORMAL && !p->m1 && p->mF > 0) set_state(p, ST_MCARRY);               /* 2203: M = 0 and F > 0 -> M30 */
    if (cube.A == 0) { cube.vis = 0; cube.F = 0; cube.X = cube.Y = 0; }                      /* 2206 */
    if (cube.A == 0 && p->mF == 0) p->mG = 0;                                                /* 2207 */
    if (cube.A == 0 && p->mF == 2) {                                                         /* 2208: forming sparkle (anim 7) */
        cube_set_anim(7, 0); cube.vis = 1;
        double ax, ay, a = p->rot / 57.29577951308232; m_action_point(p, &ax, &ay);
        cube.x = (float)(int)(ax + fd_sin(a) * -15); cube.y = (float)(int)(ay + fd_cos(a) * -15); cube.ang = (float)p->rot;
    }
    if (p->mF == 2 && p->mG == 40 && cube.A == 0) {                                          /* 2209: the cube appears */
        cube.A = 1; cube_set_anim(0, 1); cube_set_anim(10, 1);
        snd_play_orig(415, 220, 128);
    }
    if (p->state != ST_MCARRY && p->mF == 2 && !ah && p->mG < 40) { set_state(p, ST_NORMAL); p->m1 = 0; p->mF = 0; p->mC4 = 0; }   /* 2210 */
    if (p->mF == 0 && cube.A == 1) cube.A = 2;                                               /* 2211 */
    if (cube.A == 1) {                                                                       /* 2212: held at MILLA's action point */
        double ax, ay, a = p->rot / 57.29577951308232; m_action_point(p, &ax, &ay);
        cube.vis = 1; cube.x = (float)(int)(ax + fd_sin(a) * -15); cube.y = (float)(int)(ay + fd_cos(a) * -15); cube.ang = (float)p->rot;
        cube.X = o_cos(O) * (3 * Q + B); cube.Y = -o_sin(O) * (3 * Q + B);
    }
    if (sh && cube.A == 1) {                                                                 /* 2213: super shield (any state) */
        set_state(p, ST_MSHIELD); p->m1 = 0;
        cube.A = 4; cube.F = 0; cube.D = px; cube.E = py; cube_set_anim(2, 1); cube.vis = 0;
        snd_play_orig(421, 220, 128);
    }
    if (cube.A == 2) {                                                                       /* 2214: flight */
        cube.x = (float)(int)(cube.x + cube.X); cube.y = (float)(int)(cube.y + cube.Y);
        cube.Y = fmin(cube.Y + 0.25, 15.0); cube.vis = 1; if (!cube_playing(10)) cube_set_anim(0, 0);   /* RV m_turr_throw: anim 0 in flight */
        if (cube.x < 0 || cube.x > G.lvl.w || cube.y < 0 || cube.y > G.lvl.h) cube.A = 0;   /* 2215 */
    }
    if (cube.A == 2) {                                                                       /* 2216: an enemy */
        obj_t c = cube_obj();
        for (int i = 0; i < G.nobjs; i++) { const obj_t *e = &G.o[i]; if (e->alive && enemy_type(e->type) && e->an.b && obj_obj_hit(&c, e)) { cube.A = 3; m_wburst(cube.x, cube.y); break; } }
    }
    if (cube.A == 3 && !cube.once416) { cube.once416 = 1; snd_play_orig(416, 220, 128); }  /* 2217 */
    if (cube.A != 3) cube.once416 = 0;
    if (cube.A == 3 && G.frame % 5 == 0) {                                                   /* 2218 */
        float bx = cube.x + fx_rand(21) - 10, by = cube.y + fx_rand(21) - 10; m_wburst(bx, by);
    }
    if (cube.A == 3) { cube.F += 1; cube.vis = 0; cube_set_anim(9, 0); }                    /* 2219 */
    if (cube.A == 3 && cube.F > 20) { cube.F = 0; cube.A = 0; cube_set_anim(0, 1); }       /* 2220 */
    if (cube.A == 4 && !cube_playing(2)) cube_set_anim(11, 0);                             /* 2221 */
    if (cube.A == 4 && p->state != ST_MSHIELD) cube.A = 0;                                  /* 2222 */
    if (cube.A == 5 && !cube_playing(5) && cube.anim != 8) cube_set_anim(8, 1);             /* 2223 */
    if (cube.A == 5 && cube_playing(5)) { cube.F = 0; cube.H = 6; }                         /* 2224 */
    if (cube.A == 5) {                                                                       /* 2225: follows Milla */
        cube.x = (float)(int)(px + o_sin(O) * -12); cube.y = (float)(int)(py + o_cos(O) * -12); cube.F += 0.5;
    }
    if (cube.A == 5 && !cube_playing(8) && !cube_playing(5)) { cube.A = 6; cube.F = 0; cube.vis = 0; }   /* 2226 */
    if (cube.A == 6) cube.F += 1;                                                            /* 2227 */
    if (cube.A == 6 && cube.F > 10) { cube.A = 0; cube.F = 0; }                             /* 2228 */
    if (cube.A != 5 && cube.H > 4) cube.H = 4;                                               /* 2229 */
    if (ap && p->mF == 2 && cube.A == 1) { cube.X = o_cos(O) * (5 * Q + B); cube.Y = -o_sin(O) * (5 * Q + B); }   /* 2231 */
    if (ap && p->mF == 2 && p->mG >= 40) {                                                   /* 2232: throw */
        p->mF = 0; p->mH = 2; snd_play_orig(417, 220, 128); p->mG = 0;
        if (p->state != ST_MSHIELD) { set_state(p, ST_NORMAL); p->m1 = 0; }
    }
    if (p->mF == 0 && p->mC4) p->mC4 = 0;                                                    /* 2234: C back to 5 */
    if (p->mF > 0 && ((M >= 6 && M <= 12) || (M >= 41 && M <= 46) || M == 25 || p->cY == 8 || p->cY == 12 || p->cY == 48)) {   /* 2235: drop */
        p->mG = 0; p->mF = 0; p->mH = 0;
    }
    if (p->mF == 2 && ah && p->mG < 40) p->mG++;                                             /* 2236 */
    if (p->mG == 1 && m_snd422 < 0) { snd_play_orig(422, 220, 128); m_snd422 = 1; }          /* 2237 (once at G = 1) */
    if (p->mG != 1) m_snd422 = -1;                                                            /* 2238: the port's one-shot channels cannot be stopped */
    if (p->mH > 0) p->mH--;                                                                  /* 2239 */
    if (p->mfl10 && p->state != ST_FLOAT) { p->mfl10 = 0; p->nol = 0; }                     /* 2240 */
}

/* ---- M20 group 0x21a45 (one frame) ---- */
static void milla_group_m20(player_t *p, const input_t *in) {
    p->boost_n = 0;                                                                          /* 2241 OTHERS.M = 0 (idle) */
    if (p->water && !p->ground && (in->held & (BTN_UP | BTN_JUMP)) && p->vy > -3) { p->vy = fmin(fmax(p->vy - 0.2, -3.), -1.); p->cY = 26; }   /* 2242 */
    if (p->mH <= 0) { set_state(p, ST_MCARRY); p->mF = 2; }                                  /* 2243 */
    else { set_state(p, ST_NORMAL); p->m1 = 0; p->mF = 0; }                                 /* 2244 */
    if (cube.A > 1) { set_state(p, ST_NORMAL); p->m1 = 0; }                                 /* 2245 (F stays 2: cleared by 2210 next frame) */
}

/* ---- M22 Puppy Float group 0x21a37 ---- */
static int m_flap_t;   /* 2249 sound timer (frames; 0.1 s = 6 frames at 60 Hz, INFERRED) */
static void milla_group_float(player_t *p, const input_t *in) {
    p->cY = 14;                                                                              /* 2247 */
    if (p->mN == 1) { p->mN = 2; p->mE += 20; snd_play_orig(244, 255, 128); m_flap_t = 0; } /* 2248 (loop_sound_attack0: Milla's 244) */
    if (++m_flap_t >= 6) { m_flap_t = 0; if (p->mN > 15) snd_play_orig(170, 200, 128); }    /* 2249 */
    p->mN++;                                                                                 /* 2250 */
    p->skinI = fmax(0., 80 - 2.0 * p->mN);
    if (p->ground) { p->vx = p->gsp; p->vy = 0; p->ground = 0; }                            /* L = 0 */
    p->c_nospd = 0; p->mE += 0.9; p->nol = 1; p->mfl10 = 1;
    if (p->vy > -2) p->vy = fmax(p->vy - 0.5, -2.);                                          /* 2251 */
    if (p->mE >= 100 || !(in->held & BTN_JUMP)) {                                            /* 2252 / 2253: end */
        set_state(p, ST_NORMAL); p->m1 = 0; p->cY = 3; p->nol = 0; p->c_fall = 1;
    }
}

/* ---- M27 dig group 0x21a4f (only where MILLA.D = 1: dig zones of other stages; spec_milla_moves 3.3) ---- */
static struct { int on, vis, frame, once185, once184, pasted; float x, y; } mdig;
static void milla_group_dig(player_t *p, const input_t *in) {
    int Q = p->face > 0 ? 1 : -1;
    p->cY = 15;                                                                              /* 2254 */
    int fr = p->an.frame;
    if (fr == 0 && p->mA > 25 && !p->mfl1) { if (!mdig.once185) { mdig.once185 = 1; snd_play_orig(185, 220, 128); mdig.x = (float)(floor(p->x) + 12 * Q); mdig.y = (float)floor(p->y - ORIG_REF_DY); mdig.frame = (int)(p->mB / 24); mdig.vis = 1; p->mC = fx_rand(2); } } else mdig.once185 = 0;   /* 2255 */
    if (fr == 3 && p->mA > 25 && !p->mfl1) { if (!mdig.once184) { mdig.once184 = 1; snd_play_orig(184, 220, 128); mdig.x = (float)(floor(p->x) + 12 * Q); mdig.y = (float)floor(p->y - ORIG_REF_DY); mdig.frame = (int)(p->mB / 24); mdig.vis = 1; p->mC = fx_rand(2); } } else mdig.once184 = 0;   /* 2256 */
    p->mA -= 1;                                                                              /* 2258 */
    if (p->mB > 5) mdig.frame = (int)fmin(p->mB / 24, 4);                                    /* 2259 */
    if ((in->pressed & BTN_ATTACK) && !p->mfl1) p->mA = fmin(p->mA + 20, 50);                /* 2260 */
    if (((p->O > 5 && p->O < 355) || p->O < -5) && !p->mfl1) { p->mfl1 = 1; p->mC = 1; }   /* 2261 (a), (b) */
    if (p->mA > 25) p->mB += 1;                                                              /* 2262 */
    if (p->mB > 120) p->mfl1 = 1;                                                            /* 2263 */
    if (p->mA <= 0 && p->mfl1) snd_play_orig(183, 220, 128);                                 /* 2265 */
    if (p->mA <= 0) { set_state(p, ST_NORMAL); p->look = 3; p->look_prev = 3; mdig.pasted = 1; }   /* 2267: back to the crouch */
    if ((p->ground ? p->gsp : p->vx) != 0 || !p->ground) { set_state(p, ST_NORMAL); p->m1 = 0; mdig.pasted = 1; }   /* 2268 */
}

/* ---- M30 carry group 0x21a35 ---- */
static void milla_group_carry(player_t *p, const input_t *in) {
    double B = fabs(p->ground ? p->gsp : p->vx);
    if (p->ground) { if (B == 0) p->cY = 0; else p->cY = B < 5 ? 1 : 2; }                  /* 2269-2271 */
    if (p->cY == 13) p->cY = 0;                                                              /* 2272 */
    if (p->water && !p->ground && (in->held & (BTN_UP | BTN_JUMP)) && p->vy > -3) { p->vy = fmin(fmax(p->vy - 0.2, -3.), -1.); p->cY = 26; }   /* 2273 */
    if (p->mF == 0) p->mF = 1;                                                               /* 2274 */
    p->mC4 = 1; p->mE = 100;                                                                 /* 2275: C = 4, E = 100 */
    if (p->mF == 2 && !(in->held & BTN_ATTACK) && p->mG < 40) { set_state(p, ST_NORMAL); p->m1 = 0; p->mF = 0; p->mC4 = 0; }   /* 2276 */
    if (p->ground && (in->pressed & BTN_JUMP) && p->state == ST_MCARRY) { m_jump(p); set_state(p, ST_NORMAL); p->m1 = 1; }   /* 2277: jump, M = 1 (F kept; 2203 returns to M30 after the landing) */
}

/* ---- M31 shield group 0x21a34 ---- */
static int m_aim;   /* MILLA frame within anim 25: 4(1 - L) + 0 / 1 / 2 */
static void milla_group_shield(player_t *p, const input_t *in) {
    double px, py; m_pxy(p, &px, &py);
    int L = p->ground, up = (in->held & BTN_UP) != 0, dn = (in->held & BTN_DOWN) != 0;
    double *b = p->ground ? &p->gsp : &p->vx;
    if (cube.A != 4 && cube.A != 5 && cube.A != 6) {                                         /* 2278 */
        cube.A = 4; cube.F = 0; cube.E = cube.y; cube_set_anim(11, 1); snd_play_orig(420, 220, 128);
    }
    if (L && *b > 0) *b = fmax(*b - 0.2, 0.); else if (L && *b < 0) *b = fmin(*b + 0.2, 0.);   /* 2279 / 2280 */
    if (L && *b > -2 && *b < 2) *b = 0;                                                      /* 2281 */
    int Q = p->face > 0 ? 1 : -1;
    if (cube.A == 4) {
        if (up == dn) { cube.C = 0; m_aim = 4 * (1 - L) + 0; }                               /* 2282 */
        else if (up) { cube.C = 45 * Q; m_aim = 4 * (1 - L) + 1; }                           /* 2283 */
        else { cube.C = -45 * Q; m_aim = 4 * (1 - L) + 2; }                                  /* 2284 */
        p->cY = 20;                                                                          /* 2285 */
        cube.dir = Q > 0 ? 0 : 16; cube.C += p->rot; cube.ang = (float)fmod(cube.C + 720, 360);
        cube.x = (float)(int)(px + o_sin(p->O) * -12); cube.y = (float)(int)(py + o_cos(p->O) * -12); cube.vis = 1; cube.Gq = Q;
    }
    if (!(in->held & BTN_SPECIAL) && cube.A == 4) {
        int super = cube_playing(2);
        if (!super) {                                                                        /* 2286: shield burst */
            cube.A = 5; cube.D = cube.x; cube.E = cube.y; cube.B = *b; snd_play_orig(418, 220, 128); p->mG = 0; p->mF = 0;
        } else {
            if (m_aim == 4 * (1 - L) + 1) { if (!p->ground) p->vy = fmax(p->vy + 5, 5.); else p->vy = 5; }     /* 2287: aimed up (pushed down) */
            if (m_aim == 4 * (1 - L) + 2) {                                                                      /* 2288: aimed down (pushed up) */
                if (p->ground) { p->vx = p->gsp; p->vy = 0; p->ground = 0; }
                p->vy = fmin(p->vy - 5, -5.); p->N = 0; p->jumping = 1;
            }
            cube.A = 5; cube.D = cube.x; cube.E = cube.y; cube.B = p->ground ? p->gsp : p->vx;   /* 2289: the beam + recoil */
            snd_play_orig(419, 220, 128); p->mG = 0; p->mF = 0; cube_set_anim(5, 1);
            if (G.cam.Z < 4) G.cam.Z = 4;
            p->mE = 0; p->face = cube.Gq;
            if (p->ground) p->gsp -= 7 * cube.Gq; else p->vx -= 7 * cube.Gq;
        }
    }
    if (cube.A == 6) {                                                                       /* 2290 / 2291: exit */
        if (!(in->held & BTN_ATTACK)) { set_state(p, ST_NORMAL); p->m1 = !p->ground; p->cY = 40 - 40 * p->ground; }
        else { set_state(p, ST_MATK); p->cY = 40 - 40 * p->ground; }
    }
    if (cube.A == 4 && (in->held & BTN_RIGHT)) p->face = 1;                                  /* 2292 */
    if (cube.A == 4 && (in->held & BTN_LEFT)) p->face = -1;                                  /* 2293 */
}

static void milla_groups(player_t *p, const input_t *in, int st0) {
    if (m_m0g) {                                                                             /* 1723-1725 (after the edge clamp: RV m_float_left) */
        m_m0g = 0;
        if (p->state == ST_NORMAL && p->ground && !p->m1 && !p->look) {
            double B = fabs(p->gsp);
            if (B == 0) { if (p->cY != 48 && p->cY != 12) p->cY = 0; } else p->cY = B < 5 ? 1 : 2;
            if (p->skidding && p->c_skid0) p->cY = 6;                                        /* M4 group: OTHERS.Y 6 from its 2nd frame */
        }
    }
    milla_group_main(p, in);
    /* a state group runs when its flag is on (M at 1680-1721 = st0) and, RE (e.g. event 2274), every event of it also
     * tests the current M: an earlier event that changed M this frame stops the group */
    if (st0 == ST_MATK && p->state == ST_MATK) milla_group_m20(p, in);
    if (st0 == ST_FLOAT && p->state == ST_FLOAT) milla_group_float(p, in);
    if (st0 == ST_DIG && p->state == ST_DIG) milla_group_dig(p, in);
    if (st0 == ST_MCARRY && p->state == ST_MCARRY) milla_group_carry(p, in);
    if (st0 == ST_MSHIELD && p->state == ST_MSHIELD) milla_group_shield(p, in);
    if (p->state == ST_HURT && st0 == ST_HURT) p->cY = 9;
    if (p->state == ST_PUSH && st0 == ST_PUSH) p->cY = 13;
    if (p->state == ST_ROLL && st0 == ST_ROLL) p->cY = 8;
    if (!p->ground && p->cY < 3 && p->vy > 0 && p->state == ST_NORMAL) p->cY = 40;         /* 2432 */
}

/* ---- animation: OTHERS.Y -> MILLA animation index (2426-2470), direction 0 / 16 minus MILLA.F, speeds 2490-2500 ---- */
static const signed char MILLA_IDX[52] = {
    0, 1, 2, 24, 11, 10, 16, 13, 7, 8, 38, 5, 20, 12, 18, 26, 22, -1, -1, 22, 25, 26, 29, 30, 28, 27, 39, 9, 36, 35,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, 15, 17, 20, 21, 32, 31, 33, 37, -1, 40, 41 };
static int m_anim_cache[42][32];   /* bank anim per (index, direction); -2 unknown */
static int m_cache_key_n = -1; static const void *m_cache_key;
static int m_lookup(int idx, int d) {
    int *c = &m_anim_cache[idx][d];
    if (*c == -2) {
        char nm[20];
        if (d == 0) snprintf(nm, sizeof nm, "a%d", idx); else if (d == 16) snprintf(nm, sizeof nm, "a%d_l", idx); else snprintf(nm, sizeof nm, "a%d_d%d", idx, d);
        *c = bank_anim(&G.lilac, nm);
    }
    return *c;
}
static int m_res_dir;   /* the direction actually shown (traces) */
static int m_has_dir(int idx, int d, int *flip) {   /* direction d exists as its own art, or as the mirror of d - 16 */
    int a = m_lookup(idx, d); *flip = 0;
    if (a >= 0) return a;
    if (d >= 16 && (a = m_lookup(idx, d - 16)) >= 0) { *flip = 1; return a; }
    return -1;
}
static int milla_bank_anim(int idx, int dir, int *flip) {
    if (m_cache_key != (const void *)G.lilac.anims) { m_cache_key = G.lilac.anims; for (int i = 0; i < 42; i++) for (int d = 0; d < 32; d++) m_anim_cache[i][d] = -2; }
    (void)m_cache_key_n;
    if (idx < 0 || idx >= 42) return -1;
    /* Chowdren: a direction without art uses the nearest direction that has some */
    for (int k = 0; k <= 16; k++)
        for (int sgn = 1; sgn >= -1; sgn -= 2) {
            int dd = ((dir + sgn * k) % 32 + 32) % 32, a = m_has_dir(idx, dd, flip);
            if (a >= 0) { m_res_dir = dd; return a; }
            if (k == 0 || k == 16) break;
        }
    return -1;
}
static void milla_set_anim(player_t *p, int idx, int dir) {
    int fl, a = milla_bank_anim(idx, dir, &fl);
    if (a < 0) return;                                         /* missing animation: Chowdren keeps the current one */
    if (p->an.b != &G.lilac || p->an.anim != a) anim_set(&p->an, &G.lilac, a);
    draw_flip = fl; m_cur_idx = idx; m_cur_dir = dir;
    /* the tail: same index and direction, its own frame counter (RV m_jump_hold) */
    if (G.hair.nanims) {
        char nm[20]; int ta = -1, tfl = 0;
        int d0 = dir >= 16 ? dir - 16 : dir;
        if (dir == 0) snprintf(nm, sizeof nm, "a%d", idx); else if (dir == 16) snprintf(nm, sizeof nm, "a%d_l", idx); else snprintf(nm, sizeof nm, "a%d_d%d", idx, dir);
        ta = bank_anim(&G.hair, nm);
        if (ta < 0 && dir >= 16) {
            if (d0 == 0) snprintf(nm, sizeof nm, "a%d", idx); else snprintf(nm, sizeof nm, "a%d_d%d", idx, d0);
            ta = bank_anim(&G.hair, nm); tfl = 1;
        }
        if (ta < 0) { snprintf(nm, sizeof nm, "a%d", idx); ta = bank_anim(&G.hair, nm); tfl = dir >= 8 && dir < 24; }
        if (ta >= 0) { if (hair.b != &G.hair || hair.anim != ta) anim_set(&hair, &G.hair, ta); hair_flip = tfl; } else hair.b = NULL;
    }
}
static void milla_anim(player_t *p) {
    int y = p->cY;
    if (p->y26 && p->state == ST_NORMAL) y = 26;
    if (p->state == ST_LADDER) y = p->lad_y;
    if (p->state == ST_CORK) y = (p->cork_e > 0.25 && p->cork_e < 1.5) ? 46 : 45;
    int idx = y >= 0 && y < 52 ? MILLA_IDX[y] : -1, force6 = 0;
    if (y == 40) { if (p->vy < 0) idx = 14; else { idx = 24; force6 = 1; } }   /* 2459 / 2460 */
    int Q = p->face > 0 ? 1 : -1;
    int dir = ((Q > 0 ? 0 : 16) - p->mF + 32) % 32;                            /* 2423 / 2424 */
    if (p->state == ST_MSHIELD) dir = Q > 0 ? 0 : 16;
    int prev = m_cur_idx;
    if (idx >= 0) milla_set_anim(p, idx, dir);
    int f6 = (force6 && prev != 24) || (p->c_fall && idx == 24);                /* 2460 / 2252-2253: anim 24 from frame 6 */
    p->c_fall = 0;
    if (f6) {
        animator_t *as[2] = { &p->an, &hair };
        for (int k = 0; k < 2; k++) if (as[k]->b) { int n = as[k]->b->anims[as[k]->anim].count; as[k]->frame = n > 6 ? 6 : n - 1; as[k]->done = 0; }
    }
    /* speeds (2490-2500) */
    double B = fabs(p->ground ? p->gsp : p->vx), Gs = fabs(p->vy);
    int sp = -1, tsp = -1;
    if (y == 1 && p->ground) sp = tsp = (int)(B * 5 + 14);                                   /* 2491 */
    else if (y == 2 && p->ground) sp = tsp = (int)(2 * B + 27);                               /* 2492 */
    else if (y == 8 && p->state == ST_ROLL) sp = tsp = (int)(B * 5 + 25);                     /* 2493 */
    else if (y == 14) sp = tsp = (int)(100 - p->skinI);                                       /* 2494 */
    else if (y == 26) { sp = (int)(fmax(Gs, B) * 5 + 5); tsp = (int)(fmax(Gs, B) * 7 + 7); }  /* 2496 */
    else if (y == 27 || y == 29) sp = tsp = (int)(Gs * 10);                                   /* 2497 */
    else if (y == 40) sp = tsp = (int)(Gs * 4 + 7);                                           /* 2498 */
    else if (y >= 45 && y <= 47) sp = tsp = (int)(2 * B + 17);                                /* 2499 / 2500 */
    else if (y == 15) sp = (int)fmin(30, (int)(p->mA / 2 + 5));                               /* 2254 */
    if (p->state == ST_MSHIELD && p->cY == 20) { p->an.frame = m_aim < p->an.b->anims[p->an.anim].count ? m_aim : 0; p->an.acc = 0; }   /* anim 25 speed 0, frame = aim */
    else anim_tick(&p->an, sp);
    if (hair.b) anim_tick(&hair, tsp);
    psc = p->an;
    /* character sensor (2471-2489) */
    if (y != 24 && y != 44) p->c_sens = y == 5 || y == 7 || y == 8 || y == 9 || y == 14 || (y >= 20 && y <= 23) || y == 26 || y == 42;
}

int player_milla_hud(void) { return m_hud; }
void player_milla_dbg(double v[7]) { v[0] = cube.A; v[1] = cube.x; v[2] = cube.y; v[3] = cube.C; v[4] = cube.F; v[5] = cube.anim * 100 + cube.an.frame + (cube.playing ? 0.5 : 0); v[6] = m_res_dir; }

/* the cube is drawn in front of MILLA (anim 11 arc / 2 / 5 / 8 rotated by C) */
static void milla_draw_cube(void) {
    if (!cube.vis || !cube.an.b) return;
    frame_draw_rot(cube.an.b, anim_frame(&cube.an), floorf(cube.x - G.camx), floorf(cube.y - G.camy), cube.ang, cube.flip, 0xffffffff);
}
