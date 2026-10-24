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
    'waterfallback': 139, 'waterfallfront': 140, 'waterfallfront2': 141, 'waterfallfront3': 142, 'waterfallfront4': 143,   # EnemyGansekiJr_835 (RE build/re/spec_ganseki.md)     # LadderTop_428: invisible obstacle at the top of each ladder (RE spec_ladder.md)
}

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
      [('bubblenumbers_405', 0, d, 'bnum%s' % d) for d in ('0', '1', '2', '3', '4', '5')]
ENS_WITH = {'enemysporeprojectile_1022': ('enemysporepod',), 'objectkeystone_429': ('enemysporepod', 'enemyturretus', 'monitorkeystone'),
            'effectsdebris_427': ('enemysporepod', 'bossserpentinehead'), 'effectsexplosion06_435': ('minibosswretchnidbase', 'minibosstredd', 'bossserpentinehead'),
            'effectsshade_436': ('enemygansekijr', 'enemydragonfly', 'enemystarflounder', 'enemycacteon'),
            'minibosstreddbomb_1060': ('minibosstredd',), 'minibosstredddebris_1058': ('minibosstredd',), 'monitoricon_363': ('minibosstredd', 'shieldmonitor_onmap'),
            'bossfireball_1003': ('bossserpentinehead',), 'bossboulder_1099': ('bossgiantboulder',), 'effectwarning_1100': ('bossgiantboulder',),
            'breakablerockpiece_432': ('bosstunnel2', 'bosstunnel'), 'breakabledebris1_1098': ('breakable1',), 'effectsdust_437': ('pushableblock1',), 'effectswatersplash_379': ('water_level',), 'bubblepatch_804': ('water_level',), 'mediumbubble_805': ('water_level',), 'generalwaterhorizon_377': ('water_level',), 'bubblenumbers_405': ('water_level',), 'shieldwood_342': ('shieldmonitor_onmap',), 'shieldearth_443': ('shieldmonitor_onmap',), 'shieldwater_439': ('shieldmonitor_onmap',), 'shieldmagnetic_344': ('shieldmonitor_onmap',), 'shieldflamefront_343': ('shieldmonitor_onmap',), 'shieldflameback_376': ('shieldmonitor_onmap',), 'effectsshield_440': ('shieldmonitor_onmap',)}

ENS_ONLY_WITH = {'effectswatersplash_379', 'bubblepatch_804', 'mediumbubble_805', 'generalwaterhorizon_377', 'bubblenumbers_405', 'shieldwood_342', 'shieldearth_443', 'shieldwater_439', 'shieldmagnetic_344', 'shieldflamefront_343', 'shieldflameback_376', 'effectsshield_440'}
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
           'levelparallaxpiece13_1068': 6387, 'levelparallaxpiece13_1092': 6387}
PAR_BOXH = {'levelparallaxpiece6_897': 160, 'levelparallaxpiece13_1068': 1024, 'levelparallaxpiece13_1092': 820}
# D, E, G (RE 5110 / 5315 / 5603; DV1's start events not read: the same table is used, INFERRED)
PAR_DEG = {491: (.99, .94, 0), 891: (.98, .93, 0), 893: (.95, .91, 0), 894: (.96, .92, -.1), 895: (.92, .88, -.3),
           896: (.6, .7, 0), 897: (.5, .6, 0), 892: (.4, .5, 0), 913: (.35, .43, 0), 987: (.35, .43, 0),
           1006: (.7, .8, 0), 1005: (.6, .7, 0), 1007: (.4, .5, 0), 1009: (.35, .43, 0), 1067: (.15, .15, 0),
           1068: (.15, .15, 0), 1092: (.15, .15, 0)}

def pow2_split(n, cap=1024):
    out, r = [], n
    while r > 0:
        p = 8
        while p * 2 <= min(r, cap): p *= 2
        if r < 8: p = 8
        out.append(p); r -= p
    return out

