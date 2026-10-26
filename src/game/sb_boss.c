/* Sky Battalion bosses and minibosses (session 19): spawn, instance lists, the rules every encounter shares, the dispatch to the
 * encounter files, and the act's exit plane (PlayerAssist_2205 + PlayerAssistCopilot_2220, 9737-9747 / 9774 / 9928) with the
 * GUICompass_2246 (9746 / 9747 / 9787).
 * Encounters (spec_sb_bosses 0, RE handle_frame_36..39 groups): SB1 Kujacker (sb_kujacker.c, everyone), SB2 Spade (sb_spade.c,
 * Lilac: 9598 MV.A == 0) or the chopper (sb_chopper.c, Carol / Milla: 9553 MV.A != 0), SB3 satellite (sb_satellite.c, everyone),
 * SB4 grappler + crystal spire (sb_grappler.c, everyone). The per-character split is the flow's class set (levels.c kills).
 * Order in the frame (RE): the test_collisions handlers (sb_boss_pre, before the shared combat events 2730-2767), then the
 * encounters' own event blocks (sb_boss_frame, after the gimmicks). Nothing tests a level name: every rule hangs on the placed
 * types. Performance (SH4): instance lists are built once at spawn; a level without these objects returns at the first test. */
#include "sb_boss.h"
#include <stdio.h>
#include <stdlib.h>

static const char *const SN[SS_N] = { "sbs_sat", "sbs_sat7", "sbs_sat8", "sbs_sat10", "sbs_arm", "sbs_arm7", "sbs_arm8", "sbs_arm10", "sbs_top", "sbs_thr",
    "sbs_beam2", "sbs_beam", "sba_plane", "sba_cop0", "sba_cop2", "sba_cop3", "sba_cop4", "sba_compass", "sba_dust" };
short sbb_sa[SS_N];
sbb_list_t sbb_l[SBB_NT];
int sbb_mly;
static int on;                                       /* the level places any of these types */
static short rails[16], bars[16], marks[4]; static int nrails, nbars;

/* ---- helpers */
obj_t *sbb_slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
obj_t *sbb_get(int type, int k) { const sbb_list_t *l = &sbb_l[type - OT_SBB_FIRST]; return k >= 0 && k < l->n ? sbb_slot(l->i[k]) : NULL; }
void sbb_set_a(obj_t *o, int k) { if (o && k >= 0 && sbb_sa[k] >= 0 && (o->an.b != &G.ens || o->an.anim != sbb_sa[k])) anim_set(&o->an, &G.ens, sbb_sa[k]); }
void sbb_restart_a(obj_t *o, int k) { if (o && k >= 0 && sbb_sa[k] >= 0) { anim_set(&o->an, &G.ens, sbb_sa[k]); anim_restart(&o->an); } }
int sbb_a_is(const obj_t *o, int k) { return o && k >= 0 && sbb_sa[k] >= 0 && o->an.b == &G.ens && o->an.anim == sbb_sa[k]; }
obj_t *sbb_spawn(int type, int sk, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    sbb_set_a(o, sk);
    return o;
}
static int dead(const player_t *p) { return p->state == ST_DEAD || p->hp <= 0; }
int sbb_vulnerable(const player_t *p) { return !dead(p) && !p->hurt_pending && !player_invincible(p); }
int sbb_hurt(player_t *p, int n) {                   /* MV.R = 1, LIFE.B += n, CAM.Z = max(CAM.Z, 3) (9895 / 9896 / 9926 ...) */
    if (!sbb_vulnerable(p) || player_orig_m(p) == 48) return 0;
    player_contact(p, n);
    if (G.cam.Z < 3) G.cam.Z = 3;
    return 1;
}
int sbb_csens_hit(const player_t *p, const obj_t *o) {                          /* PlayerCharacterSensor_375 (combat.c char_rect) */
    if (!o || !o->alive || !o->an.b) return 0;
    int X = (int)sbb_px(p), Y = (int)sbb_py(p), y0, y1;
    if (player_char_sensor_small(p)) { y0 = Y - 13; y1 = Y + 15; } else { y0 = Y - 34; y1 = Y + 13; }
    return obj_rect_hit(o, X - 7, y0, X + 9, y1);
}
int sbb_p326_hit(const obj_t *o) {
    int ox, oy, h; const signed char *x0, *x1;
    return o && o->alive && o->an.b && mv_sensor_rows(SENS_MAIN, &ox, &oy, &x0, &x1, &h) && obj_rows_hit(o, ox, oy, x0, x1, h);
}
float sbb_marker(int t, int x) { obj_t *o = t >= OT_CAMLIM_L && t <= OT_CAMLIM_B ? sbb_slot(marks[t - OT_CAMLIM_L]) : NULL; return o ? (x ? o->x : o->y) : -1e9f; }
int sbb_rails(void) { int n = 0; for (int k = 0; k < nrails; k++) { obj_t *o = sbb_slot(rails[k]); if (o && o->type == OT_WR_RAIL) n++; } return n; }
int sbb_bars(void) { int n = 0; for (int k = 0; k < nbars; k++) { obj_t *o = sbb_slot(bars[k]); if (o && o->type == OT_WR_BARRIER) n++; } return n; }
void sbb_draw(const obj_t *o, uint32_t col) {
    if (!o || !o->alive || !o->an.b || o->hidden) return;
    float sx = floorf(o->x - G.camx), sy = floorf(o->y - G.camy);
    if (sx < -300 || sx > SCREEN_W + 300 || sy < -300 || sy > SCREEN_H + 300) return;
    int inv = boss_flash(o);
    if (inv) r_set_invert(1);
    if (o->scale != 0 && o->scale != 1.f) r_set_scale(o->scale);
    frame_draw_rot(o->an.b, anim_frame(&o->an), sx, sy, o->ang, o->flip, col);
    r_set_scale(1.f);
    if (inv) r_set_invert(0);
}
void sbb_draw_yscaled(const obj_t *o, double ys) {
    if (!o || !o->alive || !o->an.b || o->hidden || ys <= 0) return;
    const frame_t *f = anim_frame(&o->an);
    float h = (float)(f->h * ys), sx = floorf(o->x - G.camx) - f->hx, sy = floorf(o->y - G.camy) - (float)(f->hy * ys);
    if (sx > SCREEN_W || sx + f->w < 0 || sy > SCREEN_H || sy + h < 0) return;
    frame_draw_scaled(o->an.b, f, sx, sy, (float)f->w, h, 0xffffffff);
}

