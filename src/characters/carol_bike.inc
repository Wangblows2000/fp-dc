/* Carol's motorbike (MV.A = 3), fuel cans, parked bikes and the red jump pads; included by player.c after carol.inc.
 * Source: build/re/spec_carol_bike.md (static RE) and the original's traces b_*, c_fuel_dv1, c_jumppad_* (RV).
 * Port objects: OT_FUEL (ObjectFuel_533), OT_PBIKE (ObjectBike_459, the parked / wrecked bike, created here),
 * OT_JUMPPAD (ObjectJumppad_539 / 1104). The rider is drawn with PlayerSkinCarolBike_460 (bank G.bike).
 * State on player_t: bike (MV.A = 3), bikeC (BSK.C hit points), rev (BSK.A), nitroF / nitroG (OTHERS.F / G),
 * bike_latch (BSK flag 1), padC..padL / padD..padI (PSC alterables of the jump-pad ride), padfl (PSC flags). */

#define PADF_VOICE 0x100       /* PSC flag 8: voice toggle */
#define PADF_X     0x400       /* PSC flag 10: X arrived */
#define PADF_Y     0x800       /* PSC flag 11: Y arrived */
#define PADF_OFF   0x1         /* port: MV.T = U = 0 since 2188 (no movement) until the launch / drop */

static pfx_t *pfx_spawn_b(float x, float y, const bank_t *b, int anim) {
    if (anim < 0) return NULL;
    for (int i = 0; i < NPFX; i++)
        if (!pfx[i].an.b) { pfx[i].x = x; pfx[i].y = y; pfx[i].vx = pfx[i].vy = 0; pfx[i].argb = 0xffffffff; anim_set(&pfx[i].an, b, anim); anim_restart(&pfx[i].an); return &pfx[i]; }
    return NULL;
}
static void c_wburst(float x, float y) { pfx_spawn_b(x, y, &G.objs, bank_anim(&G.objs, "wburst")); }
static void c_sparkle(float x, float y, int frame) {
    int a = bank_anim(&G.objs, "ringsparkle");
    pfx_spawn_b(x, y, &G.objs, a);
    if (frame && a >= 0) for (int i = NPFX - 1; i >= 0; i--) if (pfx[i].an.b && pfx[i].an.anim == a && pfx[i].x == x && pfx[i].y == y) { pfx[i].an.frame = frame; break; }
}
/* BSK action point: (4,50) / (59,50) with the hotspot (32,40) / (31,40) -> (-28, +10) facing right, rotated with the skin */
static void bike_action_point(const player_t *p, float *x, float *y) {
    double ox = -28. * p->face, oy = 10., a = p->rot / 57.29577951308232;
    *x = (float)(floor(p->x) + ox * fd_cos(a) + oy * fd_sin(a));
    *y = (float)(floor(p->y - ORIG_REF_DY) - ox * fd_sin(a) + oy * fd_cos(a));
}

/* ---- engine sound (2103 / 2125 / 2102): samples 159 + 160 looping; ch 9 volume max(0, 100 - 20 rev), ch 10 volume
 * clamp(60 rev, 0, 100) and frequency 22000 + 7000 rev Hz (for the 44.1 kHz original; the port's samples are 22.05 kHz) */
static int eng_ch9 = -1, eng_ch10 = -1;
static void bike_engine(const player_t *p) {
    if (!p->bike || p->state == ST_DEAD) {                                  /* 2102 / 1959 / 1976 */
        if (eng_ch9 >= 0) { snd_loop_stop(eng_ch9); eng_ch9 = -1; }
        if (eng_ch10 >= 0) { snd_loop_stop(eng_ch10); eng_ch10 = -1; }
        return;
    }
    double r = p->rev;
    int v9 = (int)fmax(0., 100 - 20 * r), v10 = (int)fmin(fmax(60 * r, 0.), 100.), f10 = (int)((22000 + 7000 * r) / 2);
    if (eng_ch9 < 0) eng_ch9 = snd_loop_orig(159, v9 * 255 / 100, 22050);   /* 2103 */
    if (eng_ch10 < 0) eng_ch10 = snd_loop_orig(160, v10 * 255 / 100, f10);
    snd_loop_update(eng_ch9, v9 * 255 / 100, 22050);
    snd_loop_update(eng_ch10, v10 * 255 / 100, f10);
}
void player_engine_stop(void) {
    if (eng_ch9 >= 0) { snd_loop_stop(eng_ch9); eng_ch9 = -1; }
    if (eng_ch10 >= 0) { snd_loop_stop(eng_ch10); eng_ch10 = -1; }
}

