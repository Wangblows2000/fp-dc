/* PC platform layer (SDL2): Linux x86_64 and Windows (mingw-w64, i686).
 *
 * The game code is unchanged; this file implements the same platform API as plat_dc.c (KallistiOS) and
 * plat_host.c (headless tests). Rendering is the software reference renderer (src/renderer/render_soft.c,
 * the one the host tests' PPM dumps come from): it draws the original's 426x240 frame, and r_end() hands it to
 * pc_present() here, which uploads it to an SDL texture and scales it to the window.
 *
 * usage: fpdc [data_dir] [options]          (data_dir default: "data" next to the executable)
 *   --fullscreen / --window N (window = N x 426x240) / --scale aspect|integer|stretch / --smooth
 *   --no-vsync / --no-audio / --save <file>
 *   test & benchmark options: --level <dir> [--char n] (start a level directly, as the host's FP_LEVEL)
 *     --script <file> (host input script "<first> <end> <LRUDJXBS>", added to the pad)
 *     --frames N (quit after N game frames) --fast (no 60 Hz pacing) --shot-dir <dir> [--shot-every N] (BMP frames)
 *     --host-draw (draw only the frames written to --shot-dir, as the host harness: pc/test_pc.sh compares them)
 * Keys: arrows move, Z jump (DC A), X attack (DC B), C special (DC X), V guard (DC Y), Enter / Esc start,
 *   A / S = L / R triggers; Alt+Enter or F11 fullscreen, F10 scaling mode, F9 smooth filter, F3 VMU screen,
 *   F12 screenshot. Game controllers: SDL GameController layout (south A jump, east B attack, west X special,
 *   north Y guard, Start, LT / RT, d-pad and left stick). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <SDL.h>
#include "../fp.h"

#define VIEW_W SCREEN_W
#define VIEW_H SCREEN_H

/* plat_dc.c's verification-disc globals that game.c reads outside FP_HOST_ENV */
int demo_letterbox = -1;
int demo_char = -1;
extern int r_letterbox_mode;          /* render_soft.c: the game's SCREEN SIZE option (1: 16:9, 0: 4:3) */

static char data_dir[1200] = "data";
static char save_path[1200];
static char pref_dir[1024];
static const char *start_level;
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex_view, *tex_up;   /* 426x240 frame; integer-upscaled copy (sharp scaling) */
static int up_k;
static int opt_fast, opt_novsync, opt_noaudio, opt_smooth, opt_fullscreen, opt_window = 3, opt_frames = -1, opt_hostdraw;
static int scale_mode;                   /* 0 aspect fit (sharp), 1 integer, 2 stretch to the window */
static const char *shot_dir; static int shot_every = 1;
static int show_vmu, quit_req, frame_no, shot_req;
static FILE *log_f;

/* ------------------------------------------------------------- basics */
void plat_init(void) {}

void plat_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    if (log_f) { va_start(ap, fmt); vfprintf(log_f, fmt, ap); va_end(ap); fflush(log_f); }
}

uint64_t plat_time_us(void) {
    static uint64_t freq;
    if (!freq) freq = SDL_GetPerformanceFrequency();
    uint64_t c = SDL_GetPerformanceCounter();
    return c / freq * 1000000u + c % freq * 1000000u / freq;
}

void *plat_alloc_aligned(size_t n) { return malloc(n ? n : 1); }

const char *plat_start_level(void) { return start_level; }

void *plat_load_file(const char *name, size_t *size) {
    int found;
    void *d = pack_load(name, size, &found);
    if (d || found) return d;
    return plat_read_raw(name, size);
}

/* SDL_RWops: UTF-8 paths on Windows too */
static void *read_whole(const char *path, size_t *size, int ticks) {
    SDL_RWops *f = SDL_RWFromFile(path, "rb");
    if (!f) return NULL;
    Sint64 n = SDL_RWsize(f);
    if (n < 0 || n > 64 * 1024 * 1024) { SDL_RWclose(f); return NULL; }
    char *d = malloc(n ? (size_t)n : 1);
    if (!d) { SDL_RWclose(f); return NULL; }
    for (Sint64 got = 0; got < n;) {
        size_t k = n - got > 65536 ? 65536 : (size_t)(n - got);
        if (SDL_RWread(f, d + got, 1, k) != k) { free(d); SDL_RWclose(f); return NULL; }
        got += (Sint64)k;
        if (ticks) loading_tick();
    }
    SDL_RWclose(f);
    *size = (size_t)n;
    return d;
}

void *plat_read_raw(const char *name, size_t *size) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", data_dir, name);
    uint64_t t0 = plat_time_us();
    void *d = read_whole(path, size, 1);
    if (!d) { plat_log("missing file %s\n", path); return NULL; }
    LP.read_us += plat_time_us() - t0; LP.nfiles++;
    LP.bytes += *size; LP.nalloc++; LP.alloc_bytes += *size;
    return d;
}

/* ------------------------------------------------------------- input */
static struct { int a, b; uint32_t m; } *script; static int nscript;
static uint32_t prev_held;
#define MAX_PADS 4
static SDL_GameController *pads[MAX_PADS];

static uint32_t parse_btn(const char *s) {
    uint32_t m = 0;
    for (; *s; s++) switch (*s) {
        case 'L': m |= BTN_LEFT; break; case 'R': m |= BTN_RIGHT; break;
        case 'U': m |= BTN_UP; break; case 'D': m |= BTN_DOWN; break;
        case 'J': m |= BTN_JUMP; break; case 'X': m |= BTN_ATTACK; break;
        case 'B': m |= BTN_SPECIAL; break; case 'S': m |= BTN_START; break;
        case 'l': m |= BTN_LT; break; case 'r': m |= BTN_RT; break;
    }
    return m;
}

