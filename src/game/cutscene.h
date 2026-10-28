/* A small data-driven cutscene player (session 24), for the adventure scenes the port does not have yet (the original's
 * story rooms, frames 75-82, and the stage dialogues it skips: SB1 Kujacker, TH arena, PL6, the ending's epilogue).
 *
 * A scene is a static const list of cues (no allocation, no file format): content lives in its own .c file (see
 * cutscene_example.c), the player (cutscene.c) knows nothing about any scene.
 *
 * Cues run in order. Each cue waits until the scene clock reaches its t (frames since the previous blocking cue ended),
 * then fires; CS_SAY and CS_WAIT block. Sprites come from an already loaded UI scene (tools/import_ui.py: any frame's
 * objects, by class name: the cast skins, NPCs, portraits), so a scene costs no extra VRAM beyond the screen it plays on.
 *
 *   op          slot  x, y          str                       arg
 *   CS_SPRITE   0-3   screen pos    class name in the scene   anim << 8 | dir   (shows the slot; the animation loops)
 *   CS_MOVE     0-3   target pos    -                         frames to get there (linear)
 *   CS_HIDE     0-3   -             -                         -
 *   CS_SAY      -     -             "Speaker|text"            frames per character (0 = 1); waits for A after the text
 *   CS_CLEAR    -     -             -                         -                  (removes the text box)
 *   CS_VOICE    -     -             voice name (voice_play)   -
 *   CS_SFX      -     -             -                         original sound id (snd_play_orig)
 *   CS_MUSIC    -     -             bgm path ("bgm/m_x.adp"), NULL stops
 *   CS_FADE     -     -             -                         > 0 fade in from black over arg frames, < 0 fade out
 *   CS_WAIT     -     -             -                         frames (blocking)
 *   CS_END      (last cue)
 * Input: A (jump) completes the typed text, then advances; Start ends a skippable scene at once (the fade-out and the
 * caller's next step follow, as the original's dialog skip: TH arena / SB1 skip the whole exchange).
 *
 * Use: cs_begin(&scene, &ui_scene, &font_bank); then each frame cs_update(&input) (1 = finished) and cs_draw() inside the
 * caller's r_begin / r_end. To add content: copy cutscene_example.c, name the scene, list its cues, and call cs_begin from
 * the place that plays it (a stage script or a front-end state) with the UI scene whose objects it shows. */
#ifndef FP_CUTSCENE_H
#define FP_CUTSCENE_H
#include "../fp.h"
#include "ui.h"

enum { CS_END, CS_WAIT, CS_SPRITE, CS_MOVE, CS_HIDE, CS_SAY, CS_CLEAR, CS_VOICE, CS_SFX, CS_MUSIC, CS_FADE };
#define CS_SLOTS 4
typedef struct { uint16_t t; uint8_t op, slot; int16_t x, y; const char *str; int16_t arg; } cs_cue_t;
typedef struct { const char *name; const cs_cue_t *cue; uint8_t skippable; } cs_scene_t;

void cs_begin(const cs_scene_t *sc, ui_scene_t *sprites, const bank_t *font);
int  cs_update(const input_t *in);      /* 1 when the scene has ended (or was skipped) */
void cs_draw(void);
int  cs_active(void);
int  cs_cue_index(void);                /* tests: the cue being played */

extern const cs_scene_t CS_EXAMPLE;     /* cutscene_example.c: the documented template (host test only, not in the game flow) */
#endif
