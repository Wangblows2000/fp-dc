/* Game state, camera, recovered-level drawing, HUD. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../fp.h"

game_t G;

#define CARD_RISE1 24
#define CARD_RISE2 48
#define CARD_FADE0 84
#define CARD_FADE1 108
#define CARD_OUT0  168
#define CARD_DONE  192

#ifndef FP_LEVEL
#define FP_LEVEL "dv1"
#endif

static void level_start(void) {
    for (int i = 0; i < G.lvl.nobj; i++)
        if (G.lvl.objs[i].type == OT_START) { G.p.spawn_x = G.lvl.objs[i].x; G.p.spawn_y = G.lvl.objs[i].y; }
    /* snap spawn to the floor below the start marker */
    float sy, a;
    if (floor_probe(&G.lvl, G.p.spawn_x, G.p.spawn_y - 32, 400, 1, &sy, &a)) G.p.spawn_y = sy;
    /* crystals, lives, energy persist across restarts (RV: debug restart kept them) */
    player_reset(&G.p, G.p.spawn_x, G.p.spawn_y);
    objects_spawn();
    G.clear_t = 0; G.frame = 0; G.card_t = 0; G.paused = 0;
    camera_reset();
    camera_update();
}

/* stage music per level dir (RV from the original's play_music events: m_stage1 frame 20,
 * m_stage1b frames 21-22, m_boss frame 23 (boss)); DV4's start track is INFERRED (m_stage1b) */
static const char *level_music(const char *dir) {
    if (!strcmp(dir, "dv1")) return "bgm/m_stage1.adp";
    return "bgm/m_stage1b.adp";
}
static int act_of(const char *dir) { return dir[2] - '0'; }

static void bank_free(bank_t *b) { free(b->frames); free(b->anims); free(b->seq); memset(b, 0, sizeof *b); }

void game_unload_all(void) {
    bank_free(&G.lilac); bank_free(&G.objs);
    free(G.lvl.pieces); free(G.lvl.objs); free(G.lvl.blob);
    memset(&G.lvl, 0, sizeof G.lvl);
    G.npar = 0; G.nobjs = 0;
    r_reset();
}

/* every level lives in its own data dir (own palettes + Lilac bank; tools/build_level.sh) */
int game_load_level(const char *dir) {
    game_unload_all();
    snprintf(G.lvdir, sizeof G.lvdir, "%s", dir);
    int bad = 0;
    char nm[48];
    for (int i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "%s/pal%d.pal", dir, i); bad |= pal_load_file(nm) < 0; }
    snprintf(nm, sizeof nm, "%s/lilac2", dir); bad |= bank_load(&G.lilac, nm) < 0;
    snprintf(nm, sizeof nm, "%s/%s_obj", dir, dir); bad |= bank_load(&G.objs, nm) < 0;
    snprintf(nm, sizeof nm, "%s/%s", dir, dir); bad |= level_load(&G.lvl, nm) < 0;
    if (bad) { plat_log("level %s: data load failed\n", dir); return -1; }
    int want = 0;
    for (int i = 0; i < G.lvl.nobj; i++)
        if (G.lvl.objs[i].type >= OT_PARALLAX && G.lvl.objs[i].anim != 0xffff && G.lvl.objs[i].anim + 1 > want)
            want = G.lvl.objs[i].anim + 1;
    for (G.npar = 0; G.npar < want && G.npar < 12; G.npar++) {
        snprintf(nm, sizeof nm, "%s/%s_par%d", dir, dir, G.npar);
        if (tex_load_file(&G.par[G.npar], nm) < 0) break;
    }
    G.p.spawn_x = G.p.spawn_y = 0;
    hud_reset();
    level_start();
    music_play(level_music(dir));
    return 0;
}

/* ---- FP_DEBUG_SELECT: debug level select ------------------------------------------------
 * Frame indices are the original's "Setting frame: N" values; names were verified from the
 * stage title card shown after GameManager::set_frame(N) in the running original
 * (tools/orig/goto_frame.sh, RUNTIME_MAP.md "Frame index table"). Stage numbers on the cards
 * depend on the character route, so only names are listed. */
