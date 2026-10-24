/* Carol (MV.A = 2; on the bike MV.A = 3), included by player.c after the shared movement helpers.
 * She runs on Lilac's shared movement block, speed events and shared state groups (the original runs the same
 * events for every character); only her own events are here, in the original's in-frame order.
 * Sources: build/re/spec_carol_moves.md (states, pounce, roll entries, wall cling), spec_carol_combat.md (Wild Claw
 * M20, Wild Kick M44, big slash), spec_carol_bike.md; RV = original traces build/orig_traces/carol (tests/orig_carol).
 * Event numbers are the original's (Frames::event_func_N).
 *
 * RV corrections to the static specs (traces recorded session 10):
 *  - 2177 lets go of the wall when the direction TOWARD the wall is not held (c_wall_jump / c_wall_away: release R ->
 *    M1 two frames later; holding R keeps the cling down to the floor, c_wall_slide). The spec's reading (press away)
 *    was the inverted Ghidra block.
 *  - the M1 group turns OTHERS.Y 7 (wall pose) into 3 like 0-2 and 40 (c_wall_jump row 100).
 *  - a roll jump keeps the speed rules on (c_roll_jump: air control after 1861); only the roll itself and a Wild Kick
 *    started from it run with MV.S = 0 (c_run_down, c_roll_special).
 *  - Wild Kick: OTHERS.E reads 2 on every M44 frame (c_air_kick), so 2144 is applied as E = max(E, 2) here (the port
 *    counts E down at the start of the next frame). */

#define C_FL_INCLAW 0x1        /* flag 0 */
#define C_FL_BUF    0x2        /* flag 1 */
#define C_FL_SHIELD 0x4        /* flag 2 */
#define C_FL_HIT    0x8        /* flag 3 (set by combat.c 2740) */
#define C_FL_BIG    0x1000     /* flag 12 */
#define C_FL_WSND   0x10000    /* port: wall-grab sound played this cling */
#define C_KICK_COST 20.0       /* 1770 / 1795 / 1866 / 1904: ENERGY >= 20 */

#define BIKE_LAND_POSE 0x2000  /* BSK flag 8 (kept in cfl): the landing pose after a can (2107) */
static int is_carol(void) { return chr_is_carol(G.character); }
static int bike_air_jump_ok(const player_t *p, const input_t *in);   /* carol_bike.inc */
static obj_t *pbike_create(const player_t *p, int wrecked);
static void bike_set(player_t *p, int on);
static int carol_skin_frame(const player_t *p);
static int c_q0, c_g0;         /* facing (MV.Q) and L at the start of the frame: the events read them before 2421 */
static int c_kick_snd;         /* frames until sample 413 (2 s) has finished (2149 "not playing") */

/* voices (voice group 0x216b2, spec_carol_combat 10); each draw is one cross_seed step */
static void c_voice_attack(void) { static const int v[4] = { 120, 116, 121, 122 }; int k = fx_rand(8); if (k < 4) snd_play_orig(v[k], 255, 128); }
static void c_voice_kick(void) { static const int v[3] = { 139, 140, 141 }; snd_play_orig(v[fx_rand(3)], 255, 128); }

/* the M0 cling (1771) is refused when the big side sensor overlaps a pushable block (qualifier 14) */
static int c_on_pblock(int side) {
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(side > 0 ? SENS_RIGHTBIG : SENS_LEFTBIG, &ox, &oy, &a, &b, &h)) return 0;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (o->alive && (o->type == OT_PBLOCK1 || o->type == OT_PBLOCK2) && o->an.b && obj_rows_hit(o, ox, oy, a, b, h)) return 1;
    }
    return 0;
}
static int c_wall_touch(int side) { return side > 0 ? mv_sensor_right_big() : mv_sensor_left_big(); }
static int c_side(const input_t *in) { return (in->held & BTN_RIGHT) ? 1 : (in->held & BTN_LEFT) ? -1 : 0; }

