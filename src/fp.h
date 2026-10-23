/* fpdc - native Dreamcast runtime for a Freedom Planet port.
 * v2: levels, collision masks and animations recovered from the original
 * Linux build (see RESEARCH_LOG.md / PROJECT_AUDIT.md). Engine code is
 * original; game data is converted from the user's copy and never committed. */
#ifndef FP_H
#define FP_H

#include <stdint.h>
#include <stddef.h>

/* D-012: the original renders 426x240 (global values 16/17 = 426, 240). Default view = the same
 * 426x240, scaled 1.5x and letterboxed to 639x360 on the 640x480 display. FP_VIEW_320 keeps the
 * old 320x240 crop at 2x. */
#ifdef FP_VIEW_320
#define SCREEN_W 320
#define SCREEN_H 240
#else
#define SCREEN_W 426
#define SCREEN_H 240
#endif
#define FPS 60

/* ---------------------------------------------------------------- platform */
enum {
    BTN_LEFT = 1 << 0, BTN_RIGHT = 1 << 1, BTN_UP = 1 << 2, BTN_DOWN = 1 << 3,
    BTN_JUMP = 1 << 4,   /* DC A  = original "A" (jump)            */
    BTN_ATTACK = 1 << 5, /* DC X  = original "B" (attack)          */
    BTN_SPECIAL = 1 << 6,/* DC B  = original "C" (Dragon Boost)    */
    BTN_START = 1 << 7,
    BTN_GUARD = 1 << 8,  /* DC Y  (reserved) */
    BTN_LT = 1 << 9, BTN_RT = 1 << 10   /* DC triggers (title-screen developer level select only) */
};

typedef struct { uint32_t held, pressed, released; } input_t;

void   plat_init(void);
void   plat_poll_input(input_t *in);
void  *plat_load_file(const char *name, size_t *size); /* malloc'd */
void   plat_log(const char *fmt, ...);
uint64_t plat_time_us(void);

void   snd_init_fp(void);
int    snd_load(const char *name);
void   snd_play(int h, int vol, int pan);
int    music_play(const char *name);
void   music_update(void);
void   music_stop(void);
void   music_volume(int pct);
void   music_pause(void);                          /* pause the stream in place (pause menu) */
void   music_resume(void);
void   snd_stop_all(void);
/* VMU / memory card (platform): LCD bitmap 48x32 1bpp (192 bytes, first byte top-left, MSB left);
 * saves: one small file on the first memory card that has it / has room. Return 0 / bytes read, <0 error */
int    plat_vmu_lcd(const uint8_t bitmap[192]);
int    plat_save_write(const void *data, size_t size);
int    plat_save_read(void *data, size_t max);
/* src/game/save.c: options, 8 files, Time Attack records (memory card) */
#define SAVE_FILES 8
#define SAVE_STAGES 16
#define SAVE_CHARS 4
#define SAVE_CARD_ACTS 4
typedef struct { uint8_t used, character, difficulty, mode, stage, lives, petals; uint16_t crystals, deaths; uint32_t time; } save_file_t;
typedef struct { uint32_t time; uint16_t crystals; } save_rec_t;   /* time in frames, 0 = no record */
typedef struct {
    uint8_t difficulty, music_volume, sfx_volume, sensitivity, unlocked;
    save_file_t file[SAVE_FILES];
    save_rec_t rec[SAVE_STAGES][SAVE_CHARS];
    /* save v2 (port additions; a v1 file loads with these at their defaults) */
    uint8_t letterbox;                 /* 1: 16:9 picture letterboxed on the 4:3 frame; 0: 426x240 fills the 4:3 frame */
    uint16_t card_bits[SAVE_CARD_ACTS];/* in-stage cards collected, one bit per card placement, DV1..DV4 */
} save_t;
int    save_card_total(void);
/* src/game/snap.c: state snapshots for the host route search */
void   snap_register(void *p, size_t n);
size_t snap_size(void);
void   snap_save(void *buf);
void   snap_load(const void *buf);
extern save_t SV;
void   save_defaults(void);
int    save_load(void);
int    save_store(void);
size_t save_payload_size(void);
/* src/game/vmu.c */
void   vmuscr_init(void);
void   vmuscr_crystal(void);
void   vmuscr_life(int character, int lives);
const uint8_t *vmuscr_current(void);                         /* stop every sound effect channel */
/* loading screen (src/game/loading.c): silent, spinning crystal bottom-right */
void   loading_begin(const char *label);   /* label: load profiler (loadprof.c) */
void   loading_tick(void);
void   loading_end(void);
/* load profiler (src/game/loadprof.c) */
typedef struct { int on; uint64_t open_us, read_us, upload_us, snd_us, music_us, obj_us, draw_us, decode_us;
                 uint64_t bytes, tex_bytes, alloc_bytes, skip_bytes; uint32_t nfiles, ntex, nsnd, ndraw, nalloc, nskip, npacked; } loadprof_t;
