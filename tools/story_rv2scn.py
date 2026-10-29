#!/usr/bin/env python3
"""
Developer tool (session 25, Adventure mode): an RV trace of an original story scene (tools/orig/trace_story.py, run by
tools/orig/story_rv.sh with the reader STORY_READ) -> the port's scene script, committed in tables/story/scripts/<id>.scn.
End users never run this: the builder only copies the committed scripts (tools/import_story.py).

usage: story_rv2scn.py <trace.log> <scene id> <json frame> <dialog handle|-> <first frame> <end frame> <out.scn>
                       [--actors REGEX] [--drop REGEX] [--next KIND:ARG] [--title TEXT] [--from F]

The trace covers one visit of a story room: frames [first, end) of the log (the room's 'F' event to the next one).
Everything in the script is RV (read from the running original), turned into segments:

  segment 0 starts at <first>; each reader press 'A <P>' (J pressed with the dialog arrow shown) ends a segment, and the
  next one starts at P + 1, the frame the original reads the press (InputsP1, one frame late). Inside a segment the times
  are frames since its start. The port's player runs a segment's entries as they come; at its end it waits for the
  player's A (with the arrow shown) instead of the reader's 30 frames. A segment cut short by an early press applies the
  rest of its entries at once (positions / images snap to the original's state at the press).

The format (one entry per line; '#' comments) is documented in src/game/story.h ("Scene script").
"""
import json, os, re, sys, argparse

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

# never actors: the DialogBox (its own entries), backdrops ('pv'), stage cards (the stage's own), fades ('f'), the player's
# hidden skins and helpers
DROP = re.compile(r'^(dialog|lipsync|levelparallax|levelcard|levelcardfade|fade|fadeedge|player_|playerskin(spade|torque)|'
                  r'playerreflect|playerdig|playerskintorquearm|whitefade|mouseobject|globalinputdata|ini_|highlighter|'
                  r'controlbox|savedata|mapdata|recorddata|guipause|crystal_447|sarassteam|windowcontrol|stagelist|'
                  r'masterlevel|playerassist|enemylimiter|sign_|link_|separator|objectcrystal_317|monitor|yellowspring|redspring|'
                  r'gimmickbridge|objectbike|thunderspark|effectswatersplash|ringsparkle|playerskincarolbike|playerskinmilla|'
                  r'effectfallingleaf)')   # ambient falling leaves (random sine paths: 36k entries a scene) are left out (INFERRED)

