/* The end of the game (session 24): Final Dreadnought round 4 -> the space ending -> the credits -> "Thanks for playing!"
 * -> the title menu. Evidence: build/re (handle_frame_9 routing, handle_frame_84 = frame index 83, handle_frame_86 = frame
 * index 85, events 22403-22446 / 22718-22800) and RV build/orig_traces/ending (screenshots e83_* / c83_* / end0_*).
 * Labels: RE (decompiled), RV (original run), INFERRED.
 *
 * Sequence (RE handle_frame_9: MenuSequence.I == 13 -> frame 83 for G0 1 and 2; RV c83: 83 -> 81 -> 82 -> 85):
 *   1. frame index 83, the space scene (END_SPACE): the Dreadnought lifts off and breaks apart over the planet; one
 *      controller (SpaceDreadnought.B, +1 a frame) drives it: lift-off (B < 240), jets flicker 140-212, rumble 425 at 120,
 *      shake 240-720, three pieces fall off at 380 / 500 / 720 (sound 354), the hero ship leaves at 400 (294), Brevon's mech
 *      at 500, the hull sinks 720-820, the flash at 816 (76 at 820), the black fade from 960, and at 1110 the frame changes
 *      (22441: frame 81 with G4 = 3). No music, no skip (RE: no input test in its events).
 *   2. frames 81 / 82, the epilogue montage (Classic: wordless story rooms, ~80 s RV; Adventure: the dialogue): NOT ported
 *      (dozens of story-room scenes). INFERRED stand-in: END_HOLD, 60 black frames. The adventure-cutscene template
 *      (cutscene.c) is the place to add it.
 *   3. frame index 85, the credits (END_CREDITS): m_credits (loop, RE 22790); the view centre Y = the scroll controller A
 *      (RE 22724), A from -60 + 1 every 2nd frame (22726), + 10 a frame while a button is held (22728: InputsP1.H > 0;
 *      which buttons set H is INFERRED: any face button or Start), at most 6000. The cast / staff text (CreditsList
 *      paragraph 0 at y 240, paragraph 1 at y 2752, font.bmp 8x20) and the statistics (y 4496) scroll; the statistics stop
 *      with their last line at y 220 (22777 / 22778), then "Thanks for playing!" and the played character (RE 22782-22784:
 *      anim 37, Lilac / Carol / Milla) fade in, CONTINUE with the crystal cursor shows at A = 180 (22779) and the next
 *      jump / start press (22780: sound 56, FadeEdge wipe, m_credits stopped) leads, after the wipe (F > 50, 22781), to frame 3:
 *      the title menu (RV end0: "Setting frame: 3", the cursor on START GAME).
 * The statistics / final score (RE 22723 / 22793-22800; save.c save_final_score) are computed once, when the ending starts,
 * from the file the run just wrote (RE 954 runs at frame 8, before 83): each stage with a time, then FINAL SCORE: TIME,
 * CRYSTALS, DEATHS = the file totals. Not ported / INFERRED: the cast members' own reveal animations and voices (each stands
 * in its first pose; the playable three wave, anim 37), Torque's rocket launch (it flies up when reached, sound 284), the
 * background layer motion (navy -> black, star drift, the clouds' parallax and the moon: shaped after RV), the
 * explosion count / positions and the ships' trajectories (spawn times RE). No per-frame allocation: every buffer is
 * static or made at load. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../fp.h"
#include "ui.h"
#include "cutscene.h"

enum { END_OFF, END_SPACE, END_HOLD, END_CREDITS, END_THANKS, END_LEAVE, END_CS };   /* END_CS: the host-only cutscene template run */
#define END_HOLD_FRAMES 60            /* INFERRED stand-in for frames 81 / 82 */
#define SPACE_NEXT 1110               /* RE 22441: B > 1110 -> frame 81 */
#define CRED_A0 (-60.)                /* RE 22790 */
#define CRED_AMAX 6000.               /* RE handle_frame_86 */
#define TEXT_X 45                     /* RV ff_010: the message text's left edge (frame x 152, display left 107) */
#define STATS_X 37                    /* RV end0_020: the statistics' left edge */
#define MAXL 160

