/* Adventure mode: the story's progression, its one gate, its triggers and the MODE_STORY game mode (session 25).
 * Everything about the format and the evidence: src/game/story.h and docs/ADVENTURE.md. RE = decompiled events,
 * RV = original runtime traces (build/orig_traces/adv, tools/orig/story_rv.sh), INFERRED = neither. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "story.h"
#include "cutscene.h"
#include "ui.h"

extern int fe_mode;                  /* frontend.c: the original's global 0 (0 Time Attack, 1 Classic, 2 Adventure) */

/* ---- the scene table (story/story.txt) */
enum { NX_NONE, NX_SCENE, NX_STAGE, NX_ROUTE, NX_MENU, NX_MARK };
typedef struct { uint8_t kind; int16_t scene; char dir[8]; } step_t;
typedef struct {
    char name[24]; int room, g4, chr, panel;   /* panel: GUIPause frame (1 SKIP / QUIT, 2 SKIP / NEXT STAGE; 0: START skips at once) */
    step_t next, skip, stage;
    int trig_new, trig_end;          /* trig_end: stage I + 1 (0 none) */
} scene_t;
static scene_t SC[STORY_MAX];
static int nsc = -1;                 /* -1: not read yet */
static char *captions;               /* story/captions.txt (the original's caption strings, tables/story/dialog.json) */

static void parse_step(const char *s, step_t *st) {
    memset(st, 0, sizeof *st); st->scene = -1;
    if (!strncmp(s, "scene:", 6)) { st->kind = NX_SCENE; st->scene = (int16_t)story_scene_id(s + 6); if (st->scene < 0) st->kind = NX_NONE; }
    else if (!strncmp(s, "stage:", 6)) { st->kind = NX_STAGE; snprintf(st->dir, sizeof st->dir, "%.7s", s + 6); }
    else if (!strcmp(s, "route")) st->kind = NX_ROUTE;
    else if (!strcmp(s, "menu")) st->kind = NX_MENU;
    else if (!strncmp(s, "mark:", 5)) st->kind = NX_MARK;     /* SKIP SCENE inside the script (frame 75 g4 0: to the character select) */
}
static void table_load(void) {
    if (nsc >= 0) return;
    nsc = 0;
    { size_t cz; captions = plat_load_file("story/captions.txt", &cz);
      if (captions) { char *z = realloc(captions, cz + 1); if (z) { captions = z; captions[cz] = 0; } else { free(captions); captions = NULL; } } }
    size_t sz; char *d = plat_load_file("story/story.txt", &sz);
    if (!d) return;
    /* two passes: names first (a step may name a later scene) */
    char *lines[STORY_MAX]; int nl = 0;
    for (char *p = d, *e = d + sz; p < e && nl < STORY_MAX;) {
        char *q = p; while (q < e && *q != '\n') q++;
        if (q >= e) break;
        *q = 0; if (q > p && q[-1] == '\r') q[-1] = 0;
        if (*p && *p != '#') lines[nl++] = p;
        p = q + 1;
    }
    for (int i = 0; i < nl; i++) {
        scene_t *s = &SC[nsc];
        char nx[32];
        memset(s, 0, sizeof *s); s->chr = 7;
        if (sscanf(lines[i], "%23s %d %d %31s", s->name, &s->room, &s->g4, nx) != 4) continue;
        nsc++;
    }
    for (int i = 0, k = 0; i < nl && k < nsc; i++) {
        scene_t *s = &SC[k];
        char nm[24], nx[32], tok[32]; int room, g4, off = 0;
        if (sscanf(lines[i], "%23s %d %d %31s%n", nm, &room, &g4, nx, &off) != 4) continue;
        parse_step(nx, &s->next); s->skip = s->next; s->panel = 1; memset(&s->stage, 0, sizeof s->stage);
        for (const char *p = lines[i] + off; sscanf(p, "%31s%n", tok, &off) == 1; p += off) {
            if (!strncmp(tok, "skip=", 5)) parse_step(tok + 5, &s->skip);
            else if (!strncmp(tok, "stage=", 6)) { s->stage.kind = NX_STAGE; snprintf(s->stage.dir, sizeof s->stage.dir, "%.7s", tok + 6); }
            else if (!strncmp(tok, "panel=", 6)) s->panel = atoi(tok + 6);
            else if (!strcmp(tok, "trig=new")) s->trig_new = 1;
            else if (!strncmp(tok, "trig=end:", 9)) s->trig_end = atoi(tok + 9) + 1;
            else if (!strncmp(tok, "chr=", 4)) s->chr = atoi(tok + 4);
        }
        k++;
    }
    free(d);
}
int story_count(void) { table_load(); return nsc; }
int story_scene_id(const char *name) { for (int i = 0; i < (nsc > 0 ? nsc : 0); i++) if (!strcmp(SC[i].name, name)) return i; return -1; }
const char *story_scene_name(int id) { return id >= 0 && id < nsc ? SC[id].name : "-"; }