static void script_load(const char *path) {
    size_t n; char *t = read_whole(path, &n, 0);
    if (!t) { plat_log("script %s: cannot read\n", path); return; }
    script = malloc(sizeof *script * 8192);
    char *p = t, *end = t + n;
    while (p < end && nscript < 8192) {
        char line[128]; size_t k = 0;
        while (p < end && *p != '\n' && k < sizeof line - 1) line[k++] = *p++;
        line[k] = 0; if (p < end) p++;
        char btn[32];
        if (line[0] != '#' && sscanf(line, "%d %d %31s", &script[nscript].a, &script[nscript].b, btn) == 3 && btn[0] >= 'A')
            script[nscript++].m = parse_btn(btn);
    }
    free(t);
    plat_log("script %s: %d lines\n", path, nscript);
}

static void pad_open(int idx) {
    if (!SDL_IsGameController(idx)) return;
    for (int i = 0; i < MAX_PADS; i++) if (!pads[i]) {
        pads[i] = SDL_GameControllerOpen(idx);
        if (pads[i]) plat_log("controller: %s\n", SDL_GameControllerName(pads[i]));
        return;
    }
}

static void toggle_fullscreen(void) {
    opt_fullscreen = !opt_fullscreen;
    if (SDL_SetWindowFullscreen(win, opt_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) plat_log("fullscreen: %s\n", SDL_GetError());
    SDL_ShowCursor(opt_fullscreen ? SDL_DISABLE : SDL_ENABLE);
    int w, h; SDL_GetWindowSize(win, &w, &h);
    plat_log("%s %dx%d\n", opt_fullscreen ? "fullscreen" : "windowed", w, h);
}

static void save_screenshot(void);
static void pump_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: quit_req = 1; break;
        case SDL_RENDER_DEVICE_RESET:                 /* (Direct3D) every texture is lost: make them again */
            if (tex_up) { SDL_DestroyTexture(tex_up); tex_up = NULL; up_k = 0; }
            if (tex_view) SDL_DestroyTexture(tex_view);
            tex_view = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, VIEW_W, VIEW_H);
            plat_log("video: render device reset\n");
            break;
        case SDL_CONTROLLERDEVICEADDED: pad_open(e.cdevice.which); break;
        case SDL_CONTROLLERDEVICEREMOVED:
            for (int i = 0; i < MAX_PADS; i++)
                if (pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == e.cdevice.which) { SDL_GameControllerClose(pads[i]); pads[i] = NULL; }
            break;
        case SDL_KEYDOWN:
            if (e.key.repeat) break;
            if ((e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) || e.key.keysym.sym == SDLK_F11) toggle_fullscreen();
            else if (e.key.keysym.sym == SDLK_F10) { scale_mode = (scale_mode + 1) % 3; plat_log("scaling: %s\n", scale_mode == 0 ? "aspect" : scale_mode == 1 ? "integer" : "stretch"); }
            else if (e.key.keysym.sym == SDLK_F9) { opt_smooth = !opt_smooth; if (tex_up) { SDL_DestroyTexture(tex_up); tex_up = NULL; up_k = 0; } }
            else if (e.key.keysym.sym == SDLK_F3) show_vmu = !show_vmu;
            else if (e.key.keysym.sym == SDLK_F12) shot_req = 1;
            break;
        }
    }
}

void plat_poll_input(input_t *in) {
    uint32_t held = 0;
    pump_events();
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    int alt = (SDL_GetModState() & KMOD_ALT) != 0;
    if (k[SDL_SCANCODE_LEFT]) held |= BTN_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) held |= BTN_RIGHT;
    if (k[SDL_SCANCODE_UP]) held |= BTN_UP;
    if (k[SDL_SCANCODE_DOWN]) held |= BTN_DOWN;
    if (k[SDL_SCANCODE_Z]) held |= BTN_JUMP;
    if (k[SDL_SCANCODE_X]) held |= BTN_ATTACK;
    if (k[SDL_SCANCODE_C]) held |= BTN_SPECIAL;
    if (k[SDL_SCANCODE_V]) held |= BTN_GUARD;
    if ((k[SDL_SCANCODE_RETURN] && !alt) || k[SDL_SCANCODE_KP_ENTER] || k[SDL_SCANCODE_ESCAPE]) held |= BTN_START;
    if (k[SDL_SCANCODE_A]) held |= BTN_LT;
    if (k[SDL_SCANCODE_S]) held |= BTN_RT;
    for (int i = 0; i < MAX_PADS; i++) {
        SDL_GameController *c = pads[i];
        if (!c) continue;
        int jx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), jy = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
        /* the DC reads |joy| > 64 of 128: half deflection */
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || jx < -16384) held |= BTN_LEFT;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || jx > 16384) held |= BTN_RIGHT;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP) || jy < -16384) held |= BTN_UP;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || jy > 16384) held |= BTN_DOWN;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A)) held |= BTN_JUMP;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B)) held |= BTN_ATTACK;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X)) held |= BTN_SPECIAL;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y)) held |= BTN_GUARD;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START)) held |= BTN_START;
        if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16384 || SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) held |= BTN_LT;
        if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16384 || SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) held |= BTN_RT;
    }
    for (int i = 0; i < nscript; i++)
        if (frame_no >= script[i].a && frame_no < script[i].b) held |= script[i].m;
    in->held = held;
    in->pressed = held & ~prev_held;
    in->released = prev_held & ~held;
    prev_held = held;
}