typedef struct { const char *name; int act, frame; const char *dir; } sel_entry_t;
static const sel_entry_t SEL[] = {
    {"DRAGON VALLEY", 1, 20, "dv1"}, {"DRAGON VALLEY", 2, 21, "dv2"}, {"DRAGON VALLEY", 3, 22, "dv3"}, {"DRAGON VALLEY", 4, 23, "dv4"},
    {"AQUA TUNNEL", 1, 16, "at1"}, {"AQUA TUNNEL", 2, 17, "at2"}, {"AQUA TUNNEL", 3, 18, "at3"}, {"AQUA TUNNEL", 4, 19, "at4"},
    {"RELIC MAZE", 1, 24, "rm1"}, {"RELIC MAZE", 2, 25, "rm2"}, {"RELIC MAZE", 3, 26, "rm3"}, {"RELIC MAZE", 4, 27, "rm4"}, {"RELIC MAZE", 5, 28, "rm5"},
    {"FORTUNE NIGHT", 1, 29, "fn1"}, {"FORTUNE NIGHT", 2, 30, "fn2"}, {"FORTUNE NIGHT", 3, 31, "fn3"}, {"FORTUNE NIGHT", 4, 32, "fn4"}, {"FORTUNE NIGHT", 5, 33, "fn5"}, {"FORTUNE NIGHT", 6, 34, "fn6"},
    {"SKY BATTALION", 1, 35, "sb1"}, {"SKY BATTALION", 2, 36, "sb2"}, {"SKY BATTALION", 3, 37, "sb3"}, {"SKY BATTALION", 4, 38, "sb4"},
    {"JADE CREEK", 1, 39, "jc1"}, {"JADE CREEK", 2, 40, "jc2"}, {"JADE CREEK", 3, 41, "jc3"}, {"JADE CREEK", 4, 42, "jc4"}, {"JADE CREEK", 5, 43, "jc5"},
    {"THERMAL BASE", 1, 44, "tb1"}, {"THERMAL BASE", 2, 45, "tb2"}, {"THERMAL BASE", 3, 46, "tb3"}, {"THERMAL BASE", 4, 47, "tb4"}, {"THERMAL BASE", 5, 48, "tb5"}, {"THERMAL BASE", 6, 49, "tb6"},
    {"PANGU LAGOON", 1, 50, "pl1"}, {"PANGU LAGOON", 2, 51, "pl2"}, {"PANGU LAGOON", 3, 52, "pl3"}, {"PANGU LAGOON", 4, 53, "pl4"}, {"PANGU LAGOON", 5, 54, "pl5"}, {"PANGU LAGOON", 6, 55, "pl6"},
    {"TRAP HIDEOUT", 1, 56, "th1"}, {"TRAP HIDEOUT", 2, 57, "th2"}, {"TRAP HIDEOUT", 3, 58, "th3"},
    {"BATTLE GLACIER", 1, 59, "bg1"}, {"BATTLE GLACIER", 2, 60, "bg2"}, {"BATTLE GLACIER", 3, 61, "bg3"}, {"BATTLE GLACIER", 4, 62, "bg4"}, {"BATTLE GLACIER", 5, 63, "bg5"}, {"BATTLE GLACIER", 6, 64, "bg6"},
    {"FINAL DREADNOUGHT", 1, 65, "fd1"}, {"FINAL DREADNOUGHT", 2, 66, "fd2"}, {"FINAL DREADNOUGHT", 3, 67, "fd3"}, {"FINAL DREADNOUGHT", 4, 68, "fd4"},
    {"FINAL DREADNOUGHT", 5, 69, "fd5"}, {"FINAL DREADNOUGHT", 6, 70, "fd6"}, {"FINAL DREADNOUGHT", 7, 71, "fd7"}, {"FINAL DREADNOUGHT", 8, 72, "fd8"},
};
#define NSEL ((int)(sizeof SEL / sizeof SEL[0]))
#define SEL_ROWS 16
static uint8_t sel_have[NSEL];

static void sel_scan(void) {           /* levels.txt: one built level dir per line */
    size_t n; char *d = plat_load_file("levels.txt", &n);
    memset(sel_have, 0, sizeof sel_have);
    if (!d) return;
    for (size_t i = 0; i < n;) {
        size_t j = i; while (j < n && d[j] != '\n' && d[j] != '\r') j++;
        for (int k = 0; k < NSEL; k++)
            if (strlen(SEL[k].dir) == j - i && !memcmp(SEL[k].dir, d + i, j - i)) sel_have[k] = 1;
        i = j + 1;
    }
    free(d);
}