/* ---- progression state: seen flags (global) and the file's story position (save.c v5) */
int  story_seen(int id) { return id >= 0 && id < STORY_MAX && (SV.story_seen[id >> 3] >> (id & 7) & 1); }
void story_mark_seen(int id) { if (id >= 0 && id < STORY_MAX) SV.story_seen[id >> 3] |= (uint8_t)(1 << (id & 7)); }

static struct {
    int run;                         /* an Adventure run is going on (armed by story_new_game / story_continue) */
    int scene, done;                 /* the scene playing (MODE_STORY), its completion ran (exactly once) */
    int paused, pause_a, sel, wipe_f; float wipe_x;   /* the story pause (GUIPause frame 1: RESUME / SKIP SCENE / QUIT) */
    int chsel, chsel_cur, chsel_t;   /* the in-scene character select (frame 75) */
    int last_stage;                  /* frame 8's MenuSequence.I for 'route' steps */
    int pos;                         /* the run's story position: the last completed scene id + 1 */
    char *dlg_buf; const char *dlg[320]; int ndlg;
    ui_banks_t font;
} S = { .scene = -1, .last_stage = -1 };

/* THE gate (session 25): every story trigger asks here. Classic (fe_mode 1) and Time Attack (fe_mode 0, G.time_attack)
 * never pass; neither does an Adventure flag left over from an earlier run (S.run is armed only by a new Adventure or an
 * Adventure continue and cleared by every return to the menu, story_reset). */
int story_gate(void) { return S.run && fe_mode == 2 && !G.time_attack && G.file_mode == 0; }
int story_active(void) { return G.mode == MODE_STORY; }
int story_paused(void) { return S.paused; }
int story_position(void) { return S.run ? S.pos : -1; }

static void dlg_free(void) { free(S.dlg_buf); S.dlg_buf = NULL; S.ndlg = 0; }
static void leave_scene(void) {
    csx_free(); dlg_free();
    if (S.font.n) ui_banks_free(&S.font);
    S.scene = -1; S.paused = 0; S.chsel = 0; S.wipe_f = 0;
}
void story_reset(void) {
    if (S.scene >= 0) leave_scene();
    S.run = 0; S.done = 0; S.last_stage = -1; S.pos = 0;
#ifdef FP_HOST_ENV
    printf("story reset\n");
#endif
}

static int dlg_load(int handle) {
    char nm[32]; size_t sz;
    dlg_free();
    snprintf(nm, sizeof nm, "story/dlg_%d.txt", handle);
    S.dlg_buf = plat_load_file(nm, &sz);
    if (!S.dlg_buf) return -1;
    char *p = S.dlg_buf, *e = S.dlg_buf + sz;
    while (p < e && S.ndlg < (int)(sizeof S.dlg / sizeof S.dlg[0])) {
        char *q = p; while (q < e && *q != '\n') q++;
        if (q >= e) break;
        *q = 0; S.dlg[S.ndlg++] = p; p = q + 1;
    }
    return 0;
}

static void go_step(const step_t *st);
static int start_scene(int id);

/* a stage from the story: the first one of a run starts fresh (title card, 5 lives), later ones carry lives, crystals and
 * the shield like frame 8 does between Classic stages (spec_records_saves 3) */
static void go_stage(const char *dir) {
    if (S.last_stage < 0) { game_start_adventure(dir, 0); return; }
    if (game_start_stage(dir) < 0) fe_enter_from_stage(0);
}
static void go_step(const step_t *st) {
    switch (st->kind) {
    case NX_SCENE: if (start_scene(st->scene) == 0) return; break;
    case NX_STAGE: go_stage(st->dir); return;
    case NX_ROUTE: { const char *lv = S.last_stage >= 0 ? level_route_next(S.last_stage) : level_route_start(G.character);
                     if (lv) { go_stage(lv); return; } } break;
    default: break;
    }
    story_reset(); fe_enter_from_stage(0);                /* NX_MENU, or nothing ported there yet */
}

/* the scene's completion: runs exactly once per scene start. how: 0 the script ended (next), 1 SKIP SCENE (skip; frame
 * 81's START), 2 NEXT STAGE (the chain's stage: RV k75_g1_row2 / k14_g11_row2 -> RM1) */
