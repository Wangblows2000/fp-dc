/* Sky Battalion stage script (session 19): the flow events that belong to no enemy / gimmick / boss family. Source:
 * build/re/spec_sb_flow.md (static RE of handle_frame_36..39_events, on_frame_36..39_start_events; RV screenshots of the
 * original). Labels: RE read from code, RV seen in the original, INFERRED reasoned (marked).
 * Structure (RE): SB1 (json 36, frame index 35) is Zao's airship, the stage hub. Its three launch pads (SwitchPlatform_2100)
 * sink under the player; a fully sunk pad fades the screen and sends her to its ship (8874-8878: pad C 0 -> SB2 red ship,
 * 1 -> SB3, 2 -> SB4, g4 -1). Each ship ends at its right edge back to SB1 (9455-9458), where she flies in from the left
 * (8882 / 8883). A ship's boss sets its MasterLevel flag (0x10 / 0x20 / 0x40, 9597 / 9925 / 10112): back in SB1 that pad
 * stays down and its name sign is hidden (9220-9222). With the three flags, walking past X 1200 on SB1's deck starts the
 * night (9005-9008) and the Kujacker (9009 -> the boss module); the stage results run in SB1 (4561-4586, 5394, 8873 wipe).
 * The generic parts are level data (levels.c rows sb1-sb4: cards, checkpoints, exits to SB1, the SB1 entry, music slots,
 * the free launch fade); this file holds what is SB's own, attached through the stage index (4), never a level name:
 *  - SB1: pads (8866-8872) and their masks, the launch (8874-8878), completed ships (9220-9222), the ship name signs, the
 *    enemy airships on the 0.25 layer (9227 directions), Zao's flags, the night sky (LevelDarkSky_2114, 9005-9009);
 *  - SB2-SB4: the opening with Zao's airship (group 0x228f3, 9460-9470 / 9868-9875): fade in, every 20 frames one of the
 *    four planks is fired from the ship (explosion, sound 99), then the hover platforms are released (flag 0x800, the
 *    gimmick module through sb_flow_intro()), the timer and HUD come back and control returns (g4 1);
 *  - the boss camera locks of SB2 / SB3 / SB4 (9552 / 9554, 9878 / 9879, 10078 / 10079) on the CameraLimitBoss markers.
 * Hooks for the boss / gimmick modules: sb_flow_ship_done(flag) (a ship boss is beaten: flag, camera, music),
 * sb_flow_night() (2 = 9009 fired: start the Kujacker), sb_flow_intro() (hover platforms held / released),
 * sb_flow_pad_down(i) (a SwitchPlatform fully sunk: SB2 / SB4 doors), results_begin_boss() for the Kujacker's end
 * (the sb1 row has the wipe, 8873-8881). */
#include "../fp.h"
#include <math.h>
#include <string.h>
#ifdef FP_HOST_ENV
#include <stdio.h>
#include <stdlib.h>
#define SBLOG(...) do { if (getenv("FP_TRACE_SND")) { printf("sbflow "); printf(__VA_ARGS__); printf(" @%d\n", G.frame); } } while (0)   /* host tests */
#else
#define SBLOG(...) do { } while (0)
#endif

#define NPAD 4
#define NBG 4
static struct {
    int act;                         /* 1-4 in Sky Battalion (stage index 4), else 0 */
    int pad[NPAD], mask[NPAD], npad; /* SwitchPlatform slots by index C (newest first, 9216) and their masks */
    double padA[NPAD], padB[NPAD];   /* base y A, sink B (0..24) */
    int padfl[NPAD];                 /* flags: 1 / 2 lowered, 4 fully pressed (8872) */
    int launch;                      /* 8874: the pad whose fade runs (-1 none) */
    int night;                       /* 9005 fired */
    int darkA, darkC; double darkB;  /* LevelDarkSky_2114 A, B (128 at creation), C */
    int kuj;                         /* 9009 fired: the Kujacker's group 0x22840 is on */
    int intro;                       /* group 0x228f3 (SB2-SB4 opening) */
    int zA, zB;                      /* LevelParallaxZaoShip A / B */
    int bg[NBG], nbg; double bgB[NBG]; int bgA[NBG];   /* LevelSBBGPlatform: plank index A (1-4), speed B */
    int hud;                         /* HUD shifted up (px) */
    int locked, lock_once;           /* boss camera (CAM.M 1) */
    int air[3], nair;                /* LevelAirship1 (SB1, layer 1 at 0.25) */
    int dark;                        /* LevelDarkSky slot */
} SB;

