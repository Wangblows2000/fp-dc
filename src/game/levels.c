/* Level definitions (session 13): one row per ported act, so the shared code (act end and transition, results, stage
 * music, water, Time Attack / save records, walk-in) reads level DATA instead of testing level names. Adding an act of
 * an already-ported mechanic = one row here + the importer run (ASSET_PIPELINE "Adding a level"); only genuinely new
 * behaviour needs new code. Values are the original's (labels per column below). */
#include "../fp.h"
#include <math.h>
#include <string.h>

/* Designated fields: anything left out is 0 / NULL (dry, no edge results, ...). Labels: the event numbers are the
 * original's (RE); see RE_NOTES "Level table". */
/* Carol's jump pads (spec_carol_bike C2): x, y, index A, angle B, chain end D, lit E, block pad (DV4 anim 7) */
static const padcfg_t PADS_DV2[] = { {2736,1144,0,0,0,1,0}, {2936,992,1,90,0,0,0}, {2912,688,2,180,0,0,0}, {3176,688,3,180,1,0,0},
    {3400,1416,4,0,0,1,0}, {3400,1224,5,180,0,0,0}, {3728,1224,6,180,0,0,0}, {4032,1224,7,180,1,0,0} };
static const padcfg_t PADS_DV3[] = { {7408,2200,0,90,0,1,0}, {7072,1952,1,270,0,0,0}, {7344,1952,2,90,1,0,0}, {680,1776,3,0,0,1,0},
    {744,1528,4,90,0,0,0}, {256,1328,5,270,0,0,0}, {520,1064,6,0,0,0,0}, {776,1064,7,0,0,0,0}, {128,1704,8,270,0,1,0}, {384,1776,9,0,1,0,0},
    {4736,2104,10,0,0,1,0}, {4904,1920,11,180,0,0,0}, {5136,2104,12,0,0,0,0}, {5408,1864,13,180,0,0,0}, {5664,1712,14,180,1,0,0} };
static const padcfg_t PADS_DV4[] = { {4040,2296,0,0,0,1,1}, {4368,2152,1,180,0,0,0}, {4616,2160,2,0,0,0,0}, {4864,2168,3,180,0,0,0},
    {5136,2152,4,180,0,0,0}, {5296,2152,5,0,0,0,0}, {5504,2176,6,90,0,0,0}, {5344,2096,7,270,0,0,0}, {5344,2000,8,270,1,0,0},
    {10120,2134,9,-4.5f,0,1,1}, {10400,2024,10,180,0,0,0}, {10432,1992,11,270,1,0,0} };
/* Milla's dig spot rewards by spot index A (AT1 4189-4194; spot 3 has no event) */
static const signed char DIGR_AT1[] = { DIG_KEYSTONE_MON, DIG_CRYSTALS, DIG_CRYSTALS, DIG_NONE, DIG_PETAL, DIG_CRYSTALS, DIG_WOOD_MON };
/* Milla's dig zones (spec_milla_moves 6, events 4171-4182 of frame 17) */
static const zone_t DIGZ_AT1[] = {
    { 6939, 7302, 102, 185 }, { 1479, 1705, 594, 671 }, { 9450, 9654, 181, 262 }, { 3214, 3315, 838, 904 },
    { 8866, 9270, 715, 794 }, { 8197, 8418, 442, 507 }, { 9247, 10206, 611, 729 }, { 10607, 10744, 656, 711 },
    { 10523, 10563, 659, 713 }, { 10413, 11262, 1052, 1154 }, { 11671, 12410, 822, 903 }, { 10388, 10482, 157, 223 } };
/* Aqua Tunnel stage flow (spec_at_flow 3 / 6): checkpoints (4196 / 4197, 4405, 4598), boss camera locks (4198 / 4199,
 * 4408 / 4409, 4599) and the act-start item sets (4318 / 4319 crystals per character, 4484 / 4485) */
static const cpzone_t CP_AT1[] = { { 7540, 7640, 2, 7600, 1100, 0, 0 }, { 11250, 11350, 3, 11300, 988, 0, 0 } };
static const cpzone_t CP_AT2[] = { { 5640, 5740, 3, 5720, 270, 0, 0 } };
static const cpzone_t CP_AT4[] = { { 3264 - 0, 3364, 2, 3264, 200, 0, 0 } };
/*                                g4 max, lock X, Y min,  Y max,  l,     t,   r,     b,    unlock X, unlock Y */
static const camzone_t CAM_AT1 = { 0,  11704, -32768, 903,    11368, 600, 12479, 903,  11704 - 240, 903 };
static const camzone_t CAM_AT2 = { 10, 6032,  976,    32767,  6032,  976, 6863,  1231, 32767,  976 };
static const camzone_t CAM_AT4 = { 10, 6464,  -32768, 32767,  6464,  8,   7039,  319,  -32768, -32768 };
static const killbox_t KILL_AT1[] = { { 7579, 7788, 732, 1129, 1, 0 }, { 7790, 8050, 929, 1133, 2, 0 } };
static const killbox_t KILL_AT2[] = { { 5260, 5467, 379, 814, 0, 1 } };
/* Aqua Tunnel's sound group (D-047: water, jets, slimes, bombs, bosses) */
/* Relic Maze flow (RE spec_rm_flow 1-2, 6.1). RM1's right edge: the middle band (260 < MV.Y < 420, 5813, behind the barriers
 * opened by stage flags 0x2 / 0x4) goes on to RM3, any other height (5814) into the side area RM2, which leaves by its left
 * edge (5916) back to RM1's right edge (2967+: X 7400, Y 190 / 540 by flag 0x1, facing left, B -3 for 32 frames, 5827).
 * RM2 is entered at X 48 (5921) at y 381 or 1466 (5600 + start point 1450 when flag 0x1); RM3 at X 48 (B 3); RM5 at X 340. */
static const exit_t EX_RM1[] = { { 1, 2, -50, 260, 420, "rm3" }, { 1, 2, -50, 0, 0, "rm2" } };
static const exit_t EX_RM2[] = { { -1, -2, 80, 0, 0, "rm1" } };
static const exit_t EX_RM3[] = { { 1, 0, 20, 0, 0, "rm4" } };
static const entry_t EN_RM1[] = { { "rm2", 7400, 190, 350, -1, -3 } };
static const entry_t EN_RM2[] = { { NULL, 48, 381, 1466 - 381, 1, 0 } };
static const entry_t EN_RM3[] = { { NULL, 48, 509, 0, 1, 3 } };
static const entry_t EN_RM5[] = { { NULL, 340, 549, 0, 1, 0 } };
static const cpzone_t CP_RM1[] = { { 6777, 7372, 2, 6360, 256, 0, 0 } };
static const cpzone_t CP_RM2[] = { { 100, 32767, 2, 6360, 256, 0, 1 } };       /* 5924: RM1's checkpoint, no snapshot */
static const cpzone_t CP_RM3[] = { { 7400, 8360, 10, 8352, 2669, 2000, 0 } };     /* 6296 -> loop_next_act_26 (Y > 2000) */
static const cpzone_t CP_RM4[] = { { 4330, 5330, 2, 4330, 830, 0, 0 } };
static const killbox_t KILL_RM2[] = { { 2915, 3204, 281, 584, 3, 0 }, { 3219, 3265, 327, 621, 4, 0 } };   /* 6013 / 6014 */
/* RM1 / RM2 gimmicks (spec_rm_gimmicks_a): dig zones 5820 and rewards 5821-5824 (spot A: newest first), RM2 jump pads
 * (5169, 4951, 5171, 5172, 5892, 5893), the stage-flag gates (10), the start clean-ups 6039 / 6040 and the basement
 * break-out zone 5925 */
static const zone_t DIGZ_RM1[] = { { -12, 1186, 966, 1173 }, { 3455, 4489, 692, 1009 }, { 4416, 5362, 523, 840 }, { 2305, 2357, 1091, 1149 },
                                   { 1207, 1620, 1109, 1226 }, { 6539, 6653, 487, 620 } };
