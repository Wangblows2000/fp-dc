/* Hub frame (original frame index 3 = Frames::on_frame_4, a 1440x480 canvas): title menu, file select,
 * mode select and options, ported from the original's events (RE numbers below; build/re/fe/spec_*.md and
 * RE_NOTES.md "Front end"). The event groups of the original become the flags in `H`; the per-frame
 * order is the original's: fade-in, title, file {scroll down, rows, mode select, scroll up, display},
 * delete, options, leave, then the row shake (RE 328/329). The original builds its "pressed" flags at the
 * END of a frame (events 82-137), so every menu event reacts one frame after the press: the hub uses the
 * previous frame's input.
 * Console layout: the original has a built-in console variant (global 32 = 1, RE 380-382 / 466-479 /
 * 606) that hides QUIT and the PC-only option rows (screen size, key config, stick sensitivity); the port
 * uses it (DECISIONS D-020). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../fp.h"
#include "ui.h"

void fe_text(float x, float y, const char *s);
extern int fe_difficulty, fe_music_volume, fe_letterbox;

#define CONSOLE 1                      /* global 32 */
#define CANVAS_W 1440
#define CANVAS_H 480

static ui_scene_t *S;
static struct {
    int fadein, fadeA, timer;          /* RE 369/370; timer = frames since the fade-in ended (0.2 s guard) */
    int title, file, sdown, rows, modesel, sup, display, del, opt, opt_in, opt_rows, opt_out, leave;
    int opt_once;
    float sdx, sdy;                    /* ScrollDot_3: the camera follows it */
    int cur, moved, confirm;           /* CrystalCursor_65.A, MOVED latch (bit 3), CONFIRM latch (MainOption1 bit 0) */
    int slot;                          /* CrystalCursor_65.M */
    float fbI, fbJ;                    /* FileBack_95.I / J: carousel offset and remaining scroll */
    int startA, ready, leaving, mode_once;
    int mark[SAVE_FILES];              /* FileSaveSlot J: marked for deletion */
    float wipe_x; int wipe_F, exit;    /* Fade_50 x / F; exit code */
    int cursor_vis;
    float cux, cuy;                    /* cursor position (canvas) */
    input_t prev;                      /* input latched at the end of the previous frame */
} H;

static ui_obj_t *mo[5], *cur, *ftext, *fid, *fdiff, *fdiff2, *fret, *fstart, *fcopy, *fback;
static ui_obj_t *odiff3, *odiff4, *omus, *omusv, *ookay, *octrl, *osens, *osensv, *oscr, *oscrv, *oact, *oact2;
static ui_obj_t *slots[SAVE_FILES], *chars[SAVE_FILES], *finfo[12];
static int nslots, nchars;

static const char *HINT[6] = {         /* FileMessageList_129 paragraphs (RE: Text constructor) */
    "Press START to begin a new game.", "This file will be deleted when you select RETURN. Press START to cancel.",
    "ADVENTURE MODE mixes story and gameplay.", "CLASSIC MODE plays every stage in order.",
    "Select RETURN to pick another file slot.", "Press LEFT or RIGHT to adjust the DIFFICULTY." };

static float camx(void) { float c = H.sdx - 213; return c < 0 ? 0 : c > CANVAS_W - SCREEN_W ? CANVAS_W - SCREEN_W : c; }
static float camy(void) { float c = H.sdy - 120; return c < 0 ? 0 : c > CANVAS_H - SCREEN_H ? CANVAS_H - SCREEN_H : c; }
static void shake(ui_obj_t *o) { if (o) o->shz = 4; }
static void place_cursor(ui_obj_t *o) { float x, y; if (o && ui_action_point(o, &x, &y) == 0) { H.cux = x; H.cuy = y; } }
static int sel_used(void) { return SV.file[H.slot].used; }          /* "FileStageIcon visible" */

static void wipe_start(void) { H.wipe_x = camx() - SCREEN_W - 120; H.wipe_F = 0; H.leave = 1; }