int sb_flow_act(void) { return SB.act; }
float sb_flow_hud_dy(void) { return -(float)SB.hud; }
int sb_flow_night(void) { return SB.kuj ? 2 : SB.night ? 1 : 0; }
int sb_flow_intro(void) { return !SB.intro ? 0 : SB.zA == 2 ? 2 : 1; }
int sb_flow_pad_down(int i) { return i >= 0 && i < SB.npad && (SB.padfl[i] & 4); }

static const char *SHIP_DIR[3] = { "sb2", "sb3", "sb4" };   /* 8876 / 8877 / 8878: frame 0x24 / 0x25 / 0x26 */

/* a ship's boss beaten (9597 chopper / 9698 Spade in SB2, 9925 satellite in SB3, 10112 SB4): MasterLevel flag, CAM.M 2 / U 0
 * (camera back to the player), play_music(5) (the stage track again). The boss module also removes its railings /
 * barriers and creates the crystals (its own events). */
void sb_flow_ship_done(int flag) {
    G.stage_flags |= flag; SBLOG("ship done %x", flag);
    SB.locked = 0;
    level_release_camera(0);
    const char *m = level_music_slot(5); if (m) music_play(m);
}

void sb_flow_spawn(void) {
    memset(&SB, 0, sizeof SB);
    SB.launch = -1; SB.dark = -1;
    const level_def_t *L = level_cur();
    SB.act = L->stage == 4 ? L->act : 0;
    if (!SB.act) return;
    /* pads and masks in json (creation) order; index C newest first (9216; the ship name signs over them agree, RE 9220-9222) */
    int pk = 0, mk = 0, n = 0;
    for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SB_SWPLAT) n++;
    for (int i = 0; i < G.nobjs; i++) {
        obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_SB_SWPLAT && pk < NPAD) { int c = n - 1 - pk++; SB.pad[c] = i; SB.padA[c] = o->y; o->hidden = 1; }   /* 9216: A = y */
        else if (o->type == OT_SB_SWMASK && mk < NPAD) { int c = n - 1 - mk++; SB.mask[c] = i; }
        else if (o->type == OT_SB_AIRSHIP && SB.nair < 3) { SB.air[SB.nair++] = i; o->hidden = 1; }   /* drawn on the 0.25 layer (sb_flow_draw) */
        else if (o->type == OT_SB_DARKSKY) { SB.dark = i; o->hidden = 1; }                            /* set_visible(0) (constructor) */
        else if (o->type == OT_SB_BGPLAT && SB.nbg < NBG) { SB.bg[SB.nbg++] = i; o->hidden = 1; }     /* shown when fired (9466) */
    }
    SB.npad = n < NPAD ? n : NPAD;
    for (int k = 0; k < SB.npad; k++) SB.padB[k] = 0;
    if (SB.act == 1) {
        par_destroy(2133); par_destroy(2134);                       /* 9211: the night pieces go unless g4 = 99 (Adventure) */
        for (int k = 0; k < 3 && k < SB.npad; k++) if (G.stage_flags & (0x10 << k)) {   /* 9220-9222: the ship is done */
            SB.padB[k] = 24; SB.padfl[k] |= 3;
            for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_SB_NAME1 + k) G.o[i].hidden = 1;
        }
        for (int k = 0; k < SB.npad; k++) { obj_t *o = &G.o[SB.pad[k]]; o->y = (float)(int)(SB.padA[k] + SB.padB[k]); }
        /* 9227: airship I = index (newest first) -> direction I */
        static const char *const AD[3] = { "sbairship0", "sbairship1", "sbairship2" };
        for (int k = 0; k < SB.nair; k++) { int a = ens_anim(AD[SB.nair - 1 - k]); if (a >= 0) anim_set(&G.o[SB.air[k]].an, &G.ens, a); }
        if (G.at_g4 < 0) G.p.vy = -5;                                /* 8882: flown in from the left, B 15, G -5 (the entry row) */
    } else if (G.at_g4 <= 0) {                                       /* 9789: an arrival from SB1 runs the opening */
        SB.intro = 1; SB.hud = 40;                                   /* 9460: HUD shifted up 40, timer off (intro()) */
        for (int k = 0; k < SB.nbg; k++) SB.bgA[k] = SB.nbg - k;    /* 9460: A = 1, 2, ... (newest first) */
    }
}