/* ---- state entries ---- */
static void c_start_claw(player_t *p, int clear, int big) {   /* M = 20: 1756 / 1803 clear flags 1, 2; 1800 / 1865 arm the big slash */
    set_state(p, ST_CLAW);
    if (clear) p->cfl &= ~(C_FL_BUF | C_FL_SHIELD);
    if (big) { p->cfl |= C_FL_BIG; p->c_nospd = 0; }
}
static void c_start_kick(player_t *p, int voice) {             /* M = 44, L = 0; 1770 / 1795 / 1904 voice sound_kick<rand(3)> (1866: none, RV) */
    set_state(p, ST_WILDKICK); p->cL = 0;
    if (voice) c_voice_kick();
}
static void c_start_roll(player_t *p) {                        /* 1748 / 1749 / 1806: S = 0, M = 6, sound 7, OTHERS.N = 0 */
    set_state(p, ST_ROLL); p->boost_n = 0; p->c_nospd = 1; p->m1 = 0;
    snd_play_orig(7, 220, 128);
}
static void c_start_wall(player_t *p) {                        /* 1771 / 1793 / 1905: M = 47, CAROL.M = 10 */
    set_state(p, ST_WALL); p->cM = 10; p->wall_t = 0; p->cfl &= ~C_FL_WSND; p->c_gwall = p->vy;
}
/* pounce (1773/1774 M0, 1796/1797 M1, 1907/1908 M40; crouch 1836): L = N = 0, M = 0, OTHERS.Y = 24; G kept */
static void c_pounce(player_t *p, int crouch) {
    int q = c_q0;
    if (p->ground) { p->vx = p->gsp; p->vy = 0; }
    p->ground = 0; p->N = 0;
    if (crouch) { p->vy = -3; p->vx = 8 * q; }
    else if (q > 0) { if (p->vx < 5) p->vx = 5; } else { if (p->vx > -5) p->vx = -5; }
    set_state(p, ST_NORMAL); p->m1 = 0; p->cY = 24; p->c_fall = 0; p->c_nospd = 0; p->springing = 0;
    /* loop_sound_pounce: 154 every pounce; voice 133 when no CAROL has S > 0 (event 1301: S = 600) */
    if (p->cS <= 0) { snd_play_orig(133, 255, 128); p->cS = 600; }
    snd_play_orig(154, 220, 128);
}
static void c_jump(player_t *p) { jump(p); p->m1 = 1; }        /* 1750 (3047): M = 1, PAV_A.C = 1 */

/* ---- movement block for Carol's frames (speed rules 1621-1636 only while MV.S = 1) ---- */
static void c_ground_move(player_t *p, const input_t *in, int control) {
    ground_move(p, in, control, !p->c_nospd);
    if (p->c_nospd && control) { if (in->held & BTN_RIGHT) p->face = 1; else if (in->held & BTN_LEFT) p->face = -1; }   /* 2421 / 2422 */
}
static void c_air_move(player_t *p, const input_t *in) {
    if (!p->c_nospd) { air_move(p, in, 1, PHYS_GRAV); return; }
    core(p);
    if (p->ground) return;
    if (in->held & BTN_RIGHT) p->face = 1; else if (in->held & BTN_LEFT) p->face = -1;
    air_speed_update(p, in, 0, PHYS_GRAV);                      /* 1647 gravity, 1648 drag */
    if (p->vy < 0 && mv_sensor_top_big()) p->vy = 0;            /* 1649 */
}

