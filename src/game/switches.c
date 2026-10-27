/* Switches and the doors they open (session 13): one reusable system for every level.
 *
 * Switches                                   Doors (all of them answer to the level's switches)
 *   PressurePlate_769     OT_PPLATE            SwitchDoor_997 / 1066  OT_DOOR    sinks 8 px a frame to 128, rises 2 (DV)
 *   SwitchMillaBlock_784  OT_MBLOCK            SwitchDoor_770         OT_SDOOR   slides left 4 px a frame to 224 (AT)
 *                                              SwitchDoor2_777        OT_SDOOR2  the AT1 arena gate (sinks / rises 4)
 *
 * The plate is one Fusion object (handle 769 in DV2, DV3 and AT1), but each frame's events press it differently, so the
 * level table chooses the rule (level_def_t.plate_rule) and the pass (switch_late):
 *   rule 0 (DV2 / DV3, RE 4971-4973 + 5182-5186): pressed on every frame that Lilac stands on it (ground + bottom-big
 *     sensor on the image box), a resting pushable block covers it (D-036 reach) or a parked bike overlaps it; the
 *     click (5183) sounds once per act. Runs in the platform block, before the movement (switch_late 0).
 *   rule 1 (AT1, RE 4093-4096): pressed by a grounded bottom-big sensor (mask) or the Iron Worm's head, and stays
 *     pressed while anything still touches a plate (also in the air); clicks on every new press. Runs after the
 *     movement and the objects pass (switch_late 1, RV a1_plates).
 * The Milla block (4362-4363) presses permanently when Milla's cube hits it while she stands on the ground.
 * Before session 13 these lived in gimmicks.c (DV plate + door, first instance only) and aqua_gim.c (AT plates retyped
 * to OT_APLATE at spawn, block, doors), chosen by the level name / act; behaviour per level is unchanged (cmp_all,
 * cmp_at, test_gameplay plate tests, test_aqua). */
#include <math.h>
#include <string.h>
#include "../fp.h"

static struct { int dv_click, at_click, mblock_click, gate_closed, skipE, skip_once; } SW;   /* skipE / skip_once: SwitchDoor E and the 5099 latch */

static void ens_set(obj_t *o, const char *name) { int a = ens_anim(name); if (a >= 0) anim_set(&o->an, &G.ens, a); else o->an.b = NULL; }
static int box_sensor(int s, float x0, float y0, float x1, float y1) {   /* a sensor mask overlapping a box */
    int ox, oy, h; const signed char *a, *b;
    if (!mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    for (int j = 0; j < h; j++) { int y = oy + j; if (b[j] < a[j] || y < y0 || y >= y1) continue; if (ox + b[j] >= x0 && ox + a[j] < x1) return 1; }
    return 0;
}
static int sensor_on_box(const obj_t *o, int s) {          /* rule 0: the image rectangle */
    float x0 = o->x, y0 = o->y, x1 = o->x + 32, y1 = o->y + 32;
    if (o->an.b) { const frame_t *f = anim_frame(&o->an); x0 = floorf(o->x) - f->hx; y0 = floorf(o->y) - f->hy; x1 = x0 + f->w; y1 = y0 + f->h; }
    return box_sensor(s, x0, y0, x1, y1);
}
static int sensor_on_mask(const obj_t *o, int s) {         /* rule 1: the image mask (Frame collision) */
    int ox, oy, h; const signed char *a, *b;
    if (!o->alive || !o->an.b || !mv_sensor_rows(s, &ox, &oy, &a, &b, &h)) return 0;
    return obj_rows_hit(o, ox, oy, a, b, h);
}

void switches_spawn(void) {
    memset(&SW, 0, sizeof SW); SW.gate_closed = 1;
    static int reg; if (!reg) { reg = 1; snap_register(&SW, sizeof SW); }
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        switch (o->type) {
        case OT_PPLATE: ens_set(o, "pplate"); o->st = 0; o->fl = 0; o->hidden = 0; break;
        case OT_MBLOCK: ens_set(o, "mblock"); o->fl = 0; o->hidden = 0; break;
        case OT_DOOR:   ens_set(o, "door"); o->c = 0; o->layer = 1; break;   /* RV plate_f90 (DV2): drawn behind the layer-2 ground, so the sunk door shows only its top */
        case OT_SDOOR:  ens_set(o, "sdoor"); o->c = 0; o->layer = 2; o->fl = 0; break;   /* B = layer + 1 */
        case OT_SDOOR2: ens_set(o, "sdoor2"); o->c = 0; o->layer = 2; o->fl = 0; break;
        }
    }
    /* 6031 / 6032: plate A = 0 (the newest) starts pressed. Session 20: this ran before the reset above, which cleared it, so no
     * plate started down (RV build/orig_traces/rm_s20/ax_a: (328,2674) starts pressed, flags 0x1084). */
    if (level_cur()->plate_rule == 2) {
        obj_t *last = NULL;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_PPLATE && G.o[i].x >= 0 && G.o[i].y < G.lvl.h) last = &G.o[i];
        if (last) { last->hidden = 1; last->fl |= 1; }
    }
}

