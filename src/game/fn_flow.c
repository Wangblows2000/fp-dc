/* Fortune Night stage script: the flow events that belong to no object family (session 17). Source: build/re/spec_fn_flow.md
 * (static RE of handle_frame_30..35_events, with the RV probes of its section 8) and the corrections of spec_fn_bosses.md Y.
 * Labels: RE read from code, RV measured in the original, INFERRED reasoned (marked).
 * The generic parts are level data (src/game/levels.c rows fn1-fn6: checkpoints, exits, walk-ins, start rule, death target
 * with a forced g4, the scripted exit flag, the fixed camera box, class sets per character, music slots, results voice /
 * wipe); this file holds what is genuinely FN's own, attached through the stage index (3), never through a level name:
 *  - FN3's end, the Serpentine pass (7710-7725, group 0x2266a): the generic scripted exit (game.c, level_def_t.script_exit)
 *    starts the walk at X > W - 50; here NPCSerpentine appears 500 px behind the player, crashes in, dashes, brakes,
 *    taunts and dashes again; the fade starts once he has passed the player (7723 / 7724 = the shared fade, game.c) and
 *    the act changes at 255 (7725: g4 -1, FN4).
 *  - FN5 8371: parallax piece 1_1898 shown again while the view's left edge is < 11400 (8370 hides 1898 / 1899 when the
 *    panther enters the view: fn_flow_panther_view, called by the boss script).
 * Hooks for the boss module (spec_fn_bosses; types 800-899): level_lock_markers() / level_release_camera() (CameraLimitBoss
 * markers, moved for Carol / Milla in FN3 by level data), level_music_slot(n) + music_play / music_fade, player_bike_off(p,
 * park) (FN5 8383, FN6 8763), results_begin_boss() (FN6 8785; the row adds the group voice 8545 and the wipe 8546-8549),
 * G.cam_nD / G.cam_nF (FN5 8424 / 8475), par_set_drift (FN6 8762), fn_flow_panther_view (FN5 8370). */
#include "../fp.h"
#include <math.h>
#include <string.h>

/* NPCSerpentine_945 animations used by the pass (RE 7715 force_animation 9, 7719 15, 7720 11; anims.json) */
enum { SA_IDLE, SA_DASH, SA_BRAKE, SA_TAUNT, SA_N };
static const char *const SAN[SA_N] = { "fnserp_idle", "fnserp_dash", "fnserp_brake", "fnserp_taunt" };

static struct {
    int act;                     /* 1-6 in Fortune Night (stage index 3), else 0 */
    int pass;                    /* 7710 fired: group 0x2266a on */
    int serp;                    /* slot of the NPC (-1 none) */
    double B, C, F, G, J;        /* NPCSerpentine alterables (7710-7722) */
    int fl4;                     /* its flag 0x4: the first dash braked (7718) */
    int fade_once;               /* 7723's once guard */
    int hud;                     /* loop_gui_shiftup_14 count (7712) */
    int sa[SA_N];
} FN;

void fn_flow_spawn(void) {
    memset(&FN, 0, sizeof FN);
    FN.serp = -1;
    const level_def_t *L = level_cur();
    FN.act = L->stage == 3 ? L->act : 0;
    for (int k = 0; k < SA_N; k++) FN.sa[k] = FN.act == 3 ? ens_anim(SAN[k]) : -1;
}
float fn_flow_hud_dy(void) { return -(float)FN.hud; }
void fn_flow_panther_view(void) { par_set_hidden(1898, 1); par_set_hidden(1899, 1); }   /* 8370: pieces 1 and 2 hidden */

static obj_t *serp(void) { return FN.serp >= 0 && G.o[FN.serp].alive && G.o[FN.serp].type == OT_FN_SERPNPC ? &G.o[FN.serp] : NULL; }
static void serp_anim(obj_t *o, int k) { if (FN.sa[k] >= 0) anim_set(&o->an, &G.ens, FN.sa[k]); }   /* force_animation: restarts only on a change */

