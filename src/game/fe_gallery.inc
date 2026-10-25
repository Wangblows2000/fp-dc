/* Gallery (original frame index 10 = Frames::on_frame_11; spec build/re/fe/spec_gallery.md, events 992-1139).
 * Included by frontend.c (it shares the screen bank s_scr / b_scr, the tile fade-in, the wipe and the camera shake).
 * Three pages that wrap: CARDS (the 100 cards: each opened card plays a music track or a sound, or shows an artwork),
 * ACHIEVEMENTS (56 icons, name + description) and DATA (delete records / cards / achievements, applied when leaving).
 * Save state (save.c v4): records x20 -> save_card_state() 0 not taken, 1 face down, 2 opened; records x2 y1..56 ->
 * save_ach() 0 locked, 1 earned (not announced), 2 announced.
 * Data (ASSET_PIPELINE.md "Session 15: Gallery"): scene ui/gal_11 + bank ui/gal_ui0 (tools/import_ui.py), gal/gal_text.txt
 * (the List* string tables), gal/art_NN.tex/.pal (PAL8, loaded on View and freed on close), gal/<voice>.wav and
 * gal/s<id>.wav (loaded on demand), gal/s443.wav (resident while the gallery is open).
 * Labels: RE read from the decompiled events, RV seen in the original (build/re/fe/gal/t_gal1..6), INFERRED neither. */

#define GAL_NL (1 + 100 + 100 + 56 + 56 + 9)
#define GAL_FILE(id) (Y.ln[(id)])                     /* ListSongFilenames: type digit + file */
#define GAL_SONG(id) (Y.ln[100 + (id)])               /* ListSongs: card label */
#define GAL_NAME(k) (Y.ln[200 + (k)])                 /* ListNames */
#define GAL_DESC(k) (Y.ln[256 + (k)])                 /* ListDescriptions */
#define GAL_WARN(i) (Y.ln[313 + (i)])                 /* ListWarnings paragraphs 0..8 (PC wording) */
static const int GAL_ROW_E[10] = { 1, 2, 0, 3, 4, 5, 6, 8, 7, 9 };   /* RE 1012-1024 + RV lists_cards.txt: stage icon frame per row */

static struct {
    int A, D, I, col, S;          /* CrystalCursor A page (0 CARDS, 1 ACHIEVEMENTS, 2 DATA), D row, I achievement column, S shake; CardSelector.A */
    int selB;                     /* CardSelector.B: > 0 opening timer, < 0 lockout after play / stop / view (RE 1034-1058) */
    int input, leaving, t;
    float sv, retv;               /* eased scroll offset of the scroll group (V/W, RE 1028) and Return_110's eased y */
    int arrow_d;                  /* header arrows bob (RE 1089) */
    int playing, play_t;          /* card whose item plays (card flag 0), frames left for a sound card */
    int art;                      /* Artwork frame shown, -1 none */
    tex_t art_tex;
    int mark[4], panel, pD;       /* DATA rows marked; confirm panel open (row), its cursor 1 YES / 2 NO */
    int dirty;                    /* opened / closed cards or deletes: saved once on leaving */
    int burst_on; float bx, by;
    int toast[SAVE_ACH], ntoast, toast_i, toast_t;
    int snd443, voice_h;
    input_t prev;
    char *text; const char *ln[GAL_NL + 1];
    ui_obj_t *card[101], *ach[SAVE_ACH + 1], *row[10], *ret, *header, *ar1, *ar2, *cursor, *selector, *data[4], *sparkle, *burst, *officer;
} Y;

static void gal_trace(const char *fmt, ...) {
#ifdef FP_HOST_ENV
    va_list ap; va_start(ap, fmt); printf("gal "); vprintf(fmt, ap); printf("\n"); va_end(ap);   /* host tests (tests/test_gallery.py) */
#else
    (void)fmt;
#endif
}

static void gal_sfx(int id, int vol) {
    if (id != 443) { snd_play_orig(id, vol, 128); return; }
#ifdef FP_HOST_ENV
    if (getenv("FP_TRACE_SID")) printf("sid %d\n", id);
#endif
    if (Y.snd443 >= 0) snd_play(Y.snd443, vol, 128);    /* not in the boot set: gal/s443.wav while the gallery is open */
}

