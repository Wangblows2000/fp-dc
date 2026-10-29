/* FD3b (json 71): the Kingdom Stone room. Spec: build/re/spec_fd_bosses_b.md part B (RE handle_frame_71_events 16236-16654;
 * RV build/orig_traces/fd_bosses_b b2_*). Labels: RE read from code, RV measured, INFERRED reasoned.
 *  - The scene (group 0x23059, KingdomStone_3680 D 0..8 with E its timer): RE 16237 starts it when MV.Y > 1024, |X - stone.x| < 200
 *    and the character is not Milla (Lilac / Carol; Classic and Adventure): CTRL.M 1 (the script drives the player), CAM.M 3, the music
 *    fades (2000 ms). D0 the camera pans to the stone (CAM.A + 3), D1 the player walks right (MV.B 5) to stone.x - 63, D2 the terminals
 *    and the room flicker (E 70-240) and Brevon appears (E 380), D3 the camera pans back to Brevon + 160, D4 the knife arm (E 10-33), D5
 *    the player walks right past the device, D6 the device flies to Brevon and bursts (E 5-180), Brevon laughs (E 210), rises (E 340),
 *    the player walks back to Milla (E 370), D7 Milla transforms (bubbles every 10 frames, the fade at E 140, E 500 -> D8), D8 the
 *    mutated Milla appears (E 60), the camera returns (E 130, ML.Y 1: the boss music), E 170: the fight (16394: tailend D 2).
 *    Port: the same timeline (RE frame counts); the player's poses of the scene (OTHERS.Y 40 skin rows) and Milla's NPC sprite are not
 *    drawn (Milla's bank is only resident when she plays; INFERRED: the bubbles and the fade carry the transformation).
 *  - The fight (group 0x2305c, BossMMillaTailEnd_3696 D the state, BossMMillaHead_3694 the Q20 head, A 60, F_NOFLY): the head runs
 *    along the floor between stone.x - 200 and stone.x + 200 (16530 / 16531: the turn points, E + 50 M), accelerating 0.2 a frame to
 *    4 px (16553); after four runs (tailend G 4, 16525) it stops and fires bounce shots (BossBounceShot_3699: 16545 / 16546, two per
 *    volley; direction (M - 1) 8 - E M, E the shots left) that bounce off the floor, hurt the player (5, 16563) and fly back at the
 *    head when the player hits them (16562: A 9; INFERRED: 10 damage to the head); D5 recovers (F + 6 to the head's D), D0 again.
 *    Contact with the head hurts (INFERRED 10, the bulldozer's claw value). The head's pose blends (H / I / J / K) drive the parts
 *    (16513-16522); the port moves the parts by fixed offsets from their placements and a two-phase gait (INFERRED).
 *  - Defeat (16576 flag 6): tailend D 60 (explosions, the music stops: 16581-16593), D 61 the fade (F 0..120) and Milla's body
 *    (NPCMillaBody_3704) where the head fell, D 62 the player walks to her, D 63 the end scene (voices), F 500: Sign.B 179 -> the
 *    results (results_begin_boss; the level's .res_wipe ends the act -> FD4a).
 *  - Milla (character 4): 16237 returns for her (no scene, no fight). 16395 (g0 != 2, Classic): once she is past the stone (MV.X >
 *    stone.x, MV.Y > 1024) CTRL.M 1, the music fades, CTRL.D 1 for 90 frames (she walks on), E 90 Sign.B 179: the results (RV b2_4: the
 *    tally at f430, FD4a at f994). 16396 (Adventure: another scene) is not ported. */
#include "fd_boss_r34.h"
#include <stdio.h>
#include <stdlib.h>

