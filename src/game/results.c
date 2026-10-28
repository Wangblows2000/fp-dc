/* DV4 stage clear / results (RE: build/re/spec_cards_results.md Part C; RV: build/orig_traces/dv4_results.csv +
 * screenshots dv4_results_f150..f550).
 * The results overlay gameplay: Sign.B runs 179 -> 200 (5386), the HUD slides up 1 px per frame (4561), "Stage
 * Complete!" builds in (4565: 22 frames, hold 20, then the still image 77 frames), then the panel and the rows
 * reveal every 25 frames with sound 34 (4569-4573, 5389), ScoreValues C = 220 starts the tally (4579), crystals
 * move into the file total (4580-4584, sounds 15/16), D = 150 hides everything and saves (5394/5395), the wipe
 * (5382/5383, sound 56) ends the stage 51 frames later (5385).
 * Path B (5381): Lilac passes the right edge first -> she keeps running right (5388) and the wipe follows D = 150.
 * Path A (5576): the boss exit starts it; the wipe runs when Lilac passes the right edge after D = 150 (5382).
 * Path 3 (results_begin_boss, e.g. RM5 7083 Sign.B = 179): any boss starts it with its own stopped time; no wipe: the
 * boss's post-results script moves the player (results_script_input) and ends the stage (results_request_end).
 * Level data (session 17, Fortune Night 6): res_voice = a results voice for everyone at the 4563 point (8545); res_wipe = path
 * 3 ends with the shared wipe after D = 150 (8546 wipe group on, 8547 sound 56 + LevelCardFade2 / FadeEdge, 8549 F > 50 ->
 * frame 8) instead of a boss script's request. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../fp.h"

static struct {
    int on, pathB, signB, t;
    int svA, svB, svC, svD, svE;
    int hud, ac1, ac1_t, panel, ac3, ac3_t, bonus1_on, bonus2_on, time_on, total_on, ac4_flash;
    int bonus1, bonus2, total, time_frames, tally_snd;
    int wipe, wipe_f; float wipe_x;
    int saved, ach_done;
    int end_req; uint32_t s_held, s_prev;   /* path 3: stage end requested by the boss script; its scripted input */
} R;

void results_reset(void) { memset(&R, 0, sizeof R); snap_register(&R, sizeof R); }
int results_on(void) { return R.on; }
float results_hud_dy(void) { return -(float)R.hud; }

void results_begin(int pathB) {
    if (R.on) return;
    memset(&R, 0, sizeof R);
    R.on = 1; R.pathB = pathB; R.signB = 179;
    R.time_frames = G.frame;
    if (pathB == 2 && aqua_snake_kill_frame >= 0) R.time_frames = aqua_snake_kill_frame;   /* AT4: 4641 stopped the timer at the kill */
    R.bonus2 = G.keystones;
}
/* generic boss results (path 3): Sign.B = 179 like AT4's snake, the stage time stopped at time_frames (the boss's
 * timer-off frames left out), no wipe after D = 150 */
void results_begin_boss(int time_frames) { if (R.on) return; results_begin(3); R.time_frames = time_frames; }
int  results_tally_done(void) { return R.on && R.svD >= 150; }                       /* D = 150: saved, panels hidden */
int  results_frames(void) { return R.on ? R.t : -1; }                                 /* frames since the results started */
void results_script_input(uint32_t held) { R.s_held = held; }                        /* path 3: CTRL.D / jump of the boss script */
void results_request_end(void) { if (R.on) R.end_req = 1; }                          /* path 3: next frame (frame 8) */

static int frames_of(const char *nm) { int a = bank_anim(&G.objs, nm); return a < 0 ? 0 : G.objs.anims[a].count; }
static void imgc(const char *nm, int frame, float x, float y, uint32_t argb);
static void img(const char *nm, int frame, float x, float y) { imgc(nm, frame, x, y, 0xffffffff); }
/* the original images' alpha-87 parts are a separate layer drawn with vertex alpha (palettized textures keep 1 bit) */
#define A87 0x57ffffff
static void imgc(const char *nm, int frame, float x, float y, uint32_t argb) {
    int a = bank_anim(&G.objs, nm);
    if (a < 0) return;
    const anim_t *an = &G.objs.anims[a];
    if (frame >= an->count) frame = an->count - 1;
    if (frame < 0) frame = 0;
    frame_draw(&G.objs, &G.objs.frames[G.objs.seq[an->first + frame]], x, y, 0, argb);
}
static void digits(int v, int n, float x, float y) {
    for (int i = n - 1; i >= 0; i--) { img("hud_digits", v % 10, x + i * 10, y); v /= 10; }
}

