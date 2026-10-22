/* Software reference renderer (host only): same API as the PVR backend,
 * ARGB1555 punch-through semantics (texel alpha bit = visible). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../fp.h"

#define MAXTEX 64
static struct { int w, h; uint16_t *px; } texs[MAXTEX];
static int ntex;
static uint32_t fb[SCREEN_W * SCREEN_H];

int r_init(void) { return 0; }
void r_shutdown(void) {}

int r_tex_load(tex_t *t, const void *data, size_t size) {
    const uint8_t *d = data;
    if (size < 16 || memcmp(d, "FPTX", 4) || ntex >= MAXTEX) return -1;
    uint16_t hdr[6]; memcpy(hdr, d + 4, 12);
    if ((size_t)hdr[0] * hdr[1] * 2 + 16 > size || hdr[2] != 0) return -1;
    texs[ntex].w = hdr[0]; texs[ntex].h = hdr[1];
    texs[ntex].px = malloc((size_t)hdr[0] * hdr[1] * 2);
    memcpy(texs[ntex].px, d + 16, (size_t)hdr[0] * hdr[1] * 2);
    t->id = ntex++; t->w = hdr[0]; t->h = hdr[1]; t->srcw = hdr[3]; t->srch = hdr[4];
    return 0;
}

void r_begin(void) { memset(fb, 0, sizeof fb); }
void r_end(void) {}

static uint32_t modulate(uint32_t c, uint32_t m) {
    uint32_t r = ((c >> 16) & 255) * ((m >> 16) & 255) / 255;
    uint32_t g = ((c >> 8) & 255) * ((m >> 8) & 255) / 255;
    uint32_t b = (c & 255) * (m & 255) / 255;
    return (r << 16) | (g << 8) | b;
}

void r_sprite(const tex_t *t, int u, int v, int uw, int vh, float x, float y, float w, float h, int flip, uint32_t argb) {
    if (t->id < 0 || t->id >= ntex || w <= 0 || h <= 0) return;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1 = (int)floorf(x + w), y1 = (int)floorf(y + h);
    const uint16_t *px = texs[t->id].px; int tw = texs[t->id].w, th = texs[t->id].h;
    for (int sy = y0 < 0 ? 0 : y0; sy < y1 && sy < SCREEN_H; sy++) {
        int tv = v + (int)((sy - y + 0.5f) * vh / h);
        if (tv < 0 || tv >= th) continue;
        for (int sx = x0 < 0 ? 0 : x0; sx < x1 && sx < SCREEN_W; sx++) {
            float fu = (sx - x + 0.5f) * uw / w;
            int tu = flip ? u + uw - 1 - (int)fu : u + (int)fu;
            if (tu < 0 || tu >= tw) continue;
            uint16_t c = px[tv * tw + tu];
            if (!(c & 0x8000)) continue;
            uint32_t rgb = (((c >> 10) & 31) << 19) | (((c >> 5) & 31) << 11) | ((c & 31) << 3);
            fb[sy * SCREEN_W + sx] = modulate(rgb, argb);
        }
    }
}

void r_rect(float x, float y, float w, float h, uint32_t argb) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1 = (int)floorf(x + w), y1 = (int)floorf(y + h);
    for (int sy = y0 < 0 ? 0 : y0; sy < y1 && sy < SCREEN_H; sy++)
        for (int sx = x0 < 0 ? 0 : x0; sx < x1 && sx < SCREEN_W; sx++)
            fb[sy * SCREEN_W + sx] = argb & 0xffffff;
}

void soft_dump(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint8_t c[3] = { fb[i] >> 16, fb[i] >> 8, fb[i] };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
}
