# gdb python: on the first hit of a function, resolve Frames members at given offsets to their class
# (vtable symbol) and dump their alterables. usage (inside gdb attached to the original):
#   set $fn = 0xb8dc10 ; set $offs = "0x3730,0x1b70" ; python exec(open("tools/re/who.py").read())
import gdb, struct
inf = gdb.selected_inferior()
FN = int(gdb.parse_and_eval('$fn')); OFFS = [int(x, 16) for x in str(gdb.parse_and_eval('$offs').string()).split(',')]
def q(a): return struct.unpack('<Q', bytes(inf.read_memory(a, 8)))[0]
class B(gdb.Breakpoint):
    def stop(self):
        this = int(gdb.parse_and_eval('$rdi')) & 0xffffffffffffffff
        for o in OFFS:
            try:
                p = q(this + o)
                vt = q(p)
                sym = gdb.execute('info symbol 0x%x' % vt, to_string=True).strip()
                alt = q(p + 0x20)
                vals = struct.unpack('<26d', bytes(inf.read_memory(alt + 0x50, 208))) if alt > 0x10000 else ()
                gdb.write('WHO +0x%x obj 0x%x vt %s\n   alt %s\n' % (o, p, sym, ' '.join('%s=%g' % (chr(65 + i), v) for i, v in enumerate(vals) if v)))
            except Exception as e:
                gdb.write('WHO +0x%x err %s\n' % (o, e))
        self.enabled = False
        gdb.post_event(lambda: gdb.execute('detach'))
        return True
B('*0x%x' % FN, internal=True)
