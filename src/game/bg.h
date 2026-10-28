/* Battle Glacier (session 24): shared declarations of the bg_*.c modules. Engine hooks are in src/fp.h ("Battle Glacier");
 * each module adds its own type enum and helpers here, in its own block. stage index 9, json 60-65 = bg1-bg6. */
#ifndef FP_BG_H
#define FP_BG_H
#include "../fp.h"

/* ---- bg_flow.c (types 2304-2335) */

/* ---- bg_enemy.c (types 2336-2399; build/re/spec_bg_enemies.md): placed 2336-2369 (tools/import_frame.py BG_ENEMIES), runtime
 * 2370-2399. Battle Glacier's other enemies are the existing modules' types (Thermal Base saucers / squids, Jade Creek's Elite,
 * meteor rollers and bull frags, Fortune Night's super turretus / ghetto shell, Sky Battalion's spawn tubes / rotor blades /
 * pelletizer, the engine's dragonflies, turretus, Tredds, extractors). */
enum {
    OT_BGE_DRILL = 2336,   /* EnemyDrillSat_3291 (Q20, A 5, ctor invisible): bursts out of the ground, hovers, knocked into a flying drill */
    OT_BGE_SPAWNER = 2337, /* EnemyEliteSpawner_3361 (not Q20): a pod that drops on the player and releases its payload (BG5 / FD) */
    OT_BGE_WAVE = 2370,    /* runtime: EnemyEliteSpawnerWave_3363 (the landing shock ring) */
    OT_BGE_GHOST = 2371,   /* runtime: EffectsSpadeImage_456 direction 12 (the falling pod's afterimage) */
    OT_BGE_DEB = 2372      /* runtime: EffectsDebris_427 anim 5 (the drill's dirt) */
};
/* EnemyEliteSpawner (RE BG5 14656-14662 / 14724, FD 15245-15249 / 21288-21294 / 22376): reusable by any stage. The pod waits
 * 300 px above its placed y (A = that y); with the player within 200 px in x and A in (MV.Y - 50, MV.Y + 150) it shows (sound
 * 345) and drops 20 px a frame with afterimages, lands (B 2: a shock wave, sound 346), opens (anim 8, frame B / 6) and at B > 30
 * releases its payload at its feet and goes. Payload: the spawner's av[BGE_SPAWNER_C] (FD's alterable C; 0 = an Elite, BG5's
 * only payload). Another module handles other codes with bg_enemy_spawner_payload(fn): fn(spawner, code) returns non-zero when
 * it created the payload. */
#define BGE_SPAWNER_C 2      /* spawner av[]: [0] A landing y, [1] B (0 waiting, 1 falling, > 1 opening clock: another module may set
                                B = 1 to drop one itself, e.g. an arena script), [2] C payload code */
typedef int (*bge_payload_fn)(obj_t *spawner, int code);
void bg_enemy_spawner_payload(bge_payload_fn fn);
int  bg_enemy_spawners_open(void);        /* spawners still waiting / falling (tests, FD arenas) */

/* ---- bg_gim.c (types 2400-2479) */

/* ---- bg_boss.c (types 2480-2559) */

#endif