/* called every gameplay frame while the results run, before Lilac's update: returns the input she gets */
input_t results_input(const input_t *in) {
    input_t none = {0};
    (void)in;
    if (!R.on) return *in;
    if (R.pathB == 2) return none;                                       /* AT4 (4564 / 4588-4592): moved by results_update */
    if (R.pathB == 3) { none.held = R.s_held; none.pressed = R.s_held & ~R.s_prev; none.released = R.s_prev & ~R.s_held; R.s_prev = R.s_held; return none; }
    if (R.pathB || R.wipe) { none.held = BTN_RIGHT; }                     /* 5388 CTRL.D = 1; wipe keeps her running */
    return none;                                                         /* 4561 CTRL.M = 1 (input locked) */
}

/* achievements at ScoreValues D = 149, one frame before the records save (session 16, RE build/re/spec_achievements.md
 * 2.1: DV4 5580 / 5579, RM5 7119 / 7115, AT4 4662 / 4661): Card Collector (all 10 cards of the stage, taken at any time)
 * and the stage speedrunner (ML.Z = the whole stage time <= the limit). Any mode (Time Attack too), any character, no
 * difficulty test (RE). Per stage index s (level_def_t.stage): 0 Aqua Tunnel, 1 Dragon Valley, 2 Relic Maze. */
static const struct { int stage, card_base, cards_ach, time_ach; uint32_t limit_ms; } ACH_STAGE[] = {
    { 1, 1, 23, 43, 300000 }, { 2, 11, 24, 44, 480000 }, { 0, 21, 25, 45, 300000 },
    { 3, 31, 26, 46, 480000 },   /* Fortune Night (FN6 8789: cards 31-40 -> 26; 8788 / inline: 46 with ML.Z <= 480000 ms) */
    { 4, 41, 27, 47, 420000 },   /* Sky Battalion (SB1 9179: cards 41-50 -> 27; 9177: 47 with ML.Z <= 420000 ms; session 21) */
    { 5, 51, 28, 48, 480000 },   /* Jade Creek (JC5 11624: cards 51-60 -> 28; 11623: 48 with ML.Z <= 480000 ms; session 21) */
    { 6, 61, 29, 49, 540000 },   /* Thermal Base (TB6 12717: cards 61-70 -> records x2 y29; handle_frame_50 inline: 49 with ML.Z <= 540000 ms, session 21c) */
    { 8, 71, 30, 50, 360000 },   /* Trap Hideout (TH3 14067: cards 71-80 -> records x2 y30; 14066: 50 with ML.Z <= 360000 ms) */
    { 7, 81, 31, 51, 480000 },   /* Pangu Lagoon (PL6 13437: cards 81-90 -> records x2 y31; 13436: 51 with ML.Z <= 480000 ms; session 23) */
    { 9, 91, 32, 52, 720000 },   /* Battle Glacier (BG6 15127: cards 91-100 -> records x2 y32; handle_frame_65 inline at D = 149: 52 with ML.Z <= 720000 ms; session 24) */
};
static void ach_results(void) {
    cards_commit();                                    /* this run's cards count (the port commits them at D = 150, after 149) */
    int s = level_cur()->stage;
    uint32_t ms = (uint32_t)((uint64_t)R.time_frames * 1000 / FPS);
#ifdef FP_HOST_ENV
    printf("ach results stage %d ms %u\n", s, (unsigned)ms);   /* host tests (tests/test_achievements.py) */
#endif
    for (unsigned i = 0; i < sizeof ACH_STAGE / sizeof ACH_STAGE[0]; i++) {
        if (ACH_STAGE[i].stage != s) continue;
        int all = 1;
        for (int c = 0; c < 10; c++) all &= save_card_has(ACH_STAGE[i].card_base + c);
        if (all) ach_earn(ACH_STAGE[i].cards_ach);
        if (ms <= ACH_STAGE[i].limit_ms) ach_earn(ACH_STAGE[i].time_ach);
    }
}