enum { MM_FOOTBACK, MM_ARMBACK, MM_PAWBACK, MM_EARBACK, MM_BODY, MM_LEG, MM_ARMFRONT, MM_FOOT, MM_PAWFRONT, MM_TAILEND, MM_EARFRONT, MM_NSUB };
static const char *const MM_ROW[MM_NSUB] = { "fdm_footback", "fdm_armback", "fdm_pawback", "fdm_earback", "fdm_body", "fdm_leg",
                                             "fdm_armfront", "fdm_foot", "fdm_pawfront", "fdm_tailend", "fdm_earfront" };
#define MAXP 24
#define NSHOT 8
static struct {
    int on, scene, D, E, fight, done, results;
    int stoneX, stoneY, brx, bry;
    float camA, camB;
    /* the fight */
    int st, t, runs, dirm, shots, volley, defF, hit_c;
    double hx, hy, base_y, vx, vy, phase, hop;
    short part[MAXP]; float px0[MAXP], py0[MAXP]; int npart;
    short shot[NSHOT];
    int head_x0, head_y0;
    int fade;                                           /* 0..255 black overlay */
    int lives, inv, mpath;                                     /* the head's O (5 lives of A 60) and C (90 frames of invulnerability) */
    int music;
} S;

static obj_t *stone(void) { return fdb_get(OT_FDB_STONE, 0); }
static obj_t *brevon(void) { return fdb_get(OT_FDB_BREVON, 0); }
static obj_t *head(void) { return fdb_get(OT_FDB_MM_HEAD, 0); }
static obj_t *knife(void) { return fdb_get(OT_FDB_KNIFE, 0); }
static obj_t *device(void) { return fdb_get(OT_FDB_DEVICE, 0); }
static obj_t *mbody(void) { return fdb_get(OT_FDB_MBODY, 0); }
int fd3b_boss_state(void) { return S.on ? (S.results ? 9 : S.fight ? 2 + (S.done ? 1 : 0) : S.scene ? 1 : 0) : -1; }

void f3b_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&S, sizeof S); }
    memset(&S, 0, sizeof S);
    for (int k = 0; k < NSHOT; k++) S.shot[k] = -1;
    obj_t *s = stone(), *h = head();
    if (!s || !h) return;
    S.on = 1; S.stoneX = (int)s->x; S.stoneY = (int)s->y;
    fdb_set(s, "fdb_stone");
    obj_t *b = brevon(); if (b) { S.brx = (int)b->x; S.bry = (int)b->y; fdb_set(b, "fdb_brv0"); b->hidden = 1; }   /* RE 16331: shown at E 380 */
    obj_t *k = knife(); if (k) { fdb_set(k, "fdb_knife"); k->hidden = 1; }
    obj_t *d = device(); if (d) fdb_set(d, "fdb_device");
    for (int i = 0; i < fdb_count(OT_FDB_TERM); i++) { obj_t *t = fdb_get(OT_FDB_TERM, i); if (t) fdb_set(t, t->par == 1 ? "fdb_term1" : t->par == 2 ? "fdb_term2" : "fdb_term0"); }
    obj_t *mb = mbody(); if (mb) { fdb_set(mb, "fdb_mbody"); mb->hidden = 1; }
    for (int i = 0; i < fdb_count(OT_FDB_HATCH); i++) { obj_t *t = fdb_get(OT_FDB_HATCH, i); if (t) fdb_set(t, "fdb_hatch"); }
    /* the rig: every part keeps its placement offset from the head (RE placements 13008-13120, 1198-1292) */
    S.head_x0 = (int)h->x; S.head_y0 = (int)h->y; S.hx = h->x; S.hy = h->y; S.base_y = h->y;
    fdb_set(h, "fdm_head"); h->A = 60; h->fl |= F_NOFLY; h->hidden = 1; h->bd = 0; S.lives = 5;
#ifdef FP_HOST_ENV
    if (getenv("FP_F3B_LIVES")) S.lives = atoi(getenv("FP_F3B_LIVES"));          /* host tests: a shorter fight */