static void sel_update(const input_t *in) {
    if (G.sel_msg_t) G.sel_msg_t--;
    if (in->pressed & BTN_DOWN) G.sel = (G.sel + 1) % NSEL;
    if (in->pressed & BTN_UP) G.sel = (G.sel + NSEL - 1) % NSEL;
    if (in->pressed & BTN_RIGHT) G.sel = G.sel + SEL_ROWS < NSEL ? G.sel + SEL_ROWS : NSEL - 1;
    if (in->pressed & BTN_LEFT) G.sel = G.sel - SEL_ROWS >= 0 ? G.sel - SEL_ROWS : 0;
    if (G.sel < G.sel_top) G.sel_top = G.sel;
    if (G.sel >= G.sel_top + SEL_ROWS) G.sel_top = G.sel - SEL_ROWS + 1;
    if (in->pressed & BTN_SPECIAL) { G.in_select = 0; fe_enter(); return; }   /* B: back to the title */
    if (in->pressed & (BTN_JUMP | BTN_START)) {
        if (!sel_have[G.sel]) { G.sel_msg_t = 90; return; }
        if (game_load_level(SEL[G.sel].dir) == 0) { G.in_select = 0; G.mode = MODE_PLAY; G.card_t = CARD_DONE; }
        else G.sel_msg_t = 90;
    }
}

static void sel_draw(void) {
    char buf[48];
    r_begin();
    r_rect(0, 0, SCREEN_W, SCREEN_H, 0xff101830);
    text_draw(12, 8, "DEBUG LEVEL SELECT", 0xffffe040, 2);
    text_draw(12, 24, "FRAME = ORIGINAL SET FRAME INDEX", 0xff8090b0, 1);
    for (int r = 0; r < SEL_ROWS && G.sel_top + r < NSEL; r++) {
        int i = G.sel_top + r;
        uint32_t c = i == G.sel ? 0xffffffff : sel_have[i] ? 0xffa0c0ff : 0xff505868;
        snprintf(buf, sizeof buf, "%s %d", SEL[i].name, SEL[i].act);
        if (i == G.sel) r_rect(8, 36 + r * 11, SCREEN_W - 16, 10, 0xff304070);
        text_draw(14, 38 + r * 11, buf, c, 1);
        snprintf(buf, sizeof buf, "F%d", SEL[i].frame);
        text_draw(170, 38 + r * 11, buf, c, 1);
        text_draw(206, 38 + r * 11, sel_have[i] ? "READY" : "NOT CONVERTED", c, 1);
    }
    text_draw(12, SCREEN_H - 14, G.sel_msg_t ? "LEVEL DATA NOT ON DISC" : "A START  B TITLE  UP DOWN  LEFT RIGHT PAGE", G.sel_msg_t ? 0xffff6060 : 0xff8090b0, 1);
    r_end();
}

/* ---- sounds: original Assets.dat sound IDs (Media::play_id, RUNTIME_MAP "Sound IDs") */
static const int SND_IDS[] = { 4, 5, 8, 9, 17, 21, 25, 26, 27, 28, 37, 53, 56, 63, 64, 81, 100, 174, 176, 288, 396, 405 };
static int snd_handle[512];
static const char *VOICES[] = { "lilac_attack0", "lilac_attack1", "lilac_attack2", "lilac_attack3", "lilac_uppercut0",
    "lilac_uppercut1", "lilac_uppercut2", "lilac_uppercut3", "lilac_cyclone1", "lilac_cyclone2", "lilac_hurt0",
    "lilac_hurt1", "lilac_hurt2", "lilac_hurt3", "lilac_die" };
static int voice_handle[16];

void snd_play_orig(int id, int vol, int pan) { if (id >= 0 && id < 512 && snd_handle[id] >= 0) snd_play(snd_handle[id], vol, pan); }
void voice_play(const char *name) {
    for (int i = 0; i < (int)(sizeof VOICES / sizeof VOICES[0]); i++)
        if (!strcmp(VOICES[i], name)) { if (voice_handle[i] >= 0) snd_play(voice_handle[i], 255, 128); return; }
}

int game_init(void) {
    memset(&G, 0, sizeof G);
    char nm[48];
    for (int i = 0; i < 512; i++) snd_handle[i] = -1;
    for (int i = 0; i < (int)(sizeof SND_IDS / sizeof SND_IDS[0]); i++) {
        snprintf(nm, sizeof nm, "sfx/s%d.wav", SND_IDS[i]); snd_handle[SND_IDS[i]] = snd_load(nm);
    }
    for (int i = 0; i < (int)(sizeof VOICES / sizeof VOICES[0]); i++) { snprintf(nm, sizeof nm, "sfx/%s.wav", VOICES[i]); voice_handle[i] = snd_load(nm); }
    /* legacy handles used by gameplay code, now mapped to the original sound IDs */
    G.sfx.jump = snd_handle[8]; G.sfx.attack = snd_handle[21]; G.sfx.boost = snd_handle[53];
    G.sfx.hurt = snd_handle[17]; G.sfx.crystal = snd_handle[27]; G.sfx.explode = snd_handle[81]; G.sfx.hit = snd_handle[25];
#ifdef FP_HOST_ENV
    /* host tests: FP_LEVEL starts a level directly (no front end) */
    if (getenv("FP_LEVEL")) { G.mode = MODE_PLAY; int r = game_load_level(getenv("FP_LEVEL")); G.card_t = getenv("FP_CARD") ? 0 : CARD_DONE; return r; }
#endif
    return fe_enter();
}