static int stage_end(void) {
    int ta = G.time_attack;
    R.on = 0;
    music_stop();
    /* frame 8 -> 9 (RE + RV, spec_achievements 1): an earned, unannounced achievement shows the "Unlocked!" pop-up after the
     * results wipe, before the next stage / menu; the pop-up continues to the same place afterwards (frontend.c) */
    if (ach_pending() && fe_enter_ach_popup(ta, !ta && level_route_next(level_cur()->stage) != NULL) == 0) return 1;
    if (!ta && game_next_stage()) return 1;                                    /* Classic: frame 8 routes to the next stage (DV / AT -> RM) */
    fe_enter_from_stage(ta);
    return 1;
}
/* after the gameplay update */
int results_update(player_t *p) {
    if (!R.on) {
        /* 5381 path B: X > frame width - 50 moving right (DV4 only) */
        if (level_cur()->edge_results && p->x > G.lvl.w - 50 && (p->ground ? p->gsp : p->vx) > 0 && !serp_active()) results_begin(1);
        return 0;
    }
    R.t++;
    if (R.signB < 200) R.signB++;                                             /* 5386 */
    if (R.signB >= 140 && R.hud < 80) R.hud++;                                /* 4561 loop_gui_shiftup_14 */
    if (R.signB >= 150 && R.t == 1) {                                         /* 4563 */
        music_play("bgm/m_clear.adp");
        R.bonus1 = G.stage_crystals;
        R.total = (G.file_slot >= 0 && G.file_slot < SAVE_FILES && !G.time_attack) ? save_file_crystals(&SV.file[G.file_slot]) : 0;   /* TotalCrystals = the file's total (y4) */
        if (level_cur()->res_voice) voice_play_once(level_cur()->res_voice);   /* FN6 8545 (MV.A != 1: everyone in the port), channel 27 */
    }
    if (level_cur()->res_skin_voice && R.t == 76)                             /* 6589-6592 (RV k_end: lilac_howitsdone 75 frames after m_clear; */
        voice_play_once(G.character == CHR_LILAC ? "lilac_howitsdone" : chr_is_carol(G.character) ? "carol_feelmypower" :   /* the same */
                        chr_is_milla(G.character) ? "milla_yay" : "");                                                  /* frame for Carol / Milla INFERRED) */
    if (R.signB >= 180 && !R.ac1) { R.ac1 = 1; R.ac1_t = 0; }                 /* 4565 */
    if (R.ac1) {
        R.ac1_t++;
        /* RV: build-in frames 0..21 one per frame, frame 21 held 20 frames, anim 8 (8005) 77 frames, then 4568/4569 */
        if (R.ac1_t == 21 + 20 + 77 + 1 && !R.panel) { R.panel = 1; R.ac3 = 0; R.ac3_t = 0; }
    }
    if (R.panel && R.ac3 < 5) {                                               /* ActClear3 anim 7: speed 4 (RV +1, then +25) */
        R.ac3_t++;
        int k = R.ac3_t == 1 ? 1 : 1 + (R.ac3_t - 1) / 25;
        if (k > 5) k = 5;
        while (R.ac3 < k) {
            R.ac3++;
            if (R.ac3 == 2) { R.bonus1_on = 1; snd_play_orig(34, 220, 128); }          /* 4570 */
            if (R.ac3 == 3) { R.bonus2_on = 1; snd_play_orig(34, 220, 128); }          /* 4571 */
            if (R.ac3 == 4) { R.time_on = 1; snd_play_orig(34, 220, 128); }            /* 5389 */
            if (R.ac3 == 5) { R.total_on = 1; snd_play_orig(34, 220, 128); }           /* 4573 */
        }
    }
    if (R.svA == 0) R.svC++;                                                  /* 4578 */
    if (R.svA == 0 && R.svC == 220) R.svA = 1;                                /* 4579 */
    if (R.svA == 1) {
        R.svB++;                                                              /* 4583 */
        if (R.svB == 120 && R.bonus1 > 0) { R.svE = R.bonus1 / 100 + 1; snd_play_orig(15, 220, 128); R.tally_snd = 1; }   /* 4580 */
        if (R.svB >= 120) for (int k = 0; k < R.svE && R.bonus1 > 0; k++) { R.bonus1--; R.total++; }                 /* 4581 loop_tally_19 */
        if (R.bonus1 <= 0) {                                                  /* 4584 / 4585 */
            R.svA = 2; snd_play_orig(16, 220, 128); R.ac4_flash = 32;
        }
    }
    if (R.svA == 2) {
        if (R.ac4_flash > 0) R.ac4_flash--;
        if (R.svD < 150) R.svD++;                                             /* 4586 */
        if (R.svD == 149 && !R.ach_done) { R.ach_done = 1; ach_results(); }  /* the D = 149 achievement events (once) */
        if (R.svD == 150 && !R.saved) {                                       /* 5394 records save / 5395 hide */
            R.saved = 1;
            cards_commit();
            game_save_stage_end((uint32_t)((uint64_t)R.time_frames * 1000 / FPS));   /* records, file block, progress (save.c) */
            save_store();
        }
    }
    if (R.pathB == 2) {                                                       /* AT4 (spec_at_flow 2.4): 4564 stand, exit group 4588-4592 */
        if (R.signB >= 150 && R.svD < 150) { if (p->ground) p->gsp = 0; else p->vx = 0; }
        if (R.svD >= 150) {
            obj_t *raft = NULL; for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_RAFT) { raft = &G.o[i]; break; }
            float x2 = raft ? raft->x + 80 : (float)G.lvl.w;                        /* get_box_index(2): 160-px box, hotspot 80 */
            if (p->x < x2 - 32) { p->face = 1; if (p->ground) p->gsp = 3; else p->vx = 3; }   /* 4588 */
            else {                                                                  /* 4589: the raft moves on with her (INFERRED carry) */
                if (p->ground) p->gsp = 0; else p->vx = 0;
                if (raft) { raft->x += 4; p->x += 4; }
                if (!R.wipe && p->x >= 7024) { R.wipe = 1; R.wipe_f = 0; R.wipe_x = -(SCREEN_W + 120); snd_play_orig(56, 230, 128); }   /* 4590 */
            }
        }
    }
    if (R.end_req) return stage_end();                                       /* path 3: the boss script's fade ended */
    /* 5382 / 5383 wipe: path B at D >= 150; path A when Lilac passes the right edge afterwards */
    if (!R.wipe && R.pathB == 3 && level_cur()->res_wipe && R.svD >= 150) {      /* FN6 8546 / 8547 */
        R.wipe = 1; R.wipe_f = 0; R.wipe_x = -(SCREEN_W + 120); snd_play_orig(56, 230, 128);
    }
    if (!R.wipe && R.pathB != 2 && R.pathB != 3 && R.svD >= 150 && (R.pathB || p->x > G.lvl.w - 50)) {
        R.wipe = 1; R.wipe_f = 0; R.wipe_x = -(SCREEN_W + 120); snd_play_orig(56, 230, 128);
    }
    if (R.wipe) {
        R.wipe_x = R.wipe_x + 32 > 0 ? 0 : R.wipe_x + 32;                     /* 5384 */
        if (++R.wipe_f > 50) return stage_end();                              /* 5385: next frame (stage card / unlocks: not ported) */
    }
    return 0;
}

