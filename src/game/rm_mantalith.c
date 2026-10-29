/* Relic Maze act 5 end (frame index 28): leaving the labyrinth (6861-6865), the Classic Kingdom Stone scene (group
 * 0x22511, 6866-6888), the Mantalith intro (0x22514, 6889-6900), the fight (0x22519 + 0x22515, 6902-7059), its defeat
 * (7060-7084), the results through the shared timeline (results.c path 3, Sign.B = 179 at 7083, voices 6589-6592) and
 * the post-results scene (0x22510, 7085-7110) that ends the stage (7097). Source: build/re/spec_rm_mantalith.md (static
 * RE; its traces section 9 is not recorded yet), spec_rm_flow 1.6 / 2.3 / 2.5 / 3 / 5.3.
 *
 * The port plays Classic: the Adventure dialog cutscene (group 0x22517, 6600-6730, 6864: g0 >= 2) is not ported (RE: 6865
 * runs for every Classic game). No character test except the RE ones (7075 bike, 6589-6592 voices, 7093 / 7094).
 * Everything is attached to the engine types 450-483 (the frame's placed objects); the head and the two arm weak points
 * are q20 enemies of the shared rules (combat.c: HP A, hit timer C, invert flash, flag 5 armour, flag 6 defeated), the
 * shockwaves are relic_boss.c's HazardShockwave, the rocks aqua_boss.c's EnemyRock, the camera the level's
 * CameraLimitBoss markers. Part pointers are cached at spawn (no per-frame searches); one pass per frame moves the
 * runtime objects. Labels RE / INFERRED.
 *
 * Alterables: MT.b = the body's A..Z, MT.h = the head's, MT.ad = BossAnimationData's (an invisible holder, not placed:
 * its constructor values are below). Body flags bit n = flag n (1 attacking, 2 slash thrown, 4 tracking off, 5, 8 whoosh);
 * head flags: 2 shockwaves done, 3 landing thud done, 7 defeat processed, 12 roar pending (flag 5 / 6 = the obj's armour
 * / defeated bits of the shared rules). */
#include "../fp.h"
#include "story.h"
#include "cutscene.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DEFEATED(o) ((o)->fl & 0x40)
#define F_ARMOURED 0x20
extern int fe_music_volume;
static int R(int n) { return cross_rand(n); }
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 180. || m == 0.) ? 0. : fd_sin(d / 57.29577951308232); }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / 57.29577951308232); }
static double PX(const player_t *p) { return floor(p->x); }
static double PY(const player_t *p) { return floor(p->y - ORIG_REF_DY); }
static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

/* sprites (tools/import_frame.py MT_ENS), looked up once per level */
enum { AN_STONE, AN_SPADE, AN_SPADE_LAND, AN_SPADE_JUMP, AN_HOOK, AN_KS1, AN_KS2, AN_RAY, AN_SHRINE, AN_CUT, AN_BODY, AN_NECK4, AN_NECK3,
       AN_NECK2, AN_NECK1, AN_KNEE, AN_LEG2, AN_LEG3, AN_BEAD0, AN_BEAD1, AN_BEAD2, AN_BEAD3, AN_BEAD31, AN_H0_0, AN_H0_1, AN_H0_2,
       AN_H7_0, AN_H7_1, AN_H7_2, AN_H8, AN_HORN2, AN_HORN2_8, AN_HORN3, AN_HORN3_8, AN_ARM3_0, AN_ARM3_4, AN_ARM3_8, AN_ARM6_0,
       AN_ARM6_4, AN_ARM6_8, AN_ARM3_SW, AN_ARM6_SW, AN_HIT1, AN_HIT2, AN_WARN, AN_SLASH, AN_CHOMP, AN_RUINS, AN_RPIECE, AN_DUST,
       AN_FDEBRIS, AN_ROCK, AN_N };
static const char *const AN_NAMES[AN_N] = { "mt_stone", "mt_spade", "mt_spade_land", "mt_spade_jump", "mt_hook", "mt_ks1", "mt_ks2",
    "mt_ray", "mt_shrine", "mt_cut", "mt_body", "mt_neck4", "mt_neck3", "mt_neck2", "mt_neck1", "mt_knee", "mt_leg2", "mt_leg3",
    "mt_bead0", "mt_bead1", "mt_bead2", "mt_bead3", "mt_bead31", "mt_h0_0", "mt_h0_1", "mt_h0_2", "mt_h7_0", "mt_h7_1", "mt_h7_2",
    "mt_h8", "mt_horn2", "mt_horn2_8", "mt_horn3", "mt_horn3_8", "mt_arm3_0", "mt_arm3_4", "mt_arm3_8", "mt_arm6_0", "mt_arm6_4",
    "mt_arm6_8", "mt_arm3_sw", "mt_arm6_sw", "mt_hit1", "mt_hit2", "mt_warn", "mt_slash", "mt_chomp", "mt_ruins", "mt_rpiece",
    "dust", "debris", "rock" };   /* the last three: shared rows (EffectsDust_437, EffectsDebris_427 frames 0-10, EnemyRock_1290) */
static int A_[AN_N];
static void setan(obj_t *o, int k) { int a = A_[k]; if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }
static void restartan(obj_t *o, int k) { int a = A_[k]; if (a >= 0) { anim_set(&o->an, &G.ens, a); anim_restart(&o->an); } }

enum { HF2 = 1 << 2, HF3 = 1 << 3, HF7 = 1 << 7, HF12 = 1 << 12 };
enum { BF1 = 1 << 1, BF2 = 1 << 2, BF5 = 1 << 5, BF8 = 1 << 8 };
static struct {
    int on;
    int g_exit, g_scene, g_intro, g_fight, g_atk, g_post, g_lilac, g_carol;   /* groups 0x2250f 0x22511 0x22514 0x22519 0x22515 0x22510 0x22512 0x2250c */
    obj_t *stone, *spade, *hook, *ks[2], *ray[2], *shrine, *cut[2], *body, *neck[4] /* Neck4, 3, 2, 1 */, *lknee, *rknee, *leg2, *leg3;
    obj_t *bead[16] /* by bead A */, *head, *horn2, *horn3, *arm[2] /* Arm3 (left), Arm6 (right) */, *hit[2] /* Hitbox1, 2 */, *warn;
    float limL, limR, limB, c0;
    double b[26], h[26], ad[26];
    int bfl, hfl;
    int armD[2], armE[2], armDir[2], armSw[2];
    int hAnim, hDir, horn8;
    double cutA[2];
    /* scene (stone D / E / A / B / C, Spade A) */
    int ex, sD, sE, spLanded; double sA, sB, sC, spA;
    int blk, blkUp, blkA, wht, whtStep;                                       /* LevelCardFade(2) black / white (direction 8) alpha, -1 none */
    int mus_fade, snd147, snd149, once6890, once6896, once7069, once7071, once7086, horn_once, bZ;
    double target;
    int timer_off, toff, kill_time;
    int res_on, post_fade, ended, postK;
    int adv;                                                                  /* the Adventure dialogue (0x22517) is playing */
} MT;
#define B_(c) MT.b[(c) - 'A']
#define H_(c) MT.h[(c) - 'A']
#define AD(c) MT.ad[(c) - 'A']

static obj_t *spawn(int type, int an, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    setan(o, an);
    return o;
}
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {      /* PlayerCharacterSensor_375 */
    int X = (int)PX(p), Y = (int)PY(p);
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return o->an.b && !o->hidden && obj_rect_hit(o, a, b, c, d); }
static int vulnerable(const player_t *p) { return p->state != ST_DEAD && !p->hurt_pending && !player_invincible(p); }   /* MV.R = 0, OTHERS.E <= 0 */
static void shake_min(float z) { float n = G.cam.Z + z; G.cam.Z = n < z ? n : z; }   /* CAM.Z = min(Z + z, z) */
static int hit_alive(int k) { return MT.hit[k]->A > 0; }
static void ad_rest(void) { AD('D') = 0; AD('F') = -16; AD('J') = 16; AD('H') = -24; AD('L') = 24; }

