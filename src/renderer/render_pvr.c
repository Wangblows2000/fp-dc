/* PowerVR2 backend (Dreamcast). All game drawing goes into the punch-through
 * list: ARGB1555 textures give 1-bit alpha which matches the pixel art.
 * Logical 320x240 is scaled x2 to the 640x480 framebuffer. */
#include <string.h>
#include <kos.h>
#include "../fp.h"

#define MAXTEX 64
#define SCALE 2.0f

static struct { pvr_ptr_t ptr; pvr_poly_hdr_t hdr; int w, h; } texs[MAXTEX];
static int ntex;
static pvr_poly_hdr_t col_hdr;
static int cur_hdr;           /* -2 none, -1 colour, >=0 texture */
static float zc;
uint32_t r_stat_polys;

int r_init(void) {
    pvr_init_params_t params = {
        .opb_sizes = { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_16 },
        .vertex_buf_size = 512 * 1024,
        .dma_enabled = 0,
        .fsaa_enabled = 0,
        .autosort_disabled = 0,
        .opb_overflow_count = 3,
    };
    if (pvr_init(&params) < 0) return -1;
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    pvr_poly_cxt_t cxt;
    pvr_poly_cxt_col(&cxt, PVR_LIST_PT_POLY);
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&col_hdr, &cxt);
    return 0;
}

void r_shutdown(void) { pvr_shutdown(); }

int r_tex_load(tex_t *t, const void *data, size_t size) {
    const uint8_t *d = data;
    uint16_t hdr[6];
    if (size < 16 || memcmp(d, "FPTX", 4) || ntex >= MAXTEX) return -1;
    memcpy(hdr, d + 4, 12);
    int w = hdr[0], h = hdr[1];
    if (hdr[2] != 0 || w < 8 || h < 8 || (w & (w - 1)) || (h & (h - 1)) || (size_t)w * h * 2 + 16 > size) return -1;
    pvr_ptr_t p = pvr_mem_malloc(w * h * 2);
    if (!p) { plat_log("VRAM exhausted (%dx%d)\n", w, h); return -1; }
    pvr_txr_load(d + 16, p, w * h * 2);
    pvr_poly_cxt_t cxt;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
                     w, h, p, PVR_FILTER_NONE);
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.txr.env = PVR_TXRENV_MODULATE;
    pvr_poly_compile(&texs[ntex].hdr, &cxt);
    texs[ntex].ptr = p; texs[ntex].w = w; texs[ntex].h = h;
    t->id = ntex++; t->w = w; t->h = h; t->srcw = hdr[3]; t->srch = hdr[4];
    return 0;
}

void r_begin(void) {
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_PT_POLY);
    cur_hdr = -2;
    zc = 1.0f;
    r_stat_polys = 0;
}

void r_end(void) {
    pvr_list_finish();
    pvr_scene_finish();
}

static inline void quad(float x0, float y0, float x1, float y1,
                        float u0, float v0, float u1, float v1, uint32_t argb) {
    pvr_vertex_t v;
    zc += 0.0001f;
    v.z = zc; v.argb = argb; v.oargb = 0;
    v.flags = PVR_CMD_VERTEX;
    v.x = x0; v.y = y0; v.u = u0; v.v = v0; pvr_prim(&v, sizeof v);
    v.x = x1; v.y = y0; v.u = u1; v.v = v0; pvr_prim(&v, sizeof v);
    v.x = x0; v.y = y1; v.u = u0; v.v = v1; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x1; v.y = y1; v.u = u1; v.v = v1; pvr_prim(&v, sizeof v);
    r_stat_polys++;
}

void r_sprite(const tex_t *t, int u, int v, int uw, int vh, float x, float y, float w, float h, int flip, uint32_t argb) {
    if (t->id < 0 || t->id >= ntex) return;
    if (x + w < 0 || y + h < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    if (cur_hdr != t->id) { pvr_prim(&texs[t->id].hdr, sizeof(pvr_poly_hdr_t)); cur_hdr = t->id; }
    float iw = 1.0f / texs[t->id].w, ih = 1.0f / texs[t->id].h;
    float u0 = u * iw, u1 = (u + uw) * iw, v0 = v * ih, v1 = (v + vh) * ih;
    if (flip) { float tmp = u0; u0 = u1; u1 = tmp; }
    quad(x * SCALE, y * SCALE, (x + w) * SCALE, (y + h) * SCALE, u0, v0, u1, v1, argb);
}

void r_rect(float x, float y, float w, float h, uint32_t argb) {
    if (x + w < 0 || y + h < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    if (cur_hdr != -1) { pvr_prim(&col_hdr, sizeof col_hdr); cur_hdr = -1; }
    quad(x * SCALE, y * SCALE, (x + w) * SCALE, (y + h) * SCALE, 0, 0, 0, 0, argb | 0xff000000);
}
