/* Adventure mode: the story (session 25). docs/ADVENTURE.md has the scene inventory and the evidence.
 *
 * ---- How the original does it (RE / RV, docs/ADVENTURE.md)
 * Global 0 is the mode (0 Time Attack, 1 Classic, 2 Adventure; the port's fe_mode). In Adventure the routing frame 8
 * (handle_frame_9) sends the end of stage I (MenuSequence.I) to a STORY ROOM (frame index 75-83, 14 / 15) with global 4
 * selecting the scene inside the room; a room's scene ends by setting the next frame (another room, or a stage) itself.
 * A new Adventure goes to frame 81 with g4 2 (hub RE 'c1' / character select RE 800). Story rooms are ordinary frames
 * (tiles, backdrops, a player) whose events script the actors and a DialogBox (DialogData_<handle> paragraphs:
 * "<speaker> <anim> <voice> <text>"). Their pause menu is GUIPause frame 1 (RESUME / SKIP SCENE / QUIT).
 *
 * ---- The port
 * - story.c: the ONE gate (story_gate), the progression state (story position + seen flags, saved: save.c v5), the
 *   triggers (new game, stage end = frame 8's Adventure routing, continue) and the MODE_STORY game mode: a scene loads its
 *   room as a level (tools/build_level.py st<json>), plays its script (cutscene.c) over it, and on completion (or SKIP
 *   SCENE) goes to the scene's next step exactly once.
 * - cutscene.c: the script player (the session-24 template extended): actors drawn from the room's _ens bank rows
 *   "s<handle>_<anim>_<dir>", the DialogBox UI (rows dlgbox / dlgname / dlgarrow, the original's art and layout), the
 *   original font, one voice clip per line (story/vo/<voice>.wav, loaded per line, text-only when missing).
 * - Data (tools/import_story.py from tables/story/, committed; the builder runs it):
 *     story/story.txt       the scene table (below)
 *     story/inline.txt      the in-stage scenes (same columns; next "back"): RM5's Adventure dialogue (session 25b)
 *     story/<id>.scn        one scene script per scene (tables/story/scripts/<id>.scn)
 *     story/dlg_<h>.txt     the DialogData paragraphs of object <h> (tables/story/dialog.json)
 *     story/vo/<voice>.wav  voice clips (tools/audio/convert_audio.py story), absent when the install has stubs
 *     st<json>/             the story rooms, built like levels, with the scene actors' rows in st<json>_ens
 *
 * ---- Scene table (story/story.txt), one scene per line, '#' comments:
 *     <id> <json room> <g4> <next> [skip=<next>] [stage=<level dir>|<next>] [panel=<0|1|2|3>] [trig=<trigger>...] [chr=<mask>]
 *   next / skip: "scene:<id>", "stage:<level dir>", "route" (frame 8: the stage after the last one), "menu", "back" (an
 *   in-stage scene, story_inline_begin: the stage that started it goes on), "credits" (ending.c's credits: the epilogue's end);
 *   skip "mark:chsel": SKIP SCENE jumps to the scene's own character select (no completion, RV k75_g0_skip700)
 *   stage: the NEXT STAGE row's target (panel 2), a level dir or (session 26) a step; panel: the pause menu (0 none: START =
 *   SKIP at once, RV k81_g2_start; 1 GUIPause frame 1 RESUME / SKIP SCENE / QUIT; 2 frame 2 RESUME / SKIP SCENE / NEXT STAGE;
 *   3 no pause at all: START does nothing, RV k69_g99_skip in the flashback room 70, RV k81_g0 / k81_g1 in the snowfields:
 *   RE 20866 opens the menu only while Player_326's flags & 0x300 are clear)
 *   trig: "new" (a new Adventure), "end:<I>" (frame 8 after stage I), "ending" (after ending.c's space scene, Adventure
 *   only); chr: g1 mask (1 Lilac, 2 Carol, 4 Milla; default 7)
 *   room: the story room st<room>: the json frame, or a split room of a large frame (json * 100 + CutsceneList index: frame
 *   83's scenes, docs/ADVENTURE.md 10). A scene whose chr excludes the character plays the scene of the same json frame and
 *   g4 that includes it (the original's room picks the content by g1: frame 83 g4 13 / 17)
 *
 * ---- Scene script (story/<id>.scn, tools/story_rv2scn.py), one entry per line:
 *     scene <id> / room st<json> / dialog <handle> / title <text>
 *     actor <n> <class>_<handle> <json layer> [ink] actor n (0..255), drawn with the rows s<handle>_<anim>_<dir>;
 *                                                    ink "add" (set_shader 0x20) or "a<alpha>" (blend colour, RE)
 *     seg <k>                                        segment k: starts at the scene start (0) or when the line of the
 *                                                    previous segment is dismissed (A with the arrow shown)
 *     <t> p <n> <x> <y>                              actor position (frame coordinates) at segment time t
 *     <t> m <n> <dx> <dy> <count>                    moves by (dx, dy) every frame for count frames from t
 *     <t> l <n> <x> <y> <k>                          moves linearly to (x, y) in k frames from t (rounded to the pixel)
 *     <t> i <n> <anim> <dir> <frame>                 actor image (the original's animation, direction, frame index)
 *     <t> v <n> <0|1>                                visibility
 *     <t> a <n> <alpha>                              actor blend alpha 0-255 (RV FrameObject blend colour, STORY_BLEND)
 *     <t> c <x> <y>                                  view top-left
 *     <t> say <line> <box frame>                     DialogBox shows paragraph <line> (DialogBox frame); voice played
 *     <t> close                                      DialogBox hidden
 *     <t> s <id> / <t> o <voice> / <t> u <bgm file|->   sound id / a voice clip outside the dialogue / music
 *     <t> f <frames> [1]                             fade: > 0 from black, < 0 to black (1: white instead of black)
 *     <t> fa <alpha> [1]                             the cover's alpha now (RV: the LevelCardFade objects' blend alpha)
 *     <t> fw <x>                                     the black wipe at x (as the pause rows' wipe: game_wipe_draw; -999 off)
 *     <t> chsel                                      the in-scene character select (frame 75: Lilac / Carol, sets g1)
 *     <t> cap <row> <key> / <t> capx                 caption row 0-3 from story/captions.txt / all captions off
 *     <t> pv <handle> <0|1>                          the room's parallax piece <handle> hidden / shown (par_set_hidden)
 *     <t> sub <key> <y> / <t> subx                   a subtitle (session 26: SubtitleData paragraph, caption key sub<h>_<k>)
 *                                                    in the box at y, word-wrapped, centred / subtitle off
 *     end <t>                                        the scene ends at time t of the last segment
 *     next <step>                                    (informative: the table's next wins) */
