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
    results_reset();
    G.clear_t = 0; G.act_in = 0; G.act_fade = 0; G.frame = 0; G.card_t = 0; G.paused = 0;
    G.pause_a = 0; G.pause_wipe_f = 0;
    G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;   /* RE 4311/4312 (act-carry values are applied after this; see next_act) */
    camera_reset();
    camera_update();
}

/* stage music per level dir (RE/RV: the original's play_music loops: m_stage1 frame index 20,
 * m_stage1b 21-22, m_boss 23 = DV4) */
const char *level_music(const char *dir) {
    /* RE: Frames::loop_play_music_N_<frame index> strings: 20 m_stage1, 21/22 m_stage1b, 23 m_boss */
    if (!strcmp(dir, "dv1")) return "bgm/m_stage1.adp";
    return "bgm/m_stage1b.adp";
}
int act_of(const char *dir) { return dir[2] - '0'; }

static void bank_free(bank_t *b) { free(b->act); free(b->frames); free(b->anims); free(b->seq); free(b->mbits); free(b->moff); memset(b, 0, sizeof *b); }

/* Lilac's banks stay resident across a level change when the next level's pack holds byte-identical
 * files (CRC-32 in the pack index): all four Dragon Valley acts share them (1.5 MB of textures). They
 * are loaded first after a full VRAM reset, so freeing everything else leaves VRAM contiguous. */
static uint32_t chr_crc[4];      /* lilac2.tex, lilac2.spr, lilachair.tex, lilachair.spr of the resident banks */
static int chr_resident;

/* only textures that were actually created (a zeroed tex_t has id 0 = somebody else's slot) */
static void tex_drop(tex_t *t) { if (t->w > 0) r_tex_free(t); memset(t, 0, sizeof *t); t->id = -1; }

void camera_shake(int z) { float n = G.cam.Z + z; G.cam.Z = n < z ? n : (float)z; }

static void level_free(void) {   /* everything of the level except Lilac's banks */
    tex_drop(&G.objs.tex); bank_free(&G.objs);
    if (G.ens.nframes) { tex_drop(&G.ens.tex); if (G.ens.tex2.w > 0) tex_drop(&G.ens.tex2); bank_free(&G.ens); }
    memset(&G.ens, 0, sizeof G.ens); G.ens.tex.id = -1;
    for (int i = 0; i < G.lvl.natlas && i < MAX_ATLAS; i++) tex_drop(&G.lvl.atlas[i]);
    for (int i = 0; i < G.npar; i++) tex_drop(&G.par[i]);
    par_free();
    free(G.lvl.pieces); free(G.lvl.objs); free(G.lvl.blob);
    memset(&G.lvl, 0, sizeof G.lvl);
    G.npar = 0; G.nobjs = 0;
}

void game_unload_all(void) {
    level_free();
    bank_free(&G.lilac); bank_free(&G.hair);
    chr_resident = 0;
    r_reset();
}

static int chr_crcs(const char *dir, uint32_t c[4]) {
    static const char *f[4] = { "lilac2.tex", "lilac2.spr", "lilachair.tex", "lilachair.spr" };
    char nm[48];
    for (int i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "%s/%s", dir, f[i]); if (pack_entry_crc(nm, &c[i]) < 0) return -1; }
    return 0;
}