static void gal_text_load(void) {
    static const char *empty = "";
    for (int i = 0; i <= GAL_NL; i++) Y.ln[i] = empty;
    size_t sz; char *d = plat_load_file("gal/gal_text.txt", &sz);
    if (!d) { plat_log("gallery: gal/gal_text.txt missing\n"); return; }
    char *t = malloc(sz + 1);
    if (!t) { free(d); return; }
    memcpy(t, d, sz); t[sz] = 0; free(d);
    int n = 0; char *p = t;
    while (*p && n < GAL_NL) {                         /* lines; the disc copy is zero-padded to 32 bytes */
        char *e = strchr(p, '\n');
        if (e) *e = 0;
        Y.ln[n++] = p;
        if (!e) break;
        p = e + 1;
    }
    if (n < GAL_NL || strncmp(Y.ln[0], "FPGT1 100 56 9", 14)) { plat_log("gallery: bad gal_text.txt\n"); free(t); for (int i = 0; i <= GAL_NL; i++) Y.ln[i] = empty; return; }
    Y.text = t;
}

static int gal_type(int id) { const char *f = GAL_FILE(id); return f[0] >= '0' && f[0] <= '9' ? f[0] - '0' : -1; }
/* RE 996-1002 (loop_cards_10): a collected type-3 card stays padlocked until one of achievements 1-3 is set */
static int gal_locked(int id) { return gal_type(id) == 3 && save_ach(1) + save_ach(2) + save_ach(3) == 0; }
static int gal_dir(int id) {                            /* MapIcon direction */
    int st = save_card_state(id);
    if (st == 0) return 0;                              /* grey star */
    if (st == 1) return gal_locked(id) ? 2 : 1;          /* padlock / face down */
    int ty = gal_type(id);
    return ty == 2 ? 4 : ty == 4 ? 5 : 3;               /* speaker / cookie (art) / note */
}
/* RE 1012-1024: a row shows its stage picture (else the padlock, frame 15). progress[] = records x1 y40 (Lilac),
 * y42 (Carol), y43 (Milla); y41 / y44 (Spade / Torque) share the port's fourth slot (INFERRED, never set). */
static int gal_row_frame(int r) {
    int E = GAL_ROW_E[r], any = 0;
    for (int c = 0; c < 10; c++) any |= save_card_state(10 * r + c + 1) > 0;
    int y40 = SV.progress[0], y41 = SV.progress[3], y42 = SV.progress[1], y43 = SV.progress[2], y44 = SV.progress[3];
    if (E == 1 || any || (E < y40 && E != 0 && E != 8) || E < y41 || (E < y42 && E != 0 && E != 7) ||
        (E < y43 && E != 1 && E != 7) || (E < y44 && E != 0 && E != 1 && E != 7 && E != 8)) return E;
    return 15;
}

static void gal_voice_free(void) {
    if (Y.voice_h >= 0) { snd_stop_all(); snd_unload(Y.voice_h); Y.voice_h = -1; }
}
/* sound cards (RE 1043-1050): one clip loaded on demand; its length (WAV header) stands for "channel 31 busy" */
static int gal_voice(const char *name) {
    char nm[48]; size_t sz; int frames = 60;
    gal_voice_free();
    snprintf(nm, sizeof nm, "gal/%s.wav", name);
    uint8_t *d = plat_load_file(nm, &sz);
    if (!d) return -1;
    uint32_t rate = 0, bits = 0, ch = 1, data = 0;
    for (size_t o = 12; o + 8 <= sz;) {
        uint32_t n = d[o + 4] | d[o + 5] << 8 | d[o + 6] << 16 | (uint32_t)d[o + 7] << 24;
        if (!memcmp(d + o, "fmt ", 4) && o + 24 <= sz) { ch = d[o + 10] | d[o + 11] << 8; rate = d[o + 12] | d[o + 13] << 8 | d[o + 14] << 16; bits = d[o + 22] | d[o + 23] << 8; }
        if (!memcmp(d + o, "data", 4)) { data = n; break; }
        o += 8 + n + (n & 1);
    }
    free(d);
    if (rate && bits && ch) frames = (int)((uint64_t)data * 8 / (bits * ch) * FPS / rate) + 1;
    Y.voice_h = snd_load(nm);
    if (Y.voice_h < 0) return -1;
    snd_play(Y.voice_h, 255, 128);
    Y.play_t = frames;
    return 0;
}

static void gal_art_free(void) {
    if (Y.art < 0) return;
    r_tex_free(&Y.art_tex); gal_trace("art free %d", Y.art); Y.art = -1;
}
static int gal_art_show(int nn) {                       /* RE 1060: Artwork frame NN, loaded on demand (PAL8, palette group 0) */
    char nm[32];
    gal_art_free();
    snprintf(nm, sizeof nm, "gal/art_%02d", nn);
    if (tex_load_file(&Y.art_tex, nm) < 0) return -1;
    snprintf(nm, sizeof nm, "gal/art_%02d.pal", nn);
    pal_load_file(nm);
    Y.art = nn; gal_trace("art load %d", nn);
    return 0;
}