extern loadprof_t LP;
void     lp_begin(const char *label);
uint32_t lp_end(void);           /* logs one line, returns total ms */
/* disc packs (src/resources/pack.c) */
void *plat_read_raw(const char *name, size_t *size);   /* one loose file, whole (aligned on the DC) */
void *plat_alloc_aligned(size_t n);                    /* free()-able */
void *pack_load(const char *name, size_t *size, int *found);
void  pack_release(void);
int   pack_entry_crc(const char *name, uint32_t *crc);
int   lz4_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len);
int    loading_active(void);

/* ---------------------------------------------------------------- renderer */
typedef struct { int id, w, h, srcw, srch; } tex_t;

int    r_init(void);
int    r_tex_load(tex_t *t, const void *fptx, size_t size);
int    r_pal_load(const void *fppl, size_t size);
void   r_begin(void);
void   r_sprite(const tex_t *t, int u, int v, int uw, int vh,
                float x, float y, float w, float h, int flip, uint32_t argb);
void   r_rect(float x, float y, float w, float h, uint32_t argb);
void   r_sprite_rot(const tex_t *t, int u, int v, int uw, int vh, float px, float py, float hx, float hy,
                    float angle_deg, int flip, uint32_t argb);   /* flip: mirrored image, hotspot uw - 1 - hx */
void   r_end(void);
void   r_tex_free(tex_t *t);           /* free one texture */
void   r_reset(void);                 /* free all textures (level change) */
void   r_set_letterbox(int on);       /* presentation only: 1 = 16:9 letterboxed (x1.5), 0 = 4:3 fill (x1.5, x2) */

/* ---------------------------------------------------------------- resources */
typedef struct { uint16_t u, v, w, h; int16_t hx, hy; } frame_t;
typedef struct { char name[16]; uint16_t first, count; uint8_t speed, back_to; int16_t loop; } anim_t;

typedef struct {
    tex_t tex;
    int nframes, nanims;
    frame_t *frames;
    anim_t *anims;
    uint16_t *seq;
    int16_t *act;                      /* optional action points (ax, ay per frame, relative to the image) */
    uint8_t *mbits; uint32_t *moff;   /* collision masks (alpha != 0), rows of (w + 7) / 8 bytes; NULL if absent */
} bank_t;
int frame_mask_px(const bank_t *b, const frame_t *f, int x, int y);   /* 1 = solid pixel of the frame image */

/* Fusion-style animation player: each tick acc += speed; a frame advances per 100.
 * loop -1 = forever; loop N = play N times then hold; wrap goes to back_to. */
typedef struct { const bank_t *b; int anim, frame, acc, loops, done; } animator_t;

int  bank_load(bank_t *b, const char *name);
int  bank_anim(const bank_t *b, const char *name);       /* -1 if missing */
void anim_set(animator_t *a, const bank_t *b, int anim);  /* restarts only if changed */
void anim_restart(animator_t *a);
void anim_tick(animator_t *a, int speed_override);       /* <0: use table speed */
const frame_t *anim_frame(const animator_t *a);
void frame_draw(const bank_t *b, const frame_t *f, float x, float y, int flip, uint32_t argb);
void frame_draw_rot(const bank_t *b, const frame_t *f, float x, float y, float angle_deg, int flip, uint32_t argb);   /* flip: mirrored image (hotspot w - 1 - hx) */
int  tex_load_file(tex_t *t, const char *name);
int  pal_load_file(const char *name);

/* ---------------------------------------------------------------- level (FPL2) */
enum { COL_NONE, COL_SOLID, COL_PLATFORM, COL_LADDER };
typedef struct { int32_t x, y; uint16_t u, v, w, h; uint8_t atlas, layer, col, pad; } piece_t;
typedef struct { uint16_t type, anim; int32_t x, y; } lvobj_t;

