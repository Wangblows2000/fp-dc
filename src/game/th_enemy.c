/* Trap Hideout enemies (session 21b): EnemyTotematic_3144, EnemyMetalPup_3148, EnemyShockPuppet_3149, EnemyBladeOrbiter_3165
 * (+ Left_3163 / Right_3164 blades), EnemyRocketPuncher_3182 (+ Hand_3183, Shield_3184, EffectRocketPuncherWarning_3185),
 * EnemyBikeNinja_3203 (+ Mask_3204, EffectBikeNinjaWarning_3206, the runtime EnemyNinjaStar_3221 and EnemyBikeNinjaDefeated_3205).
 * Source: build/re/spec_th_enemies.md. Labels: RE (TH2 / TH3 events 13494-13497, 13603-13692, 13730-13794 and TH1 13488 /
 * 13494-13497, constructors, on_frame_57..59_init qualifiers), RV (original traces build/orig_traces/th_enemies), INFERRED.
 * Shared code: src/game/enemy_kit.h (per-type lists, ENS rows, R-hurt, limiters, burst death, mask sensors); the shared Q20
 * hits / defeat / fly-away of combat.c (2730-2768; RE: every class but the blades, hands, warnings and stars is Q20); the
 * Ganseki-style corpse landing (combat.c corpse_land, sound 50: objdefs OD_LANDS) for the pup, the puppet and the orbiter.
 * Order: th_enemy_pre (objects.c, before combat) = the collision triggers that feed the shared rules (totem solidity, hand /
 * bike / blade vs totem and mask); th_enemy_obj = each object's events, after the animations advanced (update_objects).
 * Alterables: av[] = the class's letters as named per class; flags: the original's flag 5 = 0x20 (armoured), 6 = 0x40
 * (defeated), 16 = F_NOFLY; own flags >= 0x1000. */
#include "th.h"
#include "enemy_kit.h"

#define DEFEATED(o) ((o)->fl & 0x40)
#define FL1 0x1000                                   /* the class's flag 1 */
#define FL2 0x2000                                   /* flag 2 */
#define FL4 0x4000                                   /* flag 4 (on the ground) */
#define FL13 0x8000                                  /* flag 13 (bike: this jump's star thrown) */
#define FLW1 0x10000                                 /* mask flag 1 (wall right) */
#define FLW2 0x20000                                 /* mask flag 2 (wall left) */
#define FLW3 0x40000                                 /* mask flag 3 (ceiling) */
#define FLGONE 0x80000                               /* totem / mask: A = -5 .. -1 (debris made): removed at its next events */
#define FLARENA 0x100000                             /* mask dropped in by the TH3 arena (th_enemy_bike_drop) */
#define RT0 OT_THE_FIRST
#define NRT (OT_THE_LAST - OT_THE_FIRST + 1)
static ek_list_t LST[NRT];
#define EACH(T, OB) EK_EACH(&LST[(T) - RT0], T, OB)
static struct { int on, skid_tick; ek_limiters_t lim; float skid_t; } S;

static int R(int n) { return cross_rand(n); }
static obj_t *spawn(int type, const char *anim, float x, float y, int dir) { return ek_spawn(&LST[type - RT0], type, anim, x, y, dir); }
static int in_view(const obj_t *o, int n) { return obj_near_view(o, n); }    /* !is_near_border(-n) */
static int g3(void) { return (int)(G.frame % 100000); }                      /* global value 3 (the frame counter, as sb_gim.c) */

int th_enemy_q20(int t) {                                                       /* RE on_frame_57..59_init qualifier 20 */
    return t == OT_TH_TOTEM || t == OT_TH_PUP || t == OT_TH_PUPPET || t == OT_TH_ORB || t == OT_TH_PUNCH || t == OT_TH_PSHIELD ||
           t == OT_TH_BMASK;
}

/* ================================================================ EnemyTotematic_3144 (RE 13494-13497 / 13617 / 13730, 13488)
 * Ctor A 1, S 2 (crystals held: one drops in the defeat frame, RV). A solid block (Q7) and a Q20 enemy: any shared hit breaks it.
 * 13494: frame (g3 / 5) mod 9 (the colour cycle), flag 0 off; 13495: OTHERS.Y 8 (the ball) -> flag 0 (not solid: she rolls
 * through and the ball's body hit breaks it). 13617 (TH2) / 13730 (TH3) / 13496 (TH1): defeated -> sound 314, explosion01, A -5,
 * 4 debris (A + 1 each), flag 1; 13497: defeated with A -1 -> removed (the next frame; RV totem_c2: hit 226, gone 227).
 * Also broken (A = 0) by a flying Rocket Puncher hand (13689), a bike ninja's mask (13794), TH1's 13488 (th_enemy_totem_zap). */
static int ball(const player_t *p) { return (chr_is_carol(G.character) && p->cY == 8) || (G.character == CHR_LILAC && p->roll_ball); }
static void totem_frame(obj_t *o) {
    if (o->fl & FLGONE) { o->alive = 0; return; }                                 /* 13497 */
    ek_force_frame(o, (g3() / 5) % 9);                                            /* 13494 */
    if (DEFEATED(o) && !(o->fl & FL1)) {                                          /* 13617 */
        snd_play_orig(314, 220, 128); obj_effect(o->x, o->y, "explosion"); ek_debris(o, 4);
        o->fl |= FL1 | FLGONE; o->fl |= 1;
    }
}
static void totem_kill(obj_t *t) { if (!DEFEATED(t)) t->A = 0; }                  /* A = 0: the shared 2764 defeats it this frame */
int th_enemy_totem_zap(const obj_t *beam) {                                     /* 13488 (TH1): a scan laser on a totem */
    obj_t *t; int n = 0;
    if (!S.on || !beam || !beam->an.b) return 0;
    EACH(OT_TH_TOTEM, t)
        if (!DEFEATED(t) && t->A > 0 && in_view(t, 80) && fabsf(t->x - beam->x) < 800 && fabsf(t->y - beam->y) < 800 && t->an.b) {
            const frame_t *f = anim_frame(&t->an);                                  /* check_overlap(totems, the lasers' qualifier) */
            int x0 = (int)floorf(t->x) - (t->flip ? f->w - f->hx - (t->flip == 2) : f->hx), y0 = (int)floorf(t->y) - f->hy;
            if (th_gim_beam_rect(beam, x0, y0, x0 + f->w - 1, y0 + f->h - 1)) { totem_kill(t); n++; }
        }
    return n;
}