void hub_bind(struct ui_scene_s *s) {
    S = s;
    static const char *mn[5] = { "mainoption1", "mainoption2", "mainoption5", "mainoption3", "mainoption4" };
    for (int i = 0; i < 5; i++) mo[i] = ui_find(s, mn[i], 0);
    cur = ui_find(s, "crystalcursor", 0);
    ftext = ui_find(s, "filetext", 0); fid = ui_find(s, "fileid", 0); fdiff = ui_find(s, "difficulty", 0);
    fdiff2 = ui_find(s, "difficulty2", 0); fret = ui_find(s, "return", 0); fstart = ui_find(s, "start", 0);
    fcopy = ui_find(s, "copy", 0); fback = ui_find(s, "fileback", 0);
    odiff3 = ui_find(s, "difficulty3", 0); odiff4 = ui_find(s, "difficulty4", 0);
    omus = ui_find(s, "musicvolume", 0); omusv = ui_find(s, "musicvolume", 1); ookay = ui_find(s, "okay", 0);
    octrl = ui_find(s, "controls", 0); osens = ui_find(s, "sensitivity", 0); osensv = ui_find(s, "controllersensitivity", 0);
    oscr = ui_find(s, "screensize", 0); oscrv = ui_find(s, "screenres", 0); oact = ui_find(s, "active", 0); oact2 = ui_find(s, "active2", 0);
    nslots = nchars = 0;
    for (int k = 0; k < SAVE_FILES && (slots[k] = ui_find(s, "filesaveslot", k)); k++) nslots++;
    for (int k = 0; k < SAVE_FILES && (chars[k] = ui_find(s, "filesaveslotcharacter", k)); k++) nchars++;
    static const char *info[12] = { "filelabel", "filelabel2", "filestageicon", "fileicon", "filecurrentstage", "filelives",
                                    "filetimemilli", "filedeaths", "filecrystals", "filetimes", "filetimem", 0 };
    for (int i = 0; info[i]; i++) finfo[i] = ui_find(s, info[i], 0);
}

/* frame start (on_frame_4_start_events: RE 602, 604, 606) */
void hub_enter(void) {
    memset(&H, 0, sizeof H);
    H.fadein = 1; H.sdx = 256; H.sdy = 120; H.fbI = 480; H.cursor_vis = 1;
    H.rows = 0; H.sdown = 1; H.display = 1; H.opt_in = 1;            /* initial child flags (on_frame_4_init) */
    for (int i = 0; i < S->nobj; i++) { S->o[i].shz = 0; S->o[i].shx = S->o[i].shy = 0; S->o[i].shhome = 0; S->o[i].dx = S->o[i].dy = 0; S->o[i].vis = 1; }
    /* hidden at start / driven by events */
    static const char *hide[] = { "warningbox", "warningmessage", "filesaveslotcopy", "filesaveslotcharactercopy", "copyarrow",
                                  "yes", "mouseobject", "livessetting", "scrolldot", "filemessage", "fade", "fadeedge", "particle",
                                  "type", "iconkeyboard", "iconjoypad", "controlbox", "controlboxname", "buttonname",
                                  "controlnamelist", "okay2", 0 };
    ui_obj_t *o;
    for (int i = 0; hide[i]; i++) for (int k = 0; (o = ui_find(S, hide[i], k)); k++) o->vis = 0;
    if (fcopy) fcopy->vis = 0;
    if (fdiff) fdiff->vis = 0;                       /* the label sits under the value; only the value shows (RV) */
    if (CONSOLE) {                                   /* RE 606 */
        for (int i = 0; i < 4; i++) if (mo[i]) mo[i]->dy = 12;
        if (mo[4]) mo[4]->vis = 0;
        ui_obj_t *h2[] = { octrl, osens, oact, oscrv, osensv, oact2 };
        for (int i = 0; i < 6; i++) if (h2[i]) h2[i]->vis = 0;
        if (oscr) oscr->dy = 4;                     /* port: SCREEN SIZE row between difficulty (88) and music (128) */
        if (odiff3) odiff3->dy = 16;
        if (odiff4) odiff4->dy = 16;
        if (ookay) ookay->dy = -32;
    }
    if (fback) ui_obj_frame(fback, 0);
    if (fstart) fstart->dy = 0;
    music_volume(fe_music_volume);
    music_play("bgm/m_menu2.adp");                   /* RE 602 (RV: m_menu2 on entering frame 3) */
}

/* lives per difficulty (RE 410-413 / 488-491) */
static void difficulty_apply(void) {
    if (fe_difficulty < 0) fe_difficulty = 3;
    if (fe_difficulty > 3) fe_difficulty = 0;
    if (fdiff2) ui_obj_select(S, fdiff2, 0, fe_difficulty);
    if (odiff4) ui_obj_select(S, odiff4, 0, fe_difficulty);
}
int hub_lives_for_difficulty(int d) { return d == 0 ? 3 : d == 3 ? 7 : 5; }

