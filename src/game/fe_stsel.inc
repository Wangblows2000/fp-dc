/* Time Attack stage select (frame index 5 = on_frame_6; RE: build/re/fe/spec_stageselect.md, events 612-745).
 * Included by frontend.c. 13-entry list (index 0 Shang Mu Academy, 1 Dragon Valley, ...), icons remapped
 * per character every frame (RE 623-643); locked entries can be selected but not confirmed. The port has
 * Dragon Valley only, so Shang Mu Academy (unlocked in the original) is shown but cannot be started
 * (DECISIONS D-020). */
static const int STAGE_LILAC[13] = { 14, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13 };   /* icon frames E (RE 623-631) */
static struct {
    int main, slide, confirm_on, back, A, D, S, T, latch, exit_to, sel_k;
    float checkA;
    input_t prev;
    ui_obj_t *icon[13], *name, *cursor, *ret, *start, *yes, *mahjong, *title;
    float iv[13], iw[13], ix[13], iy[13];                  /* icon eased position V/W, target X/Y */
    float nv, nw, nx, ny;                                  /* StageName V/W and home X/Y */
    float startx, yesx, retx;
    int e[13];
    ui_obj_t *rec[9];
    float bgB;
} T;

static int stsel_progress(void) { return 0; }              /* RecordData(1, 40): stages beyond DV are not in the port */
/* Dragon Valley for Lilac / Carol; Aqua Tunnel for Milla (session 12: RE 623 puts AT (E 0) in DV's slot for G1 = 1 / 4) */
static int stage_playable(int e) { return chr_is_milla(G.character) ? e == 0 : e == 1; }

static void stsel_enter(void) {
    memset(&T, 0, sizeof T);
    ui_scene_t *s = &s_scr;
    for (int i = 0; i < s->nobj; i++) { s->o[i].vis = 1; s->o[i].dx = s->o[i].dy = 0; s->o[i].shz = 0; s->o[i].shx = s->o[i].shy = 0; }
    ui_obj_t *o;
    for (int k = 0; (o = ui_find(s, "stageicon1", k)) && k < 13; k++) {
        int idx = (o->y - 48) / 40;                        /* A = E = 0..12 top to bottom (RE start) */
        if (idx >= 0 && idx < 13) T.icon[idx] = o;
    }
    for (int i = 0; i < 13; i++) if (T.icon[i]) { T.iv[i] = T.ix[i] = T.icon[i]->x; T.iw[i] = T.iy[i] = T.icon[i]->y; }
    { ui_obj_t *lk = ui_find(s, "lockedback", 0); int win = lk && lk->an.b ? anim_frame(&lk->an)->w : 272;   /* preview window = the lock panel (272 px) */
      for (int i = 0; i < s->nobj; i++) if (!strncmp(s->o[i].cls, "bg", 2) && strstr(s->o[i].cls, "layer")) s->o[i].tilew = win; }   /* INFERRED: strips narrower than the window repeat */
    T.name = ui_find(s, "stagename", 0); T.cursor = ui_find(s, "crystalcursor", 0); T.ret = ui_find(s, "return", 0);
    T.start = ui_find(s, "start", 0); T.yes = ui_find(s, "yes", 0); T.mahjong = ui_find(s, "mahjong", 0); T.title = ui_find(s, "stageselect", 0);
    if (T.name) { T.nv = T.nx = T.name->x; T.nw = T.ny = T.name->y; }
    T.startx = T.yesx = 107 + 506; T.retx = T.ret ? T.ret->x : 479;      /* RE 744 */
    static const char *rn[9] = { "recordback", "recordlabel", "recordcolon2", "recordcolon2", "timerecordmin", "timerecordsec", "timerecordmil", "crystalrecord", "active11" };
    for (int i = 0; i < 9; i++) T.rec[i] = ui_find(s, rn[i], i == 3 ? 1 : 0);
    static const char *hide[] = { "fadein", "fade", "fadeedge", "helpmessage", "listhelpmessage", "icona", "iconb", "ringsparkle",
                                  "backgroundsystembox2", "mouseobject", "guirings", "guienergy", "guilifebasemeter", "guicrystal",
                                  "playerothers", "controllermusic", "masterlevel", 0 };
    for (int i = 0; hide[i]; i++) for (int k = 0; (o = ui_find(s, hide[i], k)); k++) o->vis = 0;
    if (T.mahjong) T.mahjong->vis = 0;                      /* RE 617: only when unlocked (records) */
    tiles_start(); wipe.on = 0;
    cam_x = 320 - 213; cam_y = 0;
    music_play("bgm/m_menu1.adp");                          /* start: m_menu1 if channel 31 is silent */
}

