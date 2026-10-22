#!/usr/bin/env python3
"""Recover Backdrop-class pieces: image id + immediate field stores from inlined creators.
usage: dump_backdrops.py <Chowdren ELF> <out.json>"""
import json, re, sys
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG
path, out = sys.argv[1], sys.argv[2]
f = open(path, 'rb'); e = ELFFile(f)
segs = [s for s in e.iter_segments() if s['p_type'] == 'PT_LOAD']
def rd(va, n):
    for s in segs:
        if s['p_vaddr'] <= va < s['p_vaddr'] + s['p_filesz']:
            f.seek(s['p_offset'] + va - s['p_vaddr']); return f.read(n)
syms = {s.name: (s['st_value'], s['st_size']) for s in e.get_section_by_name('.symtab').iter_symbols() if s.name}
addr2name = {v[0]: k for k, v in syms.items() if v[1]}
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
res = {}
for n, (va, sz) in syms.items():
    m = re.match(r'_Z\d+create_(\w+?)_(\d+)ii$', n)
    if not m or not sz: continue
    code = rd(va, sz)
    ent = {'backdrop': False, 'image': None, 'fields': {}, 'or': {}}
    last_edi = None
    for ins in md.disasm(code, va):
        ops = ins.operands
        if ins.mnemonic == 'call' and ops and ops[0].type == X86_OP_IMM:
            tgt = addr2name.get(ops[0].imm, '')
            if 'Backdrop' in tgt and 'C' in tgt: ent['backdrop'] = True
            if 'get_internal_image' in tgt: ent['image'] = last_edi
            last_edi = None
        elif ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == X86_OP_REG and ins.reg_name(ops[0].reg) == 'edi' and ops[1].type == X86_OP_IMM:
            last_edi = ops[1].imm
        elif ins.mnemonic in ('movb', 'mov', 'movl', 'movw') and len(ops) == 2 and ops[0].type == X86_OP_MEM and ops[1].type == X86_OP_IMM and ins.reg_name(ops[0].mem.base) in ('rbx', 'rbp'):
            ent['fields']['0x%x' % ops[0].mem.disp] = ops[1].imm
        elif ins.mnemonic == 'or' and len(ops) == 2 and ops[0].type == X86_OP_MEM and ops[1].type == X86_OP_IMM:
            ent['or']['0x%x' % ops[0].mem.disp] = ops[1].imm
        if ins.mnemonic == 'ret': break
    if ent['backdrop']:
        res['%s_%s' % (m.group(1), m.group(2))] = ent
json.dump(res, open(out, 'w'), indent=0, sort_keys=True)
print(len(res), 'backdrop classes')
