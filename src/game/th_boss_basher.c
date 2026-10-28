/* TH3: the Crystal Basher (BossCrystalBasherBase_3199, Face_3198 (Q20, A 120), Head_3197, 4 x Segment_3194, BossCrystalSpike_3195
 * (the class shares the object list of BossCrystalSpike_2324) and Spike2_3196) and BossDoor_3200, at the top of the start shaft.
 * RE handle_frame_59_events 13880-13887 (rig, door, trigger; group 0x22136), 13888-13931 (the fight, group 0x22dd1), 13932-13935
 * (the parts fly off), start events 14093 / 14094, constructors; RV build/orig_traces/th_bosses c1_c (Carol, idle 1300 frames),
 * c2_c (Carol, face A 0 at f1000). spec_th_bosses.md Part B. No character test in these events (RE).
 * Base alterables (ctor A 90, B 70, C 90, D 10): A the neck's angle, B the wave phase (-5 a frame in the fight), C the wave
 * amplitude, D the phase step per link, E a frame count once the door shut (13886); flag 2: the parts fly (13930).
 * Head (ctor A 5, D 5, E 180): A its link index, D the state (1 spikes out together, 3 crossed, 5 the swing, 6 shown face
 * (from then on), even states wait 91 frames, 7 -> 1, 9 defeated), E the state timer; flag 1 the slam done (13891 / 13892).
 * Segments: A = 1..4 by selection order (14093: last placed first: y 304 -> 1 ... 232 -> 4). Spikes: A the diagonal reach (+-100),
 * flag 1 the wall hit done. Door: A its closed x (14093), B the slide (0..112), flag 0 open. */
#include "th_boss.h"
#include <stdio.h>
#include <stdlib.h>

static struct {
    int on, fight, door_once, parts, opened;          /* placed; group 0x22dd1; 0x22dd6 (13885); base flag 2 (13930); 0x22de4 (13931) */
    double bA, bB, bC, bD, bE;                        /* base */
    double hD, hE; int hfl1;                          /* head D / E, flag 1 */
    double sA[2]; int sfl1[2];                        /* spike / spike2 A, flag 1 */
    double dA, dB; int dfl0;                          /* door A (closed x), B (slide), flag 0 (open) */
    double vx[8], vy[8];                              /* the flying parts' X / Y (13930): segments 0-3, spike, spike2, head */
} C;

static obj_t *base(void) { return thb_get(OT_THB_CB_BASE, 0); }
static obj_t *head(void) { return thb_get(OT_THB_CB_HEAD, 0); }
static obj_t *face(void) { return thb_get(OT_THB_CB_FACE, 0); }
static obj_t *spike(int k) { return thb_get(k ? OT_THB_CB_SPIKE2 : OT_THB_CB_SPIKE, 0); }
static obj_t *door(void) { return thb_get(OT_THB_DOOR, 0); }
static obj_t *seg(int k) { return thb_get(OT_THB_CB_SEG, k); }
static double seg_A(int k) { return thb_count(OT_THB_CB_SEG) - k; }            /* 14093: 1.. in selection order (the last placed first) */
int th_boss_basher_state(void) { return C.on ? (int)C.hD : -1; }

void thc_spawn(void) {
    static int reg; if (!reg) { reg = 1; snap_register(&C, sizeof C); }
    memset(&C, 0, sizeof C);
    obj_t *b = base(), *h = head(), *f = face(), *d = door();
    if (d) { thb_set_a(d, TS_DOOR); C.dA = d->x; }                              /* 14093: door A = x */
    if (!b || !h) return;
    C.on = 1;
    C.bA = 90; C.bB = 70; C.bC = 90; C.bD = 10;                                 /* ctors (RE) */
    C.hD = 5; C.hE = 180;
    thb_set_a(b, TS_CB_BASE); thb_set_a(h, TS_CB_HEAD);
    for (int k = 0; k < thb_count(OT_THB_CB_SEG); k++) thb_set_a(seg(k), TS_CB_SEG);
    thb_set_a(spike(0), TS_CB_SPIKE); thb_set_a(spike(1), TS_CB_SPIKE2);
    if (f) { thb_set_a(f, TS_CB_FACE); f->A = 120; f->fl |= F_NOFLY; f->hidden = 1; }   /* ctor A 120, invisible; 14093 flag 16 */
    if (G.at_g4 >= 5) {                                                         /* 14094: a restart past the fight */
        for (int k = 0; k < thb_count(OT_THB_CB_SEG); k++) { obj_t *s = seg(k); if (s) s->alive = 0; }
        if (h) h->alive = 0;
        if (f) f->alive = 0;
        for (int k = 0; k < 2; k++) { obj_t *s = spike(k); if (s) s->alive = 0; }
        thb_rails_kill();
        C.on = 0;
    }
}