static void gal_stop_item(void) {                        /* RE 1055: stop channel 31 */
    if (!Y.playing) return;
    if (gal_type(Y.playing) == 2) gal_voice_free(); else music_stop();
    gal_trace("stop %d", Y.playing);
    Y.playing = 0; Y.play_t = 0;
}

static void gal_release(void) {
    gal_art_free(); gal_voice_free();
    if (Y.snd443 >= 0) { snd_unload(Y.snd443); Y.snd443 = -1; }
    free(Y.text); Y.text = NULL;
}

static void gal_dump(void) {                             /* host trace of what the screen shows */
#ifdef FP_HOST_ENV
    char c[101], d[101], a[57], r[11];
    for (int id = 1; id <= 100; id++) { c[id - 1] = (char)('0' + save_card_state(id)); d[id - 1] = (char)('0' + gal_dir(id)); }
    for (int k = 1; k <= SAVE_ACH; k++) a[k - 1] = (char)('0' + save_ach(k));
    for (int i = 0; i < 10; i++) { int f = gal_row_frame(i); r[i] = f == 15 ? 'L' : (char)('0' + f); }
    c[100] = d[100] = a[56] = r[10] = 0;
    printf("gal cards %s\ngal dirs %s\ngal ach %s\ngal rows %s\n", c, d, a, r);
#endif
}

static void gal_enter(void) {
    memset(&Y, 0, sizeof Y);
    Y.D = -1; Y.art = -1; Y.panel = -1; Y.voice_h = -1; Y.retv = 52; Y.art_tex.id = -1;   /* RE start: cursor on the header (RV "CARDS") */
    ui_scene_t *s = &s_scr; ui_obj_t *o;
    for (int i = 0; i < s->nobj; i++) { s->o[i].vis = 1; s->o[i].dx = s->o[i].dy = 0; s->o[i].shz = 0; s->o[i].shx = s->o[i].shy = 0; }
    for (int k = 0; (o = ui_find(s, "mapicon", k)); k++) {          /* 100 cards: x 238 + 24c, y 115 + 40r, id = 10r + c + 1 */
        int c = (o->x - 238) / 24, r = (o->y - 115) / 40;
        if (c >= 0 && c < 10 && r >= 0 && r < 10) Y.card[10 * r + c + 1] = o;
    }
    for (int k = 0; (o = ui_find(s, "achievement", k)); k++) {      /* 8 x 7 icons: x 164 + 40c, y 103 + 40r, k = 8r + c + 1 */
        int c = (o->x - 164) / 40, r = (o->y - 103) / 40;
        if (c >= 0 && c < 8 && r >= 0 && r < 7) Y.ach[8 * r + c + 1] = o;
    }
    for (int k = 0; (o = ui_find(s, "stageicon1", k)); k++) { int r = (o->y - 102) / 40; if (r >= 0 && r < 10) Y.row[r] = o; }
    Y.ret = ui_find(s, "return", 0); Y.header = ui_find(s, "header", 0); Y.ar1 = ui_find(s, "arrow1", 0); Y.ar2 = ui_find(s, "arrow2", 0);
    Y.cursor = ui_find(s, "crystalcursor", 0); Y.selector = ui_find(s, "cardselector", 0);
    Y.data[0] = ui_find(s, "return2", 0); Y.data[1] = ui_find(s, "return3", 0); Y.data[2] = ui_find(s, "return4", 0); Y.data[3] = ui_find(s, "return5", 0);
    Y.sparkle = ui_find(s, "sparkle5", 0); Y.burst = ui_find(s, "effectswhiteburst", 0); Y.officer = ui_find(s, "npcstofficer", 0);
    if ((o = ui_find(s, "ringsparkle", 0))) o->vis = 0;               /* idle twinkles (RE 1027): not ported */
    for (int k = 0; (o = ui_find(s, "fadein", k)); k++) o->vis = 0;   /* drawn by tiles_draw */
    for (int k = 0; (o = ui_find(s, "checkerpattern", k)); k++) o->tileh = 1;   /* the side columns repeat down the view */
    gal_text_load();
    Y.snd443 = snd_load("gal/s443.wav");
    /* RE start event + loop_check_a_10 (992): Card Collector for every stage row whose 10 cards are all taken; the
     * original saves and shows frame 9 (the "UNLOCKED" pop-up), which announces every value-1 achievement (-> 2) and
     * saves again. The port shows that pop-up as an overlay before the gallery takes input (INFERRED layout). */
    int any = 0;
    for (int i = 0; i < 10; i++) {
        if (save_ach(23 + i) != 0) continue;
        int all = 1;
        for (int c = 1; c <= 10; c++) all &= save_card_has(10 * i + c);
        if (all) { save_ach_set(23 + i, 1); any = 1; gal_trace("ach %d earned", 23 + i); }
    }
    if (any) save_store();
    for (int k = 1; k <= SAVE_ACH; k++) if (save_ach(k) == 1) Y.toast[Y.ntoast++] = k;
    tiles_start_layout(112, 14, 96); wipe.on = 0;                  /* FadeIn_146 x112: 14 x 8 tiles from (96,0) */
    cam_x = 320 - 213; cam_y = 0;                                   /* RV: view centre x 320 */
    music_stop();
    music_play("bgm/m_results.adp");                                /* RE 1136, RV t_gal5: m_results on channel 31 */
    gal_trace("enter");
    gal_dump();
}