enum { DONE_END, DONE_SKIP, DONE_STAGE };
static void complete(int skipped) {
    if (S.done || S.scene < 0) return;
    S.done = 1;
    int id = S.scene;
    csx_stop_audio(); music_stop();
    story_mark_seen(id);
    S.pos = id + 1;
    if (G.file_slot >= 0 && G.file_slot < SAVE_FILES && !G.file_new && SV.file[G.file_slot].used) {   /* the file's story position (save.c v5) */
        SV.file[G.file_slot].story = (uint8_t)S.pos;
        save_store();
    }                                                       /* a new file gets it with its first stage end (game_save_stage_end) */
#ifdef FP_HOST_ENV
    printf("story complete %s skipped %d\n", SC[id].name, skipped);
#endif
    step_t st = skipped == DONE_SKIP ? SC[id].skip : skipped == DONE_STAGE && SC[id].stage.kind ? SC[id].stage : SC[id].next;
    leave_scene();
    go_step(&st);
}

/* a scene that does not play for this character: the scene of the same json frame and g4 that does. The original's room
 * picks the content by g1 at one g4 (frame 82 g4 13: jc_cave / jc_cave_cm, g4 17: tb_arrest / tb_outside_team; RV
 * c83_g13 / c83_g13c, c83_g17 / c83_g17c), so a step into that g4 names either. A split room (st83<list>, ADVENTURE.md 10) is
 * numbered json * 100 + its CutsceneList index. */
static int json_of(int room) { return room >= 1000 ? room / 100 : room; }
static int chr_variant(int id) {
    int cs = chr_slot(G.character); if (cs > 2) cs = 0;
    if (SC[id].chr >> cs & 1) return id;
    for (int i = 0; i < nsc; i++)
        if (i != id && SC[i].g4 == SC[id].g4 && json_of(SC[i].room) == json_of(SC[id].room) && (SC[i].chr >> cs & 1)) return i;
    return id;
}

static int start_scene(int id) {
    table_load();
    if (id < 0 || id >= nsc || !story_gate()) return -1;
    id = chr_variant(id);
    char nm[48], room[8];
    snprintf(nm, sizeof nm, "story/%s.scn", SC[id].name);
    if (csx_load(nm) < 0) {                                 /* no script (not ported, or no story data): the scene's next step */
#ifdef FP_HOST_ENV
        printf("story missing %s\n", SC[id].name);
#endif
        story_mark_seen(id);
        step_t st = SC[id].next; go_step(&st); return 0;
    }
    snprintf(room, sizeof room, "st%d", SC[id].room);
    G.stage_carry = 1;                                      /* lives, crystals and the shield go through the room */
    int r = game_load_level(room);
    G.stage_carry = 0;
    if (r < 0) { csx_free(); story_mark_seen(id); step_t st = SC[id].next; go_step(&st); return 0; }
    music_stop();
    { short ids[48]; int n = csx_sfx_ids(ids, 48); static short keep[48]; memcpy(keep, ids, sizeof ids); game_sfx_set(keep, n); }
    memset(&S.font, 0, sizeof S.font);
    if (bank_load(&S.font.bank[0], "ui/font") == 0) S.font.n = 1;
    if (csx_dialog() > 0) dlg_load(csx_dialog()); else dlg_free();
    G.mode = MODE_STORY; G.paused = 0; G.card_t = 192;
    S.scene = id; S.done = 0; S.paused = 0; S.pause_a = 0; S.wipe_f = 0; S.chsel = 0;
    csx_set_captions(captions);
    csx_begin(&G.ens, S.font.n ? &S.font.bank[0] : NULL, S.dlg, S.ndlg);
#ifdef FP_HOST_ENV
    printf("story start %s room %s g4 %d\n", SC[id].name, room, SC[id].g4);
#endif
    return 0;
}