/* ---- movement constants on the bike (1560-1564; E RV b_run_right / b_nitro) ---- */
static void bike_phys(player_t *p, const input_t *in) {
    phys_w.D = 15.;                                                          /* 1543 table */
    phys_w.C = p->nitroF ? 10. : 8.;                                        /* 1560 / 1561 */
    phys_w.E = p->nitroF ? 0.36 : (in->held & (BTN_LEFT | BTN_RIGHT)) ? 0.06 : 0.00575;   /* 1563 / 1564 (RV values) */
    if (!p->water) phys_w.I = 0.3;                                           /* 1562 */
}

static void bike_set(player_t *p, int on) {                                 /* MV.A 3 / 2 and global value 1 */
    p->bike = on; G.character = on ? CHR_CAROL_BIKE : CHR_CAROL;
}

/* ---- the parked / wrecked bike (ObjectBike_459) ---- */
static obj_t *pbike_create(const player_t *p, int wrecked) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    o->type = OT_PBIKE; o->alive = 1; o->hp = 1; o->layer = 2;
    o->x = (float)floor(p->x); o->y = (float)floor(p->y - ORIG_REF_DY);   /* PLAYER (int X, int Y) */
    o->dir = p->face;                                                        /* bike.A = MV.Q */
    o->ang = p->rot;                                                         /* BSK angle */
    o->fl = wrecked ? 0x40 : 0x2;                                            /* flag 6 wrecked / flag 1 just dismounted */
    o->st = 0;                                                               /* B: explosion countdown */
    int a = bank_anim(&G.lilac, p->face < 0 ? "pbike_l" : "pbike");
    o->flip = 0;
    if (a < 0 && p->face < 0) { a = bank_anim(&G.lilac, "pbike"); o->flip = 1; }
    if (a >= 0) anim_set(&o->an, &G.lilac, a); else o->an.b = NULL;
    o->born = G.frame;
    return o;
}
/* 2112 / 4729 / 4930: a wrecked bike explodes the frame it is created: explosion, sound 29, B = -5, 4 debris pieces
 * (EffectsDebris_427 frames 50, 49, 48, 47); 2113 destroys it (B = -1) */
static void pbikes_wreck(void) {
    int ad = bank_anim(&G.lilac, "bdebris");
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_PBIKE || !(o->fl & 0x40)) continue;
        obj_effect(o->x, o->y, "explosion"); snd_play_orig(29, 220, 128);
        for (int B = -5; B < -1; B++) {                                     /* loop_spawndebris x5 (the 5th finds B = -1) */
            int spd = 50 + fx_rand(30);                                      /* debris speed (direction from the debris class: INFERRED) */
            obj_t *d = obj_alloc();
            if (!d || ad < 0) continue;
            int k = B + 5;
            d->type = OT_DEBRIS; d->alive = 1; d->x = o->x; d->y = o->y; d->layer = 3;
            anim_set(&d->an, &G.lilac, ad); d->an.frame = (int)(45 - B) - 47;
            d->vx = (float)((k - 1.5) * 1.5 * spd / 64.); d->vy = (float)(-3. - k * 0.5);
        }
        o->alive = 0;                                                        /* 2113 */
    }
}
/* a stage script takes Carol off the bike (session 17: FN3 7711 / FN6 8763 MV.A 3 -> 2, g1 2; FN5 8383 also leaves an
 * ObjectBike at her: park). The engine loop stops. */
