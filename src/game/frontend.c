/* Front end built from the ORIGINAL menu frames (tools/import_ui.py):
 *   title_3 = frame index 2 ("Press Start"), title_4 = index 3 (title menu / file select / options /
 *   controls on one 1440x480 canvas, camera pans between panels), chsel_7 = index 6 (character select),
 *   stsel_12 = index 11 (Time Attack stage select).
 * Camera offsets, cursor positions and strings were measured from / read out of the running original
 * (RUNTIME_MAP "Front end"). Where the original's behaviour was not observed the code says INFERRED. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "../fp.h"
#include "ui.h"

enum { FE_PRESS, FE_MENU, FE_FILE, FE_OPTIONS, FE_CHSEL, FE_STSEL, FE_OUT };

static ui_banks_t b_title, b_chsel, b_stsel, b_font;
static ui_scene_t s_press, s_hub, s_chsel, s_stsel;
static int fe_state, fe_t, fe_sel, fe_file, fe_loaded, fe_lt_t, fe_rt_t;
static float cam_x, cam_y, cam_tx, cam_ty;
static int fe_go_time_attack;
static const char *fe_next_level;

/* settings (RV defaults: fresh install shows Difficulty NORMAL, Music Volume 100%, Sensitivity 70%) */
int fe_difficulty = 1;           /* difficulty2/4 directions: 0 HARD, 1 NORMAL, 2 EASY, 3 CASUAL */
int fe_music_volume = 100;
int fe_sensitivity = 70;

/* original menu sound IDs (Media::play_id, traced: RUNTIME_MAP "Sound IDs") */
#define SND_START   100   /* "Press Start" */
#define SND_MOVE     64   /* cursor move */
#define SND_SELECT   63   /* confirm */
#define SND_BEGIN    56   /* begin game (heard on START GAME / file confirm; exact role UNVERIFIED) */

/* ---- original font (Data/font.bmp, 8x20 cells) */
void fe_text(float x, float y, const char *s) {
    if (!b_font.n) return;
    const bank_t *b = &b_font.bank[0];
    for (; *s; s++, x += 8) {
        int c = (unsigned char)*s - 32;
        if (c <= 0 || c >= b->nframes) continue;
        frame_draw(b, &b->frames[c], x, y, 0, 0xffffffff);
    }
}

static void cursor_to(ui_scene_t *s, ui_obj_t *cur, float sx, float sy) {
    /* positions measured on screen in the original; convert to scene coordinates */
    if (!cur) return;
    cur->dx = sx + cam_x - cur->x; cur->dy = sy + cam_y - cur->y;
    (void)s;
}

static int fe_load(void) {
    if (fe_loaded) return 0;
    int bad = 0;
    bad |= ui_banks_load(&b_title, "title", 1) < 0;
    bad |= ui_banks_load(&b_chsel, "chsel", 1) < 0;
    bad |= ui_banks_load(&b_stsel, "stsel", 1) < 0;
    bad |= ui_banks_load(&b_font, "font_", 0) < 0;
    if (bank_load(&b_font.bank[0], "ui/font") == 0) b_font.n = 1; else bad = 1;
    bad |= ui_load(&s_press, "title_3", &b_title) < 0;
    bad |= ui_load(&s_hub, "title_4", &b_title) < 0;
    bad |= ui_load(&s_chsel, "chsel_7", &b_chsel) < 0;
    bad |= ui_load(&s_stsel, "stsel_12", &b_stsel) < 0;
    if (bad) { plat_log("front end data missing\n"); return -1; }
    fe_loaded = 1;
    return 0;
}

static void fe_unload(void) {
    if (!fe_loaded) return;
    ui_free(&s_press); ui_free(&s_hub); ui_free(&s_chsel); ui_free(&s_stsel);
    ui_banks_free(&b_title); ui_banks_free(&b_chsel); ui_banks_free(&b_stsel); ui_banks_free(&b_font);
    r_reset();
    fe_loaded = 0;
}

static void set_state(int s) { fe_state = s; fe_t = 0; fe_sel = 0; }
static void title_start(void);
static void hub_prepare(void);

int fe_enter(void) {
    game_unload_all();
    if (fe_load() < 0) return -1;
    G.mode = MODE_FRONTEND;
    set_state(FE_PRESS);
    cam_x = cam_tx = 107; cam_y = cam_ty = 0;
    title_start();
    fe_lt_t = fe_rt_t = -1000;
    music_volume(fe_music_volume);
    music_play("bgm/m_title.adp");          /* RV: m_title starts in on_frame_3_start_events */
    return 0;
}