static void ease_shake(ui_scene_t *s) {
    for (int i = 0; i < s->nobj; i++) {
        ui_obj_t *o = &s->o[i];
        if (o->shz > 0) {
            double a = 200.0 * o->shz, c = (a == 90. || a == 270.) ? 0. : cos(a / 57.29577951308232), sn = a == 180. ? 0. : sin(a / 57.29577951308232);
            o->shx = (int)floor(c * o->shz); o->shy = (int)floor(sn * o->shz * 0.5);
            o->shz -= 0.4f; if (o->shz < 0.01f) { o->shz = 0; o->shx = o->shy = 0; }
        }
    }
}

static void stsel_update(const input_t *now) {
    const input_t *in = &T.prev;
    uint32_t p = in->pressed;
    int jump = p & (BTN_JUMP | BTN_START), back = p & BTN_ATTACK;
    T.bgB += 1;
    T.checkA = fmodf(T.checkA + 0.5f, 64.f);
    if (!T.main && !T.slide && !T.confirm_on && !T.back && !wipe.on) { if (tiles_step()) T.main = 1; }   /* RE 616 */
    /* MAP (RE 623-643): Lilac's list and the locks */
    for (int i = 0; i < 13; i++) {
        T.e[i] = STAGE_LILAC[i];
        if (chr_is_milla(G.character) && T.e[i] == 1) T.e[i] = 0;                       /* RE 623: Aqua Tunnel replaces Dragon Valley */
        if (chr_is_carol(G.character) || chr_is_milla(G.character)) { if (i == 6) T.e[i] = 8; if (i == 7) T.e[i] = 6; }   /* RE 624 / 625 */
        if (i > 1 && T.e[i] != 14 && T.e[i] > stsel_progress()) T.e[i] = 99;           /* RE 641-643 locks */
    }
    if (T.main) {
        /* RE 660/661: ease to home / targets 1/8 per frame */
        T.nv = (7 * T.nv + T.nx) / 8; T.nw = (7 * T.nw + T.ny) / 8;
        for (int i = 0; i < 13; i++) { T.iv[i] = (7 * T.iv[i] + T.ix[i]) / 8; T.iw[i] = (7 * T.iw[i] + T.iy[i]) / 8; }
        if (T.A == 0 && (p & BTN_UP) && T.D > 0) { T.D--; snd_play_orig(64, 220, 128); if (T.name) T.name->shz = 4; T.S += 7; for (int i = 0; i < 13; i++) T.iy[i] += 30; }     /* 674 */
        if (T.A == 0 && (p & BTN_DOWN) && T.D < 12) { T.D++; snd_play_orig(64, 220, 128); if (T.name) T.name->shz = 4; T.S += 7; for (int i = 0; i < 13; i++) T.iy[i] -= 30; } /* 675 */
        int mj = T.mahjong && T.mahjong->vis;
        if ((p & BTN_RIGHT) && T.A == 0) { T.A = 1; snd_play_orig(64, 220, 128); T.S += 7; T.T += 5; }
        if (p & BTN_LEFT) { if (T.A > 0) { T.A--; snd_play_orig(64, 220, 128); T.S += 7; } }
        else if (T.A == 1 && !mj) T.A = 2;
        if (T.T > 0) T.T--;
        if ((p & BTN_DOWN) && T.A == 1 && T.T <= 0) { T.A = 2; snd_play_orig(64, 220, 128); }
        if ((p & BTN_UP) && T.A == 2 && mj) { T.A = 1; snd_play_orig(64, 220, 128); }
        if ((p & BTN_LEFT) && T.A == 1 && !mj) T.A = 0;
        if (T.A == 2 && T.ret && T.ret->shz == 0 && T.prev.pressed == 0) { }
        if (jump) T.latch = 1;                                                                    /* 683 */
        int playable = T.D <= 12 && T.e[T.D] < 99 && stage_playable(T.e[T.D]);                  /* 684 (+ port: in the port) */
        if (T.latch && T.A == 0 && playable) {                                                    /* 686 choose */
            snd_play_orig(63, 230, 128); T.main = 0; T.slide = 1; T.latch = 0; T.sel_k = T.D;
            T.retx = 107 + 506; if (T.ret) ui_obj_select(&s_scr, T.ret, 0, 8);
            if (T.title) T.title->vis = 0;
        }
        if (T.latch && T.A == 0) T.latch = 0;
        if ((T.latch && T.A == 2) || (back && T.A == 2)) {                                        /* 689 RETURN */
            snd_play_orig(63, 230, 128); T.main = 0; wipe_start(); T.exit_to = 6; T.latch = 0;
        } else if (back && T.A != 2) { snd_play_orig(64, 220, 128); T.A = 2; T.S += 7; T.T += 5; if (T.ret) T.ret->shz = 4; }
    }
    if (T.slide) {                                                                                /* 691-697 */
        int k = T.sel_k;
        /* RV (r_ta trace: YES active 68 frames after the confirm): the slide works on integer positions */
        T.iv[k] = (int)((9 * T.iv[k] + 184) * 0.1f); T.iw[k] = (int)((9 * T.iw[k] + 16) * 0.1f);
        T.nv = (int)((9 * T.nv + 336) * 0.1f); T.nw = (int)((9 * T.nw + 32) * 0.1f);
        for (int i = 0; i < 13; i++) if (i != k) T.iv[i] = (int)((9 * T.iv[i] - 80) * 0.1f);
        if (T.nw <= 37) {
            if (T.startx > 320) T.startx = T.startx - 20 < 320 ? 320 : T.startx - 20;            /* in order, same frame */
            if (T.startx <= 320 && T.yesx > 320) T.yesx = T.yesx - 20 < 320 ? 320 : T.yesx - 20;
            if (T.yesx <= 320 && T.retx > 320) T.retx = T.retx - 20 < 320 ? 320 : T.retx - 20;
        }
        if (T.retx < 321) { T.slide = 0; T.confirm_on = 1; T.A = 2; }
    }
    if (T.confirm_on) {                                                                           /* 699-706 */
        if (jump) T.latch = 1;
        if (T.latch && T.A == 2) {                                                                /* 702 YES */
            snd_play_orig(56, 230, 128); music_stop(); T.A = 9; T.confirm_on = 0; wipe_start(); T.exit_to = 20; T.latch = 0;
        } else if ((T.latch && T.A == 3) || (back && T.A == 3)) {                                 /* 704 NO */
            T.confirm_on = 0; T.back = 1; T.latch = 0; T.startx = T.yesx = 107 + 506;
            if (T.title) T.title->vis = 1;
            T.retx = 479; if (T.ret) ui_obj_select(&s_scr, T.ret, 0, 0);
        } else {
            if ((p & BTN_UP) && T.A > 2) { T.A--; snd_play_orig(64, 220, 128); if (T.yes) T.yes->shz = 4; T.S += 7; }
            if (((p & BTN_DOWN) || back) && T.A < 3) { T.A++; snd_play_orig(64, 220, 128); if (T.ret) T.ret->shz = 4; T.S += 7; }
        }
    }
    if (T.back) { T.back = 0; T.main = 1; T.A = 0; }                                             /* BACK: one frame, then MAIN */
    if (T.S > 10) { int n = (int)((T.S - 10) * 0.1); cam_x = 107 + fe_rand(n); cam_y = fe_rand(n); } else { cam_x = 107; cam_y = 0; }   /* 731 */
    if (T.S > 0) T.S--;
    ease_shake(&s_scr);
    ui_tick(&s_scr);
    T.prev = *now;
    if (wipe_step()) {
        wipe.on = 0;
        if (T.exit_to == 6) { if (scr_load(0) == 0) { set_state(FE_CHSEL); chsel_enter(); } }
        else { const char *lv = level_first_of_stage(T.e[T.sel_k]); fe_unload(); game_start_adventure(lv ? lv : "dv1", 1); }   /* RE 714 / 715: icon E = stage index -> its first act (levels.c) */
    }
}