def export_parallax(fr, src, name, out):
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
        D, E, G = PAR_DEG.get(o['id'], (.5, .5, 0))
        flags = 0
        if has_switch and o['id'] in (491, 891): flags |= 1        # shown while view top < 1360 (5101/5102)
        if has_switch and o['id'] in (1006, 1005): flags |= 2      # shown while view top >= 1360
        if o['id'] == 1005: flags |= 4                             # piece9: water frames (5094)
        iw, ih = images[imgidx[img]][0], images[imgidx[img]][1]
        pieces.append((imgidx[img], D, E, G, o['y'], iw, PAR_BOXH.get(key, ih), flags))
    d = bytearray(b'FPPX' + struct.pack('<4H', len(images), len(pieces), 1 if water else 0, 0))
    for w, h, chunks in images:
        d += struct.pack('<3H', w, h, len(chunks))
        for c in chunks: d += struct.pack('<4H', *c)
    for (ii, D, E, G, y0, bw, bh, fl) in pieces: d += struct.pack('<H3fi3H', ii, D, E, G, y0, bw, bh, fl)
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
            pieces.append({'img': b['image'], 'x': o['x'], 'y': o['y'], 'layer': o['layer'] or 0, 'col': col,
                           'box': box, 'invisible': invisible, 'cls': o['cls']})
        else:
            t = ENGINE_TYPES.get(o['cls'])
            if t is None:
                skipped[o['cls']] = skipped.get(o['cls'], 0) + 1; continue
            objs.append({'type': t, 'x': o['x'], 'y': o['y'], 'cls': key, 'layer': o['layer'] or 0})

    # ---- piece atlases
    ids = sorted({p['img'] for p in pieces})
    imgs = {i: load_img(src, i) for i in ids}
    remaining = list(ids); atlases = []; where = {}
    while remaining:
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
        if len(take) == len(remaining):          # last atlas: the smallest power-of-two height that holds the rest
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
        im, hx, hy = imgs[p['img']]
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
           ('hud_clock', an['guitimer_536']['anims'][0]['0']['frames']),
           ('hud_digits', an['guilives_425']['images'][:10]),   # counter digits 0-9 (lives/crystals/timer)
           ('card_stage', an['levelcardintro3_684']['anims'][0]['0']['frames'][:1]),   # "STAGE 1" (Dragon Valley is stage 1)
           ('card_name', an['levelcardintro2_683']['anims'][0]['0']['frames'][:1]),    # "Dragon Valley"
           ('card_fade', [129]), ('card_gameover', [27]),
           ('pause', an['guipause_347']['anims'][0]['0']['frames']),               # Paused banner + options
           ] + ([('hud_air', an['guiair_700']['images'])] if n in (22, 23) else []) + ([('res_complete', an['levelcardactclear1_864']['anims'][7]['0']['frames']),  # "Stage Complete!" build-in
           ('res_results', [955]), ('res_panel', [10860]), ('res_rows', an['levelcardactclear3_866']['anims'][7]['0']['frames']),   # 6-frame reveal (RE 4570-4573)
           ('res_bar', [10844, 10660]),
           ('res_complete_t', [-8005]), ('res_panel_t', [-10860]), ('res_bar_t', [-10844, -10660]),   # alpha-87 layers (RV screenshots)
           ('res_digits', an['totalbonus_870']['images'][:10]),
           ('res_small', an['levelcardbonus3_869']['images'][:10])] if n == 24 else [])   # results: DV4 only
    for nm, frames in HUD:
        bank['anims'].append({'name': nm, 'frames': frames, 'speed': 0, 'loop': -1, 'back_to': 0})
    write_bank2(src, bank, out, report)
    # ---- enemies / bosses / gimmicks bank (cropped; their collision uses masks relative to the hotspot)
    placed = {o['cls'].rsplit('_', 1)[0] for o in fr['objects']}
    if any(o['cls'].startswith('monitor') and o['cls'].endswith('shield') and o['x'] >= 0 for o in fr['objects']): placed.add('shieldmonitor_onmap')
    if n in (22, 23): placed.add('water_level')          # DV2 / DV3: MasterLevel A = 1 (5109 / 5314)
    ens = {'name': name + '_ens', 'anims': [], 'keep_box': set()}
    for e in ENS:
        cls, ai, dk, nm = e[:4]
        base = cls.rsplit('_', 1)[0]
        if cls in ENS_ONLY_WITH:                     # off-map templates do not count (shields: only with an on-map shield monitor)
            if not any(w in placed for w in ENS_WITH.get(cls, ())): continue
        elif base not in placed and not any(w in placed for w in ENS_WITH.get(cls, ())): continue
        try: d = an[cls]['anims'][ai][dk]
        except (KeyError, IndexError, TypeError): report.append('  ens: missing %s anim %d dir %s' % (cls, ai, dk)); continue
        fr_ = d['frames'][e[4]] if len(e) > 4 else d['frames']
        ens['anims'].append({'name': nm, 'frames': fr_, 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
    if ens['anims']: write_bank2(src, ens, out, report)
    report.append('ens bank: %s' % [a['name'] for a in ens['anims']])
    # parallax (RE + RV: build/re/spec_parallax.md): layer-0 SystemBox pieces, tiled, moved every frame by
    # sx = fmod(trunc(D*vl + F - vl), A), sy = trunc(E*vt + C) - vt. Images are split into power-of-two chunks
    # (each <= 16 colours: palettize --pal4 makes them 4 bpp with one 16-entry block per image).
    par = export_parallax(fr, src, name, out)
    report.append('parallax pieces: %s' % par)

    # ---- Lilac (playerskinsash_341) from recovered animation tables, both facings
    # Animation numbers RUNTIME-VERIFIED from original traces (sprite image -> images_* table),
    # except hurt/defeat (still by eye). See RUNTIME_MAP.md "Lilac animations".
    LILAC = [('idle', 0), ('walk', 1), ('run', 2), ('fastrun', 23), ('jump', 24), ('lookup', 11),
             ('crouch', 10), ('skid', 16), ('attack', 26), ('airatk', 25), ('uppercut', 28),
             ('dkwind', 7), ('divekick', 19), ('cyclone', 18), ('charge', 7), ('boost', 13),
             ('hurt', 8), ('defeat', 38), ('crouchatk', 30), ('spring', 14), ('hang', 15),
             ('push', 12), ('climb', 9), ('climbdown', 35), ('climbover', 36), ('catch', 33),
             ('cork', 32), ('cork2', 31), ('swim', 39)]   # cork: RV cork_dv1 (M45, OTHERS.Y 45 -> skin 32, 46 -> 31); RV push_wall / ladder_* traces, RE events 2456-2467
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
            for dkey in {21: ('1', '17'), 0: ('4', '20')}.get(ai, ()):
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
    whole = bank['name'] in ('lilac2', 'lilachair', 'carol2', 'caroltail', 'carolbike')
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
    for i in crop_ids:
        im, hx, hy = imgs[i]; bb = im.getbbox()
        if bb and (bb[2] - bb[0], bb[3] - bb[1]) != im.size:
            imgs[i] = (im.crop(bb), hx - bb[0], hy - bb[1])
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
    # Carol's banks (session 10 perf): a content height just over a power of two (carol2 574 rows -> a 1024x1024
    # texture, 450 KB of it empty) is split into a half-height first atlas and a small second one (<name>_2.tex,
    # frames v | 0x8000, the _ens mechanism). Drawing is unchanged; only the VRAM layout differs.
    if not second and bank['name'] in ('carol2', 'carolbike', 'caroltail'):
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
    pieces = sorted(pieces, key=lambda p: p['layer'])   # stable: keeps the frame's instance (z) order inside a layer
    names = [a['name'] for a in bank['anims']]
    d = bytearray(b'FPL2' + struct.pack('<2I4H2HI', W, H, len(pieces), len(objs), natlas, len(table), TW, TH, 0))
    for p in pieces:
        im, hx, hy = imgs[p['img']]
        k, u, v = where[p['img']]
        d += struct.pack('<2i4H4B', p['x'] - hx, p['y'] - hy, u, v, im.width, im.height, k, p['layer'], p['col'], 1 if p['invisible'] else 0)
    for o in objs:
        a = names.index(o['cls'][:15]) if o['cls'][:15] in names else (o.get('par_index', 0xffff) if 20 <= o['type'] < 28 else 0xffff)
        if 139 <= o['type'] <= 143: a = o.get('layer', 2)   # waterfalls: the instance's layer
        d += struct.pack('<2H2i', o['type'], a, o['x'], o['y'])
    for t in table: d += t
    d += struct.pack('<%dH' % len(smap), *smap)
    d += struct.pack('<%dH' % len(pmap), *pmap)
    open(path, 'wb').write(d)

if __name__ == '__main__':
    main()