/* every level lives in its own data dir (own palettes + Lilac bank; tools/build_level.sh) */
int game_load_level(const char *dir) {
    uint32_t want_crc[4];
    lp_begin(dir);
    int keep = chr_resident && chr_crcs(dir, want_crc) == 0 && !memcmp(want_crc, chr_crc, sizeof chr_crc);
    if (keep) level_free(); else game_unload_all();
    loading_begin(dir);                            /* silent loading screen (src/game/loading.c) */
    snprintf(G.lvdir, sizeof G.lvdir, "%s", dir);
    int bad = 0;
    char nm[48];
    for (int i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "%s/pal%d.pal", dir, i); bad |= pal_load_file(nm) < 0; }
    if (keep) { LP.nskip += 4; LP.skip_bytes += (uint64_t)G.lilac.tex.w * G.lilac.tex.h + (uint64_t)G.hair.tex.w * G.hair.tex.h; }
    else {
        snprintf(nm, sizeof nm, "%s/lilac2", dir); bad |= bank_load(&G.lilac, nm) < 0;
        snprintf(nm, sizeof nm, "%s/lilachair", dir); if (bank_load(&G.hair, nm) < 0) plat_log("no hair bank\n");
        chr_resident = !bad && chr_crcs(dir, chr_crc) == 0;   /* only packed data can be matched later */
    }
    snprintf(nm, sizeof nm, "%s/%s_obj", dir, dir); bad |= bank_load(&G.objs, nm) < 0;
    snprintf(nm, sizeof nm, "%s/%s_ens", dir, dir); if (bank_load(&G.ens, nm) < 0) { memset(&G.ens, 0, sizeof G.ens); G.ens.tex.id = -1; }
    snprintf(nm, sizeof nm, "%s/%s", dir, dir); bad |= level_load(&G.lvl, nm) < 0;
    if (bad) { loading_end(); lp_end(); plat_log("level %s: data load failed\n", dir); return -1; }
    par_load(dir);                                 /* layer-0 parallax (src/game/parallax.c) */
    G.p.spawn_x = G.p.spawn_y = 0;
    hud_reset();
    { uint64_t to = plat_time_us(); level_start(); LP.obj_us += plat_time_us() - to; }
    loading_end();
    G.mus_resume_t = 0;
    { uint64_t tm = plat_time_us(); music_play(level_music(dir)); LP.music_us += plat_time_us() - tm; }
    lp_end();
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
static const int SND_IDS[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 21, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 37, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 49, 50, 51, 53, 56, 57, 58, 63, 64, 74, 76, 78, 80, 81, 95, 98, 100, 174, 176, 220, 288, 289, 290, 311, 313, 314, 315, 319, 320, 396, 405 };
static int snd_handle[512];
static const char *VOICES[] = { "lilac_attack0", "lilac_attack1", "lilac_attack2", "lilac_attack3", "lilac_uppercut0",
    "lilac_uppercut1", "lilac_uppercut2", "lilac_uppercut3", "lilac_cyclone1", "lilac_cyclone2", "lilac_hurt0",
    "lilac_hurt1", "lilac_hurt2", "lilac_hurt3", "lilac_die", "lilac_cantmoveit" };
static int voice_handle[16];

void snd_play_orig(int id, int vol, int pan) {
#ifdef FP_HOST_ENV
    if (getenv("FP_TRACE_SID")) printf("sid %d\n", id);   /* original sound id, belongs to the next 'f' row */
#endif
    if (id >= 0 && id < 512 && snd_handle[id] >= 0) snd_play(snd_handle[id], vol, pan); }
void voice_play(const char *name) {
    for (int i = 0; i < (int)(sizeof VOICES / sizeof VOICES[0]); i++)
        if (!strcmp(VOICES[i], name)) { if (voice_handle[i] >= 0) snd_play(voice_handle[i], 255, 128); return; }
}

int game_init(void) {
    memset(&G, 0, sizeof G);
    char nm[48];
    for (int i = 0; i < 512; i++) snd_handle[i] = -1;
    vmuscr_init();
    save_load();                                   /* memory card (VMU) / host FP_SAVE */
#ifdef FP_HOST_ENV
    printf("save cards %d\n", save_card_total());   /* host tests: card total read back from the save */
#endif
    r_set_letterbox(SV.letterbox);
#ifndef FP_HOST_ENV
    { extern int demo_letterbox; if (demo_letterbox >= 0) r_set_letterbox(demo_letterbox); }   /* demo discs: LETTERBOX n */
#endif
    loading_begin("boot");                         /* boot: sound effects (title data follows in fe_enter) */
    for (int i = 0; i < (int)(sizeof SND_IDS / sizeof SND_IDS[0]); i++) {
        snprintf(nm, sizeof nm, "sfx/s%d.wav", SND_IDS[i]); snd_handle[SND_IDS[i]] = snd_load(nm); loading_tick();
    }
    for (int i = 0; i < (int)(sizeof VOICES / sizeof VOICES[0]); i++) { snprintf(nm, sizeof nm, "sfx/%s.wav", VOICES[i]); voice_handle[i] = snd_load(nm); loading_tick(); }
    loading_end(); lp_end();
    /* legacy handles used by gameplay code, now mapped to the original sound IDs */
    G.sfx.jump = snd_handle[8]; G.sfx.attack = snd_handle[21]; G.sfx.boost = snd_handle[53];
    G.sfx.hurt = snd_handle[17]; G.sfx.crystal = snd_handle[27]; G.sfx.explode = snd_handle[81]; G.sfx.hit = snd_handle[25];
    /* host tests (FP_LEVEL) and DC verification demos ("LEVEL <dir>"): start a level directly (no front end) */
    if (plat_start_level()) {
        G.mode = MODE_PLAY; int r = game_load_level(plat_start_level()); G.card_t = CARD_DONE;
#ifdef FP_HOST_ENV
        if (getenv("FP_CARD")) G.card_t = 0;
#endif
        return r;
    }
    return fe_enter();
}

