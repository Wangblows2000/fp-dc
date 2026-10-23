# gdb Python: frame-exact input injection + per-logic-frame state dump for the ORIGINAL game.
# Run inside gdb attached to a Chowdren process that is already in gameplay with menu state set:
#   gdb -q -batch -p PID -ex 'set $script="in.txt"' -ex 'set $out="trace.csv"' -ex 'set $nframes=300' \
#       -ex 'set $restart=1' -ex 'python exec(open("tools/orig/trace_orig.py").read())' -ex continue -ex detach
# Input script: lines "<start> <end> <buttons>" (same as fpdc host), buttons LRUDJXBS:
#   L/R/U/D arrows, J=z (jump), X=x (attack), B=c (special), S=Return.  [key->action mapping: see RUNTIME_MAP.md]
# Verified (RUNTIME_MAP.md):
#   key list: count @manager+0x88 (0x285ece8), entries @0x285ecec {int sdl_keycode; u8 state} 8 bytes;
#             state 0 = pressed this frame, 1 = held, 2 = up. is_key_pressed: state!=2; _once: state==0.
#   0x9fcb7a = right after platform_poll_events() inside GameManager::update (one Frame::update per call).
#   FrameObject: x @+8, y @+0xc (int), alterables* @+0x20; values A..Z = 26 doubles @alt+0x50.
#   Active animation index: s8 @+0xb0.
import gdb, os, struct, os

inf = gdb.selected_inferior()
KEYS = {'L': 0x40000050, 'R': 0x4000004f, 'U': 0x40000052, 'D': 0x40000051,
        'J': 0x7a, 'X': 0x78, 'B': 0x63, 'S': 0x0d}
ORDER = 'LRUDJXBS'
VT = {'mv': 0x20b2910, 'player': 0x20b6f30, 'skin': 0x20b7890}   # vtable+0x10 of the classes
# optional HUD/gameplay objects dumped when present (vtable+0x10): alterables A-Z + x scale + image id
XVT = {'en': 0x20aa470, 'life': 0x20aa510, 'others': 0x20aa3d0, 'turr': 0x20d65d0, 'cam': 0x20bbbd0,
       'hair': 0x20bc5d0, 'sp1': 0x20b68f0, 'sp2': 0x20b9c30, 'sp3': 0x20b9cd0, 'sp4': 0x20b9d70, 'sp5': 0x20b3c70, 'plat3': 0x20dab50}   # plat3 = LevelMovingPlatform3_948; hair = PlayerSkinSashHair_424 (Lilac's hair overlay)
# extra objects for one trace: XVT_EXTRA="name=0xvtable+0x10,name2=..." (vtable addresses from build/re/nm.txt + 0x10)
# "name=0xvtable#i" picks the i-th live instance (heap address order) instead of the last one (multi-instance classes)
XVT_IDX = {}
for _kv in os.environ.get('XVT_EXTRA', '').split(','):
    if '=' in _kv:
        _n, _v = _kv.split('=')
        if '#' in _v: _v, _i = _v.split('#'); XVT_IDX[_n] = int(_i)
        XVT[_n] = int(_v, 0)
# instances picked by x position instead of the last one found (probes of one specific enemy)
XVT_PICK = {'turr': 1233}   # others = PlayerOthers_126 (invulnerability timer E, RE)
# HUD counters (Counter::set: value double @+0xa0, min int @+0xa8, max int @+0xac)
CVT = {'c_petals': 0x20aa330, 'c_lives': 0x20bc670, 'c_crystals': 0x20aa290,
       'c_tm': 0x20bd890, 'c_ts': 0x20bd750, 'c_tms': 0x20bd610}
HEAP = None
NGV = 40   # global values dumped (manager+0x8 -> doubles)

def heap_range():
    for l in open('/proc/%d/maps' % inf.pid):
        p = l.split()
        if len(p) > 5 and p[5] == '[heap]':
            a, b = [int(x, 16) for x in p[0].split('-')]
            return a, b

def find_objs():
    a, b = heap_range()
    mem = bytes(inf.read_memory(a, b - a))
    found = {}
    for k, vt in list(VT.items()) + list(XVT.items()) + list(CVT.items()):
        pat = struct.pack('<Q', vt)
        i = mem.find(pat)
        while i >= 0:
            if i % 8 == 0:
                obj = a + i
                alt = struct.unpack_from('<Q', mem, i + 0x20)[0]
                if a <= alt < b or k in XVT or k in CVT:  # live object with alterables (HUD objects may have none)
                    found.setdefault(k, []).append(obj)
            i = mem.find(pat, i + 1)
    return found