/* ---- Press Start / title (frame index 2 = on_frame_3; RE events 307-330, docs/RE_NOTES.md "Title screen").
 * Objects: PlanetAvalice (centre of the star field), Moon (orbits the planet), Stars (3D star field,
 * created one per frame while fewer than 81), TitleStar (250 twinkling stars placed by formula at load;
 * those touching the view border are removed), PressStart (pulses, flashes when pressed), Fade (white
 * fade-in over 32 frames; black fade-out after the press), PressStart shake (Z = 5, -0.4 per frame). */
#define TS_MAX 250
#define ST_MAX 96
static struct { int x, y, frame, alive; animator_t an; } tstar[TS_MAX];
static struct { double a, b, c; int x, y, alive; } star[ST_MAX];
static int nstar, ttimer, tfade_in, tactive, tleaving, tfade_dir, tpress_dir;
static double tfadeA, tmoonA, tpressD, tshakeZ, tshake_x0, tshake_y0;
static int tplanet_x, tplanet_y, tmoon_x, tmoon_y, tfade_alpha, tpress_alpha, tmusic;
static unsigned tseed;
static unsigned t_rand(void) { tseed = tseed * 0x343fd + 0x269ec3; return (tseed >> 16) & 0x7fff; }
#define TDEG 57.29577951
static double t_cos(double n) { return (n == 270. || n == 90.) ? 0. : cos(n / TDEG); }
static double t_sin(double n) { return n == 180. ? 0. : sin(n / TDEG); }
static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

static void title_start(void) {
    ui_obj_t *pl = ui_find(&s_press, "planetavalice", 0), *ts = ui_find(&s_press, "titlestar", 0);
    ttimer = 0; tfade_in = 1; tactive = 0; tleaving = 0; tfade_dir = 1; tfadeA = 0; tfade_alpha = 255;
    tmoonA = 180; tpressD = 0; tpress_dir = 0; tpress_alpha = 255; tshakeZ = 0; tmusic = 100;
    nstar = 0; memset(star, 0, sizeof star); tseed = 0;                 /* initial RNG seed INFERRED */
    tplanet_x = 107 + 0x38; tplanet_y = pl ? pl->y : 184;            /* RE 330: display left + 56 */
    /* RE 307 (x250): A = i; x = (G/2 - cos(i^8) G/2) + i, then wrapped into the view (G = 426);
     * y = sin(i*i - i) * 120 + 120, then (y + 120) mod 240; frame i mod 21 */
    const frame_t *f0 = NULL;
    if (ts && ui_obj_select(&s_press, ts, 0, 0) == 0) f0 = anim_frame(&ts->an);
    for (int i = 0; i < TS_MAX; i++) {
        double d = i, d4 = d * d * d * d, d8 = d4 * d4;
        int x1 = (int)((213. - t_cos(d8) * 213.) + d);
        int x = 107 + (int)fmod(x1 + 213., 426.);
        double e = d * d - d;
        int y1 = e == 180. ? 120 : (int)(t_sin(e) * 120. + 120.);
        int y = (y1 + 120) % 240;
        tstar[i].x = x; tstar[i].y = y; tstar[i].frame = i % 21; tstar[i].alive = 1;
        if (ts) { tstar[i].an = ts->an; tstar[i].an.frame = i % 21; tstar[i].an.acc = 0; }
        /* RE 309: removed while near the border (box not inside the view shrunk by 1 px) */
        if (f0) {
            int bx0 = x - f0->hx, by0 = y - f0->hy;
            if (bx0 < 107 + 1 || by0 < 1 || bx0 + f0->w > 107 + 426 - 1 || by0 + f0->h > 240 - 1) tstar[i].alive = 0;
        }
    }
}

static void title_leave(void) {
    snd_play_orig(SND_START, 255, 128);
    music_stop();
    set_state(FE_MENU); hub_prepare(); cam_x = cam_tx = 43; cam_y = cam_ty = 0;
}

