/* Dreamcast platform layer (KallistiOS): boot, controller, files, audio. */
#include <kos.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <dc/sound/stream.h>
#include <dc/sound/sfxmgr.h>
#include <dc/vmu_pkg.h>
#include <dc/vmufs.h>
#include <dc/maple/vmu.h>
#include "../fp.h"

#ifndef FP_DATA_ROOT
#define FP_DATA_ROOT "/cd/data"
#endif

static uint32_t prev_held;
/* demo playback (verification builds): a "demo.txt" input script on the disc (same format as the host
 * scripts: "<first frame> <end frame> <buttons LRUDJXBS>") replaces the pad; positions go to the serial log
 * every 60 frames so a run can be compared with the host replay of the same script. */
static struct { int a, b; uint32_t m; } *demo; static int ndemo, demo_frame, demo_on;
static char demo_level[8];
int demo_letterbox = -1;
const char *plat_start_level(void) { return demo_level[0] ? demo_level : NULL; }
static void demo_load(void) {
    size_t sz; char *t = plat_read_raw("demo.txt", &sz);
    if (!t) return;
    demo = malloc(sizeof *demo * 8192);
    char *p = t, *end = t + sz;
    while (p < end && ndemo < 8192) {
        int a, b, n = 0; char btn[16];
        char line[64]; size_t k = 0; while (p < end && *p != '\n' && k < sizeof line - 1) line[k++] = *p++; line[k] = 0; if (p < end) p++;
        if (!strncmp(line, "LETTERBOX ", 10)) { demo_letterbox = line[10] - '0'; continue; }   /* verification: presentation mode */
        if (!strncmp(line, "LEVEL ", 6)) { size_t L = strlen(line + 6); if (L > 0 && L < sizeof demo_level) memcpy(demo_level, line + 6, L + 1); continue; }
        if (sscanf(line, "%d %d %15s%n", &a, &b, btn, &n) == 3) {
            uint32_t m = 0;
            for (char *c = btn; *c; c++) switch (*c) {
                case 'L': m |= BTN_LEFT; break; case 'R': m |= BTN_RIGHT; break; case 'U': m |= BTN_UP; break; case 'D': m |= BTN_DOWN; break;
                case 'J': m |= BTN_JUMP; break; case 'X': m |= BTN_ATTACK; break; case 'B': m |= BTN_SPECIAL; break; case 'S': m |= BTN_START; break; }
            demo[ndemo].a = a; demo[ndemo].b = b; demo[ndemo].m = m; ndemo++;
        }
    }
    free(t); demo_on = 1;
    printf("demo: %d script lines\n", ndemo);
}

void plat_init(void) {}

void plat_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}

uint64_t plat_time_us(void) { return timer_us_gettime64(); }

/* Whole-file read through the KOS VFS into a 32-byte aligned buffer. fs_iso9660 only takes its
 * multi-sector DMA path (cdrom_stream / cdrom_read_sectors_ex) when the destination is 32-byte
 * aligned and the file position is on a sector boundary; with newlib malloc (8-byte aligned) +
 * stdio every 2 KB sector was a separate drive command through the sector cache (measured in
 * Flycast: 120-160 KB/s, DV1 load 41 s). 64 KB chunks keep both conditions and let the loading
 * screen redraw between them. The buffer is free()-able (newlib memalign). */
void *plat_alloc_aligned(size_t n) { return memalign(32, (n + 31) & ~(size_t)31); }

void *plat_load_file(const char *name, size_t *size) {
    int found;
    void *d = pack_load(name, size, &found);
    if (d || found) return d;
    return plat_read_raw(name, size);
}

void *plat_read_raw(const char *name, size_t *size) {
    char path[128];
    snprintf(path, sizeof path, FP_DATA_ROOT "/%s", name);
    uint64_t t0 = plat_time_us();
    file_t f = fs_open(path, O_RDONLY);
    if (f < 0) { plat_log("missing file %s\n", path); return NULL; }
    ssize_t n = (ssize_t)fs_total(f);
    uint64_t t1 = plat_time_us(); LP.open_us += t1 - t0; LP.nfiles++;
    if (n <= 0 || n > 8 * 1024 * 1024) { plat_log("bad size %s %ld\n", path, (long)n); fs_close(f); return NULL; }
    void *d = plat_alloc_aligned((size_t)n);
    if (!d) { plat_log("out of memory reading %s (%ld)\n", path, (long)n); fs_close(f); return NULL; }
    for (ssize_t got = 0; got < n;) {
        ssize_t k = n - got > 65536 ? 65536 : n - got;
        if (fs_read(f, (char *)d + got, (size_t)k) != k) { plat_log("read error %s\n", path); free(d); fs_close(f); return NULL; }
        got += k; uint64_t tr = plat_time_us(); LP.read_us += tr - t1; loading_tick(); t1 = plat_time_us();
    }
    fs_close(f);
    LP.bytes += (uint64_t)n; LP.nalloc++; LP.alloc_bytes += (uint64_t)n;
    *size = (size_t)n;
    return d;
}