#ifndef FP_STORY_H
#define FP_STORY_H
#include "../fp.h"

#define STORY_MAX 64                 /* scenes 0..63; seen flags (8 bytes in the save) and the saved story position use the
                                      * scene's permanent number sid= (tables/story/sids.json, append-only), not its row */
#define STORY_ROWS 96                /* session 26: rows of story.txt (every port scene and character variant: 71 with groups
                                      * A-E; variants share their base's sid, so the rows outnumber the scene numbers) */
enum { STORY_TRIG_NEW, STORY_TRIG_END, STORY_TRIG_CONTINUE };

int  story_gate(void);               /* THE gate: 1 only in an Adventure run (fe_mode 2, not Time Attack, story run armed) */
void story_reset(void);              /* no run (menu, Time Attack, Classic, developer select): nothing of a run survives */
int  story_new_game(int character);  /* a new Adventure: the opening scene (frame 81 g4 2); 0 = started, -1 = no story data */
int  story_stage_end(int stage);     /* frame 8 after stage I in Adventure: a scene (1 = started) or 0 (the caller routes on) */
int  story_continue(int last_stage); /* continue an Adventure file: frame 8 with the file's stage (1 = a scene started) */
int  story_inline_begin(const char *name);   /* an in-stage scene over the loaded stage (next "back"); 0 = started */
int  story_inline_done(void);        /* 1 once after the in-stage scene ended (the stage goes on) */
int  story_epilogue(void);           /* ending.c, after the space scene: the Adventure epilogue (0 = started, -1 = not Adventure / no data) */
void story_update(const input_t *in);
void story_draw_layer(int pass);     /* game_draw: actors behind (0) / with the play layer (1) / in front (2) */
void story_draw_overlay(void);       /* DialogBox, pause menu, fades */
int  story_active(void);             /* MODE_STORY */
int  story_paused(void);
int  story_position(void);          /* the run's story position (-1: no Adventure run): written into the file at stage ends */
int  story_scene_id(const char *name);   /* -1 if unknown */
const char *story_scene_name(int id);
int  story_seen(int id);
void story_mark_seen(int id);
int  story_count(void);              /* scenes in story/story.txt */
#ifdef FP_HOST_ENV
void story_host_dump(void);          /* host "<f> STORY": state line */
int  story_host_play(const char *name);   /* host "<f> SCENE <id>": play a scene as if triggered (gate still applies) */
int  story_host_new(int character);      /* host "<f> ADVNEW <g1>": a new Adventure as the hub starts it */
int  story_host_stage_end(void);         /* host "<f> STAGEEND": frame 8 after the current stage (1 scene, 2 next stage) */
void story_host_drop(void);              /* host "<f> LOAD": a scene playing is left without its completion (the run stays) */
#endif
#endif