void player_bike_off(player_t *p, int park) {
    if (!p->bike) return;
    bike_set(p, 0);
    player_engine_stop();
    if (park) pbike_create(p, 0);
    p->nitroF = 0; p->nitroG = 0;
}
static void bike_wreck(player_t *p) {                                       /* 1285 / 2111: MV.A = 2, G1 = 2, wrecked bike at PLAYER */
    if (!p->bike) return;
    bike_set(p, 0);
    pbike_create(p, 1);
    pbikes_wreck();
    p->nitroF = 0; p->nitroG = 0;
}

/* ---- per-frame bike events after the state groups (engine group 0x21a33 + 2107-2113) ---- */
static void bike_events(player_t *p, const input_t *in, int st0) {
    /* 2108: a just-dismounted bike can be remounted once S_BB has stopped touching it */
    int ox, oy, h; const signed char *a, *b;
    int sbb = mv_sensor_rows(SENS_BOTTOMBIG, &ox, &oy, &a, &b, &h);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_PBIKE || o->born == G.frame) continue;
        int touch = 0;                                                       /* S_BB rows vs the bike's box (initialize_active(1)) */
#ifdef FP_HOST_ENV
        if (getenv("FP_DBG_BIKE")) { const frame_t *f = o->an.b ? anim_frame(&o->an) : NULL; printf("bike f%d X %.2f Y %.2f vy %.2f sbb oy %d h %d ox %d | bike %.0f,%.0f box y %d..%d fl %x\n", G.frame, p->x, p->y - ORIG_REF_DY, p->vy, oy, h, ox, o->x, o->y, f ? (int)o->y - f->hy : 0, f ? (int)o->y - f->hy + f->h : 0, o->fl); }