void plat_poll_input(input_t *in) {
    uint32_t held = 0;
    maple_device_t *dev = demo_on ? NULL : maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (demo_on) {
        for (int i = 0; i < ndemo; i++) if (demo_frame >= demo[i].a && demo_frame < demo[i].b) held |= demo[i].m;
        if (demo_frame % 60 == 0)
            printf("demo f %d mode %d lv %s x %.2f y %.2f hp %d lives %d\n", demo_frame, G.mode, G.lvdir, G.p.x, G.p.y, G.p.hp, G.p.lives);
        demo_frame++;
    }
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
    /* whole file through the aligned reader, then KOS parses it from memory (snd_sfx_load reads the
     * 44-byte header and then the data from a non-sector-aligned position: one drive command per sector) */
    size_t wsz; char *w = plat_load_file(name, &wsz);
    if (!w) return -1;
    uint64_t ts = plat_time_us();
    sfxhnd_t h = snd_sfx_load_buf(w);
    LP.snd_us += plat_time_us() - ts; LP.nsnd++;
    free(w);
    if (h == SFXHND_INVALID) { plat_log("sfx load failed %s\n", path); return -1; }
    sfx[nsfx] = h;
    return nsfx++;
}

void snd_stop_all(void) { snd_sfx_stop_all(); }

void snd_play(int h, int vol, int pan) {
    if (h >= 0 && h < nsfx) snd_sfx_play(sfx[h], vol, pan);
}

/* Music (D-013): AICA 4-bit ADPCM, stereo, 32 kHz (.adp, byte-interleaved; hardware-decoded so the
 * SH4 only reads the disc) or legacy raw s16le stereo 22050 Hz (.pcm). Every track loops whole-file
 * (the original's ChowdrenAudio has no loop points). Streaming runs on its own thread. */
#define MUS_CHUNK 16384
static snd_stream_hnd_t mus = SND_STREAM_INVALID;
static file_t mus_f = -1;      /* KOS VFS: aligned sector reads take the DMA path (see plat_load_file) */
static char mus_name[64];
static int mus_vol = 255;
static int mus_paused;           /* pause menu: stream stopped in place, not polled */
static uint8_t mus_buf[MUS_CHUNK * 2] __attribute__((aligned(32)));

static void *mus_cb(snd_stream_hnd_t hnd, int smp_req, int *smp_recv) {
    /* KOS passes the request in BYTES (snd_stream.c: get_data(hnd, needed_bytes, &got_bytes)) */
    (void)hnd;
    int want = smp_req > (int)sizeof mus_buf ? (int)sizeof mus_buf : smp_req;
    int got = 0, rewinds = 0;
    while (got < want && rewinds < 2) {
        ssize_t n = fs_read(mus_f, mus_buf + got, (size_t)(want - got));
        if (n <= 0) { fs_seek(mus_f, 0, SEEK_SET); rewinds++; continue; }   /* whole-file loop */
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
        if (mus != SND_STREAM_INVALID && !mus_paused) snd_stream_poll(mus);
        mutex_unlock(&mus_lock);
        thd_sleep(10);
    }
    return NULL;
}

static void music_stop_locked(void) {
    if (mus != SND_STREAM_INVALID) { snd_stream_stop(mus); snd_stream_destroy(mus); mus = SND_STREAM_INVALID; }
    mus_paused = 0;
    if (mus_f >= 0) { fs_close(mus_f); mus_f = -1; }
    mus_name[0] = 0;
}