static const signed char DIGR_RM1[] = { DIG_EARTH_MON, DIG_CRYSTALS, DIG_OPEN_HOLE, DIG_KEYSTONE_MON };
static const padcfg_t PADS_RM2[] = { {3264,432,0,90,0,1,0}, {3264,224,1,90,0,0,0}, {3264,32,2,90,1,0,0}, {800,608,3,0,0,1,0}, {664,384,4,270,0,0,0}, {992,40,5,180,1,0,0} };
/* RM3 jump pads (spread index A newest first: pad 0 = the last in the frame JSON). 0-7 on the Claw corridor floor (y 512:
 * Carol's scripted rides 6242-6245, src/game/rm_claw.c); 6096-6101: pad 8 D 1 B 270, pads with x > 10400 anim 7 (block),
 * pad 9 E 1, pad 11 B 270, pad 12 B 150, pad 13 D 1 B 270 (spec_rm_gimmicks_b 18) */
static const padcfg_t PADS_RM3[] = { {1120,512,0,0,0,0,0}, {1488,512,1,0,0,0,0}, {1856,512,2,0,0,0,0}, {2224,512,3,0,0,0,0},
    {2592,512,4,0,0,0,0}, {2960,512,5,0,0,0,0}, {3336,512,6,0,0,0,0}, {3656,512,7,0,0,0,0}, {3800,320,8,270,1,0,0},
    {11256,2984,9,0,0,1,1}, {11080,2840,10,0,0,0,1}, {10840,3000,11,270,0,0,1}, {10944,2792,12,150,0,0,1}, {10576,2872,13,270,1,0,1} };
static const gate_t GATES_RM1[] = { { 0x2, OT_WR_RAIL, 0, 0, 0, 0, 0 }, { 0x4, OT_RAIL2, 0, 0, 0, 0, 0 } };   /* 5832 / 5833 */
static const gate_t GATES_RM2[] = { { 0x2, OT_WR_RAIL, 116, 336, -32768, 1000, 1 }, { 0x4, OT_RAIL2, 116, 1424, 1000, 32767, 1 },   /* 5922 / 5923 */
                                    { 0x4, OT_RAIL2, 0, 0, 0, 0, 0 } };                                                      /* 5926 */
static const flagclear_t CLEAR_RM2[] = { { 0x2, 0, 3499, 0, 1199 }, { 0x4, 22, 1937, 1600, 3189 } };                        /* 6039 / 6040 */
static const zone_t BREAKOUT_RM2 = { 587, 824, 2328, 2566 };
static const camroom_t ROOMS_RM5[] = { { -32768, 900, 840, 1096, 768, 1480, -1 }, { -32768, 32767, 1095, 1608, 896, 1480, 1 },   /* 6745 / 6746, 6747 (a railing left) */
                                        { -32768, 32767, 1095, 1608, 1280, 1480, 0 }, { -32768, 32767, 1608, 32767, 1280, 2888, -1 } };   /* 6748, 6750 */
static const warp_t WARPS_RM5[] = { { -32768, 79, -32768, 1096, -1, 1024, 1816, -1, -3, -3, 704 },   /* 6731 + 6734 */
                                    { -32768, 79, 1095, 32767, -1, 704, 920, -1, -3, -3, 704 },      /* 6731 + 6735 */
                                    { 654, 900, 840, 1096, 1, 0, 1432, 1, 3, -2, 0 },                 /* 6740 (the fade trigger for a right exit INFERRED: B > 0) */
                                    { 974, 32767, 1608, 1864, 1, 0, 920, 1, 3, -2, 0 } };             /* 6743 */
static const short SFX_RM[] = { 54, 55, 59, 60, 62, 67, 68, 70, 73, 75, 77, 96, 99, 101, 110, 111, 135, 149, 213, 219, 260, 262, 306, 312, 426,   /* not in the global set (D-047): enemies, gimmicks, flow */
                                132, 134, 146, 148, 151, 152 };   /* RM5 Mantalith (rm_mantalith.c), 11025 Hz: 147 / 214 / 215 left out (sound RAM) */
/* Relic Maze acts 3-5 gimmicks (relic_gimb.c, spec_rm_gimmicks_b): moving-pillar zones (RE 6379-6385 / 6527-6531: box origin
 * in [x0, x1) x [y0, y1) -> travel C), jump pads (6096-6101 / 6429 / 6430 / 6570-6576; index = spread index, newest first;
 * RM3 pads 0-7 belong to the Claw chase, pads with x > 10400 show anim 7; RM4: no lit start pad, U8), door return, basement */
#define N_(a) (int)(sizeof a / sizeof a[0])
static const pillarzone_t PIL_RM3[] = { { 10811, 10905, 2245, 2736, -116 }, { 10905, 10955, 2248, 2739, -100 }, { 10953, 11003, 2246, 2737, -84 },
    { 11000, 11050, 2246, 2737, -68 }, { 11354, 11404, 2230, 2721, 136 }, { 11403, 11453, 2230, 2721, 88 }, { 11452, 11502, 2228, 2719, 40 } };
static const pillarzone_t PIL_RM4[] = { { 4411, 4458, 658, 894, 144 }, { 4459, 4506, 658, 894, 128 }, { 4508, 4555, 660, 896, 112 },
    { 4557, 4604, 659, 895, 96 }, { 4604, 4651, 661, 897, 80 } };
static const rmgim_t RMG_RM3 = { PIL_RM3, N_(PIL_RM3), 0, 1, 0 }, RMG_RM4 = { PIL_RM4, N_(PIL_RM4), 8, 1, 0 }, RMG_RM5 = { NULL, 0, 0, 1, 1608 };
static const padcfg_t PADS_RM4[] = { {368,632,0,0,0,0,0}, {496,392,1,0,0,0,0}, {560,632,2,0,1,0,0}, {1656,912,3,0,0,0,0}, {1472,688,4,270,1,0,0} };
/* Fortune Night pads (ObjectJumpPad_1529; spec_fn_gimmicks_a 16: A = spread newest first, RE 4950 / 7186-7196 (FN1), 4950 / 4951 / 7649 /
 * 4954 / 7650-7654 / 5365 / 7655-7663 (FN3)); FN2's only pad is an off-map template (inert) */
/* Sky Battalion pads (ObjectJumpPad_539; A = 3564's spread index, newest first, the off-map template (-104, 576) last; RE session 19b):
 * SB2 9309 (A 0: E 1, B 0), 5357 (A 1: B 180), 9310 (A 2: B 180, D 1); SB3 4950 (A 0: E 1), 5357, 9310, 5172 (A 3: E 1), 9816 (A 4: B 90,
 * D 1), 9817 (A 5: E 1), 9818 (A 6: B 270, D 1); SB4 5169 (A 0: E 1, B 90), 5170 (A 1: B 270), 9988 (A 2: B 90), 9989 (A 3: B 270, D 1) */
static const padcfg_t PADS_SB2[] = { {3984,656,0,0,0,1,0}, {4224,272,1,180,0,0,0}, {4496,272,2,180,1,0,0} };
static const padcfg_t PADS_SB3[] = { {7024,344,0,0,0,1,0}, {7200,176,1,180,0,0,0}, {7488,176,2,180,1,0,0}, {12000,1048,3,0,0,1,0},
    {12280,720,4,90,1,0,0}, {11664,1048,5,0,0,1,0}, {11392,720,6,270,1,0,0} };
static const padcfg_t PADS_SB4[] = { {9856,784,0,90,0,1,0}, {9744,600,1,270,0,0,0}, {10224,600,2,90,0,0,0}, {10112,784,3,270,1,0,0} };
static const padcfg_t PADS_FN1[] = { {3880,1328,0,0,0,1,0}, {3944,1008,1,90,1,0,0}, {3464,1032,2,0,0,1,0}, {3520,744,3,270,0,0,0}, {3584,440,4,270,1,0,0},
    {7528,1072,6,90,0,1,0}, {7208,952,7,180,1,0,0}, {10472,704,8,90,0,1,0}, {10472,416,9,90,0,0,0}, {10472,128,10,90,1,0,0},
    {7656,1168,11,270,0,1,0}, {7784,1016,12,270,0,0,0}, {7912,728,13,270,1,0,0} };
