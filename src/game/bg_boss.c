/* Battle Glacier bosses and arena pieces (session 24): types 2480-2559; spawn, instance lists, the rules the encounters share, the
 * dispatch to the encounter files. Spec: build/re/spec_bg_bosses.md.
 * Encounters (RE handle_frame_60..65 groups, MV.A = the character: 0 Lilac, 2 Carol, 3 bike-Carol, 4 Milla):
 *  - BG1-BG3: the MinibossTredd rigs (bosses.c, the DV engine type: events 5253-5267 shared) - no code here.
 *  - BG2: the chopper (sb_chopper.c, BG2 variant: Lilac / Carol only, handle_frame_61 inline trigger X >= 2201, MV.A != 4, g4 < 2).
 *  - BG2 / BG3 / BG4: the Brevon mech in the background and its lasers (bg_boss_mech.c, 14212-14227 / 14314-14320 / 14503-14512).
 *  - BG4: Absolution (bg_boss_mech.c, group 0x22af2, 14418-14502) at the act start, then the Sparky boss (bg_boss_sparky.c, group
 *    0x22e8b, 14513-14549) in the CameraLimitBoss room.
 *  - BG5: the MinibossWretchnid (bosses.c, DV1 events 4844-4871 shared) and Milla's Sparky ride (bg_boss_sparky.c, 14681-14716;
 *    14726: the Sparky is destroyed unless MV.A == 4).
 *  - BG6: Prince Dail on the Shade Beast (bg_boss_dail.c, 14903-15136).
 * Order in the frame (RE): the boss events run after the gimmicks (bg_boss_frame from game.c). Nothing tests a level name: every rule
 * hangs on the placed types. Performance (SH4): instance lists are built once at spawn; a level without these types returns at once. */
#include "bg_boss.h"
#include <stdio.h>
#include <stdlib.h>

bgb_list_t bgb_l[BGB_NT];
static int on;                                       /* the level places any of these types (or the BG2 mech background) */
#define RMAX 160
static short R[RMAX]; static int nR;                 /* runtime objects (2510-2559) */
static short rails[48], bars[64], marks[4]; static int nrails, nbars;