#endif
    /* ctor A 60, H 1, M 1, O 5 (RE BossMMillaHead_3694) */
    S.dirm = 1;
    for (int t = OT_FDB_MM_PART; t <= OT_FDB_MM_TAIL; t++)
        for (int i = 0; i < fdb_count(t) && S.npart < MAXP; i++) {
            obj_t *o = fdb_get(t, i); if (!o) continue;
            if (t == OT_FDB_MM_PART && o->par < MM_NSUB) fdb_set(o, MM_ROW[o->par]); else if (t == OT_FDB_MM_TAIL) fdb_set(o, "fdm_tail");
            o->hidden = 1;
            S.part[S.npart] = (short)(o - G.o); S.px0[S.npart] = o->x - h->x; S.py0[S.npart] = o->y - h->y; S.npart++;
        }
}

/* ---- scripted player: walk at MV.B = b (0: stand) */
static void walk(player_t *p, double b) {
    if (p->ground) { p->gsp = (float)b; if (b) p->face = b > 0 ? 1 : -1; }
}

static void show_boss(int on) {
    obj_t *h = head(); if (h) h->hidden = !on;
    for (int k = 0; k < S.npart; k++) { obj_t *o = fdb_slot(S.part[k]); if (o) o->hidden = !on; }
}

/* ======================================================================== the scene (RE 16237-16394) */
static void scene_start(player_t *p) {
    S.scene = 1; S.D = 0; S.E = 0;
    fdb_ctrl = 1; fdb_keys = 0; walk(p, 0);
    music_fade(120, 0);                                                         /* fadeout_music(2000) + unqueue + lock */
    G.cam.M3 = 1; S.camA = G.cam.A; S.camB = G.cam.B;
}
static void scene_frame(player_t *p) {
    obj_t *b = brevon(), *k = knife(), *d = device(), *h = head();
    float X = p->x;
    G.cam.M3 = 1;
    switch (S.D) {
    case 0:                                                                     /* 16304 / 16305 */
        S.camA += 3; S.camB = fminf(S.camB + 2, S.stoneY + 40);
        if (S.stoneX <= S.camA) { S.D = 1; S.E = 0; walk(p, 0); }
        break;
    case 1:                                                                     /* 16306 / 16307 */
        walk(p, 5);
        if (X >= S.stoneX - 63) { walk(p, 0); p->face = -1; S.D = 2; S.E = 0; }
        break;
    case 2:                                                                     /* 16308-16331 */
        S.E++;
        if (S.E == 30) p->face = 1;
        if (S.E == 70 && d) { obj_effect(d->x, d->y, "ringsparkle"); snd_play_orig(286, 220, 128); }   /* 16311: media 2288 */
        if (S.E == 90) snd_play_orig(389, 220, 128);                            /* 16312: media 3112 */
        if (S.E == 150 || S.E == 180 || S.E == 210) snd_play_orig(41, 220, 128);   /* 16316 / 16319 / 16322: media 328 */
        if (S.E == 240) snd_play_orig(425, 220, 128);                           /* 16325: media 3400 */
        if (S.E > 380 && b) { b->hidden = 0; fdb_set(b, "fdb_brv5"); S.D = 3; S.E = 0; }
        break;
    case 3:                                                                     /* 16332 / 16333 */
        S.camA -= 2;
        if (b && S.camA <= b->x + 160) { S.D = 4; S.E = 0; }
        break;
    case 4:                                                                     /* 16334-16341 */
        S.E++;
        if (S.E < 20) { p->x -= 5; player_sync_sensors(p); }                    /* 16335: pushed back (INFERRED: the knife's hit) */
        if (S.E == 10 && k && b) { k->x = b->x; k->y = b->y; k->hidden = 0; snd_play_orig(21, 220, 128); }   /* 16337 */
        if (S.E == 33 && k) k->hidden = 1;                                      /* 16338 */
        if (S.E > 60 && S.E < 80) { p->x += 1; player_sync_sensors(p); }        /* 16340 */
        if (S.E > 150) { S.D = 5; S.E = 0; if (b) fdb_set(b, "fdb_brv0"); }
        break;
    case 5:                                                                     /* 16342 / 16343 */
        walk(p, 5);
        if (!d || X > d->x + 32) { walk(p, 0); S.D = 6; S.E = 0; }
        break;
    case 6:                                                                     /* 16344-16371 */
        S.E++;
        if (d && b && S.E >= 5 && !d->hidden) {                                 /* 16347-16349: A -4, B -1 until Brevon's height */
            d->x -= 4; if (d->y > b->y - 8) d->y -= 1;
            if (d->x < b->x + 33 || S.E > 180) { d->hidden = 1; obj_effect(d->x, d->y, "explosion"); snd_play_orig(17, 220, 128); }   /* 16350 / 16365 */
        }
        if (S.E == 180 && b) fdb_set(b, "fdb_brv11h");                          /* 16364: anim 11 dir 26 */
        if (S.E == 310 && b) fdb_set(b, "fdb_brv10");                           /* 16367 */
        if (S.E == 340) snd_play_orig(390, 220, 128);                           /* 16368: media 3120 */
        if (S.E > 340 && b) { fdb_set(b, "fdb_brv7"); b->y -= 3; if (b->y < G.camy - 80) b->hidden = 1; }   /* 16369 (INFERRED: rises out) */
        if (S.E > 370) {                                                        /* 16370 / 16371 */
            walk(p, -5);
            if (!h || X <= h->x + 49) { walk(p, 0); S.D = 7; S.E = 0; }
        }
        break;
    case 7:                                                                     /* 16372-16387 */
        S.E++;
        if (S.E == 60) snd_play_orig(373, 220, 128);                            /* 16373: sound 0x175 */
        if (S.E >= 70 && S.E < 80) { p->x += 10; player_sync_sensors(p); }      /* 16375 */
        if (S.E < 170 && S.E % 10 == 0 && h) {                                  /* 16381: npctransformbubble */
            obj_t *u = fdb_spawn(FDB_K_FX, "fdm_bubble", h->x - 20 + cross_rand(41), h->y - cross_rand(30), 3);
            if (u) { u->vy = -1.5f; u->tm = 30; }
        }
        if (S.E >= 170 && S.E % 20 == 10) snd_play_orig(374, 220, 128);         /* 16380: media 2992 */
        if (S.E > 140) S.fade = S.fade + 8 > 255 ? 255 : S.fade + 8;            /* 16378 / 16379 */
        if (S.E > 500) { S.D = 8; S.E = 0; }                                    /* 16387 */
        break;
    case 8:                                                                     /* 16388-16394 */
        S.E++;
        if (S.E == 60) { show_boss(1); p->face = -1; }                          /* 16390 */
        if (S.E > 60) S.fade = S.fade - 8 < 0 ? 0 : S.fade - 8;
        if (S.E > 130) {                                                        /* 16393: the camera back, ML.Y 1 */
            S.camA = fminf(S.camA + 2, X);
            if (!S.music) { S.music = 1; const char *m = level_music_slot(4); if (m) music_play(m); }
        }
        if (S.E > 170) {                                                        /* 16394: the fight */
            S.scene = 0; S.fight = 1; S.st = 2; S.t = 0; fdb_ctrl = 0; G.cam.M3 = 0;
            fdb_cam_lock(fdb_marker(OT_CAMLIM_L, 1), fdb_marker(OT_CAMLIM_R, 1), G.camy, G.camy + SCREEN_H);
        }
        break;
    }
    G.cam.A = S.camA; G.cam.B = S.camB;
}

