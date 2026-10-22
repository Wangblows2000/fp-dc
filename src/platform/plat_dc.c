/* Dreamcast platform layer (KallistiOS): boot, controller, files, audio. */
#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dc/sound/stream.h>
#include <dc/sound/sfxmgr.h>
#include "../fp.h"

#ifndef FP_DATA_ROOT
#define FP_DATA_ROOT "/cd/data"
#endif

static uint32_t prev_held;

void plat_init(void) {}

void plat_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}

uint64_t plat_time_us(void) { return timer_us_gettime64(); }

void *plat_load_file(const char *name, size_t *size) {
    char path[128];
    snprintf(path, sizeof path, FP_DATA_ROOT "/%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) { plat_log("missing file %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 4 * 1024 * 1024) { plat_log("bad size %s %ld\n", path, n); fclose(f); return NULL; }
    void *d = malloc(n);
    if (!d) { fclose(f); return NULL; }
    if (fread(d, 1, n, f) != (size_t)n) { free(d); fclose(f); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return d;
}

void plat_poll_input(input_t *in) {
    uint32_t held = 0;
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (dev) {
        cont_state_t *st = (cont_state_t *)maple_dev_status(dev);
        if (st) {
            uint32_t b = st->buttons;
            if ((b & CONT_DPAD_LEFT) || st->joyx < -64) held |= BTN_LEFT;
            if ((b & CONT_DPAD_RIGHT) || st->joyx > 64) held |= BTN_RIGHT;
            if ((b & CONT_DPAD_UP) || st->joyy < -64) held |= BTN_UP;
            if ((b & CONT_DPAD_DOWN) || st->joyy > 64) held |= BTN_DOWN;
            if (b & CONT_A) held |= BTN_JUMP;
            if (b & CONT_X) held |= BTN_ATTACK;
            if (b & CONT_B) held |= BTN_SPECIAL;
            if (b & CONT_Y) held |= BTN_GUARD;
            if (b & CONT_START) held |= BTN_START;
            if (st->ltrig > 128) held |= BTN_LT;
            if (st->rtrig > 128) held |= BTN_RT;
        }
    }
    in->held = held;
    in->pressed = held & ~prev_held;
    in->released = prev_held & ~held;
    prev_held = held;
}

/* ------------------------------------------------------------- audio */
#define MAX_SFX 96
static sfxhnd_t sfx[MAX_SFX];
static int nsfx;

void snd_init_fp(void) {
    snd_stream_init();
}

int snd_load(const char *name) {
    char path[128];
    if (nsfx >= MAX_SFX) return -1;
    snprintf(path, sizeof path, FP_DATA_ROOT "/%s", name);
    sfxhnd_t h = snd_sfx_load(path);
    if (h == SFXHND_INVALID) { plat_log("sfx load failed %s\n", path); return -1; }
    sfx[nsfx] = h;
    return nsfx++;
}

void snd_play(int h, int vol, int pan) {
    if (h >= 0 && h < nsfx) snd_sfx_play(sfx[h], vol, pan);
}

/* Music (D-013): AICA 4-bit ADPCM, stereo, 32 kHz (.adp, byte-interleaved; hardware-decoded so the
 * SH4 only reads the disc) or legacy raw s16le stereo 22050 Hz (.pcm). Every track loops whole-file
 * (the original's ChowdrenAudio has no loop points). Streaming runs on its own thread. */
#define MUS_CHUNK 16384
static snd_stream_hnd_t mus = SND_STREAM_INVALID;
static FILE *mus_f;
static char mus_name[64];
static int mus_vol = 255;
static uint8_t mus_buf[MUS_CHUNK * 2] __attribute__((aligned(32)));

static void *mus_cb(snd_stream_hnd_t hnd, int smp_req, int *smp_recv) {
    /* KOS passes the request in BYTES (snd_stream.c: get_data(hnd, needed_bytes, &got_bytes)) */
    (void)hnd;
    int want = smp_req > (int)sizeof mus_buf ? (int)sizeof mus_buf : smp_req;
    int got = 0, rewinds = 0;
    while (got < want && rewinds < 2) {
        size_t n = fread(mus_buf + got, 1, want - got, mus_f);
        if (n == 0) { fseek(mus_f, 0, SEEK_SET); rewinds++; continue; }   /* whole-file loop */
        got += (int)n;
    }
    *smp_recv = got & ~3;
    return mus_buf;
}

/* snd_stream_poll() calls mus_cb(), which reads the disc; doing that in the game loop caused
 * ~130 ms frame spikes in Flycast, so it runs here. */
static volatile int mus_thread_run;
static kthread_t *mus_thd;
static mutex_t mus_lock = MUTEX_INITIALIZER;
static void *mus_thread(void *arg) {
    (void)arg;
    while (mus_thread_run) {
        mutex_lock(&mus_lock);
        if (mus != SND_STREAM_INVALID) snd_stream_poll(mus);
        mutex_unlock(&mus_lock);
        thd_sleep(10);
    }
    return NULL;
}

static void music_stop_locked(void) {
    if (mus != SND_STREAM_INVALID) { snd_stream_stop(mus); snd_stream_destroy(mus); mus = SND_STREAM_INVALID; }
    if (mus_f) { fclose(mus_f); mus_f = NULL; }
    mus_name[0] = 0;
}

int music_play(const char *name) {
    char path[128];
    mutex_lock(&mus_lock);
    if (mus != SND_STREAM_INVALID && !strcmp(mus_name, name)) { mutex_unlock(&mus_lock); return 0; }  /* no restart / no duplicate */
    music_stop_locked();
    snprintf(path, sizeof path, FP_DATA_ROOT "/%s", name);
    mus_f = fopen(path, "rb");
    if (!mus_f) { mutex_unlock(&mus_lock); plat_log("music missing %s\n", path); return -1; }
    int adpcm = strstr(name, ".adp") != NULL;
    mus = snd_stream_alloc(mus_cb, adpcm ? SND_STREAM_BUFFER_MAX_ADPCM : MUS_CHUNK);
    if (mus == SND_STREAM_INVALID) { fclose(mus_f); mus_f = NULL; mutex_unlock(&mus_lock); return -1; }
    snd_stream_volume(mus, mus_vol);
    if (adpcm) snd_stream_start_adpcm(mus, 32000, 1);
    else snd_stream_start(mus, 22050, 1);
    snprintf(mus_name, sizeof mus_name, "%s", name);
    mutex_unlock(&mus_lock);
    if (!mus_thd) { mus_thread_run = 1; mus_thd = thd_create(0, mus_thread, NULL); }
    return 0;
}

void music_volume(int pct) {
    mutex_lock(&mus_lock);
    mus_vol = pct * 255 / 100;
    if (mus != SND_STREAM_INVALID) snd_stream_volume(mus, mus_vol);
    mutex_unlock(&mus_lock);
}

void music_update(void) { /* handled by mus_thread */ }

void music_stop(void) {
    mutex_lock(&mus_lock);
    music_stop_locked();
    mutex_unlock(&mus_lock);
}

/* QUIT on the title menu: return to the Dreamcast BIOS menu (the console equivalent of quitting) */
void plat_quit(void) { music_stop(); arch_menu(); }

/* ------------------------------------------------------------- main */
extern uint32_t r_stat_polys;

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    plat_init();
    if (r_init() < 0) { printf("pvr init failed\n"); return 1; }
    snd_init_fp();
    if (game_init() < 0) {
        printf("game init failed: data missing under %s\n", FP_DATA_ROOT);
        for (;;) { r_begin(); text_draw(20, 100, "DATA MISSING", 0xffff4040, 3); r_end(); }
    }
    input_t in;
    uint64_t t_prev = timer_us_gettime64();
    uint32_t frames = 0, slow = 0;
    for (;;) {
        uint64_t t0 = timer_us_gettime64();
        plat_poll_input(&in);
        game_update(&in);
        music_update();
        uint64_t t1 = timer_us_gettime64();
        game_draw();       /* r_begin() waits for the PVR -> paces to vblank */
        uint64_t t2 = timer_us_gettime64();
        G.prof_update_us = (uint32_t)(t1 - t0);
        G.prof_draw_us = (uint32_t)(t2 - t1);
        if (t0 - t_prev > 17500) slow++;
        t_prev = t0;
        if (++frames % 600 == 0)
            printf("perf: frame %lu upd %lu us draw %lu us polys %lu slow %lu\n",
                   (unsigned long)frames, (unsigned long)G.prof_update_us,
                   (unsigned long)G.prof_draw_us, (unsigned long)r_stat_polys, (unsigned long)slow);
    }
    return 0;
}
