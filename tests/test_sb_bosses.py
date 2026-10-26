#!/usr/bin/env python3
"""Host tests for the Sky Battalion bosses (session 19; src/game/sb_boss.c, sb_satellite.c, sb_grappler.c, sb_chopper.c,
sb_spade.c, sb_kujacker.c; build/re/spec_sb_bosses.md). Each encounter from entry to defeat to progression, per character,
against the original's traces (RV numbers, build/orig_traces/sb_boss: s_kill, g_kill, c_kill, p_kill, k_def, k_end; the port's
scripts teleport the player like the trace scripts, so the port frame is the trace frame - 20 in SB2 and - 55 in SB1).
usage: test_sb_bosses.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
DBG = ('sbs', 'sbg', 'sbc', 'sbp', 'sbk')

def run(level, char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_SB_ASSIST_I')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char), 'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1',
              'FP_DBG_SBS': '1', 'FP_DBG_SBG': '1', 'FP_DBG_SBC': '1', 'FP_DBG_SBP': '1', 'FP_DBG_SBK': '1'}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev, dbg = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] in ('sid', 'music', 'sbflow'):
            ev.append((len(rows), line))                     # (the frame the event happened in)
        elif t[0] in DBG and len(t) > 2:
            dbg.setdefault(t[0], {})[int(t[1])] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
    return rows, ev, dbg

def sids(ev, *ids): return [f for f, l in ev if l.startswith('sid ') and int(l.split()[1]) in ids]
def music(ev, name): return [f for f, l in ev if l.startswith('music ' + name)]
def sbf(ev, what): return [f for f, l in ev if l.startswith('sbflow ' + what)]
def near(a, b, tol=1): return len(a) == len(b) and all(abs(x - y) <= tol for x, y in zip(a, b))
def first(d, cond): return next((f for f in sorted(d) if cond(d[f])), None)

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
CHARS = ((0, 'lilac'), (1, 'carol'), (2, 'milla'))
INV = ''.join('%d SETV others E 99999\n' % f for f in (3, 500, 1000, 1500, 2000, 2500, 3000, 3500))

# ---- SB3: the satellite (everyone). RV s_kill (TP 11900 1000 at 60, A 0 at 400): the lock and sound 7 at 60, the beam first
# (165 at 276, 166 at 295, 169 at 354), defeat: the arms burst outside-in (38 at 409 / 429 / 449 / 469, the body at 489),
# crystals and the gates (9925: 40 at 519, ship flag 0x20), the stage music back
for ch, cn in CHARS:
    rows, ev, d = run('sb3', ch, 700, INV + '60 TP 11900 1000\n400 BOSSHP 0\n')
    s = d.get('sbs', {})
    check('sb3_%s_start' % cn, sids(ev, 7)[:1] == [60] and music(ev, 'bgm/m_midboss')[:1] == [60] and s.get(61, {}).get('D') == '-1', str((sids(ev, 7), music(ev, 'bgm/m_midboss'))))
    check('sb3_%s_beam_first' % cn, near(sids(ev, 165, 166, 169), [276, 295, 354]), str(sids(ev, 165, 166, 169)))
    check('sb3_%s_defeat' % cn, s.get(401, {}).get('D') == '6' and near(sids(ev, 38), [409, 429, 449, 469, 489]), str(sids(ev, 38)))
    check('sb3_%s_arena_end' % cn, near(sids(ev, 40), [519]) and near(sbf(ev, 'ship done 20'), [519]) and music(ev, 'bgm/m_stage4')[-1:] == [519], str((sids(ev, 40), sbf(ev, 'ship done'))))

# ---- the exit plane (9737-9745): with the cannons down (host hook FP_SB_ASSIST_I = sb_boss_cannon_down twice) it flies in
# from the left (D 5), parks over the player, a jump boards it (9743), it speeds up to 16 and leaves by the right edge: SB1
# (the flow's fly-in at x -40, vx 15)
rows, ev, d = run('sb3', 0, 1100, INV + '60 TP 11900 1000\n400 BOSSHP 0\n760 790 J\n', {'FP_SB_ASSIST_I': '600'})
board = next((r['f'] for r in rows if r['f'] > 760 and r['y'] < 920), None)   # the player snaps to the copilot seat
fly = [r for r in rows if board and board < r['f'] <= 1030]
check('sb3_plane_boarding', board and 780 <= board <= 800 and fly and fly[-1]['x'] > 15000 and fly[-1]['y'] < 930 and all(b['x'] >= a['x'] for a, b in zip(fly, fly[1:])), str(fly[-1:] and fly[-1]['x']))
arr = [r for r in rows if r['f'] > 1030 and r['x'] < 400]
check('sb3_plane_to_sb1', len(arr) > 0, str([(r['f'], r['x']) for r in rows[-3:]]))

# ---- SB4: the grappler (everyone). RV g_kill (TP 12950 940 at 60, A 0 at 700 / 1000 / 1300 / 1600 / 1900): a hit sound 47 for
# each of the first four, 77 the hands' slam, 266 the spire; the fifth: explosions, 38 at 2023 / 2050, crystals 40 at 2051
gk = INV + '60 TP 12950 940\n' + ''.join('%d BOSSHP 0\n' % f for f in (700, 1000, 1300, 1600, 1900))
for ch, cn in CHARS:
    rows, ev, d = run('sb4', ch, 2100, gk)
    g = d.get('sbg', {})
    check('sb4_%s_start' % cn, music(ev, 'bgm/m_midboss')[:1] == [60] and near(sids(ev, 266)[:4], [70, 92, 114, 136]), str(sids(ev, 266)[:4]))
    check('sb4_%s_hits' % cn, sids(ev, 47) == [700, 1000, 1300, 1600], str(sids(ev, 47)))
    check('sb4_%s_slams' % cn, near(sids(ev, 13), [522, 1127, 1742]), str(sids(ev, 13)))
    check('sb4_%s_arena_end' % cn, near(sids(ev, 40), [2051]) and near(sbf(ev, 'ship done 40'), [2051]), str((sids(ev, 40), sbf(ev, 'ship done'))))

# ---- SB2: Carol / Milla fight the chopper (9553: MV.A != 0), Lilac fights Spade (9598). RV c_kill (A 0 at 900): explosions
# every 7 frames (311 / 319 / 320, 38 every 21), crystals 132 frames after the first (1032); Spade's own data is never touched
for ch, cn in CHARS[1:]:
    rows, ev, d = run('sb2', ch, 600, INV + '40 TP 11550 700\n400 BOSSHP 0\n')
    c = d.get('sbc', {})
    x0 = sids(ev, 311, 319, 320)
    check('sb2_%s_chopper_start' % cn, music(ev, 'bgm/m_midboss')[:1] == [40] and c.get(41, {}).get('on') == '2', str(music(ev, 'bgm/m_midboss')))
    check('sb2_%s_chopper_defeat' % cn, x0[:1] and [b - a for a, b in zip(x0, x0[1:])][:6] == [7] * 6 and near(sids(ev, 40), [x0[0] + 132]), str((x0[:3], sids(ev, 40))))
    check('sb2_%s_chopper_end' % cn, near(sbf(ev, 'ship done 10'), sids(ev, 40)) and 'sbp' not in d, str(sbf(ev, 'ship done')))
# Spade, RV p_kill (TP at 60 there, 40 here; hitbox A 0 at 900 -> 880): card fans 71 x3 at 245 / 362 / 545 / 803 (265 / 382 /
# 565 / 823), dash jumps 8 + air-dash explosions 155, the 8-card burst 445 from 628 (648), defeat 156 at 880 (900), crystals 40 at
# 1147 (1167); the music: fade-out at the appearance, m_midboss at 135 (156; m_trouble before it is not ported)
rows, ev, d = run('sb2', 0, 1200, INV + '40 TP 11550 700\n880 BOSSHP 0\n')
check('sb2_lilac_spade_music', music(ev, 'bgm/m_midboss') and abs(music(ev, 'bgm/m_midboss')[0] - 136) <= 1, str(music(ev, 'bgm/m_midboss')))
check('sb2_lilac_spade_cards', near(sids(ev, 71), [245, 248, 251, 362, 365, 368, 545, 548, 551, 803, 806, 809]), str(sids(ev, 71)))
check('sb2_lilac_spade_dashes', near(sids(ev, 8), [308, 427, 489, 608, 747, 866]) and near(sids(ev, 155)[:4], [318, 332, 437, 451]), str((sids(ev, 8), sids(ev, 155)[:4])))
check('sb2_lilac_spade_burst', near(sids(ev, 445), [628, 633, 643, 649, 659, 665, 675, 681]), str(sids(ev, 445)))
check('sb2_lilac_spade_defeat', sids(ev, 156) == [880] and near(sids(ev, 40), [1147]) and near(sbf(ev, 'ship done 10'), [1147]), str((sids(ev, 156), sids(ev, 40))))

# ---- SB1: the Kujacker (everyone, after the three ships: the night). RV k_def / k_end (TP 1300 1150 every 100 frames, head A 0
# at 300 / 1000 / 1700 / 2400 / 3100; here - 55): m_boss3 with the fight (237 -> 182); per head hit 47 and the four dash passes
# (268 / 269 at hit + 77 / 202 / 327 / 452); the beam after the second hit (272 at 1597, the charge 271 at 1660); the fifth hit:
# slow motion, explosions (98 / 99 / 311 every 6 frames, then every 5), the flash 76 at 3538, the scene reset (C 152, the player
# at 1460) and the fly-off: results (m_clear) when the base is above y 400 (group off at 3885)
kk = '2 FLAGS 112\n' + INV + '5 TP 1300 1150\n' + ''.join('%d TP 1300 1150\n' % (f - 55) for f in range(160, 4201, 100)) + \
     ''.join('%d BOSSHP 0\n' % (f - 55) for f in (300, 1000, 1700, 2400, 3100))
O = -55
for ch, cn in CHARS:
    rows, ev, d = run('sb1', ch, 3900, kk)
    k = d.get('sbk', {})
    st = first(k, lambda r: r.get('I') is not None)
    check('sb1_%s_start' % cn, st is not None and abs(st - (237 + O)) <= 1 and music(ev, 'bgm/m_boss3')[:1] and abs(music(ev, 'bgm/m_boss3')[0] - (237 + O)) <= 1,
          str((st, music(ev, 'bgm/m_boss3'))))
    check('sb1_%s_head_hits' % cn, sids(ev, 47) == [f + O for f in (300, 1000, 1700, 2400, 3100)], str(sids(ev, 47)))
    check('sb1_%s_dash_passes' % cn, near(sids(ev, 268), [f + O for f in (377, 627, 1067, 1317, 1761, 2011, 2457, 2707)]) and
          near(sids(ev, 269), [f + O for f in (502, 752, 1192, 1442, 1886, 2136, 2582, 2832)]), str((sids(ev, 268), sids(ev, 269))))
    check('sb1_%s_beam' % cn, near(sids(ev, 272), [1597 + O]) and near(sids(ev, 271), [1660 + O]), str((sids(ev, 272), sids(ev, 271))))
    ex = sids(ev, 98, 99, 311)
    check('sb1_%s_end_explosions' % cn, ex[:1] == [3100 + O] and len(ex) in range(74, 79) and near(sids(ev, 76), [3538 + O]), str((ex[:2], len(ex), sids(ev, 76))))
    r152 = first(k, lambda r: r.get('pC') == '152')
    check('sb1_%s_end_reset' % cn, r152 is not None and abs(r152 - (3554 + O)) <= 1 and k[r152]['E'] == '1556.0', str(r152))
    check('sb1_%s_results' % cn, music(ev, 'bgm/m_clear')[:1] and abs(music(ev, 'bgm/m_clear')[0] - (3885 + O)) <= 2, str(music(ev, 'bgm/m_clear')))

# the dash hurts: without invulnerability the head's pass at the warned height costs 10 (9087) and knocks the player up (sound 23)
rows, ev, d = run('sb1', 0, 520, '2 FLAGS 112\n5 TP 1300 1150\n105 TP 1300 1150\n205 TP 1300 1150\n245 BOSSHP 0\n')
hp = [r['hp'] for r in rows if 'hp' in r]
check('sb1_dash_hurts', hp and min(hp[330:]) < hp[300] and sids(ev, 268)[:1] == [322], str((hp[300] if len(hp) > 300 else None, min(hp[330:]) if len(hp) > 330 else None, sids(ev, 268)[:1])))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