/* ------------------------------------------------------------- audio */
/* Software mixer standing in for the AICA: sound effects are the converted2 WAVs (AICA 4-bit ADPCM mono, or PCM),
 * decoded once at load; music is the streamed stereo ADPCM (.adp, 32 kHz, byte-interleaved: low nibble right,
 * high nibble left as snd_adpcm_split) decoded while mixing, whole-file loop at (size & ~31) as plat_dc.c.
 * Volumes / pans follow the KOS AICA driver: volume through its logs[] table (TL, 0.375 dB steps), pan as
 * DIPAN (3 dB steps on the other side). */
#define MAX_SFX 512
#define NVOICE 48
typedef struct { int16_t *pcm; int len, rate; } sample_t;
static sample_t sfx[MAX_SFX];
static int nsfx;
typedef struct { int s, on, loop; double pos, step; float gl, gr; uint32_t age; } voice_t;
static voice_t vc[NVOICE];
static uint32_t voice_age;
static SDL_AudioDeviceID adev;
static int out_rate = 48000;
static float tl_gain[256];       /* KOS logs[] -> linear gain */
static const uint8_t kos_logs[256] = {
    255, 127, 111, 102, 95, 90, 86, 82, 79, 77, 74, 72, 70, 68, 66, 65, 63, 62, 61, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 50, 49, 48,
    47, 47, 46, 45, 45, 44, 43, 43, 42, 42, 41, 41, 40, 40, 39, 39, 38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 34, 33, 33, 33, 32, 32,
    31, 31, 31, 30, 30, 30, 29, 29, 29, 28, 28, 28, 27, 27, 27, 27, 26, 26, 26, 25, 25, 25, 25, 24, 24, 24, 24, 23, 23, 23, 23, 22,
    22, 22, 22, 21, 21, 21, 21, 20, 20, 20, 20, 20, 19, 19, 19, 19, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16,
    15, 15, 15, 15, 15, 15, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 10,
    10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6,
    6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static void vol_pan(int vol, int pan, float *gl, float *gr) {
    float g = tl_gain[vol < 0 ? 0 : vol > 255 ? 255 : vol];
    float l = g, r = g;
    if (pan < 0) pan = 0;
    if (pan > 255) pan = 255;
    if (pan < 0x80) { int n = (0x7f - pan) >> 3; r *= n >= 15 ? 0.f : powf(10.f, -3.f * n / 20.f); }
    else if (pan > 0x80) { int n = (pan - 0x80) >> 3; l *= n >= 15 ? 0.f : powf(10.f, -3.f * n / 20.f); }
    *gl = l; *gr = r;
}

/* AICA ADPCM (tools/audio/adpcm_check.py; Flycast DecodeADPCM): no leak, low nibble first */
static const int adpcm_qs[8] = { 0xe6, 0xe6, 0xe6, 0xe6, 0x133, 0x199, 0x200, 0x266 };
typedef struct { int prev, q; } adpcm_t;
static inline int adpcm_step(adpcm_t *st, int nib) {
    int rv = (st->q * (1 + 2 * (nib & 7))) >> 3;
    if (rv > 0x7fff) rv = 0x7fff;
    int p = st->prev + ((nib & 8) ? -rv : rv);
    st->prev = p < -32768 ? -32768 : p > 32767 ? 32767 : p;
    int q = (st->q * adpcm_qs[nib & 7]) >> 8;
    st->q = q < 127 ? 127 : q > 24576 ? 24576 : q;
    return st->prev;
}

static int wav_decode(const uint8_t *d, size_t n, sample_t *out) {
    if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return -1;
    int fmt = -1, ch = 1, rate = 22050, bits = 16;
    const uint8_t *data = NULL; uint32_t dlen = 0;
    for (size_t p = 12; p + 8 <= n;) {
        uint32_t cl = d[p + 4] | d[p + 5] << 8 | d[p + 6] << 16 | (uint32_t)d[p + 7] << 24;
        if (!memcmp(d + p, "fmt ", 4) && cl >= 16 && p + 8 + 16 <= n) {
            fmt = d[p + 8] | d[p + 9] << 8; ch = d[p + 10] | d[p + 11] << 8;
            rate = (int)(d[p + 12] | d[p + 13] << 8 | d[p + 14] << 16 | (uint32_t)d[p + 15] << 24);
            bits = d[p + 22] | d[p + 23] << 8;
        } else if (!memcmp(d + p, "data", 4)) {
            data = d + p + 8; dlen = cl; if (p + 8 + dlen > n) dlen = (uint32_t)(n - p - 8);
            break;
        }
        p += 8 + cl + (cl & 1);
    }
    if (!data || fmt < 0 || ch < 1 || rate <= 0) return -1;
    if (fmt == 20 && bits == 4) {                    /* Yamaha / AICA ADPCM (KOS snd_sfx_load: mono; stereo halves) */
        int per = (int)(dlen / ch) * 2;
        out->pcm = malloc(sizeof(int16_t) * (per ? per : 1)); out->len = per; out->rate = rate;
        adpcm_t st = { 0, 127 };
        for (int i = 0; i < per; i++) { int b = data[i >> 1]; out->pcm[i] = (int16_t)adpcm_step(&st, (i & 1) ? b >> 4 : b & 15); }
        return 0;
    }
    if (fmt == 1 && (bits == 16 || bits == 8)) {     /* PCM: channel 0 only (mono samples, as KOS's left) */
        int bps = bits / 8, per = (int)(dlen / (bps * ch));
        out->pcm = malloc(sizeof(int16_t) * (per ? per : 1)); out->len = per; out->rate = rate;
        for (int i = 0; i < per; i++) {
            const uint8_t *s = data + (size_t)i * bps * ch;
            out->pcm[i] = bits == 16 ? (int16_t)(s[0] | s[1] << 8) : (int16_t)((s[0] - 128) << 8);
        }
        return 0;
    }
    return -1;
}

/* music stream state (guarded by the audio device lock) */
static uint8_t *mus_data; static int mus_len, mus_loop_len, mus_pos, mus_adp;
static char mus_name[64];
static int mus_vol = 255, mus_paused, mus_once, mus_serial;
static volatile int mus_ended;
static adpcm_t mus_st[2];
static int mus_cur[2], mus_nxt[2];
static double mus_frac, mus_step;
static float mus_g;

static void mus_fetch(void) {        /* advance one source frame: mus_cur <- mus_nxt, decode the next */
    mus_cur[0] = mus_nxt[0]; mus_cur[1] = mus_nxt[1];
    if (mus_ended) { mus_nxt[0] = mus_nxt[1] = 0; return; }
    if (mus_adp) {
        if (mus_pos >= mus_loop_len) {
            if (mus_once || mus_loop_len <= 0) { mus_ended = 1; mus_nxt[0] = mus_nxt[1] = 0; return; }
            mus_pos = 0; mus_st[0].prev = mus_st[1].prev = 0; mus_st[0].q = mus_st[1].q = 127;   /* sealed tracks end at (0, 127) (D-033) */
        }
        int b = mus_data[mus_pos++];
        mus_nxt[1] = adpcm_step(&mus_st[1], b & 15);     /* snd_adpcm_split: low nibble right, high nibble left */
        mus_nxt[0] = adpcm_step(&mus_st[0], b >> 4);
    } else {                                              /* legacy .pcm: s16le stereo 22050 Hz */
        if (mus_pos + 4 > mus_loop_len) {
            if (mus_once || mus_loop_len < 4) { mus_ended = 1; mus_nxt[0] = mus_nxt[1] = 0; return; }
            mus_pos = 0;
        }
        const uint8_t *s = mus_data + mus_pos; mus_pos += 4;
        mus_nxt[0] = (int16_t)(s[0] | s[1] << 8); mus_nxt[1] = (int16_t)(s[2] | s[3] << 8);
    }
}

static void SDLCALL audio_cb(void *ud, Uint8 *stream, int len) {
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int n = len / 4;
    static float *acc; static int acc_n;
    if (acc_n < n * 2) { free(acc); acc = malloc(sizeof(float) * n * 2); acc_n = n * 2; }
    memset(acc, 0, sizeof(float) * n * 2);
    for (int v = 0; v < NVOICE; v++) {
        voice_t *c = &vc[v];
        if (!c->on) continue;
        const sample_t *s = &sfx[c->s];
        if (!s->pcm || s->len <= 0) { c->on = 0; continue; }
        for (int i = 0; i < n; i++) {
            int p = (int)c->pos;
            if (p >= s->len) {
                if (c->loop) { c->pos -= s->len; p = (int)c->pos; if (p >= s->len) { c->pos = 0; p = 0; } }
                else { c->on = 0; break; }
            }
            float f = (float)(c->pos - p);
            int a = s->pcm[p], b = p + 1 < s->len ? s->pcm[p + 1] : (c->loop ? s->pcm[0] : 0);
            float x = a + (b - a) * f;
            acc[i * 2] += x * c->gl; acc[i * 2 + 1] += x * c->gr;
            c->pos += c->step;
        }
    }
    if (mus_data && !mus_paused) {
        for (int i = 0; i < n; i++) {
            if (mus_ended) break;
            float f = (float)mus_frac;
            acc[i * 2] += (mus_cur[0] + (mus_nxt[0] - mus_cur[0]) * f) * mus_g;
            acc[i * 2 + 1] += (mus_cur[1] + (mus_nxt[1] - mus_cur[1]) * f) * mus_g;
            mus_frac += mus_step;
            while (mus_frac >= 1.0) { mus_frac -= 1.0; mus_fetch(); }
        }
    }
    for (int i = 0; i < n * 2; i++) {
        float x = acc[i] * 0.7f;
        if (x > 24000.f) x = 24000.f + (x - 24000.f) / (1.f + (x - 24000.f) / 8767.f);       /* soft knee, then clamp */
        else if (x < -24000.f) x = -24000.f + (x + 24000.f) / (1.f + (-24000.f - x) / 8768.f);
        out[i] = (int16_t)(x > 32767.f ? 32767 : x < -32768.f ? -32768 : x);
    }
}

static void alock(void) { if (adev) SDL_LockAudioDevice(adev); }
static void aunlock(void) { if (adev) SDL_UnlockAudioDevice(adev); }

void snd_init_fp(void) {
    for (int i = 0; i < 256; i++) tl_gain[i] = i == 0 ? 0.f : powf(10.f, -0.375f * kos_logs[i] / 20.f);
    if (opt_noaudio) return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) { plat_log("audio: init failed: %s\n", SDL_GetError()); return; }
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 48000; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
    adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!adev && !SDL_getenv("SDL_AUDIODRIVER")) {      /* e.g. WASAPI without a default endpoint: try the other back ends */
        char first[32]; snprintf(first, sizeof first, "%s", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?");
        plat_log("audio: %s: %s\n", first, SDL_GetError());
        for (int i = 0; i < SDL_GetNumAudioDrivers() && !adev; i++) {
            const char *dn = SDL_GetAudioDriver(i);
            if (!strcmp(dn, "disk") || !strcmp(dn, first)) continue;
            SDL_AudioQuit();
            if (SDL_AudioInit(dn) == 0) adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
            if (!adev) plat_log("audio: %s: %s\n", dn, SDL_GetError());
        }
    }
    if (!adev) { plat_log("audio: open failed: %s (no sound)\n", SDL_GetError()); return; }
    out_rate = have.freq;
    plat_log("audio: driver %s, %d Hz, %d-sample buffer\n", SDL_GetCurrentAudioDriver(), have.freq, have.samples);
    SDL_PauseAudioDevice(adev, 0);
}