/* from the front end: character/stage chosen */
void game_start_adventure(const char *dir, int time_attack) {
    G.time_attack = time_attack;
    G.p.energy_init = 0;                       /* fresh run: lives/crystals reset by player_reset */
    G.mode = MODE_PLAY;
    if (game_load_level(dir) < 0) { fe_enter(); return; }
}

/* developer level select: reachable only from the title screen (L+R within 2 s, frontend.c) */
void game_dev_select(void) {
    game_unload_all();
    G.mode = MODE_SELECT; G.in_select = 1; sel_scan();
}

/* Camera: MasterCamera_408, recovered (RE; docs/RE_NOTES.md "Camera"). A,B = centre target; W = landing
 * ease; G = look-ahead; H = look up/down; S,T = display offsets; N,O = look-up / crouch hold counters.
 * Constants are the object's alterables at DV1 start (I -16, J 0, K -48, L 16, P 120, Q 64, R 96, X 213,
 * Y 120; bounds C 0 .. E, D 0 .. F). Shake (Z) is not used yet. */
#define CAM_I (-16.f)
#define CAM_J 0.f
#define CAM_K (-48.f)
#define CAM_L 16.f
#define CAM_P 120.f
#define CAM_Q 64.f
#define CAM_R 96.f
#define CAM_HX 213.f
#define CAM_HY 120.f
void camera_reset(void) {
    const player_t *p = &G.p;
    G.cam.A = p->x; G.cam.B = p->y - 14.f; G.cam.W = G.cam.G = G.cam.H = G.cam.S = G.cam.T = G.cam.N = G.cam.O = 0;
    G.cam.V = p->ground;
    float C = 0, E = (float)G.lvl.w, D = 0, F = (float)G.lvl.h;
    if (G.cam.A < C + CAM_HX) G.cam.A = C + CAM_HX;
    if (G.cam.A > E - CAM_HX) G.cam.A = E - CAM_HX;
    if (G.cam.B < D + CAM_HY) G.cam.B = D + CAM_HY;
    if (G.cam.B > F - CAM_HY) G.cam.B = F - CAM_HY;
}