/* ---- triggers */
/* RE hub 'c1' / character select 800: a new Adventure -> frame 81 with g4 2 (the opening) */
int story_new_game(int character) {
    table_load();
    story_reset();
    if (fe_mode != 2) return -1;
    S.run = 1; S.last_stage = -1;
    G.character = character; G.time_attack = 0;
    game_run_reset(); G.p.energy_init = 0;
    for (int i = 0; i < nsc; i++)
        if (SC[i].trig_new && (SC[i].chr >> (chr_slot(character) < 3 ? chr_slot(character) : 0) & 1)) {
            if (start_scene(i) == 0) return 0;
        }
    S.run = 0; return -1;                                   /* no story data: the caller starts the first stage */
}
/* RE frame 8 (handle_frame_9, g0 = 2 block): the end of stage I goes to a story room by I and g1 */
static int route_from(int stage) {
    S.last_stage = stage;
    int cs = chr_slot(G.character); if (cs > 2) cs = 0;
    for (int i = 0; i < nsc; i++)
        if (SC[i].trig_end == stage + 1 && (SC[i].chr >> cs & 1)) return start_scene(i) == 0;
    return 0;
}
int story_stage_end(int stage) {
    table_load();
    if (!story_gate()) return 0;
    return route_from(stage);
}
int story_continue(int last_stage) {
    table_load();
    story_reset();
    if (fe_mode != 2 || G.file_mode != 0) return 0;
    S.run = 1;
    if (G.file_slot >= 0 && G.file_slot < SAVE_FILES) S.pos = SV.file[G.file_slot].story;
    if (last_stage == 0xff || last_stage < 0) { S.last_stage = -1; return 0; }
    return route_from(last_stage);
}

/* ---- the in-scene character select (frame 75 g4 0, RV a75_g0 / s75_g0b): after Carol's last line the overlay
 * CharacterSelect_215 / Character1_219 (Lilac) / Character2_220 (Carol) / CharName1-2 / CharacterSlot_3911 slides in (script
 * actors); the slot under the cursor blinks (direction 1, as frame 6's slots RE 752 / 753); A picks: sound 63 + the
 * character's voice (220 Lilac / 221 Carol, frame 6 RE 4.3.9), the portrait's animation 7, then the script's own wipe
 * (sound 56 50 frames later) and the scene ends into DV1 with g1 = the choice. Left / right move the cursor (sound 64). */
enum { CH_SLOT = 3911, CH_LILAC = 219, CH_CAROL = 220 };
static int ch_act[4];                /* slot Lilac, slot Carol, portrait Lilac, portrait Carol */
static void chsel_show(void) {
    for (int k = 0; k < 2; k++) csx_actor_image(ch_act[k], 0, S.chsel_cur == k ? 1 : 0, -1);
}
static void chsel_begin(void) {
    int s0 = csx_actor_find(CH_SLOT, 0), s1 = csx_actor_find(CH_SLOT, 1);
    if (s0 >= 0 && s1 >= 0 && csx_actor_x(s1) < csx_actor_x(s0)) { int t = s0; s0 = s1; s1 = t; }   /* Lilac's slot is the left one */
    ch_act[0] = s0; ch_act[1] = s1; ch_act[2] = csx_actor_find(CH_LILAC, 0); ch_act[3] = csx_actor_find(CH_CAROL, 0);
    for (int k = 0; k < 4; k++) csx_actor_lock(ch_act[k], 1);
    S.chsel = 1; S.chsel_cur = chr_is_carol(G.character) ? 1 : 0; S.chsel_t = 0;
    chsel_show();
}
static void chsel_update(uint32_t p) {
    S.chsel_t++;
    int c0 = S.chsel_cur;
    if ((p & BTN_RIGHT) && S.chsel_cur < 1) S.chsel_cur = 1;
    if ((p & BTN_LEFT) && S.chsel_cur > 0) S.chsel_cur = 0;
    if (S.chsel_cur != c0) { snd_play_orig(64, 220, 128); chsel_show(); }
    if (p & (BTN_JUMP | BTN_START)) {
        G.character = S.chsel_cur ? CHR_CAROL : CHR_LILAC;
        snd_play_orig(63, 230, 128); snd_play_orig(S.chsel_cur ? 221 : 220, 255, 128);
        csx_actor_image(ch_act[2 + S.chsel_cur], 7, 0, 0);
        S.chsel = 0;
        csx_chsel_done();
#ifdef FP_HOST_ENV
        printf("story chsel %d\n", G.character);
#endif
    }
}

/* ---- the story pause (RE 18656 open / 18657 + 18660 resume / 18661 confirm / 18662-18666 the rows' actions, frame 76;
 * the panel is GUIPause_347 frame 1: RESUME / SKIP SCENE / QUIT). Opening pauses every sample (Media::pause_samples) */
