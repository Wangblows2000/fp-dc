/* Data-driven cutscene player (session 24): see cutscene.h for the cue table and how to add a scene. Port design (no
 * original counterpart is ported: the original's story rooms run on Fusion events and DialogData_1624 tables); the text box
 * uses the original font (font.bmp, 8x20) and the dialog box layout is INFERRED. Static state only. */
#include <stdio.h>
#include <string.h>
#include "cutscene.h"

#define BOX_X 16
#define BOX_Y 8
#define BOX_W (SCREEN_W - 32)
#define COLS ((BOX_W - 16) / 8)
#define ROWS 3

static struct {
    const cs_scene_t *sc; ui_scene_t *spr; const bank_t *font;
    int i, clock, active, done;
    struct { ui_obj_t *o; float x, y, x0, y0, x1, y1; int t, n, on; animator_t an; } s[CS_SLOTS];
    const char *speaker; int nspk; const char *say; int len, shown, rate, acc, wait_a;
    int wait_t, fade, fade_n, fade_dir;
    uint32_t prev;
} C;

int cs_active(void) { return C.active; }
int cs_cue_index(void) { return C.i; }

void cs_begin(const cs_scene_t *sc, ui_scene_t *sprites, const bank_t *font) {
    memset(&C, 0, sizeof C);
    C.sc = sc; C.spr = sprites; C.font = font; C.active = sc && sc->cue;
#ifdef FP_HOST_ENV
    printf("cs begin %s\n", sc ? sc->name : "-");
#endif
}

static void fire(const cs_cue_t *c) {
    switch (c->op) {
    case CS_SPRITE: if (c->slot < CS_SLOTS && C.spr) {
        ui_obj_t *o = ui_find(C.spr, c->str, 0);
        C.s[c->slot].o = o; C.s[c->slot].on = o != NULL; C.s[c->slot].x = c->x; C.s[c->slot].y = c->y; C.s[c->slot].n = 0;
        if (o && ui_obj_select(C.spr, o, c->arg >> 8, c->arg & 255) == 0) { C.s[c->slot].an = o->an; anim_restart(&C.s[c->slot].an); }
        else if (o) C.s[c->slot].an = o->an;
    } break;
    case CS_MOVE: if (c->slot < CS_SLOTS) { C.s[c->slot].x0 = C.s[c->slot].x; C.s[c->slot].y0 = C.s[c->slot].y; C.s[c->slot].x1 = c->x; C.s[c->slot].y1 = c->y;
                                            C.s[c->slot].n = c->arg > 0 ? c->arg : 1; C.s[c->slot].t = 0; } break;
    case CS_HIDE: if (c->slot < CS_SLOTS) C.s[c->slot].on = 0; break;
    case CS_SAY: {
        const char *s = c->str ? c->str : "", *bar = strchr(s, '|');
        C.speaker = bar ? s : NULL; C.nspk = bar ? (int)(bar - s) : 0; C.say = bar ? bar + 1 : s;
        C.len = (int)strlen(C.say); C.shown = 0; C.rate = c->arg > 0 ? c->arg : 1; C.acc = 0; C.wait_a = 1;
    } break;
    case CS_CLEAR: C.say = NULL; C.speaker = NULL; break;
    case CS_VOICE: if (c->str) voice_play(c->str); break;
    case CS_SFX: snd_play_orig(c->arg, 230, 128); break;
    case CS_MUSIC: if (c->str) music_play(c->str); else music_stop(); break;
    case CS_FADE: C.fade_n = c->arg > 0 ? c->arg : -c->arg; C.fade_dir = c->arg > 0 ? -1 : 1; C.fade = c->arg > 0 ? 255 : 0; break;
    case CS_WAIT: C.wait_t = c->arg; break;
    }
#ifdef FP_HOST_ENV
    printf("cs cue %d op %d\n", C.i, c->op);
#endif
}