/* SwitchPlatform (8866-8872, every SB act; RE): the player standing on it on flat ground (O < 30 or > 330) sinks it by
 * 0.5 a frame to 24 (her Y follows, 8870 / 8871), sound 62; fully down: flags 2 + 4, sound 49 (8872); y = A + B (8866) */
static int on_pad(const obj_t *o, const player_t *p) {
    if (!p->ground || !(p->O < 30 || p->O > 330)) return 0;
    double X = p->x, Y = p->y - ORIG_REF_DY;
    return X >= o->x - 24 - 6 && X <= o->x + 24 + 6 && Y + 14 >= o->y - 4 && Y + 14 <= o->y + 8;   /* the bottom sensor on its top (INFERRED box) */
}
static void pads(player_t *p) {
    for (int k = 0; k < SB.npad; k++) {
        obj_t *o = &G.o[SB.pad[k]];
        if (!o->alive) continue;
        if (SB.padB[k] < 24 && on_pad(o, p)) {
            if (SB.padB[k] == 0) snd_play_orig(62, 200, 128);
            double ny = (int)(SB.padA[k] + fmin(SB.padB[k] + 0.5, 24)), dy = ny - o->y;
            SB.padB[k] = fmin(SB.padB[k] + 0.5, 24); o->y = (float)ny; p->y += dy;
            if (SB.padB[k] < 24) sb_gim_door_rise(k);              /* 9341 / 9342, 9832 / 9833: its SwitchDoor (A = C) rises 2 (sb_gim.c) */
        }
        if (!(SB.padfl[k] & 2) && SB.padB[k] >= 24) { SB.padfl[k] |= 2 | 4; snd_play_orig(49, 220, 128); SBLOG("pad %d down", k); }
        o->y = (float)(int)(SB.padA[k] + SB.padB[k]);
        if (SB.mask[k] >= 0 && G.o[SB.mask[k]].alive && G.o[SB.mask[k]].type == OT_SB_SWMASK) { G.o[SB.mask[k]].x = o->x; G.o[SB.mask[k]].y = o->y; }
    }
}

/* SB1 (frame index 35) */
static void hub(player_t *p) {
    pads(p);
    /* 8874-8878: a pressed pad (flag 4) starts the fade; at 255 the ship (g4 -1). The player keeps control (no CTRL.M write,
     * no 3311 in frame 36: the row's clear_free) */
    if (SB.launch < 0 && !G.clear_t)
        for (int k = 0; k < 3 && k < SB.npad; k++) if ((SB.padfl[k] & 4) && !(G.stage_flags & (0x10 << k))) {
            SB.launch = k; G.clear_t = 1; G.exit_to = SHIP_DIR[k]; G.exit_b = 0; SBLOG("launch %d %s", k, SHIP_DIR[k]); break;
        }
    /* 9005: grounded past X 1200 with the three ships done: the night falls (darksky shown, A 1), music fades over 4 s,
     * the timer and the ships' fire stop */
    if (!SB.night && p->ground && p->x > 1200 && (G.stage_flags & 0x70) == 0x70) {
        SB.night = 1; SB.darkA = 1; SB.darkB = 128; SBLOG("night");
        music_fade(240, 0);
        G.timer_stop = G.frame + 1;
    }
    if (SB.darkA == 1) { SB.darkB -= 0.5; SB.hud = SB.hud + 1 > 40 ? 40 : SB.hud + 1; }   /* 9006: B - 0.5, alpha (128 - B) 2, HUD up */
    if (SB.darkA == 1 && SB.darkB <= 64) SB.darkA = 2;                                    /* 9007 */
    if (SB.darkA == 2 && !SB.kuj) SB.darkC++;                                              /* 9008 (CAM.H to -50: not ported) */
    if (SB.darkA == 2 && SB.darkC == 50 && !SB.kuj) { SB.kuj = 1; G.timer_stop = 0; SBLOG("kujacker"); }   /* 9009: group 0x22840 (boss module), timer on */
}