/* ===================================================================================================== spawn (7144) */
void rm_mantalith_spawn(void) {
    memset(&MT, 0, sizeof MT);
    MT.blk = MT.wht = -1;
    if (!objects_have(OT_MT_BODY) || !objects_have(OT_MT_HEAD)) return;
    for (int k = 0; k < AN_N; k++) A_[k] = ens_anim(AN_NAMES[k]);
    obj_t *beads[16]; int nb = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_MT_STONE: MT.stone = o; break;            case OT_MT_SPADE: MT.spade = o; break;
        case OT_MT_HOOK: MT.hook = o; break;              case OT_MT_KS1: MT.ks[0] = o; break;
        case OT_MT_KS2: MT.ks[1] = o; break;              case OT_MT_RAY1: MT.ray[0] = o; break;
        case OT_MT_RAY2: MT.ray[1] = o; break;            case OT_MT_SHRINE: MT.shrine = o; break;
        case OT_MT_CUT1: MT.cut[0] = o; break;            case OT_MT_CUT2: MT.cut[1] = o; break;
        case OT_MT_BODY: MT.body = o; break;              case OT_MT_NECK4: MT.neck[0] = o; break;
        case OT_MT_NECK3: MT.neck[1] = o; break;          case OT_MT_NECK2: MT.neck[2] = o; break;
        case OT_MT_NECK1: MT.neck[3] = o; break;          case OT_MT_LKNEE: MT.lknee = o; break;
        case OT_MT_RKNEE: MT.rknee = o; break;            case OT_MT_LEG2: MT.leg2 = o; break;
        case OT_MT_LEG3: MT.leg3 = o; break;              case OT_MT_BEAD: if (nb < 16) beads[nb++] = o; break;
        case OT_MT_HEAD: MT.head = o; break;              case OT_MT_HEAD2: MT.horn2 = o; break;
        case OT_MT_HEAD3: MT.horn3 = o; break;            case OT_MT_ARM3: MT.arm[0] = o; break;
        case OT_MT_ARM6: MT.arm[1] = o; break;            case OT_MT_HIT1: MT.hit[0] = o; break;
        case OT_MT_HIT2: MT.hit[1] = o; break;            case OT_MT_WARN: MT.warn = o; break;
        case OT_CAMLIM_L: MT.limL = o->x; break;          case OT_CAMLIM_R: MT.limR = o->x; break;
        case OT_CAMLIM_B: MT.limB = o->y; break;
        }
    }
    if (!MT.body || !MT.head || !MT.horn2 || !MT.horn3 || !MT.arm[0] || !MT.arm[1] || !MT.hit[0] || !MT.hit[1] || !MT.lknee || !MT.rknee ||
        !MT.leg2 || !MT.leg3 || nb < 16 || !MT.neck[0] || !MT.neck[1] || !MT.neck[2] || !MT.neck[3]) return;
    if (!MT.limR) { MT.limL = 3344; MT.limR = 7871; MT.limB = 1623; }          /* the RM5 markers (frame_29.json), if the level lacks them */
    MT.on = 1;
    for (int a = 0; a < 16; a++) {                                            /* 7144: A = 0..15, last created first */
        obj_t *o = MT.bead[a] = beads[nb - 1 - a];
        static const int DIR[16] = { 31, 31, 31, 31, 1, 1, 2, 2, 3, 3, 1, 1, 2, 2, 3, 3 };
        setan(o, DIR[a] == 31 ? AN_BEAD31 : AN_BEAD0 + DIR[a]); o->layer = 1; o->hidden = 1;
    }
    obj_t *l1[] = { MT.body, MT.neck[0], MT.neck[1], MT.neck[2], MT.neck[3], MT.lknee, MT.rknee, MT.leg2, MT.leg3 };
    static const int A1[] = { AN_BODY, AN_NECK4, AN_NECK3, AN_NECK2, AN_NECK1, AN_KNEE, AN_KNEE, AN_LEG2, AN_LEG3 };
    for (int k = 0; k < 9; k++) { setan(l1[k], A1[k]); l1[k]->layer = 1; l1[k]->hidden = 1; }   /* layer 1: behind the play layer (RE 0) */
    setan(MT.head, AN_H0_0); setan(MT.horn2, AN_HORN2); setan(MT.horn3, AN_HORN3);
    setan(MT.arm[0], AN_ARM3_0); setan(MT.arm[1], AN_ARM6_0);
    obj_t *l2[] = { MT.head, MT.horn2, MT.horn3, MT.arm[0], MT.arm[1] };
    for (int k = 0; k < 5; k++) { l2[k]->hidden = 1; l2[k]->layer = 2; }
    MT.head->y = MT.arm[0]->y = MT.arm[1]->y = 0;                              /* 7144: arms and head to y 0 */
    for (int k = 0; k < 2; k++) {                                              /* Hitbox1 / 2: invisible q20, A 15 (ctor) */
        obj_t *hb = MT.hit[k]; setan(hb, k ? AN_HIT2 : AN_HIT1); hb->hidden = 1; hb->A = 15; hb->bd = 0; hb->fl = F_NOFLY; hb->c = 0;
    }
    MT.head->A = 10; MT.head->bd = 0; MT.head->fl = F_NOFLY | F_ARMOURED;      /* ctor A 10; flag 16 (stays), armoured */
    B_('C') = MT.body->x; B_('D') = MT.body->y; B_('I') = 260; B_('J') = 200; B_('N') = 8;   /* ctor I 260, J 200, N 8; 7144 C / D */
    MT.c0 = MT.body->x;
    AD('F') = -16; AD('G') = 40; AD('H') = -24; AD('I') = -40; AD('J') = 16; AD('K') = 40; AD('L') = 24; AD('M') = -40;   /* BossAnimationData ctor */
    AD('O') = -40; AD('P') = 24; AD('Q') = 340; AD('S') = 40; AD('T') = 24; AD('U') = 20;
    if (MT.stone) setan(MT.stone, AN_STONE);
    if (MT.spade) setan(MT.spade, AN_SPADE);
    if (MT.hook) { setan(MT.hook, AN_HOOK); MT.hook->hidden = 1; }            /* drawn by rm_mantalith_draw (y-scaled) */
    for (int k = 0; k < 2; k++) {
        if (MT.ks[k]) { setan(MT.ks[k], k ? AN_KS2 : AN_KS1); MT.ks[k]->hidden = 1; }   /* ctor: invisible */
        if (MT.ray[k]) { setan(MT.ray[k], AN_RAY); MT.ray[k]->hidden = 1; }            /* drawn by rm_mantalith_draw (x-scaled, additive) */
        if (MT.cut[k]) { setan(MT.cut[k], AN_CUT); MT.cut[k]->hidden = 1; }            /* drawn with their alpha */
        MT.cutA[k] = 128;
    }
    if (MT.shrine) setan(MT.shrine, AN_SHRINE);
    if (MT.warn) { setan(MT.warn, AN_WARN); MT.warn->hidden = 1; MT.warn->layer = 5; }   /* ctor: invisible, layer 5 */
    MT.g_exit = 1;
}
int rm_mantalith_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }   /* every type is moved by rm_mantalith_frame */

/* ===================================================================================================== helpers */
static float shrine_x(void) { return MT.shrine ? MT.shrine->x : 3568; }
static float shrine_y(void) { return MT.shrine ? MT.shrine->y : 888; }
static void ruin_piece(double x, double y) {                                  /* BreakableRuinsPiece_1451 (6891 / 7089; motion 5675 / 5676) */
    double A = (R(21) - 10) * 0.1, B = R(40) * 0.1;
    double d = x, a;
    obj_t *q = spawn(OT_MT_RPIECE, AN_RPIECE, (float)d, (float)y, 2);
    a = R(360);
    if (!q) return;
    q->av[0] = A; q->av[1] = B; q->av[2] = 0.3; q->fx = (float)d; q->fy = (float)y; q->ang = (float)a;
}
static void arm_point(int k, double *x, double *y) { relic_act_point(MT.arm[k], x, y); }
static void slash_new(int k, double ang, int dir) {                           /* BossSlash_1598 at the claw's action point */
    double x, y; arm_point(k, &x, &y);
    obj_t *s = spawn(OT_MT_SLASH, AN_SLASH, (float)x, (float)y, MT.arm[k]->layer);
    if (s) { s->ang = (float)ang; s->dir = dir; }
}
static void rock_new(int k, int dir) {                                        /* EnemyRock_1290 (aqua_boss.c rules): 6949-6956 */
    double x, y; arm_point(k, &x, &y);
    obj_t *r = spawn(OT_ROCK_AT, AN_ROCK, (float)x, (float)(MT.arm[k]->y + 40), 2);
    if (!r) return;
    r->scale = (float)(1 - 0.05 * R(10)); r->ang = (float)R(360);             /* scale 0.55..1 (INFERRED range) */
    double sp = 50 + 10 + R(10), a = dir * 11.25 / 57.29577951308232;        /* PinballMovement speed (1/10 px) */
    r->vx = (float)(sp * cos(a)); r->vy = (float)(-sp * sin(a)); r->fx = r->fy = 0; r->tm = 0;
}
static obj_t *debris_new(double x, double y, int an, double ang, double C, double D, int E, int layer) {   /* BossMantalithDebris_1617 */
    obj_t *d = spawn(OT_MT_DEBRIS, an, (float)x, (float)y, layer);
    if (d) { d->ang = (float)ang; d->av[0] = 0; d->av[1] = 0; d->av[2] = C; d->av[3] = D; d->av[4] = E; }
    return d;
}
static void parts_visible(int vis) {                                          /* 6998 / 7004 (beads, feet, arms, knees, necks, horns, body, head) */
    obj_t *l[] = { MT.body, MT.neck[0], MT.neck[1], MT.neck[2], MT.neck[3], MT.lknee, MT.rknee, MT.leg2, MT.leg3, MT.head, MT.horn2, MT.horn3, MT.arm[0], MT.arm[1] };
    for (int k = 0; k < 14; k++) l[k]->hidden = !vis;
    for (int a = 0; a < 16; a++) MT.bead[a]->hidden = !vis;
}
static void music_boss(void) {                                                /* 6896: unlock, stop, play slot 4 (m_boss) */
    MT.mus_fade = 0; music_volume(fe_music_volume); music_stop(); music_play("bgm/m_boss.adp");
}

/* ===================================================================================================== 6861-6865 */
static void exit_events(player_t *p) {
    if (!MT.ex && p->x > 2920 - 213) {                                        /* 6861: MV.X > 2920 - CAM.X, once */
        MT.ex = 1; MT.timer_off = 1; G.input_lock = 1;                         /* timer / pause groups off, CTRL.M = 1 */
        MT.blk = 0; MT.blkUp = 1;                                              /* LevelCardFade2 alpha 0 */
        G.cam.M3 = 1; G.cam_m2 = 0;                                            /* CAM.M = 3: the camera stays */
        MT.mus_fade = 180;                                                     /* fadeout_music(3000) */
    }
    if (!MT.ex) return;
    if (MT.blkUp) MT.blk = 2 * (MT.blk >> 1) + 4 > 255 ? 255 : 2 * (MT.blk >> 1) + 4;   /* 6862 */
    p->face = 1; if (p->ground) { if (p->gsp < 3) p->gsp = 3; } else if (p->vx < 3) p->vx = 3;   /* CTRL.D = 1 (auto-run right; speed INFERRED) */
    if (MT.blk >= 255) {                                                       /* 6864 (Adventure) / 6865 (Classic) */
        p->x = 0; p->y = 1432 + ORIG_REF_DY; p->vx = p->vy = p->gsp = 0; p->face = 1; p->state = ST_NORMAL;
        p->scripted = 1;                                                       /* parked out of view (6881); player block off */
        MT.blkUp = 0; MT.blk = 255; MT.blkA = 0;                               /* fade2 destroyed; a black LevelCardFade at the view */
        MT.g_exit = 0;
        /* 6864 (session 25b): g0 >= 2, g1 < 4 and ML flag 0x80 off -> the Adventure dialogue (group 0x22517, DialogData_1624,
         * m_trouble), played as the in-stage scene rm5_spade (RV b28_g0_l / b28_g0_c, story.c story_inline_begin) */
        if (!chr_is_milla(G.character) && !(G.stage_flags & 0x80) && story_gate()) {
            G.stage_flags |= 0x80;                                             /* ML 0x80: once */
            if (story_inline_begin("rm5_spade") == 0) {
                MT.adv = 1; MT.blk = -1; MT.mus_fade = 0; music_volume(fe_music_volume); MT.sC = 888;
                return;
            }
        }
        MT.g_scene = 1;                                                        /* 6865: MV.M = 0, group 0x22511 on */
    }
}