int snd_load(const char *name) {
    int slot = nsfx;
    for (int i = 0; i < nsfx; i++) if (!sfx[i].pcm) { slot = i; break; }
    if (slot >= MAX_SFX) return -1;
    size_t n; uint8_t *d = plat_load_file(name, &n);
    if (!d) return -1;
    sample_t s;
    uint64_t t0 = plat_time_us();
    int r = wav_decode(d, n, &s);
    LP.snd_us += plat_time_us() - t0; LP.nsnd++;
    free(d);
    if (r < 0) { plat_log("sfx load failed %s\n", name); return -1; }
    alock(); sfx[slot] = s; aunlock();
    if (slot == nsfx) nsfx++;
    return slot;
}

void snd_unload(int h) {
    if (h < 0 || h >= nsfx || !sfx[h].pcm) return;
    alock();
    for (int v = 0; v < NVOICE; v++) if (vc[v].on && vc[v].s == h) vc[v].on = 0;
    free(sfx[h].pcm); sfx[h].pcm = NULL; sfx[h].len = 0;
    aunlock();
}

int snd_mem_free_kb(void) { return -1; }

static int voice_alloc(void) {
    int best = -1; uint32_t oldest = 0xffffffffu;
    for (int v = 0; v < NVOICE; v++) if (!vc[v].on) return v;
    for (int v = 0; v < NVOICE; v++) if (!vc[v].loop && vc[v].age < oldest) { oldest = vc[v].age; best = v; }
    return best;
}