/* SB2-SB4 opening (group 0x228f3) */
static void intro(void) {
    if (!SB.intro) return;
    if (SB.zA < 2 && !G.timer_stop) G.timer_stop = G.frame + 1;     /* 9460: timer group off (level_start clears timer_stop after the spawn) */
    if (!G.act_in && SB.zA == 0) SB.zA = 1;                         /* 9463 / 9868: the fade (32 frames) is over */
    if (SB.zA == 1 && G.frame % 20 == 0) SB.zB++;                   /* 9465 / 9870: g3 mod 20 (the port's g3: the stage frame) */
    for (int k = 0; k < SB.nbg; k++) {
        obj_t *o = &G.o[SB.bg[k]];
        if (SB.zA == 1 && SB.bgA[k] == SB.zB && SB.bgB[k] == 0) {   /* 9466 / 9871: fired */
            o->hidden = 0; obj_effect(o->x, o->y, "explosion01"); snd_play_orig(99, 220, 128); SB.bgB[k] = -8; SBLOG("plank %d", k);
        }
        o->x += (float)SB.bgB[k];                                    /* 9464 / 9869: x + B (y = the ship's y + 242) */
    }
    if (SB.zA == 1 && SB.zB > 7) { SB.zA = 2; SBLOG("release"); }                         /* 9467 / 9872: the hover platforms go (sb_flow_intro 2) */
    if (SB.zA == 2) { G.timer_stop = 0; SB.hud = SB.hud - 2 < 0 ? 0 : SB.hud - 2; }   /* 9874: timer on, HUD down */
    if (SB.zA == 2 && SB.hud == 0) { SB.intro = 0; G.at_g4 = 1; SBLOG("intro end"); }    /* 9470 / 9875: control back, g4 1 */
}

/* boss camera (CAM.M 1 on the CameraLimitBoss markers while a railing / barrier of the arena exists) */
static void boss_lock(const player_t *p) {
    float l = -1e9f, r = 1e9f, b = 1e9f; int gates = 0;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_CAMLIM_L) l = o->x; else if (o->type == OT_CAMLIM_R) r = o->x; else if (o->type == OT_CAMLIM_B) b = o->y;
        else if (o->type == OT_WR_RAIL || o->type == OT_WR_BARRIER) gates++;
    }
    if (l < -1e8f) return;
    double X = (int)p->x, Y = (int)(p->y - ORIG_REF_DY);
    static const int LOCK_DX[5] = { 0, 0, 600, 200, 80 };           /* SB2 9552 (+600), SB3 9878 (+200), SB4 10078 (+0x50) */
    if (!SB.locked && !SB.lock_once && gates && X > l + LOCK_DX[SB.act] && X < r && Y < b) {
        SB.locked = 1; if (SB.act != 3) SB.lock_once = 1;            /* SB2 / SB4: once (0x2290c, 0x229d0); SB3 again after leaving */
        level_lock_markers(); SBLOG("lock");
    } else if (SB.locked && gates && SB.act >= 3 && X <= l + (SB.act == 3 ? 39 : -1) && G.camx > l - 100) {   /* 9879 / 10079: back out left */
        SB.locked = 0; level_release_camera(0);
    } else if (SB.locked && !gates) {                                 /* 9554 (SB2): the barriers are gone */
        SB.locked = 0; level_release_camera(0);
    }
}

void sb_flow_frame(player_t *p) {
    if (!SB.act) return;
    if (SB.act == 1) hub(p);
    else { pads(p); intro(); boss_lock(p); }
}

/* drawing: the airships (SB1 layer 1 scrolls at 0.25, Layer::init) and the opening planks (SB2-SB4 layer 1 at 0.8) before
 * the play layer; the night sky over them (LevelDarkSky_2114 image 6058 = RGB (8, 0, 0), alpha (128 - B) 2, 9006) */
static void draw_scaled_layer(const obj_t *o, double k) {
    if (!o->an.b) return;
    float sx = floorf(o->x - (float)trunc(k * G.camx)), sy = floorf(o->y - (float)trunc(k * G.camy));
    frame_draw(o->an.b, anim_frame(&o->an), sx, sy, o->flip, 0xffffffff);
}
void sb_flow_draw(int pass) {
    if (!SB.act) return;
    if (pass == -1) {                                /* json layer 1, behind the json-2 pieces (game_draw, session 20) */
        for (int k = 0; k < SB.nair; k++) if (G.o[SB.air[k]].alive) draw_scaled_layer(&G.o[SB.air[k]], 0.25);
        for (int k = 0; k < SB.nbg; k++) { const obj_t *o = &G.o[SB.bg[k]]; if (o->alive && !o->hidden) draw_scaled_layer(o, 0.8); }
        return;
    }
    if (pass != 0) return;
    if (SB.night) {
        int a = (int)((128 - SB.darkB) * 2); if (a > 255) a = 255; if (a < 0) a = 0;
        if (a) r_rect(0, 0, SCREEN_W, SCREEN_H, ((uint32_t)a << 24) | 0x080000);
    }
}
