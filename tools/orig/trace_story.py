# gdb Python: RV tracer for the ORIGINAL game's story scenes (Adventure mode, session 25).
# Run inside gdb attached to Chowdren (tools/orig/story_rv.sh drives it):
#   gdb -q -batch -p PID -ex 'set $out="x.log"' -ex 'set $nframes=4000' -ex 'python exec(open("tools/orig/trace_story.py").read())' \
#       -ex continue -ex detach
# Output <out> (one line per event, frame numbers = logic frames since attach):
#   F <f> <frame index>                  GameManager::set_frame (the frame that starts next)
#   G <f> g0 g1 ... g15                  global values when any of them changed
#   O <f> <obj> <class> x y anim afr dir vis img_anim img_dir img_frame [A=v ...]   an object's state when it changed
#                                         (vis: the FrameObject flags word; img_*: the image shown, from images_<class>_<a>_<d>)
#   D <f> <obj>                          object gone (vtable changed / freed)
#   S <f> id|str <value>                 Media::play_id / Media::play (sounds, voices, music; channel, loop)
#   K <f> <keys>                         keys injected this frame
#   C <f> x y                            the view's top-left (Frames + 0x213b4, Frame::set_display_center) when it changed
#   A <f> / I <f>                        J pressed by the reader (STORY_READ frames after the dialog arrow) / the idle tap
# Environment: STORY_TAP=N (tap J every N frames, default 0 = never), STORY_KEYS="f0-f1:KEYS,..." (explicit holds,
# LRUDJXBS as trace_orig.py), STORY_CLASSES=regex (classes to dump, default: every class of the json frame except
# tiles, backdrops, sensors, GUI and helpers), STORY_FRAMES=dir (tables/frames), STORY_SETG="f:g=v,..." (write global g
# at frame f), STORY_SETFRAME="f:idx" (GameManager::set_frame from the frame loop: armed by writing the pending-frame
# fields is not possible here, so it calls nothing; see story_rv.sh which restarts frames through gdb before tracing).
# Never calls inferior functions from a breakpoint (gdb hangs). Host-only tool: never shipped.
import gdb, os, re, struct, json, subprocess

inf = gdb.selected_inferior()
OUT = str(gdb.parse_and_eval('$out')).strip('"')
NFR = int(gdb.parse_and_eval('$nframes'))
FRAMESDIR = os.environ.get('STORY_FRAMES', 'tables/frames')
KEYS = {'L': 0x40000050, 'R': 0x4000004f, 'U': 0x40000052, 'D': 0x40000051, 'J': 0x7a, 'X': 0x78, 'B': 0x63, 'S': 0x0d}
ORDER = 'LRUDJXBS'
ACTORS = re.compile(r'^(npc|placeholder|playerskin(sash|carol|milla|torque|spade)?_|player_|the|brevon|torque|dialog|zao|gong|neera|'
                    r'syntax|kingdom|serpent|boss|comet|space|parallaxcrash|caravan|soldier|jeep|enemy|minibossufo|ship|pigeon)')
SKIP = re.compile(r'^(tile|levelparallax|levellayer|layer|solid|general|playersensor|gui|separator|sparkle|monitor|shield|'
                  r'obj\d|effects|spring|yellowspring|redspring|camera|master|input|globalinput|mouse|windowcontrol|savedata|'
                  r'mapdata|recorddata|sarassteam|stringparser|bubblenumbers|link|sign|enemylimiter|addtimebonus|scorevalues|'
                  r'timebonustotal|objectcrystal|objectpetal|playerstartingpoint|ringsparkle|thunderspark|levelcard|'
                  r'playeraction|playerothers|playergimmick|playercharactersensor|hazard|breakable|bg|objectcodec|laddertop|'
                  r'objectjumppad|objectfuel|platform|function|queue|controllermusic|levelfloating|gimmick)')

nm = {}
for l in open('build/re/nm.txt'):
    m = re.match(r'([0-9a-f]+) [Vv] vtable for ([A-Za-z0-9]+)_(\d+)$', l.strip())
    if m: nm[(m.group(2).lower(), int(m.group(3)))] = int(m.group(1), 16) + 0x10
IMGSYM = {}
for l in open('build/re/nm.txt'):
    m = re.match(r'([0-9a-f]+) [bBdD] (?:_ZL\d+)?images_([a-z0-9]+)_(\d+)_(\d+)_(\d+)$', l.strip())
    if m: IMGSYM.setdefault(m.group(2) + '_' + m.group(3), []).append((int(m.group(1), 16), int(m.group(4)), int(m.group(5))))

def frame_classes(idx):
    try: f = json.load(open(os.path.join(FRAMESDIR, 'frame_%d.json' % (idx + 1))))
    except Exception: return {}
    want = os.environ.get('STORY_CLASSES')
    out = {}
    for o in f['objects']:
        c = o['cls']
        if (want and re.search(want, c)) or (not want and ACTORS.match(c + '_') and not SKIP.match(c)):
            vt = nm.get((c, o['id']))
            if vt: out[vt] = '%s_%d' % (c, o['id'])
    return out