void snd_play(int h, int vol, int pan) {
    if (!adev || h < 0 || h >= nsfx || !sfx[h].pcm) return;
    alock();
    int v = voice_alloc();
    if (v >= 0) {
        voice_t *c = &vc[v];
        c->s = h; c->on = 1; c->loop = 0; c->pos = 0; c->step = (double)sfx[h].rate / out_rate; c->age = ++voice_age;
        vol_pan(vol, pan, &c->gl, &c->gr);
    }
    aunlock();
}

int snd_loop_start(int h, int vol, int freq) {
    if (!adev || h < 0 || h >= nsfx || !sfx[h].pcm) return -1;
    alock();
    int v = -1;
    for (int i = 0; i < NVOICE; i++) if (!vc[i].on) { v = i; break; }
    if (v >= 0) {
        voice_t *c = &vc[v];
        c->s = h; c->on = 1; c->loop = 1; c->pos = 0; c->step = (double)(freq > 0 ? freq : sfx[h].rate) / out_rate; c->age = ++voice_age;
        vol_pan(vol, 128, &c->gl, &c->gr);
    }
    aunlock();
    return v;
}
void snd_loop_set(int ch, int vol, int freq) {
    if (ch < 0 || ch >= NVOICE) return;
    alock();
    if (vc[ch].on && vc[ch].loop) { vol_pan(vol, 128, &vc[ch].gl, &vc[ch].gr); if (freq > 0) vc[ch].step = (double)freq / out_rate; }
    aunlock();
}
void snd_loop_end(int ch) {
    if (ch < 0 || ch >= NVOICE) return;
    alock(); vc[ch].on = 0; vc[ch].loop = 0; aunlock();
}
void snd_stop_all(void) {        /* every sound effect channel (KOS snd_sfx_stop_all: the looping ones too) */
    alock();
    for (int v = 0; v < NVOICE; v++) { vc[v].on = 0; vc[v].loop = 0; }
    aunlock();
}

static void music_stop_locked(void) {
    free(mus_data); mus_data = NULL; mus_len = mus_loop_len = mus_pos = 0;
    mus_paused = 0; mus_once = 0; mus_ended = 0; mus_name[0] = 0;
}

int music_play(const char *name) {
    alock();
    int same = mus_data && !strcmp(mus_name, name) && !mus_ended;
    aunlock();
    if (same) return 0;                                          /* no restart / no duplicate */
    char path[2048]; snprintf(path, sizeof path, "%s/%s", data_dir, name);
    size_t n;
    uint8_t *d = read_whole(path, &n, 0);           /* whole track in memory (<= 6 MB; game.c times it as LP.music_us) */
    alock();
    music_stop_locked();
    if (!d) { aunlock(); plat_log("music missing %s\n", path); return -1; }
    mus_data = d; mus_len = (int)n; mus_loop_len = (int)(n & ~(size_t)31);
    mus_adp = strstr(name, ".adp") != NULL;
    mus_step = (mus_adp ? 32000.0 : 22050.0) / out_rate;
    mus_st[0].prev = mus_st[1].prev = 0; mus_st[0].q = mus_st[1].q = 127;
    mus_cur[0] = mus_cur[1] = mus_nxt[0] = mus_nxt[1] = 0; mus_frac = 0;
    mus_fetch(); mus_fetch();
    mus_g = tl_gain[mus_vol];
    snprintf(mus_name, sizeof mus_name, "%s", name); mus_serial++;
    aunlock();
    return 0;
}

