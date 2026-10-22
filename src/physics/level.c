/* Pixel-mask terrain collision. Masks are the alpha channels of the original
 * terrain pieces (solid = obstacle type 9, platform = type 8), rasterised by
 * tools/import_frame.py into deduplicated 16x16 1-bit tiles. */
#include <math.h>
#include "../fp.h"

static inline int bit(const level_t *l, const uint16_t *map, int x, int y) {
    if (x < 0 || x >= l->w) return map == l->smap;   /* level sides are walls */
    if (y < 0 || y >= l->h) return 0;                  /* open top, bottomless pit */
    int t = map[(y >> 4) * l->tw + (x >> 4)];
    if (t == 0) return 0;
    if (t == 1) return 1;
    return (l->tiles[t * 16 + (y & 15)] >> (x & 15)) & 1;
}

int px_solid(const level_t *l, int x, int y) { return bit(l, l->smap, x, y); }
int px_platform(const level_t *l, int x, int y) { return bit(l, l->pmap, x, y); }
int solid_at(const level_t *l, float x, float y) { return px_solid(l, (int)floorf(x), (int)floorf(y)); }

static int surface(const level_t *l, int x, int y0, int maxdist, int oneway, int *sy) {
    /* starting inside solid: this column is a wall at this height */
    if (px_solid(l, x, y0)) return 0;
    for (int y = y0 + 1; y <= y0 + maxdist; y++) {
        if (px_solid(l, x, y)) { *sy = y; return 1; }
        if (oneway && px_platform(l, x, y) && !px_platform(l, x, y - 1)) { *sy = y; return 1; }
    }
    return 0;
}

int floor_probe(const level_t *l, float fx, float fy0, float maxdist, int oneway, float *sy, float *angle) {
    int x = (int)floorf(fx), y0 = (int)floorf(fy0), s;
    if (!surface(l, x, y0, (int)maxdist, oneway, &s)) return 0;
    *sy = (float)s;
    if (angle) {
        int sl, sr, okl, okr;
        okl = surface(l, x - 6, s - 12, 24, oneway, &sl);
        okr = surface(l, x + 6, s - 12, 24, oneway, &sr);
        if (!okl) sl = s;
        if (!okr) sr = s;
        float dx = (okl ? 6.f : 0.f) + (okr ? 6.f : 0.f);
        *angle = dx > 0 ? atan2f((float)(sl - sr), dx) : 0.f;
    }
    return 1;
}