static const short END_SFX[] = { 425, 435, 294, 354, 284 };   /* rumble, jets, hero ship, pieces, Torque's rocket (+ resident 76, 56, 311, 319, 320) */

static struct {
    int st, t, B, armed, leave_f, cont_t, hold_t; float leave_x;
    double A;                          /* credits scroll controller (RE 0x145f0 A) */
    int rumble_ch, ship_frame, jets_on, piece_n, rocket_on; float rocket_y;
    float sx, sy, sx0, sy0, sE, sscale; /* the Dreadnought: position, rest position, sink speed E, scale */
    float hx, hy, hA, hB;              /* hero ship: position, velocity A / B (RE 22425 C += A, D += B, B -= 0.02) */
    float mx, my, mA, mB;              /* Brevon's mech (RE 22430: A -= 0.03, >= -3) */
    struct { float x, y, vx, vy; int f, on; } piece[3];
    struct { float x, y, s; animator_t an; int on; } ex[24];
    int stats_pinned; float stats_y; int thanks_a;
    uint32_t prev;
    int ended_once;
} E;
static final_score_t SC;
static ui_banks_t eb, fb;            /* fb: the font (ui/font) */
static ui_scene_t es;
#define font (fb.bank[0])
static int loaded;                     /* 1 space scene, 2 credits */
static char *ctext; static const char *cl[MAXL * 2]; static int n0, n1, nnames; static const char *names[16];
static char stats[MAXL][40]; static int nstats;

static uint32_t rnd_s = 0x1234567;
static int rnd(int n) { rnd_s = rnd_s * 0x343fd + 0x269ec3; return (int)((((rnd_s >> 16) & 0x7fff) * (unsigned)n) >> 15); }

static void text(float x, float y, const char *s, uint32_t argb) {   /* font.bmp 8x20, as frontend.c fe_text */
    if (!font.nframes) return;
    for (; *s; s++, x += 8) { int c = (unsigned char)*s - 32; if (c > 0 && c < font.nframes) frame_draw(&font, &font.frames[c], x, y, 0, argb); }
}

static void unload(void) {
    if (loaded) { ui_free(&es); ui_banks_free(&eb); }
    if (fb.n) ui_banks_free(&fb);
    free(ctext); ctext = NULL;
    loaded = 0;
}

static int load(int which) {
    unload();
    game_unload_all();
    loading_begin(which == 1 ? "end" : "cred");
    int bad = which == 1 ? ui_banks_load(&eb, "end/end", 1) < 0 || ui_load(&es, "end/end_84", &eb) < 0
                         : ui_banks_load(&eb, "end/cred", 2) < 0 || ui_load(&es, "end/cred_86", &eb) < 0;
    if (!bad) loaded = which;
    if (which == 2) { memset(&fb, 0, sizeof fb); if (bank_load(&fb.bank[0], "end/font") == 0) fb.n = 1; else memset(&fb, 0, sizeof fb); }   /* font.bmp 8x20 (copied into end/) */
    if (which == 2) {                   /* the credits text (tools/import_ending.py) */
        size_t sz; ctext = plat_load_file("end/credits.txt", &sz);
        n0 = n1 = nnames = 0;
        if (ctext) {
            char *p = ctext, *e = ctext + sz; int a, b, c, k = 0;
            if (sz > 5 && !memcmp(p, "FPCR1", 5) && sscanf(p, "FPCR1 %d %d %d", &a, &b, &c) == 3 && a + b <= 2 * MAXL && c <= 16) {
                while (p < e && *p != '\n') p++;
                if (p < e) p++;
                while (p < e && k < a + b + c) {
                    char *q = p; while (q < e && *q != '\n') q++;
                    if (q < e) *q = 0;
                    if (k < a + b) cl[k] = p; else names[k - a - b] = p;
                    k++; p = q + 1;
                }
                if (k == a + b + c) { n0 = a; n1 = b; nnames = c; }
            }
        }
    }
    if (which == 1) game_sfx_set(END_SFX, (int)(sizeof END_SFX / sizeof END_SFX[0]));
    loading_end(); lp_end();
    if (bad) plat_log("ending data missing (end/)\n");
    return bad ? -1 : 0;
}