int cs_update(const input_t *in) {
    if (!C.active) return 1;
    uint32_t p = C.prev; C.prev = in->pressed;          /* one frame of latency, as the menus (RE InputsP1) */
    if ((p & BTN_START) && C.sc->skippable && !C.done) {
        C.done = 1; C.say = NULL; C.speaker = NULL; C.fade_n = 20; C.fade_dir = 1; if (C.fade < 0) C.fade = 0;
#ifdef FP_HOST_ENV
        printf("cs skip\n");
#endif
    }
    for (int k = 0; k < CS_SLOTS; k++) if (C.s[k].on) {
        if (C.s[k].an.b) anim_tick(&C.s[k].an, -1);
        if (C.s[k].n && C.s[k].t < C.s[k].n) { C.s[k].t++; float f = (float)C.s[k].t / C.s[k].n; C.s[k].x = C.s[k].x0 + (C.s[k].x1 - C.s[k].x0) * f; C.s[k].y = C.s[k].y0 + (C.s[k].y1 - C.s[k].y0) * f; }
    }
    if (C.fade_n) { C.fade += C.fade_dir * (255 / C.fade_n + 1); if (C.fade <= 0) { C.fade = 0; C.fade_n = 0; } if (C.fade >= 255) { C.fade = 255; C.fade_n = 0; } }
    if (C.done) { if (!C.fade_n) { C.active = 0; return 1; } return 0; }
    if (C.say && C.shown < C.len) {                        /* typing */
        if (++C.acc >= C.rate) { C.acc = 0; C.shown++; }
        if (p & BTN_JUMP) C.shown = C.len;
        return 0;
    }
    if (C.say && C.wait_a) { if (p & BTN_JUMP) { C.wait_a = 0; C.clock = 0; } else return 0; }   /* the line waits for A */
    if (C.wait_t > 0) { C.wait_t--; if (!C.wait_t) C.clock = 0; return 0; }
    while (C.active) {
        const cs_cue_t *c = &C.sc->cue[C.i];
        if (c->op == CS_END) { if (!C.fade_n) { C.done = 1; } return 0; }
        if (C.clock < c->t) break;
        fire(c); C.i++;
        if (c->op == CS_SAY || c->op == CS_WAIT) { C.clock = 0; return 0; }
    }
    C.clock++;
    return 0;
}

static void txt(float x, float y, const char *s, int n) {
    if (!C.font || !C.font->nframes) return;
    for (int i = 0; i < n && s[i]; i++, x += 8) { int ch = (unsigned char)s[i] - 32; if (ch > 0 && ch < C.font->nframes) frame_draw(C.font, &C.font->frames[ch], x, y, 0, 0xffffffff); }
}
void cs_draw(void) {
    if (!C.active) return;
    for (int k = 0; k < CS_SLOTS; k++) if (C.s[k].on && C.s[k].an.b) {
        const frame_t *f = anim_frame(&C.s[k].an); if (f) frame_draw(C.s[k].an.b, f, C.s[k].x, C.s[k].y, 0, 0xffffffff);
    }
    if (C.say) {                                           /* text box: up to 3 rows, word-wrapped at draw time (no buffer) */
        r_rect(BOX_X, BOX_Y, BOX_W, 8 + ROWS * 20 + 4, 0xc0081040);
        const char *s = C.say; int left = C.shown, row = 0;
        while (*s && left > 0 && row < ROWS) {
            int n = 0, cut = 0;
            while (s[n] && n < COLS) { if (s[n] == ' ') cut = n; n++; }
            if (s[n] && cut > 0) n = cut;
            txt(BOX_X + 8, BOX_Y + 6 + row * 20, s, n < left ? n : left);
            left -= n; s += n; while (*s == ' ') { s++; left--; } row++;
        }
        if (C.speaker) { r_rect(BOX_X, BOX_Y + 8 + ROWS * 20 + 4, C.nspk * 8 + 16, 22, 0xc0081040); txt(BOX_X + 8, BOX_Y + 8 + ROWS * 20 + 6, C.speaker, C.nspk); }
    }
    if (C.fade > 0) r_rect(0, 0, SCREEN_W, SCREEN_H, (uint32_t)C.fade << 24);
}
