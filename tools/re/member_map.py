# gdb python: on the first call of a Frames:: function (rdi = the Frames object), map every member
# offset that points at a live FrameObject to its class (vtable symbol). Writes $out.
# usage: set $fn=0xaa1570 ; set $out="build/re/fe/members_f4.txt" ; python exec(open("tools/re/member_map.py").read())
import gdb, struct
inf = gdb.selected_inferior()
FN = int(gdb.parse_and_eval('$fn')); OUT = gdb.parse_and_eval('$out').string()
def q(a): return struct.unpack('<Q', bytes(inf.read_memory(a, 8)))[0]
class B(gdb.Breakpoint):
    def stop(self):
        this = int(gdb.parse_and_eval('$rdi')) & 0xffffffffffffffff
        mem = bytes(inf.read_memory(this, 0x22800))
        cache = {}; lines = []
        for off in range(0, len(mem), 8):
            p = struct.unpack_from('<Q', mem, off)[0]
            if p < 0x1000000 or p > 0x7fffffffffff: continue
            try: vt = q(p)
            except Exception: continue
            if vt not in cache:
                try: s = gdb.execute('info symbol 0x%x' % vt, to_string=True)
                except Exception: s = ''
                cache[vt] = s.split(' in section')[0].replace('vtable for ', '').replace(' + 16', '') if 'vtable for' in s else None
            if cache[vt]:
                try: x, y = struct.unpack('<ii', bytes(inf.read_memory(p + 8, 8)))
                except Exception: x = y = 0
                lines.append('0x%x %s %d %d' % (off, cache[vt], x, y))
        open(OUT, 'w').write('\n'.join(lines) + '\n')
        gdb.write('member_map: %d members -> %s\n' % (len(lines), OUT))
        self.enabled = False
        gdb.post_event(lambda: gdb.execute('detach'))
        return False
B('*0x%x' % FN, internal=True)