/* engine object types (tools/import_frame.py ENGINE_TYPES) */
enum { OT_START = 1, OT_CRYSTAL = 2, OT_DRAGONFLY = 3, OT_MON_INVINC = 4, OT_MON_RINGS = 18, OT_MON_1UP = 19, OT_PETAL = 6, OT_SPIKES = 7,
       OT_SPRING = 8, OT_SIGN = 9, OT_TURRETUS = 10, OT_ROCK = 11, OT_MONITOR = 12,
       OT_SPRING_UL = 13, OT_SPRING_UR = 14, OT_SPRING_R = 15, OT_SPRING_L = 16, OT_HANG = 17,
       OT_PARALLAX = 20, OT_LADDERTOP = 30, OT_GANSEKI = 31,
       OT_PLAT_VALLEY = 32, OT_PLAT_MOVE2 = 33, OT_PLAT_MOVE3 = 34, OT_BRIDGE = 35, OT_LOG = 36, OT_EXPLODE = 100, OT_PELLET = 101, OT_ITEM_FLY = 102 };

#define MAX_ATLAS 4
typedef struct {
    int w, h;                 /* px */
    int npieces, nobj, natlas, nuniq, tw, th;
    piece_t *pieces;
    lvobj_t *objs;
    const uint16_t *tiles;    /* nuniq * 16 rows */
    const uint16_t *smap, *pmap;
    void *blob;
    tex_t atlas[MAX_ATLAS];
} level_t;

int  level_load(level_t *l, const char *name);
int  px_solid(const level_t *l, int x, int y);
int  px_platform(const level_t *l, int x, int y);
/* first exposed surface in column x within (y0, y0+maxdist]; platforms only if oneway */
int  floor_probe(const level_t *l, float x, float y0, float maxdist, int oneway, float *sy, float *angle);
int  solid_at(const level_t *l, float x, float y);
int  px_span_solid(const level_t *l, int x0, int x1, int y);   /* any solid pixel in row y, x0..x1 */

/* Lilac's movement core (src/physics/sensors.c, RE events 1584-1611): the original's movement object */
typedef struct { double X, Y, B, G, N, O, W; int L, P; } mv_t;
enum { MV_LANDED = 1, MV_LEFT_GROUND = 2, MV_CEILING = 4 };
enum { OBJGRP_SOLID, OBJGRP_OBSTACLE, OBJGRP_NOEDGE, OBJGRP_OBSTACLE_ALL };   /* QualifierLists +0x218c0, +0x218d8 (minus flag-0 platforms), +0x218f0; +0x218d8 with every instance */
void mv_reset(void);
int  mv_core(mv_t *m);
void mv_allsensors(mv_t *m);

int  mv_sensor_left_big(void), mv_sensor_right_big(void), mv_sensor_top_big(void);
/* objects of a collision group overlapping a sensor mask (row spans x0[j]..x1[j] at (ox, oy + j)) */
int  objects_sensor_hit(int grp, int ox, int oy, const signed char *x0, const signed char *x1, int h);
int  objects_sensor_hit_type(int type, int ox, int oy, const signed char *x0, const signed char *x1, int h);  /* index of the first hit object, -1 */
int  level_test_ladder(int x, int y);
int  tex_load_file_skip_empty(tex_t *t, const char *name);        /* RE Frame::test_ladder: point inside a ladder backdrop */
int  mv_z(int sensor);                         /* last test result (Z) of a sensor */
int  mv_sensor_obj(double X, double Y, double N, int sensor, int objtype);   /* sensor placed at (X, Y, N) overlaps an object of that type: index or -1 */
enum { SENSOR_TOPBIG = 6, SENSOR_BOTTOMBIG = 8 };