/* ---- ST_NORMAL: M0 (ground / air), M1 (air), M2 look up, M3 crouch (the port keeps M2 / M3 in look_prev) ---- */
static void carol_normal(player_t *p, const input_t *in, int *crouch_look) {
    int jp = (in->pressed & BTN_JUMP) != 0, ap = (in->pressed & BTN_ATTACK) != 0, sph = (in->held & BTN_SPECIAL) != 0;
    int was_ground = p->ground;
    p->c_skid0 = p->skidding;
    int cm = p->m1 ? 1 : !p->ground ? 0 : p->look_prev == 2 ? 2 : p->look_prev == 3 ? 3 : 0;
    int lr = (in->held & (BTN_LEFT | BTN_RIGHT)) != 0;
    int up = (in->held & BTN_UP) && !(in->held & BTN_DOWN), dn = (in->held & BTN_DOWN) && !(in->held & BTN_UP);
    /* movement + speed rules (the input rules are skipped in M2 / M3) */
    if (p->ground) c_ground_move(p, in, cm != 2 && cm != 3);
    else c_air_move(p, in);
    int landed = !was_ground && p->ground;

    if (cm == 0 && p->ground) {                                    /* ---- M0 group, grounded ---- */
        if (p->cY <= 2) p->c_nospd = 0;                            /* 1722 */
        p->c_pavO = 0;                                             /* 1741: PAV_B.O = 0 (the bike's air jump is available again) */
        double B = fabs(p->gsp);
        if (B == 0) { if (p->cY != 48 && p->cY != 12) p->cY = 0; } else p->cY = B < 5 ? 1 : 2;   /* 1723-1725 (5, also on the bike: RV) */
        if (p->skidding && p->c_skid0) p->cY = 6;                  /* M4 group (skid): OTHERS.Y 6 from its 2nd frame (RV b_skid) */
        if (p->cY == 12 && ++p->c_y12 > 33) p->cY = 0;             /* 2443: Lilac's SKIN anim 20 (8 frames at 25) has ended (INFERRED) */
        /* 1742 / 1743: look up / crouch (switch the M0 group off); RV for Lilac: Down with no direction crouches once
         * |B| < 2 (B = 0) */
        if (dn && !lr && B < PHYS_LOWSPD) { p->gsp = 0; p->look = 3; *crouch_look = 1; return; }
        if (up && !lr && B < 0.01) { p->look = 2; *crouch_look = 1; return; }
        if (!p->bike && (in->pressed & BTN_DOWN) && B >= 2) { c_start_roll(p); return; }   /* 1748 (A = 2; group off) */
        if (!p->bike && landed && (in->held & BTN_DOWN) && B >= 2) c_start_roll(p);         /* 1749 (turns M1 off, not M0: original bug kept) */
        if (jp) { set_state(p, ST_NORMAL); c_jump(p); return; }                            /* 1750 (group off) */
        if (p->state == ST_NORMAL && push_wants(p, in)) { set_state(p, ST_PUSH); p->face = (in->held & BTN_RIGHT) ? 1 : -1; }   /* 1752 / 1753 */
        if (ap && p->state != ST_CLAW) c_start_claw(p, 1, 0);                               /* 1756 */
        if (p->water && (in->held & BTN_JUMP) && p->vy > -3) { set_state(p, ST_NORMAL); c_jump(p); return; }   /* 1758 */
        if (p->cN > 0) p->cN--;                                                            /* 1759 */
        if (sph && p->energy >= C_KICK_COST && p->cN <= 0 && !p->bike) c_start_kick(p, 1);   /* 1770 (on the bike only airborne) */
        return;
    }
    if (cm == 0) {                                                 /* ---- M0 group, airborne (walked off, pounce, M40) ---- */
        int side = c_side(in);
        if (p->springing && p->cY != 24) p->cY = 40;                                                    /* M40 group: OTHERS.Y 40 */
        if (ap && p->state != ST_CLAW) c_start_claw(p, 1, 0);                                         /* 1756 */
        if (p->cN > 0) p->cN--;                                                                         /* 1759 */
        if (sph && p->energy >= C_KICK_COST && p->cN <= 0) c_start_kick(p, 1);                          /* 1770 */
        if (side && c_wall_touch(side) && !c_on_pblock(side) && p->cM <= 0) c_start_wall(p);           /* 1771 */
        if (p->bike) { if (bike_air_jump_ok(p, in)) set_state(p, ST_BIKEAIR); }                         /* 1779 (A = 3): M = 32 */
        else if (jp && p->cY != 24) c_pounce(p, 0);                                                      /* 1773 / 1774 */
        if (p->cY == 24) {
            int s = p->vx > 1 ? 1 : p->vx < -1 ? -1 : 0;
            if (s != c_q0) { p->cY = 3; p->c_fall = 1; }                                                /* 1775: cancel (anim 24 frame 6) */
            else { if (c_q0 > 0) p->vx = fmax(p->vx, 6.); else p->vx = fmin(p->vx, -6.); p->cfl |= C_FL_BIG; }   /* 1776 / 1777 */
        }
        if ((p->cfl & C_FL_BIG) && p->cY != 24) p->cfl &= ~C_FL_BIG;                                    /* 1778 */
        return;
    }
    if (cm == 1) {                                                 /* ---- M1 group (jump, roll-off, wall let-go) ---- */
        int side = c_side(in);
        if (p->cY <= 2 || p->cY == 4 || p->cY == 5 || p->cY == 6 || p->cY == 7 || p->cY == 40) p->cY = 3;   /* 1785 (RV: 0-2, 7, 40 -> 3) */
        if (!p->ground && !(in->held & BTN_JUMP) && !p->water && side && c_wall_touch(side) && p->cM <= 0) c_start_wall(p);   /* 1793 */
        if (sph && p->energy >= C_KICK_COST) c_start_kick(p, 1);                                         /* 1795 */
        if (p->bike) {
            if (fabs(p->vx) <= 9) p->cfl &= ~C_FL_BIG;                                                   /* 1798 */
            if (bike_air_jump_ok(p, in)) set_state(p, ST_BIKEAIR);                                       /* 1799: M = 32 */
        } else if (jp && !p->water) c_pounce(p, 0);                                                      /* 1796 / 1797 */
        if (!p->bike && p->cY == 8 && ap) c_start_claw(p, 0, 1);                                         /* 1800: ball-jump attack -> big slash */
        if (p->jumping && !p->ground && !(in->held & BTN_JUMP) && p->vy < -PHYS_JUMP_CUT) p->vy = -PHYS_JUMP_CUT;   /* 1801 */
        if (p->water && !p->ground && (in->held & (BTN_UP | BTN_JUMP)) && p->vy > -3) p->vy = fmin(fmax(p->vy - 0.2, -3.), -1.);   /* 1802 swim */
        if (ap && p->state != ST_CLAW) c_start_claw(p, 1, 0);                                            /* 1803 */
        if (p->ground) {
            if (!p->bike && (in->held & BTN_DOWN) && fabs(p->gsp) >= 0.2) c_start_roll(p);              /* 1806 (M1 group off) */
            else { if (p->state != ST_WILDKICK && p->state != ST_CLAW) set_state(p, ST_NORMAL);
                   p->m1 = 0; p->c_nospd = 0; p->c_land_m1 = 1;                                         /* 1807 */
                   if (p->bike) snd_play_orig(161, 220, 128);                                           /* loop_action_land (bike set, RV) */
                   if (p->bike && (p->cfl & BIKE_LAND_POSE)) { p->cfl &= ~BIKE_LAND_POSE; p->cY = 12; p->c_y12 = 0; } }   /* 2107 */
        }
        return;
    }
    if (cm == 2) {                                                 /* ---- M2 look up (1813-1819) ---- */
        p->cY = 4;
        if (p->ground && p->gsp == 0 && (in->held & BTN_UP)) { p->look = 2; *crouch_look = 1; }        /* 1814-1816 leave otherwise */
        if (ap) { c_start_claw(p, 0, 0); p->look = 0; }                                                  /* 1819 (no jump for Carol) */
        return;
    }
    /* ---- M3 crouch (1823-1836) ---- */
    if (p->cY != 23) { p->cY = 5; p->gsp = 0; }                                                          /* 1823 */
    if (p->ground && p->gsp == 0 && (in->held & BTN_DOWN)) { p->look = 3; *crouch_look = 1; }           /* 1825-1827 */
    if (p->bike && p->ground && fabs(p->gsp) < 3 && (in->held & BTN_DOWN) && jp) {                       /* 1828: get off the bike */
        pbike_create(p, 0); bike_set(p, 0); p->cN = 25; p->nitroF = 0;
    }
    if (ap) { c_start_claw(p, 0, 0); p->look = 0; }                                                      /* 1834 */
    if (jp && p->ground && !p->bike) { c_pounce(p, 1); p->look = 0; }                                    /* 1836 (A = 2: also right after 1828, RV) */
    (void)up; (void)landed;
}

