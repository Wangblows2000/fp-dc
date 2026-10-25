/* Relic Maze act 3: the Lilac / Carol Claw sequence (shrine scene, corridor destruction, caravan ride, the Claw fight,
 * the engine wreck). Source: build/re/spec_rm_claw.md (static RE of handle_frame_27_events; section numbers below refer
 * to it), with spec_rm_flow.md 1.6 / 2.3 / 3 / 4 / 5.1. Labels: RE read from code, INFERRED reasoned (marked).
 * Every rule is attached to the object types of the set (Shrine3_1298, BossClaw_1288 + Talons / Piece1 / Piece4 / Hitbox /
 * Base, CaravanEngine_1332, CaravanWheel_1308, NPCMerc_1329, PushableBlock3_1287, the corridor scenery) and runs only
 * where a level places them; character tests are the original's MV.A tests (G.character), never the level name.
 * Groups of §1 are the flags g_* (0x22463 shrine, 0x2245e caravan, 0x2245d driving, 0x22460 wreck, 0x22462 rig, 0x22465
 * intro, 0x22461 hover, 0x2245f lunge, 0x22464 shoot, 0x2245c defeat). Positions are (int) truncations of the doubles
 * (set_x / set_y), angles Fusion degrees counter-clockwise. The shared enemy rules (combat.c 2731-2767) run before this
 * block, as in the original (§3). Object pointers are cached at spawn (slots never move); runtime objects (beams, relic
 * debris) are scanned only while they exist. */
#include "../fp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFEATED(o) ((o)->fl & 0x40)
#define F_ARM 0x20
static double c_sin(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 0. || m == 180.) ? 0. : fd_sin(d / 57.29577951308232); }
static double c_cos(double d) { double m = fmod(d, 360.); if (m < 0) m += 360; return (m == 90. || m == 270.) ? 0. : fd_cos(d / 57.29577951308232); }
static int R(int n) { return cross_rand(n); }
static double MVY(const player_t *p) { return p->y - ORIG_REF_DY; }
static double getB(const player_t *p) { return p->ground ? p->gsp : p->vx; }
static void setB(player_t *p, double b) { if (p->ground) p->gsp = b; else p->vx = b; }
static void shake_max(float z) { if (G.cam.Z < z) G.cam.Z = z; }                  /* CAM.Z = max(Z, z) */

/* sprites (cached: ens_anim is a name lookup) */
enum { S_SHRINE3, S_CLAW0, S_CLAW1, S_CLAW2, S_CLAW16, S_CLAW17, S_CLAW16H, S_CLAW17H, S_TAL0, S_TAL1, S_TAL16, S_TAL17,
       S_LINK, S_LINKLIT, S_LINKH, S_BEAM, S_BEAMUP, S_ENGINE, S_ENGINEW, S_BASEW, S_DEBRIS, S_RUIN, S_CAMTRIG,
       S_MERC, S_MERC_L, S_MRUN, S_MRUN_L, S_MALARM, S_MALARM_L, S_MJUMP, S_MJUMP_L, S_MFALL, S_MFALL_L, S_HPIDLE, S_N };
static const char *const SN[S_N] = { "shrine3", "claw0", "claw1", "claw2", "claw16", "claw17", "claw16_h", "claw17_h", "talons0", "talons1",
    "talons16", "talons17", "clink", "clink_lit", "clink_h", "cbeam", "cbeam_up", "engine", "engine_w", "clawbase_w", "relicdebris", "ruinpiece",
    "camtrig", "merc", "merc_l", "merc_run", "merc_run_l", "merc_alarm", "merc_alarm_l", "merc_jump", "merc_jump_l", "merc_fall", "merc_fall_l", "hp_idle" };
static int SA[S_N];
static void set_s(obj_t *o, int s) { int a = SA[s]; if (a >= 0 && (o->an.b != &G.ens || o->an.anim != a)) anim_set(&o->an, &G.ens, a); }

#define NLINK 8
#define NMAX 40
static struct {
    int on;                                   /* the set is placed (Shrine3 + BossClaw) */
    short shrine, claw, tal, joint, hit, base, engine, block, trig, shrine2, ground[8], nground;
    short link[NLINK], nlink, merc[2], nmerc;
    short wall[4], nwall, wall2[24], nwall2, col[40], ncol, rwall[24], nrwall, wheel[20], nwheel, roof[24], nroof, spikes[4], nspikes, ul[6], nul;
    int g_shrine, g_car, g_drive, g_wreck, g_rig, g_intro, g_hover, g_lunge, g_shoot, g_defeat;
    int trigfl; double tx, ty;                /* CameraTriggerBoss flags 0x1 / 0x2 and its position */
    int MLY, mly_once, wreck_mus_once, arena_once, g4_once, milla_once;
    int g3; double acc;                       /* GV[3] frame counter (0 at 6202); the 0.4 s rumble accumulator (6212) */
    int ctrlM; uint32_t ctrl;                 /* CTRL.M and the scripted pad */
    int hold;                                 /* MV.M = 50: the cutscene hold */
    int hud;                                  /* GUIIcon.A: the HUD slid up (6206 / 6258) */
    int conv;                                 /* the road conveyor latch (BossClawTalons flag 0, 6262-6265) */
    double sD;                                /* Shrine3.D: the scene timeline */
    /* BossClaw alterables (§4) */
    double A, B, C, D, E, F, G_, H, I, J, K, L, X, Y, Z, ang, tang; int dash, dir, tdir, hurt;
    double jA, jB, jC, jD; int jdir;          /* Piece4 */
    int nbeam, ndebris;                       /* runtime objects alive (scan only then) */
    double eD; int fadeA;                     /* CaravanEngine.D, the white LevelCardFade alpha (-1: none) */
    int signx;
} C;

static obj_t *O(int i) { return i >= 0 && i < G.nobjs && G.o[i].alive ? &G.o[i] : NULL; }
static obj_t *spawn(int type, int s, float x, float y, int layer) {
    obj_t *o = obj_alloc();
    if (!o) return NULL;
    memset(o, 0, sizeof *o);
    o->type = type; o->alive = 1; o->hp = 1; o->x = o->ox = x; o->y = o->oy = y; o->layer = layer; o->born = G.frame;
    if (s >= 0) set_s(o, s);
    return o;
}
static void chunk(float x, float y, double A, double B, int layer) {     /* BreakableRuinsPiece_1203: 5675 / 5676 motion = chase_chunk_update */
    obj_t *d = spawn(OT_CHUNK, S_RUIN, x, y, layer);
    if (!d) return;
    d->fx = x; d->fy = y; d->av[0] = A; d->av[1] = B; d->av[2] = 0.3; d->ang = (float)(90 * R(4));
}
static void csens(const player_t *p, int *x0, int *y0, int *x1, int *y1) {   /* PlayerCharacterSensor_375 */
    int X = (int)floor(p->x), Y = (int)floor(MVY(p));
    *x0 = X - 7; *x1 = X + 9;
    if (player_char_sensor_small(p)) { *y0 = Y - 13; *y1 = Y + 15; } else { *y0 = Y - 34; *y1 = Y + 13; }
}
static int csens_hit(const player_t *p, const obj_t *o) { int a, b, c, d; csens(p, &a, &b, &c, &d); return o->an.b && obj_rect_hit(o, a, b, c, d); }
static int r_hurt(player_t *p, int n) {                 /* MV.R = 1, LIFE.B += n (2414 applies it next frame) */
    if (p->state == ST_DEAD || p->hurt_pending || player_invincible(p)) return 0;
    player_contact(p, n); return 1;
}
static int sens_hit(int s, const obj_t *o) {
    int ox, oy, h; const signed char *a, *b;
    return o->an.b && mv_sensor_rows(s, &ox, &oy, &a, &b, &h) && obj_rows_hit(o, ox, oy, a, b, h);
}