/* ---------------------------------------------------------------- game */
typedef struct {
    double x, y, vx, vy, gsp;   /* doubles like the original (Clickteam alterables; event 1574 floors to 4 decimals);
                                 * x = movement X, y = movement Y + ORIG_REF_DY */
    double N, O;                /* raw / smoothed surface angle in DEGREES (original alterables N, O) */
    int ground, face;
    int state, state_t;
    animator_t an;
    int hp, invuln, crystals, lives;   /* hp in half-petals; crystals count DOWN from 200 (RV HUD) */
    int invinc;                    /* monitor invincibility (PlayerOthers D = 1), with p->invuln as its timer */
    int jumping, cyclone_used, boost_used, cyclone_ground_start, hurt_land_t;
    float energy, energy_drain;  /* RV energy meter 0..100 (boost needs 100) */
    int energy_init, energy_hold;
    int springing, skidding, ctrl_lock;
    int look, look_prev;           /* 2 = looking up (M2), 3 = crouching (M3) this / last frame */
    uint32_t held;                 /* this frame's held buttons (object events that test input) */
    double hang_x, hang_y;         /* grabbed hang sensor position (state M41) */                 /* launched by a spring, until landing (spring anim while rising) */
    float rot;                   /* sprite angle, degrees CCW (original Active angle @+0xc4) */
    double boost_vx, boost_vy;    /* (unused since the RE boost; kept for the save layout of player_t) */
    int boost_n, boost_q, m5_bounced;
    int lad_y, lad_o, lad_lt, lad_first;
    int hitstop;                  /* RE PlayerOthers.K */
    double mom_vx;                /* port change: horizontal speed when the down-kick wind-up started */
    int hurt_pending;             /* RE movement R = 1 + GUILifeBaseMeter.B: contact damage applied by 2414 the next frame */   /* RE ladder: OTHERS.Y selector (27 climb, 29 down, 28 climb-over, 47 catch), SKIN.O counter, LadderTop, first frame after a grab */         /* RE: OTHERS.N charge/boost timer (fires at 30), OTHERS.Q latched vertical direction (-1 up, 0, +1 down) */
    float spawn_x, spawn_y;
    int dead_t;
} player_t;

typedef struct {
    int type, alive, hp;
    int st, tm, dir, born;       /* recovered enemy state (Turretus D, E; direction); frame created */
    float x, y, ox, oy, vx, vy, fx, fy;
    int t, flip, par;
    int c, fl;                   /* recovered enemy hit timer C (5 after a hit) and alterable flags (bit 5 armoured, bit 6 defeated) */
    animator_t an;
} obj_t;

#define MAX_OBJS 320

typedef struct {
    level_t lvl;
    bank_t lilac, hair, objs;    /* hair: Lilac's hair overlay (PlayerSkinSashHair_424) */
    tex_t par[12]; int npar;
    player_t p;
    obj_t o[MAX_OBJS];
    int nobjs;
    float camx, camy;
    /* MasterCamera_408 alterables (RE, loop_camera_mode0_13 + events 2873-2886, 3539) */
    struct { float A, B, W, G, H, S, T, N, O; int V, M4; } cam;   /* M4: CAM.M = 4 while entering a ladder from the top (not modelled further) */
    int frame, clear_t, paused, kills;
    int act_in, act_fade, act_carry;
    int mus_resume_t;           /* frames until the stage music resumes after a jingle */   /* act change (RE 4135/5045, 3310/3311/3578, 4526/5047) */
    char lvdir[8];              /* current level data dir under the data root ("dv1") */
    int debug_select, in_select, sel, sel_top, sel_msg_t;
    int mode, card_t, pause_sel, time_attack, res_t, res_time;
    int pause_a, pause_wipe_f, frame_draw_t; float pause_wipe_x;   /* pause menu (GUIPause.A, wipe) */
    int act_lives, act_crystals;  /* RE 4311/4312: lives / crystals at act start (restart restores them) */
    int character, file_slot, file_mode, deaths;   /* file being played (-1 none), 0 Adventure / 1 Classic */
    int character_pad;              /* 0 Lilac, 1 Carol, 2 Carol (bike), 3 Milla (life icon order); only Lilac is playable */
    struct { int jump, crystal, hit, explode, attack, hurt, boost; } sfx;
    uint32_t prof_update_us, prof_draw_us;
    int orig_moves;                 /* 1: down-kick / uppercut exactly as the original (comparisons); 0: port's momentum change */
} game_t;

extern game_t G;