/* from the front end: character/stage chosen */
void game_start_adventure(const char *dir, int time_attack) {
    G.time_attack = time_attack;
    G.p.energy_init = 0;                       /* fresh run: lives/crystals reset by player_reset */
    G.stage_crystals = 0; G.keystones = 0;
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
    G.cam.V = p->ground; G.cam.U = 0; G.cam.M3 = 0; G.cam.minC = 0; G.cam.cine = 0;
    float C = 0, E = (float)G.lvl.w, D = 0, F = (float)G.lvl.h;
    if (G.cam.A < C + CAM_HX) G.cam.A = C + CAM_HX;
    if (G.cam.A > E - CAM_HX) G.cam.A = E - CAM_HX;
    if (G.cam.B < D + CAM_HY) G.cam.B = D + CAM_HY;
    if (G.cam.B > F - CAM_HY) G.cam.B = F - CAM_HY;
}

/* boss camera (RE loop_camera_mode1_13 + 2862 once, 2865-2868 every frame): the bounds start at the current view
 * (or the limits if already inside them) and move 2 px per frame to the cameralimitboss objects */
void camera_boss_bounds(float l, float t, float r, float b) {
    float vl = G.camx, vt = G.camy;
    G.cam.U = 1; G.cam.lC = l; G.cam.lD = t; G.cam.lE = r; G.cam.lF = b;
    G.cam.bC = fminf(l, vl); G.cam.bD = fminf(t, vt); G.cam.bE = fmaxf(r, vl + SCREEN_W); G.cam.bF = fmaxf(b, vt + SCREEN_H);
}
void camera_normal_bounds(void) { G.cam.U = 0; }
float camera_bound_C(void) { return G.cam.U == 1 ? G.cam.bC : 0.f; }
float camera_bound_E(void) { return G.cam.U == 1 ? G.cam.bE : (float)G.lvl.w; }

