#!/usr/bin/env python3
"""
Recover per-class animation tables (and image ids) from the Linux Chowdren ELF.

usage: dump_anims.py <Chowdren ELF> <out.json> [class_filter_regex]

Decoded layouts (x86-64 build, see RESEARCH_LOG.md):
  anim_<cls>            : { u64 count; Animation **array }
  anim_array_<cls>      : Animation*[count]            (NULL = unused slot)
  Animation (anim_<cls>_<a>) : Direction*[32]           (Fusion's 32 directions)
  Direction (direction_<cls>_<a>_<d>, 24 bytes):
      u8 index, u8 unk, u16 min_speed, u16 max_speed, u16 back_to,
      s16 loop_count (-1 = forever), u16 frame_count, u32 pad, Image **images
  images_<cls>_<a>_<d>  : Image*[frame_count], filled in the class constructor by
      mov $IMAGE_ID,%edi ; call get_internal_image_direct ; mov %rax,images_..+8*i
IMAGE_ID indexes Assets.dat images (fp-assets img_<id>.png).
Classes without animation tables (backdrops) get 'images': every image id their
constructor loads, in order.
"""
import json, re, sys, struct, bisect
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_RIP

def main():
    path, out = sys.argv[1], sys.argv[2]
    filt = re.compile(sys.argv[3]) if len(sys.argv) > 3 else None
    f = open(path, 'rb'); e = ELFFile(f)
    segs = [s for s in e.iter_segments() if s['p_type'] == 'PT_LOAD']
    def rd(va, n):
        for s in segs:
            if s['p_vaddr'] <= va < s['p_vaddr'] + s['p_filesz']:
                f.seek(s['p_offset'] + va - s['p_vaddr']); return f.read(n)
        return b'\0' * n          # .bss
    syms = {}
    for s in e.get_section_by_name('.symtab').iter_symbols():
        if s.name: syms[s.name] = (s['st_value'], s['st_size'])
    def strip(n):  # _ZL25anim_playerskinsash_341 -> anim_playerskinsash_341
        m = re.match(r'_ZL\d+(.*)$', n); return m.group(1) if m else n
    plain = {}
    for n, v in syms.items():
        plain.setdefault(strip(n), v)

    # classes = create_<cls>_<id>
    classes = {}
    for n in syms:
        m = re.match(r'_Z\d+create_(\w+?)_(\d+)ii$', n)
        if m: classes['%s_%s' % (m.group(1), m.group(2))] = int(m.group(2))
    # constructor per class id: _ZN<len><Name>_<id>C2Eii
    ctors = {}
    for n, (va, sz) in syms.items():
        m = re.match(r'_ZN\d+\w+?_(\d+)C[12]Eii$', n)
        if m and sz: ctors.setdefault(int(m.group(1)), (va, sz))
    # images_* symbol ranges for address -> (cls,a,d,idx)
    img_syms = sorted((v[0], k) for k, v in plain.items() if k.startswith('images_'))
    img_addrs = [a for a, _ in img_syms]
    gii = {syms[n][0] for n in syms if n.startswith('_Z25get_internal_image_directj') or n.startswith('_Z18get_internal_imagej')}

    md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
    result = {}
    for cls, cid in sorted(classes.items()):
        if filt and not filt.search(cls): continue
        entry = {'id': cid}
        # animation table
        a_sym = plain.get('anim_' + cls)
        dirs = {}
        if a_sym:
            cnt, arr = struct.unpack('<QQ', rd(a_sym[0], 16))
            anims = []
            for ai in range(min(cnt, 256)):
                aptr = struct.unpack('<Q', rd(arr + 8 * ai, 8))[0]
                if not aptr: anims.append(None); continue
                dptrs = struct.unpack('<32Q', rd(aptr, 256))
                ds = {}
                for di, dp in enumerate(dptrs):
                    if not dp: continue
                    idx, unk, mn, mx, back, loop, fc, _pad, imgs = struct.unpack('<BBHHHhHIQ', rd(dp, 24))
                    if idx not in ds:
                        ds[idx] = {'min_speed': mn, 'max_speed': mx, 'back_to': back, 'loop': loop,
                                   'frames': [None] * fc, '_imgs': imgs, 'used_by': []}
                    ds[idx]['used_by'].append(di)
                    dirs[imgs] = (ai, idx)
                anims.append(ds)
            entry['anims'] = anims
        # constructor scan for image ids
        loaded = []
        if cid in ctors:
            va, sz = ctors[cid]
            code = rd(va, sz)
            last = None
            for ins in md.disasm(code, va):
                if ins.mnemonic == 'mov' and len(ins.operands) == 2:
                    d0, s0 = ins.operands
                    if d0.type == X86_OP_REG and ins.reg_name(d0.reg) in ('edi', 'rdi') and s0.type == X86_OP_IMM:
                        last = s0.imm & 0xffffffff
                    elif d0.type == X86_OP_MEM and d0.mem.base == X86_REG_RIP and s0.type == X86_OP_REG and ins.reg_name(s0.reg) == 'rax' and loaded and loaded[-1][1] is None:
                        tgt = ins.address + ins.size + d0.mem.disp
                        loaded[-1] = (loaded[-1][0], tgt)
                elif ins.mnemonic == 'call' and ins.operands and ins.operands[0].type == X86_OP_IMM:
                    if ins.operands[0].imm in gii:
                        loaded.append((last, None))
                    last = None
        entry['images'] = [i for i, _ in loaded]
        # place image ids into direction frame lists
        if a_sym:
            for img, tgt in loaded:
                if tgt is None: continue
                k = bisect.bisect_right(img_addrs, tgt) - 1
                if k < 0: continue
                base, name = img_syms[k]
                if base in dirs:
                    ai, di = dirs[base]
                    fi = (tgt - base) // 8
                    fr = entry['anims'][ai][di]['frames']
                    if fi < len(fr): fr[fi] = img
            for ds in entry['anims']:
                if ds:
                    for d in ds.values(): d.pop('_imgs', None)
            # JSON keys must be str
            entry['anims'] = [None if ds is None else {str(k): v for k, v in ds.items()} for ds in entry['anims']]
        result[cls] = entry
    json.dump(result, open(out, 'w'), indent=0, sort_keys=True)
    print(len(result), 'classes')

if __name__ == '__main__':
    main()