static void title_events(const input_t *in) {
    uint32_t p = in->pressed;
    ui_obj_t *row = mo[H.cur < 0 ? 0 : H.cur > 4 ? 4 : H.cur];
    place_cursor(row);                                                   /* RE 379 */
    if ((p & BTN_UP) && !H.moved && H.cur == 0) { H.cur = CONSOLE ? 3 : 4; snd_play_orig(64, 220, 128); H.moved = 1; }   /* 380/381 */
    if ((p & BTN_DOWN) && H.cur == 3 && !H.moved && CONSOLE) { H.cur = 0; snd_play_orig(64, 220, 128); H.moved = 1; }   /* 382 */
    if ((p & BTN_UP) && !H.moved && H.cur > 0) { H.cur--; snd_play_orig(64, 220, 128); H.moved = 1; }                    /* 383 */
    if ((p & BTN_DOWN) && !H.moved && H.cur < 4) { H.cur++; snd_play_orig(64, 220, 128); H.moved = 1; }                  /* 384 */
    if ((p & BTN_DOWN) && !H.moved && H.cur == 4) { H.cur = 0; snd_play_orig(64, 220, 128); H.moved = 1; }               /* 385 */
    if (H.moved) { H.moved = 0; shake(mo[H.cur]); }                                                                      /* 386 */
    if ((p & (BTN_JUMP | BTN_SPECIAL | BTN_START)) && H.timer >= 12) H.confirm = 1;                                    /* 387: 0.2 s */
    if (!H.confirm) return;
    switch (H.cur) {
    case 0:                                                              /* 388 START GAME */
        H.confirm = 0; H.title = 0; H.file = 1; H.sdown = 1; H.display = 1;
        snd_play_orig(63, 230, 128);
        for (int i = 0; i < SAVE_FILES; i++) H.mark[i] = 0;
        break;
    case 1:                                                              /* 389 TIME ATTACK */
        H.confirm = 0; H.title = 0; snd_play_orig(56, 230, 128); wipe_start(); music_stop(); H.exit = HUB_EXIT_TIME_ATTACK;
        break;
    case 2:                                                              /* 390 GALLERY -> frame 10 (fe_gallery.inc): the hub wipe, sound 56 */
        H.confirm = 0; H.title = 0; snd_play_orig(56, 230, 128); wipe_start(); music_stop(); H.exit = HUB_EXIT_GALLERY;
        break;
    case 3:                                                              /* 391 OPTIONS */
        H.confirm = 0; snd_play_orig(63, 230, 128); H.title = 0; H.opt = 1; H.opt_in = 1; H.opt_once = 1;
        break;
    case 4:                                                              /* 392 QUIT */
        H.confirm = 0; H.title = 0; snd_play_orig(56, 230, 128); wipe_start(); music_stop(); H.exit = HUB_EXIT_QUIT;
        break;
    }
}

static ui_obj_t *file_row(int a) {               /* RE 456-461 */
    switch (a) { case 0: return ftext; case 1: return fdiff; case 2: return fret; case 3: return fstart; case 4: return fcopy; default: return fret; }
}