/* ===================================================================================================== 6866-6888 */
static void fade_down(int *a, int step) { if (*a < 0) return; *a = 2 * (*a >> 1) - step; if (*a < 0) *a = 0; if ((*a >> 1) == 0) *a = -1; }
static void scene_events(player_t *p) {
    obj_t *st = MT.stone, *sp = MT.spade;
    if (MT.blk >= 0 && !MT.blkUp) {                                            /* 6866 / 6867: 32-frame fade-in */
        if (MT.blkA < 32) { MT.blk = MT.blk == 255 ? 248 : (2 * (MT.blk >> 1) - 8 < 0 ? 0 : 2 * (MT.blk >> 1) - 8); MT.blkA++; }
        else MT.blk = -1;
    }
    if (MT.sD == 0) {                                                          /* 6868 */
        for (int i = 0; i < G.nobjs; i++) {                                    /* the labyrinth's enemies go (the beaten Bone Golem's bones stay: INFERRED) */
            obj_t *o = &G.o[i];
            if (o->alive && (o->type == OT_SENTINEL || o->type == OT_SSPIKE || o->type == OT_FSPITTER || o->type == OT_FSSHELL || o->type == OT_IRIS)) o->alive = 0;
        }
        MT.sD = 1; MT.sC = st ? st->y : 888;
    }
    if (MT.sD == 1) MT.sE++;                                                   /* 6869 */
    if (sp && MT.sD == 1 && MT.sE == 15) { restartan(sp, AN_SPADE_JUMP); MT.spA = -8; MT.sE = 16; snd_play_orig(8, 220, 128); }   /* 6870 */
    if (sp && st && MT.sD == 1 && MT.sE > 15 && !MT.spLanded) {
        sp->y = (float)(int)(sp->y + MT.spA); MT.spA += 0.5;                  /* 6871 */
        if (G.frame % 2 == 0) sp->x = fminf(sp->x + 1, st->x - 14);           /* 6872 */
        if (MT.spA > 0 && sp->y >= st->y) {                                    /* 6873: lands on the stone */
            sp->y = st->y; setan(sp, AN_SPADE_LAND); MT.spLanded = 1; snd_play_orig(33, 220, 128);
            for (int k = 0; k < 2; k++) if (MT.ks[k]) MT.ks[k]->hidden = 0;
            MT.wht = 255; MT.whtStep = 8; snd_play_orig(77, 220, 128);
        }
    }
    if (MT.sD >= 1) { fade_down(&MT.wht, MT.whtStep); if (!MT.blkUp) fade_down(&MT.blk, 8); }   /* 6874 / 6875 (every fade) */
    for (int k = 0; k < 2; k++) if (MT.ks[k] && MT.ks[k]->alive && !MT.ks[k]->hidden) MT.ks[k]->x += k ? 8 : -8;   /* 6876 / 6877 */
    if (st && MT.sD == 1 && MT.sE >= 80 && G.frame % 3 == 0) { obj_effect((float)(st->x - 20 + R(41)), st->y + 4, "dust"); snd_play_orig(55, 220, 128); }   /* 6878 */
    if (st && MT.sD == 1 && MT.sE >= 140) {                                    /* 6879 */
        MT.sD = 2; MT.sE = 0; obj_effect(st->x, st->y, "explosion"); snd_play_orig(45, 220, 128); snd_play_orig(111, 220, 128);
        for (int k = 0; k < 2; k++) if (MT.ks[k]) { MT.ks[k]->alive = 0; MT.ks[k] = NULL; }
    }
    if (MT.sD >= 2) {                                                          /* 6880: the stone rises, Spade and the hook ride it */
        MT.sB = fmax(MT.sB - 0.1, -16); MT.sA += MT.sB;
        if (st) { st->y = (float)(int)(MT.sC + MT.sA); if (sp) sp->y = st->y; if (MT.hook) MT.hook->y = st->y - 32 + 1; }
    }
    if (MT.sD < 3) {                                                           /* 6881: player parked; camera on the shrine */
        p->x = 0; p->y = 952 + ORIG_REF_DY; p->vx = p->vy = p->gsp = 0;
        G.cam_nE = 6344; G.cam.A = shrine_x() - 30; G.cam.B = shrine_y(); G.cam.M3 = 1; G.cam_m2 = 0;
    }
    if (MT.sD == 2 && MT.sA < -100) {                                          /* 6882: run-in from the left of the view */
        MT.sD = 3; p->scripted = 0; p->x = G.camx - 50; p->y = shrine_y() + 50 + ORIG_REF_DY; p->ground = 0; p->vx = 5; p->vy = 0; p->gsp = 5; p->face = 1;
    }
    float sx = st ? st->x : 3592;
    if (MT.sD == 3 && p->x < sx - 80) { p->face = 1; if (p->ground) p->gsp = 5; else p->vx = 5; }   /* 6883: scripted run, 5 px / frame */
    if (MT.sD == 3 && p->x >= sx - 80) {                                       /* 6884: control back */
        G.input_lock = 0; MT.timer_off = 0; if (p->ground) p->gsp = 5; else p->vx = 5; p->face = 1; MT.sD = 4;
        G.cam.M3 = 0; G.cam_m2 = 1;                                            /* CAM.M = 2 */
    }
    G.cam_nC = 3200; G.cam.H = -60;                                            /* 6887 */
    if (MT.sD >= 4 && p->x >= sx - 80 && G.camx >= MT.c0 - 213) {              /* 6888: view left >= 4147 */
        MT.g_scene = 0; MT.g_intro = 1; MT.timer_off = 1;                      /* 0x22514 on, timer off, CAM.M = 1 */
    }
}

/* ===================================================================================================== 6889-6900 */
static void intro_events(player_t *p) {
    double *L = &H_('L');
    (void)p;
    if (*L <= 61 && !MT.once6890) { MT.once6890 = 1; snd_play_orig(215, 220, 128); }   /* 6890 (215 not loaded: sound RAM, the shake carries it) */
    if (*L <= 61 && MT.head->hidden && G.frame % 4 == 0) ruin_piece(MT.body->x - 100 + R(201), G.camy - 50);   /* 6891 (D: x from 4360 - 100) */
    if (*L <= 61) { B_('C') = MT.c0; G.cam.H = -60; }                         /* 6892 */
    if (*L < 60) { *L += 1; G.cam.Z = 2; G.cam_nC = MT.c0 - 213; G.cam_nE = MT.c0 + 213; }   /* 6893: one screen on x 4360 */
    if (*L == 60) { MT.g_fight = 1; MT.g_atk = 1; *L = 61; if (MT.warn) MT.warn->hidden = 0; }   /* 6894 */
    if (*L == 61) { G.cam_nC = MT.c0 - 213; G.cam_nE = MT.c0 + 213; }         /* 6895 */
    if (H_('D') > 30 && !MT.once6896) { MT.once6896 = 1; music_boss(); }       /* 6896 */
    if (*L == 61 && B_('I') == 0 && H_('D') <= 0) {                            /* 6897: the arena */
        *L = 62; G.cam_m2 = 1; G.cam.M3 = 0; G.cam_nC = fminf(MT.limL, G.camx); G.cam_nE = MT.limR;
    }
    if (*L == 62) { MT.timer_off = 0; MT.g_intro = 0; }                        /* 6898 HUD (not ported) / 6899 */
    if (MT.hit[0]->A <= 0 && MT.hit[1]->A <= 0) ach_earn(36);                /* 6900 (group 0x22514): both arm weak points A <= 0 (RE) */
}