def rd_obj(o):
    m = bytes(inf.read_memory(o, 0xb8))
    x, y = struct.unpack_from('<ii', m, 8)
    alt = struct.unpack_from('<Q', m, 0x20)[0]
    vals = struct.unpack('<26d', bytes(inf.read_memory(alt + 0x50, 208)))
    anim = struct.unpack_from('<b', m, 0xb0)[0]
    return x, y, vals, anim

IMGMAP = {}
def build_imgmap():
    # images_playerskinsash_341_<anim>_<dir>: Image*[n] (bss, filled at class init) -> (anim, dir, frame)
    import re, subprocess
    out = subprocess.run(['nm', '/home/claude/fprun/bin64/Chowdren'], capture_output=True, text=True).stdout
    for l in out.splitlines():
        m = re.match(r'([0-9a-f]+) [bBdD] (?:_ZL\d+)?images_playerskinsash_341_(\d+)_(\d+)$', l)
        if m:
            base = int(m.group(1), 16); a, d = int(m.group(2)), int(m.group(3))
            ptrs = struct.unpack('<64Q', bytes(inf.read_memory(base, 512)))
            for i, ptr in enumerate(ptrs):
                if ptr == 0: break
                IMGMAP.setdefault(ptr, (a, d, i))

SETC = {}
WATCH = {}
REFIND = set()
WHO = {}
WATCHES = []
TP = {}   # debug route: "<frame> TP <x> <y>" teleports Lilac (writes PlayerMovementValues X/Y, zeroes B/G)
def load_script(path):
    ev = []
    if path and os.path.exists(path):
        for line in open(path):
            t = line.split()
            if len(t) == 3 and t[1] == 'EN':      # debug: "<frame> EN <v>" writes the energy meter (GUIEnergy alt A)
                SETC[int(t[0])] = ('en_alt', float(t[2]))
            elif len(t) == 3 and t[1] == 'HP':      # debug: "<frame> HP <v>" writes the petal counter value
                SETC[int(t[0])] = ('c_petals', float(t[2]))
            elif len(t) == 2 and t[1] == 'REFIND':  # "<f> REFIND": find the objects again (after a frame change)
                REFIND.add(int(t[0]))
            elif len(t) == 4 and t[1] == 'WHO':     # "<f> WHO <func addr> <off,off,..>": log member classes on each call
                WHO.setdefault(int(t[0]), []).append((int(t[2], 16), [int(x, 16) for x in t[3].split(',')]))
            elif len(t) == 4 and t[1] == 'WATCH':   # "<f> WATCH <obj> <alt index|cnt>": log every write (bt)
                WATCH.setdefault(int(t[0]), []).append((t[2], t[3]))
            elif len(t) == 4 and t[1] == 'TP':
                TP[int(t[0])] = (float(t[2]), float(t[3]))
            elif len(t) == 3 and not line.startswith('#'):
                ev.append((int(t[0]), int(t[1]), t[2]))
    return ev

def held_at(ev, f):
    s = set()
    for a, b, btn in ev:
        if a <= f < b: s |= set(btn)
    return s

SND = []   # (frame, kind, value): Media::play_id(this, id, channel, loop) / Media::play(this, std::string&, ch, loop)
class SndBP(gdb.Breakpoint):
    def __init__(self, spec, kind, tracer):
        super().__init__(spec, internal=True); self.kind = kind; self.t = tracer
    def stop(self):
        try:
            if self.kind == 'id':
                v = '%d ch%d loop%d' % (int(gdb.parse_and_eval('$esi')) & 0xffffffff, int(gdb.parse_and_eval('$edx')), int(gdb.parse_and_eval('$ecx')))
            else:
                rsi = int(gdb.parse_and_eval('$rsi')) & 0xffffffffffffffff
                ptr, ln = struct.unpack('<QQ', bytes(inf.read_memory(rsi, 16)))
                v = '%s ch%d loop%d' % (bytes(inf.read_memory(ptr, min(ln, 200))).decode('latin1'), int(gdb.parse_and_eval('$edx')), int(gdb.parse_and_eval('$ecx')))
        except Exception as e:
            v = 'err %s' % e
        if self.t.sndf: self.t.sndf.write('%d %s %s\n' % (self.t.f, self.kind, v)); self.t.sndf.flush()
        return False