#endif
        if (sbb && o->an.b) {
            const frame_t *f = anim_frame(&o->an);
            int bx = (int)o->x - f->hx, by = (int)o->y - f->hy;
            for (int j = 0; j < h && !touch; j++) if (b[j] >= a[j] && oy + j >= by && oy + j < by + f->h && ox + b[j] >= bx && ox + a[j] < bx + f->w) touch = 1;
        }
        if ((o->fl & 2) && !touch) o->fl &= ~2;
        /* 2109: remount (A = 2, airborne, falling, M not 8/9/47/48, S_BB on a bike without flag 1, |dx| <= 23) */
        if (!p->bike && chr_is_carol(G.character) && !p->ground && p->vy > 0 && touch && !(o->fl & 2) &&
            p->state != ST_HURT && p->state != ST_DEAD && p->state != ST_WALL && p->state != ST_PAD &&
            floor(p->x) >= o->x - 23 && floor(p->x) <= o->x + 23) {
            snd_play_orig(126, 220, 128);
            p->face = o->dir; p->x = o->x; p->y = o->y + ORIG_REF_DY; p->rot = o->ang;
            set_state(p, ST_NORMAL); p->m1 = 0; p->ground = 1; p->gsp = 0; p->vx = 0; p->vy = 0;
            p->cY = 12; p->c_y12 = 0; bike_set(p, 1); p->c_nospd = 0;
            o->alive = 0;
            break;
        }
    }
    if (!p->bike) return;
    double B = p->ground ? p->gsp : p->vx;
    /* 2114-2119: engine rev follows |B| (pushing: up to 8 at +0.1; on the wall: |G|) by 0.2 per frame */
    { double t = p->state == ST_PUSH ? 8. : p->state == ST_WALL ? fabs(p->c_gwall) : fabs(B), up = p->state == ST_PUSH ? 0.1 : 0.2;
      if (p->rev < t) p->rev = fmin(t, p->rev + up); else if (p->rev > t) p->rev = fmax(t, p->rev - 0.2); }
    /* 2120 / 2121 / 2122: nitro (grounded or on the wall, a direction held, Down not held, special held, not boosting,
     * latch off); energy > 0 required (INFERRED, U T5) */
    int dir = (in->held & BTN_RIGHT) ? 1 : (in->held & BTN_LEFT) ? -1 : 0;
    if ((in->held & BTN_SPECIAL) && dir && !(in->held & BTN_DOWN) && !p->nitroF && !p->bike_latch && p->energy > 0 &&
        ((p->ground && p->state != ST_WILDKICK) || p->state == ST_WALL)) {
        p->nitroG = 30; p->nitroF = 2; p->bike_latch = 1; p->cfl |= C_FL_BIG;
        float wx, wy; bike_action_point(p, &wx, &wy); c_wburst(wx, wy);
        if (p->state == ST_WALL) p->vy = -8;
        else if (p->face > 0) p->gsp = fmax(10., p->gsp); else p->gsp = fmin(-10., p->gsp);
        snd_play_orig(387, 220, 128);
    }
    if (!(in->held & BTN_SPECIAL)) p->bike_latch = 0;                                         /* 2126 */
    if (p->nitroF == 2) p->energy = fmaxf(0.f, p->energy - 1.f);                              /* 2123 */
    if (p->nitroF > 0 && p->nitroG > 0) p->nitroG--;                                          /* 2405 (RV: before the end test) */
    if (p->nitroF == 2 && (p->nitroG <= 0 || (p->state != ST_WALL && B == 0))) p->nitroF = 0; /* 2124 (RV b_nitro: no clamp seen) */
    if (p->nitroF == 2 && G.frame % 4 == 0) { float wx, wy; bike_action_point(p, &wx, &wy); c_sparkle(wx, wy, 0); }   /* 2130 */
    /* 2127 / 2128: skid sample 162, looping while skidding (RV b_skid: loop count 0) */
    if (p->skidding && !p->c_skid_snd) p->c_skid_snd = 1 + snd_loop_orig(162, 200, 22050);
    if (!p->skidding && p->c_skid_snd) { snd_loop_stop(p->c_skid_snd - 1); p->c_skid_snd = 0; }
    /* 2129: exhaust puff every 8th frame standing / every 4th moving, not under water, not boosting */
    if (!p->water && p->nitroF != 2 && G.frame % (B == 0 ? 8 : 4) == 0) {
        float wx, wy; bike_action_point(p, &wx, &wy);
        /* EffectsDust01_369 (RE create_effectsdust01_369): blend alpha 0x74, random direction of 32 (one cross_seed
         * step, seed >> 26 & 31), BallMovement speed 9 = 1.125 px/frame (y up); fraction kept as a float (INFERRED step) */
        pfx_t *d = pfx_spawn_b(wx, wy, &G.lilac, bank_anim(&G.lilac, "bdust"));
        int dir = fx_rand(32);
        if (d) { d->argb = 0x74ffffff; d->vx = (float)(1.125 * fd_cos(dir * 11.25 / 57.29577951308232)); d->vy = (float)(-1.125 * fd_sin(dir * 11.25 / 57.29577951308232)); }
    }
    /* 1727 (M0, OTHERS.Y 8) / 2165 (M32): spin hit box, ShieldInstaShield A = 2 direction 24, when none exists */
    if ((p->state == ST_BIKEAIR || (p->state == ST_NORMAL && !p->m1 && p->cY == 8))) {
        int any = 0; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SHIELD) { any = 1; break; }
        if (!any) combat_shield_create(p, 24, 2, 0);
    }
    (void)st0;
}

/* ---- M32: the bike's air jump (1779 M0 / 1799 M1 / 1913 M40 / 2157 M44 entries; group 2164-2172) ---- */
static int bike_air_jump_ok(const player_t *p, const input_t *in) {
    return p->bike && !p->ground && (in->pressed & BTN_JUMP) && !p->c_pavO && !p->water;   /* PAV_B.O < 1, OTHERS.H != 1 */
}
static void bike_air_group(player_t *p, const input_t *in) {
    p->cY = 8;                                                                  /* 2164: Y 8, BSK speed 100, energy -0.5, BSK.B -= 0.2 */
    p->energy = fmaxf(0.f, p->energy - 0.5f);
    p->bikeB -= 0.2;
    if (!p->c_pavO) {                                                           /* 2166: first frame (PAV_B.O < 1) */
        snd_play_orig(232, 220, 128); snd_play_orig(409, 220, 128);
        float wx, wy; bike_action_point(p, &wx, &wy); c_wburst(wx, wy);
        p->bikeB = 6; p->c_pavO = 1;
    }
    if (p->bikeB > 2 && p->vy > 1 - p->bikeB) p->vy = 1 - p->bikeB;           /* 2167: while BSK.B > 2, G = min(G, 1 - BSK.B) (thrust; RV b_airjump) */
    if (p->bikeB <= -1) { set_state(p, ST_NORMAL); p->m1 = 0; p->bikeB = 0; p->cY = 3; }   /* 2168 */
    if (p->ground) { p->c_nospd = 0; set_state(p, ST_NORMAL); p->m1 = 0; p->bikeB = 0; p->cY = 0; }   /* 2169: landed */
    if (in->pressed & BTN_ATTACK) c_start_claw(p, 1, 0);                        /* 2170 */
    if ((in->pressed & BTN_SPECIAL) && p->energy >= C_KICK_COST) c_start_kick(p, 1);   /* 2171 (stops sample 409) */
    int side = c_side(in);                                                      /* 2172: cling as 1793 */
    if (!p->ground && !p->water && side && !(in->held & BTN_JUMP) && c_wall_touch(side) && p->cM <= 0) c_start_wall(p);
}