static void title_update(uint32_t p) {
    ui_obj_t *ps = ui_find(&s_press, "pressstart", 0);
    /* 315-318: star field */
    for (int i = 0; i < nstar; i++) if (star[i].alive && star[i].c > 0) star[i].c -= 5;   /* Speed counter = 5 */
    for (int i = 0; i < nstar; i++) if (star[i].alive) {
        double ox = star[i].c != 0 ? (star[i].a * 256.) / star[i].c : 0., oy = star[i].c != 0 ? (star[i].b * 256.) / star[i].c : 0.;
        star[i].x = (int)(tplanet_x + ox); star[i].y = (int)(tplanet_y + oy);
    }
    tmoonA -= 0.08;
    tmoon_x = (int)(t_sin(tmoonA) * 48. + tplanet_x);
    tmoon_y = (int)(t_cos(tmoonA + 30.) * 48. + (tplanet_y - 10));
    int count = 0;
    for (int i = 0; i < nstar; i++) count += star[i].alive;
    if (count < 81) {
        int k = 0;
        while (k < ST_MAX && star[k].alive) k++;
        if (k < ST_MAX) {
            unsigned r1 = t_rand(), r2 = t_rand();
            star[k].alive = 1; star[k].c = 256; star[k].x = 136; star[k].y = -9;
            star[k].a = (double)(int)(((r1 * 5) >> 9) - 20);
            star[k].b = (double)(int)(((r2 * 240) >> 15) - 220);
            if (k >= nstar) nstar = k + 1;
        }
    }
    for (int i = 0; i < nstar; i++) if (star[i].alive && star[i].c <= 0) star[i].alive = 0;
    /* 310-312: timer; the planet drifts 1 px left every 40 frames and 1 px down every 39 */
    ttimer++;
    if (ttimer % 40 == 20) tplanet_x--;
    if (ttimer % 39 == 30) tplanet_y++;
    /* 313/314: white fade-in, then Press Start becomes active */
    if (tfade_in) {
        tfadeA += 1; double v = tfadeA * 4;
        tfade_alpha = v > 128 ? 0 : clamp255((128 - (int)v) * 2);
        if (tfadeA >= 32) { tactive = 1; tfade_in = 0; }
    }
    /* 319/321: pulse (direction 0) */
    if (tactive && tpress_dir == 0) {
        tpressD += 4;
        tpress_alpha = (tpressD == 90. || tpressD == 270.) ? 128 : clamp255((128 - (int)(t_cos(tpressD) * 128. + 64.)) * 2);
        if (tpressD >= 360) tpressD -= 360;
    }
    /* 322: Start / jump / attack pressed: sound 100, flash (direction 1), black fade, music at 60%, shake */
    if (tactive && tpress_dir == 0 && (p & (BTN_START | BTN_JUMP | BTN_ATTACK))) {
        snd_play_orig(SND_START, 255, 128);
        tpress_alpha = 255; tpress_dir = 1; if (ps) ui_obj_select(&s_press, ps, 0, 1);
        tfade_dir = 0; tmusic = 60; music_volume(tmusic * fe_music_volume / 100);
        tshakeZ = 5; if (ps) { tshake_x0 = ps->x; tshake_y0 = ps->y; }
    }
    /* 324: flash finished -> direction 2, leaving. 325: 32 s without input does the same */
    if (tactive && tpress_dir == 1 && ps && ps->an.done) { tpress_dir = 2; ui_obj_select(&s_press, ps, 0, 2); tactive = 0; tleaving = 1; }
    if (tactive && fe_t >= 32 * FPS) { tactive = 0; tleaving = 1; tfade_dir = 0; tpress_dir = 2; if (ps) { ui_obj_select(&s_press, ps, 0, 2); ps->vis = 0; } }
    /* 326/327: fade to black over the music fade; next frame (title menu) at A <= -100 */
    if (tleaving && tpress_dir == 2) {
        tfadeA -= 2; double v = tfadeA * 4;
        tfade_alpha = v < 0 ? 255 : clamp255((128 - (int)v) * 2);
        if (tmusic > 0) { tmusic--; music_volume(tmusic * fe_music_volume / 100); }
        if (tfadeA <= -100) { title_leave(); return; }
    }
    /* 328/329: shake after the press */
    if (ps && tshakeZ > 0) {
        ps->dx = (float)(t_cos(200 * tshakeZ) * tshakeZ + tshake_x0) - ps->x;
        ps->dy = (float)(t_sin(200 * tshakeZ) * tshakeZ * 0.5 + tshake_y0) - ps->y;
        tshakeZ -= 0.4;
        if (tshakeZ <= 0) { ps->dx = ps->dy = 0; }
    }
    for (int i = 0; i < TS_MAX; i++) if (tstar[i].alive && tstar[i].an.b) anim_tick(&tstar[i].an, -1);
    if (ps) anim_tick(&ps->an, -1);
}