/* ================================================================ spawn (constructors, start events 6374-6377) */
int rm_claw_enemy(int t) { return t == OT_SHRINE3 || t == OT_CLAW_HITBOX || t == OT_CAR_ENGINE; }
int rm_claw_obj(obj_t *o, player_t *p) { (void)o; (void)p; return 1; }   /* the set's rules run in rm_claw_frame */

static void destroy_set(void) {                         /* 6196 (g4 >= 10, once): the Claw / corridor / caravan actors */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_CLAW_BASE: case OT_BGWALL: case OT_BGWALL2: case OT_COLUMN1: case OT_COLUMN2: case OT_RELICWALL: case OT_RELICDEBRIS:
        case OT_SHRINE2: case OT_SHRINE3: case OT_CLAW: case OT_CLAW_TALONS: case OT_CLAW_LINK: case OT_CLAW_JOINT: case OT_CLAW_HITBOX:
        case OT_CAR_GROUND: case OT_CAR_WHEEL: case OT_MERC: o->alive = 0; break;
        }
    }
    C.on = 0;
}

void rm_claw_spawn(void) {
    memset(&C, 0, sizeof C);
    snap_register(&C, sizeof C);
    C.shrine = C.claw = C.tal = C.joint = C.hit = C.base = C.engine = C.block = C.trig = C.shrine2 = -1; C.fadeA = -1;
    if (level_cur()->nobike && G.character == CHR_CAROL_BIKE) { G.character = CHR_CAROL; G.p.bike = 0; }   /* 6198: g1 = 2, MV.A = 2 */
    if (!objects_have_from(OT_SHRINE3)) return;
    for (int s = 0; s < S_N; s++) SA[s] = ens_anim(SN[s]);
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_SHRINE3: C.shrine = (short)i; o->A = 9999; o->bd = 0; o->fl |= F_ARM | F_NOFLY; break;   /* ctor A 9999; 6376 flag 5 (armoured) */
        case OT_CLAW: C.claw = (short)i; o->hidden = 1; break;                                           /* invisible */
        case OT_CLAW_TALONS: C.tal = (short)i; o->hidden = 1; break;
        case OT_CLAW_JOINT: C.joint = (short)i; break;                                                   /* visible */
        case OT_CLAW_LINK: if (C.nlink < 7) C.link[C.nlink++] = (short)i; o->hidden = 1; break;
        case OT_CLAW_HITBOX: C.hit = (short)i; o->A = 18; o->hidden = 1; o->bd = 0; break;              /* ctor A 18 (HP), invisible */
        case OT_CLAW_BASE: C.base = (short)i; break;
        case OT_CAR_ENGINE: C.engine = (short)i; o->A = 20; o->bd = 0; break;                            /* ctor A 20 */
        case OT_PBLOCK3: C.block = (short)i; memset(o->av, 0, sizeof o->av); o->dir = 0; o->fl = 0; break;
        case OT_CAMTRIGGER: C.trig = (short)i; set_s(o, S_CAMTRIG); o->hidden = 1; break;
        case OT_SHRINE2: C.shrine2 = (short)i; break;
        case OT_CAR_GROUND: o->hidden = 1; if (C.nground < 8) C.ground[C.nground++] = (short)i; break;   /* invisible until 6290 */
        case OT_CAR_ROOF2: case OT_CAR_ROOF3: o->hidden = 1; if (C.nroof < 24) C.roof[C.nroof++] = (short)i; break;   /* invisible Q8 floors */
        case OT_MERC: if (C.nmerc < 2) C.merc[C.nmerc++] = (short)i; o->layer = 0; o->av[0] = o->x; o->av[1] = o->y; break;   /* layer 0; 6375: A, B = position */
        case OT_BGWALL: o->layer = 1; if (C.nwall < 4) C.wall[C.nwall++] = (short)i; break;              /* layer 1 */
        case OT_BGWALL2: o->layer = 1; if (C.nwall2 < 24) C.wall2[C.nwall2++] = (short)i; break;
        case OT_COLUMN1: case OT_COLUMN2: if (C.ncol < 40) C.col[C.ncol++] = (short)i; break;
        case OT_RELICWALL: if (C.nrwall < 24) C.rwall[C.nrwall++] = (short)i; break;
        case OT_CAR_WHEEL: if (C.nwheel < 20) C.wheel[C.nwheel++] = (short)i; break;
        case OT_SPIKES_R: if (C.nspikes < 4) C.spikes[C.nspikes++] = (short)i; break;
        case OT_SPRING_UL: if (C.nul < 6) C.ul[C.nul++] = (short)i; break;
        case OT_SIGN: if (o->x > 6848 && !C.signx) C.signx = (int)o->x; break;
        case OT_ALTPATH: if (chr_is_milla(G.character)) o->alive = 0; break;           /* 6204 (MV.A = 4, once) */
        case OT_ALTPATH2: if (!chr_is_milla(G.character)) o->alive = 0; break;         /* 6203 (MV.A != 4, once) */
        }
    }
    if (C.shrine < 0 || C.claw < 0 || C.joint < 0) return;
    C.on = 1; C.g_rig = 1; C.g_drive = 1;                                         /* 0x22462 on at start; 0x2245d default-on (U3, INFERRED) */
    obj_t *cl = &G.o[C.claw], *jo = &G.o[C.joint];
    C.A = cl->x; C.B = cl->y; C.G_ = cl->x; C.H = cl->y + 80; C.I = 10; C.J = 20;   /* ctor I 10, J 20; 6377 */
    C.jA = jo->x; C.jB = jo->y;
    for (int k = 0; k < C.nlink; k++) { obj_t *l = &G.o[C.link[k]]; l->st = C.nlink - k; l->av[0] = l->av[1] = 0; }   /* I = n - rank (1..7) */
    for (int k = 0; k < C.nmerc; k++) G.o[C.merc[k]].tm = C.nmerc - 1 - k;     /* merc I by selection rank (6273, INFERRED) */
    if (C.trig >= 0) { C.tx = G.o[C.trig].x; C.ty = G.o[C.trig].y; }
    if (G.at_g4 >= 10) { destroy_set(); return; }                                /* 6196 */
    if (chr_is_milla(G.character)) G.o[C.shrine].hidden = 1;                     /* 6204: Shrine3 invisible (Milla's route) */
}