/* ---- fuel cans (ObjectFuel_533, 3220-3228) ---- */
static int box_disc27(float bx0, float by0, float bw, float bh, int px, int py) {   /* box vs PLAYER's 27-px disc */
    float cx = fmaxf(bx0, fminf((float)px, bx0 + bw - 1)), cy = fmaxf(by0, fminf((float)py, by0 + bh - 1));
    float dx = cx - px, dy = cy - py;
    return dx * dx + dy * dy <= 13.5f * 13.5f;
}
static void fuel_events(player_t *p) {
    int carol_foot = chr_is_carol(G.character) && !p->bike;
    int PX = (int)floor(p->x), PY = (int)floor(p->y - ORIG_REF_DY);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_FUEL) continue;
        if (!carol_foot) { o->an.frame = 0; o->an.acc = 0; }                    /* 3220: frozen (3221 animates again for Carol on foot) */
        int E = (int)o->av[0];
        if (E == 0 && p->state != ST_DEAD && box_disc27(o->x - 12, o->y - 12, 24, 24, PX, PY)) {
            if (chr_is_carol(G.character) && !p->bike) {                        /* 3222: Carol gets the bike */
                double n = p->N, c = o_cos(n), sn = o_sin(n), Bv = p->ground ? p->gsp : p->vx, j = -PHYS_JUMP;
                p->vy = c * j - Bv * sn; p->vx = Bv * c + j * sn;
                p->ground = 0; p->N = 0; set_state(p, ST_NORMAL); p->m1 = 1; p->jumping = 1;
                snd_play_orig(126, 220, 128);
                p->cY = 8; p->c_anim_hold = 1;                                       /* (after the animation events: RV) */
                for (int k = 0; k < G.nobjs; k++)
                    if (G.o[k].alive && G.o[k].type == OT_PBIKE) { c_wburst(G.o[k].x, G.o[k].y); G.o[k].alive = 0; }
                p->cfl |= BIKE_LAND_POSE; p->bikeC = 3;
                if (p->invuln < 30) p->invuln = 30;
                o->av[0] = 1; bike_set(p, 1);
            } else if (p->bike) {                                                /* 3223: repair */
                p->bikeC = 3; o->av[0] = 1; if (p->invuln < 30) p->invuln = 30;
            }
        }
        E = (int)o->av[0];
        if (E == 1) { c_sparkle(o->x, o->y, 0); o->av[0] = 2; o->hidden = 1; }      /* 3224 */
        else if (E > 1 && G.frame % 7 == 2) {                                      /* 3225 */
            int r1 = (int)((fx_seed >> 27) & 15), r2 = fx_rand(33), r3 = (int)((fx_seed >> 27) & 15), r4 = fx_rand(33);
            c_sparkle(o->x + r1 - r2, o->y + r3 - r4, 1); o->av[0] = E + 1;
        }
        if (o->av[0] > 3) o->alive = 0;                                            /* 3226 */
    }
}