/* ---- M20 Wild Claw: movement (the shared speed rules keep running), then the group 2131-2142 from the 2nd frame ---- */
static void carol_claw_move(player_t *p, const input_t *in) {
    if (p->ground) c_ground_move(p, in, 1); else c_air_move(p, in);
}
static void carol_claw_group(player_t *p, const input_t *in) {
    int fr = carol_skin_frame(p), face16 = c_q0 < 0;
    p->cY = 20 + p->cA; p->cfl &= ~C_FL_INCLAW;                                            /* 2131 */
    if (fr == 2 && !(p->cfl & C_FL_SHIELD) && !(p->cfl & C_FL_BIG)) {                      /* 2132 */
        combat_shield_create(p, (face16 ? 18 : 2) + p->cA, 1, 100);
        c_voice_attack();
        p->cfl |= C_FL_SHIELD;
        int k = 1 + fx_rand(3);
        (void)fx_rand(8001);                                                                /* 2133-2135: pitch 40100 + rand(8001) Hz (not reproduced) */
        p->cS = 0;
        snd_play_orig(409 + k, 220, 128);
    }
    if (p->cfl & C_FL_BIG) p->cA = 2;                                                       /* 2136 */
    if (fr == 2 && !(p->cfl & C_FL_SHIELD) && (p->cfl & C_FL_BIG)) {                       /* 2137: big slash */
        combat_shield_create(p, face16 ? 14 : 30, 0, 0);
        c_voice_attack();
        p->cfl &= ~C_FL_BIG; p->cfl |= C_FL_SHIELD;
        snd_play_orig(414, 220, 128);
    }
    if (cur_anim == A_C_CLAW0 || cur_anim == A_C_CLAW1 || cur_anim == A_C_CLAW2) p->cfl |= C_FL_INCLAW;   /* 2138 */
    if ((in->pressed & BTN_ATTACK) && (p->cfl & C_FL_INCLAW) && fr >= 2) p->cfl |= C_FL_BUF;           /* 2139 */
    if ((p->cfl & C_FL_INCLAW) && (p->cfl & C_FL_BUF) && (fr >= 5 || (fr >= 3 && (p->cfl & C_FL_HIT)))) {   /* 2140: next swing */
        p->cfl &= ~(C_FL_BUF | C_FL_SHIELD | C_FL_HIT); p->cA = (p->cA + 1) % 3; p->cY = 0;
    }
    if ((p->cfl & C_FL_INCLAW) && fr >= 5 && !(p->cfl & C_FL_BUF)) {                       /* 2141: combo over (also after 2140, RE/RV quirk) */
        p->cfl &= ~(C_FL_BUF | C_FL_SHIELD); p->cA = (p->cA + 1) % 3;
        p->cY = p->ground ? 0 : 40;
        set_state(p, ST_NORMAL); p->m1 = !p->ground;
    }
    if (p->ground && (in->pressed & BTN_JUMP)) jump(p);                                     /* 2142: jump, M stays */
}