static const padcfg_t PADS_FN3[] = { {5568,784,0,0,0,1,0}, {5888,640,1,90,0,0,0}, {5632,640,2,270,1,0,0}, {7312,1088,4,0,0,1,0}, {7792,1088,5,0,1,0,0},
    {9504,800,6,0,0,1,0}, {9568,424,7,180,0,0,0}, {9984,464,8,90,0,0,0}, {9856,752,9,270,0,0,0}, {10176,712,10,180,0,0,0}, {10752,752,11,90,0,0,0},
    {10400,520,12,180,0,0,0}, {10496,800,13,0,0,0,0}, {10368,656,14,270,0,0,0}, {10720,616,15,180,0,0,0}, {11360,616,16,180,1,0,0},
    {10656,800,17,0,0,1,0}, {10656,616,18,180,0,0,0}, {11360,616,19,180,1,0,0} };
static const padcfg_t PADS_RM5[] = { {352,2664,0,0,0,0,0}, {624,2632,1,0,1,0,0}, {1176,520,2,270,0,1,0}, {1176,216,3,270,1,0,0} };

static const short SFX_AT[] = { 55, 62, 69, 99, 172, 173, 240, 242, 251, 252, 278, 306, 308, 316 };
/* Fortune Night flow (session 17, RE spec_fn_flow 1-3, 9; RV where the spec says so). Checkpoints (FN1 7366, FN2 / FN4 inline,
 * FN3 7818, FN5 8420 / 8421: the second one takes g4 to 3, so a restart there never re-arms the samurai 8422); FN4 / FN5 end
 * at X > W + 20 (8295, B > 0); FN3 ends by its Serpentine pass (fn_flow.c, 7710-7725); FN6 deaths go to FN5 with g4 2 */
static const cpzone_t CP_FN1[] = { { 7700, 11524, 2, 7700, 1500, 0, 0 } };
static const cpzone_t CP_FN2[] = { { 6520, 11524, 2, 6520, 490, 0, 0 } };
static const cpzone_t CP_FN3[] = { { 9800, 10000, 2, 9536, 360, 0, 0 } };
static const cpzone_t CP_FN4[] = { { 7670, 7770, 2, 7670, 840, 0, 0 } };
static const cpzone_t CP_FN5[] = { { 8430, 9430, 2, 8430, 608, 0, 0 }, { 10560, 11560, 3, 10880, 560, 0, 0 } };
static const exit_t EX_FN45[] = { { 1, 0, 20, 0, 0, NULL } };
/* FN3 start by character (8082 MV.A = 0 / 8083 MV.A != 0). The boss parts are the boss module's types (spec_fn_bosses Part X:
 * 821-825 Serpentine arm / hitbox / floor check / shield / charge glow, 840-843 samurai body / hands / wheel / floor probe);
 * Lilac also loses every MonitorBomb (FN3's 8 at y 336). Carol / Milla: the limit markers move around the samurai at
 * (11136, 496), R = 65: left 11136 - 240 - 65, right 11136 + 240 + 65, bottom 1424 (the top marker is at 0 already). */
static const classkill_t CK_FN3[] = { { 1, 840, 843 }, { 1, OT_MON_BOMB, OT_MON_BOMB }, { 2, 821, 825 } };
static const markmove_t CM_FN3[] = { { 2, 10831, -32768, 11441, 1424 } };
static const cambox_t BOX_FN6 = { 224, 0, 960, 311 };   /* 8553 / handle_frame_35 inline (RV at each frame start: C 224, D 0, E 960, F 311) */
/* the stage's own sounds (D-047), one group per act (session 17: one shared FN group was 420 KB; sound RAM needs >= 256 KB free
 * with the heaviest character, ASSET_PIPELINE). Ids from the modules' snd_play_orig calls and the act's object classes (json
 * frames 30-35): flow 134 (7713 Serpentine's crash; also the lamppost break) and 61 (7716; a Carol group id, needed by
 * Lilac / Milla); everywhere: 33 hang (Carol id), coil 164 / 371 / 167, lights 171; pigeons 243 (FN1-3), boost blocks 163
 * (FN1-2), teeter 143 (FN2-5), trolley 260 (FN2), gachapon 187 / 188 (FN3-5), fireworks / flares 430, Turretus 127 (FN2-5),
 * Milla cube reflect 219, shell 231 / gimmicks B 208-249 / 428 (FN4-5); bosses (fn_boss.c): UFO 168 / 169 (FN1, FN3 chase),
 * bulldozer 165 / 166 (FN1-2), Serpentine 62 / 70 / 75 / 191-200 / 317 (FN3). All 11025 Hz (convert_sfx.sh). */
static const short SFX_FN1[] = { 33, 134, 164, 371, 171, 167, 163, 430, 243, 165, 166, 168, 169 };
static const short SFX_FN2[] = { 33, 134, 164, 371, 171, 167, 143, 163, 260, 430, 243, 127, 219, 165, 166 };
static const short SFX_FN3[] = { 33, 134, 164, 371, 171, 167, 61, 143, 187, 188, 430, 243, 127, 219,
                                 62, 70, 75, 168, 169, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 317 };
static const short SFX_FN45[] = { 33, 134, 164, 371, 171, 167, 143, 187, 188, 430, 127, 219, 231,
                                  208, 212, 216, 217, 218, 223, 224, 225, 226, 227, 228, 229, 230, 249, 428,
                                  126, 193 };   /* + the FN5 Panther chase (fn_panther.c): 8413 white burst 126 (a Carol id), 8407 taunt 193 (242 KB) */
#define SFX_FN4 SFX_FN45
#define SFX_FN5 SFX_FN45
/* FN6: flow 134 + the Panther fight (fn_panther.c, spec_fn_bosses G.11): explosion 99, white burst 126, assist strike 137,
 * plasma 165 / 166 / 191 / 436, rapid fire 193 / 194, back cannon 195 / 254 / voices 258 (Lilac) / 259, missiles 169 / 200,
 * laser 256 / 257, defeat 261, Milla's platform 219 (228 KB at 11025 Hz) */
static const short SFX_FN6[] = { 134, 99, 126, 137, 165, 166, 169, 191, 193, 194, 195, 200, 219, 254, 256, 257, 258, 259, 261, 436, 241, 288 };   /* + 241 / 288: fn_assist.c (session 18) */
/* Fortune Night bosses (fn_boss.c, spec_fn_bosses C / E): FN3 beam chase (7794 window, 7797 / 7798 / 7810 zones, 7800 / 7801
 * floors at A >= 4, Floor2 pieces at MV.B) and samurai (7815 lock, 8040 / 8048 music, 8045 y 512, turrets 8047 / 8048); FN5
 * samurai (8422 g4 < 3, 8424 CAM.D 336, no music; 8473 turret; its 8045 copy's y is not in the spec: home y + 16, INFERRED) */
static const fnboss_t FNB_FN3 = { 2024, 2064, { { 2554, 3451, 553, 824 }, { 2559, 3458, 831, 1145 }, { 3454, 3844, 835, 1194 } }, 4, 1,
                                  1, 1, 0, 0, 512, { { 11550, 949, 24 }, { 11552, 852, 48 } } };
static const fnboss_t FNB_FN5 = { 0, 0, { { 0 } }, 5, 0, 0, 0, 336, 3, 192, { { 10931, 550, 48 }, { 0, 0, 0 } } };

#define DV(d, f, n, mus) .dir = d, .frame = f, .stage = 1, .act = n, .nacts = 4, .music = mus
#define AT(d, f, n, mus) .dir = d, .frame = f, .stage = 0, .act = n, .nacts = 4, .music = mus, .plate_rule = 1, .switch_late = 1, .monitor_hop = 1, .checkpoints = 1, .sfx = SFX_AT, .nsfx = N_(SFX_AT)
/* Fortune Night enemies (fn_enemy.c, spec_fn_enemies): FN1 7346 flips the coils below y 1500 and has no laser-wall handler
 * (no col_21_29 in test_collisions_29); FN2 has no flare-wall handler (col_24_30); FN5's balloon gives a life (8349) */