/* the statistics text, built once (RE 22793 / 22723 / 22794-22800; leading spaces as the original's strings) */
static void stats_build(void) {
    nstats = 0;
    #define L(...) do { if (nstats < MAXL) snprintf(stats[nstats++], sizeof stats[0], __VA_ARGS__); } while (0)
    L("                GAME STATS"); L("%s", "");
    for (int s = 0; s < SCORE_STAGES; s++) {
        if (!(SC.shown >> s & 1)) continue;
        uint32_t ms = SC.stage[s].time_ms;
        L("%s", s < nnames ? names[s] : "");
        L("              Time     %s%u:%02u:%02u", ms < 600000 ? "0" : "", (unsigned)(ms / 60000), (unsigned)(ms / 1000 % 60), (unsigned)(ms / 10 % 100));
        L("          Crystals     %d", SC.stage[s].crystals);
        L("            Deaths     %d", SC.stage[s].deaths);
        L("%s", "");
    }
    uint32_t ms = SC.time_ms;
    L("%s", ""); L("%s", ""); L("                FINAL SCORE"); L("%s", "");
    L("     TIME"); L("     %s%u:%02u:%02u", ms < 600000 ? "0" : "", (unsigned)(ms / 60000), (unsigned)(ms / 1000 % 60), (unsigned)(ms / 10 % 100));
    L("%s", ""); L("     CRYSTALS"); L("     %d", SC.crystals);
    L("%s", ""); L("     DEATHS"); L("     %d", SC.deaths);
    #undef L
}

/* ---- validation: the ending follows only the last act of stage 13 in a story run (RE frame 8 routing) */
static int valid_now(void) {
    return G.mode == MODE_PLAY && !G.time_attack && G.lvdir[0] && level_cur()->stage == 13 && !level_next(G.lvdir) && E.st == END_OFF;
}
static void save_round(void) {         /* a round-4 script without results: its stage block first (RE 954 at frame 8) */
    if (!results_saved()) {
        cards_commit();
        game_save_stage_end((uint32_t)((uint64_t)game_timer() * 1000 / FPS));
        save_store();
    }
    results_game_clear_ach();
}
int ending_arm(void) {
    if (!valid_now()) return -1;
    save_round();
    E.armed = 1;
    return 0;
}
static void space_start(void);
int game_ending_begin(void) {
    if (E.st != END_OFF) return -1;                 /* no second start while it runs */
    if (!E.armed) { if (!valid_now()) return -1; save_round(); }
    int fs = G.file_slot;
    save_final_score(save_run_file(fs), &SC);       /* computed once, from the run's file (RE 22790: file<G10>.sav) */
#ifdef FP_HOST_ENV
    printf("ending begin chr %d file %d score time %u crystals %d deaths %d shown %x\n", G.character, fs, (unsigned)SC.time_ms, SC.crystals, SC.deaths, SC.shown);
#endif
    music_stop(); snd_stop_all();
    G.mode = MODE_ENDING;
    if (load(1) < 0) { E.armed = 0; E.st = END_HOLD; E.t = 0; E.hold_t = 0; return 0; }   /* no data: straight to the credits attempt */
    space_start();
    return 0;
}
int ending_state(void) { return E.st; }