/* ---- M44 Wild Kick ---- */
static void carol_kick_move(player_t *p, const input_t *in) {
    int face = p->face;                                            /* M44 is in the facing-freeze list (2421 / 2422) */
    if (p->ground) c_ground_move(p, in, 1); else c_air_move(p, in);
    p->face = face;
}
static void carol_kick_group(player_t *p, const input_t *in) {
    int fr = carol_skin_frame(p);
    p->cY = 44;                                                                             /* 2143 */
    if (!p->hurt_pending && p->invuln < 2) p->invuln = 2;                                   /* 2144 (E >= 3; RV reads 2, see top) */
    if (!(in->held & BTN_JUMP) && p->vy < -PHYS_JUMP_CUT && p->jumping && !p->ground) p->vy = -PHYS_JUMP_CUT;   /* 2145 */
    if (p->water && !p->ground && (in->held & (BTN_UP | BTN_JUMP)) && p->vy > -3) { p->vy = fmin(fmax(p->vy - 0.2, -3.), -1.); p->cY = 26; }   /* 2146 */
    if (p->ground) { double f = 5 * PHYS_ACC; if (p->gsp >= 0) p->gsp = fmax(0., p->gsp - f); if (p->gsp <= 0) p->gsp = fmin(0., p->gsp + f); }   /* 2147 / 2148 */
    if (c_kick_snd > 0) c_kick_snd--;
    if (fr > 1 && p->cL < 20 && c_kick_snd <= 0) { snd_play_orig(413, 220, 128); c_kick_snd = 120; }   /* 2149 */
    if ((fr == 2 || fr == 5 || fr == 7) && !(p->cfl & C_FL_SHIELD)) {                       /* 2150 */
        p->cfl |= C_FL_SHIELD;
        combat_shield_create(p, (c_q0 < 0 ? 16 : 0) + (c_g0 ? 0 : 1) + 2 + p->cA, 1, 100);   /* skin direction (+1 in the air, 2425) */
        p->cA = (p->cA + 1) % 3;
    }
    if (fr != 2 && fr != 5 && fr != 7) p->cfl &= ~C_FL_SHIELD;                              /* 2151 */
    if (p->cL <= 20 && p->hitstop <= 2) p->energy = fmaxf(0.f, p->energy - 1.5f);           /* 2152 */
    if (p->cL < 20 || (p->cL == 20 && !(in->held & BTN_SPECIAL)) || (p->cL == 20 && p->energy <= 0)) p->cL++;   /* 2153 */
    if (p->cL > 20) { p->an.frame = 0; psc.frame = 0; if (hair.b) hair.frame = 0; p->cL++; c_kick_snd = 0; }   /* 2154 (stop 413: not reproduced) */
    if (p->cL > 30) { set_state(p, ST_NORMAL); p->m1 = 0; p->cY = 0; }                       /* 2155 */
    if (p->ground && (in->pressed & BTN_JUMP)) jump(p);                                       /* 2156: M stays */
}

