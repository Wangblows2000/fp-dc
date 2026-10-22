# gdb python: on the first call of a Frames:: function, list the classes in a QualifierList member
# (int count @+0, ObjectList** @+8; ObjectList: items @+8 (16-byte entries, index 0 unused), count @+0x10).
# usage: set $fn=0x... ; set $qoff=0x218d8 ; python exec(open("tools/re/qualifier.py").read())
import gdb, struct
inf = gdb.selected_inferior()
FN = int(gdb.parse_and_eval('$fn')); QOFF = int(gdb.parse_and_eval('$qoff'))
def q(a): return struct.unpack('<Q', bytes(inf.read_memory(a, 8)))[0]
def i32(a): return struct.unpack('<i', bytes(inf.read_memory(a, 4)))[0]
class B(gdb.Breakpoint):
    def stop(self):
        this = int(gdb.parse_and_eval('$rdi')) & 0xffffffffffffffff
        n = i32(this + QOFF); arr = q(this + QOFF + 8)
        gdb.write('QUAL +0x%x: %d object lists\n' % (QOFF, n))
        for i in range(n):
            ol = q(arr + 8 * i); items = q(ol + 8); cnt = q(ol + 0x10)
            cls = '?'
            if cnt > 1:
                obj = q(items + 16)
                cls = gdb.execute('info symbol 0x%x' % q(obj), to_string=True).split(' in section')[0]
            gdb.write('  list %d (+0x%x in Frames): %d instances, %s\n' % (i, ol - this, cnt - 1, cls))
        self.enabled = False
        gdb.post_event(lambda: gdb.execute('detach'))
        return True
B('*0x%x' % FN, internal=True)