/* ---- frame index 83 */
static ui_obj_t *O(const char *c, int n) { return ui_find(&es, c, n); }
static void space_start(void) {
    int n = E.ended_once;
    memset(&E, 0, sizeof E); E.ended_once = n;
    E.st = END_SPACE; E.t = 0; E.rumble_ch = -1;
    ui_obj_t *d = O("spacedreadnought", 0);
    E.sx = E.sx0 = d ? d->x : 213; E.sy = E.sy0 = d ? d->y : 119; E.sscale = 1;
    E.jets_on = 1; snd_play_orig(435, 230, 128);   /* handle_frame_84: A == 0, once: play_id 435 channel 24 */
    E.hx = -1000; E.mx = -1000;
}
static void piece_off(int k) {          /* 22424 / 22429 / 22432: the hull loses a part, the part falls (motion INFERRED) */
    if (k > 2) return;
    E.ship_frame = k + 1;
    E.piece[k].on = 1; E.piece[k].x = E.sx; E.piece[k].y = E.sy; E.piece[k].f = k;
    E.piece[k].vx = k == 0 ? -0.4f : k == 1 ? 0.45f : 0; E.piece[k].vy = 0.2f;
    snd_play_orig(354, 230, 128);
}
static void boom(float x, float y, float s) {
    ui_obj_t *o = O("effectsexplosion06", 0);
    if (!o || !o->an.b) return;
    for (int i = 0; i < 24; i++) if (!E.ex[i].on) {
        E.ex[i].on = 1; E.ex[i].x = x; E.ex[i].y = y; E.ex[i].s = s; E.ex[i].an = o->an; anim_restart(&E.ex[i].an);
        static const int SN[3] = { 311, 319, 320 };   /* loop_sound_explode3 / 4 / 5 (RE: sound ids 311, 319, 320) */
        snd_play_orig(SN[rnd(3)], 180, 128);
        return;
    }
}
static void space_update(void) {
    int B = ++E.B;
    if (B < 120 && B % 4 == 0) E.sy0 -= 1;                          /* 22409 */
    if (B >= 120 && B < 240 && B % 10 == 0) E.sy0 -= 1;            /* 22410 */
    if (B == 120) E.rumble_ch = snd_loop_orig(425, 77, 0);           /* 22411: channel 23, volume 30 */
    static const int JET[9] = { 140, 160, 170, 190, 196, 200, 204, 208, 212 };   /* 22412-22420: off / on ... off */
    for (int k = 0; k < 9; k++) if (B == JET[k]) { E.jets_on = k & 1; if (E.jets_on) snd_play_orig(435, 230, 128); }
    E.sx = E.sx0; E.sy = E.sy0;
    if (B > 240 && B < 720) { E.sx = E.sx0 - 1 + rnd(3); E.sy = E.sy0 - 1 + rnd(3); }   /* 22421 */
    if ((B > 280 && B < 380 && B % 8 == 0) || (B > 400 && B < 500 && B % 6 == 0) || (B > 520 && B < 720 && B % 4 == 0))   /* 22423 / 22428 / 22431 */
        boom(E.sx - 30 + rnd(61), E.sy - 10 + rnd(61), 0.5f + rnd(50) / 100.f);
    if (B == 380) piece_off(0);
    if (B == 500) piece_off(1);
    if (B == 720) piece_off(2);
    if (B == 400) { E.hx = E.sx; E.hy = E.sy + 40; E.hA = 0.6f; E.hB = 1.5f; }                 /* 22425 (start / speeds INFERRED) */
    if (B > 400) { E.hx += E.hA; E.hy += E.hB; E.hB = E.hB - 0.02f > 0 ? E.hB - 0.02f : 0;
                   if (E.hB == 0) { if (E.rumble_ch >= 0 && E.hA > 0) { snd_loop_stop(E.rumble_ch); E.rumble_ch = snd_loop_orig(294, 77, 0); } E.hA -= 0.4f; } }   /* 22426 / 22427 */
    if (B == 520) { E.mx = E.sx + 20; E.my = E.sy + 20; E.mA = 1.0f; E.mB = -0.6f; }           /* 22430 (start INFERRED) */
    if (B > 520) { E.mx += E.mA; E.my += E.mB; E.mA = E.mA - 0.03f > -3 ? E.mA - 0.03f : -3; }
    if (B > 720 && B < 820) { E.sE += 0.005f; E.sy0 += E.sE; E.sscale -= 0.5f * E.sE; if (E.sscale < 0) E.sscale = 0; }   /* 22433 */
    if (B == 820) { snd_play_orig(76, 255, 128); if (E.rumble_ch >= 0) { snd_loop_stop(E.rumble_ch); E.rumble_ch = -1; } }   /* 22435 */
    for (int k = 0; k < 3; k++) if (E.piece[k].on) { E.piece[k].x += E.piece[k].vx; E.piece[k].y += E.piece[k].vy; E.piece[k].vy += 0.01f; }
    for (int i = 0; i < 24; i++) if (E.ex[i].on) { anim_tick(&E.ex[i].an, -1); if (E.ex[i].an.done) E.ex[i].on = 0; }
    ui_tick(&es);
    if (B > SPACE_NEXT) {                                              /* 22441: frame 81 (G4 3), Media::stop_samples */
        if (E.rumble_ch >= 0) snd_loop_stop(E.rumble_ch);
        snd_stop_all();
        E.st = END_HOLD; E.hold_t = 0;
    }
}
static void draw_at(ui_obj_t *o, float x, float y, uint32_t argb) {
    if (!o || !o->an.b) return;
    const frame_t *f = anim_frame(&o->an); if (f) frame_draw(o->an.b, f, x, y, 0, argb);
}
static void space_draw(void) {
    int B = E.B;
    r_rect(0, 0, SCREEN_W, SCREEN_H, 0xff000000);
    ui_obj_t *sp = O("space", 0);
    if (sp) ui_draw_obj(&es, sp, 0, 0, 0xffffffff);
    for (int i = 0; ; i++) {                                           /* handle_frame_84: every 2nd frame A = (A + 1) % 21 */
        ui_obj_t *s = O("titlestar", i); if (!s) break;
        if (s->x < 0) continue;
        ui_obj_frame(s, (i + B / 2) % 21); ui_draw_obj(&es, s, 0, 0, 0xffffffff);
    }
    ui_obj_t *jet = O("liftoffjet", 0), *jet2 = O("liftoffjet", 1), *d = O("spacedreadnought", 0);
    float dy = E.sy - (d ? d->y : 119);
    if (E.jets_on) { if (jet) draw_at(jet, jet->x, jet->y + dy, 0xffffffff); if (jet2) draw_at(jet2, jet2->x, jet2->y + dy, 0xffffffff); }
    ui_obj_t *pc = O("spacedebris", 0);
    for (int k = 0; k < 3; k++) if (E.piece[k].on && pc) { ui_obj_frame(pc, E.piece[k].f); draw_at(pc, E.piece[k].x, E.piece[k].y, 0xffffffff); }
    if (d && E.sscale > 0) {
        ui_obj_select(&es, d, 0, B > 520 && B < 720 && (B / 2) % 2 ? 1 : 0);   /* 22422: direction (B / 2) % 2 after 520 (the flash) */
        ui_obj_frame(d, E.ship_frame);
        const frame_t *f = anim_frame(&d->an);
        if (f) frame_draw_scaled(d->an.b, f, E.sx - f->hx * E.sscale, E.sy - f->hy * E.sscale, f->w * E.sscale, f->h * E.sscale, 0xffffffff);
    }
    ui_obj_t *h = O("spaceheroship", 0), *m = O("spacebrevonmech", 0);
    if (h && B > 400) { ui_obj_select(&es, h, 0, E.hA < 0 ? 16 : 0); draw_at(h, E.hx, E.hy, 0xffffffff); }
    if (m && B > 520) draw_at(m, E.mx, E.my, 0xffffffff);
    for (int i = 0; i < 24; i++) if (E.ex[i].on) {
        const frame_t *f = anim_frame(&E.ex[i].an);
        if (f) frame_draw_scaled(E.ex[i].an.b, f, E.ex[i].x - f->hx * E.ex[i].s, E.ex[i].y - f->hy * E.ex[i].s, f->w * E.ex[i].s, f->h * E.ex[i].s, 0xffffffff);
    }
    if (B >= 816 && B < 860) { int a = 255 - (B - 816) * 6; r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)(a < 0 ? 0 : a) << 24) | 0xf8fcf8); }   /* shockwave flash (INFERRED for anim 8) */
    if (B >= 960) { int a = (B - 960) * 2; r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)(a > 255 ? 255 : a) << 24)); }   /* 22439 / 22440 LevelCardFade */
}

