#!/usr/bin/env python3
"""Host tests for the save (v3, session 13; build/re/spec_records_saves.md): stage-indexed records, files, progress and
cards for every stage, the original's record rules, conversion of v1 / v2 saves, CRC rejection, size bound.
usage: test_saves.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, CHARS, CARDBYTES = 15, 8, 4, 13

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW')]; e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    return r.stdout + r.stderr

def crc16(b):
    c = 0xffff
    for x in b:
        c ^= x << 8
        for _ in range(8): c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c

def parse(path):
    d = open(path, 'rb').read()
    assert d[:3] == b'FPS' and d[3] == 3 and crc16(d[:-2]) == struct.unpack('<H', d[-2:])[0], 'bad v3 save'
    p = 4; sv = {}
    sv['opts'] = list(d[p:p + 5]); p += 5
    sv['files'] = []
    for i in range(FILES):
        used, ch, mode, last, lives, shield, counter = struct.unpack_from('<6BH', d, p); p += 8
        st = []
        for s in range(STAGES): st.append(struct.unpack_from('<IHB', d, p)); p += 7
        sv['files'].append(dict(used=used, ch=ch, mode=mode, last=last, lives=lives, shield=shield, counter=counter, stage=st))
    sv['rec'] = [[struct.unpack_from('<IH', d, p + 6 * (s * CHARS + c)) for c in range(CHARS)] for s in range(STAGES)]; p += 6 * STAGES * CHARS
    sv['progress'] = list(d[p:p + CHARS]); p += CHARS
    cb = d[p:p + CARDBYTES]; p += CARDBYTES
    sv['cards'] = {n for n in range(1, 101) if cb[(n - 1) >> 3] >> ((n - 1) & 7) & 1}
    assert p == len(d) - 2, 'size %d vs %d' % (p, len(d) - 2)
    sv['size'] = len(d)
    return sv

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

DV4_END = '600 TP 16000 2280\n620 1300 R\n'           # DV4's right edge after 10 s (results path B, RE 5381) -> D = 150 save
AT4_END = '60 TP 4952 254\n60 SETV others E 99999\n950 BOSSHP 0\n'   # AT4: the snake's defeat -> results -> save
with tempfile.TemporaryDirectory() as d:
    sv = os.path.join(d, 'save.bin')
    # ---- Time Attack, Lilac, DV4 results: record of stage 1 (Dragon Valley) for Lilac in ms; no file touched
    out = run(DV4_END, 1100, {'FP_LEVEL': 'dv4', 'FP_TA': '1', 'FP_SAVE': sv})
    s = parse(sv)
    t, k = s['rec'][1][0]
    check('ta_record_stage1_lilac_ms', 10000 < t < 30000 and all(s['rec'][x][c][0] == 0 for x in range(STAGES) for c in range(CHARS) if (x, c) != (1, 0)), 'time %d ms' % t)
    check('ta_leaves_files', not any(f['used'] for f in s['files']) and s['progress'] == [0, 0, 0, 0])
    check('save_size_bound', s['size'] <= 1536, '%d bytes' % s['size'])
    # a slower run keeps the time
    out = run('900 TP 16000 2280\n920 1600 R\n', 1400, {'FP_LEVEL': 'dv4', 'FP_TA': '1', 'FP_SAVE': sv, 'FP_TRACE_SND': '1'})
    s2 = parse(sv)
    check('ta_slower_run_keeps_time', any(l.startswith('save ') for l in out.splitlines()) and s2['rec'][1][0][0] == t, '%d -> %d' % (t, s2['rec'][1][0][0]))
    # ---- runs under 10 s are not records (RE 907: ML.Z > 10000)
    sv0 = os.path.join(d, 'short.bin')
    run('5 TP 16000 2280\n20 700 R\n', 520, {'FP_LEVEL': 'dv4', 'FP_TA': '1', 'FP_SAVE': sv0})
    s0 = parse(sv0)
    check('ta_under_10s_ignored', s0['rec'][1][0][0] == 0, str(s0['rec'][1][0]))
    # ---- story (file 2, new game): the stage block, last stage, progress, and the records too (RE 907 has no mode test)
    svs = os.path.join(d, 'story.bin')
    run(DV4_END, 1100, {'FP_LEVEL': 'dv4', 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': svs})
    s = parse(svs)
    f = s['files'][2]
    check('story_file_block_stage1', f['used'] == 1 and f['last'] == 1 and 10000 < f['stage'][1][0] < 30000 and f['ch'] == 0, str(f['stage'][1]) + ' last %d' % f['last'])
    check('story_progress_lilac', s['progress'][0] == 2, str(s['progress']))
    check('story_updates_records', s['rec'][1][0][0] == f['stage'][1][0], '%s vs %s' % (s['rec'][1][0], f['stage'][1]))
    check('story_only_its_file', [x['used'] for x in s['files']] == [0, 0, 1, 0, 0, 0, 0, 0])
    # ---- Aqua Tunnel is stage 0, Milla's slot: the AT4 results write rec[0][Milla] and never DV's row
    sva = os.path.join(d, 'at.bin')
    run(AT4_END, 1700, {'FP_LEVEL': 'at4', 'FP_CHAR': '4', 'FP_FILE': '0', 'FP_FILE_NEW': '1', 'FP_SAVE': sva})
    s = parse(sva)
    check('at_record_stage0_milla', s['rec'][0][2][0] > 10000 and s['rec'][1][2][0] == 0, 'AT %s DV %s' % (s['rec'][0][2], s['rec'][1][2]))
    check('at_file_block_stage0', s['files'][0]['last'] == 0 and s['files'][0]['stage'][0][0] > 0 and s['progress'][2] == 1, str(s['files'][0]['stage'][0]))
    # ---- deaths of the stage go into its block
    svd = os.path.join(d, 'deaths.bin')
    run('5 HP 0\n5 HURT 10\n' + DV4_END.replace('600 TP', '700 TP').replace('620 1300', '720 1400'), 1300,
        {'FP_LEVEL': 'dv4', 'FP_FILE': '1', 'FP_FILE_NEW': '1', 'FP_SAVE': svd})
    s = parse(svd)
    check('story_deaths_in_block', s['files'][1]['stage'][1][2] == 1, str(s['files'][1]['stage'][1]))
    # ---- cards: AT1's first card is id 21 (spec_at_flow 1.2) and is saved at the act clear
    svc = os.path.join(d, 'cards.bin')
    run('5 TP 6282 1357\n40 TP 12400 1170\n45 300 R\n', 340, {'FP_LEVEL': 'at1', 'FP_CHAR': '4', 'FP_SAVE': svc})
    s = parse(svc)
    check('at1_card_21_saved', s['cards'] == {21}, str(sorted(s['cards'])))
    out = run('', 2, {'FP_LEVEL': 'at1', 'FP_SAVE': svc})
    check('card_total_reloaded', 'save cards 1' in out)
    # ---- CRC: a damaged save is ignored (defaults)
    b = bytearray(open(svc, 'rb').read()); b[20] ^= 0xff; open(svc, 'wb').write(b)
    out = run('', 2, {'FP_LEVEL': 'dv1', 'FP_SAVE': svc})
    check('damaged_save_ignored', 'save cards 0' in out)
    # ---- v2 save converted: records frames -> ms, DV card bits -> ids, a used file -> its route's first stage block
    files = b''
    for i in range(FILES):
        files += (bytes([1, 4, 1, 1, 0, 5]) + struct.pack('<H', 77) + bytes([14]) + struct.pack('<IH', 600000, 3)) if i == 3 else bytes(15)
    rec = b''.join(struct.pack('<IH', 1200 if (s_, c) == (1, 0) else 0, 50 if (s_, c) == (1, 0) else 0) for s_ in range(16) for c in range(CHARS))
    v2 = b'FPS\x02' + bytes([2, 80, 100, 70, 0]) + files + rec + bytes([0]) + struct.pack('<4H', 0b101, 0, 0, 0)
    v2 += struct.pack('<H', crc16(v2))
    svv = os.path.join(d, 'v2.bin'); open(svv, 'wb').write(v2)
    run('40 42 S\n200 201 U\n230 231 J\n280 281 U\n300 301 J\n', 380, {'FP_SAVE': svv})   # options OKAY: written as v3
    s = parse(svv)
    f = s['files'][3]
    check('v2_converted_records_ms', s['rec'][1][0] == (20000, 50), str(s['rec'][1][0]))
    check('v2_converted_cards', s['cards'] == {1, 3}, str(sorted(s['cards'])))
    check('v2_converted_file', f['used'] == 1 and f['ch'] == 4 and f['last'] == 0 and f['stage'][0] == (600000, 77, 3) and s['progress'][2] == 1,
          '%s last %d progress %s' % (f['stage'][0], f['last'], s['progress']))
    check('v2_options_kept', s['opts'][:2] == [2, 80] and s['opts'][4] == 0, str(s['opts']))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