static void file_events(const input_t *in) {
    uint32_t p = in->pressed, h = in->held;
    int jumpish = p & (BTN_JUMP | BTN_SPECIAL | BTN_START), back = p & BTN_ATTACK;
    if (H.sdown) {                                                       /* 393/394 */
        H.sdy = H.sdy + 24 > 360 ? 360 : H.sdy + 24;
        H.cur = 0; if (fcopy) fcopy->vis = 0;
        if (H.sdy >= 360) { H.rows = 1; H.sdown = 0; }
    }
    if (H.rows) {
        if (H.cur == 0 && (h & BTN_LEFT) && H.fbJ == 0) { H.fbJ = 120; H.slot = (H.slot + 7) % 8; snd_play_orig(64, 220, 128); H.moved = 1; }    /* 396 */
        if (H.cur == 0 && (h & BTN_RIGHT) && H.fbJ == 0) { H.fbJ = -120; H.slot = (H.slot + 1) % 8; snd_play_orig(64, 220, 128); H.moved = 1; }  /* 397 */
        if ((p & BTN_UP) && H.fbJ == 0) { snd_play_orig(64, 220, 128); H.moved = 1; H.cur = (H.cur + 2) % 3; }                                    /* 398 */
        if ((p & BTN_DOWN) && H.fbJ == 0) { snd_play_orig(64, 220, 128); H.moved = 1; H.cur = (H.cur + 1) % 3; }                                  /* 399 */
        if (H.moved && H.cur <= 2) { H.moved = 0; if (H.cur == 0) shake(ftext); else if (H.cur == 1) { shake(fdiff); shake(fdiff2); } else shake(fret); }  /* 400-402 */
        if (jumpish) H.confirm = 1;                                                                                                              /* 403 */
        if (H.confirm && H.fbJ == 0 && H.cur == 0 && !H.mark[H.slot]) {                                                                         /* 404 */
            snd_play_orig(63, 230, 128); H.rows = 0; H.modesel = 1; H.cursor_vis = 0; H.confirm = 0; H.cur = 3; H.mode_once = 1;
        } else if (H.confirm && H.fbJ == 0 && H.cur == 0 && H.mark[H.slot]) {                                                                   /* 405 */
            snd_play_orig(63, 230, 128); H.confirm = 0; for (int i = 0; i < SAVE_FILES; i++) H.mark[i] = 0;
        }
        if (H.cur == 1 && (p & BTN_LEFT)) { fe_difficulty++; snd_play_orig(64, 220, 128); }                                                     /* 406 */
        if (H.cur == 1 && (p & BTN_RIGHT)) { fe_difficulty--; snd_play_orig(64, 220, 128); }                                                    /* 407 */
        if (H.confirm && H.cur == 1) { H.confirm = 0; snd_play_orig(64, 220, 128); fe_difficulty--; }                                           /* 408 */
        difficulty_apply();                                                                                                                      /* 409-414 */
        if ((H.confirm || back) && H.fbJ == 0 && H.cur == 2) {                                                                                  /* 415 RETURN */
            H.confirm = 0; H.cur = 0; snd_play_orig(63, 230, 128); H.rows = 0; H.display = 0; H.sup = 1;
            place_cursor(mo[3]);
        } else if (back && H.cur != 2 && H.rows) { H.cur = 2; snd_play_orig(64, 220, 128); H.moved = 1; }                                       /* 416 */
    }
    if (H.modesel) {
        int used = sel_used();
        if (H.mode_once) {                                               /* 419/420 */
            H.mode_once = 0; if (fstart) fstart->dy = 0;
            if (fcopy) fcopy->vis = 1;
            if (fstart) { ui_obj_select(S, fstart, 0, 0); ui_obj_frame(fstart, used ? 0 : 1); }
            if (fcopy) { ui_obj_select(S, fcopy, 0, 0); ui_obj_frame(fcopy, used ? 0 : 1); }
        }
        if (!H.leaving && H.startA < 300) {                              /* 417: slide in */
            H.startA += 30;
            ui_obj_t *sl[] = { fstart, fcopy, ftext, fdiff, fdiff2 };
            for (int i = 0; i < 5; i++) if (sl[i]) sl[i]->dx -= 30;
        }
        if (!H.ready && !H.leaving && H.startA >= 300) {                 /* 418 */
            H.ready = 1; H.cursor_vis = 1;
            if (fret) { if (fstart) { fstart->dx = fret->x - fstart->x; fstart->shhome = 0; } if (fcopy) { fcopy->dx = fret->x - fcopy->x; fcopy->shhome = 0; } }
        }
        if (H.ready && !H.leaving) {
            if ((p & BTN_UP) && !H.moved && H.cur == 3) { H.cur = 5; snd_play_orig(64, 220, 128); H.moved = 1; }      /* 421 */
            if ((p & BTN_UP) && !H.moved && H.cur > 3) { H.cur--; snd_play_orig(64, 220, 128); H.moved = 1; }         /* 422 */
            if ((p & BTN_DOWN) && !H.moved && H.cur < 5) { H.cur++; snd_play_orig(64, 220, 128); H.moved = 1; }       /* 423 */
            if ((p & BTN_DOWN) && !H.moved && H.cur == 5) { H.cur = 3; snd_play_orig(64, 220, 128); H.moved = 1; }    /* 424 */
        }
        if (H.moved && H.cur >= 3) { H.moved = 0; shake(H.cur == 3 ? fstart : H.cur == 4 ? fcopy : fret); }        /* 425-427 */
        if (H.ready && jumpish) H.confirm = 1;                                                                        /* 428 */
        if (H.confirm && (H.cur == 3 || (H.cur == 4 && !used))) {                                                    /* 429/430 */
            H.confirm = 0; H.ready = 0; H.leaving = 1; H.modesel = 0; H.display = 0;
            snd_play_orig(56, 230, 128); music_stop(); wipe_start();
            if (used) H.exit = HUB_EXIT_CONTINUE;
            else H.exit = H.cur == 3 ? HUB_EXIT_NEW_ADVENTURE : HUB_EXIT_NEW_CLASSIC;
            H.cur = 3;
        } else if (H.confirm && H.cur == 4 && used) {                                                                 /* 431 DELETE */
            snd_play_orig(63, 230, 128); H.cursor_vis = 0; H.confirm = 0; H.cur = 0; H.leaving = 1; H.mark[H.slot] = 1;
        } else if ((H.confirm && H.cur == 5) || (back && H.ready && H.cur == 5)) {                                   /* 432 RETURN */
            snd_play_orig(63, 230, 128); H.cursor_vis = 0; H.confirm = 0; H.cur = 0; H.leaving = 1;
        } else if (back && H.ready && H.cur != 5) { H.cur = 5; snd_play_orig(64, 220, 128); H.moved = 1; }            /* 433 */
        if (H.leaving && H.startA > 0 && !H.leave) {                                                                  /* 434: slide out */
            H.startA -= 30;
            ui_obj_t *sl[] = { fstart, fcopy, ftext, fdiff, fdiff2 };
            for (int i = 0; i < 5; i++) if (sl[i]) sl[i]->dx += 30;
        }
        if (H.leaving && H.startA <= 0 && !H.leave) { H.leaving = 0; H.cursor_vis = 1; H.modesel = 0; H.rows = 1; H.ready = 0;
            if (fstart) fstart->dx = 0;
            if (fcopy) { fcopy->dx = 0; fcopy->vis = 0; } }                                                            /* 435 */
    }
    if (H.sup) {                                                         /* 436/437 */
        H.sdy = H.sdy - 24 < 120 ? 120 : H.sdy - 24;
        if (H.sdy <= 120) { H.title = 1; H.sup = 0; H.file = 0; H.del = 1; H.display = 1; H.sdown = 1; }
    }
    if (H.display) {                                                     /* 438-461 */
        if (H.fbJ > 0) { H.fbI = fmodf(H.fbI + 10, 960); H.fbJ -= 10; }                                               /* 452 */
        if (H.fbJ < 0) { H.fbI = fmodf(H.fbI + 950, 960); H.fbJ += 10; }                                              /* 453 */
        place_cursor(file_row(H.cur));
    }
}