def load_dialog(handle):
    d = json.load(open(os.path.join(ROOT, 'tables', 'story', 'dialog.json'), encoding='utf-8'))
    return d['dialog'].get(str(handle), [])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log'); ap.add_argument('sid'); ap.add_argument('json', type=int); ap.add_argument('dialog')
    ap.add_argument('first', type=int); ap.add_argument('end', type=int); ap.add_argument('out')
    ap.add_argument('--actors', default=r'.', help='classes that may be actors (default: every traced class but the --drop ones)')
    ap.add_argument('--drop', default=r'^$', help='more classes left out (besides DROP: the DialogBox, backdrops, cards, fades, helpers)')
    ap.add_argument('--next', default='route')
    ap.add_argument('--title', default='')
    ap.add_argument('--chsel', default='', help='OPEN,PRESS: the in-scene character select opens at frame OPEN, the choice at PRESS')
    ap.add_argument('--rows', default='', help='extra rows class_handle:anim:dir,... (rows the trace did not show)')
    ap.add_argument('--caption', action='append', default=[], help='F0,F1,key|key: caption rows shown from F0 to F1 (RV screenshots)')
    ap.add_argument('--flash', default='', help='F,HOLD,N: white from frame F, HOLD frames, then a fade from white of N frames (RV screenshots; timing INFERRED)')
    ap.add_argument('--fadeout', default='', help='F,N: a fade to black of N frames from frame F (INFERRED: the wipe)')
    a = ap.parse_args()
    lines = load_dialog(a.dialog) if a.dialog != '-' else []
    fr = json.load(open(os.path.join(ROOT, 'tables', 'frames', 'frame_%d.json' % a.json)))
    layer_of = {}
    for o in fr['objects']: layer_of.setdefault('%s_%d' % (o['cls'], o['id']), o.get('layer', 2))
    act_re, drop_re = re.compile(a.actors), re.compile(a.drop)

    # ---- parse the window
    obj = {}           # addr -> {'cls', 'st': {f: state}}
    par = {}           # parallax class -> {'st': {f: vis}, 'alt'}
    box = []           # (f, vis, E, F, C, img frame)
    snd, cams, presses, gone = [], [], [], {}
    par_addr = {}      # a parallax piece's address -> its class (a destroyed piece is hidden: 'pv h 0')
    for l in open(a.log, encoding='latin1'):
        t = l.split()
        if not t or len(t) < 2 or not t[1].lstrip('-').isdigit(): continue
        f = int(t[1])
        if f < a.first or f >= a.end:
            if t[0] == 'O' and f < a.first and a.first == 0: pass   # (a trace that starts in the room: its first states)
            elif t[0] == 'C' and f < a.first: pass                  # the view before the window
            else: continue
        if t[0] == 'O':
            addr, cls = t[2], t[3]
            x, y, aid, afr, dr, fl = int(t[4]), int(t[5]), int(t[6]), int(t[7]), int(t[8]), int(t[9], 16)
            ia, ib, ic = int(t[10]), int(t[11]), int(t[12])
            alt = dict(kv.split('=') for kv in t[13:])
            if cls.startswith('dialogbox_'):
                box.append((f, fl & 1, int(float(alt.get('E', 0))), int(float(alt.get('F', 0))), int(float(alt.get('C', 0))), ic))
                continue
            if fl >= 0x10000: continue                  # not an object (old traces: a non-list member)
            if cls.startswith(('levelparallax', 'parallax')):   # backdrop pieces: visibility per scene ('pv'), scroll from D / E / F (RV)
                pp = par.setdefault(cls, {'st': {}, 'alt': alt}); par_addr[addr] = cls
                pp['st'][max(f, a.first)] = fl & 1
                continue
            o = obj.setdefault(addr, {'cls': cls, 'st': {}})
            o['st'][max(f, a.first)] = (x, y, ia, ib, ic, fl & 1)
        elif t[0] == 'D' and a.first <= f < a.end: gone[t[2]] = f
        elif t[0] == 'S': snd.append((f, t[2], ' '.join(t[3:])))
        elif t[0] == 'C': cams.append((f, int(t[2]), int(t[3])))
        elif t[0] == 'A': presses.append(f)
    # segment starts
    chsel = [int(v) for v in a.chsel.split(',')] if a.chsel else None
    if chsel: presses = sorted(set(presses + [chsel[1]]))
    segs = [a.first] + [p + 1 for p in presses if a.first <= p < a.end - 1]
    def seg_of(f):
        k = 0
        while k + 1 < len(segs) and segs[k + 1] <= f: k += 1
        return k
    ev = [[] for _ in segs]                    # per segment: (t, order, text)
    def put(f, order, txt):
        k = seg_of(f); ev[k].append((f - segs[k], order, txt))

    # ---- actors: visible at least once in the window with a known image
    # the view per frame (an actor must be seen: a story room holds the props of all its scenes)
    camf = sorted((f, x, y) for f, x, y in cams)
    def view(f):
        v = (0, 0)
        for cf, x, y in camf:
            if cf > f: break
            v = (x, y)
        return v
    actors = []
    for addr, o in obj.items():
        c = o['cls']
        if drop_re.search(c) or DROP.match(c) or not act_re.match(c.rsplit('_', 1)[0] + '_'): continue
        if re.match(r'^(levelcardfade|fadeedge|fade_)', c): continue          # fades: 'f' entries below
        fixed = layer_of.get(c, 2) >= 6
        seen = []
        fs_ = sorted(o['st'])
        for k_, f in enumerate(fs_):
            st_ = o['st'][f]
            if not st_[5] or st_[2] < 0: continue
            f2 = fs_[k_ + 1] if k_ + 1 < len(fs_) else a.end
            vs = [(0, 0)] if fixed else [view(f)] + [(x, y) for cf, x, y in camf if f < cf < f2]
            if any(vx - 48 < st_[0] < vx + 426 + 48 and vy - 48 < st_[1] < vy + 240 + 48 for vx, vy in vs): seen.append(f)
        if seen: actors.append((min(seen), addr))
    if len(actors) > 128:
        from collections import Counter
        sys.exit('%d actors (the player holds 128): narrow --actors / --drop: %s' % (len(actors), Counter(obj[a_]['cls'] for _, a_ in actors)))
    actors.sort()
    rows = set()
    for r in filter(None, a.rows.split(',')):
        c_, an_, d_ = r.split(':'); rows.add((c_, int(an_), int(d_)))
    decl = []
    for n, (_, addr) in enumerate(actors):
        o = obj[addr]; c = o['cls']; h = int(c.rsplit('_', 1)[1])
        lay = 2 if c.startswith(('playerskin', 'player_')) else layer_of.get(c, 2)   # the player's templates sit on layer 5 off-map
        decl.append('actor %d %s %d' % (n, c, lay))
        prev = None; run = None                      # run: [f0, dx, dy, count]
        def flush_run():
            if run and run[3]: put(run[0], 1, 'm %d %d %d %d' % (n, run[1], run[2], run[3]))
        for f in sorted(o['st']):
            x, y, ia, ib, ic, vis = o['st'][f]
            if prev is None:
                put(f, 0, 'p %d %d %d' % (n, x, y)); put(f, 2, 'v %d %d' % (n, vis))
                if ia >= 0: put(f, 2, 'i %d %d %d %d' % (n, ia, ib, ic)); rows.add((c, ia, ib))
                prev = (f, x, y, ia, ib, ic, vis); continue
            pf, px, py, pia, pib, pic, pvis = prev
            dx, dy = x - px, y - py
            if (dx or dy):
                if f == pf + 1 and run and run[1] == dx and run[2] == dy and run[0] + run[3] == f and seg_of(run[0]) == seg_of(f):
                    run[3] += 1
                elif f == pf + 1:
                    flush_run(); run = [f, dx, dy, 1]
                else:
                    flush_run(); run = None; put(f, 0, 'p %d %d %d' % (n, x, y))
            if (ia, ib, ic) != (pia, pib, pic) and ia >= 0:
                put(f, 2, 'i %d %d %d %d' % (n, ia, ib, ic)); rows.add((c, ia, ib))
            if vis != pvis: put(f, 2, 'v %d %d' % (n, vis))
            prev = (f, x, y, ia, ib, ic, vis)
        flush_run()
        if addr in gone: put(gone[addr], 2, 'v %d 0' % n)
    # ---- fades: LevelCardFade2 (the black cover) visible at the window's start = a fade from black until it hides (RV
    # visibility; the original's alpha ramp is not traced: linear, INFERRED)
    for addr, o in obj.items():
        if not o['cls'].startswith('levelcardfade2'): continue
        st = sorted(o['st'].items())
        if st and st[0][1][5]:
            off = next((f for f, s_ in st if not s_[5]), None)
            if off is not None and off - a.first < 120: put(a.first, 3, 'f %d' % (off - a.first))
    for c in a.caption:
        f0, f1, keys = c.split(',', 2)
        for r, k in enumerate(keys.split('|')): put(int(f0), 3, 'cap %d %s' % (r, k))
        if int(f1) < a.end: put(int(f1), 3, 'capx')
    if a.flash:
        ff, hh, nn = [int(v) for v in a.flash.split(',')]; put(ff, 3, 'f -1 1'); put(ff + hh, 3, 'f %d 1' % nn)
    if a.fadeout:
        ff, nn = [int(v) for v in a.fadeout.split(',')]; put(ff, 3, 'f %d' % -nn)
    if chsel:
        put(chsel[0], 6, 'chsel')
        k0 = seg_of(chsel[0])                                 # the idle wait inside the select: nothing of it is kept
        ev[k0] = [e for e in ev[k0] if e[0] <= chsel[0] - segs[k0]]
    # ---- parallax pieces: shown / hidden per scene (the room's other scenes use other sets)
    pinfo = {}
    for addr, c in par_addr.items():           # destroyed at the room's start (RV c81_g2: 3223 / 3224 / 3225 / 3227 at frame 1)
        if addr in gone: par[c]['st'][max(gone[addr], a.first)] = 0
    for c, pp in par.items():
        h = int(c.rsplit('_', 1)[1]); prev = None
        for f in sorted(pp['st']):
            if pp['st'][f] != prev: put(f, 3, 'pv %d %d' % (h, pp['st'][f])); prev = pp['st'][f]
        al = pp['alt']
        pinfo[str(h)] = [float(al.get('D', 0)), float(al.get('E', 0)), float(al.get('F', 0))]
    # ---- camera
    cam0 = None
    for f, x, y in cams:
        if f < a.first: cam0 = (x, y); continue
        put(f, 3, 'c %d %d' % (x, y))
    if cam0: put(a.first, 3, 'c %d %d' % cam0)
    # ---- dialogue: a line starts when the box shows with C 1 (18606 / 18607: F 1 -> 2, three characters)
    voiced = set(); said = []
    pv, pe = 0, -1
    for f, vis, E, F, C, img in box:
        if f < a.first: pv, pe = vis, E; continue
        if vis and F == 2 and C == 1 and (not pv or E != pe or True):
            if not said or said[-1][1] != E or said[-1][0] < f - 2:
                put(f, 4, 'say %d %d' % (E, img)); said.append((f, E))
                if 0 <= E < len(lines): voiced.add((lines[E][2], f))
        if pv and not vis: put(f, 4, 'close')
        pv, pe = vis, E
    vids = {v for v, _ in voiced}
    for f, kind, val in snd:
        if f < a.first: continue
        m = re.match(r'(\S+) ch(-?\d+) loop(\d+)$', val)
        what = m.group(1) if m else val
        if kind == 'id':
            if chsel and chsel[1] < f <= chsel[1] + 3 and what in ('63', '220', '221', '186'): continue   # the choice's own sounds (story.c)
            put(f, 5, 's %s' % what)
        else:
            b = what.replace('\\', '/').split('/')[-1]
            if '/BGM/' in what.replace('\\', '/'): put(f, 5, 'u bgm/%s.adp' % b[:-4])
            elif b[:-4] in vids: continue                       # the line's own voice (the player plays it with 'say')
            else: put(f, 5, 'o %s' % b[:-4])                    # a voice outside the dialogue
    out = ['# scene %s: RV, tools/story_rv2scn.py %s frames %d-%d (json frame %d)' % (a.sid, os.path.basename(a.log), a.first, a.end, a.json),
           'scene %s' % a.sid, 'room st%d' % a.json, 'dialog %s' % a.dialog]
    if a.title: out.append('title %s' % a.title)
    out += decl
    for k, e in enumerate(ev):
        out.append('seg %d' % k)
        out += ['%d %s' % (t, txt) for t, o_, txt in sorted(e, key=lambda r: (r[0], r[1]))]
    out.append('end %d' % (a.end - segs[-1]))
    out.append('next %s' % a.next)
    with open(a.out, 'w', encoding='utf-8', newline='\n') as f: f.write('\n'.join(out) + '\n')
    rp = os.path.join(ROOT, 'tables', 'story', 'rows.json')
    R = json.load(open(rp)) if os.path.exists(rp) else {}
    R[a.sid] = {'json': a.json, 'rows': sorted([c, ia, ib] for c, ia, ib in rows), 'par': pinfo}
    with open(rp, 'w', encoding='utf-8', newline='\n') as f: json.dump(R, f, indent=0, sort_keys=True); f.write('\n')
    print('%s: %d actors, %d segments, %d lines said, %d rows, %d entries' % (a.sid, len(actors), len(segs), len(said), len(rows), sum(len(e) for e in ev)))

if __name__ == '__main__':
    main()