static float gal_target(void) {                          /* RE 1030/1031 (cards, 30 px per row), 1062/1063 (achievements, 25 px); DATA none */
    return Y.A == 0 ? -(Y.D + 1) * 30.f : Y.A == 1 ? -(Y.D + 1) * 25.f : 0.f;
}

static void gal_leave(void) {                            /* RE 1083 + LEAVING 1111-1127 */
    gal_sfx(56, 230); gal_stop_item(); music_stop();
    Y.input = 0; Y.leaving = 1;
    if (Y.mark[0]) for (int s = 0; s < SAVE_STAGES; s++) for (int c = 0; c < SAVE_CHARS; c++) SV.rec[s][c].time = 0;      /* x3..x7 odd rows */
    if (Y.mark[1]) for (int s = 0; s < SAVE_STAGES; s++) for (int c = 0; c < SAVE_CHARS; c++) SV.rec[s][c].crystals = 0;  /* x3..x7 even rows */
    if (Y.mark[2]) { memset(SV.cards, 0, sizeof SV.cards); memset(SV.cards_open, 0, sizeof SV.cards_open); }               /* x20 y1..100 */
    if (Y.mark[3]) for (int k = 1; k <= SAVE_ACH; k++) if (k != 42) save_ach_set(k, 0);  /* x2 except y42 ("Milla Unlocked!", RE 1004) */
    if (Y.mark[0] || Y.mark[1] || Y.mark[2] || Y.mark[3]) { Y.dirty = 1; gal_trace("delete %d%d%d%d", Y.mark[0], Y.mark[1], Y.mark[2], Y.mark[3]); }
    /* RE 1125: records.dat is written once here. The port writes only when something changed (opened / closed cards,
     * deletes), so leaving the gallery never replaces a save it could not read (INFERRED deviation: fewer VMU writes) */
    if (Y.dirty) { save_store(); gal_trace("save"); }
    wipe_start();
}

static void gal_burst(int id) {                          /* EffectsWhiteBurst on reveal / close (RE 1039 / 1041) */
    if (!Y.burst || !Y.card[id]) return;
    Y.burst_on = 1; ui_obj_select(&s_scr, Y.burst, 4, 0); anim_restart(&Y.burst->an);
    Y.bx = Y.card[id]->x; Y.by = Y.card[id]->y + Y.sv;
}

static void gal_card_action(int id, int close) {
    int dir = gal_dir(id), ty = gal_type(id);
    const char *f = GAL_FILE(id);
    char nm[48];
    if (close) {                                          /* RE 1041: Close Card -> face down (records x20 = 1) */
        if (dir < 3) return;
        if (Y.playing == id) gal_stop_item();
        gal_sfx(443, 230); save_card_open(id, 0); Y.dirty = 1;
        gal_burst(id);
        gal_trace("card %d state 1", id);
        return;
    }
    if (dir == 1) { gal_sfx(32, 230); Y.selB = 1; gal_trace("open %d", id); return; }   /* RE 1034: open */
    if (dir == 3 || dir == 4) {
        if (Y.playing == id) { gal_stop_item(); Y.selB = -2; return; }                  /* RE 1055: stop */
        gal_stop_item();
        if (ty == 2) {                                                                  /* RE 1043-1050: sound, once */
            music_stop();                                                               /* channel 31: m_results does not come back */
            if (gal_voice(f + 1) == 0) { Y.playing = id; gal_trace("play %s", f + 1); }
        } else {                                                                        /* RE 1042: music, loop = type digit */
            snprintf(nm, sizeof nm, "bgm/%s.adp", f + 1);
            snd_stop_all();
            if (ty == 1) music_play_once(nm); else music_play(nm);                      /* type 3 loops (INFERRED) */
            Y.playing = id; gal_trace("play %s %s", nm, ty == 1 ? "once" : "loop");
        }
        Y.selB = -2;
        return;
    }
    if (dir == 5) {                                                                     /* RE 1060: view art */
        if (gal_art_show(atoi(f + 1)) == 0) gal_sfx(443, 230);
        Y.selB = -2;
    }
}