/* ---- jump pads (ObjectJumppad_539 / 1104): configuration (3564 + 4950-4957 / 5169-5180 / 5355-5366) ---- */
/* the per-pad configuration (index A, angle B, chain end D, lit E) is level data: level_def_t.pads (src/game/levels.c) */
#define PAD_A(o) ((o)->st)
#define PAD_D(o) ((o)->tm)
#define PAD_E(o) ((o)->av[0])
#define PAD_F(o) ((o)->av[1])
static double pad_B(const obj_t *o) { return o->av[2]; }
/* the pads' slots (per-type list: the per-frame passes and pad_by_A never scan every object) */
#define NPADS 64
static short pad_ix[NPADS]; static int npad;
void pads_spawn(void) {
    const padcfg_t *t = level_cur()->pads; int n = level_cur()->npads;
    npad = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type != OT_JUMPPAD) continue;
        o->st = -1; o->tm = 0; memset(o->av, 0, sizeof o->av);
        for (int k = 0; k < n; k++) if (t[k].x == (int)o->x && t[k].y == (int)o->y) {
            o->st = t[k].A; o->tm = t[k].D; o->av[0] = t[k].E; o->av[2] = t[k].B;
            o->dir = t[k].blk;                                                   /* DV4 block pads (anim 7) */
        }
        if (o->st < 0) o->alive = o->x >= 0 && o->y >= 0 ? 1 : 0;           /* DV1's single pad is off the map (inert) */
#ifdef FP_HOST_ENV
        if (o->st < 0 && o->alive) fprintf(stderr, "pads: %s pad at (%d, %d) has no level table row (inert, not drawn)\n", G.lvdir, (int)o->x, (int)o->y);
#endif
        o->layer = 2;
        if (o->st >= 0 && npad < NPADS) pad_ix[npad++] = (short)i;
    }
}
static void pad_point(const obj_t *o, float *x, float *y) {                 /* action point (32,-16) rotated by the pad angle */
    double a = pad_B(o) / 57.29577951308232;
    double s = pad_B(o) == 180. || pad_B(o) == 0. ? 0. : fd_sin(a), c = pad_B(o) == 90. || pad_B(o) == 270. ? 0. : fd_cos(a);
    *x = (float)(o->x + (-16.) * s); *y = (float)(o->y + (-16.) * c);
}
static obj_t *pad_by_A(int A) {
    for (int k = 0; k < npad; k++) { obj_t *o = &G.o[pad_ix[k]]; if (o->alive && o->type == OT_JUMPPAD && PAD_A(o) == A) return o; }
    return NULL;
}
static int pad_sensor(const obj_t *o, int s) {
    int ox, oy, h; const signed char *a, *b;
    return o->an.b && mv_sensor_rows(s, &ox, &oy, &a, &b, &h) && obj_rows_hit(o, ox, oy, a, b, h);
}
/* the ride entry (4946's actions; also RM3 6242 / 6243, the scripted Claw-chase rides of rm_claw.c): M = 48 from this pad */
void pad_ride_enter(player_t *p, obj_t *o) {
    set_state(p, ST_PAD); p->padD = p->x; p->padE = p->y - ORIG_REF_DY; p->padC = PAD_A(o);
    PAD_E(o) = -1; p->padfl &= ~(PADF_VOICE | PADF_OFF); p->padJ = 0;
}
/* pads after the state groups: Lilac's crystal pop (4944 / 3275 / 4945), Carol's entry (4946-4948 / 5364), re-arm 4949,
 * display 4943 */