/* 13880: the neck. A link of index a sits at angle base.A + C sin(D a + B), 32 a px from the base, never lower than base.y - 10 a */
static void link_pos(double a, double *x, double *y) {
    const obj_t *b = base();
    double ang = C.bA + C.bC * sbb_sin(C.bD * a + C.bB);
    *x = b->x + 32 * a * sbb_cos(ang);
    *y = fmin(b->y - 10 * a, b->y - 32 * a * sbb_sin(ang));
}
static void rig(void) {
    obj_t *h = head(), *f = face();
    if (C.parts || !base()) return;                                             /* 13880: base flag 2 off */
    for (int k = 0; k < thb_count(OT_THB_CB_SEG); k++) {
        obj_t *s = seg(k); double x, y;
        if (!s) continue;
        link_pos(seg_A(k), &x, &y); s->x = (float)(int)x; s->y = (float)(int)y;
    }
    if (h) {
        double x, y; link_pos(5, &x, &y); h->x = (float)(int)x; h->y = (float)(int)y;
        obj_t *s0 = spike(0), *s1 = spike(1);
        if (s0) { s0->x = (float)(int)(h->x + C.sA[0]); s0->y = (float)(int)(h->y + C.sA[0]); }   /* the "\" diagonal */
        if (s1) { s1->x = (float)(int)(h->x - C.sA[1]); s1->y = (float)(int)(h->y + C.sA[1]); }   /* the "/" diagonal */
        if (f) { f->x = h->x; f->y = h->y; }
    }
}

static void door_frame(player_t *p) {
    obj_t *d = door();
    if (!d) return;
    double Y = sbb_py(p);
    if (!C.dfl0) C.dB = fmax(C.dB - 12, 0); else C.dB = fmin(C.dB + 12, 112);  /* 13881 / 13882 */
    d->x = (float)(int)(C.dA + C.dB);
    if (!C.dfl0 && d->y < Y && Y - 80 < d->y) C.dfl0 = 1;                       /* 13883: the player just below: open */
    else if (C.dfl0 && d->y < Y - 80) C.dfl0 = 0;                               /* 13884: gone down: shut */
    if (G.at_g4 < 5 && C.dfl0 && Y <= d->y && sbb_px(p) < thb_marker(OT_CAMLIM_R, 1)) {   /* 13885: through: shut behind */
        C.dfl0 = 0;
        if (!C.door_once) C.door_once = 1;
    }
    if (C.door_once && !C.parts && sbb_px(p) < thb_marker(OT_CAMLIM_R, 1)) {    /* 13886: the camera's floor rises to the door */
        G.cam_nF = fmaxf(fminf(G.camy + SCREEN_H, G.cam_nF - 4), d->y + 16);
        C.bE += 1;
    }
    if (C.door_once && C.bE == 20 && !C.fight) { C.bE = 21; C.fight = 1; }      /* 13887 */
}

static void crash_fx(float x, float y) { obj_effect(x, y, "explosion01"); }
static void shake(void) { snd_play_orig(132, 220, 128); if (G.cam.Z < 4) G.cam.Z = 4; }
static void hurt10(player_t *p) { if (!p->hurt_pending && p->invuln <= 0 && p->state != ST_DEAD) player_contact(p, 10); }

