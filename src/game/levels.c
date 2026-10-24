/* Level definitions (session 13): one row per ported act, so the shared code (act end and transition, results, stage
 * music, water, Time Attack / save records, walk-in) reads level DATA instead of testing level names. Adding an act of
 * an already-ported mechanic = one row here + the importer run (ASSET_PIPELINE "Adding a level"); only genuinely new
 * behaviour needs new code. Values are the original's (labels per column below). */
#include "../fp.h"
#include <math.h>
#include <string.h>

/* Designated fields: anything left out is 0 / NULL (dry, no edge results, ...). Labels: the event numbers are the
 * original's (RE); see RE_NOTES "Level table". */
/* Carol's jump pads (spec_carol_bike C2): x, y, index A, angle B, chain end D, lit E, block pad (DV4 anim 7) */
static const padcfg_t PADS_DV2[] = { {2736,1144,0,0,0,1,0}, {2936,992,1,90,0,0,0}, {2912,688,2,180,0,0,0}, {3176,688,3,180,1,0,0},
    {3400,1416,4,0,0,1,0}, {3400,1224,5,180,0,0,0}, {3728,1224,6,180,0,0,0}, {4032,1224,7,180,1,0,0} };
static const padcfg_t PADS_DV3[] = { {7408,2200,0,90,0,1,0}, {7072,1952,1,270,0,0,0}, {7344,1952,2,90,1,0,0}, {680,1776,3,0,0,1,0},
    {744,1528,4,90,0,0,0}, {256,1328,5,270,0,0,0}, {520,1064,6,0,0,0,0}, {776,1064,7,0,0,0,0}, {128,1704,8,270,0,1,0}, {384,1776,9,0,1,0,0},
    {4736,2104,10,0,0,1,0}, {4904,1920,11,180,0,0,0}, {5136,2104,12,0,0,0,0}, {5408,1864,13,180,0,0,0}, {5664,1712,14,180,1,0,0} };
static const padcfg_t PADS_DV4[] = { {4040,2296,0,0,0,1,1}, {4368,2152,1,180,0,0,0}, {4616,2160,2,0,0,0,0}, {4864,2168,3,180,0,0,0},
    {5136,2152,4,180,0,0,0}, {5296,2152,5,0,0,0,0}, {5504,2176,6,90,0,0,0}, {5344,2096,7,270,0,0,0}, {5344,2000,8,270,1,0,0},
    {10120,2134,9,-4.5f,0,1,1}, {10400,2024,10,180,0,0,0}, {10432,1992,11,270,1,0,0} };
/* Milla's dig zones (spec_milla_moves 6, events 4171-4182 of frame 17) */
static const zone_t DIGZ_AT1[] = {
    { 6939, 7302, 102, 185 }, { 1479, 1705, 594, 671 }, { 9450, 9654, 181, 262 }, { 3214, 3315, 838, 904 },
    { 8866, 9270, 715, 794 }, { 8197, 8418, 442, 507 }, { 9247, 10206, 611, 729 }, { 10607, 10744, 656, 711 },
    { 10523, 10563, 659, 713 }, { 10413, 11262, 1052, 1154 }, { 11671, 12410, 822, 903 }, { 10388, 10482, 157, 223 } };
/* Aqua Tunnel stage flow (spec_at_flow 3 / 6): checkpoints (4196 / 4197, 4405, 4598), boss camera locks (4198 / 4199,
 * 4408 / 4409, 4599) and the act-start item sets (4318 / 4319 crystals per character, 4484 / 4485) */