/* ================================================================ EnemyMetalPup_3148 (RE 13603-13616; RV pup)
 * Ctor A 15, E -1. av: [0] D state (0 walk, 1 turn, 2 bite, 6 defeated), [1] E facing sign, [2] F bite cooldown.
 * Walks 2 px a frame in view (50); turns on an EnemyLimiter (anim 10, then E = -E); bites when it overlaps the player's main
 * object (Player_326): anim 7 (5 frames x 3 loops, RV 59 frames), contact damage B 5 on its frames >= 3, then F 30.
 * Defeated: anim 8, the shared fly-away and the Ganseki landing (13616: shade + explosion01 + sound 50; combat.c). */
static void pup_spawn(obj_t *o) { o->A = 15; o->av[1] = -1; o->dir = 16; ek_set_a(o, "thp_idle"); }
static void pup_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2];
    if (*E == -1) ek_face(o, 16); else if (*E == 1) ek_face(o, 0);                /* 13603 / 13604 */
    if (in_view(o, 50) && *D == 0) { o->x = (float)(int)(floorf(o->x) + *E + *E); ek_set_a(o, "thp_walk"); }   /* 13605 */
    int lim = ek_limiter_hit(&S.lim, o);
    if (*D == 0 && (o->fl & FL1) && !lim) o->fl &= ~FL1;                           /* 13606 */
    if (in_view(o, 50) && *D == 0 && lim && !(o->fl & FL1)) { *D = 1; ek_restart_a(o, "thp_turn"); o->fl |= FL1; }   /* 13607 */
    if (*D == 1 && !(ek_a_is(o, "thp_turn") && !o->an.done)) { *D = 0; *E = -*E; }   /* 13608 */
    if (*D == 0 && *F <= 0 && !ek_dead(p) && ek_p326_hit(o)) { *D = 2; ek_restart_a(o, "thp_bite"); }   /* 13609 */
    if (*D == 2) o->bd = o->an.frame >= 3 ? 5 : 0;                                 /* 13610 / 13611 */
    if (*D == 2 && !(ek_a_is(o, "thp_bite") && !o->an.done)) { *D = 0; o->bd = 0; *F = 30; }   /* 13612 */
    if (*F > 0) *F -= 1;                                                           /* 13613 */
    if (DEFEATED(o) && *D != 6) *D = 6;                                            /* 13614 */
    if (*D == 6) { ek_set_a(o, "thp_dead"); o->bd = 0; }                           /* 13615 */
}

/* ================================================================ EnemyShockPuppet_3149 (RE 13618-13627; RV puppet)
 * Ctor A 12. av: [0] D (0 idle, 1 wind-up, 2 shocking, 6 defeated), [1] E timer. In view (100) E counts to 30 (D 1, anim 7),
 * 40 more frames (D 2, anim 8), at its frame 6 sound 167 and flag 1 (the shock: R-hurt 10 on the character sensor, sound
 * 371, not the metal shield, 13624), E counts 100 more frames, then E -60, anim 0, D 0 (RV cycle: 30 + 41 + 10 + 101 + 91). */
static void puppet_spawn(obj_t *o) { o->A = 12; ek_set_a(o, "ths_idle"); }
static void puppet_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1];
    if (*D == 0 && in_view(o, 100)) *E += 1;                                       /* 13618 */
    if (*D == 0 && *E > 30) { *E = 0; *D += 1; }                                   /* 13619 */
    if (*D == 1) { ek_set_a(o, "ths_wind"); *E += 1; }                             /* 13620 */
    if (*D == 1 && *E > 40) { *E = 0; *D = 2; ek_set_a(o, "ths_shock"); o->fl &= ~FL1; }   /* 13621 */
    if (*D == 2 && ek_a_is(o, "ths_shock") && o->an.frame == 6 && !(o->fl & FL1)) { snd_play_orig(167, 220, 128); o->fl |= FL1; }   /* 13622 */
    if (*D == 2 && (o->fl & FL1)) *E += 1;                                         /* 13623 */
    if ((o->fl & FL1) && !DEFEATED(o) && ek_csens_hit(p, o) && ek_r_hurt(p, 10, 3)) snd_play_orig(371, 220, 128);   /* 13624 */
    if (*D == 2 && *E > 100) { *E = -60; ek_set_a(o, "ths_idle"); o->fl &= ~FL1; *D = 0; }   /* 13625 */
    if (DEFEATED(o)) { *D = 6; ek_set_a(o, "ths_dead"); o->fl &= ~FL1; }           /* 13626 */
}