/* ===================================================================================================== 6902-7017 */
static void walk_step(int dir) {                                              /* 6907 / 6908 */
    B_('A') += 10 * dir; B_('C') += 4 * dir;
    double A = B_('A'), s = c_sin(A / 2);
    B_('F') = fabs(10 * c_sin(A));
    AD('D') = -8 * s;
    if (dir > 0) { AD('F') = 5 * s - 16; AD('J') = 8 * s + 16; AD('H') = 10 * s - 24; AD('L') = 16 * s + 24; }
    else { AD('F') = 8 * s - 16; AD('J') = 5 * s + 16; AD('H') = 16 * s - 24; AD('L') = 10 * s + 24; }   /* the arms swap amplitudes (RE as read) */
    AD('B') = fabs(10 * c_sin(A)); AD('C') = -fabs(10 * c_sin(A));
}
static void swipe_start(int h) {                                              /* 6923 / 6931 */
    int k = h < 0 ? 0 : 1;
    B_('H') = h; B_('B') = 0; MT.bfl |= BF1; MT.bfl &= ~BF2; B_('O') += 1;
    MT.armSw[k] = 1; restartan(MT.arm[k], k ? AN_ARM6_SW : AN_ARM3_SW);
    MT.bfl &= ~BF8; B_('P') = 80;
}
static void recover_left(void) { AD('W') = fmax(AD('W') - 3, 0); AD('E') = fmax(AD('E') - 6, 0); AD('F') = fmax(AD('F') - 4, -16); AD('H') = fmax(AD('H') - 4, -24); AD('G') = fmax(AD('G') - 4, 40); AD('I') = fmax(AD('I') - 6, -40); }   /* 6930 */
static void recover_right(void) { AD('X') = fmin(AD('X') + 3, 0); AD('E') = fmax(AD('E') - 6, 0); AD('J') = fmin(AD('J') + 4, 16); AD('L') = fmin(AD('L') + 4, 24); AD('K') = fmax(AD('K') - 4, 40); AD('M') = fmax(AD('M') - 6, -40); }   /* 6938 */
static int arm_dir_clamp(double v) { int d = (int)v; return d < 5 ? 5 : d > 8 ? 8 : d; }
static void fight_events(player_t *p) {
    obj_t *hd = MT.head;
    double px = PX(p);
    int a1 = hit_alive(0), a2 = hit_alive(1);
    /* ---- walking (6902-6910) */
    if (B_('B') == 0 && B_('H') == 0 && B_('I') <= 0 && H_('D') <= 0) {
        if (AD('E') != 120) { if (px - 80 > B_('C')) B_('B') = 1; else if (px + 80 < B_('C')) B_('B') = -1; }   /* 6902 / 6903 */
        else B_('B') = B_('C') <= px ? 1 : -1;                                 /* 6904 / 6905 */
    }
    if (B_('B') != 0 && B_('I') <= 0) {                                        /* 6906: legs */
        double A = B_('A');
        AD('P') = 24 + fmin(0, 40 * c_sin(A)); AD('T') = 24 + fmin(0, 40 * c_sin(A + 180)); AD('Q') = 320 + 20 * c_cos(A); AD('U') = 40 + 20 * c_cos(A + 180);
    }
    if (B_('B') == 1 && B_('A') < 360 && B_('I') <= 0) walk_step(1);
    else if (B_('B') == -1 && B_('A') > -360 && B_('I') <= 0) walk_step(-1);
    { double A = B_('A'); if ((A == 170 || A == -170 || A == 350 || A == -350) && B_('I') <= 0) { snd_play_orig(146, 220, 128); shake_min(3); } }   /* 6909 */
    if ((B_('B') == 1 && B_('A') >= 360) || (B_('B') == -1 && B_('A') <= -360)) { B_('A') = B_('B') = B_('F') = 0; ad_rest(); }   /* 6910 */
    /* ---- jaw / bite (6911-6918) */
    if (!a1 && !a2) H_('F') = fmod(H_('F') + 1, 340);                          /* 6911 */
    if (MT.hAnim != 7) hd->fl |= F_ARMOURED; else hd->fl &= ~F_ARMOURED;      /* 6912 / 6913 */
    if (H_('F') > 50 && H_('F') < 250) { H_('F') += 1; AD('E') = fmin(AD('E') + 6, 120); MT.hAnim = 7; }   /* 6914 */
    if (MT.hAnim == 7 && (AD('E') <= 0 || a1 || a2)) MT.hAnim = 0;            /* 6915 */
    if (H_('F') >= 250 && AD('E') > 0) { AD('E') = fmax(AD('E') - 5, 0); MT.hDir = 0; H_('E') = 0; H_('F') = fmod(H_('F') + 1, 400); }   /* 6916 */
    if (AD('E') == 120) { H_('E') += 1; MT.hDir = H_('E') <= 0 ? 0 : (int)clampd((int)(H_('E') / 5), 0, 2); }   /* 6917 */
    if (H_('E') > 20) { H_('E') = -10; spawn(OT_MT_CHOMP, AN_CHOMP, hd->x, hd->y, hd->layer); snd_play_orig(152, 220, 128); }   /* 6918 */
    if (B_('P') > 0) B_('P') -= 1;                                             /* 6919 */
    /* ---- swipes (6920-6938) */
    if (B_('H') != 0 && B_('B') == 0 && B_('G') <= 360) { double g = B_('G'); AD('T') = 34 - 10 * c_sin(g + 90); B_('F') = -10 * c_sin(g - 90) - 10; }   /* 6920: crouch */
    if (B_('H') != 0 && B_('G') >= 240 && !(MT.bfl & BF8)) { MT.bfl |= BF8; snd_play_orig(151, 220, 128); }   /* 6921 */
    int can = B_('P') <= 0 && B_('H') == 0 && B_('B') == 0 && B_('I') == 0 && H_('D') <= 0 && MT.hAnim != 7 && AD('E') <= 1;
    double C = B_('C'), I = B_('I');
    if ((can && px - 56 < C && C < px && B_('O') < 2) || (B_('M') == -1 && I > 0 && I < 90 && B_('H') == 0)) swipe_start(-1);   /* 6923 */
    if (B_('H') == -1 && !a1) B_('H') = 1;                                     /* 6924 */
    if (B_('H') == -1) {
        double *g = &B_('G');
        *g += I <= 1 ? I + 5 : 6;                                              /* 6925 */
        if (*g <= 360) { AD('G') = 40 * c_cos(*g); AD('E') = -10 * c_sin(*g - 90) - 10; AD('M') = -20 * c_cos(*g) - 20; AD('W') = 22.5 * c_cos(*g) - 22.5; }   /* 6926 */
        if (*g > 180 && *g < 330) *g += 8;                                     /* 6927 */
        if (*g >= 350 && I > 0 && !(MT.bfl & BF2) && a1) { MT.bfl |= BF2; slash_new(0, 315, 28); }   /* 6928 */
        if (*g > 180 && *g <= 360) {                                           /* 6929 */
            double w = 2 * (*g - 90);
            AD('I') = 50 * c_cos(*g) + 10; AD('E') = 30 - 50 * c_sin(*g - 90); AD('W') = w > 360 ? 35 : 35 * c_cos(w);
            AD('F') = 20 * c_cos(*g) + 4; AD('H') = 30 * c_cos(*g) + 6; MT.armDir[0] = AD('F') <= 0 ? 5 : arm_dir_clamp(0.2 * AD('F') + 5);
        }
        if (*g > 360) recover_left();                                          /* 6930 */
    }
    can = B_('P') <= 0 && B_('H') == 0 && B_('B') == 0 && B_('I') == 0 && H_('D') <= 0 && MT.hAnim != 7 && AD('E') <= 1;
    if ((can && C - 56 < px && px <= C && B_('O') < 2) || (B_('M') == 1 && I > 0 && I < 90 && B_('H') == 0)) swipe_start(1);   /* 6931 */
    if (B_('H') == 1 && !a2) B_('H') = -1;                                     /* 6932 */
    if (B_('H') == 1) {
        double *g = &B_('G');
        *g += I <= 1 ? I + 5 : 6;                                              /* 6933 */
        if (*g <= 360) { AD('K') = 40 * c_cos(*g); AD('E') = -10 * c_sin(*g - 90) - 10; AD('I') = -20 * c_cos(*g) - 20; AD('X') = 22.5 - 22.5 * c_cos(*g); }   /* 6934 */
        if (*g > 180 && *g < 330) *g += 8;                                     /* 6935 */
        if (*g >= 350 && I > 0 && !(MT.bfl & BF2) && a2) { MT.bfl |= BF2; slash_new(1, 45, 20); }    /* 6936 */
        if (*g > 180 && *g <= 360) {                                           /* 6937 (the 6929 mirror; clamp 5..8 INFERRED) */
            double w = 2 * (*g - 90);
            AD('M') = 10 + 50 * c_cos(*g); AD('E') = 30 - 50 * c_sin(*g - 90); AD('X') = w > 360 ? -35 : -35 * c_cos(w);
            AD('J') = 16 - 20 * c_cos(*g) - 20; AD('L') = 24 - 30 * c_cos(*g) - 30; MT.armDir[1] = arm_dir_clamp(-0.2 * AD('J') + 5);
        }
        if (*g > 360) recover_right();                                         /* 6938 */
    }
    /* ---- ground slam (6939-6958) */
    if (B_('H') == 2 && H_('G') <= 0) B_('H') = (R(2) << 1) - 1;               /* 6939: phase 1 -> a random swipe */
    { double d = fabs(px - B_('C'));
      if (B_('H') == 0 && B_('B') == 0 && H_('D') <= 0 &&
          ((B_('P') <= 0 && B_('I') == 0 && d >= 56 && d < 80 && MT.hAnim != 7 && AD('E') <= 1) ||   /* 6940 (branch limits INFERRED within the 56 / 80 px) */
           (B_('P') <= 0 && B_('I') == 0 && B_('O') >= 2 && d < 80) || (B_('M') == 2 && B_('I') > 0 && B_('I') < 90))) {
          B_('H') = 2; B_('O') = 0; MT.bfl |= BF1; MT.bfl &= ~(BF2 | BF8); MT.armE[0] = MT.armE[1] = 0; B_('P') = 80;
      } }
    if (B_('H') == 2) {
        double *g = &B_('G'); I = B_('I');
        int still = MT.armD[0] <= 0 && MT.armD[1] <= 0;
        if (still) *g += I <= 1 ? I + 5 : 6;                                   /* 6941 */
        if (*g <= 360) { AD('G') = 40 * c_cos(*g); AD('E') = -10 * c_sin(*g - 90) - 10; AD('K') = 40 * c_cos(*g); MT.armDir[0] = MT.armDir[1] = (int)fmin(*g / 10, 8); }   /* 6942 */
        if (*g > 180 && *g < 330 && still) *g += 8;                            /* 6943 */
        if (*g > 180 && *g <= 360) { AD('I') = 50 * c_cos(*g) + 10; AD('E') = 30 - 50 * c_sin(*g - 90); AD('M') = 10 + 50 * c_cos(*g); }   /* 6944 */
        if (*g > 360) { recover_left(); recover_right(); }                     /* 6945 */
        if (I > 0 && *g >= 350 && !(MT.bfl & BF2)) {                          /* 6946 / 6947 / 6948: slashes straight down */
            if (a2) slash_new(1, 90, 24);
            if (a1) slash_new(0, 90, 24);
            MT.bfl |= BF2;
        }
        if (*g >= 240 && *g < 340)                                             /* 6949-6956: claw impacts (backdrop obstacles) */
            for (int k = 0; k < 2; k++) {
                if (MT.armD[k] != 0 || !hit_alive(k) || MT.arm[k]->hidden || !obj_overlaps_bg(MT.arm[k])) continue;
                rock_new(k, 9);
                if (MT.armE[k] < 8) {
                    rock_new(k, 7); MT.armD[k] = 5;
                    if (MT.cut[k]) { MT.cut[k]->x = MT.arm[k]->x; MT.cut[k]->y = (float)(B_('D') + 80); MT.cutA[k] = -60; }
                    MT.armE[k]++;
                    if (k == 1) *g -= 4;
                }
            }
    }
    { int d5 = MT.armD[0] == 5 || MT.armD[1] == 5; if (d5 && !MT.snd149) snd_play_orig(149, 220, 128); MT.snd149 = d5; }   /* 6957 */
    for (int k = 0; k < 2; k++) { if (MT.armD[k] > 0) MT.armD[k]--; MT.cutA[k] = fmin(MT.cutA[k] + 1, 128); }   /* 6958 */
    if (B_('G') > 480) { MT.bfl &= ~BF8; B_('G') = 0; B_('H') = 0; }          /* 6959 */
    /* ---- jumps and the combo (6960-6995) */
    I = B_('I');
    if (I > 0 && I < 485 && B_('J') != 200) B_('I') = I += 5;                  /* 6960 */
    if (H_('G') > 1 && H_('H') == 0 && (a1 || a2)) H_('H') = 103;             /* 6961 */
    if (H_('H') > 100 && H_('D') <= 0 && I == 0 && B_('G') == 0) {             /* 6962 */
        B_('J') = 100; B_('I') = I = 5; B_('A') = 0; ad_rest(); AD('B') = AD('C') = 0; MT.bfl &= ~(BF1 | BF5);
        B_('N') = fabs(B_('C') - px) * 0.013 + 5 + 3 * c_sin(90 * H_('H'));
    }
    if (G.frame % 5 == 0 && H_('H') > 0 && H_('H') <= 100) H_('H') -= 1;      /* 6964 */
    if (H_('H') > 100 && I > 0 && I <= 450 && B_('M') == 0) {                 /* 6965-6968 */
        double bx = MT.body->x;
        B_('M') = px >= bx && px <= bx + 99 ? 1 : px >= bx - 99 && px < bx ? -1 : 2;
    }
    double J = B_('J');
    if (I > 0 && I <= 450) B_('F') = c_sin(I + 45) * J - 0.7 * J;             /* 6973 */
    if (I > 0 && J != 200 && !(MT.bfl & BF1) && MT.hAnim != 7) AD('E') = 20 * c_sin(1.5 * I - 90) + 20;   /* 6974 */
    if (I >= 45 && I <= 450) {                                                 /* 6975 */
        double s = c_sin(I + 45);
        AD('P') = 65 - 60 * s; AD('O') = 4 - 60 * s; AD('T') = 65 - 60 * s; AD('S') = 60 * s - 4; AD('B') = 20 * c_sin(1.5 * I); AD('C') = -20 * c_sin(1.5 * I);
    }
    { int on = I >= 45 && I < 200; if (on && !MT.snd147) snd_play_orig(147, 220, 128); MT.snd147 = on; }   /* 6976 (147 not loaded: sound RAM) */
    if (I < 370) MT.hfl &= ~HF3;                                               /* 6977 */
    if (I >= 370 && !(MT.hfl & HF3)) {                                         /* 6978 / 6979: landing thud */
        if (J > 150) shake_min(8); else shake_min(5);
        snd_play_orig(132, 220, 128); MT.hfl |= HF3; if (MT.warn) MT.warn->hidden = 1;
    }
    if (I >= 45 && I < 405) {                                                  /* 6987: knee / foot angles */
        double s = c_sin(1.5 * I);
        AD('Q') = -20 * s; AD('R') = 20 - 20 * s; AD('U') = 20 * s; AD('V') = 20 * s - 20;
    }
    if (I >= 45 && I <= 450 && B_('H') == 0) AD('G') = AD('K') = 100 - 80 * c_sin(I + 45);   /* 6988 */
    if (I > 450 && B_('H') == 0) {                                             /* 6989 (range: INFERRED, the landing) */
        double s = c_sin(1.5 * I);
        AD('G') = 40 - 20 * s; AD('I') = -20 * s - 20; AD('K') = 40 - 20 * s; AD('M') = -20 * s - 20;
    }
    if (I > 450) { AD('Q') = 340; AD('R') = 0; AD('U') = 20; AD('V') = 0; }    /* 6990 (INFERRED: rest angles) */
    if (I >= 485 && B_('L') >= -1 && B_('L') <= 1) {                           /* 6991: landed */
        B_('I') = I = 0; AD('B') = AD('C') = 0; MT.bfl &= ~BF1; B_('M') = 0; B_('L') = 0; H_('H') = fmax(H_('H') - 1, 0);
    }
    if (I >= 75 && I < 370 && B_('B') == 0) B_('B') = B_('C') <= px ? 1 : -1;  /* 6992 / 6993 */
    if (B_('B') != 0 && I >= 75 && I < 370) B_('C') += B_('B') * B_('N');     /* 6994 */
    /* 6969-6972, 6980-6986, 6995, 7000 (BossJumpPoint at x 9776 / floor steps): never fire in this layout (RE 4.5) */
    /* ---- the high jump (6996-7017) */
    if ((hd->A <= 0 || DEFEATED(hd)) && H_('G') < 5 && AD('E') <= 2 && B_('I') < 4) {   /* 6996 */
        MT.bfl &= ~(BF1 | BF5); B_('J') = 200; B_('A') = 0; ad_rest(); AD('B') = AD('C') = 0; MT.hDir = 0; H_('E') = 0; MT.hAnim = 0; MT.horn8 = 0;
        B_('I') = 4; B_('G') = 0; B_('N') = 0; B_('M') = 0;
    }
    I = B_('I'); J = B_('J');
    if (I > 0 && I < 260 && J == 200) B_('I') = I = fmin(I + 4, 260);          /* 6997 */
    if (I == 260 && J == 200) {                                                /* 6998: hidden; the arms regrow */
        B_('K') += 1; parts_visible(0);
        for (int k = 0; k < 2; k++) { MT.hit[k]->A = 7 * H_('G') + 25; MT.hit[k]->fl &= ~0x40; MT.hit[k]->c = 0; MT.hit[k]->inv = 0; }
        if (!MT.g_intro && !(MT.bfl & (1 << 4))) {                             /* 7001 / 7002: track the player's predicted x (not in the intro: INFERRED) */
            double v = p->ground ? p->gsp : p->vx;
            MT.target = px + clampd(10 * v, -300, 300);
            if (MT.target < B_('C')) { B_('N') = 15; B_('B') = -1; } else if (B_('C') < MT.target) { B_('N') = 15; B_('B') = 1; }
        } else MT.target = B_('C');
        if (B_('K') > 0 && G.frame % 5 == 0) {                                 /* 7003: a resting debris is thrown up */
            obj_t *c[32]; int n = 0;
            for (int i = 0; i < G.nobjs && n < 32; i++) { obj_t *d = &G.o[i]; if (d->alive && d->type == OT_MT_DEBRIS && d->av[4] < 2) c[n++] = d; }
            if (n) { obj_t *d = c[n > 1 ? R(n) : (R(1), 0)]; d->av[4] = 2; d->av[3] = -15; }
        }
        if (B_('K') > 80 && (fabs(B_('C') - MT.target) < 50 || MT.g_intro)) {  /* 7004: reappear and fall */
            B_('K') = 0; parts_visible(1); B_('N') = 0;
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_MT_DEBRIS && G.o[i].av[4] < 3) G.o[i].alive = 0;
            hd->A = 10; hd->fl &= ~0x40; MT.hfl &= ~HF7; hd->c = 0; hd->inv = 0;
            B_('J') = 200; B_('I') = I = 261; H_('E') = 0; MT.hAnim = 0; hd->fl |= F_ARMOURED;
            for (int k = 0; k < 2; k++) { MT.armSw[k] = 0; MT.armDir[k] = 0; }
        }
    }
    J = B_('J');
    if (I > 260 && I < 370 && J == 200) B_('I') = I = I + 4;                   /* 7005 */
    if (I > 0 && J == 200 && !(MT.bfl & BF1) && MT.hAnim != 7) AD('E') = 40 * c_sin(1.5 * I - 90) + 40;   /* 7006 */
    if (I >= 370 && I < 400 && J == 200) B_('I') = I = I + 1;                  /* 7007 */
    if (I >= 400 && I < 485 && J == 200) { B_('I') = I = I + 2; MT.hDir = (int)clampd((int)((I - 420) / 15), 0, 2); }   /* 7008 */
    if ((MT.hfl & HF12) && H_('D') <= 0 && AD('E') < 120) {                   /* 7009: roar */
        snd_play_orig(148, 230, 128); H_('D') = 120; B_('G') = B_('H') = 0; MT.armDir[0] = MT.armDir[1] = 0; MT.hfl &= ~HF12;
    }
    if (H_('D') > 0) {                                                         /* 7010 */
        double D = H_('D') -= 1, w = 0.025 * D * c_sin(141 * D);
        MT.hDir = (int)clampd(D / 10, 0, 2); AD('B') = w; AD('C') = -w; AD('E') = 0.025 * D + w;
        if (G.cam.Z < 2) G.cam.Z = 2;
    }
    if (I >= 370 && I < 400 && J == 200 && !(MT.hfl & HF2)) {                  /* 7011-7016: 6 shockwaves at the feet */
        static const double SP[3] = { 20, 40, 60 };
        for (int k = 0; k < 6; k++) {
            obj_t *s = relic_shock_new(MT.body->x, B_('D') + 80, (k & 1) ? 16 : 0, SP[k >> 1] / 8);   /* BallMovement speed / 8 px per frame */
            if (s) { s->av[1] = 30; s->layer = 2; }                             /* A 10, B 30; y-scale 1 / 1.25 / 1.5 not drawn (uniform scale only) */
        }
        MT.hfl |= HF2 | HF12;
    }
    if (I >= 400 && J == 200) MT.hfl &= ~HF2;                                  /* 7017 */
}

