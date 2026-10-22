#ifndef FP_UI_H
#define FP_UI_H
#include "../fp.h"

#define UI_MAX_ENT 40
#define UI_MAX_BANKS 4

typedef struct { bank_t bank[UI_MAX_BANKS]; int n; } ui_banks_t;
typedef struct {
    char cls[24];
    int16_t x, y;
    uint8_t layer, kind, nent;
    struct { uint8_t anim, dir, bank; uint16_t ba; } ent[UI_MAX_ENT];
    int vis, cur, hold;
    float dx, dy;                 /* runtime offset (menu animation) */
    animator_t an;
} ui_obj_t;
typedef struct { int w, h, nobj; ui_obj_t *o; ui_banks_t *banks; } ui_scene_t;

int  ui_banks_load(ui_banks_t *b, const char *prefix, int n);
void ui_banks_free(ui_banks_t *b);
int  ui_load(ui_scene_t *s, const char *file, ui_banks_t *banks);
void ui_free(ui_scene_t *s);
ui_obj_t *ui_find(ui_scene_t *s, const char *cls, int nth);
int  ui_obj_select(ui_scene_t *s, ui_obj_t *o, int anim, int dir);
void ui_obj_frame(ui_obj_t *o, int frame);
void ui_tick(ui_scene_t *s);
void ui_draw(ui_scene_t *s, float camx, float camy, int lmin, int lmax);
void ui_draw_obj(ui_scene_t *s, ui_obj_t *o, float camx, float camy, uint32_t argb);
#endif