static const cpzone_t CP_AT1[] = { { 7540, 7640, 2, 7600, 1100 }, { 11250, 11350, 3, 11300, 988 } };
static const cpzone_t CP_AT2[] = { { 5640, 5740, 3, 5720, 270 } };
static const cpzone_t CP_AT4[] = { { 3264 - 0, 3364, 2, 3264, 200 } };
/*                                g4 max, lock X, Y min,  Y max,  l,     t,   r,     b,    unlock X, unlock Y */
static const camzone_t CAM_AT1 = { 0,  11704, -32768, 903,    11368, 600, 12479, 903,  11704 - 240, 903 };
static const camzone_t CAM_AT2 = { 10, 6032,  976,    32767,  6032,  976, 6863,  1231, 32767,  976 };
static const camzone_t CAM_AT4 = { 10, 6464,  -32768, 32767,  6464,  8,   7039,  319,  -32768, -32768 };
static const killbox_t KILL_AT1[] = { { 7579, 7788, 732, 1129, 1, 0 }, { 7790, 8050, 929, 1133, 2, 0 } };
static const killbox_t KILL_AT2[] = { { 5260, 5467, 379, 814, 0, 1 } };
/* Aqua Tunnel's sound group (D-047: water, jets, slimes, bombs, bosses) */
static const short SFX_AT[] = { 55, 62, 69, 99, 172, 173, 240, 242, 251, 252, 278, 306, 308, 316 };
#define N_(a) (int)(sizeof a / sizeof a[0])

#define DV(d, f, n, mus) .dir = d, .frame = f, .stage = 1, .act = n, .nacts = 4, .music = mus
#define AT(d, f, n, mus) .dir = d, .frame = f, .stage = 0, .act = n, .nacts = 4, .music = mus, .plate_rule = 1, .switch_late = 1, .monitor_hop = 1, .checkpoints = 1, .sfx = SFX_AT, .nsfx = N_(SFX_AT)
static const level_def_t LEVELS[] = {
    /* Dragon Valley: frame index 20-23 (RE loop_play_music_N_20..23), water DV2 / DV3 (5109 / 5314), DV4 results at the
     * right edge (5381) as well as after the boss */
    { DV("dv1", 20, 1, "bgm/m_stage1.adp") },
    { DV("dv2", 21, 2, "bgm/m_stage1b.adp"), .water = 2700, .pads = PADS_DV2, .npads = N_(PADS_DV2) },
    { DV("dv3", 22, 3, "bgm/m_stage1b.adp"), .water = 2700, .pads = PADS_DV3, .npads = N_(PADS_DV3) },
    { DV("dv4", 23, 4, "bgm/m_stage1b.adp"), .edge_results = 1, .pads = PADS_DV4, .npads = N_(PADS_DV4) },
    /* Aqua Tunnel: frame index 16-19 (tools/re/bgm_refs.py: 16 m_stage0, 17/18 m_stage0b; AT2 after the Chomper (g4 >=
     * 10, 4330) m_stage0b), water 1200 / 1200 / none / 1480 (4309 / 4478 / 4530 / 4685), AT2 walk-in B = 0 (4403), AT3
     * ends at the top (4521: MV.Y < 380), AT4 results through the boss only (4652) */
    { AT("at1", 16, 1, "bgm/m_stage0.adp"), .water = 1200, .axel2_top = 320, .dig_zones = DIGZ_AT1, .ndig_zones = N_(DIGZ_AT1),
      .start_fall = 1, .cps = CP_AT1, .ncps = N_(CP_AT1), .boss_cam = &CAM_AT1, .kills = KILL_AT1, .nkills = N_(KILL_AT1), .cam_right = 12480 + 213 },
    { AT("at2", 17, 2, "bgm/m_stage0.adp"), .walkin_still = 1, .music_cp = "bgm/m_stage0b.adp", .water = 1200,
      .cps = CP_AT2, .ncps = N_(CP_AT2), .boss_cam = &CAM_AT2, .kills = KILL_AT2, .nkills = N_(KILL_AT2), .cam_cp10_left = 6144 - 213, .cam_cp10_pad = 50 },
    { AT("at3", 18, 3, "bgm/m_stage0b.adp"), .exit_max_y = 380, .pipe_auto_y = 2900, .cam_right = 856 + 213 },
    { AT("at4", 19, 4, "bgm/m_stage0b.adp"), .water = 1480, .cps = CP_AT4, .ncps = N_(CP_AT4), .boss_cam = &CAM_AT4, .cam_right = 7424 + 213 },
};
static const level_def_t DEFAULT = { .dir = "", .frame = -1, .stage = -1, .act = 1, .nacts = 1, .music = "bgm/m_stage1.adp" };