/* ================================================================ the rig (0x22462: 6299-6304, §4) */
static void claw_sprites(void) {
    obj_t *cl = O(C.claw), *t = O(C.tal);
    if (cl) {
        int s = C.dir == 1 ? S_CLAW1 : C.dir == 2 ? S_CLAW2 : C.dir == 16 ? (C.hurt ? S_CLAW16H : S_CLAW16) : C.dir == 17 ? (C.hurt ? S_CLAW17H : S_CLAW17) : S_CLAW0;
        set_s(cl, s); cl->ang = (float)C.ang;
    }
    if (t) { int d = C.tdir, s = d == 17 ? S_TAL17 : d == 16 ? S_TAL16 : d == 1 || d == 2 ? S_TAL1 : S_TAL0; set_s(t, s); t->ang = (float)C.tang; }   /* no anim 7 (U8) */
    obj_t *j = O(C.joint);
    if (j) set_s(j, C.hurt ? S_LINKH : C.jdir == 4 ? S_LINKLIT : S_LINK);
    for (int k = 0; k < C.nlink; k++) { obj_t *l = O(C.link[k]); if (l) set_s(l, C.hurt ? S_LINKH : l->dir >= 3 ? S_LINKLIT : S_LINK); }
}
static void rig(player_t *p) {
    (void)p;
    obj_t *cl = O(C.claw), *jo = O(C.joint), *t = O(C.tal), *h = O(C.hit);
    if (!cl) return;
    if (C.F == 0) C.E = fmod(C.E + 2, 360);                                      /* 6299 */
    float px = jo ? jo->x : (float)C.jA, py = jo ? jo->y : (float)C.jB;          /* 6300: last frame's positions */
    for (int k = 0; k < C.nlink; k++) {
        obj_t *l = O(C.link[k]); if (!l) continue;
        l->x = (float)(px + l->st * (cl->x - px) / 8.0); l->y = (float)(py + l->st * (cl->y - py) / 8.0);
    }
    cl->x = (float)(int)(C.A + c_sin(C.E) * C.I); cl->y = (float)(int)(C.B + c_cos(2 * C.E) * C.J);   /* 6301 */
    C.A += C.C; C.B += C.D;
    if (t) { t->x = cl->x; t->y = cl->y; C.tdir = C.dir; }
    if (jo) { jo->x = (float)(int)(C.jA + c_sin(C.E + 30) * C.I * 0.5); jo->y = (float)(int)(C.jB + c_cos(2 * (C.E + 30)) * C.J * 0.5); }
    C.jA += C.jC; C.jB += C.jD;
    for (int k = 0; k < C.nlink; k++) { obj_t *l = O(C.link[k]); if (l) { l->x = floorf(l->x + (float)l->av[0]); l->y = floorf(l->y + (float)l->av[1]); } }
    if (h && C.MLY == 1) { h->x = cl->x; h->y = cl->y; }                          /* 6302 */
    if (h && h->c > 1 && !C.hurt) { C.hurt = 1; h->c = 50; snd_play_orig(47, 220, 128); }   /* 6303: 50 frames of flash, no hits */
    if (h && h->c == 1 && !DEFEATED(h)) C.hurt = 0;                               /* 6304 */
}
static void link_bow(void) {                                                     /* 6305 / 6308 / 6337: the chain bows out */
    for (int k = 0; k < C.nlink; k++) {
        obj_t *l = O(C.link[k]); if (!l) continue;
        double v = 3.0 * l->st * l->st, s = 32 * c_sin(v);
        l->av[0] = fmin(l->av[0] + 1, s); l->av[1] = fmax(l->av[1] - 1, -s);
    }
}
static void link_dirs_charge(void) { for (int k = 0; k < C.nlink; k++) { obj_t *l = O(C.link[k]); if (l) { int d = (int)C.K - l->st; l->dir = d < 0 ? 0 : d > 3 ? 3 : d; } } }   /* 6328 */
static void link_dirs_fire(void) {                                               /* 6331 */
    for (int k = 0; k < C.nlink; k++) { obj_t *l = O(C.link[k]); if (!l) continue;
        int d = (l->st > 0 ? 2 * l->st : 0) - (int)fmin(C.K, 16); l->dir = d < 0 ? 0 : d > 3 ? 3 : d; }
}

/* ================================================================ phase A: the shrine scene and the corridor (0x22463, §5) */
static void claw_visible(int on) {
    obj_t *o;
    if ((o = O(C.claw))) o->hidden = !on;
    if ((o = O(C.tal))) o->hidden = !on;
    if ((o = O(C.joint))) o->hidden = !on;
    for (int k = 0; k < C.nlink; k++) if ((o = O(C.link[k]))) o->hidden = !on;
}
static void destroy_wall_loop(const obj_t *w) {                                  /* loop_destroy_wall_26 (6063, i 0..31) */
    for (int i = 0; i < 32; i++) {
        obj_effect(w->x + (i % 6) * 16, w->y + 12 + (i / 6) * 16, "explosion");
        chunk(w->x + (i % 6) * 16, w->y + 6 + (i / 6) * 16, (i / 4 + 1) * (2 * (i % 2) - 1), i / 2 - 9, 1);
    }
}
static void corridor(void) {                                                    /* 6246-6254, sweep = Piece4.x - 48 */
    obj_t *jo = O(C.joint); if (!jo) return;
    float sweep = jo->x - 48;
    for (int k = 0; k < C.nwall2; k++) {                                         /* 6246 */
        obj_t *w = O(C.wall2[k]); if (!w || w->x >= sweep) continue;
        shake_max(4);
        for (int n = 1; n <= 5; n++)
            for (int r = 0; r < 2; r++) chunk(w->x + 16 * n, w->y + (r ? 24 : 12), 3 + R(6), R(4) - 1, 3);
        obj_effect(w->x + 16, w->y + 16, "explosion"); obj_effect(w->x + 33, w->y + 35, "explosion");
        obj_effect(w->x + 52, w->y + 16, "explosion"); obj_effect(w->x + 80, w->y + 36, "explosion");
        w->alive = 0; snd_play_orig(38, 220, 128);
    }
    for (int k = 0; k < C.ncol; k++) {
        obj_t *c = O(C.col[k]); if (!c) continue;
        if (!(c->fl & 0x40) && c->x < sweep) {                                    /* 6247 / 6248 */
            c->fl |= 0x40;
            if (c->type == OT_COLUMN1) { c->av[0] = 8; shake_max(8); } else { c->av[0] = 7 + R(4); shake_max(4); }
        }
        if (c->fl & 0x40) {                                                       /* 6249 / 6250 */
            c->x += (float)c->av[0]; c->ang += (float)(c->type == OT_COLUMN1 ? c->av[0] : -c->av[0]);
            c->ang = fmodf(c->ang + 360.f, 360.f);
            c->av[1] += 0.2; c->y += (float)c->av[1];
            int bg = c->av[1] > 2 && obj_overlaps_bg(c);                            /* test_collisions_26: rising edge */
            if (bg && !(c->fl & 0x100)) { obj_effect(c->x, c->y, "explosion"); snd_play_orig(29, 220, 128); c->alive = 0; continue; }
            c->fl = (c->fl & ~0x100) | (bg ? 0x100 : 0);
            if (c->y > G.lvl.h + 64) c->alive = 0;
        }
    }
    for (int k = 0; k < C.nrwall; k++) {
        obj_t *w = O(C.rwall[k]); if (!w) continue;
        if (w->x < sweep && w->av[0] == 0) w->av[0] = 1;                           /* 6251 */
        while (w->av[0] > 0 && w->av[0] < 13) {                                     /* loop_ceiling_crush_26 (x13) */
            obj_t *d = spawn(OT_RELICDEBRIS, S_DEBRIS, w->x, w->y, 2);
            if (d) { d->an.frame = (int)w->av[0] - 1; d->av[0] = w->y; d->av[1] = -w->av[0] + R(10) * 0.1; C.ndebris++; }
            w->av[0] += 1;
        }
        if (w->av[0] >= 13) w->alive = 0;                                           /* 6252 */
    }
}
static void debris_frame(void) {                                                /* 6253 / 6254 (speed 0: frame = piece index) */
    if (!C.ndebris) return;
    int n = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *d = &G.o[i]; if (!d->alive || d->type != OT_RELICDEBRIS) continue;
        int fr = d->an.frame; d->y = (float)(int)d->av[0]; d->av[0] += fmax(d->av[1], 0); d->av[1] = fmin(d->av[1] + 0.2, 15); d->an.frame = fr; d->an.acc = 0;
        if (d->av[0] > G.camy + 260) { d->alive = 0; continue; }
        n++;
    }
    C.ndebris = n;
}
static obj_t *pad_under(int maxA) {                                             /* a jump pad (index 0..maxA) under S_BottomBig */
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (o->alive && o->type == OT_JUMPPAD && o->st >= 0 && o->st <= maxA && fabsf(o->x - (float)G.p.x) < 64 && sens_hit(SENS_BOTTOMBIG, o)) return o;
    }
    return NULL;
}
static obj_t *pad_A(int A) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_JUMPPAD && G.o[i].st == A) return &G.o[i]; return NULL; }

