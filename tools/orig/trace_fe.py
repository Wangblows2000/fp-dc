# gdb Python: front-end tracer for the ORIGINAL game (no gameplay objects needed).
# Injects scripted keys like trace_orig.py and writes one JSON line per logic frame with the frame
# index, the watched members (x, y, anim bytes, direction, frame, flags, alterables A-Z) and the
# global values; sounds / music / frame changes go to <out>.snd.
# usage (gdb -p PID -batch): set $script="in.txt"  set $out="out.jsonl"  set $nframes=300
#   set $watch="0xdb0,0xcb0,..."  (member offsets, tools/re/member_map.py)  then exec this file
# Script lines: "<start> <end> KEYS" (LRUDJXBS + l/r = LT/RT on f/v ... see KEYS) or "<f> SHOT name".
import gdb, struct, os, json

inf = gdb.selected_inferior()
KEYS = {'L': 0x40000050, 'R': 0x4000004f, 'U': 0x40000052, 'D': 0x40000051,
        'J': 0x7a, 'X': 0x78, 'B': 0x63, 'S': 0x0d, 'E': 0x1b}   # E = Escape
ORDER = 'LRUDJXBSE'
GM = 0x285ec60

def q(a): return struct.unpack('<Q', bytes(inf.read_memory(a, 8)))[0]

def load_script(path):
    ev, shots = [], {}
    for line in open(path):
        t = line.split()
        if len(t) == 3 and t[1] == 'SHOT': shots[int(t[0])] = t[2]
        elif len(t) == 3 and not line.startswith('#'): ev.append((int(t[0]), int(t[1]), t[2]))
    return ev, shots

class Snd(gdb.Breakpoint):
    def __init__(self, spec, kind, tr):
        super().__init__(spec, internal=True); self.kind = kind; self.tr = tr
    def stop(self):
        try:
            if self.kind == 'id':
                v = 'id %d ch%d loop%d' % (int(gdb.parse_and_eval('$esi')) & 0xffffffff, int(gdb.parse_and_eval('$edx')), int(gdb.parse_and_eval('$ecx')))
            elif self.kind == 'str':
                rsi = int(gdb.parse_and_eval('$rsi')) & 0xffffffffffffffff
                ptr, ln = struct.unpack('<QQ', bytes(inf.read_memory(rsi, 16)))
                v = 'str %s ch%d loop%d' % (bytes(inf.read_memory(ptr, min(ln, 200))).decode('latin1'), int(gdb.parse_and_eval('$edx')), int(gdb.parse_and_eval('$ecx')))
            elif self.kind == 'frame':
                v = 'set_frame %d' % (int(gdb.parse_and_eval('$esi')) & 0xffffffff)
                self.tr.idx = int(gdb.parse_and_eval('$esi')) & 0xffffffff
            else:
                v = '%s %d' % (self.kind, int(gdb.parse_and_eval('$esi')) & 0xffffffff)
        except Exception as e:
            v = 'err %s' % e
        self.tr.sndf.write('%d %s\n' % (self.tr.f, v)); self.tr.sndf.flush()
        return False

class Tracer(gdb.Breakpoint):
    def __init__(self):
        super().__init__('*0x9fcb7a', internal=True)
        self.ev, self.shots = load_script(gdb.parse_and_eval('$script').string())
        self.out = open(gdb.parse_and_eval('$out').string(), 'w')
        self.outpath = gdb.parse_and_eval('$out').string()
        self.sndf = open(self.outpath.rsplit('.', 1)[0] + '.snd', 'w')
        self.n = int(gdb.parse_and_eval('$nframes'))
        w = gdb.parse_and_eval('$watch').string()
        self.watch = [int(x, 16) for x in w.split(',') if x]
        self.f = -1; self.prev = set(); self.idx = -1
        Snd('*0xa4e650', 'id', self); Snd('*0xa4e2c0', 'str', self); Snd('*0x9fc8c0', 'frame', self)
    def keys(self, held):
        data = b''
        for c in ORDER:
            st = (0 if c not in self.prev else 1) if c in held else 2
            data += struct.pack('<iB3x', KEYS[c], st)
        inf.write_memory(0x285ecec, data); inf.write_memory(0x285ece8, struct.pack('<i', len(ORDER)))
    def stop(self):
        self.f += 1
        held = set()
        for a, b, k in self.ev:
            if a <= self.f < b: held |= set(k)
        self.keys(held); self.prev = held
        if self.f in self.shots:
            os.system('DISPLAY=%s import -window root %s_%s.png' % (os.environ.get('SHOT_DISPLAY', ':93'), self.outpath.rsplit('.', 1)[0], self.shots[self.f]))
        row = {'f': self.f, 'held': ''.join(sorted(held)), 'idx': self.idx, 'o': {}}
        try:
            fr = q(GM)
            for off in self.watch:
                try:
                    p = q(fr + off)
                    if p < 0x100000: continue
                    m = bytes(inf.read_memory(p, 0xc8))
                    x, y = struct.unpack_from('<ii', m, 8)
                    fl = struct.unpack_from('<I', m, 0x18)[0]
                    alt = struct.unpack_from('<Q', m, 0x20)[0]
                    b0, b1, b2 = m[0xb0], m[0xb1], m[0xb2]
                    afr = struct.unpack_from('<h', m, 0xb4)[0]; dr = m[0xb8]
                    vals = struct.unpack('<26d', bytes(inf.read_memory(alt + 0x50, 208))) if alt > 0x100000 else ()
                    row['o']['%x' % off] = [x, y, b0, b1, b2, dr, afr, '%x' % fl, {chr(65 + i): round(v, 6) for i, v in enumerate(vals) if v}]
                except Exception as e:
                    row['o']['%x' % off] = str(e)
            gp = q(GM + 8)
            row['gv'] = [round(v, 6) for v in struct.unpack('<40d', bytes(inf.read_memory(gp, 320)))] if gp else []
        except Exception as e:
            row['err'] = str(e)
        self.out.write(json.dumps(row) + '\n'); self.out.flush()
        if self.f >= self.n:
            self.out.close(); self.enabled = False
            gdb.post_event(lambda: gdb.execute('interrupt'))
        return False

Tracer()