/* ===================================================================================================== 7022-7038 */
static void skeleton(void) {
    obj_t *bd = MT.body, *hd = MT.head, *n4 = MT.neck[0], *n3 = MT.neck[1], *n2 = MT.neck[2], *n1 = MT.neck[3];
    double bx = (int)(B_('C') + B_('E')), by = (int)(B_('D') + B_('F'));
    bd->x = (float)bx; bd->y = (float)by;
    MT.leg2->x = MT.lknee->x; MT.leg2->y = MT.lknee->y; MT.leg2->ang = (float)AD('R');   /* the feet at the previous knee positions */
    MT.leg3->x = MT.rknee->x; MT.leg3->y = MT.rknee->y; MT.leg3->ang = (float)AD('V');
    MT.lknee->x = (float)(int)(bx - 20 + AD('O')); MT.lknee->y = (float)(int)(by + AD('P')); MT.lknee->ang = (float)AD('Q');
    MT.rknee->x = (float)(int)(bx + 20 + AD('S')); MT.rknee->y = (float)(int)(by + AD('T')); MT.rknee->ang = (float)AD('U');
    double hxp = hd->x, hyp = hd->y;
    n4->x = (float)bx; n4->y = (float)by;
    n3->x = (float)(int)((3 * bx + hxp) / 4); n3->y = (float)(int)((3 * by + hyp) / 4);
    hd->ang = (float)AD('A');
    hd->x = (float)(int)(bx + (AD('E') - 84) * c_sin(AD('D'))); hd->y = (float)(int)(by + (AD('E') - 84) * c_cos(AD('D')));
    n2->x = (float)(int)((bx + hd->x) / 2); n2->y = (float)(int)((by + hd->y) / 2);
    n1->x = (float)(int)((bx + 3. * hd->x) / 4); n1->y = (float)(int)((by + 3. * hd->y) / 4);
    n1->ang = n2->ang = n3->ang = n4->ang = (float)AD('D');
    MT.horn3->x = hd->x - 16; MT.horn3->y = hd->y - 24; MT.horn3->ang = (float)AD('B');
    MT.horn2->x = hd->x + 16; MT.horn2->y = hd->y - 24; MT.horn2->ang = (float)AD('C');
    double nx = n3->x, ny = n3->y;
    MT.arm[0]->x = (float)(int)(nx - 30 + AD('F') + AD('H')); MT.arm[0]->y = (float)(int)(ny + AD('G') + AD('I')); MT.arm[0]->ang = (float)AD('W');
    MT.arm[1]->x = (float)(int)(nx + 30 + AD('J') + AD('L')); MT.arm[1]->y = (float)(int)(ny + AD('K') + AD('M')); MT.arm[1]->ang = (float)AD('X');
    /* beads (table 3.1): shoulders, elbows, hands; hips, knees */
    double lsx = nx - 30, rsx = nx + 30, sy = ny;
    double lex = lsx + AD('F'), ley = sy + AD('G'), rex = rsx + AD('J'), rey = sy + AD('K');
    double lhx = lex + AD('H'), lhy = ley + AD('I'), rhx = rex + AD('L'), rhy = rey + AD('M');
    double lix = bx - 20, rix = bx + 20, iy = by, lkx = lix + AD('O'), lky = iy + AD('P'), rkx = rix + AD('S'), rky = iy + AD('T');
    double P[16][2] = {
        { (rex + 2 * rhx) / 3, (rey + 2 * rhy) / 3 }, { (lex + 2 * lhx) / 3, (ley + 2 * lhy) / 3 },
        { (2 * rex + rhx) / 3, (2 * rey + rhy) / 3 }, { (2 * lex + lhx) / 3, (2 * ley + lhy) / 3 },
        { rex, rey }, { lex, ley }, { (rsx + rex) / 2, (sy + rey) / 2 }, { (lsx + lex) / 2, (sy + ley) / 2 },
        { rsx, sy }, { lsx, sy }, { (rix + 3 * rkx) / 4, (iy + 3 * rky) / 4 }, { (lix + 3 * lkx) / 4, (iy + 3 * lky) / 4 },
        { (3 * rix + rkx) / 4, (3 * iy + rky) / 4 }, { (2 * lix + lkx) / 3, (2 * iy + lky) / 3 }, { rix, iy }, { lix, iy } };
    for (int a = 0; a < 16; a++) { MT.bead[a]->x = (float)(int)P[a][0]; MT.bead[a]->y = (float)(int)P[a][1]; }
    MT.hit[0]->x = MT.bead[3]->x; MT.hit[0]->y = MT.bead[3]->y;                /* Hitbox1 on bead 3, Hitbox2 on bead 2 */
    MT.hit[1]->x = MT.bead[2]->x; MT.hit[1]->y = MT.bead[2]->y;
}

