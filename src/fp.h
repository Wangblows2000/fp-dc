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
                    float angle_deg, uint32_t argb);
void   r_end(void);
void   r_reset(void);                 /* free all textures (level change) */

/* ---------------------------------------------------------------- resources */
typedef struct { uint16_t u, v, w, h; int16_t hx, hy; } frame_t;
typedef struct { char name[16]; uint16_t first, count; uint8_t speed, back_to; int16_t loop; } anim_t;

typedef struct {
    tex_t tex;
    int nframes, nanims;
    frame_t *frames;
    anim_t *anims;
    uint16_t *seq;
} bank_t;

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
void frame_draw_rot(const bank_t *b, const frame_t *f, float x, float y, float angle_deg, uint32_t argb);
int  tex_load_file(tex_t *t, const char *name);
int  pal_load_file(const char *name);

/* ---------------------------------------------------------------- level (FPL2) */
enum { COL_NONE, COL_SOLID, COL_PLATFORM, COL_LADDER };
typedef struct { int32_t x, y; uint16_t u, v, w, h; uint8_t atlas, layer, col, pad; } piece_t;
typedef struct { uint16_t type, anim; int32_t x, y; } lvobj_t;

/* engine object types (tools/import_frame.py ENGINE_TYPES) */
enum { OT_START = 1, OT_CRYSTAL = 2, OT_DRAGONFLY = 3, OT_PETAL = 6, OT_SPIKES = 7,
       OT_SPRING = 8, OT_SIGN = 9, OT_TURRETUS = 10, OT_ROCK = 11, OT_MONITOR = 12,
       OT_SPRING_UL = 13, OT_SPRING_UR = 14, OT_SPRING_R = 15, OT_SPRING_L = 16,
       OT_PARALLAX = 20, OT_EXPLODE = 100, OT_PELLET = 101 };

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

/* ---------------------------------------------------------------- game */
typedef struct {
    double x, y, vx, vy, gsp, angle;   /* doubles like the original (Clickteam alterables; event 1574 floors to 4 decimals) */
    double angle_s;             /* smoothed angle (original alt O): movement direction + slope force */
    int ground, face;
    int state, state_t;
    animator_t an;
    int hp, invuln, crystals, lives;   /* hp in half-petals; crystals count DOWN from 200 (RV HUD) */
    int jumping, cyclone_used, boost_used, cyclone_ground_start, hurt_land_t;
    float energy, energy_drain;  /* RV energy meter 0..100 (boost needs 100) */
    int energy_init;
    int springing, skidding, ctrl_lock;
    int look;                      /* 2 = looking up (M2), 3 = crouching (M3) this frame (camera) */                 /* launched by a spring, until landing (spring anim while rising) */
    float rot;                   /* sprite angle, degrees CCW (original Active angle @+0xc4) */
    float boost_dx, boost_dy;
    float spawn_x, spawn_y;
    int dead_t;
} player_t;

typedef struct {
    int type, alive, hp;
    int st, tm, dir, born;       /* recovered enemy state (Turretus D, E; direction); frame created */
    float x, y, ox, oy, vx, vy, fx, fy;
    int t, flip, par;
    animator_t an;
} obj_t;

#define MAX_OBJS 320

typedef struct {
    level_t lvl;
    bank_t lilac, objs;
    tex_t par[12]; int npar;
    player_t p;
    obj_t o[MAX_OBJS];
    int nobjs;
    float camx, camy;
    /* MasterCamera_408 alterables (RE, loop_camera_mode0_13 + events 2873-2886, 3539) */
    struct { float A, B, W, G, H, S, T, N, O; int V; } cam;
    int frame, clear_t, paused, kills;
    char lvdir[8];              /* current level data dir under the data root ("dv1") */
    int debug_select, in_select, sel, sel_top, sel_msg_t;
    int mode, card_t, pause_sel, time_attack, res_t, res_time;
    struct { int jump, crystal, hit, explode, attack, hurt, boost; } sfx;
    uint32_t prof_update_us, prof_draw_us;
} game_t;

extern game_t G;

enum { MODE_FRONTEND, MODE_PLAY, MODE_RESULTS, MODE_SELECT };
int  game_init(void);
void game_unload_all(void);
void game_start_adventure(const char *dir, int time_attack);
void game_dev_select(void);
int  fe_enter(void);
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
void player_hurt(player_t *p, double from_x);
void player_damage(player_t *p, int amount);   /* RE event 2414: amount in GUILifeBaseMeter units (5 = 1 half-petal on Normal) */
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
#define PHYS_MAXFALL    12.0      /* UNVERIFIED */
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
#define PHYS_CYCLONE_GRAV 0.1     /* RV: gravity during the cyclone (held or not) */
#define PHYS_CYCLONE_T  79         /* RV: cyclone state lasts 79 frames (4 traces, ground and air); landing keeps it */
#define PHYS_GCYC_JUMP  4.9       /* RV: jump out of a grounded cyclone: vy -4.9 */
#define PHYS_DK_VY_ADD  5.0       /* RV: down-kick adds +5 to vy (after the windup frame's gravity) */
#define PHYS_CROUCHATK_T 18        /* RV: crouch kick (anim 30) length */
#define PHYS_BOOST_DIAG 8.0       /* RV: diagonal boost components (8, 8) */
#define PHYS_BOOST_LAND 10.0      /* RV: ground speed when a down-diagonal boost lands */
#define PHYS_ENERGY_MAX 100.0     /* RV: GUIEnergy_127 alt A; boost needs a full meter */
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
