/* UI scenes: original front-end frames imported by tools/import_ui.py (every object with all of its
 * animations/directions). The menu code drives the original objects by class name. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fp.h"
#include "ui.h"

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int ui_banks_load(ui_banks_t *b, const char *prefix, int n) {
    char nm[48];
    memset(b, 0, sizeof *b);
    for (int i = 0; i < n && i < UI_MAX_BANKS; i++) {
        snprintf(nm, sizeof nm, "ui/%s_ui%d", prefix, i);
        if (bank_load(&b->bank[i], nm) < 0) return -1;
        b->n = i + 1;
    }
    return 0;
}

void ui_banks_free(ui_banks_t *b) {
    for (int i = 0; i < b->n; i++) { r_tex_free(&b->bank[i].tex); free(b->bank[i].frames); free(b->bank[i].anims); free(b->bank[i].seq); free(b->bank[i].act); free(b->bank[i].mbits); free(b->bank[i].moff); }
    memset(b, 0, sizeof *b);
}

int ui_load(ui_scene_t *s, const char *file, ui_banks_t *banks) {
    size_t sz; char path[48];
    memset(s, 0, sizeof *s);
    snprintf(path, sizeof path, "ui/%s.ui", file);
    uint8_t *d = plat_load_file(path, &sz);
    if (!d) return -1;
    if (sz < 12 || memcmp(d, "FPU1", 4)) { free(d); return -1; }
    s->w = rd16(d + 4); s->h = rd16(d + 6); s->nobj = rd16(d + 8);
    s->banks = banks;
    s->o = calloc(s->nobj ? s->nobj : 1, sizeof(ui_obj_t));
    size_t off = 12;
    for (int i = 0; i < s->nobj; i++) {
        ui_obj_t *o = &s->o[i];
        if (off + 32 > sz) { free(d); return -1; }
        memcpy(o->cls, d + off, 24); o->cls[23] = 0;
        o->x = (int16_t)rd16(d + off + 24); o->y = (int16_t)rd16(d + off + 26);
        o->layer = d[off + 28]; o->kind = d[off + 29]; o->nent = d[off + 30];
        int nent = o->nent;
        off += 32;
        if (o->nent > UI_MAX_ENT) o->nent = UI_MAX_ENT;
        if (off + (size_t)nent * 6 > sz) { free(d); return -1; }
        for (int k = 0; k < nent; k++, off += 6) {
            if (k >= UI_MAX_ENT) continue;
            o->ent[k].anim = d[off]; o->ent[k].dir = d[off + 1]; o->ent[k].bank = d[off + 2];
            o->ent[k].ba = rd16(d + off + 4);
        }
        o->vis = 1; o->cur = 0;
        ui_obj_select(s, o, o->ent[0].anim, o->ent[0].dir);
    }
    free(d);
    return 0;
}

void ui_free(ui_scene_t *s) { free(s->o); memset(s, 0, sizeof *s); }

ui_obj_t *ui_find(ui_scene_t *s, const char *cls, int nth) {
    for (int i = 0; i < s->nobj; i++)
        if (!strncmp(s->o[i].cls, cls, 23) && nth-- == 0) return &s->o[i];   /* names stored truncated to 23 */
    return NULL;
}

/* choose the (animation, direction) the original object shows; falls back to the first entry */
int ui_obj_select(ui_scene_t *s, ui_obj_t *o, int anim, int dir) {
    if (!o || !o->nent) return -1;
    int k = 0;
    for (int i = 0; i < o->nent; i++) if (o->ent[i].anim == anim && o->ent[i].dir == dir) { k = i; goto found; }
    for (int i = 0; i < o->nent; i++) if (o->ent[i].anim == anim) { k = i; goto found; }
    return -1;
found:
    if (k != o->cur || !o->an.b) {
        o->cur = k;
        const bank_t *b = &s->banks->bank[o->ent[k].bank];
        o->an.b = NULL;
        anim_set(&o->an, b, o->ent[k].ba);
        anim_restart(&o->an);
    }
    return 0;
}

void ui_obj_frame(ui_obj_t *o, int frame) { o->an.frame = frame; o->an.acc = 0; o->hold = 1; }

void ui_tick(ui_scene_t *s) {
    for (int i = 0; i < s->nobj; i++) if (s->o[i].vis && !s->o[i].hold && s->o[i].an.b) anim_tick(&s->o[i].an, -1);
}

/* Fusion action point of the object's current image, in canvas coordinates (falls back to the hot spot) */
int ui_action_point(ui_obj_t *o, float *x, float *y) {
    if (!o || !o->an.b) return -1;
    const frame_t *f = anim_frame(&o->an);
    if (!f) return -1;
    const bank_t *b = o->an.b;
    int fi = (int)(f - b->frames);
    float ax = f->hx, ay = f->hy;
    if (b->act && fi >= 0 && fi < b->nframes) { ax = b->act[2 * fi]; ay = b->act[2 * fi + 1]; }
    *x = o->x + o->dx + o->shx - f->hx + ax; *y = o->y + o->dy + o->shy - f->hy + ay;
    return 0;
}

void ui_draw_obj(ui_scene_t *s, ui_obj_t *o, float camx, float camy, uint32_t argb) {
    if (!o || !o->vis || !o->an.b) return;
    const frame_t *f = anim_frame(&o->an);
    if (!f) return;
    /* backdrops are placed by their top-left corner (their image hot spots are not used) */
    float hx = o->kind == 0 ? f->hx : 0, hy = o->kind == 0 ? f->hy : 0;
    frame_draw(o->an.b, f, o->x - camx + o->dx + o->shx + hx, o->y - camy + o->dy + o->shy + hy, 0, argb);
}

void ui_draw(ui_scene_t *s, float camx, float camy, int lmin, int lmax) {
    for (int l = lmin; l <= lmax; l++)
        for (int i = 0; i < s->nobj; i++) {
            ui_obj_t *o = &s->o[i];
            if (o->layer != l) continue;
            /* Fusion quick backdrops repeat their pattern across the frame; the original title panels
             * show the 320-wide sky/sea repeated across the whole view */
            if (o->kind == 0 && (!strncmp(o->cls, "quickbackdrop", 13) || !strncmp(o->cls, "backgroundsystembox", 19)) && o->vis && o->an.b) {
                const frame_t *f = anim_frame(&o->an);
                if (f && f->w > 0) {
                    float x0 = o->x - camx; while (x0 > 0) x0 -= f->w;
                    float dx = o->dx;
                    for (float x = x0; x < SCREEN_W; x += f->w) { o->dx = x - (o->x - camx); ui_draw_obj(s, o, camx, camy, 0xffffffff); }
                    o->dx = dx;
                    continue;
                }
            }
            ui_draw_obj(s, o, camx, camy, 0xffffffff);
        }
}