/* ================================================================ EnemyBladeOrbiter_3165 + Left_3163 / Right_3164 (RE 13628-13645;
 * RV orb). Ctor A 8; start: G = X, H = Y. av: [0] E swing angle, [1] F state (0 idle, 1 wind-up, 2 swing, 6 defeated), [2] I spin,
 * [3] J timer, [4] K blink timer, [5] G, [6] H. Blades (ctor C -10 / 10): av [0] A angle, [1] B = the orbiter's F, [2] / [3] the
 * target x / y (E, F). Idle: I += 12 (the body bobs 4 sin I), J counts; J > 30 with the player within 99 px across and
 * Y - 59 .. Y + 149 below -> F 1 (sound 151): 20 frames E + 1 (the blades draw back), then F 2: E + 6 a frame (+12 between 210
 * and 318), a 21-frame hold at 321, then up to 360 -> F 0, J -40. The blades hurt on the character sensor while B = 2: R-hurt 10
 * and MV.B -2 (left) / +2 (right). Blade angle min(max(2 (E - 30), 0), 315) (the right one -A); positions below. */
static void orb_spawn(obj_t *o) { o->A = 8; o->av[5] = floorf(o->x); o->av[6] = floorf(o->y); ek_set_a(o, "tho_body"); }
static void orb_frame(obj_t *o, player_t *p) {
    double *E = &o->av[0], *F = &o->av[1], *I = &o->av[2], *J = &o->av[3], *K = &o->av[4], *Gx = &o->av[5], *H = &o->av[6];
    if (*F < 6) { o->x = (float)(int)*Gx; o->y = (float)(int)(*H + 4 * ek_sin(*I)); *K += 1; }   /* 13628 */
    {                                                                              /* 13629 / 13630 / 13631: the blades */
        double X = floorf(o->x), Y = floorf(o->y), A = fmin(fmax(2 * (*E - 30), 0), 315);
        double r = 32 + fmax(0, (*E - 180) * 0.2);
        double by = (Y - 20) - r * ek_sin(2 * *E) + 3 * ek_sin(*I - 30);
        obj_t *b;
        for (int side = 0; side < 2; side++) {
            int t = side ? OT_TH_BLADER : OT_TH_BLADEL;
            EACH(t, b) {
                if (b->par != (int)(o - G.o)) continue;
                b->av[0] = A; b->av[1] = *F; ek_force_frame(b, 0);
                b->av[2] = side ? fmax(X + 20 + 32 * ek_sin(*E), X + 20) : fmin(X - 20 - 32 * ek_sin(*E), X - 20);
                b->av[3] = by;
                if (b->av[1] < 6) { b->ang = (float)(side ? ek_nrm(-A) : A); b->x = (float)(int)b->av[2]; b->y = (float)(int)b->av[3]; }
            }
        }
    }
    if (*K > 20 && *F < 6) { *K -= 80 - ((R(32768) * 5) >> 12); ek_restart_a(o, "tho_blink"); }   /* 13632 */
    if (ek_a_is(o, "tho_blink") && o->an.done) ek_set_a(o, "tho_body");             /* (anim 7 plays once: INFERRED back to 0) */
    if (*F == 0) { *I = fmod(*I + 12, 360); *J += 1; }                             /* 13633 */
    if (*F == 0 && *J > 30 && !ek_dead(p)) {                                       /* 13634 */
        double X = floorf(o->x), Y = floorf(o->y), px = ek_px(p), py = ek_py(p);
        if (X >= px - 99 && X <= px + 99 && Y <= py + 59 && Y >= py - 149) { *F = 1; *J = 0; *E = 0; snd_play_orig(151, 220, 128); }
    }
    if (*F == 1) { double e = *E + 1; if (e <= 360) { *E = e; *I = e + e + 180; } else { *E = 360; *I = 900; } *J += 1; }   /* 13635 */
    if (*F == 1 && *J > 20) { *F = 2; *J = 0; }                                    /* 13636 */
    if (*F == 2 && (*E < 318 || *J > 20)) { double e = *E + 6; if (e <= 360) { *E = e; *I = e + e + 180; } else { *E = 360; *I = 900; } }   /* 13637 */
    if (*F == 2 && *E >= 210 && *E < 318) *E = fmin(*E + 6, 360);                  /* 13638 */
    if (*F == 2 && *E >= 318) *J += 1;                                             /* 13639 */
    if (*F == 2 && *E >= 360) { *E = 0; *F = 0; *J = -40; }                        /* 13640 */
    if (DEFEATED(o) && *F < 6) *F = 6;                                             /* 13643 */
    if (*F == 6) ek_set_a(o, "tho_dead");                                          /* 13644 */
}
static void blade_frame(obj_t *b, player_t *p) {
    obj_t *o = ek_partner(b);
    if (!o || o->type != OT_TH_ORB) { b->alive = 0; return; }                     /* 13646-13648: more blades than orbiters */
    if (b->av[1] == 2 && ek_csens_hit(p, b) && ek_r_hurt(p, 10, 0))                /* 13641 / 13642 */
        ek_add_mvB(p, b->type == OT_TH_BLADEL ? -2 : 2);
}

/* ================================================================ EnemyRocketPuncher_3182 + Hand_3183 + Shield_3184 + Warning_3185
 * (RE 13649-13690; RV punch). Ctor A 25, E -24, F 16, G 24, I 1; hand B 1; shield A 99, E 1 (Q7 solid, Q20). av: [0] D state
 * (0 idle, 1 wind-up, 2 punch out, 3 back, 4 recover, 6 defeated), [1] E hand x offset, [2] F hand y offset, [3] G shield offset,
 * [4] I facing, [5] J timer, [6] Q explosions. 13649 every frame: hand at (X + E I, Y + F), A = D; shield at (X + G I, Y), frame
 * 4 - int(G / 5), D = D; warning A = X + E I, B = Y, C = D. Idle in view (150): faces the player, armoured (flag 5), J counts;
 * J > 30: D 1 (anim 7): 30 frames E + 8 (to 28), F - 2 (to 8), G - 2 (to 0); D 2: E + 10 to 200 for 40 frames (Milla's cube
 * sends it back, 13673); D 3: E - 10 to 28; D 4 (anim 8): back in 15 frames, J -40. The hand flying (A 2 / 3) hurts 20 and
 * pushes MV.B + 8 I (13676), breaks totems (13689) and barrels (13688: the gimmick module's). Defeated: anim 10, the burst
 * death (13681-13683: sound 29 x 10, explosion01, keystone, debris, sound explode3-5); the hand bursts (explosion02). */