#define K_UP 1u
#define K_DOWN 2u
#define K_RIGHT 4u
#define K_JUMP 8u
#define K_ATK 16u
static void shrine_scene(player_t *p) {
    obj_t *sh = O(C.shrine), *cl = O(C.claw), *s2 = O(C.shrine2), *jo = O(C.joint);
    double X = p->x;
    if (C.I > 0) C.I = fmax(C.I - 1, 0);                                          /* 6205 */
    if (C.J > 0) C.J = fmax(C.J - 1, 0);
    if (C.hud < 80) C.hud++;                                                       /* 6206: loop_gui_shiftup_14 */
    int D = (int)C.sD;
    if (D == 0) {
        if (sh && X < sh->x - 80) { setB(p, 3); p->face = 1; }                    /* 6207: walk in */
        else { C.ctrl &= ~K_RIGHT; setB(p, 0); }                                  /* 6208 */
        if (getB(p) == 0 && !C.hold) { C.hold = 1; p->face = 1; }                 /* 6209: M50 */
        if (C.g3 > 80) { C.sD += 1; G.cam.M3 = 1; G.cam.U = 0; camera_normal_bounds(); }   /* 6210: CAM.M = 3, U = 0 */
    } else if (D < 300) C.sD += 1;                                                 /* 6211 (SKIN.H <= 0, U1) */
    D = (int)C.sD;
    if (D > 30 && D < 100) {                                                       /* 6212 / 6213: every 0.4 s */
        C.acc += 1.0 / FPS;
        if (C.acc >= 0.4) { C.acc -= 0.4; snd_play_orig(149, 220, 128); shake_max(3); }
    }
    if (D == 50) { p->face = -1; C.hold = 0; }                                     /* 6214 */
    if (D == 70) p->face = 1;                                                      /* 6215 */
    if (D == 100 && cl) {                                                          /* 6216 */
        shake_max(12); snd_play_orig(38, 220, 128); claw_visible(1);
        music_fade(0, 0); music_play("bgm/m_chase.adp");                           /* play_music(6) */
        if (C.nwall) { obj_t *w = O(C.wall[C.nwall - 1]); if (w) destroy_wall_loop(w); }   /* the wall instance: U2 (INFERRED: the last placed) */
    }
    if (D > 100) for (int k = 0; k < C.nwall; k++) { obj_t *w = O(C.wall[k]); if (w) w->alive = 0; }   /* 6217 (the shake decays in camera_update) */
    if (D >= 106 && D <= 129) C.ctrl |= K_DOWN; else if (D == 130) C.ctrl &= ~K_DOWN;   /* 6218: crouch */
    if (D == 160) { C.hold = 1; C.D = 6; C.F = 1; C.ctrl &= ~K_DOWN; p->face = 1; snd_play_orig(68, 220, 128); }   /* 6220 / 6221 */
    if (D >= 160 && D <= 209 && sh && C.B > sh->y - 30) { C.D = 0; C.B = sh->y - 30; }   /* 6222 */
    if (D == 165 && G.character == CHR_LILAC) snd_play_orig(95, 255, 128);        /* 6219: Lilac only */
    if (D == 180) C.dir = 1;                                                       /* 6223 */
    if (D >= 181 && D <= 209 && cl) {                                              /* 6224 / 6225: struggle jitter */
        if (C.g3 % 4 == 0) { C.B -= 1; cl->y -= 1; } else if (C.g3 % 4 == 2) { C.B += 1; cl->y += 1; }
    }
    if (D == 190) { C.hold = 0; C.ctrl |= K_JUMP | K_RIGHT; setB(p, 1); p->energy = 0; }   /* 6226: jump right */
    if (D >= 191 && D <= 209) { if (!p->ground && p->vy < 0) C.ctrl |= K_ATK; else C.ctrl &= ~K_ATK; }   /* 6227: air attack */
    if (D == 210) {                                                                /* 6228: knocked back (M12, B -5) */
        shake_max(8); C.ctrl = 0; player_shake(p, -5); C.D = -6; C.dir = 2;
        if (sh) sh->hidden = 1;
    }
    if (D >= 210 && sh) sh->y = 0;                                                 /* 6229 */
    if (p->state == ST_SHAKE && !p->ground) p->vy += 0.5;                          /* 6230 */
    if (D >= 210 && D <= 229 && s2 && C.B < s2->y - 120) { C.B = s2->y - 120; C.D = 0; }   /* 6231 */
    if (D >= 210 && D <= 259 && p->ground && !C.hold) { C.hold = 1; p->state = ST_NORMAL; p->face = -1; }   /* 6232 (OTHERS.Y 51 pose: U1) */
    if (D >= 230 && jo) {                                                          /* 6233 / 6234 / 6235 */
        C.jC = fmin(C.jC + 0.1, 12); C.B = fmin(C.B + 1, C.jB);
        C.ang = fmod(C.ang - 1 + 360, 360); C.tang = C.ang;
        if (C.ang > 90 && C.ang < 270) { C.ang += 1; C.tang = C.ang; }
        if (C.jC > 1.5) C.C = fmin(C.C + 0.1, 12);
    }
    if (D == 245) p->face = 1;                                                     /* 6237 */
    if (D == 252) snd_play_orig(96, 220, 128);                                     /* 6236 */
    if (D == 275 && G.character == CHR_LILAC) {                                    /* 6238: Lilac's Dragon Boost (M5) */
        C.hold = 0; p->state = ST_CHARGE; p->state_t = 0; p->boost_n = 0; p->boost_q = 0; snd_play_orig(9, 230, 128);
        p->energy = 100; C.ctrl = K_RIGHT; p->ground = 0; p->y -= 8;
    }
    if (D == 275 && G.character == CHR_CAROL) {                                    /* 6239: Carol runs */
        C.hold = 0; p->energy = 100; C.ctrl = K_RIGHT; setB(p, 6); p->cY = 2; p->state = ST_NORMAL;
    }
    if (D >= 275 && cl && G.cam.A - 40 < X) G.cam.A = (float)fmin(X + 40, cl->x - 80);   /* 6240 */
    if (D >= 280 && p->state != ST_CHARGE && p->state != ST_BOOST) p->energy = 0;  /* 6241 */
    if (D >= 230 && G.character == CHR_CAROL) {                                    /* 6242-6245: automatic pad rides 0 .. 7 */
        obj_t *pd;
        if (p->state != ST_PAD && (pd = pad_under(6)) && pd->x < p->x) pad_ride_enter(p, pd);
        else if (p->state == ST_PAD && p->padJ >= 3 && p->padC < 7 && (pd = pad_A(p->padC))) pad_ride_enter(p, pd);
        else if (p->state == ST_PAD && p->padJ >= 3 && p->padC == 7) p->padJ = 26;   /* 6244: off the last corridor pad (drops next frame) */
    }
    if (D >= 230) corridor();
    if (C.hold) setB(p, 0);
    if (X > 3760 && p->ground && G.cam.M3) {                                       /* 6255 */
        G.cam.M3 = 0; G.cam_m2 = 1; C.C = 15; C.jC = 15; C.ctrl |= K_JUMP;
    }
    if (X > 3900) { C.ctrl &= ~K_JUMP; par_set_hidden(1309, 0); }                  /* 6256 (card group: U2) */
    if (X > 3900 && p->ground) {                                                   /* 6257: the caravan ride starts */
        C.ctrlM = 0; C.ctrl = 0; C.hold = 0; C.g_shrine = 0; C.g_car = 1;
        par_set_drift(1133, -1); par_set_drift(1134, -3); par_set_drift(1137, -4.8f); par_set_drift(1138, -5); par_set_drift(1309, -8);
        G.cam_nC = 4032; C.dir = 16; C.ang = 90; C.I = C.J = 0; C.tang = C.ang;
        C.tx = 6296; C.ty = 437; if (C.trig >= 0 && O(C.trig)) { G.o[C.trig].x = 6296; G.o[C.trig].y = 437; }
        obj_t *b = O(C.base);
        if (b) { C.A = C.jA = b->x; C.B = C.jB = b->y; }
        C.C = C.jC = 0; C.F = 0;
    }
}

