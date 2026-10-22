/* fpdc - native Dreamcast runtime for a Freedom Planet port.
 * Common types and module interfaces. Engine code is original; game data is
 * converted from the user's own copy by tools/convert.py and never committed. */
#ifndef FP_H
#define FP_H

#include <stdint.h>
#include <stddef.h>

#define SCREEN_W 320          /* logical resolution, scaled x2 to 640x480 */
#define SCREEN_H 240
#define TILE 16
#define FPS 60

/* ---------------------------------------------------------------- platform */
enum {
    BTN_LEFT = 1 << 0, BTN_RIGHT = 1 << 1, BTN_UP = 1 << 2, BTN_DOWN = 1 << 3,
    BTN_JUMP = 1 << 4,   /* A */
    BTN_ATTACK = 1 << 5, /* X */
    BTN_SPECIAL = 1 << 6,/* B */
    BTN_START = 1 << 7,
    BTN_GUARD = 1 << 8   /* Y (reserved) */
};

typedef struct {
    uint32_t held, pressed, released;
} input_t;

void   plat_init(void);
void   plat_shutdown(void);
int    plat_running(void);
void   plat_poll_input(input_t *in);          /* fills held/pressed/released */
void  *plat_load_file(const char *name, size_t *size); /* name relative to data root; malloc'd */
void   plat_log(const char *fmt, ...);
uint64_t plat_time_us(void);

/* audio */
void   snd_init_fp(void);
int    snd_load(const char *name);            /* returns handle or -1 */
void   snd_play(int h, int vol, int pan);     /* vol 0..255, pan 0..255 (128 centre) */
int    music_play(const char *name);          /* raw s16le stereo 22050 Hz, loops */
void   music_update(void);
void   music_stop(void);

/* ---------------------------------------------------------------- renderer */
typedef struct {
    int id;                    /* backend handle */
    int w, h;                  /* texture (pow2) size */
    int srcw, srch;            /* meaningful image size */
} tex_t;

int    r_init(void);
int    r_tex_load(tex_t *t, const void *fptx, size_t size);
void   r_begin(void);
/* draw texel rect (u,v,uw,vh) of t at screen (x,y) size (w,h); flip=1 mirrors X */
void   r_sprite(const tex_t *t, int u, int v, int uw, int vh,
                float x, float y, float w, float h, int flip, uint32_t argb);
void   r_rect(float x, float y, float w, float h, uint32_t argb);
void   r_end(void);
void   r_shutdown(void);

/* ---------------------------------------------------------------- resources */
typedef struct { uint16_t u, v, w, h; int16_t hx, hy; } frame_t;
typedef struct { char name[16]; uint16_t first, count, speed, loop; } anim_t;

typedef struct {
    tex_t tex;
    int nframes, nanims;
    frame_t *frames;
    anim_t *anims;
    uint16_t *seq;
} bank_t;

int  bank_load(bank_t *b, const char *name);  /* loads name.tex + name.spr */
int  bank_anim(const bank_t *b, const char *name);
const frame_t *bank_frame(const bank_t *b, int anim, int tick);
int  anim_done(const bank_t *b, int anim, int tick);
void bank_draw(const bank_t *b, int anim, int tick, float x, float y, int flip, uint32_t argb);
int  tex_load_file(tex_t *t, const char *name);

/* ---------------------------------------------------------------- level */
enum { T_EMPTY, T_SOLID, T_SLOPE_R, T_SLOPE_L, T_ONEWAY, T_SPIKE,
       T_GENTLE_R_LO, T_GENTLE_R_HI, T_GENTLE_L_HI, T_GENTLE_L_LO, T_COUNT };
enum { O_NONE, O_PLAYER, O_CRYSTAL, O_HORNET, O_CHECKPOINT, O_GOAL };

typedef struct { uint16_t type; int16_t x, y, pad; } lvlobj_t;

typedef struct {
    int w, h;                  /* tiles */
    uint8_t *tiles;
    int nobj;
    lvlobj_t *objs;
} level_t;

int  level_load(level_t *l, const char *name);
int  level_tile(const level_t *l, int tx, int ty);
/* surface height (0..16 from tile bottom) of column cx (0..15) in tile type t; -1 none */
int  tile_height(int t, int cx);
/* Find floor surface y at world x, scanning from y0 downward up to maxdist px.
 * Returns 1 and sets *sy (surface y) / *angle (radians, +ve = rising to right)
 * when found. oneway: whether one-way tiles count. */
int  floor_probe(const level_t *l, float x, float y0, float maxdist, int oneway, float *sy, float *angle);
int  solid_at(const level_t *l, float x, float y); /* full-solid test for walls/ceilings */
int  spike_at(const level_t *l, float x, float y);

/* ---------------------------------------------------------------- game */
typedef struct {
    float x, y;                /* feet centre */
    float vx, vy, gsp;
    int ground;
    float angle;
    int face;                  /* 1 right, -1 left */
    int state, state_t;
    int anim, anim_tick;
    int hp, invuln, crystals;
    int jumping, cyclone_used, attack_t, charge_t, boost_t, hurt_t;
    float boost_dx, boost_dy;
    float spawn_x, spawn_y;
    int dead_t;
} player_t;

typedef struct {
    int type, alive;
    float x, y, ox, oy;
    int t, anim, tick;
    int flip;
} obj_t;

#define MAX_OBJS 128

typedef struct {
    level_t lvl;
    bank_t lilac, objs;
    tex_t sky, mount, forest, rock, log;
    player_t p;
    obj_t o[MAX_OBJS];
    int nobjs;
    float camx, camy;
    int frame;
    int clear_t;
    int paused;
    struct { int jump, crystal, hit, explode, attack, hurt, boost; } sfx;
    /* stats for profiling */
    uint32_t prof_update_us, prof_draw_us;
} game_t;

extern game_t G;

int  game_init(void);
void game_update(const input_t *in);
void game_draw(void);
void player_reset(player_t *p, float x, float y);
void player_update(player_t *p, const input_t *in);
void player_draw(const player_t *p);
int  player_attacking(const player_t *p);
int  player_attack_box(const player_t *p, float *x0, float *y0, float *x1, float *y1);
void player_hurt(player_t *p, float from_x);
void objects_spawn(void);
void objects_update(void);
void objects_draw(void);
void camera_update(void);
void hud_draw(void);
void text_draw(float x, float y, const char *s, uint32_t argb, int scale);

/* tunables (units: px/frame @60Hz). UNVERIFIED - see DECISIONS.md */
#define PHYS_ACC        0.125f
#define PHYS_DEC        0.5f
#define PHYS_FRIC       0.09375f
#define PHYS_TOP        6.0f
#define PHYS_MAXSPD     14.0f
#define PHYS_AIR_ACC    0.1875f
#define PHYS_GRAV       0.21875f
#define PHYS_JUMP       6.5f
#define PHYS_JUMP_CUT   4.0f
#define PHYS_SLOPE      0.125f
#define PHYS_MAXFALL    12.0f
#define PHYS_BOOST      10.0f

#endif