static void punch_spawn(obj_t *o) {
    o->A = 25; o->av[1] = -24; o->av[2] = 16; o->av[3] = 24; o->av[4] = 1; o->hidden = 1; ek_set_a(o, "thr_idle");
}
static obj_t *part_of(const obj_t *o, int type) { obj_t *q; EACH(type, q) if (q->par == (int)(o - G.o)) return q; return NULL; }
static void punch_frame(obj_t *o, player_t *p) {
    double *D = &o->av[0], *E = &o->av[1], *F = &o->av[2], *Gs = &o->av[3], *I = &o->av[4], *J = &o->av[5], *Q = &o->av[6];
    double X = floorf(o->x), Y = floorf(o->y);
    obj_t *h = part_of(o, OT_TH_PHAND), *s = part_of(o, OT_TH_PSHIELD), *w = part_of(o, OT_TH_PWARN);
    if (h) { h->x = (float)(int)(*E * *I + X); h->y = (float)(int)(Y + *F); h->av[0] = *D; h->av[1] = *I; h->av[2] = *I; }   /* 13649 */
    if (s) { s->x = (float)(int)(*Gs * *I + X); s->y = (float)(int)Y; s->av[0] = *D; s->av[1] = *I; ek_force_frame(s, 4 - (int)(0.2 * *Gs)); }
    if (w) { w->av[0] = X + *E * *I; w->av[1] = Y; w->av[2] = *D; w->hidden = 1; }
    if (w && w->av[2] > 0 && w->av[2] < 3 && w->av[1] > G.camy && w->av[1] < G.camy + SCREEN_H) {   /* 13650 / 13651 (the outer bound INFERRED) */
        if (w->av[0] > G.camx + SCREEN_W + 30 && w->av[0] < G.camx + SCREEN_W + 200) { w->y = (float)(int)w->av[1]; w->x = (float)(int)(G.camx + SCREEN_W - 20); w->hidden = 0; }
        else if (w->av[0] <= G.camx - 30 && w->av[0] > G.camx - 200) { w->y = (float)(int)w->av[1]; w->x = (float)(int)(G.camx + 20); w->hidden = 0; }
    }
    if (*I == 1) ek_face(o, 0); else if (*I == -1) ek_face(o, 16);                 /* 13652 / 13653 */
    if (*D == 0 && in_view(o, 150) && !ek_dead(p)) {                               /* 13666 / 13667 */
        o->fl |= 0x20; *I = floorf(o->x) < ek_px(p) ? 1 : -1; *J += 1;
    }
    if (*D == 0 && *J > 30) { *D = 1; ek_set_a(o, "thr_wind"); o->fl &= ~0x20; *J = 0; }   /* 13668 */
    if (*D == 1) { *Gs = fmax(*Gs - 2, 0); *F = fmax(*F - 2, 8); *E = fmin(*E + 8, 28); *J += 1; }   /* 13669 */
    if (*D == 1 && *J > 30) { *D = 2; *J = 0; }                                    /* 13670 */
    if (*D == 2) { *E = fmin(*E + 10, 200); *J += 1; }                             /* 13671 */
    if (*D == 2 && *J > 40) { *D = 3; *J = 0; }                                    /* 13672 */
    if (*D == 2 && h && player_cube_hits(h)) { *D = 3; *J = 0; }                  /* 13673: on PlayerReflect (any visible cube; RV punch_cube_c4: the held cube, D 1 -> 4 in one frame) */
    if (*D == 3) *E = fmax(*E - 10, 28);                                           /* 13674 */
    if (*D == 3 && *E <= 28) { *D = 4; *J = 0; ek_set_a(o, "thr_back"); }          /* 13675 */
    if (*D == 4) { *Gs = fmin(*Gs + 2, 24); *F = fmin(*F + 2, 16); *E = fmax(*E - 8, -24); *J += 1; }   /* 13677 */
    if (*D == 4 && *J > 15) { *D = 0; *J = -40; ek_set_a(o, "thr_idle"); }         /* 13678 (anim 8 holds its last frame: back to 0 INFERRED) */
    if (DEFEATED(o) && *D != 6) { ek_set_a(o, "thr_dead"); *D = 6; o->fl &= ~0x20; }   /* 13680 */
    if (DEFEATED(o) && ek_burst_death(o, Q, 29, 0)) {                              /* 13681-13683 */
        if (h) h->alive = 0;                                                       /* 13685-13687: more parts than punchers */
        if (s) s->alive = 0;
        if (w) w->alive = 0;
    }
}
static void phand_frame(obj_t *h, player_t *p) {
    obj_t *o = ek_partner(h);
    if (!o || o->type != OT_TH_PUNCH) { h->alive = 0; return; }                   /* 13684 / 13685 */
    double A = h->av[0];
    if (A == 2) ek_set_a(h, "thr_fly");                                            /* 13664 */
    else if (A == 1 || A == 3) ek_set_a(h, "thr_hand2");                           /* 13663 */
    else if (A == 0 || A == 4) ek_set_a(h, "thr_hand");                            /* 13662 */
    if (h->av[1] == 1) ek_face(h, 0); else if (h->av[1] == -1) ek_face(h, 16);     /* 13654 / 13655 */
    if (A == 6) { if (!(h->fl & 0x40)) { h->fl |= 0x40; h->hidden = 1; obj_effect(h->x, h->y, "explosion02"); } return; }   /* 13665 */
    h->hidden = 1;                                                                 /* (drawn by th_enemy_draw with its puncher) */
    if (A >= 2 && A <= 3) {
        if (ek_csens_hit(p, h) && ek_r_hurt(p, 20, 0)) ek_add_mvB(p, 8 * h->av[2]);   /* 13676 */
        obj_t *t; EACH(OT_TH_TOTEM, t) if (!DEFEATED(t) && fabsf(t->x - h->x) < 96 && fabsf(t->y - h->y) < 96 && obj_obj_hit(h, t)) totem_kill(t);   /* 13689 */
        th_gim_barrels_hit_by(h);                                                  /* 13688 (th_gim.c) */
    }
}
static void pshield_frame(obj_t *s) {
    obj_t *o = ek_partner(s);
    if (!o || o->type != OT_TH_PUNCH) { s->alive = 0; return; }
    if (s->av[1] == 1) ek_face(s, 0); else if (s->av[1] == -1) ek_face(s, 16);     /* 13660 / 13661 */
    if (s->av[0] < 6) s->fl &= ~1; else s->fl |= 1;                                /* 13658 / 13659: not solid once its puncher is beaten */
    s->hidden = 1; s->fl &= ~0x40; s->A = 99;                                      /* (99 hit points: it never breaks; INFERRED kept) */
}

