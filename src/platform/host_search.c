/* Host-only route search (FP_SEARCH=1): finds an input sequence through the current act by beam search over
 * the deterministic simulation (game state snapshots, src/game/snap.c). The result is ordinary pad input,
 * replayed frame by frame by the real run and written as a script (FP_SEARCH_OUT), so the same inputs can be
 * replayed later with no search (tests/play). It only presses buttons; it never edits game state.
 * Targets: FP_WP="dv1=x,y x,y ...;dv2=..." (optional waypoints, world px; Y = feet); the act's goal is reaching
 * the act end (clear) or the results screen. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../fp.h"

#define MACRO 10
enum { NACT = 14 };
static const uint32_t ACT[NACT][2] = {      /* buttons for the first half / second half of a macro */
    { BTN_RIGHT, BTN_RIGHT }, { BTN_LEFT, BTN_LEFT },
    { BTN_RIGHT | BTN_JUMP, BTN_RIGHT | BTN_JUMP }, { BTN_LEFT | BTN_JUMP, BTN_LEFT | BTN_JUMP },
    { BTN_JUMP, BTN_JUMP }, { 0, 0 }, { BTN_RIGHT | BTN_JUMP, BTN_RIGHT },
    { BTN_UP, BTN_UP }, { BTN_DOWN, BTN_DOWN }, { BTN_DOWN | BTN_JUMP, BTN_DOWN },
    { BTN_RIGHT | BTN_ATTACK, BTN_RIGHT }, { BTN_LEFT | BTN_ATTACK, BTN_LEFT },
    { BTN_RIGHT | BTN_SPECIAL, BTN_RIGHT | BTN_SPECIAL }, { BTN_JUMP, BTN_JUMP | BTN_RIGHT },
};

typedef struct { uint8_t *st; uint32_t prev; double score; int wp; int parent, act, depth; int done; } node_t;

static int nwp; static double wpx[64], wpy[64];
static void wp_parse(void) {
    nwp = 0;
    const char *s = getenv("FP_WP"); if (!s) return;
    char key[8]; snprintf(key, sizeof key, "%.4s=", G.lvdir);
    const char *p = strstr(s, key); if (!p) return;
    p += strlen(key);
    while (*p && *p != ';' && nwp < 64) {
        double x, y; int n;
        if (sscanf(p, "%lf,%lf%n", &x, &y, &n) != 2) break;
        wpx[nwp] = x; wpy[nwp] = y; nwp++; p += n;
        while (*p == ' ') p++;
    }
}

static double score_of(int *wp) {
    const player_t *p = &G.p;
    double fy = p->y - ORIG_REF_DY + 14;
    while (*wp < nwp && fabs(p->x - wpx[*wp]) < 48 && fabs(fy - wpy[*wp]) < 64) (*wp)++;
    double s = *wp * 100000.0;
    if (*wp < nwp) s -= hypot(p->x - wpx[*wp], (fy - wpy[*wp]) * 1.5);
    else s += p->x;
    s += p->hp * 50.0 + p->lives * 2000.0;
    s += boss_progress() * 3000.0;                           /* damage dealt to the act's (mini)boss */
    return s;
}

static int sim(uint32_t *prev, int act, int *done) {
    char dir0[8]; strcpy(dir0, G.lvdir);
    for (int k = 0; k < MACRO; k++) {
        input_t in; in.held = ACT[act][k < MACRO / 2 ? 0 : 1];
        in.pressed = in.held & ~*prev; in.released = *prev & ~in.held; *prev = in.held;
        game_update(&in);
        if (G.clear_t || G.mode != MODE_PLAY || strcmp(dir0, G.lvdir) || results_on()) { *done = 1; return 1; }
        if (G.p.state == 6 || G.p.hp <= 0) return 0;          /* dead: prune */
    }
    return 1;
}