def heap_ranges():
    rs = []
    for l in open('/proc/%d/maps' % inf.pid):
        p = l.split()
        if len(p) >= 2 and 'rw' in p[1] and (len(p) == 5 or (len(p) > 5 and p[5] in ('[heap]',))):
            a, b = [int(x, 16) for x in p[0].split('-')]
            if b - a >= 1 << 20: rs.append((a, b))
    return rs

def rq(a, n): return bytes(inf.read_memory(a, n))

class Snd(gdb.Breakpoint):
    def __init__(self, spec, kind, t):
        super().__init__(spec, internal=True); self.kind = kind; self.t = t
    def stop(self):
        try:
            if self.kind == 'id':
                v = '%d ch%d loop%d' % (int(gdb.parse_and_eval('$esi')) & 0xffffffff, int(gdb.parse_and_eval('$edx')), int(gdb.parse_and_eval('$ecx')))
            else:
                rsi = int(gdb.parse_and_eval('$rsi')) & 0xffffffffffffffff
                ptr = struct.unpack('<Q', rq(rsi, 8))[0]
                ln = struct.unpack('<Q', rq(ptr - 0x18, 8))[0]
                v = '%s ch%d loop%d' % (rq(ptr, min(ln, 200)).decode('latin1'), int(gdb.parse_and_eval('$edx')), int(gdb.parse_and_eval('$ecx')))
        except Exception as e:
            v = 'err %s' % e
        self.t.w('S %d %s %s' % (self.t.f, self.kind, v)); return False

class SetFrame(gdb.Breakpoint):
    def __init__(self, t): super().__init__('*0x9fc8c0', internal=True); self.t = t
    def stop(self):
        idx = int(gdb.parse_and_eval('$esi')) & 0xffffffff
        self.t.w('F %d %d' % (self.t.f, idx)); self.t.cur = idx; self.t.rescan = 3
        return False