static void delete_events(void) {                /* RE 462/463: marked files are erased on leaving through RETURN */
    if (!H.del) return;
    int any = 0;
    for (int i = 0; i < SAVE_FILES; i++) if (H.mark[i]) { memset(&SV.file[i], 0, sizeof SV.file[i]); any = 1; }
    if (any) save_store();
    for (int i = 0; i < SAVE_FILES; i++) H.mark[i] = 0;
    H.del = 0;
}

static ui_obj_t *opt_row(int a) {                /* RE 506-511 */
    switch (a) { case 0: return odiff3; case 1: return oscr; case 2: return omus; case 3: return octrl; case 4: return osens; default: return ookay; }
}

static void options_events(const input_t *in) {
    uint32_t p = in->pressed;
    int jumpish = p & (BTN_JUMP | BTN_SPECIAL | BTN_START), back = p & BTN_ATTACK;
    if (H.opt_once) { H.opt_once = 0; H.cur = 0; }                      /* 505 (once) */
    if (H.opt_in) {                                                      /* 464/465 */
        H.sdx = H.sdx + 30 > 736 ? 736 : H.sdx + 30;
        if (H.sdx >= 735) { H.opt_rows = 1; H.opt_in = 0; }
    }
    if (H.opt_rows) {

        if ((p & BTN_UP) && !H.moved && H.cur == 0) { H.cur = 5; snd_play_orig(64, 220, 128); H.moved = 1; }        /* 466 */
        if ((p & BTN_UP) && !H.moved && H.cur > 0) { H.cur--; snd_play_orig(64, 220, 128); H.moved = 1; }           /* 467 */
        if ((p & BTN_DOWN) && !H.moved && H.cur < 5) { H.cur++; snd_play_orig(64, 220, 128); H.moved = 1; }         /* 468 */
        if ((p & BTN_DOWN) && !H.moved && H.cur == 5) { H.cur = 0; snd_play_orig(64, 220, 128); H.moved = 1; }      /* 469 */
        if (H.moved && CONSOLE) {                                        /* 471-477: rows 3, 4 skipped */
            /* port: row 1 (the original's SCREEN SIZE, hidden in its console layout) is kept for the
             * 16:9 letterbox setting (D-025), so 472/473 no longer skip it */
            if (H.cur == 3) H.cur = 5;                                   /* 476 */
            if (H.cur == 4) H.cur = 2;                                   /* 477 */
        }
        if (H.moved) { H.moved = 0; shake(opt_row(H.cur)); }            /* 470-479 */
        if (jumpish) H.confirm = 1;                                      /* 480 */
        if (H.cur == 0 && (p & BTN_LEFT)) { fe_difficulty++; snd_play_orig(64, 220, 128); }                         /* 484 */
        if (H.cur == 0 && (p & BTN_RIGHT)) { fe_difficulty--; snd_play_orig(64, 220, 128); }                        /* 485 */
        if (H.confirm && H.cur == 0) { H.confirm = 0; fe_difficulty--; snd_play_orig(64, 220, 128); }               /* 486 */
        difficulty_apply();                                                                                          /* 487-492 */
        if (H.cur == 2 && (p & BTN_LEFT)) { fe_music_volume = (fe_music_volume + 100) % 110; music_volume(fe_music_volume); snd_play_orig(64, 220, 128); }   /* 493 */
        if ((H.cur == 2 && (p & BTN_RIGHT)) || (H.confirm && H.cur == 2)) {                                          /* 494 */
            H.confirm = 0; fe_music_volume = (fe_music_volume + 10) % 110; music_volume(fe_music_volume); snd_play_orig(64, 220, 128);
        }
        if (H.cur == 1 && ((p & (BTN_LEFT | BTN_RIGHT)) || H.confirm)) {   /* port: SCREEN SIZE = 16:9 letterbox on/off */
            H.confirm = 0; fe_letterbox = !fe_letterbox; r_set_letterbox(fe_letterbox); snd_play_orig(64, 220, 128);
        }
        if ((H.confirm || back) && H.cur == 5) {                         /* 501 OKAY: save */
            H.confirm = 0; H.cur = 3; snd_play_orig(63, 230, 128); H.opt_rows = 0; H.opt_out = 1;
            SV.difficulty = (uint8_t)fe_difficulty; SV.music_volume = (uint8_t)fe_music_volume; SV.letterbox = (uint8_t)fe_letterbox; save_store();
        } else if (back && H.cur < 5) { H.cur = 5; snd_play_orig(64, 220, 128); H.moved = 1; }                       /* 502 */
        if (H.confirm && H.cur != 0 && H.cur != 1 && H.cur != 2 && H.cur != 5) H.confirm = 0;
    }
    if (H.opt_out) {                                                     /* 503/504 */
        H.sdx = H.sdx - 30 < 256 ? 256 : H.sdx - 30;
        if (H.sdx <= 256) { H.title = 1; H.opt_out = 0; H.opt = 0; H.opt_in = 1; }
    }
    if (H.opt) place_cursor(opt_row(H.cur));                            /* 505-511 */
}