void camera_update(void) {
    const player_t *p = &G.p;
    float X = p->x, Y = p->y - 14.f;                    /* movement object X, Y (reference point) */
    float C = 0, E = (float)G.lvl.w, D = 0, F = (float)G.lvl.h;
    int L = p->ground;
    float Bsp = p->ground ? p->gsp : p->vx;             /* movement B */
    float Q = (float)p->face;
    int M = p->state == 4 || p->state == 3 ? 5 : p->state == 8 ? 19 : p->look;   /* boost/charge M5, kick windup M19 */
    /* vertical */
    if (L == 1) {
        if (L != G.cam.V) G.cam.W = G.cam.B - Y;         /* landing: keep the current offset, then ease it */
        if (G.cam.W > 0) G.cam.W = fmaxf(G.cam.W - 4.f, 0.f);
        if (G.cam.W < 0) G.cam.W = fminf(G.cam.W + 4.f, 0.f);
        G.cam.B = fminf(fmaxf(Y + G.cam.W, D + CAM_HY), F - CAM_HY);
    }
    G.cam.V = L;
    /* horizontal dead zone */
    if (X < G.cam.A + CAM_I && C + CAM_HX < G.cam.A) G.cam.A = X - CAM_I;
    if (CAM_J + G.cam.A < X && G.cam.A < E - CAM_HX) G.cam.A = X - CAM_J;
    if (L == 0) {                                        /* air dead zone */
        if (Y < G.cam.B + CAM_K && D + CAM_HY < G.cam.B) G.cam.B = Y - CAM_K;
        if (CAM_L + G.cam.B < Y && G.cam.B < F - CAM_HY) G.cam.B = Y - CAM_L;
    }
    /* look up / crouch holds */
    G.cam.N = (M == 2 && L && Bsp == 0) ? G.cam.N + 1 : 0;
    if (G.cam.N < CAM_P && G.cam.O < CAM_P && G.cam.H < 0) G.cam.H = fminf(G.cam.H + 2.f, 0.f);
    if (G.cam.N >= CAM_P) G.cam.H = fmaxf(G.cam.H - 2.f, -CAM_R);
    G.cam.O = (M == 3 && L && Bsp == 0) ? G.cam.O + 1 : 0;
    if (G.cam.O < CAM_P && G.cam.N < CAM_P && G.cam.H > 0) G.cam.H = fmaxf(G.cam.H - 2.f, 0.f);
    if (G.cam.O >= CAM_P) G.cam.H = fminf(G.cam.H + 2.f, CAM_R);
    /* look-ahead */
    if (Bsp >= 5.f && Q == 1 && L) G.cam.G = fminf(G.cam.G + 2.f, CAM_Q);
    if (Bsp <= -5.f && Q == -1 && L) G.cam.G = fmaxf(G.cam.G - 2.f, -CAM_Q);
    if (M == 5) G.cam.G = Q == 1 ? fminf(G.cam.G + 2.5f, CAM_Q) : fmaxf(G.cam.G - 2.5f, -CAM_Q);
    else if (M == 19) G.cam.G = Q == 1 ? fminf(G.cam.G + 2.f, CAM_Q) : fmaxf(G.cam.G - 2.f, -CAM_Q);
    {   /* event 1357 + loop tail: decay when airborne or slow (not boosting / kicking) */
        int slow = Bsp < 5.f && Bsp > -5.f && M != 5 && M != 19;
        if (G.cam.G > 0 && (!L || slow)) G.cam.G = fmaxf(G.cam.G - 2.f, 0.f);
        else if (G.cam.G < 0 && (!L || slow)) G.cam.G = fminf(G.cam.G + 2.f, 0.f);
    }
    /* display offsets (events 2877-2886, no shake) */
    float S = G.cam.G, T = G.cam.H;
    if (G.cam.A <= C + CAM_HX - S) S = fminf(0.f, fmaxf(S, C + CAM_HX - G.cam.A));
    if (E - CAM_HX - S <= G.cam.A) S = fmaxf(0.f, fminf(S, E - CAM_HX - G.cam.A));
    if (S > 0) S = fminf(S, fmaxf(0.f, S - (G.cam.A - X)));
    else if (S < 0) S = fmaxf(S, fminf(0.f, S - (G.cam.A - X)));
    if (G.cam.B <= D + CAM_HY - T) T = fminf(0.f, fmaxf(T, D + CAM_HY - G.cam.B));
    if (F - CAM_HY - T <= G.cam.B) T = fmaxf(0.f, fminf(T, F - CAM_HY - G.cam.B));
    if (T > 0) T = fminf(T, fmaxf(0.f, T - (G.cam.B - Y)));
    else if (T < 0) T = fmaxf(T, fminf(0.f, T - (G.cam.B - Y)));
    G.cam.S = S; G.cam.T = T;
    /* event 3539 + Frame::set_display_center */
    float cx = floorf(G.cam.A + S), cy = floorf(G.cam.B + T);
    G.camx = fminf(fmaxf(cx - 213.f, 0.f), (float)G.lvl.w - SCREEN_W);
    G.camy = fminf(fmaxf(cy - 120.f, 0.f), (float)G.lvl.h - SCREEN_H);
}

/* ---- stage title card (RV images levelcardintro2/3 = name / "STAGE n"; sequence from a 10 fps
 * capture of the original: STAGE rises, name rises, hold on black, stage fades in, hold, both slide out
 * while the HUD drops in; frame counts are approximate) */

static void next_act(void) {
    char d[8]; snprintf(d, sizeof d, "%s", G.lvdir);
    int a = act_of(d);
    if (a >= 4 || G.time_attack) { G.mode = MODE_RESULTS; G.res_t = 0; G.res_time = G.frame; music_play("bgm/m_clear.adp"); return; }
    d[2] = (char)('0' + a + 1);
    int frame = G.frame;                         /* RV: the stage timer continues across acts */
    if (game_load_level(d) < 0) { fe_enter(); return; }
    G.frame = frame; G.card_t = 0;
}

static void pause_update(const input_t *in) {
    uint32_t p = in->pressed;
    if (p & BTN_UP) { G.pause_sel = (G.pause_sel + 2) % 3; snd_play_orig(64, 220, 128); }
    if (p & BTN_DOWN) { G.pause_sel = (G.pause_sel + 1) % 3; snd_play_orig(64, 220, 128); }
    if (p & BTN_START) { G.paused = 0; return; }
    if (p & BTN_JUMP) {
        snd_play_orig(63, 230, 128);
        if (G.pause_sel == 0) G.paused = 0;
        else if (G.pause_sel == 1) { int f = G.frame; game_load_level(G.lvdir); G.frame = f; }   /* RESTART act */
        else fe_enter();                                                                         /* QUIT to title */
    }
}