void camera_update(void) {
    const player_t *p = &G.p;
    float X = p->x, Y = p->y - 14.f;                    /* movement object X, Y (reference point) */
    if (G.cam.U == 1) {                                  /* 2865-2868 */
        if (G.cam.bC < G.cam.lC) G.cam.bC = fminf(G.cam.lC, G.cam.bC + 2.f);
        if (G.cam.bE > G.cam.lE) G.cam.bE = fmaxf(G.cam.lE, G.cam.bE - 2.f);
        if (G.cam.bD < G.cam.lD) G.cam.bD = fminf(G.cam.lD, G.cam.bD + 2.f);
        if (G.cam.bF > G.cam.lF) G.cam.bF = fmaxf(G.cam.lF, G.cam.bF - 2.f);
    }
    float C = G.cam.minC, E = (float)G.lvl.w, D = 0, F = (float)G.lvl.h;
    if (G.cam.U == 1) { C = G.cam.bC; E = G.cam.bE; D = G.cam.bD; F = G.cam.bF; }
    if (!G.cam.M3) {   /* CAM.M = 3 (DV4 chase): A / B are set by 5404-5408, the mode-0 rules are skipped */
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
    if (p->state == 13) {                                /* M46 (ST_LADDER): 1931/1932 CAM.B toward Y, 4 px per frame */
        if (G.cam.B > Y) G.cam.B = fmaxf(G.cam.B - 4.f, Y);
        else if (G.cam.B < Y) G.cam.B = fminf(G.cam.B + 4.f, Y);
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
    }
    /* display offsets (events 2877-2886): shake Z adds G - Z + rand(2Z) (one cross_seed step each, every frame) */
    float S, T;
    { double Z = G.cam.Z; S = (float)(G.cam.G - Z + cross_rand_raw((int)(Z + Z) & 0xffff)); T = (float)(G.cam.H - Z + cross_rand_raw((int)(Z + Z) & 0xffff)); }
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
    G.cam.Z = G.cam.Z - 0.1f >= 0 ? G.cam.Z - 0.1f : 0;
    if (G.cam.cine) { cx = floorf(G.cam.cineX); cy = floorf(G.cam.cineY); C = 0; E = (float)G.lvl.w; D = 0; F = (float)G.lvl.h; }   /* 5590 */
    G.camx = fminf(fmaxf(cx - 213.f, C), E - SCREEN_W);
    G.camy = fminf(fmaxf(cy - 120.f, D), F - SCREEN_H);
}

/* ---- stage title card (RV images levelcardintro2/3 = name / "STAGE n"; sequence from a 10 fps
 * capture of the original: STAGE rises, name rises, hold on black, stage fades in, hold, both slide out
 * while the HUD drops in; frame counts are approximate) */

static void next_act(void) {
    char d[8]; snprintf(d, sizeof d, "%s", G.lvdir);
    int a = act_of(d);
    if (cards_commit()) save_store();             /* cards collected this act: one VMU write at the act change, never mid-stage */
    /* DV4 ends with the Serpentine boss and then the results (events 5385 / 4560-4589); the boss is not
     * ported, so the end of DV4 goes straight to the results. Time Attack plays every act as well. */
    if (a >= 4) {
        G.mode = MODE_RESULTS; G.res_t = 0; G.res_time = G.frame; music_play("bgm/m_clear.adp");
        if (G.time_attack) {                          /* Time Attack record (stage list order: 0 Shang Mu Academy, 1 Dragon Valley) */
            save_rec_t *r = &SV.rec[1][G.character < SAVE_CHARS ? G.character : 0];
            if (!r->time || (uint32_t)G.frame < r->time) { r->time = (uint32_t)G.frame; r->crystals = (uint16_t)(CRYSTALS_PER_LIFE - G.p.crystals); save_store(); }
        }
        return;
    }
    d[2] = (char)('0' + a + 1);
    /* RV (DV1 -> DV2 traces): petals, lives, crystals, energy and the timer carry over (globals) */
    int frame = G.frame, hp = G.p.hp; double energy = G.p.energy;
    G.act_carry = 1;
    if (game_load_level(d) < 0) { G.act_carry = 0; fe_enter(); return; }
    G.act_carry = 0;
    G.frame = frame; G.p.hp = hp; G.p.energy = energy;
    /* RE (events 4526 / 5047): no stage card on an act change; a 32-frame fade from black while Lilac,
     * placed 50 px left of the start marker, walks in at B = 3 with no control; then control returns */
    G.card_t = CARD_DONE; G.act_in = 1; G.act_fade = 248;
    G.p.x = G.p.spawn_x - 50; G.p.face = 1;
    camera_reset(); camera_update();
}

/* ---- pause menu (RE, DV1 = handle_frame_21: open 4878, close 4879, timer 2928, art 4296/4299/4300, cursor
 * 4297/4298/4301/4302, confirm 4303, wipe 3551, restart 4881 -> loop_restart_20 (event 1276), quit 4305 ->
 * loop_return_0; layout 4477). GUIPause.A = G.pause_a: 5 on opening, counts down to 3 (menu live) and from
 * 2 to 0 after closing; the cursor row (Crystal_447.A) is kept between pauses. */
static int pause_frame(void) { return G.p.lives <= 1 ? 4 : 3; }   /* RE 4299/4300: RESTART greyed out */

/* RE 4878: start edge, pause group active (after the title card, not during the act-end sequence),
 * menu closed (A == 0), player not dying */
static int pause_try_open(const input_t *in) {
    if (!(in->pressed & BTN_START) || G.pause_a != 0) return 0;
    if (G.card_t < CARD_DONE || G.clear_t || G.act_in || G.p.state == 6 /* ST_DEAD */ || G.p.hp <= 0) return 0;
    G.paused = 1; G.pause_a = 5;
    music_pause(); snd_stop_all();                 /* Media::pause_samples: every channel, BGM (31) included */
    snd_play_orig(63, 230, 128);                   /* played after pausing, so it is heard */
    G.pause_a--;                                   /* RE 2928 runs in the same frame: 5 -> 4 */
    return 1;
}

static void pause_update(const input_t *in) {
    uint32_t p = in->pressed;
    int jump = p & BTN_JUMP, start = p & BTN_START;
    if (G.pause_wipe_f) {                          /* RE 3551: wipe from the left, 32 px/frame; action 50 frames after confirm */
        G.pause_wipe_x = G.pause_wipe_x + 32 > 0 ? 0 : G.pause_wipe_x + 32;
        if (++G.pause_wipe_f > 50) {
            G.pause_wipe_f = 0; G.pause_a = 0; G.paused = 0;
            if (G.pause_sel == 3) {                /* RESTART: loop_restart_20 / event 1276 */
                int lives = G.act_lives - 1 < 1 ? 1 : G.act_lives - 1;
                int crystals = G.act_crystals, frame = G.frame;
                music_stop(); snd_stop_all();
                G.pause_sel = 1;
                { char d[8]; snprintf(d, sizeof d, "%s", G.lvdir); game_load_level(d); }
                G.p.lives = lives; G.p.crystals = crystals; G.frame = frame;
                G.act_lives = lives;
            } else {                                /* QUIT: loop_return_0 -> frame 7 dispatcher; RV: Time Attack -> stage select */
                G.pause_sel = 2;
                music_stop();
                fe_enter_from_stage(G.time_attack);
            }
        }
        return;
    }
    /* RE 4879: close on start, or confirm on RESUME (silent) */
    if (G.pause_a == 3 && (start || (jump && G.pause_sel == 0))) {
        G.pause_a = 2 - 1;                          /* 2928 in the same frame */
        G.paused = 0;
        music_resume();                             /* Media::resume_samples */
        return;
    }
    if (G.pause_a > 0 && G.pause_a != 3) G.pause_a--;   /* RE 2928 */
    if (G.pause_a != 3) return;
    int fr = pause_frame();
    if ((p & BTN_DOWN) && G.pause_sel < 2) { G.pause_sel++; snd_play_orig(64, 220, 128); if (G.pause_sel == 1 && fr == 4) G.pause_sel = 2; }
    if ((p & BTN_UP) && G.pause_sel > 0 && G.pause_sel <= 2) { G.pause_sel--; snd_play_orig(64, 220, 128); if (G.pause_sel == 1 && fr == 4) G.pause_sel = 0; }
    if (jump && G.pause_sel >= 1 && G.pause_sel <= 2) {   /* RE 4303 */
        snd_play_orig(56, 230, 128);
        G.pause_sel += 2;                           /* 3 = restart, 4 = quit */
        G.pause_wipe_x = -(SCREEN_W + 120); G.pause_wipe_f = 1;
    }
}

void game_update(const input_t *in) {
    /* VMU screen: the character's life icon and lives in a stage, the crystal everywhere else */
    if (G.mode == MODE_PLAY && !G.in_select) vmuscr_life(G.character, G.p.lives); else vmuscr_crystal();
    if (G.mode == MODE_FRONTEND) { fe_update(in); return; }
    if (G.in_select) { sel_update(in); return; }
    if (G.mode == MODE_RESULTS) {
        G.res_t++;
        if (G.res_t == 150) music_play("bgm/m_results.adp");
        if (G.res_t > 120 && (in->pressed & (BTN_JUMP | BTN_START))) { snd_play_orig(63, 230, 128); fe_enter(); }
        return;
    }
#ifdef FP_DEBUG_ACTJUMP
    /* test builds only (make FP_DEBUG_ACTJUMP=1): Y teleports Lilac near the act's right edge, so the
     * act change (and the resident-Lilac level load) can be exercised in the emulator */
    if ((in->pressed & BTN_GUARD) && !strcmp(G.lvdir, "dv1")) { G.p.x = 14600; G.p.y = 980; }   /* host test spot */
#endif
    if (G.card_t < CARD_DONE) {                   /* intro card: no control, timer not running */
        G.card_t++;
        input_t none = {0};
        objects_platforms(&G.p);
        player_update(&G.p, &none);
        camera_update();
        return;
    }
    G.frame_draw_t++;
    if (G.paused) { pause_update(in); return; }
    if (G.pause_a > 0) G.pause_a--;                  /* RE 2928: closing lockout 1 -> 0 */
    if (pause_try_open(in)) return;                  /* (the original pauses after this frame's gameplay events) */
    G.frame++;
    input_t none = {0}, rin = results_input(in), sin_ = {0};
    const input_t *pin = (G.clear_t || G.act_in) ? &none : results_on() ? &rin : in;   /* act end / walk-in / results: no control (RE) */
    { static uint32_t sprev; uint32_t h; if (serp4_input(&h)) { sin_.held = h; sin_.pressed = h & ~sprev; sprev = h; pin = &sin_; } else sprev = 0; }   /* DV4 boss exit (5547 lock, 5573-5577) */
    par_update();                                 /* RE 3541: parallax drift */
    objects_platforms(&G.p);                      /* RE: platform/bridge events run before Lilac's movement */
    player_update(&G.p, pin);
    layers_update(&G.p);                          /* RE 1613-1616 + 3031/3032: loop collision layer */
    if (G.act_in) {                               /* RE 4526: B = 3 every frame for 32 frames */
        if (G.p.ground) G.p.gsp = 3; else G.p.vx = 3;
        G.act_fade = G.act_fade - 8 < 0 ? 0 : G.act_fade - 8;
        if (++G.act_in > 32) { G.act_in = 0; G.act_fade = 0; }  /* RE 5047: control returns at A >= 32 */
    }
    if (G.clear_t) {                              /* RE 3311: grounded, B = max(B, 2), facing right */
        G.p.face = 1;
        if (G.p.ground && G.p.gsp < 2) G.p.gsp = 2;
    }
    objects_update();
    if (G.p.state == 6 /* ST_DEAD */ && G.p.dead_t > 120) {     /* death: the act restarts with one life less (RE 1972 / loop_death_13) */
        int lives = G.p.lives - 1, crystals = G.act_crystals, frame = G.frame, ks = G.act_keystones, sc = G.act_stage_crystals;
        if (lives < 1) { music_stop(); snd_stop_all(); fe_enter_from_stage(G.time_attack); return; }   /* game over (card not ported) */
        music_stop(); snd_stop_all();
        { char d[8]; snprintf(d, sizeof d, "%s", G.lvdir); if (game_load_level(d) < 0) { fe_enter(); return; } }
        G.p.lives = lives; G.p.crystals = crystals; G.frame = frame; G.act_lives = lives; G.keystones = ks; G.stage_crystals = sc;
        G.card_t = CARD_DONE;
        return;
    }
    shields_update(&G.p);                         /* RE 2673-2705, 2804-2812 */
    water_events(&G.p);                           /* RE 4150-4170, 4396/4397, 4464-4473 */
    pblocks_update(&G.p);                         /* RE 2711-2735 (after the state groups) */
    spinlogs_update(&G.p);                        /* RE 4980-4986 (DV2) */
    tunnel_events(&G.p);                          /* RE 4739-4755 (end of the frame) */
    cork_start_check(&G.p);                       /* RE 4756/4757 (end of the frame) */
    camera_update();
    /* stage music back after the 1-up jingle / when monitor invincibility ends */
    if (G.mus_resume_t > 0 && --G.mus_resume_t == 0)
        music_play(G.p.invinc ? "bgm/m_invincibility.adp" : level_music(G.lvdir));
    if (G.p.invinc && G.p.invuln == 0) { G.p.invinc = 0; music_play(level_music(G.lvdir)); }
    if (G.clear_t) {                              /* RE 3310 / 3578: fade +4 per frame; at 255 next frame */
        int a = G.clear_t == 1 ? 0 : G.clear_t;     /* fade object created with alpha 0 */
        if (a != 255 && a < 128) a = 2 * (a >> 1) + 2;                       /* RE 2905: +2 while < 128 */
        if (a != 255) a = 2 * (a >> 1) + 4 > 255 ? 255 : 2 * (a >> 1) + 4;  /* RE 3310: +4 */
        G.clear_t = a ? a : 1;
        if (G.clear_t == 255) { next_act(); return; }
    }
    /* RE (events 4135 / 5045): the act ends when Lilac passes X > frame width - 50 moving right; DV4 runs the
     * results instead (5381 / 5576, src/game/results.c) */
    if (results_update(&G.p)) return;
    if (act_of(G.lvdir) < 4 && !G.clear_t && !G.act_in && G.p.x > G.lvl.w - 50 && (G.p.ground ? G.p.gsp : G.p.vx) > 0) G.clear_t = 1;
}

static void draw_parallax(void) { par_draw(G.frame); }

static void draw_pieces(int lmin, int lmax) {
    const level_t *l = &G.lvl;
#ifdef FP_HOST_ENV
    { static int done; const char *e = getenv("FP_DBG_PIECE"); int qx, qy;
      if (e && !done && G.frame > 30 && sscanf(e, "%d,%d", &qx, &qy) == 2) { done = 1;
        for (int i = 0; i < l->npieces; i++) { const piece_t *p = &l->pieces[i];
            if (qx >= p->x && qx < p->x + p->w && qy >= p->y && qy < p->y + p->h) printf("piece %d x %d y %d w %d h %d layer %d col %d pad %d atlas %d\n", i, p->x, p->y, p->w, p->h, p->layer, p->col, p->pad, p->atlas); } } }
#endif
    int cx = (int)floorf(G.camx), cy = (int)floorf(G.camy);
    for (int i = 0; i < l->npieces; i++) {
        const piece_t *p = &l->pieces[i];
        if (p->layer < lmin || p->layer > lmax || (p->pad & 1)) continue;   /* bit0: invisible (blend alpha 0) */
#ifdef FP_HOST_ENV
        { static const char *hp; static int init; if (!init) { init = 1; hp = getenv("FP_HIDE_PIECE"); }
          if (hp) { char k[16]; snprintf(k, sizeof k, ",%d,", i); if (strstr(hp, k)) continue; } }
#endif
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
#define NHUD 16
static int hud_anim[NHUD] = { -2 };
void hud_reset(void) { hud_anim[0] = -2; }
static void hud_init(void) {
    if (hud_anim[0] == -2) {
        static const char *nm[NHUD] = { "hud_frame", "hud_fill", "hud_petals", "hud_icon", "hud_clock", "hud_digits",
            "card_stage", "card_name", "pause", "res_complete", "res_results", "res_panel", "res_rows", "res_digits", "res_small", "pausecursor" };
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
    hud_dy += results_hud_dy();                                    /* RE 4561: slides up 80 px during the results */
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
    /* RE 4477: GUIPause at (0.5 G - (105 - 53 J), 101) = (214, 101) at 426 px (RV); Crystal at its action point,
     * y + 24 per row (RV: (166, 141 + 24 row)) */
    hud_img_hot(8, pause_frame(), 214, 101);
    if (G.pause_sel <= 2) {
        hud_init();
        if (hud_anim[15] >= 0) {
            const anim_t *an = &G.objs.anims[hud_anim[15]];
            int k = (G.frame_draw_t / 6) % an->count;      /* Crystal_447 spins (speed 18) */
            frame_draw(&G.objs, &G.objs.frames[G.objs.seq[an->first + k]], 166, 141 + 24 * G.pause_sel, 0, 0xffffffff);
        }
    }
    if (G.pause_wipe_f) {                          /* levelcardfade2 (near black) + fadeedge_77 diagonal edge */
        float x = G.pause_wipe_x;
        if (x + SCREEN_W > 0) r_rect(0, 0, x + SCREEN_W, SCREEN_H, 0xff080000);
        for (int y = 0; y < SCREEN_H; y += 2) {
            float w = 120.f * (1.f - (float)y / SCREEN_H);
            float x0 = x + SCREEN_W; if (x0 < 0) { w += x0; x0 = 0; }
            if (w > 0) r_rect(x0, (float)y, w, 2, 0xff080000);
        }
    }
}

void game_draw(void) {
    if (G.mode == MODE_FRONTEND) { fe_draw(); return; }
    if (G.in_select) { sel_draw(); return; }
    r_begin();
    draw_parallax();
    draw_pieces(0, 1);
    objects_draw(0);
    draw_pieces(2, 2);
    objects_draw(1);
    water_draw_front();
    player_draw(&G.p);
    shields_draw(&G.p);
    spiral_front_draw();
    water_draw_tint();
    draw_pieces(3, 3);
    objects_draw(2);
    draw_pieces(4, 7);
    hud_draw();
    water_draw_hud();
    serp_draw_hud();
    results_draw();
    card_draw();
    pause_draw();
    if (G.clear_t) r_fade(G.clear_t / 255.f);     /* act end: fade object alpha (RE 3310) */
    if (G.act_in && G.act_fade) r_fade(G.act_fade / 255.f);   /* act start: fade from black (RE 4526) */
    r_end();
}