/* returns an exit code (HUB_EXIT_*) once the leaving fade has run 50 frames, else 0 */
int hub_update(const input_t *now) {
    const input_t *in = &H.prev;                 /* menu events see the previous frame's presses (RE order) */
    if (H.fadein) {                                                      /* 369/370 */
        H.fadeA++;
        if (H.fadeA >= 32) { H.fadein = 0; H.timer = 0; H.title = 1; }
    } else H.timer++;
    if (H.title) title_events(in);
    if (H.file) file_events(in);
    delete_events();
    if (H.opt) options_events(in);
    int ex = 0;
    if (H.leave) {                                                       /* 577 */
        H.wipe_x = H.wipe_x + 32 > camx() ? camx() : H.wipe_x + 32;
        if (++H.wipe_F > 50) ex = H.exit;                                /* 578/579/583-584 */
    }
    for (int i = 0; i < S->nobj; i++) {                                  /* 328/329 row shake */
        ui_obj_t *o = &S->o[i];
        if (o->shz > 0) {
            double a = 200.0 * o->shz;
            double c = (a == 90. || a == 270.) ? 0. : cos(a / 57.29577951308232), s = a == 180. ? 0. : sin(a / 57.29577951308232);
            o->shx = (int)floor(c * o->shz); o->shy = (int)floor(s * o->shz * 0.5);
            o->shz -= 0.4f; if (o->shz < 0.01f) { o->shz = 0; o->shx = o->shy = 0; }
        }
    }
    H.prev = *now;
    ui_tick(S);
    return ex;
}

