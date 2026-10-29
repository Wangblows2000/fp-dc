#!/usr/bin/env python3
"""
Recover the story dialogue (DialogData objects) from the Linux Chowdren ELF (session 25, Adventure mode).

usage: dump_dialog.py <Chowdren ELF> <out.json>

Each DialogData_<handle> object is a Fusion Text object whose constructor adds one paragraph per line, in order (RE:
DialogData_1624::DialogData_1624 -> Text::Text, then one std::string per paragraph from a string literal). The literal is
passed as `mov $<address>, %esi` (x86-64, non-PIE), so the paragraph order is the order of those immediates in the
constructor's code. A class without its own constructor symbol has it inlined in create_dialogdata_<handle> (3616).
Every paragraph has the form "<speaker> <anim> <voice id> <text>" (two-digit speaker = the DialogBox2 name label frame,
two-digit anim = the speaker actor's animation, voice id = Data/voices/eng/<xx>/<id>.ogg; "xx_xx_xx" / "<who>_xx_xx" =
a continuation without a voice). LipSynchingData_<handle> objects (same layout) are dumped too.

Output: {"binary_md5", "dialog": {"<handle>": [[speaker, anim, voice, text], ...]}, "lipsync": {...}}
"""
import hashlib, json, re, sys
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM

LINE = re.compile(rb'^(\d\d) (\d\d) ([a-z0-9_]+) (.*)$', re.S)

def main(path, out):
    f = open(path, 'rb'); e = ELFFile(f)
    segs = [s for s in e.iter_segments() if s['p_type'] == 'PT_LOAD']
    def rd(va, n):
        for s in segs:
            if s['p_vaddr'] <= va < s['p_vaddr'] + s['p_filesz']:
                f.seek(s['p_offset'] + va - s['p_vaddr']); return f.read(n)
        return None
    def cstr(va):
        b = rd(va, 1024)
        return b.split(b'\0', 1)[0] if b else None
    funcs = {}
    for s in e.get_section_by_name('.symtab').iter_symbols():
        m = re.match(r'^_ZN(\d+)(DialogData|LipSynchingData)_(\d+)C[12]Eii$', s.name) or \
            re.match(r'^_Z\d+create_(dialogdata|lipsynchingdata)_(\d+)ii$', s.name)
        if not m or not s['st_size']: continue
        if s.name.startswith('_ZN'): kind, h, ctor = m.group(2).lower(), m.group(3), 1
        else: kind, h, ctor = m.group(1), m.group(2), 0
        key = (kind, h)
        if ctor or key not in funcs or not funcs[key][2]:
            funcs[key] = (s['st_value'], s['st_size'], ctor)
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
    res = {'binary_md5': hashlib.md5(open(path, 'rb').read()).hexdigest(), 'dialog': {}, 'lipsync': {}}
    for (kind, h), (va, size, ctor) in sorted(funcs.items(), key=lambda kv: int(kv[0][1])):
        lines = []
        for ins in md.disasm(rd(va, size), va):
            if ins.mnemonic != 'mov': continue
            for op in ins.operands:
                if op.type == X86_OP_IMM and 0x400000 <= op.imm < 0x4000000:
                    sb = cstr(op.imm)
                    m = LINE.match(sb) if sb else None
                    if m: lines.append([int(m.group(1)), int(m.group(2)), m.group(3).decode(), m.group(4).decode('latin1')])
        if lines: res['dialog' if kind == 'dialogdata' else 'lipsync'][h] = lines
    with open(out, 'w', encoding='utf-8', newline='\n') as o:
        json.dump(res, o, indent=0, ensure_ascii=False); o.write('\n')
    print('dialog: %d objects, %d lines; lipsync: %d objects' % (len(res['dialog']), sum(len(v) for v in res['dialog'].values()), len(res['lipsync'])))

if __name__ == '__main__':
    if len(sys.argv) != 3: sys.exit(__doc__)
    main(*sys.argv[1:])
