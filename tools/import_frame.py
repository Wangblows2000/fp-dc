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
from convert import write_tex, pack, argb1555

# active classes the runtime understands -> engine type id
ENGINE_TYPES = {
    'playerstartingpoint': 1, 'objectcrystal': 2, 'enemydragonfly': 3, 'objectpetal': 6,
    'hazardspikesup': 7, 'yellowspringup': 8, 'yellowspringupleft': 13, 'yellowspringupright': 14,
    'yellowspringright': 15, 'yellowspringleft': 16, 'gimmickhangingidlesensor': 17, 'sign': 9, 'enemyturretus': 10,
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
           'levelferriswheel4_1777': 3, 'goldplatform2_1685': 1, 'goldplatform3_1720': 2, 'breakablefloor2_1767': 1,
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
    ('breakablefloor1_1766', 'fnbfloor0'), ('breakablefloor2_1767', 'fnbfloor1'), ('breakablefloorpiece_1764', 'fnbfpiece'),
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
                'fireworkparticle_1736': ('fireworklauncher',), 'effectsdust01_369': ('gimmickboostblock',),
                'effectsdebris_427': ('lamppostfront',), 'objectkeystone_429': ('gachaponwheel',)}
FNA_ENS_FRAMES = {'objectjumppad_1529': {30, 31, 32}}
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
def fnb_pose_rows(entries, n, pre=''):
    out = []
    for ai, dirs in (FNB_POSE_DIRS.items() if n in FNB_POSE_FRAMES else ()):
        for dk in dirs:
            if ai < len(entries) and entries[ai] and dk in entries[ai]:
                d = entries[ai][dk]; out.append({'name': '%sa%d_d%s' % (pre, ai, dk), 'frames': d['frames'], 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
    return out
ENGINE_TYPES_ID.update(FNB_TYPES_ID); ENS += FNB_ENS; ENS_FRAMES.update(FNB_ENS_FRAMES)
for _k, _v in FNB_ENS_WITH.items(): ENS_WITH[_k] = ENS_WITH.get(_k, ()) + _v
ENS_ONLY_WITH = {'guiweaponslot_426', 'effectswatersplash_379', 'bubblepatch_804', 'mediumbubble_805', 'generalwaterhorizon_377', 'bubblenumbers_405', 'shieldwood_342', 'shieldearth_443', 'shieldwater_439', 'shieldmagnetic_344', 'shieldflamefront_343', 'shieldflameback_376', 'effectsshield_440'}
# classes whose untrimmed image box matters (spring top edges RV, box-collision platforms/logs, invisible
# sensors): their images are never cropped. Everything else in the object bank is cropped to its opaque pixels.
BOX_TYPES = {8, 13, 14, 15, 16, 17, 30, 32, 33, 34, 35, 36}
# collision mask threshold: Chowdren builds image collision bits from alpha != 0 (RE Image::upload_texture,
# collide_sprite_sprite); FP_ALPHA_MIN overrides for experiments
ALPHA_MIN = int(os.environ.get('FP_ALPHA_MIN', '1'))

def load_img(src, i):
    if i < 0:   # translucent layer of image -i: its semi-transparent pixels made opaque (drawn with vertex alpha)
        im, hx, hy = load_img(src, -i)
        px = [(r, g, b, 255 if 0 < a < 255 else 0) for r, g, b, a in im.getdata()]
        im = Image.new('RGBA', im.size); im.putdata(px)
        return im, hx, hy
    im = Image.open(os.path.join(src, 'img_%d.png' % i)).convert('RGBA')
    m = [int(v, 0) for v in open(os.path.join(src, 'img_%d_meta.txt' % i)).read().split()]
    hx, hy = [v - 65536 if v > 32767 else v for v in m[:2]]
    return im, hx, hy

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
           'levelparallaxpiece2_1899': 4369}
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
           1897: (.99, .94, 0), 1898: (.72, .72, 0), 1899: (.60, .70, 0)}
PAR_DEG_FRAME = {(27, 1112): (1.0, .94, 0),          # the same handle with RM3's own D (RE spec_rm_flow 4)
                 (35, 1629): (.99, .94, -.1), (35, 1640): (.99, .94, -.35)}   # FN6's drifting sky (inline, spec_fn_flow 4)