/* Fusion counters are anchored at their bottom-right corner (RV: MusicVolume_93 at (874,136), digits end
 * at x 874, bottom 136) */
static void counter_br(ui_obj_t *o, int value, float cx, float cy, int mindigits) {
    if (!o || !o->vis) return;
    char buf[12]; snprintf(buf, sizeof buf, "%0*d", mindigits, value < 0 ? 0 : value);
    int n = (int)strlen(buf);
    float x = o->x + o->dx + o->shx - cx, y = o->y + o->dy + o->shy - cy;
    for (int i = n - 1; i >= 0; i--) {
        ui_obj_frame(o, buf[i] - '0');
        const frame_t *f = anim_frame(&o->an);
        if (!f) return;
        x -= f->w;
        frame_draw(o->an.b, f, x + f->hx, y - f->h + f->hy, 0, 0xffffffff);
    }
}

/* port: SCREEN SIZE value "16:9" / "4:3" in the music-volume digits, right-aligned like the volume value;
 * the colon (not in the digit set) is two 2x2 dots in the digits' yellow with their dark edge */
static void ratio_value(ui_obj_t *o, float rx, float y, int wide, float cx, float cy) {
    if (!o) return;
    const char *s = wide ? "16:9" : "4:3";
    int vis = o->vis; o->vis = 1;
    float x = rx - cx;
    for (int i = (int)strlen(s) - 1; i >= 0; i--) {
        if (s[i] == ':') {
            x -= 5;
            for (int k = 0; k < 2; k++) {
                float dy = y - cy - (k ? 5 : 11);
                r_rect(x + 1, dy, 4, 4, 0xff281800);
                r_rect(x + 1, dy, 3, 3, 0xfff8f800);
            }
            continue;
        }
        ui_obj_frame(o, s[i] - '0');
        const frame_t *f = anim_frame(&o->an);
        if (!f) break;
        x -= f->w;
        frame_draw(o->an.b, f, x + f->hx, y - cy - f->h + f->hy, 0, 0xffffffff);
    }
    o->vis = vis;
}

static void wrap_text(float x, float y, const char *m) {   /* TextBlitter box: 26 columns (RV), 20 px lines */
    char line[40];
    while (*m) {
        size_t n = strlen(m), cut = n <= 26 ? n : 26;
        if (n > 26) while (cut > 0 && m[cut] != ' ') cut--;
        if (cut == 0) cut = n < 26 ? n : 26;
        memcpy(line, m, cut); line[cut] = 0;
        fe_text(x, y, line);
        m += cut; while (*m == ' ') m++;
        y += 20;
    }
}