static void title_draw(void) {
    ui_obj_t *o;
    if ((o = ui_find(&s_press, "shockwave", 0))) o->vis = 0;
    if ((o = ui_find(&s_press, "pressstart2", 0))) o->vis = 0;
    if ((o = ui_find(&s_press, "stars", 0))) o->vis = 0;
    if ((o = ui_find(&s_press, "titlestar", 0))) o->vis = 0;
    if ((o = ui_find(&s_press, "fade", 0))) o->vis = 0;
    ui_obj_t *ps = ui_find(&s_press, "pressstart", 0), *pl = ui_find(&s_press, "planetavalice", 0), *mo = ui_find(&s_press, "moon", 0);
    if (ps) ps->vis = 0;
    if (pl) { pl->dx = (float)(tplanet_x - pl->x); pl->dy = (float)(tplanet_y - pl->y); }
    if (mo) { mo->dx = (float)(tmoon_x - mo->x); mo->dy = (float)(tmoon_y - mo->y); }
    /* moved to the back (RE 307/317 move_back): title stars, then the star field */
    ui_obj_t *ts = ui_find(&s_press, "titlestar", 0), *st = ui_find(&s_press, "stars", 0);
    if (ts) for (int i = 0; i < TS_MAX; i++) if (tstar[i].alive && tstar[i].an.b) {
        const frame_t *f = anim_frame(&tstar[i].an);
        frame_draw(tstar[i].an.b, f, tstar[i].x - cam_x, tstar[i].y - cam_y, 0, 0xffffffff);
    }
    if (st) for (int i = 0; i < nstar; i++) if (star[i].alive) {
        int d = (int)(0.125 * star[i].c) & 31;
        d &= ~1;                                    /* 16 direction images (even); odd ones INFERRED */
        if (ui_obj_select(&s_press, st, 0, d) < 0) continue;
        const frame_t *f = anim_frame(&st->an);
        if (f) frame_draw(st->an.b, f, star[i].x - cam_x, star[i].y - cam_y, 0, 0xffffffff);
    }
    ui_draw(&s_press, cam_x, cam_y, 0, 1);
    if (ps && tactive + tleaving + (tpress_dir == 0) > 0 && !tfade_in) {
        ps->vis = 1; ui_draw_obj(&s_press, ps, cam_x, cam_y, ((uint32_t)tpress_alpha << 24) | 0xffffff); ps->vis = 0;
    }
    ui_draw(&s_press, cam_x, cam_y, 2, 2);
    if (tfade_alpha > 0)
        r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)tfade_alpha << 24) | (tfade_dir ? 0xf8fcf8 : 0x080000));
}

/* ---- per-panel object handles */
static const int MENU_Y[5] = { 131, 151, 171, 191, 211 };          /* RV screen rows (cursor x 138) */
static const int FILE_Y[3] = { 160, 184, 216 };                     /* RV: FILE n, difficulty, RETURN */
static const int OPT_Y[5] = { 72, 128, 160, 184, 216 };             /* DIFFICULTY, MUSIC, CONTROLS, SENSITIVITY, OKAY */

static void hub_prepare(void) {
    /* SCREEN SIZE is a PC window option; it has no meaning on the Dreamcast, so its row is hidden */
    ui_obj_t *o;
    if ((o = ui_find(&s_hub, "screensize", 0))) o->vis = 0;
    if ((o = ui_find(&s_hub, "screenres", 0))) o->vis = 0;
    /* objects the original keeps off-panel or drives from save data */
    static const char *hide[] = { "warningbox", "warningmessage", "filesaveslotcopy", "filesaveslotcharactercopy",
                                  "copyarrow", "yes", "copy", "mouseobject", "livessetting", "active", "scrolldot",
                                  /* save-info widgets: only shown for an existing save (all slots are NEW) */
                                  "filecurrentstage", "filestageicon", "fileicon", "filelives", "filetimemilli",
                                  "filedeaths", "filecrystals", "filelabel", "filelabel2", "filetimes", "filetimem",
                                  "filesaveslot", "filesaveslotcharacter", "filemessage", 0 };
    for (int i = 0; hide[i]; i++) for (int k = 0; (o = ui_find(&s_hub, hide[i], k)); k++) o->vis = 0;
    /* the file panel's DIFFICULTY label object sits under the value (only the value shows there) */
    if ((o = ui_find(&s_hub, "difficulty", 0))) o->vis = 0;
    if ((o = ui_find(&s_hub, "fileback", 0))) ui_obj_frame(o, 0);    /* frame 0 = hint box */
}