void game_update(const input_t *in) {
    if (G.mode == MODE_FRONTEND) { fe_update(in); return; }
    if (G.in_select) { sel_update(in); return; }
    if (G.mode == MODE_RESULTS) {
        G.res_t++;
        if (G.res_t == 150) music_play("bgm/m_results.adp");
        if (G.res_t > 120 && (in->pressed & (BTN_JUMP | BTN_START))) { snd_play_orig(63, 230, 128); fe_enter(); }
        return;
    }
    if (G.card_t < CARD_DONE) {                   /* intro card: no control, timer not running */
        G.card_t++;
        input_t none = {0};
        player_update(&G.p, &none);
        camera_update();
        return;
    }
    if (G.paused) { pause_update(in); return; }
    if (in->pressed & BTN_START) { G.paused = 1; G.pause_sel = 0; return; }
    G.frame++;
    if (G.clear_t) { if (++G.clear_t > 60) next_act(); return; }
    player_update(&G.p, in);
    objects_update();
    camera_update();
    /* act end: the original moves to the next frame when Lilac reaches the right end of the act
     * (RV: running right at X 14700+ in DV1 set frame 21); the port uses the level's right edge */
    if (G.p.x > G.lvl.w - 40 && !G.clear_t) G.clear_t = 1;
}

/* Parallax bands: positions/images from the level's levelparallaxpiece objects.
 * Scroll factors are RECONSTRUCTED (the original moves them in event code). */
static void draw_parallax(void) {
    int k = 0;
    float vmax = (float)(G.lvl.h - SCREEN_H);
    for (int i = 0; i < G.lvl.nobj; i++) {
        const lvobj_t *o = &G.lvl.objs[i];
        if (o->type < OT_PARALLAX || o->anim >= G.npar) continue;
        const tex_t *t = &G.par[o->anim];
        float fx = k == 0 ? 0.f : 0.04f + 0.07f * k;
        float y = (float)o->y - (vmax > 0 ? G.camy / vmax : 0) * 0.55f * vmax * (k == 0 ? 0.f : 1.f);
        if (k == 0) {    /* sky: stretch to the screen */
            r_sprite(t, 0, 0, t->srcw, t->srch, 0, 0, SCREEN_W, SCREEN_H, 0, 0xffffffff);
        } else if (y < SCREEN_H && y + t->srch > 0) {
            float off = fmodf(G.camx * fx, (float)t->srcw);
            for (float x = -off; x < SCREEN_W; x += t->srcw)
                r_sprite(t, 0, 0, t->srcw, t->srch, floorf(x), floorf(y), t->srcw, t->srch, 0, 0xffffffff);
        }
        k++;
    }
}

static void draw_pieces(int lmin, int lmax) {
    const level_t *l = &G.lvl;
    int cx = (int)floorf(G.camx), cy = (int)floorf(G.camy);
    for (int i = 0; i < l->npieces; i++) {
        const piece_t *p = &l->pieces[i];
        if (p->layer < lmin || p->layer > lmax || (p->pad & 1)) continue;   /* bit0: invisible (blend alpha 0) */
        int sx = p->x - cx, sy = p->y - cy;
        if (sx >= SCREEN_W || sy >= SCREEN_H || sx + p->w <= 0 || sy + p->h <= 0) continue;
        r_sprite(&l->atlas[p->atlas], p->u, p->v, p->w, p->h, (float)sx, (float)sy, p->w, p->h, 0, 0xffffffff);
    }
}

static const uint8_t font3x5[][3] = {
    {31,17,31},{0,31,0},{29,21,23},{21,21,31},{7,4,31},{23,21,29},{31,21,29},{1,1,31},{31,21,31},{23,21,31},
    {30,5,30},{31,21,10},{14,17,17},{31,17,14},{31,21,17},{31,5,1},{14,17,29},{31,4,31},{17,31,17},{8,16,15},
    {31,4,27},{31,16,16},{31,2,31},{31,1,30},{14,17,14},{31,5,2},{14,25,30},{31,5,26},{18,21,9},{1,31,1},{15,16,15},
    {7,24,7},{31,8,31},{27,4,27},{3,28,3},{25,21,19}
};