int music_play_once(const char *name) {      /* gallery music cards of type 1: no loop, silence after the end */
    music_stop();
    int r = music_play(name);
    alock(); if (r == 0) mus_once = 1; aunlock();
    return r;
}
int music_serial(void) { return mus_serial; }
int music_active(void) { return mus_data && !mus_ended && !mus_paused; }
void music_volume(int pct) {
    alock();
    mus_vol = pct * 255 / 100; if (mus_vol < 0) mus_vol = 0; if (mus_vol > 255) mus_vol = 255;
    mus_g = tl_gain[mus_vol];
    aunlock();
}
void music_pause(void) { alock(); if (mus_data) mus_paused = 1; aunlock(); }
void music_resume(void) { alock(); mus_paused = 0; aunlock(); }
void music_update(void) {}
void music_stop(void) { alock(); music_stop_locked(); aunlock(); }

/* ------------------------------------------------------------- VMU screen and saves */
static uint8_t vmu_bm[192]; static int vmu_have;
int plat_vmu_lcd(const uint8_t bm[192]) { memcpy(vmu_bm, bm, 192); vmu_have = 1; return 0; }

int plat_save_write(const void *data, size_t size) {
    if (!save_path[0]) return -1;
    char tmp[2048]; snprintf(tmp, sizeof tmp, "%s.tmp", save_path);
    SDL_RWops *f = SDL_RWFromFile(tmp, "wb");
    if (!f) { plat_log("save: cannot write %s\n", tmp); return -1; }
    size_t n = SDL_RWwrite(f, data, 1, size);
    if (SDL_RWclose(f) != 0 || n != size) { remove(tmp); return -1; }
#ifdef _WIN32
    remove(save_path);                           /* rename() does not replace on Windows */
#endif
    if (rename(tmp, save_path) != 0) { plat_log("save: cannot replace %s\n", save_path); return -1; }
    return 0;
}
int plat_save_read(void *data, size_t max) {
    if (!save_path[0]) return -1;
    SDL_RWops *f = SDL_RWFromFile(save_path, "rb");
    if (!f) return -1;
    size_t n = SDL_RWread(f, data, 1, max);
    SDL_RWclose(f);
    return (int)n;
}

void plat_quit(void) { quit_req = 1; }

/* ------------------------------------------------------------- video */
static uint32_t present_us_acc, present_us_max;
static const uint32_t *last_fb;

static void dest_rect(int ww, int wh, SDL_Rect *r) {
    double aspect = r_letterbox_mode == 0 ? 4.0 / 3.0 : (double)VIEW_W / VIEW_H;   /* SCREEN SIZE off: the DC's 4:3 fill */
    if (scale_mode == 2) { r->x = r->y = 0; r->w = ww; r->h = wh; return; }
    if (scale_mode == 1 && r_letterbox_mode != 0) {
        int k = ww / VIEW_W < wh / VIEW_H ? ww / VIEW_W : wh / VIEW_H;
        if (k >= 1) { r->w = VIEW_W * k; r->h = VIEW_H * k; r->x = (ww - r->w) / 2; r->y = (wh - r->h) / 2; return; }
    }
    if (scale_mode == 1) {                       /* integer: whole multiples of the 4:3 frame's height */
        int k = wh / VIEW_H; while (k > 1 && (int)(VIEW_H * k * aspect) > ww) k--;
        if (k < 1) k = 1;
        r->h = VIEW_H * k; r->w = (int)(r->h * aspect + 0.5); r->x = (ww - r->w) / 2; r->y = (wh - r->h) / 2; return;
    }
    if ((double)ww / wh > aspect) { r->h = wh; r->w = (int)(wh * aspect + 0.5); }
    else { r->w = ww; r->h = (int)(ww / aspect + 0.5); }
    r->x = (ww - r->w) / 2; r->y = (wh - r->h) / 2;
}

static void write_bmp(const char *path, const uint32_t *fb) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom((void *)fb, VIEW_W, VIEW_H, 32, VIEW_W * 4, SDL_PIXELFORMAT_RGB888);
    if (!s) return;
    if (SDL_SaveBMP(s, path) != 0) plat_log("screenshot %s: %s\n", path, SDL_GetError());
    SDL_FreeSurface(s);
}

static void save_screenshot(void) {
    if (!last_fb) return;
    char p[2048];
    for (int i = 0; i < 10000; i++) {
        snprintf(p, sizeof p, "%sshot%04d.bmp", pref_dir, i);
        SDL_RWops *f = SDL_RWFromFile(p, "rb");
        if (f) { SDL_RWclose(f); continue; }
        write_bmp(p, last_fb); plat_log("screenshot %s\n", p);
        return;
    }
}

static void draw_vmu(int ww, int wh) {
    if (!show_vmu || !vmu_have) return;
    int k = wh >= 720 ? 4 : wh >= 480 ? 3 : 2, x0 = ww - 48 * k - 8, y0 = 8;
    SDL_Rect r = { x0 - k, y0 - k, 48 * k + 2 * k, 32 * k + 2 * k };
    SDL_SetRenderDrawColor(ren, 20, 20, 30, 255); SDL_RenderFillRect(ren, &r);
    r.x = x0; r.y = y0; r.w = 48 * k; r.h = 32 * k;
    SDL_SetRenderDrawColor(ren, 150, 190, 200, 255); SDL_RenderFillRect(ren, &r);
    SDL_SetRenderDrawColor(ren, 20, 30, 60, 255);
    for (int y = 0; y < 32; y++) for (int x = 0; x < 48; x++)
        if (vmu_bm[y * 6 + x / 8] & (0x80 >> (x & 7))) { SDL_Rect p = { x0 + x * k, y0 + y * k, k, k }; SDL_RenderFillRect(ren, &p); }
}