/* counter objects hold digit images 0-9; RV placement: right-aligned, last digit top-left at
 * (sx, sy) on screen, 14 px pitch (options) */
static void counter(ui_scene_t *s, ui_obj_t *o, int value, float sx, float sy, float pitch) {
    if (!o) return;
    char buf[8]; snprintf(buf, sizeof buf, "%d", value);
    int n = (int)strlen(buf);
    for (int i = 0; i < n; i++) {
        ui_obj_frame(o, buf[i] - '0');
        const frame_t *f = anim_frame(&o->an);
        if (!f) return;
        frame_draw(o->an.b, f, sx - (n - 1 - i) * pitch + f->hx, sy + f->hy, 0, 0xffffffff);
    }
    (void)s;
}

static int fe_dev_select_check(const input_t *in) {
    /* developer level select: L and R triggers within the same two seconds, title screen only */
    if (fe_state != FE_PRESS && fe_state != FE_MENU) return 0;
    if (in->pressed & BTN_LT) fe_lt_t = G.frame;
    if (in->pressed & BTN_RT) fe_rt_t = G.frame;
    if (fe_lt_t > 0 && fe_rt_t > 0 && abs(fe_lt_t - fe_rt_t) <= 2 * FPS) return 1;
    return 0;
}

void fe_update(const input_t *in) {
    G.frame++; fe_t++;
    if (fe_dev_select_check(in)) { fe_unload(); game_dev_select(); return; }
    cam_x += (cam_tx - cam_x) * 0.2f; cam_y += (cam_ty - cam_y) * 0.2f;   /* pan speed INFERRED */
    if (fabsf(cam_tx - cam_x) < 0.5f) cam_x = cam_tx;
    if (fabsf(cam_ty - cam_y) < 0.5f) cam_y = cam_ty;
    uint32_t p = in->pressed;
    int up = p & BTN_UP, down = p & BTN_DOWN, left = p & BTN_LEFT, right = p & BTN_RIGHT;
    int ok = p & (BTN_JUMP | BTN_START), back = p & BTN_SPECIAL;
    switch (fe_state) {
    case FE_PRESS:
        title_update(p);
        break;
    case FE_MENU:
        ui_tick(&s_hub);
        if (up) { fe_sel = (fe_sel + 4) % 5; snd_play_orig(SND_MOVE, 220, 128); }
        if (down) { fe_sel = (fe_sel + 1) % 5; snd_play_orig(SND_MOVE, 220, 128); }
        if (ok) {
            switch (fe_sel) {
            case 0: snd_play_orig(SND_BEGIN, 230, 128); set_state(FE_FILE); cam_tx = 43; cam_ty = 240; break;
            case 1: snd_play_orig(SND_SELECT, 230, 128); fe_go_time_attack = 1; set_state(FE_STSEL); cam_x = cam_tx = 107; cam_y = cam_ty = 0; music_play("bgm/m_menu1.adp"); break;
            case 2: break;                                   /* GALLERY: not implemented in the port */
            case 3: snd_play_orig(SND_SELECT, 230, 128); set_state(FE_OPTIONS); cam_tx = 523; cam_ty = 0; break;
            case 4: plat_quit(); break;                      /* QUIT: back to the Dreamcast menu */
            }
        }
        break;
    case FE_FILE:
        ui_tick(&s_hub);
        if (up) { fe_sel = (fe_sel + 2) % 3; snd_play_orig(SND_MOVE, 220, 128); }
        if (down) { fe_sel = (fe_sel + 1) % 3; snd_play_orig(SND_MOVE, 220, 128); }
        if (fe_sel == 0 && (left || right)) { fe_file = (fe_file + (right ? 1 : 7)) % 8; snd_play_orig(SND_MOVE, 220, 128); }
        if (fe_sel == 1 && (left || right)) { fe_difficulty = (fe_difficulty + (right ? 3 : 1)) % 4; snd_play_orig(SND_MOVE, 220, 128); }
        if ((ok && fe_sel == 2) || back) { snd_play_orig(SND_SELECT, 230, 128); set_state(FE_MENU); cam_tx = 43; cam_ty = 0; break; }
        if ((p & BTN_START) || (ok && fe_sel == 0)) {
            snd_play_orig(SND_BEGIN, 230, 128); fe_go_time_attack = 0;
            set_state(FE_CHSEL); cam_x = cam_tx = 107; cam_y = cam_ty = 0;
            music_play("bgm/m_menu1.adp");                   /* RV: m_menu1 in on_frame_7_start_events */
        }
        break;
    case FE_OPTIONS:
        ui_tick(&s_hub);
        if (up) { fe_sel = (fe_sel + 4) % 5; snd_play_orig(SND_MOVE, 220, 128); }
        if (down) { fe_sel = (fe_sel + 1) % 5; snd_play_orig(SND_MOVE, 220, 128); }
        if (fe_sel == 0 && (left || right)) { fe_difficulty = (fe_difficulty + (right ? 3 : 1)) % 4; snd_play_orig(SND_MOVE, 220, 128); }
        if (fe_sel == 1 && (left || right)) {
            fe_music_volume += right ? 10 : -10;
            if (fe_music_volume < 0) fe_music_volume = 0;
            if (fe_music_volume > 100) fe_music_volume = 100;
            music_volume(fe_music_volume); snd_play_orig(SND_MOVE, 220, 128);
        }
        if (fe_sel == 3 && (left || right || ok)) {          /* RV: Return steps sensitivity +5% */
            fe_sensitivity += left ? -5 : 5;
            if (fe_sensitivity > 100) fe_sensitivity = 5;
            if (fe_sensitivity < 5) fe_sensitivity = 100;
            snd_play_orig(SND_SELECT, 220, 128);
        }
        if ((ok && fe_sel == 4) || back) { snd_play_orig(SND_SELECT, 230, 128); set_state(FE_MENU); fe_sel = 3; cam_tx = 43; cam_ty = 0; }
        break;
    case FE_CHSEL:
        ui_tick(&s_chsel);
        /* rows: 0 Lilac, 1 Carol (not implemented in the port), 2 RETURN */
        if (left || up) { fe_sel = (fe_sel + 2) % 3; snd_play_orig(SND_MOVE, 220, 128); }
        if (right || down) { fe_sel = (fe_sel + 1) % 3; snd_play_orig(SND_MOVE, 220, 128); }
        if (back || (ok && fe_sel == 2)) { snd_play_orig(SND_SELECT, 230, 128); set_state(FE_FILE); cam_x = cam_tx = 43; cam_y = cam_ty = 240; music_play("bgm/m_title.adp"); break; }
        if (ok && fe_sel == 0) { snd_play_orig(SND_BEGIN, 230, 128); fe_next_level = "dv1"; set_state(FE_OUT); }
        break;
    case FE_STSEL:
        ui_tick(&s_stsel);
        if (up || down) { fe_sel ^= 1; snd_play_orig(SND_MOVE, 220, 128); }
        if (back || (ok && fe_sel == 1)) { snd_play_orig(SND_SELECT, 230, 128); set_state(FE_MENU); fe_sel = 1; cam_x = cam_tx = 43; cam_y = cam_ty = 0; music_play("bgm/m_title.adp"); break; }
        if (ok && fe_sel == 0) { snd_play_orig(SND_BEGIN, 230, 128); fe_next_level = "dv1"; set_state(FE_OUT); }
        break;
    case FE_OUT:
        if (fe_t >= 30) {                                     /* fade length INFERRED */
            const char *lv = fe_next_level;
            fe_unload();
            game_start_adventure(lv, fe_go_time_attack);
        }
        break;
    }
}