/* ---- M47 wall cling: the movement block runs with T = 0 (X frozen) from the second frame, then 2173-2183 ---- */
static void carol_wall(player_t *p, const input_t *in) {
    int q = c_q0;
    mv_t m = { p->x, p->y - ORIG_REF_DY, p->ground ? p->gsp : p->vx, p->ground ? 0. : p->vy, p->N, p->O, 0, p->ground, 0, 0, p->wall_t > 0 };
    int r = mv_core(&m);
    p->x = m.X; p->y = m.Y + ORIG_REF_DY; p->N = m.N; p->O = m.O; p->ground = m.L;
    if (m.L) { p->gsp = m.B; p->vy = 0; if (r & MV_LANDED) on_land(p); ground_speed_update(p, in, 1); }
    else {
        p->vx = m.B; p->vy = m.G;
        horiz_accel(p, in, 1, 0); wall_stop(p);
        air_speed_update(p, in, 0, PHYS_GRAV);
        if (p->vy < 0 && mv_sensor_top_big()) p->vy = 0;
    }
    if (p->vy > 4) p->vy = 4;                                                       /* 2173: G = min(G, 4), O = N = 0, Y = 7, S = 1 */
    p->c_gwall = p->vy;                                                             /* (the engine rev reads G here, 2118) */
    if (p->bike && ((q > 0 && (in->held & BTN_RIGHT)) || (q < 0 && (in->held & BTN_LEFT)))) p->vy = fmax(p->vy - 0.6, -8.);   /* 2176: the bike climbs */
    p->O = 0; p->N = 0; p->cY = 7; p->c_nospd = 0;
    int f1 = carol_skin_frame(p) == 1;                                              /* 2174 / 2175: PSC frame 1, once while true (RV) */
    if (f1 && !(p->cfl & C_FL_WSND)) snd_play_orig(p->bike ? 161 : 33, 220, 128);
    if (f1) p->cfl |= C_FL_WSND; else p->cfl &= ~C_FL_WSND;
    p->wall_t++;
    int let_go = p->ground || (!p->bike && ((q > 0 && !(in->held & BTN_RIGHT)) || (q < 0 && !(in->held & BTN_LEFT))));   /* 2177 (RV; the release branches need A = 2) */
    if (let_go || !c_wall_touch(q)) { set_state(p, ST_NORMAL); p->m1 = 1; }        /* 2177 / 2179: M = 1, T = U = 1 */
    if (in->pressed & BTN_JUMP) {                                                   /* 2181 + 2183 (bike 2182: B = -6 Q) */
        p->vx = (p->bike ? -6 : -3) * q; p->vy = -PHYS_JUMP;
        p->ground = 0; p->N = 0; set_state(p, ST_NORMAL); p->m1 = 1; p->jumping = 1;
        snd_play_orig(117, 220, 128);
    }
    if (in->held & BTN_RIGHT) p->face = 1; else if (in->held & BTN_LEFT) p->face = -1;   /* 2421 / 2422 */
}

/* per-frame Carol timers of group 0x21a4a (2104 cling cooldown, 2105 voice cooldown) */
static void carol_timers(player_t *p) {
    if (p->cM > 0) p->cM--;
    if (p->cS > 0) p->cS--;
}