/* ================================================================ phase B: the caravan (0x2245e / 0x2245d, §6) */
static void caravan_drive(player_t *p) {
    double X = p->x, Y = MVY(p);
    if (C.hud > 0) C.hud = C.hud > 2 ? C.hud - 2 : 0;                             /* 6258 */
    if (C.MLY == 0 && !G.cam.U && X < 6040) G.cam_nC = 4032;                      /* 6259 */
    for (int k = 0; k < C.nwheel; k++) { obj_t *w = O(C.wheel[k]); if (w) w->ang = fmodf(w->ang - 10 + 360, 360); }   /* 6260 */
    obj_t *e = O(C.engine);
    if (e) e->fl |= F_NOFLY;
    if (G.cam_nE > 6848) G.cam_nE = 6848;                                          /* 6261 */
    if (Y > 600 && p->ground && !C.conv) { setB(p, fmin(getB(p) + 8, 7)); C.conv = 1; }   /* 6262 */
    if (C.conv) p->x -= 6 + 2 * p->ground;                                         /* 6263: the road moves -8 px/frame */
    if (Y < 600) C.conv = 0;                                                       /* 6264 */
    if (p->x < 4008) {                                                             /* 6265: thrown back up at the rear */
        p->face = 1; if (p->ground) { p->vx = p->gsp; p->ground = 0; }
        p->vy = -8; p->vx = fmax(p->vx, 8); p->state = ST_NORMAL; p->m1 = 1; C.conv = 0;
    }
    for (int i = 0; i < G.nobjs; i++) {                                           /* 6266-6268: the road's actors scroll with it */
        obj_t *o = &G.o[i]; if (!o->alive) continue;
        if (o->type == OT_HOPPER && o->y > 600 && obj_near_view(o, -40) && o->an.b == &G.ens && o->an.anim == SA[S_HPIDLE] && !o->an.done) { o->fx -= 8; o->x -= 8; }
        else if (o->type == OT_CRYSTAL && o->y > 640 && o->y < 800) o->x -= 8;
        else if (o->type == OT_PETAL && o->y > 640 && o->st == 0) o->x -= 8;
    }
    if (getB(p) > 0)                                                               /* 6269: the spikes at the front (+ the shared 3262) */
        for (int k = 0; k < C.nspikes; k++) { obj_t *s = O(C.spikes[k]); if (s && s->x > 6000 && sens_hit(SENS_LEFTBIG, s)) r_hurt(p, 20); }
    obj_t *b = O(C.block);
    if (b && e && !DEFEATED(e) && obj_obj_hit(b, e)) {                             /* 6271: the block dropped on the engine */
        int x1, y1, x2, y2; pblock_box(b, &x1, &y1, &x2, &y2);
        if (e->y < y2) e->A = 0;
    }
    if (e && DEFEATED(e) && e->dir == 0) {                                         /* 6272 / 6273 */
        obj_t *h = O(C.hit);
        if (h && h->A > 0) { h->A = 0; ach_earn(19); }                             /* RecordData[2][19]: the Claw still alive (RE 6272) */
        obj_effect(e->x - 12, e->y, "blast"); camera_shake(3);                     /* MonitorIcon frame 9 */
        C.g_wreck = 1; e->dir = 8; set_s(e, S_ENGINEW);
        if (b) b->av[5] = 0;
        music_fade(90, 0);                                                         /* fadeout_music(1500) */
        for (int k = 0; k < C.nmerc; k++) { obj_t *m = O(C.merc[k]); if (m) set_s(m, m->dir == 16 ? S_MALARM_L : S_MALARM); }
    }
}
static void mercs(void) {                                                       /* 6199-6201, 6286 / 6287: av A, B, C, D; st E; tm I */
    for (int k = 0; k < C.nmerc; k++) {
        obj_t *m = O(C.merc[k]); if (!m) continue;
        m->x = (float)(int)m->av[0]; m->y = (float)(int)m->av[1]; m->av[0] += m->av[2]; m->av[1] += m->av[3];
        if (m->st == 1) { m->av[3] = fmin(m->av[3] + 0.4, 12); if (m->av[3] >= 0) set_s(m, m->dir == 16 ? S_MFALL_L : S_MFALL); }
        if (m->st == 1 && m->av[1] < 530) m->layer = 3;                            /* 6286: layer 3 + move_front */
        if (m->st == 1 && m->av[1] >= 655) { m->av[1] = 655; m->av[3] = 0; m->st = 2; m->av[2] = -6; set_s(m, m->dir == 16 ? S_MRUN_L : S_MRUN); }   /* 6287 */
    }
}
static void wreck(player_t *p) {                                                /* 0x22460 (§8), engine D timeline */
    obj_t *e = O(C.engine);
    if (p->x < 6300) G.cam_nC = 6040;                                              /* 6278 */
    if (C.eD < 20 && C.g3 % 7 == 0) C.eD += 1;                                    /* 6279 */
    if (C.eD <= 20 && C.g3 % 7 == 0 && e) {                                        /* 6280 */
        snd_play_orig(29, 220, 128); obj_effect(e->x - 30 + R(61), e->y - 10 + R(61), "explosion06"); camera_shake(3);
    }
    int D = (int)C.eD;
    for (int k = 0; k < C.nmerc; k++) {                                            /* 6282-6285 */
        obj_t *m = O(C.merc[k]); if (!m) continue;
        if ((m->tm == 0 && D == 3) || (m->tm == 1 && D == 5)) m->dir = 16;
        if ((m->tm == 0 && D == 4) || (m->tm == 1 && D == 6)) { set_s(m, S_MJUMP_L); m->av[3] = m->tm == 0 ? -4 : -5; m->av[2] = 0; m->st = 1; }
    }
    if (D == 20) { C.eD = 21; C.fadeA = 0; }                                       /* 6281: LevelCardFade (white) */
    if ((int)C.eD == 21 && !C.wreck_mus_once) {                                    /* 6288 */
        C.wreck_mus_once = 1; snd_play_orig(76, 230, 128);
        music_fade(0, 0); music_play("bgm/m_outdoor.adp"); music_fade(180, 1);      /* play_music(7) + fadein_music(3000) */
    }
    if ((int)C.eD == 21 && C.fadeA >= 0 && C.fadeA != 255) {                       /* 6289 */
        int v = 2 * (C.fadeA >> 1) + 16; C.fadeA = ((C.fadeA >> 1) > 120 || v == 256) ? 255 : v;
    }
    if ((int)C.eD == 21 && C.fadeA == 255) {                                       /* 6290: the caravan stops */
        C.eD = 22; C.g_drive = 0;
        for (int h = 1133; h <= 1138; h++) par_set_drift(h, 0);
        par_set_drift(1112, 0);
        par_destroy(1309);
        for (int k = 0; k < C.nground; k++) { obj_t *g = O(C.ground[k]); if (g) g->hidden = 0; }
        obj_t *b = O(C.block); if (b) b->alive = 0;
        for (int k = 0; k < C.nspikes; k++) { obj_t *s = O(C.spikes[k]); if (s) s->alive = 0; }
    }
    if (C.eD >= 22 && C.eD <= 39) C.eD += 1;                                      /* 6291 */
    if ((int)C.eD == 40 && C.fadeA > 0) C.fadeA -= 2;                              /* 6292 */
    if ((int)C.eD == 40 && C.fadeA >= 0 && (C.fadeA >> 1) == 0) {                  /* 6293 */
        C.fadeA = -1; C.trigfl &= ~2; C.ty = -99; if (C.trig >= 0 && O(C.trig)) G.o[C.trig].y = -99;
        for (int k = 0; k < C.nmerc; k++) { obj_t *m = O(C.merc[k]); if (m) m->alive = 0; }
        C.eD = 41;
    }
    if (C.eD < 40 && C.g3 % 7 == 0 && e) {                                         /* 6294: dust (motion: the effect's own, approximated) */
        obj_effect(e->x, e->y, "dust");
        G.cam_nE = (float)((C.signx ? C.signx : G.lvl.w - 213) + 213);             /* CAM.E = Sign.x + CAM.X */
    }
}