/* ================================================================ EnemyBikeNinja_3203 + Mask_3204 + Warning_3206 (RE 13732-13794;
 * RV bike). The invisible mask (Q20, A 2, S 4 crystals, D -1, E 1, U -4) runs the AI; the bike is its sprite one frame behind
 * (13786: A = D, B = E, C = H, D = mask flag 4, E = mask C, at the mask's position before it moves).
 * Mask av: [0] D (-1 waiting, 0 riding, 1 airborne past the rise), [1] E target side, [2] F float x, [3] G float y, [4] H x speed,
 * [5] I y speed, [6] J turn timer, [7] K jump timer; L in tm, Q (warning) in st. RE: waits until in view (80), then H = MV.B; rides
 * toward the side E (H +- 0.2 a frame, +- 0.4 below 2, max 10), J > 30 re-aims at the player (J = R(20)); a reverse skids
 * (sound 335 every 0.1 s); K > 30 on the ground in view (60): jump (sound 232, I -10, K = 10 - R(30)); I + 0.3 in the air
 * (max 15), D 1 once I > U; a star (sound 71) once per jump at I > U + 2 in view (80); a yellow spring sends it up (I -12,
 * sound 5). Defeated: sound 29, the EnemyBikeNinjaDefeated sprite flies with the knock-back, explosion01 at y - 20, debris. */
