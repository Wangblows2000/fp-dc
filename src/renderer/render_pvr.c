/* PowerVR2 backend (Dreamcast). All game drawing goes into the punch-through
 * list: ARGB1555 textures give 1-bit alpha which matches the pixel art.
 * Logical 426x240 is scaled x1.5 and letterboxed on the 640x480 framebuffer (D-012). */
#include <string.h>
#include <kos.h>
#include "../fp.h"

#define MAXTEX 64
#ifdef FP_VIEW_320
static float SX = 2.0f, SY = 2.0f, OFF_X = 0.0f, OFF_Y = 0.0f;
static int letterbox = 0;
void r_set_letterbox(int on) { (void)on; }
#else
/* Presentation (SV.letterbox, Options > SCREEN SIZE). The logical view stays 426x240 either way:
 *  on : x1.5 both ways -> 639x360, centred, opaque bars 60 px top and bottom (16:9 picture on a 4:3 frame);
 *  off: x1.5 across, x2 down -> 639x480 (fills the 4:3 frame; integer vertical scale). D-025. */
static float SX = 1.5f, SY = 1.5f, OFF_X = 0.5f, OFF_Y = 60.0f;
static int letterbox = 1;
void r_set_letterbox(int on) {
    letterbox = on ? 1 : 0;
    SY = on ? 1.5f : 2.0f; OFF_Y = on ? 60.0f : 0.0f;
}
#endif

static struct { pvr_ptr_t ptr; pvr_poly_hdr_t hdr, hdr_tr, hdr_ia, hdr_ib; int w, h; } texs[MAXTEX];
/* hit flash (Chowdren invert shader 21): two translucent passes, A = white silhouette (texel alpha, colour
 * from the specular offset), B = destination x (1 - texel colour); transparent texels are black (index 0) */
static int invert_on; static uint32_t cur_oargb;
void r_set_invert(int on) { invert_on = on; }
/* Fades: r_fade() queues a translucent black quad; everything drawn after it in the same frame is
 * deferred and replayed in the translucent list on top of the fade (PVR lists are submitted once). */
typedef struct { int kind; const tex_t *t; int u, v, uw, vh, flip; float a[8]; uint32_t argb; int inv; } dcmd_t;
static dcmd_t dq[192]; static int ndq, overlay, in_tr;
static pvr_poly_hdr_t col_hdr_tr;
static int ntex;
static pvr_poly_hdr_t col_hdr;
static int cur_hdr;           /* -2 none, -1 colour, >=0 texture */
static float zc;
uint32_t r_stat_polys;

int r_init(void) {
    pvr_init_params_t params = {
        .opb_sizes = { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16 },
        .vertex_buf_size = 128 * 1024,     /* ~150-300 polys per frame (perf log): 128 KB x 2 is ample; frees VRAM for textures */
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
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&col_hdr_tr, &cxt);
    return 0;
}


