/* Resource loading with bounds validation for all converted formats. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../fp.h"

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

int tex_load_file(tex_t *t, const char *name) {
    size_t sz;
    char fn[64];
    snprintf(fn, sizeof fn, "%s.tex", name);
    void *d = plat_load_file(fn, &sz);
    if (!d) return -1;
    int r = r_tex_load(t, d, sz);
    free(d);
    if (r < 0) plat_log("tex %s: invalid\n", fn);
    return r;
}

int bank_load(bank_t *b, const char *name) {
    char fn[64];
    size_t sz;
    memset(b, 0, sizeof *b);
    if (tex_load_file(&b->tex, name) < 0) return -1;
    snprintf(fn, sizeof fn, "%s.spr", name);
    uint8_t *d = plat_load_file(fn, &sz);
    if (!d) return -1;
    if (sz < 12 || memcmp(d, "FPSB", 4)) goto bad;
    b->nframes = rd16(d + 4);
    b->nanims = rd16(d + 6);
    size_t need = 12 + (size_t)b->nframes * 12 + (size_t)b->nanims * 24;
    if (need > sz) goto bad;
    b->frames = malloc(sizeof(frame_t) * b->nframes);
    b->anims = malloc(sizeof(anim_t) * b->nanims);
    const uint8_t *p = d + 12;
    for (int i = 0; i < b->nframes; i++, p += 12) {
        frame_t *f = &b->frames[i];
        f->u = rd16(p); f->v = rd16(p + 2); f->w = rd16(p + 4); f->h = rd16(p + 6);
        f->hx = (int16_t)rd16(p + 8); f->hy = (int16_t)rd16(p + 10);
        if (f->u + f->w > b->tex.w || f->v + f->h > b->tex.h) goto bad;
    }
    int total = 0;
    for (int i = 0; i < b->nanims; i++, p += 24) {
        anim_t *a = &b->anims[i];
        memcpy(a->name, p, 16); a->name[15] = 0;
        a->first = rd16(p + 16); a->count = rd16(p + 18);
        a->speed = rd16(p + 20); a->loop = rd16(p + 22);
        if (a->count == 0 || a->speed == 0) goto bad;
        if (a->first + a->count > total) total = a->first + a->count;
    }
    if (need + (size_t)total * 2 > sz) goto bad;
    b->seq = malloc(2 * total);
    for (int i = 0; i < total; i++) {
        b->seq[i] = rd16(p + 2 * i);
        if (b->seq[i] >= b->nframes) goto bad;
    }
    free(d);
    return 0;
bad:
    plat_log("bank %s: invalid file\n", name);
    free(d);
    return -1;
}

int bank_anim(const bank_t *b, const char *name) {
    for (int i = 0; i < b->nanims; i++)
        if (!strcmp(b->anims[i].name, name)) return i;
    plat_log("missing anim %s\n", name);
    return 0;
}

const frame_t *bank_frame(const bank_t *b, int anim, int tick) {
    const anim_t *a = &b->anims[anim];
    int i = tick / a->speed;
    if (a->loop) i %= a->count;
    else if (i >= a->count) i = a->count - 1;
    return &b->frames[b->seq[a->first + i]];
}

int anim_done(const bank_t *b, int anim, int tick) {
    const anim_t *a = &b->anims[anim];
    return tick >= a->count * a->speed;
}

void bank_draw(const bank_t *b, int anim, int tick, float x, float y, int flip, uint32_t argb) {
    const frame_t *f = bank_frame(b, anim, tick);
    float dx = flip ? x - (f->w - f->hx) : x - f->hx;
    r_sprite(&b->tex, f->u, f->v, f->w, f->h, dx, y - f->hy, f->w, f->h, flip, argb);
}

int level_load(level_t *l, const char *name) {
    char fn[64];
    size_t sz;
    snprintf(fn, sizeof fn, "%s.lvl", name);
    uint8_t *d = plat_load_file(fn, &sz);
    memset(l, 0, sizeof *l);
    if (!d) return -1;
    if (sz < 12 || memcmp(d, "FPLV", 4)) goto bad;
    l->w = rd16(d + 4); l->h = rd16(d + 6); l->nobj = rd16(d + 8);
    if (!l->w || !l->h || 12 + (size_t)l->w * l->h + (size_t)l->nobj * 8 > sz) goto bad;
    l->tiles = malloc((size_t)l->w * l->h);
    memcpy(l->tiles, d + 12, (size_t)l->w * l->h);
    for (int i = 0; i < l->w * l->h; i++) if (l->tiles[i] >= T_COUNT) goto bad;
    l->objs = malloc(sizeof(lvlobj_t) * (l->nobj ? l->nobj : 1));
    const uint8_t *p = d + 12 + l->w * l->h;
    for (int i = 0; i < l->nobj; i++, p += 8) {
        l->objs[i].type = rd16(p);
        l->objs[i].x = (int16_t)rd16(p + 2);
        l->objs[i].y = (int16_t)rd16(p + 4);
    }
    free(d);
    return 0;
bad:
    plat_log("level %s: invalid\n", name);
    free(d);
    return -1;
}