void text_draw(float x, float y, const char *s, uint32_t argb, int scale) {
    for (; *s; s++, x += 4 * scale) {
        int g = -1;
        if (*s >= '0' && *s <= '9') g = *s - '0';
        else if (*s >= 'A' && *s <= 'Z') g = 10 + *s - 'A';
        else if (*s == ':') { r_rect(x + scale, y + scale, scale, scale, argb); r_rect(x + scale, y + 3 * scale, scale, scale, argb); continue; }
        if (g < 0) continue;
        for (int c = 0; c < 3; c++)
            for (int r = 0; r < 5; r++)
                if (font3x5[g][c] & (1 << r)) r_rect(x + c * scale, y + r * scale, scale, scale, argb);
    }
}

/* ---- HUD: original GUI images (object bank anims hud_*), layout measured in the original at its
 * native 426x240 by template matching a screenshot (RUNTIME_MAP "HUD"). All coordinates = image top-left. */
#define NHUD 15
static int hud_anim[NHUD] = { -2 };
void hud_reset(void) { hud_anim[0] = -2; }
static void hud_init(void) {
    if (hud_anim[0] == -2) {
        static const char *nm[NHUD] = { "hud_frame", "hud_fill", "hud_petals", "hud_icon", "hud_clock", "hud_digits",
            "card_stage", "card_name", "pause", "res_complete", "res_results", "res_panel", "res_rows", "res_digits", "res_small" };
        for (int i = 0; i < NHUD; i++) hud_anim[i] = bank_anim(&G.objs, nm[i]);
    }
}
static void hud_img(int which, int frame, float x, float y, int w_override) {
    hud_init();
    int a = hud_anim[which];
    if (a < 0) return;
    const anim_t *an = &G.objs.anims[a];
    if (frame < 0) frame = 0;
    if (frame >= an->count) frame = an->count - 1;
    const frame_t *f = &G.objs.frames[G.objs.seq[an->first + frame]];
    int w = w_override >= 0 ? w_override : f->w;
    if (w <= 0) return;
    r_sprite(&G.objs.tex, f->u, f->v, w, f->h, x, y, w, f->h, 0, 0xffffffff);
}
/* draw a bank frame by its hotspot (cards, pause, results are placed by their original hot spots) */
static void hud_img_hot(int which, int frame, float x, float y) {
    hud_init();
    int a = hud_anim[which];
    if (a < 0) return;
    const anim_t *an = &G.objs.anims[a];
    if (frame >= an->count) frame = an->count - 1;
    if (frame < 0) frame = 0;
    frame_draw(&G.objs, &G.objs.frames[G.objs.seq[an->first + frame]], x, y, 0, 0xffffffff);
}

static void hud_number(int v, int digits, float x, float y) {
    for (int i = digits - 1; i >= 0; i--) { hud_img(5, v % 10, x + i * 10, y, -1); v /= 10; }
}

static float hud_dy;
void hud_draw(void) {
    const player_t *p = &G.p;
    /* HUD drops in from the top when the card slides out (RV capture); offset applied to all items */
    hud_dy = G.card_t >= CARD_DONE ? 0.f : G.card_t < CARD_OUT0 ? -40.f : -40.f + 40.f * (G.card_t - CARD_OUT0) / (CARD_DONE - CARD_OUT0);
    if (G.card_t < CARD_OUT0) return;
    /* energy meter: fill = guienergy alt A / 100 as x scale (RV en_xs); shine loop while full (RV en_img) */
    int full = p->energy >= PHYS_ENERGY_MAX;
    int fill_frame = full ? (G.frame / 2) % 20 : 0;      /* anim 1, speed 50 = one frame per 2 ticks */
    hud_img(0, 0, 8, 8 + hud_dy, -1);                             /* frame interior is opaque: fill goes on top */
    hud_img(1, fill_frame, 16, 10 + hud_dy, (int)(100.f * p->energy / PHYS_ENERGY_MAX + 0.5f));
    /* life petals: counter value = half-petals 0..14 (RV GUIRings_125, max 14) */
    if (p->hp > 0) hud_img(2, p->hp, 8, 19 + hud_dy, -1);
    /* character head + crystal icon, lives (2 digits), crystals (3 digits) */
    hud_img(3, 0, 128, 8 + hud_dy, -1);
    hud_number(p->lives < 0 ? 0 : p->lives > 99 ? 99 : p->lives, 2, 144, 8 + hud_dy);
    hud_number(p->crystals < 0 ? 0 : p->crystals > 999 ? 999 : p->crystals, 3, 176, 8 + hud_dy);
    /* clock + stage timer mm:ss:cc (RV GUITimerM/S/Milli counters; colons are part of the clock image) */
    hud_img(4, 0, 322, 0 + hud_dy, -1);
    int cs = G.frame * 100 / FPS, mm = cs / 6000, ss = cs / 100 % 60;
    hud_number(mm > 99 ? 99 : mm, 2, 343, 8 + hud_dy);
    hud_number(ss, 2, 369, 8 + hud_dy);
    hud_number(cs % 100, 2, 395, 8 + hud_dy);
    char buf[32];

#ifdef FP_PROFILE
    snprintf(buf, sizeof buf, "U%u D%u", (unsigned)G.prof_update_us, (unsigned)G.prof_draw_us);
    text_draw(8, SCREEN_H - 12, buf, 0xff80ff80, 1);
#else
    (void)buf;
#endif
}