/* 7710-7725, in the original's order (each event sees the values the previous ones wrote this frame) */
static void serp_pass(player_t *p) {
    if (!FN.pass) {                                            /* 7710 (once, X > W - 50, no B test): game.c set the exit walk */
        if (!G.exit_walk) return;
        FN.pass = 1;
        obj_t *o = obj_alloc();
        if (!o) return;
        FN.serp = (int)(o - G.o);
        o->type = OT_FN_SERPNPC; o->alive = 1; o->hp = 1; o->layer = 2;          /* set_layer(3): the play layer */
        o->x = o->ox = (float)((int)p->x - 500); o->y = o->oy = 196;             /* (Player.x - 500, 196) */
        o->dir = 0; o->flip = 0;                                                  /* direction 0 (right) */
        serp_anim(o, SA_IDLE);
        FN.B = -1; FN.G = 0; FN.F = 1; FN.J = 196; FN.fl4 = 0;
    }
    obj_t *s = serp();
    if (G.character == CHR_CAROL_BIKE) player_bike_off(p, 0);                     /* 7711: MV.A 3 -> 2 */
    if (!s) return;
    if (FN.B == -1) { FN.G += 1; FN.hud = FN.hud + 2 > 80 ? 80 : FN.hud + 2; }   /* 7712: G + 1, loop_gui_shiftup_14 x2 */
    if (FN.B == -1 && FN.G == 1) {                                                /* 7713: the crash in */
        FN.G = 2; snd_play_orig(17, 230, 128); snd_play_orig(134, 230, 128); G.cam.Z = 16;
    }
    if (FN.B == -1 && FN.G > 50) { FN.G = 0; FN.B = 0; }                          /* 7714 */
    if (FN.B == 0) serp_anim(s, SA_DASH);                                         /* 7715: animation 9 */
    if (FN.B == 0 && s->an.b && s->an.frame > 3) {                                /* 7716: its frame > 3 */
        snd_play_orig(61, 230, 128); obj_effect(s->x, s->y, "wburst");           /* white burst behind him */
        FN.C = 10 * FN.F; FN.G = 0; FN.B += 1;
    }
    if (FN.B >= 1) { s->x = (float)(int)(s->x + FN.C); FN.G += 1; s->y = (float)(int)FN.J; }   /* 7717: x += C (set_x truncates), y = J */
    if (FN.B == 1 && FN.G > 15 && FN.F == 1 && (int)s->x >= (int)p->x - 299 && !FN.fl4) {   /* 7718: the first dash brakes */
        FN.G = 0; FN.B = 2; snd_play_orig(6, 230, 128); FN.fl4 = 1;
    }
    if (FN.B == 2) { serp_anim(s, SA_BRAKE); FN.C = FN.C - 0.5 > 0 ? FN.C - 0.5 : 0; }   /* 7719: animation 15 */
    if (FN.B == 2 && FN.C == 0) { FN.B = 3; serp_anim(s, SA_TAUNT); }             /* 7720: animation 11 */
    if (FN.B == 3) FN.G += 1;                                                     /* 7721 (7717 counts too: G + 2 a frame) */
    if (FN.B == 3 && FN.G > 220) FN.B = 0;                                        /* 7722: the second dash (flag 0x4 set: no brake) */
    if ((int)p->x < (int)s->x) {                                                  /* 7723 / 7724 / 7725 */
        if (!FN.fade_once) { FN.fade_once = 1; p->face = 1; if (!G.clear_t) G.clear_t = 1; }   /* LevelCardFade2 at alpha 0, skins right, MV.Q 1 */
    }
    if (s->x > G.lvl.w + 1000) s->alive = 0;                                      /* INFERRED: gone far past the frame (it never stops) */
}

void fn_flow_frame(player_t *p) {
    if (!FN.act) return;
    if (FN.act == 3) serp_pass(p);
    if (FN.act == 5 && par_hidden(1898) && G.camx < 11400) par_set_hidden(1898, 0);   /* 8371 */
}