static void fight(player_t *p) {
    obj_t *b = base(), *h = head(), *f = face();
    if (!b || !h) return;
    C.bB = sbb_fmod360(C.bB + 355);                                             /* 13888 */
    if (fmod(C.hD, 2) == 0) { C.hE += 1; C.bC = fmin(C.bC + 1, 20); }          /* 13889 */
    if (fmod(C.hD, 2) == 0 && C.hE > 90) { C.hE = 0; C.hD += 1; }              /* 13890 */
    if (C.hfl1 && h->y < b->y - 50) C.hfl1 = 0;                                 /* 13891 */
    else if (!C.hfl1 && h->y >= b->y - 50) {                                    /* 13892: the head hits the floor */
        crash_fx(h->x, h->y + 50); crash_fx(h->x - 36, h->y + 50); crash_fx(h->x + 36, h->y + 50); (void)cross_rand(4);
        C.hfl1 = 1; shake();
    }
    for (int k = 0; k < 2; k++) {                                               /* 13893-13898: a spike at full reach hits the wall */
        obj_t *s = spike(k);
        if (!s) continue;
        if (C.sfl1[k] && C.sA[k] > -100 && C.sA[k] < 100) C.sfl1[k] = 0;
        else if (!C.sfl1[k] && (C.sA[k] <= -100 || C.sA[k] >= 100)) {
            int sg = C.sA[k] <= -100 ? -1 : 1;
            crash_fx(s->x + (k ? -sg : sg) * 48, s->y + sg * 48); C.sfl1[k] = 1; shake();
        }
    }
    if (C.hD == 1 || C.hD == 3) {                                               /* 13899-13912: the spike attacks */
        int x3 = C.hD == 3;
        C.bC = fmax(C.bC - 3, 0); C.hE += 1;
        if (C.hE == 1) snd_play_orig(74, 220, 128);
        if (C.hE < 30) for (int k = 0; k < 2; k++) { obj_t *s = spike(k); if (s && s->an.b) s->an.frame = (int)fmod(0.5 * C.hE, 2); }   /* they flash */
        if (C.hE > 30 && C.hE < 90) { C.sA[0] = fmin(C.sA[0] + 10, 100); C.sA[1] = x3 ? fmax(C.sA[1] - 10, -100) : fmin(C.sA[1] + 10, 100);
                              for (int k = 0; k < 2; k++) { obj_t *s = spike(k); if (s && s->an.b) s->an.frame = 0; } }
        if (C.hE > 90 && C.hE < 150) { C.sA[0] = fmax(C.sA[0] - 10, -100); C.sA[1] = x3 ? fmin(C.sA[1] + 10, 100) : fmax(C.sA[1] - 10, -100); }
        if (C.hE > 150) { C.sA[0] = fmin(C.sA[0] + 10, 0); C.sA[1] = x3 ? fmax(C.sA[1] - 10, 0) : fmin(C.sA[1] + 10, 0); }
        if (C.hE > 180) { C.hE = 0; C.hD += 1; }
    } else if (C.hD == 5) {                                                     /* 13913-13916: the swing */
        C.hE += 1;
        if (C.hE < 180) { C.bC = fmin(C.bC + 1, 90); C.bD = fmax(C.bD - 0.2, 10); }
        if (C.hE > 180) { C.bC = fmax(C.bC - 2, 30); C.bD = fmin(C.bD + 0.4, 20); }
        if (C.hE > 210) { C.hE = 0; C.hD += 1; if (f) f->hidden = 0; }
    } else if (C.hD == 7) C.hD = 1;                                             /* 13917 */
    /* 13919-13921: the spikes out (A != 0) and the slamming head above the player hurt 10 */
    for (int k = 0; k < 2; k++) { obj_t *s = spike(k); if (s && C.sA[k] != 0 && sbb_csens_hit(p, s)) hurt10(p); }
    if (C.hfl1 && h->y < sbb_py(p) && sbb_csens_hit(p, h)) hurt10(p);
    if (f && (f->fl & THB_DEF) && C.hD < 9) { C.hD = 9; C.hE = 0; }             /* 13923 */
    if (C.hD == 9) {
        for (int k = 0; k < 2; k++) C.sA[k] = C.sA[k] < 0 ? fmin(C.sA[k] + 20, 0) : fmax(C.sA[k] - 20, 0);   /* 13924-13927 */
        C.bB = sbb_fmod360(C.bB + 355); C.bC = fmax(C.bC - 2, 30); C.bD = fmin(C.bD + 1, 20);   /* 13928 */
        for (int k = 0; k < 2; k++) { obj_t *s = spike(k); if (s && s->an.b) s->an.frame = (int)fmod(0.5 * C.hE, 2); }
        if (C.hE < 120 && thb_g3() % 7 == 0 && f) {                             /* 13929 */
            int ex = cross_rand(101), ey = cross_rand(101);
            obj_effect(f->x - 50 + ex, f->y - 50 + ey, "explosion06"); thb_explode_snd();
        }
        C.hE += 1;
        if (C.hE == 120) {                                                      /* 13930: the parts fly off */
            for (int k = 0; k < thb_count(OT_THB_CB_SEG) && k < 4; k++) { C.vx[k] = 3.5 * seg_A(k) - 9; C.vy[k] = -2 * seg_A(k); }
            C.vx[4] = -8; C.vy[4] = -8; C.vx[5] = 8; C.vy[5] = -8; C.vx[6] = -4; C.vy[6] = -14;
            C.parts = 1; snd_play_orig(38, 220, 128);
        }
        if (C.hE > 160 && !C.opened) {                                          /* 13931: the room opens */
            C.opened = 1; thb_arena_open();
            level_release_camera(0);                                            /* CAM.M 2, U 0 */
            C.fight = 0;
            G.at_g4 = 5; G.at_g5 = 560; G.at_g6 = 170;                          /* g4 / g5 / g6 (no restart snapshot: RE) */
        }
    }
}