class WatchBP(gdb.Breakpoint):
    # hardware write watchpoint on one double; logs frame, new value and the backtrace to <out>.watch
    def __init__(self, addr, name, tracer):
        super().__init__('*(double*)0x%x' % addr, gdb.BP_WATCHPOINT, gdb.WP_WRITE, internal=True)
        self.addr = addr; self.name = name; self.t = tracer
        self.wf = open(tracer.outpath[:-4] + '.watch', 'a')
    def stop(self):
        try:
            v = struct.unpack('<d', bytes(inf.read_memory(self.addr, 8)))[0]
            bt = gdb.execute('bt 8', to_string=True)
        except Exception as e:
            v, bt = 0, 'err %s' % e
        self.wf.write('== f%d %s = %.6g\n%s' % (self.t.f, self.name, v, bt)); self.wf.flush()
        return False

class WhoBP(gdb.Breakpoint):
    # on each call of a Frames:: function, log the class (vtable symbol), position and alterables of
    # the objects pointed to by this+off (e.g. the selected instance of a collision handler)
    def __init__(self, fn, offs, tracer):
        super().__init__('*0x%x' % fn, internal=True); self.offs = offs; self.t = tracer; self.n = 0
        self.wf = open(tracer.outpath[:-4] + '.who', 'a')
    def stop(self):
        self.n += 1
        if self.n > 40: return False
        this = int(gdb.parse_and_eval('$rdi')) & 0xffffffffffffffff
        for o in self.offs:
            try:
                p = struct.unpack('<Q', bytes(inf.read_memory(this + o, 8)))[0]
                vt = struct.unpack('<Q', bytes(inf.read_memory(p, 8)))[0]
                sym = gdb.execute('info symbol 0x%x' % vt, to_string=True).split(' in section')[0]
                x, y = struct.unpack('<ii', bytes(inf.read_memory(p + 8, 8)))
                alt = struct.unpack('<Q', bytes(inf.read_memory(p + 0x20, 8)))[0]
                vals = struct.unpack('<26d', bytes(inf.read_memory(alt + 0x50, 208))) if alt > 0x10000 else ()
                self.wf.write('f%d +0x%x %s @%d,%d %s\n' % (self.t.f, o, sym, x, y, ' '.join('%s=%g' % (chr(65 + i), v) for i, v in enumerate(vals) if v)))
            except Exception as e:
                self.wf.write('f%d +0x%x err %s\n' % (self.t.f, o, e))
        self.wf.flush()
        return False