class Story(gdb.Breakpoint):
    def __init__(self):
        super().__init__('*0x9fcb7a', internal=True)
        self.o = open(OUT, 'w'); self.f = 0; self.cur = int(os.environ.get('STORY_START_FRAME', '-1')); self.rescan = 1
        self.objs = {}; self.lists = {}; self.last = {}; self.g = None; self.prev = set(); self.imgmap = {}
        self.tap = int(os.environ.get('STORY_TAP', '0')); self.holds = []; self.tap_from = int(os.environ.get('STORY_TAP_FROM', '0'))
        for kv in filter(None, os.environ.get('STORY_KEYS', '').split(',')):
            r, k = kv.split(':'); a, b = r.split('-'); self.holds.append((int(a), int(b), k))
        self.fr = None; self.cam = None
        self.shot = int(os.environ.get('STORY_SHOT', '0'))
        self.read = int(os.environ.get('STORY_READ', '0')); self.arrow_t = 0
        self.idle = int(os.environ.get('STORY_IDLE', '0')); self.last_j = 0
        self.setg = {}
        for kv in filter(None, os.environ.get('STORY_SETG', '').split(',')):
            fr, gv = kv.split(':'); g, v = gv.split('='); self.setg.setdefault(int(fr), []).append((int(g), float(v)))
        Snd('*0xa4e650', 'id', self); Snd('*0xa4e2c0', 'str', self); SetFrame(self)
    def w(self, s): self.o.write(s + '\n')
    def scan(self):
        """the actor classes' instance lists: the Frames members (ObjectList {back, items, count}, items of 16 bytes from
        index 1, as tools/orig/trace_orig.py XLIST) whose back object has one of the classes' vtables"""
        cls = frame_classes(self.cur) if self.cur >= 0 else {}
        self.cls = cls
        if not cls or not self.fr: return
        try: mem = rq(self.fr, 0x22800)
        except Exception: return
        cand = {}
        for off in range(0, len(mem) - 24, 8):
            p_ = struct.unpack_from('<Q', mem, off)[0]
            if 0x1000000 <= p_ < 0x7fffffffffff: cand.setdefault(p_, []).append(off)
        for p_, offs in cand.items():
            try: vt = struct.unpack('<Q', rq(p_, 8))[0]
            except Exception: continue
            if vt in cls:
                for off in offs: self.lists[off] = cls[vt]
        for c in set(self.lists.values()):
            for base, an, dr in IMGSYM.get(c, []):
                try: ptrs = struct.unpack('<64Q', rq(base, 512))
                except Exception: continue
                for i, p in enumerate(ptrs):
                    if not p: break
                    self.imgmap.setdefault(p, (an, dr, i))
    def live(self):
        out = {}
        for off, c in self.lists.items():
            try:
                items, cnt = struct.unpack('<QQ', rq(self.fr + off + 8, 16))
                if not items or cnt > 512: continue
                arr = rq(items, 16 * cnt)
                for i in range(1, cnt):
                    ob = struct.unpack_from('<Q', arr, 16 * i)[0]
                    if ob: out[ob] = c
            except Exception: pass
        return out
    def dump(self):
        self.objs = self.live()
        for ob in [o for o in self.last if o not in self.objs]:
            self.w('D %d %x' % (self.f, ob)); del self.last[ob]
        for ob, c in self.objs.items():
            try:
                m = rq(ob, 0x118)
                x, y = struct.unpack_from('<ii', m, 8); fl = struct.unpack_from('<I', m, 0x18)[0]
                alt = struct.unpack_from('<Q', m, 0x20)[0]
                aid = struct.unpack_from('<b', m, 0xb1)[0]; afr = struct.unpack_from('<h', m, 0xb4)[0]
                dr = struct.unpack_from('<i', m, 0x54)[0]; img = struct.unpack_from('<Q', m, 0x110)[0]
                vals = struct.unpack('<26d', rq(alt + 0x50, 208)) if alt > 0x10000 else ()
                ia = self.imgmap.get(img, (-1, -1, -1))
                s = '%d %d %d %d %d %x %d %d %d %s' % (x, y, aid, afr, dr, fl, ia[0], ia[1], ia[2],
                                                    ' '.join('%s=%.6g' % (chr(65 + i), v) for i, v in enumerate(vals) if v))
            except Exception:
                continue
            if self.last.get(ob) != s:
                self.last[ob] = s; self.w('O %d %x %s %s' % (self.f, ob, c, s))
    def keys(self):
        if self.f < self.tap_from: return              # before STORY_TAP_FROM: real (xdotool) keys pass through
        held = set()
        if self.tap and self.f % self.tap == 0: held.add('J')
        if self.read:                                   # STORY_READ=n: J n frames after the dialog arrow shows (a reader)
            vis = False
            for ob, c in self.objs.items():
                if c.startswith('dialogarrow'):
                    try: vis = vis or (struct.unpack('<I', rq(ob + 0x18, 4))[0] & 0x1001) == 0x1001
                    except Exception: pass
            self.arrow_t = self.arrow_t + 1 if vis else 0
            if self.arrow_t == self.read: held.add('J'); self.w('A %d' % self.f)
        if self.idle:                                   # STORY_IDLE=n: J when no J for n frames (menus inside scenes)
            if 'J' in held: self.last_j = self.f
            elif self.f - self.last_j >= self.idle: held.add('J'); self.last_j = self.f; self.w('I %d' % self.f)
        for a, b, k in self.holds:
            if a <= self.f < b: held |= set(k)
        data = b''
        for c in ORDER:
            st = (0 if c not in self.prev else 1) if c in held else 2
            data += struct.pack('<iB3x', KEYS[c], st)
        inf.write_memory(0x285ecec, data); inf.write_memory(0x285ece8, struct.pack('<i', len(ORDER)))
        if held != self.prev and held: self.w('K %d %s' % (self.f, ''.join(sorted(held))))
        self.prev = held
    def stop(self):
        gp = struct.unpack('<Q', rq(0x285ec68, 8))[0]
        for g, v in self.setg.get(self.f, []): inf.write_memory(gp + 8 * g, struct.pack('<d', v))
        g = struct.unpack('<16d', rq(gp, 128))
        gk = g[:3] + g[4:]                                  # g3 counts frames: not a change by itself
        if gk != self.g: self.g = gk; self.w('G %d %s' % (self.f, ' '.join('%g' % v for v in g)))
        if self.shot and self.f % self.shot == 0:
            os.makedirs(OUT + '_shots', exist_ok=True)
            os.system('DISPLAY=:93 import -window root %s_shots/f%05d.png' % (OUT, self.f))
        if self.fr is None or self.rescan:                 # the Frames object: the GameManager word whose +0x213a0 is the globals
            self.fr = None
            gm = rq(0x285ec60, 0x800)
            for k in range(0, 0x800, 8):
                cand = struct.unpack_from('<Q', gm, k)[0]
                if cand < 0x10000: continue
                try:
                    if struct.unpack('<Q', rq(cand + 0x213a0, 8))[0] == gp: self.fr = cand; break
                except Exception: pass
        if self.fr:                                       # C: the view's top-left (Frame off_x / off_y, set_display_center)
            try:
                cam = struct.unpack('<ii', rq(self.fr + 0x213b4, 8))
                if cam != self.cam: self.cam = cam; self.w('C %d %d %d' % (self.f, cam[0], cam[1]))
            except Exception: pass
        if self.rescan:
            self.rescan -= 1
            if self.rescan == 0: self.lists = {}; self.scan()
        elif self.f % 120 == 60: self.scan()
        self.dump(); self.keys()
        self.f += 1
        if self.f % 200 == 0: self.o.flush(); gdb.write('frame %d (index %d)\n' % (self.f, self.cur)); gdb.flush()
        if self.f >= NFR:
            self.o.flush(); gdb.execute('detach'); return True
        return False

STORY = Story()