/* ---- helpers */
obj_t *bgb_slot(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
obj_t *bgb_get(int type, int k) {                                              /* the slot may have been reused since: check the type */
    const bgb_list_t *l = &bgb_l[type - OT_BGB_SPARKY]; obj_t *o = k >= 0 && k < l->n ? bgb_slot(l->i[k]) : NULL;
    return o && o->type == type ? o : NULL;
}
void bgb_set_i(obj_t *o, int a) { if (o && a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
void bgb_set(obj_t *o, const char *row) { if (o && row) bgb_set_i(o, ens_anim(row)); }
int bgb_is(const obj_t *o, const char *row) { int a = row ? ens_anim(row) : -1; return o && a >= 0 && o->an.b == &G.ens && o->an.anim == a; }
obj_t *bgb_spawn(int type, const char *row, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = o->fx = x; o->y = o->oy = o->fy = y; o->layer = layer; o->born = G.frame;
    if (row) bgb_set(o, row);
    short i = (short)(o - G.o); int k;
    for (k = 0; k < nR && R[k] != i; k++) ;
    if (k == nR) { if (nR < RMAX) R[nR++] = i; else o->alive = 0; }
    return o;
}
static void r_compact(void) {
    int n = 0;
    for (int k = 0; k < nR; k++) { const obj_t *o = &G.o[R[k]]; if (o->alive && o->type >= OT_BGB_RT && o->type <= OT_BGB_END) R[n++] = R[k]; }
    nR = n;
}
int bgb_runtime_n(void) { return nR; }
obj_t *bgb_runtime(int k) { if (k < 0 || k >= nR) return NULL; obj_t *o = &G.o[R[k]]; return o->alive && o->type >= OT_BGB_RT && o->type <= OT_BGB_END ? o : NULL; }
int bgb_g3(void) { return G.frame; }
void bgb_explode_snd(void) { static const int SX[3] = { 311, 319, 320 }; snd_play_orig(SX[cross_rand(3)], 220, 128); }   /* "sound_explode" + (3 + R(3)) */
int bgb_milla(void) { return chr_is_milla(G.character); }
void bgb_act_point(const obj_t *o, float *ax, float *ay) {
    *ax = o ? o->x : 0; *ay = o ? o->y : 0;
    if (!o || !o->an.b || !o->an.b->act) return;
    const bank_t *b = o->an.b; const frame_t *f = anim_frame(&o->an); int fi = (int)(f - b->frames);
    double dx = b->act[2 * fi] - f->hx, dy = b->act[2 * fi + 1] - f->hy, a = o->ang;
    if (o->flip) dx = -dx;
    double c = sbb_cos(a), s = sbb_sin(a);
    *ax = floorf(o->x) + (float)(int)floor(dx * c + dy * s + 0.5); *ay = floorf(o->y) + (float)(int)floor(-dx * s + dy * c + 0.5);
}
float bgb_marker(int t, int x) { obj_t *o = t >= OT_CAMLIM_L && t <= OT_CAMLIM_B ? bgb_slot(marks[t - OT_CAMLIM_L]) : NULL; return o && o->type == t ? (x ? o->x : o->y) : -1e9f; }
void bgb_arena_open(float x0, float x1) {
    int ca = bank_anim(&G.objs, "objectcrystal_3");
    for (int k = 0; k < nrails; k++) {
        obj_t *r = bgb_slot(rails[k]);
        if (!r || r->type != OT_WR_RAIL || r->x < x0 || r->x > x1) continue;
        for (int j = 0; j < 2; j++) {
            obj_t *c = obj_alloc(); if (!c) break; memset(c, 0, sizeof *c);       /* ObjectCrystal_317 (static), move_back */
            c->type = OT_CRYSTAL; c->alive = 1; c->hp = 1; c->layer = r->layer; c->x = c->ox = r->x; c->y = c->oy = r->y + (j ? 36 : 12);
            if (ca >= 0) anim_set(&c->an, &G.objs, ca);
        }
    }
    snd_play_orig(40, 220, 128);
    for (int k = 0; k < nrails; k++) { obj_t *r = bgb_slot(rails[k]); if (r && r->type == OT_WR_RAIL && r->x >= x0 && r->x <= x1) r->alive = 0; }
    for (int k = 0; k < nbars; k++) { obj_t *b = bgb_slot(bars[k]); if (b && b->type == OT_WR_BARRIER && b->x >= x0 && b->x <= x1) b->alive = 0; }
}

/* ======================================================================== spawn / dispatch */
static void snap_once(void) { static int reg; if (!reg) { reg = 1; snap_register(&R, sizeof R); snap_register(&nR, sizeof nR); } }
void bg_boss_spawn(void) {
    snap_once();
    memset(bgb_l, 0, sizeof bgb_l); nR = 0; nrails = nbars = 0; marks[0] = marks[1] = marks[2] = marks[3] = -1;
    on = 0;
    for (int j = 0; j < G.lvl.nobj && !on; j++) if (G.lvl.objs[j].type >= OT_BGB_SPARKY && G.lvl.objs[j].type <= OT_BGB_PLACED_LAST) on = 1;
    if (!on) return;
    /* lvobj anim: instance layer << 8 (tools/import_frame.py BG_BOSS); the k-th object of a type is its k-th placement */
    static uint16_t AN[BGB_NT][BGB_LMAX]; static short na[BGB_NT], seen[BGB_NT];
    memset(na, 0, sizeof na); memset(seen, 0, sizeof seen);
    for (int j = 0; j < G.lvl.nobj; j++) { int t = G.lvl.objs[j].type - OT_BGB_SPARKY; if (t >= 0 && t < BGB_NT && na[t] < BGB_LMAX) AN[t][na[t]++] = G.lvl.objs[j].anim; }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        int t = o->type;
        if (t == OT_WR_RAIL && nrails < 48) rails[nrails++] = (short)i;
        if (t == OT_WR_BARRIER && nbars < 64) bars[nbars++] = (short)i;
        if (t >= OT_CAMLIM_L && t <= OT_CAMLIM_B && marks[t - OT_CAMLIM_L] < 0) marks[t - OT_CAMLIM_L] = (short)i;
        if (t < OT_BGB_SPARKY || t > OT_BGB_PLACED_LAST) continue;
        int ti = t - OT_BGB_SPARKY, an = seen[ti] < na[ti] ? AN[ti][seen[ti]] : (3 << 8); seen[ti]++;
        o->layer = (an >> 8) & 0x7f; o->st = o->tm = o->c = o->fl = o->dir = 0; o->ang = 0; o->scale = 0; o->bd = 0;
        memset(o->av, 0, sizeof o->av); o->hidden = (objdef(t)->flags & OD_HIDDEN) != 0; o->an.b = NULL;
        bgb_list_t *l = &bgb_l[ti]; if (l->n < BGB_LMAX) l->i[l->n++] = (short)i;
    }
    bgs_spawn(); bgm_spawn(); bgd_spawn();
}
void bg_boss_pre(player_t *p) { (void)p; if (!on) return; r_compact(); }
void bg_boss_frame(player_t *p) {
    if (!on) return;
    for (int k = 0; k < nR; k++) {                                              /* effects: one-shot, or moving for av[6] frames */
        obj_t *o = &G.o[R[k]];
        if (!o->alive || o->type != OT_BGB_FX) continue;
        if (o->av[6] > 0) { o->x += o->vx; o->y += o->vy; if (--o->av[6] <= 0) o->alive = 0; }
        else if (!o->an.b || o->an.done) o->alive = 0;
    }
    bgm_frame(p);                                    /* RE handle_frame_63: Absolution (14418-14512) before the Sparky room (14513-14549) */
    bgs_frame(p);
    bgd_frame(p);
}
int  bg_boss_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }        /* every rule runs in bg_boss_frame */
int  bg_boss_q20(int type) { return type == OT_BGB_SPARKY || type == OT_BGB_M_WEAK || type == OT_BGB_DAIL; }   /* + BG6 Dail (frame 65 Q20) */   /* RE qualifier 0x219f8 (14402: Sparky, the mech's weak spot) */
int  bg_boss_draw_obj(const obj_t *o, int pass) {
    if (!on) return 1;
    if (bgs_draw(o, pass) || bgm_draw(o, pass) || bgd_draw(o, pass)) return 1;
    if (o->hidden) return 1;
    sbb_draw(o, 0xffffffff);
    return 1;
}
#ifdef FP_HOST_ENV
/* host tests only (FP_BGB_BOT=1): a boss-fight bot that plays with each character's normal moves (no state edits): it walks under
 * the current target (the Sparky, the mech's weak spot, Dail's hit box), jumps and attacks near the apex. Lilac / Carol: jump +
 * attack; Milla: jump + a shield burst aimed up (spec_milla_combat: special held then released, 4 damage), or the attack on the ground. */

static int bot(uint32_t *held) {
    static int en = -1, t, ph; if (en < 0) en = getenv("FP_BGB_BOT") != NULL;
    if (!en || G.mode != MODE_PLAY || G.p.hp <= 0) return 0;
    obj_t *tg = bgm_bot_target();
    if (!tg) tg = bgd_bot_target();
    if (!tg) { obj_t *b = bgb_get(OT_BGB_SPARKY, 0); if (b && !(b->fl & BGB_DEF) && !b->hidden && bg_boss_sparky_state() >= 0 && bg_boss_sparky_state() < 6) tg = b; }
    if (!tg) { obj_t *c = sbb_get(OT_SB_CH, 0); if (c && c->av[7] == 2 && !(c->fl & BGB_DEF) && !c->hidden) tg = c; }   /* BG2: the chopper (sb_chopper.c) */
    if (!tg) for (int i = 0; i < G.nobjs && !tg; i++) {                      /* BG5: the Wretchnid's weak spot (bosses.c; host scan) */
        obj_t *o = &G.o[i];
        if (o->alive && o->type == OT_WR_WEAK && o->A > 0 && fabs(o->x - G.p.x) < 400 && fabs(o->y - G.p.y) < 300) tg = o;
    }
    if (!tg) { ph = 0; return 0; }
    const player_t *p = &G.p;
    double tx = tg->x, ty = tg->y;                                             /* the target image's centre */
    if (tg->an.b) { const frame_t *f = anim_frame(&tg->an); tx = floorf(tg->x) - (tg->flip ? f->w - 1 - f->hx : f->hx) + f->w / 2; ty = floorf(tg->y) - f->hy + f->h / 2; }
    if (level_cam_locked()) {                                                  /* stay off the room's edges (BG4: the exit's anti-grav column) */
        float l = bgb_marker(OT_CAMLIM_L, 1), r = bgb_marker(OT_CAMLIM_R, 1);
        if (l > -1e8f && tx < l + 170) tx = l + 170;
        if (r > -1e8f && tx > r - 170) tx = r - 170;
    }
    double dx = tx - p->x, dy = ty - sbb_py(p);
    uint32_t b = 0;
    if (p->state == ST_RIDE) { *held = (t++ & 8) ? BTN_ATTACK : 0; ph = 0; return 1; }   /* a magnet / slide caught the bot: let go (14390) */
    int milla = bgb_milla(), carol = G.character == CHR_CAROL;
    t++;
    switch (ph) {
    case 0:                                                                    /* walk under the target */
        if (dx > 10) b |= BTN_RIGHT; else if (dx < -10) b |= BTN_LEFT;
        if (fabs(dx) < 40 && p->ground && fabs(p->gsp) < 3) { ph = 1; t = 0; }
        break;
    case 1:                                                                    /* full jump, steer toward it */
        b |= BTN_JUMP;
        if (dx > 4) b |= BTN_RIGHT; else if (dx < -4) b |= BTN_LEFT;
        if (t > 4 && (p->vy >= -1.5f || dy > -24)) { ph = 2; t = 0; }
        if (t > 40) { ph = 2; t = 0; }
        break;
    case 2:                                                                    /* attack at the apex */
        if (milla) { b |= BTN_SPECIAL | BTN_UP; if (dx > 0) b |= BTN_RIGHT; else b |= BTN_LEFT; if (t > 6) { ph = 3; t = 0; } }
        else { if (carol) b |= dx > 0 ? BTN_RIGHT : BTN_LEFT; b |= BTN_ATTACK; ph = 3; t = 0; }
        break;
    default:                                                                   /* release and land */
        if (t > 4 && p->ground) { ph = 0; t = 0; }
        if (t > 90) { ph = 0; t = 0; }
        break;
    }
    *held = b;
    return 1;
}
#else
static int bot(uint32_t *held) { (void)held; return 0; }
#endif
int   bg_boss_input(uint32_t *held) { return on && (bgs_input(held) || bgd_input(held) || bot(held)); }
int   bg_boss_player_hidden(void) { return on && (bgs_player_hidden() || bgd_player_hidden()); }
float bg_boss_hud_dy(void) { return on ? bgm_hud_dy() + bgd_hud_dy() : 0.f; }