static const fnenemy_t FNE_FN1 = { 1500, 1, 0, 0 }, FNE_FN2 = { 0, 0, 1, 0 }, FNE_FN5 = { 0, 0, 0, 1 };
/* Fortune Night (session 17, spec_fn_flow 9.1): music slots 4 / 6 / 7 of the frame's loop table (tools/re/bgm_refs.py, 3) */
#define FN(d, f, n, mus) .dir = d, .frame = f, .stage = 3, .act = n, .nacts = 6, .music = mus, .checkpoints = 1, .sfx = SFX_FN##n, .nsfx = N_(SFX_FN##n)
#define FN_M123 .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage3.adp", .mus7 = "bgm/m_stage3.adp"
#define FN_M45 .mus4 = "bgm/m_boss.adp", .mus6 = "bgm/m_stage3b.adp", .mus7 = "bgm/m_stage3b.adp"
/* Sky Battalion (session 19, spec_sb_flow): checkpoints (SB2 9550 / 9551; SB3 9876 / 9877 and SB4 10076 / 10077 up to W - 100,
 * SB4's second with MV.Y > 830), the ships' right-edge exit back to SB1 (9455-9458, B > 0, g4 -1), the entries: SB1 from a
 * ship (8882: X -40, Y = start y + 16 - 400, B 15 while the fade runs; G -5 in sb_flow.c), a ship from SB1 (2955-2957: x -32,
 * start y + 16; no B event: the opening of sb_flow.c holds the player on the first hover platform) */
static const cpzone_t CP_SB2[] = { { 7192, 7392, 2, 7192, 616, 0, 0 }, { 10800, 10900, 3, 10872, 588, 0, 0 } };
static const cpzone_t CP_SB3[] = { { 5920, 14300, 2, 5920, 580, 0, 0 }, { 10100, 14300, 3, 10290, 710, 0, 0 } };
static const cpzone_t CP_SB4[] = { { 5980, 15524, 2, 5980, 266, 0, 0 }, { 12090, 15524, 3, 12090, 1160, 830, 0 } };
static const exit_t EX_SBSHIP[] = { { 1, 0, -50, 0, 0, "sb1" } };
static const entry_t EN_SB1[] = { { NULL, -40, 1189 + 16 - 400, 0, 1, 15 } };
static const entry_t EN_SBSHIP[] = { { NULL, -32, 469 + 16, 0, 1, 0 } };
/* stage sounds per act (11025 Hz; Media::play_id of the frames' own events, tools: spec_sb_flow 7): pads 62, planks 99, the
 * monitors' 172 / 173 / 278 / 308 / 316 (bombs, crates, shields: the AT ids, placed in SB2-SB4); enemies / gimmicks 54, 127,
 * 237, 264 (+ 265 / 371 SB3; 75 / 77 / 163 / 171 / 255 / 266 / 438 SB4); bosses: SB1 Kujacker 267-274, 371; SB2 chopper /
 * Spade 103, 156, 195, 446; SB3 satellite 165, 166, 169. The enemy / boss modules add their ids here. */
static const short SFX_SB1[] = { 62, 267, 268, 269, 270, 271, 272, 273, 274, 371, 99 };   /* + 99: the Kujacker's end (sb_kujacker.c) */
static const short SFX_SB2[] = { 62, 99, 54, 127, 237, 264, 103, 156, 195, 446, 172, 173, 278, 308, 316, 312, 263, 71, 155, 240, 445 };   /* + enemies / gimmicks: frag bomb 312, flamethrower 263; + Spade (sb_spade.c) 71 155 240 445 */
static const short SFX_SB3[] = { 62, 99, 54, 127, 237, 264, 265, 371, 165, 166, 169, 172, 173, 278, 308, 316, 312 };
static const short SFX_SB4[] = { 62, 99, 54, 127, 237, 264, 75, 77, 163, 171, 255, 266, 438, 172, 173, 278, 308, 316, 312 };
#define SB(d, f, n, mus) .dir = d, .frame = f, .stage = 4, .act = n, .nacts = 4, .music = mus, .checkpoints = 1, .sfx = SFX_SB##n, .nsfx = N_(SFX_SB##n)
#define SB_M34 .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage4.adp", .mus7 = "bgm/m_stage4.adp"
/* Jade Creek (session 21, stage index 5; spec_jc_flow, jc_flow.c): checkpoints JC2 10483 (7050 < X < 7250, MV.Y < 820), JC3 10655
 * (5300 < X < 5700, MV.Y > 800), JC4 10848 (4100 < X < 4400), JC5 11192 (9800 < X < 9999; its group 0x22af7 is the act-5 module's,
 * INFERRED on); JC1 has none (RE: no g4 writer in handle_frame_40). JC3's g4 10 comes from its boss (jc_flow_act3_cleared). */
static const cpzone_t CP_JC2[] = { { 7050, 7250, 2, 7080, 790, -820, 0 } };   /* ymin -820: MV.Y < 820 */
static const cpzone_t CP_JC3[] = { { 5300, 5700, 2, 5555, 1530, 800, 0 } };
static const cpzone_t CP_JC4[] = { { 4100, 4400, 2, 4184, 1640, 0, 0 } };
static const cpzone_t CP_JC5[] = { { 9800, 9999, 2, 9826, 780, 0, 0 } };
/* the stage's own sounds per act (11025 Hz): 62 jump pads (JC2-JC4), 99 explosions; the enemy / gimmick / boss modules add theirs */
/* Jade Creek stage groups (session 21; ids the JC modules play that are not resident, host FP_SID_MISS runs + the module
 * reports): enemies 60 99 101 192 233 275 314 (314 is Lilac's own: kept for her, loaded for Carol / Milla), gimmicks 208 426
 * 72 69, act 5 (jc_boss5.c + the FN Serpentine variant) 61 67 70 126 134 165 166 191 192 193 194 195 196 233 292 427 */
#define SFX_JC_ACTS 62, 99, 60, 101, 192, 233, 275, 314, 208, 426, 72, 69
static const short SFX_JC1[] = { SFX_JC_ACTS, 276 };                                            /* + the Hovercop fly-by (jc_boss.c) */
static const short SFX_JC2[] = { SFX_JC_ACTS, 277, 165, 166, 195, 77 };                          /* + the Hovercop chase */
static const short SFX_JC3[] = { SFX_JC_ACTS, 277, 165, 166, 195, 77, 296, 297, 298, 299, 315, 31, 33 };   /* + Neera (31 / 33: Lilac's / Carol's own, kept) */
static const short SFX_JC4[] = { SFX_JC_ACTS };
static const short SFX_JC5[] = { 62, 99, 61, 67, 70, 126, 134, 165, 166, 191, 192, 193, 194, 195, 196, 233, 292, 427 };   /* (no frogs, streams or collapsing platforms in JC5: 314 208 426 left out, Milla sound RAM) */
/* Trap Hideout (session 21b, stage index 8; Carol / Milla only; spec_th_flow, th_flow.c). Checkpoints (RE): TH1 13501 (g4 < 2, MV.Y > 2208,
 * X < 2000 -> (2, 180, 2288); RV th1_bottom_carol), TH2 13690 + its inline copy in handle_frame_58 (7280 < X < 7480 -> (2, 7380, 570)),
 * TH3 13878 (5446 < X < 5646 -> (6, 5546, 1520)) and 13879 (11460 < X < 11560, MV.Y < 400 -> (7, 11500, 160)); TH3's g4 5 comes from
 * the Crystal Basher's end (13931, th_flow_basher_cleared). TH1's shaft camera (13498-13500, RV th1_start / th1_bottom: E 511 until
 * MV.Y > 2180, then D 2016, E 6100) */
static const cpzone_t CP_TH1[] = { { -32768, 2000, 2, 180, 2288, 2208, 0 } };
static const cpzone_t CP_TH2[] = { { 7280, 7480, 2, 7380, 570, 0, 0 } };
static const cpzone_t CP_TH3[] = { { 5446, 5646, 6, 5546, 1520, 0, 0 }, { 11460, 11560, 7, 11500, 160, -400, 0 } };   /* ymin -400: MV.Y < 400 */
static const camshaft_t SHAFT_TH1 = { 2180, 2016, 100 };
/* the stage's own sounds per act (11025 Hz): the flow plays none (RE: no Media::play_id in the flow events of handle_frame_57..59);
 * 62 jump pads (TH3), 99 explosions; the enemy / gimmick / boss modules add theirs */