/* render_soft.c r_end(): one finished 426x240 frame (0x00RRGGBB) */
void pc_present(const uint32_t *fb, int w, int h) {
    (void)w; (void)h;
    uint64_t t0 = plat_time_us();
    last_fb = fb;
    if (shot_dir && frame_no > 0 && (frame_no - 1) % shot_every == 0 && !loading_active()) {   /* named by game frame, as the host's PPM dumps */
        char p[2048]; snprintf(p, sizeof p, "%s/f%05d.bmp", shot_dir, frame_no - 1); write_bmp(p, fb);
    }
    if (shot_req) { shot_req = 0; save_screenshot(); }
    if (loading_active()) pump_events();           /* loading screens draw from inside file reads */
    SDL_UpdateTexture(tex_view, NULL, fb, VIEW_W * 4);
    int ww, wh; SDL_GetRendererOutputSize(ren, &ww, &wh);
    SDL_Rect dr; dest_rect(ww, wh, &dr);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    /* sharp scaling: nearest-neighbour to the next integer multiple, then a filtered scale to the window
     * (even pixel sizes at any window size); integer mode and exact multiples draw nearest directly */
    int k = (dr.w + VIEW_W - 1) / VIEW_W; if ((dr.h + VIEW_H - 1) / VIEW_H > k) k = (dr.h + VIEW_H - 1) / VIEW_H;
    int exact = dr.w % VIEW_W == 0 && dr.h % VIEW_H == 0 && dr.w / VIEW_W == dr.h / VIEW_H;
    if (k > 8) k = 8;
    if (!opt_smooth && !exact && k >= 2) {
        if (up_k != k) {
            if (tex_up) SDL_DestroyTexture(tex_up);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
            tex_up = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, VIEW_W * k, VIEW_H * k);
            up_k = tex_up ? k : -1;
        }
    }
    SDL_SetTextureScaleMode(tex_view, opt_smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    if (!opt_smooth && !exact && k >= 2 && tex_up && up_k == k && SDL_SetRenderTarget(ren, tex_up) == 0) {
        SDL_RenderCopy(ren, tex_view, NULL, NULL);
        SDL_SetRenderTarget(ren, NULL);
        SDL_RenderCopy(ren, tex_up, NULL, &dr);
    } else {
        SDL_RenderCopy(ren, tex_view, NULL, &dr);
    }
    draw_vmu(ww, wh);
    SDL_RenderPresent(ren);
    uint32_t d = (uint32_t)(plat_time_us() - t0);
    present_us_acc += d; if (d > present_us_max) present_us_max = d;
}

/* ------------------------------------------------------------- main */
static int file_exists(const char *p) { SDL_RWops *f = SDL_RWFromFile(p, "rb"); if (f) SDL_RWclose(f); return f != NULL; }
static int data_ok(const char *dir) { char p[2048]; snprintf(p, sizeof p, "%s/levels.txt", dir); return file_exists(p); }

