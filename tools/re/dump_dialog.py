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
a continuation without a voice; its anim may be "xx" too, kept as -1: it still counts as a paragraph, the line index E
counts every paragraph, RV DialogData_3923 E 76 = "It's an ambush!"). LipSynchingData_<handle> objects (same layout) are dumped too.

Output: {"binary_md5", "dialog": {"<handle>": [[speaker, anim, voice, text], ...]}, "lipsync": {...},
        "subtitles": {"<handle>": [text, ...]} (SubtitleData objects: the subtitle paragraphs, session 26)}
"""
import hashlib, json, re, sys
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM

LINE = re.compile(rb'^(\d\d) (\d\d|xx) ([a-z0-9_]+) (.*)$', re.S)   # "03 xx xx_xx_xx like in the stories!": anim xx -> -1

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
        m = re.match(r'^_ZN(\d+)(DialogData|LipSynchingData|SubtitleData)_(\d+)C[12]Eii$', s.name) or \
            re.match(r'^_Z\d+create_(dialogdata|lipsynchingdata|subtitledata)_(\d+)ii$', s.name)
        if not m or not s['st_size']: continue
        if s.name.startswith('_ZN'): kind, h, ctor = m.group(2).lower(), m.group(3), 1
        else: kind, h, ctor = m.group(1), m.group(2), 0
        key = (kind, h)
        if ctor or key not in funcs or not funcs[key][2]:
            funcs[key] = (s['st_value'], s['st_size'], ctor)
    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
    res = {'binary_md5': hashlib.md5(open(path, 'rb').read()).hexdigest(), 'dialog': {}, 'lipsync': {}, 'subtitles': {}}
    # one paragraph per Text::add_line call (session 26): a literal the source repeats is built once and added again
    # (DialogData_3989 paragraphs 47-49 are the same za_04_14 line: RV e81_g3 DialogBox E 83 = li_49_01, i.e. two
    # paragraphs more than the literals before it)
    addl = next((s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols() if s.name == '_ZN4Text8add_lineERKSs'), None)
    for (kind, h), (va, size, ctor) in sorted(funcs.items(), key=lambda kv: int(kv[0][1])):
        lines = []; last = None
        for ins in md.disasm(rd(va, size), va):
            if ins.mnemonic == 'call' and addl is not None and ins.op_str == hex(addl):
                if last: lines.append(last if isinstance(last, str) else list(last))
                continue
            if ins.mnemonic != 'mov': continue
            for op in ins.operands:
                if op.type == X86_OP_IMM and 0x400000 <= op.imm < 0x4000000:
                    sb = cstr(op.imm)
                    if kind == 'subtitledata':    # SubtitleData_<h> (session 26): plain paragraphs (the epilogue's speech, RE 22248 / 22309)
                        if sb and len(sb) > 1 and sb != b'Arial' and all(32 <= c < 127 for c in sb): last = sb.decode('latin1')
                        continue
                    m = LINE.match(sb) if sb else None
                    if m:
                        last = [int(m.group(1)), -1 if m.group(2) == b'xx' else int(m.group(2)), m.group(3).decode(), m.group(4).decode('latin1')]
                        if addl is None: lines.append(last)
        if lines: res['dialog' if kind == 'dialogdata' else 'subtitles' if kind == 'subtitledata' else 'lipsync'][h] = lines
    # scene captions ("Three days later," ...): global std::string constants str_<slug>_<n> set from a literal at start-up;
    # the literal is the .rodata string whose slug is <slug> (RE: TextBlitter::set_text(str_three_days_later_330) in events)
    # Session 27: the value is read from the static initialiser that builds the constant (mov $len, %edx; mov $literal, %esi;
    # mov $str_<slug>_<n>, %edi; call std::string(const char *, size_t, alloc)): a slug can match several literals (str_dragon_valley_10
    # is 'DRAGON VALLEY', not the padded caption literal). Also the location names LocationName_592 types out at a story room's start
    # (RE: TextBlitter text = <name> "\n" <second line> in events 3322 / 3581 / 18771 / 19418 / 20319 / 20934 / 20974 / 21538 /
    # 21827 / 21912 / 21942 / 21962)
    CAPS = re.compile(r'^str_(.*(?:later|meanwhile|valley).*|lilac_s_treehouse|ancient_temple|shang_mu|commercial_district|'
                      r'shuigang_snowfields|north_avalice|royal_palace|shang_tu|shuigang_palace|jade_creek_outskirts|storm_drainage_tunnel)_(\d+)$')
    syms = {s['st_value']: s.name for s in e.get_section_by_name('.symtab').iter_symbols() if CAPS.match(s.name)}
    ctor = next((s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols() if s.name == '_ZNSsC1EPKcmRKSaIcE'), None)
    res['captions'] = {}
    for s in e.get_section_by_name('.symtab').iter_symbols():
        if '__static_initialization_and_destruction_0' not in s.name or not s['st_size']: continue
        reg = {}
        for ins in md.disasm(rd(s['st_value'], s['st_size']), s['st_value']):
            if ins.mnemonic == 'mov' and re.match(r'^e(dx|si|di), (0x[0-9a-f]+|\d+)$', ins.op_str):   # (a length below 10 prints
                reg[ins.op_str[:3]] = int(ins.op_str.split(', ')[1], 0)                             # in decimal: 'SHANG TU', 8)
            elif ins.mnemonic == 'call':
                if ctor and ins.op_str == hex(ctor) and reg.get('edi') in syms and 'edx' in reg and 'esi' in reg:
                    res['captions'][syms[reg['edi']]] = rd(reg['esi'], reg['edx']).decode('latin1')
                reg = {}
    res['captions'] = dict(sorted(res['captions'].items()))
    # the actors' ink (story scenes draw them): create_<class>_<handle> sets FrameObject::set_shader(<esi>) (0x20 = additive,
    # RE_NOTES "Blend modes") and the blend colour's alpha byte (movb $a, 0x5f(%rbx) after initialize_active; RE
    # create_effectsdust01_369: ff ff ff 74). Only non-default values are kept.
    shader_va = next((s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()
                      if s.name == '_ZN11FrameObject10set_shaderEi'), None)
    ink = {}
    for s in e.get_section_by_name('.symtab').iter_symbols():
        m = re.match(r'^_Z\d+create_([a-z0-9_]+_\d+)ii$', s.name)
        if not m or not s['st_size'] or s['st_size'] > 0x4000: continue
        esi, rec = None, {}
        for ins in md.disasm(rd(s['st_value'], s['st_size']), s['st_value']):
            if ins.mnemonic == 'mov' and ins.op_str.startswith('esi, 0x'): esi = int(ins.op_str.split(', ')[1], 16)
            elif ins.mnemonic == 'call' and shader_va and ins.op_str == hex(shader_va) and esi is not None: rec['shader'] = esi
            elif ins.mnemonic == 'mov' and re.match(r'byte ptr \[rbx \+ 0x5f\], 0x([0-9a-f]+)$', ins.op_str):
                a_ = int(ins.op_str.rsplit('0x', 1)[1], 16)
                if a_ != 0xff: rec['alpha'] = a_
        if rec: ink[m.group(1)] = rec
    res['ink'] = dict(sorted(ink.items()))
    with open(out, 'w', encoding='utf-8', newline='\n') as o:
        json.dump(res, o, indent=0, ensure_ascii=False); o.write('\n')
    print('dialog: %d objects, %d lines; lipsync: %d objects' % (len(res['dialog']), sum(len(v) for v in res['dialog'].values()), len(res['lipsync'])))

if __name__ == '__main__':
    if len(sys.argv) != 3: sys.exit(__doc__)
    main(*sys.argv[1:])
