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
/* host: no audio output; FP_TRACE_SND logs music starts so tests can check transitions (no duplicates) */
static char host_mus[64];
int  music_play(const char *name) {
    if (!strcmp(host_mus, name)) return 0;
    snprintf(host_mus, sizeof host_mus, "%s", name);
    if (getenv("FP_TRACE_SND")) printf("music %s @%d\n", name, cur_frame);
    return 0;
}
void music_volume(int pct) { (void)pct; }
void music_update(void) {}
void music_stop(void) { if (host_mus[0] && getenv("FP_TRACE_SND")) printf("music stop @%d\n", cur_frame); host_mus[0] = 0; }

extern void soft_dump(const char *path);
void plat_quit(void) { printf("quit @%d\n", cur_frame); exit(0); }

/* debug-route commands shared with tools/orig/trace_orig.py: "<f> TP x y" (original units: Y is the
 * reference point 14 px above the feet), "<f> EN v" energy, "<f> HP v" petals (half-petals);
 * host only: "<f> HURT dx" a hit from x+dx, ignored while invulnerable (like an enemy contact) */
typedef struct { int f; char k; float a, b; } dbg_t;
static dbg_t dbg[256]; static int ndbg;
static void apply_dbg(int f, int tp) {
    for (int i = 0; i < ndbg; i++) {
        if ((dbg[i].k == 'T') != tp) continue;
        if (dbg[i].f != f) continue;
        player_t *p = &G.p;
        /* like the tracer's TP: writes the movement X, Y and zeroes B, G; L, N, O are left as they are.
         * The tracer writes it at the START of the original's logic frame f, which the comparison (input
         * shift 2) lines up with the END of our frame f - 2: applied after that update, before its row */
        if (dbg[i].k == 'T') { p->x = dbg[i].a; p->y = dbg[i].b + ORIG_REF_DY; p->vx = p->vy = p->gsp = 0; }
        if (dbg[i].k == 'E') p->energy = dbg[i].a;
        if (dbg[i].k == 'D' && !player_invincible(p)) player_hurt(p, p->x + dbg[i].a);   /* test hit from dx */
        if (dbg[i].k == 'H') p->hp = (int)dbg[i].a;
    }
}