static void pause_update(const input_t *in) {
    uint32_t p = in->pressed;
    if (S.wipe_f) {                                         /* RE 3551 wipe, 50 frames, then the row's action */
        S.wipe_x = S.wipe_x + 32 > 0 ? 0 : S.wipe_x + 32;
        if (++S.wipe_f > 50) {
            S.wipe_f = 0; S.paused = 0;
            if (S.sel == 1) complete(DONE_SKIP);            /* SKIP SCENE (RV k75_g1_skip, k14_g11_skip, k76_g1_skip: the next step) */
            else if (SC[S.scene].panel == 2) complete(DONE_STAGE);   /* NEXT STAGE */
            else { csx_stop_audio(); music_stop(); story_reset(); fe_enter_from_stage(0); }   /* QUIT: the title menu (RV k75_g0_row2: frame 3) */
        }
        return;
    }
    if (S.pause_a > 3) { S.pause_a--; return; }
    if ((p & BTN_START) || ((p & BTN_JUMP) && S.sel == 0)) { S.paused = 0; S.pause_a = 1; music_resume(); return; }   /* RESUME */
    if ((p & BTN_DOWN) && S.sel < 2) { S.sel++; snd_play_orig(64, 220, 128); }
    if ((p & BTN_UP) && S.sel > 0) { S.sel--; snd_play_orig(64, 220, 128); }
    if ((p & BTN_JUMP) && S.sel >= 1) {
        snd_play_orig(56, 230, 128);
        if (S.sel == 1 && SC[S.scene].skip.kind == NX_MARK) {   /* RV k75_g0_skip700: the scene jumps to its character select, no wipe */
            S.paused = 0; S.pause_a = 1; music_resume(); csx_skip_to_chsel(); return;
        }
        S.wipe_f = 1; S.wipe_x = -(SCREEN_W + 120);
    }
}

void story_update(const input_t *in) {
    G.frame++; G.frame_draw_t++;
    if (S.scene < 0) return;
    if (S.paused) { pause_update(in); return; }
    if (S.pause_a > 0) S.pause_a--;
    if ((in->pressed & BTN_START) && !S.pause_a && !S.chsel && !S.done && SC[S.scene].panel == 0) { complete(DONE_SKIP); return; }   /* RV k81_g2_start */
    if ((in->pressed & BTN_START) && !S.pause_a && !S.chsel && !S.done) {   /* RE 18656 */
        S.paused = 1; S.pause_a = 5; S.sel = 0;
        music_pause(); snd_stop_all(); snd_play_orig(63, 230, 128);
        return;
    }
    par_update();
    static uint32_t prev; uint32_t p = prev; prev = in->pressed;
    if (S.chsel) { chsel_update(p); csx_update(in); return; }
    int r = csx_update(in);
    if (r == CSX_CHSEL && !S.chsel) chsel_begin();
    if (r == CSX_DONE) complete(0);
}

void story_draw_layer(int pass) { if (S.scene >= 0) csx_draw_actors(pass); }
void story_draw_overlay(void) {
    if (S.scene < 0) return;
    csx_draw_ui();
    if (S.paused) game_pause_panel(SC[S.scene].panel == 2 ? 2 : 1, S.wipe_f ? -1 : S.sel, S.wipe_f, S.wipe_x);   /* GUIPause frame 1 / 2 (RV) */
}

#ifdef FP_HOST_ENV
void story_host_dump(void) {
    unsigned long long seen = 0; for (int i = 0; i < 64; i++) if (story_seen(i)) seen |= 1ull << i;
    printf("story run %d gate %d scene %s seg %d line %d paused %d sel %d chsel %d mode %d fe_mode %d chr %d file %d pos %d lv %s seen %llx\n",
           S.run, story_gate(), story_scene_name(S.scene), csx_segment(), csx_line(), S.paused, S.sel, S.chsel, G.mode, fe_mode,
           G.character, G.file_slot, G.file_slot >= 0 && G.file_slot < SAVE_FILES ? SV.file[G.file_slot].story : -1, G.lvdir[0] ? G.lvdir : "-", seen);
}
int story_host_play(const char *name) {
    table_load();
    if (fe_mode == 2 && !S.run) { S.run = 1; G.time_attack = 0; }   /* an Adventure run as the triggers arm it (gate: fe_mode) */
    return start_scene(story_scene_id(name));
}
int story_host_new(int character) {                     /* as the hub's NEW ADVENTURE (frontend.c) */
    fe_mode = 2; G.file_mode = 0;
    G.file_slot = getenv("FP_FILE") ? atoi(getenv("FP_FILE")) : -1; G.file_new = getenv("FP_FILE_NEW") != NULL || G.file_slot < 0;
    return story_new_game(character);
}
int story_host_stage_end(void) {                        /* as results.c's stage end in a story run */
    if (story_stage_end(level_cur()->stage)) return 1;
    if (!G.time_attack && game_next_stage()) return 2;
    return 0;
}
#endif
