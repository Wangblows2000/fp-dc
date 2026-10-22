/* Tile collision: per-column heightmaps, floor probes, solid tests.
 * Heights are measured in px from the tile's bottom edge (0..16). */
#include <math.h>
#include "../fp.h"

int level_tile(const level_t *l, int tx, int ty) {
    if (tx < 0 || tx >= l->w) return T_SOLID;      /* side walls */
    if (ty < 0) return T_EMPTY;
    if (ty >= l->h) return T_EMPTY;                /* bottomless */
    return l->tiles[ty * l->w + tx];
}

int tile_height(int t, int cx) {
    switch (t) {
    case T_SOLID: case T_SPIKE: case T_ONEWAY: return 16;
    case T_SLOPE_R: return cx + 1;
    case T_SLOPE_L: return 16 - cx;
    case T_GENTLE_R_LO: return (cx + 2) / 2;
    case T_GENTLE_R_HI: return 8 + (cx + 2) / 2;
    case T_GENTLE_L_HI: return 16 - cx / 2;
    case T_GENTLE_L_LO: return 8 - cx / 2;
    default: return 0;
    }
}

/* topmost surface in column x within [y0, y0+maxdist] */
static int surface(const level_t *l, float x, float y0, float maxdist, int oneway, float *sy) {
    int tx = (int)floorf(x / TILE);
    int cx = (int)floorf(x) - tx * TILE;
    int ty0 = (int)floorf(y0 / TILE);
    int ty1 = (int)floorf((y0 + maxdist) / TILE);
    for (int ty = ty0; ty <= ty1; ty++) {
        int t = level_tile(l, tx, ty);
        if (t == T_ONEWAY && !oneway) continue;
        int h = tile_height(t, cx);
        if (h <= 0) continue;
        float s = (float)(ty * TILE + TILE - h);
        if (s < y0) {
            /* started inside this tile's solid part; a surface above is not
             * reachable this way. Fully solid tile => wall, keep scanning. */
            continue;
        }
        if (s > y0 + maxdist) return 0;
        /* the surface must be exposed: tile above must not be solid at the top */
        if (h == 16) {
            int above = level_tile(l, tx, ty - 1);
            if (above != T_ONEWAY && tile_height(above, cx) > 0) continue;
        }
        *sy = s;
        return 1;
    }
    return 0;
}

int floor_probe(const level_t *l, float x, float y0, float maxdist, int oneway, float *sy, float *angle) {
    float s;
    if (!surface(l, x, y0, maxdist, oneway, &s)) return 0;
    *sy = s;
    if (angle) {
        float sl, sr;
        int okl = surface(l, x - 4, s - 8, 16, oneway, &sl);
        int okr = surface(l, x + 4, s - 8, 16, oneway, &sr);
        if (!okl) sl = s;
        if (!okr) sr = s;
        float dx = (okl ? 4.f : 0.f) + (okr ? 4.f : 0.f);
        *angle = dx > 0 ? atan2f(sl - sr, dx) : 0.f;
    }
    return 1;
}

int solid_at(const level_t *l, float x, float y) {
    int tx = (int)floorf(x / TILE), ty = (int)floorf(y / TILE);
    int t = level_tile(l, tx, ty);
    if (t == T_EMPTY || t == T_ONEWAY) return 0;
    int cx = (int)floorf(x) - tx * TILE;
    float ly = y - ty * TILE;                 /* 0 top .. 16 bottom */
    return ly >= TILE - tile_height(t, cx);
}

int spike_at(const level_t *l, float x, float y) {
    int t = level_tile(l, (int)floorf(x / TILE), (int)floorf(y / TILE));
    return t == T_SPIKE;
}