/* animation from OTHERS.Y (2426-2470); shared gimmick states as Lilac's */
static int carol_anim(const player_t *p, int *force6) {
    switch (p->state) {
    case ST_HURT: if (p->cY == 9) return A_HURT; break;     /* OTHERS.Y 9 from the frame after the hit */
    case ST_PUSH: if (p->cY == 13) return A_PUSH; break;    /* OTHERS.Y 13 from the 2nd M11 frame */
    case ST_CORK: return (p->cork_e > 0.25 && p->cork_e < 1.5) ? A_CORK2 : A_CORK;
    case ST_LADDERIN: return A_CLIMBOVER;
    case ST_LADDER: return p->lad_y == 28 ? A_CLIMBOVER : p->lad_y == 47 ? A_CATCH : p->lad_y == 29 ? A_CLIMBDOWN : A_CLIMB;
    default: break;
    }
    if (p->y26 && p->state == ST_NORMAL && p->cY != 24 && p->cY != 8) return A_SWIM;
    switch (p->cY) {
    case 0: return A_IDLE;
    case 1: return A_WALK;
    case 2: return A_RUN;
    case 3: return A_JUMP;
    case 4: return A_LOOKUP;
    case 5: return A_CROUCH;
    case 6: return A_SKID;
    case 7: return A_C_WALL;
    case 8: return A_CHARGE;
    case 9: return A_HURT;
    case 13: return A_PUSH;
    case 20: return A_C_CLAW0;
    case 21: return A_C_CLAW1;
    case 22: return A_C_CLAW2;
    case 23: return A_C_PADHOLD;
    case 24: return A_C_POUNCE;
    case 26: return A_SWIM;
    case 44: return A_C_KICK;
    case 40: default:
        if (!p->ground || p->cY == 40) {                     /* 2459 (G < 0: anim 14) / 2460 (anim 24 from frame 6) */
            if (p->vy < 0) return A_C_RISE;
            *force6 = 1; return A_JUMP;
        }
        return A_IDLE;
    }
}
/* animation speeds (2490-2500): force_speed persists until restore_speed (2490: Y not in 1, 2, 8, 15, 26, 40) */
static void carol_speeds_foot(player_t *p);
static void carol_speeds(player_t *p) {
    carol_speeds_foot(p);
    p->c_spd_bsk = p->state == ST_BIKEAIR ? 100 : (p->state == ST_PAD && p->padJ < 3) ? 35 : -1;   /* BSK: restore_speed except M32 (2164) and the pad hop (2184) */
}
static void carol_speeds_foot(player_t *p) {
    if (p->state == ST_PAD && p->padJ < 3) return;   /* 2184 forced 35 */
    double B = fabs(p->ground ? p->gsp : p->vx), Gs = fabs(p->vy);
    int y = p->y26 && p->state == ST_NORMAL && p->cY != 24 && p->cY != 8 ? 26 : p->cY;
    if (p->state == ST_LADDER) y = p->lad_y;
    if (p->state == ST_CORK) y = 45;
    if (!(y == 1 || y == 2 || y == 8 || y == 15 || y == 26 || y == 40 || y == 27 || y == 29 || y == 45 || y == 46)) { p->c_spd_sk = p->c_spd_tl = -1; return; }
    if (y == 1 && p->ground) p->c_spd_sk = p->c_spd_tl = (int)(B * 5 + 14);                                   /* 2491 */
    if (y == 2 && p->ground) { p->c_spd_sk = (int)(2 * B + 27); p->c_spd_tl = (int)(B * 3 + 20); }           /* 2492 */
    if (y == 8 && p->state == ST_ROLL) p->c_spd_sk = p->c_spd_tl = (int)(B * 5 + 25);                         /* 2493 */
    if (y == 26) p->c_spd_sk = p->c_spd_tl = (int)(fmax(Gs, B) * 7 + 7);                                      /* 2496 */
    if (y == 27 || y == 29) p->c_spd_sk = p->c_spd_tl = (int)(Gs * 10);                                       /* 2497 */
    if (y == 40) p->c_spd_sk = p->c_spd_tl = (int)(Gs * 4 + 7);                                               /* 2498 */
    if (y == 45 || y == 46) p->c_spd_sk = p->c_spd_tl = (int)(2 * B + 17);                                    /* 2499 */
}

/* character sensor animation (2471-2489): small for Y 5, 7, 8, 9, 14, 20-23, 26, 42; Y 24 / 44 keep the last one */
static void carol_sensor(player_t *p) {
    int y = p->cY;
    if (y == 24 || y == 44) return;
    p->c_sens = y == 5 || y == 7 || y == 8 || y == 9 || y == 14 || (y >= 20 && y <= 23) || y == 26 || y == 42;
}
