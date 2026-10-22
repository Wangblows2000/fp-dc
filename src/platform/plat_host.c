/* Host (Linux) test platform: headless, deterministic, scripted input.
 * usage: fpdc_host <data_dir> <frames> [input_script] [dump_dir] [dump_every]
 * input_script lines: "<start> <end> <buttons>"  buttons from LRUDJXBS (J=jump A, X=attack, B=special, S=start)
 * Frames are written as PPM into dump_dir; a state trace goes to stdout. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "../fp.h"

static const char *data_dir = ".";
static struct { int a, b; uint32_t m; } script[512];
static int nscript, cur_frame;
static uint32_t prev_held;

void plat_init(void) {}
void plat_shutdown(void) {}
int  plat_running(void) { return 1; }

void plat_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

uint64_t plat_time_us(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000;
}

void *plat_load_file(const char *name, size_t *size) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", data_dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { plat_log("missing file %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 64 * 1024 * 1024) { fclose(f); return NULL; }
    void *d = malloc(n ? n : 1);
    if (fread(d, 1, n, f) != (size_t)n) { free(d); fclose(f); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return d;
}

void plat_poll_input(input_t *in) {
    uint32_t held = 0;
    for (int i = 0; i < nscript; i++)
        if (cur_frame >= script[i].a && cur_frame < script[i].b) held |= script[i].m;
    in->held = held;
    in->pressed = held & ~prev_held;
    in->released = prev_held & ~held;
    prev_held = held;
}

/* audio is a no-op on host (logged) */
static int nsnd;
void snd_init_fp(void) {}
int  snd_load(const char *name) { size_t s; void *d = plat_load_file(name, &s); if (!d) return -1; free(d); return nsnd++; }
void snd_play(int h, int vol, int pan) { (void)vol; (void)pan; if (getenv("FP_TRACE_SND")) printf("snd %d @%d\n", h, cur_frame); }
int  music_play(const char *name) { (void)name; return 0; }
void music_update(void) {}
void music_stop(void) {}

extern void soft_dump(const char *path);

static uint32_t parse_btn(const char *s) {
    uint32_t m = 0;
    for (; *s; s++) switch (*s) {
        case 'L': m |= BTN_LEFT; break; case 'R': m |= BTN_RIGHT; break;
        case 'U': m |= BTN_UP; break; case 'D': m |= BTN_DOWN; break;
        case 'J': m |= BTN_JUMP; break; case 'X': m |= BTN_ATTACK; break;
        case 'B': m |= BTN_SPECIAL; break; case 'S': m |= BTN_START; break;
    }
    return m;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s data_dir frames [script] [dump_dir] [every]\n", argv[0]); return 1; }
    data_dir = argv[1];
    int frames = atoi(argv[2]);
    const char *dump = argc > 4 ? argv[4] : NULL;
    int every = argc > 5 ? atoi(argv[5]) : 1;
    if (argc > 3 && strcmp(argv[3], "-")) {
        FILE *f = fopen(argv[3], "r");
        if (!f) { perror(argv[3]); return 1; }
        char line[128], btn[32];
        while (fgets(line, sizeof line, f) && nscript < 512) {
            if (line[0] == '#') continue;
            if (sscanf(line, "%d %d %31s", &script[nscript].a, &script[nscript].b, btn) == 3)
                script[nscript++].m = parse_btn(btn);
        }
        fclose(f);
    }
    plat_init();
    r_init();
    snd_init_fp();
    if (game_init() < 0) return 2;
    input_t in;
    for (cur_frame = 0; cur_frame < frames; cur_frame++) {
        plat_poll_input(&in);
        game_update(&in);
        const player_t *p = &G.p;
        printf("f %d x %.2f y %.2f gsp %.3f vx %.3f vy %.3f gnd %d ang %.3f st %d hp %d cr %d k %d\n",
               cur_frame, p->x, p->y, p->gsp, p->vx, p->vy, p->ground, p->angle, p->state, p->hp, p->crystals, G.kills);
        if (dump && cur_frame % every == 0) {
            game_draw();
            char path[512];
            snprintf(path, sizeof path, "%s/f%05d.ppm", dump, cur_frame);
            soft_dump(path);
        }
    }
    return 0;
}