# BGStar_16 (FN1-FN3, FN6; spec_fn_flow 4): 7471 stores A = x, B = y, C = the instance index and forces frame C; 7466 puts
# every star at (piece0a.x + A, piece0a.y + B) each frame. A class listed here (class_handle: parent piece handle) is exported
# as one composed image attached to that piece (flag 1024, parallax.c). Fortune Night's stars are objects of fn_gima.c
# (OT_FN_STAR 603, merged first), so the table is empty: listing bgstar_16 here would draw them twice.
PAR_STARS = {}
PAR_SKY_BELOW = {27: (1112, 1133, 1134, 1137, 1138)} # RM3 6298: destroyed once the player is below y 2400 (one way)
# layers scrolling at 0.8 (RE Layer::init): Aqua Tunnel and RM1-RM4 layer 1, RM5 layer 0 (spec_rm_flow 2.4); json frame
SCROLL08 = {17: 1, 18: 1, 19: 1, 20: 1, 25: 1, 26: 1, 27: 1, 28: 1, 29: 0, 30: 1, 31: 1, 32: 1, 33: 1, 34: 1, 35: 1}   # FN (json 30-35): layer 1 (spec_fn_flow 2.4)

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
        if key not in PAR_IMG or o['y'] is None: continue
        img = PAR_IMG[key]
        if img not in imgidx:
            im, hx, hy = load_img(src, img)
            chunks = []
            y = 0
            for ch in pow2_split(im.height):
                x = 0
                for cw in pow2_split(im.width):
                    sub = im.crop((x, y, min(x + cw, im.width), min(y + ch, im.height)))
                    tn = '%s_px%d_%d' % (name, len(images), len(chunks))
                    write_tex(sub, os.path.join(out, tn + '.tex'))
                    chunks.append((x, y, sub.width, sub.height))
                    x += cw
                y += ch
            imgidx[img] = len(images); images.append((im.width, im.height, chunks))
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
        iw, ih = images[imgidx[img]][0], images[imgidx[img]][1]
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
        images.append((comp.width, comp.height, chunks))
        pieces.append((len(images) - 1, 0., 0., 0., parent, x0, y0, 1024, stars[0]['id']))   # y0 = the parent's handle, (bw, bh) = offset
    d = bytearray(b'FPPX' + struct.pack('<4H', len(images), len(pieces), 1 if water else 0, 1))   # version 1: + the piece's handle
    for w, h, chunks in images:
        d += struct.pack('<3H', w, h, len(chunks))
        for c in chunks: d += struct.pack('<4H', *c)
    for (ii, D, E, G, y0, bw, bh, fl, hd) in pieces: d += struct.pack('<H3fi4H', ii, D, E, G, y0, bw, bh, fl, hd)
    if water:
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
    main = next((o['layer'] for o in fr['objects'] if o['cls'] == 'playerstartingpoint' and o['layer'] is not None
                 and o['x'] is not None and 0 <= o['x'] < W and 0 <= o['y'] < H), 2)
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
            if b['image'] is None:
                skipped['no-image'] = skipped.get('no-image', 0) + 1; continue
            ac = b['fields'].get('0xac')
            col = 0 if ac is None else {9: 1, 8: 1, 11: 3}.get(ac, 0)
            box = ac == 9
            invisible = b['fields'].get('0x5f') == 0 and '0x5c' in b['fields']
            if SCROLL08.get(n) == o['layer0']: invisible_extra = 2   # the frame's 0.8 layer (RE Layer::init, spec_at_flow 4.1, spec_rm_flow 2.4)
            else: invisible_extra = 0
            pieces.append({'img': b['image'], 'x': o['x'], 'y': o['y'], 'layer': o['layer'] or 0, 'layer0': o['layer0'], 'scroll08': invisible_extra, 'col': col,
                           'box': box, 'invisible': invisible, 'cls': o['cls']})
        else:
            t = engine_type(o['cls'], o['id'])
            if t is None:
                skipped[o['cls']] = skipped.get(o['cls'], 0) + 1; continue
            objs.append({'type': t, 'x': o['x'], 'y': o['y'], 'cls': key, 'layer': o['layer'] or 0})

    for o in objs: o['fna'] = FNA_SUB.get(o['cls'], 0) | FNA_FLAGS.get((o['cls'], n), 0) | (o['layer'] << 8)   # FN_GIMA (fn_gima.c)
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
            best = min(((len(sc | ccache[i]) - len(sc), k) for k, (sc, _) in enumerate(sets) if len(sc | ccache[i]) <= PIECE_COLS), default=None)
            if best is None: sets.append((set(ccache[i]), [i]))
            else: sets[best[1]][0].update(ccache[i]); sets[best[1]][1].append(i)
        order = [sorted(m) for _, m in sets]
        report.append('pieces: %d colours, split into %d colour sets %s' % (len(allc), len(order), [len(sc) for sc, _ in sets]))
    else:
        order = [list(ids)]
    remaining = []; atlases = []; where = {}
    while order or remaining:
        if not remaining: remaining = order.pop(0)
        k = len(atlases)
        # the first atlas is up to 1024x1024; later ones at most 1024x512 (task 48: a 1024x1024 second atlas was
        # often only ~55-65 % used, wasting 384-512 KB of VRAM per level; the renderer takes up to MAX_ATLAS = 4)
        cap = (lambda t: pack([(i, imgs[i][0]) for i in t], 1024)) if k == 0 else (lambda t: pack([(i, imgs[i][0]) for i in t], 1024, 512))
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
                p2 = pack([(i, imgs[i][0]) for i in take], 1024, hh)
                if p2: pos = p2; break
        atlas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
        for i in take:
            atlas.paste(imgs[i][0], pos[i]); where[i] = (k, pos[i][0], pos[i][1])
        bbox = atlas.getbbox() or (0, 0, 8, 8)
        atlas = atlas.crop((0, 0, max(bbox[2], 8), max(bbox[3], 8)))
        tw, th = write_tex(atlas, os.path.join(out, '%s_p%d.tex' % (name, k)))
        atlases.append((tw, th))
        remaining = [i for i in remaining if i not in take]
    report.append('pieces: %d placements, %d unique images, %d atlases %s' % (len(pieces), len(ids), len(atlases), atlases))

    # ---- collision rasterisation (alpha >= 128) on collision layers
    TW, TH = (W + 15) // 16, (H + 15) // 16
    solid = bytearray(TW * 16 * TH * 16)
    plat = bytearray(TW * 16 * TH * 16)
    stride = TW * 16
    masks = {}
    for p in pieces:
        if p['col'] not in (1, 2) or p['layer'] not in col_layers: continue
        im, hx, hy = imgs[p['img']]; hx = hy = 0   # backdrops ignore the image hotspot (RE Backdrop::draw adds it, draw_image subtracts it)
        mk = (p['img'], p['box'])
        if mk not in masks:
            masks[mk] = b'\x01' * (im.width * im.height) if p['box'] else im.getchannel('A').point(lambda v: 1 if v >= ALPHA_MIN else 0).tobytes()
        m = masks[mk]; x0 = p['x'] - hx; y0 = p['y'] - hy
        tgt = solid if p['col'] == 1 else plat
        for yy in range(im.height):
            Y = y0 + yy
            if Y < 0 or Y >= TH * 16: continue
            row = m[yy * im.width:(yy + 1) * im.width]
            base = Y * stride
            for xx, v in enumerate(row):
                if v:
                    X = x0 + xx
                    if 0 <= X < stride: tgt[base + X] = 1
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

    # ---- active-object sprite bank (first anim, right-facing) for implemented classes
    bank_classes = sorted({o['cls'] for o in objs if (o['type'] < 20 or 30 <= o['type'] < 37) and o['type'] not in (1, 31, 35)})   # >= 37: the _ens bank   # 20-27 parallax pieces; Ganseki uses its gan_* anims; the bridge box is invisible
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
    if any(o['cls'].startswith('monitor') and o['cls'].endswith('shield') and o['x'] >= 0 for o in fr['objects']): placed.add('shieldmonitor_onmap')
    if n in WATER_FRAMES: placed.add('water_level')   # DV2 / DV3: MasterLevel A = 1 (5109 / 5314); AT1 / AT2 / AT4 (4309 / 4478 / 4685)
    ens = {'name': name + '_ens', 'anims': [], 'keep_box': set()}
    placed_handles = {}
    for o in fr['objects']: placed_handles.setdefault(o['cls'], set()).add(o['id'])
    for e in ENS:
        cls, ai, dk, nm = e[:4]
        if cls in ENS_FRAMES and n not in ENS_FRAMES[cls]: continue
        if len(e) > 5 and not any(w in placed for w in e[5]): continue   # rows tied to their creators (FN_ENEMIES)
        base = cls.rsplit('_', 1)[0]
        if cls in ENS_ONLY_WITH:                     # off-map templates do not count (shields: only with an on-map shield monitor)
            if not any(w in placed for w in ENS_WITH.get(cls, ())): continue
        elif base not in placed and not any(w in placed for w in ENS_WITH.get(cls, ())): continue
        # the frame's own object (class + handle): when the row names a handle this frame does not place but the frame
        # places the class under another handle (another stage's object: its own images), the sprite comes from the
        # placed handle (session 14; e.g. AT4's spikes 671, DV3's wall plate 1029 and door 1066, RM's spikes 1106-1109)
        hs = placed_handles.get(base, set())
        if hs and int(cls.rsplit('_', 1)[1]) not in hs:
            for h2 in sorted(hs):
                try: an['%s_%d' % (base, h2)]['anims'][ai][dk]; cls = '%s_%d' % (base, h2); break
                except (KeyError, IndexError, TypeError): pass
        try: d = an[cls]['anims'][ai][dk]
        except (KeyError, IndexError, TypeError): report.append('  ens: missing %s anim %d dir %s' % (cls, ai, dk)); continue
        fr_ = d['frames'][e[4]] if len(e) > 4 and e[4] is not None else d['frames']
        ens['anims'].append({'name': nm, 'frames': fr_, 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
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
            for dkey in {21: ('1', '17'), 0: ('4', '20'), **(FNB_POSE_DIRS if n in FNB_POSE_FRAMES else {})}.get(ai, ()):   # + FN_GIMB poses
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
        write_bank2(src, mb, out, report)
    write_lv2(os.path.join(out, name + '.lv2'), W, H, pieces, where, imgs, objs, TW, TH, table, smap, pmap, len(atlases), bank)
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
    # images of these animations are cropped to their opaque pixels (hotspot shifted to match): drawing
    # is unchanged, and nothing uses their untrimmed box (explosions: effects; Ganseki: hotspot rules +
    # pixel masks). Keeps the 1024x1024 object atlas within one PVR texture.
    # Lilac's skin and hair banks too: her collision is the separate sensor objects, never the sprite box
    whole = bank['name'] in ('lilac2', 'lilachair', 'carol2', 'caroltail', 'carolbike', 'milla2', 'millatail', 'millafx')
    keep = bank.get('keep_box', set())
    if bank['name'].endswith('_ens'):
        crop_ids = {i for a in bank['anims'] for i in a['frames']}
    elif bank['name'].endswith('_obj'):
        keep_ids = {i for a in bank['anims'] if a['name'] in keep for i in a['frames']}
        keep_ids |= {i for a in bank['anims'] if a['name'].startswith(('hud_', 'res_', 'pellet')) for i in a['frames']}   # drawn by top-left; pellet: whole image box (RV)
        crop_ids = {i for a in bank['anims'] for i in a['frames']} - keep_ids
    else:
        crop_ids = {i for a in bank['anims'] if whole or a['name'].startswith(('gan_', 'explosion')) for i in a['frames']}
        # the parked bike (ObjectBike_459) collides with its whole image box (initialize_active(1)): keep it (RV b_remount)
        crop_ids -= {i for a in bank['anims'] if a['name'].startswith('pbike') for i in a['frames']}
    crop_off = {}
    for i in crop_ids:
        im, hx, hy = imgs[i]; bb = im.getbbox()
        if bb and (bb[2] - bb[0], bb[3] - bb[1]) != im.size:
            imgs[i] = (im.crop(bb), hx - bb[0], hy - bb[1]); crop_off[i] = (bb[0], bb[1])
    size, pos, second = 64, None, []
    while not pos:          # smallest power-of-two atlas: square, then half height (PVR textures may be 2:1)
        pos = pack([(i, imgs[i][0]) for i in ids], size, size // 2) or pack([(i, imgs[i][0]) for i in ids], size)
        if not pos:
            size *= 2
            if size > 1024:
                if not bank['name'].endswith('_ens'): raise SystemExit('bank %s too large' % bank['name'])
                # _ens only: a second 1024 atlas (<name>_2.tex); its frames carry v | 0x8000 (assets.c bank_load)
                size = 1024; take = list(ids)
                while take:
                    pos = pack([(i, imgs[i][0]) for i in take], 1024)
                    if pos: break
                    take = take[:len(take) * 7 // 8]
                second = [i for i in ids if i not in take]
                pos2 = None; s2 = 64
                while not pos2:
                    pos2 = pack([(i, imgs[i][0]) for i in second], s2, s2 // 2) or pack([(i, imgs[i][0]) for i in second], s2)
                    if not pos2:
                        s2 *= 2
                        if s2 > 1024: raise SystemExit('bank %s too large for two atlases' % bank['name'])
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
        if P >= 256:
            order = [i for i, _ in sorted(((i, imgs[i][0]) for i in ids), key=lambda kv: (-kv[1].height, kv[0]))]
            lo, hi = 0, len(order)                 # the shelf packer places in this order: the fitting prefix is monotone
            while lo < hi:
                m = (lo + hi + 1) // 2
                if pack([(i, imgs[i][0]) for i in order[:m]], size, P // 2): lo = m
                else: hi = m - 1
            take, rest = order[:lo], order[lo:]
            if take and rest:
                pos1 = pack([(i, imgs[i][0]) for i in take], size, P // 2)
                pos2, s2 = None, 64
                while not pos2 and s2 <= 1024:
                    pos2 = pack([(i, imgs[i][0]) for i in rest], s2, s2 // 2) or pack([(i, imgs[i][0]) for i in rest], s2)
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

def write_lv2(path, W, H, pieces, where, imgs, objs, TW, TH, table, smap, pmap, natlas, bank):
    """'FPL2' u32 w, h (px); u16 npieces, nobj, natlas, nuniq; u16 tw, th; u32 pad
       piece {s32 x, y (top-left); u16 u, v, w, h; u8 atlas, layer, col, pad}  (20 B)
       obj   {u16 type, anim (index in <name>_obj bank or 0xffff); s32 x, y}  (12 B)
       uniq tiles: 32 B each (16 rows of u16, bit x = pixel x)
       solid map u16[tw*th]; platform map u16[tw*th]"""
    pieces = sorted(pieces, key=lambda p: (p['layer'], p['layer0']))   # stable: keeps the frame's instance (z) order inside a layer (mapped layers keep their original order)
    names = [a['name'] for a in bank['anims']]
    d = bytearray(b'FPL2' + struct.pack('<2I4H2HI', W, H, len(pieces), len(objs), natlas, len(table), TW, TH, 0))
    for p in pieces:
        im, hx, hy = imgs[p['img']]; hx = hy = 0   # backdrop pieces: drawn at the instance x, y (RE Backdrop::draw)
        k, u, v = where[p['img']]
        d += struct.pack('<2i4H4B', p['x'] - hx, p['y'] - hy, u, v, im.width, im.height, k, p['layer'], p['col'], (1 if p['invisible'] else 0) | p.get('scroll08', 0))
    for o in objs:
        a = names.index(o['cls'][:15]) if o['cls'][:15] in names else (o.get('par_index', 0xffff) if 20 <= o['type'] < 28 else 0xffff)
        if 139 <= o['type'] <= 143 or o['type'] in (169, 170, 187, 204): a = o.get('layer', 2)   # waterfalls, AT water jets / crushers / slime floor: the instance's layer
        if 300 <= o['type'] < 350: a = RMB_SUB.get(o['cls'], 0)   # Relic Maze acts 3-5 families: the member index (relic_gimb.c)
        if 560 <= o['type'] < 680: a = o.get('fna', 0)            # FN_GIMA: member index | act flags | layer << 8 (fn_gima.c)
        if 680 <= o['type'] < 800: a = FNB_SUB.get(o['cls'], 0) | (o['layer'] << 8)   # FN_GIMB (fn_gimb.c): member index | instance layer << 8
        d += struct.pack('<2H2i', o['type'], a, o['x'], o['y'])
    for t in table: d += t
    d += struct.pack('<%dH' % len(smap), *smap)
    d += struct.pack('<%dH' % len(pmap), *pmap)
    open(path, 'wb').write(d)

if __name__ == '__main__':
    main()
