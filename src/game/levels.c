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
static const padcfg_t PADS_FN1[] = { {3880,1328,0,0,0,1,0}, {3944,1008,1,90,1,0,0}, {3464,1032,2,0,0,1,0}, {3520,744,3,270,0,0,0}, {3584,440,4,270,1,0,0},
    {7528,1072,6,90,0,1,0}, {7208,952,7,180,1,0,0}, {10472,704,8,90,0,1,0}, {10472,416,9,90,0,0,0}, {10472,128,10,90,1,0,0},
    {7656,1168,11,270,0,1,0}, {7784,1016,12,270,0,0,0}, {7912,728,13,270,1,0,0} };
static const padcfg_t PADS_FN3[] = { {5568,784,0,0,0,1,0}, {5888,640,1,90,0,0,0}, {5632,640,2,270,1,0,0}, {7312,1088,4,0,0,1,0}, {7792,1088,5,0,1,0,0},
    {9504,800,6,0,0,1,0}, {9568,424,7,180,0,0,0}, {9984,464,8,90,0,0,0}, {9856,752,9,270,0,0,0}, {10176,712,10,180,0,0,0}, {10752,752,11,90,0,0,0},
    {10400,520,12,180,0,0,0}, {10496,800,13,0,0,0,0}, {10368,656,14,270,0,0,0}, {10720,616,15,180,0,0,0}, {11360,616,16,180,1,0,0},
    {10656,800,17,0,0,1,0}, {10656,616,18,180,0,0,0}, {11360,616,19,180,1,0,0} };
static const padcfg_t PADS_RM5[] = { {352,2664,0,0,0,0,0}, {624,2632,1,0,1,0,0}, {1176,520,2,270,0,1,0}, {1176,216,3,270,1,0,0} };

static const short SFX_AT[] = { 55, 62, 69, 99, 172, 173, 240, 242, 251, 252, 278, 306, 308, 316 };

#define DV(d, f, n, mus) .dir = d, .frame = f, .stage = 1, .act = n, .nacts = 4, .music = mus
#define AT(d, f, n, mus) .dir = d, .frame = f, .stage = 0, .act = n, .nacts = 4, .music = mus, .plate_rule = 1, .switch_late = 1, .monitor_hop = 1, .checkpoints = 1, .sfx = SFX_AT, .nsfx = N_(SFX_AT)
#define FN(d, f, n, mus) .dir = d, .frame = f, .stage = 3, .act = n, .nacts = 6, .music = mus, .checkpoints = 1   /* Fortune Night (session 17, provisional: spec_fn_flow) */
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
    /* Fortune Night: frame index 29-34 (session 17; rows provisional until spec_fn_flow is ported: music, cards, exits) */
    { FN("fn1", 29, 1, "bgm/m_stage3.adp"), .card_first = 31, .pads = PADS_FN1, .npads = N_(PADS_FN1) },
    { FN("fn2", 30, 2, "bgm/m_stage3.adp"), .card_first = 33 },
    { FN("fn3", 31, 3, "bgm/m_stage3.adp"), .card_first = 36, .pads = PADS_FN3, .npads = N_(PADS_FN3) },
    { FN("fn4", 32, 4, "bgm/m_stage3b.adp"), .card_first = 37 },
    { FN("fn5", 33, 5, "bgm/m_stage3b.adp"), .card_first = 39 },
    { FN("fn6", 34, 6, "bgm/m_boss.adp"), .card_first = 40 },
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
void level_flow_spawn(void) {
    const level_def_t *L = level_cur();
    cam_locked = 0;
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
}
/* the exit the player is taking this frame (NULL: none); acts without an exit list use the right edge (4135 / 5045) */
const exit_t *level_exit_check(const player_t *p) {
    static const exit_t RIGHT = { 1, 0, -50, 0, 0, NULL };
    const level_def_t *L = level_cur();
    double B = p->ground ? p->gsp : p->vx, Y = p->y - ORIG_REF_DY;
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
/* Classic route after a stage (RE frame 8, spec_records_saves 1 / spec_rm_flow 1.1): Aqua Tunnel (s 0) and Dragon Valley
 * (s 1) -> Relic Maze (s 2); later stages are not ported (NULL: back to the front end) */
const char *level_route_next(int stage) { return stage == 0 || stage == 1 ? level_first_of_stage(2) : NULL; }
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
    double X = p->x;
    for (int k = 0; k < L->ncps; k++) {
        const cpzone_t *c = &L->cps[k];
        if (!(X > c->x0 && X < c->x1 && G.at_g4 < c->g4) || (c->ymin && !(p->y - ORIG_REF_DY > c->ymin))) continue;
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