enum { MODE_FRONTEND, MODE_PLAY, MODE_RESULTS, MODE_SELECT };
int  game_init(void);
void game_unload_all(void);
void game_start_adventure(const char *dir, int time_attack);
void game_dev_select(void);
int  fe_enter(void);
int  fe_enter_from_stage(int time_attack);
/* hub (frame index 3: title menu / file select / mode select / options), src/game/fe_hub.c */
enum { HUB_EXIT_TIME_ATTACK = 1, HUB_EXIT_QUIT, HUB_EXIT_NEW_ADVENTURE, HUB_EXIT_NEW_CLASSIC, HUB_EXIT_CONTINUE };
struct ui_scene_s;
void hub_bind(struct ui_scene_s *s);
void hub_enter(void);
int  hub_update(const input_t *in);
void hub_draw(void);
int  hub_selected_file(void);
int  hub_cursor(void);
int  hub_on_title(void);
int  hub_lives_for_difficulty(int d);
void fe_trace_info(int *idx, int *cur);   /* pause QUIT: Time Attack -> stage select, otherwise the title */
void fe_update(const input_t *in);
void fe_draw(void);
void hud_reset(void);
void snd_play_orig(int id, int vol, int pan);
void voice_play(const char *name);
void plat_quit(void);
void r_fade(float alpha);      /* full-screen black overlay with alpha (translucent list) */
int  game_load_level(const char *dir);   /* unloads the current level first */
void game_update(const input_t *in);
void game_draw(void);
void player_reset(player_t *p, double x, double y);
void player_update(player_t *p, const input_t *in);
void player_draw(const player_t *p);
int  player_attack_box(const player_t *p, float *x0, float *y0, float *x1, float *y1);
int  player_invincible(const player_t *p);
int  player_contact_safe(const player_t *p);   /* RE 2731: movement Z = 1 (boost...), attacking states, R set, E > 0 */
int  player_attack_kind(const player_t *p);    /* 1 ground attack (insta-shield), 2 body hit (Z = 1), 0 none */
void player_hitstop(player_t *p, int k);
void player_contact(player_t *p, int damage);  /* RE 2731: R = 1, damage queued for the next frame's 2414 */       /* RE PlayerOthers.K: Lilac frozen K - 1 frames (1654/1655) */
int  cross_rand(int n);                        /* RE cross_seed LCG: ((seed >> 16) & 0x7fff) * n >> 15 */
void objects_platforms(player_t *p);
void objects_plat_pretick(int n);         /* RE 3270-3307, 4061, 4737/4738: platforms and bridges, before Lilac's movement */
void player_hurt(player_t *p, double from_x);
void player_damage(player_t *p, int amount);
void player_sync_sensors(player_t *p);
void player_hang(player_t *p, double hx, double hy);
int  player_can_hang(const player_t *p);   /* RE event 2414: amount in GUILifeBaseMeter units (5 = 1 half-petal on Normal) */
void player_bounce(player_t *p, double vy);
void player_spring(player_t *p, double vx, double vy, double top);
void player_spring_side(player_t *p, double gsp);
void objects_spawn(void);
void objects_update(void);
void objects_draw(int front);
void camera_update(void);
void camera_reset(void);
void objects_pretick(int n);
void hud_draw(void);
void text_draw(float x, float y, const char *s, uint32_t argb, int scale);

/* Physics tunables, px/frame at 60 Hz.
 * RV = runtime-verified against the original (PlayerMovementValues_253 alterables,
 * traces in build/orig_traces/, see RUNTIME_MAP.md). Others are still UNVERIFIED. */