static void pads_events(player_t *p, const input_t *in) {
    int carol = chr_is_carol(G.character), jp = (in->pressed & BTN_JUMP) != 0;
    if (level_cur()->rmg && level_cur()->rmg->pad_bike_pop) carol = carol && !p->bike;   /* RM 6092-6095 (level data): the bike pops, only Carol on foot rides */
    const bank_t *pb = &G.objs;
    int ajp = bank_anim(&G.objs, "jpad"), ajl = bank_anim(&G.objs, "jpad_lit"), ajb = bank_anim(&G.objs, "jpad_blk"), ajbl = -1;
    if (ens_anim("jpad_rm") >= 0) {                                                       /* RM handles 1402 / 1529: their own images (_ens bank) */
        pb = &G.ens; ajp = ens_anim("jpad_rm"); ajl = ens_anim("jpad_rm_lit"); ajb = ens_anim("jpad_rm7"); ajbl = ens_anim("jpad_rm7_lit");
    }
    int inert = level_cur()->pads_inert;
    for (int k = 0; k < npad; k++) {
        obj_t *o = &G.o[pad_ix[k]];
        if (!o->alive || o->type != OT_JUMPPAD || PAD_A(o) < 0) continue;
        double B = pad_B(o);
        if (inert) goto show;                                                    /* JC3: none of 4943-4949 (its pad keeps its start state) */
        if (!carol && jp && B == 0 && PAD_E(o) == 1 && PAD_F(o) == 0 && pad_sensor(o, SENS_BOTTOMBIG)) {   /* 4944 */
            PAD_E(o) = 0; PAD_F(o) = 1; float x, y; pad_point(o, &x, &y); c_wburst(x, y);
        }
        if (PAD_F(o) == 1 || PAD_F(o) == 11) { float x, y; pad_point(o, &x, &y); obj_pop_crystal(x, y); }   /* 3275 */
        if (PAD_F(o) > 0 && PAD_F(o) < 12) PAD_F(o) += 1;                                                    /* 4945 */
        if (carol && jp && p->state != ST_PAD && PAD_E(o) == 1 && PAD_D(o) == 0 &&
            (B == 0 ? pad_sensor(o, SENS_BOTTOMBIG) : B == -4.5 ? (!p->bike && pad_sensor(o, SENS_BOTTOMBIG)) :
                      (pad_sensor(o, SENS_RIGHTBIG) || pad_sensor(o, SENS_LEFTBIG)))) {                     /* 4946-4948 / 5364 */
            pad_ride_enter(p, o);
        }
        if (PAD_E(o) == -1 && p->state != ST_PAD) PAD_E(o) = 1;                                              /* 4949 */
    show:;
        int want = o->dir ? (PAD_E(o) == 1 && ajbl >= 0 ? ajbl : ajb) : PAD_E(o) == 1 ? ajl : ajp;          /* 4943 */
        if (want >= 0 && (o->an.b != pb || o->an.anim != want)) anim_set(&o->an, pb, want);
        o->ang = (float)B;
    }
}