int r_tex_load(tex_t *t, const void *data, size_t size) {
    const uint8_t *d = data;
    uint16_t hdr[6];
    int slot = 0;
    while (slot < ntex && texs[slot].ptr) slot++;                            /* reuse freed slots */
    if (size < 16 || memcmp(d, "FPTX", 4) || slot >= MAXTEX) return -1;
    memcpy(hdr, d + 4, 12);
    int w = hdr[0], h = hdr[1], fmt = hdr[2], grp = hdr[5];
    size_t bpp = fmt >= 1 ? 1 : 2;      /* fmt 1: PAL8 linear, 2: PAL8 pre-twiddled on the disc (tools/twiddle_disc.py) */
    if (fmt > 2 || (fmt >= 1 && grp > 3) || w < 8 || h < 8 || (w & (w - 1)) || (h & (h - 1)) || (size_t)w * h * bpp + 16 > size) return -1;
    pvr_ptr_t p = pvr_mem_malloc(w * h * bpp);
    if (!p) { plat_log("VRAM exhausted (%dx%d)\n", w, h); return -1; }
    int txfmt;
    if (fmt == 2) {       /* already in VRAM order: straight store-queue copy */
        pvr_txr_load(d + 16, p, w * h);
        txfmt = PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(grp);
    } else if (fmt == 1) { /* 8-bit palettized, linear: KOS twiddles every texel while uploading */
        pvr_txr_load_ex(d + 16, p, w, h, PVR_TXRLOAD_8BPP);
        txfmt = PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(grp);
    } else {
        pvr_txr_load(d + 16, p, w * h * 2);
        txfmt = PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED;
    }
    pvr_poly_cxt_t cxt;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_PT_POLY, txfmt, w, h, p, PVR_FILTER_NONE);
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.txr.env = PVR_TXRENV_MODULATE;
    pvr_poly_compile(&texs[slot].hdr, &cxt);
    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, txfmt, w, h, p, PVR_FILTER_NONE);
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.txr.env = PVR_TXRENV_MODULATE;
    pvr_poly_compile(&texs[slot].hdr_tr, &cxt);
    cxt.gen.specular = PVR_SPECULAR_ENABLE;
    cxt.blend.src = PVR_BLEND_SRCALPHA; cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&texs[slot].hdr_ia, &cxt);
    cxt.gen.specular = PVR_SPECULAR_DISABLE;
    cxt.blend.src = PVR_BLEND_ZERO; cxt.blend.dst = PVR_BLEND_INVDESTCOLOR;   /* as a destination factor: 1 - source colour */
    pvr_poly_compile(&texs[slot].hdr_ib, &cxt);
    texs[slot].ptr = p; texs[slot].w = w; texs[slot].h = h;
    t->id = slot; if (slot == ntex) ntex++; t->w = w; t->h = h; t->srcw = hdr[3]; t->srch = hdr[4];
    return 0;
}

/* free one texture (front-end screen banks are swapped) */
void r_tex_free(tex_t *t) {
    if (t->id < 0 || t->id >= ntex || !texs[t->id].ptr) return;
    pvr_wait_ready(); pvr_mem_free(texs[t->id].ptr); texs[t->id].ptr = NULL; t->id = -1;
}

/* drop every texture (level change): VRAM texture memory is reset as a whole */
void r_reset(void) { pvr_wait_ready(); pvr_mem_reset(); for (int i = 0; i < ntex; i++) texs[i].ptr = NULL; ntex = 0; }

int r_pal_load(const void *data, size_t size) {
    const uint8_t *d = data;
    uint16_t g, pal[256];
    if (size < 8 + 512 || memcmp(d, "FPPL", 4)) return -1;
    memcpy(&g, d + 4, 2);
    if (g > 3) return -1;
    memcpy(pal, d + 8, 512);
    pvr_set_pal_format(PVR_PAL_ARGB1555);
    for (int i = 0; i < 256; i++) pvr_set_pal_entry(g * 256 + i, pal[i]);
    return 0;
}

void r_begin(void) {
    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_PT_POLY);
    cur_hdr = -2;
    zc = 1.0f;
    r_stat_polys = 0;
    ndq = 0; overlay = 0; in_tr = 0;
}

static void replay(void);
static void bars(void);
void r_end(void) {
    bars();
    pvr_list_finish();
    if (ndq) {
        pvr_list_begin(PVR_LIST_TR_POLY);
        in_tr = 1; cur_hdr = -2;
        replay();
        bars();
        pvr_list_finish();
    }
    pvr_scene_finish();
}

void r_fade(float alpha) {
    if (alpha <= 0) return;
    if (alpha > 1) alpha = 1;
    if (ndq < 192) { dcmd_t *c = &dq[ndq++]; c->kind = 2; c->argb = ((uint32_t)(alpha * 255) << 24); }
    overlay = 1;
}
static int inv_pass;          /* 0 normal, 1 / 2 invert passes (replay) */
#define HDR(i) (in_tr ? (inv_pass == 1 ? &texs[i].hdr_ia : inv_pass == 2 ? &texs[i].hdr_ib : &texs[i].hdr_tr) : &texs[i].hdr)