#define PHYS_ACC        0.07375   /* RV: alt E. +E/frame while holding a direction */
#define PHYS_ACC_LOWMUL 9.0       /* RV: x9 (0.66375) while |gsp| < PHYS_LOWSPD (start burst) */
#define PHYS_LOWSPD     2.0       /* RV: low-speed threshold for the x9 accel/friction */
#define PHYS_FRIC_MUL   5.0       /* RV: release friction 5*E (0.36875) at |gsp| >= 2, 9*E below */
#define PHYS_DEC        0.5       /* RV: skid (input against motion) -0.5/frame */
#define PHYS_TOP        5.0       /* RV: alt C. Above it, holding keeps speed (no friction) */
#define PHYS_MAXSPD     15.0      /* INF: alt D = 15 */
#define PHYS_AIR_ACC    0.1475    /* RV: 2*E per frame in the air */
#define PHYS_AIR_DRAG   0.96875   /* RV: vx *= 0.96875 each air frame while -4 < vy < 0 (after gravity) */
#define PHYS_DRAG_VY    4.0       /* RV: drag window upper bound on rising speed */
#define PHYS_GRAV       0.25      /* RV: alt I */
#define PHYS_JUMP       7.0       /* RV: jump sets vy = -7 (apex 102 px from flat ground) */
#define PHYS_JUMP_CUT   3.0       /* RV: releasing jump clamps rising speed to -3 */
#define PHYS_SLOPE      0.203125  /* RV: alt F. gsp -= F*sin(angle) ... */
#define PHYS_SLOPE_MIN  0.5       /* RV: ... only when |sin(angle)| > 0.5 (steeper than ~30 deg) */
#define PHYS_LAND_J     0.84      /* RE: movement J, landing speed transfer (events 1591, 1597) */
#define PHYS_ANGLE_D    15.0      /* RE: movement D, angle smoothing divisor (event 1606) */
#define PHYS_MAXFALL    15.0      /* RE: movement H = 15; event 1577 clamps G to H before gravity -> 15.25 */
#define PHYS_BOOST      12.0      /* RV: Dragon Boost speed (B = 12 held constant) */
#define PHYS_BOOST_T    33         /* RV: boost lasts 33 frames, then speed carries with normal friction */
#define PHYS_CHARGE_T   30         /* RV: holding special charges 30 f (speed frozen), then auto-fires */
#define PHYS_ATTACK_T   22         /* RV: ground (anim 26) / air (anim 25) attack length, no physics change */
#define PHYS_UPPER_WIND 10         /* RV: uppercut stands 10 f before launching */
#define PHYS_UPPER_VX   2.0       /* RV: uppercut launch vx = 2*facing */
#define PHYS_UPPER_VY   7.0       /* RV: uppercut launch vy = -7 (also after a jump = high uppercut) */
#define PHYS_UPPER_T    39         /* RV: uppercut state length (~39 f, +-1) */
#define PHYS_DK_WIND_T  15         /* RV: down-kick windup (anim 7) */
#define PHYS_DK_WIND_GRAV 0.125   /* RV: gravity during the windup */
#define PHYS_DK_VX      5.0       /* RV: kick vx = 5*facing */
#define PHYS_CYCLONE_VY 3.0       /* RV: Dragon Cyclone sets vy = -3 */
#define PHYS_CYCLONE_GRAV_SUB 0.15 /* RE event 2041: taken off G after the normal gravity while in the cyclone without Down */
#define PHYS_CYCLONE_T  79         /* RV: cyclone state lasts 79 frames (4 traces, ground and air); landing keeps it */
#define PHYS_GCYC_JUMP_MUL 0.7    /* RE event 2045: grounded-cyclone jump scales the perpendicular part of the jump by 0.7 */
#define MOM_UPPER_KEEP  0.9       /* port change: uppercut keeps 90% of a faster run speed at launch */
#define MOM_UPPER_SLIDE 0.97      /* port change: ground speed kept per frame during the uppercut wind-up */
#define PHYS_DK_VY_ADD  5.0       /* RV: down-kick adds +5 to vy (after the windup frame's gravity) */
#define PHYS_CROUCHATK_T 18        /* RV: crouch kick (anim 30) length */
#define PHYS_BOOST_DIAG 8.0       /* RV: diagonal boost components (8, 8) */
#define PHYS_BOOST_LAND 10.0      /* RV: ground speed when a down-diagonal boost lands */
#define PHYS_ENERGY_MAX 100.0     /* RV: GUIEnergy_127 alt A; boost needs a full meter */
#define PHYS_ENERGY_CYCLONE 0.25 /* RV (cyclone, cyc_down traces): -0.25/frame in the cyclone (M26) */
#define PHYS_ENERGY_DRAIN 3.0     /* RV: -3/frame during the boost (100 -> 0 over 33 f) */
#define PHYS_ENERGY_REFILL 0.4    /* RV: +0.4/frame otherwise (250 f = 4.17 s from empty) */
#define PHYS_ENERGY_CRYSTAL 5.0   /* RV: each crystal pickup adds 5 (petals add none) */
#define CRYSTALS_PER_LIFE 200     /* RV: HUD crystal counter starts at 200 on a fresh file and counts down */
#define LIVES_START     5          /* RV: fresh file shows 05 */
#define PHYS_INVULN_T    60      /* RE: PlayerOthers.E = 60 on a hit, -1/frame (event 2414 + countdown); RV f189..f249 */
#define PHYS_HURT_VX    2.2       /* RV: knockback speed away from the hazard */
#define PHYS_HURT_VY    4.2       /* RV: knockback rise */
#define PHYS_HURT_GRAV  0.5       /* RV: gravity during hurt knockback */
#define PHYS_SPRING     10.75     /* RV spring_yellow_up: vy -10.75 on contact, then normal gravity */
#define PHYS_SPRING_DX  9.0       /* RV spring_yellow_upleft: (-9, -8.75) */
#define PHYS_SPRING_DY  8.75
#define PHYS_SPRING_GSP 10.0      /* RV spring_yellow_right: ground speed 10 (no input lock) */
#define ORIG_REF_DY     14.0      /* RV: original's reference point is ~14 px above the feet */

#endif