static void card_draw(void) {
    int t = G.card_t, act = act_of(G.lvdir);
    if (t >= CARD_DONE) return;
    (void)act;
    /* black until the stage fades in */
    if (t < CARD_FADE1) r_fade(t < CARD_FADE0 ? 1.f : 1.f - (float)(t - CARD_FADE0) / (CARD_FADE1 - CARD_FADE0));
    float ys = 159, yn = 185;                      /* RV final positions (sel_20 capture) */
    float rise1 = t < CARD_RISE1 ? 1.f - (float)t / CARD_RISE1 : 0.f;
    float rise2 = t < CARD_RISE1 ? 1.f : t < CARD_RISE2 ? 1.f - (float)(t - CARD_RISE1) / (CARD_RISE2 - CARD_RISE1) : 0.f;
    float out = t < CARD_OUT0 ? 0.f : (float)(t - CARD_OUT0) / (CARD_DONE - CARD_OUT0);
    rise1 *= rise1; rise2 *= rise2;                /* decelerating rise */
    hud_img_hot(6, 0, 213 - out * 300, ys + rise1 * 100);         /* "STAGE 1": Dragon Valley is stage 1 */
    if (t >= CARD_RISE1) hud_img_hot(7, 0, 213 + out * 300, yn + rise2 * 80);
}

static void pause_draw(void) {
    if (!G.paused) return;
    hud_img_hot(8, 1, 213, 101);                  /* RV image 6685: Paused + RESUME/RESTART/QUIT */
    /* selection marker: original crystal cursor not in the level bank; the item icons stay as drawn
     * (cursor placement INFERRED): small red petal left of the selected row */
    hud_img(2, 1, 150, 136 + G.pause_sel * 23, -1);
}

/* results: original images (Stage Complete! build-in, Results banner, panel, rows, bonus digits);
 * the original sequence was not observed (it follows the Dragon Valley boss), so layout and timing
 * here are INFERRED from the assets. */
static void results_draw(void) {
    int t = G.res_t;
    r_fade(1.f);
    int nf = 0;
    hud_init();
    if (hud_anim[9] >= 0) nf = G.objs.anims[hud_anim[9]].count;
    /* top-left placement: banner 320x48 centred; panel 180x48 and rows 148x40 centred */
    hud_img(9, t / 2 < nf ? t / 2 : nf - 1, 53, 24, -1);
    if (t > 60) {
        hud_img(11, 0, 123, 112, -1);
        hud_img(12, 0, 139, 116, -1);
        int cr = CRYSTALS_PER_LIFE - G.p.crystals;              /* crystals collected */
        hud_number(cr < 0 ? 0 : cr > 999 ? 999 : cr, 3, 139 + 20, 118);
        hud_number(G.p.hp / 2, 1, 139 + 120, 118);              /* petals left (INFERRED meaning of the icon) */
        int cs = G.res_time * 100 / FPS, mm = cs / 6000, ss = cs / 100 % 60;
        hud_number(mm > 99 ? 99 : mm, 2, 139 + 76, 144);         /* colons in the rows image at x 98 and 124 */
        hud_number(ss, 2, 139 + 102, 144);
        hud_number(cs % 100, 2, 139 + 128, 144);
    }
}

void game_draw(void) {
    if (G.mode == MODE_FRONTEND) { fe_draw(); return; }
    if (G.in_select) { sel_draw(); return; }
    r_begin();
    if (G.mode == MODE_RESULTS) { results_draw(); r_end(); return; }
    draw_parallax();
    draw_pieces(0, 2);
    objects_draw(0);
    player_draw(&G.p);
    draw_pieces(3, 7);
    hud_draw();
    card_draw();
    pause_draw();
    if (G.clear_t) r_fade(G.clear_t / 60.f);      /* act transition: fade to black (RV: DV1 -> card) */
    r_end();
}