static void bmask_spawn(obj_t *m) {
    m->A = 2; m->absorbed = 4; m->av[0] = -1; m->av[1] = 1; m->av[2] = floorf(m->x); m->av[3] = floorf(m->y); m->hidden = 1;
    ek_set_a(m, "thb_m0");
}
static void bstar_new(const obj_t *m) {                                            /* 13765: EnemyNinjaStar at the action point - (0, 12), A 7 */
    double ax, ay; relic_act_point(m, &ax, &ay);                                   /* (RV bike: mask (2248, 991) -> star (2223, 990)) */
    obj_t *s = spawn(OT_TH_STAR, "thb_star", (float)(int)ax, (float)(int)ay - 12, 0);
    if (s) { s->av[0] = 7; s->layer = 3; }
    snd_play_orig(71, 220, 128);
}
static void bmask_frame(obj_t *m, player_t *p) {
    obj_t *b = ek_partner(m);
    double *D = &m->av[0], *E = &m->av[1], *F = &m->av[2], *Gy = &m->av[3], *H = &m->av[4], *I = &m->av[5], *J = &m->av[6], *K = &m->av[7];
    const double U = -4;
    if (m->fl & FLGONE) { m->alive = 0; if (b) b->alive = 0; return; }           /* 13788 / 13789 */
    double X = floorf(m->x), px = ek_px(p);
    if (*D == -2) { *H = 0; *I = 15; }                                             /* 13746: dropped in (TH3 arena): straight down */
    if (*D == -2 && (m->fl & FL4)) {                                               /* 13747: landed: riding, a white burst, sound 61 */
        *D = 0; obj_effect(m->x, m->y, "wburst"); snd_play_orig(61, 220, 128);
    }
    m->st = 0;                                                                     /* 13740 */
    if (*D == 0 && X > G.camx + SCREEN_W + 30 && X < G.camx + SCREEN_W + 230) m->st = 1;   /* 13741 (outer bound INFERRED) */
    if (*D == 0 && X < G.camx - 30 && X > G.camx - 230) m->st = 1;                 /* 13742 */
    if (*D == -1 && in_view(m, 80)) { *D = 0; *H = p->ground ? p->gsp : p->vx; }   /* 13748: H = MV.B */
    if (*D == 0) *J += 1;                                                          /* 13749 */
    if (*D == 0 && *J > 30 && px < *F) { *E = -1; *J = (R(32768) * 5) >> 13; }    /* 13750 */
    if (*D == 0 && *J > 30 && px > *F) { *E = 1; *J = (R(32768) * 5) >> 13; }     /* 13751 */
    if (S.skid_tick && *D == 0 && in_view(m, 80) && ((*E == -1 && *H > 0) || (*E == 1 && *H < 0))) {   /* 13752 / 13753 */
        snd_play_orig(335, 220, 128); S.skid_tick = 0;                             /* (one sample: channel 23) */
    }
    if (*D == 0 && *E == -1) { *H = fmax(*H - 0.2, -10); if (*H > -2) *H = fmax(*H - 0.2, -10); }   /* 13754 / 13755 */
    if (*D == 0 && *E == 1) { *H = fmin(*H + 0.2, 10); if (*H < 2) *H = fmin(*H + 0.2, 10); }      /* 13756 / 13757 */
    if (*D == 0 && (m->fl & FL4)) *K += 1;                                         /* 13758 */
    if (*D == 0 && *K > 30 && (m->fl & FL4) && in_view(m, 60)) {                   /* 13759 / 13760 */
        snd_play_orig(232, 220, 128); m->fl &= ~FL13; *I = -10; *K = 10 - ((R(32768) * 30) >> 15);
    }
    if (*D == 0 && *I >= 0) {                                                      /* 13761: a yellow spring */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *s = &G.o[i];
            if (!s->alive || s->type != OT_SPRING || !s->an.b || fabsf(s->x - m->x) > 96 || fabsf(s->y - m->y) > 96 || !obj_obj_hit(m, s)) continue;
            snd_play_orig(5, 220, 128); *I = -12; break;                           /* (the spring's bounce frame / beam: INFERRED not drawn) */
        }
    }
    if ((m->fl & FL4) && *I > 0) *I = 0;                                           /* 13762 */
    if (!(m->fl & FL4)) *I = fmin(*I + 0.3, 15);                                   /* 13763 */
    if (*D == 0 && !(m->fl & FL4) && !(m->fl & FL13) && *I > U) *D = 1;           /* 13764 */
    if (*D == 1 && *I > U + 2 && !(m->fl & FL13)) { if (in_view(m, 80)) bstar_new(m); m->fl |= FL13; }   /* 13765 / 13766 */
    if (*D == 1 && (m->fl & FL4)) *D = 0;                                          /* 13767 */
    /* 13775-13785: the sensors (directions 1 floor, 2 right wall, 3 left wall, 4 ceiling), as the Elite's (jc_enemy.c) */
    m->fl &= ~(FL4 | FLW1 | FLW2 | FLW3);
    m->y += 1;                                                                     /* 13775 */
    if (ek_mask_sensor(m, "thb_m", 1)) { m->y -= 1; m->fl |= FL4; m->tm = 0; *Gy -= 1; }   /* 13776 */
    for (int k = 0; k < 3; k++) if (ek_mask_sensor(m, "thb_m", 1)) { m->y -= 1; *Gy -= 1; }   /* 13777: three copies in the event list (RE handle_frame_59) */
    for (int k = 0; k < 4; k++) if (*D >= 0 && !ek_mask_sensor(m, "thb_m", 1)) { m->y += 1; *Gy += 1; m->tm += 1; }   /* 13778: four copies (RV: G + I + 4, L + 4 a frame in the air) */
    if (ek_mask_sensor(m, "thb_m", 2)) { m->fl |= FLW1; *H = fmin(*H, 0); }
    if (ek_mask_sensor(m, "thb_m", 3)) { m->fl |= FLW2; *H = fmax(*H, 0); }
    if (ek_mask_sensor(m, "thb_m", 4)) { m->fl |= FLW3; *I = fmax(*I, 0); }       /* (the ceiling's effect INFERRED as the Elite's 11004) */
    if (b) {                                                                       /* 13786: the sprite at the old position */
        b->x = m->x; b->y = m->y; b->av[0] = *D; b->av[1] = *E; b->av[2] = *H; b->av[3] = (m->fl & FL4) ? 1 : 0; b->av[4] = m->c;
        b->inv = m->inv;                                                           /* 13738 / 13739: the hit flash */
    }
    *F += *H; *Gy += *I; m->x = (float)(int)*F; m->y = (float)(int)*Gy;
    if (m->y > G.lvl.h + 400) { m->alive = 0; if (b) b->alive = 0; return; }
    if (DEFEATED(m) && !(m->fl & FLGONE)) {                                        /* 13787 */
        snd_play_orig(29, 220, 128);
        obj_t *d = spawn(OT_TH_BDEAD, "thb_dead", m->x, m->y, *E > 0 ? 0 : 16);
        if (d) { d->av[0] = m->kx; d->av[1] = m->ky; }
        obj_effect(m->x, m->y - 20, "explosion"); ek_debris(m, 4);
        m->fl |= FLGONE; if (b) b->hidden = 1;
    }
}
static void bike_frame(obj_t *b) {
    obj_t *m = ek_partner(b);
    if (!m || m->type != OT_TH_BMASK) { b->alive = 0; return; }                   /* 13789 / 13790 */
    if (b->av[1] == 1) ek_face(b, 0); else if (b->av[1] == -1) ek_face(b, 16);     /* 13733 / 13734 */
    if (b->av[0] == -2) ek_set_a(b, "thb_drop");                                   /* 13745: anim 10 while dropped in (TH3 arena) */
    if (b->av[0] == 0 && b->av[3] == 1) ek_set_a(b, "thb_ride");                   /* 13735 */
    if (b->av[0] == 0 && b->av[3] == 0) ek_set_a(b, "thb_jump");                   /* 13736 */
    if (b->av[0] == 1) ek_set_a(b, "thb_throw");                                   /* 13737 */
}
static void bwarn_frame(obj_t *w) {                                                /* 13743 / 13744 */
    obj_t *m = ek_partner(w);
    if (!m || m->type != OT_TH_BMASK || (m->fl & FLGONE)) { w->alive = 0; return; }
    w->x = (float)(int)fmax(fmin(floorf(m->x), G.camx + SCREEN_W - 20), G.camx + 20); w->y = floorf(m->y);
    w->hidden = m->st != 1;
}
static void bdead_frame(obj_t *d) {                                                /* 13791 / 13792 */
    d->x = (float)(int)(floorf(d->x) + d->av[0]); d->y = (float)(int)(floorf(d->y) + d->av[1]); d->av[1] = fmin(d->av[1] + 0.3, 15);
    if (!in_view(d, 60)) d->alive = 0;
}
/* EnemyNinjaStar_3221 (13768-13774): A 7. Spins (angle - 2A), hovers 40 frames, aims (A = -A when the player is left of it) and
 * flies (x + A, y + |A|, direction 1); off the view (50) or into the background (sound 240, white burst) it goes; R-hurt 5 and
 * MV.B - 2 on the character sensor; Milla's cube reflects it once (sound 219, A = -A). */