const level_def_t *level_def(const char *dir) {
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (!strcmp(LEVELS[i].dir, dir)) return &LEVELS[i];
    return &DEFAULT;
}
const level_def_t *level_cur(void) { return level_def(G.lvdir); }
/* Classic / continue route: the first stage of each character (RE 799: Classic with G1 == 4 (Milla) -> frame 16, Aqua
 * Tunnel; everyone else -> frame 20, Dragon Valley). Indexed by the stage record index (s). */
const char *level_route_start(int character) { return level_first_of_stage(chr_is_milla(character) ? 0 : 1); }
/* first act of a stage record index (Time Attack / routes) */
const char *level_first_of_stage(int stage) {
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (LEVELS[i].stage == stage && LEVELS[i].act == 1) return LEVELS[i].dir;
    return NULL;
}
/* the next act of the same stage (NULL after the last act) */
const char *level_next(const char *dir) {
    const level_def_t *d = level_def(dir);
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (LEVELS[i].stage == d->stage && LEVELS[i].act == d->act + 1) return LEVELS[i].dir;
    return NULL;
}

/* ---- stage flow shared by every level (session 13; before: aqua.c, by act number) ---------------------------------- */
static int cam_locked;
int level_cam_locked(void) { return cam_locked; }   /* 4212 / 4451: the boss camera trigger flag */
/* the act start: remove the item sets this character does not get (4318 / 4319) and the AT2 jelly / petal row (4484 /
 * 4485: only while g1 != 1, i.e. always in the port) */
void level_flow_spawn(void) {
    const level_def_t *L = level_cur();
    cam_locked = 0;
    for (int k = 0; k < L->nkills; k++) {
        const killbox_t *z = &L->kills[k];
        if ((z->who == 1 && chr_is_milla(G.character)) || (z->who == 2 && !chr_is_milla(G.character))) continue;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || o->x < z->x0 || o->x > z->x1 || o->y < z->y0 || o->y > z->y1) continue;
            if (o->type == OT_CRYSTAL || (z->jelly && (o->type == OT_JELLY || o->type == OT_PETAL))) o->alive = 0;
        }
    }
}
/* normal camera bounds (spec_at_flow 3.1): C = 0, E = cam_right or the frame width; after g4 >= 10 the AT2 values */
void level_camera_bounds(void) {
    const level_def_t *L = level_cur();
    if (L->cam_cp10_pad && G.at_g4 >= 10) { G.cam_nC = (float)L->cam_cp10_left; G.cam_nE = (float)G.lvl.w + L->cam_cp10_pad; }
    else if (L->cam_right) G.cam_nE = (float)L->cam_right;
}
void level_release_camera(int keep_trigger) { if (!keep_trigger) cam_locked = 0; camera_normal_bounds(); G.cam_m2 = 1; }
static void checkpoint(int g4, float x, float y) {                  /* g4 / g5 / g6 + loop_cp_16 (restart values) */
    G.at_g4 = g4; G.at_g5 = x; G.at_g6 = y;
    G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
}
/* per frame, after the objects: checkpoints, then the boss camera lock (lock: X > lock_x and lock_ymin < Y < lock_ymax;
 * release: X < unlock_x and Y < unlock_y; a zone with g4_max does nothing once g4 >= g4_max) */
void level_flow_frame(const player_t *p) {
    const level_def_t *L = level_cur();
    double X = p->x;
    for (int k = 0; k < L->ncps; k++) { const cpzone_t *c = &L->cps[k]; if (X > c->x0 && X < c->x1 && G.at_g4 < c->g4) checkpoint(c->g4, c->x, c->y); }
    const camzone_t *z = L->boss_cam;
    if (!z || (z->g4_max && G.at_g4 >= z->g4_max)) return;
    int px = (int)p->x, py = (int)floor(p->y - ORIG_REF_DY);
    if (!cam_locked && px > z->lock_x && py > z->lock_ymin && py < z->lock_ymax) { cam_locked = 1; camera_boss_bounds(z->l, z->t, z->r, z->b); }
    else if (cam_locked && px < z->unlock_x && py < z->unlock_y) level_release_camera(0);
}
