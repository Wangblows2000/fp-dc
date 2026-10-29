#!/usr/bin/env python3
"""
Import a recovered Freedom Planet frame (level) into fpdc runtime data.

usage: import_frame.py <frames_dir> <images_dir> <frame_no> <name> <out_dir> [--layers 2]

Inputs (from tools/re/*.py, run on the user's Linux build):
  frames_dir/frame_N.json   object placements (class, x, y, layer, kind)
  frames_dir/backdrops.json backdrop classes: image id + collision field 0xac
  frames_dir/anims.json     active classes: animation tables (image ids, speeds)
  images_dir                fp-assets extraction of the Linux Assets.dat

Collision typing (RESEARCH_LOG "backdrop collision", CONFIRMED via collider vtables):
  0xac=9  + InstanceBox       -> solid, whole image rectangle
  0xac=8  + BackdropCollision -> solid, pixel mask (alpha >= 128)
  0xac=11 + InstanceBox       -> ladder (not solid)
  absent                      -> decoration
Pieces with blend colour alpha 0 (field 0x5f == 0) are invisible (collision only).
Collision is rasterised on the collision layers (default: 2, the main play layer).

Outputs:
  <name>_pN.tex   piece atlases (ARGB1555, <=1024x1024)
  <name>.lv2      level (see write_lv2)
  <name>_obj.tex/.spr  sprite bank of the active objects the engine implements
  <name>_report.txt
"""
import json, os, struct, sys, hashlib
from PIL import Image, ImageChops, ImageOps
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert import write_tex, pack, pack_skyline, pack_skyline_fill, argb1555

# active classes the runtime understands -> engine type id
ENGINE_TYPES = {
    'playerstartingpoint': 1, 'objectcrystal': 2, 'enemydragonfly': 3, 'objectpetal': 6,
    'hazardspikesup': 7, 'yellowspringup': 8, 'yellowspringupleft': 13, 'yellowspringupright': 14,
    'yellowspringright': 15, 'yellowspringleft': 16, 'yellowspringdown': 29, 'gimmickhangingidlesensor': 17, 'sign': 9, 'enemyturretus': 10,
    'breakablerock': 11, 'monitorcrystals': 12, 'monitorrings': 18, 'monitor1up': 19,
    'monitorinvincibility': 4, 'levelparallaxpiece0': 20, 'levelparallaxpiece1': 21,
    'levelparallaxpiece2': 22, 'levelparallaxpiece3': 23, 'levelparallaxpiece4': 24,
    'levelparallaxpiece5': 25, 'levelparallaxpiece6': 26, 'levelparallaxpiece7': 27,
    'laddertop': 30,
    'enemygansekijr': 31,
    'levelvalleyplatform1': 32, 'levelmovingplatform2': 33, 'levelmovingplatform3': 34,   # RE spec_platforms.md
    'gimmickbridge12logs': 35, 'gimmickbridgelog': 36,
    # session 8 (RE build/re/spec_dv_enemies.md, spec_wretchnid.md, spec_dv3_gimmicks.md, spec_traversal.md,
    # spec_dv_bosses.md, spec_cards_results.md); numbers mirror src/fp.h
    'enemystarflounder': 37, 'enemycacteon': 38, 'enemysporepod': 39, 'enemylimiter': 40, 'objectcodec': 41,
    'hazardspikesleft': 42, 'hazardspikesright': 43, 'hazardspikesdown': 44, 'redspringupleft': 45,
    'halfcorkscrewleft': 46, 'halfcorkscrewright': 47, 'layerchangeswitch': 48, 'layerchangeto0': 49, 'layerchangeto1': 50,
    'spiraltunnelfront': 147, 'monitorwoodshield': 151, 'monitorearthshield': 152, 'monitorfireshield': 153, 'monitormetalshield': 154, 'monitorwatershield': 155, 'bubblepatch': 157, 'frontrock1': 148, 'fronttree': 149, 'treefront': 150, 'spiraltunneltop': 51, 'spiraltunnelbottom': 52, 'spiraltunnelmask': 53, 'levelballtunnel1': 54, 'levelgeyserplate': 55,
    'minibosswretchnidbase': 56, 'minibosswretchnidturret': 57, 'minibosswretchnidweakspot': 58, 'minibossrailing': 59, 'minibossbarrier': 60,
    'minibosswretchnidleftleg1': 61, 'minibosswretchnidleftleg2': 62, 'minibosswretchnidleftleg3': 63, 'minibosswretchnidleftleg4': 64,
    'minibosswretchnidleftleg5': 65, 'minibosswretchnidleftleg6': 66, 'minibosswretchnidrightleg1': 67, 'minibosswretchnidrightleg2': 68,
    'minibosswretchnidrightleg3': 69, 'minibosswretchnidrightleg4': 70, 'minibosswretchnidrightleg5': 71, 'minibosswretchnidrightleg6': 72,
    'minibosstredd': 73, 'minibosstreddleft1': 74, 'minibosstreddleft2': 75, 'minibosstreddleft3': 76, 'minibosstreddleft4': 77,
    'minibosstreddleft5': 78, 'minibosstreddleft6': 79, 'minibosstreddright1': 80, 'minibosstreddright2': 81, 'minibosstreddright3': 82,
    'minibosstreddright4': 83, 'minibosstreddright5': 84, 'minibosstreddright6': 85, 'enemytreddpushleft': 86, 'enemytreddpushleft2': 87,
    'waterfalllog': 88, 'waterfallloggenerator': 89, 'levelfloatingplatform': 90, 'waterfallfront3': 91,
    'gimmickboulder': 92, 'switchwallplate': 93, 'switchtrapdoor': 94, 'switchpressureplate': 95, 'switchdoor': 96,
    'treetrunk': 97, 'pushableblock1': 98, 'pushableblock2': 99,
    'gimmickbamboospike': 110, 'gimmickbamboospike2': 111, 'gimmickbamboospike3': 112, 'gimmickbamboospike4': 113,
    'objecttaostone': 114, 'spinninglog': 115, 'spinninglogmask': 116, 'breakablerock2': 117,
    'bossserpentinehead': 118, 'bosssnakebody': 119, 'bosssnakehitbox': 120, 'bossgeyseroverlap1': 121, 'bossgeyseroverlap2': 122,
    'bossgiantboulder': 123, 'bosstunnel': 124, 'bosstunnel2': 125, 'bosstunnel3': 126, 'breakable1': 127, 'breakable2': 128, 'breakable3': 129,
    'bossbouncemode': 130, 'cameratriggerboss': 131, 'objectjumppad': 132, 'monitorkeystone': 133, 'objectfuel': 158,
    'levellayer016': 134, 'levellayer0loop04': 135, 'levellayer115': 136, 'levellayer1loop03': 137,
    'objectdigspot': 160,   # Aqua Tunnel (session 12, src/game/aqua.c)
    'enemyfirefly': 161, 'enemyslime': 162, 'objectwhispmaker': 163, 'breakableaquabrick': 164, 'breakableaquabrick2': 165,
    'waterjet': 169, 'waterjetright': 170, 'waterjetback': 171, 'waterjetmaker': 172, 'jellyspring': 173,   # spec_at_gimmicks_a.md
    'redspringup': 174, 'redspringdown': 175, 'redspringleft': 176, 'redspringright': 177, 'redspringupright': 178,
    'monitorbomb': 179, 'monitorcrate': 180,
    'aquabrickplatform': 183, 'collapsingplatform': 184, 'switchdoor2': 185, 'switchmillablock': 186, 'levelcrusher1': 187, 'levelcrusher2': 189,
    'gimmickrotatingpipe': 190, 'gimmickrotatingpipebase': 191, 'gimmickrotatingpipebase2': 192, 'objectspinningbeam': 193,
    'levelaxelplatform1': 194, 'levelaxelplatform2': 195, 'axelplatformmaker1': 196, 'axelplatformmaker2': 197,
    'gimmickswingspawner1': 198, 'gimmickswingspawner2': 199, 'scrollingbrick': 203, 'slimefloor': 204, 'slimeraft': 205,
    'aquatreeoverlay': 206, 'fishstatuefront': 207,
    'minibossironworm': 214, 'minibossironwormsegment': 215, 'minibossironwormbase': 216,   # spec_ironworm.md
    'minibosschomper': 217, 'minibosschompertail': 218, 'minibosschomperball': 219, 'minibosschompermouth': 220,   # spec_chomper.md
    'bosschompersnake': 221, 'bosschompersnakesegment': 222, 'bosschompersnakemouth': 223,   # spec_chompersnake.md
    # Relic Maze enemies (session 14, spec_rm_enemies.md; src/game/relic.c)
    'enemyiris': 232, 'enemyslug': 233, 'enemyhopper': 234, 'enemyfirespitter': 235, 'enemysentinel': 236,
    'enemyextractorbase': 237, 'enemyextractorhead': 238, 'objectpot': 239, 'objectbluepot': 240,
    # Relic Maze acts 1-2 gimmicks (session 14, spec_rm_gimmicks_a.md; src/game/relic_gima.c)
    'minibossrailing2': 246, 'gimmickrelicpanel': 247, 'gimmickrelicflippanel': 249, 'gimmickrelicflippanel2': 250,
    'gimmickrelicflippanel3': 251, 'gimmickrelicflippanel4': 252, 'gimmickfireballspawner': 253, 'gimmickfireballturret1': 254,
    'gimmickfireballturret2': 255, 'gimmickfireballturretback': 256, 'gimmickhangingmovingbar': 259, 'gimmickhangingmovingsensor': 260,
    'gimmickhangingmovingbar2': 261, 'gimmickhangingmovingsensor2': 262, 'gimmickhangingidleswitch': 263, 'gimmickhangingswitch': 264,
    'gimmickhangingidlemover': 265, 'gimmickhangingmover': 266, 'gimmickmovingfloor1': 267, 'gimmickmovingfloor2': 268,
    'gimmickmovingfloor3': 269, 'gimmickrollingplatform': 270, 'gimmickrollingplatform2': 271, 'gimmickspinball': 272,
    'clockdial1': 273, 'clockdial2': 274, 'gimmickaxletunnel': 275, 'basementlight': 276, 'switchcolumnfront1': 277,
    'switchcolumnback1': 278, 'switchcolumnback2': 279, 'breakableruins': 280, 'digspothole': 282, 'layerchangeswitch2': 283,
    'levellayer001': 285, 'levellayer002': 286, 'levellayer015': 287, 'levellayer02': 288, 'levellayer101': 289,
    'levellayer116': 290, 'levellayer1loop01': 291, 'levellayer1loop02': 292,
    # Relic Maze golems (spec_rm_golems.md; src/game/relic_boss.c) and the boss camera limit markers
    'minibossgolem': 350, 'minibossgolem2': 351, 'minibossgolempiece1': 352, 'minibossgolempiece2': 353, 'minibossbonegolem': 357,
    'minibossbonegolempiece5': 358, 'minibossbonegolempiece3': 359, 'minibossbonegolempiece': 360, 'minibossbonegolempiece6': 361,
    'minibossbonegolemneck1': 362, 'minibossbonegolemneck2': 363, 'minibossbonegolemneck3': 364, 'minibossbonegolemneck4': 365,
    'minibossbonegolemhead': 366, 'cameralimitbossleft': 368, 'cameralimitbossright': 369, 'cameralimitbosstop': 370, 'cameralimitbossbottom': 371,
    'waterfallback': 139, 'waterfallfront': 140, 'waterfallfront2': 141, 'waterfallfront3': 142, 'waterfallfront4': 143,   # EnemyGansekiJr_835 (RE build/re/spec_ganseki.md)     # LadderTop_428: invisible obstacle at the top of each ladder (RE spec_ladder.md)
}

# Fusion object identity is class + handle: a class name used by two objects with different handles (a separate object
# per stage, with its own constructor values and events) gets its own engine type, so the runtime never has to test the
# level to tell them apart (session 13 generalisation rule, RE_NOTES "Reusable objects"). Keys: (class, handle).
ENGINE_TYPES_ID = {
    ('switchdoor', 770): 210,            # SwitchDoor_770 (AT1 / AT2: slides left); DV's 997 / 1066 sink (96)
    ('switchwallplate', 831): 212,       # SwitchWallPlate_831 (AT2 / AT3: turns the pipes); DV3's 1029 (93) drops the boulder
    ('aquabrickplatform', 803): 228,     # AquaBrickPlatform_803 (AT2 / AT4); 731 (AT1) = 183
    ('levelcrusher1', 847): 188,         # LevelCrusher1_847: AT3's horizontal slab; 808 (AT2) = the vertical crusher 187
    ('levelaxelplatform1', 845): 229,    # LevelAxelPlatform1_845 / 2_846: AT3's placed bobbing platforms; 680 / 737 are made by
    ('levelaxelplatform2', 846): 230,
    ('switchwallplate', 1149): 284,      # SwitchWallPlate_1149 (RM1: raises the switch column, RM2: turns the flip panels)    #   the AxelPlatformMakers (AT1 / AT4) and stay 194 / 195
    ('gimmickswingspawner1', 848): 231,  # GimmickSwingSpawner1_848 (AT3: 6 links); 824 (AT2: 8 links) = 198
}
def engine_type(cls, handle): return ENGINE_TYPES_ID.get((cls, handle), ENGINE_TYPES.get(cls))

# stage title card per stage (RE build/re/spec_stage_cards.md): (first frame index, last, LevelCardIntro2 name frame,
# LevelCardIntro3 "STAGE n" frame, alternative STAGE frame for Carol / Milla or None). on_frame_N = index + 1.
STAGE_CARDS = [(16, 19, 3, 0, None), (20, 23, 0, 0, None), (24, 28, 1, 1, None), (29, 34, 2, 2, None), (35, 38, 4, 3, None),
               (39, 43, 5, 4, None), (44, 49, 6, 5, 6), (50, 55, 8, 6, None), (56, 58, 7, 5, None), (59, 64, 10, 7, None)]
def stage_card(n):   # n = on_frame number -> (name frame, [stage frames]); Final Dreadnought 65-72: ROUND 1-4 two frames each
    i = n - 1
    if 65 <= i <= 72: return 11 + (i - 65) // 2, [8]
    for a, b, nm, st, alt in STAGE_CARDS:
        if a <= i <= b: return nm, [st] + ([alt] if alt is not None else [])
    return 0, [0]

# stage frames with water (MasterLevel A = 1: DV2 5109, DV3 5314, AT1 4309, AT2 4478, AT4 4685): air meter + water effects
WATER_FRAMES = (22, 23, 17, 18, 20)

# ---- Aqua Tunnel (session 12, src/game/aqua.c; specs build/re/spec_at_*.md)
AT_ENS = [('objectdigspot_779', 0, '0', 'digspot'), ('guiweaponslot_426', 7, '0', 'dighint')] + \
         [('enemyfirefly_749', 0, d, 'ff%s' % d) for d in ('0', '1', '2', '16', '17', '18')] + \
         [('enemyfirefly_749', 8, '0', 'ff_dead'), ('enemyfirefly_749', 8, '16', 'ff_dead_l'),
          ('enemyslime_717', 0, '0', 'sl_idle'), ('enemyslime_717', 0, '16', 'sl_idle_l'), ('enemyslime_717', 7, '0', 'sl_jump'),
          ('enemyslime_717', 7, '16', 'sl_jump_l'), ('enemyslime_717', 8, '0', 'sl_land'), ('enemyslime_717', 8, '16', 'sl_land_l'),
          ('enemyslime_717', 10, '0', 'sl_dead'), ('enemyslimeburst_718', 0, '0', 'slb'), ('enemyslimeburst_718', 7, '0', 'slb_pop'),
          ('effectwhisp_767', 0, '0', 'whisp'), ('breakableaquabrick_681', 0, '0', 'abrick'), ('breakableaquabrick2_819', 0, '0', 'abrick2'),
          ('breakableaquabrickpiece_682', 0, '0', 'abpiece'),
          ('waterjet_740', 0, '0', 'wjet'), ('waterjetback_759', 0, '0', 'wjetb'), ('waterjetright_751', 0, '0', 'wjetr'),
          ('waterjetmaker_820', 0, '0', 'wjetmk'), ('effectswaterhit_756', 4, '0', 'waterhit'), ('jellyspring_704', 0, '0', 'jelly'),
          ('redspringup_419', 0, '0', 'rs_up'), ('redspringup_419', 5, '0', 'rs_up_b'), ('redspringdown_422', 0, '0', 'rs_down'),
          ('redspringdown_422', 5, '0', 'rs_down_b'), ('redspringleft_421', 0, '0', 'rs_left'), ('redspringleft_421', 5, '0', 'rs_left_b'),
          ('redspringright_420', 0, '0', 'rs_right'), ('redspringright_420', 5, '0', 'rs_right_b'),
          ('redspringupright_417', 0, '0', 'rs_ur'), ('redspringupright_417', 5, '0', 'rs_ur_b'),
          ('monitorbomb_535', 0, '0', 'mon_bomb'), ('monitorbomb_535', 7, '0', 'mon_bomb_arm'), ('monitorcrate_364', 0, '0', 'mon_crate'),
          # spec_at_gimmicks_b.md
          ('aquabrickplatform_731', 0, '0', 'abplat'), ('aquabrickplatform_803', 0, '0', 'abplat2'), ('collapsingplatform_732', 0, '0', 'collapse'),
          ('switchdoor_770', 0, '0', 'sdoor'), ('switchdoor2_777', 0, '0', 'sdoor2'), ('switchmillablock_784', 0, '0', 'mblock'),
          ('levelcrusher1_808', 0, '0', 'crushv'),   # wall / pressure plates: the shared 'wallplate' / 'pplate' entries (session 13)
          ('levelcrusher1_847', 0, '0', 'crushh'), ('levelcrusher2_851', 0, '0', 'crushh2'), ('gimmickrotatingpipe_828', 0, '0', 'pipe'),
          ('gimmickrotatingpipebase_782', 0, '0', 'pipehub'), ('gimmickrotatingpipebase2_783', 0, '0', 'pipecog'),
          ('objectspinningbeam_806', 0, '0', 'beam'), ('levelaxelplatform1_680', 0, '0', 'axel1'), ('levelaxelplatform1_845', 0, '0', 'axel1b'),
          ('levelaxelplatform2_737', 0, '0', 'axel2'), ('levelaxelplatform2_846', 0, '0', 'axel2b'), ('gimmickswingbase_821', 0, '0', 'swbase'),
          ('gimmickswingchain_822', 0, '0', 'swchain'), ('gimmickswingplatform_823', 0, '0', 'swplat'), ('scrollingbrick_859', 0, '0', 'scrollbrick'),
          ('slimefloor_857', 0, '0', 'slimefloor'), ('slimeraft_858', 0, '0', 'raft'), ('aquatreeoverlay_762', 0, '0', 'treeovl'),
          ('fishstatuefront_813', 0, '0', 'fishstatue'), ('effectsexplosion01_367', 4, '0', 'explosion01'),
          # spec_ironworm.md (AT1 miniboss)
          ('minibossironworm_773', 0, '0', 'iw_r'), ('minibossironworm_773', 0, '16', 'iw_l'), ('minibossironworm_773', 7, '0', 'iw_open_r'),
          ('minibossironworm_773', 7, '16', 'iw_open_l'), ('minibossironworm_773', 8, '0', 'iw_close_r'), ('minibossironworm_773', 8, '16', 'iw_close_l'),
          ('minibossironworm_773', 10, '0', 'iw_blink_r'), ('minibossironworm_773', 10, '16', 'iw_blink_l'),
          ('minibossironwormsegment_772', 0, '0', 'iws_r'), ('minibossironwormsegment_772', 0, '16', 'iws_l'),
          ('minibossironwormsegment_772', 10, '0', 'iws_blink_r'), ('minibossironwormsegment_772', 10, '16', 'iws_blink_l'),
          ('minibossironwormbase_775', 0, '0', 'iwbase'), ('enemyrock_776', 0, '0', 'rock'), ('enemyrock_776', 7, '0', 'rock_break'),
          ('minibossacid_774', 0, '0', 'acid'), ('minibossacid_774', 10, '0', 'acid_splat'),
          # spec_chomper.md (AT2 miniboss)
          ('minibosschomper_815', 0, '0', 'ch'), ('minibosschomper_815', 7, '0', 'ch_daze0'), ('minibosschomper_815', 7, '1', 'ch_daze1'),
          ('minibosschompertail_816', 0, '0', 'cht'), ('minibosschomperball_817', 0, '0', 'chb'),
          ('minibosschompermouth_818', 0, '0', 'chm_close'), ('minibosschompermouth_818', 0, '1', 'chm_close1'),
          ('minibosschompermouth_818', 10, '0', 'chm_open'), ('minibosschompermouth_818', 10, '1', 'chm_open1'),
          ('effectshade_436', 0, '0', 'shade_at'),
          # spec_chompersnake.md (AT4 boss)
          ('bosschompersnake_861', 0, '0', 'sn'), ('bosschompersnake_861', 7, '1', 'sn_pale'),
          ('bosschompersnakesegment_860', 0, '0', 'sns'), ('bosschompersnakesegment_860', 0, '1', 'sns_pale'),
          ('bosschompersnakemouth_863', 0, '0', 'snm_close'), ('bosschompersnakemouth_863', 0, '1', 'snm_close1'),
          ('bosschompersnakemouth_863', 10, '0', 'snm_open'), ('bosschompersnakemouth_863', 10, '1', 'snm_open1'),
          ('bossbubblebomb_862', 0, '0', 'bub')]
# Relic Maze enemies, pots and their children (spec_rm_enemies.md 10); facing sets: '' dir 0, '_l' dir 16
RM_ENS = [('enemyiris_1191', a, '0', nm) for a, nm in ((0, 'ir_idle'), (1, 'ir_chase'), (8, 'ir_dead'), (9, 'ir_blink'), (10, 'ir_dive'), (11, 'ir_ret'))] + \
    [('enemyslug_1190', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((0, 'sl_idle'), (1, 'sl_walk'), (8, 'sl_dead'), (10, 'sl_spit')) for d in ('0', '16')] + \
    [('enemyslug_1190', 0, '8', 'sl_mask'), ('enemyfiresprite_1189', 0, '0', 'fsprite'), ('enemyfiresprite_1189', 7, '0', 'fsprite_burn')] + \
    [('enemyhopper_1150', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((0, 'hp_idle'), (7, 'hp_crouch'), (8, 'hp_air'), (9, 'hp_dead'), (10, 'hp_land')) for d in ('0', '16')] + \
    [('enemyhopper_1150', 0, d, 'hp_m' + d) for d in ('7', '8', '9', '10')] + \
    [(c, a, d, nm + ('' if d == '0' else '_l')) for c, pre in (('enemyextractorhead_1166', 'ex'), ('enemyextractorbase_1162', 'eb'))
     for a, nm in ((0, '_idle'), (6, '_shoot'), (7, '_aim'), (8, '_8'), (9, '_9'), (10, '_drill'), (11, '_ret')) for d in ('0', '16') for nm in (pre + nm,)] + \
    [('enemyspark_1200', 0, '0', 'spark'), ('enemyspark_1200', 0, '16', 'spark_l'), ('enemyspark_1200', 0, '24', 'spark_run'),
     ('enemyrock_1194', 0, '0', 'rock'), ('enemyrock_1194', 7, '0', 'rock_break'), ('enemyrock_1290', 0, '0', 'rock'), ('enemyrock_1290', 7, '0', 'rock_break')] + \
    [('enemyfirespitter_1307', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((0, 'fs_walk'), (7, 'fs_open'), (8, 'fs_dead'), (10, 'fs_turn')) for d in ('0', '16')] + \
    [('enemyfirespitterbase_1313', 0, '0', 'fs_shell'), ('enemyfirespitterbase_1313', 0, '16', 'fs_shell_l'), ('minibossflame_1264', 4, '0', 'flame'),
     ('enemysentinel_1474', 0, '0', 'sn_eye')] + [('enemysentinel_1474', 7, d, 'sn_open' + d) for d in ('0', '8', '16', '24')] + \
    [('enemysentinelspike_1475', 0, '0', 'sn_spike'), ('objectpot_1206', 0, '0', 'pot'), ('objectpot_1206', 0, '8', 'pot_broken'),
     ('objectbluepot_1620', 0, '0', 'pot'), ('objectbluepot_1620', 0, '8', 'pot_broken'), ('effectsdebris_427', 0, '0', 'debris_pot', slice(51, 53)),
     ('effectsdebris_427', 0, '0', 'debris_hop', slice(16, 18))]
RM_ENS += [('minibossrailing2_1204', 0, '0', 'rail2'), ('gimmickrelicpanelpiece_1193', 0, '0', 'rpiece'), ('gimmickrelicflippanel_1226', 0, '0', 'flippanel')] + \
    [('gimmickrelicpanel_1175', 0, d, 'rpanel' + d) for d in ('0', '1', '2', '3')] + \
    [('gimmickfireballspawner_1179', 0, '0', 'fbspawn'), ('gimmickfireballspawner_1179', 6, '0', 'fbspawn_open'), ('gimmickfireballturret1_1180', 0, '0', 'fbturret1'),
     ('gimmickfireballturret2_1182', 0, '0', 'fbturret2'), ('gimmickfireballturretback_1181', 0, '0', 'fbback'), ('gimmickfireballturretback_1181', 11, '0', 'fbback_end')] + \
    [('gimmickfireballturretback_1181', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((6, 'fbback_fire'), (10, 'fbback_load')) for d in ('0', '16')] + \
    [('gimmickfireball_1192', 0, '0', 'fball'), ('gimmickfireball_1192', 7, '0', 'fball_burst'), ('effectember_1025', 4, '0', 'ember'),
     ('gimmickhangingmovingbar_1111', 0, '0', 'hbar'), ('gimmickhangingmovingsensor_1110', 0, '0', 'hsens'), ('gimmickhangingmovingbar2_1255', 0, '0', 'hbar2'),
     ('gimmickhangingmovingsensor2_1254', 0, '0', 'hsens2'), ('gimmickhangingidleswitch_1268', 0, '0', 'hiswitch'), ('gimmickhangingswitch_1269', 0, '0', 'hswitch'),
     ('gimmickhangingidlemover_1271', 0, '0', 'himover'), ('gimmickhangingmover_1270', 0, '0', 'hmover'), ('gimmickmovingfloor1_1265', 0, '0', 'mfloor1'),
     ('gimmickmovingfloor2_1266', 0, '0', 'mfloor2'), ('gimmickmovingfloor3_1267', 0, '0', 'mfloor3'), ('gimmickrollingplatform_1161', 0, '0', 'rplat'),
     ('gimmickrollingplatform2_1159', 0, '0', 'rwheel'), ('gimmickspinball_1173', 0, '0', 'spinball'), ('clockdial1_1243', 0, '0', 'clock1'), ('clockdial2_1244', 0, '0', 'clock2'),
     ('gimmickaxletunnel_1278', 0, '0', 'axle'), ('basementlight_1276', 0, '0', 'blight'), ('switchcolumnfront1_1157', 0, '0', 'colfront'),
     ('switchcolumnback1_1155', 0, '0', 'colback1'), ('switchcolumnback2_1156', 0, '0', 'colback2'), ('breakableruins_1202', 0, '0', 'ruins'),
     ('breakableruinspiece_1203', 0, '0', 'ruinpiece'), ('digspothole_1209', 0, '0', 'dighole'), ('digspothole_1209', 0, '8', 'dighole_open'),
     ('layerchangeswitch2_1241', 0, '0', 'lsw2'), ('levellayer001_1211', 0, '0', 'll001'), ('levellayer002_1212', 0, '0', 'll002'), ('levellayer015_1217', 0, '0', 'll015'),
     ('levellayer02_1236', 0, '0', 'll02'), ('levellayer101_1213', 0, '0', 'll101'), ('levellayer116_1233', 0, '0', 'll116'), ('levellayer1loop01_1215', 0, '0', 'll1l01'),
     ('levellayer1loop02_1216', 0, '0', 'll1l02')]
RM_ENS += [('minibossgolem_1259', 0, '0', 'geye'), ('minibossgolem_1259', 0, '8', 'geye_open'), ('minibossgolem_1259', 7, '0', 'geye_opening'),
    ('minibossgolem2_1258', 0, '0', 'glid0'), ('minibossgolem2_1258', 0, '1', 'glid1'), ('minibossgolempiece1_1257', 0, '0', 'gp1'), ('minibossgolempiece2_1256', 0, '0', 'gp2'),
    ('badnikgolemshot_1260', 0, '0', 'gshot'), ('badnikgolemshot_1260', 4, '0', 'gshot_pop'), ('badnikgolemshot2_1261', 0, '0', 'gshot2'),
    ('minibossbonegolem_1567', 0, '0', 'bgcore'), ('minibossbonegolemneck1_1571', 0, '0', 'bgn1'), ('minibossbonegolemneck2_1572', 0, '0', 'bgn2'),
    ('minibossbonegolemneck3_1573', 0, '0', 'bgn3'), ('hazardshockwave_1576', 0, '0', 'shock'), ('hazardshockwave_1576', 0, '16', 'shock_l')] + \
    [(c, 0, d, nm + {'0': '', '16': '_l', '24': '_24'}[d]) for c, nm, ds in (('minibossbonegolemneck4_1574', 'bgn4', ('0', '16')), ('minibossbonegolemhead_1575', 'bghead', ('0', '16')),
     ('minibossbonegolempiece3_1566', 'bgp3', ('0', '16')), ('minibossbonegolempiece_1568', 'bgpc', ('0', '16')), ('minibossbonegolempiece5_1570', 'bgp5', ('0', '16', '24')),
     ('minibossbonegolempiece6_1569', 'bgp6', ('0', '16', '24'))) for d in ds]
RM_ENS_WITH = {'badnikgolemshot_1260': ('minibossgolem',), 'badnikgolemshot2_1261': ('minibossgolem',), 'hazardshockwave_1576': ('minibossbonegolem',),
               'enemyrock_1194': ('enemyextractorhead', 'minibossgolem'), 'gimmickrelicpanelpiece_1193': ('gimmickrelicpanel',), 'gimmickfireball_1192': ('gimmickfireballspawner',), 'effectember_1025': ('gimmickfireballspawner',),
               'breakableruinspiece_1203': ('breakableruins',), 'monitorearthshield_442': ('objectdigspot',), 'minibossrailing2_1204': ('minibossrailing2', 'minibossbarrier'),'enemyfiresprite_1189': ('enemyslug',), 'enemyspark_1200': ('enemyextractorhead',),
               'enemyrock_1290': ('enemyextractorhead',), 'enemyfirespitterbase_1313': ('enemyfirespitter',),
               'minibossflame_1264': ('enemyfirespitter', 'minibossgolem'), 'enemysentinelspike_1475': ('enemysentinel',)}
# ---- Relic Maze act 3 Claw sequence (spec_rm_claw.md; src/game/rm_claw.c): engine types 400-449 (fp.h), sprites
CLAW_TYPES = {'shrine3': 400, 'bossclaw': 401, 'bossclawtalons': 402, 'bossclawpiece1': 403, 'bossclawpiece4': 404,
              'bossclawhitbox': 405, 'bossclawbase': 406, 'caravanengine': 408, 'caravanwheel': 409, 'npcmerc': 410,
              'pushableblock3': 411, 'levelfloatingplatform2': 413, 'levelfloatingplatform3': 414, 'backgroundwall': 415,
              'backgroundwall2': 416, 'breakablecolumn1': 417, 'breakablecolumn2': 418, 'breakablerelicwall': 419}
CLAW_TYPES_ID = {('active', 1338): 412, ('shrine2', 1296): 421, ('rmalternatepath', 1426): 422, ('rmalternatepath2', 1438): 423}   # + the route split (6203 / 6204)   # Active_1338 (the road strip), Shrine2_1296 (RM5's 1588 is another object)
CLAW_ENS = [('shrine3_1298', 0, '0', 'shrine3'), ('cameratriggerboss_389', 0, '0', 'camtrig')] + \
    [('bossclaw_1288', 0, d, 'claw' + d) for d in ('0', '1', '2', '16', '17')] + [('bossclaw_1288', 7, d, 'claw%s_h' % d) for d in ('16', '17')] + \
    [('bossclawtalons_1299', 0, d, 'talons' + d) for d in ('0', '1', '16', '17')] + \
    [('bossclawpiece1_1300', 0, '0', 'clink'), ('bossclawpiece1_1300', 0, '4', 'clink_lit'), ('bossclawpiece1_1300', 7, '0', 'clink_h'),
     ('bossclawhitbox_1320', 0, '0', 'clawhit'), ('bossclawbase_1319', 0, '0', 'clawbase'), ('bossclawbase_1319', 0, '8', 'clawbase_w'),
     ('bossreflectobeam_1321', 0, '20', 'cbeam'), ('bossreflectobeam_1321', 0, '12', 'cbeam_up'),
     ('caravanengine_1332', 0, '0', 'engine'), ('caravanengine_1332', 0, '8', 'engine_w'), ('caravanwheel_1308', 0, '0', 'cwheel'),
     ('pushableblock3_1287', 0, '0', 'pblock3'), ('active_1338', 0, '0', 'cground'), ('levelfloatingplatform2_1311', 0, '0', 'croof2'),
     ('levelfloatingplatform3_1403', 0, '0', 'croof3'), ('backgroundwall_1291', 0, '0', 'bgwall'), ('backgroundwall2_1304', 0, '0', 'bgwall2'),
     ('breakablecolumn1_1293', 0, '0', 'column1'), ('breakablecolumn2_1294', 0, '0', 'column2'), ('breakablerelicwall_1295', 0, '0', 'relicwall'),
     ('breakablerelicwalldebris_1297', 0, '0', 'relicdebris'), ('shrine2_1296', 0, '0', 'shrine2'), ('rmalternatepath_1426', 0, '0', 'altpath'),
     ('rmalternatepath2_1438', 0, '0', 'altpath2')] + \
    [('npcmerc_1329', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((0, 'merc'), (2, 'merc_run'), (5, 'merc_alarm'), (7, 'merc_jump'), (8, 'merc_fall')) for d in ('0', '16')]
# children (merged into ENS_WITH: the parents are added to an existing entry's tuple)
CLAW_ENS_WITH = {'bossreflectobeam_1321': ('bossclaw',), 'breakablerelicwalldebris_1297': ('breakablerelicwall',),
                 'effectsdust_437': ('caravanengine',), 'breakableruinspiece_1203': ('backgroundwall',),
                 'effectsexplosion06_435': ('bossclaw', 'caravanengine'), 'monitoricon_363': ('caravanengine',)}
CLAW_ENS_FRAMES = {'cameratriggerboss_389': {27}, 'shrine2_1296': {27}}   # the trigger's box only where the Claw sequence uses it (DV4's stays imageless)
# rows exported only in these frames (json number): the creator's own events name the handle per frame (RM1 5793 rock
# 1194, RM3 / RM4 6175 rock 1290); the pots share names (one image set per level)
ENS_FRAMES = {'enemyrock_1194': {25, 26}, 'enemyrock_1290': {27, 28}}   # RM2: the golem's debris rocks (5972 / 5976)
# ---- Relic Maze act 5 end (src/game/rm_mantalith.c, build/re/spec_rm_mantalith.md): Kingdom Stone scene, the Mantalith's
# parts (q29 visuals + the q20 head / arm weak points), its projectiles and the post-results ruins. Engine types 450-499
# by (class, handle): these class names are other stages' objects too (KingdomStone_3680, Shrine2_1296, ...)
MT_ENGINE_TYPES_ID = {('kingdomstone', 1589): 450, ('placeholdercharacter', 1625): 451, ('npchookline', 1590): 452,
    ('ksprojectile1', 1593): 453, ('ksprojectile2', 1626): 454, ('lightray', 1591): 455, ('lightray2', 1592): 456, ('shrine2', 1588): 457,
    ('bossgroundcut1', 1600): 459, ('bossgroundcut2', 1601): 460, ('bossmantalithbody', 1603): 461, ('bossmantalithneck4', 1604): 462,
    ('bossmantalithneck3', 1605): 463, ('bossmantalithneck2', 1610): 464, ('bossmantalithneck1', 1611): 465, ('bossmantalithleftknee', 1606): 466,
    ('bossmantalithrightknee', 1607): 467, ('bossmantalithleg2', 1609): 468, ('bossmantalithleg3', 1608): 469, ('bossmantalithleg1', 1602): 470,
    ('bossmantalithhead', 1614): 471, ('bossmantalithhead2', 1613): 472, ('bossmantalithhead3', 1612): 473, ('bossmantalitharm6', 1615): 474,
    ('bossmantalitharm3', 1616): 475, ('bossmantalithhitbox1', 1618): 476, ('bossmantalithhitbox2', 1619): 477, ('effectwarning', 1528): 478}
MT_ENS = [('kingdomstone_1589', 0, '0', 'mt_stone'), ('placeholdercharacter_1625', 0, '0', 'mt_spade'), ('placeholdercharacter_1625', 0, '1', 'mt_spade_land'),
    ('placeholdercharacter_1625', 7, '0', 'mt_spade_jump'), ('npchookline_1590', 0, '0', 'mt_hook'), ('ksprojectile1_1593', 0, '0', 'mt_ks1'),
    ('ksprojectile2_1626', 0, '0', 'mt_ks2'), ('lightray_1591', 0, '0', 'mt_ray'), ('shrine2_1588', 0, '0', 'mt_shrine'), ('bossgroundcut1_1600', 0, '0', 'mt_cut'),
    ('bossmantalithbody_1603', 0, '0', 'mt_body'), ('bossmantalithneck4_1604', 0, '0', 'mt_neck4'), ('bossmantalithneck3_1605', 0, '0', 'mt_neck3'),
    ('bossmantalithneck2_1610', 0, '0', 'mt_neck2'), ('bossmantalithneck1_1611', 0, '0', 'mt_neck1'), ('bossmantalithleftknee_1606', 0, '0', 'mt_knee'),
    ('bossmantalithleg2_1609', 0, '0', 'mt_leg2'), ('bossmantalithleg3_1608', 0, '0', 'mt_leg3')] + \
    [('bossmantalithleg1_1602', 0, d, 'mt_bead' + d) for d in ('0', '1', '2', '3', '31')] + \
    [('bossmantalithhead_1614', a, d, 'mt_h%d_%s' % (a, d)) for a in (0, 7) for d in ('0', '1', '2')] + [('bossmantalithhead_1614', 8, '0', 'mt_h8')] + \
    [(c, a, '0', nm + ('_8' if a == 8 else '')) for c, nm in (('bossmantalithhead2_1613', 'mt_horn2'), ('bossmantalithhead3_1612', 'mt_horn3')) for a in (0, 8)] + \
    [(c, 0, d, nm + d) for c, nm in (('bossmantalitharm3_1616', 'mt_arm3_'), ('bossmantalitharm6_1615', 'mt_arm6_')) for d in ('0', '4', '8')] + \
    [('bossmantalitharm3_1616', 7, '0', 'mt_arm3_sw'), ('bossmantalitharm6_1615', 7, '0', 'mt_arm6_sw'), ('bossmantalithhitbox1_1618', 0, '0', 'mt_hit1'),
     ('bossmantalithhitbox2_1619', 0, '0', 'mt_hit2'), ('effectwarning_1528', 0, '0', 'mt_warn'), ('bossslash_1598', 0, '0', 'mt_slash'), ('bosschomp_1599', 4, '0', 'mt_chomp'),
     ('breakableruins_1450', 0, '0', 'mt_ruins'), ('breakableruinspiece_1451', 0, '0', 'mt_rpiece')]   # + the shared 'dust', 'debris', 'explosion06', 'rock'
MT_ENS_WITH = {'bossslash_1598': ('bossmantalithbody',), 'bosschomp_1599': ('bossmantalithbody',), 'breakableruins_1450': ('bossmantalithbody',),
    'breakableruinspiece_1451': ('bossmantalithbody',), 'effectsdust_437': ('kingdomstone',), 'effectsdebris_427': ('bossmantalithbody',),
    'enemyrock_1290': ('bossmantalithbody',), 'effectsexplosion06_435': ('bossmantalithbody',)}   # created by the boss / the scene (6878, 6918, 6928, 7050, 7061, 7088)
ENS_FRAMES['enemyrock_1290'] = ENS_FRAMES['enemyrock_1290'] | {29}   # RM5 (json 29): the slam's rocks (6949-6956)
ENGINE_TYPES_ID.update(MT_ENGINE_TYPES_ID)   # RM5 end (rm_mantalith.c)
ENS_FRAMES.update({c: {29} for c in {e[0] for e in MT_ENS}})   # RM5 end rows: frame 29 only (the class names exist in other stages)
AT_ENS_WITH = {'enemyrock_776': ('minibossironworm',), 'minibossacid_774': ('minibossironworm', 'minibosschomper', 'bosschompersnake'),
               'bossbubblebomb_862': ('bosschompersnake',), 'effectshade_436': ('minibosschomper', 'bosschompersnake'),
               'levelaxelplatform1_680': ('axelplatformmaker1',), 'levelaxelplatform2_737': ('axelplatformmaker2',),
               'gimmickswingbase_821': ('gimmickswingspawner1', 'gimmickswingspawner2'), 'gimmickswingchain_822': ('gimmickswingspawner1', 'gimmickswingspawner2'),
               'gimmickswingplatform_823': ('gimmickswingspawner1', 'gimmickswingspawner2'), 'effectsexplosion01_367': ('collapsingplatform', 'minibosschomper'), 'effectswaterhit_756': ('waterjetright', 'waterjetmaker'), 'waterjetright_751': ('waterjetmaker',), 'enemyslimeburst_718': ('enemyslime',), 'effectwhisp_767': ('objectwhispmaker',),
               'breakableaquabrickpiece_682': ('breakableaquabrick', 'breakableaquabrick2'), 'breakableaquabrick_681': ('breakableaquabrick2',)}

# ---- per-level bank of enemies, bosses and gimmicks (<name>_ens): (class, anim, direction, name[, frame slice]);
# an entry is exported when its class is placed in the frame or when it is a child of a placed class (ENS_WITH)
ENS = [('enemydragonfly_942', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((0, 'df_fly'), (8, 'df_dead'), (9, 'df_dive'), (10, 'df_turn'), (11, 'df_wind')) for d in ('0', '16')] + \
      [('enemystarflounder_1020', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((0, 'sf_swim'), (8, 'sf_dead'), (10, 'sf_turn')) for d in ('0', '16')] + \
      [('enemycacteon_1013', a, d, nm + ('' if d == '0' else '_l')) for a, nm in ((0, 'ca_idle'), (1, 'ca_walk'), (7, 'ca_jump'), (8, 'ca_dead'), (10, 'ca_land')) for d in ('0', '16')] + \
      [('enemycacteon_1013', 0, '8', 'ca_body'), ('enemycacteon_1013', 0, '24', 'ca_feet'),
       ('enemysporepod_1012', 0, '0', 'sp_rest'), ('enemysporepod_1012', 7, '0', 'sp_open'), ('enemysporepod_1012', 8, '0', 'sp_dead'), ('enemysporepod_1012', 11, '0', 'sp_close'),
       ('enemysporeprojectile_1022', 0, '0', 'spore'), ('enemysporeprojectile_1022', 7, '0', 'spore_burst'),
       ('objectkeystone_429', 0, '0', 'keystone'), ('objectkeystone_429', 7, '0', 'keystone_t'), ('monitorkeystone_534', 0, '0', 'mon_key'), ('effectsdebris_427', 0, '0', 'debris', slice(0, 11)),
       ('objectcodec_430', 0, '0', 'card'),
       ('hazardspikesleft_876', 0, '0', 'spk_left'), ('hazardspikesright_877', 0, '0', 'spk_right'), ('hazardspikesdown_875', 0, '0', 'spk_down'),
       ('redspringupleft_418', 0, '0', 'rspring_ul'), ('redspringupleft_418', 5, '0', 'rspring_ul_b'),
       ('minibosswretchnidbase_966', 0, '0', 'wr_base'), ('minibosswretchnidbase_966', 10, '0', 'wr_base_die'),
       ('minibosswretchnidturret_967', 0, '0', 'wr_turret'), ('minibosswretchnidturret_967', 7, '0', 'wr_turret_fire'),
       ('minibosswretchnidweakspot_968', 0, '0', 'wr_weak'), ('minibosswretchnidleftleg1_954', 0, '0', 'wr_seg1'), ('minibosswretchnidleftleg2_960', 0, '0', 'wr_seg2'), ('minibosswretchnidleftleg3_962', 0, '0', 'wr_seg3'), ('minibossrailing_953', 0, '0', 'wr_rail'), ('minibossbarrier_969', 0, '0', 'wr_barrier'),
       ('effectsexplosion06_435', 4, '0', 'explosion06'), ('effectsshade_436', 0, '0', 'shade'),
       ('minibosstredd_1050', 0, '0', 'td_body'), ('minibosstredd_1050', 10, '0', 'td_die'),
       ('minibosstreddleft1_1057', 0, '0', 'td_claw'), ('minibosstreddleft1_1057', 0, '8', 'td_claw_up'), ('minibosstreddleft1_1057', 7, '0', 'td_claw_open'), ('minibosstreddleft1_1057', 8, '0', 'td_claw_snap'),
       ('minibosstreddleft2_1055', 0, '0', 'td_shaft'), ('minibosstreddleft3_1056', 0, '0', 'td_column'), ('minibosstreddleft4_1053', 0, '0', 'td_joint'),
       ('minibosstreddleft5_1054', 0, '0', 'td_p5'), ('minibosstreddleft6_1052', 0, '0', 'td_p6'),
       ('minibosstreddbomb_1060', 0, '0', 'td_bomb'), ('minibosstredddebris_1058', 0, '0', 'td_debris'), ('monitoricon_363', 0, '0', 'blast', slice(9, 10))] + \
      [('waterfalllog_1039', 0, d, 'wlog%d' % (int(d) // 4)) for d in ('0', '4', '8', '12', '16', '20', '24', '28')] + \
      [('gimmickboulder_1063', 0, '0', 'boulder'), ('switchwallplate_831', 0, '0', 'wallplate'), ('switchtrapdoor_1065', 0, '0', 'trapdoor'),
       ('switchpressureplate_769', 0, '0', 'pplate'), ('switchdoor_997', 0, '0', 'door'), ('treetrunk_1030', 0, '0', 'trunk'), ('treetrunk_1030', 0, '16', 'trunk_l'),
       ('pushableblock1_493', 0, '0', 'pblock1'), ('pushableblock2_1028', 0, '0', 'pblock2'), ('pushableblock2_1028', 0, '16', 'pblock2_l'),
       ('gimmickbamboospike_1070', 0, '0', 'bam1'), ('gimmickbamboospike2_1071', 0, '0', 'bam2'), ('gimmickbamboospike3_1069', 0, '0', 'bam3'), ('gimmickbamboospike4_1072', 0, '0', 'bam4'),
       ('objecttaostone_541', 0, '0', 'taostone'), ('breakablerock2_1023', 0, '0', 'brock2'),
       ('bossgiantboulder_1091', 0, '0', 'gboulder'), ('bosstunnel_1090', 0, '0', 'btunnel'), ('bosstunnel2_1093', 0, '0', 'btunnel2'), ('bosstunnel3_1103', 0, '0', 'btunnel3'),
       ('breakable1_1095', 0, '0', 'brk1'), ('breakable2_1096', 0, '0', 'brk2'), ('breakable3_1097', 0, '0', 'brk3'),
       ('bossfireball_1003', 0, '0', 'fireball'),
       ('bossserpentinehead_943', 0, '8', 'sh_rest'), ('bossserpentinehead_943', 7, '0', 'sh_arc'), ('bossserpentinehead_943', 7, '16', 'sh_arc_l'),
       ('bossserpentinehead_943', 10, '0', 'sh_stun'), ('bossserpentinehead_943', 10, '16', 'sh_stun_l'), ('bossserpentinehead_943', 11, '8', 'sh_fire'),
       ('bossserpentinehead_943', 12, '0', 'sh_die'), ('bossserpentinehead_943', 12, '16', 'sh_die_l'),
       ('bosssnakebody_989', 0, '0', 'sb_idle'), ('bosssnakebody_989', 0, '8', 'sb_skull'), ('bosssnakebody_989', 7, '0', 'sb_fly'), ('bosssnakebody_989', 8, '0', 'sb_fly8'),
       ('bosssnakehitbox_1002', 0, '8', 'sx_rest'), ('bosssnakehitbox_1002', 7, '0', 'sx_arc'), ('bosssnakehitbox_1002', 7, '16', 'sx_arc_l'),
       ('bosssnakehitbox_1002', 10, '0', 'sx_stun'), ('bosssnakehitbox_1002', 10, '16', 'sx_stun_l'), ('bosssnakehitbox_1002', 11, '8', 'sx_fire'),
       ('bossboulder_1099', 0, '0', 'bboulder'), ('effectwarning_1100', 4, '0', 'warning'),   # guiboss_1087 dropped: never visible in the final game (U-G1, session 10)
       ('breakablerockpiece_432', 0, '0', 'rockpiece'), ('breakabledebris1_1098', 0, '0', 'brkdebris'),
       ('levellayer016_983', 0, '0', 'loop016'), ('levellayer0loop04_981', 0, '0', 'loop0q'), ('levellayer115_984', 0, '0', 'loop115'), ('levellayer1loop03_982', 0, '0', 'loop1q'),
       ('waterfallback_1036', 0, '0', 'wfback'), ('waterfallfront_1037', 0, '0', 'wffront'), ('waterfallfront2_1038', 0, '0', 'wffront2'),
       ('waterfallfront3_1040', 0, '0', 'wffront3'), ('waterfallfront4_1059', 0, '0', 'wffront4'),
       ('levelgeyserplate_994', 0, '0', 'geyser'), ('levelfloatingplatform_1027', 0, '0', 'floatplat'), ('spiraltunnelmask_889', 0, '0', 'spiralmask'), ('levelballtunnel1_1032', 0, '0', 'balltunnel'),
       ('spiraltunnelfront_974', 0, '0', 'spiralfront'), ('frontrock1_946', 0, '0', 'frontrock1'), ('fronttree_947', 0, '0', 'fronttree'), ('treefront_1041', 0, '0', 'treefront'), ('spinninglogmask_1011', 0, '0', 'slogmask'), ('effectsdust_437', 4, '0', 'dust')] + \
      [('spinninglog_1010', 0, d, 'slog%d' % (int(d) // 4)) for d in ('0', '4', '8', '12', '16', '20', '24', '28')] + \
      [(c, ai, '0', '%s%s' % (nm, sfx)) for c, nm in (('shieldwood_342', 'shw'), ('shieldearth_443', 'she'), ('shieldwater_439', 'shwa'),
                                                       ('shieldmagnetic_344', 'shm'), ('shieldflamefront_343', 'shff'))
       for ai, sfx in ((0, ''), (7, '_in'), (8, '_out'))] + \
      [('shieldflameback_376', 0, '0', 'shfb'), ('monitoricon_363', 0, '0', 'monicon'), ('effectsshield_440', 0, '0', 'shpart'),
       ('monitorwoodshield_441', 0, '0', 'mon_wood'), ('monitorearthshield_442', 0, '0', 'mon_earth'), ('monitorfireshield_359', 0, '0', 'mon_fire'),
       ('monitormetalshield_360', 0, '0', 'mon_metal'), ('monitorwatershield_361', 0, '0', 'mon_water'),
       ('effectswatersplash_379', 4, '0', 'splash'), ('bubblepatch_804', 0, '0', 'bpatch'), ('bubblepatch_804', 7, '0', 'bpatch_open'),
       ('bubblepatch_804', 8, '0', 'bpatch_close'), ('mediumbubble_805', 0, '0', 'mbubble'), ('generalwaterhorizon_377', 0, '0', 'horizon')] + \
      [('bubblenumbers_405', 0, d, 'bnum%s' % d) for d in ('0', '1', '2', '3', '4', '5')] + \
      AT_ENS + RM_ENS
ENS += MT_ENS   # RM5 end (rm_mantalith.c)
# Fortune Night flow (session 17, src/game/fn_flow.c): NPCSerpentine_945 in FN3's exit pass (7715 anim 9, 7719 15, 7720 11),
# json frame 32 only (the class is placed in other stages too)
FN_FLOW_ENS = [('npcserpentine_945', a, '0', nm) for a, nm in ((0, 'fnserp_idle'), (9, 'fnserp_dash'), (15, 'fnserp_brake'), (11, 'fnserp_taunt'))]
ENS += FN_FLOW_ENS
ENS_FRAMES['npcserpentine_945'] = {32}
ENS_WITH = {**AT_ENS_WITH, **RM_ENS_WITH, 'guiweaponslot_426': ('objectdigspot',), 'enemysporeprojectile_1022': ('enemysporepod', 'enemyhopper'), 'objectkeystone_429': ('enemysporepod', 'enemyturretus', 'monitorkeystone', 'enemyhopper', 'enemyfirespitter', 'enemysentinel', 'enemyextractorhead'),
            'effectsdebris_427': ('enemysporepod', 'bossserpentinehead', 'objectdigspot', 'objectpot', 'objectbluepot', 'enemyhopper', 'enemyfirespitter', 'enemysentinel'), 'effectsexplosion06_435': ('minibosswretchnidbase', 'minibosstredd', 'bossserpentinehead', 'monitorbomb', 'minibossironworm', 'bosschompersnake', 'enemyextractorhead'),
            'effectsshade_436': ('enemygansekijr', 'enemydragonfly', 'enemystarflounder', 'enemycacteon', 'enemyiris', 'enemyslug'),
            'minibosstreddbomb_1060': ('minibosstredd',), 'minibosstredddebris_1058': ('minibosstredd',), 'monitoricon_363': ('minibosstredd', 'shieldmonitor_onmap'),
            'bossfireball_1003': ('bossserpentinehead',), 'bossboulder_1099': ('bossgiantboulder',), 'effectwarning_1100': ('bossgiantboulder',),
            'breakablerockpiece_432': ('bosstunnel2', 'bosstunnel'), 'breakabledebris1_1098': ('breakable1',), 'effectsdust_437': ('pushableblock1',), 'effectswatersplash_379': ('water_level',), 'bubblepatch_804': ('water_level',), 'mediumbubble_805': ('water_level',), 'generalwaterhorizon_377': ('water_level',), 'bubblenumbers_405': ('water_level',), 'shieldwood_342': ('shieldmonitor_onmap',), 'shieldearth_443': ('shieldmonitor_onmap',), 'shieldwater_439': ('shieldmonitor_onmap',), 'shieldmagnetic_344': ('shieldmonitor_onmap',), 'shieldflamefront_343': ('shieldmonitor_onmap',), 'shieldflameback_376': ('shieldmonitor_onmap',), 'effectsshield_440': ('shieldmonitor_onmap',)}

# ---- Relic Maze acts 3-5 gimmicks (spec_rm_gimmicks_b.md; src/game/relic_gimb.c). Types 300-336 mirror src/fp.h. Objects
# of one family share a type; RMB_SUB gives each member's index, written in the lvobj anim field (write_lv2).
RMB_TYPES_ID = {
    ('gimmickmovingpillar1', 1395): 300, ('gimmickmovingpillar2', 1396): 300, ('gimmickmovingpillar3', 1397): 300,
    ('gimmickhangingidleswitch', 1268): 301, ('gimmickhangingswitch', 1398): 302,
    ('gimmickrisingswing', 1392): 303, ('gimmickrisingswingpiece', 1393): 304, ('gimmickpickaxe', 1335): 305,
    ('gimmickpickaxehandle', 1336): 306, ('gimmickpickaxebase', 1337): 306, ('gimmickpickaxebase2', 1383): 306,
    ('gimmickrelicflippanel2', 1245): 307, ('gimmickspinnycup', 1380): 308, ('gimmickelevator', 1340): 309, ('gimmickelevator2', 1399): 310,
    ('levelhandcar', 1421): 311, ('levelhandcarhandle', 1420): 312, ('levelhandcartop', 1422): 312,
    ('levelhandcarfloor1', 1423): 313, ('levelhandcarfloor2', 1424): 313, ('levelhandcarfloor3', 1425): 313, ('levelminetrackswitch', 1419): 314,
    ('switchwallplate', 1149): 315, ('switchwallplate2', 1429): 315, ('switchwallplate', 1526): 315, ('switchwallplate', 1543): 315, ('switchwallplate2', 1578): 315,
    ('levelmovingplatform01', 1389): 318, 
    ('gimmickrollingplatformcave', 1449): 321, ('gimmickrollingplatformcave2', 1447): 322,
    ('gimmickdrillplatform', 1480): 323, ('breakableruins', 1450): 324, ('levelrmplatform', 1517): 325, ('gatewayfront', 1524): 326, ('gatewayfront2', 1525): 327,
    ('cavesoilcraterfront', 1379): 328, ('cavesoilcraterfront2', 1385): 328,
    ('levellayer001', 1441): 329, ('levellayer002', 1442): 329, ('levellayer015', 1443): 329, ('levellayer02', 1477): 329, ('levellayer03', 1494): 329,
    ('levellayer117', 1488): 330, ('levellayer1loop2', 1489): 330,
    ('lightstonebase', 1562): 331, ('lightstonebase2', 1564): 331, ('lightstonetop', 1563): 332, ('lightstonetop2', 1565): 332,
    ('lightstonedoor1', 1585): 333, ('lightstonedoor2', 1586): 333, ('spikeceiling', 1557): 334, ('levelcrushdeath', 1627): 335, ('objectchair', 1596): 336}
RMB_SUB = {'gimmickmovingpillar2_1396': 1, 'gimmickmovingpillar3_1397': 2, 'gimmickpickaxebase_1337': 1, 'gimmickpickaxebase2_1383': 2,
           'levelhandcartop_1422': 1, 'levelhandcarfloor2_1424': 1, 'levelhandcarfloor3_1425': 2,
           'switchwallplate2_1429': 1, 'switchwallplate_1526': 2, 'switchwallplate_1543': 3, 'switchwallplate2_1578': 4,
           'levelfloatingplatform3_1403': 1, 'rmalternatepath2_1438': 1, 'cavesoilcraterfront2_1385': 1,
           'levellayer002_1442': 1, 'levellayer015_1443': 2, 'levellayer02_1477': 3, 'levellayer03_1494': 4, 'levellayer1loop2_1489': 1,
           'lightstonebase2_1564': 1, 'lightstonetop2_1565': 1, 'lightstonedoor2_1586': 1}
RMB_ENS = [(c, 0, '0', nm) for c, nm in (
    ('gimmickmovingpillar1_1395', 'pil0'), ('gimmickmovingpillar2_1396', 'pil1'), ('gimmickmovingpillar3_1397', 'pil2'), ('gimmickhangingswitch_1398', 'hswitch'),
    ('gimmickrisingswing_1392', 'rswing'), ('gimmickrisingswingpiece_1393', 'rspiece'), ('gimmickpickaxe_1335', 'pickaxe'),
    ('gimmickpickaxehandle_1336', 'pxpart0'), ('gimmickpickaxebase_1337', 'pxpart1'), ('gimmickpickaxebase2_1383', 'pxpart2'),
    ('gimmickrelicflippanel2_1245', 'marker2'), ('gimmickspinnycup_1380', 'cup'), ('gimmickelevator_1340', 'elev'), ('gimmickelevator2_1399', 'elev2'),
    ('levelhandcar_1421', 'cart'), ('levelhandcarhandle_1420', 'cartpart0'), ('levelhandcartop_1422', 'cartpart1'),
    ('levelhandcarfloor1_1423', 'cartprobe0'), ('levelhandcarfloor2_1424', 'cartprobe1'), ('levelhandcarfloor3_1425', 'cartprobe2'), ('levelminetrackswitch_1419', 'tswitch'),
    ('switchwallplate_1149', 'rmplate0'), ('switchwallplate2_1429', 'rmplate1'), ('switchwallplate_1526', 'rmplate2'), ('switchwallplate_1543', 'rmplate3'), ('switchwallplate2_1578', 'rmplate4'),
    ('breakablespike_1436', 'spikebit'), ('levelfloatingplatform2_1311', 'floatbox0'), ('levelfloatingplatform3_1403', 'floatbox1'), ('levelmovingplatform01_1389', 'movplat'),
    ('active_1338', 'floorbox'), ('rmalternatepath_1426', 'altpath0'), ('rmalternatepath2_1438', 'altpath1'),
    ('gimmickrollingplatformcave_1449', 'rcave'), ('gimmickrollingplatformcave2_1447', 'rcave2'), ('gimmickdrillplatform_1480', 'drill'),
    ('breakableruins_1450', 'ruins'), ('breakableruinspiece_1451', 'ruinpiece'), ('levelrmplatform_1517', 'rmplat'), ('gatewayfront_1524', 'gate'), ('gatewayfront2_1525', 'gate2'),
    ('cavesoilcraterfront_1379', 'crater0'), ('cavesoilcraterfront2_1385', 'crater1'),
    ('levellayer001_1441', 'rmloop0_0'), ('levellayer002_1442', 'rmloop0_1'), ('levellayer015_1443', 'rmloop0_2'), ('levellayer02_1477', 'rmloop0_3'), ('levellayer03_1494', 'rmloop0_4'),
    ('levellayer117_1488', 'rmloop1_0'), ('levellayer1loop2_1489', 'rmloop1_1'),
    ('lightstonebase_1562', 'lsbase0'), ('lightstonebase2_1564', 'lsbase1'), ('lightstonetop_1563', 'lstop0'), ('lightstonetop2_1565', 'lstop1'),
    ('lightstonedoor1_1585', 'lsdoor0'), ('lightstonedoor2_1586', 'lsdoor1'), ('levelcrushdeath_1627', 'crushbox'), ('objectchair_1596', 'chair'))] + \
    [('gimmickpickaxe_1335', 0, '1', 'pickaxe_b'), ('gimmickpickaxebase_1337', 0, '1', 'pxpart1_b'), ('gimmickpickaxebase2_1383', 0, '1', 'pxpart2_b'),
     ('gimmickelevator_1340', 1, '0', 'elev_on'), ('gimmickdrillplatform_1480', 0, '24', 'drill_done'),
     ('lightstonebase_1562', 0, '5', 'lsbase0_lit'), ('lightstonebase2_1564', 0, '5', 'lsbase1_lit'),
     ('lightstonetop_1563', 0, '8', 'lstop0_lit'), ('lightstonetop2_1565', 0, '8', 'lstop1_lit')] + \
    [('lightstonebase_1562', 7, str(d), 'lsbase0_c%d' % d) for d in range(6)] + [('lightstonebase2_1564', 7, str(d), 'lsbase1_c%d' % d) for d in range(6)] + \
    [('spikeceiling_1557', 0, str(d), 'sceil%d' % d) for d in range(5)] + \
    [('objectjumppad_1402', 0, '0', 'jpad_rm'), ('objectjumppad_1402', 0, '1', 'jpad_rm_lit'), ('objectjumppad_1402', 7, '0', 'jpad_rm7'), ('objectjumppad_1402', 7, '1', 'jpad_rm7_lit')]
RMB_ENS_WITH = {'breakablespike_1436': ('levelhandcar',), 'breakableruinspiece_1451': ('breakableruins',)}
# rows only in the acts that own these handles (their classes are placed by other stages too)
RMB_ENS_FRAMES = {c: {27, 28, 29} for c in ('objectjumppad_1402', 'gimmickmovingpillar1_1395', 'gimmickmovingpillar2_1396', 'gimmickmovingpillar3_1397')}
RMB_ENS_FRAMES.update({'switchwallplate_1149': {25, 26, 27}, 'switchwallplate2_1429': {27}, 'switchwallplate_1526': {28}, 'switchwallplate_1543': {29}, 'switchwallplate2_1578': {29},
                       'gimmickhangingswitch_1398': {27, 28}, 'gimmickrelicflippanel2_1245': {27, 28}, 'breakableruins_1450': {28, 29}, 'breakableruinspiece_1451': {28, 29},
                       'levellayer001_1441': {28}, 'levellayer002_1442': {28}, 'levellayer015_1443': {28}, 'levellayer02_1477': {28}, 'levellayer03_1494': {28}})
ENGINE_TYPES_ID.update(RMB_TYPES_ID)
ENS += RMB_ENS
ENS_WITH.update(RMB_ENS_WITH)
ENS_FRAMES.update(RMB_ENS_FRAMES)
# Relic Maze act 3 Claw sequence (CLAW_* above): one merge line per table
ENGINE_TYPES.update(CLAW_TYPES); ENGINE_TYPES_ID.update(CLAW_TYPES_ID); ENS += CLAW_ENS; ENS_FRAMES.update(CLAW_ENS_FRAMES)
for _k, _v in CLAW_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v

for _k, _v in MT_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # RM5 end (rm_mantalith.c)
# ---- FN_ENEMIES: Fortune Night enemies (spec_fn_enemies.md 13; src/game/fn_enemy.c), engine types 512-559 (fp.h FN enemy
# block). Facing sets: '' direction 0, '_l' 16. A 6th element = rows exported only when one of those classes is placed
# (the keystone directions and debris frames of the FN crashes: other stages' banks stay unchanged)
FN_ENEMY_TYPES = {'enemycoil': 512, 'enemycoilhead': 513, 'enemycoilpiece1': 514, 'enemycoilpiece2': 515, 'enemycoilpiece3': 516,
    'enemycoilpiece4': 517, 'enemycoilflip': 518, 'enemytrafficle': 519, 'enemytrafficle1': 520, 'enemytrafficle2': 521, 'enemytrafficle3': 522,
    'enemysuperturretus': 523, 'enemyflarecannon': 524, 'enemyroller': 525, 'enemyghettoshell': 526, 'enemyduster': 527, 'enemydusterarm2': 528,
    'enemydusterarm1': 529, 'enemydustermouth': 530, 'enemyballoon': 531, 'pigeon': 532, 'effectflareburst': 533}
FN_ENEMY_KS = ('enemycoilhead', 'enemysuperturretus', 'enemyflarecannon')
FN_ENEMY_ENS = [(c, a, '0', nm) for c, a, nm in (
    ('enemycoil_1742', 0, 'fc_base'), ('enemycoilhead_1743', 0, 'fc_head'), ('enemycoilhead_1743', 7, 'fc_zap'), ('enemycoilhead_1743', 8, 'fc_chg'),
    ('enemycoilhead_1743', 10, 'fc_post'), ('enemycoilhead_1743', 11, 'fc_hdead'), ('enemycoilpiece1_1738', 0, 'fc_p1'), ('enemycoilpiece2_1740', 0, 'fc_p2'),
    ('enemycoilpiece3_1739', 0, 'fc_p3'), ('enemycoilpiece4_1741', 0, 'fc_p4'), ('enemycoilflip_1759', 0, 'fc_flip'), ('enemytrafficle_1760', 0, 'ft_pole'),
    ('enemyflarecannon_1808', 0, 'fk_idle'), ('enemyflarecannon_1808', 7, 'fk_fire'), ('enemyflarecannon_1808', 8, 'fk_dead'), ('enemyflare_1809', 0, 'fk_flare'),
    ('enemyflare_1809', 7, 'fk_fall'), ('enemyflare_1809', 8, 'fk_blast'), ('effectflareburst_1810', 4, 'fk_burst'), ('enemyduster_1938', 0, 'du_body'),
    ('enemydusterarm2_1935', 0, 'du_arm2'), ('enemydusterarm1_1936', 0, 'du_arm1'), ('enemydustermouth_1937', 0, 'du_mouth'),
    ('enemyballoon_1945', 0, 'bl_float'), ('enemyballoon_1945', 8, 'bl_pop'))] + \
    [(c, a, d, nm + ('' if d == '0' else '_l')) for c, a, nm in (
    ('enemytrafficle1_1761', 0, 'ft_l1'), ('enemytrafficle2_1762', 0, 'ft_l2'), ('enemytrafficle3_1763', 0, 'ft_l3'), ('enemysuperturretus_1833', 0, 'st_fly'),
    ('enemysuperturretus_1833', 7, 'st_shoot'), ('enemysuperturretus_1833', 8, 'st_dead'), ('enemysuperturretus_1833', 10, 'st_brake'),
    ('enemyroller_1934', 0, 'ro_idle'), ('enemyroller_1934', 1, 'ro_roll'), ('enemyroller_1934', 7, 'ro_jump'), ('enemyroller_1934', 8, 'ro_dead'),
    ('enemyroller_1934', 10, 'ro_land'), ('enemyghettoshell_1988', 0, 'gs_idle'), ('enemyghettoshell_1988', 8, 'gs_dead'), ('enemyghettoshell_1988', 10, 'gs_blast'),
    ('enemysoundwave_1989', 0, 'gs_wave'), ('pigeon_1641', 0, 'pg_sit'), ('pigeon_1641', 1, 'pg_fly'), ('pigeon_1641', 7, 'pg_peck')) for d in ('0', '16')] + \
    [('enemytrafficle%d_%d' % (k, 1760 + k), 8, '0', 'ft_l%d_dead' % k) for k in (1, 2, 3)] + [('enemylaser_1781', 0, d, 'ft_laser' + d) for d in ('0', '1', '2')] + \
    [('pigeon_1641', 7, d, 'pg_peck' + d) for d in ('1', '17')] + [('objectkeystone_429', 0, d, 'keystone' + d, None, (c,)) for d, c in (('2', 'enemycoilhead'), ('3', 'enemyflarecannon'), ('4', 'enemysuperturretus'))] + \
    [('effectsdebris_427', 0, '0', 'debris_fnh', slice(41, 45), ('enemycoilhead',)), ('effectsdebris_427', 0, '0', 'debris_fns', slice(57, 61), ('enemyghettoshell',))]
FN_ENEMY_ENS_WITH = {'enemylaser_1781': ('enemytrafficle1', 'enemytrafficle2', 'enemytrafficle3'), 'enemyflare_1809': ('enemyflarecannon',),
    'effectflareburst_1810': ('enemyflarecannon',), 'enemysoundwave_1989': ('enemyghettoshell',), 'objectkeystone_429': FN_ENEMY_KS + ('enemyghettoshell',),
    'effectsdebris_427': FN_ENEMY_KS + ('enemyghettoshell',), 'effectsexplosion06_435': ('enemyghettoshell', 'enemyduster'),
    'effectshade_436': ('enemytrafficle1', 'enemytrafficle2', 'enemytrafficle3', 'enemyroller')}
ENGINE_TYPES.update(FN_ENEMY_TYPES); ENS += FN_ENEMY_ENS   # FN_ENEMIES
for _k, _v in FN_ENEMY_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # FN_ENEMIES

# ---- FN_GIMA: Fortune Night gimmicks, acts 1-3 (build/re/spec_fn_gimmicks_a.md; src/game/fn_gima.c). Types 560-605 mirror
# src/fp.h (OT_FNA_FIRST..OT_FNA_LAST). Families share a type; the lvobj anim field carries the member index (FNA_SUB, bits
# 0-5), per-frame instance flags (FNA_FLAGS, bits 6-7: the act's own event copies) and the instance layer (bits 8-15).
# Reused here by class + handle: the FN chair 1797 = the RM chair type 336 (relic_gimb.c chairs, RE 7272 ~ 6585), the
# gachapon hanging switch 1268 = 301 (already in RMB_TYPES_ID) and its rope 1850 = 264 (ENGINE_TYPES 'gimmickhangingswitch').
FNA_TYPES_ID = {
    ('gimmickdragontunnel1', 1734): 560, ('gimmickdragontunnel2', 1733): 560, ('gimmickdragontunnel1', 1860): 561,
    ('gimmickdragontunnelhead', 1737): 562, ('gimmickdragontunnelhead2', 1783): 562,
    ('gimmickdragontunnelmask1', 1798): 563, ('gimmickdragontunnelmask2', 1799): 563,
    ('dragontunnelhide1', 1782): 564, ('dragontunnelhide2', 1859): 564, ('dragontunnelhide3', 1787): 564,
    ('levelferriswheel1', 1774): 565, ('levelferriswheel2', 1775): 565, ('levelferriswheel3', 1776): 565, ('levelferriswheel4', 1777): 565,
    ('levelferriswheelpiece', 1778): 566, ('goldplatform1', 1684): 567, ('goldplatform2', 1685): 567, ('goldplatform3', 1720): 567,
    ('gimmickrollingarchbase', 1659): 568, ('gimmickrollingarch', 1661): 569, ('gimmickrollingarch2', 1663): 570,
    ('gimmickboostblock', 1728): 571, ('gimmickboostblockflame', 1731): 572, ('gimmickboostblockturn', 1730): 573,
    **{('gimmicklanternbridge%d' % k, 1667 + k): 574 for k in range(1, 9)},
    ('gimmickverticalhalfpipe1', 1828): 575, ('gimmickverticalhalfpipe2', 1829): 575, ('gimmickverticalhalfpipe3', 1830): 575,
    ('gimmickverticalhalfpipe4', 1838): 575, ('gimmickverticalhalfpipe5', 1839): 575, ('gimmickverticalhalfpipe6', 1840): 575,
    ('gimmickteetertotter', 1831): 576, ('breakablewall', 1637): 577, ('breakablefloor1', 1766): 579, ('breakablefloor2', 1767): 579,
    ('breakablefloor1', 1996): 579, ('breakablefloor2', 1997): 579,   # FN5's samurai arena (same objects 922 / 923, own images; session 18)
    ('fireworklauncher', 1735): 581, ('gimmickclothesline2', 1702): 583, ('gimmickhanginglongsensor2', 1703): 584, ('flag', 1701): 585,
    ('paperlantern', 527): 586, ('floatinglantern', 1846): 587, ('gachaponwheel', 1851): 588, ('gachapondoor', 1852): 589,
    ('gachaponfront', 1854): 590, ('signstop', 1698): 591, ('signyield', 1697): 591, ('lamppostfront', 1791): 592,
    ('strobelight', 1857): 594, ('largesign1', 1755): 595, ('largesign2', 1757): 595, ('largesign3', 1849): 595, ('largesign3', 1893): 595,
    ('largecolumn', 1864): 596, ('largecolumn2', 1874): 596, ('ramp1', 1870): 597, ('ramp2', 1871): 597,
    ('leveltrollycar', 1803): 598, ('leveltrolleyplatform', 1806): 599, ('leveltrolleyplatform2', 1807): 599,
    ('leveltrollycarlight', 1812): 600, ('leveltrollycarlight2', 1813): 600,
    ('layer0loop', 1708): 601, ('layer0loop2', 1711): 601, ('layer0loop3', 1856): 601, ('layer0block', 1884): 601,
    ('layer1loop', 1713): 602, ('layer1loop2', 1712): 602, ('layer1loop3', 1719): 602, ('bgstar', 16): 603,
    ('objectchair', 1797): 336}
FNA_SUB = {'gimmickdragontunnel2_1733': 1, 'gimmickdragontunnelhead2_1783': 1, 'gimmickdragontunnelmask2_1799': 1,
           'dragontunnelhide2_1859': 1, 'dragontunnelhide3_1787': 2, 'levelferriswheel2_1775': 1, 'levelferriswheel3_1776': 2,
           'levelferriswheel4_1777': 3, 'goldplatform2_1685': 1, 'goldplatform3_1720': 2, 'breakablefloor2_1767': 1, 'breakablefloor2_1997': 1,
           'signyield_1697': 1, 'largesign2_1757': 1, 'largesign3_1849': 2, 'largesign3_1893': 3, 'largecolumn2_1874': 1, 'ramp2_1871': 1,
           'leveltrolleyplatform2_1807': 1, 'leveltrollycarlight2_1813': 1, 'layer0loop2_1711': 1, 'layer0loop3_1856': 2,
           'layer0block_1884': 3, 'layer1loop2_1712': 1, 'layer1loop3_1719': 2,
           **{'gimmicklanternbridge%d_%d' % (k, 1667 + k): k - 1 for k in range(1, 9)},
           **{'gimmickverticalhalfpipe%d_%d' % (k, h): k - 1 for k, h in ((1, 1828), (2, 1829), (3, 1830), (4, 1838), (5, 1839), (6, 1840))}}
# per-act event copies (RE): 0x40 = this half-pipe wall is only solid in M38 (A2 7535 / 7536: hp2; A3 7703 / 7704: hp2 and hp5);
# 0x80 = entering at hp1 also needs B < 0 (A3 7705). Keys (class_handle, json frame).
FNA_FLAGS = {('gimmickverticalhalfpipe2_1829', 31): 0x40, ('gimmickverticalhalfpipe2_1829', 32): 0x40, ('gimmickverticalhalfpipe5_1839', 32): 0x40,
             ('gimmickverticalhalfpipe1_1828', 32): 0x80}
# per-act constants as parameter objects (type 604 at x = value, y = key): key 1 = the floating lanterns' start X (MV.X >, A2 group
# +0x2262c from 7589's gate, A3 7792)
FNA_PARAMS = {31: ((1, 8060),), 32: ((1, 4400),)}
FNA_ENS = [(c, 0, '0', nm) for c, nm in (
    ('gimmickdragontunnel1_1734', 'fndseg0'), ('gimmickdragontunnel2_1733', 'fndseg1'), ('gimmickdragontunnel1_1860', 'fndsegp'),
    ('gimmickdragontunnelhead_1737', 'fndhead0'), ('gimmickdragontunnelhead2_1783', 'fndhead1'),
    ('gimmickdragontunnelmask1_1798', 'fndmask0'), ('gimmickdragontunnelmask2_1799', 'fndmask1'),
    ('dragontunnelhide1_1782', 'fndhide0'), ('dragontunnelhide2_1859', 'fndhide1'), ('dragontunnelhide3_1787', 'fndhide2'),
    ('levelferriswheel1_1774', 'fnfcar0'), ('levelferriswheel2_1775', 'fnfcar1'), ('levelferriswheel3_1776', 'fnfcar2'),
    ('levelferriswheel4_1777', 'fnfcar3'), ('levelferriswheelpiece_1778', 'fnfspoke'),
    ('goldplatform1_1684', 'fngold0'), ('goldplatform2_1685', 'fngold1'), ('goldplatform3_1720', 'fngold2'),
    ('gimmickrollingarchbase_1659', 'fnarchbase'), ('gimmickrollingarch_1661', 'fnarch'), ('gimmickrollingarch2_1663', 'fnarchwheel'),
    ('gimmickboostblock_1728', 'fnboost'), ('gimmickboostblockturn_1730', 'fnbturn'),
    ('gimmickteetertotter_1831', 'fnteeter0'), ('breakablewall_1637', 'fnbwall'), ('breakablewallpiece_1638', 'fnbwpiece'),
    ('breakablefloor1_1766', 'fnbfloor0'), ('breakablefloor2_1767', 'fnbfloor1'), ('breakablefloorpiece_1764', 'fnbfpiece'), ('breakablefloorpiece_1995', 'fnbfpiece'),
    ('fireworklauncher_1735', 'fnfwl'), ('fireworkparticle_1736', 'fnfwp'), ('gimmickclothesline2_1702', 'fnclothes'),
    ('gimmickhanginglongsensor2_1703', 'fnclstop'), ('flag_1701', 'fnflag'), ('paperlantern_527', 'fnplantern'),
    ('floatinglantern_1846', 'fnflantern'), ('gachaponwheel_1851', 'fngwheel0'),
    ('gachapondoor_1852', 'fngdoor'), ('gachaponfront_1854', 'fngfront0'), ('signstop_1698', 'fnsign0'), ('signyield_1697', 'fnsign1'),
    ('lamppostfront_1791', 'fnlamp'), ('strobelight_1857', 'fnstrobe'),
    ('largesign1_1755', 'fnlsign0'), ('largesign2_1757', 'fnlsign1'), ('largesign3_1849', 'fnlsign2'), ('largesign3_1893', 'fnlsign3'),
    ('largecolumn_1864', 'fncolumn0'), ('largecolumn2_1874', 'fncolumn1'), ('ramp1_1870', 'fnramp0'), ('ramp2_1871', 'fnramp1'),
    ('leveltrolleyplatform_1806', 'fntplat0'), ('leveltrolleyplatform2_1807', 'fntplat1'),
    ('leveltrollycarlight_1812', 'fntlight0'), ('leveltrollycarlight2_1813', 'fntlight1'),
    ('layer0loop_1708', 'fnloop0_0'), ('layer0loop2_1711', 'fnloop0_1'), ('layer0loop3_1856', 'fnloop0_2'), ('layer0block_1884', 'fnloop0_3'),
    ('layer1loop_1713', 'fnloop1_0'), ('layer1loop2_1712', 'fnloop1_1'), ('layer1loop3_1719', 'fnloop1_2'), ('bgstar_16', 'fnstar'))] + \
    [('gimmicklanternbridge%d_%d' % (k, 1667 + k), 0, d, 'fnlb%d_%s' % (k - 1, d)) for k in range(1, 9) for d in ('0', '1')] + \
    [('gimmickverticalhalfpipe%d_%d' % (k, h), 0, '0', 'fnhpipe%d' % (k - 1)) for k, h in ((1, 1828), (2, 1829), (3, 1830), (4, 1838), (5, 1839), (6, 1840))] + \
    [('gimmickboostblock_1728', 7, d, 'fnboost7_' + d) for d in ('0', '8', '16', '24')] + \
    [('gimmickboostblockflame_1731', 0, d, 'fnbflame' + d) for d in ('0', '8', '16', '24')] + \
    [('gimmickteetertotter_1831', 0, '16', 'fnteeter16'), ('fireworkparticle_1736', 7, '0', 'fnfwp7'), ('lamppostfront_1791', 8, '0', 'fnlamp8'),
     ('gachaponwheel_1851', 0, '1', 'fngwheel1'), ('gachaponwheel_1851', 0, '2', 'fngwheel2'), ('gachaponwheel_1851', 0, '4', 'fngwheel4'),
     ('gachaponfront_1854', 0, '1', 'fngfront1'), ('effectsdust01_369', 4, '0', 'fndust'), ('effectsdebris_427', 0, '0', 'fnlampdeb', slice(61, 66))] + \
    [('objectkeystone_429', 0, str(d), 'fnks%d' % d) for d in (1, 2, 3, 4)] + \
    [('objectjumppad_1529', 0, '0', 'jpad_rm'), ('objectjumppad_1529', 0, '1', 'jpad_rm_lit')]   # FN's pad handle: its own images (carol_bike.inc)
FNA_ENS_IMG = [('leveltrollycar_1803', 'fntrolley', [6914])]   # SystemBox (no animation table): its image as a one-frame row
FNA_ENS_WITH = {'breakablewallpiece_1638': ('breakablewall',), 'breakablefloorpiece_1764': ('breakablefloor1', 'breakablefloor2'),
                'breakablefloorpiece_1995': ('breakablefloor1', 'breakablefloor2'),
                'fireworkparticle_1736': ('fireworklauncher',), 'effectsdust01_369': ('gimmickboostblock',),
                'effectsdebris_427': ('lamppostfront',), 'objectkeystone_429': ('gachaponwheel',)}
FNA_ENS_FRAMES = {'objectjumppad_1529': {30, 31, 32},
                  'breakablefloorpiece_1764': {30, 31, 32, 35}, 'breakablefloorpiece_1995': {34}}   # FN5 8343-8346 create piece 1995 (its own image)
ENGINE_TYPES_ID.update(FNA_TYPES_ID); ENS += FNA_ENS; ENS_FRAMES.update(FNA_ENS_FRAMES)
for _k, _v in FNA_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v
# ---- FN_GIMB: Fortune Night gimmicks, acts 4-5 (build/re/spec_fn_gimmicks_b.md; src/game/fn_gimb.c). Types 680-707 mirror
# src/fp.h (OT_FNB_FIRST..OT_FNB_LAST). Families share a type; the lvobj anim field carries the member index (FNB_SUB, bits
# 0-7) and the instance layer (bits 8-15, write_lv2). Reused by class + handle: SwitchWallPlate_1149 = RM's plate type 315
# (relic_gimb.c presses it, the FN rows add its sprite), GimmickHangingLongSensor2_1703 = the acts 1-3 rope stop (584, the
# same type in both FN blocks: clothesline 1987 and clothesline2 1702 share the stops). Every row is limited to the A4 / A5
# frames (json 33 / 34): several classes are placed elsewhere under other handles (RM bars, DV moving platform 938, RM elevator).
FNB_TYPES_ID = {
    ('escalatorfront1', 1906): 680, ('escalatorfront2', 1914): 680, ('escalatorright', 1908): 681, ('escalatorleft', 1915): 681,
    ('fountainfront', 1918): 682, ('fountainfront2', 1919): 682, ('levelwaterplatform', 1921): 683,
    ('gimmickclothesline', 1987): 684, ('gimmickhanginglongsensor2', 1703): 584,
    ('gimmickhangingmovingbar', 1951): 686, ('gimmickhangingmovingsensor', 1950): 687, ('gimmickhangingmovingwheel', 1952): 688,
    ('gimmickhangingmovingstopper', 1949): 689, ('levelmovingplatform1', 1957): 690,
    ('platformstilt1', 1958): 691, ('platformstilt2', 1959): 691, ('platformstilt3', 1960): 691, ('platformstilt4', 1961): 691,
    ('levelmovingplatform2', 1976): 692, ('turningspring', 1944): 693, ('gimmickairballoon', 1991): 694,
    ('gimmickhangingidleballoon', 1992): 695, ('keyboard', 1984): 696, ('discolight', 1932): 697,
    ('objectdiscoball', 1930): 698, ('objectdiscoball2', 1933): 698, ('objectparasol', 1940): 699,
    ('restroommen', 1973): 700, ('restroomwomen', 1974): 700, ('arrowlight', 1971): 701, ('carousel', 2004): 702,
    ('carouselplatform', 2005): 703, ('gimmickelevator', 2035): 704, ('gimmickelevatordoor', 2034): 705,
    ('gimmickpolepodcaseinner', 2001): 706, ('layer0loop4', 1977): 707, ('layer0loop5', 1978): 707, ('layer0loop6', 1983): 707}
FNB_SUB = {'escalatorfront2_1914': 1, 'escalatorleft_1915': 1, 'fountainfront2_1919': 1, 'platformstilt2_1959': 1, 'platformstilt3_1960': 2,
           'platformstilt4_1961': 3, 'objectdiscoball2_1933': 1, 'restroomwomen_1974': 1, 'layer0loop5_1978': 1, 'layer0loop6_1983': 2}
FNB_ENS = [(c, 0, '0', nm) for c, nm in (
    ('escalatorfront1_1906', 'escf0'), ('escalatorfront2_1914', 'escf1'), ('escalatorright_1908', 'escs0'), ('escalatorleft_1915', 'escs1'),
    ('fountainfront_1918', 'fount0'), ('fountainfront2_1919', 'fount1'), ('levelwaterplatform_1921', 'wplat'), ('gimmickclothesline_1987', 'rope'),
    ('gimmickhangingmovingbar_1951', 'tbar'), ('gimmickhangingmovingsensor_1950', 'tsens'),
    ('gimmickhangingmovingwheel_1952', 'twheel'), ('gimmickhangingmovingstopper_1949', 'tstop'), ('levelmovingplatform1_1957', 'lift'),
    ('platformstilt1_1958', 'stilt0'), ('platformstilt2_1959', 'stilt1'), ('platformstilt3_1960', 'stilt2'), ('platformstilt4_1961', 'stilt3'),
    ('levelmovingplatform2_1976', 'mplat2f'), ('turningspring_1944', 'tspr'), ('gimmickairballoon_1991', 'balloon'),
    ('gimmickhangingidleballoon_1992', 'iballoon'), ('keyboard_1984', 'key'), ('objectdiscoball_1930', 'dball0'), ('objectdiscoball2_1933', 'dball1'),
    ('objectparasol_1940', 'parasol'), ('restroommen_1973', 'rroom0'), ('restroomwomen_1974', 'rroom1'), ('arrowlight_1971', 'arrow'),
    ('carousel_2004', 'carousel'), ('carouselplatform_2005', 'carplat'), ('gimmickelevator_2035', 'pod'), ('gimmickelevatordoor_2034', 'poddoor'),
    ('layer0loop4_1977', 'fnloopb_0'), ('layer0loop5_1978', 'fnloopb_1'), ('layer0loop6_1983', 'fnloopb_2'))] + \
    [('escalatorfront1_1906', 0, '16', 'escf0r'), ('escalatorfront2_1914', 0, '16', 'escf1r'), ('gimmickhangingmovingbar_1951', 0, '8', 'tbar_sw'),
     ('turningspring_1944', 8, '0', 'tspr_w'), ('turningspring_1944', 10, '0', 'tspr_t'), ('turningspring_1944', 11, '0', 'tspr_s'),
     ('keyboard_1984', 7, '0', 'key_p'), ('objectparasol_1940', 7, '0', 'parasol_b'), ('gimmickpolepodcaseinner_2001', 0, '16', 'podcase'),
     ('effectsspringbeam_540', 4, '0', 'sbeam0'), ('effectsspringbeam_540', 4, '8', 'sbeam8'), ('effectsspringbeam_540', 4, '24', 'sbeam24'),
     ('effectsheart_444', 7, '0', 'note')]
FNB_ENS_WITH = {'effectsspringbeam_540': ('turningspring', 'restroommen'), 'effectsheart_444': ('keyboard',), 'effectswaterhit_756': ('levelwaterplatform',)}
FNB_ENS_FRAMES = {e[0]: {33, 34} for e in FNB_ENS}
FNB_ENS_FRAMES['switchwallplate_1149'] = RMB_ENS_FRAMES['switchwallplate_1149'] | {33, 34}   # the RM plate's sprite in A4 / A5 (relic_gimb.c)
# the poses the A4 / A5 events force on every skin (RE 8138 trolley swing anim 40 dir 7, 8306-8308 dance anim 40 dir 8, 8193 /
# 8194 restroom anim 40 dir 24): Lilac's, her hair's and Carol's banks get 'a40_d<dir>' rows in these frames only (Milla's bank
# already holds every direction)
FNB_POSE_FRAMES, FNB_POSE_DIRS = {33, 34}, {40: ('7', '8', '24')}
# the extra pose directions per json frame (player_pose 'a<index>_d<dir>' rows): FN_GIMB's, and other stages' (TH_GIM: the barrel)
POSE_DIRS = {n_: FNB_POSE_DIRS for n_ in FNB_POSE_FRAMES}
CAROL_POSE_DIRS = {}   # Carol's skin / tail / bike banks only (Trap Hideout: TH_GIM)
def fnb_pose_rows(entries, n, pre=''):
    out = []
    for ai, dirs in POSE_DIRS.get(n, {}).items():
        for dk in dirs:
            if ai < len(entries) and entries[ai] and dk in entries[ai]:
                d = entries[ai][dk]; out.append({'name': '%sa%d_d%s' % (pre, ai, dk), 'frames': d['frames'], 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
    return out
ENGINE_TYPES_ID.update(FNB_TYPES_ID); ENS += FNB_ENS; ENS_FRAMES.update(FNB_ENS_FRAMES)
for _k, _v in FNB_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v
# ---- FN_BOSS: Fortune Night bosses and minibosses (build/re/spec_fn_bosses.md A-E / X; src/game/fn_boss.c). Types 800-859 mirror
# src/fp.h (OT_FNBOSS_FIRST..OT_FNBOSS_LAST); the lvobj anim field carries the sub id (FNBOSS_SUB) | instance layer << 8. The
# classes are mapped only in FNBOSS_FRAMES (json 30-32, 34): NPCSerpentine_945 / ShieldSerpentine_1872 are placed in other
# stages too. Every sprite row names its creator class (rows exported only where that boss is placed).
FNBOSS_FRAMES = {30, 31, 32, 34}
FNBOSS_TYPES_ID = {('minibossufo', 1772): 800, ('minibossufo2', 1773): 801, ('minibossufohatch1', 1770): 802, ('minibossufohatch2', 1771): 802,
    ('minibossufobeam', 1769): 803, ('minibossufoborder', 1768): 804, ('minibossufodebris', 1875): 805,
    ('minibossbulldozer', 1746): 810, ('minibossbulldozerarm', 1747): 811, ('minibossbulldozerclaw', 1750): 812, ('minibossbulldozerdoor', 1745): 813,
    ('minibossbulldozerwheel1', 1748): 814, ('minibossbulldozerwheel2', 1749): 814, ('minibossbulldozerhitbox', 1752): 815, ('minibosslimiter', 1751): 816,
    ('npcserpentine', 945): 820, ('bossserpentinearm', 1865): 821, ('bossserphitbox', 1873): 822, ('bossserpfloorcheck', 1869): 823,
    ('shieldserpentine', 1872): 824, ('bosschargeglow', 1868): 825,
    ('minibosssamu', 1890): 840, ('minibosssamulefthand', 1888): 841, ('minibosssamurighthand', 1889): 841, ('minibosssamuwheel', 1886): 842,
    ('minibosssamufloorcheck', 1891): 843, ('bossgate', 1894): 845, ('bossgate', 2030): 845}
FNBOSS_SUB = {'minibossufohatch2_1771': 1, 'minibossbulldozerwheel2_1749': 1, 'minibosssamurighthand_1889': 1, 'bossgate_2030': 1}
_U, _D, _S, _M = ('minibossufo',), ('minibossbulldozer',), ('bossserphitbox',), ('minibosssamu',)
FNBOSS_ENS = [(c, a, d, nm, None, w) for c, a, d, nm, w in (
    ('minibossufo_1772', 0, '0', 'fb_ufo', _U), ('minibossufo_1772', 7, '0', 'fb_ufo7', _U), ('minibossufo_1772', 8, '0', 'fb_ufo8', _U),
    ('minibossufo2_1773', 0, '0', 'fb_ufop', _U), ('minibossufohatch1_1770', 0, '0', 'fb_hatch0', _U), ('minibossufohatch2_1771', 0, '0', 'fb_hatch1', _U),
    ('minibossufobeam_1769', 0, '0', 'fb_beam', _U), ('minibossufoborder_1768', 0, '0', 'fb_border', _U), ('minibossufodebris_1875', 0, '0', 'fb_ufodeb', _U),
    ('minibossbulldozerwheel1_1748', 0, '16', 'fb_wheel0', _D), ('minibossbulldozerwheel2_1749', 0, '16', 'fb_wheel1', _D),
    ('minibosslimiter_1751', 0, '0', 'fb_lim', _D), ('minibossboomerang_1753', 0, '0', 'fb_boom', _D),
    ('bossserphitbox_1873', 0, '0', 'fb_shit', _S), ('bossserpfloorcheck_1869', 0, '0', 'fb_sprobe', _S), ('shieldserpentine_1872', 0, '0', 'fb_sshield', _S),
    ('bosschargeglow_1868', 0, '0', 'fb_glow', _S), ('effectwarning2_1895', 0, '0', 'fb_warn2', _S),
    ('bossrapidbullet_1866', 0, '8', 'fb_rb8', _S), ('bossrapidbullet_1866', 0, '24', 'fb_rb24', _S), ('bossmissile_1867', 0, '0', 'fb_mis', _S),
    ('bossmissile_1867', 0, '16', 'fb_mis_l', _S), ('enemylaser_1781', 0, '3', 'fb_laser3', _S + _M), ('enemylaser_1781', 0, '0', 'fb_laser0', _S + _M),
    ('npcserpentine_945', 12, '0', 'fs_12', _S), ('npcserpentine_945', 13, '0', 'fs_13', _S), ('npcserpentine_945', 9, '8', 'fs_9u', _S),
    ('npcserpentine_945', 14, '1', 'fs_14', _S), ('npcserpentine_945', 14, '17', 'fs_14_l', _S),
    ('minibosssamu_1890', 0, '0', 'fb_samu0', _M), ('minibosssamu_1890', 0, '16', 'fb_samu16', _M), ('minibosssamu_1890', 0, '24', 'fb_samu24', _M),
    ('minibosssamu_1890', 8, '24', 'fb_samu8', _M), ('minibosssamu_1890', 9, '0', 'fb_samu9', _M), ('minibosssamulefthand_1888', 0, '0', 'fb_hand0', _M),
    ('minibosssamulefthand_1888', 8, '0', 'fb_hand0_8', _M), ('minibosssamurighthand_1889', 0, '0', 'fb_hand1', _M), ('minibosssamurighthand_1889', 8, '0', 'fb_hand1_8', _M),
    ('minibosssamuwheel_1886', 0, '0', 'fb_swheel', _M), ('minibosssamufloorcheck_1891', 0, '0', 'fb_sprobe2', _M), ('minibosssamudebris_1892', 0, '0', 'fb_sdeb', _M),
    ('bossgate_1894', 0, '0', 'fb_gate0', _M), ('bossgate_2030', 0, '0', 'fb_gate1', _M),
    # the bulldozer keeps direction 0 (E = -1 is never changed, RE): no direction-16 rows
    ('minibossbulldozer_1746', 0, '0', 'fb_dz', _D), ('minibossbulldozer_1746', 8, '0', 'fb_dz8', _D), ('minibossbulldozer_1746', 10, '0', 'fb_dz10', _D),
    ('minibossbulldozerarm_1747', 0, '0', 'fb_dzarm', _D), ('minibossbulldozerclaw_1750', 0, '0', 'fb_dzclaw', _D),
    ('minibossbulldozerdoor_1745', 0, '0', 'fb_door', _D), ('minibossbulldozerdoor_1745', 7, '0', 'fb_door7', _D), ('minibossbulldozerdoor_1745', 8, '0', 'fb_door8', _D),
    ('minibossbulldozerdoor_1745', 10, '0', 'fb_door10', _D), ('minibossbulldozerhitbox_1752', 0, '0', 'fb_dzhit', _D), ('minibossbulldozerhitbox_1752', 8, '0', 'fb_dzhit8', _D),
    ('minibossbulldozerdebris_1754', 0, '0', 'fb_dzdeb', _D))] + \
    [(c, a, d, nm + ('' if d == '0' else '_l'), None, w) for c, a, nm, w in (
    ('npcserpentine_945', 0, 'fs_0', _S), ('npcserpentine_945', 6, 'fs_6', _S), ('npcserpentine_945', 7, 'fs_7', _S), ('npcserpentine_945', 8, 'fs_8', _S),
    ('npcserpentine_945', 9, 'fs_9', _S), ('npcserpentine_945', 15, 'fs_15', _S), ('npcserpentine_945', 16, 'fs_16', _S)) for d in ('0', '16')] + \
    [('bossserpentinearm_1865', 0, d, 'fb_sarm' + d, None, _S) for d in ('1', '2', '3', '17', '18', '19')]
# (EffectWarning_1528 has the same three images as EffectWarning2_1895: the bank keeps one row per image set, fb_warn2 serves both)
FNBOSS_ENS_WITH = {'minibossboomerang_1753': _D, 'minibossbulldozerdebris_1754': _D, 'effectwarning2_1895': _S,
                   'bossrapidbullet_1866': _S, 'bossmissile_1867': _S, 'enemylaser_1781': _S + _M, 'minibossflame_1264': ('npcserpentine',),
                   'minibosssamudebris_1892': _M, 'effectsexplosion06_435': _D + _M + _S + _U, 'effectsdust01_369': _D, 'effectsexplosion01_367': _S}
ENGINE_TYPES_ID.update(FNBOSS_TYPES_ID); ENS += FNBOSS_ENS   # FN_BOSS
for _k, _v in FNBOSS_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # FN_BOSS
# ---- FN_PANTHER: the FN5 intro / chase and the FN6 Panther (build/re/spec_fn_bosses.md F / G / X; src/game/fn_panther.c). Types 860-891
# mirror src/fp.h (OT_FNP_*), one per (class, handle); the rider is 2009 in FN5 and 2039 in FN6 (the rows below name 2039: the
# placed handle's images are taken). Rows are exported for json frames 34 (FN5) and 35 (FN6) only; the runtime objects (plasma,
# missile, rapid bullet, gun spark, speed line) ride with the values object.
FNP_TYPES_ID = {('bosspanthervalues', 2023): 860, ('bosspantherhead', 2013): 861, ('bosspantherjaw', 2014): 862, ('bosspantherfrontcannon', 2015): 863,
    ('bosspantherbackcannon', 2038): 864, ('bosspantherrider', 2039): 865, ('bosspantherrider', 2009): 865, ('bosspanthershoulderf', 2019): 866,
    ('bosspanthershoulderf2', 2020): 867, ('bosspantherhipf', 2016): 868, ('bosspantherkneef', 2017): 869, ('bosspantherlegfl', 2018): 870,
    ('bosspantherlegfr', 2022): 871, ('bosspantherfootfl', 2012): 872, ('bosspantherfootfr', 2021): 873, ('bosspantherribs', 2011): 874,
    ('bosspanthertail', 2010): 875, ('bosspantherhitbox', 2043): 876, ('bosspantherlaser', 2047): 877, ('bosspanthertarget', 2044): 878,
    ('bosschargeglow', 2042): 879, ('bosspantherwheel', 2052): 880, ('scrollingfloor', 2037): 881, ('effectspeedback', 2049): 882,
    ('planeplatform', 2054): 883, ('playerassist', 2045): 884, ('playerassistoffense', 2048): 885, ('playerassistdefense', 2046): 886,
    ('playerassistprojectile', 2051): 887, ('ship1', 2024): 888, ('ship2', 2025): 889, ('shipcable', 2026): 890, ('shipcable2', 2027): 891}
FNP_ENS = [(c, a, d, nm) for c, a, d, nm in (
    ('bosspantherhead_2013', 0, '0', 'fp_head'), ('bosspantherjaw_2014', 0, '0', 'fp_jaw'), ('bosspantherjaw_2014', 7, '0', 'fp_jaw7'),
    ('bosspantherfrontcannon_2015', 0, '0', 'fp_fcan'), ('bosspantherbackcannon_2038', 0, '0', 'fp_bcan'), ('bosspantherbackcannon_2038', 0, '1', 'fp_bcan1'),
    ('bosspantherrider_2039', 0, '0', 'fp_rider'), ('bosspantherrider_2039', 0, '1', 'fp_rider1'), ('bosspantherrider_2039', 0, '2', 'fp_rider2'),
    ('bosspantherrider_2039', 12, '0', 'fp_rider12'), ('bosspantherrider_2039', 13, '0', 'fp_rider13'), ('bosspanthershoulderf_2019', 0, '0', 'fp_shf'),
    ('bosspanthershoulderf2_2020', 0, '0', 'fp_shf2'), ('bosspantherhipf_2016', 0, '0', 'fp_hip'), ('bosspantherkneef_2017', 0, '0', 'fp_knee'),
    ('bosspantherlegfl_2018', 0, '0', 'fp_legfl'), ('bosspantherlegfr_2022', 0, '0', 'fp_legfr'), ('bosspantherfootfl_2012', 0, '0', 'fp_footfl'),
    ('bosspantherfootfr_2021', 0, '0', 'fp_footfr'), ('bosspantherfootfr_2021', 7, '0', 'fp_footfr7'), ('bosspantherfootfr_2021', 8, '0', 'fp_footfr8'),
    ('bosspantherribs_2011', 0, '0', 'fp_rib'), ('bosspanthertail_2010', 0, '0', 'fp_tail'), ('bosspantherwheel_2052', 0, '0', 'fp_wheel'),
    ('bosspantherhitbox_2043', 0, '0', 'fp_hit'), ('bosspantherlaser_2047', 0, '0', 'fp_laser'), ('bosspanthertarget_2044', 0, '0', 'fp_target'),
    ('bosschargeglow_2042', 0, '0', 'fp_glow'), ('effectspeedback_2049', 0, '0', 'fp_sback'), ('effectspeedline_2050', 0, '0', 'fp_sline'),
    ('bossplasmashot_2040', 0, '0', 'fp_plasma'), ('bossmissile_2041', 0, '0', 'fp_missile'), ('bossmissile_2041', 0, '16', 'fp_missile_l'),
    ('bossmissile_2041', 8, '0', 'fp_mblast'), ('bossrapidbullet_1866', 0, '8', 'fp_rbul'), ('bossrapidbullet_1866', 0, '24', 'fp_rbul_d'),
    ('shieldserpentine_1872', 0, '0', 'fp_shield'), ('effectwarning_1528', 0, '0', 'fp_warn'), ('effectgunspark_1887', 4, '0', 'fp_spark'),
    ('scrollingfloor_2037', 0, '0', 'fp_floor'), ('planeplatform_2054', 0, '0', 'fp_plane'), ('playerassist_2045', 0, '0', 'fp_assist'),
    ('playerassist_2045', 0, '1', 'fp_assist1'), ('playerassistoffense_2048', 0, '0', 'fp_aoff0'), ('playerassistoffense_2048', 0, '1', 'fp_aoff1'),
    ('playerassistoffense_2048', 7, '0', 'fp_aoff7'), ('playerassistoffense_2048', 7, '1', 'fp_aoff7b'), ('playerassistdefense_2046', 0, '0', 'fp_adef'),
    # session 18 (src/game/fn_assist.c): Milla in the plane (far pass 1, hold-up 7, drop 11, after 10), the helpers knocked (8),
    # Lilac's purple slashes (PlayerAssistProjectile_2051 0 / 7 / 8)
    ('playerassistdefense_2046', 0, '1', 'fp_adef1'), ('playerassistdefense_2046', 7, '0', 'fp_adef7'), ('playerassistdefense_2046', 10, '0', 'fp_adef10'),
    ('playerassistdefense_2046', 11, '0', 'fp_adef11'), ('playerassistoffense_2048', 8, '0', 'fp_aoff8'), ('playerassistoffense_2048', 8, '1', 'fp_aoff8b'),
    ('playerassistprojectile_2051', 0, '0', 'fp_aproj'), ('playerassistprojectile_2051', 7, '0', 'fp_aproj7'), ('playerassistprojectile_2051', 8, '0', 'fp_aproj8'),
    ('ship1_2024', 0, '0', 'fp_ship1'), ('ship2_2025', 0, '0', 'fp_ship2'), ('shipcable_2026', 0, '0', 'fp_cable'), ('shipcable2_2027', 0, '0', 'fp_cable2'))]
FNP_ENS = [e + ((None, ('scrollingfloor',)) if e[3] in ('fp_jaw7', 'fp_rider1', 'fp_rider12', 'fp_rider13', 'fp_footfr7', 'fp_footfr8') else ()) for e in FNP_ENS]   # FN6-only rows of FN5 classes
FNP_ENS_WITH = {c: ('scrollingfloor',) for c in ('bossplasmashot_2040', 'bossmissile_2041', 'bossrapidbullet_1866', 'effectgunspark_1887',
    'effectspeedline_2050', 'effectsexplosion06_435', 'effectsdebris_427')}
ENGINE_TYPES_ID.update(FNP_TYPES_ID); ENS += FNP_ENS   # FN_PANTHER
# session 18 (fn_assist.c, RE 8765-8785, RV session-18 traces): the closing scene's cast. Lilac (sits 41/31, gets up 41/30, walks 1,
# stands 40 frame 0 both facings, victory 37/16), Carol (lies 41/31, gets up 41/30, 40/16 frame 0, 37/16), Milla (runs 2, 40 frame 0,
# 37) and Torque (walks 1, 40 frame 0, 37) walk in from the plane; FN6 only (the player's own bank is not used: any character)
_C1 = slice(0, 1)
FNP_CAST = [('playerskinsash_341', 41, '31', 'fc_l41s', None), ('playerskinsash_341', 41, '30', 'fc_l41g', None),
    ('playerskinsash_341', 1, '0', 'fc_l1', None), ('playerskinsash_341', 40, '0', 'fc_l40', _C1), ('playerskinsash_341', 40, '16', 'fc_l40l', _C1),
    ('playerskinsash_341', 37, '16', 'fc_l37l', None), ('playerskincarol_449', 41, '31', 'fc_c41s', None),
    ('playerskincarol_449', 41, '30', 'fc_c41g', None), ('playerskincarol_449', 40, '16', 'fc_c40l', _C1), ('playerskincarol_449', 37, '16', 'fc_c37l', None),
    ('playerskinmilla_461', 2, '0', 'fc_m2', None), ('playerskinmilla_461', 40, '0', 'fc_m40', _C1), ('playerskinmilla_461', 37, '0', 'fc_m37', None),
    ('playerskintorque_466', 1, '0', 'fc_t1', None), ('playerskintorque_466', 40, '0', 'fc_t40', _C1), ('playerskintorque_466', 37, '0', 'fc_t37', None)]
ENS += FNP_CAST
for _e in FNP_CAST: ENS_FRAMES[_e[0]] = ENS_FRAMES.get(_e[0], set()) | {35}
FNP_A5 = ('head', 'jaw', 'rider', 'shoulderf', 'hipf', 'kneef', 'legf', 'footf', 'ribs', 'tail', 'ship', 'cable')   # the rig parts FN5 shows (the rest: FN6 only)
ENS_FRAMES.update({e[0]: ({34, 35} if any(w in e[0] for w in FNP_A5) else {35}) for e in FNP_ENS
                   if e[0] not in ENS_FRAMES and e[0] not in ('shieldserpentine_1872', 'bossrapidbullet_1866', 'effectgunspark_1887')})   # FN_PANTHER (shared A3 classes left open)
ENS_FRAMES['effectwarning_1528'] = ENS_FRAMES.get('effectwarning_1528', set()) | {34, 35}   # FN_PANTHER: the mortar's ground warning (shared with RM5's row)
ROW_FRAMES_OLD = {'mt_warn': set(ENS_FRAMES['effectwarning_1528']), 'fp_warn': set(ENS_FRAMES['effectwarning_1528'])}   # (session 19b: unchanged)
for _k, _v in FNP_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # FN_PANTHER
# ---- SB_FLOW: Sky Battalion stage objects (session 19, build/re/spec_sb_flow.md; src/game/sb_flow.c). Types 904-959 mirror
# src/fp.h (OT_SB_SWPLAT ...). SB1 (json 36) is Zao's airship, the hub: the three launch pads (SwitchPlatform_2100, an
# invisible solid box, and its drawn SwitchPlatformMask_2099; SB2 / SB4 use the same pair to open doors), the ship names
# over the pads, the enemy airships on the 0.25 layer, Zao's flags, the night sky of the Kujacker; SB2-SB4 (json 37-39) open
# with Zao's airship in the background launching the four planks (LevelParallaxZaoShip_2243 + LevelSBBGPlatform_2244).
SB_TYPES = {'switchplatform': 904, 'switchplatformmask': 905, 'levelshipname1': 906, 'levelshipname2': 907, 'levelshipname3': 908,
            'levelairship1': 909, 'levelzaoflag': 910, 'leveldarksky': 911, 'levelsbbgplatform': 913}
SB_FRAMES = {36, 37, 38, 39}
ENGINE_TYPES.update(SB_TYPES)
SB_ENS = [('switchplatform_2100', 0, '0', 'sbswplat'), ('switchplatformmask_2099', 0, '0', 'sbswmask'), ('levelshipname1_2111', 0, '0', 'sbname1'), ('levelshipname2_2112', 0, '0', 'sbname2'),
          ('levelshipname3_2113', 0, '0', 'sbname3'), ('levelzaoflag_2107', 0, '0', 'sbflag'), ('levelzaoflag_2107', 0, '8', 'sbflag8'),
          ('levelsbbgplatform_2244', 0, '0', 'sbbgplat')] + \
         [('levelairship1_2105', 0, d, 'sbairship' + d) for d in ('0', '1', '2', '16')]
ENS += SB_ENS
ENS_FRAMES.update({e[0]: SB_FRAMES for e in SB_ENS})
# ---- SB_BOSS: Sky Battalion bosses and minibosses (build/re/spec_sb_bosses.md; src/game/sb_boss*.c). Types 1150-1279 mirror src/fp.h
# (OT_SB_*); the lvobj anim field carries the sub id (SBBOSS_SUB) | instance layer << 8. The classes are mapped only in SBBOSS_FRAMES
# (json 36-39); MinibossUFOBeam_1769 (FN type 803) is the satellite's main beam in SB3 (remapped below). Every sprite row names its
# creator class (rows exported only where that boss is placed).
SBBOSS_FRAMES = {36, 37, 38, 39}
SBBOSS_TYPES_ID = {('playerassist', 2205): 1150, ('playerassistcopilot', 2220): 1151, ('guicompass', 2246): 1152,
    ('bosssatellite', 2286): 1160, ('bosssatellitetop', 2284): 1161, ('bosssatellitethrusters', 2285): 1162, ('bosssatellitearm', 2287): 1163,
    ('minibossufobeam2', 2288): 1165,
    ('bossgrappler', 2331): 1240, ('bossgrapplerhandleft', 2330): 1241, ('bossgrapplerhandright', 2329): 1242, ('bossgrapplerarmleft', 2327): 1243,
    ('bossgrapplerarmright', 2328): 1244, ('bosscrystalspire', 2333): 1245, ('bosscrystalspikespawner1', 2325): 1246, ('bosscrystalspikespawner2', 2326): 1247,
    ('crystalplatform', 2334): 1250,
    ('bosschopper', 2216): 1200, ('bosschopperblade', 2217): 1201, ('bosschoppertail', 2215): 1202, ('bosschoppertailblade', 2218): 1203,
    ('bosschoppertailblade2', 2214): 1204, ('bosschopperbars', 2212): 1205, ('bosschopperbars2', 2213): 1206,
    ('bossspademask', 2248): 1220, ('bossspade', 2247): 1221, ('bossspadehitbox', 2250): 1222,
    ('bosskujackerbase', 2118): 1170, ('bosskujackerhead', 2125): 1171, ('bosskujackerhead2', 2119): 1172, ('bosskujackerleg1', 2117): 1173,
    ('bosskujackerleg2', 2116): 1174, ('bosskujackerpilot', 2124): 1175, ('bosskujackertail', 2115): 1176, ('bosskujackertailtarget', 2120): 1177,
    ('bosskujackerwing1', 2122): 1178, ('bosskujackerwing2', 2123): 1179, ('bosstornado', 2131): 1180, ('effectschargebeam', 2130): 1182}
SBBOSS_SUB = {}
_SA, _AS, _GR, _CH, _SP = ('bosssatellite',), ('playerassist',), ('bossgrappler',), ('bosschopper',), ('bossspade',)
_KJ = ('bosskujackerbase',)
TOP_ROWS = {'sbk_tail_d1': 48, 'sbk_tail_d2': 48, 'sbk_tail_d3': 48, 'sbk_tail_d31': 48}   # eye overlays (sb_kujacker.c, session 20)
SBBOSS_ENS = [(c, a, d, nm, None, w) for c, a, d, nm, w in (
    ('bosssatellite_2286', 0, '0', 'sbs_sat', _SA), ('bosssatellite_2286', 7, '0', 'sbs_sat7', _SA), ('bosssatellite_2286', 8, '0', 'sbs_sat8', _SA),
    ('bosssatellite_2286', 10, '0', 'sbs_sat10', _SA), ('bosssatellitearm_2287', 0, '0', 'sbs_arm', _SA), ('bosssatellitearm_2287', 7, '0', 'sbs_arm7', _SA),
    ('bosssatellitearm_2287', 8, '0', 'sbs_arm8', _SA), ('bosssatellitearm_2287', 10, '0', 'sbs_arm10', _SA), ('bosssatellitetop_2284', 0, '0', 'sbs_top', _SA),
    ('bosssatellitethrusters_2285', 0, '0', 'sbs_thr', _SA), ('minibossufobeam2_2288', 0, '0', 'sbs_beam2', _SA), ('minibossufobeam_1769', 0, '0', 'sbs_beam', _SA),
    ('playerassist_2205', 0, '0', 'sba_plane', _AS), ('playerassistcopilot_2220', 0, '0', 'sba_cop0', _AS), ('playerassistcopilot_2220', 0, '2', 'sba_cop2', _AS),
    ('playerassistcopilot_2220', 0, '3', 'sba_cop3', _AS), ('playerassistcopilot_2220', 0, '4', 'sba_cop4', _AS), ('guicompass_2246', 0, '0', 'sba_compass', _AS),
    ('bossgrappler_2331', 0, '0', 'sbg_gr', _GR), ('bossgrappler_2331', 0, '1', 'sbg_gr1', _GR), ('bossgrappler_2331', 0, '2', 'sbg_gr2', _GR),
    ('bossgrappler_2331', 7, '0', 'sbg_gr7', _GR), ('bossgrappler_2331', 7, '1', 'sbg_gr71', _GR), ('bossgrappler_2331', 7, '2', 'sbg_gr72', _GR),
    ('bossgrappler_2331', 8, '0', 'sbg_gr8', _GR), ('bossgrapplerhandleft_2330', 0, '0', 'sbg_hl', _GR), ('bossgrapplerhandleft_2330', 7, '0', 'sbg_hl7', _GR),
    ('bossgrapplerhandright_2329', 0, '0', 'sbg_hr', _GR), ('bossgrapplerhandright_2329', 7, '0', 'sbg_hr7', _GR),
    ('bossgrapplerarmleft_2327', 0, '0', 'sbg_al', _GR), ('bossgrapplerarmleft_2327', 7, '0', 'sbg_al7', _GR),
    ('bossgrapplerarmright_2328', 0, '0', 'sbg_ar', _GR), ('bossgrapplerarmright_2328', 7, '0', 'sbg_ar7', _GR),
    ('bosscrystalspire_2333', 0, '0', 'sbg_spire', _GR), ('bosscrystalspike_2324', 0, '0', 'sbg_spike', _GR), ('bosshomingspark_2332', 0, '24', 'sbg_spark', _GR),
    ('bosscrystalspikespawner1_2325', 0, '0', 'sbg_spawn', _GR), ('crystalplatform_2334', 0, '0', 'sbg_plat', _GR), ('effectwarning_1528', 0, '0', 'sbg_warn', _GR))] + \
    [(c, a, d, nm + ('' if d == '0' else '_l'), None, _CH) for c, a, nm in (
    ('bosschopper_2216', 0, 'sbc_ch'), ('bosschopper_2216', 8, 'sbc_ch8'), ('bosschopperblade_2217', 0, 'sbc_blade'), ('bosschoppertail_2215', 0, 'sbc_tail'),
    ('bosschoppertailblade_2218', 0, 'sbc_tb'), ('bosschoppertailblade2_2214', 0, 'sbc_tb2'), ('bosschopperbars_2212', 0, 'sbc_bars'),
    ('bosschopperbars2_2213', 0, 'sbc_bars2'), ('bossmissile_2219', 0, 'sbc_mis')) for d in ('0', '16')] + \
    [('bossmissile_2219', 8, '0', 'sbc_mis8', None, _CH)] + \
    [('bossspade_2247', a, d, 'sbp_%d' % a + ('' if d == '0' else '_l'), None, _SP) for a in (0, 1, 2, 8, 15, 21, 24, 26, 29) for d in ('0', '16')] + \
    [(c, a, '0', nm, None, _SP) for c, a, nm in (('bossspadecard_2249', 0, 'sbp_card'), ('bossspadecard_2249', 7, 'sbp_card7'),
                                                 ('bossspadehitbox_2250', 0, 'sbp_hit'), ('thunderspark_410', 4, 'sbp_tspark'))] + \
    [(c, a, d, nm, None, _KJ) for c, a, d, nm in [('bosskujackerbase_2118', 0, '0', 'sbk_base'), ('bosskujackerbase_2118', 7, '0', 'sbk_base7'), ('bosskujackerbase_2118', 11, '0', 'sbk_base11'), ('bosskujackerhead_2125', 0, '0', 'sbk_head'), ('bosskujackerhead_2125', 0, '16', 'sbk_head_l'), ('bosskujackerhead_2125', 0, '24', 'sbk_head_d'), ('bosskujackerhead_2125', 7, '0', 'sbk_head7'), ('bosskujackerhead_2125', 7, '16', 'sbk_head7_l'), ('bosskujackerhead_2125', 8, '0', 'sbk_head8'), ('bosskujackerhead_2125', 9, '0', 'sbk_head9'), ('bosskujackerhead_2125', 10, '0', 'sbk_head10'), ('bosskujackerhead_2125', 11, '24', 'sbk_head11'), ('bosskujackerhead2_2119', 0, '0', 'sbk_neck'), ('bosskujackerhead2_2119', 11, '0', 'sbk_neck11'), ('bosskujackerwing1_2122', 0, '0', 'sbk_w1'), ('bosskujackerwing1_2122', 11, '0', 'sbk_w1_11'), ('bosskujackerwing2_2123', 0, '0', 'sbk_w2'), ('bosskujackerwing2_2123', 11, '0', 'sbk_w2_11'), ('bosskujackerleg1_2117', 0, '0', 'sbk_leg1'), ('bosskujackerleg1_2117', 7, '0', 'sbk_leg1_7'), ('bosskujackerleg1_2117', 7, '16', 'sbk_leg1_7l'), ('bosskujackerleg1_2117', 11, '0', 'sbk_leg1_11'), ('bosskujackerleg2_2116', 0, '0', 'sbk_leg2'), ('bosskujackerleg2_2116', 7, '0', 'sbk_leg2_7'), ('bosskujackerleg2_2116', 7, '16', 'sbk_leg2_7l'), ('bosskujackerleg2_2116', 11, '0', 'sbk_leg2_11'), ('bosskujackertail_2115', 0, '0', 'sbk_tail'), ('bosskujackertail_2115', 0, '1', 'sbk_tail_d1'), ('bosskujackertail_2115', 0, '2', 'sbk_tail_d2'), ('bosskujackertail_2115', 0, '3', 'sbk_tail_d3'), ('bosskujackertail_2115', 0, '31', 'sbk_tail_d31'), ('bosskujackertail_2115', 10, '0', 'sbk_tail10'), ('bosskujackertail_2115', 11, '0', 'sbk_tail11'), ('bosskujackertailtarget_2120', 0, '0', 'sbk_tt'), ('bosskujackertailtarget_2120', 11, '0', 'sbk_tt11'), ('bosskujackerpilot_2124', 0, '0', 'sbk_pilot'), ('bosskujackerpilot_2124', 7, '0', 'sbk_pilot7'), ('bosskujackerpilot_2124', 7, '16', 'sbk_pilot7_l'), ('bosstornado_2131', 0, '0', 'sbk_tornado'), ('bossbolt_2129', 0, '0', 'sbk_bolt'), ('bosslightning_2121', 0, '0', 'sbk_light'), ('bosslightning2_2132', 0, '0', 'sbk_light2'), ('bosslightning2_2132', 0, '1', 'sbk_light2v'), ('effectschargebeam_2130', 0, '0', 'sbk_beam'), ('effectsgust_2127', 4, '0', 'sbk_gust'), ('effectsgust2_2128', 0, '0', 'sbk_gust2'), ('effectsdust01_369', 4, '0', 'sbk_dust'), ('effectsdust01_369', 4, '16', 'sbk_dust_l'), ('effectsexplosion02_368', 4, '0', 'sbk_exp2'), ('effectwarning_1528', 0, '0', 'sbk_warn')]]
SBBOSS_ENS_WITH = {'effectsexplosion01_367': _SA + _GR + _CH + _SP, 'effectsexplosion06_435': _SA + _GR + _CH + _KJ, 'effectshade_436': _SA,
                   'bosscrystalspike_2324': _GR, 'bosshomingspark_2332': _GR, 'bossmissile_2219': _CH, 'minibosstreddbomb_1060': _CH, 'monitoricon_363': _CH,
                   'bossspadecard_2249': _SP, 'thunderspark_410': _SP, 'bossbolt_2129': _KJ, 'bosslightning_2121': _KJ, 'bosslightning2_2132': _KJ,
                   'effectsgust_2127': _KJ, 'effectsgust2_2128': _KJ, 'effectsdust01_369': _KJ, 'effectsexplosion02_368': _KJ, 'effectwarning_1528': _KJ}
ENGINE_TYPES_ID.update(SBBOSS_TYPES_ID); ENS += SBBOSS_ENS   # SB_BOSS
ENS_FRAMES['effectwarning_1528'] = ENS_FRAMES.get('effectwarning_1528', set()) | {36, 39}   # SB_BOSS: the spire's marker (SB4), the Kujacker's dash warnings (SB1)
for _k, _v in SBBOSS_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # SB_BOSS
# ---- SB_ENEMIES: Sky Battalion enemies (session 19, build/re/spec_sb_enemies.md; src/game/sb_enemy.c), engine types 1024-1099
# (fp.h OT_SB_*). Rows '' = direction 0, '_l' = 16. Runtime children are exported with their creators (ENS_WITH). Placed off-map
# templates (the lasers) map to the runtime types and are dropped at spawn.
SBE_TYPES_ID = {('enemyhoppermkii', 2189): 1024, ('enemymagmeon', 2203): 1025, ('enemyrotorblade', 2184): 1026,
    ('enemyrotorbladespikes', 2183): 1027, ('enemyspawntube', 2185): 1028, ('enemydrone', 2138): 1029, ('enemyaxleturret', 2139): 1030,
    ('enemyaxleturret2', 2140): 1031, ('enemydnacannon', 2260): 1032, ('enemydnacannonbase', 2261): 1033, ('enemypelletizer', 2303): 1034,
    ('enemymagnetizer', 2294): 1035, ('enemymagnetizercrate', 2295): 1036, ('enemymagnetizerhand', 2292): 1037,
    ('enemymagnetizerline', 2293): 1038, ('enemydronelaser', 2232): 1063, ('enemydnalaser', 2290): 1065}
SBE_ENS = [(c, a, d, nm + ('' if d == '0' else '_l')) for c, a, nm in (
    ('enemyhoppermkii_2189', 0, 'sbh_idle'), ('enemyhoppermkii_2189', 7, 'sbh_crouch'), ('enemyhoppermkii_2189', 8, 'sbh_wreck'),
    ('enemyhoppermkii_2189', 10, 'sbh_throw'), ('enemyhoppermkii_2189', 11, 'sbh_land'),
    ('enemymagmeon_2203', 0, 'sbm_idle'), ('enemymagmeon_2203', 1, 'sbm_walk'), ('enemymagmeon_2203', 7, 'sbm_jump'),
    ('enemymagmeon_2203', 8, 'sbm_dead'), ('enemymagmeon_2203', 10, 'sbm_land'),
    ('enemydrone_2138', 0, 'sbd_fly'), ('enemydrone_2138', 7, 'sbd_dash'), ('enemydrone_2138', 8, 'sbd_dead'),
    ('enemydnacannon_2260', 0, 'sbn_idle'), ('enemydnacannon_2260', 8, 'sbn_dead'), ('enemydnacannon_2260', 10, 'sbn_fire'),
    ('enemydnacannon_2260', 11, 'sbn_charge'), ('enemydnacannonbase_2261', 0, 'sbn_base')) for d in ('0', '16')] + \
    [(c, a, d, nm) for c, a, d, nm in (
    ('enemyfragbomb_2103', 0, '0', 'sbh_bomb'), ('enemyfragbomb_2103', 7, '0', 'sbh_bombx'), ('enemymagmeon_2203', 0, '24', 'sbm_feet'),
    ('effectsdust01_369', 4, '0', 'sbm_smoke'), ('enemyrotorblade_2184', 0, '0', 'sbr_roll'), ('enemyrotorbladedeath_2231', 0, '0', 'sbr_hub'),
    ('enemyrotorbladespikes_2183', 0, '0', 'sbr_spikes'), ('enemyspawntube_2185', 0, '0', 'sbr_tube'), ('breakablespike2_2228', 0, '0', 'sbr_bit'),
    ('effectsgroundspark_2053', 4, '0', 'sbr_spark'), ('enemydronelaser_2232', 0, '0', 'sbd_laser'),
    ('enemyaxleturret_2139', 0, '0', 'sba_idle'), ('enemyaxleturret_2139', 8, '0', 'sba_dead'), ('enemyaxleturret_2139', 10, '0', 'sba_fire'),
    ('enemyaxleturret2_2140', 0, '0', 'sba_base'), ('enemybullet_2097', 0, '0', 'sba_bullet'), ('enemydnalaser_2290', 0, '0', 'sbn_laser'),
    ('enemypelletizer_2303', 0, '0', 'sbp_0'), ('enemypelletizer_2303', 0, '1', 'sbp_1'), ('enemypelletizer_2303', 0, '2', 'sbp_2'),
    ('effectsdust_437', 4, '0', 'sbp_dust'), ('enemymagnetizer_2294', 0, '0', 'sbg_body'), ('enemymagnetizercrate_2295', 0, '0', 'sbg_crate'),
    ('enemymagnetizerhand_2292', 0, '0', 'sbg_hand'), ('enemymagnetizerline_2293', 0, '0', 'sbg_line'))]
_SBE_KS = ('enemyhoppermkii', 'enemyrotorblade', 'enemydrone', 'enemyaxleturret', 'enemydnacannon', 'enemypelletizer', 'enemymagnetizer')
SBE_ENS_WITH = {'enemyfragbomb_2103': ('enemyhoppermkii',), 'effectsdust01_369': ('enemymagmeon',),
    'enemyrotorbladedeath_2231': ('enemyrotorblade',), 'breakablespike2_2228': ('enemyrotorblade',), 'effectsgroundspark_2053': ('enemyrotorblade',),
    'enemybullet_2097': ('enemyaxleturret',), 'effectsdust_437': ('enemypelletizer',), 'objectkeystone_429': _SBE_KS,
    'effectsdebris_427': _SBE_KS, 'effectsexplosion06_435': ('enemydnacannon',)}
# session 19b: EffectWarning_1528 in SB1 (the deck bombardment) / SB3 (the DNA cannons' off-screen warning) is OT_SB_WARN (1039;
# remapped per frame below, SB4's stays the boss agent's); SB1's airships fire ShipBullet_2106 and drop GimmickCannonBullet_2098
# balls whose blast is the bomb's (explosion06): exported with LevelAirship1 (SB1 only)
SBE_ENS += [('effectwarning_1528', 0, '0', 'sbwarn'), ('shipbullet_2106', 0, '0', 'sbshot')]
SBE_ENS_WITH.update({'shipbullet_2106': ('levelairship1',)})
SBE_ENS_WITH['effectsexplosion06_435'] = SBE_ENS_WITH['effectsexplosion06_435'] + ('levelairship1',)
SBE_WARN_FRAMES = {36, 38}
# SB4's swing spawners create their own part classes (9949-9951: SwingChain_2320, SwingPlatform_2321 (96 x 24), SwingBase_2319),
# AT's rows 'swbase' / 'swchain' / 'swplat' take those images there (RE, session 19b)
ROW_HANDLE_SUB = {39: {'gimmickswingbase_821': 'gimmickswingbase_2319', 'gimmickswingchain_822': 'gimmickswingchain_2320',
                       'gimmickswingplatform_823': 'gimmickswingplatform_2321'}}
ROW_FRAMES = {'sbwarn': {38}}   # SB1 draws them with the boss block's 'sbk_warn' row (same images)   # row-level frame filter (the class-level ENS_FRAMES keeps RM5 / FN6's rows as they were)
ENGINE_TYPES_ID.update(SBE_TYPES_ID); ENS += SBE_ENS   # SB_ENEMIES
for _k, _v in SBE_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # SB_ENEMIES
# ---- SB_GIM: Sky Battalion gimmicks (session 19, build/re/spec_sb_gimmicks.md; src/game/sb_gim.c), engine types 1100-1149 (fp.h
# OT_SB_*). Families share a type; SBG_SUB gives the member index (lvobj anim bits 0-7, the instance layer in bits 8-15).
SBG_TYPES_ID = {('gimmickcannon', 2142): 1100, ('gimmickcannonbase', 2141): 1101, ('gimmickcannonbase2', 2176): 1102,
    ('hazardfireup', 2237): 1105, ('hazardfiredown', 2238): 1105, ('hazardfireleft', 2241): 1105, ('hazardfireright', 2240): 1105,
    ('hazardlightningradial', 2254): 1106, ('hazardlightningradial2', 2256): 1107, ('gimmickflamethrower', 2182): 1108,
    ('gimmickspincup', 2269): 1109, ('gimmickspincupstartpoint', 2275): 1110, ('gimmickspincuplandpoint', 2270): 1111,
    ('gimmickchainwallup', 2307): 1112, ('gimmickchainwalldown', 2308): 1112, ('gimmickchainwallmaskup', 2304): 1113,
    ('gimmickchainwallmaskdown', 2306): 1113, ('gimmickcrusherplatform', 2310): 1114, ('gimmickcrusherchain', 2309): 1115,
    ('levelsbmovingblock1', 2318): 1116, ('levelsbplatform1', 2095): 1117, ('levelsbplatformflame1', 2096): 1118,
    ('levelsbplatform3', 2188): 1119, ('gimmicksbbar', 2314): 1121, ('gimmicksbbarmask', 2313): 1122,
    ('levelfanblade', 2193): 1123, ('levelfanblade', 2264): 1123, ('levelfanblade', 2297): 1123, ('gimmickfan', 2223): 1124,
    ('gimmickfanside', 2245): 1125, ('levelscrollingchainup', 2302): 1126, ('levelscrollingchaindown', 2305): 1126,
    ('sbbackeye', 2282): 1127, ('sbbackeye2', 2283): 1127, ('sbloop1', 2206): 1128, ('sbloop4', 2224): 1128, ('sbloop7', 2289): 1128,
    ('active3', 2278): 1128, ('active4', 2279): 1128, ('sbloop2', 2207): 1129, ('sbloop6', 2226): 1129, ('active', 2276): 1129,
    ('active2', 2277): 1129, ('sbloop3', 2208): 1130, ('sbloop5', 2225): 1130, ('pushableblock4', 2259): 1131,
    ('levelballtunnel2', 2271): 1132, ('levelballtunnel3', 2273): 1132, ('switchdoor', 2102): 1133,
    ('breakablefloor1', 2196): 1103, ('breakablefloor2', 2197): 1103, ('breakablefloor1', 2298): 1103, ('breakablefloor2', 2299): 1103}
SBG_SUB = {'hazardfiredown_2238': 1, 'hazardfireleft_2241': 2, 'hazardfireright_2240': 3, 'gimmickchainwalldown_2308': 1,
    'gimmickchainwallmaskdown_2306': 1, 'levelfanblade_2264': 1, 'levelfanblade_2297': 2, 'levelscrollingchaindown_2305': 1,
    'sbbackeye2_2283': 1, 'sbloop4_2224': 1, 'sbloop7_2289': 2, 'active3_2278': 3, 'active4_2279': 4, 'sbloop6_2226': 1,
    'active_2276': 2, 'active2_2277': 3, 'sbloop5_2225': 1, 'levelballtunnel3_2273': 1,
    'breakablefloor2_2197': 1, 'breakablefloor1_2298': 2, 'breakablefloor2_2299': 3}
SBG_ENS = [(c, 0, '0', nm) for c, nm in (
    ('gimmickcannon_2142', 'sbc_cannon'), ('gimmickcannonbase_2141', 'sbc_base'), ('gimmickcannonbase2_2176', 'sbc_base2'),
    ('gimmickcannonbullet_2098', 'sbc_ball'), ('hazardfireup_2237', 'sbf_up'), ('hazardfiredown_2238', 'sbf_down'),
    ('hazardfireleft_2241', 'sbf_left'), ('hazardfireright_2240', 'sbf_right'), ('hazardlightningradial_2254', 'sbz_ray'),
    ('hazardlightningradial2_2256', 'sbz_ray2'), ('gimmickflamethrower_2182', 'sbf_thrower'), ('gimmickspincup_2269', 'sbs_cup'),
    ('gimmickspincupstartpoint_2275', 'sbs_start'), ('gimmickspincuplandpoint_2270', 'sbs_land'), ('gimmickchainwallup_2307', 'sbw_up'),
    ('gimmickchainwalldown_2308', 'sbw_down'), ('gimmickchainwallmaskup_2304', 'sbw_mup'), ('gimmickchainwallmaskdown_2306', 'sbw_mdown'),
    ('gimmickcrusherplatform_2310', 'sbk_plat'), ('gimmickcrusherchain_2309', 'sbk_chain'), ('levelsbmovingblock1_2318', 'sbk_block'),
    ('levelsbplatform1_2095', 'sbp_plat1'), ('levelsbplatformflame1_2096', 'sbp_flame'), ('levelsbplatform3_2188', 'sbp_plat3'),
    ('gimmicksbbar_2314', 'sbb_bar'), ('gimmicksbbarmask_2313', 'sbb_mask'), ('levelfanblade_2193', 'sbv_blade0'),
    ('levelfanblade_2264', 'sbv_blade1'), ('levelfanblade_2297', 'sbv_blade2'), ('gimmickfan_2223', 'sbv_fan'),
    ('gimmickfanside_2245', 'sbv_fanside'), ('gimmickfandebris_2230', 'sbv_debris'), ('levelscrollingchainup_2302', 'sbv_chain'),
    ('sbbackeye_2282', 'sbv_eye0'), ('sbbackeye2_2283', 'sbv_eye1'), ('sbloop1_2206', 'sbl_0_0'), ('sbloop4_2224', 'sbl_0_1'),
    ('sbloop7_2289', 'sbl_0_2'), ('active3_2278', 'sbl_0_3'), ('active4_2279', 'sbl_0_4'), ('sbloop2_2207', 'sbl_1_0'),
    ('sbloop6_2226', 'sbl_1_1'), ('active_2276', 'sbl_1_2'), ('active2_2277', 'sbl_1_3'), ('sbloop3_2208', 'sbl_2_0'),
    ('sbloop5_2225', 'sbl_2_1'), ('pushableblock4_2259', 'sbq_block'), ('levelballtunnel2_2271', 'sbt_tun0'),
    ('levelballtunnel3_2273', 'sbt_tun1'), ('breakablefloorpiece2_2229', 'sbq_fpiece'), ('breakablefloor1_2196', 'sbq_f0'),
    ('breakablefloor2_2197', 'sbq_f1'), ('breakablefloor1_2298', 'sbq_f2'), ('breakablefloor2_2299', 'sbq_f3'))] + \
    [('gimmickcannon_2142', 7, '0', 'sbc_fire'), ('gimmickcannonbase_2141', 8, '0', 'sbc_bdead')] + \
    [('hazardfire%s_%d' % (n, h), 0, '1', 'sbf_%s1' % n) for n, h in (('up', 2237), ('down', 2238), ('left', 2241), ('right', 2240))]
SBG_ENS_WITH = {'gimmickcannonbullet_2098': ('gimmickcannon',), 'gimmickfandebris_2230': ('gimmickfan',),
    'breakablefloorpiece2_2229': ('breakablefloor1', 'breakablefloor2'), 'minibossflame_1264': ('gimmickflamethrower',),
    'effectsexplosion06_435': ('gimmickcannonbase',)}
ENGINE_TYPES_ID.update(SBG_TYPES_ID); ENS += SBG_ENS   # SB_GIM
SBG_ENS_WITH['gimmickcannonbullet_2098'] = SBG_ENS_WITH['gimmickcannonbullet_2098'] + ('levelairship1',)   # SB1's falling balls (session 19b)
for _k, _v in SBG_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # SB_GIM

OBJ_BANK_SKIP = set()   # class_handle keys of _obj-type classes drawn from the _ens bank instead (session 21)
# ==== Jade Creek (session 21; json 40-44 = jc1-jc5; types 1280-1535, one block per module; each block updates
# ENGINE_TYPES / ENGINE_TYPES_ID / ENS / ENS_FRAMES / ENS_WITH itself, like the SB blocks above)
JC_FRAMES = {40, 41, 42, 43, 44}
TH_FRAMES = {57, 58, 59}   # Trap Hideout (session 21b; th1-th3)
TB_FRAMES = {45, 46, 47, 48, 49, 50}   # Thermal Base (session 21c; tb1-tb6)
PL_FRAMES = {51, 52, 53, 54, 55, 56}   # Pangu Lagoon (session 23; pl1-pl6)
BG_FRAMES = {60, 61, 62, 63, 64, 65}   # Battle Glacier (session 24; bg1-bg6)
FD_FRAMES = {66, 67, 68, 69, 70, 71, 72, 73}   # Final Dreadnought (session 24; fd1a..fd4b)
NEWPACK_FRAMES = JC_FRAMES | TH_FRAMES | TB_FRAMES | PL_FRAMES | BG_FRAMES | FD_FRAMES   # stages built with the session-21 packers (D-105); earlier levels keep theirs
JC_SUB = {}   # class_handle -> member index of a JC family type (lvobj anim bits 0-7; bits 8-14 = the instance layer, see write_lv2)
# ---- JC_FLOW (jc_flow.c, 1280-1309): the creek's loop pieces (RE on_frame_41 / 42_init qualifier tables: JCLoop1_2461 Q7 only
# (its flag 0 is driven by the frame's own events, jc_flow.c), JCLoop2_2462 Q7 Q11 (solid on layer 1), JCLoop3 / 4_2463 / 2464
# Q7 Q10 (solid on layer 0); visible Actives with pixel masks, constructors initialize_active(false))
JCF_TYPES_ID = {('jcloop3', 2463): 1280, ('jcloop4', 2464): 1280, ('jcloop2', 2462): 1281, ('jcloop1', 2461): 1282}
JC_SUB.update({'jcloop4_2464': 1})
JCF_ENS = [('jcloop3_2463', 0, '0', 'jcl_0_0'), ('jcloop4_2464', 0, '0', 'jcl_0_1'), ('jcloop2_2462', 0, '0', 'jcl_1_0'), ('jcloop1_2461', 0, '0', 'jcl_x')]
ENGINE_TYPES_ID.update(JCF_TYPES_ID); ENS += JCF_ENS   # JC_FLOW
# water (MasterLevel A = 1: JC1 10378 ML.B 1200, JC2 10566 / JC3 10822 / JC4 10863 ML.B 1600; JC5: the act-5 start events)
WATER_FRAMES = WATER_FRAMES + (40, 41, 42, 43, 44)

# ---- JC_ENEMIES (jc_enemy.c, 1310-1369; build/re/spec_jc_enemies.md). Placed classes 1310-1339, runtime children 1340-1369
# (jc.h OT_JCE_*). Rows 'jeX_name' ('_l' = direction 16); runtime children are exported with their creators (ENS_WITH).
JCE_TYPES_ID = {('enemymeteorroller', 2416): 1310, ('enemymeteorrollerflame', 2417): 1311, ('enemybuoybuoy', 2407): 1312,
    ('enemyplantturretus', 2393): 1314, ('enemyplantturretushat', 2394): 1315, ('enemyplantturretusstalk', 2395): 1316,
    ('enemybullfrag', 2389): 1318, ('enemybullfragmask', 2410): 1319, ('enemysharktorpedo', 2421): 1321,
    ('enemybubblorbiter', 2455): 1323, ('hazardtorpedolauncher', 2515): 1325, ('enemyelite', 2529): 1326,
    ('enemyelitemask', 2528): 1327}
JCE_ENS = [(c, a, d, nm + ('' if d == '0' else '_l')) for c, a, nm in (
    ('enemymeteorroller_2416', 0, 'jem_roll'), ('enemymeteorroller_2416', 5, 'jem_land'), ('enemymeteorroller_2416', 8, 'jem_fall'),
    ('enemymeteorroller_2416', 9, 'jem_hurt'), ('enemymeteorrollerflame_2417', 0, 'jem_flame'),
    ('enemyplantturretus_2393', 0, 'jep_idle'), ('enemyplantturretus_2393', 7, 'jep_shoot'), ('enemyplantturretushat_2394', 0, 'jep_hat'),
    ('enemybuoybuoy_2407', 9, 'jeb_dead'), ('enemybullfrag_2389', 0, 'jef_idle'), ('enemybullfrag_2389', 8, 'jef_hurt'),
    ('enemybullfrag_2389', 10, 'jef_blow'), ('enemybullfrag_2389', 11, 'jef_shut'), ('enemybubblorbiter_2455', 0, 'jeo_body'))
    for d in ('0', '16')] + [(c, a, d, nm) for c, a, d, nm in (
    ('enemyplantturretusstalk_2395', 0, '0', 'jep_stalk'), ('effectfallingleaf_2396', 0, '0', 'jep_leaf'),
    ('enemybuoybuoy_2407', 0, '0', 'jeb_idle'), ('enemybuoybuoy_2407', 7, '0', 'jeb_open'), ('enemybuoybuoy_2407', 8, '0', 'jeb_shut'),
    ('enemybuoybullet_2409', 0, '0', 'jeb_jet'), ('enemybuoybullet_2409', 0, '16', 'jeb_jet_l'), ('enemybullfragbubble_2408', 0, '0', 'jef_bubble'),
    ('enemysharktorpedo_2421', 0, '0', 'jes_r0'), ('enemysharktorpedo_2421', 0, '1', 'jes_r1'), ('enemysharktorpedo_2421', 0, '16', 'jes_l0'),
    ('enemysharktorpedo_2421', 0, '17', 'jes_l1'), ('enemysharktorpedo_2421', 7, '0', 'jes_hr0'), ('enemysharktorpedo_2421', 7, '1', 'jes_hr1'),
    ('enemysharktorpedo_2421', 7, '16', 'jes_hl0'), ('enemysharktorpedo_2421', 7, '17', 'jes_hl1'), ('enemysharktorpedo2_2422', 0, '0', 'jes_torp'),
    ('badnikgolemshot_1260', 0, '0', 'jeo_shot'), ('badnikgolemshot_1260', 4, '0', 'jeo_pop'), ('hazardtorpedolauncher_2515', 0, '0', 'jet_hatch'),
    ('effectwarning_1528', 0, '0', 'jem_warn'))] + \
    [(c, a, d, nm + ('' if d == '0' else '_l')) for c, a, nm in (
    ('enemyelite_2529', 0, 'jee_idle'), ('enemyelite_2529', 1, 'jee_walk'), ('enemyelite_2529', 2, 'jee_run'), ('enemyelite_2529', 5, 'jee_down'),
    ('enemyelite_2529', 7, 'jee_jump'), ('enemyelite_2529', 8, 'jee_knock'), ('enemyelite_2529', 10, 'jee_charge'), ('enemyelite_2529', 11, 'jee_rec'),
    ('enemyelite_2529', 12, 'jee_shoot'), ('enemyelite_2529', 13, 'jee_up')) for d in ('0', '16')] + \
    [('enemyelitemask_2528', 0, str(d), 'jee_m%d' % d) for d in range(5)] + \
    [('enemyelitechargeglow_2527', 0, '0', 'jee_glow'), ('enemytrooperlaser_2530', 0, '0', 'jee_laser')]
_JCE_ALL = ('enemymeteorroller', 'enemybuoybuoy', 'enemyplantturretus', 'enemybullfrag', 'enemysharktorpedo', 'enemybubblorbiter', 'enemyelite')
JCE_ENS_WITH = {'effectfallingleaf_2396': ('enemyplantturretus',), 'enemybuoybullet_2409': ('enemybuoybuoy',),
    'enemybullfragbubble_2408': ('enemybullfrag',), 'enemysharktorpedo2_2422': ('enemysharktorpedo', 'hazardtorpedolauncher'),
    'badnikgolemshot_1260': ('enemybubblorbiter',), 'minibosstreddbomb_1060': ('hazardtorpedolauncher',),
    'effectwarning_1528': ('enemymeteorroller',), 'enemyelitechargeglow_2527': ('enemyelite',), 'enemytrooperlaser_2530': ('enemyelite',),
    'objectkeystone_429': _JCE_ALL, 'effectsdebris_427': _JCE_ALL, 'effectsexplosion06_435': _JCE_ALL + ('hazardtorpedolauncher',),
    }
JCE_ROW_FRAMES = {'jem_warn': JC_FRAMES, 'jeo_shot': JC_FRAMES, 'jeo_pop': JC_FRAMES}   # (the Tredd bomb: DV's 'td_bomb' row; white burst: the objects bank's 'wburst')
ENGINE_TYPES_ID.update(JCE_TYPES_ID); ENS += JCE_ENS   # JC_ENEMIES
for _k, _v in JCE_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # JC_ENEMIES
ROW_FRAMES.update(JCE_ROW_FRAMES)   # JC_ENEMIES

# ---- JC_GIM (jc_gim.c, 1370-1429; build/re/spec_jc_gimmicks.md). Families share a type; JCG_SUB gives the member index
# (lvobj anim bits 0-7, the instance layer in bits 8-14, the same encoding as SB_GIM). LevelCreekPlatform_2425 is DV's
# LevelValleyPlatform1 (ctor F 1 G 10 H 1, Q8 Q9 Q12 Q13, the shared path events 3283-3289 / 4061 / 3303-3307: RE).
JCG_TYPES_ID = {('jcwaterstream1', 2366): 1370, ('jcwaterstream2', 2367): 1370, ('jcwaterstream3', 2368): 1370,
    ('jcwaterstream4', 2369): 1370, ('jcwaterstream5', 2371): 1370, ('jcwaterstream6', 2372): 1370, ('jcwaterstream7', 2370): 1370,
    ('jcwaterstream8', 2373): 1370, ('gimmickleafplatformback', 2357): 1371, ('gimmickleafplatform1', 2355): 1372,
    ('gimmickleafplatform2', 2356): 1372, ('gimmickflyingleafplatform', 2476): 1373, ('gimmickflyingleafplatformmask', 2477): 1374,
    ('jcslidingplatform', 2377): 1375, ('jcslidingplatformmask', 2381): 1376, ('jcslidingplatformlandpoint', 2382): 1377,
    ('leveljccollapsingplatform', 2456): 1378, ('leveljccollapsingplatform', 2475): 1378, ('gimmickseedspawner', 2418): 1380,
    ('gimmickbubblespringspawner', 2449): 1382, ('levelslipperysurface', 2473): 1385, ('effectfallingleaf', 2396): 1386,
    ('levelcreekplatform', 2425): 32}   # 32 = OT_PLAT_VALLEY
JCG_SUB = {'jcwaterstream1_2366': 1, 'jcwaterstream2_2367': 2, 'jcwaterstream3_2368': 3, 'jcwaterstream4_2369': 4,
    'jcwaterstream5_2371': 5, 'jcwaterstream6_2372': 6, 'jcwaterstream7_2370': 7, 'jcwaterstream8_2373': 8,
    'gimmickleafplatform2_2356': 1, 'leveljccollapsingplatform_2475': 1}
JCG_ENS = [('jcwaterstream%d_%d' % (k, h), 0, '0', 'jcg_st%d' % k) for k, h in ((1, 2366), (2, 2367), (3, 2368), (4, 2369),
           (5, 2371), (6, 2372), (7, 2370), (8, 2373))] + [(c, 0, '0', nm) for c, nm in (
    ('gimmickleafplatformback_2357', 'jcg_lback'), ('gimmickleafplatform1_2355', 'jcg_leaf1'), ('gimmickleafplatform2_2356', 'jcg_leaf2'),
    ('gimmickflyingleafplatform_2476', 'jcg_fleaf'), ('gimmickflyingleafplatformmask_2477', 'jcg_fleafm'),
    ('jcslidingplatform_2377', 'jcg_slide'), ('jcslidingplatformmask_2381', 'jcg_slidem'), ('jcslidingplatformlandpoint_2382', 'jcg_slidel'),
    ('leveljccollapsingplatform_2456', 'jcg_coll0'), ('leveljccollapsingplatform_2475', 'jcg_coll1'),
    ('leveljccollapsingplatformpiece_2457', 'jcg_cpiece'), ('gimmickseedspawner_2418', 'jcg_seedsp'), ('gimmickflyingseed_2419', 'jcg_seed'),
    ('gimmickbubblespringspawner_2449', 'jcg_bubsp'), ('levelslipperysurface_2473', 'jcg_slip'), ('effectfallingleaf_2396', 'jcg_fallleaf'))] + \
    [('gimmickbubblespringback2_2459', a, d, 'jcg_bub%s%s' % (a, d)) for a in (0, 7) for d in ('0', '1')] + \
    [('gimmickbubblespringfront2_2460', 0, d, 'jcg_bubf%s' % d) for d in ('0', '1')]
JCG_ENS_WITH = {'leveljccollapsingplatformpiece_2457': ('leveljccollapsingplatform',), 'gimmickflyingseed_2419': ('gimmickseedspawner',),
    'gimmickbubblespringback2_2459': ('gimmickbubblespringspawner',), 'gimmickbubblespringfront2_2460': ('gimmickbubblespringspawner',),
    'effectfallingleaf_2396': ('gimmickflyingleafplatform',),
    'effectswaterhit_756': tuple('jcwaterstream%d' % k for k in range(1, 7)), 'effectsexplosion01_367': ('jcslidingplatform',),
    'effectsdust_437': ('jcslidingplatform',), 'ringsparkle_142': ('gimmickbubblespringspawner',),
    'effectswhiteburst_31': ('gimmickbubblespringspawner',)}
# Leaf-back tilts A (the start events give A by selection index, newest first: JC1 10382-10385 I 0 / 3 -> 120, I 2 / 7 -> 60;
# every other back 90; RE + RV init_f39 / leaf1): keyed by placement, encoded as the member index A / 30 (0 = 90)
JCG_LEAF_TILT = {(4528, 895): 120, (4592, 895): 60, (4152, 1111): 120, (10128, 647): 60}
# LevelCreekPlatform's sprite comes from the _ens bank (jc_gim_spawn: valley platforms without an _obj sprite take 'jcg_creek'),
# which keeps JC5's _obj palette group within 255 colours
OBJ_BANK_SKIP.add('levelcreekplatform_2425'); JCG_ENS.append(('levelcreekplatform_2425', 0, '0', 'jcg_creek'))
ENGINE_TYPES_ID.update(JCG_TYPES_ID); ENS += JCG_ENS   # JC_GIM
for _k, _v in JCG_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # JC_GIM

# ---- JC_BOSS (jc_boss.c, 1430-1479): Jade Creek acts 1-4 bosses (build/re/spec_jc_bosses.md): the Hovercop rig (JC1 fly-by,
# JC2 / JC3 chase), PlayerIceDeath, Neera + her hitbox and the LevelIceBarrier (JC3). Types mirror src/game/jc.h (OT_JCB_*). The
# classes are shared with JC5 (jc_boss5.c): JCB_CLS remaps them by class name in JCB_FRAMES only (json 40-43; the per-frame
# remap next to the SB_BOSS ones), the lvobj anim field carries the instance layer << 8.
JCB_FRAMES = {40, 41, 42, 43}
JCB_CLS = {'bosshovercop_2429': 1430, 'bosshovercopturbine1_2430': 1431, 'bosshovercopturbine2_2428': 1432,
    'bosshovercoppropellor_2431': 1433, 'bosshovercoppropellor2_2432': 1434, 'bosshovercopsiren1_2433': 1435,
    'bosshovercopsiren2_2434': 1436, 'bosshovercopsiren3_2427': 1437, 'bosshovercopsiren4_2426': 1438,
    'bosshovercophook_2468': 1439, 'bosshovercophook2_2472': 1440, 'levelhovercraftlight_2465': 1441,
    'playericedeath_2467': 1442, 'npcneera_2483': 1443, 'bossneerahitbox_2482': 1444, 'levelicebarrier_2480': 1445,
    'levelicebarrierpiece_2495': 1446}
for _c, _t in JCB_CLS.items(): ENGINE_TYPES_ID.setdefault((_c.rsplit('_', 1)[0], int(_c.rsplit('_', 1)[1])), _t)   # JC_BOSS (setdefault: JC5 may own the key)
_HC, _NE, _ID, _IB = ('bosshovercop',), ('npcneera',), ('playericedeath',), ('levelicebarrier',)
JCB_ENS = [(c, a, d, nm, None, w) for c, a, d, nm, w in (
    ('bosshovercop_2429', 0, '0', 'jcb_hc', _HC), ('bosshovercop_2429', 0, '16', 'jcb_hc_l', _HC),
    ('bosshovercopturbine1_2430', 0, '0', 'jcb_tb1', _HC), ('bosshovercopturbine1_2430', 0, '16', 'jcb_tb1_l', _HC),
    ('bosshovercopturbine2_2428', 0, '0', 'jcb_tb2', _HC), ('bosshovercopturbine2_2428', 0, '16', 'jcb_tb2_l', _HC),
    ('bosshovercoppropellor_2431', 0, '0', 'jcb_prop', _HC), ('bosshovercopsiren1_2433', 0, '0', 'jcb_sir1', _HC),
    ('bosshovercopsiren2_2434', 0, '0', 'jcb_sir2', _HC), ('bosshovercophook2_2472', 0, '0', 'jcb_hook2', _HC),
    ('bosshovercophook_2468', 0, '0', 'jcb_hook', _ID), ('bosshovercophook_2468', 0, '16', 'jcb_hook_l', _ID),
    ('levelhovercraftlight_2465', 0, '0', 'jcb_light', _HC), ('levelhovercraftlight_2465', 0, '16', 'jcb_light_l', _HC),
    ('bosshovercopchargeglow_2471', 4, '16', 'jcb_glow_l', _HC),   # dir 0's table has a missing image (frame 3): drawn mirrored
    ('bossicecrystal_2466', 0, '0', 'jcb_crystal', _HC), ('bossicemissile_2470', 0, '0', 'jcb_mis', _HC),
    ('bossicemissile_2470', 0, '16', 'jcb_mis_l', _HC), ('effectstickyice_2469', 0, '0', 'jcb_sticky', _HC),
    ('bossmeteor_2435', 0, '0', 'jcb_meteor', _HC), ('playericedeath_2467', 0, '0', 'jcb_iced', _ID),
    ('playericedeath_2467', 7, '0', 'jcb_iced7', _ID), ('levelicebarrier_2480', 0, '0', 'jcb_bar', _IB),
    ('levelicebarrierpiece_2495', 0, '0', 'jcb_barp', _IB), ('bossicering_2481', 0, '0', 'jcb_ring', _NE),
    ('bossneerahitbox_2482', 0, '0', 'jcb_nhit', _NE))] + \
    [('npcneera_2483', a, d, 'jcb_n%d_%s' % (a, d), None, _NE) for a, d in (
     (0, '1'), (0, '17'), (7, '0'), (7, '16'), (7, '1'), (7, '17'), (8, '1'), (8, '17'), (10, '0'), (10, '16'),
     (10, '1'), (10, '17'), (12, '0'), (12, '16'), (13, '0'), (13, '16'), (16, '0'), (17, '0'), (17, '16'), (17, '1'), (17, '17'))]
# Neera's directions (RE 10711 / 10712 / 10729-10732 / 10751 / 10752 / 10762 / 10763: 1 / 17; 10769: toward the player, any of
# 32): anim 0 in 1 / 17 only (dirs 0 / 16 are other images, never set: INFERRED, VRAM); anim 16 dirs 0 / 16 share their images
JCB_ENS_WITH = {'bosshovercopchargeglow_2471': _HC, 'bossicecrystal_2466': _HC, 'bossicemissile_2470': _HC, 'effectstickyice_2469': _HC,
                'bossmeteor_2435': _HC, 'bossicering_2481': _NE}
ENS += JCB_ENS   # JC_BOSS
for _k, _v in JCB_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # JC_BOSS
for _e in JCB_ENS: ROW_FRAMES[_e[3]] = JCB_FRAMES   # JC_BOSS rows only in JC1-JC4 (JC5 exports its own)
# JC5's ice ambush (session 22; jc_boss.c M_AMBUSH, RE 11043-11072 group 0x22afe, RV build/orig_traces/jc_boss5 amb_l / amb_c /
# amb_m): JC5 places the same Hovercop rig, NPCNeera, her hitbox, PlayerIceDeath (her ice block) and the hook, so the remap also runs
# in json 44 (JCB_REMAP_FRAMES). Only the rows that ambush draws are exported there: the rig, the ice missile flying left, the
# spikes, the ice block (anim 7), the hook facing left, Neera's fall / landing / cast (anims 7 / 10 / 12 facing left). The rig's
# parts and the ice block are off-map templates in json 44 (y < 0): JCB_AMBUSH_WITH pulls their rows in with the placed Neera.
JCB_AMBUSH_FRAMES = {44}
JCB_REMAP_FRAMES = JCB_FRAMES | JCB_AMBUSH_FRAMES
JCB_AMBUSH_ROWS = ('jcb_hc_l', 'jcb_tb1', 'jcb_tb1_l', 'jcb_tb2', 'jcb_tb2_l', 'jcb_prop', 'jcb_sir1', 'jcb_sir2', 'jcb_hook2',
                   'jcb_hook_l', 'jcb_light', 'jcb_light_l', 'jcb_mis_l', 'jcb_sticky', 'jcb_iced7', 'jcb_n7_17', 'jcb_n10_17', 'jcb_n12_16')
for _nm in JCB_AMBUSH_ROWS: ROW_FRAMES[_nm] = ROW_FRAMES[_nm] | JCB_AMBUSH_FRAMES
# BossHovercop anim 0 (direction 0) runs at speed 0: only its first image is ever shown (RE anims.json max_speed 0), so JC5 takes
# that image alone ('jcb5_hc'; jc_boss.c falls back to it when 'jcb_hc' is absent). Its anim 7 (11068, E 3) is not exported: the
# body is above the view then (RV amb_l f420: view top 1578, the Hovercop's box 1496-1624) and the frames are 192 x 128 (INFERRED)
ENS.append(('bosshovercop_2429', 0, '0', 'jcb5_hc', slice(0, 1), _HC)); ROW_FRAMES['jcb5_hc'] = JCB_AMBUSH_FRAMES
JCB_AMBUSH_WITH = {'bosshovercopturbine1', 'bosshovercopturbine2', 'bosshovercoppropellor', 'bosshovercopsiren1', 'bosshovercopsiren2',
                   'bosshovercophook', 'bosshovercophook2', 'levelhovercraftlight', 'playericedeath'}
# JC3's moving layer (the subtanker scene 0x22a5d, 10799-10817, spec_jc_bosses 4.8): json layer 4 (port 3) moves after Neera, and
# a player beyond X 10580 is on it (10804 / 10805: she tests that layer's backdrops only). <name>_mvl.bin: 'MVL1' u8 port layer,
# u8 json layer, u16 nobj, tw, th, nuniq, tx0; nobj x {u16 type; s32 x, y} (the layer's objects: they move with it); pad to 4;
# nuniq x 32 B tiles; solid map u8[tw*th]; platform map u8[tw*th] (the layer's own collision in its unmoved coordinates, tile columns from tx0:
# the player is on it only at X > 10580, its x offset is at most 600)
MVL_FRAMES = {42: (4, 3, 9900)}   # json frame: (json layer, port layer, first x of the collision kept)
def write_mvl(path, n, pieces, imgs, objs, W, H):
    jl, pl, x0 = MVL_FRAMES[n]
    TW, TH = (W + 15) // 16, (H + 15) // 16
    tx0 = x0 // 16
    stride = TW * 16
    solid = bytearray(TW * 16 * TH * 16); plat = bytearray(TW * 16 * TH * 16)
    for p in pieces:
        if p.get('layer0') != jl or p['col'] not in (1, 2): continue
        im = imgs[p['img']][0]
        m = b'\x01' * (im.width * im.height) if p['box'] else im.getchannel('A').point(lambda v: 1 if v >= ALPHA_MIN else 0).tobytes()
        tgt = solid if p['col'] == 1 else plat
        for yy in range(p.get('ch', im.height)):
            Y = p['y'] + yy
            if Y < 0 or Y >= TH * 16: continue
            row = m[yy * im.width:yy * im.width + p.get('cw', im.width)]
            for xx, v in enumerate(row):
                X = p['x'] + xx
                if v and 0 <= X < stride: tgt[Y * stride + X] = 1
    uniq = {bytes(32): 0, b'\xff' * 32: 1}; table = [bytes(32), b'\xff' * 32]
    def tilemap(buf):
        out_map = []
        for ty in range(TH):
            for tx in range(tx0, TW):
                bits = bytearray(32)
                for yy in range(16):
                    base = (ty * 16 + yy) * stride + tx * 16; v = 0
                    for xx in range(16):
                        if buf[base + xx]: v |= 1 << xx
                    bits[yy * 2] = v & 255; bits[yy * 2 + 1] = v >> 8
                b = bytes(bits)
                if b not in uniq: uniq[b] = len(table); table.append(b)
                out_map.append(uniq[b])
        return out_map
    sm, pm = tilemap(solid), tilemap(plat)
    mo = [o for o in objs if o['layer'] == pl]
    assert len(table) <= 256
    d = bytearray(b'MVL1' + struct.pack('<2B5H', pl, jl, len(mo), TW - tx0, TH, len(table), tx0))
    for o in mo: d += struct.pack('<H2i', o['type'], o['x'], o['y'])
    d += bytes(-len(d) & 3)   # the tiles start 4-byte aligned (u16 reads on the SH4)
    for t in table: d += t
    d += bytes(sm) + bytes(pm)
    open(path, 'wb').write(bytes(d))
    return 'moving layer: json %d (port %d), %d objects, %d collision tiles' % (jl, pl, len(mo), len(table))

# ---- JC_BOSS5 (jc_boss5.c, 1480-1535): Jade Creek act 5's finale (build/re/spec_jc_boss5.md). Types mirror src/game/jc.h
# (OT_J5_*); they are placed only in json 44. Serpentine 2 is FN3's fight (fn_boss.c types 820-825) with its own hitbox handle
# 2552 (sub id 1 = the JC5 variant: A 180, D 6, no UFO, the fall into the gunship); the Brevon mech FIGHT parts (weak spot,
# turret, charge, laser, warnings; group 0x22af4) are Torque-only (loop_cutscene_start_43: g1 == 5) and not mapped.
JCB5_FRAMES = {44}
JCB5_TYPES_ID = {('bossgunship', 2559): 1480, ('bossgunshipbladeleft', 2557): 1481, ('bossgunshipbladeright', 2558): 1482,
    ('bossgunshipcannonleft', 2566): 1483, ('bossgunshipcannonright', 2567): 1484, ('bossgunshiphitbox', 2577): 1485,
    ('bossgunshiplight', 2562): 1486, ('bossgunshiplightleft', 2563): 1487, ('bossgunshiplightright', 2564): 1488,
    ('bossgunshipmissilerackleft', 2555): 1489, ('bossgunshipmissilerackright', 2556): 1490, ('bossgunshippilot', 2565): 1491,
    ('bossgunshipplatform', 2553): 1492, ('bossgunshiptrooper', 2554): 1493, ('bossgunshipturbineleft', 2560): 1494,
    ('bossgunshipturbineright', 2561): 1495, ('enemyelite', 2529): 1498, ('enemyelitemask', 2528): 1499}
ENGINE_TYPES_ID.update(JCB5_TYPES_ID)
FNBOSS_FRAMES.add(44)                                         # Serpentine 2 (fn_boss.c): its classes are mapped in jc5 too
ENGINE_TYPES_ID[('bossserphitbox', 2552)] = 822
FNBOSS_SUB['bossserphitbox_2552'] = 1
ENS_FRAMES['npcserpentine_945'] = ENS_FRAMES['npcserpentine_945'] | {44}
_G5 = ('bossgunship',)
JCB5_ENS = [(c, a, d, nm, None, _G5) for c, a, d, nm in (
    ('bossgunship_2559', 0, '0', 'j5g_ship'), ('bossgunshipbladeleft_2557', 0, '0', 'j5g_bladel'), ('bossgunshipbladeright_2558', 0, '0', 'j5g_blader'),
    ('bossgunshipcannonleft_2566', 0, '0', 'j5g_canl'), ('bossgunshipcannonleft_2566', 7, '0', 'j5g_canl7'),
    ('bossgunshipcannonright_2567', 0, '0', 'j5g_canr'), ('bossgunshipcannonright_2567', 7, '0', 'j5g_canr7'),
    ('bossgunshiphitbox_2577', 0, '0', 'j5g_hit'), ('bossgunshiphitbox_2577', 0, '8', 'j5g_hit8'),
    ('bossgunshiplight_2562', 0, '0', 'j5g_light'), ('bossgunshiplightleft_2563', 0, '0', 'j5g_ll'), ('bossgunshiplightleft_2563', 0, '1', 'j5g_ll1'),
    ('bossgunshiplightright_2564', 0, '0', 'j5g_lr'), ('bossgunshiplightright_2564', 0, '1', 'j5g_lr1'),
    ('bossgunshipmissilerackleft_2555', 0, '0', 'j5g_rackl'), ('bossgunshipmissilerackright_2556', 0, '0', 'j5g_rackr'),
    ('bossgunshippilot_2565', 0, '0', 'j5g_pilot'), ('bossgunshippilot_2565', 5, '0', 'j5g_pilot5'), ('bossgunshipplatform_2553', 0, '0', 'j5g_plat'),
    ('bossgunshiptrooper_2554', 0, '0', 'j5g_trp'), ('bossgunshiptrooper_2554', 0, '16', 'j5g_trp_l'),
    ('bossgunshipturbineleft_2560', 0, '0', 'j5g_turbl'), ('bossgunshipturbineleft_2560', 0, '8', 'j5g_turbl8'), ('bossgunshipturbineright_2561', 0, '0', 'j5g_turbr'),
    ('bossgunshipflare_2568', 0, '0', 'j5g_flare'), ('bossgunshipflare_2568', 10, '0', 'j5g_flare10'),
    ('bossgunshipflare_2568', 11, '0', 'j5g_flare11'), ('bossgunshipflare_2568', 11, '16', 'j5g_flare11_l'),
    # the Elite troopers (10954-11041): walk 1, jump 7, hurt 8, charge 10, fire 12 (right facing only: drawn flipped), the charge glow,
    # the laser; the lightning bolt (11619, both facings: its hot spots differ)
    ('enemyelite_2529', 1, '0', 'j5e_walk'), ('enemyelite_2529', 7, '0', 'j5e_jump'), ('enemyelite_2529', 8, '0', 'j5e_hurt'),
    ('enemyelite_2529', 10, '0', 'j5e_charge'), ('enemyelite_2529', 12, '0', 'j5e_fire'), ('enemyelitechargeglow_2527', 0, '0', 'j5e_glow'),
    ('enemytrooperlaser_2530', 0, '0', 'j5e_laser'), ('levellightningbolt_2525', 0, '0', 'j5_bolt'), ('levellightningbolt_2525', 0, '16', 'j5_bolt_l'))]
ENS += JCB5_ENS
for _e in JCB5_ENS: ENS_FRAMES[_e[0]] = ENS_FRAMES.get(_e[0], set()) | JCB5_FRAMES
JCB5_ENS_WITH = {'bossgunshipflare_2568': _G5, 'bossmissile_1867': _G5, 'effectsexplosion06_435': _G5, 'effectsexplosion01_367': _G5,
                 'effectsdebris_427': _G5, 'enemyelitechargeglow_2527': _G5, 'enemytrooperlaser_2530': _G5,
                 'playerskinsash_341': _G5, 'playerskincarol_449': _G5, 'playerskinmilla_461': _G5}
for _nm in ('fc_l37l', 'fc_c37l', 'fc_m37'): ROW_FRAMES[_nm] = ROW_FRAMES.get(_nm, {35}) | {44}   # the closing's three heroes (11369 / 11372): FN6's cast rows
for _k, _v in JCB5_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # JC_BOSS5

# the enemies module's Elite rows (jee_*): the only Elites are jc5's, which the act-5 module owns (types 1498 / 1499, its own
# j5e_* rows); jc_enemy.c removes any in stage 5. Leaving them out keeps jc5_ens smaller (session 21 merge).
for _e in JCE_ENS:
    if _e[3].startswith('jee_'): ROW_FRAMES[_e[3]] = set()
# VRAM (session 21 merge): a JC enemy's direction-16 row whose every frame is the exact mirror of the direction-0 frame
# (pixels flipped, hotspot x = w - hx, same hy) is left out; jc_enemy.c draws the base row flipped (frame_draw: flip
# hotspot w - hx). RE images: BullFrag 22/22, SharkTorpedo 14/14 mirrored; the Elite has 3 genuine left frames (kept).
def _jce_mirror_rows(rows=None):   # rows: any ENS list (default the JC one; TH_ENEMIES passes its own)
    from PIL import ImageChops
    an_ = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'frames', 'anims.json')))
    src_ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'extracted', 'linux', 'images')
    out_ = set()
    for c, ai, d, nm in (JCE_ENS if rows is None else rows):
        if not nm.endswith('_l') or not (d.isdigit() and int(d) >= 16): continue   # (jc_enemy.c falls back from '<base>_l' to '<base>' only)
        d0 = str(int(d) - 16)
        try: ad = an_[c]['anims'][ai]; R, L = ad[d0]['frames'], ad[d]['frames']
        except (KeyError, IndexError, TypeError): continue
        ok = len(R) == len(L)
        for r, l in zip(R, L):
            if not ok: break
            A = Image.open(os.path.join(src_, 'img_%d.png' % r)).convert('RGBA'); B_ = Image.open(os.path.join(src_, 'img_%d.png' % l)).convert('RGBA')
            mr = [int(v, 0) for v in open(os.path.join(src_, 'img_%d_meta.txt' % r)).read().split()]
            ml = [int(v, 0) for v in open(os.path.join(src_, 'img_%d_meta.txt' % l)).read().split()]
            ok = (A.size == B_.size and ImageChops.difference(A.transpose(Image.FLIP_LEFT_RIGHT), B_).getbbox() is None
                  and ml[0] == A.width - 1 - mr[0] and ml[1] == mr[1])
        if ok: out_.add(nm)
    return out_
try:
    JCE_MIRRORED = _jce_mirror_rows()
except FileNotFoundError:
    JCE_MIRRORED = set()
for _nm in JCE_MIRRORED: ROW_FRAMES[_nm] = set()
# ==== end Jade Creek

# ==== Trap Hideout (session 21b; json 57-59 = th1-th3; types 1536-1791, one block per module like Jade Creek)
# ---- TH_FLOW (th_flow.c, 1536-1559)
# Stage-wide scenery (spec_th_flow 5, RE create_* / 13532 / 14082 / 14083 / 14099): ThBacklight1_3113 (6 frames, frame by g3), ThFlame_3176
# (torch flame, looping speed 16), ThSpotlight_3209 (TH3 json layer 0, ctor invisible; drawn by the parallax hook). lvobj anim =
# instance layer << 8 | 0x8000 json layer 1 of a mapped frame (write_lv2); the rows come with on-map placements only.
THF_TYPES_ID = {('thbacklight1', 3113): 1536, ('thflame', 3176): 1537, ('thspotlight', 3209): 1538}
THF_ENS = [('thbacklight1_3113', 0, '0', 'thbl'), ('thflame_3176', 0, '0', 'thfl'), ('thspotlight_3209', 0, '0', 'thsp', slice(0, 1))]   # the spotlight's 2 frames (6123 / 2740) are pixel-identical: one image
ENGINE_TYPES_ID.update(THF_TYPES_ID); ENS += THF_ENS   # TH_FLOW

# ---- TH_ENEMIES (th_enemy.c, 1560-1619; build/re/spec_th_enemies.md). Placed classes 1560-1589, runtime children 1590-1619
# (th.h OT_TH_*). Rows 'thX_name' ('_l' = direction 16; left rows that are exact mirrors are left out: _ens_mirror_rows).
# EnemyNinja_3193 (th3) belongs to the act-3 finale (th_boss.c), not here.
THE_TYPES_ID = {('enemytotematic', 3144): 1560, ('enemymetalpup', 3148): 1561, ('enemyshockpuppet', 3149): 1562,
    ('enemybladeorbiter', 3165): 1563, ('enemybladeorbiterleft', 3163): 1564, ('enemybladeorbiterright', 3164): 1565,
    ('enemyrocketpuncher', 3182): 1566, ('enemyrocketpuncherhand', 3183): 1567, ('enemyrocketpunchershield', 3184): 1568,
    ('effectrocketpuncherwarning', 3185): 1569, ('enemybikeninja', 3203): 1570, ('enemybikeninjamask', 3204): 1571,
    ('effectbikeninjawarning', 3206): 1572}
THE_ENS = [(c, a, d, nm + ('' if d == '0' else '_l')) for c, a, nm in (
    ('enemymetalpup_3148', 0, 'thp_idle'), ('enemymetalpup_3148', 1, 'thp_walk'), ('enemymetalpup_3148', 7, 'thp_bite'),
    ('enemymetalpup_3148', 8, 'thp_dead'), ('enemymetalpup_3148', 10, 'thp_turn'),
    ('enemyrocketpuncher_3182', 0, 'thr_idle'), ('enemyrocketpuncher_3182', 7, 'thr_wind'), ('enemyrocketpuncher_3182', 8, 'thr_back'),
    ('enemyrocketpuncher_3182', 10, 'thr_dead'), ('enemyrocketpuncherhand_3183', 0, 'thr_hand'), ('enemyrocketpuncherhand_3183', 7, 'thr_fly'),
    ('enemyrocketpuncherhand_3183', 8, 'thr_hand2'), ('enemyrocketpunchershield_3184', 0, 'thr_shield'),
    ('enemybikeninja_3203', 0, 'thb_ride'), ('enemybikeninja_3203', 7, 'thb_jump'), ('enemybikeninja_3203', 9, 'thb_throw'),
    ('enemybikeninjadefeated_3205', 0, 'thb_dead')) for d in ('0', '16')] + [
    ('enemytotematic_3144', 0, '0', 'tht_body'), ('enemyshockpuppet_3149', 0, '0', 'ths_idle'), ('enemyshockpuppet_3149', 7, '0', 'ths_wind'),
    ('enemyshockpuppet_3149', 8, '0', 'ths_shock'), ('enemyshockpuppet_3149', 10, '0', 'ths_dead'),
    ('enemybladeorbiter_3165', 0, '0', 'tho_body'), ('enemybladeorbiter_3165', 7, '0', 'tho_blink'), ('enemybladeorbiter_3165', 8, '0', 'tho_dead'),
    ('enemybladeorbiterleft_3163', 0, '0', 'tho_bl'), ('enemybladeorbiterright_3164', 0, '0', 'tho_br'),
    ('effectrocketpuncherwarning_3185', 0, '0', 'thr_warn'), ('effectbikeninjawarning_3206', 0, '0', 'thb_warn'),
    ('enemyninjastar_3221', 0, '0', 'thb_star'), ('enemyninjastar_3221', 0, '1', 'thb_star1')] + \
    [('enemybikeninjamask_3204', 0, str(d), 'thb_m%d' % d) for d in range(5)]
_THE_ALL = ('enemytotematic', 'enemymetalpup', 'enemyshockpuppet', 'enemybladeorbiter', 'enemyrocketpuncher', 'enemybikeninjamask')
THE_ENS_WITH = {'enemyninjastar_3221': ('enemybikeninjamask',), 'enemybikeninjadefeated_3205': ('enemybikeninjamask',),
    'objectkeystone_429': ('enemyrocketpuncher',), 'effectsdebris_427': _THE_ALL, 'effectsexplosion06_435': ('enemyrocketpuncher',)}
ENGINE_TYPES_ID.update(THE_TYPES_ID); ENS += THE_ENS   # TH_ENEMIES
for _k, _v in THE_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # TH_ENEMIES
try:
    THE_MIRRORED = _jce_mirror_rows(THE_ENS)   # (the JC helper, generalised to any row list)
except FileNotFoundError:
    THE_MIRRORED = set()
for _nm in THE_MIRRORED: ROW_FRAMES[_nm] = set()   # TH_ENEMIES

# ---- TH_GIM (th_gim.c, 1620-1679; build/re/spec_th_gimmicks.md). Families share a type; THG_SUB gives the member index (lvobj
# anim bits 0-7, the instance layer in bits 8-14, as JC_GIM). LevelTHCollapsingPlatform_3186 (+ Piece_3187) is Jade Creek's
# collapsing platform (jc_gim.c OT_JCG_COLL member 2: the same events 13598-13602 / loop_collapse_57, RE).
THG_TYPES_ID = {('hazardscanlasersweep', 3128): 1620, ('hazardscanlaserright', 3129): 1620, ('hazardscanlaserleft', 3130): 1620,
    ('hazardscanlaserdown', 3131): 1620, ('hazardscanlaserup', 3132): 1620, ('hazardscanlaserhead', 3133): 1621,
    ('hazardscanlaserhead2', 3134): 1621, ('hazardscanlaserhead3', 3137): 1621, ('hazardscanlaserhead4', 3135): 1621,
    ('hazardscanlaserhead5', 3136): 1621, ('levelhideoutplatform1', 3172): 1622, ('levelhideoutplatformflame', 3173): 1623,
    ('hazardshuriken', 3166): 1625, ('hazardshurikenbase', 3167): 1626, ('hazardshurikenturn', 3168): 1627,
    ('gimmickbarrel', 3177): 1628, ('levelthcollapsingplatform', 3186): 1378}   # (THFlame / THBacklight1: th_flow.c types 1537 / 1536, merge)
THG_SUB = {'hazardscanlasersweep_3128': 0, 'hazardscanlaserright_3129': 1, 'hazardscanlaserleft_3130': 2, 'hazardscanlaserdown_3131': 3,
    'hazardscanlaserup_3132': 4, 'hazardscanlaserhead_3133': 0, 'hazardscanlaserhead2_3134': 2, 'hazardscanlaserhead3_3137': 3,
    'hazardscanlaserhead4_3135': 4, 'hazardscanlaserhead5_3136': 5}
JCG_SUB['levelthcollapsingplatform_3186'] = 2
THG_ENS = [(c, 0, d, nm) for c, d, nm in (
    ('hazardscanlasersweep_3128', '0', 'thg_beam0'), ('hazardscanlasersweep_3128', '1', 'thg_beam1'),   # the five beam classes share these images
    ('hazardscanlaserhead_3133', '0', 'thg_head'), ('hazardscanlaserhead2_3134', '0', 'thg_head2'), ('hazardscanlaserhead3_3137', '0', 'thg_head3'),
    ('hazardscanlaserhead4_3135', '0', 'thg_head4'), ('hazardscanlaserhead5_3136', '0', 'thg_head5'),
    ('levelhideoutplatform1_3172', '0', 'thg_pillar'), ('levelhideoutplatformflame_3173', '8', 'thg_pflame'),
    ('hazardshuriken_3166', '0', 'thg_shur'), ('hazardshurikenbase_3167', '0', 'thg_shbase'), ('gimmickbarrel_3177', '0', 'thg_barrel'),
    ('gimmickbarreldebris_3127', '0', 'thg_bdebris'),
    ('levelthcollapsingplatform_3186', '0', 'thg_coll'), ('levelthcollapsingplatformpiece_3187', '0', 'thg_cpiece'))]
_THG_BEAMS = ('hazardscanlasersweep', 'hazardscanlaserright', 'hazardscanlaserleft', 'hazardscanlaserdown', 'hazardscanlaserup')
THG_ENS.append(('effectsspringbeam_540', 4, '0', 'thg_sbeam', None, ('gimmickbarrel',)))   # 13588: a barrel on a YellowSpringUp
ROW_FRAMES['thg_sbeam'] = {58, 59}                   # (the class's FN rows stay FN-only through ENS_FRAMES)
THG_ENS_WITH = {'hazardscanlasersweep_3128': _THG_BEAMS[1:], 'effectsdust_437': _THG_BEAMS,
    'gimmickbarreldebris_3127': ('gimmickbarrel',), 'levelthcollapsingplatformpiece_3187': ('levelthcollapsingplatform',),
    'effectsspringbeam_540': ('gimmickbarrel',)}
# the barrel ride (13581 / 13582) forces skin animation 40 direction 14 / 30 on Carol (her banks hold directions 0 / 16 only); Milla's
# bank holds every direction. Direction 30 is the pixel mirror of 14 on Carol's skin, tail and bike (checked against the images):
# player_pose draws the 14 row flipped. Only the acts that place a barrel (TH2 / TH3).
for _n in (58, 59): CAROL_POSE_DIRS[_n] = {**CAROL_POSE_DIRS.get(_n, {}), 40: ('14',)}
ENGINE_TYPES_ID.update(THG_TYPES_ID); ENS += THG_ENS   # TH_GIM
for _k, _v in THG_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # TH_GIM

# ---- TH_BOSS (th_boss.c, 1680-1791): Trap Hideout bosses (build/re/spec_th_bosses.md). Types mirror src/game/th.h (OT_THB_*); the
# lvobj anim field carries the instance layer << 8 | 0x8000 json layer 1 (th_boss.c spawn). Rows: the Laser Iris rig (TH1), the scan
# laser boss, the Crystal Basher (TH3), the TH3 arena (crowd, goal message; the counter draws the HUD digits; the bike ninja's
# drop-in anim 10), and Spade's TH3 anims 7 / 10 / 38 (sb_spade.c drop-in variant: BossSpade* keep their SB_BOSS types in TH3).
THB_TYPES_ID = {('bosslaseriris', 3141): 1680, ('bosslaseririscannon', 3140): 1681, ('bosslaseririswingleft', 3138): 1682,
                ('bosslaseririswingright', 3139): 1683, ('hazardscanlaserboss', 3142): 1684,
                ('bosscrystalbasherbase', 3199): 1685, ('bosscrystalbasherface', 3198): 1686, ('bosscrystalbasherhead', 3197): 1687,
                ('bosscrystalbashersegment', 3194): 1688, ('bosscrystalspike', 3195): 1689, ('bosscrystalspike2', 3196): 1690,
                ('bossdoor', 3200): 1691, ('thcrowd', 3213): 1692, ('guigoalmessage', 3217): 1694, ('guienemiesleft', 3218): 1695}
ENGINE_TYPES_ID.update(THB_TYPES_ID)
THB_SPADE_FRAMES = {59}   # TH3 places BossSpadeMask / BossSpade / BossSpadeHitbox (SB_BOSS types 1220-1222, sb_spade.c)
THB_SUB = {}   # class -> sub id (low byte of the lvobj anim field)
_IR = ('bosslaseriris',)
_CB = ('bosscrystalbasherbase',)
THB_ENS = [(c, a, d, nm, None, w) for c, a, d, nm, w in (
    ('bosslaseriris_3141', 0, '0', 'thb_iris', _IR), ('bosslaseriris_3141', 0, '8', 'thb_iris8', _IR),
    ('bosslaseriris_3141', 0, '16', 'thb_iris16', _IR), ('bosslaseriris_3141', 0, '24', 'thb_iris24', _IR),
    ('bosslaseririscannon_3140', 0, '0', 'thb_can', _IR), ('bosslaseririscannon_3140', 0, '16', 'thb_can16', _IR),
    ('bosslaseririswingleft_3138', 0, '0', 'thb_wl', _IR), ('bosslaseririswingleft_3138', 0, '16', 'thb_wl16', _IR),
    ('bosslaseririswingright_3139', 0, '0', 'thb_wr', _IR), ('bosslaseririswingright_3139', 0, '16', 'thb_wr16', _IR),
    ('hazardscanlaserboss_3142', 0, '0', 'thb_las', ('hazardscanlaserboss',)), ('hazardscanlaserboss_3142', 0, '1', 'thb_las1', ('hazardscanlaserboss',)),
    ('bossdoor_3200', 0, '0', 'thb_door', ('bossdoor',)), ('bosscrystalbasherbase_3199', 0, '0', 'thc_base', _CB),
    ('bosscrystalbasherhead_3197', 0, '0', 'thc_head', _CB), ('bosscrystalbashersegment_3194', 0, '0', 'thc_seg', _CB),
    ('bosscrystalspike_3195', 0, '0', 'thc_spike', _CB), ('bosscrystalspike2_3196', 0, '0', 'thc_spike2', _CB),
    ('bosscrystalbasherface_3198', 0, '0', 'thc_face', _CB),
    ('thcrowd_3213', 0, '0', 'thb_crowd', ('thcrowd',)), ('guigoalmessage_3217', 0, '0', 'thb_goal', ('guigoalmessage',)),
    ('enemybikeninja_3203', 10, '0', 'thb_drop', ('guienemiesleft',)), ('enemybikeninja_3203', 10, '16', 'thb_drop_l', ('guienemiesleft',)))] + \
    [('bossspade_2247', a, d, 'sbp_%d' % a + ('' if d == '0' else '_l'), None, ('bossspade',)) for a in (7, 10, 38) for d in ('0', '16')]
THB_ENS_WITH = {'effectsexplosion06_435': _IR + _CB, 'effectshade_436': _IR, 'effectsdust_437': ('hazardscanlaserboss',)}
ENS += THB_ENS   # TH_BOSS
for _k, _v in THB_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # TH_BOSS
for _e in THB_ENS: ROW_FRAMES[_e[3]] = TH_FRAMES   # TH_BOSS rows only in Trap Hideout

# ==== end Trap Hideout

# ==== Thermal Base (session 21c; json 45-50 = tb1-tb6; types 1792-2047, one block per module like Trap Hideout)
# ---- TB_FLOW (tb_flow.c, 1792-1823; build/re/spec_tb_flow.md)
# LevelLift_2649 (RE 11709-11719 / 11881 / 12030, RV tb1_start: a 96 x 96 box riding 8 px a frame between the LevelLiftTop_2650 /
# LevelLiftBottom_2651 markers (64 x 16, invisible from the ctor); Q7 Q13), TBWallLightAnim_2644 (scenery: 11826 / 12091 force
# frame (g3 / 4) mod 12 every frame). The rows come with on-map placements only (NEWPACK_FRAMES).
TBF_TYPES_ID = {('levellift', 2649): 1792, ('levellifttop', 2650): 1793, ('levelliftbottom', 2651): 1794, ('tbwalllightanim', 2644): 1795}
TBF_ENS = [('levellift_2649', 0, '0', 'tblift'), ('tbwalllightanim_2644', 0, '0', 'tbwl')]
ENGINE_TYPES_ID.update(TBF_TYPES_ID); ENS += TBF_ENS   # TB_FLOW
# water (MasterLevel A = 1: TB4 12461 ML.B 3400, TB5 12546 ML.B 2920; TB6 12733 writes A 1, B 2920 below its 2000-px frame: dry in
# effect, left out like TH; TB1-TB3 A 0): air meter + water effects. RV tb4_start / tb5_start: ml_A 1, ml_B 3400 / 2920
WATER_FRAMES = WATER_FRAMES + (48, 49)

# ---- TB_ENEMIES (tb_enemy.c, 1824-1887; build/re/spec_tb_enemies.md). Placed classes 1824-1855 (src/game/tb.h OT_TBE_*), runtime
# children 1856-1887. Rows 'tbX_name' + a direction suffix ('' = 0, '_l' = 16, '_<d>' otherwise); a row whose every frame is the
# exact mirror of the row of direction (16 - d) & 31 is left out (_tbe_mirror_rows) and drawn flipped (enemy_kit.h ek_set_ad).
# Not here: EnemyWallPusher_2748 (TB2's miniboss arena script with the trap door, 11996-12011: bosses), NPCSyntaxEye_2682 (no Q20,
# invisible: flow / bosses), EnemySeeker2_2700 / 3_2703 (TB2 off-map templates no event references).
TBE_TYPES_ID = {('enemyseeker', 2603): 1824, ('enemypushup', 2669): 1825, ('enemypushuphitbox', 2663): 1826,
    ('enemylavasquid', 2740): 1827, ('enemytankbot', 2838): 1828, ('enemysaucer', 2828): 1829, ('enemysaucerleg1', 2827): 1830,
    ('enemysaucerleg2', 2829): 1831, ('enemysaucerleg3', 2830): 1832, ('enemysaucerleg4', 2826): 1833, ('enemysaucerlimiter', 2832): 1834}
_TBE_SFX = {'0': '', '16': '_l'}
_TBE_ROWS = [   # (class, anim, directions, row)
    ('enemyseeker_2603', 0, ('0', '16'), 'tbs_idle'), ('enemyseeker_2603', 7, ('0', '16'), 'tbs_open'), ('enemyseeker_2603', 8, ('0', '16'), 'tbs_close'),
    ('enemyhomingbullet_2648', 0, ('0',), 'tbs_bul'),
    ('enemypushup_2669', 0, ('0', '1'), 'tbp_up'), ('enemypushup_2669', 5, ('0', '1'), 'tbp_fire'),
    ('enemypushuphitbox_2663', 0, ('0',), 'tbp_head'), ('enemypushuphitbox_2663', 8, ('0',), 'tbp_head8'),
    ('enemypushuplaser_2670', 0, ('0',), 'tbp_las'), ('enemypushuplaser2_2695', 0, ('0',), 'tbp_las2'), ('enemypushuplaser3_2696', 4, ('0',), 'tbp_las3'),
    ('enemypushupdebris_2664', 0, ('0',), 'tbp_deb'),
    ('enemylavasquid_2740', 0, ('0', '16'), 'tbq_idle'), ('enemylavasquid_2740', 7, ('0', '1', '15', '16', '17', '31'), 'tbq_fire'),
    ('enemylavasquid_2740', 9, ('0',), 'tbq_turn'), ('enemylavasquid_2740', 11, ('0', '1', '15', '16', '17', '31'), 'tbq_swim'),
    ('enemylavasquidlaser_2741', 0, ('0',), 'tbq_las'), ('enemylavasquiddebris_2742', 0, ('0', '16'), 'tbq_deb'),
    ('enemytankbot_2838', 0, ('0', '16'), 'tbt_idle'), ('enemytankbot_2838', 1, ('0', '16'), 'tbt_walk'), ('enemytankbot_2838', 7, ('0', '16'), 'tbt_fire'),
    ('enemytankbot_2838', 8, ('0', '16'), 'tbt_dead'), ('enemytankbotdebris_2839', 0, ('0', '16'), 'tbt_deb'),
    ('enemysaucer_2828', 0, ('0',), 'tba_body'), ('enemysaucer_2828', 0, ('1',), 'tba_open'), ('enemysaucerleg1_2827', 0, ('0',), 'tba_leg1'),
    ('enemysaucerleg2_2829', 0, ('0',), 'tba_leg2'), ('enemysaucerdebris_2831', 0, ('0',), 'tba_deb')]
# runtime rows come with their creator (the class's on-map placements)
_TBE_WITH = {'enemyhomingbullet_2648': ('enemyseeker',), 'enemypushuplaser_2670': ('enemypushup',), 'enemypushuplaser2_2695': ('enemypushup',),
    'enemypushuplaser3_2696': ('enemypushup',), 'enemypushupdebris_2664': ('enemypushup',), 'enemylavasquidlaser_2741': ('enemylavasquid',),
    'enemylavasquiddebris_2742': ('enemylavasquid',), 'enemytankbotdebris_2839': ('enemytankbot',), 'enemysaucerdebris_2831': ('enemysaucer',),
    'enemypushuphitbox_2663': ('enemypushup',)}
TBE_ENS = [(c, a, d, nm + _TBE_SFX.get(d, '_' + d), None, _TBE_WITH.get(c, (c.rsplit('_', 1)[0],))) for c, a, ds, nm in _TBE_ROWS for d in ds]
_TBE_ALL = ('enemyseeker', 'enemypushup', 'enemylavasquid', 'enemytankbot', 'enemysaucer')
# GimmickFireball_1192 / EffectEmber_1025 (Relic Maze's fireball, the same class: the saucer's shots, 12406-12409 / 12423 / 5703 / 5710);
# EnemyPellet_934 (the tank's shots) is the objects bank's 'pellet'; keystones / debris for the Turretus-style crashes
TBE_ENS_WITH = {'gimmickfireball_1192': ('enemysaucer',), 'effectember_1025': ('enemysaucer',), 'objectkeystone_429': _TBE_ALL,
    'effectsdebris_427': _TBE_ALL, **_TBE_WITH}
def _tbe_mirror_rows(rows):   # rows of direction d that are the exact mirror of direction (16 - d) & 31 (generalised _jce_mirror_rows)
    from PIL import ImageChops
    an_ = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'frames', 'anims.json')))
    src_ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'extracted', 'linux', 'images')
    out_ = set()
    for e in rows:
        c, ai, d, nm = e[:4]
        if not d.isdigit() or int(d) not in (15, 16, 17): continue   # the left-facing side; its mirror is 1 / 0 / 31
        dm = str((16 - int(d)) & 31)
        try: ad = an_[c]['anims'][ai]; R, L = ad[dm]['frames'], ad[d]['frames']
        except (KeyError, IndexError, TypeError): continue
        ok = len(R) == len(L)
        for r, l in zip(R, L):
            if not ok: break
            A = Image.open(os.path.join(src_, 'img_%d.png' % r)).convert('RGBA'); B_ = Image.open(os.path.join(src_, 'img_%d.png' % l)).convert('RGBA')
            mr = [int(v, 0) for v in open(os.path.join(src_, 'img_%d_meta.txt' % r)).read().split()]
            ml = [int(v, 0) for v in open(os.path.join(src_, 'img_%d_meta.txt' % l)).read().split()]
            ok = (A.size == B_.size and ImageChops.difference(A.transpose(Image.FLIP_LEFT_RIGHT), B_).getbbox() is None
                  and ml[0] == A.width - 1 - mr[0] and ml[1] == mr[1])
        if ok: out_.add(nm)
    return out_
ENGINE_TYPES_ID.update(TBE_TYPES_ID); ENS += TBE_ENS   # TB_ENEMIES
for _k, _v in TBE_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # TB_ENEMIES
for _e in TBE_ENS: ROW_FRAMES[_e[3]] = TB_FRAMES   # TB_ENEMIES rows only in Thermal Base
try:
    TBE_MIRRORED = _tbe_mirror_rows(TBE_ENS)
except FileNotFoundError:
    TBE_MIRRORED = set()
for _nm in TBE_MIRRORED: ROW_FRAMES[_nm] = set()   # TB_ENEMIES

# ---- TB_GIM (tb_gim.c, 1888-1967; build/re/spec_tb_gimmicks.md). Families share a type; TBG_SUB gives the member index (lvobj
# anim bits 0-7, the instance layer in bits 8-14, as TH_GIM). Types mirror src/game/tb.h OT_TBG_*.
TBG_TYPES_ID = {('objectkeycard', 2685): 1890, ('lockeddoorterminal', 2689): 1891, ('lockeddoorback', 2658): 1892, ('lockeddoorfront', 2659): 1892,
    ('lockeddoorseal', 2660): 1892, ('lockeddoorbarrier', 2688): 1893, ('metaldoorback', 2641): 1894, ('metaldoorfront', 2643): 1894,
    ('levelfireplatform1', 2708): 1896, ('levelfireplatform1', 2665): 1934,
    ('levelfireplatformflame1', 2666): 1897, ('levelfireplatformflame2', 2667): 1897,
    ('levelscrollingplatform1', 2677): 1898, ('levelscrollingplatform2', 2678): 1898, ('levelscrollingplatformmask1', 2680): 1899,
    ('levelscrollingplatformmask2', 2681): 1899, ('levelscrollingplatformflame1', 2668): 1900, ('levelscrollingplatformflame2', 2679): 1900,
    ('levelscrollingplatformstart', 2687): 1901, ('levelscrollingplatformend', 2686): 1901, ('levelscrollingplatformplayerpush', 2697): 1902,
    ('levelpistonplatform1', 2713): 1903, ('levelpistonplatformmask', 2712): 1904, ('levelspinplatform', 2731): 1905,
    ('levelspinplatformmask', 2733): 1906, ('levelspinplaformsupport', 2732): 1907, ('airventplatform', 2694): 1908,
    ('hazardlavapit', 2736): 1909, ('hazardlavafall', 2737): 1910, ('hazardlavafall3', 2777): 1910, ('hazardlavafallwarning', 2739): 1911,
    ('magmaflow', 2676): 1912, ('objectexplosiveblock', 2738): 1913, ('switchbuttonleft', 2707): 1914, ('switchbuttonright', 2705): 1914,
    ('hazardfloortrap', 2761): 1915, ('playerblocker', 2780): 1916,
    ('gimmickconveyorbeltright', 2835): 1917, ('gimmickconveyorbeltright2', 2836): 1917, ('gimmickconveyorbeltright3', 2837): 1917,
    ('gimmickconveyorbeltleft', 2844): 1917, ('gimmickconveyorbeltleft2', 2845): 1917, ('gimmickconveyorbeltleft3', 2846): 1917,
    ('tbpipeloop2', 2816): 1918, ('tbpipeloop3', 2817): 1918, ('tbpipeloop6', 2821): 1918, ('tbpipeloop7', 2822): 1918,
    ('tbpipeloop5', 2820): 1919, ('tbpipeloop9', 2842): 1919, ('tbpipeloop11', 2859): 1919,
    ('tbpipeloop1', 2815): 1920, ('tbpipeloop4', 2818): 1920, ('tbpipeloop8', 2841): 1920, ('tbpipeloop10', 2858): 1921,
    ('gimmickcrankplatformtop1', 2809): 1922, ('gimmickcrankplatformtop2', 2810): 1922,
    ('gimmickcrankplatformmid1', 2803): 1923, ('gimmickcrankplatformmid2', 2804): 1923, ('gimmickcrankplatformbase1', 2805): 1923,
    ('gimmickcrankplatformbase2', 2806): 1923, ('gimmickcrankplatformarm1', 2808): 1923, ('gimmickcrankplatformarm2', 2807): 1923,
    ('gimmickgiantpressertube', 2856): 1924, ('gimmckgiantpresser', 2857): 1924, ('gimmckgiantpresserhitbox', 2879): 1925,
    ('gimmickwaterturbine', 2863): 1926, ('tubebubblespawner', 2848): 1927, ('underwaterbgturbine', 2880): 1929,
    ('tbwaterlight', 2866): 1930, ('gimmickwaterlever1', 2871): 1931, ('gimmickwaterlever2', 2877): 1931,
    ('gimmickwaterlevelmask1', 2872): 1932, ('gimmickwaterlevelmask2', 2878): 1932,
    ('gimmickbubblewavespawner', 2833): 1933, ('gimmickbubblewavespawner2', 2834): 1933}
TBG_SUB = {'lockeddoorfront_2659': 1, 'lockeddoorseal_2660': 2, 'metaldoorfront_2643': 1,
    'levelfireplatformflame2_2667': 1, 'levelscrollingplatform2_2678': 1, 'levelscrollingplatformmask2_2681': 1,
    'levelscrollingplatformflame2_2679': 1, 'levelscrollingplatformend_2686': 1, 'hazardlavafall3_2777': 1, 'switchbuttonright_2705': 1,
    'gimmickconveyorbeltright2_2836': 1, 'gimmickconveyorbeltright3_2837': 2, 'gimmickconveyorbeltleft_2844': 3,
    'gimmickconveyorbeltleft2_2845': 4, 'gimmickconveyorbeltleft3_2846': 5,
    'tbpipeloop3_2817': 1, 'tbpipeloop6_2821': 2, 'tbpipeloop7_2822': 3, 'tbpipeloop9_2842': 1, 'tbpipeloop11_2859': 2,
    'tbpipeloop4_2818': 1, 'tbpipeloop8_2841': 2,
    'gimmickcrankplatformtop2_2810': 1, 'gimmickcrankplatformmid2_2804': 1, 'gimmickcrankplatformbase1_2805': 2,
    'gimmickcrankplatformbase2_2806': 3, 'gimmickcrankplatformarm1_2808': 4, 'gimmickcrankplatformarm2_2807': 5,
    'gimmckgiantpresser_2857': 1, 'gimmickwaterlever2_2877': 1, 'gimmickwaterlevelmask2_2878': 1, 'gimmickbubblewavespawner2_2834': 1}
TBG_ENS = [(c, a, d, nm) for c, a, d, nm in (
    ('objectkeycard_2685', 0, '0', 'tbg_key'),
    ('lockeddoorterminal_2689', 0, '0', 'tbg_term'), ('lockeddoorterminal_2689', 7, '0', 'tbg_term7'),
    ('lockeddoorback_2658', 0, '0', 'tbg_ldback'), ('lockeddoorfront_2659', 0, '0', 'tbg_ldfront'), ('lockeddoorseal_2660', 0, '0', 'tbg_seal'),
    ('lockeddoorbarrier_2688', 0, '0', 'tbg_barrier'), ('metaldoorback_2641', 0, '0', 'tbg_mdback'), ('metaldoorfront_2643', 0, '0', 'tbg_mdfront'),
    ('levelfireplatform1_2708', 0, '0', 'tbg_fplat'),
    ('levelfireplatformflame1_2666', 0, '0', 'tbg_ff0'), ('levelfireplatformflame1_2666', 0, '1', 'tbg_ff1'),
    ('levelfireplatformflame1_2666', 0, '16', 'tbg_ff16'), ('levelfireplatformflame1_2666', 0, '17', 'tbg_ff17'),
    ('levelscrollingplatform1_2677', 0, '0', 'tbg_splat0'), ('levelscrollingplatform2_2678', 0, '0', 'tbg_splat1'),
    ('levelscrollingplatformmask1_2680', 0, '0', 'tbg_smask0'), ('levelscrollingplatformmask2_2681', 0, '0', 'tbg_smask1'),
    ('levelscrollingplatformflame1_2668', 0, '0', 'tbg_sf0'), ('levelscrollingplatformflame1_2668', 0, '1', 'tbg_sf1'),
    ('levelscrollingplatformflame1_2668', 0, '16', 'tbg_sf16'), ('levelscrollingplatformflame1_2668', 0, '17', 'tbg_sf17'),
    ('levelpistonplatform1_2713', 0, '0', 'tbg_piston'), ('levelpistonplatformmask_2712', 0, '0', 'tbg_pmask'),
    ('levelspinplatform_2731', 0, '0', 'tbg_spin'), ('levelspinplatformmask_2733', 0, '0', 'tbg_spinmask'),
    ('levelspinplaformsupport_2732', 0, '0', 'tbg_spinsup'), ('airventplatform_2694', 0, '0', 'tbg_vent'),
    ('hazardlavapit_2736', 0, '0', 'tbg_lava'), ('hazardlavafall_2737', 0, '0', 'tbg_lfall'), ('hazardlavafall3_2777', 0, '0', 'tbg_lfall3'),
    ('hazardlavafallwarning_2739', 0, '0', 'tbg_lwarn0'), ('hazardlavafallwarning_2739', 0, '1', 'tbg_lwarn1'),
    ('magmaflow_2676', 0, '0', 'tbg_magma'), ('objectexplosiveblock_2738', 0, '0', 'tbg_xblock'),
    ('switchbuttonleft_2707', 0, '0', 'tbg_btn0'), ('switchbuttonright_2705', 0, '0', 'tbg_btn1'),
    ('objectexplosiveblockdestroyer_2744', 0, '0', 'tbg_destroyer'),
    ('hazardfloortrap_2761', 0, '0', 'tbg_ftrap'), ('hazardfloortrap_2761', 7, '0', 'tbg_ftrap7'), ('playerblocker_2780', 0, '0', 'tbg_blocker'),
    ('gimmickconveyorbeltright_2835', 0, '0', 'tbg_conv0'), ('gimmickconveyorbeltright2_2836', 0, '0', 'tbg_conv1'),
    ('gimmickconveyorbeltright3_2837', 0, '0', 'tbg_conv2'),
    ('tbpipeloop2_2816', 0, '0', 'tbl_0_0'), ('tbpipeloop3_2817', 0, '0', 'tbl_0_1'), ('tbpipeloop6_2821', 0, '0', 'tbl_0_2'),
    ('tbpipeloop7_2822', 0, '0', 'tbl_0_3'), ('tbpipeloop5_2820', 0, '0', 'tbl_0b_0'), ('tbpipeloop9_2842', 0, '0', 'tbl_0b_1'),
    ('tbpipeloop11_2859', 0, '0', 'tbl_0b_2'), ('tbpipeloop1_2815', 0, '0', 'tbl_1_0'), ('tbpipeloop4_2818', 0, '0', 'tbl_1_1'),
    ('tbpipeloop8_2841', 0, '0', 'tbl_1_2'), ('tbpipeloop10_2858', 0, '0', 'tbl_1b_0'),
    ('gimmickcrankplatformtop1_2809', 0, '0', 'tbg_ctop'), ('gimmickcrankplatformmid1_2803', 0, '0', 'tbg_cmid'),
    ('gimmickcrankplatformbase1_2805', 0, '0', 'tbg_cbase'),
    ('gimmickgiantpressertube_2856', 0, '0', 'tbg_ptube'), ('gimmckgiantpresser_2857', 0, '0', 'tbg_press'),
    ('gimmckgiantpresserhitbox_2879', 0, '0', 'tbg_phit'), ('gimmickwaterturbine_2863', 0, '0', 'tbg_turb'),
    ('tubebubble_2849', 0, '0', 'tbg_tbub'), ('underwaterbgturbine_2880', 0, '0', 'tbg_bgturb'), ('tbwaterlight_2866', 0, '0', 'tbg_wtlight'),
    ('gimmickwaterlever1_2871', 0, '0', 'tbg_lever'), ('gimmickwaterlevelmask1_2872', 0, '0', 'tbg_wmask'),
    ('gimmickbubblewavespawner_2833', 0, '0', 'tbg_bwave0'), ('gimmickbubblewavespawner2_2834', 0, '0', 'tbg_bwave1'),
    ('guikeycardicon_2692', 0, '0', 'tbg_kicon'))]
# rows shared by sibling classes (same images) or created at run time: exported where a sibling / creator is placed
TBG_ENS_WITH = {'levelfireplatform1_2708': ('levelfireplatform1',), 'levelfireplatformflame1_2666': ('levelfireplatformflame2',),
    'levelscrollingplatformflame1_2668': ('levelscrollingplatformflame2',), 'gimmickconveyorbeltright_2835': ('gimmickconveyorbeltleft',),
    'gimmickconveyorbeltright2_2836': ('gimmickconveyorbeltleft2',), 'gimmickconveyorbeltright3_2837': ('gimmickconveyorbeltleft3',),
    'gimmickcrankplatformtop1_2809': ('gimmickcrankplatformtop2',), 'gimmickcrankplatformmid1_2803': ('gimmickcrankplatformmid2',),
    'gimmickcrankplatformbase1_2805': ('gimmickcrankplatformbase2', 'gimmickcrankplatformarm1', 'gimmickcrankplatformarm2'),
    'gimmickwaterlever1_2871': ('gimmickwaterlever2',), 'gimmickwaterlevelmask1_2872': ('gimmickwaterlevelmask2',),
    'objectexplosiveblockdestroyer_2744': ('switchbuttonleft', 'switchbuttonright'),
    'tubebubble_2849': ('tubebubblespawner', 'gimmickwaterturbine'), 'guikeycardicon_2692': ('objectkeycard',)}
ENGINE_TYPES_ID.update(TBG_TYPES_ID); ENS += TBG_ENS   # TB_GIM
for _e in TBG_ENS: ROW_FRAMES[_e[3]] = TB_FRAMES   # TB_GIM rows only in Thermal Base
for _k, _v in TBG_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # TB_GIM

# ---- TB_BOSS (tb_boss.c, 1968-2047): Thermal Base bosses (build/re/spec_tb_bosses.md). Types mirror src/game/tb.h (OT_TBB_*); the
# lvobj anim field carries TBB_SUB (the member of a family type) | instance layer << 8 (tb_boss.c spawn). Rows tied to their
# encounter's on-map class (the off-map templates do not pull rows in a NEWPACK frame): the RoboShrimp (TB1), the wall pusher (TB2).
TBB_TYPES_ID = {('bossroboshrimp', 2673): 1968, ('bossroboshrimpsegment', 2672): 1969, ('bossroboshrimptail', 2675): 1970,
                ('bossroboshrimplightning', 2674): 1971, ('bossroboshrimpblocker', 2691): 1972,
                ('minibosstrapdoor', 2747): 1973, ('enemywallpusher', 2748): 1974,
                ('bossrobosquid', 2765): 1975, ('bossrobosquidglass', 2764): 1976, ('bossrobosquidjet', 2766): 1977,
                ('bossrobosquidleftarm', 2762): 1978, ('bossrobosquidrightarm', 2763): 1979,
                ('bosssyntaxspider', 2755): 1980, ('bosssyntaxspiderbody', 2753): 1981, ('bosssyntaxspiderlavacannon', 2752): 1982,
                ('bosssyntaxspiderarm', 2754): 1983, ('bosssyntaxspiderlegfront', 2756): 1984, ('bosssyntaxspiderlegback', 2751): 1985,
                ('bosssyntaxgrappleline', 2770): 1987, ('bosslavasplash1', 2758): 1988, ('bosslavasplash2', 2759): 1988,
                ('bosssyntaxsquidback', 2887): 1990, ('bosssyntaxsquidshoulderright', 2888): 1990, ('bosssyntaxsquidshoulderleft', 2889): 1990,
                ('bosssyntaxsquidhead', 2890): 1990, ('bosssyntaxsquidbody', 2893): 1990, ('bosssyntaxsquidclawright', 2894): 1990,
                ('bosssyntaxsquidclawleft', 2895): 1990, ('bosssyntaxsquidarmleft', 2891): 1991, ('bosssyntaxsquidarmright', 2892): 1992,
                ('bosssyntaxsquidhitbox', 2902): 1993, ('bosslavalaser', 2896): 1994, ('bossspikesphere', 2897): 1995, ('bossspikesphere2', 2898): 1995,
                ('bosslavasplashleft1', 2899): 1996, ('bosslavasplashright1', 2901): 1996, ('bosslavasplashleft2', 2903): 1996,
                ('bosslavasplashright2', 2904): 1996, ('bosstbplatform', 2886): 1997, ('bosstbplatform2', 2908): 1998, ('bosswaypoint', 2905): 1999}
ENGINE_TYPES_ID.update(TBB_TYPES_ID)   # TB_BOSS
TBB_SUB = {'bosssyntaxsquidback_2887': 0, 'bosssyntaxsquidshoulderright_2888': 1, 'bosssyntaxsquidshoulderleft_2889': 2,
           'bosssyntaxsquidhead_2890': 3, 'bosssyntaxsquidbody_2893': 4, 'bosssyntaxsquidclawright_2894': 5, 'bosssyntaxsquidclawleft_2895': 6,
           'bossspikesphere_2897': 0, 'bossspikesphere2_2898': 1, 'bosslavasplashleft1_2899': 0, 'bosslavasplashright1_2901': 1,
           'bosslavasplashleft2_2903': 2, 'bosslavasplashright2_2904': 3, 'bosslavasplash1_2758': 0, 'bosslavasplash2_2759': 1}   # class -> member index (low byte of the lvobj anim field)
_SX = ('bosswaypoint',)   # (the rig's instances lie below the frame: tied to the on-map waypoints)
_SH = ('bossroboshrimp',)
_WP = ('enemywallpusher',)
_SQ = ('bossrobosquid',)
_SP = ('bosssyntaxspider',)
TBB_ENS = [(c, a, d, nm, None, w) for c, a, d, nm, w in (
    ('bossroboshrimp_2673', 0, '0', 'tbs_head', _SH), ('bossroboshrimp_2673', 8, '0', 'tbs_head8', _SH),
    ('bossroboshrimpsegment_2672', 0, '0', 'tbs_seg', _SH), ('bossroboshrimpsegment_2672', 8, '0', 'tbs_seg8', _SH),
    ('bossroboshrimpsegment_2672', 9, '0', 'tbs_seg9', _SH), ('bossroboshrimptail_2675', 0, '0', 'tbs_tail', _SH),
    ('bossroboshrimptail_2675', 9, '0', 'tbs_tail9', _SH), ('bossroboshrimplightning_2674', 0, '0', 'tbs_lt0', _SH),
    ('bossroboshrimplightning_2674', 0, '1', 'tbs_lt1', _SH),
    ('minibosstrapdoor_2747', 0, '0', 'tbw_trap', ('minibosstrapdoor',)), ('enemywallpusher_2748', 0, '0', 'tbw_wp', _WP),
    ('enemywallpusher_2748', 0, '16', 'tbw_wp16', _WP),
    ('bossrobosquid_2765', 0, '0', 'tbq_squid', _SQ), ('bossrobosquid_2765', 8, '0', 'tbq_squid8', _SQ),
    ('bossrobosquid_2765', 10, '0', 'tbq_squid10', _SQ), ('bossrobosquid_2765', 11, '0', 'tbq_squid11', _SQ),
    ('bossrobosquidglass_2764', 0, '0', 'tbq_glass', _SQ), ('bossrobosquidglass_2764', 0, '1', 'tbq_glass1', _SQ),
    ('bossrobosquidjet_2766', 0, '0', 'tbq_jet', _SQ), ('bossrobosquidjet_2766', 7, '0', 'tbq_jet7', _SQ),
    ('bossrobosquidleftarm_2762', 0, '0', 'tbq_arm', _SQ), ('enemyhomingbullet_2648', 0, '0', 'tbq_hb', _SQ),
    ('bosssyntaxspider_2755', 0, '0', 'tbp_head0', _SP),
    ('bosssyntaxspider_2755', 0, '16', 'tbp_head16', _SP),
    ('bosssyntaxspider_2755', 0, '24', 'tbp_head24', _SP),
    ('bosssyntaxspider_2755', 7, '24', 'tbp_head7', _SP),
    ('bosssyntaxspider_2755', 8, '0', 'tbp_head8_0', _SP),
    ('bosssyntaxspider_2755', 8, '16', 'tbp_head8_16', _SP),
    ('bosssyntaxspider_2755', 8, '24', 'tbp_head8_24', _SP),
    ('bosssyntaxspider_2755', 11, '0', 'tbp_head11', _SP),
    ('bosssyntaxspiderbody_2753', 0, '0', 'tbp_body0', _SP),
    ('bosssyntaxspiderbody_2753', 0, '16', 'tbp_body16', _SP),
    ('bosssyntaxspiderbody_2753', 0, '24', 'tbp_body24', _SP),
    ('bosssyntaxspiderbody_2753', 8, '0', 'tbp_body8_0', _SP),
    ('bosssyntaxspiderbody_2753', 8, '16', 'tbp_body8_16', _SP),
    ('bosssyntaxspiderbody_2753', 8, '24', 'tbp_body8_24', _SP),
    ('bosssyntaxspiderlavacannon_2752', 0, '0', 'tbp_can0', _SP),
    ('bosssyntaxspiderlavacannon_2752', 0, '16', 'tbp_can16', _SP),
    ('bosssyntaxspiderlavacannon_2752', 0, '24', 'tbp_can24', _SP),
    ('bosssyntaxspiderlavacannon_2752', 0, '1', 'tbp_can1', _SP),
    ('bosssyntaxspiderlavacannon_2752', 0, '17', 'tbp_can17', _SP),
    ('bosssyntaxspiderlavacannon_2752', 7, '0', 'tbp_can7_0', _SP),
    ('bosssyntaxspiderlavacannon_2752', 7, '16', 'tbp_can7_16', _SP),
    ('bosssyntaxspiderarm_2754', 0, '0', 'tbp_arm0', _SP),
    ('bosssyntaxspiderarm_2754', 0, '16', 'tbp_arm16', _SP),
    ('bosssyntaxspiderlegfront_2756', 0, '0', 'tbp_legf0', _SP),
    ('bosssyntaxspiderlegfront_2756', 0, '16', 'tbp_legf16', _SP),
    ('bosssyntaxspiderlegback_2751', 0, '0', 'tbp_legb0', _SP),
    ('bosssyntaxspiderlegback_2751', 0, '16', 'tbp_legb16', _SP),
    ('bosssyntaxgrappleline_2770', 0, '0', 'tbp_line', _SP),
    ('bosslavasplash1_2758', 0, '0', 'tbp_spl1', _SP),
    ('bosslavasplash2_2759', 0, '0', 'tbp_spl2', _SP),
    ('hazardlavafall2_2757', 0, '0', 'tbp_lf2', _SP),
    ('bossspikelauncher_2769', 0, '0', 'tbp_launch', _SP),
    ('bossspikelauncher_2769', 7, '0', 'tbp_launch7', _SP),
    ('bossspikelauncher_2769', 8, '0', 'tbp_launch8', _SP),
    ('bosssyntaxsquidback_2887', 0, '0', 'tbx_back', _SX), ('bosssyntaxsquidshoulderright_2888', 0, '0', 'tbx_shr', _SX),
    ('bosssyntaxsquidshoulderright_2888', 7, '0', 'tbx_shr7', _SX), ('bosssyntaxsquidshoulderleft_2889', 0, '0', 'tbx_shl', _SX),
    ('bosssyntaxsquidshoulderleft_2889', 7, '0', 'tbx_shl7', _SX), ('bosssyntaxsquidhead_2890', 0, '0', 'tbx_head', _SX),
    ('bosssyntaxsquidbody_2893', 0, '0', 'tbx_body', _SX), ('bosssyntaxsquidbody_2893', 0, '8', 'tbx_body8', _SX),
    ('bosssyntaxsquidclawright_2894', 0, '0', 'tbx_clawr', _SX), ('bosssyntaxsquidclawleft_2895', 0, '0', 'tbx_clawl', _SX),
    ('bosssyntaxsquidarmleft_2891', 0, '0', 'tbx_arml', _SX), ('bosssyntaxsquidarmright_2892', 0, '0', 'tbx_armr', _SX),
    ('bosssyntaxsquidhitbox_2902', 0, '0', 'tbx_hit', _SX), ('bosssyntaxsquidhitbox_2902', 0, '8', 'tbx_hit8', _SX),
    ('bosslavalaser_2896', 0, '0', 'tbx_laser', _SX), ('bossspikesphere_2897', 0, '0', 'tbx_sph', _SX),
    ('bossspikesphere2_2898', 0, '0', 'tbx_sph2', _SX), ('bosslavasplashleft1_2899', 0, '0', 'tbx_spl1', _SX),
    ('bosslavasplashleft2_2903', 0, '0', 'tbx_spl2', _SX), ('bosslavasplashmain_2900', 4, '0', 'tbx_splash', _SX),
    ('bosstbplatform_2886', 0, '0', 'tbx_plat', ('bosstbplatform', 'bosstbplatform2')))]
TBB_ENS_WITH = {'effectsexplosion06_435': _SH + _WP + _SQ + _SP, 'enemyhomingbullet_2648': _SQ, 'effectsexplosion02_368': _SQ + _SP, 'bosslavasplash1_2758': _SP, 'bosslavasplash2_2759': _SP, 'bossspikelauncher_2769': _SP, 'bosssyntaxgrappleline_2770': _SP, 'bosssyntaxspider_2755': _SP, 'bosssyntaxspiderarm_2754': _SP, 'bosssyntaxspiderbody_2753': _SP, 'bosssyntaxspiderlavacannon_2752': _SP, 'bosssyntaxspiderlegback_2751': _SP, 'bosssyntaxspiderlegfront_2756': _SP, 'hazardlavafall2_2757': _SP, 'effectshade_436': _SH, 'effectsdebris_427': _SH + _WP + _SQ,
    'bossroboshrimpsegment_2672': _SH, 'bossroboshrimptail_2675': _SH, 'bossroboshrimplightning_2674': _SH,   # (off-map templates)
    'effectswhiteburst_31': _SX, 'bosslavalaser_2896': _SX, 'bosslavasplashmain_2900': _SX, 'bosstbplatform_2886': ('bosstbplatform2',),
    **{c: _SX for c in ('bosssyntaxsquidback_2887', 'bosssyntaxsquidshoulderright_2888', 'bosssyntaxsquidshoulderleft_2889', 'bosssyntaxsquidhead_2890',
       'bosssyntaxsquidbody_2893', 'bosssyntaxsquidclawright_2894', 'bosssyntaxsquidclawleft_2895', 'bosssyntaxsquidarmleft_2891',
       'bosssyntaxsquidarmright_2892', 'bosssyntaxsquidhitbox_2902', 'bossspikesphere_2897', 'bossspikesphere2_2898',
       'bosslavasplashleft1_2899', 'bosslavasplashleft2_2903')}}
ENS += TBB_ENS   # TB_BOSS
for _k, _v in TBB_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # TB_BOSS
for _e in TBB_ENS: ROW_FRAMES[_e[3]] = TB_FRAMES   # TB_BOSS rows only in Thermal Base

# ==== end Thermal Base

# ==== Pangu Lagoon (session 23; json 51-56 = pl1-pl6; types 2048-2303, one block per module like Thermal Base)
# ---- PL_FLOW (pl_flow.c, 2048-2079; build/re/spec_pl_flow.md)
# The loop pieces of PL1-PL3 (RE 12747-12750 = the shared 3031 / 3032 on Q10 / Q11 by MV.P, + group 0x21a3a): PLLayer01_2975 /
# PLLayer02_2976 are solid on loop layer 0, PLLayer11_2973 / PLLayer12_2974 on layer 1 (visible Actives, pixel masks; ctors
# initialize_active, no set_visible(0)). One type per class (the registry's spawn sprite is per type): OD_SOLID | OD_LAYER0 / 1,
# gimmicks.c layers_refresh / layers_update drive them like every loop set. The rows come with on-map placements only.
# PLWiiUSign_2995 (PL1) is shown only when the global W = 1 (12845 / PL1 start inline; RV gv32 = 0 in every Linux trace): not exported.
PLF_TYPES_ID = {('pllayer01', 2975): 2048, ('pllayer02', 2976): 2049, ('pllayer11', 2973): 2050, ('pllayer12', 2974): 2051}
PLF_ENS = [('pllayer01_2975', 0, '0', 'pll01'), ('pllayer02_2976', 0, '0', 'pll02'), ('pllayer11_2973', 0, '0', 'pll11'),
           ('pllayer12_2974', 0, '0', 'pll12')]
ENGINE_TYPES_ID.update(PLF_TYPES_ID); ENS += PLF_ENS   # PL_FLOW
for _e in PLF_ENS: ROW_FRAMES[_e[3]] = PL_FRAMES       # PL_FLOW rows only in Pangu Lagoon
# no water: every act's start writes ML.A 1, ML.Y 0, ML.B 2000 but handle_frame_51..56 hold none of the water events 4150-4170 (RE,
# as Trap Hideout): not in WATER_FRAMES

# ---- PL_ENEMIES (pl_enemy.c, 2080-2143; build/re/spec_pl_enemies.md). Placed classes 2080-2111, runtime children 2112-2143 (pl.h
# OT_PL_*). Rows 'plX_name' ('_l' = direction 16; exact-mirror left rows are left out: _jce_mirror_rows). EnemyCacteon_1013 is the
# engine's cacteon (same class and handle as DV). The lvobj anim word carries the instance layer << 8 | 0x8000 json layer 1 of a
# mapped frame (the wall stingers' back swarm). Stinger anim 11 = anim 1's images (RE ctor): one row.
PLE_TYPES_ID = {('enemystinger', 2933): 2080, ('enemystingernest', 2937): 2081, ('enemysunflora', 2940): 2082, ('enemysleepgas', 2941): 2083,
    ('enemyterreon', 3006): 2084, ('enemyterreonleftleg', 3004): 2085, ('enemyterreonrightleg', 3005): 2086,
    ('enemyterreonleftshoulder', 3002): 2087, ('enemyterreonrightshoulder', 3003): 2088, ('effectterreonwarning', 3008): 2089,
    ('enemyobelisk', 3047): 2090, ('enemywallstinger', 3056): 2091, ('enemywallstingerback', 3058): 2092,
    ('wallstingerlimittop', 3060): 2093, ('wallstingerlimitbottom', 3061): 2094}
PLE_ENS = [(c, a, d, nm + ('' if d == '0' else '_l')) for c, a, nm in (
    ('enemystinger_2933', 0, 'pls_idle'), ('enemystinger_2933', 1, 'pls_fly'), ('enemystinger_2933', 8, 'pls_dead'),
    ('enemystinger_2933', 9, 'pls_att'), ('enemysunflora_2940', 10, 'plf_shoot'), ('enemysunflora_2940', 11, 'plf_grow'),
    ('enemyobelisk_3047', 0, 'plo_idle'), ('enemyobelisk_3047', 7, 'plo_fire'), ('enemyobelisk_3047', 8, 'plo_dead'),
    ('enemyobelisk_3047', 10, 'plo_close'), ('enemyobelisk_3047', 11, 'plo_open'), ('enemyobelisklaser_3048', 0, 'plo_laser'),
    ('enemywallstingerback_3058', 0, 'plw_back')) for d in ('0', '16')] + [
    ('enemystingernest_2937', 0, '0', 'pln_nest'), ('enemystingernest_2937', 8, '0', 'pln_dead'),
    ('enemysunflora_2940', 0, '0', 'plf_turn'), ('enemysleepgas_2941', 0, '0', 'plz_gas'), ('effectsconfetti_2943', 0, '0', 'plf_conf'),
    ('enemyterreon_3006', 0, '0', 'plt_body'), ('enemyterreon_3006', 5, '0', 'plt_blink'), ('enemyterreon_3006', 8, '0', 'plt_dead'),
    ('enemyterreonleftleg_3004', 0, '16', 'plt_ll'), ('enemyterreonrightleg_3005', 0, '16', 'plt_rl'),   # (anim 8's pale frames: drawn additive,
    ('enemyterreonleftshoulder_3002', 0, '0', 'plt_ls'), ('enemyterreonrightshoulder_3003', 0, '0', 'plt_rs'),   # the debris with these rows: VRAM)
    ('effectterreonwarning_3008', 0, '0', 'plt_warn'),
    ('enemywallstinger_3056', 0, '0', 'plw_fly'), ('enemywallstingerspawn_3057', 0, '0', 'plw_spawn')]
_PLE_ALL = ('enemystinger', 'enemystingernest', 'enemysunflora', 'enemyterreon', 'enemyobelisk', 'enemywallstinger')
PLE_ENS_WITH = {'enemystinger_2933': ('enemystingernest', 'enemywallstinger'), 'enemysleepgas_2941': ('enemysunflora',),
    'effectsconfetti_2943': ('enemysunflora',), 'enemyobelisklaser_3048': ('enemyobelisk',),
    'enemywallstingerspawn_3057': ('enemywallstinger',), 'hazardshockwave_1576': ('enemyterreon',),
    'objectkeystone_429': ('enemystingernest', 'enemyterreon', 'enemyobelisk'), 'effectsdebris_427': ('enemystingernest',),
    'effectsexplosion06_435': ('enemystingernest', 'enemyterreon', 'enemyobelisk')}
ENGINE_TYPES_ID.update(PLE_TYPES_ID); ENS += PLE_ENS   # PL_ENEMIES
for _k, _v in PLE_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # PL_ENEMIES
try:
    PLE_MIRRORED = _jce_mirror_rows(PLE_ENS)
except FileNotFoundError:
    PLE_MIRRORED = set()
for _nm in PLE_MIRRORED: ROW_FRAMES[_nm] = set()   # PL_ENEMIES

# ---- PL_GIM (pl_gim.c, 2144-2223; build/re/spec_pl_gimmicks.md). Families share a type; PLG_SUB gives the member index (lvobj
# anim bits 0-7, the instance layer in bits 8-14, as TB_GIM). Types mirror src/game/pl.h OT_PLG_*. BreakablePLRock_3012 is Fortune
# Night's breakable wall (fn_gima.c OT_FN_BWALL = 577, member 1: the same events 12899-12905 = 7211 / 7213-7217, RE), and
# GimmickSpinball_1173 runs Pangu Lagoon's own orbit (13040 / 13118 / 13225, not RM's 5684-5690): OT_PLG_SPINBALL in PL frames only.
PLG_TYPES_ID = {('objectvanishingblock', 3035): 2144, ('objecttimegate', 2915): 2145, ('objecttimegatepiece', 2916): 2146,
    ('gimmickbowstring', 2997): 2147, ('gimmickbowarrow', 2998): 2148, ('gimmickbowarrowplatform', 2999): 2149,
    ('objectpinwheel', 2992): 2150, ('objectcyclonetop', 2969): 2151,
    ('gimmickbreakoutwallleft', 3053): 2152, ('gimmickbreakoutwallright', 3049): 2152, ('gimmickbreakoutball', 3052): 2153,
    ('gimmickbreakoutplatform', 3050): 2154, ('gimmickbreakoutplatformmask', 3051): 2155,
    ('levellagoonplatform1', 2994): 2156, ('levellagoonplatform2', 3000): 2157,
    ('hazardgiantsword', 3067): 2158, ('hazardgiantswordmask', 3068): 2159,
    ('gimmickspinballchain', 3042): 2161, ('gimmickspinballchain2', 3043): 2161, ('gimmickspinballchain3', 3044): 2161,
    ('gimmickspinballchain4', 3045): 2161, ('gimmickhangingplswitch', 3023): 2162,
    ('objectstonedoor1', 3015): 2163, ('objectstonedoor2', 3018): 2163, ('objectstonedoor3', 3016): 2163, ('objectstonedoor4', 3017): 2163,
    ('active', 2927): 2164, ('breakableplrock', 3012): 577}
PLG_SUB = {'gimmickbreakoutwallright_3049': 1, 'gimmickspinballchain2_3043': 1, 'gimmickspinballchain3_3044': 2,
    'gimmickspinballchain4_3045': 3, 'objectstonedoor2_3018': 1, 'objectstonedoor3_3016': 2, 'objectstonedoor4_3017': 3}
FNA_SUB['breakableplrock_3012'] = 1                  # fn_gima.c: the PL rock member (sprite, pieces, no 7212 pass-through)
PLG_SPINBALL = (272, 2160)                           # OT_SPINBALL -> OT_PLG_SPINBALL in PL_FRAMES (the remap in main)
PLG_ENS = [(c, a, d, nm) for c, a, d, nm in (
    ('objectvanishingblock_3035', 0, '0', 'plg_vblock'),
    ('objecttimegatepiece_2916', 0, '0', 'plg_tpiece'), ('objecttimegatepiece_2916', 7, '0', 'plg_tpiece7'),
    ('gimmickbowstring_2997', 0, '0', 'plg_string'), ('gimmickbowarrow_2998', 0, '0', 'plg_arrow'),
    ('gimmickbowarrowplatform_2999', 0, '0', 'plg_arrowplat'), ('objectpinwheel_2992', 0, '0', 'plg_pinwheel'),
    ('objectcyclonetop_2969', 0, '0', 'plg_ctop0'), ('objectcyclonetop_2969', 0, '1', 'plg_ctop1'), ('objectcyclonetop_2969', 0, '2', 'plg_ctop2'),
    ('effectswhiteburst_31', 4, '0', 'plg_burst'),
    ('gimmickbreakoutball_3052', 0, '0', 'plg_bball'), ('gimmickbreakoutball_3052', 7, '0', 'plg_bball7'),
    ('gimmickbreakoutplatform_3050', 0, '0', 'plg_bplat'), ('gimmickbreakoutplatform_3050', 7, '0', 'plg_bplat7'),
    ('gimmickbreakoutplatformmask_3051', 0, '0', 'plg_bmask'),
    ('effectrebound_3054', 0, '0', 'plg_reb0'), ('effectrebound2_3055', 0, '0', 'plg_reb1'),
    ('levellagoonplatform1_2994', 0, '0', 'plg_lplat'),
    ('hazardgiantsword_3067', 0, '0', 'plg_sword'), ('hazardgiantswordmask_3068', 0, '0', 'plg_swmask'),
    ('hazardgiantswordmask_3068', 7, '0', 'plg_swmask7'),
    ('gimmickspinballchain_3042', 0, '0', 'plg_chain'), ('gimmickhangingplswitch_3023', 0, '0', 'plg_plswitch'),
    ('objectstonedoor1_3015', 0, '0', 'plg_sdoor0'), ('objectstonedoor2_3018', 0, '0', 'plg_sdoor1'),
    ('objectstonedoor3_3016', 0, '0', 'plg_sdoor2'), ('objectstonedoor4_3017', 0, '0', 'plg_sdoor3'),
    ('active_2927', 0, '0', 'plg_active'),
    ('breakableplrock_3012', 0, '0', 'plrock'), ('breakableplrockpiece_3013', 0, '0', 'plrockpiece'))]
PLG_ENS_WITH = {'gimmickbowarrowplatform_2999': ('gimmickbowarrow',), 'effectswhiteburst_31': ('objectcyclonetop',),
    'effectrebound_3054': ('gimmickbreakoutball',), 'effectrebound2_3055': ('gimmickbreakoutball',),
    'levellagoonplatform1_2994': ('levellagoonplatform2',), 'gimmickspinballchain_3042': ('gimmickspinballchain2', 'gimmickspinballchain3', 'gimmickspinballchain4'),
    'breakableplrockpiece_3013': ('breakableplrock',)}
ENGINE_TYPES_ID.update(PLG_TYPES_ID); ENS += PLG_ENS   # PL_GIM
for _e in PLG_ENS: ROW_FRAMES[_e[3]] = PL_FRAMES     # PL_GIM rows only in Pangu Lagoon
for _k, _v in PLG_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # PL_GIM
# the poses the gimmicks force on Lilac's skin (RE 12971 / 12875 / 13157: the arrow ride, skin animation 40 direction 6; 13002 /
# 13234: the stone door, direction 24): 'a40_d6' / 'a40_d24' rows in the acts that place a bow / a door (player_pose)
for _n, _dirs in ((52, ('6',)), (53, ('6', '24')), (54, ('6',)), (55, ('6',)), (56, ('24',))):
    POSE_DIRS[_n] = {**POSE_DIRS.get(_n, {}), 40: tuple(sorted(set(POSE_DIRS.get(_n, {}).get(40, ())) | set(_dirs)))}

# ---- PL_BOSS (pl_boss.c, 2224-2303; build/re/spec_pl_bosses.md): the Holo-Dragon of PL6. Types mirror src/game/pl.h (OT_PLB_*).
# The placed rig (json 56: neck controller, head parts, 93 body scales + their hit spots) keeps its json order (the draw order); the
# off-map templates (claws, eye2, laser, NPC Pangu + arms) are created by pl_boss.c (the importer drops instances outside the frame).
# Rows: direction 0 only (every direction-1 image of these classes is the pixel mirror of direction 0: drawn flipped, checked
# image by image) and EffectWarning_1528 (the laser's edge warning). The fight's effects (explosion02, whiteburst) are the objects
# bank's shared 'explosion02' / 'wburst' rows.
PLB_TYPES_ID = {('bossholodragonneck', 3086): 2224, ('bossholodragonbreath', 3078): 2225, ('bossholodragontoptooth', 3080): 2226,
                ('bossholodragonbottomtooth', 3081): 2227, ('bossholodragonscale', 3082): 2228, ('bossholodragonscale2', 3083): 2229,
                ('bossholodragonscale3', 3084): 2230, ('bossholodragonscale4', 3085): 2231, ('bossholodragonear', 3087): 2232,
                ('bossholodragoneye', 3088): 2233, ('bossholodragonpupil', 3089): 2234, ('bossholodragonbrow', 3090): 2235,
                ('bossholodragonhitspot1', 3094): 2236, ('bossholodragonhitspot2', 3095): 2237, ('bossholodragonhitspot3', 3096): 2238}
ENGINE_TYPES_ID.update(PLB_TYPES_ID)
_HD = ('bossholodragonneck',)
PLB_ENS = [(c, a, d, nm, None, _HD) for c, a, d, nm in (
    ('bossholodragonbreath_3078', 0, '0', 'plb_breath'), ('bossholodragontoptooth_3080', 0, '0', 'plb_ttooth'),
    ('bossholodragonbottomtooth_3081', 0, '0', 'plb_btooth'), ('bossholodragonscale_3082', 0, '0', 'plb_hscale'),
    ('bossholodragonscale2_3083', 0, '0', 'plb_s2'), ('bossholodragonscale3_3084', 0, '0', 'plb_s3'),
    ('bossholodragonscale4_3085', 0, '0', 'plb_s4'), ('bossholodragonear_3087', 0, '0', 'plb_ear'),
    ('bossholodragoneye_3088', 0, '0', 'plb_eye'), ('bossholodragonpupil_3089', 0, '0', 'plb_pupil'),
    ('bossholodragonbrow_3090', 0, '0', 'plb_brow'), ('bossholodragonhitspot1_3094', 0, '0', 'plb_hit1'),
    ('bossholodragonhitspot2_3095', 0, '0', 'plb_hit2'), ('bossholodragonhitspot3_3096', 0, '0', 'plb_hit3'),
    ('bossholodragonclaw_3093', 0, '0', 'plb_claw'), ('bossholodragoneye2_3097', 0, '0', 'plb_eye2'),
    ('bossholodragonlaser_3079', 0, '0', 'plb_laser'), ('bossholodragonflame_3092', 0, '0', 'plb_flame'),
    ('npcpangu_3098', 0, '0', 'plb_pangu'), ('npcpanguleftarm_3101', 0, '0', 'plb_larm'), ('npcpangurightarm_3102', 0, '0', 'plb_rarm'),
    ('effectwarning_1528', 0, '0', 'plb_warn'))]
ENS += PLB_ENS   # PL_BOSS
PLB_ENS_WITH = {c: _HD for c in ('bossholodragonflame_3092', 'bossholodragonclaw_3093', 'bossholodragoneye2_3097',
                                 'bossholodragonlaser_3079', 'npcpangu_3098', 'npcpanguleftarm_3101', 'npcpangurightarm_3102')}
for _k, _v in PLB_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # PL_BOSS (the off-map templates and runtime effects)
for _e in PLB_ENS: ROW_FRAMES[_e[3]] = {56}   # PL_BOSS rows only in PL6
# 13347: Lilac's skin animation 41 direction 24 during the dragon's entrance (player_pose)
POSE_DIRS[56] = {**POSE_DIRS.get(56, {}), 41: ('24',)}

# ==== end Pangu Lagoon

# ==== Battle Glacier (session 24; json 60-65 = bg1-bg6; types 2304-2559)
# ---- BG_FLOW (bg_flow.c, 2304-2335; build/re/spec_bg_flow.md)
# The loop pieces of BG4 / BG5 (RE on_frame_63 / 64_init qualifier tables: BGLoop1_3340 in Q7 + Q11 = solid on loop layer 1,
# BGLoop2_3343 / BGLoop3_3344 in Q7 + Q10 = solid on loop layer 0; 14345 / 14346 / 14348 / 14349 = the shared flag rule 3031 / 3032 by
# MV.P). Visible Actives (ctors: initialize_active, no set_visible(0)); opaque 64 x 64 / 128 x 64 images. One type per class (the
# registry's spawn sprite is per type): OD_SOLID | OD_LAYER1 / OD_LAYER0, gimmicks.c layers_refresh / layers_update drive them.
# The off-map templates (one of each per frame) are removed by bg_flow_spawn.
BGF_TYPES_ID = {('bgloop1', 3340): 2304, ('bgloop2', 3343): 2305, ('bgloop3', 3344): 2306}
BGF_ENS = [('bgloop1_3340', 0, '0', 'bgl1'), ('bgloop2_3343', 0, '0', 'bgl2'), ('bgloop3_3344', 0, '0', 'bgl3')]
ENGINE_TYPES_ID.update(BGF_TYPES_ID); ENS += BGF_ENS   # BG_FLOW
for _e in BGF_ENS: ROW_FRAMES[_e[3]] = {63, 64}         # BG_FLOW rows only in BG4 / BG5 (the only acts that place them)
# no water: ML.A 1 / ML.B 2000 (BG1-BG3) / 3000 (BG4-BG6) are written by the start events but handle_frame_60..65 hold none of the
# water events 4150-4170 (RE; RV ml_B with no air HUD): not in WATER_FRAMES

# ---- BG_ENEMIES (bg_enemy.c, 2336-2399; build/re/spec_bg_enemies.md). New classes: EnemyDrillSat_3291 (bg3-bg5, Q20) and the
# reusable EnemyEliteSpawner_3361 (bg5; FD maps its spawners to the same type) + the runtime EliteSpawnerWave_3363, the spawner's
# afterimage (EffectsSpadeImage_456 direction 12 = the pod's image) and the drill's dirt (EffectsDebris_427 anim 5).
# Reused (no BG copy): Thermal Base's saucers / saucer limiters / lava squids (tb_enemy.c, RE: BG1-BG5 call 12400-12424 and
# BG3 / BG5 11969-11991, the TB events), Jade Creek's Elite (jc_enemy.c: BG4 / BG5 call the JC5 Elite events 10949-11038; 14397
# / 14655 = 11029 with the act's Q20 lists): BG frames take the enemies module's Elite types 1326 / 1327 (jee_* rows), not the
# JC5 gunship module's 1498 / 1499.
BG_E_FRAMES = {60, 61, 62, 63, 64, 65}
BGE_TYPES_ID = {('enemydrillsat', 3291): 2336, ('enemyelitespawner', 3361): 2337}
BGE_ENS = [('enemydrillsat_3291', 0, '0', 'bgd_idle'), ('enemydrillsat_3291', 7, '0', 'bgd_fly'), ('enemydrillsat_3291', 11, '0', 'bgd_rise'),
    ('effectsdebris_427', 5, '0', 'bgd_deb', None, ('enemydrillsat',)),
    ('enemyelitespawner_3361', 0, '0', 'bgs_pod'), ('enemyelitespawner_3361', 8, '0', 'bgs_open'),
    ('enemyelitespawnerwave_3363', 0, '0', 'bgs_wave', None, ('enemyelitespawner',))]
BGE_ENS_WITH = {'effectsdust_437': ('enemydrillsat',), 'effectsdebris_427': ('enemydrillsat',), 'enemyelitespawnerwave_3363': ('enemyelitespawner',),
    # the Elite and its runtime parts come with a spawner too (14661 creates the mask and the sprite)
    'enemyelite_2529': ('enemyelitespawner',), 'enemyelitemask_2528': ('enemyelitespawner',),
    'enemyelitechargeglow_2527': ('enemyelite', 'enemyelitespawner'), 'enemytrooperlaser_2530': ('enemyelite', 'enemyelitespawner')}
ENGINE_TYPES_ID.update(BGE_TYPES_ID); ENS += BGE_ENS   # BG_ENEMIES
for _k, _v in BGE_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # BG_ENEMIES
for _e in BGE_ENS: ROW_FRAMES[_e[3]] = BG_E_FRAMES
for _e in JCE_ENS:                                   # the JC Elite rows (jee_*) in Battle Glacier (exact-mirror left rows stay out)
    if _e[3].startswith('jee_') and _e[3] not in JCE_MIRRORED: ROW_FRAMES[_e[3]] = ROW_FRAMES.get(_e[3], set()) | BG_E_FRAMES
for _e in TBE_ENS:                                   # TB saucer / saucer debris / lava squid rows in Battle Glacier
    if _e[3].startswith(('tba_', 'tbq_')) and _e[3] not in TBE_MIRRORED: ROW_FRAMES[_e[3]] = ROW_FRAMES.get(_e[3], set()) | BG_E_FRAMES
# the ObjectExplosiveBlock_2738 walls the drill sats break (tb_gim.c type 1913, BG3-BG6): their TB row, else they have no image and no
# collision in BG (RE 14306; RV drill3_wall)
ROW_FRAMES['tbg_xblock'] = ROW_FRAMES.get('tbg_xblock', set()) | BG_E_FRAMES
FRAME_TYPE_REMAP = globals().get('FRAME_TYPE_REMAP', {})   # (json frame, type) -> type, applied after the type lookup
for _n in BG_E_FRAMES: FRAME_TYPE_REMAP.update({(_n, 1498): 1326, (_n, 1499): 1327})   # BG Elites: jc_enemy.c's types

# ---- BG_GIM (bg_gim.c, 2400-2479; build/re/spec_bg_gimmicks.md). Families share a type; BGG_SUB gives the member index (lvobj
# anim bits 0-7, the instance layer in bits 8-14, as PL_GIM). Types mirror src/game/bg.h OT_BGG_*. Final Dreadnought places the
# same classes (HazardSpinMine_3331, GimmickAntigravSpawner_3323, ObjectSiren_3362, ObjectTransporter_3360): its block maps them to
# these types (one implementation; the rows below are not frame-restricted).
BGG_TYPES_ID = {('objectteleblockmask', 3255): 2400, ('gimmickspiralslidestart', 3268): 2402, ('gimmickspiralslideend', 3269): 2402,
    ('gimmicksnakedrill', 3273): 2403, ('gimmicksnowspire', 3249): 2404, ('gimmicksnowspiremask', 3251): 2405,
    ('bgsolarpanel', 3252): 2406, ('levelglacierplatform1', 3275): 2407, ('levelglacierplatform2', 3293): 2408,
    ('gimmickbgbar', 3283): 2409, ('gimmickbgbarmask', 3284): 2410, ('bgplatform1', 3313): 2411, ('levelcrankferris', 3317): 2412,
    ('levelcrankferrismask', 3316): 2413, ('levelcrankferrismask2', 3318): 2413, ('levelcrankferrismask3', 3319): 2413,
    ('levelcrankferrismask4', 3320): 2413, ('levelcrankferriscollisionstopper', 3324): 2414, ('gimmickantigravspawner', 3323): 2415,
    ('gimmickmagnetlauncher', 3326): 2417, ('gimmickmagnetlauncherleft', 3328): 2418, ('gimmickmagnetlauncherright', 3329): 2418,
    ('gimmickmagnetlauncherlight', 3330): 2419, ('hazardspinmine', 3331): 2420, ('objectswitchblock1', 3354): 2421,
    ('objectswitchblock2', 3355): 2421, ('gimmickteleswitch', 3356): 2422, ('gimmickteleswitchcrystal1', 3357): 2423,
    ('gimmickteleswitchcrystal2', 3358): 2423, ('objecttransporter', 3360): 2424, ('objectsiren', 3362): 2425}
BGG_SUB = {'gimmickspiralslideend_3269': 1, 'levelcrankferrismask2_3318': 1, 'levelcrankferrismask3_3319': 2, 'levelcrankferrismask4_3320': 3,
    'gimmickmagnetlauncherright_3329': 1, 'objectswitchblock2_3355': 1, 'gimmickteleswitchcrystal2_3358': 1}
BGG_ENS = [(c, a, d, nm) for c, a, d, nm in (
    ('objectteleblock_3254', 0, '0', 'bgg_tblock'),                                  # the runtime block (14175); also the mask's collision square
    ('gimmicksnakedrill_3273', 0, '0', 'bgg_snake'), ('gimmicksnakedrill_3273', 0, '1', 'bgg_snakeh'),
    ('gimmicksnowspire_3249', 0, '0', 'bgg_spire'),                                  # also the mask's collision (13546 = 13593 + 495 px)
    ('bgsolarpanel_3252', 0, '0', 'bgg_solar'), ('levelglacierplatform1_3275', 0, '0', 'bgg_gplat'),
    ('gimmickbgbar_3283', 0, '0', 'bgg_bar'), ('gimmickbgbarmask_3284', 0, '0', 'bgg_barmask'),
    ('bgplatform1_3313', 0, '0', 'bgg_bgplat'), ('levelcrankferrismask_3316', 0, '0', 'bgg_ferris'),
    ('levelcrankferrismask2_3318', 0, '0', 'bgg_ferris2'),                           # masks 2-4 share image 13665
    ('gimmickantigravspawner_3323', 0, '0', 'bgg_agspawn'), ('gimmickantigrav_3322', 0, '0', 'bgg_ag'),
    ('gimmickmagnetlauncher_3326', 0, '0', 'bgg_magnet'), ('gimmickmagnetlauncher_3326', 0, '1', 'bgg_magnet1'),   # dir 31 = mirror of 1
    ('gimmickmagnetlauncherlight_3330', 0, '0', 'bgg_mlight'), ('effectsspringbeam_540', 4, '0', 'bgg_sbeam'),
    ('hazardspinmine_3331', 0, '0', 'bgg_mine'),
    ('objectswitchblock1_3354', 0, '0', 'bgg_swb1'), ('objectswitchblock1_3354', 0, '1', 'bgg_swb1o'),
    ('objectswitchblock2_3355', 0, '0', 'bgg_swb2'), ('objectswitchblock2_3355', 0, '1', 'bgg_swb2o'),
    ('gimmickteleswitch_3356', 0, '0', 'bgg_tsw0'), ('gimmickteleswitch_3356', 0, '1', 'bgg_tsw1'),
    ('gimmickteleswitchcrystal1_3357', 0, '0', 'bgg_tsc0'), ('gimmickteleswitchcrystal1_3357', 0, '1', 'bgg_tsc1'),
    ('objecttransporter_3360', 0, '0', 'bgg_transp'), ('objectsiren_3362', 0, '0', 'bgg_siren'))]
BGG_ENS_WITH = {'objectteleblock_3254': ('objectteleblockmask',), 'gimmicksnowspire_3249': ('gimmicksnowspiremask',),
    'levelglacierplatform1_3275': ('levelglacierplatform2',), 'levelcrankferrismask_3316': ('levelcrankferris',),
    'gimmickantigrav_3322': ('gimmickantigravspawner',), 'effectsspringbeam_540': ('gimmickmagnetlauncher',),
    'gimmickteleswitchcrystal1_3357': ('gimmickteleswitchcrystal2',), 'enemyrock_1194': ('gimmicksnakedrill',)}
ROW_FRAMES['bgg_sbeam'] = {63, 64}                    # the magnet release beam (14390): BG4 / BG5 (the class's other rows stay as they were)
ENS_FRAMES['enemyrock_1194'] = ENS_FRAMES['enemyrock_1194'] | {60, 61}   # 14154 / 14159: the snake drills throw EnemyRock_1194 (BG1 / BG2)
ENGINE_TYPES_ID.update(BGG_TYPES_ID); ENS += BGG_ENS   # BG_GIM
for _k, _v in BGG_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # BG_GIM

# ---- BG_BOSS (bg_boss.c, 2480-2559; build/re/spec_bg_bosses.md). Types mirror src/game/bg.h (OT_BGB_*); the lvobj anim field carries the
# instance layer << 8. Placed: the Sparky rig (BG4 boss, BG5 Milla's ride), the hatch doors (BG4 / BG6), the background Brevon mech
# (BG2-BG4, BG6) and BG4's claw warnings. The BG2 chopper is SB2's (sb_chopper.c): its classes are mapped in json 61 too.
# Rows: direction 0 only (the parts are drawn turned / flipped); the runtime mech parts are tied to the background mech and kept to BG4.
BGB_FRAMES = {60, 61, 62, 63, 64, 65}
BGB_TYPES_ID = {('bosssparky', 3336): 2480, ('bosssparkyleftarm', 3337): 2481, ('bosssparkyrightarm', 3338): 2482,
                ('bosssparkytail', 3335): 2483, ('bosshatchdoor', 3312): 2484, ('brevonmechbackground', 2444): 2485,
                ('effectbrevonmechwarning2', 2586): 2486}
ENGINE_TYPES_ID.update(BGB_TYPES_ID)
SBBOSS_FRAMES.add(61)                                         # BG2: BossChopper_2216 + parts (sb_chopper.c, the BG2 variant)
_SPK = ('bosssparky',)
BGB_ENS = [(c, a, d, nm, None, w) for c, a, d, nm, w in (
    ('bosssparky_3336', 0, '0', 'bgs_body', _SPK), ('bosssparky_3336', 7, '0', 'bgs_body7', _SPK), ('bosssparky_3336', 8, '0', 'bgs_body8', _SPK),
    ('bosssparky_3336', 11, '0', 'bgs_body11', _SPK),
    ('bosssparkyleftarm_3337', 0, '0', 'bgs_larm', _SPK), ('bosssparkyleftarm_3337', 7, '0', 'bgs_larm7', _SPK),
    ('bosssparkyrightarm_3338', 0, '0', 'bgs_rarm', _SPK), ('bosssparkyrightarm_3338', 7, '0', 'bgs_rarm7', _SPK),
    ('bosssparkytail_3335', 0, '0', 'bgs_tail', _SPK), ('bosssparkytail_3335', 7, '0', 'bgs_tail7', _SPK),
    ('bosssparkytail_3335', 8, '0', 'bgs_tail8', _SPK), ('bosssparkytail_3335', 11, '0', 'bgs_tail11', _SPK),
    ('bosssparkyplasmashot_2587', 0, '0', 'bgs_plasma', _SPK),
    ('bosshatchdoor_3312', 0, '0', 'bgb_hatch', ('bosshatchdoor',)))]
# Absolution (BG4, bg_boss_mech.c): its classes are off-map templates of json 63 only (the rows go with brevonmechtorso). Direction 0
# only: the direction-16 images are mirrors (drawn flipped); the front and back wings share one image (14579), so one row.
_MCH = ('brevonmechtorso',)
# VRAM (bg4_ens spills into a second atlas): the head keeps its closed and open images (frames 0, 3), the charge orb every other
# frame, the background mech two of its four wing frames, the plasma burst every other frame (the port steps them accordingly)
_BGM_SL = {'bgm_head': slice(0, 4, 3), 'bgm_charge': slice(0, 8, 2), 'bgm_bg': slice(0, 4, 2), 'bgm_burst': slice(1, 13, 2)}
_MBG = ('brevonmechbackground',)
BGB_ENS += [(c, a, d, nm, _BGM_SL.get(nm), _MCH) for c, a, d, nm in (
    ('brevonmechtorso_2539', 0, '0', 'bgm_torso'), ('brevonmechhead_2540', 0, '0', 'bgm_head'), ('brevonmechhead_2540', 10, '0', 'bgm_head10'),
    ('brevonmechweakspot_2581', 0, '0', 'bgm_weak0'), ('brevonmechweakspot_2581', 0, '1', 'bgm_weak1'),
    ('brevonmechweakspot_2581', 0, '2', 'bgm_weak2'), ('brevonmechweakspot_2581', 0, '3', 'bgm_weak3'),
    ('brevonmechturret_2585', 0, '0', 'bgm_turret'), ('brevonmechcharge_2584', 0, '0', 'bgm_charge'),
    ('brevonmechabdomen_2536', 0, '0', 'bgm_abd'), ('brevonmechaxle_2546', 0, '0', 'bgm_axle'),
    ('brevonmechbackarm_2534', 0, '0', 'bgm_barm'), ('brevonmechbackclaw_2533', 0, '0', 'bgm_bclaw'),
    ('brevonmechbackshoulder_2532', 0, '0', 'bgm_bsh'), ('brevonmechbackwing_2531', 0, '0', 'bgm_wing'),
    ('brevonmechfrontleg_2535', 0, '0', 'bgm_fleg'), ('brevonmechfrontleg2_2537', 0, '0', 'bgm_fleg2'),
    ('brevonmechfrontknee_2538', 0, '0', 'bgm_fknee'), ('brevonmechfrontfoot_2541', 0, '0', 'bgm_ffoot'),
    ('brevonmechfrontshoulder_2543', 0, '0', 'bgm_fsh'), ('brevonmechfrontarm_2544', 0, '0', 'bgm_farm'),
    ('brevonmechfrontclaw_2545', 0, '0', 'bgm_fclaw'),
    ('effectbrevonmechwarning2_2586', 0, '0', 'bgm_warn2'))]
# the background mech (BG2-BG4) and its lasers / warnings / burst (created at run time: tied to the background mech)
BGB_ENS += [(c, a, d, nm, _BGM_SL.get(nm), _MBG) for c, a, d, nm in (
    ('brevonmechbackground_2444', 0, '0', 'bgm_bg'), ('brevonmechlaser_2583', 0, '0', 'bgm_laser'),
    ('effectbrevonmechwarning_2582', 0, '0', 'bgm_warn'), ('effectplasmaburst_3091', 4, '0', 'bgm_burst'))]
BGB_ENS_WITH = {'effectsexplosion06_435': _SPK + _MCH, 'effectshade_436': _SPK, 'bosssparkyplasmashot_2587': _SPK + _MCH,
                'hazardshockwave_1576': _MCH, 'effectsdebris_427': _MCH, 'brevonmechlaser_2583': _MBG,
                'effectbrevonmechwarning_2582': _MBG, 'effectplasmaburst_3091': _MBG}
ENS += BGB_ENS   # BG_BOSS
for _k, _v in BGB_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # BG_BOSS
for _e in BGB_ENS: ROW_FRAMES[_e[3]] = BGB_FRAMES   # BG_BOSS rows only in Battle Glacier
for _e in BGB_ENS:
    if _e[5] == _MBG: ROW_FRAMES[_e[3]] = {61, 62, 63}   # BG6 places the background mech too (it stays hidden there)
ROW_FRAMES['bgm_burst'] = {61}                        # BG2's set piece only
ROW_FRAMES['bgs_body11'] = ROW_FRAMES['bgs_tail11'] = {64}   # Milla aboard (BG5's ride) only
# BG6 (bg_boss_dail.c): Prince Dail, the Shade Beast (head, crown placed; mouth, teeth, eyes, vortex, cards, lightning at run time)
BGB_TYPES_ID6 = {('bossdail', 3370): 2490, ('bossdailfall', 3383): 2491, ('bossdailspawnpoint', 3366): 2492,
                 ('bossshadebeast', 3368): 2493, ('bossshadebeastcrown', 3369): 2494}
ENGINE_TYPES_ID.update(BGB_TYPES_ID6)
_SB6 = ('bossshadebeast',)
BGB_ENS6 = [(c, a, d, nm, None, _SB6) for c, a, d, nm in (
    ('bossdail_3370', 0, '0', 'bgd_dail'), ('bossdail_3370', 7, '0', 'bgd_dail7'), ('bossdail_3370', 8, '0', 'bgd_dail8'),
    ('bossdail_3370', 10, '0', 'bgd_dail10'), ('bossdailfall_3383', 0, '0', 'bgd_fall'),
    ('bossshadebeast_3368', 0, '0', 'bgd_beast'), ('bossshadebeast_3368', 8, '0', 'bgd_beast8'),
    ('bossshadebeastcrown_3369', 0, '0', 'bgd_crown'), ('bossshadebeastcrown_3369', 8, '0', 'bgd_crown8'),
    ('bossshadebeastmouth_3373', 0, '0', 'bgd_mouth'), ('bossshadebeasttoptooth_3375', 0, '0', 'bgd_ttooth'),
    ('bossshadebeastbottomtooth_3376', 0, '0', 'bgd_btooth'), ('bossshadebeasteye_3365', 0, '0', 'bgd_eye'),
    ('bossshadebeasteye_3365', 7, '0', 'bgd_eye7'), ('bossvortex1_3374', 0, '0', 'bgd_vortex1'),
    ('bossvortex2_3378', 0, '0', 'bgd_vortex2'), ('bossvortexstar_3379', 0, '0', 'bgd_star'),
    ('bossdailcard_3371', 0, '0', 'bgd_card'), ('bossdailcardtrail_3372', 0, '0', 'bgd_trail'),
    ('bossdailcardeffect_3367', 0, '0', 'bgd_cardx'), ('bosslightning2_2132', 0, '0', 'bgd_bolt'),
    ('effectbossshade_3377', 0, '0', 'bgd_shade'))]
ENS += BGB_ENS6   # BG_BOSS
for _e in BGB_ENS6: ROW_FRAMES[_e[3]] = {65}
for _k in ('bossshadebeastmouth_3373', 'bossshadebeasttoptooth_3375', 'bossshadebeastbottomtooth_3376', 'bossshadebeasteye_3365',
           'bossvortex1_3374', 'bossvortex2_3378', 'bossvortexstar_3379', 'bossdailcard_3371', 'bossdailcardtrail_3372',
           'bossdailcardeffect_3367', 'bosslightning2_2132', 'effectbossshade_3377'):
    ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _SB6   # BG_BOSS (created at run time by the BG6 fight)

# ==== end Battle Glacier

# ==== Final Dreadnought (session 24; json 66-73 = fd1a..fd4b; types 2560-3071)
# ---- FD_FLOW (fd_flow.c, 2560-2623; build/re/spec_fd_flow.md)
# The loop-layer pieces of FD3 (fd3a / fd3b, json 70 / 71; RE on_frame_70_init qualifier tables: Q10 (+0x21908, solid on loop layer 0)
# = FDLoopLayer01 / 02 / 03 / 04 / 05, Q11 (+0x21920, layer 1) = FDLoopLayer11 / 12 / 13 / 14 / 15; the shared flag rule 3031 / 3032 by
# MV.P). Visible Actives (ctors: no set_visible(0)), opaque 64 x 64 / 128 x 128 images (02 / 12: a 4-frame conveyor animation). Each
# piece is placed on json layer 3 (the play layer) and most again on layer 4 (in front of the player): the lvobj anim holds the instance
# layer << 8 (fd_flow_spawn sets obj.layer; the off-map templates are removed there). One type per class (the spawn sprite is per type).
FDF_LOOPS = [('fdlooplayer01', 3643, 0), ('fdlooplayer02', 3644, 0), ('fdlooplayer03', 3646, 0), ('fdlooplayer04', 3663, 0), ('fdlooplayer05', 3664, 0),
             ('fdlooplayer11', 3647, 1), ('fdlooplayer12', 3648, 1), ('fdlooplayer13', 3649, 1), ('fdlooplayer14', 3662, 1), ('fdlooplayer15', 3665, 1)]
FDF_TYPES_ID = {(c, h): 2560 + k for k, (c, h, _) in enumerate(FDF_LOOPS)}
FDF_ENS = [('%s_%d' % (c, h), 0, '0', 'fdl%s' % c[11:]) for c, h, _ in FDF_LOOPS]
ENGINE_TYPES_ID.update(FDF_TYPES_ID); ENS += FDF_ENS   # FD_FLOW
for _e in FDF_ENS: ROW_FRAMES[_e[3]] = {70, 71}         # FD_FLOW rows only in fd3a / fd3b (the only acts that place them)
# no water: ML.A 1 / ML.B 2000 (4800 in fd2a / fd2b) are written by the start events, but only handle_frame_69 (fd2b) holds air events,
# its own vacuum rule (15790-15796: the air counter rises while the siren alarm is on, not ML.B): not in WATER_FRAMES

# ---- FD_ENEMIES (fd_enemy.c, 2624-2751; build/re/spec_fd_enemies.md). Reused classes (RE: the FD frames run the same events as
# their home stages, build/re/fde/f66..f73_calls): the EliteSpawner pods (bg_enemy.c 2337, BG5's 14657-14662 everywhere; payload by
# alterable C in FD1 15245-15249), the Elites (jc_enemy.c 1326 / 1327, the JC5 events 10949-11038; not the JC5 gunship module's
# 1498 / 1499), Thermal Base's tank bots (12425-12444, FD1-FD3) and seekers (11761-11782, FD3), Jade Creek's bubble orbiters
# (their GolemShot rows, FD3), Sky Battalion's DNA cannon warnings (9854-9858: EffectWarning_1528 = OT_SB_WARN in FD3 / FD4a).
FDE_FRAMES = FD_FRAMES
for _n in FDE_FRAMES: FRAME_TYPE_REMAP.update({(_n, 1498): 1326, (_n, 1499): 1327})   # FD Elites: jc_enemy.c's types
for _e in JCE_ENS:                                   # the Elite rows (exact-mirror left rows stay out)
    if _e[3].startswith('jee_') and _e[3] not in JCE_MIRRORED: ROW_FRAMES[_e[3]] = ROW_FRAMES.get(_e[3], set()) | FDE_FRAMES
for _e in BGE_ENS:                                   # the pod rows
    if _e[3].startswith('bgs_'): ROW_FRAMES[_e[3]] = ROW_FRAMES[_e[3]] | FDE_FRAMES
for _e in TBE_ENS:                                   # tank bots (fd1a..fd3b), seekers + their homing bullets (fd3a / fd3b)
    if _e[3].startswith(('tbt_', 'tbs_')) and _e[3] not in TBE_MIRRORED: ROW_FRAMES[_e[3]] = ROW_FRAMES.get(_e[3], set()) | {66, 67, 68, 69, 70, 71}
for _nm in ('jeo_shot', 'jeo_pop'): ROW_FRAMES[_nm] = ROW_FRAMES[_nm] | {70, 71}   # the bubble orbiters' shots (fd3a / fd3b)
ROW_FRAMES['sbwarn'] = ROW_FRAMES['sbwarn'] | {70, 71, 72}
SBE_WARN_FRAMES |= {70, 71, 72}                      # EffectWarning_1528 -> OT_SB_WARN (the DNA cannons' off-view warnings)
# New classes (src/game/fd.h OT_FDE_*): EnemyPounder_3423 (FD1 / FD3, Q20; its HazardShockwave_1576 is Relic Maze's 'shock' rows),
# the gatling gun EnemyGatlingGun_3669 (Q20) + Table_3666 / Right_3667 / Left_3668 / Barrel_3670 / Gun2_3671 (+ the runtime
# Debris_3673; its shots are the Elite's TrooperLaser_2530 'jee_laser'), EnemyBeastIris_3735 (FD4a, Q20), EnemySpiderBeamer_3755
# (FD4a / FD4b, Q20; its shots are the seeker's EnemyHomingBullet_2648 'tbs_bul'), EnemyEliteSpawnerSpawner_3453 (FD1a's 24 hidden
# drop points of the pod rain, 15287). Inert (no event references them, RE): EnemyCrane_3422, HazardBeamCannonHorizontal_3436,
# FDLaserCannonLeftLight_3434 (off-map / unused templates): not exported. Right and left share the image ('fdg_arm').
FDE_TYPES_ID = {('enemypounder', 3423): 2624, ('enemygatlingguntable', 3666): 2625, ('enemygatlinggunright', 3667): 2626,
    ('enemygatlinggunleft', 3668): 2627, ('enemygatlinggun', 3669): 2628, ('enemygatlinggunbarrel', 3670): 2629,
    ('enemygatlinggun2', 3671): 2630, ('enemybeastiris', 3735): 2631, ('enemyspiderbeamer', 3755): 2632,
    ('enemyelitespawnerspawner', 3453): 2633}
_FDE_SFX = {'0': '', '16': '_l'}
_FDE_ROWS = [   # (class, anim, directions, row)
    ('enemypounder_3423', 0, ('0', '16', '24'), 'fdp'), ('enemypounder_3423', 8, ('0', '16', '24'), 'fdp_dead'),
    ('enemygatlinggun_3669', 0, ('0',), 'fdg_gun'), ('enemygatlinggun2_3671', 0, ('0',), 'fdg_gun2'),
    ('enemygatlinggunbarrel_3670', 0, ('0',), 'fdg_bar'), ('enemygatlinggunleft_3668', 0, ('0',), 'fdg_arm'),
    ('enemygatlingguntable_3666', 0, ('0',), 'fdg_table'), ('enemygatlinggundebris_3673', 0, ('0',), 'fdg_deb'),
    ('enemybeastiris_3735', 0, ('0', '16'), 'fdi'), ('enemybeastiris_3735', 7, ('0', '16'), 'fdi_fall'),
    ('enemybeastiris_3735', 8, ('0',), 'fdi_dead'), ('enemybeastiris_3735', 10, ('0', '16'), 'fdi_bite'),
    ('enemyspiderbeamer_3755', 0, ('0', '8', '16', '24'), 'fds'), ('enemyspiderbeamer_3755', 10, ('0', '8', '16', '24'), 'fds_dead'),
    ('enemyspiderbeamer_3755', 11, ('0', '8', '16', '24'), 'fds_open')]
_FDE_WITH = {'enemygatlinggundebris_3673': ('enemygatlinggun',), 'enemygatlinggunleft_3668': ('enemygatlinggunleft', 'enemygatlinggunright')}
FDE_ENS = [(c, a, d, nm + _FDE_SFX.get(d, '_' + d), None, _FDE_WITH.get(c, (c.rsplit('_', 1)[0],))) for c, a, ds, nm in _FDE_ROWS for d in ds]
FDE_ENS += [('enemyhomingbullet_2648', 0, '0', 'tbs_bul', None, ('enemyspiderbeamer',))]   # the spider beamers' shots (16694)
ROW_FRAMES['tbs_bul'] = ROW_FRAMES['tbs_bul'] | {72, 73}
FDE_ENS_WITH = {'hazardshockwave_1576': ('enemypounder',), 'enemytrooperlaser_2530': ('enemygatlinggun',),
    'enemyhomingbullet_2648': ('enemyspiderbeamer',), 'enemygatlinggundebris_3673': ('enemygatlinggun',),
    'objectkeystone_429': ('enemypounder', 'enemygatlinggun'), 'effectsdebris_427': ('enemypounder', 'enemyspiderbeamer'),
    'effectsexplosion06_435': ('enemypounder', 'enemygatlinggun')}
ENGINE_TYPES_ID.update(FDE_TYPES_ID); ENS += FDE_ENS   # FD_ENEMIES
for _k, _v in FDE_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v   # FD_ENEMIES
for _e in FDE_ENS: ROW_FRAMES[_e[3]] = ROW_FRAMES.get(_e[3], set()) | FDE_FRAMES
try:
    FDE_MIRRORED = _tbe_mirror_rows(FDE_ENS)
except FileNotFoundError:
    FDE_MIRRORED = set()
for _nm in FDE_MIRRORED: ROW_FRAMES[_nm] = set()   # FD_ENEMIES (exact-mirror left rows: drawn flipped)
# the spider beamers' directions per act (VRAM): FD4a faces 0 / 16 (16698), FD4b 16 / 24 (16807); direction 8 is never used
for _b in ('fds', 'fds_dead', 'fds_open'):
    ROW_FRAMES[_b] = {72}; ROW_FRAMES[_b + '_24'] = {73}; ROW_FRAMES[_b + '_8'] = set()

# ---- FD_GIM (fd_gim.c, 2752-2911)

# ---- FD_BOSS (fd_boss.c, 2912-3071)

# ==== end Final Dreadnought
SHL_CLASSES = {'shieldwood_342', 'shieldearth_443', 'shieldwater_439', 'shieldmagnetic_344', 'shieldflamefront_343', 'shieldflameback_376', 'effectsshield_440'}
ENS_ONLY_WITH = {'guiweaponslot_426', 'effectswatersplash_379', 'bubblepatch_804', 'mediumbubble_805', 'generalwaterhorizon_377', 'bubblenumbers_405', 'shieldwood_342', 'shieldearth_443', 'shieldwater_439', 'shieldmagnetic_344', 'shieldflamefront_343', 'shieldflameback_376', 'effectsshield_440'}
# classes whose untrimmed image box matters (spring top edges RV, box-collision platforms/logs, invisible
# sensors): their images are never cropped. Everything else in the object bank is cropped to its opaque pixels.
BOX_TYPES = {8, 13, 14, 15, 16, 17, 30, 32, 33, 34, 35, 36}
# collision mask threshold: Chowdren builds image collision bits from alpha != 0 (RE Image::upload_texture,
# collide_sprite_sprite); FP_ALPHA_MIN overrides for experiments
ALPHA_MIN = int(os.environ.get('FP_ALPHA_MIN', '1'))

# Quick Backdrops (session 19, RE QuickBackdrop::draw 0x203b530): a box of width 0x4c x height 0x50 at the instance x, y;
# with an image (motif) the image tiles it from the box's top-left corner (no hotspot), without one draw_gradient fills it
# with colour 0xc4-0xc7 (R G B A; 0xc8 = 1: a gradient to 0xcc-0xcf). Before, the importer placed the image once (or skipped
# an image-less one), leaving most of RM3's layer-1 cave wall, its track supports, RM2's dungeon wall, RM5's brick wall, FN2's
# rails and DV3's / FN5's strips undrawn (and the box-solid ones without collision). A box larger than its tile is written
# as one TILED piece (pad bit 2 + a size record, write_lv2; game.c draws only the visible tiles); a solid fill tiles a
# shared 64 x 64 colour block (synthetic image ids >= QB_BASE). Collision is rasterised from the expanded tiles.
QB_BASE, QB_SYNTH = 10000000, {}
def quick_backdrop(src, b):
    f = b['fields']; W, H = f.get('0x4c', 0), f.get('0x50', 0)
    if W <= 0 or H <= 0: return None
    if b['image'] is not None:
        im, hx, hy = load_img(src, b['image'])
        return (b['image'], im.width, im.height, W, H)
    if f.get('0xc8', 0) != 0: return None              # gradients: none in the ported levels (not implemented)
    rgba = (f.get('0xc4', 0), f.get('0xc5', 0), f.get('0xc6', 0), f.get('0xc7', 255))
    bw, bh = min(W, 64), min(H, 64)
    key = ('solid', rgba, bw, bh)
    if key not in QB_SYNTH:
        QB_SYNTH[key] = QB_BASE + len(QB_SYNTH); QB_SYNTH[QB_SYNTH[key]] = (Image.new('RGBA', (bw, bh), rgba), 0, 0)
    return (QB_SYNTH[key], bw, bh, W, H)

def load_img(src, i):
    if i in QB_SYNTH: return QB_SYNTH[i]
    if i < 0:   # translucent layer of image -i: its semi-transparent pixels made opaque (drawn with vertex alpha)
        im, hx, hy = load_img(src, -i)
        px = [(r, g, b, 255 if 0 < a < 255 else 0) for r, g, b, a in im.getdata()]
        im = Image.new('RGBA', im.size); im.putdata(px)
        return im, hx, hy
    im = Image.open(os.path.join(src, 'img_%d.png' % i)).convert('RGBA')
    m = [int(v, 0) for v in open(os.path.join(src, 'img_%d_meta.txt' % i)).read().split()]
    hx, hy = [v - 65536 if v > 32767 else v for v in m[:2]]
    return im, hx, hy

# ---- Dreamcast button labels (D-110, session 22, user request). The original's buttons are A / B / C (jump / attack /
# special, RV keys z / x / c); the port's pad maps them to A / B / X (src/platform/plat_dc.c), so the player-facing "C" is
# shown as X. Milla's HUD panel (GUIScoreTimeRings_345 direction 4: 'hudm', RE 2197-2201 pick the frame) labels her cube
# "B" (attack) and her shield "C" (special). Its C glyph (x 90-99, y 1-12 of every 116x18 frame: 1-px dark outline, top
# rows 2-6 white, rows 7-11 in the frame's yellow / orange) is redrawn as an X with the same outline rule (8-neighbour) and
# the frame's own colours, so no new colour enters the palette and the image box is unchanged.
DC_X_GLYPH = ('XX....XX', 'XXX..XXX', 'XXXXXXXX', '.XXXXXX.', '..XXXX..', '..XXXX..', '.XXXXXX.', 'XXXXXXXX', 'XXX..XXX', 'XX....XX')
def dc_hud_c_to_x(im):
    im = im.copy(); px = im.load()
    ol, top, bot = px[90, 4], px[92, 3], px[92, 9]
    # the C this edit expects: outline left of the stem, the stem's two tones, the C's open right side (transparent)
    if im.size != (116, 18) or ol[3] != 255 or top[3] != 255 or bot[3] != 255 or top == bot or px[95, 6][3] != 0 or px[99, 6] != ol:
        raise SystemExit('dc_hud_c_to_x: not GUIScoreTimeRings direction 4 art')
    for y in range(0, 14):
        for x in range(90, 99): px[x, y] = (0, 0, 0, 0)          # x 99 is shared with the shield box's outline: kept
    glyph = {(91 + gx, 2 + gy) for gy, row in enumerate(DC_X_GLYPH) for gx, c in enumerate(row) if c == 'X'}
    for x, y in glyph:
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if (x + dx, y + dy) not in glyph and 90 <= x + dx <= 99: px[x + dx, y + dy] = ol
    for x, y in glyph: px[x, y] = top if y <= 6 else bot
    return im

PAR_IMG = {'levelparallaxpiece0_491': 16032, 'levelparallaxpiece1_891': 3030, 'levelparallaxpiece2_893': 3031,
           'levelparallaxpiece3_894': 937, 'levelparallaxpiece4_895': 326, 'levelparallaxpiece5_896': 3032,
           'levelparallaxpiece6_897': 3033, 'levelparallaxpiece2_892': 4216, 'levelparallaxpiece7_913': 3238,
           'levelparallaxpiece7_987': 3404, 'levelparallaxpiece8_1006': 5422, 'levelparallaxpiece9_1005': 6320,
           'levelparallaxpiece10_1007': 6272, 'levelparallaxpiece11_1009': 5129, 'levelparallaxpiece12_1067': 6388,
           'levelparallaxpiece13_1068': 6387, 'levelparallaxpiece13_1092': 6387,
           # Aqua Tunnel (session 12, RE spec_at_flow 4.2)
           'levelparallaxpiece6_678': 2532, 'levelparallaxpiece2_676': 2430, 'levelparallaxpiece5_677': 7125,
           'levelparallaxpiece8_837': 8305, 'levelparallaxpiece9_842': 3482, 'levelparallaxpiece7_836': 4868,
           'levelparallaxpiece10_843': 3028, 'levelparallaxpiece6_844': 2532, 'levelparallaxpiece8_850': 8305,
           'levelparallaxpiece7_849': 4868, 'levelparallaxpiece6_853': 4866, 'levelparallaxpiece9_855': 687,
           'levelparallaxpiece10_873': 3028,
           # Relic Maze (session 14, RE spec_rm_flow 4): RM1 / RM3 sky set 0r-4r, RM2 its own 3r / 4r, RM3 / RM4 caves, RM5
           'levelparallaxpiece0r_1112': 12345, 'levelparallaxpiece1r_1133': 12347, 'levelparallaxpiece2r_1134': 12324,
           'levelparallaxpiece3r_1137': 12325, 'levelparallaxpiece4r_1138': 12326, 'levelparallaxpiece3r_1220': 2250,
           'levelparallaxpiece4r_1221': 3335, 'levelparallaxpiece6_1349': 3383, 'levelparallaxpiece7_1350': 3426,
           'levelparallaxpiece9_1351': 1990, 'levelparallaxpiece6_1452': 3383, 'levelparallaxpiece7_1453': 3426,
           'levelparallaxpiece9_1454': 1990, 'levelparallaxpiece6_1531': 2058, 'levelparallaxpiece7_1532': 1601,
           'levelparallaxpiece5_1309': 12333,
           # Fortune Night (session 17, RE spec_fn_flow 4): FN1-FN3 / FN6 night sky set, FN2's own piece5, FN4 / FN5 mall set
           'levelparallaxpiece0a_1629': 3626, 'levelparallaxboat1_1640': 1040, 'levelparallaxpiece1_1630': 3613,
           'levelparallaxpiece3_1633': 3625, 'levelparallaxpiece2_1632': 3473, 'levelparallaxpiece4_1634': 187,
           'levelparallaxpiece5_1635': 3611, 'levelparallaxpiece5_1802': 3612, 'levelparallaxcar_1725': 7698,
           'levelparallaxpiece2_1631': 7053, 'levelparallaxpiece0m_1897': 3653, 'levelparallaxpiece1_1898': 4358,
           'levelparallaxpiece2_1899': 4369,
           # Sky Battalion (session 19, RE constructors create_levelparallaxpiece*_20xx: SystemBox images, boxes = image size)
           'levelparallaxpiece0_2059': 4814, 'levelparallaxpiece1_2060': 4822, 'levelparallaxpiece2_2061': 4807,
           'levelparallaxpiece3_2062': 4812, 'levelparallaxpiece4_2063': 4811, 'levelparallaxpiece5_2064': 8426,
           'levelparallaxpiece6_2065': 8421, 'levelparallaxpiece7_2094': 8425, 'levelparallaxsbnight1_2134': 4813,
           'levelparallaxsbnight2_2133': 4815, 'levelparallaxzaoship_2243': 305}
# piece5_1309 (RM3 layer 3, the caravan-chase foreground, spec_rm_flow 4 / spec_rm_claw 6): exported with flags 256 (layer 3:
# drawn in front of the player, par_draw_front) + 512 (hidden until rm_claw.c shows it at 6256); D = E = 0 (never written)
PAR_FRONT = {(27, 1309)}
PAR_BOXH = {'levelparallaxpiece6_897': 160, 'levelparallaxpiece13_1068': 1024, 'levelparallaxpiece13_1092': 820,
            'levelparallaxpiece6_678': 800, 'levelparallaxpiece2_676': 960, 'levelparallaxpiece5_677': 1105,
            'levelparallaxpiece8_837': 1105, 'levelparallaxpiece9_842': 1105, 'levelparallaxpiece7_836': 1105, 'levelparallaxpiece10_843': 1105,
            'levelparallaxpiece6_1349': 720, 'levelparallaxpiece7_1350': 720, 'levelparallaxpiece9_1351': 1600,   # RM3 (spec_rm_flow 4)
            'levelparallaxpiece1_1630': 120}   # FN (spec_fn_flow 4): a 320 x 120 box over the 240-high image (its top half shows)
PAR_BOXW = {'levelparallaxpiece6_853': 400, 'levelparallaxpiece5_1309': 384}          # box wider than its image (AT4 screen-fixed backdrop)
PAR_WRAPY = {18, 19, 20}                             # AT3 / AT4 frames (json 19, 20): 4532 / 4527 wrap in both axes; AT2 json 18 has none (see below)
# D, E, G (RE 5110 / 5315 / 5603; DV1's start events not read: the same table is used, INFERRED)
PAR_DEG = {491: (.99, .94, 0), 891: (.98, .93, 0), 893: (.95, .91, 0), 894: (.96, .92, -.1), 895: (.92, .88, -.3),
           896: (.6, .7, 0), 897: (.5, .6, 0), 892: (.4, .5, 0), 913: (.35, .43, 0), 987: (.35, .43, 0),
           1006: (.7, .8, 0), 1005: (.6, .7, 0), 1007: (.4, .5, 0), 1009: (.35, .43, 0), 1067: (.15, .15, 0),
           1068: (.15, .15, 0), 1092: (.15, .15, 0),
           678: (.75, .75, 0), 676: (.55, .55, 0), 677: (.40, .40, 0), 837: (.60, .60, 0), 842: (.50, .50, 0), 836: (.40, .40, 0),
           843: (.30, .30, 0), 844: (.50, .50, 0), 850: (.45, .45, 0), 849: (.40, .40, 0), 853: (1.0, 1.0, 0), 855: (.70, .70, 0),
           873: (.30, .30, 0),
           # Relic Maze (spec_rm_flow 4; 5842 / 6370 / 3559)
           1112: (.99, .94, 0), 1133: (.95, .92, 0), 1134: (.85, .85, 0), 1137: (.40, .40, 0), 1138: (.37, .36, 0),
           1220: (.40, .40, 0), 1221: (.37, .36, 0), 1349: (.85, .85, 0), 1350: (.60, .60, 0), 1351: (.30, .30, 0),
           1452: (.85, .85, 0), 1453: (.60, .60, 0), 1454: (.30, .30, 0), 1531: (.85, .85, 0), 1532: (.60, .60, 0), 1309: (0, 0, 0),
           # Fortune Night (spec_fn_flow 4: FN1 inline, FN2 / FN3 7472, FN4 inline, FN5 8318; G = the drift per frame, 3541)
           1629: (.99, .94, 0), 1640: (.99, .94, -.25), 1630: (.90, .90, 0), 1633: (.88, .88, 0), 1632: (.80, .80, 0),
           1634: (.70, .70, 0), 1635: (.65, .65, 0), 1802: (.65, .65, 0), 1725: (.40, .505, -4), 1631: (.40, .50, 0),
           1897: (.99, .94, 0), 1898: (.72, .72, 0), 1899: (.60, .70, 0),
           # Sky Battalion (RE 9215 SB1, 9764 SB2, 9934 SB3, 10146 SB4: the same values in every act; G = the drift, 3541)
           2059: (.99, .99, -.01), 2060: (.97, .97, -.1), 2061: (.95, .95, -.4), 2062: (.98, .98, -.05), 2063: (.96, .96, -.2),
           2064: (.70, .97, -3), 2065: (.50, .90, -6), 2094: (.60, .94, -4.5), 2133: (.97, .97, -.1), 2134: (.99, .99, -.01),
           2243: (0, 0, 0)}
PAR_DEG_FRAME = {(27, 1112): (1.0, .94, 0),          # the same handle with RM3's own D (RE spec_rm_flow 4)
                 (35, 1629): (.99, .94, -.1), (35, 1640): (.99, .94, -.35)}   # FN6's drifting sky (inline, spec_fn_flow 4)
# BGStar_16 (FN1-FN3, FN6; spec_fn_flow 4): 7471 stores A = x, B = y, C = the instance index and forces frame C; 7466 puts
# every star at (piece0a.x + A, piece0a.y + B) each frame. A class listed here (class_handle: parent piece handle) is exported
# as one composed image attached to that piece (flag 1024, parallax.c). Fortune Night's stars are objects of fn_gima.c
# (OT_FN_STAR 603, merged first), so the table is empty: listing bgstar_16 here would draw them twice.
PAR_STARS = {}
PAR_SKY_BELOW = {27: (1112, 1133, 1134, 1137, 1138)} # RM3 6298: destroyed once the player is below y 2400 (one way)
# layers scrolling at 0.8 (RE Layer::init): Aqua Tunnel and RM1-RM4 layer 1, RM5 layer 0 (spec_rm_flow 2.4); json frame
SCROLL08 = {17: 1, 18: 1, 19: 1, 20: 1, 25: 1, 26: 1, 27: 1, 28: 1, 29: 0, 30: 1, 31: 1, 32: 1, 33: 1, 34: 1, 35: 1,   # FN (json 30-35): layer 1 (spec_fn_flow 2.4)
            37: 1, 38: 1, 39: 1}   # SB2-SB4 layer 1 (the ships' backdrops); SB1's layer 1 scrolls at 0.25 and holds only the airships (sb_flow.c)
# Sky Battalion (session 19): 9198 replaces 3542 in every SB act: K = (view top + 20 sin(g3 deg) - 20) E + C, the whole sky bobs
# (flag 2048); LevelParallaxZaoShip_2243 is a SystemBox on layer 0 at a world position, between piece6 and piece7 (flag 4096:
# drawn once at (D, y0) in world coordinates; RE create_levelparallaxzaoship_2243: image 0x131 = 305, 507 x 323)
PAR_BOB_FRAMES = {36, 37, 38, 39}
PAR_WORLD = {2243}
# images with per-pixel alpha (Sky Battalion's mist: pieces 5 / 6 / 7, every pixel alpha 1..~100, RE images 8426 / 8421 / 8425):
# flag 1 = ARGB4444 texture (FPTX fmt 4) drawn through the translucent list, flag 2 = stored at half resolution, drawn x2
# (soft mist; a quarter of the VRAM: about 200 KB instead of 800 KB). Image id -> flags (session 19)
PAR_ALPHA = {8426: 3, 8421: 3, 8425: 3}
# ---- JC_FLOW parallax (session 21, build/re/spec_jc_flow.md 5): SystemBox pieces (RE create_levelparallaxpiece*_2336 / 19 / 2337 /
# 2338 / 2374 / 2524: image, box w x h); D / E the same in every act (RE on_frame_40_start inline, 10379 JC2 / JC3, 10864 JC4,
# 11644 JC5); JC4 alone drifts: G = (1 - D) * -4 (10864, qualifier 0 = every piece); layer 1 scrolls 0.8 in JC1-JC3 / JC5 and 1.0
# in JC4 (RE Layer::init of on_frame_40..44_init). LevelParalaxWaterSurface_14 (10368-10373) is the moonlit strip between piece5's
# bottom and the water line, piece6 follows it (par water mode 2, parallax.c)
PAR_IMG.update({'levelparallaxpiece0_2336': 16078, 'levelparallaxpiece1_19': 16065, 'levelparallaxpiece2_2337': 16066,
                'levelparallaxpiece5_2338': 16046, 'levelparallaxpiece6_2374': 71, 'levelparallaxpiece7_2524': 16067})
PAR_BOXW.update({'levelparallaxpiece1_19': 320, 'levelparallaxpiece6_2374': 640})
PAR_BOXH.update({'levelparallaxpiece5_2338': 270})
PAR_DEG.update({2336: (1.0, .94, 0), 19: (.90, .90, 0), 2337: (.95, .95, 0), 2338: (.85, .85, 0), 2374: (1.0, 0, 0), 2524: (.80, .85, 0)})
PAR_DEG_FRAME.update({(43, h): (d, e, (1 - d) * -4) for h, (d, e, g) in PAR_DEG.items() if h in (2336, 19, 2337, 2338, 2374, 2524)})
SCROLL08.update({40: 1, 41: 1, 42: 1, 44: 1})
# ---- TH_FLOW parallax / layers (th_flow.c, session 21b)
# Layer::init (RE on_frame_57..59_init): layer 1 scrolls 0.8 in every act (layer 6 0, others 1). No water: ML.A 1 / ML.B 2000 are
# written (start events) but handle_frame_57..59 hold no water events (4150-4170): not in WATER_FRAMES (RV th1_bottom: OTHERS.H 0)
SCROLL08.update({57: 1, 58: 1, 59: 1})
# Parallax (RE create_levelparallaxpiece0_3109 / 3147 / 3192, 1_3207, 2_3208: SystemBox image, box w x h; D / E: TH1 start inline,
# TH2 13539, TH3 14090; no drift). TH3: piece2 hidden at the start (14099), piece0 hidden / piece2 shown once g4 >= 5 (14083, th_flow.c)
PAR_IMG.update({'levelparallaxpiece0_3109': 10992, 'levelparallaxpiece0_3147': 13021, 'levelparallaxpiece0_3192': 13021,
                'levelparallaxpiece1_3207': 5994, 'levelparallaxpiece2_3208': 2737})
PAR_BOXH.update({'levelparallaxpiece0_3109': 863, 'levelparallaxpiece0_3147': 720, 'levelparallaxpiece0_3192': 1000})
PAR_DEG.update({3109: (.6, .6, 0), 3147: (.6, .6, 0), 3192: (.6, .6, 0), 3207: (.95, .95, 0), 3208: (.8, .85, 0)})
# ---- TB_FLOW parallax / layers (tb_flow.c, session 21c; spec_tb_flow 5)
# Layer::init (RE on_frame_45..50_init): layer 1 scrolls 0.8 in TB1-TB3, TB5, TB6 and 1.0 in TB4 (layer 6 0, the rest 1).
# Pieces (RE create_levelparallaxpiece*_25xx-28xx: SystemBox image, box w x h; the image tiles in the box). D / E / G: TB1 / TB2
# start inline, TB3 12029 (piece0 .99 .94, piece1 .98 .93, piece2 .95 .91, piece5 .84 .8, piece6 / piece7 .5 .6); TB4 start inline,
# TB5 12462, TB6 start inline (piece2 .7 .7, piece1 .7 .7 with the drift G -4, piece3 .5 .5). LevelHeatSky_2779 (TB3 / TB6 layer 5)
# is an Active invisible from its ctor: not a parallax piece (not exported)
SCROLL08.update({45: 1, 46: 1, 47: 1, 49: 1, 50: 1})
PAR_IMG.update({'levelparallaxpiece0_2589': 6795, 'levelparallaxpiece1_2590': 4701, 'levelparallaxpiece2_2591': 6128,
                'levelparallaxpiece5_2592': 6368, 'levelparallaxpiece6_2593': 9225, 'levelparallaxpiece7_2725': 1608,
                'levelparallaxpiece1_2783': 6237, 'levelparallaxpiece2_2784': 6242, 'levelparallaxpiece3_2851': 2753,
                'levelparallaxpiece2_2855': 3413, 'levelparallaxpiece3_2875': 5935, 'levelparallaxpiece2_2885': 2319})
PAR_BOXW.update({'levelparallaxpiece0_2589': 320})
PAR_BOXH.update({'levelparallaxpiece7_2725': 640, 'levelparallaxpiece1_2783': 1280, 'levelparallaxpiece2_2784': 1280,
                 'levelparallaxpiece3_2851': 1920, 'levelparallaxpiece2_2855': 1280, 'levelparallaxpiece3_2875': 1920,
                 'levelparallaxpiece2_2885': 1280})
PAR_DEG.update({2589: (.99, .94, 0), 2590: (.98, .93, 0), 2591: (.95, .91, 0), 2592: (.84, .8, 0), 2593: (.5, .6, 0), 2725: (.5, .6, 0),
                2783: (.7, .7, -4), 2784: (.7, .7, 0), 2855: (.7, .7, 0), 2885: (.7, .7, 0), 2851: (.5, .5, 0), 2875: (.5, .5, 0)})
# ---- PL_FLOW parallax / layers (pl_flow.c, session 23; spec_pl_flow 5)
# Layer::init (RE disassembly of on_frame_51..56_init): layer 1 scrolls 0.8 in every act (layer 6 0, the rest 1).
# Pieces (RE create_levelparallaxpiece*_2909-3070: SystemBox image, box w x h; the image tiles in the box; 894 / 895 are DV's
# handles with PL1's own D / E / G). D / E / G (RE: PL1 / PL2 start inline, PL3 12960, PL4 start inline, PL5 13104, PL6 start
# inline): piece0 .94 .92, piece1 .93 .92, piece2 .91 .92, piece3 .9 .92 G -.1, piece4 .92 .88 G -.3 (PL1), piece5 .88 .92,
# piece6 .85 .92 (PL1-PL3); PL4 / PL5 piece0_3024 .8 1, piece1_3034 .7 .8, piece2_3040 .6 .6, piece3_3041 .4 .4; PL6 piece0_3070 .8 1
SCROLL08.update({51: 1, 52: 1, 53: 1, 54: 1, 55: 1, 56: 1})
PAR_IMG.update({'levelparallaxpiece0_2909': 462, 'levelparallaxpiece1_2910': 279, 'levelparallaxpiece2_2911': 13596,
                'levelparallaxpiece5_2912': 5686, 'levelparallaxpiece6_2913': 13597, 'levelparallaxpiece0_3024': 12873,
                'levelparallaxpiece1_3034': 5503, 'levelparallaxpiece2_3040': 8541, 'levelparallaxpiece3_3041': 12772,
                'levelparallaxpiece0_3070': 12924})
PAR_BOXH.update({'levelparallaxpiece1_3034': 640, 'levelparallaxpiece0_3070': 320})
PAR_DEG.update({2909: (.94, .92, 0), 2910: (.93, .92, 0), 2911: (.91, .92, 0), 2912: (.88, .92, 0), 2913: (.85, .92, 0),
                3024: (.8, 1, 0), 3034: (.7, .8, 0), 3040: (.6, .6, 0), 3041: (.4, .4, 0), 3070: (.8, 1, 0)})
PAR_DEG_FRAME.update({(f, 894): (.9, .92, -.1) for f in (51, 52, 53)})
PAR_DEG_FRAME.update({(51, 895): (.92, .88, -.3)})
# ---- BG_FLOW parallax / layers (bg_flow.c, session 24; spec_bg_flow 5)
# Layer::init (RE disassembly of on_frame_60..65_init): layer 1 scrolls 0.5 in BG2-BG4 / BG6, 1.0 in BG1 / BG5; it holds only
# BrevonMechBackground_2444 (bosses module), no backdrop: nothing to export (not in SCROLL08). Pieces (RE create_levelparallaxpiece*_3223-
# 3227 / 3307 / 3308: SystemBox image, box w x h; the image tiles in the box). D / E / G: BG1 start inline, BG2 / BG3 14173 (piece0 .9 .9
# G -.3, piece1 .96 .96, piece2 .9 .9, piece5 .85 .85, piece6 .75 .75); BG4 14603, BG5 start inline, BG6 15151 (piece0_3307 .9 .9 G -.3,
# piece1 .96 .96, piece2_3308 .95 .85, piece5 .98 .92). Brevon's ship (BG4 LevelBrevonShip_3310, a SystemBox on layer 0 between piece5 and
# piece2; 14603: D .96, E .9, F = its x) and its close views (BG5 LevelBrevonShipClose_3353 / Close2_3364: D .94, E .8, F = x) are moved
# by 14550 / 14551 (the BG copy of 3541 / 3542 without the wrap): drawn once at (D vl + F, E vt + C) (flag 8192, x in the box width; the
# box size is the image's, RE ctor 306 x 320). The close views are not exported (PAR_FIXED_SKIP); BG6's (x -40 / 264, y -88) and the
# lift-off jets belong to its ending scene (bosses module).
PAR_IMG.update({'levelparallaxpiece0_3223': 1025, 'levelparallaxpiece1_3224': 1614, 'levelparallaxpiece2_3225': 1612,
                'levelparallaxpiece5_3226': 1525, 'levelparallaxpiece6_3227': 1172, 'levelparallaxpiece0_3307': 11544,
                'levelparallaxpiece2_3308': 7981, 'levelbrevonship_3310': 11355, 'levelbrevonshipclose_3353': 11356,
                'levelbrevonshipclose2_3364': 11357})
PAR_BOXW.update({'levelparallaxpiece2_3308': 320})
PAR_BOXH.update({'levelparallaxpiece1_3224': 180})
PAR_DEG.update({3223: (.9, .9, -.3), 3224: (.96, .96, 0), 3225: (.9, .9, 0), 3226: (.85, .85, 0), 3227: (.75, .75, 0),
                3307: (.9, .9, -.3), 3308: (.95, .85, 0), 3310: (.96, .9, 0), 3353: (.94, .8, 0), 3364: (.94, .8, 0)})
PAR_DEG_FRAME.update({(f, 3226): (.98, .92, 0) for f in (63, 64, 65)})
PAR_FIXED = {(63, 3310)}                              # json frame, handle: drawn once at its world position moved by D / E (flag 8192)
PAR_FIXED_SKIP = {3353, 3364}                         # the close views: not exported (BG5: on screen only at view left > 5300 near the top,
                                                      # two 305 x 657 images ~400 KB of VRAM, INFERRED not worth it; BG6: its ending's)
PAR_ALPHA.update({1614: 3})                           # piece1_3224: white mist, alpha 0..211 (ARGB4444 at half resolution, drawn x2)
# ---- FD_FLOW parallax (session 24, build/re/spec_fd_flow.md 5): SystemBox pieces (RE create_levelparallaxpiece*_33xx-37xx /
# create_levelparallaxtunnel1_3464 / 2_3463: image, box w x h; the image tiles in the box). D / E / G: fd1a 15309, fd1b its start inline (the
# same values + the tunnel strips: tunnel2 D .5, tunnel1 D .25, E 0), fd2a / fd2b their start inline, fd3a / fd3b 16200, fd4a 16728 (piece0 D 1
# E .98); fd4b's piece0 is moved by the final boss's phases (bosses module; its first write D 1 E .98 is exported). Not exported (INFERRED):
# fd2b's piece5 / 7 / 9 (3589 / 3588 / 3591) and fd4a's piece1_3726 (no event writes their D / E: values unknown), fd4b's
# ParallaxSpaceBack 1-3 (the final boss's backdrops), FDJumbotron (fd1a), FD3BackPillar / FlashbackBG (fd3a / fd3b scenery Actives).
PAR_IMG.update({'levelparallaxpiece0_3390': 3752, 'levelparallaxpiece1_3395': 2959, 'levelparallaxpiece10_3393': 1397,
                'levelparallaxpiece11_3394': 8566, 'levelparallaxpiece2_3413': 2960, 'levelparallaxpiece7_3391': 1628,
                'levelparallaxtunnel1_3464': 15075, 'levelparallaxtunnel2_3463': 3827,
                'levelparallaxpiece1_3497': 368, 'levelparallaxpiece10_3496': 1021, 'levelparallaxpiece2_3559': 1016,
                'levelparallaxpiece3_3560': 1036, 'levelparallaxpiece8_3558': 994,
                'levelparallaxpiece0_3590': 5872, 'levelparallaxpiece1_3566': 368, 'levelparallaxpiece10_3565': 1021,
                'levelparallaxpiece2_3570': 1016, 'levelparallaxpiece3_3571': 1036, 'levelparallaxpiece8_3569': 994,
                'levelparallaxpiece0_3608': 1281, 'levelparallaxpiece1_3609': 11371, 'levelparallaxpiece7_3611': 245,
                'levelparallaxpiece8_3612': 206, 'levelparallaxpiece9_3613': 1215, 'levelparallaxpiece0_3725': 14804})
PAR_BOXW.update({'levelparallaxpiece10_3393': 400, 'levelparallaxpiece11_3394': 400, 'levelparallaxpiece2_3413': 400,
                 'levelparallaxpiece7_3391': 400, 'levelparallaxtunnel2_3463': 64})
PAR_BOXH.update({'levelparallaxpiece1_3395': 238, 'levelparallaxpiece2_3413': 238, 'levelparallaxpiece1_3497': 1440,
                 'levelparallaxpiece10_3496': 1920, 'levelparallaxpiece2_3559': 1440, 'levelparallaxpiece3_3560': 1440,
                 'levelparallaxpiece8_3558': 1600, 'levelparallaxpiece1_3566': 3200, 'levelparallaxpiece10_3565': 4800,
                 'levelparallaxpiece2_3570': 3200, 'levelparallaxpiece3_3571': 3200, 'levelparallaxpiece8_3569': 4000,
                 'levelparallaxpiece0_3725': 380})
PAR_DEG.update({3390: (.9, .9, 0), 3391: (.8, .88, 0), 3393: (.7, .9, 0), 3394: (.7, .86, 0), 3395: (.95, .95, 0), 3413: (1, 1, -.3),
                3463: (.5, 0, 0), 3464: (.25, 0, 0),
                3496: (.3, .3, 0), 3497: (.6, .6, 0), 3558: (.45, .45, 0), 3559: (.6, .6, 0), 3560: (.6, .6, 0),
                3565: (.3, .3, 0), 3566: (.6, .6, 0), 3569: (.45, .45, 0), 3570: (.6, .6, 0), 3571: (.6, .6, 0), 3590: (1, 1, -1),
                3608: (.99, .94, 0), 3609: (.8, .8, 0), 3611: (.8, .8, 0), 3612: (.7, .7, 0), 3613: (.6, .65, 0), 3725: (1, .98, 0)})
PAR_DEG_FRAME.update({(69, 3224): (1, 1, 0)})          # fd2b's mist (BG's piece1_3224 with D 1 E 1, on_frame_69 start inline)
PAR_ALPHA.update({1281: 2})                           # fd3a / fd3b sky piece0_3608 (2048 x 320, D .99): half resolution, drawn x2 (VRAM, INFERRED)
JC_WATER = {40: 1200, 41: 1600, 42: 1600, 43: 1600, 44: 1600}   # ML.B (10378 / 10566 / 10822 / 10863 / 11643)
JC_SURF_FRAMES = [7795, 7802, 7803, 7804, 7796, 7797, 7798]      # LevelParalaxWaterSurface_14 anim 0 (both directions use the same images);
                                                                 # its 8th frame (7799) is shown as the 7th: one 512 x 512 4 bpp atlas (VRAM)

def write_tex4444(img, path):
    """FPTX fmt 4: ARGB4444, linear (16 levels of alpha; the PVR's translucent list blends it)"""
    img = img.convert('RGBA')
    w, h = 8, 8
    while w < img.width: w *= 2
    while h < img.height: h *= 2
    canvas = Image.new('RGBA', (w, h), (0, 0, 0, 0)); canvas.paste(img, (0, 0))
    out = bytearray(b'FPTX' + struct.pack('<6H', w, h, 4, img.width, img.height, 0))
    for r, g, b, a in canvas.getdata():
        out += struct.pack('<H', ((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4))
    open(path, 'wb').write(out)
    return w, h

# invisible static obstacle objects baked into the solid collision map (class_handle; RE: constructor set_visible(0),
# initialize_active(false) = pixel mask, qualifier 8 obstacle, no event moves them): Sky Battalion's ship decks (session 19)
BAKE_SOLID = {'shipplatform1_2088', 'shipplatform2_2089', 'shipplatform3_2091'}

def pow2_split(n, cap=1024):
    out, r = [], n
    while r > 0:
        p = 8
        while p * 2 <= min(r, cap): p *= 2
        if r < 8: p = 8
        out.append(p); r -= p
    return out

def export_parallax(fr, src, name, out, an=None):
    """<name>_par.bin + chunk textures <name>_px<img>_<k>.tex (ARGB1555; palettize --pal4 converts them)"""
    pieces, images, ntex = [], [], 0
    imgidx = {}
    has_switch = any(o['cls'] == 'levelparallaxpiece8' for o in fr['objects'])
    water = None
    for o in fr['objects']:
        key = '%s_%d' % (o['cls'], o['id'])
        if o['cls'] == 'levelparalaxwatersurface' and o['x'] is not None: water = 2700
        if o['cls'] == 'levelparalaxwatersurface' and fr['frame'] in JC_WATER: water = JC_WATER[fr['frame']]   # mode 2 (Jade Creek)
        if key not in PAR_IMG or o['y'] is None: continue
        if o['id'] in PAR_FIXED_SKIP and (fr['frame'], o['id']) not in PAR_FIXED: continue   # BG_FLOW: the ships only where exported
        img = PAR_IMG[key]
        if img not in imgidx:
            im, hx, hy = load_img(src, img)
            fw, fh = im.width, im.height
            iflags = PAR_ALPHA.get(img, 0)
            if iflags & 2:                                 # half resolution (drawn x2): premultiplied resize
                im = im.convert('RGBa').resize(((im.width + 1) // 2, (im.height + 1) // 2), Image.BILINEAR).convert('RGBA')
            chunks = []
            y = 0
            for ch in pow2_split(im.height):
                x = 0
                for cw in pow2_split(im.width):
                    sub = im.crop((x, y, min(x + cw, im.width), min(y + ch, im.height)))
                    tn = '%s_p%s%d_%d' % (name, 'a' if iflags & 1 else 'x', len(images), len(chunks))   # _pa: ARGB4444, outside the palettize sets
                    (write_tex4444 if iflags & 1 else write_tex)(sub, os.path.join(out, tn + '.tex'))
                    chunks.append((x, y, sub.width, sub.height))
                    x += cw
                y += ch
            imgidx[img] = len(images); images.append((fw, fh, chunks, iflags))
        D, E, G = PAR_DEG_FRAME.get((fr['frame'], o['id']), PAR_DEG.get(o['id'], (.5, .5, 0)))
        flags = 0
        if has_switch and o['id'] in (491, 891): flags |= 1        # shown while view top < 1360 (5101/5102)
        if has_switch and o['id'] in (1006, 1005): flags |= 2      # shown while view top >= 1360
        if o['id'] == 1005: flags |= 4                             # piece9: water frames (5094)
        if fr['frame'] in (19, 20, 28, 29): flags |= 8             # AT3 / AT4, RM4 / RM5: wrap in y too (4532 / 4527)
        if o['id'] in PAR_SKY_BELOW.get(fr['frame'], ()): flags |= 64   # RM3 6298: gone once the player is below y 2400
        if fr['frame'] == 18 and o['id'] in (677, 676): flags |= 16    # AT2 (4398 / 4399): shown while view left < 7544
        if fr['frame'] == 18 and o['id'] in (836, 837, 842, 843): flags |= 32   # shown while view left > 7544
        if (fr['frame'], o['id']) in PAR_FRONT: flags |= 256 | 512                # layer-3 piece, hidden until shown (RM3 piece5)
        if fr['frame'] in PAR_BOB_FRAMES and o['id'] not in PAR_WORLD: flags |= 2048   # SB 9198: the sky bobs with g3
        if o['id'] in PAR_WORLD: flags |= 4096; D = float(o['x'])                  # SB: Zao's airship at its world position
        iw, ih = images[imgidx[img]][0], images[imgidx[img]][1]
        if (fr['frame'], o['id']) in PAR_FIXED:                    # BG_FLOW (14550 / 14551): once at (D vl + x, E vt + y); x in the box width
            pieces.append((imgidx[img], D, E, G, o['y'], max(0, o['x']), ih, flags | 8192, o['id'])); continue
        pieces.append((imgidx[img], D, E, G, o['y'], PAR_BOXW.get(key, iw), PAR_BOXH.get(key, ih), flags, o['id']))
    stars = [o for o in fr['objects'] if '%s_%d' % (o['cls'], o['id']) in PAR_STARS and o['x'] is not None]
    if stars:
        parent = PAR_STARS['%s_%d' % (stars[0]['cls'], stars[0]['id'])]
        fl = an['%s_%d' % (stars[0]['cls'], stars[0]['id'])]['anims'][0]['0']['frames']
        ims = [(o['x'], o['y']) + load_img(src, fl[k % len(fl)]) for k, o in enumerate(stars)]   # C = the instance index (json order, INFERRED)
        x0 = min(x - hx for x, y, im, hx, hy in ims); y0 = min(y - hy for x, y, im, hx, hy in ims)
        x1 = max(x - hx + im.width for x, y, im, hx, hy in ims); y1 = max(y - hy + im.height for x, y, im, hx, hy in ims)
        comp = Image.new('RGBA', (x1 - x0, y1 - y0))
        for x, y, im, hx, hy in ims: comp.alpha_composite(im, (x - hx - x0, y - hy - y0))
        chunks, cy = [], 0
        for ch in pow2_split(comp.height):
            cx = 0
            for cw in pow2_split(comp.width):
                sub = comp.crop((cx, cy, min(cx + cw, comp.width), min(cy + ch, comp.height)))
                write_tex(sub, os.path.join(out, '%s_px%d_%d.tex' % (name, len(images), len(chunks))))
                chunks.append((cx, cy, sub.width, sub.height)); cx += cw
            cy += ch
        images.append((comp.width, comp.height, chunks, 0))
        pieces.append((len(images) - 1, 0., 0., 0., parent, x0, y0, 1024, stars[0]['id']))   # y0 = the parent's handle, (bw, bh) = offset
    ver = 2 if any(im[3] for im in images) else 1   # version 1: + the piece's handle; 2 (only with ARGB4444 / half-size images): + image flags
    wmode = 2 if water and fr['frame'] in JC_WATER else 1 if water else 0
    d = bytearray(b'FPPX' + struct.pack('<4H', len(images), len(pieces), wmode, ver))
    for w, h, chunks, iflags in images:
        d += struct.pack('<4H', w, h, len(chunks), iflags) if ver >= 2 else struct.pack('<3H', w, h, len(chunks))
        for c in chunks: d += struct.pack('<4H', *c)
    for (ii, D, E, G, y0, bw, bh, fl, hd) in pieces: d += struct.pack('<H3fi4H', ii, D, E, G, y0, bw, bh, fl, hd)
    if wmode == 2:   # Jade Creek (10368-10373): the surface frames stacked in one atlas; line, frames, frame w / h, the horizon piece
        ims = [load_img(src, i)[0] for i in JC_SURF_FRAMES]   # (its handle, box height) and the follower piece (handle)
        fw, fh = ims[0].width, ims[0].height
        atl = Image.new('RGBA', (fw, fh * len(ims)))
        for k, im in enumerate(ims): atl.paste(im, (0, k * fh))
        write_tex(atl, os.path.join(out, '%s_pv0.tex' % name))   # _pv: its own 4 bpp set (9 colours; build_level.sh), outside par_top's PAL8 set
        d += struct.pack('<i6H', water, len(ims), fw, fh, 2338, 270, 2374)
    elif water:
        for img in (987, 1078):
            im, hx, hy = load_img(src, img)
            tn = '%s_pw%d' % (name, img == 1078)
            write_tex(im, os.path.join(out, tn + '.tex'))
        d += struct.pack('<i', water)
    open(os.path.join(out, name + '_par.bin'), 'wb').write(d)
    return [(p[0], p[4]) for p in pieces]

def first_anim_frames(entry):
    """frames of the first non-empty animation, right-facing direction"""
    for ds in entry.get('anims') or []:
        if ds:
            d = ds.get('0') or ds[sorted(ds, key=int)[0]]
            return d
    return None

def main():
    fd, src, n, name, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5]
    col_layers = {2}
    if '--layers' in sys.argv:
        col_layers = {int(v) for v in sys.argv[sys.argv.index('--layers') + 1].split(',')}
    os.makedirs(out, exist_ok=True)
    fr = json.load(open(os.path.join(fd, 'frame_%d.json' % n)))
    bd = json.load(open(os.path.join(fd, 'backdrops.json')))
    an = json.load(open(os.path.join(fd, 'anims.json')))
    W, H = fr['width'], fr['height']
    # Layer map (session 12, D-045): the port draws and collides with the play layer as 2 (Dragon Valley's). Aqua Tunnel
    # has one more background layer: the player and the collision backdrops are on layer 3 (RE: FrameObject::
    # overlaps_background tests the object's own layer only), layer 2 holds objects behind the play layer and layer 4
    # is in front. Such frames are mapped 2 -> 1, 3 -> 2, 4 -> 3; pieces keep the original layer order (stable sort below).
    # (session 21: JC1's start point sits at x -27, just left of the frame: the player enters at X = -11, RE 2956)
    main = next((o['layer'] for o in fr['objects'] if o['cls'] == 'playerstartingpoint' and o['layer'] is not None
                 and o['x'] is not None and -64 <= o['x'] < W and -64 <= o['y'] < H), 2)   # (TH1's start point is at y -35)   # -64: JC1's start point is at x -27 (walk-in)
    if n == 44 and main == 4: main = 3   # JC5: its start point sits on json layer 4 but the collision backdrops are on 3 (the act-5 agent's RV runs need it; INFERRED rule)
    LMAP = {2: 1, 3: 2, 4: 3} if main == 3 else {}
    for o in fr['objects']:
        o['layer0'] = o['layer'] or 0
        if o['layer'] is not None: o['layer'] = LMAP.get(o['layer'], o['layer'])
    report = ['frame %d -> %s  (%dx%d px, %d placements)' % (n, name, W, H, len(fr['objects']))]

    pieces, objs, skipped = [], [], {}
    for o in fr['objects']:
        key = '%s_%d' % (o['cls'], o['id'])
        if o['x'] is None or o['y'] is None:
            skipped['unresolved'] = skipped.get('unresolved', 0) + 1; continue
        if key in bd:
            b = bd[key]
            qb = quick_backdrop(src, b) if '0x4c' in b['fields'] else None   # session 19: Quick Backdrops fill their own size
            if b['image'] is None and not qb:
                skipped['no-image'] = skipped.get('no-image', 0) + 1; continue
            ac = b['fields'].get('0xac')
            col = 0 if ac is None else {9: 1, 8: 1, 11: 3}.get(ac, 0)
            box = ac == 9
            invisible = b['fields'].get('0x5f') == 0 and '0x5c' in b['fields']
            if SCROLL08.get(n) == o['layer0']: invisible_extra = 2   # the frame's 0.8 layer (RE Layer::init, spec_at_flow 4.1, spec_rm_flow 2.4)
            else: invisible_extra = 0
            if qb:                                   # the tiles of a Quick Backdrop (cw, ch: the cropped last row / column)
                qimg, qtw, qth, qw, qh = qb
                tiled = qw > qtw or qh > qth
                for ty in range(0, qh, qth):
                    for tx in range(0, qw, qtw):
                        pieces.append({'img': qimg, 'x': o['x'] + tx, 'y': o['y'] + ty, 'layer': o['layer'] or 0, 'layer0': o['layer0'], 'scroll08': invisible_extra,
                                       'col': col, 'box': box, 'invisible': invisible, 'cls': o['cls'], 'cw': min(qtw, qw - tx), 'ch': min(qth, qh - ty),
                                       'qb': (qw, qh) if tiled and tx == 0 and ty == 0 else ('tile' if tiled else None)})
                continue
            pieces.append({'img': b['image'], 'x': o['x'], 'y': o['y'], 'layer': o['layer'] or 0, 'layer0': o['layer0'], 'scroll08': invisible_extra, 'col': col,
                           'box': box, 'invisible': invisible, 'cls': o['cls']})
        else:
            t = engine_type(o['cls'], o['id'])
            if t is None:
                skipped[o['cls']] = skipped.get(o['cls'], 0) + 1; continue
            objs.append({'type': t, 'x': o['x'], 'y': o['y'], 'cls': key, 'layer': o['layer'] or 0})

    for o in objs:   # SB_BOSS: SB3's MinibossUFOBeam_1769 is the satellite's main beam
        if o['type'] == 803 and n == 38: o['type'] = 1164
        if o['type'] == 478 and n == 39: o['type'] = 1188   # SB4's EffectWarning_1528: the spire's drop marker (10123 / col_30_38)
    objs = [o for o in objs if not (800 <= o['type'] < 860 and n not in FNBOSS_FRAMES)]   # FN_BOSS: its classes only in FN1-FN3 / FN5
    objs = [o for o in objs if not (1150 <= o['type'] < 1280 and n not in SBBOSS_FRAMES and not (n in THB_SPADE_FRAMES and 1220 <= o['type'] <= 1222))]   # SB_BOSS: its classes only in SB1-SB4 (TH_BOSS: Spade in TH3)
    for o in objs:                                   # JC_BOSS: the shared Hovercop / Neera classes are jc_boss.c's types in JC1-JC4
        if n in JCB_REMAP_FRAMES and o['cls'] in JCB_CLS: o['type'] = JCB_CLS[o['cls']]
    for o in objs: o['type'] = FRAME_TYPE_REMAP.get((n, o['type']), o['type'])   # per-frame type remaps (BG_ENEMIES: the BG Elites)
    objs = [o for o in objs if not (1430 <= o['type'] < 1480 and n not in JCB_REMAP_FRAMES)]   # JC_BOSS: its types only in JC1-JC5 (JC5: the ice ambush)
    for o in objs:                                   # SB_ENEMIES (session 19b): SB1 / SB3's placed EffectWarning_1528 (SB4's: the boss's 1188)
        if o['cls'] == 'effectwarning_1528' and n in SBE_WARN_FRAMES: o['type'] = 1039
    for o in objs:                                   # PL_GIM: Pangu Lagoon's spinball rule (pl_gim.c)
        if n in PL_FRAMES and o['type'] == PLG_SPINBALL[0]: o['type'] = PLG_SPINBALL[1]
    for o in objs: o['fna'] = FNA_SUB.get(o['cls'], 0) | FNA_FLAGS.get((o['cls'], n), 0) | (o['layer'] << 8)   # FN_GIMA (fn_gima.c)
    for o in objs: o['s08'] = SCROLL08.get(n) == o.get('layer0')   # PL_ENEMIES: the instance's json layer scrolls at 0.8 (pl_enemy.c)
    objs += [{'type': 604, 'x': v, 'y': k, 'cls': 'fnparam', 'layer': 0, 'fna': 0} for k, v in FNA_PARAMS.get(n, ())]   # FN_GIMA parameters
    # ---- piece atlases
    ids = sorted({p['img'] for p in pieces})
    imgs = {i: load_img(src, i) for i in ids}
    # colour-aware split (session 17, Fortune Night): the pieces of a level share one 256-entry palette group unless the
    # palettizer spills whole atlases to another group. When the images need more colours than one group holds, they
    # are partitioned greedily into colour sets of at most PIECE_COLS colours (ARGB1555, as written) and each set gets
    # its own atlas(es), so palettize.py can move an atlas whole to a group with room. Levels that fit are unchanged.
    PIECE_COLS = 240
    def cols555(im):
        return {argb1555(bytes(px)) for px in im.getdata() if px[3] >= 128}
    for i in ids:                                  # a single image with more colours than a set: reduce it (lossy, reported)
        c = cols555(imgs[i][0])
        if len(c) > PIECE_COLS:
            im, hx, hy = imgs[i]; a = im.getchannel('A')
            q = im.convert('RGB').quantize(PIECE_COLS - 8, method=Image.Quantize.MEDIANCUT).convert('RGBA'); q.putalpha(a)
            imgs[i] = (q, hx, hy)
            report.append('piece image %d: %d colours reduced to %d (palette limit)' % (i, len(c), len(cols555(q))))
    ccache = {i: cols555(imgs[i][0]) for i in ids}
    allc = set().union(*ccache.values()) if ids else set()
    if len(allc) > PIECE_COLS:
        sets = []
        for i in sorted(ids, key=lambda i: -len(ccache[i])):
            area = lambda m: sum(imgs[j][0].width * imgs[j][0].height for j in m)   # Quick Backdrop blocks go to the emptier set (session 19)
            best = min(((0 if i >= QB_BASE else len(sc | ccache[i]) - len(sc), area(m) if i >= QB_BASE else 0, k) for k, (sc, m) in enumerate(sets) if len(sc | ccache[i]) <= PIECE_COLS), default=None)
            if best is None: sets.append((set(ccache[i]), [i]))
            else: sets[best[2]][0].update(ccache[i]); sets[best[2]][1].append(i)
        order = [sorted(m) for _, m in sets]
        report.append('pieces: %d colours, split into %d colour sets %s' % (len(allc), len(order), [len(sc) for sc, _ in sets]))
    else:
        order = [list(ids)]
    # images taller than 512 px fit only the first (1024 x 1024) atlas: their colour set goes first and they lead it
    # (Sky Battalion 1's 996 x 708 airship ship19_2075, session 19; no other level has one, so their order is unchanged)
    tall = lambda i: imgs[i][0].height > 512
    order = sorted([sorted(m, key=lambda i: (not tall(i), i)) for m in order], key=lambda m: not any(tall(i) for i in m))
    remaining = []; atlases = []; where = {}
    while order or remaining:
        if not remaining: remaining = order.pop(0)
        k = len(atlases)
        # the first atlas is up to 1024x1024; later ones at most 1024x512 (task 48: a 1024x1024 second atlas was
        # often only ~55-65 % used, wasting 384-512 KB of VRAM per level; the renderer takes up to MAX_ATLAS = 4)
        ppk = pack_skyline if n in NEWPACK_FRAMES else pack   # (session 21: Jade Creek's piece atlases with the skyline packer; other levels unchanged)
        cap = (lambda t: ppk([(i, imgs[i][0]) for i in t], 1024)) if k == 0 else (lambda t: ppk([(i, imgs[i][0]) for i in t], 1024, 512))
        pos = cap(remaining); take = list(remaining)
        if not pos:                              # largest prefix that fits (binary search)
            lo, hi = 0, len(remaining)
            while hi - lo > 1:
                mid = (lo + hi) // 2
                if cap(remaining[:mid]): lo = mid
                else: hi = mid
            take = remaining[:lo]; pos = cap(take) if take else None
        if not take: raise SystemExit('piece too large for the atlas')
        if len(take) == len(remaining) and not order:   # last atlas: the smallest power-of-two height that holds the rest
            for hh in (128, 256, 512):
                p2 = ppk([(i, imgs[i][0]) for i in take], 1024, hh)
                if p2: pos = p2; break
        atlas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
        for i in take:
            atlas.paste(imgs[i][0], pos[i]); where[i] = (k, pos[i][0], pos[i][1])
        bbox = atlas.getbbox() or (0, 0, 8, 8)
        atlas = atlas.crop((0, 0, max(bbox[2], 8), max(bbox[3], 8)))
        tw, th = write_tex(atlas, os.path.join(out, '%s_p%d.tex' % (name, k)))
        atlases.append((tw, th))
        remaining = [i for i in remaining if i not in take]
    nq = sum(1 for p in pieces if p.get('qb') and p.get('qb') != 'tile')
    report.append('pieces: %d placements, %d unique images, %d atlases %s' % (len(pieces) - sum(1 for p in pieces if p.get('qb') == 'tile'), len(ids), len(atlases), atlases)
                  + ('; %d tiled Quick Backdrops' % nq if nq else ''))

    # ---- collision rasterisation (alpha >= 128) on collision layers
    TW, TH = (W + 15) // 16, (H + 15) // 16
    solid = bytearray(TW * 16 * TH * 16)
    plat = bytearray(TW * 16 * TH * 16)
    stride = TW * 16
    masks = {}
    # invisible static obstacle objects (qualifier 8, no events: set_visible(0) in the constructor, pixel mask) are part of
    # the terrain: Sky Battalion's decks (SB1 has no collision backdrops at all). Rasterised like a mask backdrop at the
    # object's image top-left (x - hotspot), never drawn (session 19, spec_sb_flow; RE on_frame_36..39_init q8)
    bimgs = {}
    for o in fr['objects']:
        key = '%s_%d' % (o['cls'], o['id'])
        if key not in BAKE_SOLID or o['x'] is None or (o['layer'] or 0) not in col_layers: continue
        if key not in bimgs: bimgs[key] = load_img(src, first_anim_frames(an[key])['frames'][0])
        im, hx, hy = bimgs[key]
        pieces.append({'img': ('bake', key), 'x': o['x'] - hx, 'y': o['y'] - hy, 'layer': o['layer'], 'col': 1, 'box': False, 'bake': True})
        imgs[('bake', key)] = (im, 0, 0)
    def jc5_sub(p):   # JC5: the parked subtanker the act starts on (json layer 4, x < 1024: RV amb_l, Lilac walks on it at Y 1434 to X ~490;
        return n == 44 and p.get('layer0') == 4 and p['x'] < 1024   # the port collides with json layer 3 only; INFERRED: its pieces join the map)
    for p in pieces:
        if p['col'] not in (1, 2) or (p['layer'] not in col_layers and not jc5_sub(p)): continue
        im, hx, hy = imgs[p['img']]; hx = hy = 0   # backdrops ignore the image hotspot (RE Backdrop::draw adds it, draw_image subtracts it)
        mk = (p['img'], p['box'])
        if mk not in masks:
            masks[mk] = b'\x01' * (im.width * im.height) if p['box'] else im.getchannel('A').point(lambda v: 1 if v >= ALPHA_MIN else 0).tobytes()
        m = masks[mk]; x0 = p['x'] - hx; y0 = p['y'] - hy
        tgt = solid if p['col'] == 1 else plat
        for yy in range(p.get('ch', im.height)):
            Y = y0 + yy
            if Y < 0 or Y >= TH * 16: continue
            row = m[yy * im.width:yy * im.width + p.get('cw', im.width)]
            base = Y * stride
            for xx, v in enumerate(row):
                if v:
                    X = x0 + xx
                    if 0 <= X < stride: tgt[base + X] = 1
    pieces[:] = [p for p in pieces if not p.get('bake')]
    # dedupe 16x16 tiles into 32-byte bit patterns
    uniq = {bytes(32): 0, b'\xff' * 32: 1}
    table = [bytes(32), b'\xff' * 32]
    def tilemap(buf):
        out_map = []
        for ty in range(TH):
            for tx in range(TW):
                bits = bytearray(32)
                for yy in range(16):
                    base = (ty * 16 + yy) * stride + tx * 16
                    v = 0
                    for xx in range(16):
                        if buf[base + xx]: v |= 1 << xx
                    bits[yy * 2] = v & 255; bits[yy * 2 + 1] = v >> 8
                b = bytes(bits)
                if b not in uniq:
                    uniq[b] = len(table); table.append(b)
                out_map.append(uniq[b])
        return out_map
    smap = tilemap(solid); pmap = tilemap(plat)
    report.append('collision: %dx%d tiles, %d unique 16x16 patterns (%d bytes), layers %s' % (TW, TH, len(table), len(table) * 32, sorted(col_layers)))
    if n in MVL_FRAMES: report.append(write_mvl(os.path.join(out, name + '_mvl.bin'), n, pieces, imgs, objs, W, H))   # JC_BOSS: JC3's moving layer

    # ---- active-object sprite bank (first anim, right-facing) for implemented classes
    bank_classes = sorted({o['cls'] for o in objs if (o['type'] < 20 or 30 <= o['type'] < 37) and o['type'] not in (1, 31, 35) and o['cls'] not in OBJ_BANK_SKIP})   # >= 37: the _ens bank   # 20-27 parallax pieces; Ganseki uses its gan_* anims; the bridge box is invisible; OBJ_BANK_SKIP: drawn from the _ens bank
    bank = {'name': name + '_obj', 'anims': []}
    for key in bank_classes:
        d = first_anim_frames(an.get(key, {}))
        if not d or not d['frames'] or None in d['frames']:
            report.append('  no animation for %s' % key); continue
        bank['anims'].append({'name': key[:15], 'frames': d['frames'], 'speed': max(1, d['max_speed']),
                              'loop': d['loop'], 'back_to': d['back_to']})
        if ENGINE_TYPES.get(key.rsplit('_', 1)[0]) in BOX_TYPES: bank.setdefault('keep_box', set()).add(key[:15])
    for extra, nm in (('effectsexplosion01_367', 'explosion'),):
        d = first_anim_frames(an.get(extra, {}))
        if d: bank['anims'].append({'name': nm, 'frames': d['frames'], 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
    # specific (class, animation, direction) sequences driven by recovered event logic (RE_NOTES.md)
    for cls, ai, dk, nm in (('enemyturretus_878', 0, '0', 'turr_idle'), ('enemyturretus_878', 0, '16', 'turr_idle_l'),
                            ('enemyturretus_878', 7, '0', 'turr_shoot'), ('enemyturretus_878', 7, '16', 'turr_shoot_l'),
                            ('enemyturretus_878', 8, '0', 'turr_off'), ('enemyturretus_878', 8, '16', 'turr_off_l'),
                            ('enemypellet_934', 0, '0', 'pellet'), ('effectsexplosion02_368', 4, '0', 'explosion02'),
                            ('ringsparkle_142', 4, '0', 'ringsparkle'), ('effectsheart_444', 4, '0', 'heart'),
                            ('sparkle1_316', 0, '0', 'sparkle1'), ('sparkle5_284', 4, '0', 'sparkle5'),
                            ('crystal_447', 0, '0', 'pausecursor'),
                            # session 10 (spec_carol_bike A/C): fuel can, jump pad dark / lit / DV4 block pad, white burst
                            ('objectfuel_533', 0, '0', 'fuel'), ('objectjumppad_539', 0, '0', 'jpad'), ('objectjumppad_539', 0, '1', 'jpad_lit'),
                            ('objectjumppad_1104', 7, '0', 'jpad_blk'), ('effectswhiteburst_31', 4, '0', 'wburst'),
                            # visible attack shields (ShieldInstaShield_407 anim 4; RE spec_combat.md): whip, uppercut, crouch kick
                            ('shieldinstashield_407', 4, '0', 'slash'), ('shieldinstashield_407', 4, '16', 'slash_l'),
                            ('shieldinstashield_407', 4, '1', 'upslash'), ('shieldinstashield_407', 4, '17', 'upslash_l'),
                            ('shieldinstashield_407', 4, '31', 'ckick'), ('shieldinstashield_407', 4, '15', 'ckick_l'),
                            ('enemygansekijr_835', 0, '0', 'gan_pose'), ('enemygansekijr_835', 0, '16', 'gan_pose_l'),
                            ('enemygansekijr_835', 1, '0', 'gan_walk'), ('enemygansekijr_835', 1, '16', 'gan_walk_l'),
                            ('enemygansekijr_835', 8, '0', 'gan_dead'), ('enemygansekijr_835', 8, '16', 'gan_dead_l'),
                            ('enemygansekijr_835', 10, '0', 'gan_curl'), ('enemygansekijr_835', 10, '16', 'gan_curl_l'),
                            ('enemygansekijr_835', 11, '0', 'gan_wake'), ('enemygansekijr_835', 11, '16', 'gan_wake_l')):
        try:
            d = an[cls]['anims'][ai][dk]
        except (KeyError, IndexError, TypeError):
            report.append('  missing %s anim %d dir %s' % (cls, ai, dk)); continue
        bank['anims'].append({'name': nm, 'frames': d['frames'], 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
    # ---- HUD (recovered GUI classes; positions measured in the original at 426x240, see RUNTIME_MAP "HUD")
    HUD = [('hud_frame', [3784, 12760]),                        # guimeters frame (normal, red flash)
           ('hud_fill', an['guienergy_127']['anims'][1]['0']['frames']),  # fill; anim 1 = shine loop when full
           ('hud_petals', an['guirings_125']['images']),        # counter 0..14 half-petals
           ('hud_icon', [311]),                                 # guiicon anim 0 = Lilac head + crystal
           ('hud_icon_c', [112]), ('hud_icon_cb', [395]),       # guiicon directions 1 / 2: Carol, Carol on the bike (session 10)
           ('hud_icon_m', [507]),                               # guiicon direction 4: Milla (session 11)
           ('hud_clock', an['guitimer_536']['anims'][0]['0']['frames']),
           ('hud_digits', an['guilives_425']['images'][:10]),   # counter digits 0-9 (lives/crystals/timer)
           ('card_stage', [an['levelcardintro3_684']['anims'][0]['0']['frames'][k] for k in stage_card(n)[1]]),   # "STAGE n" (frame 1: Carol / Milla variant, Thermal Base)
           ('card_name', [an['levelcardintro2_683']['anims'][0]['0']['frames'][stage_card(n)[0]]]),    # the stage name (STAGE_CARDS)
           ('card_fade', [129]), ('card_gameover', [27]),
           ('pause', an['guipause_347']['anims'][0]['0']['frames']),               # Paused banner + options
           ] + ([('hud_air', an['guiair_700']['images'])] if n in WATER_FRAMES else []) + ([('res_complete', an['levelcardactclear1_864']['anims'][7]['0']['frames']),  # "Stage Complete!" build-in
           ('res_results', [955]), ('res_panel', [10860]), ('res_rows', an['levelcardactclear3_866']['anims'][7]['0']['frames']),   # 6-frame reveal (RE 4570-4573)
           ('res_bar', [10844, 10660]),
           ('res_complete_t', [-8005]), ('res_panel_t', [-10860]), ('res_bar_t', [-10844, -10660]),   # alpha-87 layers (RV screenshots)
           ('res_digits', an['totalbonus_870']['images'][:10]),
           ('res_small', an['levelcardbonus3_869']['images'][:10])] if any(o['cls'] == 'levelcardactclear1' for o in fr['objects']) else [])   # results: every frame that places the clear card (DV4, AT4, later stages)
    for nm, frames in HUD:
        bank['anims'].append({'name': nm, 'frames': frames, 'speed': 0, 'loop': -1, 'back_to': 0})
    write_bank2(src, bank, out, report)
    # ---- enemies / bosses / gimmicks bank (cropped; their collision uses masks relative to the hotspot)
    placed = {o['cls'].rsplit('_', 1)[0] for o in fr['objects']}
    if n in NEWPACK_FRAMES:   # Jade Creek / Trap Hideout (session 21): an off-map template of a JC module's own class does not pull its rows in (jc5 holds
        # unused off-map BullFrag / water-stream / slab templates; their rows overflowed jc5_ens past two atlases). Runtime-created
        # children still come in through ENS_WITH from their on-map creators.
        _jct = {c for (c, h), t in ENGINE_TYPES_ID.items() if 1280 <= t < 3200}
        placed = {o['cls'].rsplit('_', 1)[0] for o in fr['objects']
                  if o['cls'] not in _jct or (o['x'] is not None and 0 <= o['x'] < fr['width'] and 0 <= o['y'] < fr['height'])}
    if any(o['cls'].startswith('monitor') and o['cls'].endswith('shield') and o['x'] >= 0 for o in fr['objects']): placed.add('shieldmonitor_onmap')
    if n in JCB_AMBUSH_FRAMES and 'npcneera' in placed: placed |= JCB_AMBUSH_WITH   # JC_BOSS: the ice ambush's off-map rig / ice block (above)
    # Lilac's own shields (spec_shields 0): every stage frame places the six off-map q21 instances and runs 2673-2705 (RE,
    # build/re/f17..f29_calls), and OTHERS.B is global, so a shield carried into an act without an on-map shield monitor is
    # still drawn there. Those acts get the shield rows in a separate bank <name>_shl, loaded only when a shield is carried
    # in (src/game/shields.c; it would grow the _ens atlas, e.g. FN3 +512 KB of VRAM): session 18
    shl = {'name': name + '_shl', 'anims': []} if ({'shieldwood', 'shieldearth', 'shieldwater', 'shieldmagnetic', 'shieldflamefront', 'shieldflameback'} <= placed
                                                 and 'shieldmonitor_onmap' not in placed) else None
    if n in WATER_FRAMES: placed.add('water_level')   # DV2 / DV3: MasterLevel A = 1 (5109 / 5314); AT1 / AT2 / AT4 (4309 / 4478 / 4685)
    ens = {'name': name + '_ens', 'anims': [], 'keep_box': set()}
    placed_handles = {}
    for o in fr['objects']: placed_handles.setdefault(o['cls'], set()).add(o['id'])
    for e in ENS:
        cls, ai, dk, nm = e[:4]
        if nm in ROW_FRAMES and n not in ROW_FRAMES[nm]: continue      # row-level frames (session 19b)
        if nm in ROW_FRAMES_OLD and n not in ROW_FRAMES_OLD[nm]: continue
        if nm == 'warning' and n in SB_FRAMES: continue                  # DV's EffectWarning_1100 row: its class name matches SB's 1528 (a duplicate)
        if cls in ENS_FRAMES and n not in ENS_FRAMES[cls] and nm not in ROW_FRAMES: continue
        if len(e) > 5 and not any(w in placed for w in e[5]): continue   # rows tied to their creators (FN_ENEMIES)
        base = cls.rsplit('_', 1)[0]
        dst = ens
        if cls in ENS_ONLY_WITH:                     # off-map templates do not count (shields: only with an on-map shield monitor)
            if shl is not None and cls in SHL_CLASSES: dst = shl   # ... else in the act's separate shield bank (session 18)
            elif not any(w in placed for w in ENS_WITH.get(cls, ())): continue
        elif base not in placed and not any(w in placed for w in ENS_WITH.get(cls, ())): continue
        # the frame's own object (class + handle): when the row names a handle this frame does not place but the frame
        # places the class under another handle (another stage's object: its own images), the sprite comes from the
        # placed handle (session 14; e.g. AT4's spikes 671, DV3's wall plate 1029 and door 1066, RM's spikes 1106-1109)
        cls = ROW_HANDLE_SUB.get(n, {}).get(cls, cls)                   # created children with their own handle in this frame (session 19b)
        hs = placed_handles.get(base, set())
        if hs and int(cls.rsplit('_', 1)[1]) not in hs:
            for h2 in sorted(hs):
                try: an['%s_%d' % (base, h2)]['anims'][ai][dk]; cls = '%s_%d' % (base, h2); break
                except (KeyError, IndexError, TypeError): pass
        try: d = an[cls]['anims'][ai][dk]
        except (KeyError, IndexError, TypeError): report.append('  ens: missing %s anim %d dir %s' % (cls, ai, dk)); continue
        fr_ = d['frames'][e[4]] if len(e) > 4 and e[4] is not None else d['frames']
        dst['anims'].append({'name': nm, 'frames': fr_, 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
    if shl and shl['anims']: write_bank2(src, shl, out, report); report.append('shield bank: %s' % [a['name'] for a in shl['anims']])
    for cls, nm, frames in FNA_ENS_IMG:   # FN_GIMA: SystemBox images (one frame, no animation table)
        if cls.rsplit('_', 1)[0] in placed: ens['anims'].append({'name': nm, 'frames': frames, 'speed': 0, 'loop': -1, 'back_to': 0})
    if ens['anims']: write_bank2(src, ens, out, report)
    report.append('ens bank: %s' % [a['name'] for a in ens['anims']])
    # parallax (RE + RV: build/re/spec_parallax.md): layer-0 SystemBox pieces, tiled, moved every frame by
    # sx = fmod(trunc(D*vl + F - vl), A), sy = trunc(E*vt + C) - vt. Images are split into power-of-two chunks
    # (each <= 16 colours: palettize --pal4 makes them 4 bpp with one 16-entry block per image).
    par = export_parallax(fr, src, name, out, an)
    report.append('parallax pieces: %s' % par)

    # ---- Lilac (playerskinsash_341) from recovered animation tables, both facings
    # Animation numbers RUNTIME-VERIFIED from original traces (sprite image -> images_* table),
    # except hurt/defeat (still by eye). See RUNTIME_MAP.md "Lilac animations".
    LILAC = [('idle', 0), ('walk', 1), ('run', 2), ('fastrun', 23), ('jump', 24), ('lookup', 11),
             ('crouch', 10), ('skid', 16), ('attack', 26), ('airatk', 25), ('uppercut', 28),
             ('dkwind', 7), ('divekick', 19), ('cyclone', 18), ('charge', 7), ('boost', 13),
             ('hurt', 8), ('defeat', 38), ('crouchatk', 30), ('spring', 14), ('hang', 15),
             ('push', 12), ('climb', 9), ('climbdown', 35), ('climbover', 36), ('catch', 33),
             ('cork', 32), ('cork2', 31), ('swim', 39), ('shake', 27), ('hangmove', 17)]   # hangmove: OTHERS.Y 42 (RE 2462; FN clotheslines 7207 / 7208)   # cork: RV cork_dv1 (M45, OTHERS.Y 45 -> skin 32, 46 -> 31); RV push_wall / ladder_* traces, RE events 2456-2467
    la = an['playerskinsash_341']['anims']
    lb = {'name': 'lilac2', 'anims': []}
    def mirrored(d0, d16):
        """direction-16 frames that are exact mirrors of direction 0 (with hotspot w - 1 - hx) are left out:
        the port draws the right-facing animation flipped (halves Lilac's VRAM)"""
        if not d16 or len(d0['frames']) != len(d16['frames']): return False
        for i0, i1 in zip(d0['frames'], d16['frames']):
            a0, hx0, hy0 = load_img(src, i0); a1, hx1, hy1 = load_img(src, i1)
            if a0.size != a1.size or hy0 != hy1 or hx1 != a0.width - 1 - hx0: return False
            if ImageChops.difference(ImageOps.mirror(a0), a1).getbbox() is not None: return False
        return True
    for nm, ai in LILAC:
        for dkey, suf in (('0', ''), ('16', '_l')):
            if dkey == '16' and (not la[ai].get('16') or mirrored(la[ai]['0'], la[ai]['16'])): continue
            dd = la[ai].get(dkey) or la[ai]['0']
            lb['anims'].append({'name': nm + suf, 'frames': dd['frames'], 'speed': dd['max_speed'],
                                'loop': dd['loop'], 'back_to': dd['back_to']})
    # Lilac's hair overlay (PlayerSkinSashHair_424, RV hair_cyc_boost probe): same animation id as the skin,
    # its own frames and speeds, same position and angle, drawn behind the skin
    lb['anims'] += fnb_pose_rows(la, n)   # FN_GIMB poses (A4 / A5 only)
    write_bank2(src, lb, out, report)
    hb = {'name': 'lilachair', 'anims': []}
    ha = an['playerskinsashhair_424']['anims']
    for nm, ai in LILAC:
        if ai >= len(ha) or not ha[ai]: continue
        for dkey, suf in (('0', ''), ('16', '_l')):
            h0 = ha[ai].get('0') or next(iter(ha[ai].values()))
            if dkey == '16' and (not ha[ai].get('16') or mirrored(h0, ha[ai]['16'])): continue
            dd = ha[ai].get(dkey) or h0
            hb['anims'].append({'name': 'h_' + nm + suf, 'frames': dd['frames'], 'speed': dd['max_speed'],
                                'loop': dd['loop'], 'back_to': dd['back_to']})
    hb['anims'] += fnb_pose_rows(ha, n, 'h_')   # FN_GIMB poses (A4 / A5 only)
    write_bank2(src, hb, out, report)
    # Carol (session 10): her skin, tail and bike-riding skin, every animation index the class has, named 'a<index>'
    # ('_l' = direction 16 when it is not a pixel mirror of direction 0). The game loads them only when Carol plays.
    for cls, bname in (('playerskincarol_449', 'carol2'), ('playerskincaroltail_448', 'caroltail'), ('playerskincarolbike_460', 'carolbike')):
        ca = an[cls]['anims']; cb = {'name': bname, 'anims': []}
        for ai, entry in enumerate(ca):
            if not entry: continue
            d0 = entry.get('0') or next(iter(entry.values()))
            for dkey, suf in (('0', ''), ('16', '_l')):
                if dkey == '16' and (not entry.get('16') or mirrored(d0, entry['16'])): continue
                dd = entry.get(dkey) or d0
                cb['anims'].append({'name': 'a%d%s' % (ai, suf), 'frames': dd['frames'], 'speed': dd['max_speed'],
                                    'loop': dd['loop'], 'back_to': dd['back_to']})
            # extra directions the events select: Wild Kick in the air (2425: direction + 1 -> 1 / 17) and the bored idle
            # (1735: direction + 4 -> 4 / 20), named 'a<index>_d<direction>'
            for dkey in {21: ('1', '17'), 0: ('4', '20'), **POSE_DIRS.get(n, {}), **CAROL_POSE_DIRS.get(n, {})}.get(ai, ()):   # + the stages' poses
                if dkey in entry and not (dkey == '17' and mirrored(entry.get('1', d0), entry[dkey])):
                    dd = entry[dkey]
                    cb['anims'].append({'name': 'a%d_d%s' % (ai, dkey), 'frames': dd['frames'], 'speed': dd['max_speed'],
                                        'loop': dd['loop'], 'back_to': dd['back_to']})
        if bname == 'carol2':
            # Carol-only objects ride in her bank (loaded only when she plays): attack shields (ShieldInstaShield_407 anim 4,
            # spec_carol_combat 2.2), the parked bike (ObjectBike_459) and the bike-wreck debris (EffectsDebris_427 frames 47-50)
            for dk in ('2', '3', '4', '18', '19', '20', '14', '30', '24'):
                dd = an['shieldinstashield_407']['anims'][4][dk]
                cb['anims'].append({'name': 'sh%s' % dk, 'frames': dd['frames'], 'speed': dd['max_speed'], 'loop': dd['loop'], 'back_to': dd['back_to']})
            for dk, suf in (('0', ''), ('16', '_l')):
                dd = an['objectbike_459']['anims'][0][dk]
                cb['anims'].append({'name': 'pbike' + suf, 'frames': dd['frames'], 'speed': dd['max_speed'], 'loop': dd['loop'], 'back_to': dd['back_to']})
            dd = an['effectsdebris_427']['anims'][0]['0']
            cb['anims'].append({'name': 'bdebris', 'frames': dd['frames'][47:51], 'speed': 0, 'loop': -1, 'back_to': 0})
            dd = an['effectsdust01_369']['anims'][4]['0']                   # bike exhaust (2129)
            cb['anims'].append({'name': 'bdust', 'frames': dd['frames'], 'speed': dd['max_speed'], 'loop': dd['loop'], 'back_to': dd['back_to']})
        write_bank2(src, cb, out, report)
    # Milla (session 11): skin and tail, every animation index and every direction that is not a pixel mirror of
    # direction - 16 (the port flips): 'a<index>' (dir 0), 'a<index>_l' (dir 16), 'a<index>_d<dir>' (others).
    # millafx: PlayerReflect_463 (the cube / shield / bursts / beam, spec_milla_combat 1), 11 colours -> 4 bpp
    for cls, bname in (('playerskinmilla_461', 'milla2'), ('playerskinmillatail_462', 'millatail'), ('playerreflect_463', 'millafx')):
        ma = an[cls]['anims']; mb = {'name': bname, 'anims': []}
        for ai, entry in enumerate(ma):
            if not entry: continue
            for dkey in sorted(entry, key=int):
                d = int(dkey); dd = entry[dkey]
                if d >= 16 and str(d - 16) in entry and mirrored(entry[str(d - 16)], dd): continue
                nm = 'a%d' % ai if d == 0 else 'a%d_l' % ai if d == 16 else 'a%d_d%d' % (ai, d)
                mb['anims'].append({'name': nm, 'frames': dd['frames'], 'speed': dd['max_speed'], 'loop': dd['loop'], 'back_to': dd['back_to']})
        if bname == 'milla2':
            dd = an['playerdig_464']['anims'][0]['0']                     # the dirt mound of the dig (M27), frame = int(B / 24)
            mb['anims'].append({'name': 'dig', 'frames': dd['frames'], 'speed': 0, 'loop': 1, 'back_to': 0})
            # her HUD panel (GUIScoreTimeRings_345 direction 4; 2197-2201 pick the frame), kept in her bank (loaded only for her)
            mb['anims'].append({'name': 'hudm', 'frames': an['guiscoretimerings_345']['anims'][0]['4']['frames'], 'speed': 0, 'loop': 1, 'back_to': 0})
            mb['patch'] = {i: dc_hud_c_to_x for i in an['guiscoretimerings_345']['anims'][0]['4']['frames']}   # "C" -> "X" (D-110)
        write_bank2(src, mb, out, report)
    write_lv2(os.path.join(out, name + '.lv2'), W, H, pieces, where, imgs, objs, TW, TH, table, smap, pmap, len(atlases), bank, bool(LMAP))
    report.append('objects exported: %d (types %s)' % (len(objs), sorted({o['type'] for o in objs})))
    report.append('skipped classes (not implemented / helpers): %s' % sorted(skipped.items(), key=lambda kv: -kv[1])[:40])
    open(os.path.join(out, name + '_report.txt'), 'w').write('\n'.join(report) + '\n')
    print('\n'.join(report))

def write_bank2(src, bank, out, report):
    """'FPS2' u16 nframes, nanims, texw, texh; frames {u16 u,v,w,h; s16 hx,hy};
       anims {char name[16]; u16 first, count; u8 speed(Fusion 0-100), u8 back_to; s16 loop}; u16 seq[]"""
    ids = []
    for a in bank['anims']:
        for i in a['frames']:
            if i not in ids: ids.append(i)
    imgs = {i: load_img(src, i) for i in ids}
    for i, fn in bank.get('patch', {}).items():          # reproducible image edits (dc_hud_c_to_x), before any crop
        im, hx, hy = imgs[i]; imgs[i] = (fn(im), hx, hy)
    # images of these animations are cropped to their opaque pixels (hotspot shifted to match): drawing
    # is unchanged, and nothing uses their untrimmed box (explosions: effects; Ganseki: hotspot rules +
    # pixel masks). Keeps the 1024x1024 object atlas within one PVR texture.
    # Lilac's skin and hair banks too: her collision is the separate sensor objects, never the sprite box
    whole = bank['name'] in ('lilac2', 'lilachair', 'carol2', 'caroltail', 'carolbike', 'milla2', 'millatail', 'millafx')
    keep = bank.get('keep_box', set())
    if bank['name'].endswith(('_ens', '_shl')):
        crop_ids = {i for a in bank['anims'] for i in a['frames']}
    elif bank['name'].endswith('_obj'):
        keep_ids = {i for a in bank['anims'] if a['name'] in keep for i in a['frames']}
        keep_ids |= {i for a in bank['anims'] if a['name'].startswith(('hud_', 'res_', 'pellet')) for i in a['frames']}   # drawn by top-left; pellet: whole image box (RV)
        crop_ids = {i for a in bank['anims'] for i in a['frames']} - keep_ids
    else:
        crop_ids = {i for a in bank['anims'] if whole or a['name'].startswith(('gan_', 'explosion')) for i in a['frames']}
        # the parked bike (ObjectBike_459) collides with its whole image box (initialize_active(1)): keep it (RV b_remount)
        crop_ids -= {i for a in bank['anims'] if a['name'].startswith('pbike') for i in a['frames']}
    # session 20: overlay-only animations keep the top rows of their images (the Kujacker feather's eye: the shaft is the dark
    # base feather's, drawn under it), so the glow directions cost a few KB instead of a second-atlas doubling
    for a in bank['anims']:
        rows = TOP_ROWS.get(a['name'])
        if not rows: continue
        others = {i for b in bank['anims'] if b['name'] not in TOP_ROWS for i in b['frames']}
        for i in a['frames']:
            if i in others or i not in imgs: continue
            im, hx, hy = imgs[i]; im = im.copy(); px = im.load()
            for y in range(rows, im.height):
                for x in range(im.width): px[x, y] = (0, 0, 0, 0)
            imgs[i] = (im, hx, hy)
    crop_off = {}
    for i in crop_ids:
        im, hx, hy = imgs[i]; bb = im.getbbox()
        if bb and (bb[2] - bb[0], bb[3] - bb[1]) != im.size:
            imgs[i] = (im.crop(bb), hx - bb[0], hy - bb[1]); crop_off[i] = (bb[0], bb[1])
    if os.environ.get('FP_ENS_AREAS') and bank['name'].endswith(('_ens', '_obj')):   # diagnostics: the largest anims of an _ens / _obj bank
        ar = {}
        for a_ in bank['anims']:
            ar[a_['name']] = sum(imgs[i][0].width * imgs[i][0].height for i in set(a_['frames']) if i in imgs)
        print('ens areas', bank['name'], sum(ar.values()), sorted(ar.items(), key=lambda t: -t[1])[:40])
    pk = pack_skyline if bank['name'][:2] in ('jc', 'th', 'tb', 'pl', 'bg', 'fd') and bank['name'].endswith('_ens') else pack   # (session 21: JC _ens only, others unchanged)
    size, pos, second = 64, None, []
    while not pos:          # smallest power-of-two atlas: square, then half height (PVR textures may be 2:1)
        pos = pk([(i, imgs[i][0]) for i in ids], size, size // 2) or pk([(i, imgs[i][0]) for i in ids], size)
        if not pos:
            size *= 2
            if size > 1024:
                if not bank['name'].endswith('_ens'): raise SystemExit('bank %s too large' % bank['name'])
                # _ens only: a second 1024 atlas (<name>_2.tex); its frames carry v | 0x8000 (assets.c bank_load)
                size = 1024; lo, hi = 0, len(ids)       # the longest prefix that fits the first atlas (binary search; session 21: the old
                # 7/8 steps could leave a whole 1024 page for the rest, e.g. jc5). JC frames take the largest images first, so the
                # second page holds small ones and crops to 1024 x 512 (JC2 / JC3 / JC5: -512 KB of VRAM each)
                jcb = bank['name'][:2] in ('jc', 'th', 'tb', 'pl', 'bg', 'fd')
                order = sorted(ids, key=lambda i: -imgs[i][0].width * imgs[i][0].height) if jcb else list(ids)
                if jcb:                                # greedy fill of the first page, the better of two orders (largest area / tallest first)
                    best_ = None
                    for ok_ in (lambda kv: (-kv[1].width * kv[1].height, kv[0]), lambda kv: (-kv[1].height, -kv[1].width, kv[0])):
                        p_ = pack_skyline_fill([(i, imgs[i][0]) for i in ids], 1024, 1024, ok_)
                        a_ = sum(imgs[i][0].width * imgs[i][0].height for i in p_)
                        if best_ is None or a_ > best_[0]: best_ = (a_, p_)
                    pos = best_[1]; take = [i for i in ids if i in pos]
                else:                                  # every other level: the original 7/8 steps (their data unchanged)
                    take = list(ids)
                    while take:
                        pos = pack([(i, imgs[i][0]) for i in take], 1024)
                        if pos: break
                        take = take[:len(take) * 7 // 8]
                second = [i for i in ids if i not in take]
                if os.environ.get('FP_ENS_AREAS'): print('atlas split', bank['name'], 'first', sum(imgs[i][0].width * imgs[i][0].height for i in take), 'second', sum(imgs[i][0].width * imgs[i][0].height for i in second), 'max h', max([imgs[i][0].height for i in second] or [0]))
                pos2 = None; s2 = 64
                while not pos2:
                    pos2 = pk([(i, imgs[i][0]) for i in second], s2, s2 // 2) or pk([(i, imgs[i][0]) for i in second], s2)
                    if not pos2 and jcb:                 # (JC: the greedy fill with either order, all of them placed)
                        for hh_ in (s2 // 2, s2):
                            for ok_ in (lambda kv: (-kv[1].width * kv[1].height, kv[0]), lambda kv: (-kv[1].height, -kv[1].width, kv[0]), lambda kv: (-kv[1].width, -kv[1].height, kv[0])):
                                p_ = pack_skyline_fill([(i, imgs[i][0]) for i in second], s2, hh_, ok_)
                                if len(p_) == len(second): pos2 = p_; break
                            if pos2: break
                    if not pos2:
                        s2 *= 2
                        if s2 > 1024: raise SystemExit('bank %s too large for two atlases' % bank['name'])
                if os.environ.get('FP_ENS_AREAS'): print('second page', bank['name'], s2, len(second), max(imgs[i][0].width for i in second), sorted(imgs[i][0].size for i in second)[-5:])
                at2 = Image.new('RGBA', (s2, s2), (0, 0, 0, 0))
                for i in second: at2.paste(imgs[i][0], pos2[i])
                bb2 = at2.getbbox() or (0, 0, 8, 8)
                write_tex(at2.crop((0, 0, s2, max(8, bb2[3]))), os.path.join(out, bank['name'] + '_2.tex'))
                for i in second: pos[i] = (pos2[i][0], pos2[i][1] | 0x8000)
                report.append('bank %s: second atlas %d (%d frames)' % (bank['name'], s2, len(second)))
    # Session 10 perf (D-038): a content height just over a power of two (carol2 574 rows -> a 1024x1024 texture,
    # 450 KB of it empty; the DV1-DV3 object banks reached 552 rows when Carol's fuel can / pads were added) is split
    # into a half-height first atlas and a small second one (<name>_2.tex, frames v | 0x8000, the _ens mechanism).
    # Drawing is unchanged (bank_draw / bank_frame_tex); only the VRAM layout differs (Lilac's banks too since session 11).
    if not second and (bank['name'] in ('lilac2', 'lilachair', 'carol2', 'carolbike', 'caroltail', 'milla2', 'millatail', 'millafx') or bank['name'].endswith(('_obj', '_ens'))):
        def ph(p):
            return max(((y + imgs[i][0].height) for i, (x, y) in p.items()), default=8)
        def p2(n):
            v = 8
            while v < n: v *= 2
            return v
        P = p2(ph(pos)); cur = size * P
        pks = pk if bank['name'][:2] == 'pl' else pack   # Pangu Lagoon (session 23): the skyline packer for the half-height split too (a
                                                          # 1024 x 1024 _ens became 1024 x 512 + a small second page); other stages unchanged
        if P >= 256:
            order = [i for i, _ in sorted(((i, imgs[i][0]) for i in ids), key=lambda kv: (-kv[1].height, kv[0]))]
            lo, hi = 0, len(order)                 # the shelf packer places in this order: the fitting prefix is monotone
            while lo < hi:
                m = (lo + hi + 1) // 2
                if pks([(i, imgs[i][0]) for i in order[:m]], size, P // 2): lo = m
                else: hi = m - 1
            take, rest = order[:lo], order[lo:]
            if bank['name'][:2] == 'pl':                 # (PL: the greedy fill of the half page, the better of two orders, as the two-page split)
                best_ = None
                for ok_ in (lambda kv: (-kv[1].width * kv[1].height, kv[0]), lambda kv: (-kv[1].height, -kv[1].width, kv[0])):
                    p_ = pack_skyline_fill([(i, imgs[i][0]) for i in ids], size, P // 2, ok_)
                    a_ = sum(imgs[i][0].width * imgs[i][0].height for i in p_)
                    if best_ is None or a_ > best_[0]: best_ = (a_, p_)
                take = [i for i in ids if i in best_[1]]; rest = [i for i in ids if i not in best_[1]]
            if take and rest:
                pos1 = best_[1] if bank['name'][:2] == 'pl' else pks([(i, imgs[i][0]) for i in take], size, P // 2)
                pos2, s2 = None, 64
                while not pos2 and s2 <= 1024:
                    pos2 = pks([(i, imgs[i][0]) for i in rest], s2, s2 // 2) or pks([(i, imgs[i][0]) for i in rest], s2)
                    if not pos2: s2 *= 2
                if pos2:
                    w2 = s2; h2 = p2(ph(pos2))
                    if size * (P // 2) + w2 * h2 < cur:
                        at2 = Image.new('RGBA', (s2, s2), (0, 0, 0, 0))
                        for i in rest: at2.paste(imgs[i][0], pos2[i])
                        write_tex(at2.crop((0, 0, s2, max(8, ph(pos2)))), os.path.join(out, bank['name'] + '_2.tex'))
                        pos = dict(pos1); second = list(rest)
                        for i in rest: pos[i] = (pos2[i][0], pos2[i][1] | 0x8000)
                        report.append('bank %s: split %dx%d -> %dx%d + %dx%d (%d frames in the second)' % (
                            bank['name'], size, P, size, P // 2, w2, h2, len(rest)))
    atlas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    # (atlas height is cropped to content by write_tex's pow2 rounding below)
    for i in ids:
        if i not in second: atlas.paste(imgs[i][0], pos[i])
    bb = atlas.getbbox() or (0, 0, 8, 8)
    atlas = atlas.crop((0, 0, size, max(8, bb[3])))
    write_tex(atlas, os.path.join(out, bank['name'] + '.tex'))
    data = bytearray(b'FPS2' + struct.pack('<4H', len(ids), len(bank['anims']), size, size))
    for i in ids:
        im, hx, hy = imgs[i]
        data += struct.pack('<4H2h', pos[i][0], pos[i][1], im.width, im.height, hx, hy)
    seq, first = [], 0
    for a in bank['anims']:
        data += a['name'].encode()[:15].ljust(16, b'\0') + struct.pack('<2H2Bh', first, len(a['frames']), min(255, a['speed']), min(255, a['back_to']), a['loop'])
        seq += [ids.index(i) for i in a['frames']]; first += len(a['frames'])
    data += struct.pack('<%dH' % len(seq), *seq)
    # collision masks (Chowdren: image collision bit = alpha != 0): 'MSK1', then per frame rows of
    # ceil(w / 8) bytes, bit 7 of byte 0 = leftmost pixel
    if bank['name'] in ('milla2',) or bank['name'].endswith('_ens'):   # session 14: enemies spawn at their action points (spec_rm_enemies)
        # 'ACT1': per-frame action point relative to the (cropped) image (img meta 3rd/4th values): the held cube sits at
        # MILLA's action point (spec_milla_combat 3.3)
        data += b'ACT1'
        for i in ids:
            m = [int(v, 0) for v in open(os.path.join(src, 'img_%d_meta.txt' % i)).read().split()] if i >= 0 else [0, 0, 0, 0]
            ax, ay = [v - 65536 if v > 32767 else v for v in m[2:4]]
            ox, oy = crop_off.get(i, (0, 0))
            data += struct.pack('<2h', ax - ox, ay - oy)
    data += b'MSK1'
    for i in ids:
        im = imgs[i][0]; a = im.getchannel('A'); w, h = im.size
        for y in range(h):
            row = bytearray((w + 7) // 8)
            for x in range(w):
                if a.getpixel((x, y)): row[x >> 3] |= 0x80 >> (x & 7)
            data += row
    open(os.path.join(out, bank['name'] + '.spr'), 'wb').write(data)
    report.append('bank %s: %d frames, %d anims, atlas %d' % (bank['name'], len(ids), len(bank['anims']), size))

def write_lv2(path, W, H, pieces, where, imgs, objs, TW, TH, table, smap, pmap, natlas, bank, mapped=False):
    """'FPL2' u32 w, h (px); u16 npieces, nobj, natlas, nuniq; u16 tw, th; u32 pad
       piece {s32 x, y (top-left); u16 u, v, w, h; u8 atlas, layer, col, pad}  (20 B); pad bit 2: a tiled piece (the tile
             repeats over the size given by the next record's x, y; that record has pad 0x81; session 19 Quick Backdrops)
       obj   {u16 type, anim (index in <name>_obj bank or 0xffff); s32 x, y}  (12 B)
       uniq tiles: 32 B each (16 rows of u16, bit x = pixel x)
       solid map u16[tw*th]; platform map u16[tw*th]"""
    pieces = sorted(pieces, key=lambda p: (p['layer'], p['layer0']))   # stable: keeps the frame's instance (z) order inside a layer (mapped layers keep their original order)
    names = [a['name'] for a in bank['anims']]
    nrec = sum(0 if p.get('qb') == 'tile' else 2 if p.get('qb') else 1 for p in pieces)   # a tiled Quick Backdrop: 2 records
    d = bytearray(b'FPL2' + struct.pack('<2I4H2HI', W, H, nrec, len(objs), natlas, len(table), TW, TH, 0))
    for p in pieces:
        im, hx, hy = imgs[p['img']]; hx = hy = 0   # backdrop pieces: drawn at the instance x, y (RE Backdrop::draw)
        k, u, v = where[p['img']]
        qb = p.get('qb')
        if qb == 'tile': continue                   # the other tiles of a tiled Quick Backdrop (collision only)
        fl = (1 if p['invisible'] else 0) | p.get('scroll08', 0)
        if qb:                                      # tiled piece: the tile rect, pad bit 2, then a size record {s32 w, h; 0...; pad 0x80}
            d += struct.pack('<2i4H4B', p['x'] - hx, p['y'] - hy, u, v, im.width, im.height, k, p['layer'], p['col'], fl | 4)
            d += struct.pack('<2i4H4B', qb[0], qb[1], 0, 0, 0, 0, k, p['layer'], 0, 0x81)
            continue
        d += struct.pack('<2i4H4B', p['x'] - hx, p['y'] - hy, u, v, p.get('cw', im.width), p.get('ch', im.height), k, p['layer'], p['col'], fl)
    for o in objs:
        a = names.index(o['cls'][:15]) if o['cls'][:15] in names else (o.get('par_index', 0xffff) if 20 <= o['type'] < 28 else 0xffff)
        if 139 <= o['type'] <= 143 or o['type'] in (169, 170, 187, 204): a = o.get('layer', 2)   # waterfalls, AT water jets / crushers / slime floor: the instance's layer
        if 300 <= o['type'] < 350: a = RMB_SUB.get(o['cls'], 0)   # Relic Maze acts 3-5 families: the member index (relic_gimb.c)
        if 560 <= o['type'] < 680: a = o.get('fna', 0)            # FN_GIMA: member index | act flags | layer << 8 (fn_gima.c)
        if 680 <= o['type'] < 800: a = FNB_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # FN_GIMB (fn_gimb.c): member index | instance layer << 8
        if 800 <= o['type'] < 860: a = FNBOSS_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # FN_BOSS (fn_boss.c): sub id | instance layer << 8
        if 1430 <= o['type'] < 1480: a = o['layer'] << 8   # JC_BOSS (jc_boss.c): instance layer << 8
        if 1150 <= o['type'] < 1280: a = SBBOSS_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # SB_BOSS (sb_boss.c): sub id | instance layer << 8
        if 1280 <= o['type'] < 1536: a = JC_SUB.get(o['cls'], 0) | (o['layer'] << 8) | (0x8000 if mapped and o.get('layer0') == 1 else 0)   # Jade Creek (jc_*.c): member index | instance layer << 8 | 0x8000 json layer 1 of a mapped frame
        if 1620 <= o['type'] < 1680: a = THG_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # TH_GIM (th_gim.c): member index | instance layer << 8
        if 1888 <= o['type'] < 1968: a = TBG_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # TB_GIM (tb_gim.c): member index | instance layer << 8
        if 2144 <= o['type'] < 2224: a = PLG_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # PL_GIM (pl_gim.c): member index | instance layer << 8
        if 2400 <= o['type'] < 2480: a = BGG_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # BG_GIM (bg_gim.c): member index | instance layer << 8
        if 2560 <= o['type'] < 2570: a = o['layer'] << 8   # FD_FLOW (fd_flow.c): the loop pieces' instance layer << 8
        if 1370 <= o['type'] < 1430: a = (JCG_LEAF_TILT.get((o['x'], o['y']), 0) // 30 if o['type'] == 1371 else JCG_SUB.get(o['cls'], 0)) | (o['layer'] << 8)   # JC_GIM (jc_gim.c): member index (leaf backs: tilt / 30) | instance layer << 8
        if 1536 <= o['type'] < 1560: a = (o['layer'] << 8) | (0x8000 if mapped and o.get('layer0') == 1 else 0)   # TH_FLOW (th_flow.c): instance layer << 8 | 0x8000 json layer 1
        if 1100 <= o['type'] < 1150: a = SBG_SUB.get(o['cls'], 0) | (o['layer'] << 8) | (0x8000 if mapped and o.get('layer0') == 1 else 0)   # SB_GIM (sb_gim.c): member index | instance layer << 8 | 0x8000: json layer 1 of a mapped frame (session 20: behind the json-2 pieces, scrolls with layer 1)
        if 1680 <= o['type'] < 1720: a = THB_SUB.get(o['cls'], 0) | (o['layer'] << 8) | (0x8000 if mapped and o.get('layer0') == 1 else 0)   # TH_BOSS (th_boss.c): sub id | instance layer << 8 | 0x8000 json layer 1
        if 1968 <= o['type'] < 2016: a = TBB_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # TB_BOSS (tb_boss.c): member index | instance layer << 8
        if 2480 <= o['type'] < 2560: a = o['layer'] << 8   # BG_BOSS (bg_boss.c): instance layer << 8
        if 2080 <= o['type'] < 2144: a = (o['layer'] << 8) | (0x8000 if mapped and o.get('layer0') == 1 else 0) | (0x4000 if o.get('s08') else 0)   # PL_ENEMIES (pl_enemy.c): instance layer << 8 | 0x8000 json layer 1 of a mapped frame | 0x4000 its layer scrolls at 0.8
        d += struct.pack('<2H2i', o['type'], a, o['x'], o['y'])
    for t in table: d += t
    d += struct.pack('<%dH' % len(smap), *smap)
    d += struct.pack('<%dH' % len(pmap), *pmap)
    open(path, 'wb').write(d)

if __name__ == '__main__':
    main()