static void star_frame(obj_t *s, player_t *p) {
    double *A = &s->av[0], *B = &s->av[1];
    s->ang = (float)ek_nrm(s->ang - 2 * *A); *B += 1;                              /* 13768 */
    if (*B == 40 && ek_px(p) < floorf(s->x)) *A = -*A;                             /* 13769 */
    if (*B > 40) { s->x = (float)(int)(floorf(s->x) + *A); ek_set_a(s, "thb_star1"); s->y = (float)(int)(floorf(s->y) + fabs(*A)); }   /* 13770 */
    if (!in_view(s, 50)) { s->alive = 0; return; }                                 /* 13771 */
    if (*B > 40 && !(s->fl & FL2) && player_cube_hits(s)) { snd_play_orig(219, 220, 128); s->fl |= FL2; *A = -*A; }   /* 13772 (PlayerReflect shown) */
    if (*B > 40 && obj_overlaps_bg(s)) { snd_play_orig(240, 220, 128); obj_effect(s->x, s->y, "wburst"); s->alive = 0; return; }   /* 13773 */
    if (*B > 40 && ek_csens_hit(p, s) && ek_r_hurt(p, 5, 0)) ek_add_mvB(p, -2);   /* 13774 */
}

/* TH3 arena waves (the boss module's 14054; RE create_effectbikeninjawarning_3206 / mask_3204 / bikeninja_3203 at (x, y)): the
 * warning, the mask (D -2: dropped in, 13745-13747; F the float x, E the side) and its bike. th_enemy_bikes_alive counts the masks
 * the arena dropped that are still up (the original counts every EnemyBikeNinjaMask of the frame: the placed ones are far behind
 * and gone by then, INFERRED) */
obj_t *th_enemy_bike_drop(float x, float y, double F, double E) {
    if (!S.on) return NULL;
    obj_t *w = spawn(OT_TH_BWARN, "thb_warn", x, y, 0), *m = spawn(OT_TH_BMASK, "thb_m0", x, y, 0), *b = spawn(OT_TH_BIKE, "thb_drop", x, y, 0);
    if (!m) { if (w) w->alive = 0; if (b) b->alive = 0; return NULL; }
    bmask_spawn(m); m->layer = 3; m->fl |= FLARENA;
    m->av[0] = -2; m->av[1] = E; m->av[2] = F; m->av[3] = floorf(y); m->x = (float)(int)F;
    if (w) { w->hidden = 1; w->layer = 3; w->par = (int)(m - G.o); }
    if (b) { b->layer = 3; b->par = (int)(m - G.o); b->av[0] = -2; b->av[1] = E; b->x = m->x; ek_face(b, E < 0 ? 16 : 0); m->par = (int)(b - G.o); }
    return m;
}
int th_enemy_bikes_alive(void) {
    if (!S.on) return 0;
    int n = 0; obj_t *m;
    EACH(OT_TH_BMASK, m) if ((m->fl & FLARENA) && !(m->fl & FLGONE)) n++;
    return n;
}