static uint32_t parse_btn(const char *s) {
    uint32_t m = 0;
    for (; *s; s++) switch (*s) {
        case 'L': m |= BTN_LEFT; break; case 'R': m |= BTN_RIGHT; break;
        case 'U': m |= BTN_UP; break; case 'D': m |= BTN_DOWN; break;
        case 'J': m |= BTN_JUMP; break; case 'X': m |= BTN_ATTACK; break;
        case 'l': m |= BTN_LT; break; case 'r': m |= BTN_RT; break;
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
            float fx, fy; int ff;
            if (sscanf(line, "%d TP %f %f", &ff, &fx, &fy) == 3 && ndbg < 256) { dbg[ndbg++] = (dbg_t){ ff, 'T', fx, fy }; continue; }
            if (sscanf(line, "%d EN %f", &ff, &fx) == 2 && ndbg < 256) { dbg[ndbg++] = (dbg_t){ ff, 'E', fx, 0 }; continue; }
            if (sscanf(line, "%d HURT %f", &ff, &fx) == 2 && ndbg < 256) { dbg[ndbg++] = (dbg_t){ ff, 'D', fx, 0 }; continue; }
            if (sscanf(line, "%d HP %f", &ff, &fx) == 2 && ndbg < 256) { dbg[ndbg++] = (dbg_t){ ff, 'H', fx, 0 }; continue; }
            if (sscanf(line, "%d %d %31s", &script[nscript].a, &script[nscript].b, btn) == 3)
                script[nscript++].m = parse_btn(btn);
        }
        fclose(f);
    }
    plat_init();
    r_init();
    snd_init_fp();
    if (game_init() < 0) return 2;
    /* comparison harness: FP_SPAWN_X overrides the spawn x (original Lilac starts at 68.715 after
     * the intro); FP_CSV writes rows in the original trace's columns/units (tools/orig/compare.py) */
    const char *sx = getenv("FP_SPAWN_X");
    if (sx) {
        float y, a;
        G.p.spawn_x = (float)atof(sx);
        if (floor_probe(&G.lvl, G.p.spawn_x, G.p.spawn_y - 64, 200, 1, &y, &a)) G.p.spawn_y = y;
        player_reset(&G.p, G.p.spawn_x, G.p.spawn_y);
        /* FP_SPAWN_Y / _L / _N: the trace's frame-0 movement Y (reference point), L and N */
        if (getenv("FP_SPAWN_Y")) G.p.y = atof(getenv("FP_SPAWN_Y")) + ORIG_REF_DY;
        if (getenv("FP_SPAWN_L")) G.p.ground = atoi(getenv("FP_SPAWN_L"));
        if (getenv("FP_SPAWN_N")) G.p.N = G.p.O = atof(getenv("FP_SPAWN_N"));
        player_sync_sensors(&G.p);
        camera_reset(); camera_update();
    }
    if (getenv("FP_COLPROBE")) {            /* "x,y0,y1": print the solid column (collision debugging) */
        int cx, y0, y1;
        if (sscanf(getenv("FP_COLPROBE"), "%d,%d,%d", &cx, &y0, &y1) == 3)
            for (int y = y0; y <= y1; y++) printf("col %d %d %d\n", cx, y, solid_at(&G.lvl, (float)cx, (float)y));
    }
    if (getenv("FP_OBJ_PRETICK")) {
        objects_pretick(atoi(getenv("FP_OBJ_PRETICK")));
    }
    if (getenv("FP_ENERGY")) G.p.energy = (float)atof(getenv("FP_ENERGY"));
    if (getenv("FP_HP")) G.p.hp = atoi(getenv("FP_HP"));
    FILE *csv = getenv("FP_CSV") ? fopen(getenv("FP_CSV"), "w") : NULL;
    if (csv) fprintf(csv, "frame,held,X,Y,B,G,L,Q,state,anim,afr,rot,energy,hp\n");
    input_t in;
    for (cur_frame = 0; cur_frame < frames; cur_frame++) {
        plat_poll_input(&in);
        apply_dbg(cur_frame, 0);
        game_update(&in);
        apply_dbg(cur_frame + 2, 1);
        const player_t *p = &G.p;
        printf("f %d x %.2f y %.2f gsp %.3f vx %.3f vy %.3f gnd %d ang %.3f st %d hp %d cr %d k %d an %d fr %d rot %.2f en %.2f lv %d face %d mode %d sel %d cx %.1f cy %.1f act %d\n",
               cur_frame, p->x, p->y, p->gsp, p->vx, p->vy, p->ground, p->N, p->state, p->hp, p->crystals, G.kills, p->an.anim, p->an.frame,
               p->rot, p->energy, p->lives, p->face, G.mode, G.in_select, G.camx, G.camy, G.lvdir[0] ? G.lvdir[2] - '0' : 0);
        if (csv) {
            char h[16]; int k = 0;
            const char *bn = "LRUDJXBS";
            for (int b = 0; b < 8; b++) if (in.held & (1u << (b < 4 ? b : b))) h[k++] = bn[b];
            h[k] = 0;
            /* original units: B = ground speed (ground) or x speed (air); G = y speed in air; Y = feet - 14 */
            fprintf(csv, "%d,%s,%.4f,%.4f,%.5f,%.5f,%d,%d,%d,%d,%d,%.3f,%.2f,%d\n", cur_frame, k ? h : "-", p->x, p->y - ORIG_REF_DY,
                    p->ground ? p->gsp : p->vx, p->ground ? 0.f : p->vy, p->ground, p->face, p->state, p->an.anim, p->an.frame,
                    p->rot, p->energy, p->hp);
        }
        if (dump && cur_frame % every == 0) {
            game_draw();
            char path[512];
            snprintf(path, sizeof path, "%s/f%05d.ppm", dump, cur_frame);
            soft_dump(path);
        }
    }
    return 0;
}