void hub_draw(void) {
    float cx = camx(), cy = camy();
    /* carousel (RE 447/448/455) */
    for (int k = 0; k < nslots; k++) {
        ui_obj_t *sl = slots[k], *ch = k < nchars ? chars[k] : NULL;
        float x = fmodf(k * 120.f + H.fbI, 960.f) + (fback ? fback->x : 256) - 480.f;
        sl->dx = x - sl->x; sl->dy = 344 - sl->y;
        ui_obj_select(S, sl, 0, k == H.slot ? 1 : 0);
        if (ch) {
            ch->dx = sl->x + sl->dx - ch->x; ch->dy = sl->y + sl->dy - ch->y;
            ui_obj_select(S, ch, 0, SV.file[k].used ? SV.file[k].character + 1 : 0);
        }
    }
    ui_obj_t *mv = omusv, *fidc = fid;
    if (mv) mv->vis = 0;
    if (fidc) fidc->vis = 0;
    int used = sel_used() && !H.mark[H.slot];
    for (int i = 0; i < 11; i++) if (finfo[i]) finfo[i]->vis = 0;
    if (cur) cur->vis = 0;
    ui_draw(S, 0, 0, 0, 1);          /* background layer: fixed to the view (RV: same sky/sea in every panel) */
    ui_draw(S, cx, cy, 2, 3);
    for (int k = 0; k < nchars; k++) if (H.mark[k] && chars[k]) { chars[k]->vis = 1; ui_draw_obj(S, chars[k], cx, cy, 0x80ffffff); }   /* 449 */
    /* FILE n (FileID counter at FileText + (30, 9)) */
    if (fidc && ftext) { fidc->vis = 1; fidc->dx = ftext->x + ftext->dx + ftext->shx + 30 - fidc->x; fidc->dy = ftext->y + ftext->dy + ftext->shy + 9 - fidc->y;
        counter_br(fidc, H.slot + 1, cx, cy, 1); fidc->vis = 0; }
    /* info for an existing save (RE 444/455) or the hint text (RE 438-446) */
    if (used) {
        const save_file_t *f = &SV.file[H.slot];
        for (int i = 0; i < 11; i++) if (finfo[i]) finfo[i]->vis = 1;
        if (finfo[3]) ui_obj_select(S, finfo[3], 0, f->character);
        if (finfo[2]) ui_obj_frame(finfo[2], f->last_stage < SAVE_STAGES ? f->last_stage : 0);   /* FileStageIcon = y5 (last stage s) */
        if (finfo[0]) ui_obj_frame(finfo[0], f->mode);
        for (int i = 0; i < 5; i++) if (finfo[i]) ui_draw_obj(S, finfo[i], cx, cy, 0xffffffff);
        uint32_t ms = save_file_time(f);                                                   /* totals = sums of the stage blocks (RE 954) */
        counter_br(finfo[5], f->lives, cx, cy, 1); counter_br(finfo[7], save_file_deaths(f), cx, cy, 1); counter_br(finfo[8], save_file_crystals(f), cx, cy, 1);
        counter_br(finfo[6], (ms / 10) % 100, cx, cy, 2); counter_br(finfo[9], (ms / 1000) % 60, cx, cy, 2); counter_br(finfo[10], ms / 60000, cx, cy, 2);
    } else if (H.file || H.sup) {
        int k = -1;
        if (SV.file[H.slot].used && H.mark[H.slot]) k = 1;
        else if (H.cur == 0) k = 0; else if (H.cur == 1) k = 5; else if (H.cur == 3) k = 2; else if (H.cur == 4) k = 3;
        if (k >= 0) wrap_text(144 - cx, 264 - cy, HINT[k]);
    }
    /* option values (RE 494 / console layout: difficulty + music) */
    if (mv) { mv->vis = 1; counter_br(mv, fe_music_volume, cx, cy, 1); mv->vis = 0; }
    if (mv && oscr && oscr->vis) ratio_value(mv, mv->x + mv->dx + mv->shx, oscr->y + oscr->dy + oscr->shy + 8, fe_letterbox, cx, cy);
    if (cur && H.cursor_vis && !H.leave) {
        cur->vis = 1; cur->dx = H.cux - cur->x; cur->dy = H.cuy - cur->y;
        ui_draw_obj(S, cur, cx, cy, 0xffffffff); cur->vis = 0;
    }
    if (H.fadein) {                                                      /* 369: alpha = clamp((128 - 4A) * 2) */
        int a = 4 * H.fadeA > 128 ? 0 : (128 - 4 * H.fadeA) * 2; if (a > 255) a = 255;
        if (a > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)a << 24) | 0x080000);
    }
    if (H.leave) {                                                       /* Fade_50 + FadeEdge_77 */
        float x = H.wipe_x - cx;
        if (x + SCREEN_W > 0) r_rect(0, 0, x + SCREEN_W, SCREEN_H, 0xff080000);
        for (int y = 0; y < SCREEN_H; y += 2) {
            float w = 120.f * (1.f - (float)y / SCREEN_H), x0 = x + SCREEN_W;
            if (x0 < 0) { w += x0; x0 = 0; }
            if (w > 0) r_rect(x0, (float)y, w, 2, 0xff080000);
        }
    }
}

int hub_selected_file(void) { return H.slot; }
int hub_vmu_file_char(void) {                       /* VMU: the highlighted used file's character on the file select, else -1 */
    if (!H.file || H.title || H.slot < 0 || H.slot >= SAVE_FILES || !SV.file[H.slot].used) return -1;
    return SV.file[H.slot].character;
}
int hub_cursor(void) { return H.cur; }
void hub_camera(float *x, float *y) { *x = camx(); *y = camy(); }
int hub_on_title(void) { return H.title || H.fadein; }