/* ================================================================ spawn / passes */
static void pair(int child, int owner) {
    obj_t *o, *q;
    EACH(child, o) { float best = 1e9f; EACH(owner, q) { float d = fabsf(q->x - o->x) + fabsf(q->y - o->y); if (d < best) { best = d; o->par = (int)(q - G.o); } } }
}
void th_enemy_spawn(void) {
    static int reg;
    if (!reg) { reg = 1; snap_register(&S, sizeof S); snap_register(LST, sizeof LST); }   /* host search snapshots */
    memset(&S, 0, sizeof S); memset(LST, 0, sizeof LST);
    for (int t = OT_THE_FIRST; t <= OT_THE_LAST && !S.on; t++) S.on = objects_have(t);
    if (!S.on) return;
    ek_limiters_collect(&S.lim);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive || o->type < OT_THE_FIRST || o->type > OT_THE_LAST) continue;
        if (o->x < 0 || o->y < -400) { o->alive = 0; continue; }                 /* off-map templates (TH3 places some at x < 0) */
        memset(o->av, 0, sizeof o->av); o->st = o->tm = 0; o->fl = 0; o->dir = 0; o->an.b = NULL; o->par = -1; o->layer = 2; o->hidden = 0;
        o->A = 0; o->c = 0; o->bd = 0; o->absorbed = 0;
        ek_list_add(&LST[o->type - RT0], o, o->type);
        switch (o->type) {
        case OT_TH_TOTEM: o->A = 1; o->absorbed = 2; ek_set_a(o, "tht_body"); break;
        case OT_TH_PUP: pup_spawn(o); break;
        case OT_TH_PUPPET: puppet_spawn(o); break;
        case OT_TH_ORB: orb_spawn(o); break;
        case OT_TH_BLADEL: o->av[0] = 0; ek_set_a(o, "tho_bl"); break;
        case OT_TH_BLADER: ek_set_a(o, "tho_br"); break;
        case OT_TH_PUNCH: punch_spawn(o); break;
        case OT_TH_PHAND: o->av[2] = 1; o->hidden = 1; ek_set_a(o, "thr_hand"); break;
        case OT_TH_PSHIELD: o->A = 99; o->hidden = 1; ek_set_a(o, "thr_shield"); break;
        case OT_TH_PWARN: o->hidden = 1; o->layer = 3; ek_set_a(o, "thr_warn"); break;
        case OT_TH_BMASK: bmask_spawn(o); break;
        case OT_TH_BIKE: ek_set_a(o, "thb_jump"); break;
        case OT_TH_BWARN: o->hidden = 1; o->layer = 3; ek_set_a(o, "thb_warn"); break;
        default: o->alive = 0; break;
        }
    }
    /* Fusion pairs a child with its owner by instance order; the placements put each part next to its owner (RE th_inventory) */
    pair(OT_TH_BLADEL, OT_TH_ORB); pair(OT_TH_BLADER, OT_TH_ORB);
    pair(OT_TH_PHAND, OT_TH_PUNCH); pair(OT_TH_PSHIELD, OT_TH_PUNCH); pair(OT_TH_PWARN, OT_TH_PUNCH);
    pair(OT_TH_BIKE, OT_TH_BMASK); pair(OT_TH_BWARN, OT_TH_BMASK);
    { obj_t *o, *q; EACH(OT_TH_BMASK, q) EACH(OT_TH_BIKE, o) if (o->par == (int)(q - G.o)) q->par = (int)(o - G.o); }   /* the mask's sprite */
}
void th_enemy_pre(player_t *p) {                                                  /* before combat_frame (test_collisions + triggers) */
    if (!S.on) return;
    obj_t *t, *m, *b;
    int ballp = ball(p);
    S.skid_t += 1.f / 60; S.skid_tick = 0;                                         /* 13752 / 13753: a 0.1 s float timer (frame time added) */
    if (S.skid_t >= 0.1f) { S.skid_t = (float)(S.skid_t - 0.1); S.skid_tick = 1; }
    EACH(OT_TH_TOTEM, t) if (!(t->fl & FL1)) t->fl = ballp ? (t->fl | 1) : (t->fl & ~1);   /* 13494 / 13495 */
    EACH(OT_TH_BMASK, m) {
        if (DEFEATED(m)) continue;
        EACH(OT_TH_TOTEM, t) if (!DEFEATED(t) && fabsf(t->x - m->x) < 96 && fabsf(t->y - m->y) < 96 && obj_obj_hit(m, t)) totem_kill(t);   /* 13794 */
        for (int s = 0; s < 2; s++) EACH(s ? OT_TH_BLADER : OT_TH_BLADEL, b)        /* 13793: a swinging blade cuts a bike ninja */
            if (s == 0 && b->av[1] == 2 && fabsf(b->x - m->x) < 96 && fabsf(b->y - m->y) < 96 && obj_obj_hit(b, m)) m->A -= 5;
    }
}
void th_enemy_draw(int pass) {                                                    /* the Rocket Puncher rigs in their order */
    if (!S.on || pass != 1 || !LST[OT_TH_PUNCH - RT0].n) return;
    obj_t *o;
    EACH(OT_TH_PUNCH, o) {
        float ox = floorf(o->x - G.camx), oy = floorf(o->y - G.camy);
        if (ox < -260 || ox > SCREEN_W + 260 || oy < -120 || oy > SCREEN_H + 120) continue;
        obj_t *h = part_of(o, OT_TH_PHAND), *s = part_of(o, OT_TH_PSHIELD);
        const obj_t *seq[4]; int n = 0;
        if (s && s->an.frame >= 1) seq[n++] = s;                                   /* 13656: move_back */
        if (h && !(h->fl & 0x40)) seq[n++] = h;                                    /* (the hand behind the body: INFERRED from the images) */
        seq[n++] = o;
        if (s && s->an.frame < 1) seq[n++] = s;                                    /* 13657: move_front */
        for (int k = 0; k < n; k++) {
            const obj_t *q = seq[k];
            if (!q->an.b) continue;
            if (q->inv) r_set_invert(1);
            frame_draw(q->an.b, anim_frame(&q->an), floorf(q->x - G.camx) + (q->flip == 2 ? 1 : 0), floorf(q->y - G.camy), q->flip, 0xffffffff);
            if (q->inv) r_set_invert(0);
        }
    }
}
int th_enemy_obj(obj_t *o, player_t *p) {
    if (!S.on) return 0;
    switch (o->type) {
    case OT_TH_TOTEM: totem_frame(o); return 1;
    case OT_TH_PUP: pup_frame(o, p); return 1;
    case OT_TH_PUPPET: puppet_frame(o, p); return 1;
    case OT_TH_ORB: orb_frame(o, p); return 1;
    case OT_TH_BLADEL: case OT_TH_BLADER: blade_frame(o, p); return 1;
    case OT_TH_PUNCH: punch_frame(o, p); return 1;
    case OT_TH_PHAND: phand_frame(o, p); return 1;
    case OT_TH_PSHIELD: pshield_frame(o); return 1;
    case OT_TH_PWARN: if (!ek_partner(o)) o->alive = 0; return 1;
    case OT_TH_BMASK: bmask_frame(o, p); return 1;
    case OT_TH_BIKE: bike_frame(o); return 1;
    case OT_TH_BWARN: bwarn_frame(o); return 1;
    case OT_TH_STAR: star_frame(o, p); return 1;
    case OT_TH_BDEAD: bdead_frame(o); return 1;
    default: return 1;
    }
}