class Tracer(gdb.Breakpoint):
    def __init__(self, script, out, n, restart):
        super().__init__('*0x9fcb7a', internal=True)
        self.ev = load_script(script); self.n = n; self.f = -1; self.prev = set()
        self.sndf = open(out[:-4] + '.snd', 'w')
        SndBP('*0xa4e650', 'id', self); SndBP('*0xa4e2c0', 'str', self)
        self.outpath = out; self.out = open(out, 'w'); self.objs = None; self.restart_wait = (int(os.environ.get('WARM', 120)) if restart else 0); self.warm = 0
        # skin_anim = animation*1000 + frame
        # sk_ang/sk_xs/sk_ys: Active angle @+0xc4, x/y scale @+0xc8/+0xcc (Active::set_angle/set_x_scale)
        hdr = (['frame', 'held', 'px', 'py', 'skin_anim'] + ['mv_' + chr(65 + i) for i in range(26)] +
               ['sk_ang', 'sk_xs', 'sk_ys'] + ['pl_' + chr(65 + i) for i in range(26)] + ['gv%d' % i for i in range(NGV)] +
               sum([[k + '_' + chr(65 + i) for i in range(26)] + [k + '_xs', k + '_img', k + '_afr', k + '_acc', k + '_aid', k + '_ang', k + '_fl', k + '_x', k + '_y'] for k in XVT], []) +
               sum([[k, k + '_max'] for k in CVT], []) +
               ['sa_' + chr(65 + i) for i in range(26)] + ['sw_%x' % o for o in range(0x10, 0xc4, 4)])
        # sa_*: skin (Lilac display Active) alterables; sw_*: raw u32 words of the skin object (flags/blink search)
        self.out.write(','.join(hdr) + '\n'); self.out.flush()
    def write_keys(self, held):
        data = b''
        for c in ORDER:
            if c in held: st = 0 if c not in self.prev else 1
            else: st = 2
            data += struct.pack('<iB3x', KEYS[c], st)
        inf.write_memory(0x285ecec, data)
        inf.write_memory(0x285ece8, struct.pack('<i', len(ORDER)))
    def extra(self):
        out = []
        for k in XVT:
            o = self.objs.get(k)
            if not o: out += ['0'] * 35; continue
            try:
                try: ox, oy, vals, _ = rd_obj(o)
                except Exception: ox, oy, vals = 0, 0, (0,) * 26
                xs = struct.unpack('<f', bytes(inf.read_memory(o + 0xc8, 4)))[0]
                img = struct.unpack('<Q', bytes(inf.read_memory(o + 0x110, 8)))[0]
                afr = struct.unpack('<h', bytes(inf.read_memory(o + 0xb4, 2)))[0]    # Active frame index
                acc = struct.unpack('<i', bytes(inf.read_memory(o + 0xc0, 4)))[0]    # Active animation counter
                aid = struct.unpack('<b', bytes(inf.read_memory(o + 0xb1, 1)))[0]   # Active animation id
                ang = struct.unpack('<f', bytes(inf.read_memory(o + 0xc4, 4)))[0]   # Active angle
                fl = struct.unpack('<I', bytes(inf.read_memory(o + 0x18, 4)))[0]    # FrameObject flags (visibility)
                out += ['%.6g' % v for v in vals] + ['%.6g' % xs, '%x' % img, str(afr), str(acc), str(aid), '%.6g' % ang, '%x' % fl, str(ox), str(oy)]
            except Exception:
                out += ['0'] * 35
        for k in CVT:
            o = self.objs.get(k)
            if not o: out += ['', '']; continue
            v = struct.unpack('<d', bytes(inf.read_memory(o + 0xa0, 8)))[0]
            mx = struct.unpack('<i', bytes(inf.read_memory(o + 0xac, 4)))[0]
            out += ['%.6g' % v, str(mx)]
        return out
    def stop(self):
        # NOTE: never call inferior functions from here (gdb hangs). Restarting the stage is
        # done by the driver script before this tracer is armed (see run_trace.sh).
        if self.warm < self.restart_wait:              # let a freshly (re)started frame settle
            self.warm += 1; self.write_keys(set()); self.prev = set(); return False
        if self.f + 1 in REFIND and self.objs is not None:
            REFIND.discard(self.f + 1); self.objs = None; self.restart_wait = 0
        if self.objs is None:
            o = find_objs()
            if not all(k in o for k in VT):
                gdb.write('objects not found yet %s\n' % {k: len(v) for k, v in o.items()})
                self.write_keys(set()); return False
            # stale (freed) objects keep their vtable in the heap: pick the live trio whose
            # positions agree (player x,y == skin x,y; movement X,Y round to player x,y)
            best = None
            for pl in o['player']:
                px, py, _, _ = rd_obj(pl)
                for sk in o['skin']:
                    sx, sy, _, _ = rd_obj(sk)
                    for mv in o['mv']:
                        _, _, vals, _ = rd_obj(mv)
                        sc = abs(px - sx) + abs(py - sy) + abs(int(vals[23]) - px) + abs(int(vals[24]) - py)
                        if best is None or sc < best[0]: best = (sc, pl, sk, mv)
            self.objs = {'player': best[1], 'skin': best[2], 'mv': best[3]}
            for k in list(XVT) + list(CVT):
                if k in o:
                    if k in XVT_IDX:
                        live = sorted(a for a in o[k] if rd_obj(a)[2])
                        if XVT_IDX[k] < len(live): self.objs[k] = live[XVT_IDX[k]]
                    elif k in XVT_PICK:
                        self.objs[k] = min(o[k], key=lambda a: abs(rd_obj(a)[0] - XVT_PICK[k]) if rd_obj(a)[2] else 1e9)
                    else: self.objs[k] = o[k][-1]
            gdb.write('extra %s\n' % {k: [hex(v) for v in o.get(k, [])] for k in XVT})
            gdb.write('match score %d\n' % best[0])
            for pl in o['player']:
                gdb.write('player cand %x %s\n' % (pl, rd_obj(pl)[:2]))
            gdb.write('objects %s\n' % {k: hex(v) for k, v in self.objs.items()})
            # petals and lives are globals that survive the stage restart: reset them so every trace starts
            # from a fresh-file state (14 half-petals, 5 lives) regardless of earlier probes
            if self.restart_wait:
                for k, v in (('c_petals', 14.0), ('c_lives', 5.0)):
                    if k in self.objs: inf.write_memory(self.objs[k] + 0xa0, struct.pack('<d', v))
        self.f += 1
        if str(self.f) in os.environ.get('SHOTS', '').split(','):   # SHOTS=f1,f2: screenshot (last presented frame)
            os.system('DISPLAY=%s import -window root %s_f%d.png' % (os.environ.get('SHOT_DISPLAY', ':93'), self.outpath[:-4], self.f))
        if self.f % 50 == 0:
            import time
            gdb.write('frame %d t=%.1f\n' % (self.f, time.time())); gdb.flush()
        held = held_at(self.ev, self.f)
        self.write_keys(held); self.prev = held
        if self.f in SETC and SETC[self.f][0] == 'en_alt' and 'en' in self.objs:
            alt = struct.unpack('<Q', bytes(inf.read_memory(self.objs['en'] + 0x20, 8)))[0]
            inf.write_memory(alt + 0x50, struct.pack('<d', SETC[self.f][1]))
        elif self.f in SETC and SETC[self.f][0] in self.objs:
            inf.write_memory(self.objs[SETC[self.f][0]] + 0xa0, struct.pack('<d', SETC[self.f][1]))
        for fn, offs in WHO.get(self.f, []):
            WATCHES.append(WhoBP(fn, offs, self))
        for k, idx in WATCH.get(self.f, []):
            if idx == 'cnt': addr = self.objs[k] + 0xa0
            else:
                alt = struct.unpack('<Q', bytes(inf.read_memory(self.objs[k] + 0x20, 8)))[0]
                addr = alt + 0x50 + 8 * int(idx)
            WATCHES.append(WatchBP(addr, '%s.%s' % (k, idx), self))   # keep a reference (else GC drops it)
            gdb.write('watch %s.%s at 0x%x\n' % (k, idx, addr)); gdb.write(gdb.execute('info watchpoints', to_string=True))
        if self.f in TP:
            alt = struct.unpack('<Q', bytes(inf.read_memory(self.objs['mv'] + 0x20, 8)))[0]
            x, y = TP[self.f]
            inf.write_memory(alt + 0x50 + 8 * 23, struct.pack('<2d', x, y))   # X, Y
            inf.write_memory(alt + 0x50 + 8 * 1, struct.pack('<d', 0.0))      # B
            inf.write_memory(alt + 0x50 + 8 * 6, struct.pack('<d', 0.0))      # G
        # state at the START of logic frame f (= result of frame f-1)
        px, py, _, _ = rd_obj(self.objs['player'])
        _, _, mv, _ = rd_obj(self.objs['mv'])
        _, _, _, anim = rd_obj(self.objs['skin'])
        img = struct.unpack('<Q', bytes(inf.read_memory(self.objs['skin'] + 0x110, 8)))[0]
        if not IMGMAP: build_imgmap()
        a_d_f = IMGMAP.get(img)
        anim = '%d.%d.%d' % a_d_f if a_d_f else 'img%x' % img
        _, _, pl, _ = rd_obj(self.objs['player'])
        sk = struct.unpack('<3f', bytes(inf.read_memory(self.objs['skin'] + 0xc4, 12)))
        gptr = struct.unpack('<Q', bytes(inf.read_memory(0x285ec60 + 8, 8)))[0]
        gv = struct.unpack('<%dd' % NGV, bytes(inf.read_memory(gptr, 8 * NGV))) if gptr else (0,) * NGV
        self.out.write(','.join([str(self.f), ''.join(sorted(held)) or '-', str(px), str(py), str(anim)] +
                                ['%.6g' % v for v in mv] + ['%.6g' % v for v in sk] + ['%.6g' % v for v in pl] +
                                ['%.6g' % v for v in gv] + self.extra() + self.skin_extra()) + '\n')
        if self.f >= self.n:
            self.out.close(); self.enabled = False
            gdb.post_event(lambda: gdb.execute('interrupt'))
        return False