static void gal_update(const input_t *now) {
    const input_t *in = &Y.prev;
    uint32_t p = in->pressed;
    int jump = p & (BTN_JUMP | BTN_START), back = p & BTN_ATTACK, special = p & BTN_SPECIAL;
    Y.t++;
    if (!Y.input && !Y.leaving && !Y.ntoast) { if (tiles_step()) Y.input = 1; }      /* RE 1007: FADEIN -> INPUT */
    else if (Y.ntoast && !Y.leaving) {                                                  /* the "UNLOCKED" pop-up (frame 9) */
        if (tiles_left) tiles_step();
        else {
            if (Y.toast_t == 0) { snd_play_orig(100, 255, 128); gal_trace("toast %d", Y.toast[Y.toast_i]); }
            Y.toast_t++;
            if (Y.toast_t >= 150 || (Y.toast_t > 30 && jump)) {                       /* 3 per page; A skips (INFERRED) */
                for (int k = Y.toast_i; k < Y.toast_i + 3 && k < Y.ntoast; k++) save_ach_set(Y.toast[k], 2);
                Y.toast_i += 3; Y.toast_t = 0;
                if (Y.toast_i >= Y.ntoast) { Y.ntoast = 0; save_store(); gal_trace("toast done"); Y.input = 1; jump = 0; }
            }
        }
    }
    /* RE 1028: the scroll group eases 1/8 per frame; Return_110 follows the header */
    Y.sv = (7 * Y.sv + gal_target()) / 8;
    Y.retv = (7 * Y.retv + (76 + Y.sv - 24)) / 8;                    /* RE 1028: Return.Y = Header.y - 24 (RV y 52) */
    Y.arrow_d = Y.D == -1 ? (Y.arrow_d + 2) % 48 : 0;                                   /* RE 1089 */
    if (Y.selB > 0) {                                                                    /* RE 1035-1039: opening (counts from the frame after the press) */
        int id = 10 * Y.D + Y.col + 1;
        Y.selB++;
        if (Y.selB == 50) {
            save_card_open(id, 1); Y.dirty = 1; gal_sfx(40, 230); gal_trace("card %d state 2", id);
            gal_burst(id);
        }
        if (Y.selB > 100) Y.selB = 0;
    } else if (Y.selB < 0) Y.selB++;                                                     /* RE 1058: 2-frame lockout */
    if (Y.input && !Y.leaving) {
        int busy = Y.art >= 0 || (Y.A == 0 && Y.selB != 0);
        if (Y.art >= 0) {                                                               /* RE 1061: A closes the art; nothing else */
            if (jump && Y.selB == 0) { gal_art_free(); gal_sfx(443, 230); Y.selB = -2; }
        } else if (Y.panel >= 0) {                                                       /* DATA confirm panel (RE 1073-1077) */
            if (p & (BTN_UP | BTN_DOWN)) { Y.pD = Y.pD == 1 ? 2 : 1; gal_sfx(64, 220); Y.S += 7; }   /* RV: Down NO -> YES (wrap INFERRED) */
            if (jump && Y.pD == 1) { gal_sfx(63, 230); Y.mark[Y.panel] = 1; Y.panel = -1; gal_trace("mark %d", Y.D); }
            else if ((jump && Y.pD == 2) || back) { gal_sfx(63, 230); Y.panel = -1; }
        } else {
            int maxD = Y.A == 0 ? 9 : Y.A == 1 ? 6 : 3;                                  /* rows: cards 0..9, achievements 0..6 (INFERRED), DATA 0..3 */
            if ((p & BTN_UP) && Y.D > -2 && !busy) { Y.D--; gal_sfx(64, 220); Y.S += 7; }
            else if ((p & BTN_DOWN) && Y.D < maxD && !busy) { Y.D++; gal_sfx(64, 220); Y.S += 7; }
            if (Y.D == -1 && (p & (BTN_LEFT | BTN_RIGHT))) {                             /* RE 1090/1091: pages wrap */
                Y.A = (Y.A + ((p & BTN_RIGHT) ? 1 : 2)) % 3; gal_sfx(64, 220); Y.S += 7; Y.I = 0;
                gal_trace("page %d", Y.A);
            } else if (Y.D >= 0 && Y.A == 0 && !busy) {                                   /* RE 1032/1033 */
                if ((p & BTN_LEFT) && Y.col > 0) { Y.col--; gal_sfx(64, 220); }
                if ((p & BTN_RIGHT) && Y.col < 9) { Y.col++; gal_sfx(64, 220); }
            } else if (Y.D >= 0 && Y.A == 1) {                                            /* RE 1064/1065 */
                if ((p & BTN_LEFT) && Y.I > 0) { Y.I--; gal_sfx(64, 220); }
                if ((p & BTN_RIGHT) && Y.I < 7) { Y.I++; gal_sfx(64, 220); }
            }
            if (back && Y.D != -2 && !busy) { Y.D = -2; gal_sfx(64, 220); Y.S += 7; }    /* RE 1083-1085 */
            else if (jump && Y.D == -2) gal_leave();
            else if (Y.A == 0 && Y.D >= 0 && Y.selB == 0 && (jump || special)) gal_card_action(10 * Y.D + Y.col + 1, !jump);
            else if (Y.A == 2 && Y.D >= 0 && jump) {                                      /* RE 1073-1077: mark / unmark */
                gal_sfx(63, 230);
                if (Y.mark[Y.D]) { Y.mark[Y.D] = 0; gal_trace("unmark %d", Y.D); } else { Y.panel = Y.D; Y.pD = 2; }   /* RV: the cursor starts on NO */
            }
        }
    }
    if (Y.playing) {                                                                     /* RE 1059: the flag clears when channel 31 is idle */
        if (gal_type(Y.playing) == 2) { if (--Y.play_t <= 0) { gal_voice_free(); Y.playing = 0; gal_trace("idle"); } }
        else if (!music_active()) { music_stop(); Y.playing = 0; gal_trace("idle"); }
    }
    if (Y.burst_on && Y.burst) { anim_tick(&Y.burst->an, -1); if (Y.burst->an.done) Y.burst_on = 0; }
    if (Y.selB > 0 && Y.selB < 50 && Y.sparkle) { anim_tick(&Y.sparkle->an, -1); if (Y.sparkle->an.done) anim_restart(&Y.sparkle->an); }
    if (Y.S > 50) Y.S = 50;
    if (Y.S > 10) { int n = (int)((Y.S - 10) * 0.1); cam_x = 107 + fe_rand(n); cam_y = fe_rand(n); } else { cam_x = 107; cam_y = 0; }   /* as frame 5 (INFERRED) */
    if (Y.S > 0) Y.S--;
    ui_tick(&s_scr);
    Y.prev = *now;
    if (wipe_step()) {                                                                   /* RV t_gal4: frame 3 at F > 50 */
        wipe.on = 0; gal_release();
        set_state(FE_HUB); hub_enter();
    }
}