static const short SFX_TH1[] = { 62, 99 };
static const short SFX_TH2[] = { 62, 99 };
static const short SFX_TH3[] = { 62, 99 };
#define TH(d, f, n, mus) .dir = d, .frame = f, .stage = 8, .act = n, .nacts = 3, .music = mus, .checkpoints = 1, .sfx = SFX_TH##n, .nsfx = N_(SFX_TH##n)
#define JC(d, f, n, mus) .dir = d, .frame = f, .stage = 5, .act = n, .nacts = 5, .music = mus, .checkpoints = 1, .sfx = SFX_JC##n, .nsfx = N_(SFX_JC##n)
#define RM(d, f, n, mus) .dir = d, .frame = f, .stage = 2, .act = n, .nacts = 5, .music = mus, .checkpoints = 1, .sfx = SFX_RM, .nsfx = N_(SFX_RM)
static const level_def_t LEVELS[] = {
    /* Dragon Valley: frame index 20-23 (RE loop_play_music_N_20..23), water DV2 / DV3 (5109 / 5314), DV4 results at the
     * right edge (5381) as well as after the boss */
    { DV("dv1", 20, 1, "bgm/m_stage1.adp"), .card_first = 1 },
    { DV("dv2", 21, 2, "bgm/m_stage1b.adp"), .card_first = 5, .door_skip_ach = 18, .water = 2700, .pads = PADS_DV2, .npads = N_(PADS_DV2) },
    { DV("dv3", 22, 3, "bgm/m_stage1b.adp"), .card_first = 7, .water = 2700, .pads = PADS_DV3, .npads = N_(PADS_DV3) },
    { DV("dv4", 23, 4, "bgm/m_stage1b.adp"), .card_first = 8, .edge_results = 1, .pads = PADS_DV4, .npads = N_(PADS_DV4) },
    /* Aqua Tunnel: frame index 16-19 (tools/re/bgm_refs.py: 16 m_stage0, 17/18 m_stage0b; AT2 after the Chomper (g4 >=
     * 10, 4330) m_stage0b), water 1200 / 1200 / none / 1480 (4309 / 4478 / 4530 / 4685), AT2 walk-in B = 0 (4403), AT3
     * ends at the top (4521: MV.Y < 380), AT4 results through the boss only (4652) */
    { AT("at1", 16, 1, "bgm/m_stage0.adp"), .card_first = 21, .water = 1200, .axel2_top = 320, .dig_zones = DIGZ_AT1, .ndig_zones = N_(DIGZ_AT1), .dig_rewards = DIGR_AT1, .ndig_rewards = N_(DIGR_AT1),
      .start_fall = 1, .cps = CP_AT1, .ncps = N_(CP_AT1), .boss_cam = &CAM_AT1, .kills = KILL_AT1, .nkills = N_(KILL_AT1), .cam_right = 12480 + 213 },
    { AT("at2", 17, 2, "bgm/m_stage0.adp"), .card_first = 24, .walkin_still = 1, .music_cp = "bgm/m_stage0b.adp", .water = 1200,
      .cps = CP_AT2, .ncps = N_(CP_AT2), .boss_cam = &CAM_AT2, .kills = KILL_AT2, .nkills = N_(KILL_AT2), .cam_cp10_left = 6144 - 213, .cam_cp10_pad = 50 },
    { AT("at3", 18, 3, "bgm/m_stage0b.adp"), .card_first = 27, .exit_max_y = 380, .pipe_auto_y = 2900, .cam_right = 856 + 213 },
    { AT("at4", 19, 4, "bgm/m_stage0b.adp"), .card_first = 28, .water = 1480, .cps = CP_AT4, .ncps = N_(CP_AT4), .boss_cam = &CAM_AT4, .cam_right = 7424 + 213 },
    /* Relic Maze: frame index 24-28 (spec_rm_flow; music m_stage2, RM3 m_stage2b after its g4 = 10 checkpoint, RM4-RM5
     * m_stage2b), cards 11 / 14 / 17 / 18 / 20, no water. RM5 ends with the Mantalith (rm_mantalith.c, spec_rm_mantalith):
     * the boss starts the results (7083) and its post-results script ends the stage (7097). */
    { RM("rm1", 24, 1, "bgm/m_stage2.adp"), .card_first = 11, .exits = EX_RM1, .nexits = N_(EX_RM1), .entries = EN_RM1, .nentries = N_(EN_RM1),
      .cps = CP_RM1, .ncps = N_(CP_RM1), .flag1_y = 340, .crystals_once = 1, .wplate_cyclone = 1, .walkin_purge = 1, .gates = GATES_RM1, .ngates = N_(GATES_RM1),
      .dig_zones = DIGZ_RM1, .ndig_zones = N_(DIGZ_RM1), .dig_rewards = DIGR_RM1, .ndig_rewards = N_(DIGR_RM1) },
    { RM("rm2", 25, 2, "bgm/m_stage2.adp"), .card_first = 14, .exits = EX_RM2, .nexits = N_(EX_RM2), .entries = EN_RM2, .nentries = N_(EN_RM2),
      .cps = CP_RM2, .ncps = N_(CP_RM2), .death_to = "rm1", .kills = KILL_RM2, .nkills = N_(KILL_RM2), .plate_rule = 2, .pads = PADS_RM2, .npads = N_(PADS_RM2),
      .gates = GATES_RM2, .ngates = N_(GATES_RM2), .flagclears = CLEAR_RM2, .nflagclears = N_(CLEAR_RM2), .breakout = &BREAKOUT_RM2, .breakout_flag = 0x4 },
    { RM("rm3", 26, 3, "bgm/m_stage2.adp"), .card_first = 17, .music_cp = "bgm/m_stage2b.adp", .exits = EX_RM3, .nexits = N_(EX_RM3),
      .entries = EN_RM3, .nentries = N_(EN_RM3), .cps = CP_RM3, .ncps = N_(CP_RM3), .cam_right = 12288 + 50,
      .rmg = &RMG_RM3, .pads = PADS_RM3, .npads = N_(PADS_RM3), .nobike = 1 },
    { RM("rm4", 27, 4, "bgm/m_stage2b.adp"), .card_first = 18, .cps = CP_RM4, .ncps = N_(CP_RM4), .rmg = &RMG_RM4, .pads = PADS_RM4, .npads = N_(PADS_RM4) },
    { RM("rm5", 28, 5, "bgm/m_stage2b.adp"), .card_first = 20, .entries = EN_RM5, .nentries = N_(EN_RM5), .cam_right = 9600 + 50,
      .rmg = &RMG_RM5, .pads = PADS_RM5, .npads = N_(PADS_RM5), .rooms = ROOMS_RM5, .nrooms = N_(ROOMS_RM5), .warps = WARPS_RM5, .nwarps = N_(WARPS_RM5) },
    /* Fortune Night: frame index 29-34 (session 17, spec_fn_flow; RV: g19 = 2, CAM.E = sign.x + 213 = 12205 / 15133 / 15501).
     * FN1 always starts with the card (4307); FN2-FN5 walk in from x -32 at B 3 (4526 / 5047, RV FN3 / FN4); FN6 walks in
     * clamped to X >= 400 without a B event (8550: its floor / plane modes move the player, RV) inside a fixed camera box;
     * FN4 always restarts slot 5 (8326: the load-time music_play does, a different track). Cards 31-40 (RE counter starts). */
    { FN("fn1", 29, 1, "bgm/m_stage3.adp"), FN_M123, .fne = &FNE_FN1, .pads = PADS_FN1, .npads = N_(PADS_FN1), .card_first = 31, .start_card = 1, .cps = CP_FN1, .ncps = N_(CP_FN1), .cam_right = 11992 + 213 },
    { FN("fn2", 30, 2, "bgm/m_stage3.adp"), FN_M123, .fne = &FNE_FN2, .card_first = 33, .cps = CP_FN2, .ncps = N_(CP_FN2), .cam_right = 11992 + 213 },
    { FN("fn3", 31, 3, "bgm/m_stage3.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage3c.adp", .mus7 = "bgm/m_trouble.adp", .card_first = 36, .pads = PADS_FN3, .npads = N_(PADS_FN3),
      .cps = CP_FN3, .ncps = N_(CP_FN3), .cam_right = 14920 + 213, .script_exit = 1, .fnb = &FNB_FN3, .ckills = CK_FN3, .nckills = N_(CK_FN3), .cmarks = CM_FN3, .ncmarks = N_(CM_FN3) },
    { FN("fn4", 32, 4, "bgm/m_stage3b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage3b.adp", .mus7 = "bgm/m_stage3b.adp", .card_first = 37, .wplate_cyclone = 1,   /* 5677: the escalator plate takes M 26 too (fn_gimb.c) */
      .cps = CP_FN4, .ncps = N_(CP_FN4), .exits = EX_FN45, .nexits = N_(EX_FN45), .cam_right = 11992 + 213 },
    { FN("fn5", 33, 5, "bgm/m_stage3b.adp"), FN_M45, .fne = &FNE_FN5, .fnb = &FNB_FN5, .wplate_cyclone = 1, .fnb_disco = 1, .card_first = 39, .cps = CP_FN5, .ncps = N_(CP_FN5), .exits = EX_FN45, .nexits = N_(EX_FN45), .cam_right = 15288 + 213 },
    { FN("fn6", 34, 6, "bgm/m_boss.adp"), .mus4 = "bgm/m_boss.adp", .mus6 = "bgm/m_boss.adp", .mus7 = "bgm/m_boss.adp", .walkin_minx = 400, .walkin_free = 1,
      .cam_box = &BOX_FN6, .death_to = "fn5", .death_g4 = 2, .res_voice = "teamlilac_madeit", .res_wipe = 1 },
    /* Sky Battalion: frame index 35-38 (session 19, spec_sb_flow; sb_flow.c): sb1 = json 36 = Zao's airship, the hub. A fresh
     * start (g4 0 -> 1, 5839) shows the card; its pads send the player to the ships (g4 -1, 8876-8878, the free fade); the
     * ships end at their right edge back to sb1 (9455-9458), where she flies in from x -40, 400 px up, B 15, G -5 (8882 /
     * 8883, 5047). The Kujacker and the results are in sb1 (results wipe 8873-8881). Cards 41-50 (counters 9763 / inline:
     * 41, 44, 48; sb1 has none). Music per frame loop table (tools/re/bgm_refs.py + the wrappers): sb1 4 m_boss3, 5 / 7
     * m_stage4, 6 m_menu2; sb2 4 m_midboss, 5 / 6 m_stage4, 7 m_trouble; sb3 / sb4 4 m_midboss, 5-7 m_stage4. Deaths
     * restart the act (1990, g4 kept). Checkpoints 9550 / 9551, 9876 / 9877, 10076 / 10077 (X < W - 100; SB4's second
     * needs MV.Y > 830). No 2860 in any SB frame: the right camera bound is the frame width. */
    /* session 21: boss voices (RE 9012-9022 / 9163 Kujacker, 9625-9628 Spade; channel 24 / 27) */
    #define VOX_SB1 (const char *const[]){ "da_04_01", "da_04_02", "da_04_03", "da_04_04", "lilac_never", "carol_fatchance", "milla_icantiwont" }
    #define VOX_SB2 (const char *const[]){ "spade_comeon", "spade_goodenough", "spade_comeon2", "lilac_hitmeifyoucan" }
    { SB("sb1", 35, 1, "bgm/m_stage4.adp"), .mus4 = "bgm/m_boss3.adp", .mus6 = "bgm/m_menu2.adp", .mus7 = "bgm/m_stage4.adp", .start_card = 1,
      .entries = EN_SB1, .nentries = N_(EN_SB1), .clear_free = 1, .res_wipe = 1,
      .vox = VOX_SB1, .nvox = 7, .res_skin_voice = 1 },
    { SB("sb2", 36, 2, "bgm/m_stage4.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage4.adp", .mus7 = "bgm/m_trouble.adp", .card_first = 41,
      .cps = CP_SB2, .ncps = N_(CP_SB2), .exits = EX_SBSHIP, .nexits = N_(EX_SBSHIP), .entries = EN_SBSHIP, .nentries = N_(EN_SBSHIP), .walkin_free = 1, .pads = PADS_SB2, .npads = N_(PADS_SB2),
      .vox = VOX_SB2, .nvox = 4 },
    { SB("sb3", 37, 3, "bgm/m_stage4.adp"), SB_M34, .card_first = 44,
      .cps = CP_SB3, .ncps = N_(CP_SB3), .exits = EX_SBSHIP, .nexits = N_(EX_SBSHIP), .entries = EN_SBSHIP, .nentries = N_(EN_SBSHIP), .walkin_free = 1, .pads = PADS_SB3, .npads = N_(PADS_SB3) },
    { SB("sb4", 38, 4, "bgm/m_stage4.adp"), SB_M34, .card_first = 48,
      .cps = CP_SB4, .ncps = N_(CP_SB4), .exits = EX_SBSHIP, .nexits = N_(EX_SBSHIP), .entries = EN_SBSHIP, .nentries = N_(EN_SBSHIP), .walkin_free = 1, .pads = PADS_SB4, .npads = N_(PADS_SB4) },
    /* Jade Creek: frame index 39-43 (session 21, spec_jc_flow; jc_flow.c). Music tables (RE loop wrappers): slot 4 m_midboss
     * (loop_play_music_4_13) in JC1-JC4, JC5 m_boss (10871; 10872 m_boss6 only for g1 = 5); slot 5 m_stage5 (JC1 / JC2, JC3
     * before its boss: 10581 g4 < 10), m_stage5b (JC3 g4 >= 10: 10582 / loop_next_act_41; JC4 10580; JC5 loop_play_music_5_42);
     * slots 6 / 7 m_stage5. Water ML.B 1200 / 1600 (10378 / 10566 / 10822 / 10863 / 11643). Cards (ObjectCodec A newest first):
     * 51 (JC1 x3), 54 (JC2 x3), 57 (JC3), 58 (JC4 x3); JC5 has none. JC1 starts with the card (4307); the others walk in
     * (4490 / 4526 / 10482). JC1-JC4 end at the right edge (4135); camera E = Sign x + 213 (4241 / 4242: Signs 13480, 14784,
     * 9624, 10240), JC3 after its boss C = Sign x - 213, E = W + 50. JC5: the boss's results and the wipe (8547-8549). */
    { JC("jc1", 39, 1, "bgm/m_stage5.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 51, .start_card = 1, .water = 1200, .cam_right = 13480 + 213 },
    { JC("jc2", 40, 2, "bgm/m_stage5.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 54, .water = 1600, .cps = CP_JC2, .ncps = N_(CP_JC2), .cam_right = 14784 + 213 },
    { JC("jc3", 41, 3, "bgm/m_stage5.adp"), .music_cp = "bgm/m_stage5b.adp", .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 57, .water = 1600, .cps = CP_JC3, .ncps = N_(CP_JC3), .cam_right = 9624 + 213, .cam_cp10_left = 9624 - 213, .cam_cp10_pad = 50 },
    { JC("jc4", 42, 4, "bgm/m_stage5b.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .card_first = 58, .water = 1600, .cps = CP_JC4, .ncps = N_(CP_JC4), .cam_right = 10240 + 213 },
    { JC("jc5", 43, 5, "bgm/m_stage5b.adp"), .mus4 = "bgm/m_boss.adp", .mus6 = "bgm/m_stage5.adp", .mus7 = "bgm/m_stage5.adp",
      .water = 1600, .cps = CP_JC5, .ncps = N_(CP_JC5), .res_wipe = 1,
      .vox = (const char *const[]){ "serp_gotyounow", "serp_kungfu" }, .nvox = 2 },
    /* Trap Hideout: frame index 56-58 (session 21b, spec_th_flow; th_flow.c). Carol / Milla only (Classic: JC's end routes g1 != 0 here,
     * handle_frame_9 inline; 13536 / the inline copy in on_frame_57_start turn g1 0 into 2). Music (RE loop wrappers): slots 5-7
     * m_stage7 (loop_play_music_5_56) in every act, slot 4 m_midboss (loop_play_music_4_13; TH3 13701 while g4 < 5, 13702 m_boss2 from
     * g4 5). Cards (ObjectCodec A newest first, RE start events): 71 (TH1), 72 (TH2), 76 (TH3) = cards 71-80; ML.Q 38 / 39 / 40.
     * ML.A 1, ML.B 2000 in every act but no water events (4150-4170) in handle_frame_57..59: dry (RE; RV th1_bottom OTHERS.H 0).
     * TH1 starts with the card (4307) at the start point + 16 (269, -19), held there until the card fade's B reaches 96 (13533,
     * th_flow_card), then falls down the shaft; TH1 / TH2 end at the right edge (4135 -> 3578); TH2 / TH3 are entered by the
     * walk-in (4526; RV th2_start / th3_start: B 3 for 30 frames, g4 1); TH3 ends with its boss's results and the wipe (4560-4586,
     * 5394, 8547 / 8548, 13795: LevelCardFade2 F > 50 -> frame 8). Signs 6000 / 12984 / 12984: E beyond the frame (no cam_right). */
    { TH("th1", 56, 1, "bgm/m_stage7.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage7.adp", .mus7 = "bgm/m_stage7.adp",
      .card_first = 71, .start_card = 1, .cps = CP_TH1, .ncps = N_(CP_TH1), .cam_shaft = &SHAFT_TH1 },
    { TH("th2", 57, 2, "bgm/m_stage7.adp"), .mus4 = "bgm/m_midboss.adp", .mus6 = "bgm/m_stage7.adp", .mus7 = "bgm/m_stage7.adp",
      .card_first = 72, .cps = CP_TH2, .ncps = N_(CP_TH2) },
    { TH("th3", 58, 3, "bgm/m_stage7.adp"), .mus4 = "bgm/m_midboss.adp", .mus4b = "bgm/m_boss2.adp", .mus4b_g4 = 5, .mus6 = "bgm/m_stage7.adp",
      .mus7 = "bgm/m_stage7.adp", .card_first = 76, .cps = CP_TH3, .ncps = N_(CP_TH3), .res_wipe = 1 },
};
static const level_def_t DEFAULT = { .dir = "", .frame = -1, .stage = -1, .act = 1, .nacts = 1, .music = "bgm/m_stage1.adp" };

const level_def_t *level_def(const char *dir) {
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (!strcmp(LEVELS[i].dir, dir)) return &LEVELS[i];
    return &DEFAULT;
}
const level_def_t *level_cur(void) { return level_def(G.lvdir); }
/* Classic / continue route: the first stage of each character (RE 799: Classic with G1 == 4 (Milla) -> frame 16, Aqua
 * Tunnel; everyone else -> frame 20, Dragon Valley). Indexed by the stage record index (s). */
const char *level_route_start(int character) { return level_first_of_stage(chr_is_milla(character) ? 0 : 1); }
/* first act of a stage record index (Time Attack / routes) */
const char *level_first_of_stage(int stage) {
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (LEVELS[i].stage == stage && LEVELS[i].act == 1) return LEVELS[i].dir;
    return NULL;
}
/* the next act of the same stage (NULL after the last act) */
const char *level_next(const char *dir) {
    const level_def_t *d = level_def(dir);
    for (int i = 0; i < (int)(sizeof LEVELS / sizeof LEVELS[0]); i++) if (LEVELS[i].stage == d->stage && LEVELS[i].act == d->act + 1) return LEVELS[i].dir;
    return NULL;
}

/* ---- stage flow shared by every level (session 13; before: aqua.c, by act number) ---------------------------------- */
static int cam_locked;
int level_cam_locked(void) { return cam_locked; }   /* 4212 / 4451: the boss camera trigger flag */
/* the act start: remove the item sets this character does not get (4318 / 4319) and the AT2 jelly / petal row (4484 /
 * 4485: only while g1 != 1, i.e. always in the port) */
static int shaft_on;   /* TH1 13499: Sign D = 1 once the player is deeper than the shaft (reset at every act start) */
void level_flow_spawn(void) {
    const level_def_t *L = level_cur();
    cam_locked = 0; shaft_on = 0;
    for (int k = 0; k < L->nkills; k++) {
        const killbox_t *z = &L->kills[k];
        if ((z->who == 1 && chr_is_milla(G.character)) || (z->who == 2 && !chr_is_milla(G.character))) continue;
        if ((z->who == 3 && G.character == CHR_LILAC) || (z->who == 4 && G.character == CHR_CAROL)) continue;   /* MV.A tests: bike-Carol keeps neither */
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive || o->x < z->x0 || o->x > z->x1 || o->y < z->y0 || o->y > z->y1) continue;
            if (o->type == OT_CRYSTAL || (z->jelly && (o->type == OT_JELLY || o->type == OT_PETAL))) o->alive = 0;
        }
    }
    if (L->crystals_once && G.at_g4 != 0 && G.at_g4 != 1)                     /* RM1 start: a return or a checkpoint restart */
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_CRYSTAL) G.o[i].alive = 0;
    /* class sets and limit markers by character (FN3 8082: MV.A = 0, 8083: MV.A != 0; bike-Carol counts as != 0) */
    int lilac = G.character == CHR_LILAC;
    for (int k = 0; k < L->nckills; k++) {
        const classkill_t *c = &L->ckills[k];
        if ((c->who == 1) != lilac) continue;
        for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type >= c->t0 && G.o[i].type <= c->t1) G.o[i].alive = 0;
    }
    for (int k = 0; k < L->ncmarks; k++) {
        const markmove_t *m = &L->cmarks[k];
        if ((m->who == 1) != lilac) continue;
        for (int i = 0; i < G.nobjs; i++) {
            obj_t *o = &G.o[i];
            if (!o->alive) continue;
            if (o->type == OT_CAMLIM_L && m->l != -32768) o->x = m->l;
            if (o->type == OT_CAMLIM_R && m->r != -32768) o->x = m->r;
            if (o->type == OT_CAMLIM_T && m->t != -32768) o->y = m->t;
            if (o->type == OT_CAMLIM_B && m->b != -32768) o->y = m->b;
        }
    }
}
/* CAM.M = 1 to the CameraLimitBoss markers of the act (loop_camera_mode1: left x, top y, right x, bottom y), for stage scripts */
void level_lock_markers(void) {
    float l = 0, t = 0, r = (float)G.lvl.w, b = (float)G.lvl.h;
    for (int i = 0; i < G.nobjs; i++) {
        const obj_t *o = &G.o[i];
        if (!o->alive) continue;
        if (o->type == OT_CAMLIM_L) l = o->x; else if (o->type == OT_CAMLIM_R) r = o->x; else if (o->type == OT_CAMLIM_T) t = o->y; else if (o->type == OT_CAMLIM_B) b = o->y;
    }
    cam_locked = 1; camera_boss_bounds(l, t, r, b);
}
/* the act's track for play_music(slot) (the frame's loop table: 4 boss / midboss, 5 the stage, 6 / 7 per frame) */
const char *level_music_slot(int slot) {
    const level_def_t *L = level_cur();
    if (slot == 4 && L->mus4b && G.at_g4 >= L->mus4b_g4) return L->mus4b;   /* TH3 13702 (g4 >= 5: m_boss2) */
    return slot == 4 ? L->mus4 : slot == 5 ? L->music : slot == 6 ? L->mus6 : slot == 7 ? L->mus7 : NULL;
}
/* the exit the player is taking this frame (NULL: none); acts without an exit list use the right edge (4135 / 5045) */
const exit_t *level_exit_check(const player_t *p) {
    static const exit_t RIGHT = { 1, 0, -50, 0, 0, NULL };
    const level_def_t *L = level_cur();
    double B = p->ground ? p->gsp : p->vx, Y = p->y - ORIG_REF_DY;
    if (L->script_exit) return NULL;                                 /* FN3: no 5045 / 3578 (the stage script ends the act) */
    int n = L->nexits ? L->nexits : 1;
    for (int k = 0; k < n; k++) {
        const exit_t *e = L->nexits ? &L->exits[k] : &RIGHT;
        if (e->edge > 0 ? !(p->x > G.lvl.w + e->x && B > 0) : !(p->x < e->x && B < 0)) continue;
        if ((e->ymin || e->ymax) && !(Y > e->ymin && Y < e->ymax)) continue;
        if (!e->to && !level_next(G.lvdir)) continue;
        return e;
    }
    return NULL;
}
/* the entry for an act change from act `from` (NULL: the default, x -32 at the start point) */
const entry_t *level_entry(const char *from) {
    const level_def_t *L = level_cur();
    for (int k = 0; k < L->nentries; k++) if (!L->entries[k].from || (from && !strcmp(L->entries[k].from, from))) return &L->entries[k];
    return NULL;
}
/* Classic route after a stage (RE frame 8 = handle_frame_9 inline, Classic block g0 = 1; spec_records_saves 1 / spec_th_flow 1.2):
 * MenuSequence.I = the finished stage s -> the next frame, by g1 (0 Lilac / != 0 Carol, bike-Carol, Milla): I 0 / 1 -> 24 (RM),
 * 2 -> 29 (FN), 3 -> 35 (SB), 4 -> 39 (JC), 5 -> 44 (Thermal Base, g1 = 0) / 56 (Trap Hideout, g1 != 0), 6 -> 50 (Pangu Lagoon,
 * g1 = 0) / 84 (before Battle Glacier), 7 -> 84 (g1 = 0) / 56 (g1 != 0), 8 -> 84 (g1 = 0) / 44 (g1 != 0). As stage indices (-1:
 * frame 84, Battle Glacier's lead-in). A stage the port lacks gives NULL: back to the front end (TB / PL / BG not ported). */
static const signed char ROUTE[9][2] = { { 2, 2 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 8 }, { 7, -1 }, { -1, 8 }, { -1, 6 } };
const char *level_route_next(int stage) {
    if (stage < 0 || stage >= (int)(sizeof ROUTE / sizeof ROUTE[0])) return NULL;
    int s = ROUTE[stage][G.character != CHR_LILAC];               /* g1 = 0: Lilac only */
    return s >= 0 ? level_first_of_stage(s) : NULL;
}
/* RM5 labyrinth warps (spec_rm_flow 2.5): the player is in a warp zone moving its way */
const warp_t *level_warp_check(const player_t *p) {
    const level_def_t *L = level_cur();
    double B = p->ground ? p->gsp : p->vx, X = p->x, Y = p->y - ORIG_REF_DY;
    for (int k = 0; k < L->nwarps; k++) {
        const warp_t *w = &L->warps[k];
        if (X > w->x0 && X <= w->x1 && Y > w->y0 && Y < w->y1 && (w->dir < 0 ? B < 0 : B > 0)) return w;
    }
    return NULL;
}
void level_warp_do(player_t *p, const warp_t *w) {   /* 6734-6743: a new 32-frame walk-in at the target, then 6599 (g4 = 1) */
    p->x = w->tx; p->y = w->ty + ORIG_REF_DY; p->ground = 0; p->vx = p->vy = 0; p->gsp = 0; p->face = w->face;
    G.at_g4 = w->g4; G.walk_b = w->walk_b; G.clear_t = 0; G.exit_b = 0; G.input_lock = 0;
    G.act_in = 1; G.act_fade = 248;
    camera_reset(); if (w->walkin_E > 0) G.cam_nE = w->walkin_E;   /* 6597 */
    camera_update();
}
/* normal camera bounds (spec_at_flow 3.1): C = 0, E = cam_right or the frame width; after g4 >= 10 the AT2 values */
void level_camera_bounds(void) {
    const level_def_t *L = level_cur();
    if (L->cam_cp10_pad && G.at_g4 >= 10) { G.cam_nC = (float)L->cam_cp10_left; G.cam_nE = (float)G.lvl.w + L->cam_cp10_pad; }
    else if (L->cam_right) G.cam_nE = (float)L->cam_right;
    if (L->cam_box) { G.cam_nC = L->cam_box->C; G.cam_nD = L->cam_box->D; G.cam_nE = L->cam_box->E; G.cam_nF = L->cam_box->F; }   /* FN6 8553 */
    if (L->cam_shaft) {                                             /* TH1 13498 (Sign D 0: E = the right marker's x) / 13500 (D 1) */
        if (!shaft_on) { for (int i = 0; i < G.nobjs; i++) if (G.o[i].alive && G.o[i].type == OT_CAMLIM_R) { G.cam_nE = G.o[i].x; break; } }
        else { G.cam_nD = L->cam_shaft->D; G.cam_nE = (float)G.lvl.w + L->cam_shaft->E_pad; }
    }
    if (L->nrooms) {                                                /* RM5 6745-6750: room bounds (the last matching row wins) */
        double X = G.p.x, Y = G.p.y - ORIG_REF_DY;
        int rails = 0; for (int i = 0; i < G.nobjs && !rails; i++) if (G.o[i].alive && G.o[i].type == OT_WR_RAIL) rails = 1;
        for (int k = 0; k < L->nrooms; k++) {
            const camroom_t *r = &L->rooms[k];
            if (X > r->x0 && X <= r->x1 && Y > r->y0 && Y < r->y1 && (r->rails < 0 || r->rails == rails)) { G.cam_nE = r->E; G.cam_nF = r->F; }
        }
    }
}
void level_release_camera(int keep_trigger) { if (!keep_trigger) cam_locked = 0; camera_normal_bounds(); G.cam_m2 = 1; }
static void checkpoint(int g4, float x, float y) {                  /* g4 / g5 / g6 + loop_cp_16 (restart values) */
    G.at_g4 = g4; G.at_g5 = x; G.at_g6 = y;
    G.act_lives = G.p.lives; G.act_crystals = G.p.crystals; G.act_keystones = G.keystones; G.act_stage_crystals = G.stage_crystals;
}
/* per frame, after the objects: checkpoints, then the boss camera lock (lock: X > lock_x and lock_ymin < Y < lock_ymax;
 * release: X < unlock_x and Y < unlock_y; a zone with g4_max does nothing once g4 >= g4_max) */
void level_flow_frame(const player_t *p) {
    const level_def_t *L = level_cur();
    if (L->nrooms && !G.act_in) level_camera_bounds();                       /* RM5 group 0x2250b: room bounds every frame (not during a walk-in: 6597) */
    if (L->cam_box) level_camera_bounds();                                   /* FN6 8553 + the inline tail of handle_frame_35: every frame */
    if (L->cam_shaft && !shaft_on && p->y - ORIG_REF_DY > L->cam_shaft->y_on) { shaft_on = 1; level_camera_bounds(); }   /* TH1 13499 -> 13500 */
    double X = p->x;
    for (int k = 0; k < L->ncps; k++) {
        const cpzone_t *c = &L->cps[k];
        if (!(X > c->x0 && X < c->x1 && G.at_g4 < c->g4) || (c->ymin > 0 && !(p->y - ORIG_REF_DY > c->ymin)) || (c->ymin < 0 && !(p->y - ORIG_REF_DY < -c->ymin))) continue;
        if (c->nosnap) { G.at_g4 = c->g4; G.at_g5 = c->x; G.at_g6 = c->y; } else checkpoint(c->g4, c->x, c->y);
        if (L->music_cp && c->g4 >= 10) music_play(L->music_cp);                 /* loop_next_act_17 / _26: play_music(5) */
    }
    if (L->flag1_y) G.stage_flags = (G.stage_flags & ~1) | (p->y - ORIG_REF_DY > L->flag1_y);   /* RM1 inline: ML flag 0x1 */
    const camzone_t *z = L->boss_cam;
    if (!z || (z->g4_max && G.at_g4 >= z->g4_max)) return;
    int px = (int)p->x, py = (int)floor(p->y - ORIG_REF_DY);
    if (!cam_locked && px > z->lock_x && py > z->lock_ymin && py < z->lock_ymax) { cam_locked = 1; camera_boss_bounds(z->l, z->t, z->r, z->b); }
    else if (cam_locked && px < z->unlock_x && py < z->unlock_y) level_release_camera(0);
}