enum { PB_PLATE_REACH = 32 };   /* D-036: a resting block counts from up to 32 px above the plate (DV2's block cannot enter the pit) */
void switches_update(player_t *p, int late) {
    const level_def_t *L = level_cur();
    if (L->switch_late != late) return;
    int pressed_p = 0, pressed_m = 0, any_touch = 0;
    if (L->plate_rule == 2) {                                                                       /* RM2 5894-5897: radio buttons */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *pl = &G.o[i];
            if (!pl->alive || pl->type != OT_PPLATE || (pl->fl & 1) || !p->ground || !sensor_on_mask(pl, SENS_BOTTOMBIG)) continue;
            for (int k = 0; k < G.nobjs; k++) if (G.o[k].alive && G.o[k].type == OT_PPLATE) { G.o[k].hidden = 0; G.o[k].fl &= ~1; }   /* 5895 loop_floorswitch */
            pl->hidden = 1; pl->fl |= 1; snd_play_orig(49, 220, 128); relic_axle_turn();          /* 5897 (the block branch 5894 / 5896: INFERRED unreachable) */
            break;
        }
        return;
    }
    if (L->plate_rule == 0) {
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *pl = &G.o[i];
            if (!pl->alive || pl->type != OT_PPLATE) continue;
            int pressed = 0;
            for (int k = 0; k < G.nobjs && !pressed; k++) {                                       /* 4971: resting q14 block */
                const obj_t *b = &G.o[k];
                if (!b->alive || (b->type != OT_PBLOCK1 && b->type != OT_PBLOCK2) || !(b->fl & 0x10)) continue;
                int x1, y1, x2, y2; pblock_box(b, &x1, &y1, &x2, &y2);
                pressed = obj_rect_hit(pl, x1, y1, x2, y2 + PB_PLATE_REACH);
            }
            if (p->ground && sensor_on_box(pl, SENS_BOTTOMBIG)) pressed = 1;                     /* 4973 */
            for (int k = 0; k < G.nobjs && !pressed; k++)                                         /* 4972: a parked bike holds it */
                if (G.o[k].alive && G.o[k].type == OT_PBIKE && obj_obj_hit(&G.o[k], pl)) pressed = 1;
            pl->hidden = pressed;                       /* 5182 shows the plate each frame; 4971/4973 hide it while pressed */
            if (pressed) pressed_p = 1;
        }
        if (pressed_p && !SW.dv_click) { SW.dv_click = 1; snd_play_orig(49, 220, 128); }         /* 5183 */
    } else {
        int any_w = 0;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_PPLATE) continue;
            if (sensor_on_mask(o, SENS_BOTTOMBIG)) any_touch = 1;
            if (aqua_worm_on_plate(o)) any_w = 1;
        }
        any_touch |= any_w;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_PPLATE) continue;
            if (!any_touch) { o->hidden = 0; o->fl &= ~1; }                                         /* 4093 */
            if (p->ground && (sensor_on_mask(o, SENS_BOTTOMBIG) || aqua_worm_on_plate(o))) { o->hidden = 1; o->fl |= 1; }   /* 4094 / 4095 */
            if (o->fl & 1) pressed_p = 1;
        }
        if (pressed_p && !SW.at_click) snd_play_orig(49, 220, 128);                                 /* 4096 once per press */
        SW.at_click = pressed_p;
    }
    for (int i = 0; i < G.nobjs; i++) {                                                             /* 4362: the Milla block */
        obj_t *o = &G.o[i]; if (!o->alive || o->type != OT_MBLOCK) continue;
        if (p->ground && player_cube_hits(o)) { o->hidden = 1; o->fl |= 1; }
        if (o->fl & 1) pressed_m = 1;
    }
    if (pressed_m && !SW.mblock_click) { SW.mblock_click = 1; snd_play_orig(49, 220, 128); }       /* 4363 */
    int pressed = pressed_p || pressed_m, door1 = 1;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *d = &G.o[i]; if (!d->alive) continue;
        switch (d->type) {
        case OT_DOOR: {                                                                             /* 5184-5186 */
            if (pressed && d->c < 128) { d->y += 8; d->c += 8; }          /* RV plate3 (DV2): sinks 8 px/frame (top stays in view) */
            int under = p->x >= d->x - 15 && p->x <= d->x + 47 && p->y - ORIG_REF_DY > d->y && p->y - ORIG_REF_DY <= d->y + 128 + 31;
            int rise = L->rmg ? L->rmg->door_rise : 0;                                              /* RM4 6431 (level data): 8 px, no under-check */
            if (!pressed && d->c > 0 && rise) { int r = d->c < rise ? d->c : rise; d->y -= r; d->c -= r; }
            else if (!pressed && d->c > 0 && !under) { d->y -= 2; d->c -= 2; }                      /* rises back 2 px/frame */
            if (L->door_skip_ach && door1) {
                /* "Lilac the Switch Skipper" (session 16, build/re/spec_achievements.md 4.1). RE 5098: with G4 < 2, the first frame
                 * that MV.X is in (door x + 32, door x + 128) and MV.Y below the door's top sets E = 1, then E counts to 10; RE 5099:
                 * 0 < E < 10 while the plate is not pressed -> records x2 y18 (no character test, RE). INFERRED: the layer x / y
                 * offsets are 0, the counter keeps running once started (else-branch of 5098), and the reading itself (the door
                 * top is above her only while it is (nearly) closed, so walking through the opened door does not count). */
                door1 = 0;
                double mx = p->x, my = p->y - ORIG_REF_DY;
                if (G.at_g4 < 2 && mx > d->x + 32 && mx < d->x + 128 && my > d->y) { if (SW.skipE == 0) SW.skipE = 1; else if (SW.skipE < 10) SW.skipE++; }
                else if (SW.skipE > 0 && SW.skipE < 10) SW.skipE++;
                if (SW.skipE > 0 && SW.skipE < 10 && !pressed_p && !SW.skip_once) { SW.skip_once = 1; ach_earn(L->door_skip_ach); }
            }
            break; }
        case OT_SDOOR:                                                                              /* 4097 / 4364: layer 2, not solid */
            if ((pressed && d->c < 224) || (d->c > 0 && d->c < 224)) { d->c += 4; d->x -= 4; d->layer = 1; d->fl |= 1; }
            break;
        case OT_SDOOR2: {                                                                           /* 4098-4100: the AT1 gate */
            if (pressed_p && d->c < 128) { d->y += 4; d->c += 4; d->layer = 1; d->fl |= 1; }
            else if (!pressed_p && !any_touch && d->c > 0) { d->y -= 4; d->c -= 4; }
            int closed = d->c <= 0;
            if (closed && !SW.gate_closed) { d->layer = 2; d->fl &= ~1; }
            SW.gate_closed = closed;
            break; }
        }
    }
}