/* tile transition (original "fadein" objects: 32 px tiles, anim 4 shrinks each tile over 6 frames).
 * The per-tile stagger is INFERRED (diagonal). */
static void tiles(const ui_scene_t *s, ui_obj_t *tile, int t, int out) {
    if (!tile) return;
    for (int ty = 0; ty < 8; ty++)
        for (int tx = 0; tx < 14; tx++) {
            int k = t - (tx + ty);
            int f = out ? 5 - k : k;
            if (f < 0) f = 0;
            if (f > 5) continue;
            ui_obj_select((ui_scene_t *)s, tile, 4, 0); ui_obj_frame(tile, f);
            float dx = tile->dx, dy = tile->dy;
            tile->dx = tx * 32 - tile->x + cam_x; tile->dy = ty * 32 - tile->y + cam_y;
            ui_draw_obj((ui_scene_t *)s, tile, cam_x, cam_y, 0xffffffff);
            tile->dx = dx; tile->dy = dy;
        }
}

void fe_draw(void) {
    r_begin();
    switch (fe_state) {
    case FE_PRESS:
        title_draw();
        break;
    case FE_MENU: case FE_FILE: case FE_OPTIONS: {
        ui_obj_t *cur = ui_find(&s_hub, "crystalcursor", 0);
        /* file panel values */
        ui_obj_t *d2 = ui_find(&s_hub, "difficulty2", 0); if (d2) ui_obj_select(&s_hub, d2, 0, fe_difficulty);
        ui_obj_t *d4 = ui_find(&s_hub, "difficulty4", 0); if (d4) ui_obj_select(&s_hub, d4, 0, fe_difficulty);
        ui_obj_t *mv = ui_find(&s_hub, "musicvolume", 1); if (mv) mv->vis = 0;
        ui_obj_t *cs = ui_find(&s_hub, "controllersensitivity", 0); if (cs) cs->vis = 0;
        ui_obj_t *fid = ui_find(&s_hub, "fileid", 0); if (fid) fid->vis = 0;
        ui_draw(&s_hub, cam_x, cam_y, 0, 3);
        if (fe_state == FE_FILE || fabsf(cam_y - 240) < 200) counter(&s_hub, fid, fe_file + 1, 244 - cam_x + 43, 152 - cam_y + 240, 14);
        counter(&s_hub, mv, fe_music_volume, 337 - cam_x + 523, 120 - cam_y, 14);
        counter(&s_hub, cs, fe_sensitivity, 337 - cam_x + 523, 176 - cam_y, 14);
        /* save-slot carousel (RV fe_a: slot centred at x 213, 120 px pitch, wraps around the 8 files) */
        {
            ui_obj_t *sl = ui_find(&s_hub, "filesaveslot", 0), *sc = ui_find(&s_hub, "filesaveslotcharacter", 0);
            for (int rel = -2; rel <= 2; rel++) {
                float sx = 213 + rel * 120 - cam_x + 43, sy = 104 - cam_y + 240;
                if (sl) { sl->vis = 1; ui_obj_select(&s_hub, sl, 0, rel == 0 ? 1 : 0); sl->dx = sx + cam_x - sl->x; sl->dy = sy + cam_y - sl->y; ui_draw_obj(&s_hub, sl, cam_x, cam_y, 0xffffffff); sl->vis = 0; }
                if (sc) { sc->vis = 1; sc->dx = sx + cam_x - sc->x - 0; sc->dy = sy + cam_y - sc->y; ui_draw_obj(&s_hub, sc, cam_x, cam_y, 0xffffffff); sc->vis = 0; }
            }
        }
        if (fe_state == FE_FILE) {
            /* RV strings (binary): hint box text for the selected row */
            static const char *msg[3] = { "Press START to begin a new game.", "Press LEFT or RIGHT to adjust the DIFFICULTY.",
                                          "Select RETURN to pick another file slot." };
            /* RV (fe_a): lines at (101,24) and (101,44) in the hint box, wrapped at 26 characters */
            const char *m = msg[fe_sel];
            char l1[32] = {0}; const char *l2 = "";
            size_t n = strlen(m), cut = n <= 26 ? n : 26;
            while (cut > 0 && n > 26 && m[cut] != ' ') cut--;
            memcpy(l1, m, cut); l2 = m + cut + (m[cut] == ' ');
            fe_text(101 - cam_x + 43, 24 - cam_y + 240, l1); fe_text(101 - cam_x + 43, 44 - cam_y + 240, l2);
            cursor_to(&s_hub, cur, 170, FILE_Y[fe_sel]);
        } else if (fe_state == FE_OPTIONS) {
            cursor_to(&s_hub, cur, fe_sel == 4 ? 170 : 90, OPT_Y[fe_sel]);
        } else {
            cursor_to(&s_hub, cur, 138, MENU_Y[fe_sel]);
        }
        if (cur) ui_draw_obj(&s_hub, cur, cam_x, cam_y, 0xffffffff);
        break;
    }
    case FE_CHSEL: {
        ui_obj_t *o;
        for (int k = 3; k <= 5; k++) { char nm[16]; snprintf(nm, sizeof nm, "character%d", k); if ((o = ui_find(&s_chsel, nm, 0))) ui_obj_select(&s_chsel, o, 5, 0); }
        for (int k = 3; k <= 5; k++) { char nm[16]; snprintf(nm, sizeof nm, "charname%d", k); if ((o = ui_find(&s_chsel, nm, 0))) o->vis = 0; }
        if ((o = ui_find(&s_chsel, "character4", 0))) o->vis = 0;
        if ((o = ui_find(&s_chsel, "character5", 0))) o->vis = 0;
        if ((o = ui_find(&s_chsel, "character1", 0))) ui_obj_select(&s_chsel, o, 0, fe_sel == 0 ? 8 : 0);
        if ((o = ui_find(&s_chsel, "character2", 0))) ui_obj_select(&s_chsel, o, 0, fe_sel == 1 ? 8 : 0);
        if ((o = ui_find(&s_chsel, "return", 0))) ui_obj_select(&s_chsel, o, 0, fe_sel == 2 ? 8 : 0);
        for (int k = 0; (o = ui_find(&s_chsel, "fadein", k)); k++) o->vis = 0;
        if ((o = ui_find(&s_chsel, "fade", 0))) o->vis = 0;
        if ((o = ui_find(&s_chsel, "helpmessage", 0))) o->vis = 0;
        if ((o = ui_find(&s_chsel, "icona", 0))) o->vis = 0;
        if ((o = ui_find(&s_chsel, "iconb", 0))) o->vis = 0;
        ui_draw(&s_chsel, cam_x, cam_y, 0, 3);
        fe_text(8, 222, " A: Select                                B: Return");
        ui_obj_t *cur = ui_find(&s_chsel, "crystalcursor", 0);
        if (cur) cur->vis = fe_sel == 2;
        if (cur && fe_sel == 2) ui_draw_obj(&s_chsel, cur, cam_x, cam_y, 0xffffffff);
        if (fe_t < 20) tiles(&s_chsel, ui_find(&s_chsel, "fadein", 0), fe_t, 0);
        break;
    }
    case FE_STSEL: case FE_OUT: {
        ui_obj_t *o;
        for (int k = 0; (o = ui_find(&s_stsel, "fadein", k)); k++) o->vis = 0;
        if ((o = ui_find(&s_stsel, "fade", 0))) o->vis = 0;
        if ((o = ui_find(&s_stsel, "helpmessage", 0))) o->vis = 0;
        if ((o = ui_find(&s_stsel, "filemessage", 0))) o->vis = 0;
        if ((o = ui_find(&s_stsel, "charname1", 0))) o->vis = 0;
        if ((o = ui_find(&s_stsel, "active2", 0))) o->vis = 0;
        if ((o = ui_find(&s_stsel, "icona", 0))) o->vis = 0;
        if ((o = ui_find(&s_stsel, "iconb", 0))) o->vis = 0;
        /* stagename / stageicon1 frames: 0 Aqua Tunnel, 1 Dragon Valley, 2 Relic Maze, 3 Fortune Night */
        if ((o = ui_find(&s_stsel, "stagename", 0))) { ui_obj_select(&s_stsel, o, 0, 0); ui_obj_frame(o, 1); }
        if ((o = ui_find(&s_stsel, "stageicon1", 0))) ui_obj_frame(o, 1);
        if ((o = ui_find(&s_stsel, "return", 0))) ui_obj_select(&s_stsel, o, 0, fe_sel == 1 ? 8 : 0);
        ui_obj_t *cur = ui_find(&s_stsel, "crystalcursor", 0);
        if (cur) cur->vis = fe_sel == 1;
        if (fe_state == FE_STSEL || fe_go_time_attack) ui_draw(&s_stsel, cam_x, cam_y, 0, 3);
        else ui_draw(&s_chsel, cam_x, cam_y, 0, 3);
        fe_text(8, 222, " A: Select                                B: Return");
        if (fe_state == FE_OUT) { ui_scene_t *sc = fe_go_time_attack ? &s_stsel : &s_chsel; tiles(sc, ui_find(sc, "fadein", 0), fe_t, 1); }
        break;
    }
    }
    r_end();
}
