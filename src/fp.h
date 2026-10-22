/* fpdc - native Dreamcast runtime for a Freedom Planet port.
 * v2: levels, collision masks and animations recovered from the original
 * Linux build (see RESEARCH_LOG.md / PROJECT_AUDIT.md). Engine code is
 * original; game data is converted from the user's copy and never committed. */
#ifndef FP_H
#define FP_H

#include <stdint.h>
#include <stddef.h>

#define SCREEN_W 320          /* logical resolution, scaled x2 to 640x480 */
#define SCREEN_H 240
#define FPS 60

/* ---------------------------------------------------------------- platform */
enum {
    BTN_LEFT = 1 << 0, BTN_RIGHT = 1 << 1, BTN_UP = 1 << 2, BTN_DOWN = 1 << 3,
    BTN_JUMP = 1 << 4,   /* DC A  = original "A" (jump)            */
    BTN_ATTACK = 1 << 5, /* DC X  = original "B" (attack)          */
    BTN_SPECIAL = 1 << 6,/* DC B  = original "C" (Dragon Boost)    */
    BTN_START = 1 << 7,
    BTN_GUARD = 1 << 8   /* DC Y  (reserved) */
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

/* ---------------------------------------------------------------- renderer */
typedef struct { int id, w, h, srcw, srch; } tex_t;

int    r_init(void);
int    r_tex_load(tex_t *t, const void *fptx, size_t size);
int    r_pal_load(const void *fppl, size_t size);
void   r_begin(void);
void   r_sprite(const tex_t *t, int u, int v, int uw, int vh,
                float x, float y, float w, float h, int flip, uint32_t argb);
void   r_rect(float x, float y, float w, float h, uint32_t argb);
void   r_end(void);

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
int  tex_load_file(tex_t *t, const char *name);
int  pal_load_file(const char *name);

/* ---------------------------------------------------------------- level (FPL2) */
enum { COL_NONE, COL_SOLID, COL_PLATFORM, COL_LADDER };
typedef struct { int32_t x, y; uint16_t u, v, w, h; uint8_t atlas, layer, col, pad; } piece_t;
typedef struct { uint16_t type, anim; int32_t x, y; } lvobj_t;

/* engine object types (tools/import_frame.py ENGINE_TYPES) */
enum { OT_START = 1, OT_CRYSTAL = 2, OT_DRAGONFLY = 3, OT_PETAL = 6, OT_SPIKES = 7,
       OT_SPRING = 8, OT_SIGN = 9, OT_TURRETUS = 10, OT_ROCK = 11, OT_MONITOR = 12,
       OT_PARALLAX = 20, OT_EXPLODE = 100 };

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
    float x, y, vx, vy, gsp, angle;
    int ground, face;
    int state, state_t;
    animator_t an;
    int hp, invuln, crystals;
    int jumping, cyclone_used, boost_used;
    float boost_dx, boost_dy;
    float spawn_x, spawn_y;
    int dead_t;
} player_t;

typedef struct {
    int type, alive, hp;
    float x, y, ox, oy, vx, vy;
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
    int frame, clear_t, paused, kills;
    struct { int jump, crystal, hit, explode, attack, hurt, boost; } sfx;
    uint32_t prof_update_us, prof_draw_us;
} game_t;

extern game_t G;

int  game_init(void);
void game_update(const input_t *in);
void game_draw(void);
void player_reset(player_t *p, float x, float y);
void player_update(player_t *p, const input_t *in);
void player_draw(const player_t *p);
int  player_attack_box(const player_t *p, float *x0, float *y0, float *x1, float *y1);
int  player_invincible(const player_t *p);
void player_hurt(player_t *p, float from_x);
void player_bounce(player_t *p, float vy);
void player_spring(player_t *p, float vy);
void objects_spawn(void);
void objects_update(void);
void objects_draw(int front);
void camera_update(void);
void hud_draw(void);
void text_draw(float x, float y, const char *s, uint32_t argb, int scale);

/* Physics tunables: px/frame at 60 Hz. UNVERIFIED placeholders until the
 * original's values are recovered (DECISIONS D-005, PROJECT_AUDIT §3). */
#define PHYS_ACC        0.125f
#define PHYS_DEC        0.5f
#define PHYS_FRIC       0.09375f
#define PHYS_TOP        6.0f
#define PHYS_MAXSPD     14.0f
#define PHYS_AIR_ACC    0.1875f
#define PHYS_AIR_DRAG   0.03125f   /* rising: |vx| decays toward PHYS_TOP (KB "Air physics") */
#define PHYS_GRAV       0.21875f
#define PHYS_JUMP       6.5f
#define PHYS_JUMP_CUT   4.0f
#define PHYS_SLOPE      0.125f
#define PHYS_MAXFALL    12.0f
#define PHYS_BOOST      10.0f
#define PHYS_SPRING     11.0f

#endif