/* ---- frame index 85 */
static void credits_start(void) {
    if (load(2) < 0) { E.st = END_LEAVE; return; }                   /* no credits data: the title menu */
    stats_build();
    E.st = END_CREDITS; E.t = 0; E.A = CRED_A0; E.stats_pinned = 0; E.thanks_a = 0; E.cont_t = 0; E.leave_f = 0;
    E.rocket_on = 0; E.rocket_y = 0;
    music_volume(SV.music_volume <= 100 ? SV.music_volume : 100);
    music_play("bgm/m_credits.adp");                                 /* 22790: m_credits, channel 31, loop */
}
static float cred_top(void) { return (float)E.A - SCREEN_H / 2; }     /* set_display_center(320, A) */
static float stats_top_nat(void) { ui_obj_t *s = O("statistics", 0); return (s ? s->y : 4496) - cred_top(); }
static void credits_update(const input_t *in) {
    uint32_t held = in->held, p = E.prev; E.prev = in->pressed;      /* menus read the previous frame's presses (RE) */
    E.t++;
    if (E.st == END_CREDITS || E.st == END_THANKS) {
        if (E.t % 2 == 0) E.A += 1;                                   /* 22726 */
        if (held & (BTN_JUMP | BTN_ATTACK | BTN_SPECIAL | BTN_START)) E.A += 10;   /* 22728 (H > 0) */
        if (E.A > CRED_AMAX) E.A = CRED_AMAX;
    }
    ui_obj_t *rk = O("creditstorquerocket", 0);
    if (rk && !E.rocket_on && rk->y < cred_top() + 170) { E.rocket_on = 1; snd_play_orig(284, 230, 128); }   /* handle_frame_86 */
    if (E.rocket_on) E.rocket_y -= 10;
    float h = (float)nstats * 20;
    if (!E.stats_pinned && (stats_top_nat() + h < 220 || E.A >= CRED_AMAX)) {   /* (the A limit: INFERRED safety for a long list) */ E.stats_pinned = 1; E.st = END_THANKS; E.cont_t = 0; }   /* 22777 */
    if (E.stats_pinned) {
        if (E.cont_t < 180) E.cont_t++;                               /* 0x1370 A to 180 (22779) */
        if (E.thanks_a < 255) E.thanks_a += 2;                        /* fade-in (22778; speed INFERRED) */
        if (E.st == END_THANKS && E.cont_t >= 180 && (p & (BTN_JUMP | BTN_START))) {   /* 22780 */
            snd_play_orig(56, 230, 128); music_stop();
            E.st = END_LEAVE; E.leave_f = 0; E.leave_x = -(SCREEN_W + 120);
        }
    }
    if (E.st == END_LEAVE) {
        E.leave_x = E.leave_x + 32 > 0 ? 0 : E.leave_x + 32;
        if (++E.leave_f > 50) {                                       /* 22781: F > 50 -> frame 3 */
            unload(); E.st = END_OFF; E.ended_once++;
            fe_enter_from_stage(0);
            return;
        }
    }
    ui_tick(&es);
}
static void cred_draw_obj(const char *c, int n, float dy, uint32_t argb) {
    ui_obj_t *o = O(c, n); if (!o) return;
    float y = o->y - cred_top() + dy;
    if (y < -160 || y > SCREEN_H + 160) return;
    draw_at(o, o->x - 107, y, argb);
}
static void credits_draw(void) {
    float top = cred_top(), prog = (float)((E.A - CRED_A0) / (CRED_AMAX - CRED_A0));
    /* the sky: navy (RV start) to black (RV end), stars drifting at half the text speed (RE 22786 0.5; INFERRED) */
    int k = (int)(prog * 1.6f * 255); if (k > 255) k = 255;
    r_rect(0, 0, SCREEN_W, SCREEN_H, 0xff000000 | (uint32_t)((8 * (255 - k) / 255) << 16) | (uint32_t)((12 * (255 - k) / 255) << 8) | (uint32_t)(56 * (255 - k) / 255));
    for (int i = 0; ; i++) {
        ui_obj_t *s = O("titlestar", i); if (!s) break;
        float y = fmodf(s->y - top * 0.5f, 6400.f * 0.5f); if (y < 0) y += 3200.f;
        y = fmodf(y, 480.f) - 120.f;
        if (y < -8 || y > SCREEN_H + 8) continue;
        ui_obj_frame(s, (i + E.t / 2) % 21); draw_at(s, s->x - 107, y, 0xffffffff);
    }
    ui_obj_t *moon = O("introsky", 0);                                 /* RV end0_030: the moon comes down at the top right at the end (path INFERRED) */
    if (moon && moon->an.b && prog > 0.9f) { const frame_t *f = anim_frame(&moon->an);
        if (f) frame_draw(moon->an.b, f, 128 + f->hx, -(float)f->h + (prog - 0.9f) * 10.f * 68.f + f->hy, 0, 0xffffffff); }
    /* the clouds (layer 1, RE 22724: layer y = 0.2 A - 850; RV: they cover the view until A ~ 4350, then leave) */
    static const char *const CL[4] = { "creditsclouds2", "creditsclouds1", "creditsclouds3", "creditsclouds4" };
    for (int t = 0; t < 2; t++) for (int c = 0; c < 4; c++) {
        ui_obj_t *o = O(CL[c], 0); if (!o || !o->an.b) continue;
        const frame_t *f = anim_frame(&o->an); if (!f) continue;
        float y = t * 594.f - 0.2f * (float)E.A - 76.f;
        if (y > SCREEN_H || y + f->h < 0) continue;
        frame_draw(o->an.b, f, o->x - 107 + f->hx, y + f->hy, 0, 0x90ffffff);   /* soft clouds: the images' graded alpha is 1 bit in ARGB1555 (INFERRED vertex alpha) */
    }
    if (E.rocket_on) cred_draw_obj("creditstorquerocket", 0, E.rocket_y, 0xffffffff);
    else cred_draw_obj("creditstorquerocket", 0, 0, 0xffffffff);
    /* the text (layer 2): paragraph 0 from y 240, paragraph 1 from y 2752 (RE 22790 / frame json) */
    for (int i = 0; i < n0 + n1; i++) {
        float y = (i < n0 ? 240.f + i * 20 : 2752.f + (i - n0) * 20) - top;
        if (y < -20 || y > SCREEN_H) continue;
        text(TEXT_X, y, cl[i], 0xffffffff);
    }
    /* the cast and the other layer-2 actives at their frame positions (first pose; the playable three wave, anim 37) */
    for (int i = 0; i < es.nobj; i++) {
        ui_obj_t *o = &es.o[i];
        if (o->layer != 2 || o->kind != 1 || !o->an.b || o->x < 0) continue;
        if (!strcmp(o->cls, "mp2") || !strncmp(o->cls, "playerskin", 10) || !strncmp(o->cls, "npc", 3) || !strcmp(o->cls, "brevon") ||
            !strcmp(o->cls, "themagister") || !strcmp(o->cls, "gongshield") || !strcmp(o->cls, "lamppostfront") || !strcmp(o->cls, "serpentinechestplate")) {
            float y = o->y - top; if (y < -120 || y > SCREEN_H + 120) continue;
            draw_at(o, o->x - 107, y, 0xffffffff);
        }
    }
    /* the statistics, then pinned with the last line at y 220 (22777 / 22778) */
    float sy = E.stats_pinned ? 220.f - (float)nstats * 20 : stats_top_nat();
    for (int i = 0; i < nstats; i++) { float y = sy + i * 20; if (y >= -20 && y <= SCREEN_H) text(STATS_X, y, stats[i], 0xffffffff); }
    if (E.stats_pinned) {                                             /* Thanks for playing! (active_4203 at 184, 88 on layer 1: screen) */
        uint32_t a = (uint32_t)(E.thanks_a > 255 ? 255 : E.thanks_a) << 24 | 0xffffff;
        ui_obj_t *ty = O("active", 0); if (ty) ui_draw_obj(&es, ty, 0, 0, a);
        static const char *const SK[3] = { "playerskinsash", "playerskincarol", "playerskinmilla" };
        int c = chr_is_carol(G.character) ? 1 : chr_is_milla(G.character) ? 2 : 0;     /* 22782-22784: G1 0 / 2-3 / 4 */
        ui_obj_t *sk = O(SK[c], 0), *tl = c == 1 ? O("playerskincaroltail", 0) : c == 2 ? O("playerskinmillatail", 0) : NULL;
        float x = (ty ? ty->x : 184) + 40, y = 200;                    /* x: the image's x + 40, y: display top + 200 (RE) */
        if (tl) draw_at(tl, x, y, a);
        if (sk) draw_at(sk, x, y, a);
        if (E.cont_t >= 180) {                                        /* 22779: CONTINUE and the cursor */
            ui_obj_t *co = O("continue", 0), *cu = O("crystalcursor", 0);
            if (co) ui_draw_obj(&es, co, 0, 0, 0xffffffff);
            if (cu) ui_draw_obj(&es, cu, 0, 0, 0xffffffff);
        }
    }
    if (E.st == END_LEAVE) {                                          /* FadeEdge_77 wipe (22780), as the menus' */
        float x = E.leave_x;
        if (x + SCREEN_W > 0) r_rect(0, 0, x + SCREEN_W, SCREEN_H, 0xff080000);
        for (int y = 0; y < SCREEN_H; y += 2) {
            float w = 120.f * (1.f - (float)y / SCREEN_H), x0 = x + SCREEN_W;
            if (x0 < 0) { w += x0; x0 = 0; }
            if (w > 0) r_rect(x0, (float)y, w, 2, 0xff080000);
        }
    }
}