static void gal_text_box(const char *l1, const char *l2) {   /* AchievementDescription (120,8): 50 columns, 20 px lines */
    char line[64]; float y = 8;
    const char *m[2] = { l1, l2 };
    for (int k = 0; k < 2; k++, y += 20) {
        const char *s = m[k];
        if (!*s) { y -= 20; continue; }                      /* RV t_gal2_row9: a hint alone sits on the first line */
        while (*s) {
            size_t n = strlen(s), cut = n <= 50 ? n : 50;
            if (n > 50) while (cut > 0 && s[cut] != ' ') cut--;
            if (cut == 0) cut = n < 50 ? n : 50;
            memcpy(line, s, cut); line[cut] = 0;
            fe_text(13 - (cam_x - 107), y - cam_y, line);
            s += cut; while (*s == ' ' && cut < n) s++;
            if (*s) y += 20;
        }
    }
}

static void gal_place(ui_obj_t *o, float x, float y) { if (o) { o->dx = x - o->x; o->dy = y - o->y; } }

static void gal_draw(void) {
    ui_scene_t *s = &s_scr;
    int id_sel = 10 * Y.D + Y.col + 1;
    /* scroll group (RE 1028): header, arrows, stage icons, cards, achievements */
    for (int id = 1; id <= 100; id++) if (Y.card[id]) {
        ui_obj_t *o = Y.card[id];
        o->vis = Y.A == 0; o->dx = 0; o->dy = Y.sv;
        int dir = gal_dir(id);
        if (Y.selB > 0 && id == id_sel && Y.selB < 50) { ui_obj_select(s, o, 0, 1); ui_obj_frame(o, Y.selB % 11); }   /* RE 1036: flip frame B mod 11 */
        else {
            ui_obj_select(s, o, 0, dir);
            if (dir == 1) { int f = (Y.t / 3 + id / 2) % 40; ui_obj_frame(o, f < 11 ? f : 10); }   /* RE 1029: shimmer, then hold */
            else ui_obj_frame(o, 0);
        }
    }
    for (int r = 0; r < 10; r++) if (Y.row[r]) { ui_obj_t *o = Y.row[r]; o->vis = Y.A == 0; o->dy = Y.sv; ui_obj_select(s, o, 0, 0); ui_obj_frame(o, gal_row_frame(r)); }
    for (int k = 1; k <= SAVE_ACH; k++) if (Y.ach[k]) {                                 /* RE loop_ac_10: frame k at >= 2, else "?" */
        ui_obj_t *o = Y.ach[k]; o->vis = Y.A == 1; o->dy = Y.sv;
        if (save_ach(k) >= 2) { ui_obj_select(s, o, 0, 0); ui_obj_frame(o, k); } else { ui_obj_select(s, o, 0, 1); ui_obj_frame(o, 0); }
    }
    for (int i = 0; i < 4; i++) if (Y.data[i]) {                                         /* DATA rows / confirm panel */
        ui_obj_t *o = Y.data[i]; o->vis = Y.A == 2;
        if (Y.panel >= 0) { o->vis = Y.A == 2 && i < 3; ui_obj_select(s, o, 0, 1); ui_obj_frame(o, 0); }
        else { ui_obj_select(s, o, 0, 0); ui_obj_frame(o, Y.mark[i] ? 1 : 0); }
    }
    if (Y.header) { Y.header->dy = Y.sv; ui_obj_select(s, Y.header, 0, 0); ui_obj_frame(Y.header, Y.A); }   /* PC wording (dir 0): CARDS / ACHIEVEMENTS / DATA */
    if (Y.ret) Y.ret->dy = Y.retv - Y.ret->y;
    int a_alpha = Y.D == -1 ? clamp255((128 - 2 * Y.arrow_d) * 2) : 255;               /* RE 1089 */
    if (Y.ar1) { Y.ar1->dx = (float)-Y.arrow_d; Y.ar1->dy = Y.sv; }
    if (Y.ar2) { Y.ar2->dx = (float)Y.arrow_d; Y.ar2->dy = Y.sv; }
    if (Y.officer) { Y.officer->vis = Y.playing == 68; if (Y.officer->vis) ui_obj_select(s, Y.officer, 5, 0); }   /* RE 1050-1054: the bird guard */
    ui_obj_t *hide[] = { Y.header, Y.ret, Y.ar1, Y.ar2, Y.cursor, Y.selector, Y.sparkle, Y.burst };   /* drawn below, by hand */
    for (int i = 0; i < 8; i++) if (hide[i]) hide[i]->vis = 0;
    ui_draw(s, cam_x, cam_y, 0, 1);
    /* header group: dimmed while the grid has focus (RV; alpha UNKNOWN, 50 % INFERRED) */
    uint32_t dim = Y.D >= 0 ? 0x80ffffff : 0xffffffff;
    if (Y.header) { Y.header->vis = 1; ui_draw_obj(s, Y.header, cam_x, cam_y, dim); Y.header->vis = 0; }
    if (Y.ret) { Y.ret->vis = 1; ui_draw_obj(s, Y.ret, cam_x, cam_y, dim); Y.ret->vis = 0; }
    uint32_t ac = Y.D >= 0 ? 0x80ffffff : ((uint32_t)a_alpha << 24) | 0xffffff;
    if (Y.ar1) { Y.ar1->vis = 1; ui_draw_obj(s, Y.ar1, cam_x, cam_y, ac); Y.ar1->vis = 0; }
    if (Y.ar2) { Y.ar2->vis = 1; ui_draw_obj(s, Y.ar2, cam_x, cam_y, ac); Y.ar2->vis = 0; }
    ui_draw(s, cam_x, cam_y, 2, 2);
    /* card selector (RE 1095 / 1028): at (cursor.x + 90 + 24 A, cursor.y - 3), hidden while a card opens */
    float cx = 0, cy = 0; int cdir = 0, show_cursor = Y.input && !Y.leaving && Y.art < 0;
    if (Y.D == -2) { if (Y.ret) ui_action_point(Y.ret, &cx, &cy); }
    else if (Y.D == -1) { if (Y.header) ui_action_point(Y.header, &cx, &cy); }
    else if (Y.A == 0 && Y.row[Y.D]) { cx = Y.row[Y.D]->x - 24; cy = Y.row[Y.D]->y + Y.sv + 16; }   /* RE 1095 */
    else if (Y.A == 1 && Y.ach[Y.I + 1 + 8 * Y.D]) { ui_obj_t *o = Y.ach[Y.I + 1 + 8 * Y.D]; cx = o->x + 16; cy = o->y + Y.sv; cdir = 8; }   /* RE 1102: dir 8 at (icon x + 16, icon y) */
    else if (Y.A == 2) { ui_obj_t *o = Y.panel >= 0 ? Y.data[Y.pD] : Y.data[Y.D]; if (o) ui_action_point(o, &cx, &cy); }
    if (Y.selector && Y.A == 0 && Y.D >= 0 && Y.selB <= 0 && show_cursor) {
        gal_place(Y.selector, cx + 90 + 24 * Y.col, cy - 3);
        Y.selector->vis = 1; ui_draw_obj(s, Y.selector, cam_x, cam_y, 0xffffffff); Y.selector->vis = 0;
    }
    if (Y.cursor && show_cursor && !Y.ntoast) {
        ui_obj_select(s, Y.cursor, 0, cdir); gal_place(Y.cursor, cx, cy);
        Y.cursor->vis = 1; ui_draw_obj(s, Y.cursor, cam_x, cam_y, 0xffffffff); Y.cursor->vis = 0;
    }
    if (Y.sparkle && Y.selB > 0 && Y.selB < 50 && Y.card[id_sel]) {                     /* RE 1037: spiral (sin, UNKNOWN vs cos) */
        ui_obj_t *c = Y.card[id_sel];
        ui_obj_select(s, Y.sparkle, 4, 0);
        gal_place(Y.sparkle, c->x + 12.f * (float)sin(20.0 * Y.selB / 57.29577951308232), c->y + Y.sv + 12 - Y.selB / 2.f);
        Y.sparkle->vis = 1; ui_draw_obj(s, Y.sparkle, cam_x, cam_y, 0xffffffff); Y.sparkle->vis = 0;
    }
    if (Y.burst && Y.burst_on) { gal_place(Y.burst, Y.bx, Y.by); Y.burst->vis = 1; ui_draw_obj(s, Y.burst, cam_x, cam_y, 0xffffffff); Y.burst->vis = 0; }
    /* top-left text (RE 1096-1102, 1112): line 1 label / name, line 2 hint / description */
    if (Y.input && !Y.leaving && Y.art < 0 && Y.D >= 0) {
        if (Y.A == 0) {
            int dir = gal_dir(id_sel);
            if (dir == 1) gal_text_box("", GAL_WARN(4));
            else if (dir == 2) gal_text_box("", GAL_WARN(7));
            else if (dir == 3 || dir == 4) gal_text_box(GAL_SONG(id_sel), GAL_WARN(Y.playing == id_sel ? 6 : 5));
            else if (dir == 5) gal_text_box(GAL_SONG(id_sel), GAL_WARN(8));
        } else if (Y.A == 1) gal_text_box(GAL_NAME(Y.I + 8 * Y.D + 1), GAL_DESC(Y.I + 8 * Y.D + 1));
        else if (Y.panel < 0 && Y.mark[Y.D]) gal_text_box(GAL_WARN(Y.D), "");
    }
    ui_draw(s, cam_x, cam_y, 3, 3);
    if (Y.cursor) Y.cursor->vis = 1;                                                     /* animated by ui_tick (drawn above) */
    if (Y.selector) Y.selector->vis = 1;
    fe_text(13, 216, "UP/DOWN: Scroll");                                                 /* HelpMessage_268 at (112,216) (RV x 13 / 341) */
    fe_text(341, 216, "B: Return");
    if (Y.art >= 0) {                                                                    /* Artwork_287 at (322,0), hotspot (w/2, 0), layer 3 on top */
        int w = Y.art_tex.srcw, h = Y.art_tex.srch;
        r_sprite(&Y.art_tex, 0, 0, w, h, 322 - w / 2 - cam_x, -cam_y, (float)w, (float)h, 0, 0xffffffff);
    }
    if (Y.ntoast && !tiles_left) {                                                       /* frame 9 "UNLOCKED" pop-up, up to 3 entries (INFERRED layout) */
        int n = Y.ntoast - Y.toast_i < 3 ? Y.ntoast - Y.toast_i : 3;
        float y0 = 60;
        r_rect(48, y0 - 8, SCREEN_W - 96, 36 + 40.f * n, 0xe0000000);
        fe_text(SCREEN_W / 2 - 4 * 8, y0 - 4, "UNLOCKED");
        for (int k = 0; k < n; k++) {
            int a = Y.toast[Y.toast_i + k];
            ui_obj_t *o = Y.ach[a];
            if (o) {
                ui_obj_select(s, o, 0, 0); ui_obj_frame(o, a);
                const frame_t *f = anim_frame(&o->an);
                if (f) frame_draw(o->an.b, f, 60 + f->hx, y0 + 22 + 40 * k + f->hy, 0, 0xffffffff);
            }
            fe_text(100, y0 + 28 + 40 * k, GAL_NAME(a));
        }
    }
    if (!Y.input && !Y.leaving && tiles_left) tiles_draw(s);
    wipe_draw();
}