void results_draw(void) {
    if (!R.on) return;
    if (R.ac1 && R.svD < 150) {
        int nf = frames_of("res_complete");
        int f = R.ac1_t - 1 < nf - 1 ? R.ac1_t - 1 : nf - 1;
        if (f == nf - 1) imgc("res_complete_t", 0, 53, 32, A87);
        img("res_complete", f, 53, 32);                                         /* RV: (53, 32) at 426x240 */
    }
    if (R.panel && R.svD < 150) {
        imgc("res_panel_t", 0, 125, 88, A87);
        img("res_panel", 0, 125, 88);                                           /* RV */
        img("res_rows", R.ac3, 141, 88);                                        /* RV; frames 10849..10837 */
        if (R.bonus1_on) digits(R.bonus1 > 9999 ? 9999 : R.bonus1, 4, 161, 98);   /* RV digit positions */
        if (R.bonus2_on) digits(R.bonus2 > 999 ? 999 : R.bonus2, 3, 259, 98);
        if (R.time_on) {
            int cs = R.time_frames * 100 / FPS, mm = cs / 6000, ss = cs / 100 % 60;
            digits(mm > 99 ? 99 : mm, 2, 216, 117); digits(ss, 2, 242, 117); digits(cs % 100, 2, 269, 117);
        }
        if (R.total_on) {
            int fl = (R.ac4_flash > 0 && ((32 - R.ac4_flash) / 2) % 2 == 0) ? 1 : 0;     /* anim 7 [10660,10844] speed 50 */
            imgc("res_bar_t", fl, 165, 212, A87);
            img("res_bar", fl, 165, 212);
            digits(R.total > 9999999 ? 9999999 : R.total, 7, 187, 219);
        }
    }
    if (R.wipe) {                                                               /* LevelCardFade2 + FadeEdge_77 */
        float x = R.wipe_x;
        if (x + SCREEN_W > 0) r_rect(0, 0, x + SCREEN_W, SCREEN_H, 0xff080000);
        for (int y = 0; y < SCREEN_H; y += 2) {
            float w = 120.f * (1.f - (float)y / SCREEN_H);
            float x0 = x + SCREEN_W; if (x0 < 0) { w += x0; x0 = 0; }
            if (w > 0) r_rect(x0, (float)y, w, 2, 0xff080000);
        }
    }
}
int results_state(void) { return R.on ? 1 + R.svA + (R.wipe ? 10 : 0) : 0; }