/* returns a malloc'd array of macro actions (count in *n), or NULL */
int *search_act(int beam, int maxdepth, int *n, uint32_t prev0) {
    wp_parse();
    size_t ss = snap_size();
    uint8_t *root = malloc(ss); snap_save(root);
    int cap = beam * NACT * maxdepth + 1, nn = 0;
    node_t *all = calloc(cap, sizeof *all);
    int *cur = malloc(sizeof(int) * beam), ncur = 1, found = -1;
    int wp0 = 0; double sc0 = score_of(&wp0);
    all[0] = (node_t){ root, prev0, sc0, wp0, -1, -1, 0, 0 }; nn = 1; cur[0] = 0;
    int *cand = malloc(sizeof(int) * beam * NACT);
    for (int d = 0; d < maxdepth && found < 0; d++) {
        int nc = 0;
        for (int i = 0; i < ncur && found < 0; i++) {
            node_t *pa = &all[cur[i]];
            for (int a = 0; a < NACT; a++) {
                snap_load(pa->st);
                uint32_t prev = pa->prev; int done = 0;
                if (!sim(&prev, a, &done)) continue;
                int wp = pa->wp; double sc = score_of(&wp);
                if (nn >= cap) break;
                node_t *c = &all[nn];
                *c = (node_t){ NULL, prev, sc, wp, cur[i], a, d + 1, done };
                if (done) { found = nn++; break; }
                c->st = malloc(ss); snap_save(c->st);
                cand[nc++] = nn++;
            }
        }
        if (found >= 0) break;
        /* keep the best `beam`, at most 2 per 32x32 cell (diversity) */
        for (int i = 1; i < nc; i++) { int v = cand[i], j = i; while (j > 0 && all[cand[j - 1]].score < all[v].score) { cand[j] = cand[j - 1]; j--; } cand[j] = v; }
        ncur = 0;
        static int cellk[4096]; int ncell = 0;
        for (int i = 0; i < nc && ncur < beam; i++) {
            node_t *c = &all[cand[i]];
            snap_load(c->st);
            int key = ((int)(G.p.x / 32) * 977 + (int)(G.p.y / 32) * 31 + G.p.ground + c->wp * 7919 + boss_progress() * 104729) & 0x7fffffff;
            int cnt = 0; for (int k = 0; k < ncell; k++) if (cellk[k] == key) cnt++;
            if (cnt >= 2) continue;
            if (ncell < 4096) cellk[ncell++] = key;
            cur[ncur++] = cand[i];
        }
        for (int i = 0; i < nc; i++) {   /* free states that left the beam */
            int keep = 0; for (int k = 0; k < ncur; k++) if (cur[k] == cand[i]) keep = 1;
            if (!keep) { free(all[cand[i]].st); all[cand[i]].st = NULL; }
        }
        if (getenv("FP_SEARCH_V") && ncur) { snap_load(all[cur[0]].st); fprintf(stderr, "search d %d best x %.0f y %.0f wp %d/%d sc %.0f\n", d, G.p.x, G.p.y - ORIG_REF_DY + 14, all[cur[0]].wp, nwp, all[cur[0]].score); }
        if (!ncur) break;
    }
    int *res = NULL;
    int end = found >= 0 ? found : (ncur ? cur[0] : -1);
    if (end >= 0) {
        int len = all[end].depth; res = malloc(sizeof(int) * (len + 1));
        for (int k = end, i = len - 1; k > 0; k = all[k].parent, i--) res[i] = all[k].act;
        *n = len;
    }
    snap_load(root);
    for (int i = 0; i < nn; i++) free(all[i].st);
    free(all); free(cur); free(cand);
    if (found < 0 && res) { fprintf(stderr, "search: act end not reached (best path %d macros)\n", *n); }
    return res;
}

/* ---- Go-Explore style search: archive of cells (64x64 px) with the shortest/healthiest path reaching each;
 * repeatedly return to a promising cell and explore with sticky random macros. Handles routes that wind
 * up/back (ladders, climbs) where a greedy beam stalls. */
typedef struct { int key; uint8_t *st; uint32_t prev; int *path, len, visits, hp, lives; double x; } cell_t;
static uint32_t gx_seed = 12345;
static int gx_rand(int n) { gx_seed = gx_seed * 1103515245u + 12345u; return (int)((gx_seed >> 8) % (unsigned)n); }
static int cell_key(void) { return ((int)(G.p.x / 64)) * 1000 + (int)((G.p.y - ORIG_REF_DY) / 64) + (G.p.state == 13 ? 500 : 0) + (boss_progress() / 4) * 1000000; }
/* progress: x plus damage dealt to the act's boss (a boss arena blocks x until the boss is beaten) */
static double prog_now(void) {
    if (G.cam.U) return 20000.0 + boss_progress() * 60.0 + G.p.hp;   /* boss arena (bounds locked): x means nothing, damage does */
    return G.p.x + boss_progress() * 60.0;
}