def _skin_extra(self):
    try:
        _, _, sa, _ = rd_obj(self.objs['skin'])
    except Exception:
        sa = (0,) * 26
    w = struct.unpack('<%dI' % ((0xc4 - 0x10) // 4), bytes(inf.read_memory(self.objs['skin'] + 0x10, 0xc4 - 0x10)))
    return ['%.6g' % v for v in sa] + ['%x' % v for v in w]
Tracer.skin_extra = _skin_extra

class CallerBP(gdb.Breakpoint):
    # TRACE_CREATE=<addr>: log every call of a function (e.g. a create_<class>) with its caller symbol
    def __init__(self, addr, tracer):
        super().__init__('*0x%x' % addr, internal=True); self.t = tracer
        self.wf = open(tracer.outpath[:-4] + '.create', 'a')
    def stop(self):
        try:
            rsp = int(gdb.parse_and_eval('$rsp')) & 0xffffffffffffffff
            ret = struct.unpack('<Q', bytes(inf.read_memory(rsp, 8)))[0]
            sym = gdb.execute('info symbol 0x%x' % ret, to_string=True).strip()
        except Exception as e:
            sym = 'err %s' % e
        self.wf.write('f%d %s\n' % (self.t.f, sym)); self.wf.flush()
        return False

class AddObjBP(gdb.Breakpoint):
    # TRACE_ADDOBJ=1: log every Frame::add_object (class of the new object, position, caller)
    def __init__(self, addr, tracer):
        super().__init__('*0x%x' % addr, internal=True); self.t = tracer
        self.wf = open(tracer.outpath[:-4] + '.addobj', 'a')
    def stop(self):
        try:
            o = int(gdb.parse_and_eval('$rsi')) & 0xffffffffffffffff
            vt = struct.unpack('<Q', bytes(inf.read_memory(o, 8)))[0]
            cls = gdb.execute('info symbol 0x%x' % vt, to_string=True).split(' in section')[0]
            x, y = struct.unpack('<ii', bytes(inf.read_memory(o + 8, 8)))
            rsp = int(gdb.parse_and_eval('$rsp')) & 0xffffffffffffffff
            ret = struct.unpack('<Q', bytes(inf.read_memory(rsp, 8)))[0]
            sym = gdb.execute('info symbol 0x%x' % ret, to_string=True).split(' in section')[0]
            self.wf.write('f%d %s @%d,%d from %s\n' % (self.t.f, cls, x, y, sym)); self.wf.flush()
        except Exception as e:
            self.wf.write('f%d err %s\n' % (self.t.f, e))
        return False

class AnimBP(gdb.Breakpoint):
    # TRACE_FANIM=<vtable+0x10>: log Active::force_animation / set_direction calls on objects of that class
    def __init__(self, addr, kind, vt, tracer):
        super().__init__('*0x%x' % addr, internal=True); self.t = tracer; self.kind = kind; self.vt = vt
        self.wf = open(tracer.outpath[:-4] + '.anim', 'a')
    def stop(self):
        try:
            this = int(gdb.parse_and_eval('$rdi')) & 0xffffffffffffffff
            if struct.unpack('<Q', bytes(inf.read_memory(this, 8)))[0] != self.vt: return False
            v = int(gdb.parse_and_eval('$esi')) & 0xffffffff
            x, y = struct.unpack('<ii', bytes(inf.read_memory(this + 8, 8)))
            rsp = int(gdb.parse_and_eval('$rsp')) & 0xffffffffffffffff
            ret = struct.unpack('<Q', bytes(inf.read_memory(rsp, 8)))[0]
            sym = gdb.execute('info symbol 0x%x' % ret, to_string=True).split(' in section')[0]
            self.wf.write('f%d %s %d obj %x @%d,%d from %s\n' % (self.t.f, self.kind, v, this, x, y, sym)); self.wf.flush()
        except Exception as e:
            self.wf.write('f%d err %s\n' % (self.t.f, e))
        return False

_tr = Tracer(str(gdb.parse_and_eval('$script').string()), str(gdb.parse_and_eval('$out').string()),
       int(gdb.parse_and_eval('$nframes')), int(gdb.parse_and_eval('$restart')))
if os.environ.get('TRACE_CREATE'):
    _cbp = [CallerBP(int(a, 16), _tr) for a in os.environ['TRACE_CREATE'].split(',')]
if os.environ.get('TRACE_ADDOBJ'):
    _aobp = [AddObjBP(int(a, 16), _tr) for a in os.environ['TRACE_ADDOBJ'].split(',')]
if os.environ.get('TRACE_FANIM'):
    _vt = int(os.environ['TRACE_FANIM'], 16)
    _abp = [AnimBP(0x20279d0, 'anim', _vt, _tr), AnimBP(0x2028040, 'dir', _vt, _tr)]