static inline void quad(float x0, float y0, float x1, float y1,
                        float u0, float v0, float u1, float v1, uint32_t argb) {
    pvr_vertex_t v;
    zc += 0.0001f;
    v.z = zc; v.argb = argb; v.oargb = cur_oargb;
    v.flags = PVR_CMD_VERTEX;
    v.x = x0; v.y = y0; v.u = u0; v.v = v0; pvr_prim(&v, sizeof v);
    v.x = x1; v.y = y0; v.u = u1; v.v = v0; pvr_prim(&v, sizeof v);
    v.x = x0; v.y = y1; v.u = u0; v.v = v1; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x1; v.y = y1; v.u = u1; v.v = v1; pvr_prim(&v, sizeof v);
    r_stat_polys++;
}

static void bars(void) {          /* letterbox: opaque black over anything drawn outside the 426x240 view */
    if (!letterbox) return;
    if (cur_hdr != -1) { pvr_prim(in_tr ? &col_hdr_tr : &col_hdr, sizeof col_hdr); cur_hdr = -1; }
    zc += 1.0f;                    /* above every sprite of this list */
    quad(0, 0, 640, OFF_Y, 0, 0, 0, 0, 0xff000000);
    quad(0, OFF_Y + 240 * SY, 640, 480, 0, 0, 0, 0, 0xff000000);
}