static void fatal(const char *msg) {
    plat_log("%s\n", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Freedom Planet DC (PC)", msg, win);
}

int main(int argc, char **argv) {
    const char *arg_data = NULL, *arg_script = NULL, *arg_save = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *nx = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--fullscreen")) opt_fullscreen = 1;
        else if (!strcmp(a, "--window") && nx) { opt_window = atoi(nx); i++; }
        else if (!strcmp(a, "--scale") && nx) { scale_mode = !strcmp(nx, "integer") ? 1 : !strcmp(nx, "stretch") ? 2 : 0; i++; }
        else if (!strcmp(a, "--smooth")) opt_smooth = 1;
        else if (!strcmp(a, "--no-vsync")) opt_novsync = 1;
        else if (!strcmp(a, "--no-audio")) opt_noaudio = 1;
        else if (!strcmp(a, "--fast")) { opt_fast = 1; opt_novsync = 1; }
        else if (!strcmp(a, "--level") && nx) { start_level = nx; i++; }
        else if (!strcmp(a, "--char") && nx) { demo_char = atoi(nx); i++; }
        else if (!strcmp(a, "--script") && nx) { arg_script = nx; i++; }
        else if (!strcmp(a, "--frames") && nx) { opt_frames = atoi(nx); i++; }
        else if (!strcmp(a, "--shot-dir") && nx) { shot_dir = nx; i++; }
        else if (!strcmp(a, "--shot-every") && nx) { shot_every = atoi(nx) > 0 ? atoi(nx) : 1; i++; }
        else if (!strcmp(a, "--host-draw")) opt_hostdraw = 1;
        else if (!strcmp(a, "--save") && nx) { arg_save = nx; i++; }
        else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            printf("usage: %s [data_dir] [--fullscreen] [--window N] [--scale aspect|integer|stretch] [--smooth] [--no-vsync]\n"
                   "       [--no-audio] [--save file] [--level dir [--char n]] [--script file] [--frames N] [--fast]\n"
                   "       [--shot-dir dir [--shot-every N] [--host-draw]]\n", argv[0]);
            return 0;
        }
        else if (a[0] != '-' && !arg_data) arg_data = a;
        else { fprintf(stderr, "unknown option %s (--help)\n", a); return 1; }
    }
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    char *base = SDL_GetBasePath();
    char *pref = SDL_GetPrefPath("fpdc", "FreedomPlanetDC");
    snprintf(pref_dir, sizeof pref_dir, "%s", pref ? pref : (base ? base : ""));
    { char lp[2048]; snprintf(lp, sizeof lp, "%sfpdc.log", pref_dir); log_f = fopen(lp, "w"); }
    /* data: the argument, else data/ next to the executable, else ./data */
    if (arg_data) snprintf(data_dir, sizeof data_dir, "%s", arg_data);
    else {
        char p[1100]; snprintf(p, sizeof p, "%sdata", base ? base : "");
        snprintf(data_dir, sizeof data_dir, "%s", data_ok(p) ? p : "data");
    }
    { size_t L = strlen(data_dir); while (L > 1 && (data_dir[L - 1] == '/' || data_dir[L - 1] == '\\')) data_dir[--L] = 0; }
    /* save: --save, else fpdc.sav next to the executable when it is already there (portable install), else the user dir */
    if (arg_save) snprintf(save_path, sizeof save_path, "%s", arg_save);
    else {
        char p[1100]; snprintf(p, sizeof p, "%sfpdc.sav", base ? base : "");
        if (base && file_exists(p)) snprintf(save_path, sizeof save_path, "%s", p);
        else snprintf(save_path, sizeof save_path, "%sfpdc.sav", pref_dir);
    }
    plat_log("fpdc PC: data %s, save %s\n", data_dir, save_path);
    SDL_free(base); SDL_free(pref);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    win = SDL_CreateWindow("Freedom Planet DC", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, VIEW_W * opt_window, VIEW_H * opt_window,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (opt_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE | (opt_novsync ? 0 : SDL_RENDERER_PRESENTVSYNC));
    if (!ren) ren = SDL_CreateRenderer(win, -1, opt_novsync ? 0 : SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }
    { SDL_RendererInfo ri; SDL_GetRendererInfo(ren, &ri); plat_log("video: %s renderer%s\n", ri.name, (ri.flags & SDL_RENDERER_PRESENTVSYNC) ? ", vsync" : ""); }
    tex_view = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, VIEW_W, VIEW_H);
    if (!tex_view) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return 1; }
    if (opt_fullscreen) SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < SDL_NumJoysticks(); i++) pad_open(i);
    if (arg_script) script_load(arg_script);

    if (!data_ok(data_dir)) {
        char m[2048]; snprintf(m, sizeof m, "Game data not found in \"%s\" (levels.txt missing).\n\nPut the converted data (the converted2 folder) in a folder named \"data\" next to the program, or pass its path as the first argument.", data_dir);
        fatal(m); return 2;
    }
    plat_init();
    r_init();
    snd_init_fp();
    if (game_init() < 0) { fatal("Game initialisation failed: data incomplete (see fpdc.log)."); return 2; }

    /* fixed 60 Hz game step; the frame is drawn after each step (vsync or a short sleep paces the loop) */
    const uint64_t step_us = 1000000 / FPS;          /* 16666 us (+ remainder handled below) */
    uint64_t t_last = plat_time_us(), acc = 0, sub = 0;
    uint64_t upd_acc = 0, drw_acc = 0; uint32_t upd_max = 0, drw_max = 0, nfr = 0;
    input_t in;
    while (!quit_req) {
        int steps = 0;
        if (opt_fast) steps = 1;
        else {
            uint64_t now = plat_time_us();
            acc += now - t_last; t_last = now;
            if (acc > step_us * 5) acc = step_us * 5;      /* after a stall: drop time, no fast-forward burst */
            while (acc >= step_us + (sub >= 2 ? 1 : 0)) {  /* 1000000 / 60 = 16666.67: every third step is 1 us longer */
                acc -= step_us + (sub >= 2 ? 1 : 0); sub = (sub + 1) % 3; steps++;
            }
            if (!steps) {
                uint64_t left = step_us - acc;
                if (left > 2000) SDL_Delay((Uint32)(left / 1000 - 1));
                else pump_events();
                continue;
            }
            if (steps > 3) steps = 3;
        }
        for (int s = 0; s < steps && !quit_req; s++) {
            uint64_t t0 = plat_time_us();
            plat_poll_input(&in);
            game_update(&in);
            music_update();
            uint32_t d = (uint32_t)(plat_time_us() - t0); upd_acc += d; if (d > upd_max) upd_max = d;
            frame_no++;
            if (opt_frames >= 0 && frame_no >= opt_frames) quit_req = 1;
        }
        if (opt_hostdraw && (frame_no - 1) % shot_every != 0 && !quit_req) { pump_events(); continue; }   /* as the host: draw only dumped frames */
        uint64_t t1 = plat_time_us();
        uint32_t pa = present_us_acc;
        game_draw();                                    /* r_end -> pc_present */
        uint32_t d = (uint32_t)(plat_time_us() - t1 - (present_us_acc - pa)); drw_acc += d; if (d > drw_max) drw_max = d;
        nfr++;
        if (nfr % 600 == 0 || quit_req)
            plat_log("perf: frame %d upd avg %lu max %lu us, soft draw avg %lu max %lu us, present avg %lu max %lu us\n", frame_no,
                     (unsigned long)(upd_acc / (frame_no ? frame_no : 1)), (unsigned long)upd_max, (unsigned long)(drw_acc / nfr), (unsigned long)drw_max,
                     (unsigned long)(present_us_acc / nfr), (unsigned long)present_us_max);
    }
    music_stop();
    if (adev) SDL_CloseAudioDevice(adev);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    SDL_Quit();
    if (log_f) fclose(log_f);
    return 0;
}