/* ================================================================ phase C: the fight (§7) */
static void beam_new(float x, float y) {
    obj_t *b = spawn(OT_CLAW_BEAM, S_BEAM, x, y, 2);
    if (!b) return;
    b->dir = 20; b->av[0] = -5; b->av[1] = 5; b->av[2] = x; b->av[3] = y; C.nbeam++;
}
static void fight(player_t *p) {
    obj_t *cl = O(C.claw), *jo = O(C.joint), *h = O(C.hit);
    if (!cl) return;
    double PA = C.jA, PB = C.jB;
    int M9 = p->state == ST_DEAD;
    if (C.g_intro) {                                                               /* 0x22465: 6305-6307 */
        C.dir = 16; link_bow();
        C.B = fmax(PB - 120, C.B - 3); C.A = PA - 32 * c_sin(PB - C.B);
        C.I = fmin(C.I + 0.4, 16); C.J = fmin(C.J + 0.4, 16);
        if (C.ang > 0) { C.ang = fmax(0, C.ang - 2); C.tang = C.ang; }
        if (C.B <= PB - 120) { C.g_intro = 0; C.g_hover = 1; }
    }
    if (C.g_hover) {                                                               /* 0x22461: 6308-6317 */
        link_bow(); if (h) h->bd = 0;
        if (C.ang > 0) { C.ang = fmax(0, C.ang - 2); C.tang = C.ang; }
        if (!C.dash && !M9) C.L += 1;                                              /* 6310 */
        int go = 0;
        if (C.L == 30 && !M9) { C.F = 0; C.K = 0; go = 1; }                        /* 6311 -> shoot */
        if (C.L == 40 && !M9) { C.dash = 1; C.C = -1; C.D = 6; }                   /* 6312: dive */
        if (C.L == 50 && !M9) { C.dash = 1; C.C = -13; C.D = 2; }                  /* 6313: dash left */
        if (C.dash) C.K += 1;                                                      /* 6316 */
        if (C.dash && C.K > 10) { C.F = 0; C.K = 0; C.dash = 0; go = 1; }          /* 6317 -> shoot */
        if (C.L == 90 && !M9) { C.F = 0; snd_play_orig(74, 220, 128); C.g_hover = 0; C.g_lunge = 1; }   /* 6314 -> lunge */
        if (C.L > 110) C.L = 0;                                                    /* 6315 */
        if (go && C.g_hover) { C.g_hover = 0; C.g_shoot = 1; }
    }
    if (C.g_lunge) {                                                               /* 0x2245f: 6318-6327 */
        if (C.F < 40) { C.I = fmax(C.I - 1, 0); C.J = fmax(C.J - 1, 0); C.A = fmin(C.A + 4, PA + 32); C.F += 1; }
        if (C.F == 35) { C.G_ = fmin(fmax(floor(p->x) + 10, PA - 400), PA - 32); C.H = floor(MVY(p)) - 10; }
        if (C.F == 40) { C.F += 1; C.C = (C.G_ - C.A) * 0.07; C.D = (C.H - C.B) * 0.07; snd_play_orig(68, 220, 128); }
        if (C.F > 40 && C.F < 100) {
            for (int k = 0; k < C.nlink; k++) { obj_t *l = O(C.link[k]); if (l) { l->av[0] = fmax(l->av[0] - 2, 0); l->av[1] = fmin(l->av[1] + 2, 0); } }
            C.F += 1;
        }
        if (C.F > 45 && C.F < 100) { C.C = C.D = 0; C.dir = 17; }
        if (h) { if (C.F > 40 && C.F < 65) h->bd = 10; else if (C.F > 65) h->bd = 0; }   /* 6323 / 6324: contact window */
        if (C.F == 100) { C.F = 101; C.G_ = PA; C.H = PB - 120; C.C = (C.G_ - C.A) * 0.05; C.D = (C.H - C.B) * 0.05; C.dir = 16; }
        if (C.F > 100) { C.I = fmin(C.I + 1, 16); C.J = fmin(C.J + 1, 16); }
        if (C.F > 100 && C.A >= C.G_) { C.F = 0; C.C = C.D = 0; C.g_lunge = 0; C.g_hover = 1; }
    }
    if (C.g_shoot) {                                                               /* 0x22464: 6328-6335 */
        if (C.F == 0 && C.K < 30) {
            C.K += 1; link_dirs_charge(); C.dir = 17; C.C = -1; C.D = 1; C.I = fmax(C.I - 1, 0); C.J = fmax(C.J - 1, 0); C.tdir = 17; C.jdir = 4;
        }
        if (C.F == 0 && C.K == 30) { C.C = 4; C.D = -4; C.F = 1; C.dir = 16; C.K = 0; snd_play_orig(73, 220, 128); }
        if (C.F == 1 && C.K < 8) beam_new(cl->x - 8, cl->y + 12);                  /* 6330: the action point of the dir-16 image */
        if (C.F == 1) { link_dirs_fire(); C.jdir = 0; C.K += 1; }
        if (C.F == 1 && C.K >= 14) { C.K = 0; C.C = -2; C.D = 2; C.F = 2; }
        if (C.F == 2) { C.I = fmin(C.I + 1, 16); C.J = fmin(C.J + 1, 16); C.K += 1; }
        if (C.F == 2 && C.K >= 12) { C.F = 3; C.G_ = PA; C.H = PB - 120; C.C = (C.G_ - C.A) * 0.1; C.D = (C.H - C.B) * 0.1; C.dir = 16; }
        if (C.F == 3) { C.g_shoot = 0; C.g_hover = 1; C.A = PA; C.B = PB - 120; C.C = C.D = C.F = C.K = 0; }
    }
    if (h && DEFEATED(h)) {                                                        /* 6336 */
        if (!C.g_defeat && C.Z == 0) C.g_defeat = 1;
        C.g_intro = C.g_hover = C.g_lunge = C.g_shoot = 0;
        h->c = 0; C.F = -1; C.C = C.D = 0; C.hurt = 1; h->bd = 0;
        obj_t *t = O(C.tal); if (t) t->hidden = 1;
    }
    (void)jo;
}
static void defeat(void) {                                                      /* 0x2245c: 6337-6347 */
    obj_t *cl = O(C.claw), *jo = O(C.joint);
    if (!cl) return;
    if (C.Z < 20) {
        C.I = fmin(C.I + 3, 20); C.J = fmin(C.J + 3, 20); C.E += 12;              /* 6337 */
        C.ang = fmod(50 * c_sin(C.E) - 45 + 360, 360); C.tang = 50 * c_sin(C.E);
        cl->x = (float)(C.A + c_sin(C.E - 40) * C.I); cl->y = (float)(C.B + c_cos(C.E) * C.J);
        C.A = fmin(C.A + 10, C.jA); C.B = fmax(C.B - 10, C.jB - 120);
        link_bow();
        if (C.g3 % 6 == 0) { static const int ex[3] = { 311, 98, 99 }; snd_play_orig(ex[R(3)], 220, 128); }   /* 6338 */
        if (C.g3 % 10 == 0) {                                                       /* 6339 / 6340 */
            obj_effect(cl->x - 20 + R(41), cl->y - 20 + R(41), "explosion06"); shake_max(3); C.Z += 1;
            int vis[NLINK], nv = 0;
            for (int k = 0; k < C.nlink; k++) { obj_t *l = O(C.link[k]); if (l && !l->hidden) vis[nv++] = k; }
            if (nv) { obj_t *l = &G.o[C.link[vis[R(nv)]]]; obj_effect(l->x - 15 + R(31), l->y - 15 + R(31), "explosion06"); }   /* U5 */
        }
    }
    if (C.Z == 20) {                                                               /* 6341 / 6342 */
        snd_play_orig(38, 220, 128);
        obj_effect(cl->x, cl->y, "explosion");
        for (int k = 0; k < C.nlink; k++) { obj_t *l = O(C.link[k]); if (l) { obj_effect(l->x, l->y, "explosion"); l->av[2] = 4 - 2 * l->st; l->av[3] = 0; } }
        if (jo) { obj_effect(jo->x, jo->y, "explosion"); jo->alive = 0; }
        for (int k = 0; k < C.nul; k++) { obj_t *s = O(C.ul[k]); if (s) { obj_effect(s->x + 16, s->y + 16, "explosion02"); s->alive = 0; } }   /* every YellowSpringUpLeft (RE) */
        C.Z = 21; C.X = -16; C.Y = 0; C.g_rig = 0;
        obj_t *b = O(C.base); if (b) set_s(b, S_BASEW);
    }
    if (C.Z > 20) {                                                                /* 6343-6346 */
        cl->x += (float)C.X; cl->y += (float)C.Y; C.Y += 0.5;
        int left = 0;
        for (int k = 0; k < C.nlink; k++) {
            obj_t *l = O(C.link[k]); if (!l) continue;
            l->x += (float)l->av[2]; l->y += (float)l->av[3]; l->av[3] += 0.5;
            int hit = obj_overlaps_bg(l);
            for (int r = 0; r < C.nroof && !hit; r++) { obj_t *f = O(C.roof[r]); if (f && obj_obj_hit(l, f)) hit = 1; }
            if (hit) { obj_effect(l->x, l->y, "explosion02"); snd_play_orig(312, 220, 128); l->alive = 0; continue; }
            if (!obj_near_view(l, 50)) { l->alive = 0; continue; }
            left++;
        }
        if (!left) {                                                               /* 6347: released, the gem is back */
            level_release_camera(1); C.g_defeat = 0; C.MLY = 0;
            obj_t *sh = O(C.shrine);
            if (sh) { sh->x = 6538; sh->y = 525; sh->layer = 0; sh->hidden = 0; }
        }
    }
}
static void beams(player_t *p) {                                                /* 6348-6350, col_25_26 */
    if (!C.nbeam) return;
    int n = 0;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *b = &G.o[i]; if (!b->alive || b->type != OT_CLAW_BEAM) continue;
        b->av[2] += b->av[0]; b->av[3] += b->av[1]; b->x = (float)b->av[2]; b->y = (float)b->av[3];   /* 6348 */
        if (b->dir != 12)                                                          /* 6349: bounces on a Q8 object */
            for (int r = 0; r < C.nroof; r++) {
                obj_t *f = O(C.roof[r]);
                if (f && fabsf(f->x + 60 - b->x) < 160 && obj_obj_hit(b, f)) {
                    b->dir = 12; b->av[1] = -5; set_s(b, S_BEAMUP);
                    obj_t *w = spawn(OT_EXPLODE, -1, b->x - 10 + R(21), b->y - 10 + R(21), 3);
                    if (w) { int a = bank_anim(&G.objs, "wburst"); if (a >= 0) anim_set(&w->an, &G.objs, a); else w->alive = 0; w->scale = 0.5f; }
                    break;
                }
            }
        if (!obj_near_view(b, 50)) { b->alive = 0; continue; }                     /* 6350 */
        if (csens_hit(p, b)) r_hurt(p, 10);                                        /* col_25_26: the beam stays */
        n++;
    }
    C.nbeam = n;
}