/* ===================================================================================================== 7039-7054 */
static void arms_and_head(void) {
    static const int LB[5] = { 1, 3, 5, 7, 9 }, RB[5] = { 0, 2, 4, 6, 8 };
    for (int k = 0; k < 2; k++) {                                              /* 7039 / 7040: the arm flashes with its hitbox */
        int inv = MT.hit[k]->c > 0;
        const int *bb = k ? RB : LB;
        for (int j = 0; j < 5; j++) MT.bead[bb[j]]->inv = inv;
        MT.arm[k]->inv = inv;
    }
    for (int k = 0; k < 2; k++) {                                              /* 7041-7044: a broken arm falls apart */
        obj_t *hb = MT.hit[k];
        if (hb->A > 0 || !DEFEATED(hb)) continue;
        const int *bb = k ? RB : LB;
        for (int j = 0; j < 3; j++) {
            obj_t *b = MT.bead[bb[j]];
            if (b->hidden) continue;
            b->hidden = 1;
            debris_new(b->x, b->y, bb[j] <= 3 ? AN_BEAD31 : AN_BEAD1, 0, 0.5 * hb->kx, 0.5 * hb->ky, 0, 1);   /* vy source UNKNOWN (rm5_arm_break): Y */
        }
        obj_t *a = MT.arm[k];
        if (!a->hidden) {
            a->hidden = 1;
            debris_new(a->x, a->y, k ? AN_ARM6_0 : AN_ARM3_0, a->ang, 0.25 * hb->kx, 0.25 * hb->ky, 0, a->layer);   /* direction 29 / 30 (claw images) */
            MT.armDir[k] = 0; MT.armSw[k] = 0;
            if (k) AD('X') = 0; else AD('W') = 0;
        }
    }
    if (!hit_alive(0) && !hit_alive(1)) { B_('H') = 0; B_('G') = 0; }          /* 7049 */
    obj_t *hd = MT.head;
    int f6 = DEFEATED(hd) != 0, f7 = (MT.hfl & HF7) != 0;
    if (f6 && !f7 && (H_('G') == 2 || H_('G') == 3)) {                         /* 7050 / 7052: the face cracks */
        snd_play_orig(134, 220, 128);
        int right = H_('G') == 3;
        for (int k = 0; k < 4; k++) {
            static const int DL[4] = { 10, 11, 12, 13 }, DR[4] = { 3, 4, 5, 6 };
            double sp = (50 + R(30)) / 10.0, a = (right ? DR[R(4)] : DL[R(4)]) * 11.25;
            obj_t *d = debris_new(hd->x + (right ? 31 : -31), hd->y + (right ? 3 : -3), AN_FDEBRIS, 0, sp * c_cos(a), -sp * c_sin(a), 3, 2);
            if (d && d->an.b) d->an.frame = 10 - k;                             /* EffectsDebris_427 (frame 36 is not exported: a rock frame of 0-10, INFERRED) */
        }
    }
    if (f6 && !f7) { MT.hAnim = 8; MT.horn8 = 1; MT.hfl |= HF7; H_('F') = 250; H_('G') += 1; snd_play_orig(47, 220, 128); }   /* 7053 */
    { int none = !DEFEATED(hd) && !(MT.hfl & HF7); if (none && !MT.horn_once) MT.horn8 = 0; MT.horn_once = none; }   /* 7054 */
}