/* ---- M48: the jump-pad ride (group 0x21a59: 2184-2196 / 4931-4935) ---- */
static void carol_pad_move(player_t *p, const input_t *in) {
    if (!(p->padfl & PADF_OFF)) {                       /* the first frame still moves with the jump (RV c_jumppad_dv2) */
        if (p->ground) c_ground_move(p, in, 1); else c_air_move(p, in);
        return;
    }
    /* MV.T = U = 0: no move. In flight (J = 2) L = 0 and the speed rules keep adding gravity to G (RV); on a pad the
     * sensor loops still run at the pad's angle (RV: 2 px push-out) and G restarts from 0 each frame (RV 0.25) */
    p->vx = p->gsp = 0;
    if (p->padJ == 2) { p->ground = 0; p->vy += PHYS_GRAV; return; }
    mv_t m = { p->x, p->y - ORIG_REF_DY, 0, 0, p->N, p->O, 0, p->ground, 0, 1, 1, 0 };
    mv_core(&m);
    p->x = m.X; p->y = m.Y + ORIG_REF_DY; p->ground = 0;
    p->vy = PHYS_GRAV;
}
static void carol_pad_group(player_t *p, const input_t *in) {
    int jp = (in->pressed & BTN_JUMP) != 0;
    obj_t *pc = pad_by_A(p->padC);
    if (p->padJ < 3) { p->cY = 8; p->c_spd_sk = p->c_spd_tl = 35; }                  /* 2184 */
    if (p->padJ >= 3) p->cY = 23;                                                      /* 2185 */
    if (p->padJ == 0) {                                                                /* 4931 */
        if (pc) { if (PAD_E(pc) > 0) PAD_E(pc) = 0; float x, y; pad_point(pc, &x, &y); c_wburst(x, y); }
        snd_play_orig(61, 220, 128);
        if (!(p->padfl & PADF_VOICE)) snd_play_orig(113, 255, 128);                  /* sound_go0 (+ stop 133) */
        obj_t *nx = pad_by_A(p->padC + 1);                                             /* 4932 */
        if (nx) { float x, y; PAD_E(nx) = 1; pad_point(nx, &x, &y); p->padF = x; p->padG = y; }
        p->padJ = 1;
    }
    if (p->padJ == 1) { p->cL = 0; p->padH = 0.05 * (p->padF - p->padD); p->padI = 0.05 * (p->padG - p->padE); p->padJ = 2; }   /* 2186 */
    if (p->padJ == 2) {
        p->ground = 0; p->padfl |= PADF_OFF;                                           /* 2188 */
        p->item_ovr = 1; p->item_x = p->x; p->item_y = p->y - ORIG_REF_DY;
        p->x += p->padH; p->y += p->padI;
        if (p->invuln < 2) p->invuln = 2;                                              /* E = max(3, E) (reads 2, as 2144) */
        p->padfl &= ~(PADF_X | PADF_Y); p->cL++;                                      /* 2189 */
        double X = p->x, Y = p->y - ORIG_REF_DY;
        if ((p->padH > 0 && X >= p->padF - 1) || (p->padH < 0 && X <= p->padF + 1)) p->padfl |= PADF_X;   /* 2190 */
        if ((p->padI > 0 && Y >= p->padG - 1) || (p->padI < 0 && Y <= p->padG + 1)) p->padfl |= PADF_Y;   /* 2191 */
        if (((p->padfl & PADF_X) && (p->padfl & PADF_Y)) || p->cL > 20) {            /* 2192 */
            p->x = p->padF; p->y = p->padG + ORIG_REF_DY; p->padJ = 3; p->padC++;
            snd_play_orig(33, 220, 128); p->padfl ^= PADF_VOICE; p->padfl &= ~(PADF_X | PADF_Y);
        }
    }
    pc = pad_by_A(p->padC);
    if (p->padJ >= 3 && pc) { p->N = p->O = pad_B(pc); p->ground = 1; p->padK = PAD_D(pc); }   /* 4933 */
    if (jp && p->padJ >= 3 && p->padJ <= 25 && p->padK != 1) { p->padD = p->x; p->padE = p->y - ORIG_REF_DY; p->padJ = 0; }   /* 2193 */
    if (p->padJ >= 3) p->padJ++;                                                       /* 2194 */
    if (jp && p->padJ >= 4 && p->padJ <= 26 && p->padK == 1 && pc) {                  /* 4934: launch from the last pad */
        double b = pad_B(pc), s = (b == 180. || b == 0.) ? 0. : fd_sin(b / 57.29577951308232), c = (b == 90. || b == 270.) ? 0. : fd_cos(b / 57.29577951308232);
        set_state(p, ST_NORMAL); p->m1 = 0; p->ground = 0; p->vx = -8.5 * s; p->vy = -8.5 * c;   /* (Y stays 23: M40 sets 40 next frame) */
        p->padfl &= ~PADF_OFF; PAD_E(pc) = 0; float x, y; pad_point(pc, &x, &y); c_wburst(x, y);
        snd_play_orig(61, 220, 128);                                                   /* RV: 61 at the launch */
        if (!(p->padfl & PADF_VOICE)) snd_play_orig(113, 255, 128);                  /* 4934 runs sound_go<PSC flag 8> too (RV c_jumppad_tb4:
                                                                                        * 113 at the launch; DV2's launch has the flag on) */
        p->N = p->O = 0; p->springing = 1;
        return;
    }
    if (p->padJ > 25 || (p->padJ >= 3 && (in->pressed & BTN_ATTACK))) {               /* 2195: drop off */
        p->padJ = -1; p->cY = 40; p->N = p->O = 0;
    }
    if (p->padJ == -1) {                                                               /* 4935 / 2196 */
        if (pc) PAD_E(pc) = 0;
        set_state(p, ST_NORMAL); p->m1 = 0; p->gsp = p->vx = p->vy = 0; p->ground = 0; p->padfl &= ~PADF_OFF;
    }
}
