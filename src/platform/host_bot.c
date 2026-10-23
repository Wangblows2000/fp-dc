/* Host-only playthrough bot (FP_BOT=1): a simple controller used to find input routes through the acts.
 * It never touches game state; it only chooses buttons from what a player would see. Every frame's buttons
 * are printed ("bot <f> <buttons>") so a run can be turned into a replayable script (tools/play/bot2script.py).
 * Strategy: run toward the goal side; attack enemies ahead; when progress stalls, try in turn: jump, longer
 * jump, Dragon Boost, back off and jump, climb (up), drop (down + jump). Waypoints (FP_BOT_WP="x:dir,...")
 * flip the run direction once Lilac passes an x (for acts that wind back). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../fp.h"

static int dir = 1, stall_t, tactic, tactic_t, best_x_t;
static double best_x = -1e9, last_x;
static int wp_n, wp_i; static struct { double x; int d; double y; } wp[64];
static char lv[8];

static void wp_load(void) {
    const char *s = getenv("FP_BOT_WP"); wp_n = 0; wp_i = 0;
    if (!s) return;
    char key[8]; snprintf(key, sizeof key, "%.6s=", G.lvdir);     /* "dv1=x:d:y,x:d;dv2=..." */
    const char *p = strstr(s, key);
    if (!p) return;
    p += strlen(key);
    while (*p && *p != ';' && wp_n < 64) {
        double x = atof(p); int d = 1; double y = -1;
        const char *c = strchr(p, ':'); const char *e = strpbrk(p, ",;");
        if (c && (!e || c < e)) { d = atoi(c + 1); const char *c2 = strchr(c + 1, ':'); if (c2 && (!e || c2 < e)) y = atof(c2 + 1); }
        wp[wp_n].x = x; wp[wp_n].d = d; wp[wp_n].y = y; wp_n++;
        if (!e || *e == ';') break;
        p = e + 1;
    }
}

uint32_t bot_input(int frame) {
    static int last_mode = -1;
    if (G.mode != MODE_PLAY) {       /* menus / results: press jump every 20 frames */
        last_mode = G.mode;
        return (frame % 20) < 2 ? BTN_JUMP : 0;
    }
    if (strcmp(lv, G.lvdir) || last_mode != MODE_PLAY) {
        strcpy(lv, G.lvdir); dir = 1; stall_t = tactic = tactic_t = 0; best_x = -1e9; best_x_t = frame; wp_load();
    }
    last_mode = MODE_PLAY;
    player_t *p = &G.p;
    if (wp_i < wp_n && ((dir > 0 && p->x >= wp[wp_i].x) || (dir < 0 && p->x <= wp[wp_i].x)) &&
        (wp[wp_i].y < 0 || fabs(p->y - ORIG_REF_DY - wp[wp_i].y) < 120)) {
        dir = wp[wp_i].d; wp_i++; best_x = -1e9; best_x_t = frame; tactic = 0;
    }
    double prog = dir * p->x;
    if (prog > best_x + 4) { best_x = prog; best_x_t = frame; tactic = 0; tactic_t = 0; }
    uint32_t b = dir > 0 ? BTN_RIGHT : BTN_LEFT;
    /* enemies ahead within reach: attack */
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive || o->st == 3) continue;
        if (o->type != OT_TURRETUS && o->type != OT_DRAGONFLY && o->type != OT_GANSEKI) continue;
        double dx = (o->x - p->x) * dir, dy = o->y - (p->y - ORIG_REF_DY);
        if (dx > -8 && dx < 48 && fabs(dy) < 40 && (frame & 15) == 0) b |= BTN_ATTACK;
    }
    int stalled = frame - best_x_t;
    if (stalled > 40) {
        if (tactic_t <= 0) { tactic = (tactic + 1) % 7; tactic_t = 70; }
        tactic_t--;
        switch (tactic) {
        case 1: if (tactic_t > 50) b |= BTN_JUMP; break;                                  /* short hop */
        case 2: if (tactic_t > 20) b |= BTN_JUMP; if (tactic_t < 50 && tactic_t > 40) b |= BTN_JUMP; break;   /* full jump + cyclone */
        case 3: b = (b & ~(BTN_LEFT | BTN_RIGHT)) | (dir > 0 ? BTN_LEFT : BTN_RIGHT); if (tactic_t < 30) { b = dir > 0 ? BTN_RIGHT : BTN_LEFT; if (tactic_t > 10) b |= BTN_JUMP; } break;   /* run-up */
        case 4: b |= BTN_UP; break;                                                        /* ladder */
        case 5: if (tactic_t > 40) b |= BTN_SPECIAL | BTN_UP; else b |= BTN_SPECIAL; break; /* boost */
        case 6: b |= BTN_DOWN; if (tactic_t > 55) b |= BTN_JUMP; break;                    /* drop through */
        }
        if (stalled > 1500) { dir = -dir; best_x = -1e9; best_x_t = frame; }               /* wrong way: turn round */
    }
    if (p->state == 13 /* ST_LADDER */) b = BTN_UP;
    last_x = p->x;
    return b;
}