/* ================================================================ per frame (§3 order) */
void rm_claw_frame(player_t *p) {
    if (!C.on) return;
    double X = p->x, Y = MVY(p);
    if (G.at_g4 >= 10 && !C.g4_once) { C.g4_once = 1; destroy_set(); if (G.cam_nE < G.lvl.w + 50) G.cam_nE = (float)G.lvl.w + 50; return; }   /* 6196 (+ 6297's E) */
    if (C.MLY == 1 && !C.mly_once) { C.mly_once = 1; music_fade(0, 0); music_play("bgm/m_midboss.adp"); }   /* 6197: play_music(4, 0, 1) */
    mercs();                                                                       /* 6199-6201 */
    obj_t *tr = O(C.trig), *e = O(C.engine);
    if (!chr_is_milla(G.character) && tr && !(C.trigfl & 2) && !C.g_shrine && C.sD == 0 && (!e || C.eD <= 0) && csens_hit(p, tr)) {   /* 6202 */
        C.trigfl |= 2; camera_boss_bounds(768, 224, 1087, 535);                   /* CAM.M = 1: the CameraLimitBoss objects (RE positions) */
        C.g_shrine = 1; C.ctrlM = 1; C.ctrl = 0; setB(p, 3); p->face = 1;
        music_fade(90, 0); C.g3 = 0;                                               /* fadeout_music(1500), g3 = 0 */
    }
    if (C.g_shrine) shrine_scene(p);
    if (C.g_car) {
        if (C.g_drive) caravan_drive(p);
        if (!C.arena_once && X > 6040 && X < 6456 && Y < 620 && !(C.trigfl & 1)) {   /* 6274 */
            C.arena_once = 1; camera_boss_bounds(6040, 320, 6456, 672);
        }
        obj_t *h = O(C.hit);
        if (!(C.trigfl & 1) && C.tx < X && Y < 620 && h && h->A > 0) {             /* 6275 */
            C.MLY = 1; C.trigfl |= 1; C.g_intro = 1; C.A = C.jA; C.B = C.jB; C.ang = 90; C.tang = C.ang;
            music_stop();                                                          /* stop_music(6) */
        }
        if (G.cam.U == 1 && C.arena_once && X < 6056 && getB(p) < 0) setB(p, 0);   /* 6276 */
        if (C.g_wreck) wreck(p);
    }
    if (C.g_rig) rig(p);
    fight(p);
    if (C.g_defeat) defeat();
    if (C.Z > 20 && !C.g_defeat) {                                                 /* after 6347 the Claw is never destroyed (RE); INFERRED: it keeps
                                                                                     * its throw (6343) until it leaves the view, then it is hidden */
        obj_t *cl = O(C.claw);
        if (cl && !cl->hidden) { cl->x += (float)C.X; cl->y += (float)C.Y; C.Y += 0.5; if (!obj_near_view(cl, 50)) cl->hidden = 1; }
    }
    claw_sprites();
    beams(p);
    debris_frame();
    if (C.ctrlM && C.hold) C.ctrl &= ~K_RIGHT;
    C.g3++;
#ifdef FP_HOST_ENV
    if (getenv("FP_DBG_CLAW")) {
        obj_t *cl = O(C.claw), *h = O(C.hit);
        printf("claw %d grp %d%d%d%d %d%d%d%d%d%d D %.0f eD %.0f M %d ctrl %d hold %d x %.0f y %.0f A %.1f B %.1f F %.0f L %.0f Z %.0f hp %.0f def %d camU %d m2 %d M3 %d nC %.0f nE %.0f beams %d fade %d hud %d\n",
               G.frame, C.g_shrine, C.g_car, C.g_drive, C.g_wreck, C.g_rig, C.g_intro, C.g_hover, C.g_lunge, C.g_shoot, C.g_defeat,
               C.sD, C.eD, C.MLY, C.ctrlM ? (int)C.ctrl : -1, C.hold, cl ? cl->x : 0, cl ? cl->y : 0, C.A, C.B, C.F, C.L, C.Z,
               h ? h->A : -1, h ? DEFEATED(h) != 0 : -1, G.cam.U, G.cam_m2, G.cam.M3, G.cam_nC, G.cam_nE, C.nbeam, C.fadeA, C.hud);
    }
#endif
}
int rm_claw_input(uint32_t *held) {
    if (!C.on || !C.ctrlM) return 0;
    uint32_t h = 0;
    if (C.ctrl & K_UP) h |= BTN_UP;
    if (C.ctrl & K_DOWN) h |= BTN_DOWN;
    if (C.ctrl & K_RIGHT) h |= BTN_RIGHT;
    if (C.ctrl & K_JUMP) h |= BTN_JUMP;
    if (C.ctrl & K_ATK) h |= BTN_ATTACK;
    *held = h;
    return 1;
}
float rm_claw_hud_dy(void) { return C.on ? -(float)C.hud : 0.f; }
void rm_claw_draw_front(void) { if (C.on && C.fadeA > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)C.fadeA << 24) | 0xffffff); }