int music_play(const char *name) {
    char path[128];
    mutex_lock(&mus_lock);
    if (mus != SND_STREAM_INVALID && !strcmp(mus_name, name)) { mutex_unlock(&mus_lock); return 0; }  /* no restart / no duplicate */
    music_stop_locked();
    snprintf(path, sizeof path, FP_DATA_ROOT "/%s", name);
    mus_f = fs_open(path, O_RDONLY);
    if (mus_f < 0) { mutex_unlock(&mus_lock); plat_log("music missing %s\n", path); return -1; }
    int adpcm = strstr(name, ".adp") != NULL;
    mus = snd_stream_alloc(mus_cb, adpcm ? SND_STREAM_BUFFER_MAX_ADPCM : MUS_CHUNK);
    if (mus == SND_STREAM_INVALID) { fs_close(mus_f); mus_f = -1; mutex_unlock(&mus_lock); return -1; }
    if (adpcm) snd_stream_start_adpcm(mus, 32000, 1);
    else snd_stream_start(mus, 22050, 1);
    snd_stream_volume(mus, mus_vol);   /* after the start: snd_stream_start_type sets chan->vol = 255 */
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

/* pause menu: stop the AICA channels but keep the file and its position; resume restarts the stream
 * there (the prefetched ring buffer, up to 32 KB = about 1 s, is skipped) */
void music_pause(void) {
    mutex_lock(&mus_lock);
    if (mus != SND_STREAM_INVALID && !mus_paused) { snd_stream_stop(mus); mus_paused = 1; }
    mutex_unlock(&mus_lock);
}
void music_resume(void) {
    mutex_lock(&mus_lock);
    if (mus != SND_STREAM_INVALID && mus_paused) {
        mus_paused = 0;
        if (strstr(mus_name, ".adp")) snd_stream_start_adpcm(mus, 32000, 1); else snd_stream_start(mus, 22050, 1);
        snd_stream_volume(mus, mus_vol);
    }
    mutex_unlock(&mus_lock);
}

void music_update(void) { /* handled by mus_thread */ }

void music_stop(void) {
    mutex_lock(&mus_lock);
    music_stop_locked();
    mutex_unlock(&mus_lock);
}

/* ------------------------------------------------------------- VMU */
/* LCD: the same bitmap on every VMU that has a screen (two slots per controller) */
int plat_vmu_lcd(const uint8_t bitmap[192]) {
    static uint32_t bm[48];                       /* vmu_draw_lcd_rotated wants 4-byte alignment */
    memcpy(bm, bitmap, 192);
    int ok = -1;
    for (int i = 0; ; i++) {
        maple_device_t *d = maple_enum_type(i, MAPLE_FUNC_LCD);
        if (!d) break;
        if (vmu_draw_lcd_rotated(d, bm) == MAPLE_EOK) ok = 0;
    }
    return ok;
}

/* Save file: one VMU file "FPDC.SAV" (vmu_pkg header + 3-frame 16-colour crystal icon + payload).
 * Read from the first memory card that has it; write to that card, else the first with room. */
#define SAVE_NAME "FPDC.SAV"
static maple_device_t *save_card(int need_blocks) {
    maple_device_t *first_free = NULL;
    for (int i = 0; ; i++) {
        maple_device_t *d = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
        if (!d) break;
        void *buf = NULL; int sz = 0;
        if (vmufs_read(d, SAVE_NAME, &buf, &sz) == 0) { free(buf); return d; }
        if (!first_free && need_blocks > 0 && vmufs_free_blocks(d) >= need_blocks) first_free = d;
    }
    return first_free;
}

int plat_save_read(void *data, size_t max) {
    maple_device_t *d = save_card(0);
    if (!d) return -1;
    void *buf = NULL; int sz = 0;
    if (vmufs_read(d, SAVE_NAME, &buf, &sz) < 0) return -1;
    vmu_pkg_t pkg;
    int n = -1;
    if (vmu_pkg_parse(buf, sz, &pkg) == 0 && pkg.data_len >= 0) {
        n = pkg.data_len < (int)max ? pkg.data_len : (int)max;
        memcpy(data, pkg.data, n);
    }
    free(buf);
    return n;
}

int plat_save_write(const void *data, size_t size) {
    size_t isz; uint8_t *icon = plat_load_file("ui/vmu_icon.bin", &isz);   /* 'FVI1' n, speed, pal[16], n*512 */
    vmu_pkg_t pkg;
    memset(&pkg, 0, sizeof pkg);
    strcpy(pkg.desc_short, "FREEDOM PLANET");
    strcpy(pkg.desc_long, "Freedom Planet DC save data");
    strcpy(pkg.app_id, "FPDC");
    if (icon && isz >= 8 + 32 && !memcmp(icon, "FVI1", 4)) {
        int n = icon[4] | icon[5] << 8;
        if (isz >= (size_t)(8 + 32 + n * 512)) {
            pkg.icon_cnt = n; pkg.icon_anim_speed = icon[6] | icon[7] << 8;
            memcpy(pkg.icon_pal, icon + 8, 32);
            pkg.icon_data = icon + 8 + 32;
        }
    }
    pkg.eyecatch_type = VMUPKG_EC_NONE;
    pkg.data_len = (int)size; pkg.data = data;
    uint8_t *out = NULL; int osz = 0;
    int r = -1;
    if (vmu_pkg_build(&pkg, &out, &osz) == 0) {
        maple_device_t *d = save_card((osz + 511) / 512);
        if (d && vmufs_write(d, SAVE_NAME, out, osz, VMUFS_OVERWRITE) == 0) r = 0;
        free(out);
    }
    free(icon);
    if (r < 0) plat_log("save: no memory card with %d free blocks\n", (osz + 511) / 512);
    return r;
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
    demo_load();
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