/* ======================================================================== the fight (INFERRED gait; RE states and numbers) */
static void shot_new(obj_t *h, int k, player_t *p) {
    int n; for (n = 0; n < NSHOT && fdb_slot(S.shot[n]); n++) ;
    if (n == NSHOT) return;
    obj_t *s = fdb_spawn(FDB_K_BOUNCE, "fdm_shot", h->x + 20 * S.dirm, h->y - 10, 3);
    if (!s) return;
    double dx = p->x - s->x, dist = fabs(dx) < 40 ? 40 : fabs(dx);
    s->fx = s->x; s->fy = s->y; s->vx = (float)((dx > 0 ? 1 : -1) * dist / 60.0) + (k ? 0.6f : -0.6f) * S.dirm; s->vy = -6.f;   /* INFERRED arc */
    s->A = 0; s->c = 0;
    S.shot[n] = (short)(s - G.o);
    snd_play_orig(28, 220, 128);
}
static void shots(player_t *p, obj_t *h) {
    for (int n = 0; n < NSHOT; n++) {
        obj_t *s = fdb_slot(S.shot[n]);
        if (!s || s->type != OT_FDB_RT || s->st != FDB_K_BOUNCE) { S.shot[n] = -1; continue; }
        s->vy += 0.25f; s->fx += s->vx; s->fy += s->vy; s->x = floorf(s->fx); s->y = floorf(s->fy);
        if (s->A == 9 && h) {                                                   /* reflected: back at the head (16562; INFERRED) */
            if (obj_obj_hit(s, h)) { if (S.inv <= 0) h->A -= 10; if (h->A <= 0) h->fl |= FDB_DEF; snd_play_orig(24, 220, 128); obj_effect(s->x, s->y, "explosion02"); s->alive = 0; continue; }
        } else {
            if (sbb_csens_hit(p, s) && sbb_hurt(p, 5)) { obj_effect(s->x, s->y, "explosion02"); s->alive = 0; continue; }   /* 16563 */
            if (player_contact_safe(p) && sbb_csens_hit(p, s)) {                       /* 16562: hit back (A 9) */
                s->A = 9; double dx = h ? h->x - s->x : 0, dy = h ? h->y - s->y : -1, l = sqrt(dx * dx + dy * dy) + 1e-6;
                s->vx = (float)(dx / l * 7); s->vy = (float)(dy / l * 7) - 2.f; snd_play_orig(30, 220, 128); continue;
            }
        }
        if (s->vy > 0 && obj_overlaps_bg(s)) {                                  /* 16557 / 16558: the floor */
            s->c++; s->vy = -fabsf(s->vy) * 0.75f; s->fy -= 2;
            if (s->c >= 3) { for (int k = 0; k < 2; k++) obj_effect(s->x, s->y, "fdm_shotdeb"); snd_play_orig(380, 220, 128); s->alive = 0; continue; }   /* media 3040 */
        }
        if (!obj_near_view(s, 64)) s->alive = 0;
    }
}
static void rig(obj_t *h) {
    S.phase += fabs(S.vx) * 3 + 2;
    double bob = sbb_sin(S.phase * 2) * 2;
    for (int k = 0; k < S.npart; k++) {
        obj_t *o = fdb_slot(S.part[k]); if (!o) continue;
        float ox = S.px0[k], oy = S.py0[k];
        double leg = 0;
        if (o->type == OT_FDB_MM_PART && (o->par == MM_FOOT || o->par == MM_FOOTBACK || o->par == MM_PAWFRONT || o->par == MM_PAWBACK))
            leg = sbb_sin(S.phase * 4 + (o->par == MM_FOOT || o->par == MM_PAWBACK ? 180 : 0)) * 6 * (fabs(S.vx) > 0.2);
        if (o->type == OT_FDB_MM_TAIL || (o->type == OT_FDB_MM_PART && o->par == MM_TAILEND)) oy += (float)(sbb_sin(S.phase + k * 30) * 3);
        o->x = h->x + (float)(S.dirm > 0 ? ox : -ox) + (float)leg * 0.5f; o->y = h->y + oy - (float)bob * (o->type != OT_FDB_MM_PART || o->par < MM_FOOT ? 1 : 0) - (float)S.hop;
        o->flip = S.dirm < 0;
    }
    h->flip = S.dirm < 0;
}
static void fight_frame(player_t *p) {
    obj_t *h = head();
    if (!h) return;
    if ((h->fl & FDB_DEF) && S.lives > 0) {                                     /* 16576 / 16577: a life lost (ctor O 5; RV b2_0 f3100-3200) */
        if (S.lives > 1) for (int k = 0; k < 2; k++) obj_throw_petal(h->x, h->y - 20);   /* 16576: petals while O > 1 */
        h->fl &= ~FDB_DEF; h->A = 60; S.inv = 90; S.lives--;
        snd_play_orig(25, 220, 128); snd_play_orig(392, 220, 128);              /* 16577: sound 25, media 3136 */
        if (S.lives <= 1) fdb_set(h, "fdm_head7");                              /* 16579: O <= 1 -> anim 7 */
        if (S.lives <= 0) h->fl |= FDB_DEF;                                     /* 16580: O <= 0 -> the defeat (D 60) */
    }
    if (S.inv > 0) { S.inv--; h->inv = 1; if (S.inv >= 6) h->c = 3; if (!S.inv) { h->inv = 0; h->c = 0; } }   /* C 90 (16577 / 16578) */
    int def = (h->fl & FDB_DEF) != 0 && S.lives <= 0;
    shots(p, h);
    if (S.done) return;
    if (!def) {
        h->bd = 10;                                                             /* INFERRED contact damage */
        int lo = S.stoneX - 200, hi = S.stoneX + 200;                           /* 16530 / 16531 */
        S.t++;
        switch (S.st) {
        case 0:                                                                 /* D0: choose (16524 / 16525) */
            S.vx *= 0.8; if (S.t > 20) { S.t = 0; S.st = S.runs < 4 ? 2 : 4; S.runs = S.runs < 4 ? S.runs + 1 : 0; }
            break;
        case 2:                                                                 /* D1 / D2: run (16553: + 0.2 M a frame, |N| <= 4) */
            S.vx += 0.2 * S.dirm; if (S.vx > 4) S.vx = 4; if (S.vx < -4) S.vx = -4;
            if ((S.dirm > 0 && S.hx > hi) || (S.dirm < 0 && S.hx < lo)) {      /* 16532: turn, E + 50 M */
                S.dirm = -S.dirm; S.vx = 0; S.st = 0; S.t = 0;
            }
            break;
        case 4:                                                                 /* D4: the volley (16545 / 16546), F > 350 -> D5 */
            S.vx *= 0.8;
            if (S.t == 60 || S.t == 120) shot_new(h, S.t == 120, p);
            if (S.t > 350) { S.st = 5; S.t = 0; }
            break;
        case 5:                                                                 /* D5: recover */
            S.vx *= 0.8; if (S.t > 30) { S.st = 0; S.t = 0; }
            break;
        }
        S.hx += S.vx; S.hop = S.st == 4 ? fmax(0, sbb_sin(S.t * 3.0) * 6) : 0;
        h->x = (float)floor(S.hx); h->y = (float)floor(S.hy);
        rig(h);
    } else {                                                                    /* 16576-16637 */
        if (S.defF == 0) { music_fade(60, 0); snd_play_orig(38, 220, 128); for (int n = 0; n < NSHOT; n++) { obj_t *s = fdb_slot(S.shot[n]); if (s) s->alive = 0; } }
        S.defF++; h->bd = 0;
        if (S.defF < 120 && S.defF % 6 == 0) { obj_effect(h->x - 40 + cross_rand(81), h->y - 30 + cross_rand(61), "explosion06"); fdb_explode_snd(); }   /* D 60 */
        if (S.defF > 60 && S.defF <= 120) S.fade = (S.defF - 60) * 4 > 255 ? 255 : (S.defF - 60) * 4;
        if (S.defF == 121) {                                                    /* D 61: the boss gone, Milla's body (16595-16600) */
            show_boss(0); snd_play_orig(76, 220, 128);
            obj_t *mb = mbody(); if (mb) { mb->x = h->x; mb->y = (float)(S.base_y + 28); mb->hidden = 0; }
            fdb_ctrl = 1; fdb_keys = 0; walk(p, 0);
        }
        if (S.defF > 121) S.fade = S.fade - 6 < 0 ? 0 : S.fade - 6;
        if (S.defF > 200) {                                                     /* D 62: walk to Milla (16604 / 16605) */
            obj_t *mb = mbody();
            if (mb && fabs(p->x - mb->x) > 47) walk(p, p->x < mb->x ? 5 : -5); else walk(p, 0);
        }
        if (S.defF > 200 + 500 && !S.results) {                                 /* D 63 F 500 (16637): Sign.B 179 */
            S.results = 1; S.done = 1; fdb_ctrl = 0; results_begin_boss(game_timer());
        }
    }
}

