/* Level definitions (session 13): one row per ported act, so the shared code (act end and transition, results, stage
 * music, water, Time Attack / save records, walk-in) reads level DATA instead of testing level names. Adding an act of
 * an already-ported mechanic = one row here + the importer run (ASSET_PIPELINE "Adding a level"); only genuinely new
 * behaviour needs new code. Values are the original's (labels per column below). */
#include "../fp.h"
#include <string.h>

/* Designated fields: anything left out is 0 / NULL (dry, no edge results, ...). Labels: the event numbers are the
 * original's (RE); see RE_NOTES "Level table". */
#define DV(d, f, n, mus) .dir = d, .frame = f, .stage = 1, .act = n, .nacts = 4, .music = mus
#define AT(d, f, n, mus) .dir = d, .frame = f, .stage = 0, .act = n, .nacts = 4, .music = mus, .plate_rule = 1, .switch_late = 1
static const level_def_t LEVELS[] = {
    /* Dragon Valley: frame index 20-23 (RE loop_play_music_N_20..23), water DV2 / DV3 (5109 / 5314), DV4 results at the
     * right edge (5381) as well as after the boss */
    { DV("dv1", 20, 1, "bgm/m_stage1.adp") },
    { DV("dv2", 21, 2, "bgm/m_stage1b.adp"), .water = 2700 },
    { DV("dv3", 22, 3, "bgm/m_stage1b.adp"), .water = 2700 },
    { DV("dv4", 23, 4, "bgm/m_stage1b.adp"), .edge_results = 1 },
    /* Aqua Tunnel: frame index 16-19 (tools/re/bgm_refs.py: 16 m_stage0, 17/18 m_stage0b; AT2 after the Chomper (g4 >=
     * 10, 4330) m_stage0b), water 1200 / 1200 / none / 1480 (4309 / 4478 / 4530 / 4685), AT2 walk-in B = 0 (4403), AT3
     * ends at the top (4521: MV.Y < 380), AT4 results through the boss only (4652) */
    { AT("at1", 16, 1, "bgm/m_stage0.adp"), .water = 1200, .axel2_top = 320 },
    { AT("at2", 17, 2, "bgm/m_stage0.adp"), .walkin_still = 1, .music_cp = "bgm/m_stage0b.adp", .water = 1200 },
    { AT("at3", 18, 3, "bgm/m_stage0b.adp"), .exit_max_y = 380, .pipe_auto_y = 2900 },
    { AT("at4", 19, 4, "bgm/m_stage0b.adp"), .water = 1480 },
};
static const level_def_t DEFAULT = { .dir = "", .frame = -1, .stage = -1, .act = 1, .nacts = 1, .music = "bgm/m_stage1.adp" };

const level_def_t *level_def(const char *dir) {
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (!strcmp(LEVELS[i].dir, dir)) return &LEVELS[i];
    return &DEFAULT;
}
const level_def_t *level_cur(void) { return level_def(G.lvdir); }
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