static void stsel_counter(ui_obj_t *o, int v, int digits) {     /* Counter: zero-padded, right edge at the object x (INF) */
    if (!o) return;
    char buf[8]; snprintf(buf, sizeof buf, "%0*d", digits, v);
    float x = o->x + o->dx + o->shx - cam_x, y = o->y + o->dy - cam_y;
    for (int i = (int)strlen(buf) - 1; i >= 0; i--) {
        ui_obj_frame(o, buf[i] - '0');
        const frame_t *f = anim_frame(&o->an);
        if (!f) return;
        x -= f->w;
        frame_draw(o->an.b, f, x + f->hx, y - f->h + f->hy, 0, 0xffffffff);
    }
}

static void stsel_draw(void) {
    ui_scene_t *s = &s_scr;
    ui_obj_t *ck = ui_find(s, "checkerpattern", 0);
    if (ck) { ck->dy = -T.checkA; ck->tileh = 1; }   /* layer 1 wraps vertically (RV orig_tasel_s13: the checker fills the left column) */
    int sel_e = T.e[T.D];
    int f = sel_e >= 99 ? 16 : sel_e;                        /* RE 730: StageName frame = min(E, 16) */
    /* BG preview (RE 612/613): layers at (C - fmod(B, A), E - 48 f); only the stages in the port are imported */
    for (int i = 0; i < s->nobj; i++) {
        ui_obj_t *o = &s->o[i];
        if (strncmp(o->cls, "bg", 2) || !strstr(o->cls, "layer")) continue;
        o->dy = -48.f * f;
    }
    for (int i = 0; i < 13; i++) if (T.icon[i]) {
        ui_obj_t *o = T.icon[i];
        o->dx = T.iv[i] - o->x; o->dy = T.iw[i] - o->y;
        ui_obj_select(s, o, 0, (T.main && i == T.D) || T.confirm_on || (T.slide && i == T.sel_k) ? 0 : 31);   /* 664/692 */
        ui_obj_frame(o, T.e[i] > 15 ? 15 : T.e[i]);
    }
    if (T.name) { T.name->dx = T.nv - T.name->x; T.name->dy = T.nw - T.name->y; ui_obj_select(s, T.name, 0, 0); ui_obj_frame(T.name, f); }
    if (T.start) T.start->dx = T.startx - T.start->x;
    if (T.yes) T.yes->dx = T.yesx - T.yes->x;
    if (T.ret) T.ret->dx = T.retx - T.ret->x;
    /* record box (RE 665-667): hidden for Shang Mu (D == 0); "TIME --:--:--" without a time */
    int cs = chr_slot(G.character);                          /* RE 657 / 658: the record column of the selected character */
    uint32_t ms = sel_e < SAVE_STAGES ? (uint32_t)((uint64_t)SV.rec[sel_e][cs].time * 1000 / 60) : 0;
    int crys = sel_e < SAVE_STAGES ? SV.rec[sel_e][cs].crystals : 0;
    int box = T.main && T.D != 0;
    for (int i = 0; i < 9; i++) if (T.rec[i]) T.rec[i]->vis = box;
    if (T.rec[1]) ui_obj_select(s, T.rec[1], 0, ms ? 0 : 8);
    for (int i = 4; i <= 7; i++) if (T.rec[i]) T.rec[i]->vis = 0;
    if (!ms) { if (T.rec[2]) T.rec[2]->vis = 0; if (T.rec[3]) T.rec[3]->vis = 0; }
    if (T.cursor) T.cursor->vis = 0;
    ui_draw(s, cam_x, cam_y, 0, 3);
    if (box) {
        if (ms) { stsel_counter(T.rec[4], ms / 60000 % 100, 2); stsel_counter(T.rec[5], ms / 1000 % 60, 2); stsel_counter(T.rec[6], ms / 10 % 100, 2); }
        stsel_counter(T.rec[7], crys, 4);
    }
    fe_text(12, 220, "A: Select");
    fe_text(SCREEN_W - 12 - 9 * 8, 220, "B: Return");
    if (T.cursor && (T.main || T.confirm_on)) {
        float x = 0, y = 0;
        if (T.main && T.A == 0 && T.icon[T.D]) { ui_obj_select(s, T.cursor, 0, 8); x = T.iv[T.D] + 24; y = T.iw[T.D]; }       /* 663 */
        else {
            ui_obj_select(s, T.cursor, 0, 0);
            ui_obj_t *tgt = T.confirm_on ? (T.A == 2 ? T.yes : T.ret) : (T.A == 1 ? T.mahjong : T.ret);
            if (tgt) ui_action_point(tgt, &x, &y);
        }
        T.cursor->dx = x - T.cursor->x; T.cursor->dy = y - T.cursor->y;
        T.cursor->vis = 1; ui_draw_obj(s, T.cursor, cam_x, cam_y, 0xffffffff); T.cursor->vis = 0;
    }
    if (!T.main && !T.slide && !T.confirm_on && !wipe.on && tiles_left) tiles_draw(s);
    wipe_draw();
}