void r_sprite(const tex_t *t, int u, int v, int uw, int vh, float x, float y, float w, float h, int flip, uint32_t argb) {
    if (t->id < 0 || t->id >= ntex) return;
    if ((overlay || invert_on) && !in_tr) {
        if (ndq < 192) { dcmd_t *c = &dq[ndq++]; *c = (dcmd_t){ 0, t, u, v, uw, vh, flip, { x, y, w, h }, argb, invert_on }; }
        return;
    }
    if (x + w < 0 || y + h < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    if (cur_hdr != t->id) { pvr_prim(HDR(t->id), sizeof(pvr_poly_hdr_t)); cur_hdr = t->id; }
    float iw = 1.0f / texs[t->id].w, ih = 1.0f / texs[t->id].h;
    float u0 = u * iw, u1 = (u + uw) * iw, v0 = v * ih, v1 = (v + vh) * ih;
    if (flip) { float tmp = u0; u0 = u1; u1 = tmp; }
    quad(x * SX + OFF_X, y * SY + OFF_Y, (x + w) * SX + OFF_X, (y + h) * SY + OFF_Y, u0, v0, u1, v1, argb);
}

/* rotated sprite: texel (hx,hy) of the rect lands on screen (px,py); angle in degrees CCW
 * (Fusion/Chowdren convention: Active::set_angle rotates around the hotspot) */
void r_sprite_rot(const tex_t *t, int u, int v, int uw, int vh, float px, float py, float hx, float hy,
                  float angle, int flip, uint32_t argb) {
    if (t->id < 0 || t->id >= ntex) return;
    if ((overlay || invert_on) && !in_tr) {
        if (ndq < 192) { dcmd_t *c = &dq[ndq++]; *c = (dcmd_t){ 1, t, u, v, uw, vh, flip, { px, py, hx, hy, angle }, argb, invert_on }; }
        return;
    }
    if (flip) hx = uw - 1 - hx;                    /* the mirrored image's hotspot (texture u reversed below) */
    float r = (uw > vh ? uw : vh) * 1.5f;
    if (px + r < 0 || py + r < 0 || px - r >= SCREEN_W || py - r >= SCREEN_H) return;
    if (cur_hdr != t->id) { pvr_prim(HDR(t->id), sizeof(pvr_poly_hdr_t)); cur_hdr = t->id; }
    float iw = 1.0f / texs[t->id].w, ih = 1.0f / texs[t->id].h;
    float u0 = u * iw, u1 = (u + uw) * iw, v0 = v * ih, v1 = (v + vh) * ih;
    if (flip) { float tmp = u0; u0 = u1; u1 = tmp; }
    float a = angle * 0.017453293f, c = fcos(a), sn = fsin(a);
    float lx[4] = { -hx, uw - hx, -hx, uw - hx }, ly[4] = { -hy, -hy, vh - hy, vh - hy };
    float tu[4] = { u0, u1, u0, u1 }, tv[4] = { v0, v0, v1, v1 };
    pvr_vertex_t vt;
    zc += 0.0001f;
    vt.z = zc; vt.argb = argb; vt.oargb = cur_oargb;
    for (int i = 0; i < 4; i++) {
        vt.flags = i == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        vt.x = (px + lx[i] * c + ly[i] * sn) * SX + OFF_X;     /* screen y points down: CCW = (c, -s; s, c) */
        vt.y = (py - lx[i] * sn + ly[i] * c) * SY + OFF_Y;
        vt.u = tu[i]; vt.v = tv[i];
        pvr_prim(&vt, sizeof vt);
    }
    r_stat_polys++;
}

void r_rect(float x, float y, float w, float h, uint32_t argb) {
    if (x + w < 0 || y + h < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    if (overlay && !in_tr) {
        if (ndq < 192) { dcmd_t *c = &dq[ndq++]; *c = (dcmd_t){ 3, NULL, 0, 0, 0, 0, 0, { x, y, w, h }, argb, 0 }; }
        return;
    }
    if (cur_hdr != -1) { pvr_prim(in_tr ? &col_hdr_tr : &col_hdr, sizeof col_hdr); cur_hdr = -1; }
    quad(x * SX + OFF_X, y * SY + OFF_Y, (x + w) * SX + OFF_X, (y + h) * SY + OFF_Y, 0, 0, 0, 0, argb | 0xff000000);
}

static void replay(void) {
    for (int i = 0; i < ndq; i++) {
        dcmd_t *c = &dq[i];
        if ((c->kind == 0 || c->kind == 1) && c->inv) {
            for (inv_pass = 1; inv_pass <= 2; inv_pass++) {
                cur_hdr = -2;                                  /* force the pass header */
                cur_oargb = inv_pass == 1 ? 0x00ffffff : 0;
                uint32_t col = inv_pass == 1 ? (c->argb & 0xff000000) : (c->argb | 0x00ffffff);
                if (c->kind == 0) r_sprite(c->t, c->u, c->v, c->uw, c->vh, c->a[0], c->a[1], c->a[2], c->a[3], c->flip, col);
                else r_sprite_rot(c->t, c->u, c->v, c->uw, c->vh, c->a[0], c->a[1], c->a[2], c->a[3], c->a[4], c->flip, col);
            }
            inv_pass = 0; cur_oargb = 0; cur_hdr = -2;
            continue;
        }
        switch (c->kind) {
        case 0: r_sprite(c->t, c->u, c->v, c->uw, c->vh, c->a[0], c->a[1], c->a[2], c->a[3], c->flip, c->argb); break;
        case 1: r_sprite_rot(c->t, c->u, c->v, c->uw, c->vh, c->a[0], c->a[1], c->a[2], c->a[3], c->a[4], c->flip, c->argb); break;
        case 2:   /* fade: translucent black over the whole display (incl. letterbox) */
            if (cur_hdr != -1) { pvr_prim(&col_hdr_tr, sizeof col_hdr_tr); cur_hdr = -1; }
            quad(0, 0, 640, 480, 0, 0, 0, 0, c->argb);
            break;
        case 3: r_rect(c->a[0], c->a[1], c->a[2], c->a[3], c->argb); break;
        }
    }
}