void f3b_frame(player_t *p) {
    if (!S.on) return;
    if (!S.scene && !S.fight && !S.results && p->y - ORIG_REF_DY > 1024 && fabs(p->x - S.stoneX) < 200 && G.character != CHR_MILLA && p->hp > 0)
        scene_start(p);                                                         /* 16237 */
    if (!S.mpath && !S.scene && !S.fight && !S.results && G.character == CHR_MILLA && p->y - ORIG_REF_DY > 1024 && p->x > S.stoneX && p->hp > 0) {
        S.mpath = 1; S.E = 0; fdb_ctrl = 1; fdb_keys = 0; music_fade(120, 0);   /* 16395 (g0 != 2): CTRL.M 1, CAM.M 3, fadeout_music(2000) */
    }
    if (S.mpath == 1) {                                                         /* 16397-16399: E < 90 CTRL.D 1 (walk on), E 90 Sign.B 179 */
        S.E++; fdb_keys = S.E < 90 ? BTN_RIGHT : 0;
        if (S.E == 90) { S.mpath = 2; S.results = 1; fdb_ctrl = 0; fdb_keys = 0; results_begin_boss(game_timer()); }
    }
    if (S.scene) scene_frame(p);
    if (S.fight) fight_frame(p);
    for (int k = 0; k < fdb_rt_n(); k++) {                                      /* transform bubbles */
        obj_t *u = fdb_rt(k);
        if (u && u->st == FDB_K_FX && fdb_is(u, "fdm_bubble")) { u->y += u->vy; if (--u->tm <= 0) u->alive = 0; }
    }
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_F3B")) { obj_t *h = head(); printf("f3b %d sc %d D %d E %d fi %d st %d t %d runs %d hx %.0f vx %.2f A %.0f def %d defF %d res %d px %.0f py %.0f fade %d lv %d inv %d\n",
        G.frame, S.scene, S.D, S.E, S.fight, S.st, S.t, S.runs, S.hx, S.vx, h ? h->A : -1, h ? (h->fl & FDB_DEF) != 0 : -1, S.defF, S.results, p->x, p->y, S.fade, S.lives, S.inv); }
#endif
}

int f3b_draw(const obj_t *o, int pass) {
    (void)pass;
    if (!S.on) return 0;
    if (o == head() && S.fade > 0) { if (!o->hidden) sbb_draw(o, 0xffffffff); r_fade(S.fade / 255.f); return 1; }   /* the scene's fades */
    if (o->type == OT_FDB_STONE && S.fade > 0 && !head()) { sbb_draw(o, 0xffffffff); r_fade(S.fade / 255.f); return 1; }
    return 0;
}
float f3b_hud_dy(void) { return 0.f; }
#ifdef FP_HOST_ENV
int f3b_host_state(char *buf, int n) { if (!S.on) return 0; obj_t *h = head(); return snprintf(buf, n, "f3b sc %d D %d fi %d A %.0f res %d ", S.scene, S.D, S.fight, h ? h->A : -1, S.results); }
#endif