/* ---- MasterLevel Y / music. ML.Y = 1 at the fight start; the frame's ControllerMusic then plays slot 4 (RV s1: m_midboss one
 * frame after 9880 in SB3; RV k2: m_boss3 in SB1). The flow owns ControllerMusic; this module plays the slot itself until it does. */
void sbb_fight_start(void) {
    if (sbb_mly == 1) return;
    sbb_mly = 1;
    const char *m = level_music_slot(4); if (m) music_play(m);
}
void sbb_music_fadeout(void) { music_fade(90, 0); }                              /* fadeout_music(1500) + lock_music */
int sbb_lock_edge(int *prev) { int l = level_cam_locked(), r = l && !*prev ? 1 : !l && *prev ? -1 : 0; *prev = l; return r; }
void sbb_compass_show(void) { obj_t *c = sbb_get(OT_SB_COMPASS, 0); if (c) { c->hidden = 0; c->av[0] = 0; } }
/* the arena end (9925; SB2 9597 / 9698, SB4 10112): two crystals per railing (y + 12, y + 36), sound 40, railings and barriers
 * destroyed, ML.Y 0, the compass shown; the flow sets the ML flag, releases the camera and plays the stage track */
void sbb_arena_end(int crystals, int flag) {
    int ca = bank_anim(&G.objs, "objectcrystal_3");
    for (int k = 0; k < nrails; k++) {
        obj_t *r = sbb_slot(rails[k]);
        if (!r || r->type != OT_WR_RAIL) continue;
        for (int j = 0; crystals && j < 2; j++) {
            obj_t *c = obj_alloc(); if (!c) break; memset(c, 0, sizeof *c);       /* ObjectCrystal_317 (static), move_back */
            c->type = OT_CRYSTAL; c->alive = 1; c->hp = 1; c->layer = r->layer; c->x = c->ox = r->x; c->y = c->oy = r->y + (j ? 36 : 12);
            if (ca >= 0) anim_set(&c->an, &G.objs, ca);
        }
    }
    snd_play_orig(40, 220, 128);
    for (int k = 0; k < nrails; k++) { obj_t *r = sbb_slot(rails[k]); if (r && r->type == OT_WR_RAIL) r->alive = 0; }
    for (int k = 0; k < nbars; k++) { obj_t *b = sbb_slot(bars[k]); if (b && b->type == OT_WR_BARRIER) b->alive = 0; }
    sbb_mly = 0;
    music_fade(0, 0);
    sb_flow_ship_done(flag);                                                    /* ML flag, CAM.M 2 / U 0, play_music(5) (sb_flow.c) */
    sbb_compass_show();
}

