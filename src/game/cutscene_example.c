/* The cutscene template (session 24): one documented example scene for src/game/cutscene.c. It is NOT part of the game
 * flow: only the host test command "<f> CUTSCENE" plays it (tests/test_ending.py), on the credits' UI scene (end/cred_86,
 * whose objects include the three playable skins), so it needs no data of its own.
 *
 * To add a real scene: copy this file, rename the table, write the cues (cutscene.h lists them), keep the text in the data
 * pipeline when it comes from the original (as tools/import_ending.py does for the credits) rather than in the source, and
 * start it from the state that owns the screen: cs_begin(&MY_SCENE, &its_ui_scene, &its_font); cs_update / cs_draw each
 * frame until cs_update returns 1. The text below is port placeholder text for the template only (never shown in play). */
#include "cutscene.h"

static const cs_cue_t EXAMPLE[] = {
    /*  t  op         slot   x    y   str                         arg */
    {   0, CS_FADE,     0,   0,   0, NULL,                        30 },          /* fade in from black over 30 frames */
    {   0, CS_SPRITE,   0, 150, 200, "playerskinsash",            37 << 8 | 1 }, /* slot 0: Lilac, anim 37 dir 1 */
    {  20, CS_SPRITE,   1, 280, 200, "playerskincarol",           37 << 8 | 1 }, /* slot 1: Carol, 20 frames later */
    {  10, CS_SAY,      0,   0,   0, "Lilac|(Template line one: a speaker, a line, then A to continue.)", 1 },
    {   0, CS_SFX,      0,   0,   0, NULL,                        64 },          /* the menu move sound */
    {   0, CS_SAY,      0,   0,   0, "Carol|(Template line two, typed two frames a character.)", 2 },
    {   0, CS_MOVE,     1, 320, 200, NULL,                        40 },          /* Carol walks right over 40 frames */
    {   0, CS_CLEAR,    0,   0,   0, NULL,                        0 },
    {   0, CS_WAIT,     0,   0,   0, NULL,                        40 },
    {   0, CS_HIDE,     1,   0,   0, NULL,                        0 },
    {   0, CS_FADE,     0,   0,   0, NULL,                        -30 },         /* fade out */
    {  30, CS_END,      0,   0,   0, NULL,                        0 },
};
const cs_scene_t CS_EXAMPLE = { "example", EXAMPLE, 1 };