/* ===================================================================================================== 7060-7084 */
static void defeat_events(player_t *p) {
    obj_t *hd = MT.head;
    if (H_('G') >= 5 && H_('I') == 0) {                                        /* 7060 */
        MT.timer_off = 1; MT.kill_time = G.frame - MT.toff; music_stop(); MT.g_atk = 0; MT.body->ang = 359.9f;
        MT.arm[0]->layer = MT.arm[1]->layer = MT.horn2->layer = MT.horn3->layer = hd->layer = 1;
        H_('I') += 1; G.cam.M3 = 1; G.cam_m2 = 0;
    }
    double I = H_('I');
    if (I <= 0) return;
    if (I < 60) {                                                              /* 7061 */
        obj_effect((float)(hd->x - 40 + R(81)), (float)(hd->y - 40 + R(81)), "explosion06");
        MT.hAnim = 8; MT.horn8 = 1; H_('I') = I = I + 1;
    }
    if (I == 2) { ach_earn(42); ach_boss_finish(); }                           /* 7118 (42, any character / mode) + 7120-7128: head I == 2 (RE) */
    if (G.frame % 4 == 0 && I >= 60 && B_('D') <= MT.limB) obj_effect((float)(hd->x - 40 + R(81)), (float)(hd->y - 40 + R(81)), "explosion06");   /* 7062 */
    if (G.frame % 5 == 0 && I >= 60 && B_('D') <= MT.limB) { static const int S[3] = { 311, 98, 99 }; snd_play_orig(S[R(3)], 220, 128); }   /* 7063: "sound_explode" + rand(3) */
    /* 7064: the limbs droop (limits partly UNKNOWN: INFERRED) */
    AD('U') = fmax(AD('U') - 2, -140); AD('P') = fmax(AD('P') - 2, -40); AD('O') = fmin(AD('O') + 2, 40); AD('R') = fmax(AD('R') - 2, -80);
    AD('V') = fmax(AD('V') - 2, -160); AD('D') = fmax(AD('D') - 2, -150); AD('E') = fmax(AD('E') - 2, 0);
    AD('A') = fmax(AD('A') - 2, -170); AD('B') = fmax(AD('B') - 2, -170); AD('C') = fmax(AD('C') - 2, -170); AD('W') = fmax(AD('W') - 2, -170); AD('X') = fmin(AD('X') + 2, 170);
    if (I > 150 && I <= 200) {                                                 /* 7065-7068: the camera follows the neck */
        float tx = MT.neck[2]->x, ty = MT.neck[2]->y;
        G.cam.A = G.cam.A < tx ? fminf(G.cam.A + 4, tx) : fmaxf(G.cam.A - 4, tx);
        G.cam.B = G.cam.B < ty ? fminf(G.cam.B + 4, fminf(ty, (float)G.lvl.h - 120)) : fmaxf(G.cam.B - 4, ty);
    }
    if (I >= 60 && !MT.once7069) { MT.once7069 = 1; G.input_lock = 1; }       /* 7069: CTRL.M = 1 */
    if (I >= 60 && MT.body->y <= MT.limB + 299) B_('D') += fmin((I - 50) * 0.2, 15);   /* 7070: sinks through the floor (I >= 60: INFERRED, see 7076) */
    if (I >= 60 && B_('D') > MT.limB && !MT.once7071) {                        /* 7071: the crash */
        MT.once7071 = 1; snd_play_orig(76, 230, 128); shake_min(10); MT.wht = 255; MT.whtStep = 4; MT.bZ = 1;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_MT_DEBRIS) G.o[i].alive = 0;
    }
    fade_down(&MT.wht, MT.whtStep);                                            /* 7073 / 7074 */
    if (MT.bZ > 0) {                                                           /* 7072: white bursts along the bottom of the view */
        double Z = MT.bZ;
        obj_effect((float)(MT.body->x + 4 * Z * c_cos(37 * Z)), (float)(G.camy + 290 - fabs(4 * Z * c_sin(37 * Z))), "wburst");
        MT.bZ = (MT.bZ + 1) % 120;
    }
    if (I > 150 && G.character == CHR_CAROL_BIKE) { p->bike = 0; G.character = CHR_CAROL; player_engine_stop(); }   /* 7075: Carol leaves the bike */
    if (I >= 60) H_('I') = I = I + 1;                                          /* 7076 (INFERRED: every frame once the explosions end) */
    if (I == 200) p->x = fmax(G.camx, fmin(p->x, G.camx + SCREEN_W));         /* 7077 */
    if (I > 200) {                                                             /* 7078, 7080 / 7081 */
        G.cam.B = fmaxf(G.cam.B - 6, (float)PY(p));
        float tx = (float)PX(p); G.cam.A = G.cam.A < tx ? fminf(G.cam.A + 1, tx) : fmaxf(G.cam.A - 1, tx);
    }
    /* 7079 / 7082: the victory pose (MV.M = 50, skins direction 5) is not ported */
    if (I >= 330 && !MT.res_on) {                                              /* 7083: Sign flag 0, Sign.B = 179 -> the results */
        MT.res_on = 1; G.input_lock = 0; p->state = p->state == ST_DEAD ? p->state : ST_NORMAL;
        results_begin_boss(MT.kill_time);
    }
    G.cam.H = -60;                                                             /* 7084 */
}

/* ===================================================================================================== runtime objects */
static void runtime_pass(player_t *p) {
    int dead = p->state == ST_DEAD || p->hp <= 0;
    int first_rest = -1;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_MT_SLASH:                                                      /* 7018-7021, col_26_28 */
            if (o->born == G.frame) break;
            o->x += o->dir == 28 ? 8 : o->dir == 20 ? -8 : 0; o->y += 8;
            if (o->y > MT.limB) { o->alive = 0; break; }
            if (!dead && csens_hit(p, o) && vulnerable(p)) player_contact(p, 5);
            break;
        case OT_MT_CHOMP:                                                      /* col_7_28; destroyed at its animation's end */
            if (o->an.done) { o->alive = 0; break; }
            if (!dead && csens_hit(p, o) && vulnerable(p)) player_contact(p, 5);
            break;
        case OT_MT_DEBRIS:                                                     /* 7045-7047 (E 3: face debris, INFERRED gravity) */
            if (o->av[4] == 0 && o->av[0] == 0 && o->av[1] == 0) { o->av[0] = o->x; o->av[1] = o->y; }   /* 7045 */
            if (o->av[4] == 3 && o->av[0] == 0 && o->av[1] == 0) { o->av[0] = o->x; o->av[1] = o->y; }
            o->x = (float)(int)o->av[0]; o->y = (float)(int)o->av[1]; o->av[0] += o->av[2]; o->av[1] += o->av[3];   /* 7046 */
            if (o->av[4] != 0) o->av[3] += 0.2;                                /* 7047 */
            if (o->av[4] == 0 && first_rest < 0) first_rest = i;
            if (o->y > G.lvl.h + 64 || (o->av[4] == 3 && o->y > G.camy + SCREEN_H + 64)) o->alive = 0;
            break;
        case OT_MT_RPIECE:                                                     /* 5675 / 5676 */
            o->fx += (float)o->av[0]; o->av[1] += o->av[2]; o->fy += (float)o->av[1];
            o->x = (float)(int)o->fx; o->y = (float)(int)o->fy; o->ang += (float)o->av[0];
            if (o->fy > G.camy + 320) o->alive = 0;
            break;
        case OT_SHOCK:                                                         /* 7055 + 7112: B += 1 twice while the fight group is on */
            if (o->av[2] > 0 && o->born != G.frame) o->av[1] += MT.g_fight ? 2 : 1;
            break;
        }
    }
    if (first_rest >= 0 && obj_overlaps_bg(&G.o[first_rest]))                 /* 7048: the first falling debris hit the floor */
        for (int i = 0; i < G.nobjs; i++) { obj_t *d = &G.o[i]; if (d->alive && d->type == OT_MT_DEBRIS && d->av[4] == 0) { d->av[4] = 1; d->av[2] = d->av[3] = 0; } }
    if (MT.g_fight && !dead) {                                                 /* col_21_28 / col_12_28: the claws in their active phase */
        double H = B_('H'), Gv = B_('G');
        if (H != 1 && Gv > 180 && Gv < 360 && csens_hit(p, MT.arm[0]) && vulnerable(p)) player_contact(p, 5);
        if (H >= 1 && Gv > 180 && Gv < 360 && csens_hit(p, MT.arm[1]) && vulnerable(p)) player_contact(p, 5);
    }
}
static void sprites(void) {                                                   /* animations / directions chosen by the events */
    obj_t *hd = MT.head;
    if (MT.hAnim == 8) setan(hd, AN_H8);
    else {
        setan(hd, (MT.hAnim == 7 ? AN_H7_0 : AN_H0_0) + (MT.hDir < 0 ? 0 : MT.hDir > 2 ? 2 : MT.hDir));
        int f = H_('G') >= 3 ? (int)H_('G') - 2 : 0;                           /* 7051: the cracked face */
        if (hd->an.b) { int n = hd->an.b->anims[hd->an.anim].count; hd->an.frame = f < n ? f : n - 1; }
    }
    setan(MT.horn2, MT.horn8 ? AN_HORN2_8 : AN_HORN2); setan(MT.horn3, MT.horn8 ? AN_HORN3_8 : AN_HORN3);
    for (int k = 0; k < 2; k++) {
        obj_t *a = MT.arm[k];
        if (MT.armSw[k] && a->an.done) MT.armSw[k] = 0;                        /* the swing (anim 7) plays once, then anim 0 (INFERRED) */
        if (!MT.armSw[k]) { int d = MT.armDir[k]; setan(a, (k ? AN_ARM6_0 : AN_ARM3_0) + (d <= 2 ? 0 : d <= 6 ? 1 : 2)); }   /* only dirs 0 / 4 / 8 exist: the nearest */
    }
}