/* ======================================================================== the exit plane (SB2-SB4, after the boss)
 * PlayerAssist alterables (RE 9738-9745, 9774): A / B its centre, C the bob phase, D / E its velocity, G state (0 parked, 1 flying in,
 * 2 alongside the player, 3 boarded), I the destroyed cannon bases (9319, the gimmicks: sb_boss_cannon_down). */
static struct { double A, B, C, D, E, G, I, F, H; int boarded, cdir; double cA, cB, cC; } PL;
void sb_boss_cannon_down(void) { PL.I += 1; }                                    /* 9319: GimmickCannonBase destroyed */
static int cop_dir(void) {                                                       /* 9743: copilot direction = MV.A */
    int a = G.p.bike ? CHR_CAROL_BIKE : G.character;
    return a == CHR_CAROL ? SS_COP2 : a == CHR_CAROL_BIKE ? SS_COP3 : a == CHR_MILLA ? SS_COP4 : SS_COP0;
}
static void plane_frame(player_t *p) {
    obj_t *pl = sbb_get(OT_SB_ASSIST, 0), *cp = sbb_get(OT_SB_COPILOT, 0);
    if (!pl) return;
#ifdef FP_HOST_ENV
    { static int once; const char *e = getenv("FP_SB_ASSIST_I"); if (e && !once && G.frame >= atoi(e)) { once = 1; PL.I = 2; } }   /* tests: the cannons */
#endif
    if (PL.G == 0 && PL.I >= 2) { PL.D = 5; PL.G = 1; PL.A = G.camx - 500; pl->hidden = 0; }   /* 9738 */
    if (PL.G == 1 && sbb_px(p) <= PL.A) PL.G = 2;                               /* 9739 */
    if (PL.G == 2) PL.A = sbb_px(p);                                            /* 9740 */
    pl->x = (float)(int)(PL.A + 32 * sbb_sin(PL.C));                            /* 9741 */
    pl->y = (float)(int)(PL.B + 8 * sbb_cos(2 * PL.C + 90));
    PL.C = sbb_fmod360(PL.C + 0.7);
    if (cp) { cp->x = pl->x; cp->y = pl->y; }
    PL.A += PL.D; PL.B += PL.E;
    /* 9742: dust behind the plane only while its direction is 1 (it keeps 0 here; 31 is the G1 == 5 variant, not ported) */
    if (cp && PL.G >= 1 && PL.G <= 2 && p->vy >= 0 && sbb_p326_hit(pl) && cp->y - 15 <= sbb_py(p)) {   /* 9743 */
        PL.G = 3; PL.boarded = 1;
        p->vy = 0; p->vx = 0; p->gsp = 0; p->scripted = 1;                       /* MV.G 0, MV.B 0, MV.M 43 */
        sbb_set_a(cp, cop_dir());                                               /* copilot direction = MV.A */
    }
    if (PL.boarded && cp) cp->hidden = 0;                                        /* 9744 / 9928: the skins hidden, the copilot shown */
    if (PL.G == 3) {                                                            /* 9745 */
        PL.D = fmin(PL.D + 0.1, 16);
        if (cp) { p->x = cp->x; p->y = cp->y + ORIG_REF_DY; }
        p->ground = 0; p->vx = PL.D; p->gsp = 0; p->face = 1;                   /* MV.B > 0 for the right-edge exit (9455; INFERRED: M43 moves right) */
    }
    pl->av[0] = PL.G; pl->av[1] = PL.I; pl->av[2] = PL.A; pl->av[3] = PL.D;
}
static void compass_frame(void) {                                                /* 9746 / 9747 */
    obj_t *c = sbb_get(OT_SB_COMPASS, 0);
    if (!c || c->hidden) return;
    c->av[0] += 1;
    double x = fmax(fmin(c->av[2], G.camx + SCREEN_W - 32), G.camx + 32);
    c->x = (float)(int)x; c->y = (float)(int)(0.5 * c->av[0] + c->av[1]);
    int a = (0x80 - (int)(4 * c->av[0])) * 2; c->av[3] = a > 255 ? 255 : a < 0 ? 0 : a;
    if (c->av[0] >= 40) c->av[0] -= 40;
}
int sb_boss_player_hidden(void) { return on && PL.boarded; }
int sb_boss_ml_y(void) { return sbb_mly; }

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&PL, sizeof PL); snap_register(&sbb_mly, sizeof sbb_mly); } }
void sb_boss_spawn(void) {
    snap_once();
    memset(sbb_l, 0, sizeof sbb_l); memset(&PL, 0, sizeof PL); sbb_mly = 0; nrails = nbars = 0;
    marks[0] = marks[1] = marks[2] = marks[3] = -1;
    on = 0;
    for (int t = OT_SBB_FIRST; t <= OT_SBB_LAST && !on; t++) if (objects_have(t)) on = 1;
    if (!on) return;
    for (int k = 0; k < SS_N; k++) sbb_sa[k] = (short)ens_anim(SN[k]);
    /* lvobj anim per object: the k-th object of a type is the k-th placement of that type (objects_spawn keeps the order) */
    static uint16_t AN[SBB_NT][SBB_LMAX]; int na[SBB_NT] = { 0 }, seen[SBB_NT] = { 0 };
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - OT_SBB_FIRST; if (t >= 0 && t < SBB_NT && na[t] < SBB_LMAX) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if (t == OT_WR_RAIL && nrails < 16) rails[nrails++] = (short)i;
        if (t == OT_WR_BARRIER && nbars < 16) bars[nbars++] = (short)i;
        if (t >= OT_CAMLIM_L && t <= OT_CAMLIM_B && marks[t - OT_CAMLIM_L] < 0) marks[t - OT_CAMLIM_L] = (short)i;
        if (t < OT_SBB_FIRST || t > OT_SBB_LAST) continue;
        int ti = t - OT_SBB_FIRST, an = seen[ti] < na[ti] ? AN[ti][seen[ti]] : 0; seen[ti]++;
        o->par = an & 0xff; o->layer = (an >> 8) & 0xff; o->st = 0; o->tm = 0; o->c = 0; o->fl = 0; o->dir = 0; o->ang = 0; o->scale = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL; o->bd = 0;
        sbb_list_t *l = &sbb_l[ti]; if (l->n < SBB_LMAX) l->i[l->n++] = (short)i;
        switch (t) {
        case OT_SB_ASSIST: sbb_set_a(o, SS_PLANE); PL.A = PL.H = o->x; PL.B = PL.F = o->y; break;   /* 9774 */
        case OT_SB_COPILOT: sbb_set_a(o, SS_COP0); break;
        case OT_SB_COMPASS: sbb_set_a(o, SS_COMPASS); o->av[1] = o->y; o->av[2] = o->x; o->layer = 4; break;   /* 9787: B = Y, C = X */
        }
    }
    sbs_spawn(); sbk_spawn(); sbc_spawn(); sbp_spawn(); sbg_spawn();
}
void sb_boss_pre(player_t *p) { if (!on) return; sbs_pre(p); sbk_pre(p); sbc_pre(p); sbp_pre(p); sbg_pre(p); }
void sb_boss_frame(player_t *p) {
    if (!on) return;
    sbk_frame(p); sbc_frame(p); sbp_frame(p); sbs_frame(p); sbg_frame(p);   /* the bosses' blocks, then the plane 9737-9747 (RE: handle_frame_39 order) */
    plane_frame(p); compass_frame();
}
int sb_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }           /* every rule runs in sb_boss_frame */
int sb_boss_q20(int t) { return t == OT_SB_SAT || t == OT_SB_KJ_HEAD || t == OT_SB_KJ_TARGET || t == OT_SB_CH || t == OT_SB_SP_HIT || t == OT_SB_GR; }
int sb_boss_input(uint32_t *held) { (void)held; return 0; }
int sb_boss_draw_obj(const obj_t *o, int pass) {
    if (sbs_draw(o, pass) || sbk_draw(o, pass) || sbc_draw(o, pass) || sbp_draw(o, pass) || sbg_draw(o, pass)) return 1;
    if (o->type == OT_SB_COMPASS) { if (!o->hidden) sbb_draw(o, ((uint32_t)o->av[3] << 24) | 0xffffff); return 1; }
    if (o->type == OT_SB_COPILOT || o->type == OT_SB_ASSIST) { sbb_draw(o, 0xffffffff); return 1; }
    return 0;
}