static void parts_fly(void) {                                                   /* 13932-13935 */
    obj_t *q[7] = { seg(0), seg(1), seg(2), seg(3), spike(0), spike(1), head() };
    for (int k = 0; k < 7; k++) {
        obj_t *o = q[k];
        if (!o) continue;
        o->x = (float)(int)(o->x + C.vx[k]); o->y = (float)(int)(o->y + C.vy[k]); C.vy[k] += 0.3;
        if (k != 6 && !obj_near_view(o, 150)) o->alive = 0;                     /* 13933-13935 (the head is left to fall, RE) */
        if (k == 6 && o->y > G.lvl.h + 100) o->alive = 0;                         /* (INFERRED: below the frame, gone) */
    }
    obj_t *f = face(); if (f) f->hidden = 1;
}

void thc_frame(player_t *p) {
    if (!thb_get(OT_THB_DOOR, 0) && !C.on) return;
    door_frame(p);
    if (!C.on) return;
    rig();
    if (C.fight) fight(p);
    if (C.parts) parts_fly();
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_THC")) {
        const obj_t *h = head(), *f = face();
        printf("thc %d px %.0f py %.0f hx %.0f hy %.0f D %.0f E %.0f bA %.0f bB %.0f bC %.1f bD %.1f bE %.0f fA %.0f fv %d s0 %.0f s1 %.0f dB %.0f dfl %d fight %d parts %d camF %.0f g4 %d\n",
            G.frame, sbb_px(p), sbb_py(p), h ? h->x : -1, h ? h->y : -1, C.hD, C.hE, C.bA, C.bB, C.bC, C.bD, C.bE, f ? f->A : -1, f ? !f->hidden : -1,
            C.sA[0], C.sA[1], C.dB, C.dfl0, C.fight, C.parts, G.cam_nF, G.at_g4);
    }
#endif
}

/* draw order (RE 13922: on the segments' layer the spikes go to the back and the head to the front; the face and base on layer 3) */
int thc_draw(const obj_t *o, int pass) {
    (void)pass;
    switch (o->type) {
    case OT_THB_CB_SEG: {
        if (o != seg(0)) return 1;                                              /* the layer-2 group drawn once, at the first link */
        for (int k = 0; k < 2; k++) { const obj_t *s = spike(k); if (s) sbb_draw(s, 0xffffffff); }
        for (int k = 0; k < thb_count(OT_THB_CB_SEG); k++) { const obj_t *s = seg(k); if (s) sbb_draw(s, 0xffffffff); }
        const obj_t *h = head(); if (h) sbb_draw(h, 0xffffffff);
        return 1; }
    case OT_THB_CB_SPIKE: case OT_THB_CB_SPIKE2: case OT_THB_CB_HEAD:
        if (seg(0)) return 1;                                                   /* drawn with the links */
        sbb_draw(o, 0xffffffff); return 1;
    case OT_THB_CB_FACE: case OT_THB_CB_BASE: case OT_THB_DOOR:
        sbb_draw(o, 0xffffffff); return 1;
    }
    return 0;
}