/* ===================================================================================================== results, 7085-7110 */
static void results_events(player_t *p) {
    int t = results_frames();
    /* 5390 / 6589-6592 (channel 27, once each; their exact frames INFERRED: the tally start and the skins' frames 14 / 12) */
    if (t == 1) voice_play_once(G.character == CHR_LILAC ? "lilac_woohoo" : chr_is_carol(G.character) ? "carol_feelmypower" : chr_is_milla(G.character) ? "milla_yay" : "");
    if (t == 40 && G.character == CHR_LILAC) voice_play_once("lilac_howitsdone");
    if (t == 36 && chr_is_milla(G.character)) voice_play_once("milla_superdog");
    /* 7119 / 7115 (ScoreValues D == 149): achievements 24 (RM cards 11-20) and 44 (stage time <= 8 min): results.c ach_results() */
    if (!MT.g_post && results_tally_done()) { MT.g_post = 1; G.cam_m2 = 0; }   /* 6594: group 0x22510 on (no wipe) */
    if (!MT.g_post) return;
    uint32_t held = 0;
    G.cam.Z = 2; double J = H_('J') += 1;                                      /* 7085 */
    if (!MT.once7086) { MT.once7086 = 1; snd_play_orig(214, 220, 128); }       /* 7086 (214 not loaded: sound RAM) */
    if (J >= 20 && J < 40) for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_MT_RUIN) G.o[i].alive = 0;   /* 7087 (skins direction 28: not ported) */
    if (J == 45) spawn(OT_MT_RUIN, AN_RUINS, (float)(PX(p) - 150), G.camy - 50, 2);   /* 7088 */
    if (J >= 60 && G.frame % 4 == 0) ruin_piece(G.camx + 213 - 250 + R(501), G.camy - 50);   /* 7089 */
    if (J >= 180 && G.frame % 4 == 0) ruin_piece(G.camx + 213 - 250 + R(501), G.camy - 50);  /* 7090 */
    if (J >= 40) for (int i = 0; i < G.nobjs; i++) {                           /* 7091: the falling block; 6554 breaks it on the floor */
        obj_t *r = &G.o[i]; if (!r->alive || r->type != OT_MT_RUIN) continue;
        r->y += 10; G.cam.Z = 4;
        if (obj_overlaps_bg(r)) {
            snd_play_orig(13, 220, 128); r->alive = 0;
            for (int k = 0; k < 9; k++) {                                       /* 9 BreakableRuinsPiece (spec_rm_gimmicks_b 10; offsets INFERRED) */
                obj_t *q = spawn(OT_MT_RPIECE, AN_RPIECE, r->x + (k % 3 - 1) * 20, r->y + (k / 3 - 1) * 20, 2);
                if (q) { q->av[0] = (k % 3 - 1) * 1.5 + (R(4) - 2) * 0.25; q->av[1] = -4 + R(4) * 0.5; q->av[2] = 0.3; q->fx = q->x; q->fy = q->y; q->ang = (float)(R(4) * 90); }
            }
        }
    }
    if (J == 80 && G.character == CHR_LILAC) MT.g_lilac = 1;                   /* 7093 (MV.A 0) */
    if (J == 80 && G.character == CHR_CAROL) MT.g_carol = 1;                   /* 7094 (MV.A 2); Milla: no branch (RE) */
    if (MT.g_lilac) {                                                          /* 0x22512 (7098-7102): she jumps and runs off */
        if (J >= 130 && MT.postK == 0) { held |= BTN_JUMP; MT.postK = 1; G.cam.M3 = 1; }   /* 7099: G -8, sample 8; CAM.M = 3 */
        else if (MT.postK == 1) { held |= BTN_JUMP; if (!p->ground && p->vy >= -1) MT.postK = 2; }
        if (MT.postK == 2) held |= BTN_RIGHT;                                  /* 7100: CTRL.D = 1 (the boost M5 / sample 9: INFERRED as an auto-run) */
        if (MT.postK > 0) G.cam.H = fminf(G.cam.H + 3, 0);                     /* 7101 */
    }
    if (MT.g_carol) {                                                          /* 0x2250c (7104-7110) */
        if (J >= 125 && MT.postK == 0) { held |= BTN_JUMP | BTN_RIGHT; MT.postK = 1; }   /* 7105 */
        else if (MT.postK == 1) {
            held |= BTN_RIGHT;
            if (p->ground && fabs(p->gsp) < 0.5) held |= BTN_JUMP;             /* 7107: blocked by a wall -> another jump (pounce 7106: INFERRED) */
            if (p->x > 7700) MT.postK = 2;                                     /* 7108 */
        }
        if (J >= 260) G.cam.M3 = 1;                                            /* 7110 */
    }
    results_script_input(held);
    if (J >= 260 && !MT.post_fade) { MT.post_fade = 1; MT.blk = 0; MT.blkUp = 1; }   /* 7095: LevelCardFade2 */
    if (MT.post_fade && !MT.ended) {
        MT.blk = 2 * (MT.blk >> 1) + 4 > 255 ? 255 : 2 * (MT.blk >> 1) + 4;    /* 7096 */
        if (MT.blk >= 255) {                                                   /* 7097: stop_samples, ML flags 0x2 / 0x4 cleared, next frame = 8 */
            MT.ended = 1; snd_stop_all(); voice_play_once(NULL); G.stage_flags &= ~(0x2 | 0x4); results_request_end();
        }
    }
}

/* ===================================================================================================== hooks */
/* the Adventure dialogue ended (6680: 0x22517 off, 0x22511 on with the stone's D at 14; 6885: control back without the
 * position copy). The RV state at that frame (b28_g0_l 1571): Lilac standing at (3453, 938) facing right, the stone, Spade
 * and the hook risen far out of view (stone A -3144, B -16, still rising: 6880), the camera following again (CAM.M 2) */
static void adv_return(player_t *p) {
    MT.adv = 0;
    p->x = 3453; p->y = 938 + ORIG_REF_DY; p->vx = p->vy = p->gsp = 0; p->face = 1; p->state = ST_NORMAL; p->scripted = 0;
    G.input_lock = 0; MT.timer_off = 0;
    MT.sD = 4; MT.sA = -3144; MT.sB = -16; MT.g_scene = 1;
    if (MT.stone) { MT.stone->y = (float)(int)(MT.sC + MT.sA); if (MT.spade) MT.spade->y = MT.stone->y; if (MT.hook) MT.hook->y = MT.stone->y - 31; }
    for (int k = 0; k < 2; k++) if (MT.ks[k]) { MT.ks[k]->alive = 0; MT.ks[k] = NULL; }
    G.cam.M3 = 0; G.cam_m2 = 1;
    MT.mus_fade = 180;                                                         /* 6680: fadeout_music(3000) */
}

void rm_mantalith_frame(player_t *p) {
    if (!MT.on || MT.ended) return;
    if (MT.adv) { if (story_inline_done()) adv_return(p); else return; }
    if (MT.timer_off) MT.toff++;
    if (MT.mus_fade > 0) { MT.mus_fade--; music_volume(fe_music_volume * MT.mus_fade / 180); if (!MT.mus_fade) { music_stop(); music_volume(fe_music_volume); } }   /* fadeout_music(3000), linear INFERRED */
    if (MT.g_exit) exit_events(p);
    if (MT.g_scene) scene_events(p);
    if (MT.g_intro) intro_events(p);
    if (MT.g_fight) {
        if (MT.g_atk) fight_events(p);
        skeleton();
        if (MT.g_atk) arms_and_head();
        defeat_events(p);
        sprites();
    }
    runtime_pass(p);
    if (MT.res_on) results_events(p);
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_MT")) printf("mt %d grp %d%d%d%d%d%d sD %d L %.0f bI %.0f bJ %.0f bK %.0f bC %.0f bH %.0f hG %.0f hI %.0f hJ %.0f hA %.0f h1 %.0f h2 %.0f hF %.0f adE %.0f res %d\n",
                                    G.frame, MT.g_exit, MT.g_scene, MT.g_intro, MT.g_fight, MT.g_atk, MT.g_post, MT.sD, H_('L'), B_('I'), B_('J'), B_('K'), B_('C'), B_('H'),
                                    H_('G'), H_('I'), H_('J'), MT.head->A, MT.hit[0]->A, MT.hit[1]->A, H_('F'), AD('E'), MT.res_on);
#endif
}

void rm_mantalith_draw(int front) {
    if (!MT.on) return;
    if (front) {                                                               /* LevelCardFade (layer 6 / 7): black and white */
        if (MT.blk > 0) r_fade(MT.blk / 255.f);
        if (MT.wht > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)MT.wht << 24) | 0xffffff);
        return;
    }
    if (MT.adv && story_active()) {                                            /* the Adventure dialogue: the rays and the hook follow
                                                                                * the scene's stone (the 6901 rule; the hook 31 px above it, as 6880: INFERRED) */
        int a = csx_actor_find(1589, 0);
        if (a >= 0) { MT.sA = csx_actor_y(a) - MT.sC; if (MT.hook && MT.sA < 0) MT.hook->y = csx_actor_y(a) - 31; }
        MT.sD = MT.sA < 0 ? 2 : 1;
    }
    /* 6901: the two light rays at shrine + (24, 20): y-scale 100, x-scale min(-0.06 A, 8), angles closing as the stone rises
     * (drawn as a column of x-scaled squares, additive; the second ray mirrors the first: INFERRED for its > 35 branch) */
    if (MT.ray[0] && MT.sA < -10) {
        double xs = fmin(-0.06 * MT.sA, 8), lg = 12 * log(-0.1 * MT.sA), a1 = lg > 35 ? 0 : 35 - lg;
        float ox = shrine_x() + 24, oy = shrine_y() + 20;
        if (xs >= 0.5 && ox - G.camx > -500 && ox - G.camx < SCREEN_W + 500) {
            r_set_additive(1);
            for (int r = 0; r < 2; r++) {
                obj_t *o = MT.ray[r]; if (!o->an.b) continue;
                const frame_t *f = anim_frame(&o->an);
                if (a1 == 0) {                                                 /* vertical: one stretched quad (both rays overlap) */
                    frame_draw_scaled(o->an.b, f, floorf((float)(ox - f->hx * xs) - G.camx), floorf(oy - f->hy * 100.f - G.camy), (float)(f->w * xs), f->h * 100.f, 0xffffffff);
                    continue;
                }
                double a = r ? -a1 : a1, st = 2 * xs, dx = -c_sin(a) * st, dy = -c_cos(a) * st;
                int n = (int)(400 / st) + 1; if (n > 200) n = 200;
                r_set_scale((float)xs);
                for (int k = 0; k < n; k++) frame_draw_rot(o->an.b, f, floorf((float)(ox + k * dx) - G.camx), floorf((float)(oy + k * dy) - G.camy), (float)a, 0, 0xffffffff);
                r_set_scale(1.f);
            }
            r_set_additive(0);
        }
    }
    if (MT.hook && MT.hook->an.b) {                                            /* NPCHookLine: y-scale 20 from 6868 */
        const frame_t *f = anim_frame(&MT.hook->an);
        float ys = MT.sD >= 1 ? 20.f : 1.f, x = MT.hook->x - f->hx - G.camx, y = MT.hook->y - f->hy * ys - G.camy;
        if (x > -32 && x < SCREEN_W + 32 && y < SCREEN_H && y + f->h * ys > 0) frame_draw_scaled(MT.hook->an.b, f, floorf(x), floorf(y), f->w, f->h * ys, 0xffffffff);
    }
    for (int k = 0; k < 2; k++) {                                              /* 6958: the ground cuts fade (alpha 2 (128 - A)) */
        obj_t *c = MT.cut[k];
        if (!c || !c->an.b || MT.cutA[k] >= 128) continue;
        int al = (int)clampd(2 * (128 - MT.cutA[k]), 0, 255);
        frame_draw(c->an.b, anim_frame(&c->an), floorf(c->x - G.camx), floorf(c->y - G.camy), 0, ((uint32_t)al << 24) | 0xffffff);
    }
}