void ending_update(const input_t *in) {
    G.frame++;
    switch (E.st) {
    case END_SPACE: space_update(); break;
    case END_HOLD: if (++E.hold_t >= END_HOLD_FRAMES) credits_start(); break;
    case END_CREDITS: case END_THANKS: case END_LEAVE: credits_update(in); break;
    case END_CS: if (cs_update(in)) { unload(); E.st = END_OFF; fe_enter_from_stage(0); return; } break;
    default: fe_enter_from_stage(0); return;
    }
#ifdef FP_HOST_ENV
    if (E.st != END_OFF) printf("end st %d b %d a %.0f pin %d cont %d stats %d\n", E.st, E.B, E.A, E.stats_pinned, E.cont_t, nstats);
#endif
}
void ending_draw(void) {
    r_begin();
    if (E.st == END_SPACE) space_draw();
    else if (E.st == END_CREDITS || E.st == END_THANKS || E.st == END_LEAVE) credits_draw();
    else if (E.st == END_CS) { r_rect(0, 0, SCREEN_W, SCREEN_H, 0xff102040); cs_draw(); }
    else r_rect(0, 0, SCREEN_W, SCREEN_H, 0xff000000);
    r_end();
}

#ifdef FP_HOST_ENV
/* host test command "<f> ENDING": the ending as if Final Dreadnought round 4 had just been cleared (the act need not be
 * loaded: tests run it before Final Dreadnought exists). Never in a Dreamcast build. */
int ending_host_force(void) {
    if (E.st != END_OFF) return -1;
    E.armed = 1;
    if (!results_saved() && G.mode == MODE_PLAY) { cards_commit(); game_save_stage_end((uint32_t)((uint64_t)game_timer() * 1000 / FPS)); save_store(); }
    return game_ending_begin();
}
/* host test command "<f> CUTSCENE": the template scene (cutscene_example.c) on the credits' UI scene; not in the game flow */
int cutscene_host_example(void) {
    if (E.st != END_OFF) return -1;
    music_stop(); snd_stop_all();
    G.mode = MODE_ENDING;
    if (load(2) < 0) { unload(); fe_enter_from_stage(0); return -1; }
    memset(&E, 0, sizeof E); E.st = END_CS;
    cs_begin(&CS_EXAMPLE, &es, &font);
    return 0;
}
/* a state print for tests: the statistics lines */
void ending_host_dump(void) { for (int i = 0; i < nstats; i++) printf("stats|%s\n", stats[i]); }
#endif