int *search_explore(int iters, int *n, uint32_t prev0) {
    size_t ss = snap_size();
    int cap = 8000, nc = 0;
    cell_t *C = calloc(cap, sizeof *C);
    uint8_t *root = malloc(ss); snap_save(root);
    C[0] = (cell_t){ cell_key(), root, prev0, NULL, 0, 0, G.p.hp, G.p.lives, prog_now() }; nc = 1;
    int *res = NULL; double W = G.lvl.w > 0 ? G.lvl.w + 20000 : 30000;
    int path[4096];
    double bestx = G.p.x;
    for (int it = 0; it < iters && !res; it++) {
        /* choose: best of 12 samples by progress + novelty */
        int ci = 0; double bs = -1e9;
        for (int k = 0; k < 12; k++) {
            int i = gx_rand(nc);
            double sc = 3.0 * C[i].x / W + 1.0 / sqrt(1.0 + C[i].visits) + 0.002 * C[i].hp;
            if (sc > bs) { bs = sc; ci = i; }
        }
        cell_t *c = &C[ci]; c->visits++;
        snap_load(c->st);
        uint32_t prev = c->prev;
        int len = c->len; if (len) memcpy(path, c->path, sizeof(int) * len);
        int a = gx_rand(NACT), steps = 4 + gx_rand(24);
        for (int s2 = 0; s2 < steps && len < 4096; s2++) {
            if (gx_rand(100) < 30) { int r = gx_rand(100); a = r < 25 ? 0 : r < 45 ? 2 : r < 50 ? 6 : gx_rand(NACT); }
            int done = 0;
            if (!sim(&prev, a, &done)) break;
            path[len++] = a;
            if (done) { res = malloc(sizeof(int) * len); memcpy(res, path, sizeof(int) * len); *n = len; break; }
            int key = cell_key(), j;
            for (j = 0; j < nc; j++) if (C[j].key == key) break;
            int better = j == nc || G.p.hp + G.p.lives * 100 > C[j].hp + C[j].lives * 100 ||
                         (G.p.hp + G.p.lives * 100 == C[j].hp + C[j].lives * 100 && len < C[j].len);
            if (!better) continue;
            if (j == nc) { if (nc >= cap) continue; nc++; C[j].st = malloc(ss); C[j].path = NULL; C[j].visits = 0; }
            C[j].key = key; snap_save(C[j].st); C[j].prev = prev; C[j].len = len; C[j].hp = G.p.hp; C[j].lives = G.p.lives; C[j].x = prog_now();
            free(C[j].path); C[j].path = malloc(sizeof(int) * len); memcpy(C[j].path, path, sizeof(int) * len);
            if (prog_now() > bestx) bestx = prog_now();
        }
        if (getenv("FP_SEARCH_V") && it % 2000 == 0) fprintf(stderr, "explore it %d cells %d best x %.0f\n", it, nc, bestx);
    }
    if (!res) {        /* not reached: return the path to the rightmost cell so the run moves on */
        int bi = 0; for (int i = 1; i < nc; i++) if (C[i].x > C[bi].x || (bi == 0 && C[i].x >= C[bi].x - 1 && C[i].len > 0)) bi = i;
        if (!C[bi].len) for (int i = 1; i < nc; i++) if (C[i].len > C[bi].len && C[i].hp >= C[0].hp) bi = i;   /* stalled: keep moving (most explored path) */
        if (C[bi].len) { res = malloc(sizeof(int) * C[bi].len); memcpy(res, C[bi].path, sizeof(int) * C[bi].len); *n = C[bi].len; }
        fprintf(stderr, "explore: act end not reached (cells %d, best x %.0f)\n", nc, C[bi].x);
    }
    snap_load(root);
    for (int i = 0; i < nc; i++) { free(C[i].st); free(C[i].path); }
    free(C);
    return res;
}

uint32_t search_macro_buttons(int act, int k) { return ACT[act][k < MACRO / 2 ? 0 : 1]; }
int search_macro_len(void) { return MACRO; }
